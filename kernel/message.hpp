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
 * LayerOperation構造体
 * @brief 
 * レイヤー操作の種類をenumで定義
 */
enum class LayerOperation {
  Move, MoveRelative, Draw, DrawArea
};

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
    kLayer,
    kLayerFinish,
    kMouseMove,
  } type;

  //! kLayer, kLayerFinishでつかう送信元タスクのID
  uint64_t src_task;

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

    struct {
      LayerOperation op;
      unsigned int layer_id;
      int x, y;
      int w, h;
    } layer;

    struct {
      //! マウス座標
      int x, y;
      //! 直前の移動量
      int dx, dy;
      //! ボタン押下状況
      uint8_t buttons;
    } mouse_move;
  } arg;
};
