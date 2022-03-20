#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include "../syscall.h"

static const int kCanvasSize = 100, kEyeSize = 10;

/**
 * @fn
 * DrawEye関数
 * @brief 
 * 目を描画する
 * @param layer_id_flags 描画するレイヤーのID
 * @param mouse_x マウスのX座標
 * @param mouse_y マウスのY座標
 * @param color 色
 */
void DrawEye(uint64_t layer_id_flags,
             int mouse_x, int mouse_y, uint32_t color) {
  // 中心座標からマウス座標の差を算出
  const double center_x = mouse_x - kCanvasSize/2 - 4;
  const double center_y = mouse_y - kCanvasSize/2 - 24;
  
  //! 中心からの角度 = (x, y)のアークタンジェント
  const double direction = atan2(center_y, center_x);
  //! 中心からの距離
  double distance = sqrt(pow(center_x, 2) + pow(center_y, 2));
  // 最大距離をウィンドウ幅で制限
  distance = std::min<double>(distance, kCanvasSize/2 - kEyeSize/2);

  //! 目玉の位置を算出
  const double eye_center_x = cos(direction) * distance;
  const double eye_center_y = sin(direction) * distance;
  const int eye_x = static_cast<int>(eye_center_x) + kCanvasSize/2 + 4;
  const int eye_y = static_cast<int>(eye_center_y) + kCanvasSize/2 + 24;
  
  SyscallWinFillRectangle(layer_id_flags, eye_x - kEyeSize/2, eye_y - kEyeSize/2, kEyeSize, kEyeSize, color);
}

/**
 * @fn
 * main関数
 * @brief 
 * 目玉アプリの実行
 */
extern "C" void main(int argc, char** argv) {
  // ウィンドウを描画
  auto [layer_id, err_openwin]
    = SyscallOpenWindow(kCanvasSize + 8, kCanvasSize + 28, 10, 10, "eye");
  if (err_openwin) {
    exit(err_openwin);
  }

  // 背景色を描画
  SyscallWinFillRectangle(layer_id, 4, 24, kCanvasSize, kCanvasSize, 0xffffffff);
  DrawEye(layer_id, 10 + kCanvasSize/2 - 4 , 10 + kCanvasSize/2 + 24 , 0x00000000);

  AppEvent events[1];
  while(true) {
    // マウスイベントを取得
    auto [ n, err ] = SyscallReadEvent(events, 1);
    if (err) {
      printf("Read Event failed: %s\n", strerror(err));
      break;
    }

    // Ctrl + Qで終了
    if (events[0].type == AppEvent::kQuit) {
      break;
    } else if (events[0].type == AppEvent::kMouseMove) {
      // マウスの移動イベントで目玉を動かす
      auto& arg = events[0].arg.mouse_move;
      // 一旦背景色でクリアする
      SyscallWinFillRectangle(layer_id | LAYER_NO_REDRAW,
          4, 24, kCanvasSize, kCanvasSize, 0xffffffff);
      DrawEye(layer_id, arg.x, arg.y, 0x00000000);
    } else {
      printf("Unknown event: type = %d\n", events[0].type);
    }
  }

  SyscallCloseWindow(layer_id);
  exit(0);
}
