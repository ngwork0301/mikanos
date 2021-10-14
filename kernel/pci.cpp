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

}
