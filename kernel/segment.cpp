
#include "segment.hpp"

#include "asmfunc.h"

namespace {
  std::array<SegmentDescriptor, 5> gdt;
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
  // コードセグメントディスクリプタ＝読み込み・実行可能
  SetCodeSegment(gdt[3], DescriptorType::kExecuteRead, 3, 0, 0xfffff);
  // データセグメントディスクリプタ＝読み書き可能
  SetDataSegment(gdt[4], DescriptorType::kReadWrite, 3, 0, 0xfffff);
  
  // 作成したGDTをCPUに登録(既存のGDTは破棄する)
  LoadGDT(sizeof(gdt) - 1, reinterpret_cast<uintptr_t>(&gdt[0]));
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
