#include "layer.hpp"
#include <algorithm>
#include <limits>
#include "console.hpp"
#include "task.hpp"
#include "timer.hpp"
#include "logger.hpp"

namespace {
  /**
   * @fn
   * EraseIf関数
   * @brief 
   * 第一引数で指定されたリストの中から第２引数で指定された要素を探してあれば削除する。
   * @tparam T リスト
   * @tparam U 削除対象のインスタンス
   * @param c リスト
   * @param pred 削除対象のインスタンス
   */
  template <class T, class U>
  void EraseIf(T& c, const U& pred) {
    auto it = std::remove_if(c.begin(), c.end(), pred);
    c.erase(it, c.end());
  }
} // namespace

/**
 * @fn
 * Layerコンストラクタ
 * 
 * @brief
 * 指定した ID を持つレイヤーを生成する 
 */
Layer::Layer(unsigned int id) : id_{id} {
}

/**
 * @fn
 * Layer::IDメソッド
 * 
 * @brief
 * このインスタンスのIDを返す
 * @return id
 */
unsigned int Layer::ID() const {
  return id_;
}

/**
 * @fn
 * Layer::SetWindowメソッド
 * 
 * @brief
 * ウィンドウを設定する。既存のウィンドウはこのレイヤーから外れる
 * @param [in] window 設定するウィンドウの共有スマートポインタ
 * @return 設定後のLayer自身
 */
Layer& Layer::SetWindow(const std::shared_ptr<Window>& window) {
  window_ = window;
  return *this;
}

/**
 * @fn
 * Layer::GetWindowメソッド
 * 
 * @brief
 * 設定されたウィンドウを返す
 * @return window
 */
std::shared_ptr<Window> Layer::GetWindow() const {
  return window_;
}

/**
 * @fn
 * Layer::Moveメソッド
 * 
 * @brief
 * レイヤーの位置情報を指定された絶対座標へと更新する。再描画はしない
 * 
 * @param [in] pos 移動先の位置Vector2D(x, y)
 * @return Layer自身のポインタ
 */
Layer& Layer::Move(Vector2D<int> pos) {
  pos_ = pos;
  return *this;
}

/**
 * @fn
 * Layer::MoveRelativeメソッド
 * 
 * @brief
 * レイヤーの位置情報を指定された相対座標へと更新する。再描画しない
 * 
 * @param [in] pos_diff 移動量Vector2D(x, y)
 * @return Layer自身のポインタ
 */
Layer& Layer::MoveRelative(Vector2D<int> pos_diff) {
  pos_ += pos_diff;  // Vector2Dのoperator+=により足し算可能
  return *this;
}

/**
 * @fn
 * Layer::GetPositionメソッド
 * 
 * @brief
 * レイヤーの原点座標を取得する。
 * @return 原点座標
 */
Vector2D<int> Layer::GetPosition() const{
  return pos_;
}

/**
 * @fn
 * Layer::DrawToメソッド
 * 
 * @brief
 * writerに現在設定されているウィンドウの内容を描画する
 * 
 * @param [in] screen 描画するFrameBuffer
 * @param [in] area 描画する範囲のRectangle
 */
void Layer::DrawTo(FrameBuffer& screen, const Rectangle<int>& area) const {
  if (window_) {
    window_->DrawTo(screen, pos_, area);
  }
}

/**
 * @fn
 * Layer::SetDraggableメソッド
 * 
 * @brief
 * このレイヤーに移動可能フラグをセットする
 * @param [in] draggable 移動可能かどうか
 * @return Layerインスタンス
 */
Layer& Layer::SetDraggable(bool draggable){
  draggable_ = draggable;
  return *this;
}

/**
 * @fn
 * Layer::IsDraggableメソッド
 * 
 * @brief
 * このレイヤーが移動可能かどうかを返却
 * @return bool 移動可能かどうか
 */
bool Layer::IsDraggable() const{
  return draggable_;
}

