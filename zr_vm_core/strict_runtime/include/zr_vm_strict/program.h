#ifndef ZR_VM_STRICT_PROGRAM_H
#define ZR_VM_STRICT_PROGRAM_H

#include <stdint.h>
#include <stdio.h>

#include "zr_vm_strict/pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The strict executable has its own magic and version; managed artifacts
 * must never be interpreted as strict programs. These limits precede allocation. */
#define ZR_STRICT_PROGRAM_ABI 8u
#define ZR_STRICT_MAX_SLOTS 4096u
#define ZR_STRICT_MAX_INSTRUCTIONS 1048576u
#define ZR_STRICT_MAX_LAYOUTS 1024u
#define ZR_STRICT_MAX_FIELDS 16u
#define ZR_STRICT_MAX_DESTRUCTOR_SLOTS 256u
#define ZR_STRICT_MAX_DESTRUCTOR_INSTRUCTIONS 4096u
#define ZR_STRICT_LAYOUT_NAME_SIZE 64u

typedef enum ZrStrictKind {
    ZR_STRICT_VALUE_VOID = 0,
    ZR_STRICT_VALUE_I64 = 1,
    ZR_STRICT_VALUE_BOOL = 2,
    ZR_STRICT_VALUE_RECORD,
    ZR_STRICT_VALUE_UNIQUE,
    ZR_STRICT_VALUE_SHARED,
    ZR_STRICT_VALUE_WEAK,
    ZR_STRICT_VALUE_MAYBE_SHARED,
    ZR_STRICT_VALUE_BORROW,
    ZR_STRICT_VALUE_READONLY_BORROW,
    ZR_STRICT_VALUE_MANUAL_BLOCK,
    ZR_STRICT_VALUE_BYTE_VIEW,
    ZR_STRICT_VALUE_READONLY_BYTE_VIEW,
    ZR_STRICT_VALUE_POOL,
    ZR_STRICT_VALUE_POOL_HANDLE
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
    ZR_STRICT_OP_HALT,
    ZR_STRICT_OP_INIT_RECORD,
    ZR_STRICT_OP_OWN_RECORD,
    ZR_STRICT_OP_TRANSFER,
    ZR_STRICT_OP_SHARE,
    ZR_STRICT_OP_CLONE,
    ZR_STRICT_OP_DEGRADE,
    ZR_STRICT_OP_WAKE,
    ZR_STRICT_OP_IS_NULL,
    ZR_STRICT_OP_DROP,
    ZR_STRICT_OP_BORROW,
    ZR_STRICT_OP_END_BORROW,
    ZR_STRICT_OP_FIELD_READ,
    ZR_STRICT_OP_FIELD_WRITE,
    ZR_STRICT_OP_MANUAL_TRY_ALLOC,
    ZR_STRICT_OP_BYTE_LENGTH,
    ZR_STRICT_OP_BYTE_READ,
    ZR_STRICT_OP_BYTE_WRITE,
    ZR_STRICT_OP_BORROW_FIELD,
    ZR_STRICT_OP_UNWRAP_SHARED,
    ZR_STRICT_OP_POOL_CREATE,
    ZR_STRICT_OP_POOL_RENT,
    ZR_STRICT_OP_POOL_CONTAINS,
    ZR_STRICT_OP_POOL_RECYCLE,
    ZR_STRICT_OP_POOL_VIEW,
    ZR_STRICT_OP_POOL_SHRINK
} ZrStrictOp;

typedef struct ZrStrictInstruction {
    uint32_t op;
    uint32_t dst;
    uint32_t lhs;
    uint32_t rhs;
    uint64_t immediate;
} ZrStrictInstruction;

/* A descriptor is either entirely empty or an allocation-free routine with
 * a live receiver in slot zero. It shares the containing program's layouts. */
typedef struct ZrStrictDestructor {
    uint8_t *kinds;
    uint32_t *slotLayouts;
    ZrStrictInstruction *instructions;
    uint32_t slotCount;
    uint32_t instructionCount;
} ZrStrictDestructor;

typedef struct ZrStrictRecordLayout {
    uint32_t fieldCount;
    uint32_t readonlyMask;
    uint8_t fields[ZR_STRICT_MAX_FIELDS];
    uint32_t fieldLayouts[ZR_STRICT_MAX_FIELDS];
    char name[ZR_STRICT_LAYOUT_NAME_SIZE];
    ZrStrictDestructor destructor;
} ZrStrictRecordLayout;

typedef struct ZrStrictProgram {
    uint8_t *kinds;
    uint32_t slotCount;
    ZrStrictInstruction *instructions;
    uint32_t instructionCount;
    /* Record values and their handles name a one-based layout. Scalars and
     * the built-in manual byte block and its views use 0. */
    uint32_t *slotLayouts;
    ZrStrictRecordLayout *layouts;
    uint32_t layoutCount;
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
    ZR_STRICT_PROGRAM_FAULT,
    ZR_STRICT_PROGRAM_OWNERSHIP_FAULT,
    ZR_STRICT_PROGRAM_BYTE_RANGE_FAULT,
    ZR_STRICT_PROGRAM_DESTRUCTOR_FAULT
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

/* The interpreter and generated C supply automatic storage for typed slots.
 * The slot representation is private to the frame helpers. */
typedef union ZrStrictCell {
    int64_t scalar;
    ZrStrictUnique unique;
    ZrStrictShared shared;
    ZrStrictWeak weak;
    ZrStrictManualBlock manual;
} ZrStrictCell;

typedef struct ZrStrictFrameSlot {
    union {
        ZrStrictCell cell;
        ZrStrictCell record[ZR_STRICT_MAX_FIELDS];
        struct {
            const ZrStrictCell *data;
            ZrStrictPoolView lease;
        } recordView;
        ZrStrictPool pool;
        ZrStrictPoolHandle poolHandle;
        ZrStrictCell *writableRecord;
        const ZrStrictManualBlock *byteView;
    } value;
    unsigned live;
} ZrStrictFrameSlot;
typedef struct ZrStrictFrame {
    const ZrStrictProgram *program;
    ZrStrictFrameSlot *slots;
    /* A verified destructor receives a live external record in slot zero. */
    ZrStrictCell *externalRecord;
    ZrStrictRuntime ownership;
} ZrStrictFrame;
/* Use verified instructions only. The caller keeps `program` and `slots`
 * alive through Close, and closes the frame on each successful Open. */
ZrStrictProgramStatus ZrStrictFrame_Open(ZrStrictFrame *frame, const ZrStrictProgram *program,
                                        ZrStrictFrameSlot *slots, uint32_t capacity,
                                        const ZrStrictAllocator *allocator);
ZrStrictProgramStatus ZrStrictFrame_Close(ZrStrictFrame *frame);
ZrStrictProgramStatus ZrStrictFrame_Apply(ZrStrictFrame *frame, const ZrStrictInstruction *instruction);
int64_t ZrStrictFrame_Scalar(const ZrStrictFrame *frame, uint32_t slot);

/* Used by both the interpreter and emitted C, after type verification. */
ZrStrictProgramStatus ZrStrictProgram_Scalar(uint32_t op, int64_t lhs, int64_t rhs, int64_t *result);
int64_t ZrStrictProgram_IntBits(uint64_t bits);
const char *ZrStrictProgram_StatusName(ZrStrictProgramStatus status);

#ifdef __cplusplus
}
#endif
#endif
