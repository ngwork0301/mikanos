#include "layer.hpp"

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
 * Layer::DrawToメソッド
 * 
 * @brief
 * writerに現在設定されているウィンドウの内容を描画する
 * 
 * @param [in] writer PixelWriterオブジェクト
 */
void Layer::DrawTo(PixelWriter& writer) const {
  if (window_) {
    window_->DrawTo(writer, pos_);
  }
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
 * レイヤーの位置情報をしていされた絶対座標へと更新する。再描画はしない
 * 
 * @param [in] id 移動するレイヤのID
 * @param [in] new_position 移動先の座標
 */
void LayerManager::Move(unsigned int id, Vector2D<int> new_position) {
  FindLayer(id)->Move(new_position);
}

/**
 * @fn
 * LayerManager::Moveメソッド
 * 
 * @brief
 * レイヤーの位置情報を指定された相対座標へと更新する。再描画はしない
 * 
 * @param [in] id 移動するレイヤのID
 * @param [in] pos_diff 移動量のベクトル
 */
void LayerManager::MoveRelative(unsigned int id, Vector2D<int> pos_diff) {
  FindLayer(id)->MoveRelative(pos_diff);
}

/**
 * @fn
 * LayerManager::Drawメソッド
 * 
 * @brief
 * 現在表示状態にあるレイヤーを描画する
 */
void LayerManager::Draw() const {
  for (auto layer : layer_stack_) {
    layer->DrawTo(*writer_);
  }
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
 * LayerManager::UpDownメソ度
 * 
 * @brief レイヤーの高さ方向の位置を指定された位置に移動する。
 * 
 * new_height に負の高さを指定するとレイヤーは非表示となり、
 * 0以上のを指定するとその高さとなる
 * 現在のレイヤー数以上の数値を指定した場合は、最前面のレイヤーとなる。
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
 * @param [in] writer PixelWriterへのポインタ
 */
void LayerManager::SetWriter(PixelWriter* writer) {
  writer_ = writer;
}
