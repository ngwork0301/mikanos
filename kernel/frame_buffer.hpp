/**
 * @file
 * frame_buffer.hpp
 * 
 * @brief
 * フレームバッファに関する処理を集めたファイル
 */
#pragma once

#include <vector>
#include <memory>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "error.hpp"

/**
 * @class
 * FrameBufferクラス
 * 
 * @brief
 * ディスプレイに描画するメモリ領域であるフレームバッファ(VRAM)を定義する。
 * 高速化のためのCopyメソッドを持つ
 */
class FrameBuffer {
  public:
    Error Initialize(const FrameBufferConfig& config);
    Error Copy(Vector2D<int> pos, const FrameBuffer& src,
               const Rectangle<int>& src_area);
    void Move(Vector2D<int> dst_pos, const Rectangle<int>& src);

    /**
     * @fn
     * Writerメソッド
     * 
     * @brief
     * getterメソッド。FrameBufferWriterのスマートポインタを返す
     * @return writer_ FrameBufferWriterのスマートポインタ
     */
    FrameBufferWriter& Writer() { return *writer_; };

  private:
    //! 描画領域の縦横サイズ、ピクセルのデータ形式などの構成情報
    FrameBufferConfig config_{};
    //! バッファ本体のピクセル配列
    std::vector<uint8_t> buffer_{};
    //! 描画するためのPixelWriterインスタンスへのスマートポインタ
    std::unique_ptr<FrameBufferWriter> writer_{};
};
