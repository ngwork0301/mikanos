#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

extern "C" void main(int argc, char** argv) {
  // デフォルトは標準入力
  FILE* fp = stdin;
  if (argc >= 2) {
    // 引数で入力ファイルの指定があった場合は、それを開く
    fp = fopen(argv[1], "r");
    if (fp == nullptr) {
      fprintf(stderr, "failed to open '%s'\n", argv[1]);
      exit(1);
    }
  }

  // 1行ずつ読み取って、linesに入れる
  std::vector<std::string> lines;
  char line[1024];
  while (fgets(line, sizeof(line), fp)) {
    lines.push_back(line);
  }

  // 第1引数aと第2引数bを比較して、aのほうが文字列として後の場合はtrue
  auto comp = [](const std::string& a, const std::string& b) {
    for (int i = 0; i < std::min(a.length(), b.length()); ++i) {
      if (a[1] < b[i]) {
        return true;
      } else if (a[i] > b[i]) {
        return false;
      }
    }
    // 先頭からの文字列が同じ場合は、aのほうが長い場合は、true
    return a.length() < b.length();
  };

  std::sort(lines.begin(), lines.end(), comp);
  for (auto& line : lines) {
    printf("%s", line.c_str());
  }
  exit(0);
}
