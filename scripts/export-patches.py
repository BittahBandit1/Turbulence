#!/usr/bin/env python3
"""Save vendor source edits without modifying any repository index."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
REPOS=[('lib/DolRecomp','dolrecomp-host'),('lib/ModernGekko','moderngekko'),('lib/ModernGekko/vendor/dolphin','dolphin'),
       ('lib/ModernGekko/vendor/dolphin/DolRecomp','dolrecomp'),
       ('lib/ModernGekko/vendor/dolphin/Externals/watcher/watcher','watcher-mobile'),
       ('lib/ModernGekko/vendor/dolphin/Externals/libspng/libspng','libspng-windows')]
def git(repo,*args):return subprocess.check_output(['git','-C',str(repo),*args])
def main():
 bases=[]
 for directory,name in REPOS:
  repo=ROOT/directory
  patch=git(repo,'diff','--ignore-submodules=all','--binary','HEAD','--')
  for raw in git(repo,'ls-files','--others','--exclude-standard','-z').split(b'\0'):
   if not raw:continue
   path=Path(raw.decode())
   if path.name=='.DS_Store' or path.suffix in ('.orig','.rej'):continue
   result=subprocess.run(['git','diff','--no-index','--binary','--','/dev/null',str(path)],cwd=repo,capture_output=True)
   if result.returncode not in (0,1):raise RuntimeError(result.stderr.decode())
   patch+=result.stdout
  (ROOT/'patches'/f'{name}.patch').write_bytes(patch)
  bases.append(name+' '+git(repo,'rev-parse','HEAD').decode().strip())
  print(f'{name}: {patch.count(b"diff --git ")} files')
 (ROOT/'patches/base-commits.txt').write_text('\n'.join(bases)+'\n')
if __name__=='__main__':main()
