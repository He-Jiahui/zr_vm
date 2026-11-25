#include "program_ownership.h"

#include <string.h>

typedef struct ZrStrictOwnershipFact {
    uint16_t base; /* 1-based root slot for a borrow */
    uint16_t readers;
    uint8_t available;
    uint8_t writer;
} ZrStrictOwnershipFact;

static int kind_is(const ZrStrictProgram *p, uint32_t slot, uint8_t kind) {
    return slot < p->slotCount && p->kinds[slot] == kind;
}

static int record_data_kind(uint8_t kind) {
    return kind == ZR_STRICT_VALUE_RECORD || kind == ZR_STRICT_VALUE_UNIQUE ||
           kind == ZR_STRICT_VALUE_SHARED || zr_strict_is_borrow(kind);
}

static int can_consume(const ZrStrictOwnershipFact *f) {
    return f->available && !f->readers && !f->writer;
}

static int verify_owner_op(const ZrStrictProgram *p, const ZrStrictInstruction *i, ZrStrictOwnershipFact *facts) {
    const uint32_t dst = i->dst, src = i->lhs;
    const uint8_t destination = dst < p->slotCount ? p->kinds[dst] : 0;
    const uint8_t source = src < p->slotCount ? p->kinds[src] : 0;
    const ZrStrictRecordLayout *layout;
    switch (i->op) {
        case ZR_STRICT_OP_INIT_RECORD:
        case ZR_STRICT_OP_OWN_RECORD:
            if (!kind_is(p, dst, i->op == ZR_STRICT_OP_INIT_RECORD ? ZR_STRICT_VALUE_RECORD : ZR_STRICT_VALUE_UNIQUE) ||
                facts[dst].available || i->rhs || (i->op == ZR_STRICT_OP_INIT_RECORD && i->immediate)) return 0;
            layout = &p->layouts[p->slotLayouts[dst] - 1];
            if (src > p->slotCount || layout->fieldCount > p->slotCount - src) return 0;
            for (uint32_t field = 0; field < layout->fieldCount; ++field) {
                if (!kind_is(p, src + field, layout->fields[field])) return 0;
            }
            break;
        case ZR_STRICT_OP_TRANSFER:
        case ZR_STRICT_OP_SHARE:
        case ZR_STRICT_OP_CLONE:
        case ZR_STRICT_OP_DEGRADE:
        case ZR_STRICT_OP_WAKE:
            if (destination < ZR_STRICT_VALUE_RECORD || source < ZR_STRICT_VALUE_RECORD ||
                zr_strict_is_borrow(destination) || zr_strict_is_borrow(source) || facts[dst].available ||
                !facts[src].available || i->rhs || i->immediate || p->slotLayouts[dst] != p->slotLayouts[src]) return 0;
            if (i->op == ZR_STRICT_OP_TRANSFER) {
                if (source != destination || !can_consume(&facts[src])) return 0;
                facts[src].available = 0;
            } else if (i->op == ZR_STRICT_OP_SHARE) {
                if (source != ZR_STRICT_VALUE_UNIQUE || destination != ZR_STRICT_VALUE_SHARED ||
                    !can_consume(&facts[src])) return 0;
                facts[src].available = 0;
            } else if (i->op == ZR_STRICT_OP_CLONE) {
                if (source != destination || facts[src].writer ||
                    (source != ZR_STRICT_VALUE_RECORD && source != ZR_STRICT_VALUE_SHARED && source != ZR_STRICT_VALUE_WEAK &&
                     source != ZR_STRICT_VALUE_MAYBE_SHARED)) return 0;
            } else if (i->op == ZR_STRICT_OP_DEGRADE) {
                if (source != ZR_STRICT_VALUE_SHARED || destination != ZR_STRICT_VALUE_WEAK) return 0;
            } else if (source != ZR_STRICT_VALUE_WEAK || destination != ZR_STRICT_VALUE_MAYBE_SHARED) {
                return 0;
            }
            break;
        case ZR_STRICT_OP_IS_NULL:
            return destination == ZR_STRICT_VALUE_BOOL && source == ZR_STRICT_VALUE_MAYBE_SHARED &&
                   facts[src].available && !i->rhs && !i->immediate;
        case ZR_STRICT_OP_DROP:
            if (i->dst || i->rhs || i->immediate || source < ZR_STRICT_VALUE_RECORD ||
                zr_strict_is_borrow(source) || !can_consume(&facts[src])) return 0;
            facts[src].available = 0;
            return 1;
        case ZR_STRICT_OP_BORROW:
            if (!zr_strict_is_borrow(destination) || !record_data_kind(source) || zr_strict_is_borrow(source) ||
                !facts[src].available || facts[src].writer || facts[dst].available || i->rhs || i->immediate ||
                p->slotLayouts[dst] != p->slotLayouts[src]) return 0;
            if (destination == ZR_STRICT_VALUE_BORROW) {
                if (facts[src].readers || source == ZR_STRICT_VALUE_SHARED) return 0;
                facts[src].writer = 1;
            } else {
                ++facts[src].readers;
            }
            facts[dst].base = (uint16_t)(src + 1);
            break;
        case ZR_STRICT_OP_END_BORROW:
            if (i->dst || i->rhs || i->immediate || !zr_strict_is_borrow(source) || !facts[src].available) return 0;
            if (source == ZR_STRICT_VALUE_BORROW) facts[facts[src].base - 1].writer = 0;
            else --facts[facts[src].base - 1].readers;
            facts[src].available = 0;
            return 1;
        case ZR_STRICT_OP_FIELD_READ:
        case ZR_STRICT_OP_FIELD_WRITE: {
            const uint32_t record = i->op == ZR_STRICT_OP_FIELD_READ ? src : dst;
            const uint32_t scalar = i->op == ZR_STRICT_OP_FIELD_READ ? dst : src;
            const uint8_t recordKind = i->op == ZR_STRICT_OP_FIELD_READ ? source : destination;
            if (i->rhs || !record_data_kind(recordKind) || !facts[record].available || facts[record].writer) return 0;
            layout = &p->layouts[p->slotLayouts[record] - 1];
            if (i->immediate >= layout->fieldCount || !kind_is(p, scalar, layout->fields[i->immediate])) return 0;
            if (i->op == ZR_STRICT_OP_FIELD_READ) return 1;
            return !facts[record].readers && recordKind != ZR_STRICT_VALUE_SHARED &&
                   recordKind != ZR_STRICT_VALUE_READONLY_BORROW && !(layout->readonlyMask & (1u << i->immediate));
        }
        default: return 0;
    }
    facts[dst].available = 1;
    return 1;
}

int zr_strict_verify_ownership(const ZrStrictProgram *p, uint32_t *badPc) {
    ZrStrictOwnershipFact facts[ZR_STRICT_MAX_SLOTS];
    memset(facts, 0, (size_t)p->slotCount * sizeof(*facts));
    for (uint32_t pc = 0; pc < p->instructionCount; ++pc) {
        const ZrStrictInstruction *i = &p->instructions[pc];
        if (i->op == ZR_STRICT_OP_JUMP || i->op == ZR_STRICT_OP_JUMP_IF_FALSE ||
            (i->op > ZR_STRICT_OP_HALT && !verify_owner_op(p, i, facts))) {
            if (badPc) *badPc = pc;
            return 0;
        }
    }
    return 1;
}
