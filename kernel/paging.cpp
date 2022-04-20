#include "paging.hpp"

#include <array>

#include "asmfunc.h"
#include "memory_manager.hpp"
#include "task.hpp"

namespace {
  const uint64_t kPageSize4K = 4096;
  const uint64_t kPageSize2M = 512 * kPageSize4K;
  const uint64_t kPageSize1G = 512 * kPageSize2M;

  alignas(kPageSize4K) std::array<uint64_t, 512> pml4_table;
  alignas(kPageSize4K) std::array<uint64_t, 512> pdp_table;
  alignas(kPageSize4K)
    std::array<std::array<uint64_t, 512>, kPageDirectoryCount> page_directory;
}

/**
 * @fn
 * ResetCR3関数
 * @brief 
 * CR3レジスタにOS用のPML4を設定する。
 */
void ResetCR3() {
  SetCR3(reinterpret_cast<uint64_t>(&pml4_table[0]));
}

/**
 * @fn
 * SetupIdentityPageTable関数
 * 
 * @brief 
 * 仮想アドレス=物理アドレスとなるようにページテーブルを設定する．
 * 最終的に CR3 レジスタが正しく設定されたページテーブルを指すようになる．
 */
void SetupIdentityPageTable() {
  // ページマップレベル4テーブルにPDPテーブル(第3階層)の先頭アドレスをマッピング
  pml4_table[0] = reinterpret_cast<uint64_t>(&pdp_table[0]) | 0x003;
  // PDPテーブル（ページディレクトリポインタテーブル第3階層）のテーブルごとにループ
  for (int i_pdpt = 0; i_pdpt < page_directory.size(); ++i_pdpt) {
    // PDPテーブルにページディレクトリ(第2階層)の先頭アドレスをマッピング
    pdp_table[i_pdpt] = reinterpret_cast<uint64_t>(&page_directory[i_pdpt]) | 0x003;
    // ページディレクトリ(第2階層)のディレクトリ毎にループ
    for (int i_pd = 0; i_pd < 512; ++i_pd) {
      // ページディレクトリに、ページテーブル（第一階層）の先頭アドレスをマッピング
      page_directory[i_pdpt][i_pd] = i_pdpt * kPageSize1G + i_pd * kPageSize2M | 0x083;
    }
  }
  // PML4（ページマップレベル4テーブル）の物理アドレスをCR3レジスタに設定
  ResetCR3();
}

/**
 * @fn
 * FindFileMaping関数
 * @brief 
 * 指定されたアドレスを含むファイルマッピングを探す
 * @param fmaps 検索対象のファイルマッピング
 * @param causal_vaddr 検索対象のアドレス
 * @return const FileMapping* 見つからなかったらnullptr
 */
const FileMapping* FindFileMapping(const std::vector<FileMapping>& fmaps,
                                  uint64_t causal_vaddr) {
  for (const FileMapping& m : fmaps) {
    if (m.vaddr_begin <= causal_vaddr && causal_vaddr < m.vaddr_end) {
      return &m;
    }
  }
  return nullptr;
}

/**
 * @fn
 * PreparePageCache関数
 * @brief 
 * 指定されたページを作成してファイルをコピーする
 * @param fd コピー対象のファイルのふぁいるで
 * @param m ファイルマッピング
 * @param causal_vaddr コピー先の物理アドレス
 * @return Error 
 */
Error PreparePageCache(FileDescriptor& fd, const FileMapping& m,
                       uint64_t causal_vaddr) {
  LinearAddress4Level page_vaddr{causal_vaddr};
  page_vaddr.parts.offset = 0;
  if (auto err = SetupPageMaps(page_vaddr, 1)) {
    return err;
  }

  const long file_offset = page_vaddr.value - m.vaddr_begin;
  void* page_cache = reinterpret_cast<void*>(page_vaddr.value);
  fd.Load(page_cache, 4096, file_offset);
  return MAKE_ERROR(Error::kSuccess);
}

