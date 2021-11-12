/**
 * @file
 * timer.hpp
 * 
 * @brief
 * タイマー関連の処理を集めたファイル
 */
#pragma once

#include <cstdint>

void InitializeLAPICTimer();
void StartLAPICTimer();
uint32_t LAPICTimerElapsed();
void StopLAPICTimer();
