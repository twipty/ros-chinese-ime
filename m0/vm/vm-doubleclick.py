#!/usr/bin/env python3
# vm-doubleclick.py - 通过 QEMU HMP monitor 精确时序双击桌面图标
# 用法: python3 vm-doubleclick.py <screen_x> <screen_y> [screen_w] [screen_h]
# 屏幕 800x600, usb-tablet 绝对坐标范围 0-0x7fff (32767)
import socket, sys, time

VM_DIR = "/home/twipty/WorkBuddy/2026-11-12-20-02-47/ros-pinyin/m0/vm"
SOCK = f"{VM_DIR}/monitor.sock"

def send(sock, cmd, wait=0.15):
    sock.sendall((cmd + "\n").encode())
    time.sleep(wait)
    try:
        sock.settimeout(0.3)
        data = sock.recv(65536)
        return data.decode(errors="replace")
    except socket.timeout:
        return ""

def main():
    sx = int(sys.argv[1]); sy = int(sys.argv[2])
    W = int(sys.argv[3]) if len(sys.argv) > 3 else 800
    H = int(sys.argv[4]) if len(sys.argv) > 4 else 600
    ax = int(sx / W * 32767)
    ay = int(sy / H * 32767)
    print(f"screen ({sx},{sy}) -> absolute ({ax},{ay})")

    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(SOCK)
    time.sleep(0.3)
    s.settimeout(0.5)
    try: s.recv(65536)  # 清掉 banner
    except socket.timeout: pass

    send(s, f"mouse_move {ax} {ay}", 0.5)
    # 双击: 按下-释放-按下-释放, 间隔 ~60ms
    send(s, "mouse_button btn-left", 0.06)
    send(s, "mouse_button release btn-left", 0.06)
    send(s, "mouse_button btn-left", 0.06)
    send(s, "mouse_button release btn-left", 0.2)
    print("double-click sent")
    time.sleep(3)
    # 截屏
    send(s, f"screendump {VM_DIR}/logs/dclick2.ppm", 1.5)
    s.close()
    print("screendump saved: logs/dclick2.ppm")

if __name__ == "__main__":
    main()
