import unittest, pathlib, tempfile, importlib.util
ROOT=pathlib.Path(__file__).resolve().parents[1]
class PackageTest(unittest.TestCase):
 def test_installer_is_available(self):
  self.assertTrue((ROOT/'dist/GameVoiceBox-Setup-1.2.0-x64.exe').is_file(),'actual installer has not been generated')
 def test_safe_optional_driver_and_manifest_uninstall(self):
  script=(ROOT/'installer/gamevoicebox.nsi').read_text()
  helper=(ROOT/'installer/driver-page.nsh').read_text()
  self.assertIn('RequestExecutionLevel user',script)
  self.assertIn('${AtLeastWin10}',script)
  self.assertIn('WriteUninstaller',script)
  self.assertIn('${NSD_Uncheck}',helper)
  self.assertIn('ExecShell "runas"',helper)
  self.assertIn('Donationware',helper)
  self.assertIn('licensing.htm',helper)
  self.assertIn('VBCABLE_Setup_x64.exe',helper)
  self.assertNotIn('RMDir /r',script)
  self.assertNotIn('Reboot',script)
  self.assertNotIn('TrustedPublisher',script+helper)
  self.assertNotRegex(helper,r'(?i)ExecShell[^\n]*\"/(?:S|silent)\"')
 def test_user_added_files_are_not_deleted(self):
  spec=importlib.util.spec_from_file_location('uninstall',ROOT/'scripts/generate_uninstall.py')
  module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
  with tempfile.TemporaryDirectory() as t:
   p=pathlib.Path(t);(p/'assets').mkdir();(p/'assets/cao.wav').write_bytes(b'pcm');(p/'app.exe').write_bytes(b'pe')
   files,dirs=module.uninstall_lines(p)
   self.assertEqual(sum(x.startswith('Delete ') for x in files),2)
   self.assertEqual(sum(x.startswith('IfFileExists ') for x in files),2)
   self.assertFalse(any('user-added.txt' in x or '/r' in x for x in files+dirs))
if __name__=='__main__':unittest.main()
