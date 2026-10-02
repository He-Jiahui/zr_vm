#ifndef ZR_EXEC_IR_CLONE_TEST_ALLOCATOR_H
#define ZR_EXEC_IR_CLONE_TEST_ALLOCATOR_H

#if defined(ZR_EXEC_IR_TEST_ALLOCATOR)

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

void *zr_exec_ir_test_malloc(size_t size);
void *zr_exec_ir_test_calloc(size_t count, size_t size);
void *zr_exec_ir_test_realloc(void *storage, size_t size);
void zr_exec_ir_test_free(void *storage);

#define malloc zr_exec_ir_test_malloc
#define calloc zr_exec_ir_test_calloc
#define realloc zr_exec_ir_test_realloc
#define free zr_exec_ir_test_free

#endif

#endif
