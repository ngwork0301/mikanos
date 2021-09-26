#include <cstdint>
#include <cstddef>

#include "frame_buffer_config.hpp"

/**
 * @struct
 * PixelColor構造体
 */
struct PixelColor {
  uint8_t r, g, b;
};

/**
 * @class
 * PixelWriterクラス(抽象クラス)
 * 
 * @brief
 * 1ピクセルごとの描画をおこなうクラス
 */
class PixelWriter {
  public:
    PixelWriter(const FrameBufferConfig& config) : config_{config} {
    }
    virtual ~PixelWriter() = default;
    virtual void Write(int x, int y, const PixelColor& c) = 0;

    protected:
      uint8_t* PixelAt(int x, int y) {
        return config_.frame_buffer + 4 * (config_.pixels_per_scan_line * y + x);
      }

    private:
      const FrameBufferConfig config_;
};

/**
 * @class
 * RGBResv8BitPerColorPixelWriter
 * 
 * @brief
 * ピクセルフォーマットがRGBResv8BitPerColorの場合のピクセル描画をおこなうクラス
 */
class RGBResv8ButPerColorPixelWriter : public PixelWriter {
  public:
    using PixelWriter::PixelWriter;

  virtual void Write(int x, int y, const PixelColor& c) override {
    auto p = PixelAt(x, y);
    p[0] = c.r;
    p[1] = c.g;
    p[2] = c.b;
  }
};

/**
 * @class
 * BGRResv8BitPerColorPixelWriter
 * 
 * @brief
 * ピクセルフォーマットがRGBResv8BitPerColorの場合のピクセル描画をおこなうクラス
 */
class BGRResv8BitPerColorPixelWriter : public PixelWriter {
  public:
    using PixelWriter::PixelWriter;

    virtual void Write(int x, int y, const PixelColor& c) override {
      auto p = PixelAt(x, y);
      p[0] = c.b;
      p[1] = c.g;
      p[2] = c.r;
    }
};

/**
 * 配置new演算子の定義
 */
void* operator new(size_t size, void* buf) {
  return buf;
}

void operator delete(void* obj) noexcept {
}

/**
 * issue #1 純粋仮想関数の呼び出しの可能性があるとリンクエラーになるため定義をいれる
 */
extern "C" void __cxa_pure_virtual() { while (1); }

/**
 * グローバル変数
 */
char pixel_writer_buf[sizeof(RGBResv8ButPerColorPixelWriter)];
PixelWriter* pixel_writer;
const uint8_t kFontA[16] = {
  0b00000000, //
  0b00011000, //    **
  0b00011000, //    **
  0b00011000, //    **
  0b00011000, //    **
  0b00100100, //   *  *
  0b00100100, //   *  *
  0b00100100, //   *  *
  0b00100100, //   *  *
  0b01111110, //  ******
  0b01000010, //  *    *
  0b01000010, //  *    *
  0b01000010, //  *    *
  0b11100111, // ***  ***
  0b00000000, //
  0b00000000, //
};
/**
 * @fn
 * WriteAscii関数
 * 
 * @brief
 * フォントを描画する
 * 
 * @param [in] writer
 * @param [in, out] x
 * @param [in, out] y
 * @param [in] char
 * @param [in] color
 */
void WriteAscii(PixelWriter& writer, int x, int y, char c, const PixelColor& color) {
  // 現状Aしかないので、それ以外は終了させる
  if (c != 'A') {
    return;
  }
  // タテ 16bit 分ループ
  for (int dy = 0; dy < 16; ++dy) {
    // ヨコ 8bit 分ループ
    for (int dx = 0; dx < 8; ++dx) {
      // dx で左シフトしたあと、0x80(=0b1000000)との論理和をとって描画するか判定
      if ((kFontA[dy] << dx) & 0x80u) {
        writer.Write(x + dx, y + dy, color);
      }
    }
  }
}

/**
 * @fn
 * KernelMain関数
 * 
 * @brief
 * カーネルのエントリポイント
 * 
 * @param [in] frame_buffer_config FrameBufferConfig構造体
 */
extern "C" void KernelMain(const struct FrameBufferConfig& frame_buffer_config) {
  // ピクセルフォーマットを判定して、対応するPixelWriterインスタンスを生成
  switch (frame_buffer_config.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf)
        RGBResv8ButPerColorPixelWriter{frame_buffer_config};
      break;
    case kPixelBGRResv8BitPerColor:
      pixel_writer = new(pixel_writer_buf)
        BGRResv8BitPerColorPixelWriter{frame_buffer_config};
      break;
  }
  // 背景ピクセルを描画
  for (int x = 0; x < frame_buffer_config.horizontal_resolution; ++x) {
    for (int y = 0; y < frame_buffer_config.vertical_resolution; ++y) {
      pixel_writer->Write(x, y, {255, 255, 255});
    }
  }
  // 仮想ウィンドウのピクセル描画
  // 位置は、左上から0,0。大きさは、200 x 100。色は(0, 255, 0)=緑
  for (int x = 0; x < 200; ++x) {
    for(int y = 0; y < 100; ++y) {
      int xoffset = 0;
      int yoffset = 0;
      pixel_writer->Write(xoffset + x, yoffset + y, {0, 255, 0});
    }
  }
  // 文字列Aを描画
  WriteAscii(*pixel_writer, 50, 50, 'A', {0,0,0});
  WriteAscii(*pixel_writer, 58, 50, 'A', {0,0,0});

  // 無限ループ
  while(1) __asm__("hlt");
}
