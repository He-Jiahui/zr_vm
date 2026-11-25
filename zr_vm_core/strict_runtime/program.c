#include "program_ownership.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* This magic denotes StrictNoGC as well as distinguishing the object format. */
static const unsigned char magic[8] = {'Z', 'R', 'S', 'T', '\r', '\n', 26, '\n'};

static int valid_slot(const ZrStrictProgram *p, uint32_t index, ZrStrictKind kind) {
    return index < p->slotCount && (kind == ZR_STRICT_VALUE_VOID || p->kinds[index] == kind);
}

static int scalar_slot(const ZrStrictProgram *p, uint32_t index) {
    return index < p->slotCount && zr_strict_is_scalar(p->kinds[index]);
}

static int verify_instruction(const ZrStrictProgram *p, uint32_t pc, int usesFrame) {
    const ZrStrictInstruction *i = &p->instructions[pc];
    if (pc + 1 == p->instructionCount && i->op != ZR_STRICT_OP_JUMP &&
        i->op != ZR_STRICT_OP_RETURN && i->op != ZR_STRICT_OP_HALT) {
        return 0;
    }
    switch (i->op) {
        case ZR_STRICT_OP_CONST:
            return scalar_slot(p, i->dst) && !i->lhs && !i->rhs &&
                   (p->kinds[i->dst] == ZR_STRICT_VALUE_I64 || i->immediate <= 1);
        case ZR_STRICT_OP_COPY:
            return scalar_slot(p, i->dst) &&
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
                   scalar_slot(p, i->lhs) &&
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
            return !i->dst && scalar_slot(p, i->lhs) && !i->rhs && !i->immediate;
        case ZR_STRICT_OP_HALT:
            return !i->dst && !i->lhs && !i->rhs && !i->immediate;
        default:
            return usesFrame && i->op >= ZR_STRICT_OP_INIT_RECORD && i->op <= ZR_STRICT_OP_BYTE_WRITE;
    }
}

static ZrStrictProgramStatus verify_code(const ZrStrictProgram *p, uint32_t *badPc, int liveReceiver) {
    for (uint32_t slot = 0; slot < p->slotCount; ++slot) {
        uint8_t kind = p->kinds[slot];
        uint32_t type = p->slotLayouts ? p->slotLayouts[slot] : 0;
        if (kind >= ZR_STRICT_VALUE_RECORD && kind <= ZR_STRICT_VALUE_READONLY_BORROW) {
            if (!type || type > p->layoutCount) return ZR_STRICT_PROGRAM_BAD_LAYOUT;
        } else if (type || (!zr_strict_is_scalar(kind) && kind != ZR_STRICT_VALUE_MANUAL_BLOCK &&
                           !zr_strict_is_byte_view(kind))) {
            return ZR_STRICT_PROGRAM_BAD_LAYOUT;
        }
    }
    int usesFrame = zr_strict_uses_frame(p);
    for (uint32_t pc = 0; pc < p->instructionCount; ++pc) {
        if (!verify_instruction(p, pc, usesFrame)) {
            if (badPc) *badPc = pc;
            return ZR_STRICT_PROGRAM_BAD_INSTRUCTION;
        }
    }
    return usesFrame ? zr_strict_verify_ownership(p, badPc, liveReceiver) : ZR_STRICT_PROGRAM_OK;
}

