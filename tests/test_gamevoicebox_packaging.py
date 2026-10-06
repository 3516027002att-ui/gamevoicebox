"""Regression checks for the full-pack successor's build and release boundaries."""
import importlib.util,inspect,json,pathlib,tempfile,unittest
from unittest.mock import patch
ROOT=pathlib.Path(__file__).resolve().parents[1]
def load(name):
 spec=importlib.util.spec_from_file_location(name,ROOT/'scripts'/f'{name}.py')
 m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
class GameVoiceBoxPackagingTest(unittest.TestCase):
 def test_new_identity_and_portable_build(self):
  make=(ROOT/'Makefile').read_text()
  self.assertIn('GameVoiceBox.exe',make)
  self.assertIn('GameVoiceBox-Setup-1.2.1-x64.exe',make)
  self.assertIn('TOOLS_ROOT ?= /usr',make)
  self.assertIn('installer/gamevoicebox.nsi',make)
 def test_installer_payload_excludes_original_video_and_source(self):
  m=load('generate_uninstall')
  self.assertTrue(hasattr(m,'runtime_asset_paths'))
  paths=m.runtime_asset_paths(ROOT/'assets')
  self.assertEqual(sum(p.suffix=='.wav' for p in paths),14)
  self.assertFalse(any('original' in p.parts or p.suffix.lower() in ('.mp3','.mp4') for p in paths))
 def test_installer_payload_preserves_root_license_and_selective_uninstall(self):
  m=load('generate_uninstall')
  with tempfile.TemporaryDirectory() as t:
   root=pathlib.Path(t)
   for name in ('dist','assets','vendor','docs'):(root/name).mkdir()
   (root/'dist/GameVoiceBox.exe').write_bytes(b'packaging-only exe fixture')
   license_bytes=(ROOT/'LICENSE.md').read_bytes()
   (root/'LICENSE.md').write_bytes(license_bytes)
   catalog=json.loads((ROOT/'assets/catalog.json').read_text())
   (root/'assets/catalog.json').write_text(json.dumps(catalog,ensure_ascii=False))
   (root/'assets/integrity.json').write_text('[]')
   for clip in catalog['clips']:(root/'assets'/clip['runtime_file']).write_bytes(b'packaging-only wav fixture')
   (root/'vendor/README-VB-CABLE.txt').write_text('vendor fixture')
   vendor_manifest=json.loads((ROOT/'vendor/integrity.json').read_text())
   (root/'vendor/integrity.json').write_text(json.dumps(vendor_manifest))
   (root/'vendor/official').mkdir()
   for name in vendor_manifest['files']:(root/'vendor/official'/name).write_bytes(b'packaging-only driver fixture')
   for name in ('中文使用教程.html','Windows验收清单.txt','验证报告.txt','THIRD_PARTY_NOTICES.md'):
    (root/'docs'/name).write_text('required document fixture')
   with patch.object(m,'ROOT',root):m.generate()
   installed=root/'build/payload/LICENSE.md'
   self.assertTrue(installed.is_file(),'Root LICENSE.md is missing from installed payload')
   self.assertEqual(installed.read_bytes(),license_bytes)
   manifest=json.loads((root/'build/payload-manifest.json').read_text())
   self.assertIn('LICENSE.md',manifest)
   self.assertIn('Delete "$INSTDIR\\LICENSE.md"',(root/'build/uninstall-files.nsh').read_text())
 def test_runtime_driver_excludes_duplicate_zip_but_keeps_all_official_files(self):
  m=load('generate_uninstall')
  self.assertTrue(hasattr(m,'runtime_vendor_paths'),'runtime vendor selection missing')
  paths=m.runtime_vendor_paths(ROOT/'vendor')
  self.assertEqual(len(paths),33)
  self.assertEqual(sum(p.parent.name=='official' for p in paths),31)
  self.assertFalse(any(p.suffix=='.zip' for p in paths))
  self.assertTrue((ROOT/'vendor/VBCABLE_Driver_Pack45.zip').is_file(),'original archive must remain in complete source')
 def test_pe_imports_inspected_from_the_extracted_executable(self):
  m=load('verify_package')
  self.assertIn('objdump',inspect.signature(m.pe_check).parameters)
  with tempfile.TemporaryDirectory() as t:
   exe=pathlib.Path(t)/'actual.exe'
   b=bytearray(128);b[:2]=b'MZ';b[0x3c:0x40]=(64).to_bytes(4,'little');b[64:68]=b'PE\0\0';b[68:70]=(0x8664).to_bytes(2,'little');exe.write_bytes(b)
   with patch.object(m.subprocess,'check_output',return_value=' DLL Name: KERNEL32.dll\n DLL Name: USER32.dll\n') as p:
    dlls=m.pe_check(exe,'test-objdump')
    p.assert_called_once_with(['test-objdump','-p',str(exe)],text=True)
    self.assertEqual(dlls,['KERNEL32.dll','USER32.dll'])
 def test_no_third_party_binaries_or_work_records_in_git(self):
  ignore=(ROOT/'.gitignore').read_text()
  for pattern in ('/vendor/official/','/vendor/VBCABLE_Driver_Pack45.zip','/build/','/dist/'):
   self.assertIn(pattern,ignore)
 def test_vendor_restore_validates_archive_and_members(self):
  self.assertTrue((ROOT/'scripts/fetch_vendor.py').is_file(),'verified vendor restore helper missing')
  m=load('fetch_vendor')
  i=json.loads((ROOT/'vendor/integrity.json').read_text())
  with tempfile.TemporaryDirectory() as t:
   dest=pathlib.Path(t)/'vendor'
   m.restore(ROOT/'vendor/VBCABLE_Driver_Pack45.zip',dest,i)
   self.assertEqual(len(list((dest/'official').iterdir())),31)
   changed=dict(i,zip_sha256='0'*64)
   with self.assertRaises(ValueError):m.restore(ROOT/'vendor/VBCABLE_Driver_Pack45.zip',pathlib.Path(t)/'bad',changed)
if __name__=='__main__':unittest.main()
