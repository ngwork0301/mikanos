#include  <Uefi.h>
#include  <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include  <Library/PrintLib.h>
#include  <Protocol/LoadedImage.h>
#include  <Protocol/SimpleFileSystem.h>
#include  <Protocol/DiskIo2.h>
#include  <Protocol/BlockIo.h>

/**
 * @struct
 * MemoryMap構造体
 */
struct MemoryMap {
  UINTN buffer_size;
  VOID* buffer;
  UINTN map_size;
  UINTN map_key;
  UINTN descriptor_size;
  UINT32 descriptor_version;
};

/**
 * @fn
 * GetMemoryMap関数
 * 
 * @brief
 * EDK IIのブートサービスからメモリマップを取得して
 * 引数で受け取ったバッファに書き込む
 * 
 * @param [in] map 書き込むバッファの先頭ポインタ
 * @return: 成功したらEFI_SUCCESSを返す(ブートサービスのGetMemoryMapの結果をそのまま返却)
 */
EFI_STATUS GetMemoryMap(struct MemoryMap* map) {
  if (map->buffer == NULL) {
    return EFI_BUFFER_TOO_SMALL;
  }

  map->map_size = map->buffer_size;
  // gBSは、ブートサービス用のグローバル変数
  // GetMemoryMapの仕様は、以下を参照
  // https://uefi.org/specifications
  return gBS->GetMemoryMap(
    &map->map_size,
    (EFI_MEMORY_DESCRIPTOR*)map->buffer,
    &map->map_key,
    &map->descriptor_size,
    &map->descriptor_version);
}

/**
 * @fn
 * GetMemoryTypeUnicode関数
 * 
 * @brief
 * メモリマップのTypeをUnicode文字列にして取得する
 * @param [in] type メモリマップのType (ex. EfiReservedMemoryType)
 * @return CHAR16* メモリマップのTypeの文字列 (ex. (EFI_MEMORY_TYPE)EfiReservedMemoryType -> "EfiReservedMemoryType")
 */
const CHAR16* GetMemoryTypeUnicode(EFI_MEMORY_TYPE type) {
  switch (type) {
    case EfiReservedMemoryType: return L"EfiReservedMemoryType";
    case EfiLoaderCode: return L"EfiLoaderCode";
    case EfiLoaderData: return L"EfiLoaderData";
    case EfiBootServicesCode: return L"EfiBootServicesCode";
    case EfiBootServicesData: return L"EfiBootServicesData";
    case EfiRuntimeServicesCode: return L"EfiRuntimeServicesCode";
    case EfiRuntimeServicesData: return L"EfiRuntimeServicesData";
    case EfiConventionalMemory: return L"EfiConventionalMemory";
    case EfiUnusableMemory: return L"EfiUnusableMemory";
    case EfiACPIReclaimMemory: return L"EfiACPIReclaimMemory";
    case EfiACPIMemoryNVS: return L"EfiACPIMemoryNVS";
    case EfiMemoryMappedIO: return L"EfiMemoryMappedIO";
    case EfiMemoryMappedIOPortSpace: return L"EfiMemoryMappedIOPortSpace";
    case EfiPalCode: return L"EfiPalCode";
    case EfiPersistentMemory: return L"EfiPersistentMemory";
    case EfiMaxMemoryType: return L"EfiMaxMemoryType";
    default: return L"InvalidMemoryType";
  }
}

/**
 * @fn
 * SaveMemoryMap関数
 * 
 * @brief
 * 第一引数で受けとったメモリマップを、第二引数で受けとったファイルに書き込む
 * 
 * @param [in] map メモリマップの先頭ポインタ
 * @param [in,out] file 書き込むファイルのファイルディスクリプタ
 * @return 成功したらEFI_SUCCESSを返す
 */
