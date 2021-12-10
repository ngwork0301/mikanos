/**
 * @file 
 * message.hpp
 * 
 * @brief
 * 割り込み処理でつかうメッセージングの処理を集めたファイル
 */
#pragma once

/**
 * @struct
 * Message構造体
 * 
 * @brief
 * キューに溜めるメッセージの形式を定義
 */
struct Message {
  enum Type {
    kInterruptXHCI,
    kInterruptLAPICTimer,
    kTimerTimeout,
  } type;

  union {
    struct {
      unsigned long timeout;
      int value;
    } timer;
  } arg;
};
