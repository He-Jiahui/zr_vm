#include "program_ownership.h"

#include <inttypes.h>
#include <string.h>

typedef struct ZrStrictHeapRecord {
    uint32_t layout;
    ZrStrictCell fields[];
} ZrStrictHeapRecord;

typedef struct ZrStrictPoolRecord {
    uint32_t layout;
    ZrStrictCell fields[ZR_STRICT_MAX_FIELDS];
} ZrStrictPoolRecord;

static void *deny_allocation(void *context, size_t size) {
    (void)context;
    (void)size;
    return NULL;
}

static void deny_free(void *context, void *pointer) {
    (void)context;
    (void)pointer;
}

static ZrStrictStatus drop_cell(ZrStrictCell *cell, uint8_t kind) {
    switch (kind) {
        case ZR_STRICT_VALUE_UNIQUE: return cell->unique.record ? ZrStrict_UniqueDrop(&cell->unique) : ZR_STRICT_OK;
        case ZR_STRICT_VALUE_SHARED:
        case ZR_STRICT_VALUE_MAYBE_SHARED: return cell->shared.record ? ZrStrict_SharedDrop(&cell->shared) : ZR_STRICT_OK;
        case ZR_STRICT_VALUE_WEAK: return cell->weak.record ? ZrStrict_WeakDrop(&cell->weak) : ZR_STRICT_OK;
        case ZR_STRICT_VALUE_MANUAL_BLOCK: return cell->manual.owner.record ? ZrStrict_ManualFree(&cell->manual) : ZR_STRICT_OK;
        default: return ZR_STRICT_OK;
    }
}

static ZrStrictStatus drop_fields(ZrStrictCell *fields, const ZrStrictRecordLayout *layout) {
    ZrStrictStatus first = ZR_STRICT_OK;
    for (uint32_t field = layout->fieldCount; field > 0; --field) {
        const ZrStrictStatus status = drop_cell(&fields[field - 1], layout->fields[field - 1]);
        if (first == ZR_STRICT_OK) first = status;
    }
    return first;
}

static ZrStrictStatus run_destructor(ZrStrictCell *fields, const ZrStrictFrame *owner, const ZrStrictDestructor *body) {
    const ZrStrictProgram code = zr_strict_destructor_program(owner->program, body);
    const ZrStrictAllocator deny = {deny_allocation, deny_free, NULL};
    ZrStrictFrame frame;
    ZrStrictFrameSlot slots[ZR_STRICT_MAX_DESTRUCTOR_SLOTS];
    ZrStrictResult ignored = {0};
    if (ZrStrictFrame_Open(&frame, &code, slots, ZR_STRICT_MAX_DESTRUCTOR_SLOTS, &deny) != ZR_STRICT_PROGRAM_OK)
        return ZR_STRICT_FAULT;
    frame.externalRecord = fields;
    slots[0].live = 1;
    const ZrStrictProgramStatus status = zr_strict_run_frame(&frame, &ignored, NULL);
    const ZrStrictProgramStatus cleanup = ZrStrictFrame_Close(&frame);
    return status == ZR_STRICT_PROGRAM_OK && cleanup == ZR_STRICT_PROGRAM_OK ? ZR_STRICT_OK : ZR_STRICT_FAULT;
}

static ZrStrictStatus destroy_payload(ZrStrictCell *cells, uint32_t type, const ZrStrictFrame *owner) {
    const ZrStrictRecordLayout *layout = &owner->program->layouts[type - 1];
    const ZrStrictStatus body = layout->destructor.instructionCount
            ? run_destructor(cells, owner, &layout->destructor) : ZR_STRICT_OK;
    const ZrStrictStatus fields = drop_fields(cells, layout);
    return body == ZR_STRICT_OK ? fields : body;
}

static ZrStrictStatus destroy_record(void *payload, void *context) {
    ZrStrictHeapRecord *record = payload;
    return destroy_payload(record->fields, record->layout, context);
}

static ZrStrictStatus destroy_rental(void *payload, void *context) {
    ZrStrictPoolRecord *record = payload;
    return destroy_payload(record->fields, record->layout, context);
}

