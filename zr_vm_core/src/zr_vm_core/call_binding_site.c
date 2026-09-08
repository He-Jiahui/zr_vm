#include "call_binding_site.h"

enum {
    CALL_BINDING_ACCESSOR_NONE = 0u,
    CALL_BINDING_ACCESSOR_GET = 1u,
    CALL_BINDING_ACCESSOR_SET = 2u,
    CALL_BINDING_ACCESSOR_INIT = 3u
};

TZrBool zr_call_binding_site_matches(const SZrFunction *function, TZrUInt32 cacheIndex) {
    const SZrFunctionCallSiteCacheEntry *entry;
    const SZrInstruction *instruction;
    TZrUInt32 kind;
    TZrUInt32 operation = ZR_CALL_BINDING_OPERATION_CALL;
    TZrUInt32 operandCacheIndex = ZR_CALL_BINDING_SLOT_NONE;
    if (function == ZR_NULL || function->callSiteCaches == ZR_NULL ||
        cacheIndex >= function->callSiteCacheLength || function->instructionsList == ZR_NULL) return ZR_FALSE;
    entry = &function->callSiteCaches[cacheIndex];
    if (entry->instructionIndex >= function->instructionsLength) return ZR_FALSE;
    instruction = &function->instructionsList[entry->instructionIndex].instruction;
    switch ((EZrInstructionCode)instruction->operationCode) {
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
            kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET;
            operation = ZR_CALL_BINDING_OPERATION_GET;
            operandCacheIndex = instruction->operand.operand1[1];
            break;
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
            kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC;
            operation = ZR_CALL_BINDING_OPERATION_GET;
            operandCacheIndex = instruction->operand.operand1[1];
            break;
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
            kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET;
            operation = ZR_CALL_BINDING_OPERATION_SET;
            operandCacheIndex = instruction->operand.operand1[1];
            break;
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
            kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC;
            operation = ZR_CALL_BINDING_OPERATION_SET;
            operandCacheIndex = instruction->operand.operand1[1];
            break;
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
            operandCacheIndex = instruction->operand.operand1[1];
            /* fall through */
        case ZR_INSTRUCTION_ENUM(META_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
            kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL;
            operation = ZR_CALL_BINDING_OPERATION_META;
            break;
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            operandCacheIndex = instruction->operand.operand1[1];
            /* fall through */
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
            kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL;
            operation = ZR_CALL_BINDING_OPERATION_META;
            break;
        case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
            kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
            operandCacheIndex = instruction->operand.operand1[1];
            break;
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL):
            kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
            operandCacheIndex = instruction->operand.operand1[0];
            break;
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL_RECV_U8):
            kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET;
            operandCacheIndex = instruction->operand.operand0[0];
            break;
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL_SPREAD):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
            kind = ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL;
            break;
        default: return ZR_FALSE;
    }
    if (entry->kind != kind || entry->binding.contract.operation != operation ||
        (operandCacheIndex != ZR_CALL_BINDING_SLOT_NONE && operandCacheIndex != cacheIndex)) return ZR_FALSE;
    if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION &&
        kind != ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL) return ZR_FALSE;
    if (entry->bindingLocation.kind == ZR_CALL_BINDING_RELOCATION_VM_MODULE &&
        entry->binding.contract.ownerTypeToken != 0u &&
        ZR_METADATA_TOKEN_TABLE(entry->binding.contract.ownerTypeToken) != ZR_METADATA_TABLE_TYPE_DEF)
        return ZR_FALSE;
    return ZR_TRUE;
}

TZrBool zr_call_binding_descriptor_matches(const SZrFunction *function,
        const SZrFunctionCallSiteCacheEntry *entry, const SZrMemberDescriptor *descriptor) {
    TZrBool isStatic;
    TZrBool isInitializer = ZR_FALSE;
    if (function == ZR_NULL || entry == ZR_NULL || descriptor == ZR_NULL) return ZR_FALSE;
    if (entry->memberEntryIndex < function->memberEntryLength && function->memberEntries != ZR_NULL) {
        const SZrFunctionMemberEntry *member = &function->memberEntries[entry->memberEntryIndex];
        isStatic = (member->reserved0 & ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR) != 0u;
        isInitializer = (member->reserved0 & ZR_FUNCTION_MEMBER_ENTRY_FLAG_PROPERTY_INITIALIZER) != 0u;
    } else {
        if (entry->kind != ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL) return ZR_FALSE;
        isStatic = ZR_TRUE;
    }
    if (isStatic != !!descriptor->isStatic ||
        isInitializer != (descriptor->accessorRole == CALL_BINDING_ACCESSOR_INIT)) return ZR_FALSE;
    if ((entry->binding.contract.operation == ZR_CALL_BINDING_OPERATION_GET ||
         entry->binding.contract.operation == ZR_CALL_BINDING_OPERATION_SET) &&
        isStatic != (entry->kind == ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC ||
                     entry->kind == ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC)) return ZR_FALSE;
    switch (entry->binding.contract.operation) {
        case ZR_CALL_BINDING_OPERATION_GET:
            if (descriptor->accessorRole != CALL_BINDING_ACCESSOR_GET) return ZR_FALSE;
            break;
        case ZR_CALL_BINDING_OPERATION_SET:
            if (descriptor->accessorRole != CALL_BINDING_ACCESSOR_SET &&
                descriptor->accessorRole != CALL_BINDING_ACCESSOR_INIT) return ZR_FALSE;
            break;
        case ZR_CALL_BINDING_OPERATION_CALL:
            if (descriptor->accessorRole != CALL_BINDING_ACCESSOR_NONE) return ZR_FALSE;
            break;
        default: return ZR_FALSE;
    }
    if (descriptor->kind != ZR_MEMBER_DESCRIPTOR_KIND_METHOD &&
        descriptor->kind != ZR_MEMBER_DESCRIPTOR_KIND_STATIC_MEMBER) return ZR_FALSE;
    if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_VIRTUAL &&
        (isStatic || descriptor->virtualSlotIndex != entry->binding.contract.dispatchSlot)) return ZR_FALSE;
    if (entry->binding.contract.bindingKind == ZR_CALL_BINDING_INTERFACE &&
        (isStatic || descriptor->interfaceContractSlot != entry->binding.contract.dispatchSlot)) return ZR_FALSE;
    return ZR_TRUE;
}
