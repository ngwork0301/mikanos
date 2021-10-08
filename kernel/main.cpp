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
#include "pci.hpp"


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
//! マウスカーソルの形のデータ
const int kMouseCursorWidth = 15;
const int kMouseCursorHeight = 24;
const char mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1] {
  "@              ",
  "@@             ",
  "@.@            ",
  "@..@           ",
  "@...@          ",
  "@....@         ",
  "@.....@        ",
  "@......@       ",
  "@.......@      ",
  "@........@     ",
  "@.........@    ",
  "@..........@   ",
  "@...........@  ",
  "@............@ ",
  "@......@@@@@@@@",
  "@......@       ",
  "@....@@.@      ",
  "@...@ @.@      ",
  "@..@   @.@     ",
  "@.@    @.@     ",
  "@@      @.@    ",
  "@       @.@    ",
  "         @.@   ",
  "         @@@   ",
};

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
 * KernelMain関数
 * 
 * @brief
 * カーネルのエントリポイント
 * 
 * @param [in] frame_buffer_config FrameBufferConfig構造体
 */
extern "C" void KernelMain(const struct FrameBufferConfig& frame_buffer_config) {
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

  // マウスカーソルを描画
  for (int dy = 0; dy < kMouseCursorHeight; ++dy) {
    for (int dx = 0; dx < kMouseCursorWidth; ++dx) {
      if (mouse_cursor_shape[dy][dx] == '@') {
        pixel_writer->Write(200 + dx, 100 + dy, {0, 0, 0});
      } else if (mouse_cursor_shape[dy][dx] == '.') {
        pixel_writer->Write(200 + dx, 100 + dy, {255, 255, 255});
      }
    }
  }

  // PCIデバイスを列挙する
  auto err = pci::ScanAllBus();
  printk("ScanAllBus: %s\n", err.Name());
  for (int i = 0; i < pci::num_device; ++i) {
    const auto& dev = pci::devices[i];
    auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
    auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
    printk("%d.%d.%d: vend %04d, class %08d, head %02x\n",
          dev.bus, dev.device, dev.function,
          vendor_id, class_code, dev.header_type);
  }

  // 無限ループ
  while(1) __asm__("hlt");
}