static ZrStrictProgramStatus from_ownership(ZrStrictStatus status) {
    if (status == ZR_STRICT_OK) return ZR_STRICT_PROGRAM_OK;
    if (status == ZR_STRICT_OUT_OF_MEMORY) return ZR_STRICT_PROGRAM_OUT_OF_MEMORY;
    if (status == ZR_STRICT_FAULT) return ZR_STRICT_PROGRAM_DESTRUCTOR_FAULT;
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

#if defined(ZR_DEBUG)
static void report_rental(void *context, const char *name, ZrStrictSite site) {
    (void)context;
    fprintf(stderr, "StrictNoGC: outstanding %s pool rental at %u:%u\n",
            name ? name : "<unnamed>", site.line, site.column);
}
#endif

static ZrStrictStatus drop_slot(ZrStrictFrame *frame, uint32_t index, int closing) {
    ZrStrictFrameSlot *slot = &frame->slots[index];
    ZrStrictStatus status = ZR_STRICT_OK;
    if (!slot->live) return status;
    uint8_t kind = frame->program->kinds[index];
    if (kind == ZR_STRICT_VALUE_RECORD) {
        if (index || !frame->externalRecord)
            status = drop_fields(slot->value.record, &frame->program->layouts[frame->program->slotLayouts[index] - 1]);
    } else if (kind == ZR_STRICT_VALUE_POOL) {
#if defined(ZR_DEBUG)
        status = ZrStrict_PoolClose(&slot->value.pool, report_rental, NULL);
#else
        status = ZrStrict_PoolClose(&slot->value.pool, NULL, NULL);
#endif
        /* The final sweep reports abandoned rentals and preserves the
         * program's exit. Explicit disposal still reports destructor faults. */
        if (closing && status == ZR_STRICT_FAULT) status = ZR_STRICT_OK;
    } else if (kind == ZR_STRICT_VALUE_READONLY_BORROW && slot->value.recordView.lease.pool) {
        status = ZrStrict_PoolCloseView(&slot->value.recordView.lease);
    } else if (zr_strict_is_field_owner(kind) || kind == ZR_STRICT_VALUE_MAYBE_SHARED) {
        status = drop_cell(&slot->value.cell, kind);
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
    for (uint32_t index = frame->program->slotCount; index > 0; --index) {
        if (!zr_strict_is_borrow(frame->program->kinds[index - 1])) continue;
        ZrStrictStatus status = drop_slot(frame, index - 1, 1);
        if (first == ZR_STRICT_OK) first = status;
    }
    for (uint32_t index = frame->program->slotCount; index > 0; --index) {
        ZrStrictStatus status = drop_slot(frame, index - 1, 1);
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

static const ZrStrictCell *owned_record_read(const ZrStrictCell *cell, uint8_t kind) {
    const ZrStrictHeapRecord *record = kind == ZR_STRICT_VALUE_UNIQUE ? ZrStrict_UniqueData(&cell->unique)
                                                                   : ZrStrict_SharedData(&cell->shared);
    return record ? record->fields : NULL;
}

static ZrStrictCell *unique_record_write(ZrStrictCell *cell) {
    ZrStrictHeapRecord *record = ZrStrict_UniqueData(&cell->unique);
    return record ? record->fields : NULL;
}

static const ZrStrictCell *record_read(const ZrStrictFrame *frame, uint32_t index) {
    const ZrStrictFrameSlot *slot = &frame->slots[index];
    uint8_t kind = frame->program->kinds[index];
    if (!index && frame->externalRecord) return frame->externalRecord;
    switch (kind) {
        case ZR_STRICT_VALUE_RECORD: return slot->value.record;
        case ZR_STRICT_VALUE_BORROW: return slot->value.writableRecord;
        case ZR_STRICT_VALUE_READONLY_BORROW: return slot->value.recordView.data;
        case ZR_STRICT_VALUE_UNIQUE:
        case ZR_STRICT_VALUE_SHARED:
        case ZR_STRICT_VALUE_MAYBE_SHARED: return owned_record_read(&slot->value.cell, kind);
        default: return NULL;
    }
}

static ZrStrictCell *record_write(ZrStrictFrame *frame, uint32_t index) {
    ZrStrictFrameSlot *slot = &frame->slots[index];
    if (!index && frame->externalRecord) return frame->externalRecord;
    switch (frame->program->kinds[index]) {
        case ZR_STRICT_VALUE_RECORD: return slot->value.record;
        case ZR_STRICT_VALUE_BORROW: return slot->value.writableRecord;
        case ZR_STRICT_VALUE_UNIQUE: return unique_record_write(&slot->value.cell);
        default: return NULL;
    }
}

static int borrow_slot(ZrStrictFrame *frame, ZrStrictFrameSlot *dst, const ZrStrictInstruction *i) {
    uint8_t destination = frame->program->kinds[i->dst];
    uint8_t source = frame->program->kinds[i->lhs];
    const ZrStrictFrameSlot *src = &frame->slots[i->lhs];
    const int fieldView = i->op == ZR_STRICT_OP_BORROW_FIELD;
    if (zr_strict_is_byte_view(destination)) {
        const ZrStrictManualBlock *block;
        if (fieldView) {
            const ZrStrictCell *record = record_read(frame, i->lhs);
            if (!record) return 0;
            block = &record[i->immediate].manual;
        } else if (zr_strict_is_byte_view(source)) {
            block = src->value.byteView;
        } else {
            block = &src->value.cell.manual;
        }
        if (!block || !block->owner.record) return 0;
        dst->value.byteView = block;
        return 1;
    }
    if (destination == ZR_STRICT_VALUE_BORROW) {
        ZrStrictCell *record = record_write(frame, i->lhs);
        if (fieldView && record) record = unique_record_write(&record[i->immediate]);
        dst->value.writableRecord = record;
        return record != NULL;
    }
    const ZrStrictCell *record = record_read(frame, i->lhs);
    if (fieldView && record) {
        const ZrStrictRecordLayout *layout = &frame->program->layouts[frame->program->slotLayouts[i->lhs] - 1];
        record = owned_record_read(&record[i->immediate], layout->fields[i->immediate]);
    }
    dst->value.recordView.data = record;
    return record != NULL;
}

int64_t ZrStrictFrame_Scalar(const ZrStrictFrame *frame, uint32_t slot) {
    return frame->slots[slot].value.cell.scalar;
}

static ZrStrictStatus rent_record(ZrStrictFrame *frame, const ZrStrictInstruction *i, ZrStrictPoolHandle *out) {
    const uint32_t type = frame->program->slotLayouts[i->dst];
    const ZrStrictRecordLayout *layout = &frame->program->layouts[type - 1];
    ZrStrictPoolRecord value = {.layout = type};
    for (uint32_t field = 0; field < layout->fieldCount; ++field)
        value.fields[field] = frame->slots[i->lhs + field].value.cell;
    const ZrStrictSite site = {NULL, (unsigned)(i->immediate >> 32), (unsigned)(i->immediate & UINT32_MAX)};
    ZrStrictStatus status = ZrStrict_PoolRent(&frame->slots[i->rhs].value.pool, &value, site, out);
    if (status == ZR_STRICT_OUT_OF_MEMORY) status = drop_fields(value.fields, layout);
    else if (status != ZR_STRICT_OK) return status;
    for (uint32_t field = 0; field < layout->fieldCount; ++field)
        if (zr_strict_is_field_owner(layout->fields[field]))
            memset(&frame->slots[i->lhs + field], 0, sizeof(*frame->slots));
    return status;
}

ZrStrictProgramStatus ZrStrictFrame_Apply(ZrStrictFrame *frame, const ZrStrictInstruction *i) {
    ZrStrictFrameSlot *dst = i->dst < frame->program->slotCount ? &frame->slots[i->dst] : NULL;
    ZrStrictFrameSlot *src = i->lhs < frame->program->slotCount ? &frame->slots[i->lhs] : NULL;
    ZrStrictStatus status = ZR_STRICT_OK;
    switch (i->op) {
        case ZR_STRICT_OP_CONST: dst->value.cell.scalar = ZrStrictProgram_IntBits(i->immediate); return ZR_STRICT_PROGRAM_OK;
        case ZR_STRICT_OP_COPY: dst->value.cell.scalar = src->value.cell.scalar; return ZR_STRICT_PROGRAM_OK;
        case ZR_STRICT_OP_POOL_CREATE: {
            const int64_t cap = src->value.cell.scalar;
            if (cap < 0 || (uint64_t)cap > SIZE_MAX) return ZR_STRICT_PROGRAM_OWNERSHIP_FAULT;
            const uint32_t type = frame->program->slotLayouts[i->dst];
            const ZrStrictRecordLayout *layout = &frame->program->layouts[type - 1];
            const ZrStrictLayout pooled = {sizeof(ZrStrictPoolRecord), _Alignof(ZrStrictPoolRecord), layout->name,
                layout->destructor.instructionCount || zr_strict_layout_has_owners(layout) ? destroy_rental : NULL, frame};
            status = ZrStrict_PoolInit(&frame->ownership, &dst->value.pool, &pooled, 64, (size_t)cap);
            break;
        }
        case ZR_STRICT_OP_POOL_RENT:
            status = rent_record(frame, i, &dst->value.poolHandle);
            break;
        case ZR_STRICT_OP_POOL_CONTAINS:
            dst->value.cell.scalar = ZrStrict_PoolContains(&src->value.pool, frame->slots[i->rhs].value.poolHandle);
            return ZR_STRICT_PROGRAM_OK;
        case ZR_STRICT_OP_POOL_RECYCLE:
            status = ZrStrict_PoolRecycle(&src->value.pool, frame->slots[i->rhs].value.poolHandle);
            if (status == ZR_STRICT_EXPIRED) dst->value.cell.scalar = 0;
            else if (status == ZR_STRICT_OK) dst->value.cell.scalar = 1;
            else return from_ownership(status);
            return ZR_STRICT_PROGRAM_OK;
        case ZR_STRICT_OP_POOL_VIEW: {
            ZrStrictPoolView *lease = &dst->value.recordView.lease;
            status = ZrStrict_PoolOpenView(&src->value.pool, frame->slots[i->rhs].value.poolHandle, 0, lease);
            if (status == ZR_STRICT_OK) {
                const ZrStrictPoolRecord *value = ZrStrict_PoolViewData(lease);
                dst->value.recordView.data = value->fields;
            }
            break;
        }
        case ZR_STRICT_OP_POOL_SHRINK: {
            size_t count = ZrStrict_PoolShrink(&src->value.pool);
            if (count > INT64_MAX) return ZR_STRICT_PROGRAM_OWNERSHIP_FAULT;
            dst->value.cell.scalar = (int64_t)count;
            return ZR_STRICT_PROGRAM_OK;
        }
        case ZR_STRICT_OP_MANUAL_TRY_ALLOC: {
            const int64_t byteCount = src->value.cell.scalar;
            const size_t size = (size_t)byteCount;
            const ZrStrictSite site = {NULL, (unsigned)(i->immediate >> 32), (unsigned)(i->immediate & UINT32_MAX)};
            if (byteCount >= 0 && (uint64_t)size == (uint64_t)byteCount) {
                status = ZrStrict_ManualAlloc(&frame->ownership, size, site, &dst->value.cell.manual);
                if (status == ZR_STRICT_OK) memset(ZrStrict_ManualData(&dst->value.cell.manual, NULL), 0, size);
                if (status == ZR_STRICT_OUT_OF_MEMORY) status = ZR_STRICT_OK; /* observed as null */
            }
            break;
        }
        case ZR_STRICT_OP_BYTE_LENGTH:
        case ZR_STRICT_OP_BYTE_READ:
        case ZR_STRICT_OP_BYTE_WRITE: {
            const uint32_t view = i->op == ZR_STRICT_OP_BYTE_WRITE ? i->dst : i->lhs;
            size_t length;
            unsigned char *bytes = ZrStrict_ManualData(frame->slots[view].value.byteView, &length);
            if (!bytes) return ZR_STRICT_PROGRAM_OWNERSHIP_FAULT;
            if (i->op == ZR_STRICT_OP_BYTE_LENGTH) {
                dst->value.cell.scalar = (int64_t)length;
                return ZR_STRICT_PROGRAM_OK;
            }
            const int64_t index = i->op == ZR_STRICT_OP_BYTE_WRITE ? src->value.cell.scalar : frame->slots[i->rhs].value.cell.scalar;
            if (index < 0 || (uint64_t)index >= length) return ZR_STRICT_PROGRAM_BYTE_RANGE_FAULT;
            if (i->op == ZR_STRICT_OP_BYTE_READ) {
                dst->value.cell.scalar = bytes[(size_t)index];
            } else {
                const int64_t value = frame->slots[i->rhs].value.cell.scalar;
                if (value < 0 || value > 255) return ZR_STRICT_PROGRAM_BYTE_RANGE_FAULT;
                bytes[(size_t)index] = (unsigned char)value;
            }
            return ZR_STRICT_PROGRAM_OK;
        }
        case ZR_STRICT_OP_INIT_RECORD:
        case ZR_STRICT_OP_OWN_RECORD: {
            const ZrStrictRecordLayout *layout = &frame->program->layouts[frame->program->slotLayouts[i->dst] - 1];
            ZrStrictCell *data = dst->value.record;
            ZrStrictBuilder builder = {0};
            if (i->op == ZR_STRICT_OP_OWN_RECORD) {
                const ZrStrictLayout heap = {sizeof(ZrStrictHeapRecord) + layout->fieldCount * sizeof(ZrStrictCell),
                    _Alignof(ZrStrictHeapRecord), layout->name,
                    layout->destructor.instructionCount || zr_strict_layout_has_owners(layout) ? destroy_record : NULL, frame};
                ZrStrictSite site = {NULL, (unsigned)(i->immediate >> 32), (unsigned)(i->immediate & UINT32_MAX)};
                void *payload;
                status = ZrStrict_OwnBegin(&frame->ownership, &heap, site, &builder, &payload);
                if (status != ZR_STRICT_OK) return from_ownership(status);
                ZrStrictHeapRecord *record = payload;
                record->layout = frame->program->slotLayouts[i->dst];
                data = record->fields;
            }
            for (uint32_t field = 0; field < layout->fieldCount; ++field) data[field] = frame->slots[i->lhs + field].value.cell;
            if (i->op == ZR_STRICT_OP_OWN_RECORD) {
                status = ZrStrict_OwnCommit(&builder, &dst->value.cell.unique);
                if (status != ZR_STRICT_OK) {
                    (void)ZrStrict_OwnAbort(&builder);
                    return from_ownership(status); /* staging still owns all fields */
                }
            }
            for (uint32_t field = 0; field < layout->fieldCount; ++field)
                if (zr_strict_is_field_owner(layout->fields[field]))
                    memset(&frame->slots[i->lhs + field], 0, sizeof(*src));
            break;
        }
        case ZR_STRICT_OP_TRANSFER:
            *dst = *src;
            memset(src, 0, sizeof(*src));
            break;
        case ZR_STRICT_OP_SHARE:
            status = ZrStrict_Share(&src->value.cell.unique, &dst->value.cell.shared);
            if (status == ZR_STRICT_OK) src->live = 0;
            break;
        case ZR_STRICT_OP_CLONE:
            switch (frame->program->kinds[i->lhs]) {
                case ZR_STRICT_VALUE_RECORD: {
                    const ZrStrictRecordLayout *layout = &frame->program->layouts[frame->program->slotLayouts[i->lhs] - 1];
                    const ZrStrictCell *data = record_read(frame, i->lhs);
                    memcpy(dst->value.record, data, layout->fieldCount * sizeof(*data));
                    break;
                }
                case ZR_STRICT_VALUE_WEAK: status = ZrStrict_WeakClone(&src->value.cell.weak, &dst->value.cell.weak); break;
                case ZR_STRICT_VALUE_POOL_HANDLE: dst->value.poolHandle = src->value.poolHandle; break;
                default:
                    if (src->value.cell.shared.record) status = ZrStrict_SharedClone(&src->value.cell.shared, &dst->value.cell.shared);
                    break;
            }
            break;
        case ZR_STRICT_OP_DEGRADE: status = ZrStrict_Degrade(&src->value.cell.shared, &dst->value.cell.weak); break;
        case ZR_STRICT_OP_WAKE:
            status = ZrStrict_Wake(&src->value.cell.weak, &dst->value.cell.shared);
            if (status == ZR_STRICT_EXPIRED) status = ZR_STRICT_OK;
            break;
        case ZR_STRICT_OP_UNWRAP_SHARED:
            if (!src->value.cell.shared.record) return ZR_STRICT_PROGRAM_OWNERSHIP_FAULT;
            status = ZrStrict_SharedClone(&src->value.cell.shared, &dst->value.cell.shared);
            break;
        case ZR_STRICT_OP_IS_NULL:
            switch (frame->program->kinds[i->lhs]) {
                case ZR_STRICT_VALUE_MANUAL_BLOCK: dst->value.cell.scalar = !src->value.cell.manual.owner.record; break;
                case ZR_STRICT_VALUE_POOL_HANDLE: dst->value.cell.scalar = !src->value.poolHandle.rental; break;
                default: dst->value.cell.scalar = !src->value.cell.shared.record; break;
            }
            return ZR_STRICT_PROGRAM_OK;
        case ZR_STRICT_OP_DROP:
        case ZR_STRICT_OP_END_BORROW: return from_ownership(drop_slot(frame, i->lhs, 0));
        case ZR_STRICT_OP_BORROW:
        case ZR_STRICT_OP_BORROW_FIELD:
            if (!borrow_slot(frame, dst, i)) return ZR_STRICT_PROGRAM_OWNERSHIP_FAULT;
            break;
        case ZR_STRICT_OP_FIELD_READ: {
            const ZrStrictCell *data = record_read(frame, i->lhs);
            if (!data) return ZR_STRICT_PROGRAM_OWNERSHIP_FAULT;
            const ZrStrictCell *field = &data[i->immediate];
            switch (frame->program->kinds[i->dst]) {
                case ZR_STRICT_VALUE_SHARED: status = ZrStrict_SharedClone(&field->shared, &dst->value.cell.shared); break;
                case ZR_STRICT_VALUE_WEAK: status = ZrStrict_WeakClone(&field->weak, &dst->value.cell.weak); break;
                default: dst->value.cell.scalar = field->scalar; return ZR_STRICT_PROGRAM_OK;
            }
            break;
        }
        case ZR_STRICT_OP_FIELD_WRITE: {
            ZrStrictCell *data = record_write(frame, i->dst);
            if (!data) return ZR_STRICT_PROGRAM_OWNERSHIP_FAULT;
            const uint8_t kind = frame->program->kinds[i->lhs];
            ZrStrictCell *field = &data[i->immediate];
            if (!zr_strict_is_field_owner(kind)) {
                field->scalar = src->value.cell.scalar;
                return ZR_STRICT_PROGRAM_OK;
            }
            ZrStrictCell previous = *field;
            *field = src->value.cell;
            memset(src, 0, sizeof(*src));
            return from_ownership(drop_cell(&previous, kind));
        }
        default:
            return ZrStrictProgram_Scalar(i->op, src->value.cell.scalar,
                    i->op == ZR_STRICT_OP_NEG || i->op == ZR_STRICT_OP_NOT ? 0 : frame->slots[i->rhs].value.cell.scalar,
                    &dst->value.cell.scalar);
    }
    if (status == ZR_STRICT_OK) dst->live = 1;
    return from_ownership(status);
}