/**
 * @fn
 * LayerManager::NewLayerメソッド
 * 
 * @brief
 * 新しいレイヤーを生成して参照を返す。 
 * 新しく生成されたレイヤーの実体は LayerManager 内部のコンテナで保持される。
 * @retval Layer
 */
Layer& LayerManager::NewLayer(){
  ++latest_id_;
  // layers_にインスタンスを追加して、参照を返す
  // 共有不可のスマートポインタを「*」でLayer&型に変換している。
  return *layers_.emplace_back(new Layer{latest_id_});
}

/**
 * @fn
 * LayerManager::FindLayerメソッド
 * 
 * @brief
 * 指定されたIDのレイヤーを探す
 * @param [in] id 見つけたいレイヤーのID
 * @retval Layer*
 */
Layer* LayerManager::FindLayer(unsigned int id) {
  // 述語predicateをラムダ式で定義
  // この関数の引数idと、elmのIDが等しいかどうかを返す
  auto pred = [id](const std::unique_ptr<Layer>& elm) {
    return elm->ID() == id;
  };
  auto it = std::find_if(layers_.begin(), layers_.end(), pred);
  if (it == layers_.end()) {
    // 見つからない場合はイテレータが末尾になるので、nullptrを返す。
    return nullptr;
  }
  return it->get();
}

/**
 * @fn
 * LayerManager::Moveメソッド
 * 
 * @brief
 * レイヤーの位置情報をしていされた絶対座標へと更新して、再描画する
 * 
 * @param [in] id 移動するレイヤのID
 * @param [in] new_position 移動先の座標
 */
void LayerManager::Move(unsigned int id, Vector2D<int> new_pos) {
  auto layer = FindLayer(id);
  const auto window_size = layer->GetWindow()->Size();
  const auto old_pos = layer->GetPosition();
  layer->Move(new_pos);
  // 移動元の再描画
  Draw({old_pos, window_size});
  // 移動先の再描画
  Draw(id);
}

/**
 * @fn
 * LayerManager::MoveRelativeメソッド
 * 
 * @brief
 * レイヤーの位置情報を指定された相対座標へと更新して、再描画する
 * 
 * @param [in] id 移動するレイヤのID
 * @param [in] pos_diff 移動量のベクトル
 */
void LayerManager::MoveRelative(unsigned int id, Vector2D<int> pos_diff) {
  auto layer = FindLayer(id);
  const auto window_size = layer->GetWindow()->Size();
  const auto old_pos = layer->GetPosition();
  layer->MoveRelative(pos_diff);
  // 移動元の再描画
  Draw({old_pos, window_size});
  // 移動先の再描画
  Draw(id);
}

/**
 * @fn
 * LayerManager::Drawメソッド
 * 
 * @brief
 * 現在表示状態にあるレイヤーのうち、引数で指定した範囲を再描画する
 * @param [in] area 描画範囲。Rectangle<int>
 */
void LayerManager::Draw(const Rectangle<int>& area) const {
  for (auto layer : layer_stack_) {
    // まずバックバッファに描画
    layer->DrawTo(back_buffer_, area);
  }
  // バックバッファを一気にフレームバッファに反映
  screen_->Copy(area.pos, back_buffer_, area);
}

/**
 * @fn
 * LayerManager::Drawメソッド
 * 
 * @brief
 * 現在表示状態にあるレイヤーのうち、引数で指定したIDのレイヤーと、
 * その前面にあるレイヤーを再描画する。
 * @param [in] id 再描画するレイヤーのID
 */
void LayerManager::Draw(unsigned int id) const {
  // 位置{0, 0}、範囲{-1, -1}にしたRectangleを生成して、オーバーロードしているLayerManager::Drawを呼び出し
  Draw(id, {{0, 0}, {-1, -1}});
}

