/* Shared VM storage/control-flow contract for source, ExecIR, host and IO.
 * Value type checks remain runtime checks. New opcodes need explicit roles. */
#ifndef ZR_VM_BYTECODE_VERIFY_H
#define ZR_VM_BYTECODE_VERIFY_H
#include "zr_vm_core/function.h"
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct SZrBytecodeVerificationView {
    const TZrInstruction *instructions;
    TZrSize instructionsLength, stackSize, constantVariablesLength;
    TZrSize closureVariablesLength, memberEntriesLength, callSiteCacheLength;
    TZrSize prototypesLength, exceptionHandlerCount, arrayInitialLengthLimit;
    TZrBool arrayInitialLengthAllowed;
    /* Private host compatibility mode; never grants the verified fast path. */
    TZrBool allowGuardedFallthrough;
    const void *context;
    TZrBool (*constantIsFunction)(const void *, TZrSize);
    TZrSize (*callArgumentCount)(const void *, TZrSize);
} SZrBytecodeVerificationView;
static TZrBool bytecode_verify_slot(const SZrBytecodeVerificationView *f, TZrInt64 slot) {
    return (TZrBool)(slot >= 0 && (TZrUInt64)slot < f->stackSize);
}

static TZrBool bytecode_verify_window(const SZrBytecodeVerificationView *f, TZrSize start, TZrSize count) {
    return (TZrBool)(start <= f->stackSize && count <= f->stackSize - start);
}

static TZrBool bytecode_verify_relative_target(const SZrBytecodeVerificationView *f, TZrSize pc,
                                         TZrInt32 offset, TZrUInt32 step) {
    /* Do arithmetic on indices before the interpreter forms a pointer. */
    TZrInt64 target = (TZrInt64)pc + offset + step;
    return (TZrBool)(target >= 0 && ((TZrUInt64)target < f->instructionsLength ||
            (f->allowGuardedFallthrough && (TZrUInt64)target == f->instructionsLength)));
}

