#pragma once

#include <cstdint>
#include "graphics.hpp"

void WriteAscii(PixelWriter& writer, int x, int y, char c, const PixelColor& color);
const uint8_t* GetFont(char c);
void WriteString(PixelWriter& writer, int x, int y, const char* s, const PixelColor& color);
