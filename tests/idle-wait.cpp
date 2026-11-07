#include "Core/PowerPC/StaticRecomp/TurbulenceIdleWait.h"
#include <cassert>
int main()
{
  using namespace TurbulenceIdleWait;
  assert(CanAdvance("GXEE8P",Address,0x8000,0,0,Instructions));
  assert(!CanAdvance("OTHER",Address,0x8000,0,0,Instructions));
  assert(!CanAdvance("GXEE8P",Address+4,0x8000,0,0,Instructions));
  assert(!CanAdvance("GXEE8P",Address,0,0,0,Instructions));
  assert(!CanAdvance("GXEE8P",Address,0x8000,1,0,Instructions));
  assert(!CanAdvance("GXEE8P",Address,0x8000,0,1,Instructions));
  auto modified=Instructions;modified[0]^=1;
  assert(!CanAdvance("GXEE8P",Address,0x8000,0,0,modified));
}
