#include "exec_ir_interpreter_internal.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define ZR_ORACLE_LOCAL_OPERAND_LIMIT ((TZrUInt32)8u)

void zr_oracle_diag(SZrExecIrDiagnostic *d, EZrExecutionDiagnosticCode code,
                           const SZrExecIrFunction *f, TZrExecIrBlockId block,
                           TZrExecIrInstructionId instruction, TZrExecIrSourceId source,
                           TZrUInt32 expected, TZrUInt32 actual) {
    if (d == ZR_NULL) return;
    memset(d, 0, sizeof(*d));
    d->code = code;
    d->functionToken = f != ZR_NULL ? f->functionToken : 0u;
    d->blockId = block;
    d->instructionId = instruction;
    d->sourceId = source;
    d->expectedVersion = expected;
    d->actualVersion = actual;
}

TZrBool zr_oracle_bytes(TZrUInt32 count, size_t element, size_t *bytes) {
    if (bytes == ZR_NULL || element == 0u || (size_t)count > SIZE_MAX / element) return ZR_FALSE;
    *bytes = (size_t)count * element;
    return ZR_TRUE;
}

void zr_oracle_undefined(SZrExecIrOracleValue *v) {
    if (v != ZR_NULL) { memset(v, 0, sizeof(*v)); v->kind = ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED; }
}

static TZrBool zr_oracle_numeric(const SZrExecIrOracleValue *v) {
    return (TZrBool)(v != ZR_NULL && v->kind >= ZR_EXEC_IR_ORACLE_VALUE_BOOL &&
                     v->kind <= ZR_EXEC_IR_ORACLE_VALUE_FLOAT);
}

static TZrInt64 zr_oracle_signed(const SZrExecIrOracleValue *v) {
    if (v->kind == ZR_EXEC_IR_ORACLE_VALUE_BOOL) return v->as.boolean ? 1 : 0;
    if (v->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED) return (TZrInt64)v->as.unsignedInteger;
    return v->as.signedInteger;
}

static TZrUInt64 zr_oracle_unsigned(const SZrExecIrOracleValue *v) {
    if (v->kind == ZR_EXEC_IR_ORACLE_VALUE_BOOL) return v->as.boolean ? 1u : 0u;
    if (v->kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED) return (TZrUInt64)v->as.signedInteger;
    return v->as.unsignedInteger;
}

static TZrFloat64 zr_oracle_float(const SZrExecIrOracleValue *v) {
    if (v->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) return v->as.floating;
    return v->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED
               ? (TZrFloat64)v->as.unsignedInteger : (TZrFloat64)zr_oracle_signed(v);
}

/* ExecIR type tokens produced by the parser use the shared scalar type
 * numbers.  Keep the oracle's pointer-free value model, but honor those
 * scalar conversion boundaries instead of treating CONVERT as an alias. */
