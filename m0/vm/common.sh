# common.sh - ReactOS VM 公共配置（被 run-reactos-install.sh / run-reactos.sh source）
# 项目: ros-pinyin M0 蓝本移植验证
# 说明: 沙箱无 X11，显示走 VNC；HMP monitor 走 unix socket（screendump 截屏用）

# ---- 路径 ----
QEMU_HOME="${QEMU_HOME:-/home/twipty/.workbuddy/binaries/qemu-user}"
export LD_LIBRARY_PATH="$QEMU_HOME/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
QEMU="$QEMU_HOME/usr/bin/qemu-system-i386"

VM_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ISO="$VM_DIR/../downloads/ReactOS-0.4.16-i386.iso"
HDD="$VM_DIR/reactos.qcow2"
LOG_DIR="$VM_DIR/logs"
mkdir -p "$LOG_DIR"

# ---- 关键参数（与 PLAN.md 1.1 一致）----
# -accel kvm:tcg  有 /dev/kvm 用 KVM，否则自动回落 TCG
# -serial file    ReactOS 内核调试输出 (KDBG) 落地串口日志
# rtl8139 + SLIRP 用户网络: guest 访问宿主机用 10.0.2.2（传文件/下载用）
# usb-tablet: 无需 PS/2 捕获，VNC 指针绝对坐标定位
COMMON_ARGS=(
    -machine pc,accel=kvm:tcg
    -m 512
    -L "$QEMU_HOME/usr/share/qemu"
    -smp 1
    -device rtl8139,netdev=n0
    -netdev user,id=n0
    -device usb-tablet
    -usb
    -serial file:"$LOG_DIR/rosdbg.log"
    -monitor unix:"$VM_DIR/monitor.sock",server,nowait
    -vnc 127.0.0.1:1
    -name reactos-pinyin
)

# ---- 便捷函数 ----
# 用法: hmp <monitor命令>   例: hmp "info status" / hmp "screendump shot.ppm"
hmp() {
    local sock="$VM_DIR/monitor.sock"
    [ -S "$sock" ] || { echo "monitor.sock 不存在，VM 未启动？"; return 1; }
    printf '%s\n' "$1" | timeout 10 socat - unix-connect:"$sock" 2>/dev/null \
        || printf '%s\n' "$1" | timeout 10 nc -U "$sock"
}

# HMP 截屏: hmp_screendump <输出ppm路径>  (随后可用 ffmpeg 转 png)
hmp_screendump() {
    hmp "screendump $1" >/dev/null
}
