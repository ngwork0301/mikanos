#include "task.hpp"

#include "asmfunc.h"
#include "timer.hpp"

alignas(16) TaskContext task_b_ctx, task_a_ctx;

namespace {
  TaskContext* current_task;
}

/**
 * @fn
 * SwitchTask関数
 * 
 * @brief 
 * コンテキストスイッチの切り替え
 */
void SwitchTask() {
  TaskContext* old_current_task = current_task;
  if (current_task == &task_a_ctx) {
    current_task = &task_b_ctx;
  } else {
    current_task = &task_a_ctx;
  }
  SwitchContext(current_task, old_current_task);
}

/**
 * @fn
 * InitializeTask関数
 * 
 * @brief 
 * タスクの初期化をおこなう
 */
void InitializeTask() {
  current_task = &task_a_ctx;

  __asm__("cli");  // 割り込み禁止
  timer_manager->AddTimer(
      Timer{timer_manager->CurrentTick() + kTaskTimerPeriod, kTaskTimerValue});
  __asm__("sti");  // 割り込み許可
}
