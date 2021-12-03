#pragma once

#include "graphics.hpp"
#include "window.hpp"

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

    Console(const PixelColor& fg_color, const PixelColor& bg_color);
    void PutString(const char* s);
    void SetWriter(PixelWriter* writer);
    void SetWindow(const std::shared_ptr<Window>& window);
    void SetLayerID(unsigned int layer_id);
    unsigned int LayerID() const;

  private:
    void Newline();
    void Refresh();

    PixelWriter* writer_;
    //! 自身を出力するウィンドウ
    std::shared_ptr<Window> window_;
    const PixelColor& fg_color_, bg_color_;
    //! 描画している文字列を保持するバッファ。行はヌル文字分を加えておく
    char buffer_[kRows][kColumns + 1];
    //! カーソル位置
    int cursor_row_, cursor_column_;
    //! 描画するコンソールのレイヤーID
    unsigned int layer_id_;
};

extern Console* console;

void InitializeConsole();
