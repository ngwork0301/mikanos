#include "task.hpp"

#include "asmfunc.h"
#include "error.hpp"
#include "segment.hpp"
#include "timer.hpp"

/**
 * @fn
 * Task::Taskコンストラクタ
 * 
 * @brief Construct a new Task:: Task object
 * @param id_ タスクID
 */
Task::Task(uint64_t id) : id_{id}, msgs_{} {
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
 * Task::IDメソッド
 * 
 * @brief 
 * getter。このタスクのIDを取得
 * @return uint64_t タスクID
 */
uint64_t Task::ID() const {
  return id_;
}

/**
 * @fn
 * Task::Sleepメソッド
 * 
 * @brief 
 * このタスクをスリープする
 * @return Task& タスクインスタンス
 */
Task& Task::Sleep() {
  task_manager->Sleep(this);
  return *this;
}

/**
 * @fn
 * Task::Wakeupメソッド
 * 
 * @brief 
 * このタスクを実行可能状態にする
 * @return Task& タスクインスタンス
 */
Task& Task::Wakeup() {
  task_manager->Wakeup(this);
  return *this;
}

/**
 * @fn
 * Task::SendMessageメソッド
 * 
 * @brief 
 * このタスクのイベントキューにメッセージを送る
 * このタスクが寝ていれば起こす
 * @param msg メッセージインスタンス
 */
void Task::SendMessage(const Message& msg) {
  msgs_.push_back(msg);
  Wakeup();
}

/**
 * @fn
 * Task::ReceiveMessageメソッド
 * 
 * @brief 
 * イベントキューからメッセージを一つ取り出す
 * @return std::optional<Message> 無効値付きのMessageインスタンス
 */
std::optional<Message> Task::ReceiveMessage() {
  if (msgs_.empty()) {
    return std::nullopt;
  }

  auto m = msgs_.front();
  msgs_.pop_front();
  return m;
}

/**
 * @fn
 * TaskManager::TaskManagerコンストラクタ
 * @brief Construct a new Task Manager:: Task Manager object
 */
TaskManager::TaskManager() {
  // これを呼び出した今実行中のコンテキストをタスクインスタンスとしてRunキューに登録しておく。
  running_.push_back(&NewTask());
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
void TaskManager::SwitchTask(bool current_sleep) {
  // Runキューから最前のタスクを取り出し
  Task* current_task = running_.front();
  running_.pop_front();
  if (!current_sleep) {
    // スリープしないのであれば、Runキューの末尾に再度タスクを入れる
    running_.push_back(current_task);
  }
  // 次のタスク
  Task* next_task = running_.front();

  // 現在のタスクと次のタスクのコンテキストの切り替え
  SwitchContext(&next_task->Context(), &current_task->Context());
}

/**
 * @fn
 * TaskManager::Sleepメソッド
 * 
 * @brief 
 * 対象のタスクをスリープさせる
 * @param task スリープさせるタスク
 */
void TaskManager::Sleep(Task* task) {
  // スリープしたいタスクをRunキューから取り出し
  auto it = std::find(running_.begin(), running_.end(), task);

  // タスクが実行中であれば、スリープさせるためにコンテキストスイッチする
  if (it == running_.begin()) {
    SwitchTask(true);
    return;
  }

  // タスクが見つからない場合は、スキップ
  if (it == running_.end()) {
    return;
  }

  // Runキューからスリープさせるタスクを削除
  running_.erase(it);
}

/**
 * @fn
 * TaskManager::Sleepメソッド
 * @brief 
 * 対象のタスクIDのタスクをスリープさせる
 * @param id スリープさせるタスクのタスクID
 * @return Error 成功した場合は、kSuccess。対象のタスクIDがない場合は、Error::kNoSuchTask
 */
Error TaskManager::Sleep(uint64_t id) {
  // タスクリストの中から、指定したタスクIDのタスクを取り出し
  auto it = std::find_if(tasks_.begin(), tasks_.end(),
                         [id](const auto& t){ return t->ID() == id; });
  if (it == tasks_.end()) {
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  Sleep(it->get());
  return MAKE_ERROR(Error::kSuccess);
}

/**
 * @fn
 * TaskManager::Wakeupメソッド
 * 
 * @brief 
 * スリープ状態のタスクを実行可能状態へ戻す
 * @param task 実行可能状態にもどすタスク
 */
void TaskManager::Wakeup(Task* task) {
  // 対象のタスクをRunキューにないか探す
  auto it = std::find(running_.begin(), running_.end(), task);
  if (it == running_.end()) {
    // Runキューに入っていない場合は、スリープ状態のため、Runキューに加えて実行可能状態にする。
    running_.push_back(task);
  }
}

/**
 * @fn
 * TaskManager::Wakeupメソッド
 * 
 * @brief 
 * スリープ状態のタスクを実行可能状態へ戻す
 * @param id タスクID
 * @return Error 成功した場合は、kSuccess。対象のタスクIDがない場合は、Error::kNoSuchTask
 */
Error TaskManager::Wakeup(uint64_t id) {
  // タスクリストの中から、指定したタスクIDのタスクを取り出し
  auto it = std::find_if(tasks_.begin(), tasks_.end(),
                         [id](const auto& t){ return t->ID() == id; });
  if (it == tasks_.end()) {
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  Wakeup(it->get());
  return MAKE_ERROR(Error::kSuccess);
}

/**
 * @fn
 * TaskManager::SendMessageメソッド
 * 
 * @brief 
 * 指定したタスクIDのタスクのイベントキューにメッセージを追加。タスクは寝ていれば起こす
 * @param id タスクID
 * @param msg 追加するメッセージインスタンス
 * @return Error 指定したタスクが見つからなかったらError:::kNoSuchTask。成功したらError::kSuccess
 */
Error TaskManager::SendMessage(uint64_t id, const Message& msg) {
  // 指定したタスクIDのタスクインスタンスを取得
  auto it = std::find_if(tasks_.begin(), tasks_.end(),
                         [id](const auto& t){ return t->ID() == id; });
  // タスクが見つからなかったときはエラー
  if (it == tasks_.end()) {
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  (*it)->SendMessage(msg);
  return MAKE_ERROR(Error::kSuccess);
}

/**
 * @fn
 * TaskManager::CurrentTaskメソッド
 * 
 * @brief 
 * 現在実行中のタスクのインスタンスを返す
 * @return Task& 現在実行中のタスク
 */
Task& TaskManager::CurrentTask() {
  return *running_.front();
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