/**
 * @fn
 * HandlePageFault関数
 * @brief 
 * ページフォルトが置きたときに、例外の原因となったページにフレームを割り当てる。
 * @param error_code エラーコード
 * @param causal_addr エラーが発生した原因のアドレス
 * @return Error 
 */
Error HandlePageFault(uint64_t error_code, uint64_t causal_addr) {
  auto& task = task_manager->CurrentTask();
  if (error_code & 1) {  // P=1 かつページレベルの権限違反により例外が置きた
    return MAKE_ERROR(Error::kAlreadyAllocated);
  }
  // 先頭側のアドレス領域を、デマンドページングにより拡張する
  if ( task.DPagingBegin() <= causal_addr &&  causal_addr < task.DPagingEnd()) {
    return SetupPageMaps(LinearAddress4Level{causal_addr}, 1);
  }
  // 末尾側のアドレス領域は、メモリマップトファイルのページキャッシュとして確保する
  if (auto m = FindFileMapping(task.FileMaps(), causal_addr)) {
    return PreparePageCache(*task.Files()[m->fd], *m, causal_addr);
  }
  return MAKE_ERROR(Error::kIndexOutOfRange);
}

/**
 * @fn
 * NewPageMap
 * 
 * @brief 
 * 階層ページング構造の仮想アドレスと物理アドレスのマッピングを新たに作成する
 * 使用しおわったら必ず解放すること
 * @return WithError<PageMapEntry*> 
 */
WithError<PageMapEntry*> NewPageMap() {
  auto frame = memory_manager->Allocate(1);
  if (frame.error) {
    return { nullptr, frame.error };
  }

  auto e = reinterpret_cast<PageMapEntry*>(frame.value.Frame());
  memset(e, 0, sizeof(uint64_t) * 512);
  return { e, MAKE_ERROR(Error::kSuccess) };
}

/**
 * @fn
 * SetNewPageMapIfNotPresent関数
 * @brief Set the New Map If Not Present object
 * 必要に応じて新たなページング構造を生成して設定する
 * @param entry エントリインデックス
 * @return WithError<PageMapEntry*> 
 */
WithError<PageMapEntry*> SetNewPageMapIfNotPresent(PageMapEntry& entry) {
  // エントリのpresentフラグが1であれば、すでにこのエントリは設定済みとして何もせず終了
  if (entry.bits.present) {
    return { entry.Pointer(), MAKE_ERROR(Error::kSuccess) };
  }

  // 新しいページング構造を生成
  auto [ child_map, err ] = NewPageMap();
  if (err) {
    return { nullptr, err };
  }

  // 生成したページング構造をエントリのaddrフィールドに設定
  entry.SetPointer(child_map);
  entry.bits.present = 1;

  return { child_map, MAKE_ERROR(Error::kSuccess)};
}

/**
 * @fn
 * SetupPageMap関数
 * 
 * @brief 
 * 指定された階層のページング設定をする
 * @param page_map その階層のページング構造の」物理アドレス
 * @param page_map_level 階層レベル
 * @param addr LOADセグメントの仮想アドレス
 * @param num_4kpages LOADセグメントの残りページ数
 * @return WithError<size_t> 未処理のページとエラー
 */
