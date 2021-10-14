#pragma once

#include <cstdint>
#include <array>

#include "error.hpp"

namespace pci {
  /**
   * @struct
   * ClassCode構造体
   * 
   * @brief PCI デバイスのクラスコード 
   */
  struct ClassCode {
    uint8_t base, sub, interface;

    /** @brief ベースクラスが等しい場合に真を返す */
    bool Match(uint8_t b) { return b == base; }
    /** @brief ベースクラスとサブクラスが等しい場合に真を返す */
    bool Match(uint8_t b, uint8_t s) { return Match(b) && s == sub; }
    /** @brief ベース，サブ，インターフェースが等しい場合に真を返す */
    bool Match(uint8_t b, uint8_t s, uint8_t i) {
      return Match(b, s) && i == interface;
    }
  };

  /**
   * @struct
   * Device構造体
   *  
   * @brief 
   * PCI デバイスを操作するための基礎データを格納する
   *
   * バス番号，デバイス番号，ファンクション番号はデバイスを特定するのに必須．
   * その他の情報は単に利便性のために加えてある．
   */
  struct Device {
    uint8_t bus, device, function, header_type;
    ClassCode class_code;
  };

  /** @brief CONFIG_ADDRESS レジスタの IOポートアドレス */
  const uint16_t kConfigAddress = 0x0cf8;
  /** @brief CONFIG_DATA レジスタの IOポートアドレス */
  const uint16_t kConfigData = 0x0cfc;
  /** @brief ScanAllBus()により発見されたPCIデバイス一覧 */
  inline std::array<Device, 32> devices;
  /** @brief devices の有効な要素の数 */
  inline int num_device;

  uint32_t MakeAddress(uint8_t bus, uint8_t device,
                      uint8_t function, uint8_t reg_addr);
  void WriteAddress(uint32_t address);
  void WriteData(uint32_t value);
  uint32_t ReadData();
  uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function);
  uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function);
  uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function);
  ClassCode ReadClassCode(uint8_t bus, uint8_t device, uint8_t function);

  inline uint16_t ReadVendorId(const Device& dev) {
    return ReadVendorId(dev.bus, dev.device, dev.function);
  }
  uint32_t ReadConfReg(const Device& dev, uint8_t reg_addr);
  void WriteConfReg(const Device& dev, uint8_t reg_addr, uint32_t value);

  uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function);
  bool IsSingleFunctionDevice(uint8_t header_type);

  Error AddDevice(uint8_t bus, uint8_t device,
                  uint8_t function, uint8_t header_type);
  Error ScanFunction(uint8_t bus, uint8_t device, uint8_t function);
  Error ScanDevice(uint8_t bus, uint8_t device);
  Error ScanBus(uint8_t bus);
  Error ScanAllBus();

  constexpr uint8_t CalcBarAddress(unsigned int bar_index) {
    return 0x10 + 4 * bar_index;
  }

  WithError<uint64_t> ReadBar(Device& device, unsigned int bar_index);

}
