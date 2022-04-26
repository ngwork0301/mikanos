/**
 * @file font.cpp
 *
 * フォント描画のプログラムを集めたファイル.
 */

#include "font.hpp"

extern const uint8_t _binary_hankaku_bin_start;
extern const uint8_t _binary_hankaku_bin_end;
extern const uint8_t _binary_hankaku_bin_size;

/**
 * @fn
 * GetFont関数
 * 
 * @brief
 * 引数で指定した文字のhankaku.oから読み取ったフォントデータを取得する
 * 
 * @param [in] c フォントを取得したい文字
 */
const uint8_t* GetFont(char c) {
  auto index = 16 * static_cast<unsigned int>(c);
  if (index >= reinterpret_cast<uintptr_t>(&_binary_hankaku_bin_size)) {
    return nullptr;
  }
  return &_binary_hankaku_bin_start + index;
}

/**
 * @fn
 * WriteAscii関数
 * 
 * @brief
 * フォントを描画する
 * 
 * @param [in] writer
 * @param [in, out] x
 * @param [in, out] y
 * @param [in] char
 * @param [in] color
 */
void WriteAscii(PixelWriter& writer, Vector2D<int> pos, char c, const PixelColor& color) {
  // フォントデータを取得
  const uint8_t* font = GetFont(c);
  if (font == nullptr) {
    return;
  }
  // タテ 16bit 分ループ
  for (int dy = 0; dy < 16; ++dy) {
    // ヨコ 8bit 分ループ
    for (int dx = 0; dx < 8; ++dx) {
      // dx で左シフトしたあと、0x80(=0b1000000)との論理和をとって描画するか判定
      if ((font[dy] << dx) & 0x80u) {
        writer.Write(pos + Vector2D<int>{dx, dy}, color);
      }
    }
  }
}

/**
 * @fn
 * WriteUnicode関数
 * @brief 
 * 与えられたコードポイントに対応する文字を描画する
 * @param writer PixelWriterインスタンス
 * @param pos 描画位置
 * @param c 描画文字列
 * @param color 色
 */
void WriteUnicode(PixelWriter& writer, Vector2D<int> pos, 
                  char32_t c, const PixelColor& color) {
  if (c < 0x7f) {
    // ASCIIコードはそのままWriteAsciiに渡す
    WriteAscii(writer, pos, c, color);
    return;
  }

  WriteAscii(writer, pos, '?', color);
  WriteAscii(writer, pos + Vector2D<int>{8, 0}, '?', color);
}

/**
 * @fn
 * CountUTF8Size関数
 * @brief 
 * UTF-8の文字のバイト数を求める
 * @param c 文字の1バイト目
 * @return int バイト数
 */
int CountUTF8Size(uint8_t c) {
  if (c < 0x80) {
    // 先頭ビットが0のときは、1バイト
    return 1;
  } else if (0xc0 <= c && c < 0xe0) {
    // 先頭ビットが110のときは、2バイト
    return 2;
  } else if (0xe0 <= c && c < 0xf0) {
    // 先頭ビットが1110のときは、3バイト
    return 3;
  } else if (0xf0 <= c && c < 0xf8) {
    // 先頭ビットが11110のときは、4バイト
    return 4;
  }
  return 0;
}

/**
 * @fn
 * ConvertUTF8To32関数
 * @brief 
 * UTF-8文字列から1文字を取り出す
 * @param u8 UTF-8文字列
 * @return std::pair<char32_t, int>  UTF-32 の1文字, バイト数
 */
std::pair<char32_t, int> ConvertUTF8To32(const char* u8) {
  switch (CountUTF8Size(u8[0])) {
    case 1:
      return {
        static_cast<char32_t>(u8[0]),
        1
      };
    case 2:
      return {
        (static_cast<char32_t>(u8[0]) & 0b0001'1111) << 6 |
        (static_cast<char32_t>(u8[1]) & 0b0011'1111) << 0,
        2
      };
    case 3:
      return {
        (static_cast<char32_t>(u8[0]) & 0b0000'1111) << 12 |
        (static_cast<char32_t>(u8[1]) & 0b0011'1111) << 6 |
        (static_cast<char32_t>(u8[2]) & 0b0011'1111) << 0,
        3
      };
    case 4:
      return {
        (static_cast<char32_t>(u8[0]) & 0b0000'0111) << 18 |
        (static_cast<char32_t>(u8[1]) & 0b0011'1111) << 12 |
        (static_cast<char32_t>(u8[2]) & 0b0011'1111) << 6 |
        (static_cast<char32_t>(u8[3]) & 0b0011'1111) << 0,
        4
      };
    default:
      return { 0, 0 };
  }
}

/**
 * @fn
 * WriteString関数
 * 
 * @brief
 * 指定した場所に、指定した文字列を、指定した色で描画する
 * 
 * @param [in] writer PixelWriter
 * @param [in] x 文字列を描画する左端のX座標
 * @param [in] y 文字列を描画する上端のY座標
 * @param [in] c 描画する文字列の先頭ポインタ
 * @param [in] color 色
 */
void WriteString(PixelWriter& writer, Vector2D<int> pos, const char* s, const PixelColor& color) {
  int x = 0;
  while (*s) {
    const auto [ u32, bytes ] = ConvertUTF8To32(s);
    WriteUnicode(writer, pos + Vector2D<int>{8 * x, 0}, u32, color);
    s += bytes;
    x += IsHankaku(u32) ? 1 : 2;
  }
}

/**
 * @fn
 * IsHankaku
 * @brief 
 * 指定された文字が半角文字であれば真を返す
 * @param c 判定する文字
 * @return true 半角
 * @return false 半角でない
 */
bool IsHankaku(char32_t c) {
  return c <= 0x7f;
}
