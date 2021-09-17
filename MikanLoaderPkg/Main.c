#include  <Uefi.h>
#include  <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include  <Library/PrintLib.h>
#include  <Library/MemoryAllocationLib.h>
#include  <Protocol/LoadedImage.h>
#include  <Protocol/SimpleFileSystem.h>
#include  <Protocol/DiskIo2.h>
#include  <Protocol/BlockIo.h>
#include  <Guid/FileInfo.h>

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
  CHAR8 buf[256];  //!< 1行の書き込み用のバッファ
  UINTN len;  //!< 1行の長さを格納するテンポラリ変数

  // ヘッダを出力
  CHAR8* header =
    "Index, Type, Type(name), PhysicalStart, NumberOfPages, Attribute\n";
  len = AsciiStrLen(header);
  file->Write(file, &len, header);

  Print(L"map->buffer = %08lx, map->map_size = %08lx\n",
    map->buffer, map->map_size);
  
  EFI_PHYSICAL_ADDRESS iter;  //! Physicalアドレスのイテレータ
  int i;  //! EFI_MEMORY_DESCRIPTORのindex(=行番号と一致)
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
 * OpenGOP関数
 * 
 * @brief
 * ブートサービスをつかってGOPを開き、初期化する。
 * 
 * @param [in] image_handle UefiMain関数の引数で受けとったEFI_HANDLE
 * @param [out] gop オープンしたGOP(EFI_GRAPHICS_OUTPUT_PROTOCOL)
 * @return EFI_STATUS 成功したらEFI_SUCCESSを返す
 */
EFI_STATUS OpenGOP(EFI_HANDLE image_handle,
                   EFI_GRAPHICS_OUTPUT_PROTOCOL** gop) {
  UINTN num_gop_handles = 0;
  EFI_HANDLE* gop_handles = NULL;
  gBS->LocateHandleBuffer(
      ByProtocol,
      &gEfiGraphicsOutputProtocolGuid,
      NULL,
      &num_gop_handles,
      &gop_handles);

  gBS->OpenProtocol(
      gop_handles[0],
      &gEfiGraphicsOutputProtocolGuid,
      (VOID**)gop,
      image_handle,
      NULL,
      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

  FreePool(gop_handles);

  return EFI_SUCCESS;
}

/**
 * @fn
 * GetPixelFormatUnicode関数
 * 
 * @brief
 * 各ピクセルフォーマットに対応した名前を文字列で取得する。
 * 
 * @param [in] fmt ピクセルフォーマット
 * @return ピクセルフォーマットの文字列
 */
const CHAR16* GetPixelFormatUnicode(EFI_GRAPHICS_PIXEL_FORMAT fmt) {
  switch (fmt) {
    case PixelRedGreenBlueReserved8BitPerColor:
      return L"PixelRedGreenBlueReserved8BitPerColor";
    case PixelBlueGreenRedReserved8BitPerColor:
      return L"PixelBlueGreenRedReserved8BitPerColor";
    case PixelBitMask:
      return L"PixelBitMask";
    case PixelBltOnly:
      return L"PixelBltOnly";
    case PixelFormatMax:
      return L"PixelFormatMax";
    default:
      return L"InvalidPixelFormat";
  }
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

  /* /////// MemoryMapの読み出しとファイルへの書き込み処理 //////////////////// */
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

  /* /////////// GOPを取得して画面を描画 ///////////////////////////////////// */
  EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
  OpenGOP(image_handle, &gop);
  Print(L"Resolution: %ux%u, Pixel Format: %s, %u pixels/line \n",
      gop->Mode->Info->HorizontalResolution,
      gop->Mode->Info->VerticalResolution,
      GetPixelFormatUnicode(gop->Mode->Info->PixelFormat),
      gop->Mode->Info->PixelsPerScanLine);
  Print(L"Frame Buffer: 0x%0lx - 0x%0lx, Size: %lu bytes\n",
      gop->Mode->FrameBufferBase,
      gop->Mode->FrameBufferBase + gop->Mode->FrameBufferSize,
      gop->Mode->FrameBufferSize);
  
  UINT8* frame_buffer = (UINT8*)gop->Mode->FrameBufferBase;
  // 全データのビットに1をたてて白で塗りつぶす
  for (UINTN i = 0; i < gop->Mode->FrameBufferSize; ++i) {
    frame_buffer[i] = 255;
  }

  /* //////////// カーネルファイルの読み出しと起動 ///////////////////////////// */
  // カーネルファイルをオープン
  EFI_FILE_PROTOCOL* kernel_file;
  root_dir->Open(
    root_dir, &kernel_file, L"\\kernel.elf",
    EFI_FILE_MODE_READ, 0);
  Print(L"Opened kernel.elf\n");

  // カーネルファイルのヘッダ部分の読み込み
  UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;
  UINT8 file_info_buffer[file_info_size];
  kernel_file->GetInfo(
    kernel_file, &gEfiFileInfoGuid,
    &file_info_size, file_info_buffer);
  
  EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)file_info_buffer;
  UINTN kernel_file_size = file_info->FileSize;

  // カーネルをページ単位のメモリにロードする
  EFI_PHYSICAL_ADDRESS kernel_base_addr = 0x100000;
  gBS->AllocatePages(
    AllocateAddress, EfiLoaderData,
    (kernel_file_size + 0xfff) / 0x1000, &kernel_base_addr);
  kernel_file->Read(kernel_file, &kernel_file_size, (VOID*)kernel_base_addr);
  Print(L"Kernel: 0x%0lx (%lu bytes)\n", kernel_base_addr, kernel_file_size);

  // カーネルを起動する前にブートサービスを停止する
  EFI_STATUS status;
  status = gBS->ExitBootServices(image_handle, memmap.map_key);
  if (EFI_ERROR(status)) {
    status = GetMemoryMap(&memmap);
    if (EFI_ERROR(status)) {
      Print(L"failed to get memory map: %r\n", status);
      while(1);
    }
    status = gBS->ExitBootServices(image_handle, memmap.map_key);
    if (EFI_ERROR(status)) {
      Print(L"Could not exit boot service: %r\n", status);
      while(1);
    }
  }

  // カーネルを起動
  UINT64 entry_addr = *(UINT64*)(kernel_base_addr + 24);
  typedef void EntryPointType(void);
  EntryPointType* entry_point = (EntryPointType*)entry_addr;
  entry_point();
  /* ////////////////////////////////////////////////////////////////////// */

  // 画面出力処理  
  Print(L"All done\n");
  while (1);
  return EFI_SUCCESS;
}
