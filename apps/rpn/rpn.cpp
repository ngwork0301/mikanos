#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include "../../kernel/logger.hpp"

// OSの関数ポインタを取得
// auto& printk = *reinterpret_cast<int (*)(const char*, ...)>(0x000000000010b030);
// auto& fill_rect = *reinterpret_cast<decltype(FillRectangle)*>(0x000000000010c110);
// auto& scrn_writer = *reinterpret_cast<decltype(screen_writer)*>(0x000000000024e078);

//! スタックへの先頭ポインタ
int stack_ptr;
//! 逆ポーランド記法のスタック
int stack[100];

/**
 * @fn
 * Pop関数
 * @brief 
 * スタックから要素を１つ取り出す
 * @return long 取り出した数値
 */
long Pop() {
    long value = stack[stack_ptr];
    --stack_ptr;
    return value;
}

/**
 * @fn
 * Push関数
 * @brief 
 * スタックに要素を１つ格納する
 * @param value 格納する要素
 */
void Push(long value) {
    ++stack_ptr;
    stack[stack_ptr] = value;
}

extern "C" void SyscallExit(int exit_code);
// extern "C" void SyscallLogString(const int log_level, const char* s);

/**
 * @fn
 * main関数
 * @brief 
 * 逆ポーランド記法で演算をおこない、その数値を返す
 */
extern "C" void main(int argc, char** argv) {
    stack_ptr = -1;

    // 引数の数だけループ
    for(int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "+") == 0) {
            long b = Pop();
            long a = Pop();
            Push(a + b);
            // printk("[%d] <- %ld\n", stack_ptr, a + b);
            // SyscallLogString(kWarn, "+");
        } else if(strcmp(argv[i], "-") == 0) {
            long b = Pop();
            long a = Pop();
            Push(a - b);
            // printk("[%d] <- %ld\n", stack_ptr, a - b);
            // SyscallLogString(kWarn, "-");
        } else {
            long a = atol(argv[i]);
            Push(a);
            // printk("[%d] <- %ld\n", stack_ptr, a);
            // SyscallLogString(kWarn, "#");
        }
    }

    // 適当な緑色の四角を描いてみる
    // fill_rect(*scrn_writer, Vector2D<int>{100, 10}, Vector2D<int>{200, 200}, ToColor(0x00ff00));

    if (stack_ptr < 0) {
        // エラーなので、-1もおかしいがひとまず
        SyscallExit(static_cast<int>(-1));
    }
    // SyscallLogString(kWarn, "\nhello, this is rpn\n");

    // 試しにlong型の変数をprintfする
    long result = 0;
    if (stack_ptr >= 0) {
        result = Pop();
    }
    printf("%ld\n", result);
    SyscallExit(static_cast<int>(result));
}
