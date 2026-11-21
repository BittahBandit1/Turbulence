/* Differential test of generated dynamic-GQR loads/stores on ARM64. */
#include "core/cpu.h"
#include <stdio.h>
#include <string.h>

void func_80003100(CPUState* cpu);
static u32 seed = 0x5e1f932du;
static u32 random_word(void) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}
typedef struct { u32 address; u64 value; u8 width; bool write; } Access;
static CPUState* left_cpu;
static Access accesses[2][8];
static unsigned access_count[2];
static u64 io_read(CPUState* cpu, u32 address, u8 width) {
    const unsigned side = cpu == left_cpu ? 0 : 1;
    const u64 value = 0x9a353018u ^ address ^ width;
    accesses[side][access_count[side]++] = (Access){address, value, width, false};
    return value;
}
static void io_write(CPUState* cpu, u32 address, u64 value, u8 width) {
    const unsigned side = cpu == left_cpu ? 0 : 1;
    accesses[side][access_count[side]++] = (Access){address, value, width, true};
}
int main(void) {
    CPUState actual, expected;
    if (!cpu_init(&actual) || !cpu_init(&expected)) return 2;
    left_cpu = &actual;
    actual.external_read = expected.external_read = io_read;
    actual.external_write = expected.external_write = io_write;
    static const u64 raw_edges[] = {
        0, 0x8000000000000000ull, 1, 0x8000000000000001ull,
        0x380fffffffffffffull, 0xb80fffffffffffffull,
        0x3ff0000010000000ull, 0x3ff000001fffffffull,
        0x7ff0000000000000ull, 0xfff0000000000000ull,
        0x7ff0000000000001ull, 0xfff0000000000001ull,
        0x7ff8000012345678ull, 0xfff8000012345678ull,
        0x7fefffffffffffffull, 0xffefffffffffffffull
    };
    unsigned checked = 0;
    for (unsigned store_only = 0; store_only < 2; ++store_only)
    for (unsigned type = 0; type < 8; ++type)
    for (unsigned scale = 0; scale < 64; ++scale)
    for (unsigned single = 0; single < 2; ++single)
    for (unsigned offset = 0; offset < 4; ++offset)
    for (unsigned enabled = 0; enabled < 2; ++enabled)
    for (unsigned mmio = 0; mmio < 2; ++mmio)
    for (unsigned iteration = 0; iteration < 16; ++iteration) {
        const u32 pc = store_only ? (single ? 0x80003120u : 0x80003118u) :
                                     (single ? 0x8000310Cu : 0x80003100u);
        const u32 addr = (mmio ? 0xCC000100u : GC_RAM_BASE + 0x500u) + offset;
        access_count[0] = access_count[1] = 0;
        memset(accesses, 0, sizeof(accesses));
        actual.pc = expected.pc = pc;
        actual.lr = expected.lr = 0x81234560u;
        actual.msr = expected.msr = 1u << 13;
        actual.hid2 = expected.hid2 = PPC_HID2_PSE | (enabled ? PPC_HID2_LSQE : 0);
        actual.fpscr = expected.fpscr = 0;
        actual.exception = expected.exception = 0;
        actual.program_exception = expected.program_exception = 0;
        actual.gpr[4] = expected.gpr[4] = addr;
        actual.gqr[0] = expected.gqr[0] = (type << 16) | type | (scale << 24) | (scale << 8);
        actual.fpr[1] = expected.fpr[1] = 123.0;
        actual.ps1[1] = expected.ps1[1] = -456.0;
        if (store_only) {
            const u64 lane0 = raw_edges[iteration];
            const u64 lane1 = ((u64)random_word() << 32) | random_word();
            memcpy(&actual.fpr[1], &lane0, 8); memcpy(&expected.fpr[1], &lane0, 8);
            memcpy(&actual.ps1[1], &lane1, 8); memcpy(&expected.ps1[1], &lane1, 8);
        }
        actual.reserve_valid = expected.reserve_valid = true;
        actual.reserve_addr = expected.reserve_addr = (addr + 8u) & ~31u;
        actual.downcount = expected.downcount = 1000;
        for (unsigned byte = 0; byte < 32; ++byte)
            actual.ram[0x500 + byte] = expected.ram[0x500 + byte] = random_word();
        func_80003100(&actual);
        if (store_only) {
            if (ppc_psq_store(&expected, 1, addr + 8u, single != 0, 0, false, pc))
                expected.pc = expected.lr & ~3u;
        } else if (ppc_psq_load(&expected, 1, addr, single != 0, 0, false, pc)) {
            ppc_psq_store(&expected, 1, addr + 8u, single != 0, 0, false, pc + 4u);
            expected.pc = expected.lr & ~3u;
        }
        if (memcmp(actual.fpr, expected.fpr, sizeof(actual.fpr)) ||
            memcmp(actual.ps1, expected.ps1, sizeof(actual.ps1)) ||
            memcmp(actual.ram + 0x500, expected.ram + 0x500, 32) ||
            actual.exception != expected.exception || actual.fpscr != expected.fpscr ||
            actual.program_exception != expected.program_exception ||
            actual.reserve_valid != expected.reserve_valid || actual.pc != expected.pc ||
            access_count[0] != access_count[1]) {
            fprintf(stderr, "PSQ mismatch type=%u scale=%u single=%u offset=%u enabled=%u iteration=%u pc=%08x/%08x exc=%x/%x\n",
                    type, scale, single, offset, enabled, iteration,
                    actual.pc, expected.pc, actual.exception, expected.exception);
            u64 af, ef, ap, ep;
            memcpy(&af, &actual.fpr[1], 8); memcpy(&ef, &expected.fpr[1], 8);
            memcpy(&ap, &actual.ps1[1], 8); memcpy(&ep, &expected.ps1[1], 8);
            fprintf(stderr, "fpr=%016llx/%016llx ps1=%016llx/%016llx reservation=%u/%u\n",
                    (unsigned long long)af, (unsigned long long)ef,
                    (unsigned long long)ap, (unsigned long long)ep,
                    actual.reserve_valid, expected.reserve_valid);
            for (unsigned byte=0;byte<16;++byte)
                fprintf(stderr,"%02x/%02x ",actual.ram[0x500+byte],expected.ram[0x500+byte]);
            fprintf(stderr,"\n");
            return 1;
        }
        for (unsigned access = 0; access < access_count[0]; ++access) {
            const Access* a = &accesses[0][access];
            const Access* b = &accesses[1][access];
            if (a->address != b->address || a->value != b->value ||
                a->width != b->width || a->write != b->write) {
                fprintf(stderr, "PSQ MMIO access mismatch type=%u scale=%u access=%u\n",
                        type, scale, access);
                return 1;
            }
        }
        ++checked;
    }
    cpu_free(&actual); cpu_free(&expected);
    printf("Dynamic PSQ: %u exact generated-code comparisons passed\n", checked);
    return 0;
}
