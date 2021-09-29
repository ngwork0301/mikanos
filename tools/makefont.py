#!/usr/bin/python3

import argparse
import functools
import re

BITMAP_PATTERN = re.compile(r'([.*@]+)')


def compile(src: str) -> bytes:
    src = src.lstrip()
    result = []

    # 1行ずつ読み込み
    for line in src.splitlines():
        # . か @ の文字を探す
        m = BITMAP_PATTERN.match(line)
        if not m:
            continue

        # . だったら、0 それ以外は 1 としてビットを並べる
        bits = [(0 if x == '.' else 1) for x in m.group(1)]
        # ビット配列をint配列に置き換え
        bits_int = functools.reduce(lambda a, b: 2*a + b, bits)
        # リトルエンディアンでバイトデータに変換
        result.append(bits_int.to_bytes(1, byteorder='little'))

    return b''.join(result)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('font', help='path to a font file')
    parser.add_argument('-o', help='path to an output file', default='font.out')
    ns = parser.parse_args()

    with open(ns.o, 'wb') as out, open(ns.font) as font:
        src = font.read()
        out.write(compile(src))


if __name__ == '__main__':
    main()
