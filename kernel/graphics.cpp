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

/**
 * @fn
 * FillRectangle関数
 * 
 * @brief
 * 指定した長方形領域を指定した色で塗りつぶす
 * 
 * @param [in] writer
 * @param [in] pos 位置(X,Y座標Vector2D)
 * @param [in] size 大きさ(width, height Vector2D)
 * @param [in] c 色
 */
void FillRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c) {
  for (int dy = 0; dy < size.y; ++dy) {
    for (int dx = 0; dx < size.x; ++dx) {
      writer.Write(pos.x + dx, pos.y + dy, c);
    }
  }
}

/**
 * @fn
 * DrawRectangle
 * 
 * @brief
 * 長方形を枠線を描画する
 * 
 * @param [in] writer PixelWriter
 * @param [in] pos 描画位置(X,Y座標Vector2D)
 * @param [in] size 大きさ(width, height Vector2D)
 * @param [in] c 色
 */
void DrawRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c) {
  // 水平線の描画
  for (int dx = 0; dx < size.x; ++dx) {
    writer.Write(pos.x + dx, pos.y, c);
    writer.Write(pos.x + dx, pos.y + size.y - 1, c);
  }
  // 垂直線の描画
  for (int dy = 0; dy < size.y; ++dy) {
    writer.Write(pos.x, pos.y + dy, c);
    writer.Write(pos.x + size.x, pos.y + dy, c);
  }
}
