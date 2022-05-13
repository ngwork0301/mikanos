#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../syscall.h"

extern "C" void main(int argc, char** argv) {
    auto [layer_id, err_openwin]
      = SyscallOpenWindow(200, 100, 10, 10, "winhello");
    if (err_openwin) {
      exit(err_openwin);
    }

    SyscallWinWriteString(layer_id, 7, 24, 0xc00000, "hello world!");
    SyscallWinWriteString(layer_id, 24, 40, 0x00c000, "hello world!");
    SyscallWinWriteString(layer_id, 40, 56, 0x0000c0, "hello world!");

    // 終了イベントが来たらアプリを終了させる
    AppEvent events[1];
    while(true) {
      // 同期：イベントがくるまで待つ
      auto [ n, err ] = SyscallReadEvent(events, 1);
      if (err) {
        printf("ReadEvent failed: %s\n", strerror(err));
        break;
      }
      if (events[0].type == AppEvent::kQuit) {
        break;
      } else if (events[0].type == AppEvent::kMouseMove ||
        events[0].type == AppEvent::kMouseButton ||
        events[0].type == AppEvent::kKeyPush) {
        // 何もしない
      } else {
        // Ctrl-Q以外のイベントが来たら、そのイベントを表示
        printf("Unknown event: type = %d\n", events[0].type);
      }
    }
    SyscallCloseWindow(layer_id);
    exit(0);
}
