/**
 * @fn
 * strncmp関数
 * @brief 
 * 指定された文字数分比較する
 * 異なる文字数の場合は、比較できるところまでをやってその文字数を返す。
 * @param a 比較文字列１
 * @param b 比較文字列２
 * @return int 比較した文字数
 */
int strcmp(const char* a, const char* b) {
    int i = 0;
    for (; a[i] != 0 && b[i] != 0; ++i){
        if (a[i] != b[i]) {
            return a[i] - b[i];
        }
    }
    return a[i] - b[i];
}

/**
 * @fn
 * atoi関数
 * @brief 
 * 文字列をlongへ変換する
 * @param s 変換する数値文字列
 * @return long 返還後の数値
 */
long atol(const char* s) {
    long v = 0;
    for (int i = 0; s[i] != 0; ++i) {
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

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
        } else if(strcmp(argv[i], "-") == 0) {
            long b = Pop();
            long a = Pop();
            Push(a - b);
        } else {
            long a = atol(argv[i]);
            Push(a);
        }
    }
    if (stack_ptr < 0) {
        return 0;
    }
    return static_cast<int>(Pop());
}
