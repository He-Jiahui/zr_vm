#ifndef ZR_TEST_SSA_DEOPT_AGGREGATE_FAULT_ALLOCATOR_H
#define ZR_TEST_SSA_DEOPT_AGGREGATE_FAULT_ALLOCATOR_H

#include <stddef.h>

void ssa_deopt_aggregate_fail_allocation(size_t ordinal);
int ssa_deopt_aggregate_allocation_failed(void);

#endif