static TZrBool zr_oracle_convert_scalar(const SZrExecIrOracleValue *source,
                                        TZrExecIrTypeToken targetType,
                                        SZrExecIrOracleValue *out) {
    long double numeric;

    if (source == ZR_NULL || out == ZR_NULL || !zr_oracle_numeric(source)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(targetType)) {
        out->kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
        if (source->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) {
            out->as.boolean = (TZrBool)(source->as.floating != 0.0);
        } else if (source->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED) {
            out->as.boolean = (TZrBool)(source->as.unsignedInteger != 0u);
        } else if (source->kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED) {
            out->as.boolean = (TZrBool)(source->as.signedInteger != 0);
        } else {
            out->as.boolean = source->as.boolean != ZR_FALSE;
        }
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(targetType)) {
        if (source->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) {
            numeric = (long double)source->as.floating;
            if (!isfinite(numeric) || numeric < (long double)INT64_MIN ||
                numeric >= 9223372036854775808.0L) {
                return ZR_FALSE;
            }
            out->as.signedInteger = (TZrInt64)numeric;
        } else {
            out->as.signedInteger = zr_oracle_signed(source);
        }
        out->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(targetType)) {
        if (source->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) {
            numeric = (long double)source->as.floating;
            if (!isfinite(numeric) || numeric < 0.0L ||
                numeric >= 18446744073709551616.0L) {
                return ZR_FALSE;
            }
            out->as.unsignedInteger = (TZrUInt64)numeric;
        } else {
            out->as.unsignedInteger = zr_oracle_unsigned(source);
        }
        out->kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(targetType)) {
        out->kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
        out->as.floating = zr_oracle_float(source);
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static int zr_oracle_compare_values(const SZrExecIrOracleValue *left,
                                    const SZrExecIrOracleValue *right) {
    if (left->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT ||
        right->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) {
        TZrFloat64 a = zr_oracle_float(left);
        TZrFloat64 b = zr_oracle_float(right);
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }
    if (left->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED ||
        right->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED) {
        TZrUInt64 a = zr_oracle_unsigned(left);
        TZrUInt64 b = zr_oracle_unsigned(right);
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }
    {
        TZrInt64 a = zr_oracle_signed(left);
        TZrInt64 b = zr_oracle_signed(right);
        if (a < b) return -1;
        if (a > b) return 1;
        return 0;
    }
}

static TZrBool zr_oracle_signed_op(TZrInt64 a, TZrInt64 b, EZrExecIrOpcode op, TZrInt64 *out) {
    if (out == ZR_NULL) return ZR_FALSE;
    switch (op) {
        case ZR_EXEC_IR_OPCODE_ADD:
        case ZR_EXEC_IR_OPCODE_ARITHMETIC:
            if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return ZR_FALSE;
            *out = a + b; return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_SUB:
            if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) return ZR_FALSE;
            *out = a - b; return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_MUL:
            if (a != 0 && b != 0 && ((a == INT64_MIN && b == -1) || (b == INT64_MIN && a == -1) ||
                (a > 0 && b > 0 && a > INT64_MAX / b) || (a > 0 && b < 0 && b < INT64_MIN / a) ||
                (a < 0 && b > 0 && a < INT64_MIN / b) || (a < 0 && b < 0 && a < INT64_MAX / b))) return ZR_FALSE;
            *out = a * b; return ZR_TRUE;
        case ZR_EXEC_IR_OPCODE_DIV:
            if (b == 0 || (a == INT64_MIN && b == -1)) return ZR_FALSE;
            *out = a / b; return ZR_TRUE;
        default: return ZR_FALSE;
    }
}

static TZrBool zr_oracle_binary(const SZrExecIrInstruction *ins,
                                const SZrExecIrOracleValue *a,
                                const SZrExecIrOracleValue *b,
                                SZrExecIrOracleValue *out) {
    if (ins == ZR_NULL || out == ZR_NULL || !zr_oracle_numeric(a) || !zr_oracle_numeric(b)) return ZR_FALSE;
    if (a->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT || b->kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) {
        TZrFloat64 x = zr_oracle_float(a), y = zr_oracle_float(b);
        out->kind = ZR_EXEC_IR_ORACLE_VALUE_FLOAT;
        switch ((EZrExecIrOpcode)ins->opcode) {
            case ZR_EXEC_IR_OPCODE_ADD: case ZR_EXEC_IR_OPCODE_ARITHMETIC: out->as.floating = x + y; break;
            case ZR_EXEC_IR_OPCODE_SUB: out->as.floating = x - y; break;
            case ZR_EXEC_IR_OPCODE_MUL: out->as.floating = x * y; break;
            case ZR_EXEC_IR_OPCODE_DIV:
                if (y == 0.0) return ZR_FALSE;
                out->as.floating = x / y;
                break;
            default: return ZR_FALSE;
        }
        return ZR_TRUE;
    }
    if (a->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED || b->kind == ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED) {
        TZrUInt64 x = zr_oracle_unsigned(a), y = zr_oracle_unsigned(b);
        out->kind = ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED;
        switch ((EZrExecIrOpcode)ins->opcode) {
            case ZR_EXEC_IR_OPCODE_ADD: case ZR_EXEC_IR_OPCODE_ARITHMETIC:
                if (x > UINT64_MAX - y) return ZR_FALSE;
                out->as.unsignedInteger = x + y;
                break;
            case ZR_EXEC_IR_OPCODE_SUB:
                if (x < y) return ZR_FALSE;
                out->as.unsignedInteger = x - y;
                break;
            case ZR_EXEC_IR_OPCODE_MUL:
                if (y != 0u && x > UINT64_MAX / y) return ZR_FALSE;
                out->as.unsignedInteger = x * y;
                break;
            case ZR_EXEC_IR_OPCODE_DIV:
                if (y == 0u) return ZR_FALSE;
                out->as.unsignedInteger = x / y;
                break;
            default: return ZR_FALSE;
        }
        return ZR_TRUE;
    }
    out->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    return zr_oracle_signed_op(zr_oracle_signed(a), zr_oracle_signed(b),
                               (EZrExecIrOpcode)ins->opcode, &out->as.signedInteger);
}

static TZrBool zr_oracle_append_event(SZrExecIrOracleExecutionResult *r,
                                       EZrExecIrOracleEventKind kind,
                                       TZrExecIrInstructionId id, TZrExecIrSourceId source,
                                       const SZrExecIrOracleValue *ops, TZrUInt32 count,
                                       const SZrExecIrFunction *f, TZrExecIrBlockId block,
                                       SZrExecIrDiagnostic *d) {
    TZrUInt32 cap, copyCount;
    size_t bytes;
    SZrExecIrOracleEvent *events;
    if (r->eventCount == UINT32_MAX) {
        zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, f, block, id,
                       source, UINT32_MAX, r->eventCount);
        return ZR_FALSE;
    }
    if (r->eventCount == r->eventCapacity) {
        cap = r->eventCapacity == 0u ? 8u : r->eventCapacity;
        if (cap > UINT32_MAX / 2u) cap = UINT32_MAX; else cap *= 2u;
        if (!zr_oracle_bytes(cap, sizeof(*events), &bytes)) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, f, block, id, source, UINT32_MAX, cap);
            return ZR_FALSE;
        }
        events = (SZrExecIrOracleEvent *)realloc(r->events, bytes);
        if (events == ZR_NULL) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, f, block, id, source, cap, r->eventCapacity);
            return ZR_FALSE;
        }
        r->events = events; r->eventCapacity = cap;
    }
    events = &r->events[r->eventCount++];
    memset(events, 0, sizeof(*events));
    events->kind = kind; events->instructionId = id; events->sourceId = source;
    copyCount = count < ZR_EXEC_IR_ORACLE_EVENT_OPERAND_LIMIT ? count : ZR_EXEC_IR_ORACLE_EVENT_OPERAND_LIMIT;
    events->operandCount = copyCount;
    if (copyCount != 0u && ops != ZR_NULL) memcpy(events->operands, ops, (size_t)copyCount * sizeof(*ops));
    return ZR_TRUE;
}

