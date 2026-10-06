"""Production portable diagnostics and thin static Win32 wiring contracts."""
import os
import pathlib
import re
import subprocess
import tempfile
import unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class HotkeyDiagnosticsTest(unittest.TestCase):
    def test_portable_production_contract(self):
        self.assertTrue((ROOT/'src/windows/hotkey_diagnostics.c').exists(), 'production diagnostic adapter is missing')
        with tempfile.TemporaryDirectory() as directory:
            binary=pathlib.Path(directory)/'test_hotkey_diagnostics'
            subprocess.run([os.environ.get('CC','gcc'),'-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(ROOT/'tests/test_hotkey_diagnostics.c'),str(ROOT/'src/windows/hotkey_diagnostics.c'),'-o',str(binary)],check=True)
            env=os.environ.copy(); env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1'; env['UBSAN_OPTIONS']='halt_on_error=1'
            subprocess.run([str(binary)],check=True,env=env)
    def test_responsive_status_geometry(self):
        with tempfile.TemporaryDirectory() as directory:
            binary=pathlib.Path(directory)/'test_hotkey_diagnostics_layout'
            subprocess.run([os.environ.get('CC','gcc'),'-std=c11','-Wall','-Wextra','-Werror',str(ROOT/'tests/test_hotkey_diagnostics_layout.c'),str(ROOT/'src/windows/ui_contract.c'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
    def test_queue_hook_precedes_dialog_filter_without_routing_change(self):
        source=(ROOT/'src/windows/main.c').read_text()
        loop=source.split('while(GetMessageW',1)[1]
        self.assertIn('if(msg.message==WM_HOTKEY && msg.hwnd==window) ui_hotkey_queue_received(window,(int)msg.wParam);',loop)
        self.assertLess(loop.index('ui_hotkey_queue_received('),loop.index('IsDialogMessageW('))
        self.assertIn('if(!IsDialogMessageW(root,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }',loop)
        self.assertIn('void ui_hotkey_queue_received(HWND hwnd,int id);',(ROOT/'src/windows/ui.h').read_text())
    def test_wndproc_tracks_controller_acceptance_without_claiming_audio(self):
        source=(ROOT/'src/windows/ui.c').read_text()
        handler=source.split('case WM_HOTKEY:',1)[1].split('case WM_KEYDOWN:',1)[0]
        self.assertIn('hotkey_diagnostics_handled(',handler)
        self.assertIn('accepted=dispatch(u,&cmd);',handler)
        self.assertIn('hotkey_diagnostics_result(',handler)
        self.assertLess(handler.index('hotkey_diagnostics_handled('),handler.index('dispatch(u,&cmd)'))
        self.assertLess(handler.index('dispatch(u,&cmd)'),handler.index('hotkey_diagnostics_result('))
        self.assertIn('u->controller.state.last_error',handler)
        self.assertIn('ui_apply_state(',handler)
    def test_event_is_independent_of_transient_status_and_only_registered_bindings(self):
        source=(ROOT/'src/windows/ui.c').read_text()
        self.assertIn('HotkeyDiagnostics hotkey_diagnostics;',source)
        self.assertIn('hotkey_diagnostics_format(&u->hotkey_diagnostics,',source)
        helper=source.split('static const Hotkey *diagnostic_key(',1)[1].split('void ui_hotkey_queue_received(',1)[0]
        self.assertIn('u->hotkeys.registered[',helper)
        self.assertIn('HOTKEY_STOP_ID',helper)
        queue=source.split('void ui_hotkey_queue_received(',1)[1].split('static bool device_present(',1)[0]
        self.assertIn('GetLocalTime(',queue)
        self.assertIn('hotkey_diagnostics_queue(',queue)
        self.assertNotIn('hotkey_diagnostics_reset',source)
        all_diagnostic=(ROOT/'src/windows/hotkey_diagnostics.c').read_text()
        self.assertNotRegex(all_diagnostic,r'\b(fopen|fwrite|CreateFile|WriteFile|send|upload)\s*\(')
        self.assertNotIn('SetWindowsHookEx',source)
        self.assertIn('RegisterHotKey(u->hwnd,id,key->modifiers|MOD_NOREPEAT,key->key)',source)
    def test_guide_and_checklist_explain_observation_without_diagnosis(self):
        source=(ROOT/'docs/中文使用教程.html').read_text()
        for text in ('id="hotkey-diagnostics"','热键诊断','接收','处理','提交','不代表已出声','不会保存日志或上传','游戏前台','截图'):
            self.assertIn(text,source)
        checklist=(ROOT/'docs/Windows验收清单.txt').read_text()
        self.assertIn('接收 / 处理 / 提交 / 拒绝',checklist)
        self.assertIn('重启后归零',checklist)
    def test_status_has_room_and_stays_in_scroll_extent(self):
        if 'ui_layout_compute' in (ROOT/'src/windows/ui_contract.h').read_text():
            source=(ROOT/'src/windows/ui_contract.c').read_text()
            self.assertIn('status_height=width>=700?240:320',source)
            self.assertIn('available,status_height',source)
            self.assertIn('content_height=y+status_height+10',source)
        else:
            source=(ROOT/'src/windows/ui.c').read_text()
            self.assertIn('706,width-2*margin,240',source)
            self.assertIn('0,960,0,0,0',source)
            self.assertIn('u->scroll>960-height',source)
if __name__=='__main__':unittest.main()
