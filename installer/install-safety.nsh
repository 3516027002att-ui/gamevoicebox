# Static source tests cover these boundaries; runtime verification needs Windows.
# Install/uninstall share the application's mutex name. Retaining its handle blocks
# both another installer and an application relaunch until the process exits.
Var AppMutexHandle
Var RegisteredInstallLocation
Var OtherRegisteredInstallLocation

!macro AppMutexFunctions PREFIX
Function ${PREFIX}AcquireAppMutex
acquire_retry:
  System::Call 'kernel32::SetLastError(i 0)'
  System::Call 'kernel32::CreateMutexW(p 0, i 0, w "${APP_MUTEX}") p .r0 ?e'
  Pop $1
  ${If} $0 == 0
    MessageBox MB_OK|MB_ICONSTOP "无法检查${APP_LABEL}的运行状态。为避免损坏现有安装，安装/卸载已停止。请检查权限后重试。"
    SetErrorLevel 1
    Abort
  ${EndIf}
  ${If} $1 == 183
    System::Call 'kernel32::CloseHandle(p r0)'
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "请先退出${APP_LABEL}（包括系统托盘中的程序），并关闭其他安装/卸载窗口，再点“重试”。安装器不会强制结束程序。" /SD IDCANCEL IDRETRY acquire_retry
    SetErrorLevel 1
    Abort
  ${EndIf}
  StrCpy $AppMutexHandle $0
FunctionEnd
Function ${PREFIX}ReleaseAppMutex
  ${If} $AppMutexHandle != 0
    System::Call 'kernel32::CloseHandle(p $AppMutexHandle)'
    StrCpy $AppMutexHandle 0
  ${EndIf}
FunctionEnd
!macroend
!insertmacro AppMutexFunctions ""
!insertmacro AppMutexFunctions "un."

# Win32 GetFullPathNameW resolves relative/dot components without requiring the
# selected directory to exist. NSIS GetFullPathName is unsafe for fresh paths:
# it empties the output if a path portion is absent. Reject API errors/truncation.
# NSIS comparisons are case-insensitive; trim trailing slashes except drive roots.
Function NormalizeInstallPath
  Exch $0
  Push $1
  Push $2
  ${If} $0 == ""
    Goto normalize_failed
  ${EndIf}
  StrCpy $1 ""
  StrCpy $2 0
  System::Call 'kernel32::GetFullPathNameW(w r0, i ${NSIS_MAX_STRLEN}, w .r1, p 0) i .r2'
  ${If} $2 == 0
  ${OrIf} $2 >= ${NSIS_MAX_STRLEN}
  ${OrIf} $1 == ""
    Goto normalize_failed
  ${EndIf}
  StrCpy $0 $1
  ${Do}
    StrLen $1 $0
    ${If} $1 <= 3
      ${ExitDo}
    ${EndIf}
    StrCpy $2 $0 1 -1
    ${If} $2 != "\"
      ${ExitDo}
    ${EndIf}
    StrCpy $0 $0 -1
  ${Loop}
  Pop $2
  Pop $1
  Exch $0
  Return
normalize_failed:
  # Restore the caller's registers and consume its path before stopping. No
  # caller may continue to SetOutPath/File with an empty or truncated directory.
  Pop $2
  Pop $1
  Pop $0
  MessageBox MB_OK|MB_ICONSTOP "安装目录为空、无效或路径过长，无法安全使用。请重新选择较短的完整目录路径后重试；现有文件没有被修改。"
  SetErrorLevel 1
  Abort
FunctionEnd

# Different products must never overwrite the same Uninstall.exe. This release
# intentionally does not migrate the three-clip app to the full-pack product.
Function RejectDifferentProductDirectory
  ${If} $OtherRegisteredInstallLocation != ""
  ${AndIf} $INSTDIR == $OtherRegisteredInstallLocation
    MessageBox MB_OK|MB_ICONSTOP "此目录属于${OTHER_APP_LABEL}，不能作为${APP_LABEL}的覆盖安装目录。请为两个产品选择各自独立的文件夹；升级旧版时请使用同一产品的安装器。"
    Abort
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\${OTHER_APP_EXE}"
    MessageBox MB_OK|MB_ICONSTOP "此目录已有${OTHER_APP_LABEL}，不能共用卸载程序。请为${APP_LABEL}选择独立的文件夹；升级旧版时请使用同一产品的安装器。"
    Abort
  ${EndIf}
FunctionEnd

Function ShortcutFailure
  MessageBox MB_OK|MB_ICONSTOP "${APP_LABEL}的快捷方式创建失败。请检查桌面及开始菜单的目录权限，再重新运行安装器；同路径重新安装会修复快捷方式。个人设置不会被删除。"
  SetErrorLevel 1
  Abort
FunctionEnd
!macro CheckedShortcut LINK TARGET
  ClearErrors
  CreateShortcut "${LINK}" "${TARGET}" "" "$INSTDIR\${APP_EXE}" 0
  ${If} ${Errors}
    Call ShortcutFailure
  ${EndIf}
  ${IfNot} ${FileExists} "${LINK}"
    Call ShortcutFailure
  ${EndIf}
!macroend
Function CreateRequiredShortcuts
  # NSIS uses $OUTDIR as the shortcut's working directory, including on repair.
  ClearErrors
  SetOutPath "$INSTDIR"
  ${If} ${Errors}
    Call ShortcutFailure
  ${EndIf}
  ClearErrors
  CreateDirectory "$SMPROGRAMS\${APP_LABEL}"
  ${If} ${Errors}
    Call ShortcutFailure
  ${EndIf}
  !insertmacro CheckedShortcut "$DESKTOP\${APP_LABEL}.lnk" "$INSTDIR\${APP_EXE}"
  !insertmacro CheckedShortcut "$SMPROGRAMS\${APP_LABEL}\${APP_LABEL}.lnk" "$INSTDIR\${APP_EXE}"
  !insertmacro CheckedShortcut "$SMPROGRAMS\${APP_LABEL}\中文使用教程.lnk" "$INSTDIR\docs\中文使用教程.html"
  !insertmacro CheckedShortcut "$SMPROGRAMS\${APP_LABEL}\卸载${APP_LABEL}.lnk" "$INSTDIR\Uninstall.exe"
FunctionEnd
