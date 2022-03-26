#include "timer.hpp"

#include "acpi.hpp"
#include "interrupt.hpp"
#include "task.hpp"
#include "logger.hpp"

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

  // ACPI PMタイマーで0.1秒計測して、その間のLocal APICのカウント数を調べる
  StartLAPICTimer();
  acpi::WaitMilliseconds(100);; // divide 1:1 分周比は1対1でそのまま減少させる
  const auto elapsed = LAPICTimerElapsed();
  StopLAPICTimer();

  // 0.1秒のカウント数なので、10をかけてHzに変換
  lapic_timer_freq = static_cast<unsigned long>(elapsed) * 10;

  divide_config = 0b1011;
  lvt_timer = (0b10 << 16) | InterruptVector::kLAPICTimer; // not-masked, periodic
  //! kTimeFreqで定義する分解能のカウント数を初期値に設定して、その数だけカウントしたら1増えるようにする
  initial_count = lapic_timer_freq / kTimerFreq;
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
 * Timer::Timerコンストラクタ
 * 
 * @brief
 * Timerインスタンスを生成する。
 * @param [in] timeout タイムアウト値
 * @param [in] value タイマーに関連付けする値
 * @param [in] task_id タイムアウトを通知するタスクID
 */
Timer::Timer(unsigned long timeout, int value, uint64_t task_id)
    : timeout_{timeout}, value_{value}, task_id_{task_id} {
}

/**
 * @fn
 * TimerManager::TimerManagerコンストラクタ
 * 
 * @brief
 * TimerManagerインスタンスを生成する
 */
TimerManager::TimerManager() {
  // 番兵として１番タイムアウト値の大きなタイマーインスタンスを加えておく
  timers_.push(Timer{std::numeric_limits<unsigned long>::max(), 0, 0});
}

/**
 * @fn
 * TimerManager::AddTimerメソッド
 * 
 * @brief
 * Timerインスタンスを加える
 */
void TimerManager::AddTimer(const Timer& timer) {
  timers_.push(timer);
}

/**
 * @fn
 * TimerManager::Tickメソッド
 * 
 * @brief
 * カウンターをインクリメントする
 * @return bool タスクタイマーのタイムアウト有無
 */
bool TimerManager::Tick() {
  ++tick_;

  //! タスクタイマーのタイムアウト発生有無
  bool task_timer_timeout = false;

  // タイムアウトの有無をチェック
  while (true) {
    const auto& t = timers_.top();
    if (t.Timeout() > tick_) {
      // １番優先度のたかいタイマーがタイムアウトしていないときはそれ以降もタイムアウトしていないので終わり
      break;
    }

    // タスクタイマーがタイムアウトしていたときは特別扱い
    if (t.Value() == kTaskTimerValue) {
      // フラグを立てるだけ
      task_timer_timeout = true;
      // イベントキューに入れないでスキップして入れ直し。
      timers_.pop();
      timers_.push(Timer{tick_ + kTaskTimerPeriod, kTaskTimerValue, 1});
      continue;
    }

    Message m{Message::kTimerTimeout};
    m.arg.timer.timeout = t.Timeout();
    m.arg.timer.value = t.Value();
    // 対象のタスクのイベントキューにkTimerTimeoutメッセージを入れる
    task_manager->SendMessage(t.TaskID(), m);

    // タイムアウトしたのでtimers_からこのタイマーを取り除く
    timers_.pop();
  }

  return task_timer_timeout;
}

TimerManager* timer_manager;
unsigned long lapic_timer_freq;

/**
 * @fn
 * LAPICTimerOnInterrupt関数
 * 
 * @brief
 * タイマー割り込みの処理をして、必要ならタイムを切り替える。
 * @param [in] ctx_stack 割り込みフレームをつかって構築したコンテキスト構造体
 */
extern "C" void LAPICTimerOnInterrupt(const TaskContext& ctx_stack) {
  const bool task_timer_timeout = timer_manager->Tick();
  NotifyEndOfInterrupt();

  if (task_timer_timeout) {
    task_manager->SwitchTask(ctx_stack);
  }
}
