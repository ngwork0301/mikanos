
#include "segment.hpp"

#include "asmfunc.h"
#include "error.hpp"
#include "logger.hpp"
#include "memory_manager.hpp"

namespace {
  std::array<SegmentDescriptor, 7> gdt;
  std::array<uint32_t, 26> tss;

  static_assert((kTSS >> 3) + 1 < gdt.size());
}

/**
 * @fn
 * SetCodeSegment関数
 * 
 * @brief
 * GDTのコードセグメントをセットアップする
 * 
 * @param [in] desc セグメントディスクリプタ
 * @param [in] type セグメントタイプ
 * @param [in] descriptor_privilege_level ディスクリプタ権限
 * @param [in, out] base メモリのペースアドレス
 * @param [in] limit 書き込むメモリの上限
 */
void SetCodeSegment(SegmentDescriptor& desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit) {
  desc.data = 0;

  desc.bits.base_low = base & 0xffffu;
  desc.bits.base_middle = (base >> 16) & 0xffu;
  desc.bits.base_high = (base >> 24) & 0xffu;

  desc.bits.limit_low = limit & 0xfffu;
  desc.bits.limit_high = (limit >> 16) & 0xfu;

  desc.bits.type = type;
  desc.bits.system_segment = 1; // 1: code & data segment
  desc.bits.descriptor_privilege_level = descriptor_privilege_level;
  desc.bits.present = 1;
  desc.bits.available = 0;
  desc.bits.long_mode = 1;
  desc.bits.default_operation_size = 0; // should be 0 when long_mode = 1
  desc.bits.granularity = 1;
}

/**
 * @fn
 * SetDataSegment関数
 * 
 * @brief
 * GDTのデータセグメントをセットアップする
 * @param [in] desc セグメントディスクリプタ
 * @param [in] type セグメントタイプ
 * @param [in] descriptor_privilege_level ディスクリプタ権限
 * @param [in, out] base メモリのペースアドレス
 * @param [in] limit 書き込むメモリの上限
 */
void SetDataSegment(SegmentDescriptor& desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit) {
  SetCodeSegment(desc, type, descriptor_privilege_level, base, limit);
  desc.bits.long_mode = 0;
  desc.bits.default_operation_size = 1; // 32-bit stack segment
}

/**
 * @fn
 * SetupSegments関数
 * 
 * @brief
 * GDT(Global Descriptor table)を構築して、ロードする
 */
void SetupSegments() {
  // GDTの1つめのディスクリプタは、CPUの仕様上ヌルディスクリプタ（8バイト分0）にする
  gdt[0].data = 0;
  // DPLがリング0=OS本体がつかうセグメントを設定
  // GDTの2つめのディスクリプタはコードセグメントディスクリプタ
  SetCodeSegment(gdt[1], DescriptorType::kExecuteRead, 0, 0, 0xfffff);
  // GDTの3つめのディスクリプタはデータセグメントディスクリプタ
  SetDataSegment(gdt[2], DescriptorType::kReadWrite, 0, 0, 0xfffff);
  // DPLがリング3=アプリケーションがつかうセグメントを設定
  // sysretで戻ってくるときのために、データセグメントを3、コードセグメントを4にしておく
  // データセグメントディスクリプタ＝読み書き可能
  SetDataSegment(gdt[3], DescriptorType::kReadWrite, 3, 0, 0xfffff);
  // コードセグメントディスクリプタ＝読み込み・実行可能
  SetCodeSegment(gdt[4], DescriptorType::kExecuteRead, 3, 0, 0xfffff);
  
  // 作成したGDTをCPUに登録(既存のGDTは破棄する)
  LoadGDT(sizeof(gdt) - 1, reinterpret_cast<uintptr_t>(&gdt[0]));
}

/**
 * @fn
 * SetSystemSegment関数
 * @brief Set the System Segment object
 * システムセグメントを設定する。
 * @param [in] desc セグメントディスクリプタ
 * @param [in] type セグメントタイプ
 * @param [in] descriptor_privilege_level ディスクリプタ権限
 * @param [in, out] base メモリのペースアドレス
 * @param [in] limit 書き込むメモリの上限
 */
void SetSystemSegment(SegmentDescriptor& desc,
                      DescriptorType type,
                      unsigned int descriptor_privilege_level,
                      uint32_t base,
                      uint32_t limit) {
  SetCodeSegment(desc, type, descriptor_privilege_level, base, limit);
  desc.bits.system_segment = 0;  // わかりにくいがシステムセグメントのときはビットフラグを0
  desc.bits.long_mode = 0;
}

/**
 * @fn
 * InitializeTSS関数
 * @brief 
 * TSSを初期化してGDTに設定する
 */
void InitializeTSS() {
  // CPUが割り込み時に積む40バイト分のInterruptFrameのメモリ領域を確保
  const int kRSP0Frames = 8;
  auto [ stack0, err ] = memory_manager->Allocate(kRSP0Frames);
  if (err) {
    Log(kError, "failed to allocate rsp0: %s\n", err.Name());
    exit(1);
  }
  // スタック領域なので、確保した領域の末尾のアドレスを入れる
  uint64_t rsp0 =
      reinterpret_cast<uint64_t>(stack0.Frame()) + kRSP0Frames * 4096;
  // TSS構造体は、先頭4バイト分予約されていて、8バイト区切りが4バイト分ずれるので分割
  tss[1] = rsp0 & 0xffffffff;  // 下位4バイト
  tss[2] = rsp0 >> 32;         // 上位4バイト

  uint64_t tss_addr = reinterpret_cast<uint64_t>(&tss[0]);
  // GDTの5つ目のエントリとしてTSSのコードセグメントを登録
  SetSystemSegment(gdt[kTSS >> 3], DescriptorType::kTSSAvailable, 0,
                   tss_addr & 0xffffffff, sizeof(tss)-1);
  // GDTの6つ目のエントリとして、TSSのデータセグメントを登録
  gdt[(kTSS >> 3) + 1].data = tss_addr >> 32;

  LoadTR(kTSS);
}


/**
 * @fn
 * InitializeSegmentation関数
 * 
 * @brief
 * GDT(Global Descriptor table)の構築・ロードと、DS、CS、SSレジスタの設定をおこなう。
 */
void InitializeSegmentation() {
  // GDT(Global Descriptor table)の構築・ロード
  SetupSegments();

  // 再構築したGDTをCPUのセグメントレジスタに反映
  SetDSAll(kKernelDS);
  SetCSSS(kKernelCS, kKernelSS);
}