WithError<size_t> SetupPageMap(
    PageMapEntry* page_map, int page_map_level, LinearAddress4Level addr, size_t num_4kpages) {
  // すべてのページを処理しきるまでループ
  while (num_4kpages > 0) {
    // 仮想アドレスから、その階層のどのエントリに属するかを取得
    const auto entry_index = addr.Part(page_map_level);

    // 下位の階層の物理アドレスを取得
    auto [ child_map, err ] = SetNewPageMapIfNotPresent(page_map[entry_index]);
    if (err) {
      return { num_4kpages, err };
    }
    // 書き込み可能フラグを1にする
    page_map[entry_index].bits.writable = 1;
    // userフラグを1にする
    page_map[entry_index].bits.user = 1;

    if (page_map_level == 1) {
      --num_4kpages;
    } else {
      // ひとつ下位の階層ページング構造を再帰的に設定
      auto [ num_remain_pages, err ] =
        SetupPageMap(child_map, page_map_level -1, addr, num_4kpages);
      if (err) {
        return { num_4kpages, err };
      }
      num_4kpages = num_remain_pages;
    }

    if (entry_index == 511) {
      break;
    }

    // 残りを次のエントリに設定
    addr.SetPart(page_map_level, entry_index+1);
    for (int level = page_map_level - 1; level >= 1; --level) {
      // 下位のエントリにすべて0を設定
      addr.SetPart(level, 0);
    }
  }
  return { num_4kpages, MAKE_ERROR(Error::kSuccess) };
}

/**
 * @fn
 * SetupPageMaps関数
 * @brief 
 * 階層ページング構造全体を設定する
 * @param addr LOADセグメントの先頭アドレス
 * @param num_4kpages LOADセグメントのページ数
 * @return Error 
 */
Error SetupPageMaps(LinearAddress4Level addr, size_t num_4kpages) {
  // CR3レジスタから階層ページング構造の最上位PML4の物理アドレスを取得
  auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
  return SetupPageMap(pml4_table, 4, addr, num_4kpages).error;
}

/**
 * @fn
 * FreePageMap関数
 * @brief 
 * 指定されたページを解放する
 * @param table 開放するページのPageMapEntry
 * @return Error 
 */
Error FreePageMap(PageMapEntry* table) {
  const FrameID frame{reinterpret_cast<uintptr_t>(table) / kBytesPerFrame};
  return memory_manager->Free(frame, 1);
}

/**
 * @fn
 * CleanPageMap関数
 * @brief 
 * 指定されたページング構造のエントリをすべて削除する
 * @param page_map 削除するページング構造
 * @param page_map_level ページング構造の階層レベル
 * @return Error 
 */
Error CleanPageMap(PageMapEntry* page_map, int page_map_level) {
  for (int i = 0; i < 512; ++i) {
    auto entry = page_map[i];
    // presentフラグが立ってない場合は、確保されていないのでスキップ
    if (!entry.bits.present) {
      continue;
    }

    if (page_map_level > 1) {
      // 再帰呼び出し
      if (auto err = CleanPageMap(entry.Pointer(), page_map_level - 1)) {
        return err;
      }
    }

    const auto entry_addr = reinterpret_cast<uintptr_t>(entry.Pointer());
    const FrameID map_frame{entry_addr / kBytesPerFrame};
    if (auto err = memory_manager->Free(map_frame, 1)) {
      return err;
    }
    page_map[i].data = 0;
  }
  return MAKE_ERROR(Error::kSuccess);
}

/**
 * @fn
 * CleanPageMaps関数
 * 
 * @brief 
 * アプリ用のページング構造を破棄する
 * PML4テーブルには、1つしかエントリがないことが前提のため注意
 * @param addr PML4のページング構造
 * @return Error 
 */
Error CleanPageMaps(LinearAddress4Level addr) {
  auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
  auto pdp_table = pml4_table[addr.parts.pml4].Pointer();
  pml4_table[addr.parts.pml4].data = 0;
  if (auto err = CleanPageMap(pdp_table, 3)) {
    return err;
  }

  const auto pdp_addr = reinterpret_cast<uintptr_t>(pdp_table);
  const FrameID pdp_frame{pdp_addr / kBytesPerFrame};
  return memory_manager->Free(pdp_frame, 1);
}

/**
 * @fn
 * InitializePaging関数
 * 
 * @brief
 * SetupIdentityPageTable関数のラッパー
 */
void InitializePaging() {
  SetupIdentityPageTable();
}
