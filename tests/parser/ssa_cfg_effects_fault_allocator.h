#ifndef ZR_TEST_SSA_CFG_EFFECTS_FAULT_ALLOCATOR_H
#define ZR_TEST_SSA_CFG_EFFECTS_FAULT_ALLOCATOR_H

#include <stddef.h>
#include "zr_vm_common.h"

void ssa_cfg_effects_fault_fail_reallocation(size_t ordinal);
size_t ssa_cfg_effects_fault_reallocation_attempts(void);
TZrBool ssa_cfg_effects_fault_reallocation_failed(void);

#endif
