#include "fat.hpp"

#include <cstring>
#include <cctype>

#include "logger.hpp"

namespace {

  /**
   * @fn
   * NextPathElement関数
   * @brief 
   * パスを/で区切った最初の要素を取得する
   * @param [in] path パス文字列
   * @param [out] path_elem 取得した最初の要素
   * @return std::pair<const char*, bool> 
   *  < 次のパス文字列のポインタ, 末尾かどうか > 
   */
  std::pair<const char*, bool>
  NextPathElement(const char* path, char* path_elem) {
    const char* next_slash = strchr(path, '/');
    if (next_slash == nullptr) {
      strcpy(path_elem, path);
      return { nullptr, false };
    }

    const auto elem_len = next_slash - path;
    strncpy(path_elem, path, elem_len);
    path_elem[elem_len] = '\0';
    return { &next_slash[1], true };
  }

} // namespace

namespace fat {

BPB* boot_volume_image;
//! 1クラスタあたりのバイト数(初期化時に算出する)
unsigned long bytes_per_cluster;

/**
 * @fn
 * fat::FileDescriptor::FileDescriptorコンストラクタ
 * @brief Construct a new File Descriptor:: File Descriptor object
 * fat::FileDescriptorのコンストラクタ
 * @param fat_entry ディレクトリエントリ
 */
FileDescriptor::FileDescriptor(DirectoryEntry& fat_entry) 
    :fat_entry_{fat_entry} {
}

/**
 * @fn
 * fat::FileDescriptor::Readメソッド
 * @brief 
 * このファイルディスクリプタを指定されたバイト数読み込む
 * @param [out] buf 読み込み先のバッファ
 * @param [in] len 読み込むデータのバイト数
 * @return size_t 
 */
size_t FileDescriptor::Read(void* buf, size_t len) {
  // 初めての読み込みのときは、まずクラスタを得る
  if (rd_cluster_ == 0) {
    rd_cluster_ = fat_entry_.FirstCluster();
  }
  uint8_t* buf8 = reinterpret_cast<uint8_t*>(buf);
  // 指定したバイト数より残りのファイル中身のバイト数が少なければそこまでを読み込むようにする。
  len = std::min(len, fat_entry_.file_size - rd_off_);

  //! 最終的によみとったバイト数
  size_t total = 0;
  while (total < len) {
    uint8_t* sec = GetSectorByCluster<uint8_t>(rd_cluster_);
    // 読み込むバイト数よりもこのクラスタ内の残りのバイト数が少なければ、そこまでを読み込む
    size_t n = std::min(len - total, bytes_per_cluster - rd_cluster_off_);
    memcpy(&buf8[total], &sec[rd_cluster_off_], n);
    total += n;

    rd_cluster_off_ += n;
    // ちょうどこのクラスタの終わりになった時
    if (rd_cluster_off_ == bytes_per_cluster) {
      rd_cluster_ = NextCluster(rd_cluster_);
      rd_cluster_off_ = 0;
    }
  }

  rd_off_ += total;
  return total;
}

/**
 * @fn
 * GetFAT関数
 * @brief 
 * FATボリューム(32ビット値の配列)の先頭ポインタを取得する
 * @return uint32_t* FATボリュームのアドレス
 */
uint32_t* GetFAT() {
  uintptr_t fat_offset =
    boot_volume_image->reserved_sector_count *
    boot_volume_image->bytes_per_sector;
  return reinterpret_cast<uint32_t*>(
      reinterpret_cast<uintptr_t>(boot_volume_image) + fat_offset);
}

/**
 * @fn
 * IsEndOfClusterchain関数
 * @brief 
 * 指定されたクラスタ番号がチェーンの末尾であるかチェックする
 * @param cluster クラスタ番号
 * @return true クラスタチェーンの末尾である
 * @return false 末尾でない
 */
bool IsEndOfClusterchain(unsigned long cluster) {
  return cluster >= 0x0ffffff8ul;
}

/**
 * @fn
 * ExtendCluster関数
 * @brief 
 * クラスターチェーンを延伸する
 * @param eoc_cluster 拡張対象のクラスタ
 * @param n 拡張するクラスタ数
 * @return unsigned long 拡張した最後のクラスタ番号
 */
unsigned long ExtendCluster(unsigned long eoc_cluster, size_t n) {
  // 引数で指定したクラスタのクラスターチェーンを辿って末尾まで移動
  uint32_t* fat = GetFAT();
  while (!IsEndOfClusterchain(fat[eoc_cluster])) {
    eoc_cluster = fat[eoc_cluster];
  }

  size_t num_allocated = 0;
  auto current = eoc_cluster;

  // FATボリューム内の空きクラスタを集める
  for (unsigned long candidate = 2; num_allocated < n; ++candidate) {
    if (fat[candidate] != 0) {  // candidate cluster is not free
      continue;
    }
    fat[current] = candidate;
    current = candidate;
    ++num_allocated;
  }
  fat[current] = kEndOfClusterchain;
  return current;
}

/**
 * @fn
 * AllocateClusterChain関数
 * @brief 
 * 指定された数の空きクラスタを確保する
 * @param n クラスタ数
 * @return unsigned long 確保した先頭クラスタ番号 
 */
unsigned long AllocateClusterChain(size_t n) {
  uint32_t* fat = GetFAT();
  unsigned long first_cluster;
  for (first_cluster = 2; ; ++first_cluster) {
    if (fat[first_cluster] == 0) {
      fat[first_cluster] = kEndOfClusterchain;
      break;
    }
  }

  if (n > 1) {
    ExtendCluster(first_cluster, n - 1);
  }
  return first_cluster;
}

/**
 * @fn
 * fat::FileDescriptor::Writeメソッド
 * @brief 
 * FATファイルシステム上のファイルへの書き込み
 * @param buf 書き込み文字列
 * @param len 書き込むバイト数
 * @return size_t 書き込んだバイト数
 */
size_t FileDescriptor::Write(const void* buf, size_t len) {
  // 指定されたバイト数書き込むのに必要なクラスタ数を算出するラムダ式
  auto num_cluster = [](size_t bytes) {
    return (bytes + bytes_per_cluster - 1) / bytes_per_cluster;
  };

  if (wr_cluster_ == 0) {
    if (fat_entry_.FirstCluster() != 0) {
      wr_cluster_ = fat_entry_.FirstCluster();
    } else {
      // 必要な数分のクラスターチェーンを確保
      wr_cluster_ = AllocateClusterChain(num_cluster(len));
      fat_entry_.first_cluster_low = wr_cluster_ & 0xffff;
      fat_entry_.first_cluster_high = (wr_cluster_ >> 16) & 0xffff;
    }
  }

  // 8バイトずつ書き込み
  const uint8_t* buf8 = reinterpret_cast<const uint8_t*>(buf);

  size_t total = 0;
  while(total < len) {
    if (wr_cluster_off_ == bytes_per_cluster) {
      const auto next_cluster = NextCluster(wr_cluster_);
      if (next_cluster == kEndOfClusterchain) {
        wr_cluster_ = ExtendCluster(wr_cluster_, num_cluster(len - total));
      } else {
        wr_cluster_ = next_cluster;
      }
      wr_cluster_off_ = 0;
    }

    uint8_t* sec = GetSectorByCluster<uint8_t>(wr_cluster_);
    size_t n = std::min(len, bytes_per_cluster - wr_cluster_off_);
    memcpy(&sec[wr_cluster_off_], &buf8[total], n);
    total += n;

    wr_cluster_off_ += n;
  }

  wr_off_ += total;
  fat_entry_.file_size = wr_off_;
  return total;
}

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

/** 
 * @fn
 * fat::ReadName関数
 * 
 * @brief 
 * ディレクトリエントリの短名を基本名と拡張子名に分割して取得する。
 * パディングされた空白文字（0x20）は取り除かれ，ヌル終端される。
 *
 * @param [in] entry  ファイル名を得る対象のディレクトリエントリ
 * @param [out] base  拡張子を除いたファイル名（9 バイト以上の配列）
 * @param [in, out] ext  拡張子（4 バイト以上の配列）
 */
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
 * FormatName関数
 * @brief 
 * ディレクトリエントリの短名を dest にコピーする。
 * 短名の拡張子が空なら "<base>" を，空でなければ "<base>.<ext>" をコピー。
 *
 * @param [in] entry  ファイル名を得る対象のディレクトリエントリ
 * @param [out] dest  基本名と拡張子を結合した文字列を格納するに十分な大きさの配列。
 */
void FormatName(const DirectoryEntry& entry, char* dest){
  char ext[5] = ".";
  ReadName(entry, dest, &ext[1]);
  if (ext[1]) {
    strcat(dest, ext);
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
 * 指定されたディレクトリからファイルを探す。
 * @param name  8+3形式のファイル名（大文字小文字は区別しない）
 * @param directory_cluster  ディレクトリの開始クラスタ（省略するとルートディレクトリから検索する）
 * @return ファイルまたはディレクトリを表すエントリと，末尾スラッシュを示すフラグの組。
 *   ファイルまたはディレクトリが見つからなければ nullptr。
 *   エントリの直後にスラッシュがあれば true。
 *   パスの途中のエントリがファイルであれば探索を諦め，そのエントリと true を返す。
 */
std::pair<DirectoryEntry*, bool>
FindFile(const char* path, unsigned long directory_cluster){
  if (path[0] == '/') {
    // /で始まる場合は、絶対パスなので、directory_clusterもrootに置き換える
    directory_cluster = boot_volume_image->root_cluster;
    ++path;
  } else if (directory_cluster == 0) {
    // ０が入った場合は、ルートディレクトリから検索
    directory_cluster = boot_volume_image->root_cluster;
  }

  //! パス区切り文字で区切った次の要素
  char path_elem[13];
  const auto [ next_path, post_slash ] = NextPathElement(path, path_elem);
  //! 取り出した要素がパスの末尾かどうか
  const bool path_last = next_path == nullptr || next_path[0] == '\0';

  while (directory_cluster != kEndOfClusterchain) {
    // クラスタ番号からブロックを取得
    auto dir = GetSectorByCluster<DirectoryEntry>(directory_cluster);
    // そのクラスタ内のディレクトリエントリごとにループ
    for (int i = 0; i < bytes_per_cluster / sizeof(DirectoryEntry); ++i) {
      if (dir[i].name[0] == 0x00) {
        goto not_found;
      } else if (!NameIsEqual(dir[i], path_elem)) {
        continue;
      }

      if (dir[i].attr == Attribute::kDirectory && !path_last) {
        // ディレクトリでかつ末尾でないときは再帰的にサブディレクトリを探す
        return FindFile(next_path, dir[i].FirstCluster());
      } else {
        // dir[i]がディレクトリではないか、パスの末尾に来てしまったので、探索をやめる
        return { &dir[i], post_slash };
      }
    }
    // 次のクラスタへ移動
    directory_cluster = NextCluster(directory_cluster);
  }

  not_found:
    return { nullptr, post_slash };
}

/**
 * @fn 
 * AllocateEntry関数
 * @brief 
 * 引数で指定されたディレクトリ内の空きエントリを探して返す
 * @param dir_cluster 検索対象のディレクトリのディレクトリエントリのクラスタ
 * @return DirectoryEntry* 空きディレクトリエントリ
 */
DirectoryEntry* AllocateEntry(unsigned long dir_cluster) {
  // 親ディレクトリのクラスタチェーンごとにループ
  while (true) {
    auto dir = GetSectorByCluster<DirectoryEntry>(dir_cluster);
    for (int i = 0; i < bytes_per_cluster / sizeof(DirectoryEntry); ++i) {
      if (dir[i].name[0] == 0 || dir[i].name[0] == 0xe5) {
        // 0または0xe5のときは空き
        return &dir[i];
      }
    }
    auto next = NextCluster(dir_cluster);
    if (next == kEndOfClusterchain) {
      break;
    }
    dir_cluster = next;
  }

  // 空きがなかったら、拡張する。
  dir_cluster = ExtendCluster(dir_cluster, 1);
  auto dir = GetSectorByCluster<DirectoryEntry>(dir_cluster);
  memset(dir, 0 , bytes_per_cluster);
  return &dir[0];
}

/**
 * @fn
 * SetFileName関数
 * @brief 
 * 指定したエントリにファイル名を設定する
 * @param entry ディレクトリエントリ
 * @param name ファイル名
 */
void SetFileName(DirectoryEntry& entry, const char* name) {
  const char* dot_pos = strrchr(name, '.');
  // 書き込む文字列を一度スペースで初期化
  memset(entry.name, ' ', 8+3);
  if (dot_pos) {
    // ドットを含む場合は、拡張子とそれ以外でそれぞれ書き込み
    for (int i = 0; i < 8 && i < dot_pos - name; ++i) {
      entry.name[i] = toupper(name[i]);
    }

    for (int i =0 ; i < 3 && i < dot_pos[i + 1]; ++i) {
      entry.name[8 + i] = toupper(dot_pos[i + 1]);
    }
  } else {
    // ドットを含まない場合は、8文字の方だけを書き込み
    for (int i = 0; i < 8 && name[i]; ++i) {
      entry.name[i] = toupper(name[i]);
    }
  }
}

/**
 * @fn
 * fat::CreateFile関数
 * @brief Create a File object
 * 新規ファイルのディレクトリエントリを作成する。
 * @param path 作成するファイルのパス文字列
 * @return WithError<DirectoryEntry*> 
 */
WithError<DirectoryEntry*> CreateFile(const char* path){
  auto parent_dir_cluster = fat::boot_volume_image->root_cluster;
  const char* filename = path;

  // ファイル名と、格納ディレクトリに分割
  if (const char* slash_pos = strrchr(path, '/')) {
    filename = &slash_pos[1];
    if (slash_pos[1] == '\0') {
      return { nullptr, MAKE_ERROR(Error::kIsDirectory)};
    }

    char parent_dir_name[slash_pos - path + 1];
    strncpy(parent_dir_name, path, slash_pos - path);
    parent_dir_name[slash_pos - path] = '\0';

    // 格納ディレクトリのディレクトリエントリを取得
    if (parent_dir_name[0] != '\0') {
      auto [ parent_dir, post_slash2 ] = fat::FindFile(parent_dir_name);
      if (parent_dir == nullptr) {
        return { nullptr, MAKE_ERROR(Error::kNoSuchEntry) };
      }
      parent_dir_cluster = parent_dir->FirstCluster();
    }
  }

  auto dir = fat::AllocateEntry(parent_dir_cluster);
  if (dir == nullptr) {
    return { nullptr, MAKE_ERROR(Error::kNoEnoughMemory) };
  }
  fat::SetFileName(*dir, filename);
  dir->file_size = 0;
  return { dir, MAKE_ERROR(Error::kSuccess) };
}

/**
 * @fn
 * LoadFile関数
 * 
 * @brief 
 * ファイル内容をバッファに読み込む
 * @param [in,out] buf 読み込んだデータをいれるバッファ
 * @param len 読み込むデータの長さ
 * @param entry 読み込むファイルのファイルエントリ
 * @return size_t 読み込んだサイズ
 */
size_t LoadFile(void* buf, size_t len, const DirectoryEntry& entry){
  auto is_valid_cluster = [](uint32_t c) {
    return c != 0 && c != fat::kEndOfClusterchain;
  };
  auto cluster = entry.FirstCluster();

  const auto buf_uint8 = reinterpret_cast<uint8_t*>(buf);
  const auto buf_end = buf_uint8 + len;
  auto p = buf_uint8;

  while (is_valid_cluster(cluster)) {
    // 残りサイズがクラスタのサイズよりも小さい場合は、必要な分だけをコピー
    if (bytes_per_cluster >= buf_end - p) {
      memcpy(p, GetSectorByCluster<uint8_t>(cluster), buf_end - p);
      return len;
    }
    // 残りサイズがそのクラスタのサイズ以上であれば、全部コピー
    memcpy(p, GetSectorByCluster<uint8_t>(cluster), bytes_per_cluster);
    p += bytes_per_cluster;
    cluster = NextCluster(cluster);
  }
  return p - buf_uint8;
}



} // namespace
