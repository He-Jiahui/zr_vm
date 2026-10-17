/* Structural validation for the legacy VM bytecode loaded from .zro files.
 * Keep operand roles explicit: E is not always a destination, and fused
 * instructions pack byte operands instead of two 16-bit slots.
 *
 * This checks storage/control-flow boundaries, not value-type dataflow. Typed
 * instructions must still validate runtime values before dereferencing them.
 * Included only by io.c after the bounded decoder's resource limits. */
#ifndef ZR_VM_IO_BYTECODE_VERIFY_H
#define ZR_VM_IO_BYTECODE_VERIFY_H

#include "zr_vm_core/constant_reference.h"

static TZrBool io_verify_slot(const SZrIoFunction *f, TZrInt64 slot) {
    return (TZrBool)(slot >= 0 && (TZrUInt64)slot < f->stackSize);
}

static TZrBool io_verify_window(const SZrIoFunction *f, TZrSize start, TZrSize count) {
    return (TZrBool)(start <= f->stackSize && count <= f->stackSize - start);
}

static TZrBool io_verify_relative_target(const SZrIoFunction *f, TZrSize pc,
                                         TZrInt32 offset, TZrUInt32 step) {
    /* Do arithmetic on indices before the interpreter forms a pointer. */
    TZrInt64 target = (TZrInt64)pc + offset + step;
    return (TZrBool)(target >= 0 && (TZrUInt64)target < f->instructionsLength);
}

static TZrBool io_verify_instruction(const SZrIoFunction *f, TZrSize pc) {
    const TZrInstruction *i = &f->instructions[pc];
    TZrUInt16 e = i->instruction.operandExtra;
    TZrUInt16 a = i->instruction.operand.operand1[0];
    TZrUInt16 b = i->instruction.operand.operand1[1];
    TZrInt32 w = i->instruction.operand.operand2[0];
    TZrUInt8 x = i->instruction.operand.operand0[0];
    TZrUInt8 y = i->instruction.operand.operand0[1];
    TZrUInt8 z = i->instruction.operand.operand0[2];
#define S(slot) io_verify_slot(f, (slot))
#define D(slot) ((slot) == ZR_INSTRUCTION_USE_RET_FLAG || S(slot))
#define K(index) ((TZrUInt64)(index) < f->constantVariablesLength)
#define W(start, count) io_verify_window(f, (start), (count))
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
            return (TZrBool)(io_verify_relative_target(f, pc, w, 1u));

        case ZR_INSTRUCTION_ENUM(JUMP_IF):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):
            return (TZrBool)(S(e) && io_verify_relative_target(f, pc, w, 1u));

        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
            return (TZrBool)(S(e) && S(a) && io_verify_relative_target(f, pc, (TZrInt16)b, 1u));

        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
            return (TZrBool)(S(e) && K(a) && io_verify_relative_target(f, pc, (TZrInt16)b, 1u));

        case ZR_INSTRUCTION_ENUM(JUMP_IF_NULL):
            return (TZrBool)(S(e) && io_verify_relative_target(f, pc, (TZrInt16)b, 1u));

        case ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            return (TZrBool)(D(e) && S(a) && pc + 2u < f->instructionsLength && io_verify_relative_target(f, pc, (TZrInt16)b, 2u));

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
            return (TZrBool)(D(e) && b < f->callSiteCacheLength && W(a, (TZrSize)f->callSiteCaches[b].argumentCount + 1u));

        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL):
            return (TZrBool)(a < f->callSiteCacheLength && W(e, (TZrSize)f->callSiteCaches[a].argumentCount + 1u));

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
            return (TZrBool)(D(e) && K(a) && f->constantVariables[a].hasFunctionValue && f->constantVariables[a].functionValue != ZR_NULL);

        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
            return (TZrBool)(D(e));

        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
            return (TZrBool)(D(e) && w >= 0 && f->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_ARRAY_INITIAL_LENGTH && (TZrUInt32)w <= ZR_IO_MAX_DECODE_BYTES / sizeof(SZrTypeValue));

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

