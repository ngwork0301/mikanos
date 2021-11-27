#include "frame_buffer.hpp"

// ユーティリティ関数群
namespace {
  /**
   * @fn
   * BytesPerPixel関数
   * 
   * @brief
   * 指定したフォーマットでの1ピクセルあたりのバイト数を取得する
   * 
   * @param [in] format PixelFormat
   * @return 1ピクセルあたりのバイト数
   */
  int BytesPerPixel(PixelFormat format) {
    switch (format) {
      case kPixelRGBResv8BitPerColor: return 4;
      case kPixelBGRResv8BitPerColor: return 4;
    }
    return -1;
  }
  
  /**
   * @fn
   * FrameAddrAt関数
   * 
   * @brief
   * 指定された座標に対応するメモリアドレスを取得する。
   * @param [in] pos 取得したい位置の座標(Vector2D)
   * @param [in] config FrameBufferConfig
   * @return メモリアドレス
   */
  uint8_t* FrameAddrAt(Vector2D<int> pos, const FrameBufferConfig& config) {
    // フレームバッファの先頭アドレス + (1行あたりのピクセル数 * y座標 + x座標)をバイト数換算
    return config.frame_buffer + BytesPerPixel(config.pixel_format) *
      (config.pixels_per_scan_line * pos.y + pos.x);
  }

  /**
   * @fn
   * BytesPerScanLine関数
   * 
   * @brief
   * 指定した構成での1行あたりのバイト数を取得する
   * @param [in] config FrameBufferConfig
   * @return 1行あたりのバイト数
   */
  int BytesPerScanLine(const FrameBufferConfig& config) {
    return BytesPerPixel(config.pixel_format) * config.pixels_per_scan_line;
  }

  /**
   * @fn
   * FrameBufferSize関数
   * 
   * @brief
   * 指定した構成でのフレームバッファのサイズをVector2Dで取得する
   * @param [in] config FrameBufferConfig
   * @return フレームバッファの{幅, 高さ}のVector2D
   */
  Vector2D<int> FrameBufferSize(const FrameBufferConfig& config) {
    return {static_cast<int>(config.horizontal_resolution),
            static_cast<int>(config.vertical_resolution)};
  }
}

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

  const auto bytes_per_pixel = BytesPerPixel(config_.pixel_format);
  if (bytes_per_pixel <= 0) {
    // 1ピクセルあたりのビット数が定義されていないPixelFormatは非サポート
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  if (config_.frame_buffer) {
    // すでにフレームバッファがある場合は、メモリ確保が不要なため、サイズを0似する
    // UEFIが確保した本来のFrameBufferを扱うことを想定
    buffer_.resize(0);
  } else {
    buffer_.resize(
      bytes_per_pixel
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
 * 指定されたバッファの指定された範囲を自身のバッファにコピーする
 * @param [in] dst_pos Vector2D<int>コピー先に描画するフレームバッファの左上の座標
 * @param [in] src コピー元のFrameBuffer
 * @param [in, out] src_area コピー対象の範囲。この範囲部分のみをコピー元からコピー先へ更新する。
 * @return Errorコード。成功時はkkSuccessを返す。
 */
Error FrameBuffer::Copy(Vector2D<int> dst_pos, const FrameBuffer& src,
                        const Rectangle<int>& src_area) {
  if (config_.pixel_format != src.config_.pixel_format) {
    // コピー元のPixelFormatがコピー先と一致しない場合は、エラー
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }

  const auto bytes_per_pixel = BytesPerPixel(config_.pixel_format);
  if (bytes_per_pixel <= 0) {
    return MAKE_ERROR(Error::kUnknownPixelFormat);
  }
  // src_areaの範囲のみをコピーするため、各範囲の重なった場所を探す
  // コピー先の描画範囲 ここからコピー先／元のフレームバッファからはみ出た部分は除外する。
  const Rectangle<int> src_area_shifted{dst_pos, src_area.size};
  // コピー元のフレームバッファ内の全範囲
  const Rectangle<int> src_outline{dst_pos - src_area.pos, FrameBufferSize(src.config_)};
  // コピー先のフレームバッファ内の全範囲
  const Rectangle<int> dst_outline{{0, 0}, FrameBufferSize(config_)};
  // 上記3つの重なった部分がコピー先
  const auto copy_area = dst_outline & src_outline & src_area_shifted;
  const auto src_start_pos = copy_area.pos - (dst_pos - src_area.pos);
  
  //! コピー先のポインタ
  uint8_t* dst_buf = FrameAddrAt(copy_area.pos, config_);
  //! コピー元のポインタ
  const uint8_t* src_buf = FrameAddrAt(src_start_pos, src.config_);

  for (int y = 0; y < copy_area.size.y; ++y) {
    memcpy(dst_buf, src_buf, bytes_per_pixel * copy_area.size.x);
    dst_buf += BytesPerScanLine(config_);
    src_buf += BytesPerScanLine(src.config_);
  }

  return MAKE_ERROR(Error::kSuccess);
}

/**
 * @fn
 * FrameBuffer::Moveメソッド
 * 
 * @brief
 * 表示領域の移動。移動前の領域のクリアなどはしないので、呼び出し側でやること
 * @param [in] pos 移動先の左上の座標
 * @param [in] src 移動する範囲領域。Rectangleインスタンス
 */
void FrameBuffer::Move(Vector2D<int> dst_pos, const Rectangle<int>& src){
  const auto bytes_per_pixel = BytesPerPixel(config_.pixel_format);
  const auto bytes_per_scan_line = BytesPerScanLine(config_);

  if (dst_pos.y < src.pos.y) { // move up
    uint8_t* dst_buf = FrameAddrAt(dst_pos, config_);
    const uint8_t* src_buf = FrameAddrAt(src.pos, config_);
    for (int y = 0; y < src.size.y; ++y) {
      memcpy(dst_buf, src_buf, bytes_per_pixel * src.size.x);
      dst_buf += bytes_per_scan_line;
      src_buf += bytes_per_scan_line;
    }
  } else {  // move down
    uint8_t* dst_buf = FrameAddrAt(dst_pos + Vector2D<int>{0, src.size.y - 1}, config_);
    const uint8_t* src_buf = FrameAddrAt(src.pos + Vector2D<int>{0, src.size.y - 1}, config_);

    // コピー元を上書きしてしまわないように下から上にコピーしていく
    for (int y = 0; y < src.size.y; ++y) {
      memcpy(dst_buf, src_buf, bytes_per_pixel * src.size.x);
      dst_buf -= bytes_per_scan_line;
      src_buf -= bytes_per_scan_line;
    }
  }
}