/**
 * @fn
 * LayerManager::Drawメソッド
 * @brief 
 * 現在表示状態にあるレイヤーのうち、引数で指定したIDのレイヤーの特定範囲を再描画する
 * @param id 再描画するレイヤーのID
 * @param area 再描画範囲
 */
void LayerManager::Draw(unsigned int id, Rectangle<int> area) const{
  //! 再描画する範囲があるかどうか
  bool draw = false;
  //! 指定したIDのレイヤーのウィンドウ範囲
  Rectangle<int> window_area;
  for (auto layer : layer_stack_) {
    if (layer->ID() == id) {
      window_area.size = layer->GetWindow()->Size();
      window_area.pos = layer->GetPosition();
      if (area.size.x >= 0 || area.size.y >= 0) {
        // area.posを相対位置から、絶対位置に変更
        area.pos = area.pos + window_area.pos;
        // window_areaを指定したIDのレイヤー全体と、areaで指定した範囲の重なった部分に限定して置換
        window_area = window_area & area;
      }
      // 対象のレイヤーがあったので、描画フラグをtrue
      draw = true;
    }
    if (draw) {
      // まずはバックバッファに描画
      layer->DrawTo(back_buffer_, window_area);
    }
  }
  // バックバッファを一気にフレームバッファに反映
  screen_->Copy(window_area.pos, back_buffer_, window_area);
}


/**
 * @fn
 * LayerManager::Hideメソッド
 * 
 * @brief
 * レイヤーを非表示とする。
 * @param [in] id 非表示にするレイヤーのID
 */
void LayerManager::Hide(unsigned int id) {
  auto layer = FindLayer(id);
  auto pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
  if (pos != layer_stack_.end()) {
    layer_stack_.erase(pos);
  }
}

/**
 * @fn
 * LayerManager::UpDownメソッド
 * 
 * @brief レイヤーの高さ方向の位置を指定された位置に移動する。
 * new_height に負の高さを指定するとレイヤーは非表示となり、
 * 0以上のを指定するとその高さとなる
 * 現在のレイヤー数以上の数値を指定した場合は、最前面のレイヤーとなる。
 * @param [in] id レイヤーID
 * @param [in] new_height 重なりの高さ
 */
void LayerManager::UpDown(unsigned int id, int new_height) {
  // 高さにマイナスの値をいれたら、非表示にして終了
  if (new_height < 0) {
    Hide(id);
    return ;
  }
  // 現在のスタックの最上位以上の高さは、最上位の高さに置き換え
  if (new_height > layer_stack_.size()) {
    new_height = layer_stack_.size();
  }

  auto layer = FindLayer(id);
  auto old_pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
  auto new_pos = layer_stack_.begin() + new_height;

  // 既存のスタックに存在しないレイヤーの場合、指定された高さにインサートするだけ
  if (old_pos == layer_stack_.end()) {
    layer_stack_.insert(new_pos, layer);
    return;
  }
  // 最前面が指定された場合、既存のスタックから取り出したあとの最後の位置にインサートするため、一つ繰り下げる
  if (new_pos == layer_stack_.end()) {
    --new_pos;
  }
  // 既存のスタックから取り出し
  layer_stack_.erase(old_pos);
  // スタックの指定した位置に割り込み
  layer_stack_.insert(new_pos, layer);
}

/**
 * @fn
 * LayerManager::SetWriterメソッド
 * 
 * @brief
 * Draw メソッドなどで描画する際の描画先を設定する
 * 
 * @param [in] writer FrameBufferへのポインタ
 */
void LayerManager::SetWriter(FrameBuffer* screen) {
  screen_ = screen;

  // バックバッファを初期化
  FrameBufferConfig back_config = screen->Config();
  back_config.frame_buffer = nullptr;
  back_buffer_.Initialize(back_config);
}

