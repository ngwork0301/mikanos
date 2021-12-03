/**
 * @file main.cpp
 *
 * カーネル本体のプログラムを書いたファイル．
 */

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <numeric>
#include <vector>

#include "asmfunc.h"
#include "console.hpp"
#include "font.hpp"
#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "interrupt.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "memory_map.hpp"
#include "memory_manager.hpp"
#include "mouse.hpp"
#include "paging.hpp"
#include "pci.hpp"
#include "queue.hpp"
#include "segment.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"

/**
 * @struct
 * Message構造体
 * 
 * @brief
 * キューに溜めるメッセージの形式を定義
 */
struct Message {
  enum Type {
    kInterrunptXHCI,
  } type;
};

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
//! コンソールクラスのインスタンス生成用バッファ
char console_buf[sizeof(Console)];
Console* console;
//! xHCI用ホストコントローラ
usb::xhci::Controller* xhc;
//! MSI割り込みイベント処理につかうキュー
ArrayQueue<Message>* main_queue;
//! BitmapMemoryManagerのインスタンス生成用バッファ
char memory_manager_buf[sizeof(BitmapMemoryManager)];
BitmapMemoryManager* memory_manager;
//! レイヤーマネージャ
LayerManager* layer_manager;

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
  main_queue->Push(Message{Message::kInterrunptXHCI});
  // 割り込み処理が終わったことを通知
  NotifyEndOfInterrupt();
}



/**
 * @fn
 * KernelMainNewStack関数
 * 
 * @brief
 * スタック領域を移動する処理を含んだ別のカーネルエントリポイント
 * @param [in] frame_buffer_config_ref FrameBufferConfig構造体への参照
 * @param [in] memory_map_ref UEFIのブートサービスが取得したメモリマップへの参照
 */
alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void KernelMainNewStack(
    const FrameBufferConfig& frame_buffer_config_ref,
    const MemoryMap& memory_map_ref) {

  // 新しいメモリ領域へ移動
  screen_config = frame_buffer_config_ref;
  MemoryMap memory_map{memory_map_ref};

  // ピクセルフォーマットを判定して、対応するPixelWriterインスタンスを生成
  PixelWriter* pixel_writer = MakeScreenWriter();
  
  // 背景の描画処理
  DrawDesktop(*pixel_writer);

  // コンソールを描画
  console = new(console_buf) Console{
    kDesktopFGColor, kDesktopBGColor
  };
  console->SetWriter(pixel_writer);
  printk("Welcome to MikanOS!\n");

  // ログレベルの設定
  SetLogLevel(kWarn);

  // セグメンテーションの設定のためGDT(Global Descriptor Table)を再構築する
  SetupSegments();
  // 再構築したGDTをCPUのセグメントレジスタに反映
  const uint16_t kernel_cs = 1 << 3;
  const uint64_t kernel_ss = 2 << 3;
  SetDSAll(0);
  SetCSSS(kernel_cs, kernel_ss);
  // ページジングの設定
  SetupIdentityPageTable();

  // メモリ管理の初期化処理
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

  // イベント処理のためのメインキューのインスタンス化
  std::array<Message, 32> main_queue_data;
  ArrayQueue<Message> main_queue{main_queue_data};
  ::main_queue = &main_queue;

  // PCIデバイスを列挙する
  auto err = pci::ScanAllBus();
  Log(kDebug, "ScanAllBus: %s\n", err.Name());
  for (int i = 0; i < pci::num_device; ++i) {
    const auto& dev = pci::devices[i];
    auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
    auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
    Log(kDebug, "%d.%d.%d: vend %04d, class(base) %d, class(sub) %d, class(interface) %d, head %02x\n",
          dev.bus, dev.device, dev.function,
          vendor_id, class_code.base, class_code.sub, class_code.interface, dev.header_type);
  }

  // 割り込み記述子IDTを設定
  // DPLは0固定で設定
  SetIDTEntry(idt[InterruptVector::kXHCI], MakeIDTAttr(DescriptorType::kInterruptGate, 0),
              reinterpret_cast<uint64_t>(IntHandlerXHCI), kernel_cs);
  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));

  // マウスデバイスを探し出してxhcへの共有ポインタを取得
  auto xhc = usb::xhci::MakeRunController();

  // スクリーンサイズを設定
  const auto screen_size = ScreenSize();

  // 背景ウィンドウを生成
  auto bgwindow = std::make_shared<Window>(
      screen_size.x, screen_size.y, screen_config.pixel_format);
  auto bgwriter = bgwindow->Writer();

  // 背景の描画処理
  DrawDesktop(*bgwriter);

  // メインウィンドウを作成
  auto main_window = std::make_shared<Window>(
      160, 52, screen_config.pixel_format);
  DrawWindow(*main_window->Writer(), "Hello Window");

 // コンソール用のウィンドウを生成
  auto console_window = std::make_shared<Window>(
      Console::kColumns * 8, Console::kRows * 16, screen_config.pixel_format);
  console->SetWindow(console_window);

  // メインウィンドウに表示するカウンタ変数を初期化
  char str[128];
  unsigned int count =0;

  // FrameBufferインスタンスの生成
  FrameBuffer screen;
  if (auto err = screen.Initialize(screen_config)) {
    Log(kError, "failed to initialize frame buffer: %s at %s:%d\n",
      err.Name(), err.File(), err.Line());
  }

  // レイヤーマネージャの生成
  layer_manager = new LayerManager;
  layer_manager->SetWriter(&screen);

  // Mouseインスタンスの生成
  auto mouse = MakeMouse();

  auto bglayer_id = layer_manager->NewLayer()
      .SetWindow(bgwindow)
      .Move({0,0})
      .ID();
  auto main_window_layer_id = layer_manager->NewLayer()
    .SetWindow(main_window)
    .SetDraggable(true)
    .Move({300, 100})
    .ID();
  console->SetLayerID(layer_manager->NewLayer()
      .SetWindow(console_window)
      .Move({0, 0})
      .ID());
  
  layer_manager->UpDown(bglayer_id, 0);
  layer_manager->UpDown(console->LayerID(), 1);
  layer_manager->UpDown(main_window_layer_id, 2);
  layer_manager->UpDown(mouse->LayerID(), 3);
  // 全体の描画
  layer_manager->Draw({{0, 0}, screen_size});

  // キューにたまったイベントを処理するイベントループ
  while(true) {
    // メインウィンドウに表示するカウンタ変数をインクリメント
    ++count;
    sprintf(str, "%010u", count);
    FillRectangle(*main_window->Writer(), {24, 28}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
    WriteString(*main_window->Writer(), {24, 28}, str, {0, 0, 0});
    layer_manager->Draw(main_window_layer_id);

    // cliオペランドで割り込みを一時的に受け取らないようにする
    __asm__("cli");
    // イベントがキューに溜まっていない場合は、割り込みを受け取る状態にして停止させる
    if (main_queue.Count() == 0) {
      // __asm__("sti\n\thlt");  // カウンタ変数のインクリメントを走らせるため、hltしない
      __asm__("sti");
      continue;
    }

    // キューからメッセージを１つ取り出し
    Message msg = main_queue.Front();
    main_queue.Pop();
    // MSI割り込み受け付けを再開
    __asm__("sti");

    switch (msg.type) {
      // XHCIからのマウスイベントの場合
      case Message::kInterrunptXHCI:
        usb::xhci::ProcessEvents(xhc);
        break;
      // どれにも該当しないイベント型だった場合
      default:
        Log(kError, "Unknown message type: %d\n", msg.type);
    }
  }

  while (1) __asm__("hlt");
}
