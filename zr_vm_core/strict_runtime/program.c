#include "zr_vm_strict/program.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* This magic denotes StrictNoGC as well as distinguishing the object format. */
static const unsigned char magic[8] = {'Z', 'R', 'S', 'T', '\r', '\n', 26, '\n'};

static int valid_slot(const ZrStrictProgram *p, uint32_t index, ZrStrictKind kind) {
    return index < p->slotCount && (kind == ZR_STRICT_VALUE_VOID || p->kinds[index] == kind);
}

static int verify_instruction(const ZrStrictProgram *p, uint32_t pc) {
    const ZrStrictInstruction *i = &p->instructions[pc];
    if (pc + 1 == p->instructionCount && i->op != ZR_STRICT_OP_JUMP &&
        i->op != ZR_STRICT_OP_RETURN && i->op != ZR_STRICT_OP_HALT) {
        return 0;
    }
    switch (i->op) {
        case ZR_STRICT_OP_CONST:
            return valid_slot(p, i->dst, ZR_STRICT_VALUE_VOID) && !i->lhs && !i->rhs &&
                   (p->kinds[i->dst] == ZR_STRICT_VALUE_I64 || i->immediate <= 1);
        case ZR_STRICT_OP_COPY:
            return valid_slot(p, i->dst, ZR_STRICT_VALUE_VOID) &&
                   valid_slot(p, i->lhs, (ZrStrictKind)p->kinds[i->dst]) && !i->rhs && !i->immediate;
        case ZR_STRICT_OP_ADD:
        case ZR_STRICT_OP_SUB:
        case ZR_STRICT_OP_MUL:
        case ZR_STRICT_OP_DIV:
        case ZR_STRICT_OP_MOD:
            return valid_slot(p, i->dst, ZR_STRICT_VALUE_I64) &&
                   valid_slot(p, i->lhs, ZR_STRICT_VALUE_I64) &&
                   valid_slot(p, i->rhs, ZR_STRICT_VALUE_I64) && !i->immediate;
        case ZR_STRICT_OP_EQ:
        case ZR_STRICT_OP_NE:
            return valid_slot(p, i->dst, ZR_STRICT_VALUE_BOOL) &&
                   valid_slot(p, i->lhs, ZR_STRICT_VALUE_VOID) &&
                   valid_slot(p, i->rhs, (ZrStrictKind)p->kinds[i->lhs]) && !i->immediate;
        case ZR_STRICT_OP_LT:
        case ZR_STRICT_OP_LE:
        case ZR_STRICT_OP_GT:
        case ZR_STRICT_OP_GE:
            return valid_slot(p, i->dst, ZR_STRICT_VALUE_BOOL) &&
                   valid_slot(p, i->lhs, ZR_STRICT_VALUE_I64) &&
                   valid_slot(p, i->rhs, ZR_STRICT_VALUE_I64) && !i->immediate;
        case ZR_STRICT_OP_NEG:
        case ZR_STRICT_OP_NOT: {
            ZrStrictKind kind = i->op == ZR_STRICT_OP_NEG ? ZR_STRICT_VALUE_I64 : ZR_STRICT_VALUE_BOOL;
            return valid_slot(p, i->dst, kind) && valid_slot(p, i->lhs, kind) && !i->rhs && !i->immediate;
        }
        case ZR_STRICT_OP_JUMP:
            return !i->dst && !i->lhs && !i->rhs && i->immediate < p->instructionCount;
        case ZR_STRICT_OP_JUMP_IF_FALSE:
            return !i->dst && valid_slot(p, i->lhs, ZR_STRICT_VALUE_BOOL) && !i->rhs &&
                   i->immediate < p->instructionCount;
        case ZR_STRICT_OP_RETURN:
            return !i->dst && valid_slot(p, i->lhs, ZR_STRICT_VALUE_VOID) && !i->rhs && !i->immediate;
        case ZR_STRICT_OP_HALT:
            return !i->dst && !i->lhs && !i->rhs && !i->immediate;
        default:
            return 0;
    }
}

ZrStrictProgramStatus ZrStrictProgram_Verify(const ZrStrictProgram *p, uint32_t *badPc) {
    if (badPc) *badPc = UINT32_MAX;
    if (!p || p->slotCount > ZR_STRICT_MAX_SLOTS || (p->slotCount && !p->kinds) ||
        !p->instructionCount || p->instructionCount > ZR_STRICT_MAX_INSTRUCTIONS || !p->instructions) {
        return ZR_STRICT_PROGRAM_BAD_LAYOUT;
    }
    for (uint32_t slot = 0; slot < p->slotCount; ++slot) {
        if (p->kinds[slot] != ZR_STRICT_VALUE_I64 && p->kinds[slot] != ZR_STRICT_VALUE_BOOL) {
            return ZR_STRICT_PROGRAM_BAD_LAYOUT;
        }
    }
    for (uint32_t pc = 0; pc < p->instructionCount; ++pc) {
        if (!verify_instruction(p, pc)) {
            if (badPc) *badPc = pc;
            return ZR_STRICT_PROGRAM_BAD_INSTRUCTION;
        }
    }
    return ZR_STRICT_PROGRAM_OK;
}

