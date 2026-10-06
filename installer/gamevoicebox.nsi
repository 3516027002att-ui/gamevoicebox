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
!define APP_KEY "Software\GameVoiceBox"
!define APP_MUTEX "Local\GameVoiceBoxSingleInstance"
!define APP_EXE "GameVoiceBox.exe"
!define APP_FOLDER "GameVoiceBox"
!define APP_LABEL "开麦匣 GameVoiceBox"
!define OTHER_APP_KEY "Software\RongyuSoundboardNative"
!define OTHER_APP_EXE "Rongyu-Soundboard-Native.exe"
!define OTHER_APP_LABEL "荣誉音板原生版"
!include "${PROJECT_DIR}/installer/install-safety.nsh"
Name "开麦匣 GameVoiceBox"
OutFile "${PROJECT_DIR}/dist/GameVoiceBox-Setup-1.2.1-x64.exe"
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
  StrCpy $AppMutexHandle 0
  ReadRegStr $RegisteredInstallLocation HKCU "${APP_KEY}" "InstallLocation"
  ${If} $RegisteredInstallLocation != ""
    Push $RegisteredInstallLocation
    Call NormalizeInstallPath
    Pop $RegisteredInstallLocation
  ${EndIf}
  ReadRegStr $OtherRegisteredInstallLocation HKCU "${OTHER_APP_KEY}" "InstallLocation"
  ${If} $OtherRegisteredInstallLocation != ""
    Push $OtherRegisteredInstallLocation
    Call NormalizeInstallPath
    Pop $OtherRegisteredInstallLocation
  ${EndIf}
  !insertmacro customInit
FunctionEnd
Function DirectoryLeave
  Push $INSTDIR
  Call NormalizeInstallPath
  Pop $INSTDIR
  Call RejectDifferentProductDirectory
  # A registered custom path or an existing own executable identifies an upgrade.
  # Do not infer product ownership from a folder name alone.
  ${If} $RegisteredInstallLocation != ""
  ${AndIf} $INSTDIR == $RegisteredInstallLocation
    Return
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\${APP_EXE}"
    Return
  ${EndIf}
  # Fresh installations keep a dedicated folder even when the user selects a root.
  ${GetFileName} "$INSTDIR" $0
  ${If} $0 != "${APP_FOLDER}"
    # Check before concatenating: NSIS StrCpy otherwise silently truncates.
    StrLen $0 $INSTDIR
    StrLen $1 "${APP_FOLDER}"
    IntOp $0 $0 + $1
    IntOp $0 $0 + 1
    ${If} $0 >= ${NSIS_MAX_STRLEN}
      MessageBox MB_OK|MB_ICONSTOP "安装目录加上产品文件夹后路径过长。请重新选择较短的目录后重试；现有文件没有被修改。"
      SetErrorLevel 1
      Abort
    ${EndIf}
    StrCpy $INSTDIR "$INSTDIR\${APP_FOLDER}"
  ${EndIf}
  Call RejectDifferentProductDirectory
FunctionEnd
Function .onGUIEnd
  Call ReleaseAppMutex
FunctionEnd
Section "开麦匣 GameVoiceBox" SEC_APP
  SetShellVarContext current
  # Repeat path validation for silent /D= installs as well as the directory page.
  Call DirectoryLeave
  Call AcquireAppMutex
  ClearErrors
  SetOutPath "$INSTDIR"
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONSTOP "无法创建或打开安装目录。请检查目录权限与磁盘空间后重试。"
    SetErrorLevel 1
    Abort
  ${EndIf}
  ClearErrors
  File /r "${PROJECT_DIR}/build/payload/*"
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONSTOP "音板文件安装失败。请关闭正在运行的开麦匣，检查磁盘空间与目录权限后重试。"
    SetErrorLevel 1
    Abort
  ${EndIf}
  ClearErrors
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONSTOP "卸载程序写入失败。请检查目录权限与磁盘空间后重试安装。"
    Abort
  ${EndIf}
  Call CreateRequiredShortcuts
  ClearErrors
  WriteRegStr HKCU "Software\GameVoiceBox" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "DisplayName" "开麦匣 GameVoiceBox 1.2.1"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "DisplayVersion" "1.2.1"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "Publisher" "Personal-use build"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GameVoiceBox" "NoRepair" 1
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONSTOP "安装记录写入失败。请检查当前用户的注册表权限后重新运行安装器。"
    SetErrorLevel 1
    Abort
  ${EndIf}
  !insertmacro customInstall
SectionEnd
Function un.onInit
  SetShellVarContext current
  StrCpy $AppMutexHandle 0
FunctionEnd
Function un.onGUIEnd
  Call un.ReleaseAppMutex
FunctionEnd
Section "Uninstall"
  SetShellVarContext current
  Call un.AcquireAppMutex
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
