#!/usr/bin/env python3
"""vnc_key.py - inject keystrokes into the QEMU VNC console (keyboard only).

抓帧走 HMP screendump；这里只发 RFB KeyEvent，不读 framebuffer，绕开
ServerInit bpp=0 的老坑。用法:

    python3 vnc_key.py "type c:\\rospinyin-trace.log"     # 输入后回车
    python3 vnc_key.py --raw "ctrl+c"                     # 组合键

keysym 覆盖 ASCII + 常见控制键；大写字母由 QEMU 处理 shift。
"""
import socket
import struct
import sys
import time

HOST, PORT = "127.0.0.1", 5901

SPECIAL = {
    "enter": 0xFF0D, "ret": 0xFF0D, "return": 0xFF0D,
    "esc": 0xFF1B, "escape": 0xFF1B,
    "back": 0xFF08, "backspace": 0xFF08, "bs": 0xFF08,
    "tab": 0xFF09, "space": 0x20, "spc": 0x20,
    "ctrl": 0xFFE3, "shift": 0xFFE1, "alt": 0xFFE9,
    "up": 0xFF52, "down": 0xFF54, "left": 0xFF51, "right": 0xFF53,
}

CHARMAP = {
    "\\": 0x5C, ":": 0x3A, ".": 0x2E, "-": 0x2D, "/": 0x2F,
    "*": 0x2A, "?": 0x3F, "\"": 0x22, "'": 0x27, "<": 0x3C,
    ">": 0x3E, "|": 0x7C, ";": 0x3B, ",": 0x2C, "=": 0x3D,
    "+": 0x2B, "(": 0x28, ")": 0x29, "[": 0x5B, "]": 0x5D,
    "{": 0x7B, "}": 0x7D, "_": 0x5F, "%": 0x25, "#": 0x23,
    "$": 0x24, "@": 0x40, "!": 0x21, "&": 0x26, "^": 0x5E,
    "~": 0x7E, "`": 0x60,
}

# QEMU's VNC keysym->PS/2 scancode table does not reliably apply Shift for
# punctuation on ReactOS 0.4.16: sending keysym 0x3A (':') arrives as '/', and
# 0x5F ('_') is dropped. In text mode we therefore press Shift explicitly and
# tap the *base* key, which the guest keyboard layout turns into the shifted
# character. --raw combo mode is unaffected (it holds modifiers as-is).
SHIFT_BASE = {
    ":": ";", "_": "-", "?": "/", "\"": "'", "<": ",", ">": ".",
    "{": "[", "}": "]", "|": "\\", "+": "=", "~": "`",
    "!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6",
    "&": "7", "*": "8", "(": "9", ")": "0",
}


def keysym(token):
    low = token.lower()
    if low in SPECIAL:
        return SPECIAL[low]
    if len(token) == 1:
        if token in CHARMAP:
            return CHARMAP[token]
        if token.isalpha():
            return ord(token.lower())
        return ord(token)
    raise ValueError(f"unknown key token: {token}")


def key_event(sock, sym, down):
    sock.sendall(struct.pack(">BBHI", 4, 1 if down else 0, 0, sym))


def tap(sock, sym):
    key_event(sock, sym, True)
    time.sleep(0.02)
    key_event(sock, sym, False)
    time.sleep(0.03)


def handshake():
    s = socket.create_connection((HOST, PORT), timeout=5)
    banner = s.recv(64)
    if not banner.startswith(b"RFB "):
        raise RuntimeError(f"bad banner: {banner!r}")
    s.sendall(b"RFB 003.008\n")
    sec = s.recv(64)
    types = list(sec[1:1 + sec[0]])
    if 1 not in types:
        raise RuntimeError(f"no 'None' security type: {types}")
    s.sendall(bytes([1]))
    result = s.recv(8)
    if len(result) < 4 or struct.unpack(">I", result[:4])[0] != 0:
        raise RuntimeError(f"security failed: {result!r}")
    s.sendall(bytes([1]))  # shared
    init = s.recv(4096)
    w, h = struct.unpack(">HH", init[:4])
    return s, w, h


def parse_tokens(text):
    out = []
    for ch in text:
        if ch == " ":
            out.append(("raw", "space"))
        else:
            out.append(("raw", ch))
    return out


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return
    raw = args[0] == "--raw"
    if raw:
        args = args[1:]
    tokens = []
    for arg in args:
        tokens.extend(parse_tokens(arg) if not raw else [("raw", arg)])

    s, w, h = handshake()
    print(f"VNC connected {w}x{h}")
    try:
        if not raw:
            # Text mode: emit each char, holding Shift explicitly for the
            # shifted punctuation above so the guest layout produces ':' and
            # '_' instead of ';'/'/' or nothing.
            for kind, tok in tokens:
                if tok == "space":
                    tap(s, SPECIAL["space"])
                    continue
                if tok in SHIFT_BASE:
                    key_event(s, SPECIAL["shift"], True)
                    time.sleep(0.02)
                    tap(s, keysym(SHIFT_BASE[tok]))
                    key_event(s, SPECIAL["shift"], False)
                    time.sleep(0.03)
                else:
                    tap(s, keysym(tok))
            tap(s, SPECIAL["enter"])
        else:
            # 组合键按 Ctrl/Shift/Alt 顺序按下，字符敲完再逆序松开。
            held = []
            for kind, tok in tokens:
                sym = keysym(tok)
                if sym in (SPECIAL["ctrl"], SPECIAL["shift"], SPECIAL["alt"]):
                    key_event(s, sym, True)
                    held.append(sym)
                else:
                    tap(s, sym)
            for sym in reversed(held):
                key_event(s, sym, False)
        print(f"injected {len(tokens)} token(s)")
    finally:
        s.close()


if __name__ == "__main__":
    main()
