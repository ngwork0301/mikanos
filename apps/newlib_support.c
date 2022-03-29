#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

#include "syscall.h"

/**
 * @fn
 * _exit関数
 * 
 * @brief 
 * プログラムを就床する
 * @param [in] status 終了コード
 */
void _exit(int status) {
  SyscallExit(status);
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

int close(int fd) {
  errno = EBADF;
  return -1;
}

off_t lseek(int fd, off_t offset, int whence) {
  errno = EBADF;
  return -1;
}

ssize_t read(int fd, void* buf, size_t count) {
  errno = EBADF;
  return -1;
}

/**
 * @fn
 * write関数
 * @brief 
 * 仕様はman writeを参照
 * @param fd 
 * @param buf 
 * @param count 
 * @return ssize_t 
 */
ssize_t write(int fd, const void* buf, size_t count) {
  struct SyscallResult res = SyscallPutString(fd, buf, count);
  if (res.error == 0) {
    return res.value;
  }
  errno = res.error;
  return -1;
}

int fstat(int fd, struct stat* buf) {
  errno = EBADF;
  return -1;
}

int isatty(int fd) {
  errno = EBADF;
  return -1;
}
