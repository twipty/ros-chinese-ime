#!/usr/bin/env bash
# probe-n.sh - Preload 探针实验 N 组（负对照）的环境准备与启动
#
# 用法:
#   ./probe-n.sh prep     暂存被测文件 + 建覆盖层（可在旧 VM 运行时执行）
#   ./probe-n.sh start    在覆盖层上启动 VM（要求 5901 空闲、基础盘未被占用）
#   ./probe-n.sh shot NAME 截屏到 m0/vm/evidence/preload-probe/N/NAME.png（HMP screendump）
#   ./probe-n.sh state     打印覆盖层大小 / VM 是否在跑 / 串口日志尾部
#   ./probe-n.sh stop      通过 HMP quit 让 QEMU 优雅退出（不 kill，保证 flush）
#   ./probe-n.sh rollback  删除覆盖层（VM 必须先关掉）；clean 是它的别名
#
# 为什么是这三条硬守卫:
#   1) 5901 已被占用 -> 说明另有一个 VM 在跑，它可能正握着基础盘；
#   2) 基础盘被锁 -> 覆盖层的 backing 在被写，此时快照没有意义；
#   3) 覆盖层不存在 -> 直接起 VM 会写基础盘，蓝屏就可能毁掉已验收的环境。
#
# 本项目禁止程序代替用户发送键盘/鼠标事件：本脚本只起 VM、截屏、收日志，
# 写 Preload 与关机重启必须人手完成（见 PRELOAD-PROBE.md §2）。
set -euo pipefail

VM_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=/dev/null
source "$VM_DIR/common.sh"

OVL="${ROSPINYIN_PROBE_OVL:-/home/twipty/.workbuddy/tmp/reactos.probe-N.qcow2}"
SHARE="${ROSPINYIN_PROBE_SHARE:-/home/twipty/.workbuddy/tmp/vmshare/probe-N}"
RELEASE_IME="${ROSPINYIN_PROBE_IME:-/home/twipty/.workbuddy/tmp/build-win-rel/ime/rospinyin.ime}"
EVID="$VM_DIR/evidence/preload-probe/N"

# N 组被测二进制必须与发版一致：alpha0.0.2 / TRACE=OFF / 940,601 B。
EXPECT_IME_SIZE=940601
EXPECT_IME_SHA=c9dc17c037be05e6c4af62eb8b37b6e4ae72dedbe6ced5f9e0f116c23e25feaa

say() { printf '[probe-n] %s\n' "$*"; }
die() { printf '[probe-n] 拒绝: %s\n' "$*" >&2; exit 1; }

port_busy() { (exec 3<>"/dev/tcp/127.0.0.1/$1") 2>/dev/null; }

base_locked() {
    local ino
    ino=$(stat -c %i "$VM_DIR/reactos.qcow2" 2>/dev/null) || return 1
    awk -v i="$ino" '$0 ~ (" " i " ") {found=1} END {exit !found}' /proc/locks 2>/dev/null
}

cmd_prep() {
    [ -f "$RELEASE_IME" ] || die "找不到发布版 .ime: $RELEASE_IME"
    local size sha
    size=$(stat -c %s "$RELEASE_IME")
    sha=$(sha256sum "$RELEASE_IME" | cut -d' ' -f1)
    [ "$size" = "$EXPECT_IME_SIZE" ] || die "被测 .ime 是 $size 字节，应为 $EXPECT_IME_SIZE（TRACE=OFF 的发版产物）"
    [ "$sha" = "$EXPECT_IME_SHA" ] || die "被测 .ime sha256 不符: $sha"

    mkdir -p "$SHARE" "$EVID"
    cp -f "$RELEASE_IME" "$SHARE/rospinyin.ime"
    cp -f "$VM_DIR/../share/rospinyin.dat" "$SHARE/rospinyin.dat"
    cp -f "$VM_DIR/../share/install-rospinyin.exe" "$SHARE/install-rospinyin.exe"
    ( cd "$SHARE" && sha256sum rospinyin.ime rospinyin.dat install-rospinyin.exe > SHA256SUMS.txt )
    say "已暂存到 $SHARE（guest 侧 http://10.0.2.2:8000/probe-N/）"
    sed 's/^/         /' "$SHARE/SHA256SUMS.txt"

    if [ -f "$OVL" ]; then
        say "覆盖层已存在，保留: $OVL ($(stat -c %s "$OVL") 字节)"
    else
        if base_locked; then
            die "基础盘正被某个 VM 占用，等它关机后再建覆盖层"
        fi
        "$QEMU_HOME/usr/bin/qemu-img" create -f qcow2 -F qcow2 \
            -b "$VM_DIR/reactos.qcow2" "$OVL" >/dev/null
        say "覆盖层已建: $OVL ($(stat -c %s "$OVL") 字节，backing=reactos.qcow2)"
    fi
}

