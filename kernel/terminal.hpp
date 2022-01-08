/**
 * @file terminal.hpp
 * @brief 
 * ターミナル関連の実装を集めたファイル
 */
#pragma once

#include <deque>
#include <map>
#include "window.hpp"
#include "task.hpp"
#include "layer.hpp"

class Terminal {
  public:
    static const int kRows = 15, kColumns = 60;

    Terminal();
    unsigned int LayerID() const { return layer_id_; }
    Rectangle<int> BlinkCursor();

  private:
    //! このターミナルウィンドウのインスタンス
    std::shared_ptr<ToplevelWindow> window_;
    //! ターミナルウィンドウのレイヤーID
    unsigned int layer_id_;

    //! カーソル座標
    Vector2D<int> cursor_{0, 0};
    //! カーソル描画の表示/不表示フラグ
    bool cursor_visible_{false};
    void DrawCursor(bool visible);
};

void TaskTerminal(uint64_t task_id, int64_t data);
