"""Portable lifecycle and static adapter checks, not native Windows execution."""
import os,pathlib,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class TrayTest(unittest.TestCase):
 def test_production_lifecycle(self):
  core=ROOT/'src/windows/tray_contract.c'
  self.assertTrue(core.exists(),'production tray lifecycle is missing')
  with tempfile.TemporaryDirectory() as t:
   exe=pathlib.Path(t)/'test_tray'
   subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(ROOT/'tests/test_tray_contract.c'),str(core),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
 def test_actual_native_adapter_with_platform_substitutes(self):
  with tempfile.TemporaryDirectory() as t:
   exe=pathlib.Path(t)/'test_tray_adapter'
   subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(ROOT/'tests/win32_tray_stubs'),str(ROOT/'tests/test_tray_adapter.c'),str(ROOT/'src/windows/tray.c'),str(ROOT/'src/windows/tray_contract.c'),'-o',str(exe)],check=True)
   result=subprocess.run([str(exe)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   print(result.stdout,end='')
 def test_native_adapter_and_ui_wiring(self):
  path=ROOT/'src/windows/tray.c'
  self.assertTrue(path.exists(),'native notification icon adapter is missing')
  source=path.read_text();ui=(ROOT/'src/windows/ui.c').read_text();main=(ROOT/'src/windows/main.c').read_text()
  for item in ('Shell_NotifyIconW(NIM_ADD','NIM_SETVERSION','NOTIFYICON_VERSION_4','NIM_DELETE','TaskbarCreated','WM_LBUTTONDBLCLK','NIN_KEYSELECT','WM_CONTEXTMENU','TrackPopupMenuEx','WM_CLOSE'):
   self.assertIn(item,source)
  self.assertIn('tray_window_message(',ui)
  self.assertIn('!u->closing || msg==u->tray.taskbar_created',ui)
  self.assertIn('SIZE_MINIMIZED',ui)
  self.assertIn('tray_window_minimize(',ui)
  self.assertIn('tray_window_remove(',ui)
  self.assertIn('WM_APP_RESTORE',main)
  self.assertIn('case WM_APP_RESTORE:',ui)
  self.assertIn('101 ICON', (ROOT/'src/windows/app.rc').read_text())
if __name__=='__main__':unittest.main()
