#include "frame_buffer.hpp"

/**
 * @fn
 * FrameBuffer::Initializeメソッド
 * 
 * @brief
 * FrameBufferを初期化する
 * @param [in] config FrameBufferConfigインスタンス
 * @return Errorコード。成功時はkkSuccessを返す。
 */
Error FrameBuffer::Initialize(const FrameBufferConfig& config) {
  config_ = config;

  const auto bits_per_pixel = BitsPerPixel(config_.pixel_format);
  if (bits_per_pixel <= 0) {
    // 1ピクセルあたりのビット数が定義されていないPixelFormatは非サポート
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  if (config_.frame_buffer) {
    // すでにフレームバッファがある場合は、メモリ確保が不要なため、サイズを0似する
    // UEFIが確保した本来のFrameBufferを扱うことを想定
    buffer_.resize(0);
  } else {
    buffer_.resize(
      ((bits_per_pixel + 7) / 8)
      * config_.horizontal_resolution * config_.vertical_resolution);
    config_.frame_buffer = buffer_.data();
    config_.pixels_per_scan_line = config_.horizontal_resolution;
  }

  switch (config_.pixel_format) {
    case kPixelRGBResv8BitPerColor:
      writer_ = std::make_unique<RGBResv8BitPerColorPixelWriter>(config_);
      break;
    case kPixelBGRResv8BitPerColor:
      writer_ = std::make_unique<BGRResv8BitPerColorPixelWriter>(config_);
      break;
    default:
      return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  return MAKE_ERROR(Error::kSuccess);
}

/**
 * @fn
 * FrameBuffer::Copyメソッド
 * 
 * @brief
 * 指定されたバッファを自身のバッファにコピーする
 * @param [in] pos Vector2D<int>コピー先に描画するフレームバッファの左上の座標
 * @param [in] src コピー元のFrameBuffer
 * @return Errorコード。成功時はkkSuccessを返す。
 */
Error FrameBuffer::Copy(Vector2D<int> pos, const FrameBuffer& src) {
  if (config_.pixel_format != src.config_.pixel_format) {
    // コピー元のPixelFormatがコピー先と一致しない場合は、エラー
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  const auto bits_per_pixel = BitsPerPixel(config_.pixel_format);
  if (bits_per_pixel <= 0) {
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }
  const auto dst_width = config_.horizontal_resolution;
  const auto dst_height = config_.vertical_resolution;
  const auto src_width = src.config_.horizontal_resolution;
  const auto src_height = src.config_.vertical_resolution;

  // 位置がはみ出たら、描画できるところまでをコピーする
  const auto copy_start_dst_x = std::max(pos.x, 0);
  const auto copy_start_dst_y = std::max(pos.y, 0);
  const int copy_end_dst_x = std::min(pos.x + src_width, dst_width);
  const int copy_end_dst_y = std::min(pos.y + src_height, dst_height);

  //! 1ピクセルあたりのバイト数。memcpyではバイト数単位でコピー
  const auto bytes_per_pixel = (bits_per_pixel + 7) / 8;
  //! 1行あたりのバイト数
  const auto bytes_per_copy_line = 
    bytes_per_pixel * (copy_end_dst_x - copy_start_dst_x);
  
  //! コピー先のポインタ
  // フレームバッファの初期アドレス + (開始y座標 * 1行あたりのピクセル数 + 開始x座標) * 1ピクセルあたりのバイト数
  uint8_t* dst_buf = config_.frame_buffer + bytes_per_pixel *
    (config_.pixels_per_scan_line * copy_start_dst_y + copy_start_dst_x);
  //! コピー元のポインタ
  const uint8_t* src_buf = src.config_.frame_buffer;

  for (int dy = 0; dy < copy_end_dst_y - copy_start_dst_y; ++dy) {
    // 1行ずつmemcpyでフレームバッファの中身をコピー
    memcpy(dst_buf, src_buf, bytes_per_copy_line);
    dst_buf += bytes_per_pixel * config_.pixels_per_scan_line;
    src_buf += bytes_per_pixel * src.config_.pixels_per_scan_line;
  }

  return MAKE_ERROR(Error::kSuccess);
}

/**
 * @fn
 * BitsPerPixel関数
 * 
 * @brief
 * 指定したPixelFormatの1ピクセルあたりのビット数を返す。
 * @param [in] format PixelFormatインスタンス
 * @return int ピクセル数。不明なPixelFormatの場合は、-1を返す
 */
int BitsPerPixel(PixelFormat format) {
  switch (format) {
    case kPixelRGBResv8BitPerColor: return 32;
    case kPixelBGRResv8BitPerColor: return 32;
  }
  return -1;
}

