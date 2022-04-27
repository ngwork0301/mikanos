#include  <Uefi.h>
#include <stdalign.h>
#include  <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include  <Library/PrintLib.h>
#include  <Library/MemoryAllocationLib.h>
#include  <Library/BaseMemoryLib.h>
#include  <Protocol/LoadedImage.h>
#include  <Protocol/SimpleFileSystem.h>
#include  <Protocol/DiskIo2.h>
#include  <Protocol/BlockIo.h>
#include  <Guid/FileInfo.h>

#include "elf.hpp"
#include "frame_buffer_config.hpp"
#include "memory_map.hpp"

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
 * Halt関数
 * 
 * @brief
 * 無限ループして止める。エラー処理などでつかう
 * 
 **/
void Halt(void) {
  while(1) __asm__("hlt");
}

/**
 * @fn
 * CalcLoadAddressRange関数
 * 
 * @brief
 * ロードアドレスの範囲を計算する
 * 
 * @param [in] ehdr ファイルヘッダのアドレス
 * @param [out] first ロードする場所の最初のアドレス
 * @param [out] last ロードする場所の最後のアドレス
 */
void CalcLoadAddressRange(Elf64_Ehdr* ehdr, UINT64* first, UINT64* last) {
  Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
  *first = MAX_UINT64;  //! 最大値で初期化
  *last = 0;            //! 最小値で初期化
  // プログラムヘッダの要素ごとにループ
  for (Elf64_Half i = 0; i < ehdr->e_phnum; ++i) {
    // プログラムヘッダ要素のtypeがPT_LOADであるセグメントを探査
    if (phdr[i].p_type != PT_LOAD) continue;
    *first = MIN(*first, phdr[i].p_vaddr);  // 仮想アドレスの一番小さいものを探査
    *last = MAX(*last, phdr[i].p_vaddr + phdr[i].p_memsz);  // 仮想アドレス+メモリサイズ
  }
}

/**
 * @fn
 * CopyLoadSegments関数
 * 
 * @brief
 * LOADセグメントをコピーする
 * 
 * @param [in, out] ehdr ファイルヘッダのアドレス
 */
void CopyLoadSegments(Elf64_Ehdr* ehdr) {
  Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
  // プログラムヘッダの要素ごとにループ
  for (Elf64_Half i = 0; i < ehdr->e_phnum; ++i) {
    if (phdr[i].p_type != PT_LOAD) continue;

    // セグメントをまるごとコピー
    UINT64 segm_in_file = (UINT64)ehdr + phdr[i].p_offset;
    CopyMem((VOID*)phdr[i].p_vaddr, (VOID*)segm_in_file, phdr[i].p_filesz);

    // 残りを0で埋める
    UINTN remain_bytes = phdr[i].p_memsz - phdr[i].p_filesz;
    SetMem((VOID*)(phdr[i].p_vaddr + phdr[i].p_filesz), remain_bytes, 0);
  }
}

/**
 * @fn
 * ReadFile関数
 * 
 * @brief 
 * 指定したファイルを読み出して、bufferに指定したメモリ領域に展開する
 * @param [in] file EFI_FILE_PROTOCOL
 * @param [out] buffer 展開するメモリ領域
 * @return EFI_STATUS 成功したらEFI_SUCCESSを返す
 */
EFI_STATUS ReadFile(EFI_FILE_PROTOCOL* file, VOID** buffer) {
  EFI_STATUS status;

  // ヘッダ部分の読み込み
  UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;
  UINT8 file_info_buffer[file_info_size];
  status = file->GetInfo(
    file, &gEfiFileInfoGuid,
    &file_info_size, file_info_buffer);
  if (EFI_ERROR(status)) {
    return status;
  }
  
  EFI_FILE_INFO* file_info = (EFI_FILE_INFO*)file_info_buffer;
  UINTN file_size = file_info->FileSize;

  // 一時領域のメモリ確保
  status = gBS->AllocatePool(EfiLoaderData, file_size, buffer);
  if (EFI_ERROR(status)) {
    return status;
  }
  return file->Read(file, &file_size, *buffer);
}

/**
 * @fn
 * OpenBlockIoProtocolForLoadedImage関数
 * 
 * @brief 
 * Block I/O Protocolを開く
 * @param [in] image_handle ボリュームイメージのハンドル
 * @param [out] block_io 読みだしたBlock I/O Protocolのハンドルを入れる
 * @return EFI_STATUS 成功したらEFI_SUCCESSを返す
 */
