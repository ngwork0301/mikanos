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
    kKeyPush,
  } type;

  union {
    struct {
      unsigned long timeout;
      int value;
    } timer;

    struct {
      uint8_t modifier;
      uint8_t keycode;
      char ascii;
    } keyboard;
  } arg;
};
