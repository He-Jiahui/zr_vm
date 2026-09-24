#ifndef ZR_TEST_SSA_OWNER_FAULT_ALLOCATOR_H
#define ZR_TEST_SSA_OWNER_FAULT_ALLOCATOR_H

#include <stddef.h>

void ssa_owner_fail_allocation(size_t ordinal);
int ssa_owner_allocation_failed(void);
size_t ssa_owner_outstanding_allocations(void);

#endif
