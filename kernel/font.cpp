/**
 * @file font.cpp
 *
 * フォント描画のプログラムを集めたファイル.
 */

#include "font.hpp"

/**
 * グローバル変数
 */
const uint8_t kFontA[16] = {
  0b00000000, //
  0b00011000, //    **
  0b00011000, //    **
  0b00011000, //    **
  0b00011000, //    **
  0b00100100, //   *  *
  0b00100100, //   *  *
  0b00100100, //   *  *
  0b00100100, //   *  *
  0b01111110, //  ******
  0b01000010, //  *    *
  0b01000010, //  *    *
  0b01000010, //  *    *
  0b11100111, // ***  ***
  0b00000000, //
  0b00000000, //
};


/**
 * @fn
 * WriteAscii関数
 * 
 * @brief
 * フォントを描画する
 * 
 * @param [in] writer
 * @param [in, out] x
 * @param [in, out] y
 * @param [in] char
 * @param [in] color
 */
void WriteAscii(PixelWriter& writer, int x, int y, char c, const PixelColor& color) {
  // 現状Aしかないので、それ以外は終了させる
  if (c != 'A') {
    return;
  }
  // タテ 16bit 分ループ
  for (int dy = 0; dy < 16; ++dy) {
    // ヨコ 8bit 分ループ
    for (int dx = 0; dx < 8; ++dx) {
      // dx で左シフトしたあと、0x80(=0b1000000)との論理和をとって描画するか判定
      if ((kFontA[dy] << dx) & 0x80u) {
        writer.Write(x + dx, y + dy, color);
      }
    }
  }
}
