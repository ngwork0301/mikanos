#include "error.hpp"
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
  for (int y = 0; y < Height(); ++y){
    for (int x = 0; x < Width(); ++x) {
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