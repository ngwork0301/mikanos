#pragma once

#include "graphics.hpp"

/**
 * @class
 * Consoleクラス
 * 
 * @brief
 * コンソール表示クラス
 * 
 */
class Console {
  public:
    //! 表示領域の行列数
    static const int kRows = 25, kColumns = 80;

    Console(PixelWriter& writer,
        const PixelColor& fg_color, const PixelColor& bg_color);
    void PutString(const char* s);

  private:
    void Newline();

    PixelWriter& writer_;
    const PixelColor& fg_color_, bg_color_;
    //! 描画している文字列を保持するバッファ。行はヌル文字分を加えておく
    char buffer_[kRows][kColumns + 1];
    //! カーソル位置
    int cursor_row_, cursor_column_;
};
