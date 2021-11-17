/**
 * @file console.cpp
 * 
 * コンソールクラス実装用のファイル
 */
#include <cstring>

#include "console.hpp"
#include "font.hpp"
#include "layer.hpp"

/**
 * @fn
 * Console::Consoleコンストラクタ
 * 
 * @brief
 * Cosoleインスタンスを生成します。
 * 
 * @param [in] writer PixelWriterインスタンス
 * @param [in] fg_color 文字列の色
 * @param [in] bg_color 背景色
 */
Console::Console(
    const PixelColor& fg_color, const PixelColor& bg_color)
    : fg_color_(fg_color), bg_color_(bg_color),
      buffer_{}, cursor_row_{0}, cursor_column_{0} {
}

/**
 * @fn
 * Console::PutStringメソッド
 * 
 * @brief
 * 引数で与えられた文字列をコンソールに描画する
 * 
 * @param [in] s 描画する文字列の先頭ポインタ
 */
void Console::PutString(const char* s){
  // 1文字ずつループ
  while(*s) {
    // 改行文字の場合は改行
    if(*s == '\n') {
      Newline();
    } else if (cursor_column_ < kColumns - 1) {
      // 最大行数以下の場合
      WriteAscii(*writer_, Vector2D<int>{8 * cursor_column_, 16 * cursor_row_}, *s, fg_color_);
      buffer_[cursor_row_][cursor_column_] = *s;
      ++cursor_column_;
    }
    ++s;
  }
  if (layer_manager) {
    layer_manager->Draw();
  }
}

/**
 * @fn
 * Console::SetWriterメソッド
 * 
 * @brief
 * 文字列書き込み先のPixelWriterを設定
 * @param [in] writer PixelWriterインスタンス
 */
void Console::SetWriter(PixelWriter* writer) {
  if (writer == writer_) {
    return;
  }
  writer_ = writer;
  Refresh();
}

/**
 * @fn
 * Console::Newlineメソッド
 * 
 * @brief
 * 改行処理をおこなう
 */
void Console::Newline() {
  // 新しい行なので行は0に戻す
  cursor_column_ = 0;
  // 列数が最大に達していなければ、そのままカーソルを下の列へ移動
  if (cursor_row_ < kRows - 1) {
    ++cursor_row_;
    return;
  } 
  
  // window_メンバ変数があるときは、高速化
  if (window_) {
    // 2行目〜最終行を長方形領域として切り出す
    Rectangle<int> move_src{{0, 16}, {8 * kColumns, 16 * kRows -1}};
    // 2行目〜最終行を1行分上に移動
    window_->Move({0,0}, move_src);
    // 追加する最終行は一旦塗りつぶし
    FillRectangle(*writer_, {0, 16 * (kRows -1)}, {8 * kColumns, 16}, bg_color_);
  } else {
    // window_メンバ変数がないときは地道に再描画
    // 一旦画面を背景色で塗りつぶし
    FillRectangle(*writer_, {0, 0}, {8 * kColumns, 16 * kRows}, bg_color_);
    // 行ごとにループ
    for (int row = 0; row < kRows - 1; ++row) {
      // 1行ずつバッファを入れ替え
      memcpy(buffer_[row], buffer_[row + 1], kColumns + 1);
      // バッファに入っている文字を1行分描画
      WriteString(*writer_, Vector2D<int>{0, 16 * row}, buffer_[row], fg_color_);
    }
    // 最後の行を0で埋める
    memset(buffer_[kRows - 1], 0, kColumns + 1);
  }
}

/**
 * @fn
 * Console::SetWindowメソッド
 * 
 * @brief
 * 自身を出力するウィンドウを設定する。setter。
 * 出力する文字列をバッファとしてもつのではなく、Windowのもつピクセルデータをつかって
 * 再描画を高速化する。
 * @param [in] window Windowインスタンス
 */
void Console::SetWindow(const std::shared_ptr<Window>& window) {
  if (window == window_) {
    return;
  }
  window_ = window;
  writer_ = window->Writer();
  Refresh();
}

/**
 * @fn
 * Console::Refreshメソッド
 * 
 * @brief
 * バッファを元に描画する
 */
void Console::Refresh() {
  for (int row = 0; row < kRows; ++row) {
    WriteString(*writer_, Vector2D<int>{0, 16 * row}, buffer_[row], fg_color_);
  }
}
