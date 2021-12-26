/**
 * @file interrupt.cpp
 *
 * 割り込み処理の実装をあつめたファイル
 */
#include "interrupt.hpp"

#include "asmfunc.h"
#include "segment.hpp"
#include "task.hpp"
#include "timer.hpp"

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
    // メインタスク(タスクID=1)のイベントキューにメッセージを送る
    task_manager->SendMessage(1, Message{Message::kInterruptXHCI});
    // 割り込み処理が終わったことを通知
    NotifyEndOfInterrupt();
  }

  /**
   * @fn
   * IntHandlerLAPICTimer関数
   * 
   * @brief
   * LAPIC割り込みハンドラの定義
   * @param [in] frame InterruptFrame(未使用)
   */
  __attribute__((interrupt))
  void IntHandlerLAPICTimer(InterruptFrame* frame) {
    // タイマー割り込み時の動作を呼び出す。割り込み処理の終了通知を呼び出し先でおこなう。
    LAPICTimerOnInterrupt();
  }
}

/**
 * @fn
 * InitializeInterrupt関数
 * 
 * @brief
 * 割り込み処理を初期化する
 */
void InitializeInterrupt() {

  // 割り込み記述子IDTを設定
  // DPLは0固定で設定
  // xHCIマウスイベントの割り込み処理
  SetIDTEntry(idt[InterruptVector::kXHCI], 
              MakeIDTAttr(DescriptorType::kInterruptGate, 0),
              reinterpret_cast<uint64_t>(IntHandlerXHCI),
              kKernelCS);
  // タイマー割り込み処理
  SetIDTEntry(idt[InterruptVector::kLAPICTimer],
              MakeIDTAttr(DescriptorType::kInterruptGate, 0),
              reinterpret_cast<uint64_t>(IntHandlerLAPICTimer),
              kKernelCS);
  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));
}