static TZrBool zr_oracle_assign(const SZrExecIrFunction *f, const SZrExecIrInstruction *ins,
                                SZrExecIrOracleExecutionResult *r, const SZrExecIrOracleValue *v,
                                TZrExecIrBlockId block, TZrExecIrInstructionId id,
                                SZrExecIrDiagnostic *d) {
    TZrExecIrValueId valueId;
    if (ins->results.count == 0u) return ZR_TRUE;
    valueId = f->results[ins->results.start];
    if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID || valueId > r->valueCount) {
        zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, block, id, ins->sourceId, r->valueCount, valueId);
        return ZR_FALSE;
    }
    r->values[valueId - 1u] = *v;
    r->ownerStates[valueId - 1u] = f->values[valueId - 1u].ownership == ZR_EXEC_IR_OWNERSHIP_UNKNOWN
            ? ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN : ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED;
    return ZR_TRUE;
}

static void zr_oracle_consume_operands(const SZrExecIrFunction *f,
                                       const SZrExecIrInstruction *ins,
                                       SZrExecIrOracleExecutionResult *r) {
    TZrUInt32 i;
    if (f == ZR_NULL || ins == ZR_NULL || r == ZR_NULL) return;
    for (i = 0u; i < ins->operands.count; ++i) {
        TZrExecIrValueId valueId = f->operands[ins->operands.start + i];
        /* Structural validation and the operand load already checked these
         * identities.  Keep the guard so a future caller cannot turn the
         * ownership transition into an out-of-bounds write. */
        if (valueId != ZR_EXEC_IR_VALUE_ID_INVALID && valueId <= r->valueCount) {
            zr_oracle_undefined(&r->values[valueId - 1u]);
            r->ownerStates[valueId - 1u] = ins->opcode == ZR_EXEC_IR_OPCODE_MOVE
                    ? ZR_EXEC_IR_STATE_MAP_OWNER_MOVED : ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED;
        }
    }
}

