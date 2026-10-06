# 第三方来源与权利说明

适用：开麦匣 / GameVoiceBox 1.2.1。核对日期：2026-10-06。

## 1. 用户提供的语音素材

14 个原始文件来自“荣誉区1.09版本.zip”。原始压缩包 SHA-256 为 `17f5c2b1d9badf57c3700676cd24f32a171867f57fa473478dec9fb3e9d86f0e`。文件级来源、容器、音频编码、视频/扩展名不符记录和 SHA-256 见 `assets/catalog.json`；完整 PCM 和运行 WAV 的哈希见 `assets/integrity.json`。

原始文件以原文件名保存在源代码包 `assets/original/` 中，保持字节不变；安装器只包含运行 WAV 和目录/校验元数据。8 个扩展名为 MP3 的文件实际使用 AAC 音轨的 MP4/M4A 容器，宽带、椅子、没办法、资本还包含 H.264 视频。构建只完整提取第一条音轨，不分发这些视频到应用运行目录。

原始素材及其派生 WAV 的语音、录音和相关权利归各自原权利人。文件名不能作为人物身份、版权归属或再许可的证明。项目没有新增素材授权，没有把这些文件改为 MIT 或其他开源许可；转换为 WAV 不改变其权利。

## 2. 标准 VB-CABLE Driver Pack45

作者与来源：VB-Audio。产品：[VB-CABLE 官方产品与安装页](https://vb-audio.com/Cable/index.htm)。许可：[VB-Audio 官方许可和分发说明](https://vb-audio.com/Services/licensing.htm)。支持：[VB-Audio 官方论坛](https://forum.vb-audio.com/)。捐赠/许可购买：[VB-Audio 官方商店](https://shop.vb-audio.com/en/)。

VB-CABLE 是 Donationware。最终用户应清楚知道它由 VB-Audio 提供，并有机会在觉得有用时捐赠或支付适用许可。官方当前许可页允许在满足可见来源和 Donationware 条件时随应用分发标准 VB-CABLE；机构、组织等专业领域如最终使用者无法识别或支付许可，须按官方要求购买适用数量的许可。请阅读官方当前说明，疑问向作者确认。

完整源码 ZIP 保留 `VBCABLE_Driver_Pack45.zip` 和其中全部 31 个官方文件；安装包只携带这 31 个文件，不重复存储官方原始 ZIP。ZIP、EXE、SYS、CAT、INF、readme 和图标均保持原始字节；SHA-256 在 `vendor/integrity.json`。保留官方文件原有签名；自制应用和安装器本身未获商业代码签名。Git 仓库仅保留来源和校验元数据，通过 `scripts/fetch_vendor.py` 从官方地址恢复并逐项校验；完整源代码 ZIP 与安装包携带原始官方文件。

包内官方 readme 的较早分发表述与现行网页可能不同，项目不修改原文件，也不替作者解释新许可。分发时在安装向导、应用说明和中文教程中保留来源、Donationware 与捐赠入口。**没有分发或捆绑 VB-CABLE A+B / C+D。**

驱动安装可选，默认不启用，由官方交互式 x64 安装器及 Windows UAC 处理；安装/卸载后需由用户保存工作并手动重启。应用卸载不移除已安装的共享驱动。

## 3. 构建工具与 Windows API

FFmpeg 仅作为开发时的音频解码工具，不作为应用运行依赖，也不随安装器提供其二进制。MinGW-w64、GCC、binutils、NSIS、7zip、Python 和 make 用于构建或核验，工具本身不随应用安装包分发；工具来源和构建环境记录见 `docs/构建工具来源.json`。工具及其运行库如有适用许可，仍按各自条款处理。

应用使用 Windows 自带 Win32 / WASAPI 系统接口，安装包不提供浏览器、Electron、.NET、Go 或额外编译器运行时 DLL。引用的产品名和标识属于其各自权利人。本项目与 VB-Audio、Microsoft、Riot Games 没有因此建立背书或合作关系。

项目源码的授权状态见根目录 `LICENSE.md`；此文件不对任何第三方素材、驱动或工具重新授予许可。
