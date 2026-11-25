#include "program_ownership.h"

#include <stdlib.h>
#include <string.h>

/* Bound the verifier's temporary storage even for untrusted executables. */
#define ZR_STRICT_FLOW_BUDGET (64u * 1024u * 1024u)

typedef struct ZrStrictOwnershipFact {
    uint16_t base; /* 1-based root slot for a borrow */
    uint16_t readers;
    uint8_t available;
    uint8_t writer;
} ZrStrictOwnershipFact;

typedef struct ZrStrictOwnershipBlock {
    uint32_t firstPc;
    unsigned reached;
} ZrStrictOwnershipBlock;

static int kind_is(const ZrStrictProgram *p, uint32_t slot, uint8_t kind) {
    return slot < p->slotCount && p->kinds[slot] == kind;
}

static int record_data_kind(uint8_t kind) {
    return kind == ZR_STRICT_VALUE_RECORD || kind == ZR_STRICT_VALUE_UNIQUE ||
           kind == ZR_STRICT_VALUE_SHARED || zr_strict_is_record_borrow(kind);
}

static int can_consume(const ZrStrictOwnershipFact *f) {
    return f->available && !f->readers && !f->writer;
}

/* Check operand types even in unreachable code. Only the second pass uses
 * path-dependent state, so malformed operands cannot index a flow table. */
static int verify_owner_shape(const ZrStrictProgram *p, const ZrStrictInstruction *i) {
    const uint32_t dst = i->dst, src = i->lhs;
    const uint8_t destination = dst < p->slotCount ? p->kinds[dst] : 0;
    const uint8_t source = src < p->slotCount ? p->kinds[src] : 0;
    const ZrStrictRecordLayout *layout;
    if (i->rhs && i->op != ZR_STRICT_OP_BYTE_READ && i->op != ZR_STRICT_OP_BYTE_WRITE) return 0;
    switch (i->op) {
        case ZR_STRICT_OP_MANUAL_TRY_ALLOC:
            return destination == ZR_STRICT_VALUE_MANUAL_BLOCK && source == ZR_STRICT_VALUE_I64;
        case ZR_STRICT_OP_BYTE_LENGTH:
            return destination == ZR_STRICT_VALUE_I64 && zr_strict_is_byte_view(source) && !i->immediate;
        case ZR_STRICT_OP_BYTE_READ:
            return destination == ZR_STRICT_VALUE_I64 && zr_strict_is_byte_view(source) &&
                   kind_is(p, i->rhs, ZR_STRICT_VALUE_I64) && !i->immediate;
        case ZR_STRICT_OP_BYTE_WRITE:
            return destination == ZR_STRICT_VALUE_BYTE_VIEW && source == ZR_STRICT_VALUE_I64 &&
                   kind_is(p, i->rhs, ZR_STRICT_VALUE_I64) && !i->immediate;
        case ZR_STRICT_OP_INIT_RECORD:
        case ZR_STRICT_OP_OWN_RECORD:
            if (!kind_is(p, dst, i->op == ZR_STRICT_OP_INIT_RECORD ? ZR_STRICT_VALUE_RECORD : ZR_STRICT_VALUE_UNIQUE) ||
                (i->op == ZR_STRICT_OP_INIT_RECORD && i->immediate)) return 0;
            layout = &p->layouts[p->slotLayouts[dst] - 1];
            if (src > p->slotCount || layout->fieldCount > p->slotCount - src) return 0;
            for (uint32_t field = 0; field < layout->fieldCount; ++field)
                if (!kind_is(p, src + field, layout->fields[field])) return 0;
            return 1;
        case ZR_STRICT_OP_TRANSFER:
        case ZR_STRICT_OP_SHARE:
        case ZR_STRICT_OP_CLONE:
        case ZR_STRICT_OP_DEGRADE:
        case ZR_STRICT_OP_WAKE:
            if (destination < ZR_STRICT_VALUE_RECORD || source < ZR_STRICT_VALUE_RECORD ||
                zr_strict_is_borrow(destination) || zr_strict_is_borrow(source) || i->immediate ||
                (p->slotLayouts && p->slotLayouts[dst] != p->slotLayouts[src])) return 0;
            if (i->op == ZR_STRICT_OP_TRANSFER) return source == destination;
            if (i->op == ZR_STRICT_OP_SHARE) return source == ZR_STRICT_VALUE_UNIQUE && destination == ZR_STRICT_VALUE_SHARED;
            if (i->op == ZR_STRICT_OP_DEGRADE) return source == ZR_STRICT_VALUE_SHARED && destination == ZR_STRICT_VALUE_WEAK;
            if (i->op == ZR_STRICT_OP_WAKE) return source == ZR_STRICT_VALUE_WEAK && destination == ZR_STRICT_VALUE_MAYBE_SHARED;
            return source == destination && (source == ZR_STRICT_VALUE_RECORD || source == ZR_STRICT_VALUE_SHARED ||
                                            source == ZR_STRICT_VALUE_WEAK || source == ZR_STRICT_VALUE_MAYBE_SHARED);
        case ZR_STRICT_OP_IS_NULL:
            return destination == ZR_STRICT_VALUE_BOOL && !i->immediate &&
                   (source == ZR_STRICT_VALUE_MAYBE_SHARED || source == ZR_STRICT_VALUE_MANUAL_BLOCK);
        case ZR_STRICT_OP_DROP:
            return !dst && source >= ZR_STRICT_VALUE_RECORD && !zr_strict_is_borrow(source) && !i->immediate;
        case ZR_STRICT_OP_BORROW:
            if (zr_strict_is_byte_view(destination)) return source == ZR_STRICT_VALUE_MANUAL_BLOCK && !i->immediate;
            return zr_strict_is_record_borrow(destination) && record_data_kind(source) && !zr_strict_is_borrow(source) &&
                   !(destination == ZR_STRICT_VALUE_BORROW && source == ZR_STRICT_VALUE_SHARED) &&
                   !i->immediate && p->slotLayouts[dst] == p->slotLayouts[src];
        case ZR_STRICT_OP_END_BORROW:
            return !dst && zr_strict_is_borrow(source) && !i->immediate;
        case ZR_STRICT_OP_FIELD_READ:
        case ZR_STRICT_OP_FIELD_WRITE: {
            const uint32_t record = i->op == ZR_STRICT_OP_FIELD_READ ? src : dst;
            const uint32_t scalar = i->op == ZR_STRICT_OP_FIELD_READ ? dst : src;
            const uint8_t recordKind = i->op == ZR_STRICT_OP_FIELD_READ ? source : destination;
            if (!record_data_kind(recordKind)) return 0;
            layout = &p->layouts[p->slotLayouts[record] - 1];
            if (i->immediate >= layout->fieldCount || !kind_is(p, scalar, layout->fields[i->immediate])) return 0;
            if (i->op == ZR_STRICT_OP_FIELD_READ) return 1;
            return recordKind != ZR_STRICT_VALUE_SHARED && recordKind != ZR_STRICT_VALUE_READONLY_BORROW &&
                   !(layout->readonlyMask & (1u << i->immediate));
        }
        default: return 0;
    }
}

