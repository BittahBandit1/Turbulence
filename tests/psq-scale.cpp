#include <initializer_list>
#include "core/psq_scale.h"
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
int main() {
  for (int s=-32;s<=32;++s)
    assert(std::bit_cast<uint32_t>(psq_scale_factor(s))==std::bit_cast<uint32_t>(std::ldexp(1.f,s)));
  // Every possible integer lane, every GQR scale: preserve original f32 rounding.
  for (int s=-32;s<32;++s)
    for (int v=-32768;v<=65535;++v) {
      const float old_value=static_cast<float>(std::ldexp(static_cast<double>(v),-s));
      const float new_value=static_cast<float>(v)*psq_scale_factor(-s);
      assert(std::bit_cast<uint32_t>(old_value)==std::bit_cast<uint32_t>(new_value));
    }
  for (int s=-32;s<32;++s)
    for (double v : {0.,-0.,0.5,-0.5,255.999,65535.,-32768.,1e-40,1e40,
                     std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}) {
      const float a=static_cast<float>(v)*std::ldexp(1.f,s);
      const float b=static_cast<float>(v)*psq_scale_factor(s);
      assert(std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b));
    }
}
