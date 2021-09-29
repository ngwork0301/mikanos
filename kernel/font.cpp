/**
 * @file font.cpp
 *
 * フォント描画のプログラムを集めたファイル.
 */

#include "font.hpp"

extern const uint8_t _binary_hankaku_bin_start;
extern const uint8_t _binary_hankaku_bin_end;
extern const uint8_t _binary_hankaku_bin_size;

/**
 * @fn
 * GetFont関数
 * 
 * @brief
 * 引数で指定した文字のhankaku.oから読み取ったフォントデータを取得する
 * 
 * @param [in] c フォントを取得したい文字
 */
const uint8_t* GetFont(char c) {
  auto index = 16 * static_cast<unsigned int>(c);
  if (index >= reinterpret_cast<uintptr_t>(&_binary_hankaku_bin_size)) {
    return nullptr;
  }
  return &_binary_hankaku_bin_start + index;
}

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
  // フォントデータを取得
  const uint8_t* font = GetFont(c);
  if (font == nullptr) {
    return;
  }
  // タテ 16bit 分ループ
  for (int dy = 0; dy < 16; ++dy) {
    // ヨコ 8bit 分ループ
    for (int dx = 0; dx < 8; ++dx) {
      // dx で左シフトしたあと、0x80(=0b1000000)との論理和をとって描画するか判定
      if ((font[dy] << dx) & 0x80u) {
        writer.Write(x + dx, y + dy, color);
      }
    }
  }
}
