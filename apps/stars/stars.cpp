#include <cstdlib>
#include <random>
#include "../syscall.h"

//! ウィンドウの幅、高さ
static constexpr int kWidth = 100, kHeight = 100;

extern "C" void main(int argc, char** argv) {
  // ウィンドウを生成
  auto [layer_id, err_openwin]
    = SyscallOpenWindow(kWidth + 8, kHeight + 28, 10, 10, "stars");
  if (err_openwin) {
      exit(err_openwin);
  }

  // 全体を黒く染める
  SyscallWinFillRectangle(layer_id, 4, 24, kWidth, kHeight, 0x000000);

  //! 星の数
  int num_stars = 100;
  if (argc >= 2) {
    num_stars = atoi(argv[1]);
  }

  // 描画前の時刻を取得
  auto [tick_start, timer_freq] = SyscallGetCurrentTick();
  // 乱数を生成
  std::default_random_engine rand_engine;
  // 乱数の生成範囲を設定
  std::uniform_int_distribution x_dist(0, kWidth - 2), y_dist(0, kHeight - 2);
  for (int i = 0; i < num_stars; ++i) {
    int x = x_dist(rand_engine);
    int y = x_dist(rand_engine);
    // 2 x 2 の点をランダムに生成(速度改善のため、再描画しない)
    SyscallWinFillRectangle(layer_id | LAYER_NO_REDRAW,
                             4 + x, 24 + y, 2, 2, 0xfff100);
  }
  // 点を描画しおわったら再描画する。
  SyscallWinRedraw(layer_id);

  // 描画後の時刻との差から描画にかかった時間を算出して表示
  auto tick_end = SyscallGetCurrentTick();
  printf("%d stars in %lu ms \n",
        num_stars,
        (tick_end.value - tick_start) * 10000 / timer_freq);
  exit(0);
}
