#include "terminal.hpp"

#include <cstring>
#include <limits>

#include "asmfunc.h"
#include "elf.hpp"
#include "font.hpp"
#include "layer.hpp"
#include "logger.hpp"
#include "memory_manager.hpp"
#include "pci.hpp"
#include "paging.hpp"

namespace {
  /**
   * @fn
   * MakeArgVector関数
   * @brief 
   * 引数ベクトルを作成する
   * @param [in] command コマンド
   * @param [in] first_arg 引数文字列
   * @param [in, out] argv 引数配列
   * @param [in] argv_len 引数の数
   * @param [in, out] argbuf 引数文字列のバッファ
   * @param [in] argbuf_len バッファの長さ
   * @return WithError構造体<int argc, Error>
   */
  WithError<int> MakeArgVector(char* command, char* first_arg,
      char** argv, int argv_len, char* argbuf, int argbuf_len) {
    //! コマンド本体を含む引数のindex
    int argc = 0;
    //! 引数の文字列を入れるバッファのindex
    int argbuf_index = 0;

    // 引数sをargbufが指す文字列データ領域にコピーして、
    // コピー先に文字列へのポインタをargvの末尾に追加する
    auto push_to_argv = [&](const char* s) {
      // 予定している確保しているメモリ以上になったらエラー
      if (argc >= argv_len || argbuf_index >= argbuf_len) {
        return MAKE_ERROR(Error::kFull);
      }

      // argvに引数を追加して、バッファへのポインタを設定
      argv[argc] = &argbuf[argbuf_index];
      // 次の引数へインクリメント
      ++argc;
      // 文字列をバッファにコピー
      strcpy(&argbuf[argbuf_index], s);
      argbuf_index += strlen(s) + 1;
      return MAKE_ERROR(Error::kSuccess);
    };

    // まずコマンド本体を追加
    if (auto err = push_to_argv(command)) {
      return { argc, err };
    }
    // first_argに指定された引数文字列が空の場合はエラー
    if (!first_arg) {
      return { argc, MAKE_ERROR(Error::kSuccess) };
    }

    char* p = first_arg;
    // 基本１文字ずつループ ※途中連続する文字数分進む
    while (true) {
      // スペースが続く限り除去
      while (isspace(p[0])) {
        ++p;
      }
      // 末尾であれば終了
      if (p[0] == 0) {
        break;
      }
      const char* arg = p;

      // 後続の文字があるならその分まで進む
      while(p[0] != 0 && !isspace(p[0])) {
        ++p;
      }
      // 末尾かどうかチェック
      const bool is_end = p[0] == 0;
      // 現在見ている文字をヌル文字にして次へ
      p[0] = 0;
      if (auto err = push_to_argv(arg)) {
        return { argc, err };
      }
      if (is_end) {
        break;
      }
      ++p;
    }

    return { argc, MAKE_ERROR(Error::kSuccess) };
  }
} // namepace

/**
 * @fn
 * CleanPageMap関数
 * @brief 
 * 指定されたページング構造のエントリをすべて削除する
 * @param page_map 削除するページング構造
 * @param page_map_level ページング構造の階層レベル
 * @return Error 
 */
Error CleanPageMap(PageMapEntry* page_map, int page_map_level) {
  for (int i = 0; i < 512; ++i) {
    auto entry = page_map[i];
    // presentフラグが立ってない場合は、確保されていないのでスキップ
    if (!entry.bits.present) {
      continue;
    }

    if (page_map_level > 1) {
      // 再帰呼び出し
      if (auto err = CleanPageMap(entry.Pointer(), page_map_level - 1)) {
        return err;
      }
    }

    const auto entry_addr = reinterpret_cast<uintptr_t>(entry.Pointer());
    const FrameID map_frame{entry_addr / kBytesPerFrame};
    if (auto err = memory_manager->Free(map_frame, 1)) {
      return err;
    }
    page_map[i].data = 0;
  }
  return MAKE_ERROR(Error::kSuccess);
}

/**
 * @fn
 * CleanPageMaps関数
 * 
 * @brief 
 * アプリ用のページング構造を破棄する
 * PML4テーブルには、1つしかエントリがないことが前提のため注意
 * @param addr PML4のページング構造
 * @return Error 
 */
Error CleanPageMaps(LinearAddress4Level addr) {
  auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
  auto pdp_table = pml4_table[addr.parts.pml4].Pointer();
  pml4_table[addr.parts.pml4].data = 0;
  if (auto err = CleanPageMap(pdp_table, 3)) {
    return err;
  }

  const auto pdp_addr = reinterpret_cast<uintptr_t>(pdp_table);
  const FrameID pdp_frame{pdp_addr / kBytesPerFrame};
  return memory_manager->Free(pdp_frame, 1);
}

/**
 * @fn
 * GetProgramHeader関数
 * @brief Get the Program Header object
 * ELFヘッダからプログラムヘッダを読み取ってそのアドレスを返す
 * @param ehdr ELFヘッダ構造体
 * @return Elf64_Phdr* 
 */
