!ifndef GAMEVOICEBOX_DRIVER_PAGE_INCLUDED
!define GAMEVOICEBOX_DRIVER_PAGE_INCLUDED
!include "MUI2.nsh"
!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!ifndef BUILD_UNINSTALLER
Var CableInstallCheckbox
Var CableRequested
Var CableAlreadyPresent
Var CableDialog
Var CableNotice
!macro customInit
  StrCpy $CableRequested "0"
  StrCpy $CableAlreadyPresent "0"
!macroend
!macro customPageAfterChangeDir
  Page custom CablePageCreate CablePageLeave
!macroend
Function CablePageCreate
  ${If} ${Silent}
    Abort
  ${EndIf}
  nsDialogs::Create 1018
  Pop $CableDialog
  ${If} $CableDialog == error
    Abort
  ${EndIf}
  !insertmacro MUI_HEADER_TEXT "官方虚拟声卡（可选）" "开麦匣需要 CABLE Input / CABLE Output 才能发送给游戏"
  ${NSD_CreateLabel} 0 0 100% 60u "本安装包内含未经修改的标准 VB-CABLE Pack45，作者 VB-Audio。$\r$\nVB-CABLE 是 Donationware；觉得有用时欢迎向作者捐赠/支付许可。$\r$\n官网：https://vb-audio.com/Cable/index.htm$\r$\n支持与捐赠：https://shop.vb-audio.com/en/$\r$\n许可：https://vb-audio.com/Services/licensing.htm"
  Pop $CableNotice
  ${NSD_CreateCheckbox} 0 68u 100% 20u "启动官方 VB-CABLE 安装器（需要管理员确认）"
  Pop $CableInstallCheckbox
  ${NSD_Uncheck} $CableInstallCheckbox
  ClearErrors
  ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Services\VBAudioVACMME" "ImagePath"
  ${If} $0 != ""
    StrCpy $CableAlreadyPresent "1"
    EnableWindow $CableInstallCheckbox 0
    ${NSD_CreateLabel} 0 92u 100% 30u "发现已有 VB-CABLE 驱动记录，已跳过驱动安装以避免进入移除流程。$\r$\n启动音板后刷新设备，确认 CABLE Input 是否实际可用。"
    Pop $0
  ${Else}
    ${NSD_CreateLabel} 0 92u 100% 55u "如已在 Windows 声音设置看到 CABLE Input / Output，请保持未勾选。$\r$\n需要安装时勾选：请在官方界面点 Install Driver；如果显示 Remove Driver，请关闭。安装后可能需要自行重启，向导不会强制重启。"
    Pop $0
  ${EndIf}
  ${NSD_CreateLink} 0 151u 100% 12u "查看 VB-Audio 许可、支持与捐赠说明"
  Pop $0
  ${NSD_OnClick} $0 CableOpenLicense
  nsDialogs::Show
FunctionEnd
Function CableOpenLicense
  Pop $0
  ExecShell "open" "https://vb-audio.com/Services/licensing.htm"
FunctionEnd
Function CablePageLeave
  ${NSD_GetState} $CableInstallCheckbox $CableRequested
FunctionEnd
!macro customInstall
  ${IfNot} ${Silent}
  ${AndIf} $CableRequested == ${BST_CHECKED}
  ${AndIf} $CableAlreadyPresent != "1"
    ClearErrors
    # Fixed official interactive executable; no guessed silent switches.
    SetOutPath "$INSTDIR\vendor\official"
    ExecShell "runas" "$INSTDIR\vendor\official\VBCABLE_Setup_x64.exe" "" SW_SHOWNORMAL
    ${If} ${Errors}
      MessageBox MB_OK|MB_ICONEXCLAMATION "管理员确认被取消或官方安装器未能启动。音板已安装；稍后可在音板内再次打开官方安装器。"
    ${Else}
      MessageBox MB_OK|MB_ICONINFORMATION "官方安装器已启动。请在其窗口点 Install Driver，并按官方提示自行重启。$\r$\n如果显示 Remove Driver，请关闭窗口。向导不能据此确认驱动安装成功；启动音板后刷新设备再检查。"
    ${EndIf}
  ${EndIf}
!macroend

!endif

!endif
