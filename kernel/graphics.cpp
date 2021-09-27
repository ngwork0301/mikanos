/**
 * @file graphics.cpp
 *
 * 画像描画関連のプログラムを集めたファイル．
 */

#include "graphics.hpp"

/**
 * @fn
 * RGBResv8BitPerColorPixelWriter::Write
 * 
 * @brief
 * 親クラスPixelWriterのWriteメソッドをoverride。
 * 第1,2引数で指定したX、Y座標のピクセルを、第3引数で指定した色に描画する。
 * 
 * @param [in] x X座標
 * @param [in] y Y座標
 * @param [in] c (PixelColor&)描画する色
 */
void RGBResv8BitPerColorPixelWriter::Write(int x, int y, const PixelColor& c) {
  auto p = PixelAt(x, y);
  p[0] = c.r;
  p[1] = c.g;
  p[2] = c.b;
}

/**
 * @fn
 * BGRResv8BitPerColorPixelWriter::Write
 * 
 * @brief
 * 親クラスPixelWriterのWriteメソッドをoverride。
 * 第1,2引数で指定したX、Y座標のピクセルを、第3引数で指定した色に描画する。
 * 
 * @param [in] x X座標
 * @param [in] y Y座標
 * @param [in] c (PixelColor&)描画する色
 */
void BGRResv8BitPerColorPixelWriter::Write(int x, int y, const PixelColor& c) {
  auto p = PixelAt(x, y);
  p[0] = c.b;
  p[1] = c.g;
  p[2] = c.r;
}
