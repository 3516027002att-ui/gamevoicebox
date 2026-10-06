#!/usr/bin/env python3
"""Restore only the official, hash-pinned standard VB-CABLE pack (not A+B/C+D)."""
from pathlib import Path
import argparse,hashlib,json,shutil,tempfile,urllib.request,zipfile
ROOT=Path(__file__).resolve().parents[1]
def restore(archive,destination,integrity):
 archive=Path(archive);destination=Path(destination)
 if hashlib.sha256(archive.read_bytes()).hexdigest()!=integrity['zip_sha256']:
  raise ValueError('Official VB-CABLE ZIP SHA-256 mismatch; refusing to extract')
 with zipfile.ZipFile(archive) as z:
  names=z.namelist()
  if len(names)!=31 or set(names)!=set(integrity['files']):
   raise ValueError('Official ZIP members do not match the pinned manifest')
  payload={}
  for name,expected in integrity['files'].items():
   if Path(name).name!=name or '\\' in name:raise ValueError('Unsafe vendor member path')
   data=z.read(name)
   if len(data)!=expected['size'] or hashlib.sha256(data).hexdigest()!=expected['sha256']:
    raise ValueError('Official member hash mismatch: '+name)
   payload[name]=data
 destination.mkdir(parents=True,exist_ok=True);official=destination/'official';official.mkdir(exist_ok=True)
 for name,data in payload.items():(official/name).write_bytes(data)
 target=destination/'VBCABLE_Driver_Pack45.zip'
 if archive.resolve()!=target.resolve():shutil.copyfile(archive,target)
 return official
def main():
 p=argparse.ArgumentParser();p.add_argument('--archive',type=Path);a=p.parse_args()
 integrity=json.loads((ROOT/'vendor/integrity.json').read_text())
 if a.archive:restore(a.archive,ROOT/'vendor',integrity)
 elif (ROOT/'vendor/VBCABLE_Driver_Pack45.zip').is_file():restore(ROOT/'vendor/VBCABLE_Driver_Pack45.zip',ROOT/'vendor',integrity)
 else:
  with tempfile.TemporaryDirectory() as t:
   archive=Path(t)/'VBCABLE_Driver_Pack45.zip'
   with urllib.request.urlopen(integrity['source'],timeout=60) as response:archive.write_bytes(response.read())
   restore(archive,ROOT/'vendor',integrity)
 print('Verified original official ZIP and all 31 VB-CABLE files; no driver installed')
if __name__=='__main__':main()