static int apply_owner_flow(const ZrStrictProgram *p, const ZrStrictInstruction *i, ZrStrictOwnershipFact *facts) {
    const uint32_t dst = i->dst, src = i->lhs;
    switch (i->op) {
        case ZR_STRICT_OP_INIT_RECORD:
        case ZR_STRICT_OP_OWN_RECORD:
        case ZR_STRICT_OP_MANUAL_TRY_ALLOC:
            if (facts[dst].available) return 0;
            break;
        case ZR_STRICT_OP_TRANSFER:
        case ZR_STRICT_OP_SHARE:
            if (facts[dst].available || !can_consume(&facts[src])) return 0;
            memset(&facts[src], 0, sizeof(facts[src]));
            break;
        case ZR_STRICT_OP_CLONE:
        case ZR_STRICT_OP_DEGRADE:
        case ZR_STRICT_OP_WAKE:
            if (facts[dst].available || !facts[src].available || facts[src].writer) return 0;
            break;
        case ZR_STRICT_OP_IS_NULL:
            return facts[src].available;
        case ZR_STRICT_OP_DROP:
            if (!can_consume(&facts[src])) return 0;
            memset(&facts[src], 0, sizeof(facts[src]));
            return 1;
        case ZR_STRICT_OP_BORROW:
            if (!facts[src].available || facts[src].writer || facts[dst].available) return 0;
            if (zr_strict_borrow_is_writable(p->kinds[dst])) {
                if (facts[src].readers) return 0;
                facts[src].writer = 1;
            } else {
                ++facts[src].readers;
            }
            facts[dst].base = (uint16_t)(src + 1);
            break;
        case ZR_STRICT_OP_END_BORROW:
            if (!facts[src].available) return 0;
            if (zr_strict_borrow_is_writable(p->kinds[src])) facts[facts[src].base - 1].writer = 0;
            else --facts[facts[src].base - 1].readers;
            memset(&facts[src], 0, sizeof(facts[src]));
            return 1;
        case ZR_STRICT_OP_FIELD_READ:
        case ZR_STRICT_OP_BYTE_LENGTH:
        case ZR_STRICT_OP_BYTE_READ:
            return facts[src].available && !facts[src].writer;
        case ZR_STRICT_OP_FIELD_WRITE:
        case ZR_STRICT_OP_BYTE_WRITE:
            return facts[dst].available && !facts[dst].writer && !facts[dst].readers;
        default: return 0;
    }
    facts[dst].available = 1;
    return 1;
}

static int is_jump(uint32_t op) { return op == ZR_STRICT_OP_JUMP || op == ZR_STRICT_OP_JUMP_IF_FALSE; }
static int is_exit(uint32_t op) { return op == ZR_STRICT_OP_RETURN || op == ZR_STRICT_OP_HALT; }

