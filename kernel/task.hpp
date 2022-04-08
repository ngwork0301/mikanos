/**
 * @file task.hpp
 * @brief 
 * マルチタスクに関連する実装を集めたファイル
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "error.hpp"
#include "fat.hpp"
#include "message.hpp"

/**
 * @struct
 * TaskContext構造体
 * 
 * @brief 
 * タスクのコンテキストを保存するための構造体
 */
struct TaskContext {
  uint64_t cr3, rip, rflags, reserved1; // offset 0x00
  uint64_t cs, ss, fs, gs;   // offset 0x20
  uint64_t rax, rbx, rcx, rdx, rdi, rsi, rsp, rbp; // offset 0x40
  uint64_t r8, r9, r10, r11, r12, r13, r14, r15;   // offset 0x80
  std::array<uint8_t, 512> fxsave_area;   // offset 0xc0
} __attribute__((packed));

using TaskFunc = void (uint64_t, int64_t);

class TaskManager;

/**
 * @class
 * Taskクラス
 * 
 * @brief 
 * コンテキストスイッチできるタスクの定義
 */
class Task {
  public:
    static const int kDefaultLevel = 1;
    static const size_t kDefaultStackBytes = 4096;
    Task(uint64_t id);
    Task& InitContext(TaskFunc* f, int64_t data);
    TaskContext& Context();
    uint64_t& OSStackPointer();

    uint64_t ID() const;
    Task& Sleep();
    Task& Wakeup();

    void SendMessage(const Message& msg);
    std::optional<Message> ReceiveMessage();
    bool Running() { return running_; }
    unsigned int Level() { return level_; }
    std::vector<std::unique_ptr<FileDescriptor>>& Files(){ return files_; };
  
  private:
    //! タスクID
    uint64_t id_;
    //! スタック領域
    std::vector<uint64_t> stack_;
    //! コンテキスト構造体
    alignas(16) TaskContext context_;
    //! ユーザアプリ呼び出し時のOS側への復帰用のOSスタック領域
    uint64_t os_stack_ptr_;
    //! このタスクのコンテキスト内でつかうイベントキュー
    std::deque<Message> msgs_;
    //! このタスクの現在のレベル
    unsigned int level_{kDefaultLevel};
    //! このタスクが実行状態／実行可能状態であるか
    bool running_{false};
    //! このタスクがひらくファイルのファイルディスクリプタ配列
    std::vector< std::unique_ptr<FileDescriptor> > files_{};

    Task& SetLevel(int level) { level_ = level; return *this; }
    Task& SetRunning(bool running) { running_ = running; return *this; }

    friend TaskManager;
};

/**
 * @class
 * TaskManagerクラス
 * 
 * @brief 
 * 複数のTaskインスタンスをまとめて管理する。
 */
class TaskManager {
  public:
    // level: 0 = Lowest, kMaxLevel = highest
    static const int kMaxLevel = 3;

    TaskManager();
    Task& NewTask();
    void SwitchTask(const TaskContext& current_ctx, bool current_sleep = false);

    void Sleep(Task* task);
    Error Sleep(uint64_t id);
    void Wakeup(Task* task, int level = -1);
    Error Wakeup(uint64_t id, int level = -1);
    Error SendMessage(uint64_t id, const Message& msg);
    Task& CurrentTask();
  private:
    //! タスクインスタンスのリスト
    std::vector< std::unique_ptr<Task> > tasks_{};
    //! 付与されている最新のタスクID
    uint64_t latest_id_{0};
    //! レベルごとのRunキューの集合
    std::array<std::deque<Task*>, kMaxLevel + 1> running_{};
    //! 現在タスクマネージャが処理しているレベル
    int current_level_{kMaxLevel};
    //! レベルの変更フラグ
    bool level_changed_{false};

    void ChangeLevelRunning(Task* task, int level);
    Task* RotateCurrentRunQueue(bool current_sleep);
};

extern TaskManager* task_manager;

void InitializeTask();
