#include "error.hpp"
#include "font.hpp"
#include "logger.hpp"
#include "window.hpp"

/**
 * @fn
 * Windowコンストラクタ
 * 
 * @brief
 * 指定されたピクセル数の平面描画領域を作成する
 * 
 * @param [in] width 幅
 * @param [in] height 高さ
 */
Window::Window(int width, int height, PixelFormat shadow_format) : width_{width}, height_{height} {
  data_.resize(height);
  for (int y = 0; y < height; ++y) {
    data_[y].resize(width);
  }

  FrameBufferConfig config{};
  config.frame_buffer = nullptr;
  config.horizontal_resolution = width;
  config.vertical_resolution = height;
  config.pixel_format = shadow_format;

  if (auto err = shadow_buffer_.Initialize(config)) {
    Log(kError, "failed to initialize shadow buffer: %s at %s:%d\n",
        err.Name(), err.File(), err.Line());
  }
}

/**
 * @fn
 * Window::DrawToメソッド
 * 
 * @brief
 * 与えられた PixelWriter にこのウィンドウの表示領域を描画させる。
 * 
 * @param [in] dst  描画先
 * @param [in, out] position writer の左上を基準とした描画位置
 */
void Window::DrawTo(FrameBuffer& dst, Vector2D<int> position) {
  // 透過色の設定がない場合は、そのまま長方形を埋める
  if (!transparent_color_) {
    dst.Copy(position, shadow_buffer_);
    return;
  }

  // 透過色がある場合は、その部分は上塗りしないで描画
  const auto tc = transparent_color_.value();
  auto& writer = dst.Writer();
  // 移動先の位置がPixelWriterの最大長をこえる場合は、そこまでで打ち切る
  for (int y = std::max(0, 0 - position.y);
       y < std::min(Height(), writer.Height() - position.y);
       ++y) {
    for (int x = std::max(0, 0 - position.x);
         x < std::min(Width(), writer.Width() - position.x);
         ++x) {
      const auto c = At(Vector2D<int>{x, y});
      if (c != tc) {
        writer.Write(position + Vector2D<int>{x, y}, c);
      }
    }
  }
}

/**
 * @fn
 * Window::SetTramsparentColorメソッド
 * 
 * @brief
 * 透過色を設定する
 * 
 * @param [in] c 透過色。nullを含むPixelColorとし、nullを渡せば透過色なし
 */
void Window::SetTransparentColor(std::optional<PixelColor> c) {
  transparent_color_ = c;
}

/**
 * @fn
 * Window::Writerメソッド
 * 
 * @brief
 * このインスタンスに紐付いた WindowWriter を取得するgetterメソッド
 * 
 * @return WindowWriter
 */
Window::WindowWriter* Window::Writer(){
  return &writer_;
}

/**
 * @fn
 * Window::Atメソッド
 * 
 * @brief
 * 指定した位置のピクセルを返す
 * @param [in] pos x,y座標のVector2D<int>
 * @return PixelColor
 */
const PixelColor& Window::At(Vector2D<int> pos) const{
  return data_[pos.y][pos.x];
}

/**
 * @fn
 * Window::Writeメソッド
 * 
 * @brief
 * 指定したピクセスを、指定した色に描画する
 * 
 * @param [in] pos Vector2D。描画したいピクセルの座標
 * @param [in] c 描画するPixelColor
 */
void Window::Write(Vector2D<int> pos, PixelColor c){
  data_[pos.y][pos.x] = c;
  shadow_buffer_.Writer().Write(pos, c);
}

/**
 * @fn
 * Window::Moveメソッド
 * 
 * @brief
 * ウィンドウ内の表示の移動
 * @param [in] pos 移動先の左上の座標
 * @param [in] src 移動する範囲領域。Rectangleインスタンス
 */
void Window::Move(Vector2D<int> dst_pos, const Rectangle<int>& src) {
  // シャドウバッファに処理を委譲
  shadow_buffer_.Move(dst_pos, src);
}

