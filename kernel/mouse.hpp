#pragma once

#include "graphics.hpp"
/**
 * @class
 * MouseCursor
 * 
 * @brief
 * マウスカーソルの描画と移動処理の責務を持つクラス
 */
class MouseCursor {
  public:
    MouseCursor(PixelWriter* writer, PixelColor erace_color,
                Vector2D<int> initial_position);
    void MoveRelative(Vector2D<int> desplacement);

  private:
    PixelWriter* pixel_writer_ = nullptr;
    PixelColor erase_color_;
    Vector2D<int> position_;
};

