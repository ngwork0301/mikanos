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
void RGBResv8BitPerColorPixelWriter::Write(Vector2D<int> pos, const PixelColor& c) {
  auto p = PixelAt(pos);
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
void BGRResv8BitPerColorPixelWriter::Write(Vector2D<int> pos, const PixelColor& c) {
  auto p = PixelAt(pos);
  p[0] = c.b;
  p[1] = c.g;
  p[2] = c.r;
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
    writer.Write(pos + Vector2D<int>{dx, 0}, c);
    writer.Write(pos + Vector2D<int>{dx, size.y - 1}, c);
  }
  // 垂直線の描画
  for (int dy = 0; dy < size.y; ++dy) {
    writer.Write(pos + Vector2D<int>{0, dy}, c);
    writer.Write(pos + Vector2D<int>{size.x - 1, dy}, c);
  }
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
      writer.Write(pos + Vector2D<int>{dx, dy}, c);
    }
  }
}

/**
 * @fn
 * DrawDesktop関数
 * 
 * @brief
 * デスクトップ背景を描画する
 * 
 * @param [in] writer PixelWriterインスタンス
 */
void DrawDesktop(PixelWriter& writer) {
  const auto width = writer.Width();
  const auto height = writer.Height();
  // 背景はkDesktopBGColorで染める
  FillRectangle(writer,
                {0, 0},
                {width, height - 50},
                kDesktopBGColor);
  // 下部のタスクバー
  FillRectangle(writer,
                {0, height - 50},
                {width, 50},
                {1, 8, 17});
  // 左下のスタートメニューっぽいところ
  FillRectangle(writer,
                {0, height - 50},
                {width / 5, 50},
                {80, 80, 80});
  // 左下のスタートボタンのような場所を枠でくり抜き
  DrawRectangle(writer,
                {10, height - 40},
                {30, 30},
                {160, 160, 160});
}

//! 画面の設定をグローバル変数として参照できるようにする。
FrameBufferConfig screen_config;

/**
 * @fn
 * ScreenSize関数
 * 
 * @brief
 * 画面のサイズを取得する。
 * @return Vector2D<int> 水平サイズ、垂直サイズ
 */
Vector2D<int> ScreenSize() {
  return {
    static_cast<int>(screen_config.horizontal_resolution),
    static_cast<int>(screen_config.vertical_resolution)
  };
}