int64_t ZrStrictProgram_IntBits(uint64_t bits) {
    return bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(UINT64_MAX - bits);
}

ZrStrictProgramStatus ZrStrictProgram_Scalar(uint32_t op, int64_t a, int64_t b, int64_t *result) {
    if (!result) return ZR_STRICT_PROGRAM_BAD_LAYOUT;
    switch (op) {
        case ZR_STRICT_OP_ADD:
            if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return ZR_STRICT_PROGRAM_FAULT;
            *result = a + b; break;
        case ZR_STRICT_OP_SUB:
            if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) return ZR_STRICT_PROGRAM_FAULT;
            *result = a - b; break;
        case ZR_STRICT_OP_MUL:
            if ((a > 0 && ((b > 0 && a > INT64_MAX / b) || (b < 0 && b < INT64_MIN / a))) ||
                (a < 0 && ((b > 0 && a < INT64_MIN / b) || (b < 0 && a < INT64_MAX / b)))) {
                return ZR_STRICT_PROGRAM_FAULT;
            }
            *result = a * b; break;
        case ZR_STRICT_OP_DIV:
        case ZR_STRICT_OP_MOD:
            if (!b || (op == ZR_STRICT_OP_DIV && a == INT64_MIN && b == -1)) return ZR_STRICT_PROGRAM_FAULT;
            *result = op == ZR_STRICT_OP_DIV ? a / b : b == -1 ? 0 : a % b;
            break;
        case ZR_STRICT_OP_NEG:
            if (a == INT64_MIN) return ZR_STRICT_PROGRAM_FAULT;
            *result = -a; break;
        case ZR_STRICT_OP_EQ: *result = a == b; break;
        case ZR_STRICT_OP_NE: *result = a != b; break;
        case ZR_STRICT_OP_LT: *result = a < b; break;
        case ZR_STRICT_OP_LE: *result = a <= b; break;
        case ZR_STRICT_OP_GT: *result = a > b; break;
        case ZR_STRICT_OP_GE: *result = a >= b; break;
        case ZR_STRICT_OP_NOT: *result = !a; break;
        default: return ZR_STRICT_PROGRAM_BAD_INSTRUCTION;
    }
    return ZR_STRICT_PROGRAM_OK;
}

ZrStrictProgramStatus ZrStrictProgram_Run(const ZrStrictProgram *p, ZrStrictResult *result, uint32_t *badPc) {
    ZrStrictProgramStatus status = ZrStrictProgram_Verify(p, badPc);
    int64_t slots[ZR_STRICT_MAX_SLOTS];
    uint32_t pc = 0;
    if (status != ZR_STRICT_PROGRAM_OK) return status;
    if (!result) return ZR_STRICT_PROGRAM_BAD_LAYOUT;
    *result = (ZrStrictResult){0};
    memset(slots, 0, (size_t)p->slotCount * sizeof(*slots));
    for (;;) {
        const ZrStrictInstruction *i = &p->instructions[pc];
        switch (i->op) {
            case ZR_STRICT_OP_CONST: slots[i->dst] = ZrStrictProgram_IntBits(i->immediate); break;
            case ZR_STRICT_OP_COPY: slots[i->dst] = slots[i->lhs]; break;
            case ZR_STRICT_OP_JUMP: pc = (uint32_t)i->immediate; continue;
            case ZR_STRICT_OP_JUMP_IF_FALSE:
                if (!slots[i->lhs]) { pc = (uint32_t)i->immediate; continue; }
                break;
            case ZR_STRICT_OP_RETURN:
                result->kind = (ZrStrictKind)p->kinds[i->lhs];
                result->scalar = slots[i->lhs];
                return ZR_STRICT_PROGRAM_OK;
            case ZR_STRICT_OP_HALT: return ZR_STRICT_PROGRAM_OK;
            default:
                status = ZrStrictProgram_Scalar(i->op, slots[i->lhs],
                        (i->op == ZR_STRICT_OP_NEG || i->op == ZR_STRICT_OP_NOT) ? 0 : slots[i->rhs],
                        &slots[i->dst]);
                if (status != ZR_STRICT_PROGRAM_OK) {
                    if (badPc) *badPc = pc;
                    return status;
                }
                break;
        }
        ++pc;
    }
}

static int write_uint(FILE *file, uint64_t value, size_t width) {
    unsigned char bytes[8];
    for (size_t index = 0; index < width; ++index) bytes[index] = (unsigned char)(value >> (8 * index));
    return fwrite(bytes, 1, width, file) == width;
}

static int read_uint(FILE *file, uint64_t *value, size_t width) {
    unsigned char bytes[8];
    if (fread(bytes, 1, width, file) != width) return 0;
    *value = 0;
    for (size_t index = 0; index < width; ++index) *value |= (uint64_t)bytes[index] << (8 * index);
    return 1;
}

