# RosPinyin

ReactOS 的开源简体中文拼音输入法 · [文档地图](docs/README.md) · [项目总览](docs/projectoverall.md)

## 项目介绍

RosPinyin 以独立 IMM32 输入法模块 `rospinyin.ime` 和码表 `rospinyin.dat` 分发，目标是在 ReactOS 0.4.16+（i386）上提供开箱即用的全拼中文输入，填补 CORE-19268 中「Chinese IME」的缺口。代码采用 MIT 许可证，数据与第三方组件的授权见 `NOTICE.txt`。

已发布版（alpha0.0.2）支持连续拼音输入、候选窗与翻页、空格/数字选字、`v` 键输入 `ü`、退格与 Esc 编辑、单独按 Shift 切换中英文；在 ReactOS 0.4.16 i386 真机验收 15/15 通过，目前只支持单字上屏。

完整的项目章程、立项依据、决策记录与开发历程见 [项目总览](docs/projectoverall.md) 和 [文档地图](docs/README.md)。

## 使用说明

以 alpha0.0.2 为例（ReactOS 0.4.16+ i386）：

1. 从 Release 下载 `rospinyin.ime` 和 `rospinyin.dat`，两个文件都要。
2. 在 ReactOS 命令行把它们复制到系统目录（注意是 `C:\ReactOS\system32`，不是 `C:\Windows\system32`）：

   ```bat
   copy /y rospinyin.ime C:\ReactOS\system32\
   copy /y rospinyin.dat C:\ReactOS\system32\
   ```

3. 注册输入法布局（可用仓库 `tools/install-rospinyin` 安装辅助，或调用 `ImmInstallIMEW` 注册 `rospinyin.ime`），然后重启；用 `Alt+Shift` 切到「中文(简体) - RosPinyin」。
4. 直接输入拼音即可选字：空格/数字选字，`=`/`.` 下一页、`-`/`,` 上一页，`v` 代替 `ü`，Backspace 回退，Esc 取消。
5. **中英文切换**：单独按一下 `Shift` 在中/英之间切换；英文态按键直接上屏，`Shift+A` 仍是正常的大写 A。
6. **`[快]` / `[全]` 字形模式**（未发布的新版本）：`[快]` 是默认模式，只显示系统保底字体可绘制的候选；`[全]` 显示全部候选，包含需要额外中文字体支持的生僻字——**未安装相应字体时会显示方框**。按 `Ctrl+Shift+G` 切换，状态显示在工具栏；该设置只对当前输入法会话生效，重选输入法后恢复 `[快]`。

## Release

- [全部版本](https://github.com/twipty/ros-chinese-ime/releases)
- [alpha0.0.2（最新，2026-11-18）](https://github.com/twipty/ros-chinese-ime/releases/tag/alpha0.0.2) — [发布说明](docs/releases/alpha0.0.2.md) · [rospinyin.ime](https://github.com/twipty/ros-chinese-ime/releases/download/alpha0.0.2/rospinyin.ime) · [rospinyin.dat](https://github.com/twipty/ros-chinese-ime/releases/download/alpha0.0.2/rospinyin.dat)
- [alpha0.0.1（首个公开版，2026-11-17）](https://github.com/twipty/ros-chinese-ime/releases/tag/alpha0.0.1)