EFI_STATUS OpenBlockIoProtocolForLoadedImage(
        EFI_HANDLE image_handle, EFI_BLOCK_IO_PROTOCOL** block_io) {
  EFI_STATUS status;
  EFI_LOADED_IMAGE_PROTOCOL* loaded_image;

  // ブートローダからLoadedImageProtocolを開く
  status = gBS->OpenProtocol(
      image_handle,
      &gEfiLoadedImageProtocolGuid,
      (VOID**)&loaded_image,
      image_handle,
      NULL,
      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
  if (EFI_ERROR(status)) {
    return status;
  }

  // 取得したloaded_image の DeiceHandleを呼んでイメージが格納されている記憶装置のハンドルから
  // Block I/O Protocolを取得
  status = gBS->OpenProtocol(
    loaded_image->DeviceHandle,
    &gEfiBlockIoProtocolGuid,
    (VOID**)block_io,
    image_handle, // agent handle
    NULL,
    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

  return status;
}

/**
 * @fn
 * ReadBlocks関数
 * 
 * @brief 
 * ブロックデバイスからデータを読み込む
 * @param [in] block_io Block I/O Protocolのハンドル
 * @param [in] media_id メディアID
 * @param [in] read_bytes 読み込むサイズ
 * @param [out] buffer 読み込んだ内容を展開するメモリ領域
 * @return EFI_STATUS 
 */
EFI_STATUS ReadBlocks(
      EFI_BLOCK_IO_PROTOCOL* block_io, UINT32 media_id,
      UINTN read_bytes, VOID** buffer) {
  EFI_STATUS status;

  // メモリ領域の確保
  status = gBS->AllocatePool(EfiLoaderData, read_bytes, buffer);
  if (EFI_ERROR(status)) {
    return status;
  }

  status = block_io->ReadBlocks(
    block_io,
    media_id,
    0,    // start LBA
    read_bytes,
    *buffer);

  return status;
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
  EFI_STATUS status;

  Print(L"Hello, Mikan World!\n");

  /* /////// MemoryMapの読み出しとファイルへの書き込み処理 //////////////////// */
  // MemoryMap構造体のメモリを確保して、メモリマップを取得して、反映
  CHAR8 memmap_buf[4096 * 4];
  struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
  status = GetMemoryMap(&memmap);
  if (EFI_ERROR(status)) {
    Print(L"failed to get memory map: %r\n", status);
    Halt();
  }

  // rootディレクトリ(/)のハンドラを取得
  EFI_FILE_PROTOCOL* root_dir;
  status = OpenRootDir(image_handle, &root_dir);
  if (EFI_ERROR(status)) {
    Print(L"failed to open root directory: %r\n", status);
    Halt();
  }

  // 出力ファイルのファイルディスクリプタを宣言
  EFI_FILE_PROTOCOL* memmap_file;

  // 出力ファイルをオープン
  status = root_dir->Open(
    root_dir, &memmap_file, L"\\memmap",
    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  if (EFI_ERROR(status)) {
    Print(L"failed to open file '\\memmap': %r\n", status);
    Print(L"Ignored.\n");
  } else {
    // オープンしたファイルに書き出して、クローズする
    status = SaveMemoryMap(&memmap, memmap_file);
    if (EFI_ERROR(status)) {
      Print(L"failed to save memory map: %r\n", status);
      Halt();
    }
    status = memmap_file->Close(memmap_file);
    if (EFI_ERROR(status)) {
      Print(L"failed to close memory map: %r\n", status);
      Halt();
    }
  }

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
  status = root_dir->Open(
    root_dir, &kernel_file, L"\\kernel.elf",
    EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(status)) {
    Print(L"failed to open file '\\kernel.elf': %r\n", status);
    Halt();
  }
  Print(L"Opened kernel.elf\n");

  // 一時領域のメモリ確保
  VOID* kernel_buffer;
  status = ReadFile(kernel_file, &kernel_buffer);
  if (EFI_ERROR(status)) {
    Print(L"error: %r\n", status);
    Halt();
  }

  // カーネルの場所をしらべて、必要なメモリのサイズを算出
  Elf64_Ehdr* kernel_ehdr = (Elf64_Ehdr*)kernel_buffer;
  UINT64 kernel_first_addr, kernel_last_addr;
  CalcLoadAddressRange(kernel_ehdr, &kernel_first_addr, &kernel_last_addr);

  // メモリをページ単位で確保
  UINTN num_pages = (kernel_last_addr - kernel_first_addr + 0xfff) / 0x1000;
  status = gBS->AllocatePages(AllocateAddress, EfiLoaderData,
                              num_pages, &kernel_first_addr);
  if (EFI_ERROR(status)) {
    Print(L"failed to allocate pages: %r\n", status);
    Halt();
  }

  // LOADセグメントを確保したメモリにコピー
  CopyLoadSegments(kernel_ehdr);
  Print(L"Kernel: 0x%0lx - 0x%0lx\n", kernel_first_addr, kernel_last_addr);

  // 一時領域を解放する
  status = gBS->FreePool(kernel_buffer);
  if (EFI_ERROR(status)) {
    Print(L"failed to free pool: %r\n", status);
    Halt();
  }

  /* //////////// ボリュームイメージのファイルシステムを読み出し ///////////////////////////// */
  VOID* volume_image;

  EFI_FILE_PROTOCOL* volume_file;
  status = root_dir->Open(
    root_dir, &volume_file, L"\\fat_disk",
    EFI_FILE_MODE_READ, 0);
  if (status == EFI_SUCCESS) {
    // 読みだしたファイルの中身をvolume_imageに展開する
    status = ReadFile(volume_file, &volume_image);
    if (EFI_ERROR(status)) {
      Print(L"failed to read volume file: %r", status);
      Halt();
    }
  } else {
    // Block I/Oを取得
    EFI_BLOCK_IO_PROTOCOL* block_io;
    status = OpenBlockIoProtocolForLoadedImage(image_handle, &block_io);
    if (EFI_ERROR(status)) {
      Print(L"failed to open Block I/O Protocol: %r\n", status);
      Halt();
    }

    // Block I/O Protocolで先頭16MiBを読み出す
    EFI_BLOCK_IO_MEDIA* media = block_io->Media;
    UINTN volume_bytes = (UINTN)media->BlockSize * (media->LastBlock + 1);
    if (volume_bytes > 32 * 1024 * 1024) {
      // 読み出すサイズは最大32MiBとする。それ以上にするとロードに時間がかかる。
      volume_bytes = 32 * 1024 * 1024;
    }

    // 読みだしたブロックの情報を出力
    Print(L"Reading %lu bytes (Present %d, BlockSize %u, LastBlock %u)\n",
        volume_bytes, media->MediaPresent, media->BlockSize, media->LastBlock);
    
    // ブロックからルートディレクトリのディレクトリエントリを探す
    status = ReadBlocks(block_io, media->MediaId, volume_bytes, &volume_image);
    if (EFI_ERROR(status)) {
      Print(L"failed to read blocks: %r\n", status);
      Halt();
    }
  }




  /* //////////////////////////////////////////////////////////////////////////// */


  // カーネルを起動する前にブートサービスを停止する
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
  UINT64 entry_addr = *(UINT64*)(kernel_first_addr + 24);

  /* /////// PixelFormat情報を取得してカーネルに渡す //////////////////////////// */
  struct FrameBufferConfig config = {
    (UINT8*)gop->Mode->FrameBufferBase,
    gop->Mode->Info->PixelsPerScanLine,
    gop->Mode->Info->HorizontalResolution,
    gop->Mode->Info->VerticalResolution,
    0
  };
  switch (gop->Mode->Info->PixelFormat) {
    case PixelRedGreenBlueReserved8BitPerColor:
      config.pixel_format = kPixelRGBResv8BitPerColor;
      break;
    case PixelBlueGreenRedReserved8BitPerColor:
      config.pixel_format = kPixelBGRResv8BitPerColor;
      break;
    default:
      Print(L"Unimplemented pixel format: %d\n", gop->Mode->Info->PixelFormat);
      Halt();
  }
  /* /////// RSDP構造体へのポインタを取得してカーネルに渡す //////////////////////////// */
  VOID* acpi_table = NULL;
  for (UINTN i = 0; i < system_table->NumberOfTableEntries; ++i) {
    // system_tableのエントリの中から、VendorGuildがEfiAcpiTableGuidと一致するものを探す
    if (CompareGuid(&gEfiAcpiTableGuid,
                    &system_table->ConfigurationTable[i].VendorGuid)) {
      acpi_table = system_table->ConfigurationTable[i].VendorTable;
      break;
    }
  }

  typedef void EntryPointType(const struct FrameBufferConfig*,
                              const struct MemoryMap*,
                              const VOID*,
                              VOID*);
  EntryPointType* entry_point = (EntryPointType*)entry_addr;
  entry_point(&config, &memmap, acpi_table, volume_image);
  /* ////////////////////////////////////////////////////////////////////// */

  // 画面出力処理  
  Print(L"All done\n");
  while (1);
  return EFI_SUCCESS;
}
