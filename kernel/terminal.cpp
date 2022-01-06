#include "terminal.hpp"

#include "font.hpp"
#include "layer.hpp"

#include "logger.hpp"

/**
 * @fn Terminal::Terminalコンストラクタ
 * @brief Construct a new Terminal:: Terminal object
 * 
 */
Terminal::Terminal() {
  // ターミナルウィンドウの生成
  window_ = std::make_shared<ToplevelWindow>(
    kColumns * 8 + 8 + ToplevelWindow::kMarginX,
    kRows * 16 + 8 + ToplevelWindow::kMarginY,
    screen_config.pixel_format,
    "MikanTerm");
  // 生成したターミナルウィンドウの描画
  DrawTerminal(*window_->InnerWriter(), {0, 0}, window_->InnerSize());

  // ターミナルレイヤーIDの設定
  layer_id_ = layer_manager->NewLayer()
    .SetWindow(window_)
    .SetDraggable(true)
    .ID();
}

/**
 * @fn
 * Terminal::BlinkCursorメソッド
 * 
 * @brief 
 * カーソルを点滅させる
 */
void Terminal::BlinkCursor() {
  // カーソル表示フラグを反転
  cursor_visible_ = !cursor_visible_;
  DrawCursor(cursor_visible_);
}

/**
 * @fn
 * Terminal::DrawCursorメソッド
 * 
 * @brief 
 * カーソルを描画する
 * 
 * @param [in] visible (bool) カーソルの表示／非表示を指定
 */
void Terminal::DrawCursor(bool visible) {
  // 表示するときは白。非表示のときは背景色と同色の黒
  const auto color = visible ? ToColor(0xffffff) : ToColor(0);
  // カーソル描画位置 現在行、列からピクセル数を算出
  const auto pos = Vector2D<int>{4 + 8*cursor_.x, 5 + 16*cursor_.y};
  FillRectangle(*window_->InnerWriter(), pos, {7, 15}, color);
}

/**
 * @fn
 * TaskTerminal関数
 * 
 * @brief 
 * ターミナルタスクを生成して、ターミナルウィンドウを描画
 * @param task_id 
 * @param data 
 */
void TaskTerminal(uint64_t task_id, int64_t data) {
  __asm__("cli"); // 割り込みを抑止
  Task& task = task_manager->CurrentTask();
  Terminal* terminal = new Terminal;
  layer_manager->Move(terminal->LayerID(), {100, 200});
  active_layer->Activate(terminal->LayerID());
  __asm__("sti"); // 割り込みを許可

  while (true) {
    __asm__("cli"); // 割り込みを抑止
    auto msg = task.ReceiveMessage();
    if (!msg) {
      task.Sleep();
      __asm__("sti");  // 割り込みを許可
      continue;
    }

    switch (msg->type) {
      case Message::kTimerTimeout:
        // カーソル点滅のためのタイマーイベントが送られてきたとき
        terminal->BlinkCursor();

        {
          // メインタスク(ID=1)に画面描画処理を呼び出し
          Message msg{Message::kLayer, task_id};
          msg.arg.layer.layer_id = terminal->LayerID();
          msg.arg.layer.op = LayerOperation::Draw;
          __asm__("cli"); // 割り込みを抑止
          task_manager->SendMessage(1, msg);
          __asm__("sti"); // 割り込みを許可
        }
        break;
      default:
        break;
    }
  }
}
