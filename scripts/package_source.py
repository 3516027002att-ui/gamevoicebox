#!/usr/bin/env python3
from pathlib import Path
import zipfile
ROOT=Path(__file__).resolve().parents[1]
EXCLUDED={'.git','dist','__pycache__','tools','cache','superpowers'}
def package():
 target=ROOT/'dist/GameVoiceBox-Source-1.2.1.zip';target.parent.mkdir(exist_ok=True)
 with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for p in sorted(ROOT.rglob('*')):
   rel=p.relative_to(ROOT)
   if not p.is_file() or any(c in EXCLUDED for c in rel.parts):continue
   if rel.parts[0]=='build':continue
   if p.suffix in ('.log','.passed') or p.name in ('implementation-ledger.md','review-summary.txt'):continue
   z.write(p,str(Path('GameVoiceBox-1.2.1-source')/rel))
 print(target,'bytes=',target.stat().st_size)
if __name__=='__main__':package()
