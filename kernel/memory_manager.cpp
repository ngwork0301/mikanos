#include "memory_manager.hpp"

/**
 * @fn
 * BitmapMemoryManagerコンストラクタ
 */
BitmapMemoryManager::BitmapMemoryManager()
  : alloc_map_{}, range_begin_{FrameID{0}}, range_end_{FrameID{kFrameCount}} {
}

/**
 * @fn
 * Allocateメソッド
 * 
 * @brief
 * 空いているメモリ猟奇をファーストフィットアルゴリズムで探し出して、そのメモリを確保する
 * 
 * @param [in] num_frames 取得するサイズ（フレーム数)
 * @return WithError エラーコードと発生したフレームID
 */
WithError<FrameID> BitmapMemoryManager::Allocate(size_t num_frames) {
  size_t start_frame_id = range_begin_.ID();
  while (true) {
    size_t i = 0;
    for(; i < num_frames; ++i) {
      if (start_frame_id + i >= range_end_.ID()) {
        // メモリ管理がもつ範囲を超えたら、確保失敗
        return {kNullFrame, MAKE_ERROR(Error::kNoEnoughMemory)};
      }
      if (GetBit(FrameID{start_frame_id + i})) {
        // "start_frame_id + i" にあるフレームは割り当て済み
        break;
      }
    }
    if (i == num_frames) {
      // num_frames 分の空きが見つかった
      MarkAllocated(FrameID{start_frame_id}, num_frames);
      return {
        FrameID{start_frame_id},
        MAKE_ERROR(Error::kSuccess)
      };
    }
    // 次のフレームから再探索
    start_frame_id += i + 1;
  }
}

/**
 * @fn
 * Freeメソッド
 * 
 * @brief
 * メモリを解放する
 * 
 * @param [in] start_frame 解放対象の最初のフレーム
 * @param [in] num_frames 開放するサイズ（フレーム数）
 * @return Error
 */
Error BitmapMemoryManager::Free(FrameID start_frame, size_t num_frames) {
  for (size_t i = 0; i < num_frames; ++i) {
    SetBit(FrameID{start_frame.ID() + i}, false);
  }
  return MAKE_ERROR(Error::kSuccess);
}

/**
 * @fn
 * MarkAllocatedメソッド
 * 
 * @brief
 * 使用中領域を設定する
 * 
 * @param [in] start_frame 開始フレームの位置
 * @param [in] num_frames フレーム数
 */
void BitmapMemoryManager::MarkAllocated(FrameID start_frame, size_t num_frames) {
  for (size_t i = 0; i < num_frames; ++i) {
    SetBit(FrameID{start_frame.ID() + i}, true);
  }
}


/**
 * @fn
 * SetMemoryRangeメソッド
 * 
 * @brief このメモリマネージャで扱うメモリ範囲を設定する.
 * この呼び出し以降、Allocate によるメモリ割り当ては設定された範囲内でのみ行われる
 * 
 * @param [in] range_begin_ メモリ範囲の視点
 * @param [in] range_end_ メモリ範囲の終点。最終フレームの次のフレーム
 */
void BitmapMemoryManager::SetMemoryRange(FrameID range_begin, FrameID range_end){
  range_begin_ = range_begin;
  range_end_ = range_end;
}

/**
 * @fn
 * GetBitメソッド
 * 
 * @brief
 * 指定したフレームのビットの読み出し
 * 
 * @param [in] frame 読み出し対象のフレームID
 */
bool BitmapMemoryManager::GetBit(FrameID frame) const {
  auto line_index = frame.ID() / kBitsPerMapLine;
  auto bit_index = frame.ID() % kBitsPerMapLine;

  return (alloc_map_[line_index] & (static_cast<MapLineType>(1) << bit_index)) != 0;
}

/**
 * @fn
 * SetBitメソッド
 * 
 * @brief
 * 使用中／未使用を設定する
 * 
 * @param [in] frame 設定対象のフレームID
 * @param [in] allocated trueは使用中。falseは未使用
 */
void BitmapMemoryManager::SetBit(FrameID frame, bool allocated) {
  auto line_index = frame.ID() / kBitsPerMapLine;
  auto bit_index = frame.ID() % kBitsPerMapLine;

  if (allocated) {
    alloc_map_[line_index] |= (static_cast<MapLineType>(1) << bit_index);
  } else {
    alloc_map_[line_index] &= ~(static_cast<MapLineType>(1) << bit_index);
  }
}

extern "C" caddr_t program_break, program_break_end;

/**
 * @fn
 * InitializeHeap関数
 * 
 * @brief
 * プログラムブレークの初期化をおこなう
 * 
 * @param [in] memory_manager BitmapMemoryManagerのインスタンス
 */
Error InitializeHeap(BitmapMemoryManager& memory_manager) {
  const int kHeapFrames = 64 * 512;
  const auto heap_start = memory_manager.Allocate(kHeapFrames);
  if (heap_start.error) {
    return heap_start.error;
  }

  program_break = reinterpret_cast<caddr_t>(heap_start.value.ID() * kBytesPerFrame);
  program_break_end = program_break + kHeapFrames * kBytesPerFrame;
  return MAKE_ERROR(Error::kSuccess);
}
