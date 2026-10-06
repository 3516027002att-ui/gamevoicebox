"""Static platform checks only: not a replacement for Windows execution."""
import pathlib
import unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
class WindowsSourceContract(unittest.TestCase):
    def test_dpi_optional_apis_resolved_dynamically(self):
        source = (ROOT/'src/windows/main.c').read_text() + (ROOT/'src/windows/ui.c').read_text()
        self.assertNotIn('SetProcessDpiAwarenessContext(', source)
        self.assertNotIn('GetDpiForWindow(', source)
        self.assertNotIn('GetDpiForSystem(', source)
    def test_audio_stopped_before_close_warning(self):
        source = (ROOT/'src/windows/ui.c').read_text().split('case WM_CLOSE:')[-1]
        self.assertIn('audio_worker_shutdown(', source)
        self.assertLess(source.index('audio_worker_shutdown('), source.index('MessageBoxW('))
    def test_fixed_driver_destination_and_no_parameters(self):
        source = (ROOT/'src/windows/ui.c').read_text()
        self.assertIn('vendor\\\\official\\\\VBCABLE_Setup_x64.exe', source)
        self.assertIn('info.lpVerb=L"runas"', source)
        self.assertNotIn('info.lpParameters=', source)
        self.assertIn('ERROR_CANCELLED', source)
        self.assertNotIn('driverReady', source)
    def test_explicit_hotkey_save_failure_requests_visible_native_error(self):
        source = (ROOT/'src/windows/ui.c').read_text()
        # Source adapter contract only; actual MessageBox behavior needs Windows.
        self.assertIn('static void hotkey_save_error(', source)
        helper = source.split('static void hotkey_save_error(', 1)[1].split('static void save_hotkeys(', 1)[0]
        self.assertIn('show_error(', helper)
        self.assertIn('MessageBoxW(u->hwnd,', helper)
        save = source.split('static void save_hotkeys(', 1)[1].split('static bool cable_available(', 1)[0]
        self.assertEqual(save.count('hotkey_save_error('), 2)
    def test_hotkey_save_success_label_and_edit_reset_are_wired(self):
        source = (ROOT/'src/windows/ui.c').read_text()
        save = source.split('static void save_hotkeys(', 1)[1].split('static bool cable_available(', 1)[0]
        self.assertIn('SetWindowTextW(u->controls[UI_SAVE_HOTKEYS],L"全局快捷键已保存")', save)
        commands = source.split('case WM_COMMAND:', 1)[1].split('case WM_HSCROLL:', 1)[0]
        self.assertIn('HIWORD(wp)==EN_CHANGE', commands)
        self.assertIn('LOWORD(wp)>=UI_HOTKEY_0', commands)
        self.assertIn('LOWORD(wp)<=UI_HOTKEY_LAST', commands)
        self.assertIn('SetWindowTextW(u->controls[UI_SAVE_HOTKEYS],L"保存全局快捷键")', commands)
    def test_native_layout_consumes_responsive_geometry(self):
        source = (ROOT/'src/windows/ui.c').read_text()
        layout = source.split('static void layout(', 1)[1].split('static void fonts(', 1)[0]
        self.assertIn('ui_layout_compute(', layout)
        self.assertNotIn('columns=width>=1100?4:3', layout)
        self.assertNotIn('ptMinTrackSize.x=px(u,740)', source)
    def test_minimum_window_size_is_capped_to_actual_monitor_work_area(self):
        source = (ROOT/'src/windows/ui.c').read_text()
        limits = source.split('case WM_GETMINMAXINFO:', 1)[1].split('case WM_DPICHANGED:', 1)[0]
        self.assertIn('GetMonitorInfoW(', limits)
        self.assertIn('monitor.rcWork.right-monitor.rcWork.left', limits)
        self.assertIn('monitor.rcWork.bottom-monitor.rcWork.top', limits)
    def test_clip_buttons_can_wrap_long_chinese_titles(self):
        source = (ROOT/'src/windows/ui.c').read_text()
        create = source.split('static void controls_create(', 1)[1].split('static int px(', 1)[0]
        self.assertIn('BS_PUSHBUTTON|BS_MULTILINE|WS_TABSTOP', create)
if __name__ == '__main__':
    unittest.main()