static TZrBool io_verify_child_captures(const SZrIoFunction *parent,
                                       const SZrIoFunction *child) {
    if (child == ZR_NULL) return ZR_FALSE;
    for (TZrSize n = 0; n < child->closureVariablesLength; ++n) {
        const SZrIoFunctionClosureVariable *capture = &child->closureVariables[n];
        if (capture->inStack > 1u ||
            (capture->inStack ? capture->index >= parent->stackSize
                              : capture->index >= parent->closureVariablesLength)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool io_verify_prototype_blob(const SZrIoFunction *f) {
    TZrUInt32 count;
    TZrSize cursor = sizeof(count);
    if (f->prototypeDataLength == 0u) return ZR_TRUE;
    if (f->prototypeDataLength < sizeof(count)) return ZR_FALSE;
    ZrCore_Memory_RawCopy(&count, f->prototypeData, sizeof(count));
    if (count != f->prototypesLength ||
        count > (f->prototypeDataLength - cursor) / sizeof(SZrCompiledPrototypeInfo)) return ZR_FALSE;
    for (TZrUInt32 n = 0; n < count; ++n) {
        SZrCompiledPrototypeInfo p;
        TZrSize indices;
        if (sizeof(p) > f->prototypeDataLength - cursor) return ZR_FALSE;
        ZrCore_Memory_RawCopy(&p, f->prototypeData + cursor, sizeof(p));
        cursor += sizeof(p);
        indices = p.inheritsCount;
        if (p.nameStringIndex >= f->constantVariablesLength ||
            f->constantVariables[p.nameStringIndex].type != ZR_VALUE_TYPE_STRING ||
            indices > (f->prototypeDataLength - cursor) / sizeof(TZrUInt32)) return ZR_FALSE;
        cursor += indices * sizeof(TZrUInt32);
        indices = p.decoratorsCount;
        if (indices > (f->prototypeDataLength - cursor) / sizeof(TZrUInt32)) return ZR_FALSE;
        cursor += indices * sizeof(TZrUInt32);
        if (p.membersCount > (f->prototypeDataLength - cursor) / sizeof(SZrCompiledMemberInfo)) return ZR_FALSE;
        cursor += (TZrSize)p.membersCount * sizeof(SZrCompiledMemberInfo);
    }
    return (TZrBool)(cursor == f->prototypeDataLength);
}

static TZrBool io_verify_function(const SZrIoFunction *f) {
    if (f->stackSize > ZR_VM_MAX_STACK || f->parametersLength > UINT16_MAX ||
        f->parametersLength > f->stackSize || f->hasVarArgs > 1u ||
        f->closureVariablesLength > ZR_VM_STACK_CLOSURE_MAX ||
        f->instructionsLength > INT32_MAX || f->frameByteSize > ZR_IO_MAX_DECODE_BYTES ||
        (f->vmEntryClearStackSizePlusOne != 0u &&
         f->vmEntryClearStackSizePlusOne - 1u > f->stackSize) ||
        !io_verify_prototype_blob(f)) return ZR_FALSE;
    for (TZrSize n = 0; n < f->instructionsLength; ++n) {
        if (!io_verify_instruction(f, n)) return ZR_FALSE;
    }
    for (TZrSize n = 0; n < f->localVariablesLength; ++n) {
        const SZrIoFunctionLocalVariable *v = &f->localVariables[n];
        if (!io_verify_slot(f, v->stackSlot) ||
            v->instructionStartIndex > v->instructionEndIndex ||
            v->instructionEndIndex > f->instructionsLength) return ZR_FALSE;
    }
    for (TZrSize n = 0; n < f->exportedVariablesLength; ++n) {
        if (!io_verify_slot(f, f->exportedVariables[n].stackSlot)) return ZR_FALSE;
    }
    for (TZrSize n = 0; n < f->returnEscapeSlotCount; ++n) {
        if (!io_verify_slot(f, f->returnEscapeSlots[n])) return ZR_FALSE;
    }
    for (TZrSize n = 0; n < f->catchClauseCount; ++n) {
        if (f->catchClauses[n].targetInstructionOffset >= f->instructionsLength) return ZR_FALSE;
    }
    for (TZrSize n = 0; n < f->exceptionHandlerCount; ++n) {
        const SZrIoFunctionExceptionHandler *h = &f->exceptionHandlers[n];
        if (h->hasFinally > 1u || h->protectedStartInstructionOffset >= f->instructionsLength ||
            h->catchClauseStartIndex > f->catchClauseCount ||
            h->catchClauseCount > f->catchClauseCount - h->catchClauseStartIndex ||
            (h->hasFinally && (h->finallyTargetInstructionOffset >= f->instructionsLength ||
                               h->afterFinallyInstructionOffset >= f->instructionsLength))) return ZR_FALSE;
    }
    for (TZrSize n = 0; n < f->debugInfosLength; ++n) {
        if (f->debugInfos[n].instructionsLength != f->instructionsLength) return ZR_FALSE;
    }
    for (TZrSize n = 0; n < f->callSiteCacheLength; ++n) {
        const SZrIoFunctionCallSiteCacheEntry *c = &f->callSiteCaches[n];
        if (c->kind == ZR_FUNCTION_CALLSITE_CACHE_KIND_NONE ||
            c->kind > ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL ||
            c->instructionIndex >= f->instructionsLength || c->argumentCount > f->stackSize) return ZR_FALSE;
    }
    for (TZrSize n = 0; n < f->closuresLength; ++n) {
        if (!io_verify_child_captures(f, f->closures[n].subFunction)) return ZR_FALSE;
    }
    for (TZrSize n = 0; n < f->constantVariablesLength; ++n) {
        const SZrIoFunctionConstantVariable *c = &f->constantVariables[n];
        if (c->hasFunctionValue && !io_verify_child_captures(f, c->functionValue)) return ZR_FALSE;
    }
    return ZR_TRUE;
}

#endif

