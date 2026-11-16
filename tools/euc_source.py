#!/usr/bin/env python3
"""Copies a game C source with its string and character literals encoded as EUC-JP.

The decomp's build converts every source from UTF-8 to EUC-JP before IDO compiles it (pokestadium0
tools/production.py), so a literal such as "タイプ" is 6 bytes there. clang only compiles UTF-8, which made those
literals 9 bytes on the host: copies into N64-sized buffers overran them, and the text lookup's md5s missed.
Non-ASCII characters inside literals become 3-digit octal escapes, so line numbers stay the same and comments are
left alone. A #line directive keeps the original path for diagnostics and debug info.

usage: euc_source.py <in.c> <out.c>
"""
import re
import sys

LITERAL = re.compile(r'''//[^\n]*|/\*.*?\*/|"(?:[^"\\\n]|\\.)*"|'(?:[^'\\\n]|\\.)*\'''', re.S)


def encode_literal(m):
    text = m.group(0)
    if text.startswith("/"):
        return text
    out = []
    for ch in text:
        if ord(ch) < 0x80:
            out.append(ch)
        else:
            out.extend("\\%03o" % b for b in ch.encode("euc-jp"))
    return "".join(out)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    with open(src, encoding="utf-8") as f:
        text = f.read()
    body = LITERAL.sub(encode_literal, text)
    with open(dst, "w", encoding="utf-8", newline="\n") as f:
        f.write('#line 1 "%s"\n' % src.replace("\\", "/"))
        f.write(body)


if __name__ == "__main__":
    main()
