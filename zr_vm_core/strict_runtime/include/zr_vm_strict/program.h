#ifndef ZR_VM_STRICT_PROGRAM_H
#define ZR_VM_STRICT_PROGRAM_H

#include <stdint.h>
#include <stdio.h>

#include "zr_vm_strict/ownership.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The strict executable has its own magic and version; managed artifacts
 * must never be interpreted as strict programs. These limits precede allocation. */
#define ZR_STRICT_PROGRAM_ABI 1u
#define ZR_STRICT_MAX_SLOTS 4096u
#define ZR_STRICT_MAX_INSTRUCTIONS 1048576u

typedef enum ZrStrictKind {
    ZR_STRICT_VALUE_VOID = 0,
    ZR_STRICT_VALUE_I64 = 1,
    ZR_STRICT_VALUE_BOOL = 2
} ZrStrictKind;

typedef enum ZrStrictOp {
    ZR_STRICT_OP_CONST = 1,
    ZR_STRICT_OP_COPY,
    ZR_STRICT_OP_ADD,
    ZR_STRICT_OP_SUB,
    ZR_STRICT_OP_MUL,
    ZR_STRICT_OP_DIV,
    ZR_STRICT_OP_MOD,
    ZR_STRICT_OP_NEG,
    ZR_STRICT_OP_EQ,
    ZR_STRICT_OP_NE,
    ZR_STRICT_OP_LT,
    ZR_STRICT_OP_LE,
    ZR_STRICT_OP_GT,
    ZR_STRICT_OP_GE,
    ZR_STRICT_OP_NOT,
    ZR_STRICT_OP_JUMP,
    ZR_STRICT_OP_JUMP_IF_FALSE,
    ZR_STRICT_OP_RETURN,
    ZR_STRICT_OP_HALT
} ZrStrictOp;

typedef struct ZrStrictInstruction {
    uint32_t op;
    uint32_t dst;
    uint32_t lhs;
    uint32_t rhs;
    uint64_t immediate;
} ZrStrictInstruction;

typedef struct ZrStrictProgram {
    uint8_t *kinds;
    uint32_t slotCount;
    ZrStrictInstruction *instructions;
    uint32_t instructionCount;
} ZrStrictProgram;

typedef struct ZrStrictResult {
    ZrStrictKind kind;
    int64_t scalar;
} ZrStrictResult;

typedef enum ZrStrictProgramStatus {
    ZR_STRICT_PROGRAM_OK = 0,
    ZR_STRICT_PROGRAM_BAD_FORMAT,
    ZR_STRICT_PROGRAM_BAD_VERSION,
    ZR_STRICT_PROGRAM_BAD_LAYOUT,
    ZR_STRICT_PROGRAM_BAD_INSTRUCTION,
    ZR_STRICT_PROGRAM_OUT_OF_MEMORY,
    ZR_STRICT_PROGRAM_IO_ERROR,
    ZR_STRICT_PROGRAM_FAULT
} ZrStrictProgramStatus;

/* Read and Write use canonical little-endian records, not native struct
 * images. Read requires a zero-initialized output and leaves it empty on error. */
ZrStrictProgramStatus ZrStrictProgram_Read(FILE *file, ZrStrictProgram *out);
ZrStrictProgramStatus ZrStrictProgram_Write(FILE *file, const ZrStrictProgram *program);
/* Emits direct C control flow and uses the same scalar runtime helpers. */
ZrStrictProgramStatus ZrStrictProgram_EmitC(FILE *file, const ZrStrictProgram *program);
void ZrStrictProgram_Free(ZrStrictProgram *program);
ZrStrictProgramStatus ZrStrictProgram_Verify(const ZrStrictProgram *program, uint32_t *badPc);
ZrStrictProgramStatus ZrStrictProgram_Run(const ZrStrictProgram *program, ZrStrictResult *result, uint32_t *badPc);

/* Used by both the interpreter and emitted C, after type verification. */
ZrStrictProgramStatus ZrStrictProgram_Scalar(uint32_t op, int64_t lhs, int64_t rhs, int64_t *result);
int64_t ZrStrictProgram_IntBits(uint64_t bits);
const char *ZrStrictProgram_StatusName(ZrStrictProgramStatus status);

#ifdef __cplusplus
}
#endif
#endif
