/* Generated ARM64 scalar arithmetic against the GXRuntime reference. */
#include "cpu_interpreter_private.h"
#include <stdio.h>
#include <string.h>

void func_80003100(CPUState* cpu);
static u64 seed = 0x7e51fbc398a246d1ull;
static u64 random_bits(void) {
    seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
    return seed;
}
static double from_bits(u64 bits) { double x; memcpy(&x, &bits, 8); return x; }
static void reference(CPUState* cpu, unsigned operation, u8 destination) {
    if (operation == 0) ppc_fadds(cpu, destination, 1, 2);
    else if (operation == 1) ppc_fsubs(cpu, destination, 1, 2);
    else ppc_fmuls(cpu, destination, 1, 2);
}
int main(void) {
    static const u64 edges[] = {
        0, 0x8000000000000000ull, 1, 0x8000000000000001ull,
        0x0010000000000000ull, 0x380fffffffffffffull,
        0x3810000000000000ull, 0x3810000000000001ull,
        0x3ff0000000000000ull, 0x3ff0000008000000ull,
        0x3ff0000010000000ull, 0x47efffffe0000000ull,
        0x47efffffe0000001ull, 0x7fefffffffffffffull,
        0xffefffffffffffffull, 0x7ff0000000000000ull,
        0xfff0000000000000ull, 0x7ff8000000000001ull,
        0x7ff0000000000001ull, 0xfff8000012345678ull,
        0x7fffffffffffffffull, 0x3fefffffffffffffull,
        0xbff0000000000000ull, 0xb810000000000000ull
    };
    CPUState actual, expected;
    if (!cpu_init(&actual) || !cpu_init(&expected)) return 2;
    unsigned long long checked = 0;
    for (unsigned mode = 0; mode < 16; ++mode) {
        const u32 fpscr = (mode & 3) | ((mode & 4) ? FPSCR_NI_BIT : 0) |
                          ((mode & 8) ? FPSCR_VE_BIT : 0) | FPSCR_FI_BIT | FPSCR_FR_BIT;
        actual.fpscr = fpscr; ppc_fpscr_control_updated(&actual);
        for (unsigned i = 0; i < 10000; ++i) {
            const unsigned n = sizeof(edges) / sizeof(edges[0]);
            double a = from_bits(i < n*n ? edges[i / n] : random_bits());
            double b = from_bits(i < n*n ? edges[i % n] : random_bits());
            if (i >= n*n && (i & 1)) {
                a = (double)(float)((int)(random_bits() % 1000000) - 500000) / 13.0;
                b = (double)(float)((int)(random_bits() % 1000000) - 500000) / 17.0;
            }
            if (i >= n*n && (i % 16) == 0) {
                a = 1.0;
                b = from_bits(0x3e70000000000000ull + ((i / 16) & 1));
            }
            for (unsigned variant = 0; variant < 26; ++variant) {
                const u32 pc = variant < 9 ? 0x80003128u + variant*8 :
                               variant == 9 ? 0x80003170u : 0x80003180u + (variant-10)*8;
                actual.pc = pc; actual.lr = 0x81234560u; actual.downcount = 1000;
                actual.msr = 1u << 13; actual.exception = 0;
                actual.fpscr = expected.fpscr = fpscr;
                for (unsigned r = 0; r < 32; ++r) {
                    actual.fpr[r] = expected.fpr[r] = 123.0 + r;
                    actual.ps1[r] = expected.ps1[r] = -456.0 - r;
                }
                actual.fpr[1] = expected.fpr[1] = a;
                actual.fpr[2] = expected.fpr[2] = b;
                double c = i < n*n ? from_bits(edges[(i + i/n) % n]) :
                           (i & 1) ? (double)(float)((int)(random_bits() % 1000000) - 500000)/19.0 : from_bits(random_bits());
                if (i >= n*n && (i % 16) == 0) c = 1.0;
                actual.fpr[3] = expected.fpr[3] = c;
                func_80003100(&actual);
                if (variant < 9) reference(&expected, variant / 3, variant % 3);
                else if (variant == 9) {
                    ppc_fadds(&expected, 0, 1, 2);
                    ppc_fsubs(&expected, 1, 0, 2);
                    ppc_fmuls(&expected, 2, 1, 2);
                } else {
                    const unsigned fused = (variant - 10) / 4;
                    const unsigned destination = (variant - 10) % 4;
                    double result = expected.fpr[destination];
                    if (ppc_fma(&expected, a, c, b, true, fused & 1, fused >= 2, &result)) {
                        expected.fpr[destination] = result;
                        expected.ps1[destination] = result;
                    }
                }
                if (memcmp(actual.fpr, expected.fpr, sizeof(actual.fpr)) ||
                    memcmp(actual.ps1, expected.ps1, sizeof(actual.ps1)) ||
                    actual.fpscr != expected.fpscr || actual.exception || actual.pc != actual.lr) {
                    u64 ab, bb; memcpy(&ab, &a, 8); memcpy(&bb, &b, 8);
                    fprintf(stderr, "generated scalar mismatch mode=%u i=%u variant=%u a=%016llx b=%016llx fpscr=%08x/%08x pc=%08x\n",
                            mode, i, variant, (unsigned long long)ab, (unsigned long long)bb,
                            actual.fpscr, expected.fpscr, actual.pc);
                    for (unsigned r=0; r<3; ++r) {
                        u64 x,y; memcpy(&x,&actual.fpr[r],8);memcpy(&y,&expected.fpr[r],8);
                        fprintf(stderr," f%u=%016llx/%016llx\n",r,(unsigned long long)x,(unsigned long long)y);
                    }
                    return 1;
                }
                ++checked;
            }
        }
    }
    actual.fpscr = 0; ppc_fpscr_control_updated(&actual);
    printf("Generated scalar: %llu exact comparisons passed\n", checked);
    cpu_free(&actual); cpu_free(&expected);
    return 0;
}
