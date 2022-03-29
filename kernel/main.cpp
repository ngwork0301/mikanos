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
#include "fat.hpp"
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
#include "syscall.hpp"
#include "task.hpp"
#include "terminal.hpp"
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
std::shared_ptr<ToplevelWindow> main_window;
//! メインウィンドウのID
unsigned int main_window_layer_id;
//! テキストボックスへの共有ポインタ
std::shared_ptr<ToplevelWindow> text_window;
//! テキストボックスのID
unsigned int text_window_layer_id;
//! テキストボックス内のインデックス
int text_window_index;

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
  main_window = std::make_shared<ToplevelWindow>(
      160, 52, screen_config.pixel_format, "Hello Window");

  main_window_layer_id = layer_manager->NewLayer()
    .SetWindow(main_window)
    .SetDraggable(true)
    .Move({300, 100})
    .ID();

  layer_manager->UpDown(main_window_layer_id, std::numeric_limits<int>::max());
}

/**
 * @fn
 * InitializeTextWindow関数
 * 
 * @brief 
 * テキストボックスを初期化する。
 */
void InitializeTextWindow() {
  //! ウィンドウの横幅
  const int win_w = 160;
  //! ウィンドウの高さ
  const int win_h = 52;

  text_window = std::make_shared<ToplevelWindow>(
      win_w, win_h, screen_config.pixel_format, "Text Box Test");
  DrawTextbox(*text_window->InnerWriter(), {0, 0}, text_window->InnerSize());

  // レイヤーを生成
  text_window_layer_id = layer_manager->NewLayer()
    .SetWindow(text_window)
    .SetDraggable(true)
    .Move({500, 100})
    .ID();

  // レイヤーマネージャにレイヤーを追加して描画
  layer_manager->UpDown(text_window_layer_id, std::numeric_limits<int>::max());
}

/**
 * @fn
 * DrawTextCursor関数
 * 
 * @brief 
 * テキストボックス内のカーソルの表示・非表示を切り替える。
 * 表示の場合は、黒。非表示の場合は白で塗りつぶし。
 * @param [in] visible 表示=true, 非表示=false 
 */
void DrawTextCursor(bool visible) {
  const auto color = visible ? ToColor(0) : ToColor(0xffffff);
  const auto pos = Vector2D<int>{4 + 8*text_window_index, 5};
  FillRectangle(*text_window->InnerWriter(), pos, {7, 15}, color);
}

/**
 * @fn
 * InputTextWindow関数
 * 
 * @brief 
 * 受け取った文字をテキストボックス末尾に追加
 * @param [in] c 描画する文字
 */
