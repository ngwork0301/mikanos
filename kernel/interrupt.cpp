/**
 * @file interrupt.cpp
 *
 * 割り込み処理の実装をあつめたファイル
 */
#include "interrupt.hpp"

//! IDT（割り込み記述子テーブル：Interrupt descriptor table）
std::array<InterruptDescriptor, 256> idt;

/**
 * @fn
 * SetIDTEntry関数
 * 
 * @brief
 * IDTにエントリを書き込む
 * @param [in] desc 書き込むIDT構造体
 * @param [in] attr IDT属性
 * @param [in] offset 割り込みハンドラのアドレス
 * @param [in] segment_selector コードセグメント
 */
void SetIDTEntry(InterruptDescriptor& desc,
                 InterruptDescriptorAttribute attr,
                 uint64_t offset,
                 uint16_t segment_selector) {
  desc.attr = attr;
  desc.offset_low = offset & 0xffffu;
  desc.offset_middle = (offset >> 16) & 0xffffu;
  desc.offset_high = offset >> 32;
  desc.segment_selector = segment_selector;
}

/**
 * @fn
 * NotifyEndOfInterrupt関数
 * 
 * @brief
 * 割り込み処理が終わったことを通知
 */
void NotifyEndOfInterrupt() {
  // EOI(End of Interruptレジスタ(メモリの0xfee000b0番地))に0を書き込む
  volatile auto end_of_interrupt = reinterpret_cast<uint32_t*>(0xfee000b0);
  *end_of_interrupt = 0;
}
