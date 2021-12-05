/**
 * @file interrupt.cpp
 *
 * 割り込み処理の実装をあつめたファイル
 */
#include "interrupt.hpp"

#include "asmfunc.h"
#include "segment.hpp"

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

namespace {
  std::deque<Message>* msg_queue;

  /**
   * @fn
   * InHandlerXHCI関数
   * 
   * @brief
   * xHCI用割り込みハンドラの定義
   * 
   * @param [in] frame InterruptFrame(未使用)
   */
  __attribute__((interrupt))
  void IntHandlerXHCI(InterruptFrame* frame) {
    // イベントをキューに溜める
    msg_queue->push_back(Message{Message::kInterruptXHCI});
    // 割り込み処理が終わったことを通知
    NotifyEndOfInterrupt();
  }
}

/**
 * @fn
 * InitializeInterrupt関数
 * 
 * @brief
 * 割り込み処理を初期化する
 */
void InitializeInterrupt(std::deque<Message>* msg_queue) {
  ::msg_queue = msg_queue;

  // 割り込み記述子IDTを設定
  // DPLは0固定で設定
  SetIDTEntry(idt[InterruptVector::kXHCI], 
              MakeIDTAttr(DescriptorType::kInterruptGate, 0),
              reinterpret_cast<uint64_t>(IntHandlerXHCI),
              kKernelCS);
  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));
}
