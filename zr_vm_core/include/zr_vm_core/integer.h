#ifndef ZR_VM_CORE_INTEGER_H
#define ZR_VM_CORE_INTEGER_H

#include <stdint.h>
#include "zr_vm_core/debug.h"

/* Shared interpreter/AOT integer semantics. Check before evaluating a signed
 * operation; neither compiler overflow flags nor host shift rules are needed. */
static inline TZrInt64 ZrCore_Integer_Add(struct SZrState *state, TZrInt64 left, TZrInt64 right) {
    if ((right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right)) {
        ZrCore_Debug_RunError(state, "signed integer addition overflow");
    }
    return left + right;
}

static inline TZrInt64 ZrCore_Integer_Subtract(struct SZrState *state, TZrInt64 left, TZrInt64 right) {
    if ((right < 0 && left > INT64_MAX + right) ||
        (right > 0 && left < INT64_MIN + right)) {
        ZrCore_Debug_RunError(state, "signed integer subtraction overflow");
    }
    return left - right;
}

static inline TZrInt64 ZrCore_Integer_Negate(struct SZrState *state, TZrInt64 value) {
    if (value == INT64_MIN) {
        ZrCore_Debug_RunError(state, "signed integer negation overflow");
    }
    return -value;
}

/* Convert two's-complement bits without an out-of-range unsigned-to-signed cast. */
static inline TZrInt64 ZrCore_Integer_FromBits(TZrUInt64 bits) {
    const TZrUInt64 signBit = (TZrUInt64)1u << 63u;
    return bits < signBit ? (TZrInt64)bits : INT64_MIN + (TZrInt64)(bits - signBit);
}

static inline TZrInt64 ZrCore_Integer_NegateUnsigned(struct SZrState *state, TZrUInt64 value) {
    if (value > ((TZrUInt64)1u << 63u)) {
        ZrCore_Debug_RunError(state, "signed integer negation overflow");
    }
    return ZrCore_Integer_FromBits((TZrUInt64)0u - value);
}

/* Counts use their low six bits. Signed right shift fills with the sign bit;
 * left shift operates on bits and can discard high bits. */
static inline TZrInt64 ZrCore_Integer_ShiftLeft(TZrInt64 value, TZrUInt64 count) {
    return ZrCore_Integer_FromBits((TZrUInt64)value << (count & 63u));
}

static inline TZrInt64 ZrCore_Integer_ShiftRight(TZrInt64 value, TZrUInt64 count) {
    const unsigned shift = (unsigned)(count & 63u);
    TZrUInt64 bits = (TZrUInt64)value >> shift;
    if (value < 0 && shift != 0u) {
        bits |= UINT64_MAX << (64u - shift);
    }
    return ZrCore_Integer_FromBits(bits);
}

static inline TZrInt64 ZrCore_Integer_Modulo(struct SZrState *state, TZrInt64 left, TZrInt64 right) {
    if (right == 0) {
        ZrCore_Debug_RunError(state, "modulo by zero");
    }
    return right == -1 ? 0 : left % right;
}

#endif