void InputTextWindow(char c) {
  if (c == 0) {
    return;
  }

  // 現在位置を返すラムダ式
  auto pos = []() { return Vector2D<int>{4 + 8*text_window_index, 6}; };

  //! 最大文字数
  const int max_chars = (text_window->InnerSize().x - 8) / 8 - 1;

  // バックスペースキーがきたら文字を消してインデックスを下げる
  if (c == '\b' && text_window_index > 0) {
    // 一時的にカーソルは非表示
    DrawTextCursor(false);
    --text_window_index;
    FillRectangle(*text_window->InnerWriter(), pos(), {8, 16}, ToColor(0xffffff));
    DrawTextCursor(true);
  // 通常の文字が入力されたとき、文字を描画して、インデックスを上げる
  } else if(c >= ' ' && text_window_index < max_chars) {
    // 一時的にカーソルを非表示
    DrawTextCursor(false);
    WriteAscii(*text_window->InnerWriter(), pos(), c, ToColor(0));
    ++text_window_index;
    DrawTextCursor(true);
  }
  // レイヤーの再描画
  layer_manager->Draw(text_window_layer_id);
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
 * @param [in] volume_image UEFIが読み取ったFAT形式のボリュームイメージ
 */
alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void KernelMainNewStack(
    const FrameBufferConfig& frame_buffer_config_ref,
    const MemoryMap& memory_map_ref,
    const acpi::RSDP& acpi_table,
    void* volume_image) {

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

  // 割り込み用のスタック領域(TSS)を初期化
  InitializeTSS();

  // 割り込み処理を初期化
  InitializeInterrupt();

  // FATボリュームの読み取り
  fat::Initialize(volume_image);

  // PCIバスをスキャンしてデバイスをロードする。
  InitializePCI();

  // layer_managerの初期化とデスクトップ背景、コンソールなど初期レイヤーの初期化
  InitializeLayer();
  // メインウィンドウの初期化と描画
  InitializeMainWindow();
  // テキストボックスの初期化と描画
  InitializeTextWindow();

  // 全体の描画
  layer_manager->Draw({{0, 0}, ScreenSize()});
  active_layer->Activate(main_window_layer_id);
  active_layer->Activate(text_window_layer_id);

  // タイマー割り込み処理の初期化
  acpi::Initialize(acpi_table);
  InitializeLAPICTimer();


  // 0.5秒でカーソルを点滅させる
  //! カーソル点滅のためのタイマーであることをしめす値としていれておく
  const int kTextboxCursorTimer = 1;
  const int kTimer05Sec = static_cast<int>(kTimerFreq * 0.5);
  timer_manager->AddTimer(Timer{kTimer05Sec, kTextboxCursorTimer, 1});
  bool textbox_cursor_visible = false;

  // 上位アプリがシステムコールを呼び出せるように初期化
  InitializeSyscall();

  // タスクタイマーの初期化。
  // 呼び出し直後からタスク切換えが発生するため、他の初期化処理完了後に呼び出すこと
  InitializeTask();
  Task& main_task = task_manager->CurrentTask();

  // ターミナルとタスクのマッピングを初期化
  terminals = new std::map<uint64_t, Terminal*>;
  // ターミナル用タスクを生成して起床させる
  const uint64_t task_terminal_id = task_manager->NewTask()
    .InitContext(TaskTerminal, 0)
    .Wakeup()
    .ID();

  // 以降の割り込みがあるものの初期化は、タスク機能の初期化がおわってから
  // xHCIマウスデバイスを探し出して初期化
  usb::xhci::Initialize();
  // マウスウィンドウの初期化と描画
  InitializeMouse();
  // キーボードの初期化
  InitializeKeyboard();

  // ボリュームイメージの中身ををバイナリ形式で表示
  // uint8_t* p = reinterpret_cast<uint8_t*>(volume_image);
  // printk("Volume Image:\n");
  // for (int i = 0; i < 16; ++i) {
  //   printk("%04x:", i * 16);  // ボリュームエントリからのオフセットの位置を出力
  //   for (int j = 0; j < 8; ++j) {
  //     printk(" %02x", *p);  // 2桁の16進数 = 32bit分ずつを8回出力
  //     ++p;
  //   }
  //   printk(" "); // みやすさのため、区切りとしてスペースを入れる
  //   for (int j = 0; j < 8; ++j) {
  //     printk(" %02x", *p);  // 2桁の16進数 = 32bit分ずつを8回出力
  //     ++p;
  //   }
  //   printk("\n");  // 8バイト分のデータを出力したら、読みやすさのため、改行
  // }

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
    FillRectangle(*main_window->InnerWriter(), {20, 4}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
    WriteString(*main_window->InnerWriter(), {20, 4}, str, {0, 0, 0});
    layer_manager->Draw(main_window_layer_id);

    // cliオペランドで割り込みを一時的に受け取らないようにする
    __asm__("cli");
    // イベントがキューに溜まっていない場合は、割り込みを受け取る状態にして停止させる
    auto msg = main_task.ReceiveMessage();
    if (!msg) {
      main_task.Sleep();
      __asm__("sti");
      continue;
    }

    // MSI割り込み受け付けを再開
    __asm__("sti");

    switch (msg->type) {
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
        if (msg->arg.timer.value  == kTextboxCursorTimer) {
          __asm__("cli");  // 割り込み禁止
          timer_manager->AddTimer(
              Timer{msg->arg.timer.timeout + kTimer05Sec, kTextboxCursorTimer, 1});
          __asm__("sti"); // 割り込み許可
          // 点滅のため、割り込みのたびに反転
          textbox_cursor_visible = !textbox_cursor_visible;
          DrawTextCursor(textbox_cursor_visible);
          layer_manager->Draw(text_window_layer_id);

          // ターミナルタスクのカーソル点滅処理を呼び出し
          __asm__("cli"); // 割り込み禁止
          task_manager->SendMessage(task_terminal_id, *msg);
          __asm__("sti"); // 割り込み禁止
        }
        break;
      // キーボード入力イベントの場合
      case Message::kKeyPush:
        if (auto act = active_layer->GetActive(); act == text_window_layer_id) {
          if (msg->arg.keyboard.press) {
            // アクティブウィンドウがテキストボックスであれば、その内に文字列を描画
            InputTextWindow(msg->arg.keyboard.ascii);
          }
        } else {
          // その他のウィンドウがアクティブの場合は、そのレイヤーにkKeyPushイベントを送る
          __asm__("cli"); //割り込み禁止
          auto task_it = layer_task_map->find(act);
          __asm__("sti"); //割り込み許可
          if (task_it != layer_task_map->end()) {
            __asm__("cli"); //割り込み禁止
            task_manager->SendMessage(task_it->second, *msg);
            __asm__("sti"); //割り込み許可
          } else {
            printk("key push not handled: keycode %02x, ascii %02x\n",
                 msg->arg.keyboard.keycode,
                 msg->arg.keyboard.ascii);
          }
        }
        break;
      // レイヤー操作イベントの場合
      case Message::kLayer:
        ProcessLayerMessage(*msg);
        __asm__("cli");  // 割り込み禁止
        // Layer操作が終了したことを呼び出し元のタスクに通知
        task_manager->SendMessage(msg->src_task, Message{Message::kLayerFinish});
        __asm__("sti");  // 割り込み許可
        break;
      // どれにも該当しないイベント型だった場合
      default:
        Log(kError, "Unknown message type: %d\n", msg->type);
    }
  }

  while (1) __asm__("hlt");
}
