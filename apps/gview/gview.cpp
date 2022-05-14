#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <tuple>
#include "../syscall.h"

#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"

/**
 * @fn
 * MapFile関数
 * @brief 
 * 引数に指定されたファイルを開き、ファイルディスクリプタとファイルサイズを取得する
 * @param filepath 対象のファイル
 * @return std::tuple<int, uint8_t*, size_t> 
 */
std::tuple<int, uint8_t*, size_t> MapFile(const char* filepath) {
  SyscallResult res = SyscallOpenFile(filepath, O_RDONLY);
  if (res.error) {
    fprintf(stderr, "%s: %s\n", strerror(res.error), filepath);
    exit(1);
  }

  const int fd = res.value;
  size_t filesize;
  res = SyscallMapFile(fd, &filesize, 0);
  if (res.error) {
    fprintf(stderr, "%s\n", strerror(res.error));
    exit(1);
  }

  return {fd, reinterpret_cast<uint8_t*>(res.value), filesize};
}

/**
 * @fn
 * WaitEvent関数
 * @brief 
 * kQuitイベントがくるまで待つ
 */
void WaitEvent() {
  AppEvent events[1];
  while (true) {
    auto [ n, err ] = SyscallReadEvent(events, 1);
    if (err) {
      fprintf(stderr, "ReadEvent failed: %s\n", strerror(err));
      return;
    }
    if (events[0].type == AppEvent::kQuit) {
      return;
    }
  }
}

/**
 * @fn
 * GetColorRGB関数
 * @brief Get the Color R G B object
 * 引数に指定されたimage_dataを配色データに変換する
 * @param image_data 
 * @return uint32_t 
 */
uint32_t GetColorRGB(unsigned char* image_data) {
  return static_cast<uint32_t>(image_data[0]) << 16 |
         static_cast<uint32_t>(image_data[1]) << 8 |
         static_cast<uint32_t>(image_data[2]);
}

/**
 * @fn
 * GetColorGray
 * @brief Get the Color Gray object
 * 灰色に相当する色を取得する
 * @param image_data 
 * @return uint32_t 
 */
uint32_t GetColorGray(unsigned char* image_data) {
  const uint32_t gray = image_data[0];
  return gray << 16 | gray << 8 | gray;
}

/**
 * @fn
 * main関数
 * @brief 
 * 画像ビューアを実行する
 */
extern "C" void main(int argc, char** argv) {
  // 引数解析
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <file>\n", argv[0]);
    exit(1);
  }

  int width, height, bytes_per_pixel;
  // 画像ファイルを開く
  const char* filepath = argv[1];
  const auto [ fd, content, filesize ] = MapFile(filepath);
  
  unsigned char* image_data = stbi_load_from_memory(
      content, filesize, &width, &height, &bytes_per_pixel, 0);
  if (image_data == nullptr)  {
    fprintf(stderr, "failed to load image: %s\n", stbi_failure_reason());
    exit(1);
  }

  fprintf(stderr, "%dx%d, %d bytes/pixel\n", width ,height, bytes_per_pixel);
  auto get_color = GetColorRGB;
  if (bytes_per_pixel <= 2) {
    get_color = GetColorGray;
  }

  // Windowの描画
  const char* last_slash = strrchr(filepath, '/');
  const char* filename = last_slash ? &last_slash[1] : filepath;
  SyscallResult window = 
    SyscallOpenWindow(8 + width, 28 + height, 10, 10, filename);
  if (window.error) {
    fprintf(stderr, "%s\n", strerror(window.error));
    exit(1);
  }
  const uint64_t layer_id = window.value;

  // 画像の描画(バッファに書き込むだけ)
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      uint32_t c = get_color(&image_data[bytes_per_pixel * (y * width + x)]);
      SyscallWinFillRectangle(layer_id | LAYER_NO_REDRAW,
                              4 + x, 24 + y, 1, 1, c);
    }
  }

  // バッファにいれた画像を描画
  SyscallWinRedraw(layer_id);
  WaitEvent();

  SyscallCloseWindow(layer_id);
  exit(0);
}
