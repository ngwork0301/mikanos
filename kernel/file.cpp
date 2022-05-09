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

/**
 * @fn
 * ReadDelim関数
 * @brief 
 * 指定された区切り文字まで読み取る
 * @param [in] fd 読み取り対象のファイルディスクリプタ
 * @param [in] delim 区切り文字
 * @param [out] buf 読み取った文字列のバッファ
 * @param [in] len 読み取る最大バイト数
 * @return size_t 読みとったバイト数
 */
size_t ReadDelim(FileDescriptor& fd, char delim, char* buf, size_t len){
  size_t i = 0;
  for (; i < len - 1; ++i) {
    if (fd.Read(&buf[i], 1) == 0) {
      break;
    }
    if (buf[i] == delim) {
      ++i;
      break;
    }
  }
  // 最後末尾にNULL文字を追加
  buf[i] = '\0';
  return i;
}
