/**
 * @file terminal.hpp
 * @brief 
 * ターミナル関連の実装を集めたファイル
 */
#pragma once

#include <deque>
#include <map>
#include "fat.hpp"
#include "window.hpp"
#include "task.hpp"
#include "layer.hpp"

class Terminal {
  public:
    static const int kRows = 15, kColumns = 60;
    static const int kLineMax = 128;

    Terminal();
    unsigned int LayerID() const { return layer_id_; }
    Rectangle<int> BlinkCursor();
    Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);

  private:
    //! このターミナルウィンドウのインスタンス
    std::shared_ptr<ToplevelWindow> window_;
    //! ターミナルウィンドウのレイヤーID
    unsigned int layer_id_;

    //! カーソル座標
    Vector2D<int> cursor_{0, 0};
    //! カーソル描画の表示/不表示フラグ
    bool cursor_visible_{false};
    void DrawCursor(bool visible);
    Vector2D<int> CalcCursorPos() const;

    //! linebuf_内のindex
    int linebuf_index_{0};
    //! キー入力を1行分ためておくバッファ
    std::array<char, kLineMax> linebuf_{};
    void Scroll1();
    void Print(char c);
    void Print(const char* s);
    void ExecuteLine();
    void ExecuteFile(const fat::DirectoryEntry& file_entry, char* command, char* first_arg);

    //! コマンドヒストリ
    std::deque<std::array<char, kLineMax>> cmd_history_{};
    //! 現在表示中のヒストリのインデックス(小さいほど新しい)
    int cmd_history_index_{-1};
    Rectangle<int> HistoryUpDown(int direction);
};

void TaskTerminal(uint64_t task_id, int64_t data);
