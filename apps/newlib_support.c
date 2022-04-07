#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

#include "syscall.h"

/**
 * @fn
 * _exit関数
 * 
 * @brief 
 * プログラムを終了する
 * @param [in] status 終了コード
 */
void _exit(int status) {
  SyscallExit(status);
}

/**
 * @fn
 * sbrk関数
 * @brief 
 * メモリを確保する
 * @param incr 
 * @return caddr_t 
 */
caddr_t sbrk(int incr) {
  static uint8_t heap[4096];
  static int i = 0;
  int prev = i;
  i += incr;
  return (caddr_t)&heap[prev];
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

/**
 * @fn
 * read関数
 * @brief 
 * ラップしてReadFileシステムコールを呼び出す
 * @param fd 読み込むファイルのファイルディスクリプタ
 * @param buf 読み込み先バッファ
 * @param count 読み込むバイト数
 * @return ssize_t 読み込んだバイト数。エラーのときは-1
 */
ssize_t read(int fd, void* buf, size_t count) {
  struct SyscallResult res = SyscallReadFile(fd, buf, count);
  if (res.error == 0) {
    return res.value;
  }
  errno = res.error;
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

/**
 * @fn
 * open関数
 * @brief 
 * ラップして、OpenFileシステムコールを呼び出す
 * @param path 読み込むファイルのパス文字列
 * @param flags 読み込みモード r/w/a
 * @return int 正常終了なら0。異常終了なら-1
 */
int open(const char* path, int flags) {
  struct SyscallResult res = SyscallOpenFile(path, flags);
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

/**
 * @fn
 * posix_memalign関数
 * @brief 
 * アライメントされたメモリ領域を確保する
 * @param memptr [out] 確保したメモリのアドレス
 * @param alignment アライメント( 2 のべき乗)
 * @param size 確保するサイズ
 * @return int 
 */
int posix_memalign(void** memptr, size_t alignment, size_t size) {
  void* p = malloc(size + alignment - 1);
  if(!p) {
    return ENOMEM;
  }
  uintptr_t addr = (uintptr_t)p;
  *memptr = (void*)((addr + alignment - 1) & ~(uintptr_t)(alignment - 1));
  return 0;
}