Elf64_Phdr* GetProgramHeader(Elf64_Ehdr* ehdr) {
  return reinterpret_cast<Elf64_Phdr*>(
      reinterpret_cast<uintptr_t>(ehdr) + ehdr->e_phoff);
}

/**
 * @fn
 * GetFirstLoadAddress関数
 * 
 * @brief Get the First Load Address object
 * ELFヘッダの最初のLOADセグメントのアドレスを取得する
 * @param ehdr ELFヘッダ構造体
 * @return uintptr_t 
 */
uintptr_t GetFirstLoadAddress(Elf64_Ehdr* ehdr) {
  auto phdr = GetProgramHeader(ehdr);

  for (int i = 0; i < ehdr->e_phnum; ++i) {
    if (phdr[i].p_type != PT_LOAD) continue;
    return phdr[i].p_vaddr;
  }
  // LOADセグメントが見つからなければ、0を返す
  return 0;
}

/**
 * @fn
 * NewPageMap
 * 
 * @brief 
 * 階層ページング構造の仮想アドレスと物理アドレスのマッピングを新たに作成する
 * 使用しおわったら必ず解放すること
 * @return WithError<PageMapEntry*> 
 */
WithError<PageMapEntry*> NewPageMap() {
  auto frame = memory_manager->Allocate(1);
  if (frame.error) {
    return { nullptr, frame.error };
  }

  auto e = reinterpret_cast<PageMapEntry*>(frame.value.Frame());
  memset(e, 0, sizeof(uint64_t) * 512);
  return { e, MAKE_ERROR(Error::kSuccess) };
}

/**
 * @fn
 * SetNewPageMapIfNotPresent関数
 * @brief Set the New Map If Not Present object
 * 必要に応じて新たなページング構造を生成して設定する
 * @param entry エントリインデックス
 * @return WithError<PageMapEntry*> 
 */
WithError<PageMapEntry*> SetNewPageMapIfNotPresent(PageMapEntry& entry) {
  // エントリのpresentフラグが1であれば、すでにこのエントリは設定済みとして何もせず終了
  if (entry.bits.present) {
    return { entry.Pointer(), MAKE_ERROR(Error::kSuccess) };
  }

  // 新しいページング構造を生成
  auto [ child_map, err ] = NewPageMap();
  if (err) {
    return { nullptr, err };
  }

  // 生成したページング構造をエントリのaddrフィールドに設定
  entry.SetPointer(child_map);
  entry.bits.present = 1;

  return { child_map, MAKE_ERROR(Error::kSuccess)};
}

/**
 * @fn
 * SetupPageMap関数
 * 
 * @brief 
 * 指定された階層のページング設定をする
 * @param page_map その階層のページング構造の」物理アドレス
 * @param page_map_level 階層レベル
 * @param addr LOADセグメントの仮想アドレス
 * @param num_4kpages LOADセグメントの残りページ数
 * @return WithError<size_t> 未処理のページとエラー
 */
WithError<size_t> SetupPageMap(
    PageMapEntry* page_map, int page_map_level, LinearAddress4Level addr, size_t num_4kpages) {
  // すべてのページを処理しきるまでループ
  while (num_4kpages > 0) {
    // 仮想アドレスから、その階層のどのエントリに属するかを取得
    const auto entry_index = addr.Part(page_map_level);

    // 下位の階層の物理アドレスを取得
    auto [ child_map, err ] = SetNewPageMapIfNotPresent(page_map[entry_index]);
    if (err) {
      return { num_4kpages, err };
    }
    // 書き込み可能フラグを1にする
    page_map[entry_index].bits.writable = 1;
    // userフラグを1にする
    page_map[entry_index].bits.user = 1;

    if (page_map_level == 1) {
      --num_4kpages;
    } else {
      // ひとつ下位の階層ページング構造を再帰的に設定
      auto [ num_remain_pages, err ] =
        SetupPageMap(child_map, page_map_level -1, addr, num_4kpages);
      if (err) {
        return { num_4kpages, err };
      }
      num_4kpages = num_remain_pages;
    }

    if (entry_index == 511) {
      break;
    }

    // 残りを次のエントリに設定
    addr.SetPart(page_map_level, entry_index+1);
    for (int level = page_map_level - 1; level >= 1; --level) {
      // 下位のエントリにすべて0を設定
      addr.SetPart(level, 0);
    }
  }
  return { num_4kpages, MAKE_ERROR(Error::kSuccess) };
}

/**
 * @fn
 * SetupPageMaps関数
 * @brief 
 * 階層ページング構造全体を設定する
 * @param addr LOADセグメントの先頭アドレス
 * @param num_4kpages LOADセグメントのページ数
 * @return Error 
 */
Error SetupPageMaps(LinearAddress4Level addr, size_t num_4kpages) {
  // CR3レジスタから階層ページング構造の最上位PML4の物理アドレスを取得
  auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
  return SetupPageMap(pml4_table, 4, addr, num_4kpages).error;
}

