#ifndef ZR_TEST_SSA_STATE_MAP_CLONE_FAULT_ALLOCATOR_H
#define ZR_TEST_SSA_STATE_MAP_CLONE_FAULT_ALLOCATOR_H

#include <stddef.h>

void ssa_state_map_clone_fail_allocation(size_t ordinal);
int ssa_state_map_clone_allocation_failed(void);
size_t ssa_state_map_clone_allocation_attempts(void);
size_t ssa_state_map_clone_outstanding_allocations(void);

#endif
