#!/usr/bin/env python3
"""Freeze exact installed file list; never recursively remove user files."""
from pathlib import Path
import shutil,json,hashlib
ROOT=Path(__file__).resolve().parents[1]
def quote_nsis(path):
 return str(path).replace('\\','/').replace('$','$$').replace('"','$\\"').replace('/','\\')
def uninstall_lines(payload):
 files=sorted(p.relative_to(payload) for p in payload.rglob('*') if p.is_file())
 dirs=sorted((p.relative_to(payload) for p in payload.rglob('*') if p.is_dir()),key=lambda p:(-len(p.parts),str(p)))
 lines=[]
 for p in files:
  lines.extend([f'IfFileExists "$INSTDIR\\{quote_nsis(p)}" 0 +2',f'Delete "$INSTDIR\\{quote_nsis(p)}"'])
 return (lines,[f'RMDir "$INSTDIR\\{quote_nsis(p)}"' for p in dirs])
def runtime_asset_paths(assets):
 catalog=json.loads((assets/'catalog.json').read_text())
 paths=[assets/c['runtime_file'] for c in catalog['clips']]
 paths += [assets/'catalog.json',assets/'integrity.json']
 if len(catalog['clips'])!=14 or any(not p.is_file() for p in paths):
  raise ValueError('Full 14-clip runtime assets missing')
 return paths
def runtime_vendor_paths(vendor):
 integrity=json.loads((vendor/'integrity.json').read_text())
 if len(integrity['files'])!=31:raise ValueError('Expected complete standard VB-CABLE file list')
 paths=[vendor/'README-VB-CABLE.txt',vendor/'integrity.json']
 for name in integrity['files']:
  if Path(name).name!=name or '\\' in name:raise ValueError('Invalid official vendor path')
  paths.append(vendor/'official'/name)
 if any(not p.is_file() for p in paths):raise ValueError('Original official driver files missing')
 return paths
def generate():
 payload=ROOT/'build/payload'
 if payload.exists():shutil.rmtree(payload)
 payload.mkdir(parents=True)
 exe=ROOT/'dist/GameVoiceBox.exe'
 if not exe.is_file():raise FileNotFoundError('Native EXE missing')
 shutil.copyfile(exe,payload/exe.name)
 shutil.copyfile(ROOT/'LICENSE.md',payload/'LICENSE.md')
 for dirname in ('assets','vendor','docs'):
  source=ROOT/dirname
  if dirname=='assets':
   (payload/dirname).mkdir()
   for p in runtime_asset_paths(source):shutil.copyfile(p,payload/dirname/p.name)
  elif dirname=='docs':
   (payload/dirname).mkdir()
   for name in ('中文使用教程.html','Windows验收清单.txt','验证报告.txt','THIRD_PARTY_NOTICES.md'):
    p=source/name
    if not p.is_file():raise FileNotFoundError('Required document missing: '+name)
    shutil.copyfile(p,payload/dirname/name)
  else:
   for p in runtime_vendor_paths(source):
    target=payload/dirname/p.relative_to(source);target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
 files,dirs=uninstall_lines(payload)
 (ROOT/'build/uninstall-files.nsh').write_text('\n'.join(files)+'\n')
 (ROOT/'build/uninstall-dirs.nsh').write_text('\n'.join(dirs)+'\n')
 manifest={str(p.relative_to(payload)): {'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(payload.rglob('*')) if p.is_file()}
 (ROOT/'build/payload-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
 print('Frozen',len(manifest),'payload files for selective uninstall')
if __name__=='__main__':generate()