cmd_start() {
    [ -f "$OVL" ] || die "覆盖层不存在，先跑: $0 prep"
    base_locked && die "基础盘已被占用（另一个 VM 在跑），先正常关掉它"
    if port_busy 5901; then
        die "5901 已被占用：先关掉当前 VM（它的 noVNC 窗口会断开），再执行 start"
    fi
    say "磁盘: $OVL"
    say "VNC: 127.0.0.1:5901（浏览器仍用 noVNC :6080 连，地址不变）"
    say "HMP: $VM_DIR/monitor.sock"
    exec env ROSPINYIN_HDD="$OVL" "$VM_DIR/run-reactos.sh"
}

cmd_shot() {
    local name="${1:-shot}"
    mkdir -p "$EVID"
    local ppm="$EVID/$name.ppm"
    hmp "screendump $ppm" >/dev/null 2>&1 || die "screendump 失败：monitor.sock 不在（VM 未跑？）"
    ffmpeg -loglevel error -y -i "$ppm" "$EVID/$name.png"
    rm -f "$ppm"
    say "已截图: $EVID/$name.png"
}

cmd_state() {
    if [ -f "$OVL" ]; then
        say "覆盖层: $(stat -c %s "$OVL") 字节 (mtime $(date -r "$OVL" +%H:%M:%S))"
    else
        say "覆盖层: 不存在"
    fi
    port_busy 5901 && say "5901: 有 VM 在跑" || say "5901: 空闲"
    base_locked && say "基础盘: 被占用" || say "基础盘: 未被占用"
    if [ -f "$LOG_DIR/rosdbg.log" ]; then
        say "串口日志: $(stat -c %s "$LOG_DIR/rosdbg.log") 字节"
    fi
}

cmd_stop() {
    if ! port_busy 5901; then
        say "没有 VM 在跑（5901 空闲）"
        return 0
    fi
    say "通过 HMP quit 让 QEMU 退出（正常 flush 覆盖层，不是 kill）..."
    hmp "quit" >/dev/null 2>&1 || true
    local i
    for i in $(seq 1 15); do
        if ! port_busy 5901; then
            say "VM 已退出"
            return 0
        fi
        sleep 1
    done
    die "15 秒内未退出；不要 kill -9（会丢未 flush 的写），先看 monitor.sock 是否还在"
}

cmd_rollback() {
    port_busy 5901 && die "VM 还在跑，先关机再 rollback"
    if [ -f "$OVL" ]; then
        rm -f "$OVL"
        say "已删除覆盖层；基础盘从未被写入"
    else
        say "覆盖层本来就不存在"
    fi
}

case "${1:-state}" in
    prep)     cmd_prep ;;
    start)    cmd_start ;;
    shot)     cmd_shot "${2:-shot}" ;;
    state)    cmd_state ;;
    stop)     cmd_stop ;;
    rollback|clean) cmd_rollback ;;
    *)        die "未知子命令: ${1:-}（prep|start|shot|state|stop|rollback）" ;;
esac
