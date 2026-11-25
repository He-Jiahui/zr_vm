#include "program_ownership.h"

#include <inttypes.h>
#include <string.h>

static ZrStrictProgramStatus from_ownership(ZrStrictStatus status) {
    if (status == ZR_STRICT_OK) return ZR_STRICT_PROGRAM_OK;
    if (status == ZR_STRICT_OUT_OF_MEMORY) return ZR_STRICT_PROGRAM_OUT_OF_MEMORY;
    return ZR_STRICT_PROGRAM_OWNERSHIP_FAULT;
}

ZrStrictProgramStatus ZrStrictFrame_Open(ZrStrictFrame *frame, const ZrStrictProgram *program,
                                        ZrStrictFrameSlot *slots, uint32_t capacity,
                                        const ZrStrictAllocator *allocator) {
    if (!frame || !program || program->slotCount > ZR_STRICT_MAX_SLOTS ||
        program->slotCount > capacity || (program->slotCount && !slots)) return ZR_STRICT_PROGRAM_BAD_LAYOUT;
    memset(frame, 0, sizeof(*frame));
    if (ZrStrictRuntime_Init(&frame->ownership, allocator) != ZR_STRICT_OK) return ZR_STRICT_PROGRAM_BAD_LAYOUT;
    if (program->slotCount) memset(slots, 0, program->slotCount * sizeof(*slots));
    frame->slots = slots;
    frame->program = program;
    return ZR_STRICT_PROGRAM_OK;
}

static ZrStrictStatus drop_slot(ZrStrictFrame *frame, uint32_t index) {
    ZrStrictFrameSlot *slot = &frame->slots[index];
    ZrStrictStatus status = ZR_STRICT_OK;
    if (!slot->live) return status;
    switch (frame->program->kinds[index]) {
        case ZR_STRICT_VALUE_UNIQUE: status = ZrStrict_UniqueDrop(&slot->value.unique); break;
        case ZR_STRICT_VALUE_WEAK: status = ZrStrict_WeakDrop(&slot->value.weak); break;
        case ZR_STRICT_VALUE_SHARED:
        case ZR_STRICT_VALUE_MAYBE_SHARED:
            if (slot->value.shared.record) status = ZrStrict_SharedDrop(&slot->value.shared);
            break;
        default: break;
    }
    memset(slot, 0, sizeof(*slot));
    return status;
}

#if defined(ZR_DEBUG)
static void report_survivor(void *context, const char *name, ZrStrictSite site, size_t count) {
    (void)context;
    fprintf(stderr, "StrictNoGC: %s allocated at %u:%u has %zu surviving strong owner(s)\n",
            name ? name : "<unnamed>", site.line, site.column, count);
}
#endif

ZrStrictProgramStatus ZrStrictFrame_Close(ZrStrictFrame *frame) {
    ZrStrictStatus first = ZR_STRICT_OK;
    ZrStrictStatus close;
    if (!frame || !frame->program) return ZR_STRICT_PROGRAM_BAD_LAYOUT;
    for (uint32_t index = 0; index < frame->program->slotCount; ++index) {
        if (zr_strict_is_borrow(frame->program->kinds[index])) frame->slots[index].live = 0;
    }
    for (uint32_t index = frame->program->slotCount; index > 0; --index) {
        ZrStrictStatus status = drop_slot(frame, index - 1);
        if (first == ZR_STRICT_OK) first = status;
    }
#if defined(ZR_DEBUG)
    close = ZrStrictRuntime_Close(&frame->ownership, report_survivor, NULL);
#else
    close = ZrStrictRuntime_Close(&frame->ownership, NULL, NULL);
#endif
    memset(frame, 0, sizeof(*frame));
    return from_ownership(first == ZR_STRICT_OK ? close : first);
}

static uint32_t base_slot(const ZrStrictFrame *frame, uint32_t index) {
    return zr_strict_is_borrow(frame->program->kinds[index]) ? frame->slots[index].value.borrowedFrom : index;
}

static const int64_t *record_read(const ZrStrictFrame *frame, uint32_t index) {
    const ZrStrictFrameSlot *slot;
    index = base_slot(frame, index);
    slot = &frame->slots[index];
    switch (frame->program->kinds[index]) {
        case ZR_STRICT_VALUE_RECORD: return slot->value.record;
        case ZR_STRICT_VALUE_UNIQUE: return ZrStrict_UniqueData(&slot->value.unique);
        case ZR_STRICT_VALUE_SHARED: return ZrStrict_SharedData(&slot->value.shared);
        default: return NULL;
    }
}

static int64_t *record_write(ZrStrictFrame *frame, uint32_t index) {
    uint8_t kind = frame->program->kinds[index];
    if (kind == ZR_STRICT_VALUE_READONLY_BORROW) return NULL;
    index = base_slot(frame, index);
    kind = frame->program->kinds[index];
    if (kind == ZR_STRICT_VALUE_RECORD) return frame->slots[index].value.record;
    if (kind == ZR_STRICT_VALUE_UNIQUE) return ZrStrict_UniqueData(&frame->slots[index].value.unique);
    return NULL;
}