static TZrBool zr_oracle_supported(EZrExecIrOpcode op, const SZrExecIrOracleInput *input) {
    if (op == ZR_EXEC_IR_OPCODE_CALL) {
        return (TZrBool)(input != ZR_NULL && input->call != ZR_NULL);
    }
    if (op == ZR_EXEC_IR_OPCODE_INVOKE) {
        return (TZrBool)(input != ZR_NULL && input->invoke != ZR_NULL);
    }
    if (op == ZR_EXEC_IR_OPCODE_ITER_INIT ||
        op == ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT ||
        op == ZR_EXEC_IR_OPCODE_ITER_CURRENT) {
        return (TZrBool)(input != ZR_NULL && input->iterator != ZR_NULL);
    }
    if (op == ZR_EXEC_IR_OPCODE_PLACE_BASE ||
        op == ZR_EXEC_IR_OPCODE_PLACE_PROJECT) {
        return (TZrBool)(input != ZR_NULL && input->place != ZR_NULL);
    }
    /* A load has no meaningful default value.  Require an explicit provider
     * so an oracle run cannot accidentally turn an unmodelled heap read into
     * a successful constant.  Stores retain the historical event-only mode
     * when no provider is supplied, while a provider enables stateful replay.
     */
    if (op == ZR_EXEC_IR_OPCODE_LOAD) {
        return (TZrBool)(input != ZR_NULL && input->memory != ZR_NULL);
    }
    if (op == ZR_EXEC_IR_OPCODE_ALLOC) {
        return (TZrBool)(input != ZR_NULL && input->allocate != ZR_NULL);
    }
    if (op == ZR_EXEC_IR_OPCODE_TYPE_TEST) {
        return (TZrBool)(input != ZR_NULL && input->typeTest != ZR_NULL);
    }
    if (op == ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD) {
        return (TZrBool)(input != ZR_NULL && input->exceptionPayload != ZR_NULL);
    }
    switch (op) {
        case ZR_EXEC_IR_OPCODE_NOP: case ZR_EXEC_IR_OPCODE_CONSTANT: case ZR_EXEC_IR_OPCODE_CONVERT:
        case ZR_EXEC_IR_OPCODE_ARITHMETIC: case ZR_EXEC_IR_OPCODE_COPY: case ZR_EXEC_IR_OPCODE_MOVE:
        case ZR_EXEC_IR_OPCODE_ADD: case ZR_EXEC_IR_OPCODE_SUB: case ZR_EXEC_IR_OPCODE_MUL:
        case ZR_EXEC_IR_OPCODE_DIV: case ZR_EXEC_IR_OPCODE_NEG: case ZR_EXEC_IR_OPCODE_COMPARE:
        case ZR_EXEC_IR_OPCODE_STORE: case ZR_EXEC_IR_OPCODE_DROP: case ZR_EXEC_IR_OPCODE_BARRIER:
        case ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED:
        case ZR_EXEC_IR_OPCODE_BRANCH: case ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH: case ZR_EXEC_IR_OPCODE_SWITCH:
        case ZR_EXEC_IR_OPCODE_THROW: case ZR_EXEC_IR_OPCODE_SUSPEND: case ZR_EXEC_IR_OPCODE_RETURN:
        case ZR_EXEC_IR_OPCODE_PHI: return ZR_TRUE;
        default: return ZR_FALSE;
    }
}

static TZrBool zr_oracle_truthy(const SZrExecIrOracleValue *v) {
    if (v == ZR_NULL) return ZR_FALSE;
    switch (v->kind) {
        case ZR_EXEC_IR_ORACLE_VALUE_BOOL: return v->as.boolean != 0u;
        case ZR_EXEC_IR_ORACLE_VALUE_SIGNED: return v->as.signedInteger != 0;
        case ZR_EXEC_IR_ORACLE_VALUE_UNSIGNED: return v->as.unsignedInteger != 0u;
        case ZR_EXEC_IR_ORACLE_VALUE_FLOAT: return v->as.floating != 0.0;
        default: return ZR_FALSE;
    }
}