/**
 * @fn
 * LayerManager::FindLayerByPositionメソッド
 * 
 * @brief
 * 指定した座標にある最前面レイヤーを探す
 * @param [in] pos 取得したいレイヤーのある位置 Vector2D<int>
 * @param [in] exclude_id 対象外にするレイヤーのID
 */
 Layer* LayerManager::FindLayerByPosition(Vector2D<int> pos, unsigned int exclude_id) const {
  auto pred = [pos, exclude_id](Layer* layer) {
    if (layer->ID() == exclude_id) {
      return false;
    }
    const auto& win = layer->GetWindow();
    if (!win) {
      return false;
    }
    const auto win_pos = layer->GetPosition();
    const auto win_end_pos = win_pos + win->Size();
    // 指定された範囲がこのレイヤーのウィンドウ内であればtrueを返す
    return win_pos.x <= pos.x && pos.x < win_end_pos.x &&
           win_pos.y <= pos.y && pos.y < win_end_pos.y;
  };
  // レイヤースタックの逆順に対象のレイヤーを探す
  auto it = std::find_if(layer_stack_.rbegin(), layer_stack_.rend(), pred);
  if (it == layer_stack_.rend()) {
    // 一番下のレイヤーが該当した場合は、移動させたくないため、対象外とする。
    return nullptr;
  }
  return *it;
 }

/**
 * @fn
 * LayerManager::GetHeightメソッド
 * 
 * @brief 
 * 指定されたレイヤーの現在のスタック上の高さを返す
 * @param id 高さを取得したいレイヤーのID
 * @return int 高さ。見つからない場合は-1を返す
 */
int LayerManager::GetHeight(unsigned int id){
  // 指定したIDのレイヤーを探査
  for (int h = 0; h < layer_stack_.size(); ++h) {
    if (layer_stack_[h]->ID() == id) {
      return h;
    }
  }
  // 指定したレイヤーが見つからない場合
  return -1;
}

/**
 * @fn
 * LayerManager::RemoveLayerメソッド
 * @brief 
 * 指定されたレイヤーを削除する
 * @param id レイヤーID
 */
void LayerManager::RemoveLayer(unsigned int id) {
  // layer_stack_から削除
  Hide(id);

  // 指定されたIDが存在するかどうかを調べるラムダ式
  auto pred = [id](const std::unique_ptr<Layer>& elem) {
    return elem->ID() == id;
  };
  EraseIf(layers_, pred);
}

/**
 * @fn ActiveLayer::ActiveLayerコンストラクタ
 * @brief Construct a new Active Layer:: Active Layer object
 * @param manager 
 */
ActiveLayer::ActiveLayer(LayerManager& manager) : manager_{manager} {
}

/**
 * @fn
 * ActiveLayer::SetMouseLayerメソッド
 * 
 * @brief 
 * マウスレイヤーを入れるsetter
 * @param mouse_layer マウスレイヤーのレイヤーID
 */
void ActiveLayer::SetMouseLayer(unsigned int mouse_layer) {
  mouse_layer_ = mouse_layer;
}

/**
 * @fn
 * SendWindowActiveMessage関数
 * @brief 
 * 指定されたレイヤーが属するタスクに、kWindowActivateメッセージを送る
 * @param layer_id レイヤーID
 * @param activate 活性/非活性
 * @return Error 
 */
Error SendWindowActiveMessage(unsigned int layer_id, int activate) {
  auto task_it = layer_task_map->find(layer_id);
  if (task_it == layer_task_map->end()) {
    return MAKE_ERROR(Error::kNoSuchTask);
  }

  Message msg{Message::kWindowActive};
  msg.arg.window_active.activate = activate;
  // task_it: ( first: layer_id, second: task_id )
  return task_manager->SendMessage(task_it->second, msg);
}

/**
 * @fn
 * ActiveLayer::Activateメソッド
 * 
 * @brief 
 * 指定したレイヤーIDのレイヤーを活性化する
 * @param layer_id 活性化するレイヤーID
 */
