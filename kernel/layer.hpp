/**
 * @file
 * layer.hpp
 * 
 * @brief
 * 重ね合わせ処理に関する実装を集めたファイル
 */
#pragma once

#include <memory>
#include <map>
#include <vector>

#include "frame_buffer.hpp"
#include "graphics.hpp"
#include "window.hpp"

/**
 * @class
 * Layerクラス
 * 
 * @brief
 * 重ね合わせを実現するための層を表す
 */
class Layer {
  public:
    Layer(unsigned int id = 0);
    unsigned int ID() const;

    Layer& SetWindow(const std::shared_ptr<Window>& window);
    std::shared_ptr<Window> GetWindow() const;

    Layer& Move(Vector2D<int> pos);
    Layer& MoveRelative(Vector2D<int> pos_diff);

    Vector2D<int> GetPosition() const;
    void DrawTo(FrameBuffer& screen, const Rectangle<int>& area) const;

    Layer& SetDraggable(bool draggable);
    bool IsDraggable() const;

  private:
    //! 識別ID
    unsigned int id_;
    //! 原点座標
    Vector2D<int> pos_{};
    //! ウィンドウへのスマートポインタ
    std::shared_ptr<Window> window_{};
    //! 移動可能なウィンドウか
    bool draggable_{false};
};

/**
 * @class
 * LayaerManagerクラス
 * 
 * @brief
 * 複数Layerの重なりを管理する
 */
class LayerManager {
  public:
    void SetWriter(FrameBuffer* screen);
    Layer& NewLayer();

    void Draw(const Rectangle<int>& area) const;
    void Draw(unsigned int id) const;

    void Move(unsigned int id, Vector2D<int> new_position);
    void MoveRelative(unsigned int id, Vector2D<int> pos_diff);

    void UpDown(unsigned int id, int new_height);
    void Hide(unsigned int id);
    Layer* FindLayerByPosition(Vector2D<int> pos, unsigned int exclude_id) const;

  private:
    //! フレームバッファ
    FrameBuffer* screen_{nullptr};
    //! 先にバックバッファに描画処理して、一気にフレームバッファに転送
    mutable FrameBuffer back_buffer_{};
    //! 生成したレイヤーを生成した順にいれるためのベクトル
    std::vector<std::unique_ptr<Layer>> layers_{};
    //! レイヤーの重なりをいれるベクトル（先頭が最背面、末尾を最前面）
    //! 非表示レイヤーはこのスタックにいれないこと
    std::vector<Layer*> layer_stack_{};
    unsigned int latest_id_{0};

    Layer* FindLayer(unsigned int id);
};

extern LayerManager* layer_manager;
