/**
 * @file fat.hpp
 * @brief 
 * FATファイルシステム関連の実装を集めたファイル
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <iostream>

#include "error.hpp"
#include "file.hpp"

namespace fat{
  struct BPB {
    uint8_t jump_boot[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
  } __attribute__((packed));
  
  enum class Attribute : uint8_t {
    kReadOnly  = 0x01,
    kHidden    = 0x02,
    kSystem    = 0x04,
    kVolumeID  = 0x08,
    kDirectory = 0x10,
    kArchive   = 0x20,
    kLongName  = 0x0f,
  };
  
  struct DirectoryEntry {
    unsigned char name[11];
    Attribute attr;
    uint8_t ntres;
    uint8_t create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
  
    uint32_t FirstCluster() const {
      return first_cluster_low |
        (static_cast<uint32_t>(first_cluster_high) << 16);
    }
  } __attribute__((packed));

  /**
   * @class
   * fat::FileDescriptorクラス
   * @brief 
   * fatフォーマットにおけるファイルディスクリプタ
   */
  class FileDescriptor : public ::FileDescriptor{
    public:
      explicit FileDescriptor(DirectoryEntry& fat_entry);
      size_t Read(void* buf, size_t len) override;
      size_t Write(const void* buf, size_t len) override;
      size_t Size() const override { return fat_entry_.file_size; };
      size_t Load(void* buf, size_t offset, size_t len) override;

    private:
      //! このファイルディスクリプタがさすファイルへの参照
      DirectoryEntry& fat_entry_;
      //! ファイル先頭からの読み込みオフセット（バイト単位）
      size_t rd_off_ = 0;
      //! rd_off_が指す位置に対応するクラスタ番号
      unsigned long rd_cluster_ = 0;
      //! 読み込みのクラスタ先頭からのオフセット（バイト単位）
      size_t rd_cluster_off_ = 0;
      //! 書き込みオフセット（バイト単位）
      size_t wr_off_ = 0;
      //! wr_off_が指す位置に対応するクラスタ番号
      unsigned long wr_cluster_ = 0;
      //! 書き込みのクラスタ先頭からのオフセット（バイト単位）
      size_t wr_cluster_off_ = 0;
  };

  
  //! クラスタチェーンの末尾を表す特別なクラスタ番号
  static const unsigned long kEndOfClusterchain = 0x0ffffffflu;

  extern BPB* boot_volume_image;
  extern unsigned long bytes_per_cluster;

  void Initialize(void* volume_image);
  uintptr_t GetClusterAddr(unsigned long cluster);
  unsigned long AllocateClusterChain(size_t n);

  /**
   * @brief Get the Sector By Cluster object
   * クラスタ番号をブロック先頭のポインタに変換する
   * @tparam T 
   * @param cluster クラスタ番号
   * @return T* ブロック先頭のポインタ
   */
  template <class T>
  T* GetSectorByCluster(unsigned long cluster) {
      return reinterpret_cast<T*>(GetClusterAddr(cluster));
  }
  void ReadName(const DirectoryEntry& entry, char* base, char* ext);
  void FormatName(const DirectoryEntry& entry, char* dest);

  unsigned long NextCluster(unsigned long cluster);
  bool NameIsEqual(const DirectoryEntry& entry, const char* name);
  std::pair<DirectoryEntry*, bool> 
      FindFile(const char* path, unsigned long directory_cluster = 0);
  WithError<DirectoryEntry*> CreateFile(const char* path);
  size_t LoadFile(void* buf, size_t len, DirectoryEntry& entry);
}
