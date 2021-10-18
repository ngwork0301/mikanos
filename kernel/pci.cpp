#include "pci.hpp"

#include "asmfunc.h"

namespace pci{
  /**
   * @fn
   * AddDevice関数
   * 
   * @brief
   * devices[num_device]に情報を書き込み、num_deviceをインクリメントする
   * @param [in] device デバイス構造体
   * @return  Error(成功時にError::kSuccessを返す)
   */
  Error AddDevice(const Device& device) {
    // 確保したメモリ以上にデバイスがあったら、エラー
    if (num_device == devices.size()) {
      return MAKE_ERROR(Error::kFull);
    }

    // Deviceインスタンスを生成してdevicesにいれる。
    devices[num_device] = device;
    ++num_device;
    return MAKE_ERROR(Error::kSuccess);
  }

  /**
   * @fn
   * ScanFunction関数
   * 
   * @brief
   * 指定のファンクションを devicesに追加する
   * もしPCI-PCIブリッジなら、セカンダリバスに対し ScanBus を実行する
   * @param [in] bus バス番号
   * @param [in] device デバイス番号
   * @param [in] function ファンクション番号
   * @return  Error(成功時にError::kSuccessを返す)
   */
  Error ScanFunction(uint8_t bus, uint8_t device, uint8_t function) {
    auto class_code = ReadClassCode(bus, device, function);
    auto header_type = ReadHeaderType(bus, device, function);
    Device dev{bus, device, function, header_type, class_code};
    if (auto err = AddDevice(dev)) {
      return err;
    }

    // クラスコード内のベースとサブを読み取ってPCI-PCIブリッジかどうか判定
    // ベースコードが0x05は、PCI。サブコードが0x04は、PCI
    if (class_code.base == 0x05u && class_code.sub == 0x04u) {
      // standard PCI-PCI bridge
      auto bus_numbers = ReadBusNumbers(bus, device, function);
      uint8_t secondary_bus = (bus_numbers >> 8) & 0xffu;
      return ScanBus(secondary_bus);
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  /**
   * @fn
   * ScanDevice関数
   * 
   * @brief
   * 指定のデバイス番号の各ファンクションをスキャンする
   * 有効なファンクションを見つけたらScanFunctionを実行する
   * @param [in] bus バス番号
   * @param [in] device デバイス番号
   * @return  Error(成功時にError::kSuccessを返す)
   */
  Error ScanDevice(uint8_t bus, uint8_t device) {
    // ファンクション番号0のファンクションをチェック
    if (auto err = ScanFunction(bus, device, 0)) {
      return err;
    }
    // ヘッダからシングルファンクションデバイスかどうか判定。
    // シングルファンクションデバイスであれば、バス0を担当するホストブリッジが1つあるのみ
    if (IsSingleFunctionDevice(ReadHeaderType(bus, device, 0))) {
      return MAKE_ERROR(Error::kSuccess);
    }

    // マルチファンクションの場合は、一つずつファンクションをチェックする
    for (uint8_t function = 1; function < 8; ++function) {
      if (ReadVendorId(bus, device, function) == 0xffffu) {
        continue;
      }
      if (auto err = ScanFunction(bus, device, function)) {
        return err;
      }
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  /**
   * @fn
   * ScanBus関数
   * 
   * @brief
   * 指定のバス番号の各デバイスをスキャンする
   * 有効なデバイスを見つけたら、ScanDeviceを実行する
   * @param [in] bus バス番号
   * @return  Error(成功時にError::kSuccessを返す)
   */
  Error ScanBus(uint8_t bus) {
    for (uint8_t device = 0; device < 32; ++device) {
      // ファンクション0のベンダーIDが無効であれば次のデバイス
      if (ReadVendorId(bus, device, 0) == 0xffffu) {
        continue;
      }
      // 各デバイスをスキャン
      if (auto err = ScanDevice(bus, device)) {
        return err;
      }
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  /**
   * @fn
   * MakeAddress関数
   * 
   * @brief
   * CONFIG_ADDRESS用の32ビット整数を生成する
   * 
   * @param [in] bus バス番号(0-255)
   * @param [in] device デバイス番号(0-32)
   * @param [in] function ファンクション番号(0-7)
   * @param [in] reg_addr レジスタアドレス
   */
  uint32_t MakeAddress(uint8_t bus, uint8_t device,
                      uint8_t function, uint8_t reg_addr) {
    // xをbits数だけ左シフトするラムダ式shlを定義
    auto shl = [](uint32_t x, unsigned int bits) {
      return x << bits;
    };

    return shl(1, 31) // enable bit
        | shl(bus, 16)
        | shl(device, 11)
        | shl(function, 8)
        | (reg_addr & 0xfcu);
  }

  /**
   * @fn
   * WriteAddress関数
   * 
   * @brief
   * 引数にしていしたアドレスをCONFIG_ADDRESSレジスタに書き込む。アセンブリコードへのラッパ
   * 
   * @param [in] address アドレス
   */
  void WriteAddress(uint32_t address) {
    IoOut32(pci::kConfigAddress, address);
  }

  /**
   * @fn
   * WriteData関数
   * 
   * @brief
   * 引数に指定した値をCONFIG_DATAレジスタに書き込む。アセンブリコードへのラッパ
   * 
   * @param [in] value CONFIG_DATAレジスタに書き込む値
   */
  void WriteData(uint32_t value) {
    IoOut32(pci::kConfigData, value);
  }

  /**
   * @fn
   * ReadData関数
   * 
   * @brief
   * CONFIG_DATAレジスタからデータを32bit整数として読み取る。アセンブリコードへのラッパ
   */
  uint32_t ReadData() {
    return IoIn32(pci::kConfigData);
  }

  /**
   * @fn
   * ReadVendorId関数
   * 
   * @brief
   * ベンダ ID レジスタを読み取る（全ヘッダタイプ共通）
   * 
   * @param [in] bus バス番号
   * @param [in] device デバイス番号
   * @param [in] function ファンクション番号
   * @return ベンダーID
   */
  uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function) {
    // CONFIG_ADDRESSに指定したバス番号、デバイス番号、ファンクション番号の整数を書き込む
    WriteAddress(MakeAddress(bus, device, function, 0x00));
    // CONFIG_DATAの値を返す
    return ReadData() & 0xffffu;
  }

  /**
   * @fn
   * ReadDeviceId関数
   * 
   * @brief
   * デバイス ID レジスタを読み取る（全ヘッダタイプ共通）
   * 
   * @param [in] bus バス番号
   * @param [in] device デバイス番号
   * @param [in] function ファンクション番号
   * @return デバイスID
   */
  uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function) {
      WriteAddress(MakeAddress(bus, device, function, 0x00));
      return ReadData() >> 16;
    }

  /**
   * @fn
   * ReadHeaderType関数
   * 
   * @brief
   * ヘッダタイプレジスタを読み取る（全ヘッダタイプ共通）
   * 
   * @param [in] bus バス番号
   * @param [in] device デバイス番号
   * @param [in] function ファンクション番号
   * @return ヘッダー種別コード
   */
  uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function) {
    WriteAddress(MakeAddress(bus, device, function, 0x0c));
    return (ReadData() >> 16) & 0xffu;
  }

  /**
   * @fn
   * ReadClassCode関数
   * 
   * @brief
   * クラスコードレジスタを読み取る（全ヘッダタイプ共通）
   * 返される 32 ビット整数の構造は次の通り
   *   - 31:24 : ベースクラス
   *   - 23:16 : サブクラス
   *   - 15:8  : インターフェース
   *   - 7:0   : リビジョン
   * 
   * @param [in] bus バス番号
   * @param [in] device デバイス番号
   * @param [in] function ファンクション番号
   * @return クラスコード
   */
  ClassCode ReadClassCode(uint8_t bus, uint8_t device, uint8_t function) {
    WriteAddress(MakeAddress(bus, device, function, 0x08));
    auto reg = ReadData();
    ClassCode cc;
    cc.base       = (reg >> 24) & 0xffu;
    cc.sub        = (reg >> 16) & 0xffu;
    cc.interface  = (reg >> 8)  & 0xffu;
    return cc;
  }

  /**
   * @fn
   * ReadBusNumbers関数
   * 
   * @brief
   * バス番号レジスタを読み取る（ヘッダタイプ 1 用）
   * 返される 32 ビット整数の構造は次の通り．
   *   - 23:16 : サブオーディネイトバス番号
   *   - 15:8  : セカンダリバス番号
   *   - 7:0   : リビジョン番号
   * 
   * @param [in] bus バス番号
   * @param [in] device デバイス番号
   * @param [in] function ファンクション番号
   * @return バス番号
   */
  uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function) {
    WriteAddress(MakeAddress(bus, device, function, 0x18));
    return ReadData();
  }

  /**
   * @fn
   * IsSingleFunctionDevice関数
   * 
   * @brief
   * 単一ファンクションの場合に真を返す
   * 
   * @param [in] header_type ヘッダー種別
   * @return 単一ファンクションの場合にtrue、マルチファンクションの場合はfalse
   */
  bool IsSingleFunctionDevice(uint8_t header_type) {
    return (header_type & 0x80u) == 0;
  }

  /**
   * @fn 
   * ScanAllBus関数
   * 
   * @brief
   * PCIデバイスをすべて探索し、devicesに格納する
   * 
   * バス0から再帰的にPCIデバイスを探索し、devicesの先頭から詰めて書き込む
   * 発見したデバイスの数を num_device に設定する。
   * 
   * @return  Error(成功時にError::kSuccessを返す)
   */
  Error ScanAllBus() {
    // ホストブリッジ(バス0、デバイス0、ファンクション0)のヘッダタイプを読み取り
    auto header_type = ReadHeaderType(0, 0, 0);
    // ヘッダからシングルファンクションデバイスかどうか判定。
    // シングルファンクションデバイスであれば、バス0を担当するホストブリッジが1つあるのみ
    if (IsSingleFunctionDevice(header_type)) {
      return ScanBus(0);
    }

    for (uint8_t function = 1; function < 8; ++function) {
      if (ReadVendorId(0, 0, function) == 0xffffu) {
          continue;
      }
      // マルチファンクションデバイスのホストブリッジであれば、
      // 複数のホストブリッジがあり、ファンクション番号が担当するバス番号になる
      if (auto err = ScanBus(function)) {
          return err;
      }
    }
    return MAKE_ERROR(Error::kSuccess);
  }

  uint32_t ReadConfReg(const Device& dev, uint8_t reg_addr) {
    WriteAddress(MakeAddress(dev.bus, dev.device, dev.function, reg_addr));
    return ReadData();
  }

  void WriteConfReg(const Device& dev, uint8_t reg_addr, uint32_t value) {
    WriteAddress(MakeAddress(dev.bus, dev.device, dev.function, reg_addr));
    WriteData(value);
  }

  WithError<uint64_t> ReadBar(Device& device, unsigned int bar_index) {
    if (bar_index >= 6) {
      return {0, MAKE_ERROR(Error::kIndexOutOfRange)};
    }

    const auto addr = CalcBarAddress(bar_index);
    const auto bar = ReadConfReg(device, addr);

    // 32 bit address
    if ((bar & 4u) == 0) {
      return {bar, MAKE_ERROR(Error::kSuccess)};
    }

    // 64 bit address
    if (bar_index >= 5) {
      return {0, MAKE_ERROR(Error::kIndexOutOfRange)};
    }

    const auto bar_upper = ReadConfReg(device, addr + 4);
    return {
      bar | (static_cast<uint64_t>(bar_upper) << 32),
      MAKE_ERROR(Error::kSuccess)
    };
  }

  /** 
   * @fn
   * ReadMSICapability関数
   * 
   * @brief
   * 指定された MSI ケーパビリティ構造を読み取る
   *
   * @param dev  MSI ケーパビリティを読み込む PCI デバイス
   * @param cap_addr  MSI ケーパビリティレジスタのコンフィグレーション空間アドレス
   * @return MSICapability
   */
  MSICapability ReadMSICapability(const Device& dev, uint8_t cap_addr) {
    MSICapability msi_cap{};

    msi_cap.header.data = ReadConfReg(dev, cap_addr);
    msi_cap.msg_addr = ReadConfReg(dev, cap_addr + 4);

    uint8_t msg_data_addr = cap_addr + 8;
    if (msi_cap.header.bits.addr_64_capable) {
      msi_cap.msg_upper_addr = ReadConfReg(dev, cap_addr + 8);
      msg_data_addr = cap_addr + 12;
    }

    msi_cap.msg_data = ReadConfReg(dev, msg_data_addr);

    if (msi_cap.header.bits.per_vector_mask_capable) {
      msi_cap.mask_bits = ReadConfReg(dev, msg_data_addr + 4);
      msi_cap.pending_bits = ReadConfReg(dev, msg_data_addr + 8);
    }

    return msi_cap;
  }

  /** 
   * @fn
   * WriteMSICapability関数
   * 
   * @brief 
   * 指定された MSI ケーパビリティ構造に書き込む
   *
   * @param dev  MSI ケーパビリティを読み込む PCI デバイス
   * @param cap_addr  MSI ケーパビリティレジスタのコンフィグレーション空間アドレス
   * @param msi_cap  書き込む値
   */
  void WriteMSICapability(const Device& dev, uint8_t cap_addr,
                          const MSICapability& msi_cap) {
    WriteConfReg(dev, cap_addr, msi_cap.header.data);
    WriteConfReg(dev, cap_addr + 4, msi_cap.msg_addr);

    uint8_t msg_data_addr = cap_addr + 8;
    if (msi_cap.header.bits.addr_64_capable) {
      WriteConfReg(dev, cap_addr + 8, msi_cap.msg_upper_addr);
      msg_data_addr = cap_addr + 12;
    }

    WriteConfReg(dev, msg_data_addr, msi_cap.msg_data);

    if (msi_cap.header.bits.per_vector_mask_capable) {
      WriteConfReg(dev, msg_data_addr + 4, msi_cap.mask_bits);
      WriteConfReg(dev, msg_data_addr + 8, msi_cap.pending_bits);
    }
  }

  /**
   * @fn
   * ReadCapabilityHeader関数
   * 
   * @brief
   * MSI ケーパビリティのヘッダを読み取る
   */
  CapabilityHeader ReadCapabilityHeader(const Device& dev, uint8_t addr) {
    CapabilityHeader header;
    header.data = pci::ReadConfReg(dev, addr);
    return header;
  }

  /**
   * @fn
   * ConfigureMSIRegister関数
   * 
   * @brief
   * 指定された MSI レジスタを設定する
   * 
   * @param [in] dev  設定対象の PCI デバイス
   * @param [in] cap_addr  MSI Capabilityのアドレス
   * @param [in] msg_addr  割り込み発生時にメッセージを書き込む先のアドレス
   * @param [in] msg_data  割り込み発生時に書き込むメッセージの値
   * @param [in] num_vector_exponent  割り当てるベクタ数（2^n の n を指定）
   */
  Error ConfigureMSIRegister(const Device& dev, uint8_t cap_addr,
                            uint32_t msg_addr, uint32_t msg_data,
                            unsigned int num_vector_exponent) {
    auto msi_cap = ReadMSICapability(dev, cap_addr);

    if (msi_cap.header.bits.multi_msg_capable <= num_vector_exponent) {
      msi_cap.header.bits.multi_msg_enable =
        msi_cap.header.bits.multi_msg_capable;
    } else {
      msi_cap.header.bits.multi_msg_enable = num_vector_exponent;
    }

    msi_cap.header.bits.msi_enable = 1;
    msi_cap.msg_addr = msg_addr;
    msi_cap.msg_data = msg_data;

    WriteMSICapability(dev, cap_addr, msi_cap);
    return MAKE_ERROR(Error::kSuccess);
  }

  /**
   * @fn
   * ConfigureMSIXRegister関数
   * 
   * @brief
   * 指定された MSI-X レジスタを設定する
   * 現状未実装のため、エラーを返す
   */
  Error ConfigureMSIXRegister(const Device& dev, uint8_t cap_addr,
                             uint32_t msg_addr, uint32_t msg_data,
                             unsigned int num_vector_exponent) {
    return MAKE_ERROR(Error::kNotImplemented);
  }

  /** 
   * @fn
   * ConfigureMSI関数
   * 
   * @brief
   * MSI または MSI-X 割り込みを設定する
   *
   * @param dev  設定対象の PCI デバイス
   * @param msg_addr  割り込み発生時にメッセージを書き込む先のアドレス
   * @param msg_data  割り込み発生時に書き込むメッセージの値
   * @param num_vector_exponent  割り当てるベクタ数（2^n の n を指定）
   */
  Error ConfigureMSI(const Device& dev, uint32_t msg_addr, uint32_t msg_data,
                     unsigned int num_vector_exponent) {
    uint8_t cap_addr = ReadConfReg(dev, 0x34) & 0xffu;
    uint8_t msi_cap_addr = 0, msix_cap_addr = 0;
    while (cap_addr != 0) {
      auto header = ReadCapabilityHeader(dev, cap_addr);
      if (header.bits.cap_id == kCapabilityMSI) {
        msi_cap_addr = cap_addr;
      } else if (header.bits.cap_id == kCapabilityMSIX) {
        msix_cap_addr = cap_addr;
      }
      cap_addr = header.bits.next_ptr;
    }

    if (msi_cap_addr) {
      return ConfigureMSIRegister(dev, msi_cap_addr, msg_addr, msg_data, num_vector_exponent);
    } else if (msix_cap_addr) {
      return ConfigureMSIXRegister(dev, msix_cap_addr, msg_addr, msg_data, num_vector_exponent);
    }
    return MAKE_ERROR(Error::kNoPCIMSI);
  }

  /**
   * @fn
   * ConfigureMSIFixedDestination関数
   * 
   * @brief
   * MSIを固定のDestination(CPUコア)に設定する
   * 
   * @param [in] dev PCIデバイス
   * @param [in] apic_id APIC ID(=CPUコア番号)
   * @param [in] trigger_mode トリガーモード
   * @param [in] delivery_mode デリバリーモード
   * @param [in] vector 割り込みベクター番号
   * @param [in] num_vector_exponent ？
   * @return Error
   */
  Error ConfigureMSIFixedDestination(
      const Device& dev, uint8_t apic_id,
      MSITriggerMode trigger_mode, MSIDeliveryMode delivery_mode,
      uint8_t vector, unsigned int num_vector_exponent) {
    uint32_t msg_addr = 0xfee00000u | (apic_id << 12);
    uint32_t msg_data = (static_cast<uint32_t>(delivery_mode) << 8) | vector;
    if (trigger_mode == MSITriggerMode::kLevel) {
      msg_data |= 0xc000;
    }
    return ConfigureMSI(dev, msg_addr, msg_data, num_vector_exponent);
  }

}
