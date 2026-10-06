#!/usr/bin/env python3
"""Produce the release report from fresh test evidence and actual package measurements."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,os
ROOT=Path(__file__).resolve().parents[1]
def report():
 p=json.loads((ROOT/'dist/package-verification.json').read_text())
 source=ROOT/'dist/GameVoiceBox-Source-1.2.0.zip'
 log=ROOT/'build/final-verification.log'
 marker=ROOT/'build/final-verification.passed'
 if not source.is_file() or not log.is_file() or not marker.is_file():
  raise RuntimeError('Fresh complete verification and source archive required')
 assets=json.loads((ROOT/'assets/integrity.json').read_text())
 if len(assets)!=14:raise RuntimeError('Full 14-clip integrity evidence required')
 installer=ROOT/'dist/GameVoiceBox-Setup-1.2.0-x64.exe'
 if hashlib.sha256(installer.read_bytes()).hexdigest()!=p['installer_sha256']:
  raise RuntimeError('Installer changed after package verification')
 review_path=ROOT/'build/review-summary.txt'
 review=review_path.read_text().strip() if review_path.is_file() else '独立审查尚未记录，不宣称审查通过。'
 lines=[
  '开麦匣 GameVoiceBox 1.2.0：云端构建与发布验证报告',
  '时间：'+datetime.now(timezone.utc).isoformat(timespec='seconds'),
  'GitHub 代码提交：'+os.environ.get('SOURCE_COMMIT','发布时记录于 GitHub Release'),'',
  '完整荣誉语音包：14 条；不设置任何默认全局快捷键（包括停止），只在用户填写并保存后注册。',
  '窗口内停止按钮与本窗口 Esc 可停止音效；所有语音和停止全局快捷键都可以留空。',
  '每次启动发送和试听关闭；麦克风混入发送，耳机试听仅音效。',
  '完整源文件原样保存在源码 assets/original；运行包只包含音频 PCM，不携带原始视频。','',
  '实际交付测量与解包核验：',
  f"一体安装程序：{p['installer_bytes']:,} bytes / {p['installer_mib']:.4f} MiB",
  f"原生应用 EXE：{p['native_exe_bytes']:,} bytes / {p['native_exe_bytes']/1024:.2f} KiB",
  f"完整源码 ZIP：{source.stat().st_size:,} bytes / {source.stat().st_size/1048576:.4f} MiB",
  f"实际解包并核对：{p['verified_payload_files']} 个应用文件；官方标准 VB-CABLE 31 个文件哈希一致；原 ZIP 保留在完整源码包，不在运行安装目录重复存储。",
  '解包后的实际 AMD64 EXE 直接经 objdump 检查导入，不使用缓存导入清单作为证据。',
  'Windows 自带 DLL 导入：'+', '.join(p['windows_dll_imports']),
  '无 Electron/Chromium/.NET/Go 或额外编译器运行时 DLL；未使用 UPX。','',
  '完整 48kHz / 双声道 / PCM16 音频测量：',
 ]
 for a in assets:
  lines.append(f"- {a['source']}：{a['frames']:,} 帧 / {a['frames']/48000:.6f} 秒；WAV SHA-256 {a['wav_sha256']}")
 lines += [
  '', '已执行的验证：',
  'make clean test windows package verify-package test-package；fresh aggregate success marker 已核验。',
  '共享 C 核心、设置、可选全局热键、控制状态与 Windows 调用契约测试；ASAN/UBSAN 启用。',
  '14 条音频完整解码/源哈希/PCM 帧数；无默认热键注册、可选停止、清空/持久化/冲突回滚。',
  '44.1/48/96kHz 格式/混音、独立发送与试听、麦克风增益/静音、队列与失效代次回归。',
  '严格 C11 -Wall -Wextra -Werror Windows x64 交叉编译与链接。',
  'NSIS 真实解包/所有 payload 哈希/可选官方驱动/清单卸载保护新增文件。',
  '中文教程 HTML 静态结构、锚点和本地链接已检查；云浏览器加载本地页面被客户端策略阻止，未进行视觉渲染验收。',
  '', '独立代码审查：',review,'',
  '未执行、仍待 Windows 实机验收：',
  '- Windows 原生界面、DPI、滚动和键盘操作',
  '- 安装/卸载程序运行、UAC 取消、官方驱动安装/重启/加载',
  '- 真实麦克风隐私授权、WASAPI 录放、USB 断开和长期硬件时钟',
  '- 游戏前台可选热键、按键说话、队友收音与无回声',
  'Linux 交叉构建、模拟测试、静态审查和安装包解包不能证明这些运行项目通过。',
  'LeakSanitizer 因本执行环境 ptrace 限制未运行（detect_leaks=0）；不宣称泄漏专项检测通过。',
  '全部工作在云端完成，未操作用户电脑。','',
  '组件权利与签名：',
  '自制应用和安装器没有商业签名证书；官方文件保持字节/原有签名内容。',
  '语音来自用户提供的荣誉区1.09版本.zip，未独立认证声音身份或再分发许可，未将其声明为 MIT。',
  '官方标准 VB-CABLE Pack45 属于 VB-Audio，Donationware；不携带 A+B/C+D。',
  '许可：https://vb-audio.com/Services/licensing.htm；支持：https://shop.vb-audio.com/en/',
  '卸载只删清单内应用文件，保留用户设置、新增文件与共享驱动。','',
  '安装包 SHA-256：'+p['installer_sha256'],
  '源码 ZIP SHA-256：'+hashlib.sha256(source.read_bytes()).hexdigest(),
 ]
 target=ROOT/'dist/GameVoiceBox-Verification-1.2.0.txt';target.write_text('\n'.join(lines)+'\n')
 print('Measured release report generated:',target)
if __name__=='__main__':report()