TZrBool zr_oracle_exec(const SZrExecIrOracleInput *input,
                              SZrExecIrOracleExecutionResult *r,
                              const SZrExecIrInstruction *ins,
                              TZrExecIrInstructionId id, TZrExecIrBlockId block,
                              TZrBool *terminated, TZrExecIrBlockId *next,
                              TZrUInt32 *nextOrdinal,
                              SZrExecIrDiagnostic *d) {
    const SZrExecIrFunction *f = input->function;
    TZrUInt32 n = ins->operands.count, i;
    SZrExecIrOracleValue local[ZR_ORACLE_LOCAL_OPERAND_LIMIT];
    SZrExecIrOracleValue *ops = local;
    SZrExecIrOracleValue v, callback;
    TZrBool threw = ZR_FALSE;
    size_t bytes;
    EZrExecIrOpcode op = (EZrExecIrOpcode)ins->opcode;
    if (terminated != ZR_NULL) *terminated = ZR_FALSE;
    if (next != ZR_NULL) *next = ZR_EXEC_IR_BLOCK_ID_INVALID;
    if (nextOrdinal != ZR_NULL) *nextOrdinal = 0u;
    /* Reject a known-but-unmodeled operation before allocating an operand
     * scratch area or touching any value slot. */
    if (!zr_oracle_supported(op, input)) {
        r->unsupportedInstructionId = id;
        zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED, f, block, id,
                       ins->sourceId, 0u, op);
        return ZR_FALSE;
    }
    if (op == ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED) {
        TZrExecIrValueId owner = f->operands[ins->operands.start];
        TZrUInt32 state = r->ownerStates[owner - 1u];
        if (state == ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED ||
            state == ZR_EXEC_IR_STATE_MAP_OWNER_MOVED || state == ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED) {
            ++r->supportedInstructionCount;
            return ZR_TRUE;
        }
        if (state != ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, block, id,
                           ins->sourceId, ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED, state);
            return ZR_FALSE;
        }
    }
    if (n > ZR_ORACLE_LOCAL_OPERAND_LIMIT) {
        if (!zr_oracle_bytes(n, sizeof(*ops), &bytes)) {
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW, f, block, id, ins->sourceId, UINT32_MAX, n); return ZR_FALSE;
        }
        ops = (SZrExecIrOracleValue *)malloc(bytes);
        if (ops == ZR_NULL) { zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY, f, block, id, ins->sourceId, n, 0u); return ZR_FALSE; }
    }
    for (i = 0u; i < n; ++i) {
        TZrExecIrValueId valueId = f->operands[ins->operands.start + i];
        if (valueId == ZR_EXEC_IR_VALUE_ID_INVALID || valueId > r->valueCount ||
            r->values[valueId - 1u].kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED ||
            ((f->values[valueId - 1u].ownership == ZR_EXEC_IR_OWNERSHIP_UNIQUE ||
              f->values[valueId - 1u].ownership == ZR_EXEC_IR_OWNERSHIP_SHARED) &&
             r->ownerStates[valueId - 1u] != ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED)) {
            if (ops != local) free(ops);
            zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, block, id, ins->sourceId, r->valueCount, valueId); return ZR_FALSE;
        }
        ops[i] = r->values[valueId - 1u];
    }
    ++r->supportedInstructionCount;
    zr_oracle_undefined(&v);
    switch (op) {
        case ZR_EXEC_IR_OPCODE_NOP: break;
        case ZR_EXEC_IR_OPCODE_CONSTANT:
            if (input->constants != ZR_NULL && ins->layoutId < input->constantCount) v = input->constants[ins->layoutId];
            else { v.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED; v.as.signedInteger = (TZrInt64)ins->layoutId; }
            if (!zr_oracle_assign(f, ins, r, &v, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_COPY: case ZR_EXEC_IR_OPCODE_CONVERT:
            if (n != 1u) goto invalid;
            v = ops[0];
            if (op == ZR_EXEC_IR_OPCODE_CONVERT) {
                TZrExecIrTypeToken targetType = ins->typeToken;
                if (targetType == 0u && ins->results.count != 0u) {
                    TZrExecIrValueId resultId = f->results[ins->results.start];
                    if (resultId != ZR_EXEC_IR_VALUE_ID_INVALID &&
                        resultId <= f->valueCount) {
                        targetType = f->values[resultId - 1u].typeToken;
                    }
                }
                if ((targetType != 0u &&
                     (ZR_VALUE_IS_TYPE_BOOL(targetType) ||
                      ZR_VALUE_IS_TYPE_NUMBER(targetType))) &&
                    !zr_oracle_convert_scalar(&ops[0], targetType, &v)) {
                    goto arithmetic;
                }
            }
            if (!zr_oracle_assign(f, ins, r, &v, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_MOVE:
            if (n != 1u) goto invalid;
            v = ops[0];
            if (!zr_oracle_assign(f, ins, r, &v, block, id, d)) goto fail;
            /* MOVE transfers the value into its SSA result, then makes the
             * source unavailable just as the state-map ownership contract
             * records it as moved. */
            zr_oracle_consume_operands(f, ins, r);
            break;
        case ZR_EXEC_IR_OPCODE_ADD: case ZR_EXEC_IR_OPCODE_SUB: case ZR_EXEC_IR_OPCODE_MUL:
        case ZR_EXEC_IR_OPCODE_DIV: case ZR_EXEC_IR_OPCODE_ARITHMETIC:
            if (n != 2u || !zr_oracle_binary(ins, &ops[0], &ops[1], &v)) goto arithmetic;
            if (!zr_oracle_assign(f, ins, r, &v, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_NEG:
            if (n != 1u || !zr_oracle_numeric(&ops[0])) goto arithmetic;
            v = ops[0];
            if (v.kind == ZR_EXEC_IR_ORACLE_VALUE_FLOAT) v.as.floating = -v.as.floating;
            else { TZrInt64 x = zr_oracle_signed(&ops[0]); if (x == INT64_MIN) goto arithmetic; v.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED; v.as.signedInteger = -x; }
            if (!zr_oracle_assign(f, ins, r, &v, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_COMPARE:
            if (n != 2u || !zr_oracle_numeric(&ops[0]) || !zr_oracle_numeric(&ops[1])) goto arithmetic;
            v.kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
            {
                int comparison = zr_oracle_compare_values(&ops[0], &ops[1]);
                switch (ins->typeToken) {
                    case 1u: v.as.boolean = (TZrBool)(comparison < 0); break;
                    case 2u: v.as.boolean = (TZrBool)(comparison <= 0); break;
                    case 3u: v.as.boolean = (TZrBool)(comparison > 0); break;
                    case 4u: v.as.boolean = (TZrBool)(comparison >= 0); break;
                    case 5u: v.as.boolean = (TZrBool)(comparison != 0); break;
                    default: v.as.boolean = (TZrBool)(comparison == 0); break;
                }
            }
            if (!zr_oracle_assign(f, ins, r, &v, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_PLACE_BASE:
        case ZR_EXEC_IR_OPCODE_PLACE_PROJECT:
            zr_oracle_undefined(&callback);
            if (input->place == ZR_NULL ||
                !input->place(input->placeUserData, ins, ops, n,
                              &callback)) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_ORACLE_PLACE_ERROR,
                               f, block, id, ins->sourceId,
                               op == ZR_EXEC_IR_OPCODE_PLACE_BASE ? 1u : 2u,
                               n);
                goto fail;
            }
            if (!zr_oracle_value_kind_valid(callback.kind) ||
                callback.kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f,
                               block, id, ins->sourceId,
                               ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                               (TZrUInt32)callback.kind);
                goto fail;
            }
            if (!zr_oracle_assign(f, ins, r, &callback, block, id, d)) {
                goto fail;
            }
            break;
        case ZR_EXEC_IR_OPCODE_ALLOC:
            zr_oracle_undefined(&callback);
            if (input->allocate == ZR_NULL ||
                !input->allocate(input->allocateUserData, ins, ops, n,
                                 &callback)) {
                zr_oracle_diag(d,
                               ZR_EXEC_IR_DIAGNOSTIC_ORACLE_ALLOCATION_ERROR,
                               f, block, id, ins->sourceId, 1u, n);
                goto fail;
            }
            if (!zr_oracle_value_kind_valid(callback.kind) ||
                callback.kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f,
                               block, id, ins->sourceId,
                               ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                               (TZrUInt32)callback.kind);
                goto fail;
            }
            if (!zr_oracle_append_event(r,
                                        ZR_EXEC_IR_ORACLE_EVENT_ALLOCATE, id,
                                        ins->sourceId, ops, n, f, block, d) ||
                !zr_oracle_assign(f, ins, r, &callback, block, id, d)) {
                goto fail;
            }
            break;
        case ZR_EXEC_IR_OPCODE_LOAD:
            if (n != 1u || input->memory == ZR_NULL ||
                !input->memory(input->memoryUserData, ins,
                               ZR_EXEC_IR_ORACLE_MEMORY_LOAD, ops, n, &v)) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_ORACLE_MEMORY_ERROR,
                               f, block, id, ins->sourceId, 1u, n);
                goto fail;
            }
            if (!zr_oracle_value_kind_valid(v.kind) ||
                v.kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
                               f, block, id, ins->sourceId,
                               ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                               (TZrUInt32)v.kind);
                goto fail;
            }
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_LOAD,
                                        id, ins->sourceId, ops, n, f, block, d) ||
                !zr_oracle_assign(f, ins, r, &v, block, id, d)) {
                goto fail;
            }
            break;
        case ZR_EXEC_IR_OPCODE_STORE:
            if (input->memory != ZR_NULL &&
                !input->memory(input->memoryUserData, ins,
                               ZR_EXEC_IR_ORACLE_MEMORY_STORE, ops, n,
                               ZR_NULL)) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_ORACLE_MEMORY_ERROR,
                               f, block, id, ins->sourceId, 2u, n);
                goto fail;
            }
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_STORE, id, ins->sourceId, ops, n, f, block, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_DROP:
        case ZR_EXEC_IR_OPCODE_DROP_IF_INITIALIZED:
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_DROP, id,
                                        ins->sourceId, ops, n, f, block, d)) {
                goto fail;
            }
            /* Publish the bounded observation before clearing the consumed
             * slot.  A later use then fails through the normal invalid-value
             * path, matching the runtime's OWN_DROP slot reset. */
            zr_oracle_consume_operands(f, ins, r);
            break;
        case ZR_EXEC_IR_OPCODE_BARRIER:
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_BARRIER, id, ins->sourceId, ops, n, f, block, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_CALL:
            zr_oracle_undefined(&callback);
            if (input->call == ZR_NULL ||
                !input->call(input->userData, ins, ops, n, &callback)) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_ORACLE_CALL_ERROR,
                               f, block, id, ins->sourceId, 1u, n);
                goto fail;
            }
            if (!zr_oracle_value_kind_valid(callback.kind) ||
                callback.kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f, block,
                               id, ins->sourceId, ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                               (TZrUInt32)callback.kind);
                goto fail;
            }
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_CALL, id, ins->sourceId, ops, n, f, block, d) ||
                !zr_oracle_assign(f, ins, r, &callback, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_INVOKE:
            zr_oracle_undefined(&callback);
            if (ins->successorRange.count != 2u || input->invoke == ZR_NULL ||
                !input->invoke(input->invokeUserData, ins, ops, n, &callback,
                               &threw)) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_ORACLE_INVOKE_ERROR,
                               f, block, id, ins->sourceId, 2u,
                               ins->successorRange.count);
                goto fail;
            }
            if (threw != ZR_FALSE) {
                if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_CALL,
                                            id, ins->sourceId, ops, n, f,
                                            block, d)) {
                    goto fail;
                }
                *nextOrdinal = 1u;
                *next = f->successors[ins->successorRange.start + 1u];
                *terminated = ZR_TRUE;
                break;
            }
            if (!zr_oracle_value_kind_valid(callback.kind) ||
                callback.kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f,
                               block, id, ins->sourceId,
                               ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                               (TZrUInt32)callback.kind);
                goto fail;
            }
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_CALL,
                                        id, ins->sourceId, ops, n, f, block,
                                        d) ||
                !zr_oracle_assign(f, ins, r, &callback, block, id, d)) {
                goto fail;
            }
            *nextOrdinal = 0u;
            *next = f->successors[ins->successorRange.start];
            *terminated = ZR_TRUE;
            break;
        case ZR_EXEC_IR_OPCODE_ITER_INIT:
        case ZR_EXEC_IR_OPCODE_ITER_MOVE_NEXT:
        case ZR_EXEC_IR_OPCODE_ITER_CURRENT:
            zr_oracle_undefined(&callback);
            if (ins->successorRange.count != 2u || input->iterator == ZR_NULL ||
                !input->iterator(input->iteratorUserData, ins, ops, n,
                                  &callback, &threw)) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_ORACLE_ITERATOR_ERROR,
                               f, block, id, ins->sourceId, 2u,
                               ins->successorRange.count);
                goto fail;
            }
            if (threw != ZR_FALSE) {
                if (!zr_oracle_append_event(
                            r, ZR_EXEC_IR_ORACLE_EVENT_ITERATOR, id,
                            ins->sourceId, ops, n, f, block, d)) {
                    goto fail;
                }
                *nextOrdinal = 1u;
                *next = f->successors[ins->successorRange.start + 1u];
                *terminated = ZR_TRUE;
                break;
            }
            if (!zr_oracle_value_kind_valid(callback.kind) ||
                callback.kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f,
                               block, id, ins->sourceId,
                               ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                               (TZrUInt32)callback.kind);
                goto fail;
            }
            if (!zr_oracle_append_event(
                        r, ZR_EXEC_IR_ORACLE_EVENT_ITERATOR, id,
                        ins->sourceId, ops, n, f, block, d) ||
                !zr_oracle_assign(f, ins, r, &callback, block, id, d)) {
                goto fail;
            }
            *nextOrdinal = 0u;
            *next = f->successors[ins->successorRange.start];
            *terminated = ZR_TRUE;
            break;
        case ZR_EXEC_IR_OPCODE_TYPE_TEST:
            if (n != 1u) goto invalid;
            v.kind = ZR_EXEC_IR_ORACLE_VALUE_BOOL;
            v.as.boolean = ZR_FALSE;
            if (input->typeTest == ZR_NULL ||
                !input->typeTest(input->typeTestUserData, ins, &ops[0],
                                 ins->matchTypeToken, &v.as.boolean)) {
                zr_oracle_diag(d,
                               ZR_EXEC_IR_DIAGNOSTIC_ORACLE_TYPE_TEST_ERROR,
                               f, block, id, ins->sourceId, 1u, n);
                goto fail;
            }
            if (!zr_oracle_assign(f, ins, r, &v, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_EXCEPTION_PAYLOAD:
            zr_oracle_undefined(&callback);
            if (n != 0u || input->exceptionPayload == ZR_NULL ||
                !input->exceptionPayload(input->exceptionPayloadUserData, ins,
                                         &callback)) {
                zr_oracle_diag(
                        d,
                        ZR_EXEC_IR_DIAGNOSTIC_ORACLE_EXCEPTION_PAYLOAD_ERROR,
                        f, block, id, ins->sourceId, 0u, n);
                goto fail;
            }
            if (!zr_oracle_value_kind_valid(callback.kind) ||
                callback.kind == ZR_EXEC_IR_ORACLE_VALUE_UNDEFINED) {
                zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE, f,
                               block, id, ins->sourceId,
                               ZR_EXEC_IR_ORACLE_VALUE_KIND_COUNT,
                               (TZrUInt32)callback.kind);
                goto fail;
            }
            if (!zr_oracle_assign(f, ins, r, &callback, block, id, d)) goto fail;
            break;
        case ZR_EXEC_IR_OPCODE_BRANCH:
            if (ins->successorRange.count < 1u) goto invalid;
            *next = f->successors[ins->successorRange.start];
            *terminated = ZR_TRUE;
            break;
        case ZR_EXEC_IR_OPCODE_CONDITIONAL_BRANCH:
            if (n != 1u || ins->successorRange.count < 2u) goto invalid;
            *nextOrdinal = zr_oracle_truthy(&ops[0]) ? 0u : 1u;
            *next = f->successors[ins->successorRange.start + *nextOrdinal];
            *terminated = ZR_TRUE;
            break;
        case ZR_EXEC_IR_OPCODE_SWITCH: { TZrUInt64 choice; if (n < 1u || ins->successorRange.count == 0u) goto invalid; choice = zr_oracle_unsigned(&ops[0]); if (choice >= ins->successorRange.count) choice = ins->successorRange.count - 1u; *nextOrdinal = (TZrUInt32)choice; *next = f->successors[ins->successorRange.start + *nextOrdinal]; *terminated = ZR_TRUE; break; }
        case ZR_EXEC_IR_OPCODE_RETURN:
            if (n > 0u) r->returnValue = ops[0];
            r->returned = ZR_TRUE;
            *terminated = ZR_TRUE;
            break;
        case ZR_EXEC_IR_OPCODE_THROW:
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_THROW, id, ins->sourceId, ops, n, f, block, d)) goto fail;
            r->terminatedByThrow = ZR_TRUE;
            *terminated = ZR_TRUE;
            break;
        case ZR_EXEC_IR_OPCODE_SUSPEND:
            if (!zr_oracle_append_event(r, ZR_EXEC_IR_ORACLE_EVENT_SUSPEND, id, ins->sourceId, ops, n, f, block, d)) goto fail;
            if (n > 0u) r->returnValue = ops[0];
            if (n > 0u && !zr_oracle_assign(f, ins, r, &ops[0], block, id, d)) goto fail;
            r->suspended = ZR_TRUE;
            *terminated = ZR_TRUE;
            break;
        case ZR_EXEC_IR_OPCODE_PHI: break;
        default: goto fail;
    }
    if (ops != local) free(ops);
    return ZR_TRUE;
arithmetic:
    zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_ARITHMETIC_ERROR, f, block, id, ins->sourceId, 1u, 0u);
    goto fail;
invalid:
    zr_oracle_diag(d, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, f, block, id, ins->sourceId, 1u, n);
fail:
    if (ops != local) free(ops);
    return ZR_FALSE;
}
