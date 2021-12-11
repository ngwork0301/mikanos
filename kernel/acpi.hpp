/**
 * @file acpi.hpp
 * @brief 
 * ACPI テーブル定義や操作用プログラムを集めたファイル
 */

#pragma once

#include <cstdint>

namespace acpi {
  struct RSDP {
    char signature[8];         //! offset: 0 RSDPのシグニチャ。「RSD PTR 」の8文字が入っている
    uint8_t checksum;          //! offset: 8 前半20バイトのためのチェックサム
    char oem_id[6];            //! offset: 9 OEMの名称
    uint8_t revision;          //! offset:15 RSDP構造体のバージョン番号。ACPI1.0の場合は0。6.2では2が入る
    uint32_t rsdt_address;     //! offset:16 RSDTを指す32ビットの物理アドレス
    uint32_t length;           //! offset:20 RSDP全体のバイト数
    uint64_t xsdt_address;     //! offset:24 XSDTを指す64ビットの物理アドレス
    uint8_t exteded_checksum;  //! offset:32 拡張領域を含めたRSDP全体のチェックサム
    char reserved[3];          //! offset:33 予約領域

    bool IsValid() const;
  } __attribute__((packed));

  void Initialize(const RSDP& rsdp);
} // namespace acpi