static TZrBool bytecode_verify_instruction(const SZrBytecodeVerificationView *f, TZrSize pc) {
    const TZrInstruction *i = &f->instructions[pc];
    TZrUInt16 e = i->instruction.operandExtra;
    TZrUInt16 a = i->instruction.operand.operand1[0];
    TZrUInt16 b = i->instruction.operand.operand1[1];
    TZrInt32 w = i->instruction.operand.operand2[0];
    TZrUInt8 x = i->instruction.operand.operand0[0];
    TZrUInt8 y = i->instruction.operand.operand0[1];
    TZrUInt8 z = i->instruction.operand.operand0[2];
#define S(slot) bytecode_verify_slot(f, (slot))
#define D(slot) ((slot) == ZR_INSTRUCTION_USE_RET_FLAG || S(slot))
#define K(index) ((TZrUInt64)(index) < f->constantVariablesLength)
#define W(start, count) bytecode_verify_window(f, (start), (count))
    switch (i->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(PROPERTY_REF_CREATE_INDEX):
        case ZR_INSTRUCTION_ENUM(BIND_INLINE_ARRAY_ELEMENT_PLACE):
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
        case ZR_INSTRUCTION_ENUM(POW):
        case ZR_INSTRUCTION_ENUM(POW_SIGNED):
        case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(POW_FLOAT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
            return (TZrBool)(D(e) && S(a) && S(b));

        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS):
            return (TZrBool)(S(e) && S(a) && S(b));

        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
            return (TZrBool)(D(e) && S(a) && S(b));

        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST):
            return (TZrBool)(D(e) && S(a) && K(b));

        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
            return (TZrBool)(S(e) && S(a) && K(b));

        case ZR_INSTRUCTION_ENUM(ITER_INIT):
        case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
        case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
        case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(TO_INT_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED):
        case ZR_INSTRUCTION_ENUM(TO_STRING):
        case ZR_INSTRUCTION_ENUM(TO_STRUCT):
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
        case ZR_INSTRUCTION_ENUM(NEG):
        case ZR_INSTRUCTION_ENUM(NEG_SIGNED):
        case ZR_INSTRUCTION_ENUM(NEG_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL):
        case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
        case ZR_INSTRUCTION_ENUM(TYPEOF):
        case ZR_INSTRUCTION_ENUM(OWN_UNIQUE):
        case ZR_INSTRUCTION_ENUM(OWN_BORROW):
        case ZR_INSTRUCTION_ENUM(OWN_LOAN):
        case ZR_INSTRUCTION_ENUM(OWN_SHARE):
        case ZR_INSTRUCTION_ENUM(OWN_DEGRADE):
        case ZR_INSTRUCTION_ENUM(OWN_DETACH):
        case ZR_INSTRUCTION_ENUM(OWN_WAKE):
        case ZR_INSTRUCTION_ENUM(OWN_DROP):
        case ZR_INSTRUCTION_ENUM(OWN_RETURN_LOAN):
        case ZR_INSTRUCTION_ENUM(OWN_VIEW_SHARED):
        case ZR_INSTRUCTION_ENUM(OWN_VIEW_MUT):
        case ZR_INSTRUCTION_ENUM(OWN_INTO_GC_BOX):
        case ZR_INSTRUCTION_ENUM(OWN_RETURN_TO_GC):
        case ZR_INSTRUCTION_ENUM(REQUIRE_NON_NULL):
        case ZR_INSTRUCTION_ENUM(PROPERTY_REF_STORE):
            return (TZrBool)(D(e) && S(a));

        case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL2):
        case ZR_INSTRUCTION_ENUM(MARK_CLOSE_PROXY):
            return (TZrBool)(S(e) && S(a));

        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
            return (TZrBool)(D(e));

        case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL):
        case ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED):
        case ZR_INSTRUCTION_ENUM(THROW):
        case ZR_INSTRUCTION_ENUM(CATCH):
            return (TZrBool)(S(e));

        case ZR_INSTRUCTION_ENUM(GET_STACK):
            return (TZrBool)(D(e) && S(w));

        case ZR_INSTRUCTION_ENUM(SET_STACK):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS):
        case ZR_INSTRUCTION_ENUM(PROPERTY_REF_LOAD):
        case ZR_INSTRUCTION_ENUM(PROPERTY_REF_CREATE_LOCAL):
            return (TZrBool)(S(e) && S(w));

        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
            return (TZrBool)(D(e) && K(w));

        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
            return (TZrBool)(S(e) && w >= 0 && (TZrSize)w < f->closureVariablesLength);

        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(SETUPVAL):
            return (TZrBool)(S(e) && a < f->closureVariablesLength);

        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
        case ZR_INSTRUCTION_ENUM(META_GET):
        case ZR_INSTRUCTION_ENUM(META_SET):
        case ZR_INSTRUCTION_ENUM(PROPERTY_REF_CREATE_MEMBER):
            return (TZrBool)(D(e) && S(a) && b < f->memberEntriesLength);

        case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
            return (TZrBool)(D(e) && S(a) && b < f->callSiteCacheLength);

        case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT_NULL):
            return (TZrBool)(S(a) && b < f->callSiteCacheLength);

        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_MOD_CONST):
            return (TZrBool)(D(e) && S(x) && S(y) && K(b));

        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK):
            return (TZrBool)(D(e) && S(x) && S(y));

        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
            return (TZrBool)(D(e) && S(x) && S(y) && S(z) && K(b));

        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4):
            return (TZrBool)(W(a, 4u) && S(b));

        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST):
            return (TZrBool)(W(a, 4u) && K(b));

        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST):
            return (TZrBool)(W(a, 4u) && S(b) && K(e));

        case ZR_INSTRUCTION_ENUM(JUMP):
            return (TZrBool)(bytecode_verify_relative_target(f, pc, w, 1u));

        case ZR_INSTRUCTION_ENUM(JUMP_IF):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):
            return (TZrBool)(S(e) && bytecode_verify_relative_target(f, pc, w, 1u));

        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
            return (TZrBool)(S(e) && S(a) && bytecode_verify_relative_target(f, pc, (TZrInt16)b, 1u));

        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_EQUAL_SIGNED_CONST):

        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED_CONST):

        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
            return (TZrBool)(S(e) && K(a) && bytecode_verify_relative_target(f, pc, (TZrInt16)b, 1u));

        case ZR_INSTRUCTION_ENUM(JUMP_IF_NULL):
            return (TZrBool)(S(e) && bytecode_verify_relative_target(f, pc, (TZrInt16)b, 1u));

        case ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            return (TZrBool)(D(e) && S(a) && pc + 2u < f->instructionsLength && bytecode_verify_relative_target(f, pc, (TZrInt16)b, 2u));

        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL_SPREAD):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(META_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
            return (TZrBool)(D(e) && W(a, (TZrSize)b + 1u));

        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
            return (TZrBool)(D(e) && S(a));

        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            return (TZrBool)(D(e) && b < f->callSiteCacheLength && W(a, (TZrSize)f->callArgumentCount(f->context, b) + 1u));

        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL):
            return (TZrBool)(a < f->callSiteCacheLength && W(e, (TZrSize)f->callArgumentCount(f->context, a) + 1u));

        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL):
            /* A is a cache index; the call window starts at E and includes
             * the callable, receiver, and explicit arguments. */
            return (TZrBool)(a < f->callSiteCacheLength && b > 0u && W(e, (TZrSize)b + 1u));

        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8):
            return (TZrBool)(x < f->callSiteCacheLength && W(e, 3u) && S(y) && S(z));

        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL_RECV_U8):
            return (TZrBool)(x < f->callSiteCacheLength && z > 0u && W(e, (TZrSize)z + 1u) && S(y));

        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            return (TZrBool)(b <= 1u && W(a, e));

        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            return (TZrBool)(D(e) && K(a) && f->constantIsFunction(f->context, a));

        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
            return (TZrBool)(D(e));

        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
            return (TZrBool)(D(e) && w >= 0 && f->arrayInitialLengthAllowed && (TZrUInt32)w <= f->arrayInitialLengthLimit);

        case ZR_INSTRUCTION_ENUM(CREATE_INLINE_ARRAY):
            return (TZrBool)(D(e) && a < f->prototypesLength);

        case ZR_INSTRUCTION_ENUM(TRY):
        case ZR_INSTRUCTION_ENUM(END_TRY):
        case ZR_INSTRUCTION_ENUM(END_FINALLY):
            return (TZrBool)(e < f->exceptionHandlerCount);

        case ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN):
            return (TZrBool)(S(e) && w >= 0 && (TZrSize)w < f->instructionsLength);

        case ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE):
            return (TZrBool)(w >= 0 && (TZrSize)w < f->instructionsLength);

        case ZR_INSTRUCTION_ENUM(NOP):
        case ZR_INSTRUCTION_ENUM(CLOSE_SCOPE):
            return (TZrBool)(ZR_TRUE);

        default:
            /* New or unsupported opcodes require an explicit operand contract. */
            return ZR_FALSE;
    }
