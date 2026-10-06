# 开麦匣 / GameVoiceBox 1.2.0

Windows 10 / 11 x64 的原生 C / Win32 / WASAPI 音板，提供用户语音包里的 14 条完整音效。真实麦克风与音效混合发送到 CABLE Input；游戏语音输入选择 CABLE Output，耳机试听只包含音效。

**默认不设置任何全局快捷键，包括停止键。** 14 条语音和停止操作均可自行绑定，也可留空。窗口内 Esc 可停止音效。每次启动都不自动连接，发送与试听均关闭。

## 使用

1. 阅读 [中文使用教程](docs/中文使用教程.html)，使用 `GameVoiceBox-Setup-1.2.0-x64.exe` 安装
2. 按需要安装包内的官方标准 VB-CABLE，安装是可选步骤，默认不勾选；由官方交互安装器和 Windows UAC 处理，完成后保存工作并手动重启
3. 选择真实麦克风、CABLE Input 和真实耳机，主动连接并开启发送
4. 游戏输入选 CABLE Output，输出选真实耳机；先自行绑定常用音效，再按住实际队伍语音键直到音效完整播放结束

游戏按键说话通常使用 V，改过就使用自己的绑定。应用不会代按游戏语音键，不改变系统默认设备，不注入游戏按键。14 条音效可滚动查看并点击播放；一次只放一段，再次触发会替换或从头播放。

## 14 条完整语音

操操操、卡了、vamos!、加纳、卡键、吞子弹了、呀嘞呀嘞、呀嘞呀嘞（缓慢版）、宽带、椅子、没办法、看小地图去了、设置、资本。

时长约 0.72–9.22 秒，逐条时长见教程。`assets/catalog.json` 是顺序、名称、来源、默认空白绑定及完整 PCM 帧数的唯一目录；`scripts/build_assets.py` 同时生成 `src/app/clip_catalog.h` 和 `assets/integrity.json`。前 3 条素材的解码 WAV 字节与上一版一致。

原始文件以原文件名留在 `assets/original/`，保持 SHA-256 不变。8 个文件虽然扩展名为 MP3，实际使用 AAC 音轨的 MP4/M4A 容器；其中 4 个还含视频。构建时使用 ffmpeg `-map 0:a:0` 完整解码第一音轨为 48 kHz 双声道 PCM16 WAV，不裁剪、不循环、不调速、不归一化。安装包只携带 14 个运行 WAV 和目录/校验元数据，不包含原始 AAC/视频文件。

## 从源码构建（Linux）

需要 gcc、make、ffmpeg、Python 3、MinGW-w64 的 x64 Win32 编译器/headers/binutils、NSIS 和 7zip。Debian 包通常为 `gcc-mingw-w64-x86-64-win32`、`mingw-w64-x86-64-dev`、`binutils-mingw-w64-x86-64`、`nsis`、`7zip`。编译工具不随安装包分发。

Git 源码保留官方驱动的来源及哈希清单，不存放其 ZIP 或二进制。先运行 `make fetch-vendor`：脚本只从 `vendor/integrity.json` 指定的 VB-Audio 官方 URL 恢复标准 Pack45，校验整个 ZIP 和全部 31 个文件的 SHA-256 后解压，不安装驱动。若已有官方 ZIP，可运行 `python3 scripts/fetch_vendor.py --archive /path/to/VBCABLE_Driver_Pack45.zip`；不同哈希会被拒绝。Release 随附的完整源代码 ZIP 已保留原始官方文件。

使用系统工具：

```sh
make fetch-vendor
make assets
make TOOLS_ROOT=/usr clean test windows package verify-package source
```

`TOOLS_ROOT` 默认为 `/usr`；可按需要覆盖 `XCC`、`XWINDRES`、`MAKENSIS`、`SEVENZIP`。首次从外部原始目录导入可运行 `python3 scripts/build_assets.py --source-dir /path/to/original-clips`，之后 `make assets` 只用仓库内 `assets/original`。源码本身不依赖开发者的本地音频目录、个人网盘 ID 或账号。

## 验证边界

Linux 自动测试覆盖产品共用的纯 C 音频、设置、热键和控制状态逻辑，使用 ASAN/UBSAN；资产测试独立核对全部原始哈希、14 个完整 PCM 帧数、逐文件重新完整解码一致性和生成目录的 C 边界。平台调用替身测试仅验证调用契约。

**尚未完成 Windows 实机运行验收。** 交叉构建成功、PE 导入检查和 NSIS 实际解包核验均不能代替 UAC、驱动加载、真实设备收音、游戏前台热键或队友收音。请按 [Windows 验收清单](docs/Windows验收清单.txt) 逐项记录。此云端环境的 LeakSanitizer ptrace 检查不可用，测试设置 `detect_leaks=0`，不宣称泄漏检测通过。

## 官方组件、签名与权利

标准 VB-CABLE 由 VB-Audio 开发，是 Donationware。最终用户应能识别来源，并能在觉得有用时捐赠或支付适用许可；机构/组织使用须按官方说明处理：[产品与安装](https://vb-audio.com/Cable/index.htm)、[许可和分发](https://vb-audio.com/Services/licensing.htm)、[官方商店](https://shop.vb-audio.com/en/)。不捆绑 VB-CABLE A+B / C+D。全部 31 个官方文件随安装包提供并保持字节不变；原始标准 Pack45 ZIP 保留于完整源码 ZIP，安装目录不重复存放该压缩包。

自制应用与安装器没有商业代码签名证书，Windows 可能显示未知发布者或 SmartScreen 提示。先核对来源和 SHA-256；不要求关闭防病毒、SmartScreen 或驱动签名检查。官方驱动文件保留自己的原始签名。

本项目尚未指定开源许可证；语音素材与官方驱动的权利由各自原权利人保留。公开源码或安装包不构成对第三方素材的改作、商用或再分发许可。详见 [LICENSE.md](LICENSE.md) 和 [第三方说明](docs/THIRD_PARTY_NOTICES.md)。

默认安装路径为 `%LOCALAPPDATA%\Programs\GameVoiceBox`，设置为 `%APPDATA%\GameVoiceBox\settings.ini`。卸载仅删除清单中的应用文件，不移除共享驱动，保留用户设置和用户新增文件。
