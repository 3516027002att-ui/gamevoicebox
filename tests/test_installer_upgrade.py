"""Static NSIS upgrade/shortcut contracts; these do not execute Windows installers.

Deleting a preflight call, path-ownership check, shortcut error branch, or explicit
icon/working-directory setting must make these source-boundary checks fail.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'installer/gamevoicebox.nsi'
HELPER = ROOT / 'installer/install-safety.nsh'


def function(source, name):
    match = re.search(r'^Function ' + re.escape(name) + r'\n(.*?)^FunctionEnd', source, re.M | re.S)
    if match is None:
        return ''
    return re.sub(r'(?m)^\s*#.*\n', '', match.group(1))


class InstallerUpgradeStaticContract(unittest.TestCase):
    def setUp(self):
        self.script = SCRIPT.read_text()
        self.helper = HELPER.read_text() if HELPER.exists() else ''

    def test_install_guard_precedes_any_payload_write_including_silent_mode(self):
        section = self.script.split('Section "开麦匣 GameVoiceBox" SEC_APP', 1)[1].split('SectionEnd', 1)[0]
        self.assertIn('Call DirectoryLeave', section)
        self.assertIn('Call AcquireAppMutex', section)
        self.assertLess(section.index('Call DirectoryLeave'), section.index('Call AcquireAppMutex'))
        self.assertLess(section.index('Call AcquireAppMutex'), section.index('SetOutPath "$INSTDIR"'))
        self.assertLess(section.index('Call AcquireAppMutex'), section.index('File /r'))

    def test_uninstall_guard_precedes_delete_and_keeps_settings_and_user_files(self):
        section = self.script.split('Section "Uninstall"', 1)[1].split('SectionEnd', 1)[0]
        self.assertIn('Call un.AcquireAppMutex', section)
        self.assertLess(section.index('Call un.AcquireAppMutex'), section.index('uninstall-files.nsh'))
        self.assertNotIn('RMDir /r', self.script + self.helper)
        self.assertNotRegex(self.script + self.helper, r'(?m)^\s*(?:Delete|RMDir|SetOutPath).*\$(?:APPDATA|LOCALAPPDATA)')
        self.assertNotRegex(self.script + self.helper, r'(?i)taskkill|TerminateProcess|nsProcess::_KillProcess')

    def test_install_directory_and_registry_write_failures_are_reported(self):
        section = self.script.split('Section "开麦匣 GameVoiceBox" SEC_APP', 1)[1].split('SectionEnd', 1)[0]
        self.assertRegex(section, r'ClearErrors\s+SetOutPath "\$INSTDIR"\s+\$\{If\} \$\{Errors\}')
        metadata = section[section.index('WriteRegStr'):]
        self.assertIn('\"NoRepair\" 1', metadata)
        self.assertIn('${If} ${Errors}', metadata)
        self.assertIn('SetErrorLevel 1', metadata)

    def test_mutex_uses_exact_application_name_checks_errors_and_is_held_until_exit(self):
        self.assertIn('!define APP_MUTEX "Local\\GameVoiceBoxSingleInstance"', self.script)
        self.assertIn('CreateMutexW(p 0, i 0, w "${APP_MUTEX}") p .r0 ?e', self.helper)
        self.assertIn('Pop $1', self.helper)
        self.assertIn('${If} $0 == 0', self.helper)
        self.assertIn('${If} $1 == 183', self.helper)
        self.assertIn('CloseHandle(p r0)', self.helper)
        self.assertIn('StrCpy $AppMutexHandle $0', self.helper)
        self.assertIn('MB_RETRYCANCEL', self.helper)
        self.assertIn('/SD IDCANCEL IDRETRY acquire_retry', self.helper)
        self.assertIn('SetErrorLevel 1', self.helper)
        self.assertIn('Abort', self.helper)
        self.assertIn('Call ReleaseAppMutex', function(self.script, '.onGUIEnd'))
        self.assertIn('Call un.ReleaseAppMutex', function(self.script, 'un.onGUIEnd'))
        self.assertNotIn('Call ReleaseAppMutex', self.script.split('Section "开麦匣 GameVoiceBox" SEC_APP', 1)[1].split('SectionEnd', 1)[0])

    def test_registered_custom_directory_is_normalized_and_preserved_before_append(self):
        init = function(self.script, '.onInit')
        directory = function(self.script, 'DirectoryLeave')
        self.assertIn('ReadRegStr $RegisteredInstallLocation HKCU "${APP_KEY}" "InstallLocation"', init)
        self.assertIn('ReadRegStr $OtherRegisteredInstallLocation HKCU "${OTHER_APP_KEY}" "InstallLocation"', init)
        self.assertIn('Call NormalizeInstallPath', init)
        self.assertIn('Call NormalizeInstallPath', directory)
        self.assertIn('GetFullPathNameW(w r0, i ${NSIS_MAX_STRLEN}, w .r1, p 0) i .r2', self.helper)
        self.assertIn('StrCpy $2 $0 1 -1', self.helper)
        self.assertIn('StrCpy $0 $0 -1', self.helper)
        self.assertNotIn('StrCmpS', directory + self.helper)
        preserved = directory.index('$INSTDIR == $RegisteredInstallLocation')
        appended = directory.index('StrCpy $INSTDIR "$INSTDIR\\${APP_FOLDER}"')
        self.assertLess(preserved, appended)
        self.assertIn('Return', directory[preserved:appended])
        self.assertIn('${FileExists} "$INSTDIR\\${APP_EXE}"', directory)
        self.assertIn('${GetFileName} "$INSTDIR" $0', directory)

    def test_fresh_nonexistent_directory_uses_checked_nonexistence_safe_api(self):
        # NSIS GetFullPathName empties output if a path portion does not exist.
        # Win32 GetFullPathNameW canonicalizes fresh paths without accessing disk.
        # This is a static API/data-flow contract, not Windows runtime execution.
        normalize = function(self.helper, 'NormalizeInstallPath')
        self.assertNotRegex(normalize, r'(?m)^\s*GetFullPathName(?: /SHORT)? ')
        self.assertIn('GetFullPathNameW(w r0, i ${NSIS_MAX_STRLEN}, w .r1, p 0) i .r2', normalize)
        self.assertIn('${If} $0 == ""', normalize)
        self.assertRegex(normalize, r'StrCpy \$1 ""\s+StrCpy \$2 0\s+System::Call')
        self.assertIn('${If} $2 == 0', normalize)
        self.assertIn('${OrIf} $2 >= ${NSIS_MAX_STRLEN}', normalize)
        self.assertIn('${OrIf} $1 == ""', normalize)
        self.assertIn('Goto normalize_failed', normalize)
        self.assertIn('StrCpy $0 $1', normalize)
        self.assertLess(normalize.index('$2 >= ${NSIS_MAX_STRLEN}'), normalize.index('StrCpy $0 $1'))
        # One path in/out on success, with $0/$1/$2 restored; failure restores
        # all registers and consumes the input before aborting the callback.
        self.assertRegex(normalize, r'^\s*Exch \$0\s+Push \$1\s+Push \$2')
        self.assertRegex(normalize, r'Pop \$2\s+Pop \$1\s+Exch \$0\s+Return')
        failure = normalize.split('normalize_failed:', 1)[1]
        self.assertRegex(failure, r'^\s*Pop \$2\s+Pop \$1\s+Pop \$0')
        self.assertIn('MessageBox MB_OK|MB_ICONSTOP', failure)
        self.assertIn('SetErrorLevel 1', failure)
        self.assertIn('Abort', failure)

    def test_fresh_subfolder_append_rejects_nsis_string_truncation_before_copy(self):
        directory = function(self.script, 'DirectoryLeave')
        append = directory.index('StrCpy $INSTDIR "$INSTDIR\\${APP_FOLDER}"')
        prefix = directory[:append]
        self.assertIn('StrLen $0 $INSTDIR', prefix)
        self.assertIn('StrLen $1 "${APP_FOLDER}"', prefix)
        self.assertIn('IntOp $0 $0 + $1', prefix)
        self.assertIn('IntOp $0 $0 + 1', prefix)
        self.assertIn('${If} $0 >= ${NSIS_MAX_STRLEN}', prefix)
        failure = prefix.split('${If} $0 >= ${NSIS_MAX_STRLEN}', 1)[1]
        self.assertIn('MessageBox MB_OK|MB_ICONSTOP', failure)
        self.assertIn('SetErrorLevel 1', failure)
        self.assertIn('Abort', failure)

    def test_cross_product_directory_is_rejected_before_and_after_normalization(self):
        directory = function(self.script, 'DirectoryLeave')
        self.assertIn('!define OTHER_APP_EXE "Rongyu-Soundboard-Native.exe"', self.script)
        self.assertIn('Call RejectDifferentProductDirectory', directory)
        self.assertGreaterEqual(directory.count('Call RejectDifferentProductDirectory'), 2)
        other = function(self.helper, 'RejectDifferentProductDirectory')
        self.assertIn('${FileExists} "$INSTDIR\\${OTHER_APP_EXE}"', other)
        self.assertIn('$INSTDIR == $OtherRegisteredInstallLocation', other)
        self.assertIn('MessageBox MB_OK|MB_ICONSTOP', other)
        self.assertIn('Abort', other)
        self.assertNotIn('DeleteRegKey', other)
        self.assertNotIn('Delete ', other)

    def test_shortcuts_are_repaired_with_explicit_icon_working_directory_and_hard_failures(self):
        section = self.script.split('Section "开麦匣 GameVoiceBox" SEC_APP', 1)[1].split('SectionEnd', 1)[0]
        self.assertIn('Call CreateRequiredShortcuts', section)
        shortcuts = function(self.helper, 'CreateRequiredShortcuts')
        self.assertRegex(shortcuts, r'ClearErrors\s+SetOutPath "\$INSTDIR"\s+\$\{If\} \$\{Errors\}\s+Call ShortcutFailure')
        self.assertIn('!insertmacro CheckedShortcut "$DESKTOP\\${APP_LABEL}.lnk" "$INSTDIR\\${APP_EXE}"', shortcuts)
        self.assertIn('CreateShortcut "${LINK}" "${TARGET}" "" "$INSTDIR\\${APP_EXE}" 0', self.helper)
        self.assertIn('ClearErrors', self.helper)
        self.assertIn('${If} ${Errors}', self.helper)
        self.assertIn('${IfNot} ${FileExists} "${LINK}"', self.helper)
        self.assertIn('Call ShortcutFailure', self.helper)
        failure = function(self.helper, 'ShortcutFailure')
        self.assertIn('MessageBox MB_OK|MB_ICONSTOP', failure)
        self.assertIn('SetErrorLevel 1', failure)
        self.assertIn('Abort', failure)
        self.assertIn('重新运行', failure)
        self.assertNotIn('IfFileExists "$DESKTOP', shortcuts)
        self.assertIn('"DisplayVersion" "1.2.1"', self.script)
        self.assertIn('Setup-1.2.1-x64.exe', self.script)


if __name__ == '__main__':
    unittest.main()
