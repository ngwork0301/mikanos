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
  
  // プロンプトを出力
  Print(">");
}

/**
 * @fn
 * Terminal::BlinkCursorメソッド
 * 
 * @brief 
 * カーソルを点滅させる
 * @return
 * カーソルの描画範囲
 */
Rectangle<int> Terminal::BlinkCursor() {
  // カーソル表示フラグを反転
  cursor_visible_ = !cursor_visible_;
  DrawCursor(cursor_visible_);
  return {CalcCursorPos(), {7, 15}};
}

/**
 * @fn
 * Terminal::InputKeyメソッド
 * @brief 
 * キー入力を受け付ける
 * @param [in] modifier モディファイア
 * @param [in] keycode キーコード
 * @param [in] ascii ASCII文字列
 * @return Rectangle<int> 
 */
Rectangle<int> Terminal::InputKey(uint8_t modifier, uint8_t keycode, char ascii){
  // 入力した文字列がみえないため、一時的にカーソルは非表示にする
  DrawCursor(false);

  Rectangle<int> draw_area{CalcCursorPos(), {8*2, 16}};

  // 改行コードの場合は、コマンド実行して次の行へ移動
  if (ascii == '\n') {
    // 新規行へ移行するためカーソル行を初期化
    linebuf_[linebuf_index_] = 0;
    linebuf_index_ = 0;
    cursor_.x = 0;

    if (cursor_.y < kRows - 1) {
      // 表示する行が最大の場合は、1行表示をへらすだけ
      ++cursor_.y;
    } else {
      // そうでない場合は、1行下へスクロール
      Scroll1();
    }
    // コマンドを実行
    ExecuteLine();
    Print(">");
    draw_area.pos = ToplevelWindow::kTopLeftMargin;
    draw_area.size = window_->InnerSize();
  } else if (ascii == '\b') {
    // バックスペースキーが入力したときは1文字消して戻る
    if (cursor_.x > 0) {
      // カーソルの位置をずらす
      --cursor_.x;
      // 1文字分黒で塗りつぶして消す
      FillRectangle(*window_->Writer(), CalcCursorPos(), {8, 16}, {0, 0, 0});
      draw_area.pos = CalcCursorPos();

      if (linebuf_index_ > 0) {
        // linebuf_1文字分消す
        --linebuf_index_;
      }
    }
  } else if (ascii != 0) {
    // その他の文字列がきたらその文字を描画する
    // カーソル位置が1行の最大文字以内の場合のみ文字を入力。それ以外は無視
    if (cursor_.x < kColumns - 1 && linebuf_index_ < kLineMax - 1) {
      linebuf_[linebuf_index_] = ascii;
      ++linebuf_index_;
      WriteAscii(*window_->Writer(), CalcCursorPos(), ascii, {255, 255, 255});
      ++cursor_.x;
    }
  }

  // カーソルを描画する
  DrawCursor(true);

  // 描画領域を返す
  return draw_area;
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
  FillRectangle(*window_->Writer(), CalcCursorPos(), {7, 15}, color);
}

/**
 * @fn
 * Terminal::CalcCursorPosメソッド
 * 
 * @brief 
 * カーソルのピクセル座標を計算する
 * @return Vector2D<int> カーソル位置のピクセル座標
 */
Vector2D<int> Terminal::CalcCursorPos() const{
  return ToplevelWindow::kTopLeftMargin +
      Vector2D<int>{4 + 8 * cursor_.x, 4 + 16 * cursor_.y};
}

/**
 * @fn
 * Terminal::Scroll1メソッド
 * 
 * @brief 
 * 1行スクロールする
 */
void Terminal::Scroll1() {
  // 移動元の範囲の座標を計算
  Rectangle<int> move_src{
    ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4 + 16},
    {8*kColumns, 16*(kRows - 1)}
  };
  // 上記の範囲を1行分移動
  window_->Move(ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4}, move_src);
  // 最終行を黒で塗りつぶし
  FillRectangle(*window_->InnerWriter(),
                {4, 4 + 16*cursor_.y}, {8*kColumns, 16}, {0, 0, 0});
}

/**
 * @fn
 * Terminal::Printメソッド
 * 
 * @brief 
 * 指定された文字列をターミナルへ表示する
 * @param s 表示する文字列
 */
void Terminal::Print(const char* s) {
  // 一時的にカーソルは非表示
  DrawCursor(false);
  
  // 1行改行する
  auto newline = [this]() {
    cursor_.x = 0;
    if (cursor_.y < kRows - 1) {
      // カーソルが表示できる行数以下の場合は、単純にカーソルを1行下に移動
      ++cursor_.y;
    } else {
      // カーソルが表示できる行数以上の場合は、Ⅰ行分スクロール
      Scroll1();
    }
  };

  while(*s) {
    if (*s == '\n') {
      // 改行コードは、1行改行する
      newline();
    } else {
      WriteAscii(*window_->Writer(), CalcCursorPos(), *s, {255, 255, 255});
      if (cursor_.x == kColumns - 1) {
        // 出力する文字数が1行の最大文字数以上になったっら1行改行して出力
        newline();
      } else {
        ++cursor_.x;
      }
    }

    ++s;
  }

  // 非表示にしたカーソルを再度表示
  DrawCursor(true);
}

/**
 * @fn
 * Terminal::ExecuteLineメソッド
 * 
 * @brief 
 * 入力された文字列をコマンドとして実行する
 */
void Terminal::ExecuteLine() {
  char* command = &linebuf_[0];
  char* first_arg = strchr(&linebuf_[0], ' ');
  if (first_arg) {
    // 第一引数があれば、コマンドと引数の間はNULL文字をいれて、アドレスを１つインクリメント
    *first_arg = 0;
    ++first_arg;
  }
  if (strcmp(command, "echo") == 0) {
    if (first_arg) {
      Print(first_arg);
    }
    Print("\n");
  } else if (strcmp(command, "clear") == 0) {
    // ターミナル画面内をすべて黒で塗りつぶす
    FillRectangle(*window_->InnerWriter(),
                  {4, 4}, {8*kColumns, 16*kRows}, {0, 0, 0});
    // カーソル位置を最初に戻す
    cursor_.y = 0;
  } else if (command[0] != 0) {
    Print("no such command: ");
    Print(command);
    Print("\n");
  }
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
  // ターミナルタスクを検索表に登録する
  layer_task_map->insert(std::make_pair(terminal->LayerID(), task_id));
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
        {
          const auto area = terminal->BlinkCursor();

          // メインタスク(ID=1)に画面描画処理を呼び出し
          Message msg = MakeLayerMessage(
              task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
          // Message msg{Message::kLayer, task_id};
          // msg.arg.layer.layer_id = terminal->LayerID();
          // msg.arg.layer.op = LayerOperation::Draw;
          __asm__("cli"); // 割り込みを抑止
          task_manager->SendMessage(1, msg);
          __asm__("sti"); // 割り込みを許可
        }
        break;
      case Message::kKeyPush:
        // キー入力イベントが送られてきたとき
        {
          const auto area = terminal->InputKey(msg->arg.keyboard.modifier,
                                               msg->arg.keyboard.keycode,
                                               msg->arg.keyboard.ascii);
          Message msg = MakeLayerMessage(
              task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
          __asm__("cli"); //割り込み禁止
          task_manager->SendMessage(1, msg);
          __asm__("sti"); //割り込み許可
        }
        break;
      default:
        break;
    }
  }
}
