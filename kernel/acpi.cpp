#include "acpi.hpp"

#include <cstring>
#include <cstdlib>
#include "error.hpp"
#include "logger.hpp"

namespace {
  /**
   * @fn
   * SumBytes関数
   * @brief 
   * 引数で与えられたデータの総和を計算する
   * 
   * @tparam T 
   * @param data [in] 総和を計算する対象データ
   * @param bytes [in] 対象データのうち、計算対象とする先頭からのバイト数
   * @return uint8_t 計算した総和
   */
  template <typename T>
  uint8_t SumBytes(const T* data, size_t bytes) {
    return SumBytes(reinterpret_cast<const uint8_t*>(data), bytes);
  }

  template <>
  uint8_t SumBytes<uint8_t>(const uint8_t* data, size_t bytes) {
    uint8_t sum = 0;
    for (size_t i = 0; i < bytes; ++i) {
      sum += data[i];
    }
    return sum;
  }
}

namespace acpi {
  /**
   * @fn
   * acpi::RSDP::IsValid関数
   * @brief 
   * RSDP構造体が有効か検査する
   * 
   * @return true 有効なRSDP
   * @return false 不正なRSDP 
   */
  bool RSDP::IsValid() const {
    // シグニチャのチェック
    if (strncmp(this->signature, "RSD PTR ", 8) != 0) {
      Log(kDebug, "invalid signature: %.8s\n", this->signature);
      return false;
    }
    // リビジョンのチェック
    if (this->revision != 2) {
      Log(kDebug, "ACPI revision must be 2: %d\n", this->revision);
      return false;
    }
    // 前半20バイトのチェックサム適合チェク
    if (auto sum = SumBytes(this, 20); sum != 0) {
      Log(kDebug, "sum of 20 bytes must be 0: %d\n", sum);
      return false;
    }
    // 拡張領域を含めたRSDP全体のチェックサム適合チェック
    if (auto sum = SumBytes(this, 36); sum != 0) {
      Log(kDebug, "sum of 36 bytes must be 0: %d\n", sum);
      return false;
    }
    return true;
  }


  /**
   * @fn
   * acpi::Initialize関数
   * 
   * @brief
   * ACPIの初期化をおこなう。
   * 
   * @param [in] rsdp RSDP構造体へのポインタ
   */
  void Initialize(const RSDP& rsdp) {
    if (!rsdp.IsValid()) {
      Log(kError, "RSDP is invalid.\n");
      exit(1);
    }
  }
}