int64_t ZrStrictFrame_Scalar(const ZrStrictFrame *frame, uint32_t slot) {
    return frame->slots[slot].value.scalar;
}

ZrStrictProgramStatus ZrStrictFrame_Apply(ZrStrictFrame *frame, const ZrStrictInstruction *i) {
    ZrStrictFrameSlot *dst = i->dst < frame->program->slotCount ? &frame->slots[i->dst] : NULL;
    ZrStrictFrameSlot *src = i->lhs < frame->program->slotCount ? &frame->slots[i->lhs] : NULL;
    ZrStrictStatus status = ZR_STRICT_OK;
    switch (i->op) {
        case ZR_STRICT_OP_CONST: dst->value.scalar = ZrStrictProgram_IntBits(i->immediate); return ZR_STRICT_PROGRAM_OK;
        case ZR_STRICT_OP_COPY: dst->value.scalar = src->value.scalar; return ZR_STRICT_PROGRAM_OK;
        case ZR_STRICT_OP_INIT_RECORD:
        case ZR_STRICT_OP_OWN_RECORD: {
            const ZrStrictRecordLayout *layout = &frame->program->layouts[frame->program->slotLayouts[i->dst] - 1];
            int64_t *data = dst->value.record;
            ZrStrictBuilder builder = {0};
            if (i->op == ZR_STRICT_OP_OWN_RECORD) {
                const ZrStrictLayout heap = {layout->fieldCount * sizeof(int64_t), _Alignof(int64_t), layout->name, NULL, NULL};
                ZrStrictSite site = {NULL, (unsigned)(i->immediate >> 32), (unsigned)(i->immediate & UINT32_MAX)};
                void *payload;
                status = ZrStrict_OwnBegin(&frame->ownership, &heap, site, &builder, &payload);
                if (status != ZR_STRICT_OK) return from_ownership(status);
                data = payload;
            }
            for (uint32_t field = 0; field < layout->fieldCount; ++field) data[field] = frame->slots[i->lhs + field].value.scalar;
            if (i->op == ZR_STRICT_OP_OWN_RECORD) {
                status = ZrStrict_OwnCommit(&builder, &dst->value.unique);
                if (status != ZR_STRICT_OK) (void)ZrStrict_OwnAbort(&builder);
            }
            break;
        }
        case ZR_STRICT_OP_TRANSFER:
            *dst = *src;
            memset(src, 0, sizeof(*src));
            break;
        case ZR_STRICT_OP_SHARE:
            status = ZrStrict_Share(&src->value.unique, &dst->value.shared);
            if (status == ZR_STRICT_OK) src->live = 0;
            break;
        case ZR_STRICT_OP_CLONE:
            switch (frame->program->kinds[i->lhs]) {
                case ZR_STRICT_VALUE_RECORD: *dst = *src; break;
                case ZR_STRICT_VALUE_WEAK: status = ZrStrict_WeakClone(&src->value.weak, &dst->value.weak); break;
                default:
                    if (src->value.shared.record) status = ZrStrict_SharedClone(&src->value.shared, &dst->value.shared);
                    break;
            }
            break;
        case ZR_STRICT_OP_DEGRADE: status = ZrStrict_Degrade(&src->value.shared, &dst->value.weak); break;
        case ZR_STRICT_OP_WAKE:
            status = ZrStrict_Wake(&src->value.weak, &dst->value.shared);
            if (status == ZR_STRICT_EXPIRED) status = ZR_STRICT_OK;
            break;
        case ZR_STRICT_OP_IS_NULL:
            dst->value.scalar = src->value.shared.record == NULL;
            return ZR_STRICT_PROGRAM_OK;
        case ZR_STRICT_OP_DROP:
        case ZR_STRICT_OP_END_BORROW: return from_ownership(drop_slot(frame, i->lhs));
        case ZR_STRICT_OP_BORROW: dst->value.borrowedFrom = i->lhs; break;
        case ZR_STRICT_OP_FIELD_READ: {
            const int64_t *data = record_read(frame, i->lhs);
            if (!data) return ZR_STRICT_PROGRAM_OWNERSHIP_FAULT;
            dst->value.scalar = data[i->immediate];
            return ZR_STRICT_PROGRAM_OK;
        }
        case ZR_STRICT_OP_FIELD_WRITE: {
            int64_t *data = record_write(frame, i->dst);
            if (!data) return ZR_STRICT_PROGRAM_OWNERSHIP_FAULT;
            data[i->immediate] = src->value.scalar;
            return ZR_STRICT_PROGRAM_OK;
        }
        default:
            return ZrStrictProgram_Scalar(i->op, src->value.scalar,
                    i->op == ZR_STRICT_OP_NEG || i->op == ZR_STRICT_OP_NOT ? 0 : frame->slots[i->rhs].value.scalar,
                    &dst->value.scalar);
    }
    if (status == ZR_STRICT_OK) dst->live = 1;
    return from_ownership(status);
}