EFI_STATUS SaveMemoryMap(struct MemoryMap* map, EFI_FILE_PROTOCOL* file) {
  //! 1行の書き込み用のバッファ
  CHAR8 buf[256];
  //! 1行の長さを格納するテンポラリ変数
  UINTN len;

  // ヘッダを出力
  CHAR8* header =
    "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n";
  len = AsciiStrLen(header);
  file->Write(file, &len, header);

  Print(L"map->buffer = %08lx, map->map_size = %08lx\n",
    map->buffer, map->map_size);
  
  //! Physicalアドレスのイテレータ
  EFI_PHYSICAL_ADDRESS iter;
  //! EFI_MEMORY_DESCRIPTORのindex(=行番号と一致)
  int i;
  // PhysicalアドレスをEFI_MEMORY_DESCRIPTOR分足しながら書き込む
  for (iter = (EFI_PHYSICAL_ADDRESS)map->buffer, i = 0;
       iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size;
       iter += map->descriptor_size, i++) {
    // Physicalアドレスの場所をEFI_MEMORY_DESCRIPTORでキャスト
    EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)iter;
    // bufに1行分のデータを書き込み
    len = AsciiSPrint(
      buf, sizeof(buf),
      "%u, %x, %-ls, %08lx, %lx, %lx\n",
      i, desc->Type, GetMemoryTypeUnicode(desc->Type),
      desc->PhysicalStart, desc->NumberOfPages,
      desc->Attribute & 0xffffflu);
    // TODO:本来はWriteの戻り値で書き込んだサイズが返ってくるので、書ききれなかった分を再度書く必要がある
    file->Write(file, &len, buf);
  }

  return EFI_SUCCESS;
}

/**
 * @fn
 * OpenRootDir関数
 * 
 * @brief
 * EDK IIのブートサービスをつかって、
 * 第4引数に、ルートディレクトリのEFI_SIMPLE_FILE_SYSTEM_PROTOCOLポインタを設定する
 * 
 * @param [in] image_handle UefiMain関数の引数で受けとったEFI_HANDLE
 * @param [out] root 設定するルートディレクトリのポインタ
 * @return 成功したらEFI_SUCCESSを返す
 */
EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root) {
  EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;

  // Loaded Imageを取得（Loader自身のイメージを取得？？）
  gBS->OpenProtocol(
      image_handle,
      &gEfiLoadedImageProtocolGuid,
      (VOID**)&loaded_image,
      image_handle,
      NULL,
      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

  // Loader自身のイメージから、ファイルシステムのポインタとして取得？？
  gBS->OpenProtocol(
      loaded_image->DeviceHandle,
      &gEfiSimpleFileSystemProtocolGuid,
      (VOID**)&fs,
      image_handle,
      NULL,
      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

  // ルートディレクトリの取り出し
  fs->OpenVolume(fs, root);

  return EFI_SUCCESS;
}

/**
 * @fn
 * UefiMain関数
 * 
 * @brief
 * メイン関数
 * 
 * @param [in] image_handle
 * @param [in] system_table
 * @return 成功したらEFI_SUCCESSを返す
 */
EFI_STATUS EFIAPI UefiMain(
    EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE *system_table) {
  Print(L"Hello, Mikan World!\n");

  // MemoryMap構造体のメモリを確保して、メモリマップを取得して、反映
  CHAR8 memmap_buf[4096 * 4];
  struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
  GetMemoryMap(&memmap);

  // rootディレクトリ(/)のハンドラを取得
  EFI_FILE_PROTOCOL* root_dir;
  OpenRootDir(image_handle, &root_dir);

  // 出力ファイルのファイルディスクリプタを宣言
  EFI_FILE_PROTOCOL* memmap_file;

  // 出力ファイルをオープン
  root_dir->Open(
    root_dir, &memmap_file, L"\\memmap",
    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);

  // オープンしたファイルに書き出して、クローズする
  SaveMemoryMap(&memmap, memmap_file);
  memmap_file->Close(memmap_file);

  // 画面出力処理  
  Print(L"All done\n");
  while (1);
  return EFI_SUCCESS;
}
