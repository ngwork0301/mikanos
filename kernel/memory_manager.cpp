#include <bitset>

#include "logger.hpp"
#include "memory_manager.hpp"

/**
 * @fn
 * BitmapMemoryManagerコンストラクタ
 */
BitmapMemoryManager::BitmapMemoryManager()
  : alloc_map_{}, range_begin_{FrameID{0}}, range_end_{FrameID{kFrameCount}} {
}

/**
 * @fn
 * Allocateメソッド
 * 
 * @brief
 * 空いているメモリ領域をファーストフィットアルゴリズムで探し出して、そのメモリを確保する
 * 
 * @param [in] num_frames 取得するサイズ（フレーム数)
 * @return WithError エラーコードと発生したフレームID
 */
WithError<FrameID> BitmapMemoryManager::Allocate(size_t num_frames) {
  size_t start_frame_id = range_begin_.ID();
  while (true) {
    size_t i = 0;
    for(; i < num_frames; ++i) {
      if (start_frame_id + i >= range_end_.ID()) {
        // メモリ管理がもつ範囲を超えたら、確保失敗
        return {kNullFrame, MAKE_ERROR(Error::kNoEnoughMemory)};
      }
      if (GetBit(FrameID{start_frame_id + i})) {
        // "start_frame_id + i" にあるフレームは割り当て済み
        break;
      }
    }
    if (i == num_frames) {
      // num_frames 分の空きが見つかった
      MarkAllocated(FrameID{start_frame_id}, num_frames);
      return {
        FrameID{start_frame_id},
        MAKE_ERROR(Error::kSuccess)
      };
    }
    // 次のフレームから再探索
    start_frame_id += i + 1;
  }
}

/**
 * @fn
 * Freeメソッド
 * 
 * @brief
 * メモリを解放する
 * 
 * @param [in] start_frame 解放対象の最初のフレーム
 * @param [in] num_frames 開放するサイズ（フレーム数）
 * @return Error
 */
Error BitmapMemoryManager::Free(FrameID start_frame, size_t num_frames) {
  for (size_t i = 0; i < num_frames; ++i) {
    SetBit(FrameID{start_frame.ID() + i}, false);
  }
  return MAKE_ERROR(Error::kSuccess);
}

/**
 * @fn
 * MarkAllocatedメソッド
 * 
 * @brief
 * 使用中領域を設定する
 * 
 * @param [in] start_frame 開始フレームの位置
 * @param [in] num_frames フレーム数
 */
void BitmapMemoryManager::MarkAllocated(FrameID start_frame, size_t num_frames) {
  for (size_t i = 0; i < num_frames; ++i) {
    SetBit(FrameID{start_frame.ID() + i}, true);
  }
}


/**
 * @fn
 * SetMemoryRangeメソッド
 * 
 * @brief このメモリマネージャで扱うメモリ範囲を設定する.
 * この呼び出し以降、Allocate によるメモリ割り当ては設定された範囲内でのみ行われる
 * 
 * @param [in] range_begin_ メモリ範囲の視点
 * @param [in] range_end_ メモリ範囲の終点。最終フレームの次のフレーム
 */
void BitmapMemoryManager::SetMemoryRange(FrameID range_begin, FrameID range_end){
  range_begin_ = range_begin;
  range_end_ = range_end;
}

/**
 * @fn
 * BitmapMemoryManager::Statメソッド
 * @brief 
 * その瞬間のメモリ状態を返す
 * @return MemoryStat構造体
 */
MemoryStat BitmapMemoryManager::Stat() const {
  size_t sum = 0;
  for (int i = range_begin_.ID() / kBitsPerMapLine;
       i < range_end_.ID() / kBitsPerMapLine; ++i) {
    sum += std::bitset<kBitsPerMapLine>(alloc_map_[i]).count();
  }
  return { sum, range_end_.ID() - range_begin_.ID() };
}

/**
 * @fn
 * GetBitメソッド
 * 
 * @brief
 * 指定したフレームのビットの読み出し
 * 
 * @param [in] frame 読み出し対象のフレームID
 */
bool BitmapMemoryManager::GetBit(FrameID frame) const {
  auto line_index = frame.ID() / kBitsPerMapLine;
  auto bit_index = frame.ID() % kBitsPerMapLine;

  return (alloc_map_[line_index] & (static_cast<MapLineType>(1) << bit_index)) != 0;
}

