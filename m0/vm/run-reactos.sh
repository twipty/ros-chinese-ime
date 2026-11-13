#!/usr/bin/env bash
# run-reactos.sh - 日常启动已安装的 ReactOS（M0 常用入口）
# 用法: ./run-reactos.sh
#   - 从硬盘引导 ReactOS
#   - 串口 KDBG 日志: logs/rosdbg.log
#   - VNC: 127.0.0.1:5901 （沙箱外可用 VNC 客户端连宿主机 5901 观察屏幕）
#   - HMP: ./monitor.sock  （hmp "screendump x.ppm" 截屏）
# 传文件进 guest: guest 里 ftp 10.0.2.2 或用SLIRP 内置服务
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo "[run] 磁盘: $HDD（无 ISO，从硬盘引导）"
echo "[run] VNC: 127.0.0.1:5901   HMP monitor: $VM_DIR/monitor.sock"
echo

exec "$QEMU" "${COMMON_ARGS[@]}" \
    -boot c \
    -drive file="$HDD",format=qcow2,if=ide
