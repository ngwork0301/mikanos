#include "syscall.hpp"

#include <array>
#include <cstdint>
#include <cerrno>
#include <cmath>

#include "app_event.hpp"
#include "asmfunc.h"
#include "font.hpp"
#include "keyboard.hpp"
#include "logger.hpp"
#include "msr.hpp"
#include "task.hpp"
#include "terminal.hpp"
#include "timer.hpp"

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

    // 開いたウィンドウをlayer_task_mapに追加
    const auto task_id = task_manager->CurrentTask().ID();
    layer_task_map->insert(std::make_pair(layer_id, task_id));
    __asm__("sti"); // 割り込み許可

    return { layer_id, 0 };
  }

  /**
   * @fn
   * syscall::GetCurrentTick関数
   * @brief 
   * 現在時刻を取得する
   */
  SYSCALL(GetCurrentTick) {
    return { timer_manager->CurrentTick(), kTimerFreq };
  }

  /**
   * @fn
   * syscall::CloseWindow関数
   * @brief
   * 引数で指定したレイヤーを削除する
   * @param [in] arg1 描画するウィンドウのレイヤーID + フラグ(再描画有無)
   */
  SYSCALL(CloseWindow) {
    // 下位32ビットからlayer_idを取り出し。
    const unsigned int layer_id = arg1 & 0xffffffff;
    const auto layer = layer_manager->FindLayer(layer_id);

    // 指定されたレイヤーがない場合はエラー
    if (layer == nullptr) {
      return { EBADF, 0 };
    }

    // 再描画のため、削除するレイヤーの位置を調べる
    const auto layer_pos = layer->GetPosition();
    const auto win_size = layer->GetWindow()->Size();

    __asm__("cli");   // 割り込み禁止
    // 再描画のため、最下位のレイヤーをActivateする
    active_layer->Activate(0);
    layer_manager->RemoveLayer(layer_id);
    // 消えた領域を再描画する。
    layer_manager->Draw({layer_pos, win_size});
    // 消したウィンドウをlayer_task_mapから削除
    layer_task_map->erase(layer_id);
    __asm__("sti");   // 割り込み許可

    return { 0, 0 };
  }

  /**
   * @fn
   * syscall::ReadEvent関数
   * @brief 
   * アプリへのキー入力をターミナルタスクから受け取る
   * @param [out] arg1 AppEvent。受け取ったイベントを入れる構造体
   * @param [in] arg2 受け取るイベントのサイズ
   * @return Result{ 受け取ったイベント数, エラーコード }
   */
  SYSCALL(ReadEvent) {
    if (arg1 < 0x8000'0000'0000'0000) {
      // 第一引数のAppEventのメモリ領域がユーザ用の後半アドレスになっていない場合は、エラー
      return { 0, EFAULT };
    }
    const auto app_events = reinterpret_cast<AppEvent*>(arg1);
    const size_t len = arg2;

    __asm__("cli"); // 割込み禁止
    auto& task = task_manager->CurrentTask();
    __asm__("sti"); // 割り込み許可
    size_t i = 0;

    while(i < len) {
      __asm__("cli"); // 割り込み禁止
      auto msg = task.ReceiveMessage();
      if (!msg && i == 0) {
        // メッセージが空かつまだイベントを一つも受け取っていない場合は、継続して待つ
        task.Sleep();
        continue;
      }
      __asm__("sti"); // 割り込み許可

      if (!msg) {
        // メッセージが1つ以上あって、2つめ以降が空なら、そこまでのイベントを送る
        break;
      }
      switch (msg->type) {
        case Message::kKeyPush:
          if (msg->arg.keyboard.keycode == 20 /* Q Key */ &&
              msg->arg.keyboard.modifier && (kLControlBitMask | kRControlBitMask)) {
            // 受け取ったイベントをAppEventに変換
            app_events[i].type = AppEvent::kQuit;
            ++i;
          }
          break;
        case Message::kMouseMove:
          app_events[i].type = AppEvent::kMouseMove;
          app_events[i].arg.mouse_move.x = msg->arg.mouse_move.x;
          app_events[i].arg.mouse_move.y = msg->arg.mouse_move.y;
          app_events[i].arg.mouse_move.dx = msg->arg.mouse_move.dx;
          app_events[i].arg.mouse_move.dy = msg->arg.mouse_move.dy;
          app_events[i].arg.mouse_move.buttons = msg->arg.mouse_move.buttons;
          ++i;
          break;
        default:
          Log(kInfo, "uncaught event type: %u\n", msg->type);
      }
    }

    return { i, 0};
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
     * @param layer_id_flags 描画するウィンドウのレイヤーID + フラグ(再描画有無)
     * @param args 残りの第3引数以降の引数
     * @return Result { 0, エラーコード } または、fの戻り値
     */
    template <class Func, class... Args>
    Result DoWinFunc(Func f, uint64_t layer_id_flags, Args... args) {
      const uint32_t layer_flags = layer_id_flags >> 32;
      const unsigned int layer_id = layer_id_flags & 0xffffffff;

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

      // 再描画しないオプション(最下位ビット0)がfalseの場合は、再描画する。
      if ((layer_flags & 1) == 0) {
        __asm__("cli");  // 割り込み禁止
        layer_manager->Draw(layer_id);
        __asm__("sti");  // 割り込み許可
      }

      return res;
    }
  }

  /**
   * @fn
   * syscall::WinWriteRectangle関数
   * @brief
   * 指定されたウィンドウに文字を描く
   * @param [in] arg1 ウィンドウインスタンス
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
   * @param [in] arg1 ウィンドウインスタンス
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

  /**
   * @fn
   * syscall::WinRedraw関数
   * @brief 
   * ウィンドウを再描画する。
   * @param [in] arg1 ウィンドウインスタンス
   */
  SYSCALL(WinRedraw) {
    return DoWinFunc(
      [](Window&) {
        return Result{ 0, 0 };
      }, arg1);
  }

  /**
   * @fn
   * syscall::WinDrawLine関数
   * @brief
   * 線を描画する
   * @param [in] arg1 ウィンドウインスタンス
   * @param [in] arg1 開始点のx座標
   * @param [in] arg2 開始点のy座標
   * @param [in] arg3 終了点のx座標
   * @param [in] arg4 終了点のy座標
   * @param [in] arg5 色
   * @return Result { 0, エラーコード }
   */
  SYSCALL(WinDrawLine) {
    return DoWinFunc(
      [](Window& win,
         int x0, int y0, int x1, int y1, uint32_t color) {
      // 傾きが0より大きいなら1、0より小さいなら-1を判定するラムダ式
      auto sign = [](int x) {
        return (x > 0) ? 1 : (x < 0) ? -1 : 0;
      };
      const int dx = x1 - x0 + sign(x1 - x0);
      const int dy = y1 - y0 + sign(y1 - y0);

      // 差が0の場合は、点のみ描画。分母が0にならないように、処理しておく。
      if (dx == 0 && dy == 0) {
        win.Writer()->Write({x0, y0}, ToColor(color));
        return Result{ 0, 0 };
      }

      const auto floord = static_cast<double(*)(double)>(floor);
      const auto ceild = static_cast<double(*)(double)>(ceil);
      // 水平距離のほうが垂直距離より長い場合
      if (abs(dx) >= abs(dy)) {
        // マイナス方向の場合は、入れ替え
        if (dx < 0) {
          std::swap(x0, x1);
          std::swap(y0, y1);
        }
        //! y1のほうが大きければ繰り上げ or y0のほうが大きければ切り捨て
        const auto roundish = y1 >= y0 ? floord : ceild;
        //! 傾き
        const double m = static_cast<double>(dy) / dx;
        for (int x = x0; x <= x1; ++x) {
          const int y = roundish(m * (x - x0) + y0);
          win.Writer()->Write({x, y}, ToColor(color));
        }
      } else {
        // 垂直距離のほうが、水平距離より長い場合
        if (dy < 0) {
          // 舞なる方向音場合は、入れ替え
          std::swap(x0, x1);
          std::swap(y0, y1);
        }
        //! x1のほうが大きければ繰り上げ or x0のほうが大きければ切り捨て
        const auto roundish = x1 < x0 ? floord : ceild;
        //! 傾き
        const double m = static_cast<double>(dx) / dy;
        for (int y = y0; y <= y1; ++y) {
          const int x = roundish(m * (y - y0) + x0);
          win.Writer()->Write({x, y}, ToColor(color));
        }
      }
      return Result{ 0, 0 };
    }, arg1, arg2, arg3, arg4, arg5, arg6);
  }

#undef SYSCALL

}  // namespace syscall

using SyscallFuncType = syscall::Result (uint64_t, uint64_t, uint64_t,
                                 uint64_t, uint64_t, uint64_t);
extern "C" std::array<SyscallFuncType*, 11> syscall_table{
  /* 0x00 */ syscall::LogString,
  /* 0x01 */ syscall::PutString,
  /* 0x02 */ syscall::Exit,
  /* 0x03 */ syscall::OpenWindow,
  /* 0x04 */ syscall::WinWriteString,
  /* 0x05 */ syscall::WinWriteRectangle,
  /* 0x06 */ syscall::GetCurrentTick,
  /* 0x07 */ syscall::WinRedraw,
  /* 0x08 */ syscall::WinDrawLine,
  /* 0x09 */ syscall::CloseWindow,
  /* 0x0a */ syscall::ReadEvent,
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
