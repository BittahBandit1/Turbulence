#include "VideoCommon/TurbulenceNativeMenu.h"
#include "../src/turbulence_native_glyphs.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

struct CPU {std::array<unsigned,32> gpr{};unsigned pc=0,lr=0;};
struct Memory {
  std::vector<unsigned char> bytes=std::vector<unsigned char>(24*1024*1024);
  unsigned char* GetPointerForRange(unsigned address,unsigned size) {
    if(address<0x80000000 || address-0x80000000>bytes.size() ||
       size>bytes.size()-(address-0x80000000)) return nullptr;
    return bytes.data()+address-0x80000000;
  }
  void Put(unsigned address,unsigned value) {
    value=Common::swap32(value);std::memcpy(GetPointerForRange(address,4),&value,4);
  }
  unsigned Get(unsigned address) {return Common::swap32(GetPointerForRange(address,4));}
};
int main() {
  using namespace TurbulenceNativeMenu;
  host_ui_available=true;
  Memory mem;CPU cpu;
  constexpr unsigned stack=0x81700000,work=0x803e28d8,task=0x803d9048;
  for(unsigned i=0;i<56;++i) *mem.GetPointerForRange(0x805edbf4+i,1)=i;
  mem.Put(0x805edbf4,0x80534a84);
  *mem.GetPointerForRange(0x805ce502,1)=4;
  std::array<unsigned char,56> original;
  std::memcpy(original.data(),mem.GetPointerForRange(0x805edbf4,56),56);
  cpu.gpr[1]=stack;cpu.gpr[4]=0x805edbf4;cpu.gpr[5]=7;cpu.lr=0x8050f8bc;
  assert(BeforeDispatch(cpu,mem,0x804f3d38));
  assert(cpu.gpr[5]==8 && cpu.gpr[1]==stack-0xf0 && cpu.pc==0x804f3d3c);
  assert(mem.Get(stack-0xf0)==stack-0x80);
  auto* list=mem.GetPointerForRange(cpu.gpr[4],64);
  assert(std::memcmp(list,original.data(),48)==0 && list[55]==4);
  assert(std::memcmp(list+56,original.data()+48,8)==0);
  assert(std::memcmp(mem.GetPointerForRange(0x805edbf4,56),original.data(),56)==0);
  assert(*mem.GetPointerForRange(0x805ce502,1)==5);
  cpu.gpr[31]=work;*mem.GetPointerForRange(work+0x39,1)=2;
  assert(BeforeDispatch(cpu,mem,0x8050f8bc));
  assert(cpu.gpr[1]==stack && cpu.pc==0x8050f8c0 && cpu.gpr[0]==2);

  mem.Put(0x8021c314,task);mem.Put(task+0x10,work);
  *mem.GetPointerForRange(task+0x14,1)=15;
  BeforeDispatch(cpu,mem,0x8050edb8);assert(options_visible);
  *mem.GetPointerForRange(work+0xd,1)=4;cpu.gpr[31]=work;
  BeforeDispatch(cpu,mem,0x80511790);assert(mem.Get(0x805eda20)==0xffffffff);
  const unsigned item=0x81700200;
  *mem.GetPointerForRange(item+0x37,1)=4;
  mem.Put(item+0x1c,0);mem.Put(0x8021b8e0,1);
  *mem.GetPointerForRange(0x805ce4d4+7,1)=0x20;
  const unsigned pool=0x81710000;
  mem.Put(0x80682ef8,pool);mem.Put(pool+0x6020,work);
  cpu.gpr[3]=item;cpu.lr=0x805135b0;
  BeforeDispatch(cpu,mem,0x805134a4);
  assert(*mem.GetPointerForRange(item+0x37,1)==3);
  assert(*mem.GetPointerForRange(work+0xd,1)==3);
  assert(mem.Get(item+0x1c)==0);
  cpu.gpr[25]=item;mem.Put(cpu.gpr[1]+0x11c,0);
  BeforeDispatch(cpu,mem,0x804f43b8);
  float y;unsigned bits=mem.Get(cpu.gpr[1]+0x11c);std::memcpy(&y,&bits,4);assert(y==38);
  BeforeDispatch(cpu,mem,0x805135b0);assert(mem.Get(item+0x1c)==0);
  assert(*mem.GetPointerForRange(item+0x37,1)==4);
  assert(*mem.GetPointerForRange(work+0xd,1)==4);

  // The native selection highlight is a separate job from the label.
  mem.Put(item+0x30,0x80512b24);*mem.GetPointerForRange(item+0x4e,1)=4;
  mem.Put(cpu.gpr[1]+0x11c,0);cpu.gpr[25]=item;
  BeforeDispatch(cpu,mem,0x804f43b8);
  bits=mem.Get(cpu.gpr[1]+0x11c);std::memcpy(&y,&bits,4);assert(y==38);

  cpu.gpr[3]=work;BeforeDispatch(cpu,mem,0x80511f1c);
  assert(settings_page && art_changed && *mem.GetPointerForRange(work+0xd,1)==2);
  cpu.gpr[3]=0x80585aa4;BeforeDispatch(cpu,mem,0x80511824);
  assert(*mem.GetPointerForRange(work+0xd,1)==4);
  const unsigned child=0x803d91e8,child_work=0x803e0fd0;
  mem.Put(0x8021c314,child);mem.Put(child+0x10,child_work);
  *mem.GetPointerForRange(child+0x14,1)=2;
  assert(!BeforeDispatch(cpu,mem,0x80585aa4)); // native initialization runs
  *mem.GetPointerForRange(child+0x14,1)=4;cpu.lr=0x80001234;
  assert(BeforeDispatch(cpu,mem,0x80585aa4));
  assert(request_settings && cpu.pc==cpu.lr && mem.Get(0x8021b8e0)==1);
  close_settings=true;
  assert(BeforeDispatch(cpu,mem,0x80585aa4));
  assert(*mem.GetPointerForRange(child+0x14,1)==6);
  assert(*mem.GetPointerForRange(child_work+4,1)==3);
  mem.Put(0x8021c314,task);BeforeDispatch(cpu,mem,0x8050edb8);
  assert(!settings_page);
  assert(!BeforeDispatch(cpu,mem,0x80585aa4)); // original Language Setting untouched
  for(const auto& glyph:turbulence::kNativeGlyphs) {
    assert(glyph.x+glyph.w <= (glyph.atlas?1024:512));
    assert(glyph.y+glyph.h <= (glyph.atlas?64:512));
  }
  std::puts("Native menu insertion, caption, page lifecycle and language preservation passed");
}
