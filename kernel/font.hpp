#pragma once

#include <cstdint>
#include "graphics.hpp"

void WriteAscii(PixelWriter& writer, Vector2D<int> pos, char c, const PixelColor& color);
void WriteUnicode(PixelWriter& writer, Vector2D<int> pos, 
    char32_t c, const PixelColor& color);
const uint8_t* GetFont(char c);
int CountUTF8Size(uint8_t c);
std::pair<char32_t, int> ConvertUTF8To32(const char* u8);
void WriteString(PixelWriter& writer, Vector2D<int> pos, const char* s, const PixelColor& color);
bool IsHankaku(char32_t c);