void ActiveLayer::Activate(unsigned int layer_id) {
  // すでに活性化済みの場合
  if (active_layer_ == layer_id) {
    return;
  }
  // active_layer_ がすでに設定されていた場合、そのレイヤは非活性にする。
  if (active_layer_ > 0) {
    Layer* layer = manager_.FindLayer(active_layer_);
    layer->GetWindow()->Deactivate();
    manager_.Draw(active_layer_);
    // 非活性になったレイヤーに通知イベントを送る。0はdeactivate
    SendWindowActiveMessage(active_layer_, 0);
  }

  active_layer_ = layer_id;
  if (active_layer_ > 0) {
    // 0より大きいときは指定したレイヤーを活性化する
    Layer* layer = manager_.FindLayer(active_layer_);
    layer->GetWindow()->Activate();
    // stack_layer_にない場合は追加するため、一旦最下層に追加
    manager_.UpDown(active_layer_, 0);
    // マウスレイヤ(常に最前面)よりひとつ手前まで持ってくる
    manager_.UpDown(active_layer_, manager_.GetHeight(mouse_layer_) - 1);

    manager_.Draw(active_layer_);
    // 活性になったレイヤーに通知イベントを送る。1はactivateフラグ
    SendWindowActiveMessage(active_layer_, 1);
  }
}

 namespace {
  FrameBuffer* screen;
}

LayerManager* layer_manager;
ActiveLayer* active_layer;
//! レイヤーとタスクのマッピング定義
std::map<unsigned int, uint64_t>* layer_task_map;

/**
 * @fn
 * InitializeLayer関数
 * 
 * @brief
 * 画面に描画するウィンドウの重なりを管理するlayer_managerを初期化する。
 */
void InitializeLayer() {
  // スクリーンサイズを設定
  const auto screen_size = ScreenSize();

  // 背景ウィンドウを生成
  auto bgwindow = std::make_shared<Window>(
      screen_size.x, screen_size.y, screen_config.pixel_format);
  auto bgwriter = bgwindow->Writer();

  // 背景の描画処理
  DrawDesktop(*bgwriter);

 // コンソール用のウィンドウを生成
  auto console_window = std::make_shared<Window>(
      Console::kColumns * 8, Console::kRows * 16, screen_config.pixel_format);
  console->SetWindow(console_window);

  // FrameBufferインスタンスの生成
  screen = new FrameBuffer;
  if (auto err = screen->Initialize(screen_config)) {
    Log(kError, "failed to initialize frame buffer: %s at %s:%d\n",
      err.Name(), err.File(), err.Line());
  }

  // レイヤーマネージャの生成
  layer_manager = new LayerManager;
  layer_manager->SetWriter(screen);

  auto bglayer_id = layer_manager->NewLayer()
      .SetWindow(bgwindow)
      .Move({0,0})
      .ID();
  console->SetLayerID(layer_manager->NewLayer()
      .SetWindow(console_window)
      .Move({0, 0})
      .ID());
  
  layer_manager->UpDown(bglayer_id, 0);
  layer_manager->UpDown(console->LayerID(), 1);

  active_layer = new ActiveLayer{*layer_manager};

  layer_task_map = new std::map<unsigned int, uint64_t>;
}

/**
 * @fn
 * ProcessLayerMessage関数
 * 
 * @brief 
 * レイヤ操作要求を実際に処理する
 * @param msg 
 */
void ProcessLayerMessage(const Message& msg){
  const auto& arg = msg.arg.layer;
  switch (arg.op) {
    case LayerOperation::Move:
      layer_manager->Move(arg.layer_id, {arg.x, arg.y});
      break;
    case LayerOperation::MoveRelative:
      layer_manager->MoveRelative(arg.layer_id, {arg.x, arg.y});
      break;
    case LayerOperation::Draw:
      layer_manager->Draw(arg.layer_id);
      break;
    case LayerOperation::DrawArea:
      layer_manager->Draw(arg.layer_id, {{arg.x, arg.y}, {arg.w, arg.h}});
      break;
  }
}