static ZrStrictProgramStatus verify_destructor(const ZrStrictProgram *owner, uint32_t layout) {
    const ZrStrictDestructor *body = &owner->layouts[layout].destructor;
    if (!body->instructionCount) {
        return body->slotCount || body->instructions || body->kinds || body->slotLayouts
                ? ZR_STRICT_PROGRAM_BAD_LAYOUT : ZR_STRICT_PROGRAM_OK;
    }
    if (!body->slotCount || body->slotCount > ZR_STRICT_MAX_DESTRUCTOR_SLOTS ||
        body->instructionCount > ZR_STRICT_MAX_DESTRUCTOR_INSTRUCTIONS || !body->kinds || !body->slotLayouts ||
        !body->instructions || body->kinds[0] != ZR_STRICT_VALUE_RECORD || body->slotLayouts[0] != layout + 1)
        return ZR_STRICT_PROGRAM_BAD_LAYOUT;
    for (uint32_t slot = 0; slot < body->slotCount; ++slot) {
        const uint8_t kind = body->kinds[slot];
        if (!zr_strict_is_scalar(kind) && kind != ZR_STRICT_VALUE_RECORD && !zr_strict_is_record_borrow(kind))
            return ZR_STRICT_PROGRAM_BAD_LAYOUT;
    }
    for (uint32_t pc = 0; pc < body->instructionCount; ++pc) {
        const ZrStrictInstruction *op = &body->instructions[pc];
        /* Reject effects regardless of reachability. The externally owned
         * receiver can be read, written, copied or borrowed, never consumed. */
        if (op->op <= ZR_STRICT_OP_HALT && op->op != ZR_STRICT_OP_RETURN) continue;
        if (op->op == ZR_STRICT_OP_FIELD_READ || op->op == ZR_STRICT_OP_FIELD_WRITE || op->op == ZR_STRICT_OP_END_BORROW)
            continue;
        if ((op->op == ZR_STRICT_OP_INIT_RECORD || op->op == ZR_STRICT_OP_CLONE || op->op == ZR_STRICT_OP_BORROW) && op->dst)
            continue;
        if (op->op == ZR_STRICT_OP_DROP && op->lhs) continue;
        return ZR_STRICT_PROGRAM_BAD_INSTRUCTION;
    }
    const ZrStrictProgram code = zr_strict_destructor_program(owner, body);
    return verify_code(&code, NULL, 1);
}

