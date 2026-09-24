#ifndef ZR_TEST_SSA_ORACLE_RESUME_FAULT_ALLOCATOR_H
#define ZR_TEST_SSA_ORACLE_RESUME_FAULT_ALLOCATOR_H

#include <stddef.h>

void ssa_oracle_resume_fail_allocation(size_t ordinal);
int ssa_oracle_resume_allocation_failed(void);

#endif
