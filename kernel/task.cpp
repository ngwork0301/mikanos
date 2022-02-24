#include "task.hpp"

#include "asmfunc.h"
#include "error.hpp"
#include "segment.hpp"
#include "timer.hpp"

namespace {
  /**
   * @fn
   * Erase関数
   * 
   * @brief 
   * 第一引数の配列(Runキューなど)から対象の要素を取り除く
   * @tparam T 削除対象の配列の型
   * @tparam U 削除対象の要素の型
   * @param c 削除対象の配列
   * @param value 削除対象の要素
   */
  template <class T, class U>
  void Erase(T& c, const U& value) {
    auto it = std::remove(c.begin(), c.end(), value);
    c.erase(it, c.end());
  }

  /**
   * @fn
   * TaskIdle関数
   * 
   * @brief 
   * アイドルタスクの動作の中身を実装。
   * 永遠にhltオペランドを実行する。
   * @param task_id タスクID
   * @param data タスクが使用するデータ。このタスクでは未使用
   */
  void TaskIdle(uint64_t task_id, int64_t data) {
    while (true) __asm__("hlt");
  }
} // namespace

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
  // これを呼び出した今実行中のコンテキストをタスクインスタンスとして生成
  Task& task = NewTask()
    .SetLevel(current_level_)
    .SetRunning(true);
  // 現在処理しているレベルのRunキューにタスクを追加
  running_[current_level_].push_back(&task);

  // Runキューが空にならないように、アイドルタスクはかならず一番下のレベルで動くようにする。
  Task& idle = NewTask()
    .InitContext(TaskIdle, 0)
    .SetLevel(0)
    .SetRunning(true);
  // 一番下のレベルのRunキュー(レベル0:アイドルタスク専用)にアイドルタスクを追加
  running_[0].push_back(&idle);

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
 * @param [in] current_cts 切り替え先タスクのコンテキスト構造体
 * @param [in] current_sleep 切り替え元タスクをスリープするかどうか
 */
void TaskManager::SwitchTask(const TaskContext& current_ctx,
                             bool current_sleep) {
  // 切り替え元のコンテキスト構造体のアドレスを取得して
  // 受け取ったコンテキスト構造体で書き換える
  TaskContext& task_ctx = task_manager->CurrentTask().Context();
  memcpy(&task_ctx, &current_ctx, sizeof(TaskContext));

  Task* current_task = RotateCurrentRunQueue(current_sleep);
  if (&CurrentTask() != current_task) {
    RestoreContext(&CurrentTask().Context());
  }
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
  // すでにスリープしてあれば、即return
  if (!task->Running()) {
    return;
  }

  task->SetRunning(false);
  if (task == running_[current_level_].front()) {
    // 現在実行中の場合は、スリープさせるために次のタスクへ即時コンテキストスイッチする
    Task* current_task = RotateCurrentRunQueue(true);
    SwitchContext(&CurrentTask().Context(), &current_task->Context());  // bool引数は、切り替え元のタスクをスリープさせるかどうか
    return;
  }
  // Runキューからスリープさせるタスクを削除
  Erase(running_[task->Level()], task);
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
 * スリープ状態のタスクを指定したレベルで実行可能状態へ戻す。
 * レベルの変更も必要に応じておこなう。
 * @param [in] task 実行可能状態にもどすタスク
 * @param [in] level 起床する対象のタスクのレベル。0未満の数値を入れた場合は、そのタスクの従来のレベルを使用する
 */
void TaskManager::Wakeup(Task* task, int level) {
  if (task->Running()) {
    // 対象のタスクが実行中の場合、動作中のレベル変更のため、ChangeLevelRuninngメソッドで処理
    ChangeLevelRunning(task, level);
    return;
  }
  if (level < 0) {
    // levelに0未満の数値を入れた場合は、指定したタスクにもともと設定されていたレベルをつかう
    level = task->Level();
  }

  task->SetLevel(level);
  task->SetRunning(true);

  // 指定したタスクをRunキューに加えて、実行可能状態にする。
  running_[level].push_back(task);
  if (level > current_level_) {
    // レベルの変更があった場合はフラグを立てて、つぎのSwitchTaskメソッドでレベル変更
    level_changed_ = true;
  }
  return;
}

/**
 * @fn
 * TaskManager::Wakeupメソッド
 * 
 * @brief 
 * スリープ状態のタスクを指定したレベルの実行可能状態へ戻す
 * @param [in] id タスクID
 * @param [in] level 変更先のレベル
 * @return Error 成功した場合は、kSuccess。対象のタスクIDがない場合は、Error::kNoSuchTask
 */
Error TaskManager::Wakeup(uint64_t id, int level) {
  // タスクリストの中から、指定したタスクIDのタスクを取り出し
  auto it = std::find_if(tasks_.begin(), tasks_.end(),
                         [id](const auto& t){ return t->ID() == id; });
  if (it == tasks_.end()) {
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  Wakeup(it->get(), level);
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
  return *running_[current_level_].front();
}

/**
 * @fn
 * TaskManager::ChangeLevelRunningメソッド
 * 
 * @brief 
 * 実行中のタスクのレベル変更
 * @param [in] task 変更対象のタスク
 * @param [in] level 変更先レベル
 */
void TaskManager::ChangeLevelRunning(Task* task, int level){
  if (level < 0 || level == task->Level()) {
    // レベルの変更がない場合はなにもしない
    return;
  }

  // 実行中のタスクではない場合
  if (task != running_[current_level_].front()){
    // change level of other task
    // 指定したタスクを実行中のレベルのRunキューから削除
    Erase(running_[task->Level()], task);
    // 変更先のレベルのRunキューに対象のタスクを追加
    running_[level].push_back(task);
    task->SetLevel(level);
    if (level > current_level_) {
      // レベルの変更があった場合は、次のSwitchTaskで変更
      level_changed_ = true;
    }
    return;
  }

  // 実行中のタスクの場合
  // change level myself
  // 現在のレベルのRunキューから現在のタスクを取り出し
  running_[current_level_].pop_front();
  // 変更先のレベルのRunキューの最前(実行中のため)に対象のタスクを追加
  running_[level].push_front(task);
  task->SetLevel(level);
  if (level >= current_level_) {
    current_level_ = level;
  } else {
    // レベルを下げたときは、実行中のタスクを終えたら、次のSwitchTaskメソッドで
    // もともとの上のレベルのRunキューに他のタスクがあればレベルを戻す必要があるので
    // レベル変更フラグを立てておく。
    current_level_ = level;
    level_changed_ = true;
  }
}

/**
 * @fn
 * TaskManager::RotateCurrentRunQueueメソッド
 * @brief 
 * 現在のRunキューを次に切り替える
 * @param current_sleep 現在のタスクをスリープするかどうか
 * @return Task* 
 */
Task* TaskManager::RotateCurrentRunQueue(bool current_sleep) {
  // Runキューから最前のタスクを取り出し
  auto& level_queue = running_[current_level_];
  Task* current_task = level_queue.front();
  level_queue.pop_front();
  if (!current_sleep) {
    // スリープしないのであれば、Runキューの末尾に再度タスクを入れる
    level_queue.push_back(current_task);
  }
  if (level_queue.empty()) {
    // このレベルのRunキューが空になったら、レベルの変更
    level_changed_ = true;
  }

  if (level_changed_) {
    // レベルの変更フラグをfalseに戻す
    level_changed_ = false;
    // 最高レベルから順に下げていく
    for (int lv = kMaxLevel; lv >= 0; --lv) {
      if (!running_[lv].empty()) {
        current_level_ = lv;
        break;
      }
    }
  }

  return current_task;
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
