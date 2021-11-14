#pragma once

#include <cstdint>
#include "graphics.hpp"

void WriteAscii(PixelWriter& writer, Vector2D<int> pos, char c, const PixelColor& color);
const uint8_t* GetFont(char c);
void WriteString(PixelWriter& writer, Vector2D<int> pos, const char* s, const PixelColor& color);
