/**
 * @file task.hpp
 * @brief 
 * マルチタスクに関連する実装を集めたファイル
 */
#pragma once

#include <array>
#include <cstdint>

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

//! それぞれのタスク用のコンテキスト
extern TaskContext task_b_ctx, task_a_ctx;

void SwitchTask();
void InitializeTask();
