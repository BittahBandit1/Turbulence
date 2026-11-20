#!/usr/bin/env python3
"""Regenerate the original Xbox controller artwork from a personal USA XISO.

Requires Pillow. Reads only the disc directory and stage 1 archive. Does not
extract a game installation or change the input disc. The archive bitstream
format is documented by romhack/sonic_riders_lzss (Griever, MIT).
"""
import argparse, io, struct
from PIL import Image
from pathlib import Path
import hashlib
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('iso',type=Path)
parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'src/turbulence_xbox_artwork.hpp')
args=parser.parse_args()
with args.iso.open('rb') as disc:
 def read(offset,size):
  disc.seek(offset);d=disc.read(size)
  if len(d)!=size:raise ValueError('Truncated Xbox disc')
  return d
 header=read(32*2048,2048)
 if header[:20]!=b'MICROSOFT*XBOX*MEDIA':raise ValueError('Expected an extracted Xbox XISO')
 sector,size=struct.unpack_from('<II',header,20)
 if size>16*1024*1024:raise ValueError('Invalid disc directory')
 directory=read(sector*2048,size);pending=[0];seen=set();stage=None
 while pending:
  offset=pending.pop()
  if offset in seen or offset+14>len(directory):raise ValueError('Invalid directory tree')
  seen.add(offset)
  left,right,sector,size,flags,length=struct.unpack_from('<HHIIBB',directory,offset)
  if offset+14+length>len(directory):raise ValueError('Invalid file name')
  name=directory[offset+14:offset+14+length]
  if name==b'1' and not flags&16:
   if size>32*1024*1024:raise ValueError('Invalid stage archive')
   stage=read(sector*2048,size);break
  if left:pending.append(left*4)
  if right:pending.append(right*4)
 if stage is None:raise ValueError('Sonic Riders stage 1 not found')
if stage[:4]!=b'\1\0\0\x80':raise ValueError('Unsupported archive')
length=struct.unpack_from('<I',stage,4)[0]
if length>32*1024*1024:raise ValueError('Invalid decompressed size')
position=0x80*8;data=bytearray()
def bits(n):
 global position
 if position+n>len(stage)*8:raise ValueError('Truncated compressed stream')
 value=0
 for _ in range(n):
  value=value*2+((stage[position//8]>>(7-position%8))&1);position+=1
 return value
while len(data)<length:
 if bits(1):
  distance=bits(8);count=bits(8)
  if not 0<distance<=len(data) or not count or count>length-len(data):raise ValueError('Invalid back reference')
  for _ in range(count):data.append(data[-distance])
 else:data.append(bits(8))
# This is the original Xbox-specific 256x64 stick/trigger sheet, not the
# adjacent shared GC/PS2 256x128 sheet. Lock its decoded digest below.
offset=0x2f8a50
if data[offset:offset+4]!=b'PVRT':raise ValueError('Unsupported stage revision')
size=struct.unpack_from('<I',data,offset+4)[0]
if struct.unpack_from('<HH',data,offset+12)!=(256,64) or data[offset+29]!=15:raise ValueError('Unexpected controller texture')
raw=data[offset+48:offset+8+size]
dds=struct.pack('<7I',124,0x81007,64,256,len(raw),0,1)+bytes(44)+struct.pack('<II4s5I',32,4,b'DXT5',0,0,0,0,0)+struct.pack('<5I',0x1000,0,0,0,0)
im=Image.open(io.BytesIO(b'DDS '+dds+raw)).convert('RGBA')
expected='3067ce2352820529ac0b84483fb5879d8a4731155f964b1e829c5618d40261d3'
if hashlib.sha256(im.tobytes()).hexdigest()!=expected:raise ValueError('Controller artwork differs from the verified USA disc')
lines=['// Original Xbox Sonic Riders controller artwork, extracted from the owner-provided disc.',
'// Source: /1, PVRT at decompressed offset 0x2f8a50; DXT5, 256x64.',
'// Full source sheet SHA256: '+hashlib.sha256(im.tobytes()).hexdigest(),
'// RLE pairs are run length, RGBA (R in the most significant byte).',
'#pragma once','#include <array>','#include <cstdint>','#include <vector>',
'namespace turbulence {','inline void RegisterXboxPromptArtwork() {']
for name,x in [('xbox-stick',0),('xbox-left-trigger',64),('xbox-right-trigger',128)]:
 rgba=im.crop((x,0,x+64,64)).tobytes();pixels=[int.from_bytes(rgba[i:i+4],'big') for i in range(0,len(rgba),4)];runs=[]
 for v in pixels:
  if runs and runs[-1][1]==v:runs[-1][0]+=1
  else:runs.append([1,v])
 lines+=['  {',f'    static constexpr uint32_t runs[] = {{']
 for i in range(0,len(runs),6):lines.append('      '+', '.join(f'{n}, 0x{v:08x}' for n,v in runs[i:i+6])+',')
 lines+=['    };','    std::vector<u8> rgba; rgba.reserve(64 * 64 * 4);',
'    for (size_t i = 0; i < std::size(runs); i += 2)',
'      for (uint32_t n = 0; n < runs[i]; ++n)',
'        for (int shift : {24, 16, 8, 0}) rgba.push_back(static_cast<u8>(runs[i + 1] >> shift));',
f'    TurbulencePromptTextures::RegisterArtwork("{name}", 64, 64, std::move(rgba));','  }']
lines+=['}','} // namespace turbulence','']
args.output.write_text('\n'.join(lines))
print('generated',args.output.stat().st_size)
