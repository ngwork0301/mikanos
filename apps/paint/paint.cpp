#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../syscall.h"

static const int kWidth = 200, kHeight = 130;

/**
 * @fn
 * IsInside関数
 * @brief 
 * 引数で指定された座標がウィンドウの中にあるかを判定する
 * @param x 
 * @param y 
 * @return true 
 * @return false 
 */
bool IsInside(int x, int y) {
  return 4 <= x && x < 4 + kWidth && 24 <= y && y < 24 + kHeight; 
}

/**
 * @fn
 * main関数
 * @brief 
 * ペイントアプリを起動する
 */
extern "C" void main(int argc, char** argv) {
  // ウィンドウを描画
  auto [layer_id, err_openwin]
    = SyscallOpenWindow(kWidth + 8, kHeight + 28, 10, 10, "paint");
  if (err_openwin) {
    exit(err_openwin);
  }

  AppEvent events[1];
  bool press = false;
  // イベントループ
  while (true) {
    auto [ n, err ] = SyscallReadEvent(events, 1);
    if (err) {
      printf("ReadEvent faild: %s\n", strerror(err));
      break;
    }
    // マウス移動イベントで線を描画
    if (events[0].type == AppEvent::kMouseMove) {
      auto& arg = events[0].arg.mouse_move;
      const auto prev_x = arg.x - arg.dx, prev_y = arg.y - arg.dy;
      if (press && IsInside(prev_x, prev_y)) {
        SyscallWinDrawLine(layer_id, prev_x, prev_y, arg.x, arg.y, 0x000000);
      }
    } else if(events[0].type == AppEvent::kMouseButton) {
      auto& arg = events[0].arg.mouse_button;
      // マウスクリック or リリースイベントでpressフラグをセット
      if (arg.button == 0) {
        press = arg.press;
        SyscallWinFillRectangle(layer_id, arg.x, arg.y, 1, 1, 0x000000);
      }
    } else {
      printf("Unknown event: type = %d\n", events[0].type);
    }
  }
  
  SyscallCloseWindow(layer_id);
  exit(0);
}