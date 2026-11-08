# Staff Runner（FM-1）

[English](README.md) | **简体中文**

一个用 C99 编写的紧凑型五线谱节奏游戏，运行于 240×240 屏幕的 M-VAVE FM-1。游戏包含独立的高音谱号与低音谱号模式、三段短小的原创音符序列，以及 MIDI 歌曲《Daughter's Love》。选择 Practice 会等待你按对音高；选择 Miss Mode 则音乐与音符继续前进，弹错或漏弹都会让 MISS 计数增加。MIDI 歌曲会播放鼓、前奏与贝斯作为伴奏，旋律音符则显示在五线谱上供玩家演奏。

<img width="737" height="476" alt="image" src="https://github.com/user-attachments/assets/95cdcd1b-7ba8-4cd5-8574-8454f9e68995" />

## 操作

- 标题界面按 **PLAY** 进入谱号选择。旋转编码器 1 在高音谱号与低音谱号之间切换，然后按 **PLAY**。
- 用编码器 1 选择 **Practice** 或 **Miss Mode**，按 **PLAY** 确认。
- 旋转编码器 1 选曲，按 **PLAY** 开始。
- 按下与判定线处音符对应的实体琴键。Practice 会等待你弹对音高；Miss Mode 则继续播放，并记录弹错与漏掉的音符。
- 接入的 MIDI 键盘也可以在任何 MIDI 通道上发送 F3–G5 的 note-on / note-off 来演奏。按 **PLAY**，用模拟器或 FM-1 面板按键选择谱号与曲目；游戏中的音高由 MIDI 音符控制。
- 按 **HOME** 返回选曲界面。一曲结束后按 **PLAY** 可选择其他曲目。按住 **HOME** 再按 **REC** 会回到标题界面重新开始。

### FM-1 浏览器模拟器操作

- 先点击 **Start**。可以点击 FM-1 面板上的虚拟 **PLAY**、**HOME** 按钮，或用 **空格** 代替 PLAY、**7** 代替 HOME。**R** 是 REC；同时按住 **7** 再按 **R** 可重置回标题界面。
- 在编码器 1 上滚动鼠标滚轮、拖动编码器 1，或按 **← / →**，即可在谱号、模式与曲目选项中移动。**9 / ARP** 后退一项，**0 / SEQ** 前进一项，因此不用鼠标也能用模拟器键盘操作菜单。
- 琴键可以直接点击虚拟按键，或使用模拟器提供的电脑键盘钢琴布局演奏。如果电脑快捷键没有生效，先点击一下模拟器面板。

谱面只使用自然音。FM-1 的音符编号中，bit 0 对应 F3，bit 26 对应 G5；具体谱号的映射由标准五线谱最下方一条线推导（高音谱号为 E4，低音谱号为 G2）。低音谱练习会把音符放在 FM-1 可演奏的音域内，需要时在谱表上方绘制加线。

## 构建与 FM-1 Open 集成

本游戏基于官方 [FM1 Open 源码](https://github.com/m-vave/fm1_open) 构建。`fm1.h` 提供面板、显示、时钟与本地 MIDI 接口。安装脚本会把游戏模块与 MIDI 谱面数据复制到 `app/`，并修改 `app/screen.c` 使其直接启动游戏。为在 flash 中腾出空间，这个游戏构建移除了原有的 FM1 Open 屏幕编辑器；FM-1 音频引擎、面板扫描、MIDI 以及经过验证的 SDK 钩子均保留。

对解压后的 FM1 Open 源码目录运行安装脚本：

~~~powershell
.\install_fm1_game.ps1 'C:\path\to\fm1_open-main'
~~~

开机后会显示游戏标题。按 **PLAY** 依次选择谱号、模式与曲目，用编码器 1 在各菜单中移动。适配层使用官方面板映射（HOME 8、PLAY 12、REC 13）、按键边沿检测、16 行异步 RGB565 显示刷新，以及本地 MIDI 播放来输出歌曲与音符音效。

按 FM1 Open README 的说明安装杰理（JieLi）Windows 工具链 2.5.3，然后在 FM1 Open 源码根目录运行 `./sdk/Build.ps1 -FirmwareOnly -Toolchain 'C:\JL\pi32'`。本仓库不负责烧录硬件。

最初的输入实现参考了 [FM1 Quest 的 `fm1_input.h`](https://github.com/hericdk/fm1-quest/blob/main/firmware/hal/fm1_input.h)；现在的实际集成改用官方 FM1 Open 公开 SDK。

`src/staff_game_fm1.c` 中紧凑的 5×7 字模表改编自 FM1 Open 的 `app/screen.c`；其源项目采用 GPL-3.0 并附带 FM1 SDK 链接例外。游戏模块沿用相同的 SPDX 许可证标识。
