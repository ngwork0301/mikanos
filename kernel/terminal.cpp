#include "terminal.hpp"

#include "font.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "pci.hpp"

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

  // コマンドヒストリの初期化
  cmd_history_.resize(8);
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
    if (linebuf_index_ > 0) {
      // コマンドヒストリの末尾を削除
      cmd_history_.pop_back();
      // コマンドヒストリに追加
      cmd_history_.push_front(linebuf_);
    }
    linebuf_index_ = 0;
    cmd_history_index_ = -1;
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
  } else if (keycode == 0x51) { // down arrow
    // 下キーが押されたら、コマンドヒストリを一つ先へ移動
    draw_area = HistoryUpDown(-1);
  } else if (keycode == 0x52) { // up arrow
    // 上キーが押されたら、コマンドヒストリを一つ前へ移動
    draw_area = HistoryUpDown(1);
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
 * 1文字ずつ出力する
 * @param c 
 */
void Terminal::Print(char c) {
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

  if (c == '\n') {
    // 改行コードは、1行改行する
    newline();
  } else {
    WriteAscii(*window_->Writer(), CalcCursorPos(), c, {255, 255, 255});
    if (cursor_.x == kColumns - 1) {
      // 出力する文字数が1行の最大文字数以上になったっら1行改行して出力
      newline();
    } else {
      ++cursor_.x;
    }
  }
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

  while(*s) {
    Print(*s);
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
  } else if (strcmp(command, "lspci") == 0) {
    char s[64];
    for (int i = 0; i < pci::num_device; ++i) {
      const auto& dev = pci::devices[i];
      auto vender_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
      sprintf(s, "%02x:%02x.%d vend=%04x head=%02x class=%02x.%02x.%02x\n",
          dev.bus, dev.device, dev.function, vender_id, dev.header_type,
          dev.class_code.base, dev.class_code.sub, dev.class_code.interface);
      Print(s);
    }
  } else if(strcmp(command, "ls") == 0) {
    // ルートディレクトリの取得
    auto root_dir_entries = fat::GetSectorByCluster<fat::DirectoryEntry>(
      fat::boot_volume_image->root_cluster);
    // ルートディレクトリ内のディレクトリエントリを取得して、1クラスタあたりのエントリ数を取得
    // クラスタ　＞ ブロック＝セクタ、＞ バイト
    auto entries_per_cluster =
      fat::boot_volume_image->bytes_per_sector / sizeof(fat::DirectoryEntry)
      * fat::boot_volume_image->sectors_per_cluster;
    char base[9], ext[4];  //! ファイルの短名(拡張子を除く)、拡張子
    char s[64];  //! ファイル名(拡張子含む)
    // ディレクトリエントリごとにループ
    for (int i = 0; i < entries_per_cluster; ++i) {
      // 短名を取得
      ReadName(root_dir_entries[i], base, ext);
      if (base[0] == 0x00) {
        // 0x00のとき、このディレクトリエントリは空であとのエントリも無効のため終了
        break;
      } else if (static_cast<uint8_t>(base[0]) == 0xe5) {
        // 0xe5のときは、空のためスキップ
        continue;
      } else if (root_dir_entries[i].attr == fat::Attribute::kLongName) {
        // 長名専用の構造になっているためスキップ
        continue;
      }

      if (ext[0]) {
        sprintf(s, "%s.%s\n", base, ext);
      } else {
        sprintf(s, "%s\n", base);
      }
      Print(s);
    }
  } else if(strcmp(command, "cat") == 0) {
    char s[64];

    // ファイルエントリを探す
    auto file_entry = fat::FindFile(first_arg);
    if (!file_entry) {
      sprintf(s, "no such file: %s\n", first_arg);
      Print(s);
    } else {
      auto cluster = file_entry->FirstCluster();
      auto remain_bytes = file_entry->file_size;

      // 一時的にカーソルを非表示
      DrawCursor(false);
      // クラスタチェーンをたどって、クラスタごとにループ
      while (cluster != 0 && cluster != fat::kEndOfClusterchain) {
        // そのクラスタのブロックのポインタを文字列として取得
        char* p = fat::GetSectorByCluster<char>(cluster);

        int i = 0;
        for (; i < fat::bytes_per_cluster && i < remain_bytes; ++i) {
          // 1文字ずつ表示する
          Print(*p);
          ++p;
        }

        // 出力した文字数分、残りバイト数から引く
        remain_bytes -= i;
        cluster = fat::NextCluster(cluster);
      }
      DrawCursor(true);
    }
  } else if (command[0] != 0) {
    // 打ち込まれた名前が組み込みコマンド以外ならファイルを探す
    auto file_entry = fat::FindFile(command);
    if(!file_entry) {
      Print("no such command: ");
      Print(command);
      Print("\n");
    } else {
      ExecuteFile(*file_entry);
    }
  }
}

/**
 * @fn
 * Terminal::HistoryUpDownメソッド
 * 
 * @brief 
 * コマンドヒストリを上下にたどり、再描画が必要な範囲を返却する
 * @param [in] direction たどる履歴の方向。1はひとつ過去。-1はひとつ未来
 * @return Rectangle<int> ターミナル画面内の再描画が必要な範囲
 */
Rectangle<int> Terminal::HistoryUpDown(int direction) {
  if (direction == -1 && cmd_history_index_ >= 0) {
    --cmd_history_index_;
  } else if(direction == 1 && cmd_history_index_ + 1 < cmd_history_.size()) {
    ++cmd_history_index_;
  }

  // カーソル位置は、一番左にする
  cursor_.x = 1;
  const auto first_pos = CalcCursorPos();

  // 一旦現在行を黒で塗りつぶし
  Rectangle<int> draw_area{first_pos, {8*(kColumns - 1), 16}};
  FillRectangle(*window_->Writer(), draw_area.pos, draw_area.size, {0, 0, 0});

  const char* history = "";
  if (cmd_history_index_ >= 0) {
    // コマンドヒストリから対象の過去のコマンド文字列を取得
    history = &cmd_history_[cmd_history_index_][0];
  }

  // linebufを置き換え
  strcpy(&linebuf_[0], history);
  // linebuf_index_も文字数で置き換え
  linebuf_index_ = strlen(history);

  // 過去のコマンド文字列を描画
  WriteString(*window_->Writer(), first_pos, history, {255, 255, 255});
  // カーソル位置を移動
  cursor_.x = linebuf_index_ + 1;
  return draw_area;
}

/**
 * @fn
 * Terminal::ExecuteFileメソッド
 * 
 * @brief 
 * ファイルを読み込んで実行する
 * @param file_entry 実行するファイルのエントリポインタ
 */
void Terminal::ExecuteFile(const fat::DirectoryEntry& file_entry){
  auto cluster = file_entry.FirstCluster();
  auto remain_bytes = file_entry.file_size;

  std::vector<uint8_t> file_buf(remain_bytes);
  auto p = &file_buf[0];

  // クラスタごとにループして、複数クラスタにまたがったファイルの中身のデータをfile_bufにコピー
  while (cluster != 0 && cluster != fat::kEndOfClusterchain) {
    //! 残りバイト数かそのクラスタのバイト数かどちらか大きい方のバイト数
    const auto copy_bytes = fat::bytes_per_cluster < remain_bytes ?
      fat::bytes_per_cluster : remain_bytes;
    memcpy(p, fat::GetSectorByCluster<uint8_t>(cluster), copy_bytes);

    remain_bytes -= copy_bytes;
    p += copy_bytes;
    cluster = fat::NextCluster(cluster);
  }

  using Func = void ();
  auto f = reinterpret_cast<Func*>(&file_buf[0]);
  f();
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
    __asm__("sti");  // 割り込みを許可

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
