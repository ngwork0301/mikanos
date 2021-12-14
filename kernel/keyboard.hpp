/**
 * @file keyboard.hpp
 * @brief 
 * USBキーボードに関する処理を集めたファイル
 */
#pragma once

#include <deque>
#include "message.hpp"

void InitializeKeyboard(std::deque<Message>& msg_queue);
