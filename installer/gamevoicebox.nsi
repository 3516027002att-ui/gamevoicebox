Unicode true
SetCompressor /SOLID lzma
SetCompressorDictSize 8
# Native NSIS installer; WriteUninstaller executes only on Windows at install time.
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!include "FileFunc.nsh"
!include "${PROJECT_DIR}/installer/driver-page.nsh"
Name "开麦匣 GameVoiceBox"
OutFile "${PROJECT_DIR}/dist/GameVoiceBox-Setup-1.2.0-x64.exe"
InstallDir "$LOCALAPPDATA\Programs\GameVoiceBox"
InstallDirRegKey HKCU "Software\GameVoiceBox" "InstallLocation"
RequestExecutionLevel user
ShowInstDetails show
ShowUninstDetails show
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE DirectoryLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro customPageAfterChangeDir
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "SimpChinese"
Function .onInit
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_OK|MB_ICONSTOP "此版本仅支持 Windows 10 / 11 x64。"
    Abort
  ${EndIf}
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "此版本仅支持 Windows 10 / 11 x64。"
    Abort
  ${EndIf}
  SetShellVarContext current
  !insertmacro customInit
FunctionEnd
Function DirectoryLeave
  # Keep a dedicated app folder so installation never dumps into a shared/root directory.
  ${GetFileName} "$INSTDIR" $0
  ${If} $0 != "GameVoiceBox"
    StrCpy $INSTDIR "$INSTDIR\GameVoiceBox"
  ${EndIf}
FunctionEnd
Section "开麦匣 GameVoiceBox" SEC_APP
  SetShellVarContext current
  SetOutPath "$INSTDIR"
  ClearErrors
  File /r "${PROJECT_DIR}/build/payload/*"
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONSTOP "音板文件安装失败。请关闭正在运行的开麦匣，检查磁盘空间与目录权限后重试。"
    Abort
  ${EndIf}
  ClearErrors
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONSTOP "卸载程序写入失败。请检查目录权限与磁盘空间后重试安装。"
    Abort
  ${EndIf}
  WriteRegStr HKCU "Software\GameVoiceBox" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "DisplayName" "开麦匣 GameVoiceBox 1.2.0"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "DisplayVersion" "1.2.0"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "Publisher" "Personal-use build"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "NoRepair" 1
  CreateDirectory "$SMPROGRAMS\开麦匣 GameVoiceBox"
  CreateShortcut "$SMPROGRAMS\开麦匣 GameVoiceBox\开麦匣 GameVoiceBox.lnk" "$INSTDIR\GameVoiceBox.exe"
  CreateShortcut "$SMPROGRAMS\开麦匣 GameVoiceBox\中文使用教程.lnk" "$INSTDIR\docs\中文使用教程.html"
  CreateShortcut "$SMPROGRAMS\开麦匣 GameVoiceBox\卸载开麦匣 GameVoiceBox.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\开麦匣 GameVoiceBox.lnk" "$INSTDIR\GameVoiceBox.exe"
  !insertmacro customInstall
SectionEnd
Function un.onInit
  SetShellVarContext current
FunctionEnd
Section "Uninstall"
  SetShellVarContext current
  # Delete only our enumerated payload files, never recursively delete unrelated user files.
  ClearErrors
  !include "${PROJECT_DIR}/build/uninstall-files.nsh"
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONEXCLAMATION "有音板文件仍被占用。请关闭开麦匣，再次运行卸载程序。VB-CABLE 驱动不会被移除。"
    Abort
  ${EndIf}
  !include "${PROJECT_DIR}/build/uninstall-dirs.nsh"
  Delete "$DESKTOP\开麦匣 GameVoiceBox.lnk"
  Delete "$SMPROGRAMS\开麦匣 GameVoiceBox\开麦匣 GameVoiceBox.lnk"
  Delete "$SMPROGRAMS\开麦匣 GameVoiceBox\中文使用教程.lnk"
  Delete "$SMPROGRAMS\开麦匣 GameVoiceBox\卸载开麦匣 GameVoiceBox.lnk"
  RMDir "$SMPROGRAMS\开麦匣 GameVoiceBox"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox"
  DeleteRegKey HKCU "Software\GameVoiceBox"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
SectionEnd
