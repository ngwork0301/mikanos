#include <cstdio>
#include <cstdlib>

extern "C" void main(int argc, char** argv) {
  // 引数がなければ、ルート直下のmemmapファイルを開く
  const char* path = "/memmap";
  if (argc >= 2) {
    // 引数があれば、引数に指定されたファイルを開く
    path = argv[1];
  }

  FILE* fp = fopen(path, "r");
  if (fp == nullptr) {
    printf("failed to open: %s\n", path);
    exit(1);
  }
  
  //! 1行分のバッファ
  char line[256];
  for (int i = 0; i < 3; ++i) {
    if (fgets(line, sizeof(line), fp) == nullptr) {
      printf("failed to get a line\n");
      exit(1);
    }
    printf("%03d : %s", i, line);
  }
  printf("----\n");
  exit(0);
}