ZrStrictProgramStatus ZrStrictProgram_Verify(const ZrStrictProgram *p, uint32_t *badPc) {
    if (badPc) *badPc = UINT32_MAX;
    if (!p || p->slotCount > ZR_STRICT_MAX_SLOTS || (p->slotCount && !p->kinds) ||
        !p->instructionCount || p->instructionCount > ZR_STRICT_MAX_INSTRUCTIONS || !p->instructions) {
        return ZR_STRICT_PROGRAM_BAD_LAYOUT;
    }
    if (p->layoutCount > ZR_STRICT_MAX_LAYOUTS || (p->layoutCount && (!p->layouts || !p->slotLayouts))) {
        return ZR_STRICT_PROGRAM_BAD_LAYOUT;
    }
    for (uint32_t index = 0; index < p->layoutCount; ++index) {
        const ZrStrictRecordLayout *layout = &p->layouts[index];
        if (layout->fieldCount > ZR_STRICT_MAX_FIELDS || layout->readonlyMask >> layout->fieldCount ||
            !layout->name[0] || !memchr(layout->name, '\0', sizeof(layout->name))) return ZR_STRICT_PROGRAM_BAD_LAYOUT;
        for (uint32_t field = 0; field < ZR_STRICT_MAX_FIELDS; ++field) {
            if (field < layout->fieldCount ? !zr_strict_is_scalar(layout->fields[field]) : layout->fields[field] != 0)
                return ZR_STRICT_PROGRAM_BAD_LAYOUT;
        }
    }
    size_t destructorInstructions = 0;
    for (uint32_t index = 0; index < p->layoutCount; ++index) {
        destructorInstructions += p->layouts[index].destructor.instructionCount;
        if (destructorInstructions > ZR_STRICT_MAX_INSTRUCTIONS) return ZR_STRICT_PROGRAM_BAD_LAYOUT;
        const ZrStrictProgramStatus status = verify_destructor(p, index);
        if (status != ZR_STRICT_PROGRAM_OK) return status;
    }
    return verify_code(p, badPc, 0);
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

ZrStrictProgramStatus zr_strict_run_frame(ZrStrictFrame *frame, ZrStrictResult *result, uint32_t *badPc) {
    const ZrStrictProgram *p = frame->program;
    ZrStrictProgramStatus status = ZR_STRICT_PROGRAM_OK;
    uint32_t pc = 0;
    while (pc < p->instructionCount) {
        const ZrStrictInstruction *i = &p->instructions[pc];
        if (i->op == ZR_STRICT_OP_RETURN) {
            result->kind = (ZrStrictKind)p->kinds[i->lhs];
            result->scalar = ZrStrictFrame_Scalar(frame, i->lhs);
            if (badPc) *badPc = pc;
            break;
        }
        if (i->op == ZR_STRICT_OP_HALT) {
            if (badPc) *badPc = pc;
            break;
        }
        if (i->op == ZR_STRICT_OP_JUMP ||
            (i->op == ZR_STRICT_OP_JUMP_IF_FALSE && !ZrStrictFrame_Scalar(frame, i->lhs))) {
            pc = (uint32_t)i->immediate;
            continue;
        }
        if (i->op == ZR_STRICT_OP_JUMP_IF_FALSE) { ++pc; continue; }
        status = ZrStrictFrame_Apply(frame, i);
        if (status != ZR_STRICT_PROGRAM_OK) {
            if (badPc) *badPc = pc;
            break;
        }
        ++pc;
    }
    return status;
}

static ZrStrictProgramStatus run_owned(const ZrStrictProgram *p, ZrStrictResult *result, uint32_t *badPc) {
    ZrStrictFrame frame;
    ZrStrictFrameSlot slots[ZR_STRICT_MAX_SLOTS];
    ZrStrictProgramStatus status = ZrStrictFrame_Open(&frame, p, slots, ZR_STRICT_MAX_SLOTS, NULL);
    if (status != ZR_STRICT_PROGRAM_OK) return status;
    uint32_t exitPc = UINT32_MAX;
    status = zr_strict_run_frame(&frame, result, &exitPc);
    const ZrStrictProgramStatus cleanup = ZrStrictFrame_Close(&frame);
    if (badPc && (status != ZR_STRICT_PROGRAM_OK || cleanup != ZR_STRICT_PROGRAM_OK)) *badPc = exitPc;
    return status == ZR_STRICT_PROGRAM_OK ? cleanup : status;
}

ZrStrictProgramStatus ZrStrictProgram_Run(const ZrStrictProgram *p, ZrStrictResult *result, uint32_t *badPc) {
    ZrStrictProgramStatus status = ZrStrictProgram_Verify(p, badPc);
    int64_t slots[ZR_STRICT_MAX_SLOTS];
    uint32_t pc = 0;
    if (status != ZR_STRICT_PROGRAM_OK) return status;
    if (!result) return ZR_STRICT_PROGRAM_BAD_LAYOUT;
    *result = (ZrStrictResult){0};
    if (zr_strict_uses_frame(p)) return run_owned(p, result, badPc);
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

static int write_instructions(FILE *file, const ZrStrictInstruction *instructions, uint32_t count) {
    for (uint32_t pc = 0; pc < count; ++pc) {
        const ZrStrictInstruction *i = &instructions[pc];
        if (!write_uint(file, i->op, 4) || !write_uint(file, i->dst, 4) || !write_uint(file, i->lhs, 4) ||
            !write_uint(file, i->rhs, 4) || !write_uint(file, i->immediate, 8)) return 0;
    }
    return 1;
}

static int write_destructor(FILE *file, const ZrStrictDestructor *body) {
    if (!write_uint(file, body->slotCount, 4) || !write_uint(file, body->instructionCount, 4)) return 0;
    if (!body->slotCount) return 1;
    if (fwrite(body->kinds, 1, body->slotCount, file) != body->slotCount) return 0;
    for (uint32_t slot = 0; slot < body->slotCount; ++slot)
        if (!write_uint(file, body->slotLayouts[slot], 4)) return 0;
    return write_instructions(file, body->instructions, body->instructionCount);
}

ZrStrictProgramStatus ZrStrictProgram_Write(FILE *file, const ZrStrictProgram *program) {
    ZrStrictProgramStatus status = ZrStrictProgram_Verify(program, NULL);
    if (status != ZR_STRICT_PROGRAM_OK) return status;
    if (!file || fwrite(magic, 1, sizeof(magic), file) != sizeof(magic) ||
        !write_uint(file, ZR_STRICT_PROGRAM_ABI, 4) || !write_uint(file, program->slotCount, 4) ||
        !write_uint(file, program->instructionCount, 4) || !write_uint(file, program->layoutCount, 4) ||
        (program->slotCount && fwrite(program->kinds, 1, program->slotCount, file) != program->slotCount)) {
        return ZR_STRICT_PROGRAM_IO_ERROR;
    }
    if (program->layoutCount) {
        for (uint32_t slot = 0; slot < program->slotCount; ++slot) {
            if (!write_uint(file, program->slotLayouts[slot], 4)) return ZR_STRICT_PROGRAM_IO_ERROR;
        }
        for (uint32_t index = 0; index < program->layoutCount; ++index) {
            const ZrStrictRecordLayout *layout = &program->layouts[index];
            if (!write_uint(file, layout->fieldCount, 4) || !write_uint(file, layout->readonlyMask, 4) ||
                fwrite(layout->fields, 1, sizeof(layout->fields), file) != sizeof(layout->fields) ||
                fwrite(layout->name, 1, sizeof(layout->name), file) != sizeof(layout->name) ||
                !write_destructor(file, &layout->destructor)) return ZR_STRICT_PROGRAM_IO_ERROR;
        }
    }
    return write_instructions(file, program->instructions, program->instructionCount)
            ? ZR_STRICT_PROGRAM_OK : ZR_STRICT_PROGRAM_IO_ERROR;
}

void ZrStrictProgram_Free(ZrStrictProgram *p) {
    if (p) {
        free(p->kinds);
        free(p->instructions);
        free(p->slotLayouts);
        if (p->layouts) {
            for (uint32_t index = 0; index < p->layoutCount; ++index) {
                ZrStrictDestructor *body = &p->layouts[index].destructor;
                free(body->kinds);
                free(body->slotLayouts);
                free(body->instructions);
            }
        }
        free(p->layouts);
        *p = (ZrStrictProgram){0};
    }
}

static int read_instructions(FILE *file, ZrStrictInstruction *instructions, uint32_t count) {
    for (uint32_t pc = 0; pc < count; ++pc) {
        uint64_t op, dst, lhs, rhs, immediate;
        if (!read_uint(file, &op, 4) || !read_uint(file, &dst, 4) || !read_uint(file, &lhs, 4) ||
            !read_uint(file, &rhs, 4) || !read_uint(file, &immediate, 8)) return 0;
        instructions[pc] = (ZrStrictInstruction){(uint32_t)op, (uint32_t)dst, (uint32_t)lhs, (uint32_t)rhs, immediate};
    }
    return 1;
}

static ZrStrictProgramStatus read_destructor(FILE *file, ZrStrictDestructor *body, size_t *budget) {
    uint64_t slots, instructions;
    if (!read_uint(file, &slots, 4) || !read_uint(file, &instructions, 4)) return ZR_STRICT_PROGRAM_BAD_FORMAT;
    if (!slots && !instructions) return ZR_STRICT_PROGRAM_OK;
    if (!slots || !instructions || slots > ZR_STRICT_MAX_DESTRUCTOR_SLOTS ||
        instructions > ZR_STRICT_MAX_DESTRUCTOR_INSTRUCTIONS || instructions > *budget) return ZR_STRICT_PROGRAM_BAD_LAYOUT;
    *budget -= (size_t)instructions;
    body->slotCount = (uint32_t)slots;
    body->instructionCount = (uint32_t)instructions;
    body->kinds = malloc(body->slotCount);
    body->slotLayouts = malloc(body->slotCount * sizeof(*body->slotLayouts));
    body->instructions = malloc(body->instructionCount * sizeof(*body->instructions));
    if (!body->kinds || !body->slotLayouts || !body->instructions) return ZR_STRICT_PROGRAM_OUT_OF_MEMORY;
    if (fread(body->kinds, 1, body->slotCount, file) != body->slotCount) return ZR_STRICT_PROGRAM_BAD_FORMAT;
    for (uint32_t slot = 0; slot < body->slotCount; ++slot) {
        uint64_t type;
        if (!read_uint(file, &type, 4)) return ZR_STRICT_PROGRAM_BAD_FORMAT;
        body->slotLayouts[slot] = (uint32_t)type;
    }
    return read_instructions(file, body->instructions, body->instructionCount)
            ? ZR_STRICT_PROGRAM_OK : ZR_STRICT_PROGRAM_BAD_FORMAT;
}

ZrStrictProgramStatus ZrStrictProgram_Read(FILE *file, ZrStrictProgram *out) {
    unsigned char actual[sizeof(magic)];
    uint64_t abi, slots, instructions, layouts;
    ZrStrictProgram p = {0};
    ZrStrictProgramStatus status;
    if (!file || !out || out->kinds || out->slotCount || out->instructions || out->instructionCount ||
        out->slotLayouts || out->layouts || out->layoutCount) {
        return ZR_STRICT_PROGRAM_BAD_FORMAT;
    }
    if (fread(actual, 1, sizeof(actual), file) != sizeof(actual) || memcmp(actual, magic, sizeof(magic)) ||
        !read_uint(file, &abi, 4)) {
        return ZR_STRICT_PROGRAM_BAD_FORMAT;
    }
    if (abi != ZR_STRICT_PROGRAM_ABI) return ZR_STRICT_PROGRAM_BAD_VERSION;
    if (!read_uint(file, &slots, 4) || !read_uint(file, &instructions, 4) || !read_uint(file, &layouts, 4) ||
        layouts > ZR_STRICT_MAX_LAYOUTS || slots > ZR_STRICT_MAX_SLOTS || !instructions || instructions > ZR_STRICT_MAX_INSTRUCTIONS) {
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
    p.layoutCount = (uint32_t)layouts;
    if (p.layoutCount) {
        p.slotLayouts = calloc(p.slotCount ? p.slotCount : 1, sizeof(*p.slotLayouts));
        p.layouts = calloc(p.layoutCount, sizeof(*p.layouts));
        if (!p.slotLayouts || !p.layouts) {
            status = ZR_STRICT_PROGRAM_OUT_OF_MEMORY;
            goto fail;
        }
        for (uint32_t slot = 0; slot < p.slotCount; ++slot) {
            uint64_t type;
            if (!read_uint(file, &type, 4)) goto bad_format;
            p.slotLayouts[slot] = (uint32_t)type;
        }
        size_t destructorBudget = ZR_STRICT_MAX_INSTRUCTIONS;
        for (uint32_t index = 0; index < p.layoutCount; ++index) {
            ZrStrictRecordLayout *layout = &p.layouts[index];
            uint64_t fields, readonly;
            if (!read_uint(file, &fields, 4) || !read_uint(file, &readonly, 4) ||
                fread(layout->fields, 1, sizeof(layout->fields), file) != sizeof(layout->fields) ||
                fread(layout->name, 1, sizeof(layout->name), file) != sizeof(layout->name)) goto bad_format;
            layout->fieldCount = (uint32_t)fields;
            layout->readonlyMask = (uint32_t)readonly;
            status = read_destructor(file, &layout->destructor, &destructorBudget);
            if (status != ZR_STRICT_PROGRAM_OK) goto fail;
        }
    }
    if (!read_instructions(file, p.instructions, p.instructionCount)) goto bad_format;
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
        case ZR_STRICT_PROGRAM_OWNERSHIP_FAULT: return "ownership fault";
        case ZR_STRICT_PROGRAM_BYTE_RANGE_FAULT: return "manual byte index or value out of range";
        case ZR_STRICT_PROGRAM_DESTRUCTOR_FAULT: return "strict destructor fault";
        default: return "unknown strict execution status";
    }
}
