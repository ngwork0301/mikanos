#include "mouse.hpp"

#include <limits>
#include <memory>
#include "graphics.hpp"
#include "layer.hpp"
#include "task.hpp"
#include "usb/classdriver/mouse.hpp"

namespace {
  //! マウスカーソルの形のデータ
  const char mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1] {
    "@              ",
    "@@             ",
    "@.@            ",
    "@..@           ",
    "@...@          ",
    "@....@         ",
    "@.....@        ",
    "@......@       ",
    "@.......@      ",
    "@........@     ",
    "@.........@    ",
    "@..........@   ",
    "@...........@  ",
    "@............@ ",
    "@......@@@@@@@@",
    "@......@       ",
    "@....@@.@      ",
    "@...@ @.@      ",
    "@..@   @.@     ",
    "@.@    @.@     ",
    "@@      @.@    ",
    "@       @.@    ",
    "         @.@   ",
    "         @@@   ",
  };
}

void DrawMouseCursor(PixelWriter* pixel_writer, Vector2D<int> position) {
  for (int dy = 0; dy < kMouseCursorHeight; ++dy) {
    for (int dx = 0; dx < kMouseCursorWidth; ++dx) {
      if (mouse_cursor_shape[dy][dx] == '@') {
        pixel_writer->Write(position + Vector2D<int>{dx, dy}, {0, 0, 0});
      } else if (mouse_cursor_shape[dy][dx] == '.') {
        pixel_writer->Write(position + Vector2D<int>{dx, dy}, {255, 255, 255});
      } else {
        pixel_writer->Write(position + Vector2D<int>{dx, dy}, kMouseTransparentColor);
      }
    }
  }
}

/**
 * @fn
 * Mouse::Mouseコンストラクタ
 * 
 * @brief
 * Mouseインスタンスを生成する。
 * @param [in] layer_id LayerManagerから取得したマウス自身のレイヤーのID
 */
Mouse::Mouse(unsigned int layer_id) : layer_id_{layer_id} {
}

/**
 * @fn
 * Mouse::SetPositionメソッド
 * 
 * @brief
 * マウスの位置を移動する。
 * @param [in] position 移動先の位置
 */
void Mouse::SetPosition(Vector2D<int> position) {
  position_ = position;
  layer_manager->Move(layer_id_, position_);
}

/**
 * @fn
 * FindActiveLayerTask関数
 * @brief 
 * 現在アクティブなレイヤーと、そのレイヤーのタスクを取得する
 * @return std::tuple<Layer*, uint64_t> 
 */
std::tuple<Layer*, uint64_t> FindActiveLayerTask() {

  // アクティブレイヤーを取得
  const auto act = active_layer->GetActive();
  if (!act) {
    return { nullptr, 0 };
  }
  const auto layer = layer_manager->FindLayer(act);
  if (!layer) {
    return { nullptr, 0 };
  }

  // アクティブレイヤーに対応するタスクを取得
  const auto task_it = layer_task_map->find(act);
  if (task_it == layer_task_map->end()) {
    return { layer, 0 };
  }
  return { layer, task_it->second };
}

/**
 * @fn
 * SendMouseMessage関数
 * @brief 
 * アクティブウィンドウにマウスイベントを送信する。
 * @param newpos 移動先のマウス座標
 * @param posdiff マウスの移動量
 * @param buttons ボタン押下状況
 * @param previous_buttons 一つ前のマウスのボタン押下状態
 */
void SendMouseMessage(Vector2D<int> newpos, Vector2D<int> posdiff,
                      uint8_t buttons, uint8_t previous_buttons) {
  const auto [ layer, task_id ] = FindActiveLayerTask();
  if (!layer || !task_id) {
    return;
  }

  //! 相対移動量
  const auto relpos = newpos - layer->GetPosition();
  // 移動していれば
  if (posdiff.x != 0 || posdiff.y != 0) {
    Message msg{Message::kMouseMove};
    msg.arg.mouse_move.x = relpos.x;
    msg.arg.mouse_move.y = relpos.y;
    msg.arg.mouse_move.dx = posdiff.x;
    msg.arg.mouse_move.dy = posdiff.y;
    msg.arg.mouse_move.buttons = buttons;
    task_manager->SendMessage(task_id, msg);
  }

  // ボタンイベントの変化があれば
  if (previous_buttons != buttons) {
    // XOR演算で変化したビットのみを抽出
    const auto diff = previous_buttons ^ buttons;
    // 変化があったビット(ボタン)ごとにループして、イベントを送る
    for (int i = 0; i < 8; ++i) {
      if ((diff >> i) & 1) {
        Message msg{Message::kMouseButton};
        msg.arg.mouse_button.x = relpos.x;
        msg.arg.mouse_button.y = relpos.y;
        msg.arg.mouse_button.press = (buttons >> i) & 1;
        msg.arg.mouse_button.button = i;
        task_manager->SendMessage(task_id, msg);
      }
    }
  }

}

