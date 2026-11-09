#!/usr/bin/env python3
"""Check built mobile packages, without installing them or reading game assets."""
import argparse
from pathlib import Path
import plistlib
import struct
import zipfile

def arm64_elf(data):
 if data[:6]!=b'\x7fELF\x02\x01' or struct.unpack_from('<H',data,18)[0]!=183:
  raise ValueError('Expected a little-endian ARM64 ELF shared library')
 offset=struct.unpack_from('<Q',data,32)[0]
 size,count=struct.unpack_from('<HH',data,54)
 alignments=[struct.unpack_from('<Q',data,offset+i*size+48)[0] for i in range(count)
             if struct.unpack_from('<I',data,offset+i*size)[0]==1]
 if not alignments or min(alignments)<16384:raise ValueError('ELF load segments lack 16 KiB page alignment')

def check(path):
 with zipfile.ZipFile(path) as archive:
  names=set(archive.namelist())
  if any(Path(name).name.lower() in ('main.dol','_main.rel') for name in names):
   raise ValueError('A personal raw game executable was accidentally packaged')
  if path.suffix=='.apk':
   for name in ['classes.dex','AndroidManifest.xml','assets/GFX.ini','assets/COPYING',
                'assets/Sys/GameSettings/GXEE8P.ini']:
    if name not in names:raise ValueError('Missing APK resource: '+name)
   for name in ['lib/arm64-v8a/libturbulence.so','lib/arm64-v8a/libc++_shared.so']:
    arm64_elf(archive.read(name))
   print(f'PASS {path.name}: ARM64, 16 KiB pages, resources, no raw game executables')
  else:
   plists=[n for n in names if n.count('/')==2 and n.endswith('.app/Info.plist')]
   if len(plists)!=1:raise ValueError('Expected exactly one iOS app bundle')
   app=plists[0].rsplit('/',1)[0];info=plistlib.loads(archive.read(plists[0]))
   data=archive.read(app+'/'+info['CFBundleExecutable'])
   if struct.unpack_from('<II',data)!=(0xfeedfacf,0x100000c):raise ValueError('Expected ARM64 Mach-O')
   offset=32;device=False
   for _ in range(struct.unpack_from('<I',data,16)[0]):
    command,size=struct.unpack_from('<II',data,offset)
    if size<8 or offset+size>len(data):raise ValueError('Invalid Mach-O load command')
    if command==0x32:
     platform,minimum=struct.unpack_from('<II',data,offset+8)
     device=platform==2 and minimum==0xf0000
    offset+=size
   if not device or info.get('MinimumOSVersion')!='15.0':raise ValueError('Expected physical iOS 15 target')
   for name in ['GFX.ini','COPYING','Sys/GameSettings/GXEE8P.ini']:
    if app+'/'+name not in names:raise ValueError('Missing iOS resource: '+name)
   print(f'PASS {path.name}: physical ARM64 iOS 15, resources, no raw game executables')

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('packages',nargs='+',type=Path)
 args=parser.parse_args()
 for path in args.packages:check(path)
if __name__=='__main__':main()
