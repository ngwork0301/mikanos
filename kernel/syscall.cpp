#include "syscall.hpp"

#include <array>
#include <cstdint>

#include "asmfunc.h"
#include "logger.hpp"
#include "msr.hpp"

namespace syscall {
  
#define SYSCALL(name) \
  int64_t name( \
    uint64_t arg1, uint64_t arg2, uint64_t arg3, \
    uint64_t arg4, uint64_t arg5, uint64_t arg6)

  SYSCALL(LogString) {
    if (arg1 != kError && arg1 != kWarn && arg1 != kInfo && arg1 != kDebug) {
      // 出力レベルフラグ以外が引数に指定された時はエラー
      return -1;
    }
    const char* s = reinterpret_cast<const char*>(arg2);
    if (strlen(s) > 1024) {
      // 長さが上限を超える場合はエラー
      return -1;
    }
    Log(static_cast<LogLevel>(arg1), "%s", s);
    return 0;
  }

#undef SYSCALL

}  // namespace syscall

using SyscallFuncType = int64_t (uint64_t, uint64_t, uint64_t,
                                 uint64_t, uint64_t, uint64_t);
extern "C" std::array<SyscallFuncType*, 1> syscall_table{
  /* 0x00 */ syscall::LogString,
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
