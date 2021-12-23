#include "task.hpp"

#include "asmfunc.h"
#include "segment.hpp"
#include "timer.hpp"

/**
 * @fn
 * Task::Taskコンストラクタ
 * 
 * @brief Construct a new Task:: Task object
 * @param id_ タスクID
 */
Task::Task(uint64_t id) : id_{id} {
}

/**
 * @fn
 * Task::Contextメソッド
 * @brief 
 * getterメソッド。このタスクのコンテキスト構造体を返す
 * @return TaskContext& 
 */
TaskContext& Task::Context() {
  return context_;
}

/**
 * @fn
 * Task::InitContextメソッド
 * 
 * @brief 
 * タスクに使用するスタック領域とコンテキスト領域のメモリ初期化
 * @param f タスクで実行する関数のポインタ
 * @param data タスクに渡す引数
 * @return Task& タスクインスタンス
 */
Task& Task::InitContext(TaskFunc* f, int64_t data) {
  // スタック領域は、デフォルトのサイズだけ確保
  const size_t stack_size = kDefaultStackBytes / sizeof(stack_[0]);
  stack_.resize(stack_size);
  uint64_t stack_end = reinterpret_cast<uint64_t>(&stack_[stack_size]);

  // コンテキストの初期化
  memset(&context_, 0, sizeof(context_));
  context_.cr3 = GetCR3();
  context_.rflags = 0x202;
  context_.cs = kKernelCS;
  context_.ss = kKernelSS;
  context_.rsp = (stack_end & ~0xflu) - 8;
  context_.rip = reinterpret_cast<uint64_t>(f);
  context_.rdi = id_;
  context_.rsi = data;

  // MXCSR のすべての例外をマスクする
  *reinterpret_cast<uint32_t*>(&context_.fxsave_area[24]) = 0x1f80;

  return *this;
}

/**
 * @fn
 * TaskManager::TaskManagerコンストラクタ
 * @brief Construct a new Task Manager:: Task Manager object
 */
TaskManager::TaskManager() {
  // これを呼び出した今実行中のコンテキストをタスクインスタンスとして登録しておく。
  NewTask();
}

/**
 * @fn
 * TaskManager::NewTaskメソッド
 * 
 * @brief 
 * 新しいTaskインスタンスを生成する
 * @return Task
 */
Task& TaskManager::NewTask() {
  ++latest_id_;
  // Taskインスタンスを生成してリストの末尾に追加
  return *tasks_.emplace_back(new Task{latest_id_});
}

/**
 * @fn
 * TaskManager::SwitchTaskメソッド
 * @brief
 * 次のタスクへの切り替えをおこなう。
 */
void TaskManager::SwitchTask() {
  size_t next_task_index = current_task_index_ + 1;
  if (next_task_index >= tasks_.size()) {
    // 次のタスクがない場合は最初にもどる
    next_task_index = 0;
  }

  Task& current_task = *tasks_[current_task_index_];
  Task& next_task = *tasks_[next_task_index];
  // 現在のタスクを置き換え
  current_task_index_ = next_task_index;

  SwitchContext(&next_task.Context(), &current_task.Context());
}

TaskManager* task_manager;

/**
 * @fn
 * InitializeTask関数
 * 
 * @brief 
 * タスクの初期化をおこなう
 */
void InitializeTask() {
  task_manager = new TaskManager();

  __asm__("cli");  // 割り込み禁止
  timer_manager->AddTimer(
      Timer{timer_manager->CurrentTick() + kTaskTimerPeriod, kTaskTimerValue});
  __asm__("sti");  // 割り込み許可
}