ZrStrictProgramStatus ZrStrictProgram_Write(FILE *file, const ZrStrictProgram *program) {
    ZrStrictProgramStatus status = ZrStrictProgram_Verify(program, NULL);
    if (status != ZR_STRICT_PROGRAM_OK) return status;
    if (!file || fwrite(magic, 1, sizeof(magic), file) != sizeof(magic) ||
        !write_uint(file, ZR_STRICT_PROGRAM_ABI, 4) || !write_uint(file, program->slotCount, 4) ||
        !write_uint(file, program->instructionCount, 4) ||
        (program->slotCount && fwrite(program->kinds, 1, program->slotCount, file) != program->slotCount)) {
        return ZR_STRICT_PROGRAM_IO_ERROR;
    }
    for (uint32_t pc = 0; pc < program->instructionCount; ++pc) {
        const ZrStrictInstruction *i = &program->instructions[pc];
        if (!write_uint(file, i->op, 4) || !write_uint(file, i->dst, 4) || !write_uint(file, i->lhs, 4) ||
            !write_uint(file, i->rhs, 4) || !write_uint(file, i->immediate, 8)) {
            return ZR_STRICT_PROGRAM_IO_ERROR;
        }
    }
    return ZR_STRICT_PROGRAM_OK;
}

void ZrStrictProgram_Free(ZrStrictProgram *p) {
    if (p) {
        free(p->kinds);
        free(p->instructions);
        *p = (ZrStrictProgram){0};
    }
}

ZrStrictProgramStatus ZrStrictProgram_Read(FILE *file, ZrStrictProgram *out) {
    unsigned char actual[sizeof(magic)];
    uint64_t abi, slots, instructions;
    ZrStrictProgram p = {0};
    ZrStrictProgramStatus status;
    if (!file || !out || out->kinds || out->slotCount || out->instructions || out->instructionCount) {
        return ZR_STRICT_PROGRAM_BAD_FORMAT;
    }
    if (fread(actual, 1, sizeof(actual), file) != sizeof(actual) || memcmp(actual, magic, sizeof(magic)) ||
        !read_uint(file, &abi, 4)) {
        return ZR_STRICT_PROGRAM_BAD_FORMAT;
    }
    if (abi != ZR_STRICT_PROGRAM_ABI) return ZR_STRICT_PROGRAM_BAD_VERSION;
    if (!read_uint(file, &slots, 4) || !read_uint(file, &instructions, 4) ||
        slots > ZR_STRICT_MAX_SLOTS || !instructions || instructions > ZR_STRICT_MAX_INSTRUCTIONS) {
        return ZR_STRICT_PROGRAM_BAD_LAYOUT;
    }
    p.slotCount = (uint32_t)slots;
    p.instructionCount = (uint32_t)instructions;
    if (p.slotCount) p.kinds = malloc((size_t)p.slotCount);
    p.instructions = malloc((size_t)p.instructionCount * sizeof(*p.instructions));
    if ((p.slotCount && !p.kinds) || !p.instructions) {
        status = ZR_STRICT_PROGRAM_OUT_OF_MEMORY;
        goto fail;
    }
    if (p.slotCount && fread(p.kinds, 1, p.slotCount, file) != p.slotCount) goto bad_format;
    for (uint32_t pc = 0; pc < p.instructionCount; ++pc) {
        uint64_t op, dst, lhs, rhs, immediate;
        if (!read_uint(file, &op, 4) || !read_uint(file, &dst, 4) || !read_uint(file, &lhs, 4) ||
            !read_uint(file, &rhs, 4) || !read_uint(file, &immediate, 8)) goto bad_format;
        p.instructions[pc] = (ZrStrictInstruction){(uint32_t)op, (uint32_t)dst, (uint32_t)lhs, (uint32_t)rhs, immediate};
    }
    if (fgetc(file) != EOF) goto bad_format;
    if (ferror(file)) {
        status = ZR_STRICT_PROGRAM_IO_ERROR;
        goto fail;
    }
    status = ZrStrictProgram_Verify(&p, NULL);
    if (status != ZR_STRICT_PROGRAM_OK) goto fail;
    *out = p;
    return ZR_STRICT_PROGRAM_OK;

bad_format:
    status = ferror(file) ? ZR_STRICT_PROGRAM_IO_ERROR : ZR_STRICT_PROGRAM_BAD_FORMAT;
fail:
    ZrStrictProgram_Free(&p);
    return status;
}

const char *ZrStrictProgram_StatusName(ZrStrictProgramStatus status) {
    switch (status) {
        case ZR_STRICT_PROGRAM_OK: return "ok";
        case ZR_STRICT_PROGRAM_BAD_FORMAT: return "not a StrictNoGC executable";
        case ZR_STRICT_PROGRAM_BAD_VERSION: return "unsupported StrictNoGC executable ABI";
        case ZR_STRICT_PROGRAM_BAD_LAYOUT: return "invalid strict executable layout";
        case ZR_STRICT_PROGRAM_BAD_INSTRUCTION: return "unsupported or ill-typed strict instruction";
        case ZR_STRICT_PROGRAM_OUT_OF_MEMORY: return "out of memory";
        case ZR_STRICT_PROGRAM_IO_ERROR: return "I/O error";
        case ZR_STRICT_PROGRAM_FAULT: return "arithmetic fault";
        default: return "unknown strict execution status";
    }
}