/**
 * @fn
 * SetBitメソッド
 * 
 * @brief
 * 使用中／未使用を設定する
 * 
 * @param [in] frame 設定対象のフレームID
 * @param [in] allocated trueは使用中。falseは未使用
 */
void BitmapMemoryManager::SetBit(FrameID frame, bool allocated) {
  auto line_index = frame.ID() / kBitsPerMapLine;
  auto bit_index = frame.ID() % kBitsPerMapLine;

  if (allocated) {
    alloc_map_[line_index] |= (static_cast<MapLineType>(1) << bit_index);
  } else {
    alloc_map_[line_index] &= ~(static_cast<MapLineType>(1) << bit_index);
  }
}

extern "C" caddr_t program_break, program_break_end;

namespace {
  char memory_manager_buf[sizeof(BitmapMemoryManager)];

  /**
   * @fn
   * InitializeHeap関数
   * 
   * @brief
   * プログラムブレークの初期化をおこなう
   * 
   * @param [in] memory_manager BitmapMemoryManagerのインスタンス
   */
  Error InitializeHeap(BitmapMemoryManager& memory_manager) {
    const int kHeapFrames = 64 * 512;
    const auto heap_start = memory_manager.Allocate(kHeapFrames);
    if (heap_start.error) {
      return heap_start.error;
    }

    program_break = reinterpret_cast<caddr_t>(heap_start.value.ID() * kBytesPerFrame);
    program_break_end = program_break + kHeapFrames * kBytesPerFrame;
    return MAKE_ERROR(Error::kSuccess);
  }
}

BitmapMemoryManager* memory_manager;

/**
 * @fn
 * InitializeMemoryManager関数
 * 
 * @brief
 * 引数で与えられたメモリマップから、MemoryManagerインスタンスを生成する
 * @param [in] memory_map メモリマップ
 */
void InitializeMemoryManager(const MemoryMap& memory_map){
  ::memory_manager = new(memory_manager_buf) BitmapMemoryManager;

  // 取得したメモリマップの情報を出力する
  // printk("memory_map: %p\n", &memory_map);
  const auto memory_map_base = reinterpret_cast<uintptr_t>(memory_map.buffer);
  uintptr_t available_end = 0;
  for (uintptr_t iter = memory_map_base;
      iter < memory_map_base + memory_map.map_size;
      iter += memory_map.descriptor_size) {
    // MemoryDescriptorごとに取り出し
    auto desc = reinterpret_cast<MemoryDescriptor*>(iter);

    // 未使用領域が物理メモリのスタート位置より小さい場合（＝最初の領域）は、使用中領域
    if (available_end < desc->physical_start) {
      // メモリ管理に使用中領域にすることを知らせる
      memory_manager->MarkAllocated(
          FrameID{available_end / kBytesPerFrame},
          (desc->physical_start - available_end) / kBytesPerFrame);
    }
    // ディスクリプタが持つ、物理メモリで使用できる最後のアドレスを計算
    const auto physical_end =
      desc->physical_start + desc->number_of_pages * kUEFIPageSize;
    if (IsAvailable(static_cast<MemoryType>(desc->type))) {
      // 未使用領域の場合
      // 未使用領域と物理領域を一致させる
      available_end = physical_end;

      // MemoryTypeごとに物理メモリアドレスやサイズ、ページ数、属性を出力
      // printk("type = %u, phys = %08lx - %08lx, pages = %lu, attr = %08lx\n",
      //        desc->type,
      //        desc->physical_start,
      //        desc->physical_start + desc->number_of_pages * 4096 -1,
      //        desc->number_of_pages,
      //        desc->attribute);

    } else {
      // 使用中領域の場合
      // メモリ管理に使用中領域にすることを知らせる
      memory_manager->MarkAllocated(
          FrameID{desc->physical_start / kBytesPerFrame},
          desc->number_of_pages * kUEFIPageSize / kBytesPerFrame);
    }
  }
  // メモリ管理に大きさを設定する。
  memory_manager->SetMemoryRange(FrameID{1}, FrameID{available_end / kBytesPerFrame});

  // mallocでつかうヒープ領域の初期化
  if (auto err = InitializeHeap(*memory_manager)) {
    Log(kError, "failed to allocate pages: %s at %s:%d\n",
        err.Name(), err.File(), err.Line());
    exit(1);
  }
}