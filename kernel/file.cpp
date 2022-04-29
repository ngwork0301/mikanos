#include "file.hpp"

#include <cstdio>

/**
 * @fn
 * PrintToFD関数
 * @brief 
 * 指定されたファイルディスクリプタに文字列を出力する
 * @param fd 出力先ファイルディスクリプタ
 * @param format 出力するフォーマット指定子付き文字列
 * @param ... フォーマット指定子に入れる変数
 * @return size_t 出力したバイト数
 */
size_t PrintToFD(FileDescriptor& fd, const char* format, ...) {
  va_list ap;
  int result;
  char s[128];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  fd.Write(s, result);
  return result;
}
