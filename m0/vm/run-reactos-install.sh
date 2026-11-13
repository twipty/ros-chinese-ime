#!/usr/bin/env bash
# run-reactos-install.sh - ReactOS 0.4.16 全新安装（只需跑一次）
# 用法: ./run-reactos-install.sh
#   - 从 ISO 引导进入 ReactOS 第二阶段安装向导（磁盘分区/格式化/复制文件）
#   - 安装完成后 Ctrl+C 或 monitor 里 system_powerdown 关机，再改用 run-reactos.sh
set -euo pipefail
source "$(dirname "$0")/common.sh"

echo "[install] 磁盘: $HDD  ISO: $ISO"
echo "[install] VNC: 127.0.0.1:5901   HMP monitor: $VM_DIR/monitor.sock"
echo "[install] 安装完成后请正常关机，再用 run-reactos.sh 启动"
echo

exec "$QEMU" "${COMMON_ARGS[@]}" \
    -boot d \
    -cdrom "$ISO" \
    -drive file="$HDD",format=qcow2,if=ide
