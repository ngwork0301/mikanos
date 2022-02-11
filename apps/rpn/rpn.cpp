#include <cstring>
#include <cstdlib>
#include "../../kernel/graphics.hpp"

// OSの関数ポインタを取得
auto& printk = *reinterpret_cast<int (*)(const char*, ...)>(0x000000000010b030);
auto& fill_rect = *reinterpret_cast<decltype(FillRectangle)*>(0x000000000010c110);
auto& scrn_writer = *reinterpret_cast<decltype(screen_writer)*>(0x000000000024e078);

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

/**
 * @fn
 * main関数
 * @brief 
 * 逆ポーランド記法で演算をおこない、その数値を返す
 */
extern "C" int main(int argc, char** argv) {
    stack_ptr = -1;

    // 引数の数だけループ
    for(int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "+") == 0) {
            long b = Pop();
            long a = Pop();
            Push(a + b);
            printk("[%d] <- %ld\n", stack_ptr, a + b);
        } else if(strcmp(argv[i], "-") == 0) {
            long b = Pop();
            long a = Pop();
            Push(a - b);
            printk("[%d] <- %ld\n", stack_ptr, a - b);
        } else {
            long a = atol(argv[i]);
            Push(a);
            printk("[%d] <- %ld\n", stack_ptr, a);
        }
    }

    // 適当な緑色の四角を描いてみる
    fill_rect(*scrn_writer, Vector2D<int>{100, 10}, Vector2D<int>{200, 200}, ToColor(0x00ff00));

    if (stack_ptr < 0) {
        return 0;
    }
    return static_cast<int>(Pop());
}
