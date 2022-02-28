/**
 * @file interrupt.cpp
 *
 * 割り込み処理の実装をあつめたファイル
 */
#include "interrupt.hpp"

#include "asmfunc.h"
#include "font.hpp"
#include "graphics.hpp"
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
  // 20.3 で割り込みフレーム情報の伝搬のため、アセンブリ実装に移す
  // __attribute__((interrupt))
  // void IntHandlerLAPICTimer(InterruptFrame* frame) {
  //   // タイマー割り込み時の動作を呼び出す。割り込み処理の終了通知を呼び出し先でおこなう。
  //   LAPICTimerOnInterrupt();
  // }
}

/**
 * @fn PrintHex関数
 * @brief 
 * 指定された値を16進でコンソールに表示する
 * @param value 出力する対象の亜害
 * @param width 桁数
 * @param pos 出力するコンソールの位置
 */
void PrintHex(uint64_t value, int width, Vector2D<int> pos) {
  for (int i = 0; i < width; ++i) {
    int x = (value >> 4 * (width - i - 1)) & 0xfu;
    if (x >= 10) {
      x += 'a' - 10;
    } else {
      x += '0';
    }
    WriteAscii(*screen_writer, pos + Vector2D<int>{8 * i, 0}, x, {0, 0, 0});
  }
}

/**
 * @fn PrintFrame関数
 * @brief 
 * スタックフレームの情報を表示する
 * @param frame InterruptFrameのポインタ
 * @param exp_name 例外名
 */
void PrintFrame(InterruptFrame* frame, const char* exp_name) {
  WriteString(*screen_writer, {500, 16*0}, exp_name, {0, 0, 0});
  WriteString(*screen_writer, {500, 16*1}, "CS:RIP", {0, 0, 0});
  PrintHex(frame->cs, 4, {500 + 8*7, 16*1});
  PrintHex(frame->rip, 16, {500 + 8*12, 16*1});
  WriteString(*screen_writer, {500, 16*2}, "RFLAGS", {0, 0, 0});
  PrintHex(frame->rflags, 16, {500 + 8*7, 16*2});
  WriteString(*screen_writer, {500, 16*3}, "SS:RSP", {0, 0, 0});
  PrintHex(frame->ss, 4, {500 + 8*7, 16*3});
  PrintHex(frame->rsp, 16, {500 + 8*12, 16*3});
}

#define FaultHandlerWithError(fault_name) \
  __attribute__((interrupt)) \
  void IntHandler ## fault_name (InterruptFrame* frame, uint64_t error_code) { \
    PrintFrame(frame, "#" #fault_name); \
    WriteString(*screen_writer, {500, 16*4}, "ERR", {0,0,0}); \
    PrintHex(error_code, 16, {500 + 8*4, 16*4}); \
    while (true) __asm__("hlt"); \
  }

#define FaultHandlerNoError(fault_name) \
  __attribute__((interrupt)) \
  void IntHandler ## fault_name (InterruptFrame* frame) { \
    PrintFrame(frame, "#" #fault_name); \
    while (true) __asm__("hlt"); \
  }

/*
 * 各例外の内容は、ABI
 * [Intel 64 and IA-32 Architectures Software Developer's Manual Vol3「6.15 Exception and Interrupt Reference](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)」
 * を参照
 */ 
FaultHandlerNoError(DE)      // Interrupt 0—Divide Error Exception (#DE)
FaultHandlerNoError(DB)      // Interrupt 1—Debug Exception (#DB)
FaultHandlerNoError(BP)      // Interrupt 3—Breakpoint Exception (#BP)
FaultHandlerNoError(OF)      // Interrupt 4—Overflow Exception (#OF)
FaultHandlerNoError(BR)      // Interrupt 5—BOUND Range Exceeded Exception (#BR)
FaultHandlerNoError(UD)      // Interrupt 6—Invalid Opcode Exception (#UD)
FaultHandlerNoError(NM)      // Interrupt 7—Device Not Available Exception (#NM)
FaultHandlerWithError(DF)    // Interrupt 8—Double Fault Exception (#DF)
FaultHandlerWithError(TS)    // Interrupt 10—Invalid TSS Exception (#TS)
FaultHandlerWithError(NP)    // Interrupt 11—Segment Not Present (#NP)
FaultHandlerWithError(SS)    // Interrupt 12—Stack Fault Exception (#SS)
FaultHandlerWithError(GP)    // Interrupt 13—General Protection Exception (#GP)
FaultHandlerWithError(PF)    // Interrupt 14—Page-Fault Exception (#PF)
FaultHandlerNoError(MF)      // Interrupt 16—x87 FPU Floating-Point Error (#MF)
FaultHandlerWithError(AC)    // Interrupt 17—Alignment Check Exception (#AC)
FaultHandlerNoError(MC)      // Interrupt 18—Machine-Check Exception (#MC)
FaultHandlerNoError(XM)      // Interrupt 19—SIMD Floating-Point Exception (#XM)
FaultHandlerNoError(VE)      // Interrupt 20—Virtualization Exception (#VE)

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
  auto set_idt_entry = [](int irq, auto handler) {
    SetIDTEntry(idt[irq],
                MakeIDTAttr(DescriptorType::kInterruptGate, 0),
                reinterpret_cast<uint64_t>(handler),
                kKernelCS);
  };

  // xHCIマウスイベントの割り込み処理
  set_idt_entry(InterruptVector::kXHCI, IntHandlerXHCI);
  // タイマー割り込み処理
  // スタック領域の切り替えでIST1を使うように設定する
  SetIDTEntry(idt[InterruptVector::kLAPICTimer],
              MakeIDTAttr(DescriptorType::kInterruptGate, 0 /* DPL */,
                          true /* present */, kISTForTimer /* IST */),
              reinterpret_cast<uint64_t>(IntHandlerLAPICTimer),
              kKernelCS);
  set_idt_entry(InterruptVector::kLAPICTimer, IntHandlerLAPICTimer);
  // CPU例外のイベント
  set_idt_entry(0, IntHandlerDE);
  set_idt_entry(1, IntHandlerDB);
  set_idt_entry(3, IntHandlerBP);
  set_idt_entry(4, IntHandlerOF);
  set_idt_entry(5, IntHandlerBR);
  set_idt_entry(6, IntHandlerUD);
  set_idt_entry(7, IntHandlerNM);
  set_idt_entry(8, IntHandlerDF);
  set_idt_entry(10, IntHandlerTS);
  set_idt_entry(11, IntHandlerNP);
  set_idt_entry(12, IntHandlerSS);
  set_idt_entry(13, IntHandlerGP);   // 一般保護例外
  set_idt_entry(14, IntHandlerPF);   // ページフォルト
  set_idt_entry(16, IntHandlerMF);
  set_idt_entry(17, IntHandlerAC);
  set_idt_entry(18, IntHandlerMC);
  set_idt_entry(19, IntHandlerXM);
  set_idt_entry(20, IntHandlerVE);

  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));
}
