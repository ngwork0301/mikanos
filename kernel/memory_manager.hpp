/**
 * @file
 * memory_manager.hpp
 * 
 * @brief
 * メモリ管理クラスと周辺機能を集めたファイル．
 */
#pragma once

#include <array>
#include <limits>

#include "error.hpp"
#include "memory_map.hpp"

namespace {
  constexpr unsigned long long operator""_KiB(unsigned long long kib) {
    return kib * 1024;
  }

  constexpr unsigned long long operator""_MiB(unsigned long long mib) {
    return mib * 1024_KiB;
  }

  constexpr unsigned long long operator""_GiB(unsigned long long gib) {
    return gib * 1024_MiB;
  }
}

/** @brief 物理メモリフレーム1つの大きさ(バイト) */
static const auto kBytesPerFrame{4_KiB};

class FrameID {
  public:
    explicit FrameID(size_t id) : id_{id}{}
    size_t ID() const { return id_; }
    void* Frame() const { return reinterpret_cast<void *>(id_ * kBytesPerFrame); }

  private:
    size_t id_;
};

//! 未定義のページフレーム番号
static const FrameID kNullFrame{std::numeric_limits<size_t>::max()};

/**
 * @class
 * BitmapMemoryManagerクラス
 * 
 * @brief
 * ビットマップ方式でメモリを管理する
 */
class BitmapMemoryManager {
  public:
    /** @brief このメモリ管理クラスで扱える最大の物理メモリ章（バイト） */
    static const auto kMaxPhysicalMemoryBytes{128_GiB};
    /** @brief kMaxPhysicalMemoryBytesまでの物理メモリを扱うために必要なフレーム数 */
    static const auto kFrameCount{kMaxPhysicalMemoryBytes / kBytesPerFrame};

    /** @brief ビットマップ配列の要素型 */
    using MapLineType = unsigned long;
    /** @brief ビットマップ配列の１つの要素のビット数==フレーム数 */
    static const size_t kBitsPerMapLine{8 * sizeof(MapLineType)};

    /** @brief インスタンスを初期化する */
    BitmapMemoryManager();

    /** @brief 要求されたフレーム数の領域を確保して先頭のフレームIDを返す */
    WithError<FrameID> Allocate(size_t num_frames);
    Error Free(FrameID start_frame, size_t num_frames);
    void MarkAllocated(FrameID start_frame, size_t num_frames);

    void SetMemoryRange(FrameID range_begin, FrameID range_end);

  private:
    /** 1ページフレームを1ビットで表したビットマップ */
    std::array<MapLineType, kFrameCount / kBitsPerMapLine> alloc_map_;
    /** @brief このメモリマネージャで扱うメモリ範囲の始点 */
    FrameID range_begin_;
    /** @brief このメモリマネージャで扱うメモリ範囲の終点。最終フレームの次のフレーム */
    FrameID range_end_;
    
    bool GetBit(FrameID frame) const;
    void SetBit(FrameID frame, bool allocated);
};

extern BitmapMemoryManager* memory_manager;
void InitializeMemoryManager(const MemoryMap& memory_map);
