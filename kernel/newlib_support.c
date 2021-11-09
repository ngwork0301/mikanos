#include <errno.h>
#include <sys/types.h>

void _exit(void) {
  while (1) __asm__("hlt");
}

//! プログラムブレークと、その上限値を示すグローバル変数
caddr_t program_break, program_break_end;

caddr_t sbrk(int incr) {
  // プログラムブレークが初期化前または、上限値を超える場合は、ENUMEMでエラー
  if (program_break == 0 || program_break + incr >= program_break_end) {
    errno = ENOMEM;
    return (caddr_t)-1;
  }

  // 現在のプログラムブレークをprev_breakに退避して必要な分増減したプログラムブレークを新たに設定
  caddr_t prev_break = program_break;
  program_break += incr;
  return prev_break;
}

int getpid(void) {
  return 1;
}

int kill(int pid, int sig) {
  errno = EINVAL;
  return -1;
}