static int merge_block(ZrStrictOwnershipBlock *blocks, unsigned char *snapshots, size_t rowSize,
                       uint32_t *pending, uint32_t *pendingCount, uint32_t idx, const ZrStrictOwnershipFact *facts) {
    unsigned char *saved = snapshots + idx * rowSize;
    if (blocks[idx].reached) return !memcmp(saved, facts, rowSize);
    memcpy(saved, facts, rowSize);
    blocks[idx].reached = 1;
    pending[(*pendingCount)++] = idx;
    return 1;
}

static ZrStrictProgramStatus verify_flow(const ZrStrictProgram *p, uint32_t *badPc, ZrStrictOwnershipFact *facts,
                                         size_t rowSize) {
    ZrStrictProgramStatus result = ZR_STRICT_PROGRAM_OUT_OF_MEMORY;
    ZrStrictOwnershipBlock *blocks = NULL;
    unsigned char *snapshots = NULL;
    uint32_t *pending = NULL;
    uint32_t *leader = calloc(p->instructionCount, sizeof(*leader));
    uint32_t blockCount = 0, pendingCount = 0, visit = 0, pc = 0;
    if (!leader) goto done;
    leader[0] = 1;
    for (pc = 0; pc < p->instructionCount; ++pc) {
        const ZrStrictInstruction *i = &p->instructions[pc];
        if (is_jump(i->op)) leader[i->immediate] = 1;
        if ((is_jump(i->op) || is_exit(i->op)) && pc + 1 < p->instructionCount) leader[pc + 1] = 1;
    }
    for (pc = 0; pc < p->instructionCount; ++pc) if (leader[pc]) leader[pc] = ++blockCount;
    if (blockCount > ZR_STRICT_FLOW_BUDGET / (rowSize + sizeof(*blocks) + sizeof(*pending))) {
        result = ZR_STRICT_PROGRAM_BAD_LAYOUT;
        goto done;
    }
    blocks = calloc(blockCount, sizeof(*blocks));
    snapshots = malloc(rowSize * blockCount);
    pending = malloc(sizeof(*pending) * blockCount);
    if (!blocks || !snapshots || !pending) goto done;
    for (pc = 0; pc < p->instructionCount; ++pc) if (leader[pc]) blocks[leader[pc] - 1].firstPc = pc;
    (void)merge_block(blocks, snapshots, rowSize, pending, &pendingCount, 0, facts);
    while (visit < pendingCount) {
        uint32_t current = pending[visit++];
        memcpy(facts, snapshots + current * rowSize, rowSize);
        pc = blocks[current].firstPc;
        for (;;) {
            const ZrStrictInstruction *i = &p->instructions[pc];
            if (is_exit(i->op)) break;
            if (is_jump(i->op)) {
                const uint32_t target = (uint32_t)i->immediate;
                if (!merge_block(blocks, snapshots, rowSize, pending, &pendingCount, leader[target] - 1, facts)) {
                    pc = target;
                    goto mismatch;
                }
                if (i->op == ZR_STRICT_OP_JUMP) break;
            } else if (i->op > ZR_STRICT_OP_HALT && !apply_owner_flow(p, i, facts)) {
                goto mismatch;
            }
            // The scalar instruction verifier requires a terminator at the end.
            ++pc;
            if (leader[pc]) {
                if (!merge_block(blocks, snapshots, rowSize, pending, &pendingCount, leader[pc] - 1, facts)) goto mismatch;
                break;
            }
        }
    }
    result = ZR_STRICT_PROGRAM_OK;
    goto done;
mismatch:
    if (badPc) *badPc = pc;
    result = ZR_STRICT_PROGRAM_BAD_INSTRUCTION;
done:
    free(pending);
    free(snapshots);
    free(blocks);
    free(leader);
    return result;
}

ZrStrictProgramStatus zr_strict_verify_ownership(const ZrStrictProgram *p, uint32_t *badPc) {
    ZrStrictOwnershipFact facts[ZR_STRICT_MAX_SLOTS];
    const size_t rowSize = (size_t)(p->slotCount ? p->slotCount : 1) * sizeof(*facts);
    int hasBranches = 0;
    memset(facts, 0, rowSize);
    for (uint32_t pc = 0; pc < p->instructionCount; ++pc) {
        const ZrStrictInstruction *i = &p->instructions[pc];
        if (is_jump(i->op)) hasBranches = 1;
        if (i->op > ZR_STRICT_OP_HALT && !verify_owner_shape(p, i)) {
            if (badPc) *badPc = pc;
            return ZR_STRICT_PROGRAM_BAD_INSTRUCTION;
        }
    }
    if (hasBranches) return verify_flow(p, badPc, facts, rowSize);
    for (uint32_t pc = 0; pc < p->instructionCount; ++pc) {
        const ZrStrictInstruction *i = &p->instructions[pc];
        if (is_exit(i->op)) break;
        if (i->op > ZR_STRICT_OP_HALT && !apply_owner_flow(p, i, facts)) {
            if (badPc) *badPc = pc;
            return ZR_STRICT_PROGRAM_BAD_INSTRUCTION;
        }
    }
    return ZR_STRICT_PROGRAM_OK;
}
