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
#include "logger.hpp"
#include "memory_map.hpp"
#include "mouse.hpp"
#include "pci.hpp"
#include "queue.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/mouse.hpp"
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
//! PixelWriterのインスタンス生成用バッファ
char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;
//! コンソールクラスのインスタンス生成用バッファ
char console_buf[sizeof(Console)];
Console* console;
//! デスクトップの背景色とコンソール文字色
const PixelColor kDesktopBGColor{45, 118, 237};
const PixelColor kDesktopFGColor{255, 255, 255};
//! マウスカーソルのインスタンス生成用バッファ
char mouse_cursor_buf[sizeof(MouseCursor)];
MouseCursor* mouse_cursor;
//! xHCI用ホストコントローラ
usb::xhci::Controller* xhc;
//! MSI割り込みイベント処理につかうキュー
ArrayQueue<Message>* main_queue;

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
 * SwitchEhci2hci関数
 * 
 * @brief
 * EHCIからxHCIへ制御モードを切り替える
 * 
 * @param [in] xhc_dev デバイス
 */
void SwitchEhci2Xhci(const pci::Device& xhc_dev) {
  bool intel_ehc_exist = false;
  for (int i = 0; i < pci::num_device; ++i) {
    if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x20u) /* EHCI */ &&
        0x8086 == pci::ReadVendorId(pci::devices[i])) {
      intel_ehc_exist = true;
      break;
    }
  }
  if (!intel_ehc_exist) {
    return;
  }

  uint32_t superspeed_ports = pci::ReadConfReg(xhc_dev, 0xdc); // USB3PRM
  pci::WriteConfReg(xhc_dev, 0xd8, superspeed_ports); // USB3_PSSEN
  uint32_t ehci2xhci_ports = pci::ReadConfReg(xhc_dev, 0xd4); // XUSB2PRM
  pci::WriteConfReg(xhc_dev, 0xd0, ehci2xhci_ports); // XUSB2PR
  Log(kDebug, "SwitchEhci2Xhci: SS = %02, xHCI = %02x\n",
      superspeed_ports, ehci2xhci_ports);
}

/**
 * @fn
 * MouseObserver関数
 * 
 * @brief
 * マウスのオブザーバーオブジェクトを生成してmouse_cursorに設定する
 * @param [in] x X座標の移動量
 * @param [in] y Y座標の移動量
 */
void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
  mouse_cursor->MoveRelative({displacement_x, displacement_y});
}

/**
 * @fn
 * KernelMain関数
 * 
 * @brief
 * カーネルのエントリポイント
 * 
 * @param [in] frame_buffer_config FrameBufferConfig構造体
 * @param [in] memory_map UEFIのブートサービスが取得したメモリマップ
 */
extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config,
                           const MemoryMap& memory_map) {
  // ログレベルの設定
  SetLogLevel(kWarn);

  // ピクセルフォーマットを判定して、対応するPixelWriterインスタンスを生成
  switch (frame_buffer_config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf)
        RGBResv8BitPerColorPixelWriter{frame_buffer_config};
      break;
    case kPixelBGRResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf)
        BGRResv8BitPerColorPixelWriter{frame_buffer_config};
      break;
  }
  // 画面一杯をフレームサイズにする
  const int kFrameWidth = frame_buffer_config.horizontal_resolution;
  const int kFrameHeight = frame_buffer_config.vertical_resolution;

  // デスクトップの背景色を描画
  FillRectangle(*pixel_writer,
                {0,0},
                {kFrameWidth, kFrameHeight - 50},
                kDesktopBGColor);
  // タスクバーを描画
  FillRectangle(*pixel_writer,
                {0, kFrameHeight - 50},
                {kFrameWidth, 50},
                {1, 8, 17});
  // メニューバーのスタートメニューを描画
  FillRectangle(*pixel_writer,
                {0, kFrameHeight - 50},
                {kFrameWidth / 5, 50},
                {80, 80, 80});
  // メニューボタンの枠を描画
  DrawRectangle(*pixel_writer,
                {10, kFrameHeight - 40},
                {30, 30},
                {160, 160, 160});
  
  // コンソールを描画
  console = new(console_buf) Console{
    *pixel_writer, kDesktopFGColor, kDesktopBGColor
  };
  printk("Welcome to MikanOS!\n");

  // 取得したメモリマップの情報を出力する
  printk("memory_map: %p\n", &memory_map);
  for (uintptr_t iter = reinterpret_cast<uintptr_t>(memory_map.buffer);
       iter < reinterpret_cast<uintptr_t>(memory_map.buffer) + memory_map.map_size;
        iter += memory_map.descriptor_size) {
    // MemoryDescriptorごとに取り出し
    auto desc = reinterpret_cast<MemoryDescriptor*>(iter);

    // 以下のMemoryTypeの中身をみてみる
    const std::array available_memory_types{
        MemoryType::kEfiBootServicesCode,
        MemoryType::kEfiBootServicesData,
        MemoryType::kEfiConventionalMemory,
      };
    for (int i = 0; i < available_memory_types.size(); ++i) {
      // MemoryTypeごとに物理メモリアドレスやサイズ、ページ数、属性を出力
      if (desc->type == available_memory_types[i]) {
        printk("type = %u, phys = %08lx - %08lx, pages = %lu, attr = %08lx\n",
               desc->type,
               desc->physical_start,
               desc->physical_start + desc->number_of_pages * 4096 -1,
               desc->number_of_pages,
               desc->attribute);
      }
    }
  }

  // マウスカーソルのインスタンス化
  mouse_cursor = new(mouse_cursor_buf) MouseCursor{
    pixel_writer, kDesktopBGColor, {300, 200}
  };

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

  // Intel製を優先して xHCのデバイスを探す
  pci::Device* xhc_dev = nullptr;
  for (int i = 0; i< pci::num_device; ++i) {
    if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x30u)) {
      xhc_dev = &pci::devices[i];

      // Vendor ID が 0x8086は Intel
      if (0x8086 == pci::ReadVendorId(*xhc_dev)) {
        break;
      }
    }

    if (xhc_dev) {
      Log(kInfo, "xHC has been found: %d.%d.%d\n",
          xhc_dev->bus, xhc_dev->device, xhc_dev->function);
    }
  }

  // 割り込み記述子IDTを設定
  // 現在のコードセグメントのセレクタ値を取得
  const uint16_t cs = GetCS();
  // DPLは0固定で設定
  SetIDTEntry(idt[InterruptVector::kXHCI], MakeIDTAttr(DescriptorType::kInterruptGate, 0),
              reinterpret_cast<uint64_t>(IntHandlerXHCI), cs);
  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));

  // MSI割り込みを有効化する
  const uint8_t bsp_local_apic_id =
      *reinterpret_cast<const uint32_t*>(0xfee00020) >> 24;
  pci::ConfigureMSIFixedDestination(
      *xhc_dev, bsp_local_apic_id,
      pci::MSITriggerMode::kLevel, pci::MSIDeliveryMode::kFixed,
      InterruptVector::kXHCI, 0);

  // PCIコンフィギュレーション空間からxHCIのBAR0を読み撮ってMMIOを探す
  const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
  Log(kDebug, "ReadBar: %s\n", xhc_bar.error.Name());
  const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);
  Log(kDebug, "xHC mmio_base = %08lx\n", xhc_mmio_base);

  // xHCの初期化と起動
  usb::xhci::Controller xhc(xhc_mmio_base);
  if(0x8086 == pci::ReadVendorId(*xhc_dev)) {
    // Intel製のxHCだった場合は、EHCIではなくxHCに切り替える処理を実行
    SwitchEhci2Xhci(*xhc_dev);
  }
  {
    auto err = xhc.Initialize();
    Log(kDebug, "xhc.Initialize: %s\n", err.Name());
  }
  Log(kInfo, "xHC starting\n");
  xhc.Run();

  ::xhc = &xhc;
  //__asm__("sti");

  // USBマウスからのデータを受信する関数として、MouseObserverをセット
  usb::HIDMouseDriver::default_observer = MouseObserver;
  // xHCIのデバイスからマウスを探し出し、設定する
  for (int i = 1; i <= xhc.MaxPorts(); ++i) {
    auto port = xhc.PortAt(i);
    Log(kDebug, "Port %d: IsConnected=%d\n", i, port.IsConnected());

    if (port.IsConnected()) {
      if (auto err = ConfigurePort(xhc, port)) {
        Log(kError, "failed to configure port: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
        continue;
      }
    }
  }

  // キューにたまったイベントを処理するイベントループ
  while(true) {
    // cliオペランドで割り込みを一時的に受け取らないようにする
    __asm__("cli");
    // イベントがキューに溜まっていない場合は、割り込みを受け取る状態にして停止させる
    if (main_queue.Count() == 0) {
      __asm__("sti\n\thlt");
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
        while (xhc.PrimaryEventRing()->HasFront()) {
          if (auto err = ProcessEvent(xhc)) {
            Log(kError, "Error while ProcessEvent: %s at %s:%d\n",
                err.Name(), err.File(), err.Line());
          }
        }
        break;
      // どれにも該当しないイベント型だった場合
      default:
        Log(kError, "Unknown message type: %d\n", msg.type);
    }
  }

  while (1) __asm__("hlt");
}
