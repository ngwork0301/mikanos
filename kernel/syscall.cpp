#include "syscall.hpp"

#include <array>
#include <cstdint>
#include <cerrno>

#include "asmfunc.h"
#include "font.hpp"
#include "logger.hpp"
#include "msr.hpp"
#include "task.hpp"
#include "terminal.hpp"

namespace syscall {
  /**
   * @struct
   * Result構造体
   * @brief 
   * value 結果
   * error エラーコード
   */
  struct Result {
    uint64_t value;
    int error;
  };
  
#define SYSCALL(name) \
  Result name( \
    uint64_t arg1, uint64_t arg2, uint64_t arg3, \
    uint64_t arg4, uint64_t arg5, uint64_t arg6)

  /**
   * @fn
   * syscall::LogString関数
   * @brief
   * Log関数を呼び出して文字を描画する。
   * @param [in] arg1 ログレベル
   * @param [in] arg2 描画する文字列
   * @return Result{ 描画した文字数, エラーコード }
   */
  SYSCALL(LogString) {
    if (arg1 != kError && arg1 != kWarn && arg1 != kInfo && arg1 != kDebug) {
      // 出力レベルフラグ以外が引数に指定された時はエラー
      return { 0, EPERM };
    }
    const char* s = reinterpret_cast<const char*>(arg2);
    const auto len = strlen(s);
    if (len > 1024) {
      // 長さが上限を超える場合はエラー
      return { 0, E2BIG };
    }
    Log(static_cast<LogLevel>(arg1), "%s", s);
    return { len, 0 };
  }

  /**
   * @fn
   * syscall::PutString関数
   * @brief
   * ターミナルに文字を描画する。
   * @param [in] arg1 出力先ターミナルの番号
   * @param [in] arg2 表示したい文字列
   * @param [in] arg3 NUL文字をふくまないバイト数
   * @return Result{ 描画した文字数, エラーコード }
   */
  SYSCALL(PutString) {
    //! 第一引数：現状は、出力先ターミナルの番号
    const auto fd = arg1;
    //! 第二引数：表示したい文字列へのポインタ
    const char* s = reinterpret_cast<const char*>(arg2);
    //! 第三引数：NUL文字をふくまないバイト数
    const auto len = arg3;
    if (len > 1024) {
      return { 0, E2BIG };
    }

    if (fd == 1) {
      // 現在のタスク＝アプリが実行されているタスクから、出力先のターミナルを探して出力
      const auto task_id = task_manager->CurrentTask().ID();
      (*terminals)[task_id]->Print(s, len);
      return { len, 0 };
    }
    return { 0, EBADF };
  }

  /**
   * @fn
   * syscall::Exit関数
   * @brief 
   * 呼び出し側のアプリを終了する。
   * @return Result{ アプリを呼び出し前のOSのスタック領域のポインタ, エラーコード }
   */
  SYSCALL(Exit) {
    __asm__("cli"); // 割り込み禁止
    auto& task = task_manager->CurrentTask();
    __asm__("sti"); // 割り込み許可
    return { task.OSStackPointer(), static_cast<int>(arg1) };
  }

  /**
   * @fn
   * syscall::OpenWindow関数
   * @brief 
   * ウィンドウを開く
   * @param [in] arg1 描画するウィンドウの幅
   * @param [in] arg2 描画するウィンドウの高さ
   * @param [in] arg3 描画するウィンドウの左上の位置(x座標)
   * @param [in] arg4 描画するウィンドウの左上の位置(y座標)
   * @return Result{ レイヤーID, エラーコード }
   */
  SYSCALL(OpenWindow) {
    const int w = arg1, h = arg2, x = arg3, y = arg4;
    const auto title = reinterpret_cast<const char*>(arg5);
    const auto win = std::make_shared<ToplevelWindow>(
        w, h, screen_config.pixel_format, title);
    
    __asm__("cli"); // 割り込み禁止
    const auto layer_id = layer_manager->NewLayer()
      .SetWindow(win)
      .SetDraggable(true)
      .Move({x, y})
      .ID();
    active_layer->Activate(layer_id);
    __asm__("sti"); // 割り込み許可

    return { layer_id, 0 };
  }

