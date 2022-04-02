#include <cstdio>
#include <cstdlib>
#include <cstring>

void PrintUsage() {
  printf("Usage fault [command]\n");
  printf("\n");
  printf("    hlt\n");
  printf("    wr_kernel\n");
  printf("    wr_app\n");
  printf("    zero\n");
}

extern "C" void main(int argc, char** argv) {
  const char* cmd = "help";
  if (argc >= 2) {
    cmd = argv[1];
  }

  if (strcmp(cmd, "hlt") == 0) {
    __asm__("hlt");
  } else if (strcmp(cmd, "wr_kernel") == 0) {
    int* p = reinterpret_cast<int*>(0x100);
    *p = 42;
  } else if (strcmp(cmd, "wr_app") == 0) {
    int* p = reinterpret_cast<int*>(0xffff8000ffff0000);
    *p = 123;
  } else if (strcmp(cmd, "zero") == 0) {
    volatile int z = 0;
    printf("100\%d = %d\n", z, 100/z);
  } else if (strcmp(cmd, "help") == 0) {
    PrintUsage();
    exit(1);
  } else {
    printf("Unknown command = %s\n", cmd);
    PrintUsage();
    exit(1);
  }

  exit(0);
}