#undef W
#undef K
#undef D
#undef S
}

static TZrBool bytecode_verify_body(const SZrBytecodeVerificationView *f) {
    if (f->instructions == ZR_NULL || f->instructionsLength == 0u ||
        f->instructionsLength > INT32_MAX || f->stackSize > ZR_VM_MAX_STACK) return ZR_FALSE;
    if (!f->allowGuardedFallthrough) switch (f->instructions[f->instructionsLength - 1u].instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            if (f->instructions[f->instructionsLength - 1u].instruction.operand.operand1[1] != 0u) return ZR_FALSE;
            break;
        case ZR_INSTRUCTION_ENUM(JUMP):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            break;
        default: return ZR_FALSE;
    }
    for (TZrSize pc = 0; pc < f->instructionsLength; ++pc)
        if (!bytecode_verify_instruction(f, pc)) {
            if (getenv("ZR_VM_TRACE_BYTECODE_VERIFY")) fprintf(stderr, "bytecode rejected pc=%zu op=%u e=%u a=%u b=%u stack=%zu constants=%zu closures=%zu\n", (size_t)pc, f->instructions[pc].instruction.operationCode, f->instructions[pc].instruction.operandExtra, f->instructions[pc].instruction.operand.operand1[0], f->instructions[pc].instruction.operand.operand1[1], (size_t)f->stackSize, (size_t)f->constantVariablesLength, (size_t)f->closureVariablesLength);
            return ZR_FALSE;
        }
    return ZR_TRUE;
}
#endif
