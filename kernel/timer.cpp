#include "timer.hpp"

#include "interrupt.hpp"

namespace {
  const uint32_t kCountMax = 0xffffffffu;
  volatile uint32_t& lvt_timer = *reinterpret_cast<uint32_t*>(0xfee00320);
  volatile uint32_t& initial_count = *reinterpret_cast<uint32_t*>(0xfee00380);
  volatile uint32_t& current_count = *reinterpret_cast<uint32_t*>(0xfee00390);
  volatile uint32_t& divide_config = *reinterpret_cast<uint32_t*>(0xfee003e0);
}

/**
 * @fn
 * InitializeLAPICTimer関数
 * 
 * @brief
 * APICタイマーを初期化する
 */
void InitializeLAPICTimer(){
  // TimerManagerインスタンスを生成
  timer_manager = new TimerManager;

  divide_config = 0b1011; // divide 1:1 分周比は1対1でそのまま減少させる
  // lvt_timer = (0b001 << 16) | 32;  // masked, one-shot
  lvt_timer = (0b010 << 16) | InterruptVector::kLAPICTimer; // not-masked, periodic
  initial_count = 0x1000000u;
}

/**
 * @fn
 * StartLAPICTimer
 * 
 * @brief
 * APICタイマーを開始する。
 * 初期カウント数を最大値にする
 */
void StartLAPICTimer() {
  initial_count = kCountMax;
}

/**
 * @fn
 * LAPICTimerElapsed関数
 * 
 * @brief
 * 測定カウント数を取得する
 * @return uint32_t 測定カウント数
 */
uint32_t LAPICTimerElapsed() {
  return kCountMax - current_count;
}

/**
 * @fn
 * StopAPICTimer関数
 * 
 * @brief
 * APICタイマーを停止する
 * 初期カウント数を0にする。
 */
void StopLAPICTimer() {
  initial_count = 0;
}

/**
 * @fn
 * TimerManager::Tickメソッド
 * 
 * @brief
 * カウンターをインクリメントする
 */
void TimerManager::Tick() {
  ++tick_;
}

TimerManager* timer_manager;

/**
 * @fn
 * LAPICTimerOnInterrupt関数
 * 
 * @brief
 * タイマー割り込み時の動作
 */
void LAPICTimerOnInterrupt() {
  timer_manager->Tick();
}
