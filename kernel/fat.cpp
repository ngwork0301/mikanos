#include "fat.hpp"

#include <cstring>

namespace fat {

BPB* boot_volume_image;

/**
 * @fn
 * fat::Initialize関数
 * @brief 
 * FATボリュームの初期化をおこなう。
 * @param volume_image BPBへのポインタ
 */
void Initialize(void* volume_image){
  boot_volume_image = reinterpret_cast<fat::BPB*>(volume_image);
}

/**
 * @brief Get the Cluster Addr object
 * クラスタ番号をブロック位置へ変換する
 * @param cluster クラスタ番号
 * @return uintptr_t ブロック位置
 */
uintptr_t GetClusterAddr(unsigned long cluster){
  // ブロック番号を取得
  // ブロック番号 = 予約ブロック数 + FAT数 * FAT32サイズ + (クラスタ番号-2) * 1クラスタあたりのブロック数 
  unsigned long sector_num = 
    boot_volume_image->reserved_sector_count +
    boot_volume_image->num_fats * boot_volume_image->fat_size_32 +
    (cluster - 2) * boot_volume_image->sectors_per_cluster;
  // オフセットを算出
  // オフセット = ブロック番号 x 1ブロックあたりのバイト数
  uintptr_t offset = sector_num * boot_volume_image->bytes_per_sector;
  // BPBブロックの先頭からオフセットを足したアドレスを返す
  return reinterpret_cast<uintptr_t>(boot_volume_image) + offset;
}

void ReadName(const fat::DirectoryEntry& entry, char* base, char* ext){
  // 拡張子以外部分のコピー (短名8+3のうちの8をコピー)
  memcpy(base, &entry.name[0], 8);
  base[8] = '\0';  // 末尾に終端文字を入れる
  for (int i = 7; i >= 0 && base[i] == ' '; --i){
    // 末尾が空白の場合は、消す。
    base[i] = '\0';
  }

  // 拡張子部分のコピー (短名8+3のうちの3をコピー)
  memcpy(ext, &entry.name[8], 3);
  ext[3] = '\0';  // 末尾に終端文字を入れる
  for (int i = 2; i >= 0 && ext[i] == ' '; --i) {
    // 末尾が空白の場合は、消す。
    ext[i] = '\0';
  }
}

} // namespace