/**
 * @fn
 * SendCloseMessage関数
 * @brief 
 * 閉じるボタンクリック時のメッセージを通知する
 */
void SendCloseMessage() {
   const auto [ layer, task_id ] = FindActiveLayerTask();
   if (!layer || !task_id) {
     return;
   }

   Message msg{Message::kWindowClose};
   msg.arg.window_close.layer_id = layer->ID();
   task_manager->SendMessage(task_id, msg);
}


/**
 * @fn
 * Mouse::OnInterruptメソッド
 * 
 * @brief
 * マウスイベントが発生したときの割り込み処理
 * @param [in] buttons クリックの有無などのフラグ
 * @param [in] x X座標の移動量
 * @param [in] y Y座標の移動量
 */
void Mouse::OnInterrupt(uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
  const auto oldpos = position_;
  auto newpos = position_ + Vector2D<int>{displacement_x, displacement_y};
  newpos = ElementMin(newpos, ScreenSize() + Vector2D<int>{-1, -1});
  // 画面からはみ出る場合は、はみ出ないギリギリに位置に置き換え
  position_ = ElementMax(newpos, {0, 0});

  // 移動ベクトルを算出
  const auto posdiff = position_ - oldpos;

  // マウスの再描画
  layer_manager->Move(layer_id_, position_);

  //! closeするレイヤーのID。closeしないときは0
  unsigned int close_layer_id = 0;

  // マウスドラッグ処理
  //! 既にマウスの左クリックをしていたかどうか
  const bool previous_left_pressed = (previous_buttons_ & 0x01);
  const bool left_pressed = (buttons & 0x01);
  if (!previous_left_pressed && left_pressed) {
    // 新たに左クリックされたとき、移動対象のレイヤーを取得
    auto layer = layer_manager->FindLayerByPosition(position_, layer_id_);
    if (layer && layer->IsDraggable()) {
      // クリックした領域をチェック
      const auto pos_layer = position_ - layer->GetPosition();
      switch (layer->GetWindow()->GetWindowRegion(pos_layer)) {
        case WindowRegion::kTitleBar:
          // レイヤーが移動可能だったらdrag_layer_idをセット
          drag_layer_id_ = layer->ID();
          break;
        case WindowRegion::kCloseButton:
          close_layer_id = layer->ID();
          break;
        default:
          break;
      }
      // 対象のレイヤーを活性化する
      active_layer->Activate(layer->ID());
    } else {
      // どのレイヤーも選択しないとき、一旦すべてのレイヤーを非活性化
      active_layer->Activate(0);
    }
  } else if (previous_left_pressed && left_pressed) {
    // 既に左クリックしてあって、まだ押し続けている場合は、対象のレイヤーを移動
    if (drag_layer_id_ > 0) {
      layer_manager->MoveRelative(drag_layer_id_, posdiff);
    }
  } else if (previous_left_pressed && !left_pressed) {
    // 既に左クリックしてあって、今回離したとき。移動対象のレイヤーをクリアする
    drag_layer_id_ = 0;
  }
  // マウス移動イベントを送る
  if (drag_layer_id_ == 0) {
    if (close_layer_id == 0) {
      // ドラッグイベントではなくかつクローズイベントでないときだけ。移動イベントを送る。
      SendMouseMessage(newpos, posdiff, buttons, previous_buttons_);
    } else {
      SendCloseMessage();
    }
  }

  // 再入時のために既存のボタン変数をいれておく。
  previous_buttons_ = buttons;
}

/**
 * @fn
 * InitializeMouse関数
 * 
 * @brief
 * マウスインスタンスを生成して、layer_managerに追加する
 */
void InitializeMouse() {
  // マウスウィンドウの生成
  auto mouse_window = std::make_shared<Window>(
      kMouseCursorWidth, kMouseCursorHeight, screen_config.pixel_format);
  mouse_window->SetTransparentColor(kMouseTransparentColor);
  // マウスウィンドウを初期化
  DrawMouseCursor(mouse_window->Writer(), {0, 0});

  auto mouse_layer_id = layer_manager->NewLayer()
    .SetWindow(mouse_window)
    .ID();

  // マウスインスタンスを生成
  auto mouse = std::make_shared<Mouse>(mouse_layer_id);
  // 初期位置のマウスを描画
  mouse->SetPosition({200, 200});

  // layer_managerに最前面に追加
  layer_manager->UpDown(mouse->LayerID(), std::numeric_limits<int>::max());
  // USBマウスからのデータを受信する関数として、Mouse::OnInterruptメソッドをセット
  usb::HIDMouseDriver::default_observer =
    [mouse](uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
      mouse->OnInterrupt(buttons, displacement_x, displacement_y);
    };
  
  // ウィンドウのアクティブ状態を管理するため、マウスレイヤーをactive_layerにいれておく
  active_layer->SetMouseLayer(mouse_layer_id);
}