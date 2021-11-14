#pragma once

#include "frame_buffer_config.hpp"

/**
 * @struct
 * PixelColor構造体
 */
struct PixelColor {
  uint8_t r, g, b;
};

inline bool operator==(const PixelColor& lhs, const PixelColor& rhs) {
  return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
}

inline bool operator!=(const PixelColor& lhs, const PixelColor& rhs) {
  return !(lhs == rhs);
}

/**
 * @struct
 * Vector2D
 * 
 * @brief
 * X, Y座標を示す2次元ベクトル
 */
template <typename T>
struct Vector2D {
  T x, y;

  template <typename U>
  Vector2D<T>& operator +=(const Vector2D<U>& rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }
};

template <typename T, typename U>
auto operator +(const Vector2D<T>& lhs, const Vector2D<U>& rhs)
    -> Vector2D<decltype(lhs.x + rhs.x)> {
  return {lhs.x + rhs.x, lhs.y + rhs.y};
  /*
  auto tmp{lhs};
  tmp.x += rhs.x;
  tmp.y += rhs.y;
  return tmp;
  */
}

/**
 * @class
 * PixelWriterクラス(抽象クラス)
 * 
 * @brief
 * 1ピクセルごとの描画をおこなうクラス
 */
class PixelWriter {
  public:
    virtual ~PixelWriter() = default;
    virtual void Write(Vector2D<int> pos, const PixelColor& c) = 0;
    virtual int Width() const = 0;
    virtual int Height() const = 0;
};

class FrameBufferWriter : public PixelWriter {
  public:
    FrameBufferWriter(const FrameBufferConfig& config) : config_{config} {
    }
    virtual ~FrameBufferWriter() = default;
    virtual int Width() const override { return config_.horizontal_resolution; }
    virtual int Height() const override { return config_.vertical_resolution; }
  protected:
    uint8_t* PixelAt(Vector2D<int> pos) {
      return config_.frame_buffer + 4 * (config_.pixels_per_scan_line * pos.y + pos.x);
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
class RGBResv8BitPerColorPixelWriter : public FrameBufferWriter {
  public:
    using FrameBufferWriter::FrameBufferWriter;

  virtual void Write(Vector2D<int> pos, const PixelColor& c) override;
};

/**
 * @class
 * BGRResv8BitPerColorPixelWriter
 * 
 * @brief
 * ピクセルフォーマットがRGBResv8BitPerColorの場合のピクセル描画をおこなうクラス
 */
class BGRResv8BitPerColorPixelWriter : public FrameBufferWriter {
  public:
    using FrameBufferWriter::FrameBufferWriter;

    virtual void Write(Vector2D<int> pos, const PixelColor& c) override;
};

void FillRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c);
void DrawRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c);

//! デスクトップの背景色とコンソール文字色
const PixelColor kDesktopBGColor{45, 118, 237};
const PixelColor kDesktopFGColor{255, 255, 255};

void DrawDesktop(PixelWriter& writer);
