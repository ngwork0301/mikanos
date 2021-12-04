/**
 * @file segment.hpp
 *
 * セグメンテーション用のプログラムを集めたファイル．
 */

#pragma once

#include <array>
#include <cstdint>

#include "x86_descriptor.hpp"

/**
 * @union
 * SegmentDescriptor共用体
 * 
 * @brief
 * メモリ管理のセグメンテーションの設定につかう
 * GDT(Global Descritor Table)内のディスクリプタ構造をを定義する共用体(構造体)
 */
union SegmentDescriptor {
  uint64_t data;
  struct {
    uint64_t limit_low : 16;
    uint64_t base_low : 16;
    uint64_t base_middle : 8;
    DescriptorType type : 4;                  //! ディスクリプタタイプ
    uint64_t system_segment : 1;              //! 1ならコードまたはデータセグメント
    uint64_t descriptor_privilege_level : 2;  //! ディスクリプタの権限レベル
    uint64_t present : 1;                     //! 1ならディスクリプタが有効
    uint64_t limit_high : 4;
    uint64_t available : 1;                   //! OSが自由に使って良いビット
    uint64_t long_mode : 1;                   //! 1なら64ビットモード用のコードセグメント
    uint64_t default_operation_size : 1;      //! long_modeが1なら必ず0とする
    uint64_t granularity : 1;                 //! 1ならリミットを4KiB単位として解釈する
    uint64_t base_high : 8;
  } __attribute__((packed)) bits;
} __attribute__((packed));

void SetCodeSegment(SegmentDescriptor& desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit);
void SetDataSegment(SegmentDescriptor& desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit);

const uint16_t kKernelCS = 1 << 3;
const uint16_t kKernelSS = 2 << 3;
const uint16_t kKernelDS = 0;

void SetupSegments();
void InitializeSegmentation();
