/**
 * @file
 * timer.hpp
 * 
 * @brief
 * タイマー関連の処理を集めたファイル
 */
#pragma once

#include <cstdint>
#include <deque>
#include <limits>
#include <queue>
#include <vector>
#include "message.hpp"

void InitializeLAPICTimer(std::deque<Message>& msg_queue);
void StartLAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();

/**
 * @class
 * Timerクラス
 * 
 * @brief
 * タイムアウト時間を設定して、タイムアウト時に指定した値を送信する。
 */
class Timer {
  public:
    Timer(unsigned long timeout, int value);
    unsigned long Timeout() const { return timeout_; }
    int Value() const { return value_; }

  private:
    //! タイムアウト時刻
    unsigned long timeout_;
    //! タイムアウト時に送信する値
    int value_;
};

/**
 * Timerインスタンスのless演算子
 * @brief
 * タイマー優先度を比較する。タイムアウトが遠いほど優先度低 */
inline bool operator<(const Timer& lhs, const Timer& rhs) {
  return lhs.Timeout() > rhs.Timeout();
}

/**
 * @class
 * TimerManagerクラス
 * 
 * @brief
 * カウント変数tick_でタイマー処理を管理する
 */
class TimerManager {
  public:
    TimerManager(std::deque<Message>& msg_queue);
    void AddTimer(const Timer& timer);
    bool Tick();
    unsigned long CurrentTick() const { return tick_; }

  private:
    //! カウント変数
    volatile unsigned long tick_{0};
    //! 複数のタイマーを格納する優先度つきキュー
    std::priority_queue<Timer> timers_{};
    //! タイマーのタイムアウトを通知するためのメッセージキュー
    std::deque<Message>& msg_queue_;
};

extern TimerManager* timer_manager;
//! Local APICタイマーのレート [Hz = Count/sec]
extern unsigned long lapic_timer_freq;
//! TimerManager::Tick()の分解レート [Hz]
const int kTimerFreq = 100;
//! タスク切り替え用タイマーの周期 (Hz)
const int kTaskTimerPeriod = static_cast<int>(kTimerFreq * 0.02);
//! タスク切り替え用タイマーの持つ数値(他のタイマーで使わない値=int最小の値)
const int kTaskTimerValue = std::numeric_limits<int>::min();

void LAPICTimerOnInterrupt();
