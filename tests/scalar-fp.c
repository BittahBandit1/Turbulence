/* Exact finite scalar paths versus the original generic operations. */
#include "cpu_interpreter_private.h"
#include <stdio.h>
#include <string.h>

static u64 seed = 0x8f31d7e9a46b5201ull;
static u64 random_bits(void) {
    seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
    return seed;
}
static f64 from_bits(u64 bits) { f64 x; memcpy(&x, &bits, sizeof(x)); return x; }
static void reference(CPUState* cpu, unsigned op, u8 d) {
    FPRes result;
    if (op == 0) result = ni_add(cpu, cpu->fpr[1], cpu->fpr[2]);
    else if (op == 1) result = ni_sub(cpu, cpu->fpr[1], cpu->fpr[2]);
    else result = ni_mul(cpu, cpu->fpr[1], force_25bit_c(cpu->fpr[2]));
    if (!fp_invalid_gated(cpu, &result)) {
        fp_write_single(cpu, d, force_single(cpu, result.value));
        if (op == 2) cpu->fpscr &= ~(FPSCR_FI_BIT | FPSCR_FR_BIT);
    }
}
int main(void) {
    static const u64 edges[] = {
        0, 0x8000000000000000ull, 1, 0x8000000000000001ull,
        0x0010000000000000ull, 0x3810000000000000ull,
        0x380fffffffffffffull, 0x3ff0000000000000ull,
        0x3ff0000008000000ull, 0x47efffffe0000000ull,
        0x7fefffffffffffffull, 0xffefffffffffffffull,
        0x7ff0000000000000ull, 0xfff0000000000000ull,
        0x7ff8000000000001ull, 0x7ff0000000000001ull,
        0xfff8000012345678ull, 0x7fffffffffffffffull
    };
    CPUState actual = {0}, expected = {0};
    unsigned long long checked = 0;
    for (unsigned mode = 0; mode < 16; ++mode) {
        const u32 fpscr = (mode & 3u) | ((mode & 4u) ? FPSCR_NI_BIT : 0u) |
                          ((mode & 8u) ? FPSCR_VE_BIT : 0u) | FPSCR_FI_BIT | FPSCR_FR_BIT;
        actual.fpscr = fpscr; ppc_fpscr_control_updated(&actual);
        for (unsigned i = 0; i < 100000; ++i) {
            const unsigned n = sizeof(edges) / sizeof(edges[0]);
            const u64 a = i < n*n ? edges[i / n] : random_bits();
            const u64 b = i < n*n ? edges[i % n] : random_bits();
            for (unsigned op = 0; op < 3; ++op) {
                const u8 d = i % 3;
                for (unsigned r = 0; r < 3; ++r) {
                    actual.fpr[r] = expected.fpr[r] = from_bits(0x405ec00000000000ull);
                    actual.ps1[r] = expected.ps1[r] = from_bits(0xc05ec00000000000ull);
                }
                actual.fpr[1] = expected.fpr[1] = from_bits(a);
                actual.fpr[2] = expected.fpr[2] = from_bits(b);
                actual.fpscr = expected.fpscr = fpscr;
                if (op == 0) ppc_fadds(&actual, d, 1, 2);
                else if (op == 1) ppc_fsubs(&actual, d, 1, 2);
                else ppc_fmuls(&actual, d, 1, 2);
                reference(&expected, op, d);
                if (memcmp(actual.fpr, expected.fpr, sizeof(actual.fpr)) ||
                    memcmp(actual.ps1, expected.ps1, sizeof(actual.ps1)) ||
                    actual.fpscr != expected.fpscr) {
                    fprintf(stderr, "scalar mismatch mode=%u iteration=%u op=%u d=%u a=%016llx b=%016llx fpscr=%08x/%08x\n",
                            mode, i, op, d, (unsigned long long)a, (unsigned long long)b,
                            actual.fpscr, expected.fpscr);
                    return 1;
                }
                ++checked;
            }
        }
    }
    actual.fpscr = 0; ppc_fpscr_control_updated(&actual);
    printf("Scalar FP: %llu exact comparisons passed (rounding, NI, VE, aliases, NaN/Inf/subnormals)\n", checked);
    return 0;
}
