#!/usr/bin/env bash
# run-test-guest.sh - ReactOS 双 guest 对照环境（nightly vs 0.4.16 正式）
#
# 两个 guest 完全隔离：独立磁盘、VNC、HMP socket、串口日志、noVNC 端口。
#
# 三台 guest 角色分工（owner 2026-11-24 定）：
#   - 6080/5901 reactos.qcow2        开发期配合测试（run-reactos.sh，日常改动上这台）
#   - 6081/5902 reactos-nightly.*    里程碑验收 —— nightly 侧（本脚本管理）
#   - 6082/5903 reactos-stable.*     里程碑验收 —— 0.4.16 正式侧（本脚本管理）
# 里程碑验收只认 6081 + 6082 成对对照，不拿 6080 充当验收证据。
#
# 用法:
#   ./run-test-guest.sh install nightly   # 从 nightly ISO 引导，进 OS 安装向导
#   ./run-test-guest.sh install stable    # 从 0.4.16 ISO 引导，进 OS 安装向导
#   ./run-test-guest.sh boot nightly      # 从硬盘引导已装好的系统
#   ./run-test-guest.sh boot stable
#   ./run-test-guest.sh novnc nightly     # noVNC 6081 -> 5902
#   ./run-test-guest.sh novnc stable      # noVNC 6082 -> 5903
#
# OS 安装向导由用户本人通过 noVNC 操作；本脚本只负责起 VM 与转发端口。
set -euo pipefail

MODE="${1:-}"
ROLE="${2:-}"

QEMU_HOME="${QEMU_HOME:-/home/twipty/.workbuddy/binaries/qemu-user}"
export LD_LIBRARY_PATH="$QEMU_HOME/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
QEMU="$QEMU_HOME/usr/bin/qemu-system-i386"
PY_VENV=/home/twipty/.workbuddy/binaries/python/envs/default/bin/python

VM_DIR="$(cd "$(dirname "$0")" && pwd)"
DL_DIR="$VM_DIR/../downloads"

case "$ROLE" in
    nightly)
        HDD="$VM_DIR/reactos-nightly.qcow2"
        ISO="$DL_DIR/reactos-bootcd-0.4.17-dev-1398-gdbbf9a2-x86-gcc-lin-dbg.iso"
        VNC_DISPLAY=2          # 127.0.0.1:5902
        NOVNC_PORT=6081
        MONITOR="$VM_DIR/monitor-nightly.sock"
        GUEST_NAME=reactos-nightly
        ;;
    stable)
        HDD="$VM_DIR/reactos-stable.qcow2"
        ISO="$DL_DIR/ReactOS-0.4.16-i386.iso"
        VNC_DISPLAY=3          # 127.0.0.1:5903
        NOVNC_PORT=6082
        MONITOR="$VM_DIR/monitor-stable.sock"
        GUEST_NAME=reactos-stable
        ;;
    *)
        echo "usage: $0 {install|boot|novnc} {nightly|stable}" >&2
        exit 2
        ;;
esac

LOG_DIR="$VM_DIR/logs/$ROLE"
mkdir -p "$LOG_DIR"

if [ "$MODE" = "novnc" ]; then
    echo "[$ROLE] noVNC: http://<host>:$NOVNC_PORT/vnc.html -> 127.0.0.1:$((5900 + VNC_DISPLAY))"
    exec "$PY_VENV" -m websockify --web "$VM_DIR/noVNC-1.5.0" \
        "$NOVNC_PORT" "127.0.0.1:$((5900 + VNC_DISPLAY))"
fi

[ -f "$HDD" ] || { echo "disk missing: $HDD" >&2; exit 2; }

BOOT_ARGS=()
case "$MODE" in
    install)
        [ -f "$ISO" ] || { echo "ISO missing: $ISO" >&2; exit 2; }
        BOOT_ARGS=(-boot d -cdrom "$ISO")
        ;;
    boot)
        # boot from disk, but expose tools.iso as a data CD-ROM (D:) so fresh
        # guests (no wget/curl) can copy wget.exe + the installer off the disc.
        BOOT_ARGS=(-boot c -cdrom "$VM_DIR/tools.iso")
        ;;
    *)
        echo "usage: $0 {install|boot|novnc} {nightly|stable}" >&2
        exit 2
        ;;
esac

echo "[$ROLE/$MODE] disk=$HDD"
echo "[$ROLE] ISO=${ISO:-<from disk>}"
echo "[$ROLE] VNC=127.0.0.1:$((5900 + VNC_DISPLAY))  HMP=$MONITOR  log=$LOG_DIR/rosdbg.log"

exec "$QEMU" \
    -machine pc,accel=kvm:tcg \
    -m 512 \
    -L "$QEMU_HOME/usr/share/qemu" \
    -smp 1 \
    -device rtl8139,netdev=n0 \
    -netdev user,id=n0 \
    -device usb-tablet \
    -usb \
    -serial file:"$LOG_DIR/rosdbg.log" \
    -monitor unix:"$MONITOR",server,nowait \
    -vnc "127.0.0.1:$VNC_DISPLAY" \
    -name "$GUEST_NAME" \
    "${BOOT_ARGS[@]}" \
    -drive file="$HDD",format=qcow2,if=ide
