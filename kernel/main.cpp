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
#include "graphics.hpp"

/**
 * 配置new演算子の定義
 */
void* operator new(size_t size, void* buf) {
  return buf;
}

void operator delete(void* obj) noexcept {
}

/**
 * issue #1 純粋仮想関数の呼び出しの可能性があるとリンクエラーになるため定義をいれる
 */
extern "C" void __cxa_pure_virtual() { while (1); }

/**
 * グローバル変数
 */
char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

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
  // 背景ピクセルを描画
  for (int x = 0; x < frame_buffer_config.horizontal_resolution; ++x) {
    for (int y = 0; y < frame_buffer_config.vertical_resolution; ++y) {
      pixel_writer->Write(x, y, {255, 255, 255});
    }
  }
  // 仮想ウィンドウのピクセル描画
  // 位置は、左上から0,0。大きさは、200 x 100。色は(0, 255, 0)=緑
  for (int x = 0; x < 200; ++x) {
    for(int y = 0; y < 100; ++y) {
      int xoffset = 0;
      int yoffset = 0;
      pixel_writer->Write(xoffset + x, yoffset + y, {0, 255, 0});
    }
  }
  // !から~(チルダ)までの文字列を描画
  int i = 0;
  for (char c = '!'; c <= '~'; ++c, ++i) {
    WriteAscii(*pixel_writer, 8 * i, 50, c, {0, 0, 0});
  }
  // Hello Worldを青字で描画
  WriteString(*pixel_writer, 0, 66, "Hello, world!", {0, 0, 255});

  // stdioのNewlibをつかって文字列を描画
  char buf[128];
  sprintf(buf, "1 + 2 = %d", 1 + 2);
  WriteString(*pixel_writer, 0, 82, buf, {0, 0, 0});

  // 無限ループ
  while(1) __asm__("hlt");
}
