#pragma once

#include <stdint.h>

enum PixelFormat {
    kPixelRGBResv8BitPerColor,
    kPixelBGRResv8BitPerColor,
};

/**
 * @struct
 * FrameBufferConfig構造体
 */
struct FrameBufferConfig {
    uint8_t* frame_buffer;           //! フレームバッファ領域へのポインタ
    uint32_t pixels_per_scan_line;   //! 余白を含めた横方向のピクセル数
    uint32_t horizontal_resolution;  //! 水平解像度
    uint32_t vertical_resolution;    //! 垂直解錠℃
    enum PixelFormat pixel_format;   //! ピクセルデータ形式
};
