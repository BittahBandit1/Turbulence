#include "core/cpu.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <initializer_list>
static unsigned char reference(double value,int scale) {
  const float f=static_cast<float>(value)*std::ldexp(1.f,scale);
  if (std::isnan(f) || f<=0) return 0;
  if (f>=255) return 255;
  return static_cast<unsigned char>(f);
}
int main() {
  CPUState cpu{};assert(cpu_init(&cpu));cpu.hid2=PPC_HID2_LSQE;
  const u32 address=GC_RAM_BASE+0x100;
  for(int s=-32;s<32;++s) {
    cpu.gqr[6]=((static_cast<u32>(s)&63)<<8)|4;
    for(int v=-1024;v<4096;++v) {
      cpu.fpr[2]=v/4.;cpu.ps1[2]=-v/7.;
      for(bool single : {false,true}) {
        mem_write8(&cpu,address+1,123);
        assert(ppc_psq_store(&cpu,2,address,single,6,false,0));
        assert(mem_read8(&cpu,address)==reference(cpu.fpr[2],s));
        assert(mem_read8(&cpu,address+1)==(single?123:reference(cpu.ps1[2],s)));
      }
    }
    for(double v : {std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),1e40,-1e40}) {
      cpu.fpr[2]=v;cpu.ps1[2]=-v;
      assert(ppc_psq_store(&cpu,2,address,false,6,false,0));
      assert(mem_read8(&cpu,address)==reference(v,s));
      assert(mem_read8(&cpu,address+1)==reference(-v,s));
    }
  }
  cpu.hid2=0;mem_write8(&cpu,address,123);
  assert(!ppc_psq_store(&cpu,2,address,false,6,false,0));
  assert(cpu.exception!=0 && mem_read8(&cpu,address)==123);
  cpu_free(&cpu);
}