  namespace {
    /**
     * @fn
     * DoWinFunc関数テンプレート
     * @brief 
     * ユーザアプリが作成したウィンドウへの描画の共通処理
     * @tparam Func 中身を描画する関数
     * @tparam Args システムコールの引数（可変引数テンプレート）
     * @param f 中身を描画する関数
     * @param layer_id 描画するウィンドウのレイヤーID
     * @param args 残りの第3引数以降の引数
     * @return Result { 0, エラーコード } または、fの戻り値
     */
    template <class Func, class... Args>
    Result DoWinFunc(Func f, unsigned int layer_id, Args... args) {
      __asm__("cli"); // 割り込み禁止
      auto layer = layer_manager->FindLayer(layer_id);
      __asm__("sti"); // 割り込み許可
      if (layer == nullptr) {
        return { 0, EBADF};
      }

      // 第一引数に指定された関数の呼び出し
      const auto res = f(*layer->GetWindow(), args...);
      if (res.error) {
        return res;
      }
      __asm__("cli");  // 割り込み禁止
      layer_manager->Draw(layer_id);
      __asm__("sti");  // 割り込み許可

      return res;
    }
  }

  /**
   * @fn
   * syscall::WinWriteRectangle関数
   * @brief
   * 指定されたウィンドウに文字を描く
   * @param [in] arg1 レイヤーID
   * @param [in] arg2 描画する文字の左上の位置(x座標)
   * @param [in] arg3 描画する文字の左上の位置(y座標)
   * @param [in] arg4 描画する文字列
   * @param [in] arg5 色
   * @return Result { 0, エラーコード }
   */
  SYSCALL(WinWriteString) {
    return DoWinFunc(
        [](Window& win,
           int x, int y, uint32_t color, const char* s) {
             WriteString(*win.Writer(), {x, y}, s, ToColor(color));
             return Result{ 0, 0 };
           }, arg1, arg2, arg3, arg4, reinterpret_cast<const char *>(arg5));
  }

  /**
   * @fn
   * syscall::WinWriteRectangle関数
   * @brief
   * 指定されたウィンドウに長方形を描く
   * @param [in] arg1 レイヤーID
   * @param [in] arg2 長方形の左上の描画位置(x座標)
   * @param [in] arg3 長方形の左上の描画位置(y座標)
   * @param [in] arg4 長方形の幅
   * @param [in] arg5 長方形の高さ
   * @param [in] arg6 長方形の色
   * @return Result { 0, エラーコード }
   */
  SYSCALL(WinWriteRectangle) {
    return DoWinFunc(
        [](Window& win,
           int x, int y, int w, int h, uint32_t color) {
             FillRectangle(*win.Writer(), {x, y}, {w, h}, ToColor(color));
             return Result{ 0, 0 };
           }, arg1, arg2, arg3 ,arg4, arg5 , arg6);
  }

#undef SYSCALL

}  // namespace syscall

using SyscallFuncType = syscall::Result (uint64_t, uint64_t, uint64_t,
                                 uint64_t, uint64_t, uint64_t);
extern "C" std::array<SyscallFuncType*, 6> syscall_table{
  /* 0x00 */ syscall::LogString,
  /* 0x01 */ syscall::PutString,
  /* 0x02 */ syscall::Exit,
  /* 0x03 */ syscall::OpenWindow,
  /* 0x04 */ syscall::WinWriteString,
  /* 0x05 */ syscall::WinWriteRectangle,
};

void InitializeSyscall() {
  // kIA32_EFERのビット0を1にして、syscallを使えるようにする。
  // ビット8(LME)とビット10(LMA)も64ビットモードのとき1
  WriteMSR(kIA32_EFER, 0x0501u);
  // SyscallEntry関数の先頭アドレスをkIA32_LSTARレジスタに書き込み
  WriteMSR(kIA32_LSTAR, reinterpret_cast<uint64_t>(SyscallEntry));
  // syscall/sysretオペランドが動作する時のCS、SSセグメントレジスタにいれる値を設定
  // syscall側のRPLは0、sysret側のRPLが3になるようにしておく。
  // MikanOSでは、コードセグメントがgdt[1](なので1<<3=8)、スタックセグメントがgdt[2](2<<3=16)
  // IA32_STARTに設定するオフセットに合わせてそれぞれ32ビット、48ビット左にシフトする
  WriteMSR(kIA32_STAR, static_cast<uint64_t>(8) << 32 |
                       static_cast<uint64_t>(16 | 3) << 48);
  // R/Wのフラグマスクらしい。今回は0にしておく。 
  WriteMSR(kIA32_FMASK, 0);
}
