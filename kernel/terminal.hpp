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
#include "layer.hpp"
#include "paging.hpp"
#include "task.hpp"
#include "window.hpp"

class Terminal {
  public:
    static const int kRows = 15, kColumns = 60;
    static const int kLineMax = 128;

    Terminal(Task& task, bool show_window);
    unsigned int LayerID() const { return layer_id_; }
    Rectangle<int> BlinkCursor();
    Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);
    void Print(const char* s, std::optional<size_t> len = std::nullopt);

    Task& UnderlyingTask() const { return task_; }

  private:
    //! このターミナルウィンドウのインスタンス
    std::shared_ptr<ToplevelWindow> window_;
    //! ターミナルウィンドウのレイヤーID
    unsigned int layer_id_;
    //! このターミナルのタスク
    Task& task_;
    //! ターミナルウィンドウの表示有無
    bool show_window_;
    //! 標準入出力のファイルディスクリプタ
    std::array<std::shared_ptr<FileDescriptor>, 3> files_;
    //! 終了コード
    int last_exit_code_{0};

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
    void Print(char32_t c);
    void ExecuteLine();
    WithError<int> ExecuteFile(fat::DirectoryEntry& file_entry, char* command, char* first_arg);

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
    explicit TerminalFileDescriptor(Terminal& term);
    size_t Read(void* buf, size_t len) override;
    size_t Write(const void* buf, size_t len) override;
    size_t Size() const override { return 0; }
    size_t Load(void* buf, size_t offset, size_t len) override;

  private:
    Terminal& term_;
};

/**
 * @struct 
 * AppLoadInfo構造体
 * @brief 
 * アプリをロードした後の状態情報
 */
struct AppLoadInfo {
  //! アプリのLOADセグメントの最終アドレス、エントリポイントのアドレス
  uint64_t vaddr_end, entry;
  //! 階層ページング構造
  PageMapEntry* pml4;
};

extern std::map<fat::DirectoryEntry*, AppLoadInfo>* app_loads;

void TaskTerminal(uint64_t task_id, int64_t data);
