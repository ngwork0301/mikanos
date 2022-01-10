/**
 * @file window.hpp
 * 
 * ウィンドウ描画処理を集めたファイル
 */
#pragma once

#include <vector>
#include <optional>
#include <string>
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
    virtual ~Window() = default;
    Window(const Window& rhs) = delete;
    Window& operator=(const Window& rhs) = delete;

    void DrawTo(FrameBuffer& dst, Vector2D<int> pos, const Rectangle<int>& area);
    void SetTransparentColor(std::optional<PixelColor> c);
    WindowWriter* Writer();

    const PixelColor& At(Vector2D<int> pos) const;

    void Write(Vector2D<int> pos, PixelColor c);
    void Move(Vector2D<int> dst_pos, const Rectangle<int>& src);

    int Width() const;
    int Height() const;
    Vector2D<int> Size() const;

    virtual void Activate() {};
    virtual void Deactivate() {};

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

/**
 * @class 
 * ToplevelWindowクラス
 * 
 * @brief 
 * タイトルバーとマージン付きのウィンドウ。活性／非活性で色を変える
 */
class ToplevelWindow : public Window {
  public:
    //! {上, 左} の余白幅の定義
    // static constexpr Vector2D<int> kTopLeftMargin{4, 24};
    static constexpr Vector2D<int> kTopLeftMargin{4, 24};
    //! {下, 右} の余白幅の定義
    static constexpr Vector2D<int> kBottomRightMargin{4, 4};
    //! 左右の余白の合計幅
    static constexpr int kMarginX = kTopLeftMargin.x + kBottomRightMargin.x;
    //! 上下の余白の合計幅
    static constexpr int kMarginY = kTopLeftMargin.y + kBottomRightMargin.y;

    class InnerAreaWriter : public PixelWriter {
      public:
        InnerAreaWriter(ToplevelWindow& window) : window_{window} {}
        virtual void Write(Vector2D<int> pos, const PixelColor& c) override {
          window_.Write(pos + kTopLeftMargin, c);
        }
        /** @brief 内側の描画領域 の横幅をピクセル単位で返す。 */
        virtual int Width() const override {
          return window_.Width() - kTopLeftMargin.x - kBottomRightMargin.x;}
        /** @brief 内側の描画領域 の高さをピクセル単位で返す。 */
        virtual int Height() const override {
          return window_.Height() - kTopLeftMargin.y - kBottomRightMargin.y;}
      
      private:
        ToplevelWindow& window_;
    };

    ToplevelWindow(int width, int height, PixelFormat shadow_format,
                   const std::string& title);
    
    virtual void Activate() override;
    virtual void Deactivate() override;

    InnerAreaWriter* InnerWriter() { return &inner_writer_; }
    Vector2D<int> InnerSize() const;

  private:
    std::string title_;
    InnerAreaWriter inner_writer_{*this};
};
void DrawWindowTitle(PixelWriter& writer, const char* title, bool activate);

void DrawWindow(PixelWriter& writer, const char* title);
void DrawTextbox(PixelWriter& writer,  Vector2D<int> pos, Vector2D<int> size);
void DrawTerminal(PixelWriter& writer,  Vector2D<int> pos, Vector2D<int> size);
