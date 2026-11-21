#include "StaticRecompABI.h"
#include <stdio.h>

extern const StaticRecompModuleDesc* staticrecomp_get_module(void);
int main(void) {
    const StaticRecompModuleDesc* module = staticrecomp_get_module();
    if (module->abi_version != STATICRECOMP_ABI_VERSION ||
        module->cpu_abi_version != GXRUNTIME_CPU_ABI_VERSION ||
        module->cpu_state_size != sizeof(CPUState) || module->num_rel_modules != 1 ||
        module->num_code_ranges != 2 || module->num_chunk_ranges != 2) return 1;
    CPUState cpu = {0};
    for (unsigned i = 0; i < 1000; ++i) {
        cpu.gpr[3] = i; cpu.lr = 0x81234560u; cpu.pc = 0x80003100u; cpu.downcount = 1000;
        if (!module->dispatch(&cpu, cpu.pc) || cpu.gpr[3] != i+1 || cpu.pc != cpu.lr) return 2;
        cpu.pc = 0x80500100u;
        if (!module->dispatch(&cpu, cpu.pc) || cpu.gpr[3] != i+8 || cpu.pc != cpu.lr) return 3;
    }
    if (module->dispatch(&cpu, 0x81234560u)) return 4;
    puts("Native DOL + native REL: ABI, metadata and 2000 dispatches passed");
    return 0;
}
