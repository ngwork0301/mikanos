/**
 * @file main.cpp
 *
 * カーネル本体のプログラムを書いたファイル．
 */

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <deque>
#include <limits>
#include <numeric>
#include <vector>

#include "acpi.hpp"
#include "asmfunc.h"
#include "console.hpp"
#include "font.hpp"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "interrupt.hpp"
#include "keyboard.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "memory_map.hpp"
#include "memory_manager.hpp"
#include "message.hpp"
#include "mouse.hpp"
#include "paging.hpp"
#include "pci.hpp"
#include "segment.hpp"
#include "timer.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"

/**
 * 配置new演算子の定義
 *
void* operator new(size_t size, void* buf) noexcept {
  return buf;
}
*/

void operator delete(void* obj) noexcept {
}

/**
 * issue #1 純粋仮想関数の呼び出しの可能性があるとリンクエラーになるため定義をいれる
 */
extern "C" void __cxa_pure_virtual() { while (1); }

/**
 * グローバル変数
 */
//! メインウィンドウへの共有ポインタ
std::shared_ptr<Window> main_window;
//! メインウィンドウのID
unsigned int main_window_layer_id;
//! MSI割り込みイベント処理につかうキュー
std::deque<Message>* main_queue;

/**
 * @fn
 * printk関数
 * 
 * @brief
 * カーネル内部のメッセージ出力をおこなう
 * 
 * @param [in] format 書式
 * @param [in] args formatに埋める変数(可変長引数)
 */
int printk(const char* format, ...) {
  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  console->PutString(s);
  return result;
}

/**
 * @fn
 * InitializeMainWindow関数
 * 
 * @brief
 * メインウィンドウの初期化をおこなう
 */
void InitializeMainWindow() {
  main_window = std::make_shared<Window>(
      160, 52, screen_config.pixel_format);
  DrawWindow(*main_window->Writer(), "Hello Window");

  main_window_layer_id = layer_manager->NewLayer()
    .SetWindow(main_window)
    .SetDraggable(true)
    .Move({300, 100})
    .ID();

  layer_manager->UpDown(main_window_layer_id, std::numeric_limits<int>::max());
}

/**
 * @fn
 * KernelMainNewStack関数
 * 
 * @brief
 * スタック領域を移動する処理を含んだ別のカーネルエントリポイント
 * @param [in] frame_buffer_config_ref FrameBufferConfig構造体への参照
 * @param [in] memory_map_ref UEFIのブートサービスが取得したメモリマップへの参照
 * @param [in] acpi_table UEFIが取得したRSDP構造体へのポインタ
 */
alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void KernelMainNewStack(
    const FrameBufferConfig& frame_buffer_config_ref,
    const MemoryMap& memory_map_ref,
    const acpi::RSDP& acpi_table) {

  // 新しいメモリ領域へ移動
  MemoryMap memory_map{memory_map_ref};

  // 画面の初期化
  InitializeGraphics(frame_buffer_config_ref);
  // コンソールを描画
  InitializeConsole();
  printk("Welcome to MikanOS!\n");

  // ログレベルの設定
  SetLogLevel(kWarn);

  // セグメンテーションの設定
  InitializeSegmentation();
  // ページジングの設定
  InitializePaging();

  // メモリ管理の初期化
  InitializeMemoryManager(memory_map);

  // イベント処理のためのメインキューのインスタンス化
  ::main_queue = new std::deque<Message>(32);

  // 割り込み処理を初期化
  InitializeInterrupt(main_queue);

  // PCIバスをスキャンしてデバイスをロードする。
  InitializePCI();
  // xHCIマウスデバイスを探し出して初期化
  usb::xhci::Initialize();

  // layer_managerの初期化とデスクトップ背景、コンソールなど初期レイヤーの初期化
  InitializeLayer();
  // メインウィンドウの初期化と描画
  InitializeMainWindow();
  // マウスウィンドウの初期化と描画
  InitializeMouse();

  // 全体の描画
  layer_manager->Draw({{0, 0}, ScreenSize()});

  // タイマー割り込み処理の初期化
  acpi::Initialize(acpi_table);
  InitializeLAPICTimer(*main_queue);

  // キーボードの初期化
  InitializeKeyboard(*main_queue);

  // 適当に200と600カウントしたらタイムアウトするタイマーを追加
  // timer_manager->AddTimer(Timer(200, 2));
  // timer_manager->AddTimer(Timer(600, -1));

  // メインウィンドウに表示するカウンタ変数を初期化
  char str[128];

  // キューにたまったイベントを処理するイベントループ
  while(true) {
    // cliオペランドで割り込みを一時的に受け取らないようにする
    __asm__("cli");
    const auto tick = timer_manager->CurrentTick();
    __asm__("sti");

    // メインウィンドウに表示するカウンタ変数を表示
    sprintf(str, "%010lu", tick);
    FillRectangle(*main_window->Writer(), {24, 28}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
    WriteString(*main_window->Writer(), {24, 28}, str, {0, 0, 0});
    layer_manager->Draw(main_window_layer_id);

    // cliオペランドで割り込みを一時的に受け取らないようにする
    __asm__("cli");
    // イベントがキューに溜まっていない場合は、割り込みを受け取る状態にして停止させる
    if (main_queue->size() == 0) {
      __asm__("sti\n\thlt");
      continue;
    }

    // キューからメッセージを１つ取り出し
    Message msg = main_queue->front();
    main_queue->pop_front();
    // MSI割り込み受け付けを再開
    __asm__("sti");

    switch (msg.type) {
      // XHCIからのマウスイベントの場合
      case Message::kInterruptXHCI:
        usb::xhci::ProcessEvents();
        break;
      // タイマー割り込みイベントの場合
      case Message::kInterruptLAPICTimer:
        printk("Timer interrupt\n");
        break;
      // タイマーがタイムアウトしたときのイベント
      case Message::kTimerTimeout:
        // printk("Timer: timeout = %lu, value = %d\n",
        //     msg.arg.timer.timeout, msg.arg.timer.value);
        // if (msg.arg.timer.value > 0) {
        //   // valueが0より大きい場合、さらに100を加えたものをタイムアウトにしたタイマーを追加する
        //   timer_manager->AddTimer(Timer(
        //       msg.arg.timer.timeout + 100, msg.arg.timer.value + 1));
        // }
        break;
      // キーボード入力イベントの場合
      case Message::kKeyPush:
        if (msg.arg.keyboard.ascii != 0) {
          printk("%c", msg.arg.keyboard.ascii);
        }
        break;
      // どれにも該当しないイベント型だった場合
      default:
        Log(kError, "Unknown message type: %d\n", msg.type);
    }
  }

  while (1) __asm__("hlt");
}
