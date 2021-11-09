/**
 * @file window.hpp
 * 
 * ウィンドウ描画処理を集めたファイル
 */
#pragma once

#include <vector>
#include <optional>
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
        virtual void Write(int x, int y, const PixelColor& c) override {
          window_.At(x, y) = c;
        }
        /** @brief Width は関連付けられた Window の横幅をピクセル単位で返す。 */
        virtual int Width() const override { return window_.Width(); }
        /** @brief Height は関連付けられた Window の高さをピクセル単位で返す */
        virtual int Height() const override { return window_.Height(); }

      private:
        Window& window_;
    };

    Window(int width, int height);
    ~Window() = default;
    Window(const Window& rhs) = delete;
    Window& operator=(const Window& rhs) = delete;

    void DrawTo(PixelWriter& writer, Vector2D<int> position);
    void SetTransparentColor(std::optional<PixelColor> c);
    WindowWriter* Writer();

    PixelColor& At(int x, int y);
    const PixelColor& At(int x, int y) const;

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
};

