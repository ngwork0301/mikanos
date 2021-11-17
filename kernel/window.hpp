/**
 * @file window.hpp
 * 
 * ウィンドウ描画処理を集めたファイル
 */
#pragma once

#include <vector>
#include <optional>
#include "frame_buffer.hpp"
#include "graphics.hpp"

/**
 * @class
 * Windowsクラス
 * 
 * @brief
 * Windowの描画処理やデータを格納するクラス
 */
class Window {
  public:
    /** @brief WindwWriter は Window と関連付けられた PixelWriter を提供する */
    class WindowWriter : public PixelWriter {
      public:
        WindowWriter(Window& window) : window_{window} {}
        /** @brief 指定された位置に指定された色を描く */
        virtual void Write(Vector2D<int> pos, const PixelColor& c) override {
          window_.Write(pos, c);
        }
        
        /** @brief Width は関連付けられた Window の横幅をピクセル単位で返す。 */
        virtual int Width() const override { return window_.Width(); }
        /** @brief Height は関連付けられた Window の高さをピクセル単位で返す */
        virtual int Height() const override { return window_.Height(); }

      private:
        Window& window_;
    };

    Window(int width, int height, PixelFormat shadow_format);
    ~Window() = default;
    Window(const Window& rhs) = delete;
    Window& operator=(const Window& rhs) = delete;

    void DrawTo(FrameBuffer& dst, Vector2D<int> position);
    void SetTransparentColor(std::optional<PixelColor> c);
    WindowWriter* Writer();

    const PixelColor& At(Vector2D<int> pos) const;

    void Write(Vector2D<int> pos, PixelColor c);
    void Move(Vector2D<int> dst_pos, const Rectangle<int>& src);

    int Width() const;
    int Height() const;

  private:
    int width_, height_;
    //! ピクセル配列
    std::vector<std::vector<PixelColor>> data_{};
    //! ピクセルへの書き込みをおこなうwriter
    WindowWriter writer_{*this};
    //! 透明色を保持する
    std::optional<PixelColor> transparent_color_{std::nullopt};
    //! 再描画の高速化のため、フレームバッファと同じものをシャドウバッファとして保持する
    FrameBuffer shadow_buffer_{};
};