/**
 * @fn
 * Window::Widthメソッド
 * 
 * @brief
 * 平面描画領域の横幅をピクセル単位で返す
 * 
 * @return 横幅
 */
int Window::Width() const {
  return width_;
}

/**
 * @fn
 * Window::Heightメソッド
 * 
 * @brief
 * 平面描画領域の高さをピクセル単位で返す
 * @return 高さ
 */
int Window::Height() const {
  return height_;
}

namespace {
  const int kCloseButtonWidth = 16;
  const int kCloseButtonHeight = 14;
  const char close_button[kCloseButtonHeight][kCloseButtonWidth + 1] = {
    "...............@",
    ".:::::::::::::$@",
    ".:::::::::::::$@",
    ".:::@@::::@@::$@",
    ".::::@@::@@:::$@",
    ".:::::@@@@::::$@",
    ".::::::@@:::::$@",
    ".:::::@@@@::::$@",
    ".::::@@::@@:::$@",
    ".:::@@::::@@::$@",
    ".:::::::::::::$@",
    ".:::::::::::::$@",
    ".$$$$$$$$$$$$$$@",
    ".@@@@@@@@@@@@@@@",
  };

  constexpr PixelColor ToColor(uint32_t c) {
    return {
      static_cast<uint8_t>((c >> 16) & 0xff),
      static_cast<uint8_t>((c >> 8) & 0xff),
      static_cast<uint8_t>(c & 0xff)
    };
  }
}


/**
 * @fn
 * DrawWindow関数
 * 
 * @brief
 * ウィンドウのタイトルバーと背景を描画する
 * @param [in] writer PixelWriterインスタンス
 * @param [in] title タイトルバーに表示する文字列
 */
void DrawWindow(PixelWriter& writer, const char* title) {
  auto fill_rect = [&writer](Vector2D<int> pos, Vector2D<int> size, uint32_t c) {
    FillRectangle(writer, pos, size, ToColor(c));
  };
  const auto win_w = writer.Width();
  const auto win_h = writer.Height();

  fill_rect({0, 0},         {win_w, 1},             0xc6c6c6);  // 上枠 外側灰色
  fill_rect({1, 1},         {win_w - 2, 1},         0xffffff);  // 上枠 内側白
  fill_rect({0, 0},         {1, win_h},             0xc6c6c6);  // 左枠 外側灰色
  fill_rect({1, 1},         {1, win_h - 2},         0xffffff);  // 左枠 内側白
  fill_rect({win_w - 2, 1}, {1, win_h - 2},         0x848484);  // 右枠 内側灰色
  fill_rect({win_w - 1, 0}, {1, win_h},             0x000000);  // 右枠 外側黒
  fill_rect({2, 2},         {win_w - 4, win_h - 4}, 0xc6c6c6);  // 内枠 灰色
  fill_rect({3, 3},         {win_w - 6, 18},        0x000084);  // タイトルバー 青色
  fill_rect({1, win_h - 2}, {win_w - 2, 1},         0x848484);  // 下枠 内側灰色
  fill_rect({0, win_h - 1}, {win_w, 1},             0x000000);  // 下枠 外側黒

  WriteString(writer, {24, 4}, title, ToColor(0xffffff));

  // クローズボタンの描画
  for (int y = 0; y < kCloseButtonHeight; ++y) {
    for (int x = 0; x < kCloseButtonWidth; ++x) {
      PixelColor c = ToColor(0xffffff);
      if (close_button[y][x] == '@') {
        c = ToColor(0x000000);
      } else if (close_button[y][x] == '$') {
        c = ToColor(0x848484);
      } else if (close_button[y][x] == ':') {
        c = ToColor(0xc6c6c6);
      }
      writer.Write({win_w - 5 - kCloseButtonWidth + x, 5 + y}, c);
    }
  }
}