/**
 * @fn
 * CopyLoadSegments関数
 * 
 * @brief 
 * LOADセグメントを、階層ページング構造を作成して最終目的地にコピーする
 * @param ehdr ELFへのポインタ
 * @return Error 
 */
Error CopyLoadSegments(Elf64_Ehdr* ehdr) {
  auto phdr = GetProgramHeader(ehdr);
  for (int i = 0; i < ehdr->e_phnum; ++i) {
    // プログラムヘッダごとにループ
    if (phdr[i].p_type != PT_LOAD) continue;

    // PML4のコピー先アドレス
    LinearAddress4Level dest_addr;
    dest_addr.value = phdr[i].p_vaddr;
    const auto num_4kpages = (phdr[i].p_memsz + 4095) / 4096;

    // 階層ページング構造を設定
    if (auto err = SetupPageMaps(dest_addr, num_4kpages)) {
      return err;
    }

    // 物理メモリ領域にコピー
    const auto src = reinterpret_cast<uint8_t*>(ehdr) + phdr[i].p_offset;
    const auto dst = reinterpret_cast<uint8_t*>(phdr[i].p_vaddr);
    memcpy(dst, src, phdr[i].p_filesz);
    // ファイルサイズとメモリサイズの差分も0で初期化して確保
    memset(dst + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
  }
  return MAKE_ERROR(Error::kSuccess);
}

/**
 * @fn
 * LoadELF関数
 * 
 * @brief 
 * ELFファイル内のLOADセグメントを階層ページング構造を作成してメモリに配置する
 * @param ehdr ELFヘッダへのポインタ
 * @return Error 
 */
Error LoadELF(Elf64_Ehdr* ehdr) {
  // 実行形式ではない場合、フォーマットエラーとする
  if (ehdr->e_type != ET_EXEC) {
    return MAKE_ERROR(Error::kInvalidFormat);
  }

  // LOADセグメントを探して、後半の仮想アドレスでなければ、フォーマットエラーとする
  const auto addr_first = GetFirstLoadAddress(ehdr);
  if (addr_first < 0xffff'8000'0000'0000) {
    return MAKE_ERROR(Error::kInvalidFormat);
  }

  // LOADセグメントのコピー
  if (auto err = CopyLoadSegments(ehdr)) {
    return err;
  }

  return MAKE_ERROR(Error::kSuccess);
}

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
      ExecuteFile(*file_entry, command, first_arg);
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
 * @param [in] file_entry 実行するファイルのエントリポインタ
 * @param [in] command コマンド
 * @param [in] first_arg 第一引数
 */
Error Terminal::ExecuteFile(const fat::DirectoryEntry& file_entry, char* command, char* first_arg){
  std::vector<uint8_t> file_buf(file_entry.file_size);
  fat::LoadFile(&file_buf[0], file_buf.size(), file_entry);

  // ELF形式かどうか
  auto elf_header = reinterpret_cast<Elf64_Ehdr*>(&file_buf[0]);
  if (memcmp(elf_header->e_ident, "\x7f" "ELF", 4) != 0) {
    // ELF形式でない場合は、ファイルエントリをそのまま関数としてキャストして呼び出す
    using Func = void ();
    auto f = reinterpret_cast<Func*>(&file_buf[0]);
    f();
    return MAKE_ERROR(Error::kSuccess);
  }
  if (auto err = LoadELF(elf_header)) {
    return err;
  }

  // 引数用のメモリ領域を確保
  LinearAddress4Level args_frame_addr{0xffff'ffff'ffff'f000};
  if (auto err = SetupPageMaps(args_frame_addr, 1)) {
    return err;
  }
  auto argv = reinterpret_cast<char**>(args_frame_addr.value);
  int argv_len = 32;  // argv = 8x32 = 256 bytes
  auto argbuf = reinterpret_cast<char*>(args_frame_addr.value + sizeof(char**) * argv_len);
  int argbuf_len = 4096 - sizeof(char**) * argv_len;
  // 引数のベクタ配列を作成する
  auto argc = MakeArgVector(command, first_arg, argv, argv_len, argbuf, argbuf_len);
  if (argc.error) {
    return argc.error;
  }

  // アプリ用のスタック領域を指定
  LinearAddress4Level stack_frame_addr{0xffff'ffff'ffff'e000};
  // 1ページ分階層ページング構造から物理ページを取得
  if (auto err = SetupPageMaps(stack_frame_addr, 1)) {
    return err;
  }

  auto entry_addr = elf_header->e_entry;
  // CS/SSレジスタを切り替えて、ユーザセグメントとして実行
  CallApp(argc.value, argv, 3 << 3 | 3, 4 << 3 | 3, entry_addr,
      stack_frame_addr.value + 4096 - 8);

  /*
  char s[64];
  sprintf(s, "app exited. ret = %d\n", ret);
  Print(s);
  */

  // マッピングした階層ページング構造を解放する
  const auto addr_first = GetFirstLoadAddress(elf_header);
  if (auto err = CleanPageMaps(LinearAddress4Level{addr_first})) {
    return err;
  }

  return MAKE_ERROR(Error::kSuccess);
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
