#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct AppEvent {
  enum Type {
    kQuit,
    kMouseMove,
    kMouseButton,
  } type;

  /**
   * @brief 
   * 引数の共用体
   */
  union {
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
  } arg;
};

#ifdef __cplusplus
} // extern "C"
#endif
