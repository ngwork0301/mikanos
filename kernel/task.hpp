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
#include <error.hpp>
#include <vector>

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

using TaskFunc = void (uint64_t, uint64_t);

/**
 * @class
 * Taskクラス
 * 
 * @brief 
 * コンテキストスイッチできるタスクの定義
 */
class Task {
  public:
    static const size_t kDefaultStackBytes = 4096;
    Task(uint64_t id);
    Task& InitContext(TaskFunc* f, int64_t data);
    TaskContext& Context();

    uint64_t ID() const;
    Task& Sleep();
    Task& Wakeup();
  
  private:
    //! タスクID
    uint64_t id_;
    // スタック領域
    std::vector<uint64_t> stack_;
    // コンテキスト構造体
    alignas(16) TaskContext context_;
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
    TaskManager();
    Task& NewTask();
    void SwitchTask(bool current_sleep = false);

    void Sleep(Task* task);
    Error Sleep(uint64_t id);
    void Wakeup(Task* task);
    Error Wakeup(uint64_t id);
  private:
    //! タスクインスタンスのリスト
    std::vector<std::unique_ptr<Task>> tasks_{};
    //! 付与されている最新のタスクID
    uint64_t latest_id_{0};
    //! Runキュー
    std::deque<Task*> running_{};
};

extern TaskManager* task_manager;

void InitializeTask();
