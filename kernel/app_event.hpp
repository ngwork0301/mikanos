#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct AppEvent {
  enum Type {
    kQuit,
    kMouseMove,
    kMouseButton,
    kTimerTimeout,
  } type;

  /**
   * @brief 
   * 引数の共用体
   */
  union {

    /**
     * @struct
     * mouse_move構造体
     * @brief 
     * マウス移動イベント
     */
    struct {
      //! マウス座標
      int x, y;
      //! 直前の移動量
      int dx, dy;
      //! ボタン押下状況
      uint8_t buttons;
    } mouse_move;

    /**
     * @struct
     * mouse_button構造体
     * @brief 
     * マウスボタンイベント
     */
    struct {
      //! マウス座標
      int x, y;
      int press;  // 1: press, 0: release
      //! 押したボタン 右クリック or 左クリック
      int button;
    } mouse_button;

    /**
     * @struct
     * timer構造体
     * @brief 
     * タイムアウトイベント
     */
    struct {
      //! タイムアウト値
      unsigned long timeout;
      //! タイムアウトしたときに通知する値
      int value;
    } timer;
  } arg;
};

#ifdef __cplusplus
} // extern "C"
#endif
