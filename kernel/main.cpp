#include <cstdint>

/**
 * @fn
 * KernelMain関数
 * 
 * @brief
 * カーネルのエントリポイント
 * 
 * @param [in] frame_buffer_base フレームバッファの先頭アドレス
 * @param [in] frame_buffer_size フレームバッファのサイズ
 */
extern "C" void KernelMain(uint64_t frame_buffer_base,
                           uint64_t frame_buffer_size) {
  // ピクセルを描画
  uint8_t* frame_buffer = reinterpret_cast<uint8_t*>(frame_buffer_base);
  for (uint64_t i = 0; i < frame_buffer_size; ++i) {
    frame_buffer[i] = i % 256;
  }
  // 無限ループ
  while(1) __asm__("hlt");
}
