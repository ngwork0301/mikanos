#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct AppEvent {
  enum Type {
    kQuit,
    kMouseMove,
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
  } arg;
};

#ifdef __cplusplus
} // extern "C"
#endif
