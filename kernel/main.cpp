/**
 * @file main.cpp
 *
 * カーネル本体のプログラムを書いたファイル．
 */

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "font.hpp"
#include "frame_buffer_config.hpp"
#include "console.hpp"
#include "graphics.hpp"
#include "logger.hpp"
#include "pci.hpp"
#include "mouse.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/mouse.hpp"
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
 */
extern "C" void KernelMain(const struct FrameBufferConfig& frame_buffer_config) {
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

  // マウスカーソルのインスタンス化
  mouse_cursor = new(mouse_cursor_buf) MouseCursor{
    pixel_writer, kDesktopBGColor, {300, 200}
  };

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

  // xHCに溜まったイベントを処理する。
  while(1) {
    if (auto err = ProcessEvent(xhc)) {
      Log(kError, "Error while ProcessEvent: %s at %s:%d\n",
          err.Name(), err.File(), err.Line());
    }
  }
}
