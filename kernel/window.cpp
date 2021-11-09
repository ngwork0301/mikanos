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
Window::Window(int width, int height) : width_{width}, height_{height} {
  data_.resize(height);
  for (int y = 0; y < height; ++y) {
    data_[y].resize(width);
  }
}

/**
 * @fn
 * Window::DrawToメソッド
 * 
 * @brief
 * 与えられた PixelWriter にこのウィンドウの表示領域を描画させる。
 * 
 * @param [in] writer 描画先
 * @param [in, out] position writer の左上を基準とした描画位置
 */
void Window::DrawTo(PixelWriter& writer, Vector2D<int> position) {
  // 透過色の設定がない場合は、そのまま長方形を埋める
  if (!transparent_color_) {
    for (int y = 0; y < Height(); ++y) {
      for (int x = 0; x < Width(); ++x) {
        writer.Write(position.x + x, position.y + y, At(x, y));
      }
    }
    return;
  }

  // 透過色がある場合は、その部分は上塗りしないで描画
  const auto tc = transparent_color_.value();
  for (int y = 0; y < Height(); ++y){
    for (int x = 0; x < Width(); ++x) {
      const auto c = At(x, y);
      if (c != tc) {
        writer.Write(position.x + x, position.y + y, c);
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
 * @param [in] x 取得したいピクセルのX座標
 * @param [in] y 取得したいピクセルのY座標
 * @return PixelColor
 */
PixelColor& Window::At(int x, int y) {
  return data_[y][x];
}

/**
 * @fn
 * Window::Atメソッド
 * 
 * @brief
 * 指定した位置のピクセルを返す
 * @param [in] x 取得したいピクセルのX座標
 * @param [in] y 取得したいピクセルのY座標
 * @return PixelColor
 */
const PixelColor& Window::At(int x, int y) const{
  return data_[y][x];
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