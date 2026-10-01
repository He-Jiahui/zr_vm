#ifndef TESTS_PARSER_SSA_EIS6_FAULT_ALLOCATOR_H
#define TESTS_PARSER_SSA_EIS6_FAULT_ALLOCATOR_H

#include <stddef.h>

/* Private allocator wrappers used only by the static EIS6 decode OOM test. */
void *ZrEis6Test_Malloc(size_t size);
void *ZrEis6Test_Calloc(size_t count, size_t size);
void *ZrEis6Test_Realloc(void *pointer, size_t size);
void ZrEis6Test_Free(void *pointer);

void ZrEis6Test_ArmFailure(size_t allocationOrdinal);
void ZrEis6Test_DisableFailure(void);
size_t ZrEis6Test_AllocationAttempts(void);
size_t ZrEis6Test_OutstandingAllocations(void);
int ZrEis6Test_TrackingOverflowed(void);

#endif
