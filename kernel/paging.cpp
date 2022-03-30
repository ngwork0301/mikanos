#include "paging.hpp"

#include <array>

#include "asmfunc.h"

namespace {
  const uint64_t kPageSize4K = 4096;
  const uint64_t kPageSize2M = 512 * kPageSize4K;
  const uint64_t kPageSize1G = 512 * kPageSize2M;

  alignas(kPageSize4K) std::array<uint64_t, 512> pml4_table;
  alignas(kPageSize4K) std::array<uint64_t, 512> pdp_table;
  alignas(kPageSize4K)
    std::array<std::array<uint64_t, 512>, kPageDirectoryCount> page_directory;
}

/**
 * @fn
 * ResetCR3関数
 * @brief 
 * CR3レジスタにOS用のPML4を設定する。
 */
void ResetCR3() {
  SetCR3(reinterpret_cast<uint64_t>(&pml4_table[0]));
}

/**
 * @fn
 * SetupIdentityPageTable関数
 * 
 * @brief 
 * 仮想アドレス=物理アドレスとなるようにページテーブルを設定する．
 * 最終的に CR3 レジスタが正しく設定されたページテーブルを指すようになる．
 */
void SetupIdentityPageTable() {
  // ページマップレベル4テーブルにPDPテーブル(第3階層)の先頭アドレスをマッピング
  pml4_table[0] = reinterpret_cast<uint64_t>(&pdp_table[0]) | 0x003;
  // PDPテーブル（ページディレクトリポインタテーブル第3階層）のテーブルごとにループ
  for (int i_pdpt = 0; i_pdpt < page_directory.size(); ++i_pdpt) {
    // PDPテーブルにページディレクトリ(第2階層)の先頭アドレスをマッピング
    pdp_table[i_pdpt] = reinterpret_cast<uint64_t>(&page_directory[i_pdpt]) | 0x003;
    // ページディレクトリ(第2階層)のディレクトリ毎にループ
    for (int i_pd = 0; i_pd < 512; ++i_pd) {
      // ページディレクトリに、ページテーブル（第一階層）の先頭アドレスをマッピング
      page_directory[i_pdpt][i_pd] = i_pdpt * kPageSize1G + i_pd * kPageSize2M | 0x083;
    }
  }
  // PML4（ページマップレベル4テーブル）の物理アドレスをCR3レジスタに設定
  ResetCR3();
}

/**
 * @fn
 * InitializePaging関数
 * 
 * @brief
 * SetupIdentityPageTable関数のラッパー
 */
void InitializePaging() {
  SetupIdentityPageTable();
}
