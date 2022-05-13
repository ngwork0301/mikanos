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
    kMouseButton,
    kWindowActive,
    kPipe,
    kWindowClose,
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
      bool press;
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

    struct {
      //! マウス座標
      int x, y;
      int press;  // 1: press, 0: release
      //! 押したボタン 右クリック or 左クリック
      int button;
    } mouse_button;
    struct {
      int activate; // 1: activate, 0: deactivate
    } window_active;

    struct {
      // 16バイトより大きくしすぎると、メッセージ構造体が大きくなり、メモリが無駄になる
      // それより小さいと何個もメッセージを送る必要があり、オーバーヘッドが大きくなる
      char data[16];
      uint8_t len;
    } pipe;

    struct {
      unsigned int layer_id;
    } window_close;
  } arg;

};
