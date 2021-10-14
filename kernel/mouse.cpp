/**
 * @file mouse.cpp
 *
 * マウス関連を実装したファイル
 */
#include "mouse.hpp"

namespace {
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

  void DrawMouseCursor(PixelWriter* pixel_writer, Vector2D<int> position) {
    for (int dy = 0; dy < kMouseCursorHeight; ++dy) {
      for (int dx = 0; dx < kMouseCursorWidth; ++dx) {
        if (mouse_cursor_shape[dy][dx] == '@') {
          pixel_writer->Write(position.x + dx, position.y + dy, {0, 0, 0});
        } else if (mouse_cursor_shape[dy][dx] == '.') {
          pixel_writer->Write(position.x + dx, position.y + dy, {255, 255, 255});
        }
      }
    }
  }

  void EraseMouseCursor(PixelWriter* pixel_writer, Vector2D<int> position,
                        PixelColor erase_color) {
    for (int dy = 0; dy < kMouseCursorHeight; ++dy) {
      for (int dx = 0; dx < kMouseCursorWidth; ++dx) {
        if (mouse_cursor_shape[dy][dx] != ' ') {
          pixel_writer->Write(position.x + dx, position.y + dy, erase_color);
        }
      }
    }
  }
}

/**
 * @fn
 * MouseCursorコンストラクタ
 * 
 * @param [in] writer PixelWriterオブジェクト
 * @param [in] erace_color 背景色
 * @param [in] initial_position 初期位置のX座標、Y座標
 */
MouseCursor::MouseCursor(PixelWriter* writer, PixelColor erase_color,
                         Vector2D<int> initial_position)
    : pixel_writer_{writer},
      erase_color_{erase_color},
      position_{initial_position} {
  DrawMouseCursor(pixel_writer_, position_);
}

/**
 * @fn
 * MoveRelativeメソッド
 * 
 * @brief
 * マウス移動時の描画処理
 * 
 * @param [in] displacement 移動ベクトル
 */
void MouseCursor::MoveRelative(Vector2D<int> displacement) {
  EraseMouseCursor(pixel_writer_, position_, erase_color_);
  position_ += displacement;
  DrawMouseCursor(pixel_writer_, position_);
}


