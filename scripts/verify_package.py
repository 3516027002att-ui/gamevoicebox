#!/usr/bin/env python3
"""Verify actual NSIS extraction, PE architecture/imports and all payload hashes."""
from pathlib import Path
import json,hashlib,subprocess,argparse,shutil,struct
ROOT=Path(__file__).resolve().parents[1]
ALLOWED={'KERNEL32.dll','msvcrt.dll','USER32.dll','GDI32.dll','COMCTL32.dll','SHELL32.dll','ole32.dll','OLEAUT32.dll','ADVAPI32.dll','avrt.dll','PROPSYS.dll','WINMM.dll'}
def pe_check(exe,objdump):
 b=exe.read_bytes();assert b[:2]==b'MZ';offset=struct.unpack_from('<I',b,0x3c)[0]
 assert b[offset:offset+4]==b'PE\0\0' and struct.unpack_from('<H',b,offset+4)[0]==0x8664,'Not AMD64 PE'
 imports=subprocess.check_output([objdump,'-p',str(exe)],text=True)
 dlls=[line.split('DLL Name:')[1].strip() for line in imports.splitlines() if 'DLL Name:' in line]
 assert dlls and all(d.lower() in {a.lower() for a in ALLOWED} for d in dlls),dlls
 return dlls
def verify(sevenzip,objdump):
 installer=ROOT/'dist/GameVoiceBox-Setup-1.2.1-x64.exe';assert installer.is_file(),'Actual installer missing'
 b=installer.read_bytes();assert b[:2]==b'MZ','Installer is not PE'
 target=ROOT/'build/package-extract'
 if target.exists():shutil.rmtree(target)
 subprocess.run([sevenzip,'x','-y',f'-o{target}',str(installer)],check=True,stdout=subprocess.DEVNULL)
 manifest=json.loads((ROOT/'build/payload-manifest.json').read_text())
 for n,v in manifest.items():
  p=target/n
  assert p.is_file(),'Missing installed payload: '+n
  assert p.stat().st_size==v['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest()==v['sha256'],'Payload changed: '+n
 expected=set(manifest);actual={str(p.relative_to(target)) for p in target.rglob('*') if p.is_file()}
 # NSIS also contains its generated uninstaller/temporary plugin binaries.
 extras=actual-expected
 assert all(n.startswith('$PLUGINSDIR/') or n=='uninstall.exe' or n=='Uninstall.exe' for n in extras),extras
 i=json.loads((ROOT/'vendor/integrity.json').read_text())
 assert len(i['files'])==31
 assert not (target/'vendor/VBCABLE_Driver_Pack45.zip').exists(),'Duplicate driver archive must stay source-only'
 assert hashlib.sha256((ROOT/'vendor/VBCABLE_Driver_Pack45.zip').read_bytes()).hexdigest()==i['zip_sha256']
 for name,v in i['files'].items():assert hashlib.sha256((target/'vendor/official'/name).read_bytes()).hexdigest()==v['sha256']
 dlls=pe_check(target/'GameVoiceBox.exe',objdump)
 groups={}
 for n,v in manifest.items():groups[n.split('/')[0]]=groups.get(n.split('/')[0],0)+v['bytes']
 report={'installer_bytes':installer.stat().st_size,'installer_mib':installer.stat().st_size/1048576,'installer_sha256':hashlib.sha256(b).hexdigest(),'native_exe_bytes':(target/'GameVoiceBox.exe').stat().st_size,'verified_payload_files':len(manifest),'official_files':31,'windows_dll_imports':dlls,'uncompressed_payload_groups':groups}
 (ROOT/'dist/package-verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps(report,ensure_ascii=False,indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--sevenzip',default='7z');p.add_argument('--objdump',default='x86_64-w64-mingw32-objdump');a=p.parse_args();verify(a.sevenzip,a.objdump)
