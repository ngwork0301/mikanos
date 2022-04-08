/**
 * @file terminal.hpp
 * @brief 
 * ターミナル関連の実装を集めたファイル
 */
#pragma once

#include <deque>
#include <map>
#include <optional>
#include "fat.hpp"
#include "file.hpp"
#include "window.hpp"
#include "task.hpp"
#include "layer.hpp"

class Terminal {
  public:
    static const int kRows = 15, kColumns = 60;
    static const int kLineMax = 128;

    Terminal(uint64_t task_id, bool show_window);
    unsigned int LayerID() const { return layer_id_; }
    Rectangle<int> BlinkCursor();
    Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);
    void Print(const char* s, std::optional<size_t> len = std::nullopt);

  private:
    //! このターミナルウィンドウのインスタンス
    std::shared_ptr<ToplevelWindow> window_;
    //! ターミナルウィンドウのレイヤーID
    unsigned int layer_id_;
    //! このターミナルのタスクID
    uint64_t task_id_;
    //! ターミナルウィンドウの表示有無
    bool show_window_;

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
    void ExecuteLine();
    Error ExecuteFile(const fat::DirectoryEntry& file_entry, char* command, char* first_arg);

    //! コマンドヒストリ
    std::deque<std::array<char, kLineMax>> cmd_history_{};
    //! 現在表示中のヒストリのインデックス(小さいほど新しい)
    int cmd_history_index_{-1};
    Rectangle<int> HistoryUpDown(int direction);
};

/**
 * @class
 * TerminalFileDescriptor
 * @brief 
 * 標準入出力をFileDescriptorとしてあつかえるようにしたクラス
 */
class TerminalFileDescriptor : public FileDescriptor {
  public:
    explicit TerminalFileDescriptor(Task& task, Terminal& term);
    size_t Read(void* buf, size_t len) override;

  private:
    Task& task_;
    Terminal& term_;
};

extern std::map<uint64_t, Terminal*>* terminals;
void TaskTerminal(uint64_t task_id, int64_t data);
