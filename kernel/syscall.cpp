#include "syscall.hpp"

#include <array>
#include <cstdint>
#include <cerrno>

#include "asmfunc.h"
#include "font.hpp"
#include "logger.hpp"
#include "msr.hpp"
#include "task.hpp"
#include "terminal.hpp"

namespace syscall {
  struct Result {
    uint64_t value;
    int error;
  };
  
#define SYSCALL(name) \
  Result name( \
    uint64_t arg1, uint64_t arg2, uint64_t arg3, \
    uint64_t arg4, uint64_t arg5, uint64_t arg6)

  SYSCALL(LogString) {
    if (arg1 != kError && arg1 != kWarn && arg1 != kInfo && arg1 != kDebug) {
      // 出力レベルフラグ以外が引数に指定された時はエラー
      return { 0, EPERM };
    }
    const char* s = reinterpret_cast<const char*>(arg2);
    const auto len = strlen(s);
    if (len > 1024) {
      // 長さが上限を超える場合はエラー
      return { 0, E2BIG };
    }
    Log(static_cast<LogLevel>(arg1), "%s", s);
    return { len, 0 };
  }

  SYSCALL(PutString) {
    //! 第一引数：現状は、出力先ターミナルの番号
    const auto fd = arg1;
    //! 第二引数：表示したい文字列へのポインタ
    const char* s = reinterpret_cast<const char*>(arg2);
    //! 第三引数：NUL文字をふくまないバイト数
    const auto len = arg3;
    if (len > 1024) {
      return { 0, E2BIG };
    }

    if (fd == 1) {
      // 現在のタスク＝アプリが実行されているタスクから、出力先のターミナルを探して出力
      const auto task_id = task_manager->CurrentTask().ID();
      (*terminals)[task_id]->Print(s, len);
      return { len, 0 };
    }
    return { 0, EBADF };
  }

  SYSCALL(Exit) {
    __asm__("cli"); // 割り込み禁止
    auto& task = task_manager->CurrentTask();
    __asm__("sti"); // 割り込み許可
    return { task.OSStackPointer(), static_cast<int>(arg1) };
  }

  SYSCALL(OpenWindow) {
    const int w = arg1, h = arg2, x = arg3, y = arg4;
    const auto title = reinterpret_cast<const char*>(arg5);
    const auto win = std::make_shared<ToplevelWindow>(
        w, h, screen_config.pixel_format, title);
    
    __asm__("cli"); // 割り込み禁止
    const auto layer_id = layer_manager->NewLayer()
      .SetWindow(win)
      .SetDraggable(true)
      .Move({x, y})
      .ID();
    active_layer->Activate(layer_id);
    __asm__("sti"); // 割り込み許可

    return { layer_id, 0 };
  }

  SYSCALL(WinWriteString) {
    const unsigned int layer_id = arg1;
    const int x = arg2, y = arg3;
    const uint32_t color = arg4;
    const auto s = reinterpret_cast<const char*>(arg5);

    __asm__("cli"); // 割り込み禁止
    auto layer = layer_manager->FindLayer(layer_id);
    __asm__("sti"); // 割り込み許可
    if (layer == nullptr) {
      return { 0, EBADF};
    }

    WriteString(*layer->GetWindow()->Writer(), {x, y}, s, ToColor(color));
    __asm__("cli");  // 割り込み禁止
    layer_manager->Draw(layer_id);
    __asm__("sti");  // 割り込み許可

    return { 0, 0 };
  }

#undef SYSCALL

}  // namespace syscall

using SyscallFuncType = syscall::Result (uint64_t, uint64_t, uint64_t,
                                 uint64_t, uint64_t, uint64_t);
extern "C" std::array<SyscallFuncType*, 5> syscall_table{
  /* 0x00 */ syscall::LogString,
  /* 0x01 */ syscall::PutString,
  /* 0x02 */ syscall::Exit,
  /* 0x03 */ syscall::OpenWindow,
  /* 0x04 */ syscall::WinWriteString,
};

void InitializeSyscall() {
  // kIA32_EFERのビット0を1にして、syscallを使えるようにする。
  // ビット8(LME)とビット10(LMA)も64ビットモードのとき1
  WriteMSR(kIA32_EFER, 0x0501u);
  // SyscallEntry関数の先頭アドレスをkIA32_LSTARレジスタに書き込み
  WriteMSR(kIA32_LSTAR, reinterpret_cast<uint64_t>(SyscallEntry));
  // syscall/sysretオペランドが動作する時のCS、SSセグメントレジスタにいれる値を設定
  // syscall側のRPLは0、sysret側のRPLが3になるようにしておく。
  // MikanOSでは、コードセグメントがgdt[1](なので1<<3=8)、スタックセグメントがgdt[2](2<<3=16)
  // IA32_STARTに設定するオフセットに合わせてそれぞれ32ビット、48ビット左にシフトする
  WriteMSR(kIA32_STAR, static_cast<uint64_t>(8) << 32 |
                       static_cast<uint64_t>(16 | 3) << 48);
  // R/Wのフラグマスクらしい。今回は0にしておく。 
  WriteMSR(kIA32_FMASK, 0);
}
