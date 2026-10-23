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

#include "bytecode/bytecode_verify.h"
static TZrBool io_verify_constant_function(const void *context, TZrSize index) {
    const SZrIoFunction *f = (const SZrIoFunction *)context;
    return (TZrBool)(f->constantVariables[index].hasFunctionValue && f->constantVariables[index].functionValue != ZR_NULL);
}
static TZrSize io_verify_call_arguments(const void *context, TZrSize index) {
    return ((const SZrIoFunction *)context)->callSiteCaches[index].argumentCount;
}
static TZrBool io_verify_slot(const SZrIoFunction *f, TZrInt64 slot) {
    return (TZrBool)(slot >= 0 && (TZrUInt64)slot < f->stackSize);
}
static TZrBool io_verify_body(const SZrIoFunction *f) {
    SZrBytecodeVerificationView view = {0};
    view.instructions = f->instructions;
    view.instructionsLength = f->instructionsLength;
    view.stackSize = f->stackSize;
    view.constantVariablesLength = f->constantVariablesLength;
    view.closureVariablesLength = f->closureVariablesLength;
    view.memberEntriesLength = f->memberEntriesLength;
    view.callSiteCacheLength = f->callSiteCacheLength;
    view.prototypesLength = f->prototypesLength;
    view.exceptionHandlerCount = f->exceptionHandlerCount;
    view.arrayInitialLengthAllowed = (TZrBool)(f->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_ARRAY_INITIAL_LENGTH);
    view.arrayInitialLengthLimit = ZR_IO_MAX_DECODE_BYTES / sizeof(SZrTypeValue);
    view.context = f;
    view.constantIsFunction = io_verify_constant_function;
    view.callArgumentCount = io_verify_call_arguments;
    return bytecode_verify_body(&view);
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
    if (!io_verify_body(f)) return ZR_FALSE;
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

