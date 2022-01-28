#include "fat.hpp"

#include <cstring>
#include <cctype>

#include "logger.hpp"
namespace fat {

BPB* boot_volume_image;
//! 1クラスタあたりのバイト数(初期化時に算出する)
unsigned long bytes_per_cluster;

/**
 * @fn
 * fat::Initialize関数
 * @brief 
 * FATボリュームの初期化をおこなう。
 * @param volume_image BPBへのポインタ
 */
void Initialize(void* volume_image){
  boot_volume_image = reinterpret_cast<fat::BPB*>(volume_image);
  // 1クラスタあたりのバイト数 = 1ブロックあたりのバイト数 x 1クラスタあたりのブロック数
  bytes_per_cluster =
    static_cast<unsigned long>(boot_volume_image->bytes_per_sector) *
    boot_volume_image->sectors_per_cluster;
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

/**
 * @fn
 * NextCluster関数
 * @brief 
 * クラスタチェーンの次のクラスタ番号を得る
 * @param [in] cluster クラスタ番号
 * @return unsigned long 次のクラスタ番号
 */
unsigned long NextCluster(unsigned long cluster){
  //! 予約ブロック数分のオフセットを算出
  uintptr_t fat_offset =
    boot_volume_image->reserved_sector_count *
    boot_volume_image->bytes_per_sector;
  //! オフセット分ずらした先頭ポインタを算出
  uint32_t* fat = reinterpret_cast<uint32_t*>(
      reinterpret_cast<uintptr_t>(boot_volume_image) + fat_offset);
  //! 次のクラスタ番号を取得
  uint32_t next = fat[cluster];
  if (next >= 0x0ffffff8ul) {
    // 0x0FFFFFF8のときはクラスタチェーンの末尾
    return kEndOfClusterchain;
  }
  return next;
}

/**
 * @fn
 * NameIsEqual関数
 * @brief 
 * 指定されたディレクトリエントリに定義されているファイル名が指定された文字列と一致するか確認
 * @param [in] entry ディレクトリエントリ
 * @param [in] name ファイル名文字列
 * @return true 
 * @return false 
 */
bool NameIsEqual(const DirectoryEntry& entry, const char* name){
  //! 短名の8+3の名前絵を入れる変数
  unsigned char name83[11];
  // 空白文字で初期化
  memset(name83, 0x20, sizeof(name83));

  int i = 0;
  int i83 = 0;
  for(; name[i] != 0 && i83 < sizeof(name83); ++i, ++i83) {
    if (name[i] == '.') {
      // ドットがあった場合はその後は拡張子として末尾の3のほうに入れる
      i83 = 7;
      continue;
    }
    // 引数nameで与えられた文字を大文字にしてname83に入れる
    name83[i83] = toupper(name[i]);
  }

  return memcmp(entry.name, name83, sizeof(name83)) == 0;
}


/**
 * @fn
 * FindFile関数
 * @brief 
 * 指定されたファイル名のファイルを探す
 * @param [in] name 検索したいファイル名
 * @param [in] directory_cluster クラスタ番号
 * @return DirectoryEntry* ファイルへの先頭ポインタ
 */
DirectoryEntry* FindFile(const char* name, unsigned long directory_cluster){
  if(directory_cluster == 0) {
    // ０が入った場合は、ルートディレクトリから検索
    directory_cluster = boot_volume_image->root_cluster;
  }

  while (directory_cluster != kEndOfClusterchain) {
    // クラスタ番号からブロックを取得
    auto dir = GetSectorByCluster<DirectoryEntry>(directory_cluster);
    // そのクラスタ内のディレクトリエントリごとにループ
    for (int i = 0; i < bytes_per_cluster / sizeof(DirectoryEntry); ++i) {
      if (NameIsEqual(dir[i], name)) {
        return &dir[i];
      }
    }
    // 次のクラスタへ移動
    directory_cluster = NextCluster(directory_cluster);
  }

  // 見つからなかった場合
  return nullptr;
}


} // namespace
