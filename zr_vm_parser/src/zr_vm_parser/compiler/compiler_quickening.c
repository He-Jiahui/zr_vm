//
// ExecBC quickening.
// Name-based access rewrites are intentionally disabled: access semantics must
// remain explicit in emitted instructions and artifacts.
//

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "compiler_internal.h"
#include "zr_vm_library/native_binding.h"

#define ZR_COMPILER_QUICKENING_MEMBER_FLAGS_NONE ((TZrUInt8)0)

typedef struct ZrCompilerQuickeningSlotAlias {
    TZrUInt32 rootSlot;
    TZrBool valid;
} ZrCompilerQuickeningSlotAlias;

typedef enum EZrCompilerQuickeningSlotKind {
    ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN = 0,
    ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT = 1,
    ZR_COMPILER_QUICKENING_SLOT_KIND_UNSIGNED_INT = 2,
    ZR_COMPILER_QUICKENING_SLOT_KIND_BOOL = 3,
    ZR_COMPILER_QUICKENING_SLOT_KIND_FLOAT = 4,
    ZR_COMPILER_QUICKENING_SLOT_KIND_STRING = 5,
    ZR_COMPILER_QUICKENING_SLOT_KIND_ARRAY_INT = 6
} EZrCompilerQuickeningSlotKind;

typedef enum EZrCompilerQuickeningCallableProvenanceKind {
    ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN = 0,
    ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_VM = 1,
    ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_NATIVE = 2
} EZrCompilerQuickeningCallableProvenanceKind;

static TZrBool compiler_quickening_instruction_writes_slot(const TZrInstruction *instruction, TZrUInt32 slot);
static TZrBool compiler_quicken_child_functions(SZrState *state, SZrFunction *function, TZrBool recurseChildren);

static TZrBool compiler_quickening_trace_enabled(void) {
    const TZrChar *flag = getenv("ZR_VM_TRACE_PROJECT_STARTUP");
    return flag != ZR_NULL && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static void compiler_quickening_trace_pass(const TZrChar *phase,
                                           const TZrChar *passName,
                                           const SZrFunction *function) {
    if (!compiler_quickening_trace_enabled() || phase == ZR_NULL || passName == ZR_NULL) {
        return;
    }

    fprintf(stderr,
            "[zr-quickening] %s %s func=%p instructions=%u childCount=%u borrowed=%d\n",
            phase,
            passName,
            (const void *)function,
            function != ZR_NULL ? function->instructionsLength : 0,
            function != ZR_NULL ? function->childFunctionLength : 0,
            function != ZR_NULL ? (int)function->childFunctionGraphIsBorrowed : 0);
}

static TZrBool compiler_quickening_callsite_alias_trace_enabled(void) {
    const TZrChar *flag = getenv("ZR_VM_TRACE_CALLSITE_CACHE_ALIAS");
    return flag != ZR_NULL && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static const TZrChar *compiler_quickening_function_name_text(SZrString *name) {
    if (name == ZR_NULL) {
        return "<anonymous>";
    }

    if (name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(name);
    }

    return ZrCore_String_GetNativeString(name);
}

static void compiler_quickening_trace_callsite_alias_recursive(const SZrFunction *rootFunction,
                                                               const SZrFunction *function,
                                                               const SZrFunctionCallSiteCacheEntry *entries,
                                                               TZrUInt32 *ioMatchCount) {
    if (function == ZR_NULL || entries == ZR_NULL || ioMatchCount == ZR_NULL) {
        return;
    }

    if (function->callSiteCaches == entries) {
        (*ioMatchCount)++;
        fprintf(stderr,
                "[zr-quickening][callsite-alias] root=%p func=%p owner=%p name=%s caches=%p cacheLen=%u borrowed=%d "
                "lines=%u-%u childCount=%u\n",
                (const void *)rootFunction,
                (const void *)function,
                (const void *)function->ownerFunction,
                compiler_quickening_function_name_text(function->functionName),
                (const void *)entries,
                function->callSiteCacheLength,
                (int)function->childFunctionGraphIsBorrowed,
                function->lineInSourceStart,
                function->lineInSourceEnd,
                function->childFunctionLength);
    }

    if (function->childFunctionList == ZR_NULL || function->childFunctionLength == 0) {
        return;
    }

    for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        compiler_quickening_trace_callsite_alias_recursive(rootFunction,
                                                           &function->childFunctionList[childIndex],
                                                           entries,
                                                           ioMatchCount);
    }
}

static void compiler_quickening_trace_callsite_aliases(SZrState *state,
                                                       const SZrFunction *function,
                                                       const SZrFunctionCallSiteCacheEntry *entries) {
    SZrRawObject *object;
    TZrUInt32 matchCount = 0;

    if (!compiler_quickening_callsite_alias_trace_enabled() || state == ZR_NULL || state->global == ZR_NULL ||
        state->global->garbageCollector == ZR_NULL || function == ZR_NULL || entries == ZR_NULL) {
        return;
    }

    fprintf(stderr,
            "[zr-quickening][callsite-alias] realloc func=%p owner=%p name=%s oldCaches=%p oldLen=%u\n",
            (const void *)function,
            (const void *)function->ownerFunction,
            compiler_quickening_function_name_text(function->functionName),
            (const void *)entries,
            function->callSiteCacheLength);

    for (object = state->global->garbageCollector->gcObjectList; object != ZR_NULL; object = object->next) {
        if (object->type != ZR_RAW_OBJECT_TYPE_FUNCTION) {
            continue;
        }

        compiler_quickening_trace_callsite_alias_recursive(ZR_CAST(const SZrFunction *, object),
                                                           ZR_CAST(const SZrFunction *, object),
                                                           entries,
                                                           &matchCount);
    }

    fprintf(stderr,
            "[zr-quickening][callsite-alias] oldCaches=%p matchCount=%u\n",
            (const void *)entries,
            matchCount);
}

#define ZR_QUICKENING_RUN_PASS(label, expr) \
    do { \
        compiler_quickening_trace_pass("begin", label, function); \
        if (!(expr)) { \
            compiler_quickening_trace_pass("fail", label, function); \
            return ZR_FALSE; \
        } \
        compiler_quickening_trace_pass("done", label, function); \
    } while (0)

static TZrBool compiler_quickening_resolve_index_access_int_constant(const SZrFunction *function,
                                                                     const TZrBool *blockStarts,
                                                                     TZrUInt32 instructionIndex,
                                                                     TZrUInt32 slot);
static TZrBool compiler_quickening_function_constant_is_int(const SZrFunction *function,
                                                            TZrUInt32 constantIndex);
static TZrBool compiler_quickening_function_constant_is_plain_primitive(const SZrFunction *function,
                                                                        TZrUInt32 constantIndex);
static const SZrFunctionLocalVariable *compiler_quickening_find_active_local_variable(const SZrFunction *function,
                                                                                       TZrUInt32 stackSlot,
                                                                                       TZrUInt32 instructionIndex);
static const TZrInstruction *compiler_quickening_find_latest_writer_in_range(const SZrFunction *function,
                                                                             TZrUInt32 rangeStart,
                                                                             TZrUInt32 instructionIndex,
                                                                             TZrUInt32 slot,
                                                                             TZrUInt32 *outWriterIndex);
static TZrBool compiler_quickening_slot_has_any_binding(const SZrFunction *function, TZrUInt32 stackSlot);
static TZrBool compiler_quickening_slot_is_int_before_instruction_in_range(const SZrFunction *function,
                                                                           TZrUInt32 rangeStart,
                                                                           TZrUInt32 instructionIndex,
                                                                           TZrUInt32 slot,
                                                                           TZrUInt32 depth);
static TZrBool compiler_quickening_slot_has_only_int_writers_in_range(const SZrFunction *function,
                                                                      TZrUInt32 rangeStart,
                                                                      TZrUInt32 rangeEnd,
                                                                      TZrUInt32 slot,
                                                                      TZrUInt32 depth);
static TZrInstruction *compiler_quickening_find_latest_block_writer(SZrFunction *function,
                                                                    const TZrBool *blockStarts,
                                                                    TZrUInt32 instructionIndex,
                                                                    TZrUInt32 slot,
                                                                    TZrUInt32 *outWriterIndex);
static TZrBool compiler_quickening_slot_is_overwritten_before_read(const SZrFunction *function,
                                                                   const TZrBool *blockStarts,
                                                                   TZrUInt32 instructionIndex,
                                                                   TZrUInt32 slot);
static TZrBool compiler_quickening_slot_is_overwritten_before_any_read_linear(const SZrFunction *function,
                                                                              TZrUInt32 instructionIndex,
                                                                              TZrUInt32 slot);
static TZrBool compiler_quickening_temp_slot_is_dead_until_block_end_with_terminal_exit(
        const SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot);
static TZrBool compiler_quickening_is_control_only_opcode(EZrInstructionCode opcode);
static TZrBool compiler_quickening_block_ends_without_fallthrough(const TZrInstruction *instruction);
static TZrBool compiler_quickening_fold_direct_result_stores(SZrFunction *function);
static TZrBool compiler_quickening_try_fold_super_array_fill_int4_const_loop_compact(SZrFunction *function,
                                                                                      const TZrBool *blockStarts,
                                                                                      TZrUInt32 instructionIndex);
static TZrBool compiler_quickening_try_fold_super_array_fill_int4_const_loop_compact_self_update(
        SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex);
static TZrBool compiler_quickening_instruction_destination_becomes_plain(const SZrFunction *function,
                                                                         const TZrInstruction *instruction,
                                                                         const TZrBool *plainSlots,
                                                                         TZrUInt32 slotCount);
static TZrBool compiler_quickening_promote_plain_destination_opcodes(SZrFunction *function,
                                                                     const TZrBool *blockStarts);
static TZrBool compiler_quickening_can_skip_over_super_array_store_to_load_forward(
        const TZrInstruction *instruction,
        TZrUInt32 receiverSlot,
        TZrUInt32 indexSlot,
        TZrUInt32 valueSlot);
static TZrBool compiler_quickening_forward_super_array_store_to_load_reads(SZrFunction *function);
static TZrBool compiler_quickening_forward_get_stack_copy_reads(SZrFunction *function);
static TZrBool compiler_quickening_fuse_jump_if_greater_signed(SZrFunction *function);
static TZrBool compiler_quickening_fuse_jump_if_not_equal_signed(SZrFunction *function);
static TZrBool compiler_quickening_fuse_jump_if_not_equal_signed_const(SZrFunction *function);
static TZrBool compiler_quicken_member_slot_accesses(SZrState *state, SZrFunction *function);
static TZrBool compiler_quickening_fuse_known_native_member_calls(SZrFunction *function);
static TZrBool compiler_quickening_fuse_known_vm_member_call_load1_u8(SZrFunction *function);
static TZrBool compiler_quickening_rewrite_null_constant_loads(SZrFunction *function);
static TZrBool compiler_quickening_try_fold_direct_result_store(SZrFunction *function,
                                                                const TZrBool *blockStarts,
                                                                TZrUInt32 instructionIndex);
static TZrBool compiler_quickening_slot_is_exported_callable_binding(const SZrFunction *function, TZrUInt32 slot);
static EZrCompilerQuickeningCallableProvenanceKind compiler_quickening_resolve_latest_prior_callable_provenance(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot,
        TZrUInt32 depth);
static EZrCompilerQuickeningCallableProvenanceKind compiler_quickening_resolve_unique_prior_callable_provenance(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot,
        TZrUInt32 depth);
static EZrCompilerQuickeningCallableProvenanceKind compiler_quickening_resolve_callable_provenance_before_instruction(
        SZrState *state,
        const SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot,
        TZrUInt32 depth);
static EZrCompilerQuickeningCallableProvenanceKind compiler_quickening_resolve_typed_slot_callable_provenance_before_instruction(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot,
        TZrUInt32 depth);
static EZrCompilerQuickeningCallableProvenanceKind compiler_quickening_resolve_child_parameter_callable_provenance_from_owner_calls(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 slot,
        TZrUInt32 depth);
static SZrFunction *compiler_quickening_resolve_owner_callable_metadata_from_closure_capture(
        SZrState *state,
        SZrFunction *function,
        TZrUInt32 closureIndex,
        TZrUInt32 depth,
        EZrCompilerQuickeningSlotKind *outSlotKind,
        EZrCompilerQuickeningCallableProvenanceKind *outProvenance);
static SZrFunction *compiler_quickening_resolve_callable_metadata_function_for_slot_before_instruction(
        SZrState *state,
        SZrFunction *function,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot,
        TZrUInt32 depth,
        EZrCompilerQuickeningSlotKind *outSlotKind,
        EZrCompilerQuickeningCallableProvenanceKind *outProvenance);
static SZrFunction *compiler_quickening_resolve_bound_member_callable_metadata_function(
        SZrState *state,
        SZrFunction *function,
        const TZrInstruction *writer,
        TZrUInt32 writerIndex,
        EZrCompilerQuickeningSlotKind *outSlotKind,
        EZrCompilerQuickeningCallableProvenanceKind *outProvenance);
static SZrFunction *compiler_quickening_find_prototype_owner_function(SZrFunction *function);
static TZrBool compiler_quickening_function_matches_inline_child(const SZrFunction *left, const SZrFunction *right);

static const TZrChar *compiler_quickening_type_name_text(SZrString *typeName) {
    if (typeName == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(typeName);
    }

    return ZrCore_String_GetNativeString(typeName);
}

static ZR_FORCE_INLINE TZrBool compiler_quickening_slot_kind_is_integral(EZrCompilerQuickeningSlotKind kind) {
    return kind == ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT ||
           kind == ZR_COMPILER_QUICKENING_SLOT_KIND_UNSIGNED_INT;
}

static ZR_FORCE_INLINE EZrCompilerQuickeningSlotKind compiler_quickening_slot_kind_from_value_type(
        EZrValueType valueType) {
    if (ZR_VALUE_IS_TYPE_BOOL(valueType)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_BOOL;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(valueType)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(valueType)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_UNSIGNED_INT;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(valueType)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_FLOAT;
    }
    if (ZR_VALUE_IS_TYPE_STRING(valueType)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_STRING;
    }
    return ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
}

static ZR_FORCE_INLINE EZrCompilerQuickeningSlotKind compiler_quickening_slot_kind_from_typed_type_ref(
        const SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
    }

    if (typeRef->isArray && ZR_VALUE_IS_TYPE_INT(typeRef->elementBaseType)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_ARRAY_INT;
    }

    return compiler_quickening_slot_kind_from_value_type(typeRef->baseType);
}

static ZR_FORCE_INLINE EZrCompilerQuickeningSlotKind compiler_quickening_slot_kind_from_type_name_text(
        const TZrChar *typeName) {
    if (typeName == ZR_NULL || typeName[0] == '\0') {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
    }

    if (strcmp(typeName, "i8") == 0 || strcmp(typeName, "i16") == 0 || strcmp(typeName, "i32") == 0 ||
        strcmp(typeName, "int") == 0) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT;
    }

    if (strcmp(typeName, "u8") == 0 || strcmp(typeName, "u16") == 0 || strcmp(typeName, "u32") == 0 ||
        strcmp(typeName, "uint") == 0) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_UNSIGNED_INT;
    }

    if (strcmp(typeName, "bool") == 0) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_BOOL;
    }

    if (strcmp(typeName, "float") == 0 || strcmp(typeName, "double") == 0) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_FLOAT;
    }

    if (strcmp(typeName, "string") == 0) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_STRING;
    }

    if (strcmp(typeName, "Array<int>") == 0 || strcmp(typeName, "container.Array<int>") == 0 ||
        strcmp(typeName, "zr.container.Array<int>") == 0) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_ARRAY_INT;
    }

    return ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
}

static ZR_FORCE_INLINE SZrRawObject *compiler_quickening_refresh_forwarded_raw_object(SZrRawObject *rawObject) {
    SZrRawObject *forwardedObject;

    if (rawObject == ZR_NULL) {
        return ZR_NULL;
    }

    forwardedObject = (SZrRawObject *)rawObject->garbageCollectMark.forwardingAddress;
    return forwardedObject != ZR_NULL ? forwardedObject : rawObject;
}

static SZrFunction *compiler_quickening_metadata_function_from_callable_value(
        SZrState *state,
        const SZrTypeValue *callableValue) {
    SZrRawObject *rawObject;

    if (state == ZR_NULL || callableValue == ZR_NULL || callableValue->value.object == ZR_NULL ||
        (callableValue->type != ZR_VALUE_TYPE_FUNCTION && callableValue->type != ZR_VALUE_TYPE_CLOSURE)) {
        return ZR_NULL;
    }

    rawObject = compiler_quickening_refresh_forwarded_raw_object(callableValue->value.object);
    if (rawObject == ZR_NULL) {
        return ZR_NULL;
    }

    if (rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_CAST_FUNCTION(state, rawObject);
    }

    if (rawObject->type != ZR_RAW_OBJECT_TYPE_CLOSURE) {
        return ZR_NULL;
    }

    if (callableValue->isNative) {
        SZrClosureNative *nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, rawObject);
        return nativeClosure != ZR_NULL ? nativeClosure->aotShimFunction : ZR_NULL;
    }

    {
        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, rawObject);
        return closure != ZR_NULL ? closure->function : ZR_NULL;
    }
}

static EZrCompilerQuickeningCallableProvenanceKind compiler_quickening_callable_value_provenance(
        const SZrTypeValue *callableValue) {
    if (callableValue == ZR_NULL) {
        return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    switch (callableValue->type) {
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
            return callableValue->isNative ? ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_NATIVE
                                           : ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_VM;
        case ZR_VALUE_TYPE_NATIVE_POINTER:
            return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_NATIVE;
        default:
            return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }
}

static EZrCompilerQuickeningSlotKind compiler_quickening_slot_kind_from_callable_value(
        SZrState *state,
        const SZrTypeValue *callableValue) {
    SZrFunction *resolvedFunction;
    const TZrChar *nativeReturnTypeName;

    if (state == ZR_NULL || callableValue == ZR_NULL) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
    }

    resolvedFunction = compiler_quickening_metadata_function_from_callable_value(state, callableValue);
    if (resolvedFunction != ZR_NULL && resolvedFunction->hasCallableReturnType) {
        return compiler_quickening_slot_kind_from_typed_type_ref(&resolvedFunction->callableReturnType);
    }

    nativeReturnTypeName = ZrLib_CallableValue_GetNativeBindingReturnTypeName(state, callableValue);
    return compiler_quickening_slot_kind_from_type_name_text(nativeReturnTypeName);
}

static const SZrTypeValue *compiler_quickening_resolve_prototype_member_callable_value(
        SZrState *state,
        SZrObjectPrototype *prototype,
        SZrString *memberName,
        const SZrMemberDescriptor **outDescriptor) {
    SZrTypeValue memberKey;

    if (outDescriptor != ZR_NULL) {
        *outDescriptor = ZR_NULL;
    }

    if (state == ZR_NULL || prototype == ZR_NULL || memberName == ZR_NULL) {
        return ZR_NULL;
    }

    if (outDescriptor != ZR_NULL) {
        *outDescriptor = ZrCore_ObjectPrototype_FindMemberDescriptor(prototype, memberName, ZR_TRUE);
    }

    ZrCore_Value_InitAsRawObject(state, &memberKey, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    memberKey.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, &prototype->super, &memberKey);
}

static SZrObjectPrototype *compiler_quickening_find_owner_runtime_prototype_by_name(
        SZrState *state,
        SZrFunction *function,
        SZrString *typeName) {
    SZrFunction *prototypeOwner;
    const TZrChar *targetName;
    TZrUInt32 prototypeLimit;

    if (state == ZR_NULL || function == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    targetName = compiler_quickening_type_name_text(typeName);
    if (targetName == ZR_NULL || targetName[0] == '\0') {
        return ZR_NULL;
    }

    prototypeOwner = compiler_quickening_find_prototype_owner_function(function);
    if (prototypeOwner == ZR_NULL || prototypeOwner->prototypeCount == 0) {
        return ZR_NULL;
    }

    if (prototypeOwner->prototypeInstances == ZR_NULL ||
        prototypeOwner->prototypeInstancesLength < prototypeOwner->prototypeCount) {
        ZrCore_Module_CreatePrototypesFromData(state, ZR_NULL, prototypeOwner);
    }

    if (prototypeOwner->prototypeInstances == ZR_NULL || prototypeOwner->prototypeInstancesLength == 0) {
        return ZR_NULL;
    }

    prototypeLimit = prototypeOwner->prototypeInstancesLength;
    if (prototypeLimit > prototypeOwner->prototypeCount) {
        prototypeLimit = prototypeOwner->prototypeCount;
    }

    for (TZrUInt32 prototypeIndex = 0; prototypeIndex < prototypeLimit; prototypeIndex++) {
        SZrObjectPrototype *prototype = prototypeOwner->prototypeInstances[prototypeIndex];
        const TZrChar *prototypeName;

        if (prototype == ZR_NULL || prototype->name == ZR_NULL) {
            continue;
        }

        if (prototype->name == typeName) {
            return prototype;
        }

        prototypeName = compiler_quickening_type_name_text(prototype->name);
        if (prototypeName != ZR_NULL && strcmp(targetName, prototypeName) == 0) {
            return prototype;
        }
    }

    return ZR_NULL;
}

static SZrObjectPrototype *compiler_quickening_resolve_type_ref_runtime_prototype(
        SZrState *state,
        SZrFunction *function,
        const SZrFunctionTypedTypeRef *typeRef) {
    const TZrChar *typeNameText;
    SZrObjectPrototype *prototype;

    if (state == ZR_NULL || typeRef == ZR_NULL || typeRef->typeName == ZR_NULL) {
        return ZR_NULL;
    }

    typeNameText = compiler_quickening_type_name_text(typeRef->typeName);
    if (typeNameText == ZR_NULL || typeNameText[0] == '\0') {
        return ZR_NULL;
    }

    if (typeRef->baseType == ZR_VALUE_TYPE_ARRAY) {
        prototype = ZrLib_Type_FindPrototype(state, "Array");
        if (prototype != ZR_NULL) {
            return prototype;
        }

        return ZrLib_Type_FindPrototype(state, "zr.container.Array");
    }

    prototype = compiler_quickening_find_owner_runtime_prototype_by_name(state, function, typeRef->typeName);
    if (prototype != ZR_NULL) {
        return prototype;
    }

    return ZrLib_Type_FindPrototype(state, typeNameText);
}

static EZrCompilerQuickeningCallableProvenanceKind compiler_quickening_resolve_prototype_meta_call_provenance(
        const SZrObjectPrototype *prototype,
        EZrCompilerQuickeningSlotKind *outSlotKind) {
    const SZrMeta *meta;

    if (outSlotKind != ZR_NULL) {
        *outSlotKind = ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
    }

    if (prototype == ZR_NULL) {
        return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    meta = prototype->metaTable.metas[ZR_META_CALL];
    if (meta == ZR_NULL || meta->function == ZR_NULL) {
        return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    if (outSlotKind != ZR_NULL && meta->function->hasCallableReturnType) {
        *outSlotKind = compiler_quickening_slot_kind_from_typed_type_ref(&meta->function->callableReturnType);
    }

    return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_VM;
}

static SZrFunction *compiler_quickening_find_prototype_owner_function(SZrFunction *function) {
    SZrFunction *current = function;

    while (current != ZR_NULL) {
        if (current->prototypeData != ZR_NULL && current->prototypeCount > 0) {
            return current;
        }
        current = current->ownerFunction;
    }

    return function;
}

static SZrFunction *compiler_quickening_find_owner_child_function_by_name(SZrFunction *ownerFunction, SZrString *name) {
    const TZrChar *targetName;

    if (ownerFunction == ZR_NULL || name == ZR_NULL || ownerFunction->childFunctionList == ZR_NULL) {
        return ZR_NULL;
    }

    targetName = compiler_quickening_type_name_text(name);
    for (TZrUInt32 childIndex = 0; childIndex < ownerFunction->childFunctionLength; childIndex++) {
        SZrFunction *childFunction = &ownerFunction->childFunctionList[childIndex];
        const TZrChar *childName = compiler_quickening_type_name_text(childFunction->functionName);

        if (childFunction->functionName == name) {
            return childFunction;
        }

        if (targetName != ZR_NULL && childName != ZR_NULL && strcmp(targetName, childName) == 0) {
            return childFunction;
        }
    }

    return ZR_NULL;
}

static SZrFunction *compiler_quickening_find_owner_child_function_by_stack_slot(SZrFunction *ownerFunction,
                                                                                 TZrUInt32 stackSlot) {
    if (ownerFunction == ZR_NULL || ownerFunction->childFunctionList == ZR_NULL) {
        return ZR_NULL;
    }

    if (ownerFunction->topLevelCallableBindings != ZR_NULL) {
        for (TZrUInt32 index = 0; index < ownerFunction->topLevelCallableBindingLength; index++) {
            const SZrFunctionTopLevelCallableBinding *binding = &ownerFunction->topLevelCallableBindings[index];
            if (binding->stackSlot == stackSlot && binding->callableChildIndex < ownerFunction->childFunctionLength) {
                return &ownerFunction->childFunctionList[binding->callableChildIndex];
            }
        }
    }

    if (ownerFunction->exportedVariables != ZR_NULL) {
        for (TZrUInt32 index = 0; index < ownerFunction->exportedVariableLength; index++) {
            const SZrFunctionExportedVariable *binding = &ownerFunction->exportedVariables[index];
            if (binding->stackSlot == stackSlot && binding->callableChildIndex < ownerFunction->childFunctionLength) {
                return &ownerFunction->childFunctionList[binding->callableChildIndex];
            }
        }
    }

    return ZR_NULL;
}

static SZrFunction *compiler_quickening_resolve_owner_callable_metadata_from_closure_capture(
        SZrState *state,
        SZrFunction *function,
        TZrUInt32 closureIndex,
        TZrUInt32 depth,
        EZrCompilerQuickeningSlotKind *outSlotKind,
        EZrCompilerQuickeningCallableProvenanceKind *outProvenance) {
    SZrFunction *ownerFunction;
    const SZrFunctionClosureVariable *closure;
    SZrFunction *resolvedFunction;

    if (outSlotKind != ZR_NULL) {
        *outSlotKind = ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
    }
    if (outProvenance != ZR_NULL) {
        *outProvenance = ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    if (function == ZR_NULL || function->closureValueList == ZR_NULL || closureIndex >= function->closureValueLength ||
        depth > function->instructionsLength) {
        return ZR_NULL;
    }

    ownerFunction = function->ownerFunction;
    if (ownerFunction == ZR_NULL) {
        return ZR_NULL;
    }

    closure = &function->closureValueList[closureIndex];

    resolvedFunction = compiler_quickening_find_owner_child_function_by_name(ownerFunction, closure->name);
    if (resolvedFunction != ZR_NULL) {
        if (outSlotKind != ZR_NULL && resolvedFunction->hasCallableReturnType) {
            *outSlotKind = compiler_quickening_slot_kind_from_typed_type_ref(&resolvedFunction->callableReturnType);
        }
        if (outProvenance != ZR_NULL) {
            *outProvenance = ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_VM;
        }
        return resolvedFunction;
    }

    if (closure->inStack) {
        resolvedFunction = compiler_quickening_find_owner_child_function_by_stack_slot(ownerFunction, closure->index);
        if (resolvedFunction != ZR_NULL) {
            if (outSlotKind != ZR_NULL && resolvedFunction->hasCallableReturnType) {
                *outSlotKind = compiler_quickening_slot_kind_from_typed_type_ref(&resolvedFunction->callableReturnType);
            }
            if (outProvenance != ZR_NULL) {
                *outProvenance = ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_VM;
            }
            return resolvedFunction;
        }

        return compiler_quickening_resolve_callable_metadata_function_for_slot_before_instruction(state,
                                                                                                  ownerFunction,
                                                                                                  ownerFunction->instructionsLength,
                                                                                                  closure->index,
                                                                                                  depth + 1u,
                                                                                                  outSlotKind,
                                                                                                  outProvenance);
    }

    if (ownerFunction->closureValueList != ZR_NULL && closure->index < ownerFunction->closureValueLength) {
        return compiler_quickening_resolve_owner_callable_metadata_from_closure_capture(state,
                                                                                        ownerFunction,
                                                                                        closure->index,
                                                                                        depth + 1u,
                                                                                        outSlotKind,
                                                                                        outProvenance);
    }

    return ZR_NULL;
}

static const SZrFunctionTypedLocalBinding *compiler_quickening_find_typed_local_binding(const SZrFunction *function,
                                                                                         TZrUInt32 stackSlot) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        if (binding->stackSlot == stackSlot) {
            return binding;
        }
    }

    return ZR_NULL;
}

static TZrBool compiler_quickening_slot_has_any_binding(const SZrFunction *function, TZrUInt32 stackSlot) {
    TZrUInt32 index;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->typedLocalBindings != ZR_NULL) {
        for (index = 0; index < function->typedLocalBindingLength; index++) {
            if (function->typedLocalBindings[index].stackSlot == stackSlot) {
                return ZR_TRUE;
            }
        }
    }

    if (function->localVariableList != ZR_NULL) {
        for (index = 0; index < function->localVariableLength; index++) {
            if (function->localVariableList[index].stackSlot == stackSlot) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool compiler_quickening_local_variable_is_active_at_instruction(
        const SZrFunctionLocalVariable *localVariable,
        TZrUInt32 instructionIndex) {
    if (localVariable == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrUInt32)localVariable->offsetActivate <= instructionIndex &&
           instructionIndex < (TZrUInt32)localVariable->offsetDead;
}

static const SZrFunctionLocalVariable *compiler_quickening_find_active_local_variable(const SZrFunction *function,
                                                                                       TZrUInt32 stackSlot,
                                                                                       TZrUInt32 instructionIndex) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->localVariableList == ZR_NULL || function->localVariableLength == 0) {
        return ZR_NULL;
    }

    for (index = 0; index < function->localVariableLength; index++) {
        const SZrFunctionLocalVariable *localVariable = &function->localVariableList[index];
        if (localVariable->stackSlot == stackSlot &&
            compiler_quickening_local_variable_is_active_at_instruction(localVariable, instructionIndex)) {
            return localVariable;
        }
    }

    return ZR_NULL;
}

static const SZrFunctionTypedLocalBinding *compiler_quickening_find_active_typed_local_binding(
        const SZrFunction *function,
        TZrUInt32 stackSlot,
        TZrUInt32 instructionIndex) {
    const SZrFunctionLocalVariable *activeLocal = ZR_NULL;
    TZrUInt32 index;

    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }

    if (function->localVariableList == ZR_NULL || function->localVariableLength == 0) {
        return compiler_quickening_find_typed_local_binding(function, stackSlot);
    }

    for (index = 0; index < function->localVariableLength; index++) {
        const SZrFunctionLocalVariable *localVariable = &function->localVariableList[index];
        if (localVariable->stackSlot != stackSlot ||
            !compiler_quickening_local_variable_is_active_at_instruction(localVariable, instructionIndex)) {
            continue;
        }

        activeLocal = localVariable;
        if (index < function->typedLocalBindingLength) {
            const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
            if (binding->stackSlot == stackSlot &&
                ((binding->name == localVariable->name) ||
                 (binding->name != ZR_NULL && localVariable->name != ZR_NULL &&
                  ZrCore_String_Equal(binding->name, localVariable->name)))) {
                return binding;
            }
        }
        break;
    }

    if (activeLocal == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        if (binding->stackSlot == stackSlot &&
            ((binding->name == activeLocal->name) ||
             (binding->name != ZR_NULL && activeLocal->name != ZR_NULL &&
              ZrCore_String_Equal(binding->name, activeLocal->name)))) {
            return binding;
        }
    }

    return ZR_NULL;
}

static void compiler_quickening_init_unknown_type_ref(SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return;
    }

    memset(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
}

static TZrBool compiler_quickening_type_name_is_array_like(const TZrChar *typeNameText) {
    if (typeNameText == ZR_NULL || typeNameText[0] == '\0') {
        return ZR_FALSE;
    }

    return (TZrBool)(strcmp(typeNameText, "Array") == 0 ||
                     strncmp(typeNameText, "Array<", 6) == 0 ||
                     strcmp(typeNameText, "container.Array") == 0 ||
                     strncmp(typeNameText, "container.Array<", 16) == 0 ||
                     strcmp(typeNameText, "zr.container.Array") == 0 ||
                     strncmp(typeNameText, "zr.container.Array<", 19) == 0);
}

static void compiler_quickening_populate_type_ref_from_type_name(SZrFunctionTypedTypeRef *typeRef,
                                                                 SZrString *typeName) {
    const TZrChar *typeNameText;

    compiler_quickening_init_unknown_type_ref(typeRef);
    if (typeRef == ZR_NULL || typeName == ZR_NULL) {
        return;
    }

    typeRef->typeName = typeName;
    typeNameText = compiler_quickening_type_name_text(typeName);
    if (typeNameText == ZR_NULL || typeNameText[0] == '\0') {
        return;
    }

    if (compiler_quickening_type_name_is_array_like(typeNameText)) {
        typeRef->baseType = ZR_VALUE_TYPE_ARRAY;
        typeRef->isArray = ZR_TRUE;
        if (compiler_quickening_slot_kind_from_type_name_text(typeNameText) ==
            ZR_COMPILER_QUICKENING_SLOT_KIND_ARRAY_INT) {
            typeRef->elementBaseType = ZR_VALUE_TYPE_INT64;
        }
        return;
    }

    typeRef->baseType = ZR_VALUE_TYPE_OBJECT;
}

static TZrBool compiler_quickening_function_constant_read_string(const SZrFunction *function,
                                                                 TZrUInt32 constantIndex,
                                                                 SZrString **outString) {
    const SZrTypeValue *constantValue;

    if (outString != ZR_NULL) {
        *outString = ZR_NULL;
    }
    if (function == ZR_NULL || function->constantValueList == ZR_NULL || outString == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    constantValue = &function->constantValueList[constantIndex];
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_STRING(constantValue->type) || constantValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    *outString = ZR_CAST(SZrString *, constantValue->value.object);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_resolve_type_ref_from_type_name_constant(
        const SZrFunction *function,
        TZrUInt32 constantIndex,
        SZrFunctionTypedTypeRef *outTypeRef) {
    SZrString *typeName = ZR_NULL;

    compiler_quickening_init_unknown_type_ref(outTypeRef);
    if (outTypeRef == ZR_NULL ||
        !compiler_quickening_function_constant_read_string(function, constantIndex, &typeName)) {
        return ZR_FALSE;
    }

    compiler_quickening_populate_type_ref_from_type_name(outTypeRef, typeName);
    return outTypeRef->typeName != ZR_NULL;
}

static TZrBool compiler_quickening_resolve_slot_type_ref_before_instruction_in_range(
        const SZrFunction *function,
        TZrUInt32 rangeStart,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot,
        TZrUInt32 depth,
        SZrFunctionTypedTypeRef *outTypeRef) {
    const SZrFunctionTypedLocalBinding *binding;
    const TZrInstruction *writer;
    TZrUInt32 writerIndex = UINT32_MAX;

    compiler_quickening_init_unknown_type_ref(outTypeRef);
    if (function == ZR_NULL || function->instructionsList == ZR_NULL || outTypeRef == ZR_NULL ||
        depth > function->stackSize + 8u) {
        return ZR_FALSE;
    }

    binding = compiler_quickening_find_active_typed_local_binding(function, slot, instructionIndex);
    if (binding == ZR_NULL &&
        (function->localVariableList == ZR_NULL || function->localVariableLength == 0)) {
        binding = compiler_quickening_find_typed_local_binding(function, slot);
    }
    if (binding != ZR_NULL && binding->type.typeName != ZR_NULL) {
        *outTypeRef = binding->type;
        return ZR_TRUE;
    }

    writer = compiler_quickening_find_latest_writer_in_range(function,
                                                             rangeStart,
                                                             instructionIndex,
                                                             slot,
                                                             &writerIndex);
    if (writer == ZR_NULL) {
        return ZR_FALSE;
    }

    switch ((EZrInstructionCode)writer->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK): {
            TZrUInt32 sourceSlot = (TZrUInt32)writer->instruction.operand.operand2[0];
            if (sourceSlot == slot) {
                return ZR_FALSE;
            }
            return compiler_quickening_resolve_slot_type_ref_before_instruction_in_range(function,
                                                                                          rangeStart,
                                                                                          writerIndex,
                                                                                          sourceSlot,
                                                                                          depth + 1u,
                                                                                          outTypeRef);
        }
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
        case ZR_INSTRUCTION_ENUM(TO_STRUCT):
            return compiler_quickening_resolve_type_ref_from_type_name_constant(
                    function,
                    (TZrUInt32)writer->instruction.operand.operand1[1],
                    outTypeRef);
        default:
            return ZR_FALSE;
    }
}

static EZrCompilerQuickeningCallableProvenanceKind compiler_quickening_resolve_typed_slot_callable_provenance_before_instruction(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot,
        TZrUInt32 depth) {
    SZrFunctionTypedTypeRef recoveredTypeRef;
    const SZrFunctionTypedLocalBinding *binding;
    const SZrFunctionTypedTypeRef *typeRef = ZR_NULL;
    SZrObjectPrototype *prototype;

    if (state == ZR_NULL || function == ZR_NULL || depth > function->stackSize + 8u) {
        return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    binding = compiler_quickening_find_active_typed_local_binding(function, slot, instructionIndex);
    if (binding == ZR_NULL &&
        (function->localVariableList == ZR_NULL || function->localVariableLength == 0)) {
        binding = compiler_quickening_find_typed_local_binding(function, slot);
    }
    if (binding != ZR_NULL && binding->type.typeName != ZR_NULL) {
        typeRef = &binding->type;
    } else if (compiler_quickening_resolve_slot_type_ref_before_instruction_in_range(function,
                                                                                      0,
                                                                                      instructionIndex,
                                                                                      slot,
                                                                                      depth + 1u,
                                                                                      &recoveredTypeRef)) {
        typeRef = &recoveredTypeRef;
    }

    prototype = compiler_quickening_resolve_type_ref_runtime_prototype(
            state,
            ZR_CAST(SZrFunction *, function),
            typeRef);
    return compiler_quickening_resolve_prototype_meta_call_provenance(prototype, ZR_NULL);
}

static TZrBool compiler_quickening_binding_is_int(const SZrFunctionTypedLocalBinding *binding) {
    const TZrChar *typeName;

    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_INT(binding->type.baseType)) {
        return ZR_TRUE;
    }

    typeName = compiler_quickening_type_name_text(binding->type.typeName);
    return typeName != ZR_NULL && strcmp(typeName, "int") == 0;
}

static TZrBool compiler_quickening_binding_is_signed_int(const SZrFunctionTypedLocalBinding *binding) {
    const TZrChar *typeName;

    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(binding->type.baseType)) {
        return ZR_TRUE;
    }

    typeName = compiler_quickening_type_name_text(binding->type.typeName);
    return typeName != ZR_NULL && strcmp(typeName, "int") == 0;
}

static TZrBool compiler_quickening_binding_is_unsigned_int(const SZrFunctionTypedLocalBinding *binding) {
    const TZrChar *typeName;

    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(binding->type.baseType)) {
        return ZR_TRUE;
    }

    typeName = compiler_quickening_type_name_text(binding->type.typeName);
    return typeName != ZR_NULL && strcmp(typeName, "uint") == 0;
}

static TZrBool compiler_quickening_binding_is_bool(const SZrFunctionTypedLocalBinding *binding) {
    const TZrChar *typeName;

    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_BOOL(binding->type.baseType)) {
        return ZR_TRUE;
    }

    typeName = compiler_quickening_type_name_text(binding->type.typeName);
    return typeName != ZR_NULL && strcmp(typeName, "bool") == 0;
}

static TZrBool compiler_quickening_binding_is_float(const SZrFunctionTypedLocalBinding *binding) {
    const TZrChar *typeName;

    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(binding->type.baseType)) {
        return ZR_TRUE;
    }

    typeName = compiler_quickening_type_name_text(binding->type.typeName);
    return typeName != ZR_NULL && (strcmp(typeName, "float") == 0 || strcmp(typeName, "double") == 0);
}

static TZrBool compiler_quickening_binding_is_string(const SZrFunctionTypedLocalBinding *binding) {
    const TZrChar *typeName;

    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_STRING(binding->type.baseType)) {
        return ZR_TRUE;
    }

    typeName = compiler_quickening_type_name_text(binding->type.typeName);
    return typeName != ZR_NULL && strcmp(typeName, "string") == 0;
}

static TZrBool compiler_quickening_opcode_produces_known_int(const SZrFunction *function,
                                                             const TZrInstruction *instruction) {
    EZrInstructionCode opcode;

    if (function == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            return compiler_quickening_function_constant_is_int(function,
                                                                (TZrUInt32)instruction->instruction.operand.operand2[0]);
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_quickening_slot_has_only_int_writers_in_range(const SZrFunction *function,
                                                                      TZrUInt32 rangeStart,
                                                                      TZrUInt32 rangeEnd,
                                                                      TZrUInt32 slot,
                                                                      TZrUInt32 depth) {
    TZrBool foundWriter = ZR_FALSE;
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || depth > function->stackSize + 4u) {
        return ZR_FALSE;
    }

    if (rangeStart >= function->instructionsLength) {
        return ZR_FALSE;
    }
    if (rangeEnd > function->instructionsLength) {
        rangeEnd = function->instructionsLength;
    }
    if (rangeStart >= rangeEnd) {
        return ZR_FALSE;
    }

    for (index = rangeStart; index < rangeEnd; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        TZrUInt32 sourceSlot;

        if (instruction->instruction.operandExtra != slot || opcode == ZR_INSTRUCTION_ENUM(NOP)) {
            continue;
        }

        foundWriter = ZR_TRUE;
        if (compiler_quickening_opcode_produces_known_int(function, instruction)) {
            continue;
        }

        if (opcode != ZR_INSTRUCTION_ENUM(GET_STACK) && opcode != ZR_INSTRUCTION_ENUM(SET_STACK)) {
            return ZR_FALSE;
        }

        sourceSlot = (TZrUInt32)instruction->instruction.operand.operand2[0];
        if (sourceSlot == slot) {
            return ZR_FALSE;
        }

        if (!compiler_quickening_slot_is_int_before_instruction_in_range(function,
                                                                         rangeStart,
                                                                         index,
                                                                         sourceSlot,
                                                                         depth + 1u)) {
            return ZR_FALSE;
        }
    }

    return foundWriter;
}

static const TZrInstruction *compiler_quickening_find_latest_writer_in_range(const SZrFunction *function,
                                                                             TZrUInt32 rangeStart,
                                                                             TZrUInt32 instructionIndex,
                                                                             TZrUInt32 slot,
                                                                             TZrUInt32 *outWriterIndex) {
    if (outWriterIndex != ZR_NULL) {
        *outWriterIndex = UINT32_MAX;
    }
    if (function == ZR_NULL || function->instructionsList == ZR_NULL || instructionIndex == 0 ||
        rangeStart >= function->instructionsLength) {
        return ZR_NULL;
    }

    if (instructionIndex > function->instructionsLength) {
        instructionIndex = function->instructionsLength;
    }

    for (TZrUInt32 scan = instructionIndex; scan > rangeStart; scan--) {
        TZrUInt32 candidate = scan - 1u;
        const TZrInstruction *writer = &function->instructionsList[candidate];
        if (writer->instruction.operandExtra != slot ||
            (EZrInstructionCode)writer->instruction.operationCode == ZR_INSTRUCTION_ENUM(NOP)) {
            continue;
        }

        if (outWriterIndex != ZR_NULL) {
            *outWriterIndex = candidate;
        }
        return writer;
    }

    return ZR_NULL;
}

static TZrBool compiler_quickening_slot_is_int_before_instruction_in_range(const SZrFunction *function,
                                                                           TZrUInt32 rangeStart,
                                                                           TZrUInt32 instructionIndex,
                                                                           TZrUInt32 slot,
                                                                           TZrUInt32 depth) {
    const SZrFunctionTypedLocalBinding *binding;
    const TZrInstruction *writer;
    TZrUInt32 writerIndex = UINT32_MAX;
    EZrInstructionCode opcode;
    TZrUInt32 sourceSlot;

    if (function == ZR_NULL || depth > function->stackSize + 4u) {
        return ZR_FALSE;
    }

    binding = compiler_quickening_find_active_typed_local_binding(function, slot, instructionIndex);
    if (compiler_quickening_binding_is_int(binding)) {
        return ZR_TRUE;
    }

    writer = compiler_quickening_find_latest_writer_in_range(function,
                                                             rangeStart,
                                                             instructionIndex,
                                                             slot,
                                                             &writerIndex);
    if (writer == ZR_NULL) {
        return ZR_FALSE;
    }

    if (compiler_quickening_opcode_produces_known_int(function, writer)) {
        return ZR_TRUE;
    }

    opcode = (EZrInstructionCode)writer->instruction.operationCode;
    if (opcode != ZR_INSTRUCTION_ENUM(GET_STACK) && opcode != ZR_INSTRUCTION_ENUM(SET_STACK)) {
        return ZR_FALSE;
    }

    sourceSlot = (TZrUInt32)writer->instruction.operand.operand2[0];
    if (sourceSlot == slot) {
        return ZR_FALSE;
    }

    return compiler_quickening_slot_is_int_before_instruction_in_range(function,
                                                                       rangeStart,
                                                                       writerIndex,
                                                                       sourceSlot,
                                                                       depth + 1u);
}

static TZrBool compiler_quickening_binding_is_array_int(const SZrFunctionTypedLocalBinding *binding) {
    const TZrChar *typeName;
    const TZrChar *elementTypeName;

    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    typeName = compiler_quickening_type_name_text(binding->type.typeName);
    if (typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    elementTypeName = compiler_quickening_type_name_text(binding->type.elementTypeName);
    if ((strcmp(typeName, "Array") == 0 || strcmp(typeName, "container.Array") == 0 ||
         strcmp(typeName, "zr.container.Array") == 0) &&
        (ZR_VALUE_IS_TYPE_INT(binding->type.elementBaseType) ||
         (elementTypeName != ZR_NULL && strcmp(elementTypeName, "int") == 0))) {
        return ZR_TRUE;
    }

    return strcmp(typeName, "Array<int>") == 0 ||
           strcmp(typeName, "container.Array<int>") == 0 ||
           strcmp(typeName, "zr.container.Array<int>") == 0;
}

static EZrCompilerQuickeningSlotKind compiler_quickening_slot_kind_from_binding(
        const SZrFunctionTypedLocalBinding *binding) {
    if (compiler_quickening_binding_is_array_int(binding)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_ARRAY_INT;
    }
    if (compiler_quickening_binding_is_signed_int(binding)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT;
    }
    if (compiler_quickening_binding_is_unsigned_int(binding)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_UNSIGNED_INT;
    }
    if (compiler_quickening_binding_is_bool(binding)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_BOOL;
    }
    if (compiler_quickening_binding_is_float(binding)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_FLOAT;
    }
    if (compiler_quickening_binding_is_string(binding)) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_STRING;
    }
    return ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
}

static void compiler_quickening_clear_aliases(ZrCompilerQuickeningSlotAlias *aliases, TZrUInt32 aliasCount) {
    if (aliases != ZR_NULL && aliasCount > 0) {
        memset(aliases, 0, sizeof(*aliases) * aliasCount);
    }
}

static void compiler_quickening_clear_slot_kinds(EZrCompilerQuickeningSlotKind *slotKinds, TZrUInt32 slotCount) {
    if (slotKinds != ZR_NULL && slotCount > 0) {
        memset(slotKinds, 0, sizeof(*slotKinds) * slotCount);
    }
}

static void compiler_quickening_clear_alias(ZrCompilerQuickeningSlotAlias *aliases,
                                            TZrUInt32 aliasCount,
                                            TZrUInt32 slot) {
    if (aliases != ZR_NULL && slot < aliasCount) {
        aliases[slot].valid = ZR_FALSE;
        aliases[slot].rootSlot = 0;
    }
}

static TZrBool compiler_quickening_opcode_uses_call_argument_slots(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrUInt32 compiler_quickening_call_argument_count(const SZrFunction *function,
                                                         const TZrInstruction *instruction) {
    EZrInstructionCode opcode;
    TZrUInt32 cacheIndex;

    if (function == ZR_NULL || instruction == ZR_NULL) {
        return 0;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
            return (TZrUInt32)instruction->instruction.operand.operand1[1];
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL):
            cacheIndex = (TZrUInt32)instruction->instruction.operand.operand1[0];
            if (function->callSiteCaches != ZR_NULL && cacheIndex < function->callSiteCacheLength) {
                return function->callSiteCaches[cacheIndex].argumentCount;
            }
            return 0;
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL):
            return (TZrUInt32)instruction->instruction.operand.operand1[1];
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            cacheIndex = (TZrUInt32)instruction->instruction.operand.operand1[1];
            if (function->callSiteCaches != ZR_NULL && cacheIndex < function->callSiteCacheLength) {
                return function->callSiteCaches[cacheIndex].argumentCount;
            }
            return 0;
        default:
            return 0;
    }
}

static void compiler_quickening_clear_slot_tracking_range(ZrCompilerQuickeningSlotAlias *aliases,
                                                          EZrCompilerQuickeningSlotKind *slotKinds,
                                                          TZrBool *constantSlotsValid,
                                                          TZrUInt32 *constantSlotIndices,
                                                          TZrUInt32 aliasCount,
                                                          TZrUInt32 firstSlot,
                                                          TZrUInt32 slotCount) {
    TZrUInt32 slotLimit;
    TZrUInt32 slot;

    if (slotCount == 0 || firstSlot >= aliasCount) {
        return;
    }

    slotLimit = firstSlot + slotCount;
    if (slotLimit < firstSlot || slotLimit > aliasCount) {
        slotLimit = aliasCount;
    }

    for (slot = firstSlot; slot < slotLimit; slot++) {
        compiler_quickening_clear_alias(aliases, aliasCount, slot);
        if (slotKinds != ZR_NULL) {
            slotKinds[slot] = ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
        }
        if (constantSlotsValid != ZR_NULL) {
            constantSlotsValid[slot] = ZR_FALSE;
        }
        if (constantSlotIndices != ZR_NULL) {
            constantSlotIndices[slot] = 0;
        }
    }
}

static void compiler_quickening_clear_call_argument_tracking(const SZrFunction *function,
                                                             const TZrInstruction *instruction,
                                                             ZrCompilerQuickeningSlotAlias *aliases,
                                                             EZrCompilerQuickeningSlotKind *slotKinds,
                                                             TZrBool *constantSlotsValid,
                                                             TZrUInt32 *constantSlotIndices,
                                                             TZrUInt32 aliasCount) {
    TZrUInt32 argumentCount;
    TZrUInt32 destinationSlot;

    if (function == ZR_NULL || instruction == ZR_NULL || aliasCount == 0) {
        return;
    }

    if (!compiler_quickening_opcode_uses_call_argument_slots(
                (EZrInstructionCode)instruction->instruction.operationCode)) {
        return;
    }

    argumentCount = compiler_quickening_call_argument_count(function, instruction);
    if (argumentCount == 0) {
        return;
    }

    destinationSlot = instruction->instruction.operandExtra;
    if (destinationSlot >= aliasCount - 1) {
        return;
    }

    compiler_quickening_clear_slot_tracking_range(aliases,
                                                  slotKinds,
                                                  constantSlotsValid,
                                                  constantSlotIndices,
                                                  aliasCount,
                                                  destinationSlot + 1u,
                                                  argumentCount);
}

static void compiler_quickening_clear_temp_only_plain_tracking_after_call(const TZrInstruction *instruction,
                                                                          TZrBool *plainState,
                                                                          const TZrBool *tempOnlySlots,
                                                                          TZrUInt32 slotCount) {
    TZrUInt32 destinationSlot;
    TZrUInt32 slot;

    if (instruction == ZR_NULL || plainState == ZR_NULL || tempOnlySlots == ZR_NULL || slotCount == 0) {
        return;
    }

    if (!compiler_quickening_opcode_uses_call_argument_slots(
                (EZrInstructionCode)instruction->instruction.operationCode)) {
        return;
    }

    destinationSlot = instruction->instruction.operandExtra;
    if (destinationSlot >= slotCount) {
        return;
    }

    for (slot = destinationSlot; slot < slotCount; slot++) {
        if (tempOnlySlots[slot]) {
            plainState[slot] = ZR_FALSE;
        }
    }
}

static TZrBool compiler_quickening_resolve_alias_slot(const SZrFunction *function,
                                                      const ZrCompilerQuickeningSlotAlias *aliases,
                                                      TZrUInt32 aliasCount,
                                                      TZrUInt32 instructionIndex,
                                                      TZrUInt32 slot,
                                                      TZrUInt32 *outRootSlot) {
    if (outRootSlot != ZR_NULL) {
        *outRootSlot = 0;
    }

    if (function == ZR_NULL || outRootSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    if (compiler_quickening_find_active_typed_local_binding(function, slot, instructionIndex) != ZR_NULL) {
        *outRootSlot = slot;
        return ZR_TRUE;
    }

    if (aliases != ZR_NULL && slot < aliasCount && aliases[slot].valid) {
        *outRootSlot = aliases[slot].rootSlot;
        return ZR_TRUE;
    }

    if ((function->localVariableList == ZR_NULL || function->localVariableLength == 0) &&
        compiler_quickening_find_typed_local_binding(function, slot) != ZR_NULL) {
        *outRootSlot = slot;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool compiler_quickening_function_constant_is_int(const SZrFunction *function,
                                                            TZrUInt32 constantIndex) {
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL || function->constantValueList == ZR_NULL || constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    constantValue = &function->constantValueList[constantIndex];
    return constantValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(constantValue->type);
}

static EZrCompilerQuickeningSlotKind compiler_quickening_function_constant_slot_kind(const SZrFunction *function,
                                                                                      TZrUInt32 constantIndex) {
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL || function->constantValueList == ZR_NULL || constantIndex >= function->constantValueLength) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
    }

    constantValue = &function->constantValueList[constantIndex];
    if (constantValue == ZR_NULL) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
    }

    return compiler_quickening_slot_kind_from_value_type(constantValue->type);
}

static TZrBool compiler_quickening_function_constant_is_plain_primitive(const SZrFunction *function,
                                                                        TZrUInt32 constantIndex) {
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL || function->constantValueList == ZR_NULL || constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    constantValue = &function->constantValueList[constantIndex];
    return constantValue != ZR_NULL &&
           !constantValue->isGarbageCollectable &&
           constantValue->ownershipKind == ZR_OWNERSHIP_VALUE_KIND_NONE &&
           constantValue->ownershipControl == ZR_NULL &&
           constantValue->ownershipWeakRef == ZR_NULL;
}

static EZrCompilerQuickeningCallableProvenanceKind compiler_quickening_function_constant_callable_kind(
        const SZrFunction *function,
        TZrUInt32 constantIndex) {
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL || function->constantValueList == ZR_NULL || constantIndex >= function->constantValueLength) {
        return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    constantValue = &function->constantValueList[constantIndex];
    if (constantValue == ZR_NULL) {
        return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    switch (constantValue->type) {
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
            return constantValue->isNative ? ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_NATIVE
                                           : ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_VM;
        case ZR_VALUE_TYPE_NATIVE_POINTER:
            return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_NATIVE;
        default:
            return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }
}

static TZrBool compiler_quickening_slot_is_exported_callable_binding(const SZrFunction *function, TZrUInt32 slot) {
    if (function == ZR_NULL || function->exportedVariables == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->exportedVariableLength; index++) {
        const SZrFunctionExportedVariable *exported = &function->exportedVariables[index];
        if (exported->stackSlot == slot &&
            exported->callableChildIndex != ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE &&
            exported->exportKind == ZR_MODULE_EXPORT_KIND_FUNCTION) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static EZrCompilerQuickeningCallableProvenanceKind compiler_quickening_resolve_latest_prior_callable_provenance(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot,
        TZrUInt32 depth) {
    const TZrInstruction *writer = ZR_NULL;
    TZrUInt32 writerIndex = UINT32_MAX;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || depth > function->instructionsLength) {
        return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    writer = compiler_quickening_find_latest_writer_in_range(function, 0, instructionIndex, slot, &writerIndex);
    if (writer == ZR_NULL) {
        return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    switch ((EZrInstructionCode)writer->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_VM;
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            return compiler_quickening_function_constant_callable_kind(
                    function,
                    (TZrUInt32)writer->instruction.operand.operand2[0]);
        case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT): {
            EZrCompilerQuickeningCallableProvenanceKind provenance =
                    ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
            compiler_quickening_resolve_bound_member_callable_metadata_function(
                    state,
                    ZR_CAST(SZrFunction *, function),
                    writer,
                    writerIndex,
                    ZR_NULL,
                    &provenance);
            return provenance;
        }
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE): {
            EZrCompilerQuickeningCallableProvenanceKind provenance =
                    ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
            compiler_quickening_resolve_owner_callable_metadata_from_closure_capture(
                    state,
                    ZR_CAST(SZrFunction *, function),
                    (TZrUInt32)writer->instruction.operand.operand1[0],
                    depth + 1u,
                    ZR_NULL,
                    &provenance);
            return provenance;
        }
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK): {
            TZrUInt32 sourceSlot = (TZrUInt32)writer->instruction.operand.operand2[0];
            if (sourceSlot == slot) {
                return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
            }
            return compiler_quickening_resolve_latest_prior_callable_provenance(state,
                                                                                function,
                                                                                writerIndex,
                                                                                sourceSlot,
                                                                                depth + 1u);
        }
        default:
            return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }
}

static EZrCompilerQuickeningCallableProvenanceKind compiler_quickening_resolve_unique_prior_callable_provenance(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot,
        TZrUInt32 depth) {
    const TZrInstruction *writer = ZR_NULL;
    TZrUInt32 writerIndex = UINT32_MAX;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || depth > function->instructionsLength) {
        return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    if (instructionIndex > function->instructionsLength) {
        instructionIndex = function->instructionsLength;
    }

    for (TZrUInt32 scanIndex = instructionIndex; scanIndex > 0; scanIndex--) {
        const TZrInstruction *candidate = &function->instructionsList[scanIndex - 1u];
        EZrInstructionCode candidateOpcode = (EZrInstructionCode)candidate->instruction.operationCode;

        if (candidate->instruction.operandExtra != slot ||
            candidateOpcode == ZR_INSTRUCTION_ENUM(NOP) ||
            compiler_quickening_is_control_only_opcode(candidateOpcode)) {
            continue;
        }

        if (writer != ZR_NULL) {
            return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
        }

        writer = candidate;
        writerIndex = scanIndex - 1u;
    }

    if (writer == ZR_NULL) {
        return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    switch ((EZrInstructionCode)writer->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_VM;
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            return compiler_quickening_function_constant_callable_kind(
                    function,
                    (TZrUInt32)writer->instruction.operand.operand2[0]);
        case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT): {
            EZrCompilerQuickeningCallableProvenanceKind provenance =
                    ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
            compiler_quickening_resolve_bound_member_callable_metadata_function(
                    state,
                    ZR_CAST(SZrFunction *, function),
                    writer,
                    writerIndex,
                    ZR_NULL,
                    &provenance);
            return provenance;
        }
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE): {
            EZrCompilerQuickeningCallableProvenanceKind provenance =
                    ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
            compiler_quickening_resolve_owner_callable_metadata_from_closure_capture(
                    state,
                    ZR_CAST(SZrFunction *, function),
                    (TZrUInt32)writer->instruction.operand.operand1[0],
                    depth + 1u,
                    ZR_NULL,
                    &provenance);
            return provenance;
        }
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK): {
            TZrUInt32 sourceSlot = (TZrUInt32)writer->instruction.operand.operand2[0];
            if (sourceSlot == slot) {
                return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
            }
            return compiler_quickening_resolve_latest_prior_callable_provenance(state,
                                                                                function,
                                                                                writerIndex,
                                                                                sourceSlot,
                                                                                depth + 1u);
        }
        default:
            return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }
}

static EZrCompilerQuickeningCallableProvenanceKind compiler_quickening_resolve_child_parameter_callable_provenance_from_owner_calls(
        SZrState *state,
        const SZrFunction *function,
        TZrUInt32 slot,
        TZrUInt32 depth) {
    const SZrFunction *ownerFunction;
    EZrCompilerQuickeningCallableProvenanceKind resolvedProvenance =
            ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    TZrUInt32 matchedCallCount = 0;

    if (state == ZR_NULL || function == ZR_NULL || function->ownerFunction == ZR_NULL ||
        function->ownerFunction->instructionsList == ZR_NULL || slot >= function->parameterCount ||
        depth > function->ownerFunction->instructionsLength + 8u) {
        return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    ownerFunction = function->ownerFunction;
    for (TZrUInt32 instructionIndex = 0; instructionIndex < ownerFunction->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &ownerFunction->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        TZrUInt32 argumentCount;
        TZrUInt32 calleeSlot;
        TZrUInt32 argumentSlot;
        SZrFunction *resolvedCallee;
        EZrCompilerQuickeningCallableProvenanceKind argumentProvenance;

        if (!compiler_quickening_opcode_uses_call_argument_slots(opcode)) {
            continue;
        }

        argumentCount = compiler_quickening_call_argument_count(ownerFunction, instruction);
        if (slot >= argumentCount) {
            continue;
        }

        calleeSlot = (TZrUInt32)instruction->instruction.operand.operand1[0];
        resolvedCallee = compiler_quickening_resolve_callable_metadata_function_for_slot_before_instruction(
                state,
                ZR_CAST(SZrFunction *, ownerFunction),
                instructionIndex,
                calleeSlot,
                depth + 1u,
                ZR_NULL,
                ZR_NULL);
        if (resolvedCallee != function &&
            !compiler_quickening_function_matches_inline_child(resolvedCallee, function)) {
            continue;
        }

        argumentSlot = calleeSlot + 1u + slot;
        argumentProvenance = compiler_quickening_resolve_latest_prior_callable_provenance(
                state,
                ownerFunction,
                instructionIndex,
                argumentSlot,
                depth + 1u);
        if (argumentProvenance == ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN) {
            argumentProvenance = compiler_quickening_resolve_typed_slot_callable_provenance_before_instruction(
                    state,
                    ownerFunction,
                    instructionIndex,
                    argumentSlot,
                    depth + 1u);
        }
        if (argumentProvenance == ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN) {
            return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
        }

        if (matchedCallCount == 0u) {
            resolvedProvenance = argumentProvenance;
            matchedCallCount = 1u;
            continue;
        }

        if (resolvedProvenance != argumentProvenance) {
            return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
        }
    }

    return matchedCallCount > 0u ? resolvedProvenance : ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
}

static TZrBool compiler_quickening_function_constant_read_int64(const SZrFunction *function,
                                                                TZrUInt32 constantIndex,
                                                                TZrInt64 *outValue) {
    const SZrTypeValue *constantValue;

    if (outValue != ZR_NULL) {
        *outValue = 0;
    }
    if (function == ZR_NULL || function->constantValueList == ZR_NULL || outValue == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    constantValue = &function->constantValueList[constantIndex];
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    *outValue = constantValue->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrBool compiler_quickening_function_constant_read_uint64(const SZrFunction *function,
                                                                 TZrUInt32 constantIndex,
                                                                 TZrUInt64 *outValue) {
    const SZrTypeValue *constantValue;

    if (outValue != ZR_NULL) {
        *outValue = 0;
    }
    if (function == ZR_NULL || function->constantValueList == ZR_NULL || outValue == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    constantValue = &function->constantValueList[constantIndex];
    if (constantValue == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(constantValue->type)) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
        *outValue = constantValue->value.nativeObject.nativeUInt64;
    } else {
        *outValue = (TZrUInt64)constantValue->value.nativeObject.nativeInt64;
    }
    return ZR_TRUE;
}

static TZrBool compiler_quickening_find_or_append_uint_constant(SZrState *state,
                                                                SZrFunction *function,
                                                                TZrUInt64 value,
                                                                TZrUInt32 *outIndex) {
    SZrGlobalState *global;
    SZrTypeValue *newConstantList;
    TZrUInt32 constantIndex;
    TZrUInt32 oldLength;

    if (outIndex != ZR_NULL) {
        *outIndex = UINT32_MAX;
    }
    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || outIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    for (constantIndex = 0; constantIndex < function->constantValueLength; constantIndex++) {
        const SZrTypeValue *constantValue = &function->constantValueList[constantIndex];
        if (constantValue != ZR_NULL && ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type) &&
            constantValue->value.nativeObject.nativeUInt64 == value) {
            *outIndex = constantIndex;
            return ZR_TRUE;
        }
    }

    if (function->constantValueLength >= UINT16_MAX) {
        return ZR_FALSE;
    }

    global = state->global;
    oldLength = function->constantValueLength;
    newConstantList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(global,
                                                                      sizeof(*newConstantList) * (oldLength + 1u),
                                                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newConstantList == ZR_NULL) {
        return ZR_FALSE;
    }

    if (oldLength > 0 && function->constantValueList != ZR_NULL) {
        memcpy(newConstantList, function->constantValueList, sizeof(*newConstantList) * oldLength);
        ZrCore_Memory_RawFreeWithType(global,
                                      function->constantValueList,
                                      sizeof(*newConstantList) * oldLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    ZrCore_Value_InitAsUInt(state, &newConstantList[oldLength], value);
    function->constantValueList = newConstantList;
    function->constantValueLength = oldLength + 1u;
    *outIndex = oldLength;
    return ZR_TRUE;
}

static EZrCompilerQuickeningCallableProvenanceKind compiler_quickening_resolve_callable_provenance_before_instruction(
        SZrState *state,
        const SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot,
        TZrUInt32 depth) {
    TZrUInt32 blockStartIndex;
    TZrUInt32 scanIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex > function->instructionsLength || depth > function->instructionsLength) {
        return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    blockStartIndex = instructionIndex;
    while (blockStartIndex > 0 && !blockStarts[blockStartIndex]) {
        blockStartIndex--;
    }

    for (scanIndex = instructionIndex; scanIndex > blockStartIndex; scanIndex--) {
        const TZrInstruction *writer = &function->instructionsList[scanIndex - 1];
        EZrInstructionCode writerOpcode = (EZrInstructionCode)writer->instruction.operationCode;

        if (writer->instruction.operandExtra != slot ||
            writerOpcode == ZR_INSTRUCTION_ENUM(NOP) ||
            compiler_quickening_is_control_only_opcode(writerOpcode)) {
            continue;
        }

        switch (writerOpcode) {
            case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
                return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_VM;
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
                return compiler_quickening_function_constant_callable_kind(
                        function,
                        (TZrUInt32)writer->instruction.operand.operand2[0]);
            case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT): {
                EZrCompilerQuickeningCallableProvenanceKind provenance =
                        ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
                compiler_quickening_resolve_bound_member_callable_metadata_function(
                        state,
                        ZR_CAST(SZrFunction *, function),
                        writer,
                        scanIndex - 1u,
                        ZR_NULL,
                        &provenance);
                return provenance;
            }
            case ZR_INSTRUCTION_ENUM(GET_STACK):
            case ZR_INSTRUCTION_ENUM(SET_STACK): {
                TZrUInt32 sourceSlot = (TZrUInt32)writer->instruction.operand.operand2[0];
                if (sourceSlot == slot) {
                    return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
                }
                return compiler_quickening_resolve_callable_provenance_before_instruction(
                        state,
                        function,
                        blockStarts,
                        scanIndex - 1u,
                        sourceSlot,
                        depth + 1u);
            }
            default:
                return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
        }
    }

    {
        EZrCompilerQuickeningCallableProvenanceKind provenance =
                compiler_quickening_resolve_unique_prior_callable_provenance(
                        state,
                        function,
                        instructionIndex,
                        slot,
                        depth + 1u);
        if (provenance != ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN) {
            return provenance;
        }
    }

    {
        EZrCompilerQuickeningCallableProvenanceKind provenance =
                compiler_quickening_resolve_typed_slot_callable_provenance_before_instruction(
                        state,
                        function,
                        instructionIndex,
                        slot,
                        depth + 1u);
        if (provenance != ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN) {
            return provenance;
        }
    }

    if (compiler_quickening_slot_is_exported_callable_binding(function, slot)) {
        return compiler_quickening_resolve_latest_prior_callable_provenance(
                state,
                function,
                instructionIndex,
                slot,
                depth + 1u);
    }

    if (slot < function->parameterCount) {
        EZrCompilerQuickeningCallableProvenanceKind provenance =
                compiler_quickening_resolve_child_parameter_callable_provenance_from_owner_calls(
                        state,
                        function,
                        slot,
                        depth + 1u);
        if (provenance != ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN) {
            return provenance;
        }
    }

    return ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
}

static SZrFunction *compiler_quickening_resolve_bound_member_callable_metadata_function_from_cache(
        SZrState *state,
        SZrFunction *function,
        TZrUInt32 cacheIndex,
        EZrCompilerQuickeningSlotKind *outSlotKind) {
    SZrFunction *prototypeOwner;
    const SZrFunctionCallSiteCacheEntry *cacheEntry;
    const SZrFunctionMemberEntry *memberEntry;
    SZrObjectPrototype *prototype;
    const SZrMemberDescriptor *descriptor;
    const SZrTypeValue *callableValue;
    SZrString *memberName;
    SZrFunction *resolvedFunction;

    if (outSlotKind != ZR_NULL) {
        *outSlotKind = ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
    }

    if (state == ZR_NULL || function == ZR_NULL) {
        return ZR_NULL;
    }
    if (function->callSiteCaches == ZR_NULL || cacheIndex >= function->callSiteCacheLength) {
        return ZR_NULL;
    }

    cacheEntry = &function->callSiteCaches[cacheIndex];
    if (function->memberEntries == ZR_NULL || cacheEntry->memberEntryIndex >= function->memberEntryLength) {
        return ZR_NULL;
    }

    memberEntry = &function->memberEntries[cacheEntry->memberEntryIndex];
    if (memberEntry->entryKind != ZR_FUNCTION_MEMBER_ENTRY_KIND_BOUND_DESCRIPTOR) {
        return ZR_NULL;
    }

    prototypeOwner = compiler_quickening_find_prototype_owner_function(function);
    if (prototypeOwner == ZR_NULL || prototypeOwner->prototypeCount == 0 ||
        memberEntry->prototypeIndex >= prototypeOwner->prototypeCount) {
        return ZR_NULL;
    }

    if (prototypeOwner->prototypeInstances == ZR_NULL ||
        prototypeOwner->prototypeInstancesLength <= memberEntry->prototypeIndex ||
        prototypeOwner->prototypeInstances[memberEntry->prototypeIndex] == ZR_NULL) {
        ZrCore_Module_CreatePrototypesFromData(state, ZR_NULL, prototypeOwner);
    }

    if (prototypeOwner->prototypeInstances == ZR_NULL ||
        prototypeOwner->prototypeInstancesLength <= memberEntry->prototypeIndex) {
        return ZR_NULL;
    }

    prototype = prototypeOwner->prototypeInstances[memberEntry->prototypeIndex];
    if (prototype == ZR_NULL || memberEntry->descriptorIndex >= prototype->memberDescriptorCount) {
        return ZR_NULL;
    }

    descriptor = &prototype->memberDescriptors[memberEntry->descriptorIndex];
    memberName = descriptor->name != ZR_NULL ? descriptor->name : memberEntry->symbol;
    if (memberName == ZR_NULL) {
        return descriptor->getterFunction;
    }

    callableValue = compiler_quickening_resolve_prototype_member_callable_value(
            state,
            prototype,
            memberName,
            ZR_NULL);
    if (outSlotKind != ZR_NULL) {
        *outSlotKind = compiler_quickening_slot_kind_from_callable_value(state, callableValue);
    }
    resolvedFunction = compiler_quickening_metadata_function_from_callable_value(state, callableValue);
    if (resolvedFunction != ZR_NULL) {
        return resolvedFunction;
    }

    return descriptor->getterFunction;
}

static SZrFunction *compiler_quickening_resolve_bound_member_callable_metadata_function(
        SZrState *state,
        SZrFunction *function,
        const TZrInstruction *writer,
        TZrUInt32 writerIndex,
        EZrCompilerQuickeningSlotKind *outSlotKind,
        EZrCompilerQuickeningCallableProvenanceKind *outProvenance) {
    const SZrFunctionCallSiteCacheEntry *cacheEntry;
    const SZrFunctionMemberEntry *memberEntry;
    const SZrFunctionTypedLocalBinding *binding;
    const SZrFunctionTypedTypeRef *receiverTypeRef = ZR_NULL;
    const SZrTypeValue *callableValue;
    const SZrMemberDescriptor *descriptor = ZR_NULL;
    SZrObjectPrototype *prototype;
    SZrFunction *resolvedFunction;
    SZrFunctionTypedTypeRef recoveredReceiverType;
    TZrUInt32 cacheIndex;
    TZrUInt32 receiverSlot;

    if (outSlotKind != ZR_NULL) {
        *outSlotKind = ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
    }
    if (outProvenance != ZR_NULL) {
        *outProvenance = ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    if (state == ZR_NULL || function == ZR_NULL || writer == ZR_NULL ||
        (EZrInstructionCode)writer->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)) {
        return ZR_NULL;
    }

    cacheIndex = (TZrUInt32)writer->instruction.operand.operand1[1];
    resolvedFunction = compiler_quickening_resolve_bound_member_callable_metadata_function_from_cache(
            state,
            function,
            cacheIndex,
            outSlotKind);
    if (resolvedFunction != ZR_NULL) {
        if (outProvenance != ZR_NULL) {
            *outProvenance = ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_VM;
        }
        return resolvedFunction;
    }

    if (function->callSiteCaches == ZR_NULL || cacheIndex >= function->callSiteCacheLength) {
        return ZR_NULL;
    }

    cacheEntry = &function->callSiteCaches[cacheIndex];
    if (function->memberEntries == ZR_NULL || cacheEntry->memberEntryIndex >= function->memberEntryLength) {
        return ZR_NULL;
    }

    memberEntry = &function->memberEntries[cacheEntry->memberEntryIndex];
    if (memberEntry->entryKind != ZR_FUNCTION_MEMBER_ENTRY_KIND_SYMBOL || memberEntry->symbol == ZR_NULL) {
        return ZR_NULL;
    }

    receiverSlot = (TZrUInt32)writer->instruction.operand.operand1[0];
    binding = compiler_quickening_find_active_typed_local_binding(function, receiverSlot, writerIndex);
    if (binding == ZR_NULL &&
        (function->localVariableList == ZR_NULL || function->localVariableLength == 0)) {
        binding = compiler_quickening_find_typed_local_binding(function, receiverSlot);
    }
    if (binding != ZR_NULL && binding->type.typeName != ZR_NULL) {
        receiverTypeRef = &binding->type;
    } else if (compiler_quickening_resolve_slot_type_ref_before_instruction_in_range(function,
                                                                                      0,
                                                                                      writerIndex,
                                                                                      receiverSlot,
                                                                                      0u,
                                                                                      &recoveredReceiverType)) {
        receiverTypeRef = &recoveredReceiverType;
    }

    if (receiverTypeRef == ZR_NULL) {
        return ZR_NULL;
    }

    prototype = compiler_quickening_resolve_type_ref_runtime_prototype(state, function, receiverTypeRef);
    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }

    callableValue = compiler_quickening_resolve_prototype_member_callable_value(
            state,
            prototype,
            memberEntry->symbol,
            &descriptor);
    if (outSlotKind != ZR_NULL) {
        *outSlotKind = compiler_quickening_slot_kind_from_callable_value(state, callableValue);
    }
    if (outProvenance != ZR_NULL) {
        *outProvenance = compiler_quickening_callable_value_provenance(callableValue);
    }

    resolvedFunction = compiler_quickening_metadata_function_from_callable_value(state, callableValue);
    if (resolvedFunction != ZR_NULL) {
        return resolvedFunction;
    }

    if (descriptor != ZR_NULL && descriptor->getterFunction != ZR_NULL) {
        if (outSlotKind != ZR_NULL && *outSlotKind == ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN &&
            descriptor->getterFunction->hasCallableReturnType) {
            *outSlotKind = compiler_quickening_slot_kind_from_typed_type_ref(
                    &descriptor->getterFunction->callableReturnType);
        }
        if (outProvenance != ZR_NULL &&
            *outProvenance == ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN) {
            *outProvenance = ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_VM;
        }
        return descriptor->getterFunction;
    }

    return ZR_NULL;
}

static SZrFunction *compiler_quickening_resolve_callable_metadata_function_for_slot_before_instruction(
        SZrState *state,
        SZrFunction *function,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot,
        TZrUInt32 depth,
        EZrCompilerQuickeningSlotKind *outSlotKind,
        EZrCompilerQuickeningCallableProvenanceKind *outProvenance) {
    const TZrInstruction *writer;
    TZrUInt32 writerIndex = UINT32_MAX;

    if (outSlotKind != ZR_NULL) {
        *outSlotKind = ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
    }
    if (outProvenance != ZR_NULL) {
        *outProvenance = ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_UNKNOWN;
    }

    if (state == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL ||
        depth > function->instructionsLength) {
        return ZR_NULL;
    }

    writer = compiler_quickening_find_latest_writer_in_range(function, 0, instructionIndex, slot, &writerIndex);
    if (writer == ZR_NULL) {
        return ZR_NULL;
    }

    switch ((EZrInstructionCode)writer->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION): {
            TZrUInt32 childIndex = (TZrUInt32)writer->instruction.operand.operand1[0];
            if (function->childFunctionList == ZR_NULL || childIndex >= function->childFunctionLength) {
                return ZR_NULL;
            }
            if (outSlotKind != ZR_NULL && function->childFunctionList[childIndex].hasCallableReturnType) {
                *outSlotKind = compiler_quickening_slot_kind_from_typed_type_ref(
                        &function->childFunctionList[childIndex].callableReturnType);
            }
            if (outProvenance != ZR_NULL) {
                *outProvenance = ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_VM;
            }
            return &function->childFunctionList[childIndex];
        }
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE): {
            TZrUInt32 constantIndex =
                    (EZrInstructionCode)writer->instruction.operationCode == ZR_INSTRUCTION_ENUM(GET_CONSTANT)
                            ? (TZrUInt32)writer->instruction.operand.operand2[0]
                            : (TZrUInt32)writer->instruction.operand.operand1[0];
            const SZrTypeValue *constantValue;
            if (function->constantValueList == ZR_NULL || constantIndex >= function->constantValueLength) {
                return ZR_NULL;
            }
            constantValue = &function->constantValueList[constantIndex];
            if (outSlotKind != ZR_NULL) {
                *outSlotKind = compiler_quickening_slot_kind_from_callable_value(state, constantValue);
            }
            if (outProvenance != ZR_NULL) {
                *outProvenance = compiler_quickening_callable_value_provenance(constantValue);
            }
            return compiler_quickening_metadata_function_from_callable_value(state, constantValue);
        }
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
            return compiler_quickening_resolve_owner_callable_metadata_from_closure_capture(
                    state,
                    function,
                    (TZrUInt32)writer->instruction.operand.operand1[0],
                    depth + 1u,
                    outSlotKind,
                    outProvenance);
        case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
            return compiler_quickening_resolve_bound_member_callable_metadata_function(
                    state,
                    function,
                    writer,
                    writerIndex,
                    outSlotKind,
                    outProvenance);
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK): {
            TZrUInt32 sourceSlot = (TZrUInt32)writer->instruction.operand.operand2[0];
            if (sourceSlot == slot) {
                return ZR_NULL;
            }
            return compiler_quickening_resolve_callable_metadata_function_for_slot_before_instruction(
                    state,
                    function,
                    writerIndex,
                    sourceSlot,
                    depth + 1u,
                    outSlotKind,
                    outProvenance);
        }
        default:
            return ZR_NULL;
    }
}

static EZrCompilerQuickeningSlotKind compiler_quickening_known_call_result_slot_kind(SZrState *state,
                                                                                      SZrFunction *function,
                                                                                      TZrUInt32 instructionIndex,
                                                                                      const TZrInstruction *instruction) {
    SZrFunction *calleeFunction;
    EZrCompilerQuickeningSlotKind resolvedSlotKind = ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
    EZrInstructionCode opcode;

    if (state == ZR_NULL || function == ZR_NULL || instruction == ZR_NULL) {
        return ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    if (opcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8)) {
        calleeFunction = compiler_quickening_resolve_bound_member_callable_metadata_function_from_cache(
                state,
                function,
                (TZrUInt32)instruction->instruction.operand.operand0[0],
                &resolvedSlotKind);
    } else if (opcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL) ||
               opcode == ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL)) {
        calleeFunction = compiler_quickening_resolve_bound_member_callable_metadata_function_from_cache(
                state,
                function,
                (TZrUInt32)instruction->instruction.operand.operand1[0],
                &resolvedSlotKind);
    } else {
        calleeFunction = compiler_quickening_resolve_callable_metadata_function_for_slot_before_instruction(
                state,
                function,
                instructionIndex,
                (TZrUInt32)instruction->instruction.operand.operand1[0],
                0,
                &resolvedSlotKind,
                ZR_NULL);
    }
    if (calleeFunction != ZR_NULL && calleeFunction->hasCallableReturnType) {
        return compiler_quickening_slot_kind_from_typed_type_ref(&calleeFunction->callableReturnType);
    }

    return resolvedSlotKind;
}

static TZrBool compiler_quickening_resolve_index_access_int_constant(const SZrFunction *function,
                                                                     const TZrBool *blockStarts,
                                                                     TZrUInt32 instructionIndex,
                                                                     TZrUInt32 slot);

static EZrCompilerQuickeningSlotKind compiler_quickening_slot_kind_for_slot(const SZrFunction *function,
                                                                             const ZrCompilerQuickeningSlotAlias *aliases,
                                                                             TZrUInt32 aliasCount,
                                                                             const EZrCompilerQuickeningSlotKind *slotKinds,
                                                                             TZrUInt32 instructionIndex,
                                                                             TZrUInt32 slot) {
    TZrUInt32 rootSlot = 0;
    const SZrFunctionTypedLocalBinding *binding;

    binding = compiler_quickening_find_active_typed_local_binding(function, slot, instructionIndex);
    if (binding != ZR_NULL) {
        return compiler_quickening_slot_kind_from_binding(binding);
    }

    if (slotKinds != ZR_NULL && slot < aliasCount &&
        slotKinds[slot] != ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN) {
        return slotKinds[slot];
    }

    if (compiler_quickening_resolve_alias_slot(function, aliases, aliasCount, instructionIndex, slot, &rootSlot)) {
        binding = compiler_quickening_find_active_typed_local_binding(function, rootSlot, instructionIndex);
        if (binding == ZR_NULL &&
            (function == ZR_NULL || function->localVariableList == ZR_NULL || function->localVariableLength == 0)) {
            binding = compiler_quickening_find_typed_local_binding(function, rootSlot);
        }
        return compiler_quickening_slot_kind_from_binding(binding);
    }

    if (function == ZR_NULL || function->localVariableList == ZR_NULL || function->localVariableLength == 0) {
        binding = compiler_quickening_find_typed_local_binding(function, slot);
        return compiler_quickening_slot_kind_from_binding(binding);
    }

    return ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
}

static const SZrFunctionTypedLocalBinding *compiler_quickening_resolve_named_binding_for_slot(
        const SZrFunction *function,
        const ZrCompilerQuickeningSlotAlias *aliases,
        TZrUInt32 aliasCount,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot) {
    const SZrFunctionTypedLocalBinding *binding;
    TZrUInt32 rootSlot = 0;

    binding = compiler_quickening_find_active_typed_local_binding(function, slot, instructionIndex);
    if (binding != ZR_NULL && binding->type.typeName != ZR_NULL) {
        return binding;
    }

    if (!compiler_quickening_resolve_alias_slot(function, aliases, aliasCount, instructionIndex, slot, &rootSlot)) {
        return ZR_NULL;
    }

    binding = compiler_quickening_find_active_typed_local_binding(function, rootSlot, instructionIndex);
    if (binding == ZR_NULL &&
        (function == ZR_NULL || function->localVariableList == ZR_NULL || function->localVariableLength == 0)) {
        binding = compiler_quickening_find_typed_local_binding(function, rootSlot);
    }

    return binding != ZR_NULL && binding->type.typeName != ZR_NULL ? binding : ZR_NULL;
}

static TZrBool compiler_quickening_type_ref_supports_static_member_slots(
        const SZrFunctionTypedTypeRef *typeRef) {
    const TZrChar *typeNameText;

    if (typeRef == ZR_NULL || typeRef->typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeRef->baseType != ZR_VALUE_TYPE_OBJECT &&
        typeRef->baseType != ZR_VALUE_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    typeNameText = compiler_quickening_type_name_text(typeRef->typeName);
    if (typeNameText == ZR_NULL || typeNameText[0] == '\0') {
        return ZR_FALSE;
    }

    if (strcmp(typeNameText, "object") == 0 || strcmp(typeNameText, "Object") == 0 ||
        strcmp(typeNameText, "dynamic") == 0 || strcmp(typeNameText, "Dynamic") == 0) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool compiler_quickening_slot_is_int(const SZrFunction *function,
                                               const ZrCompilerQuickeningSlotAlias *aliases,
                                               TZrUInt32 aliasCount,
                                               const EZrCompilerQuickeningSlotKind *slotKinds,
                                               const TZrBool *blockStarts,
                                               TZrUInt32 instructionIndex,
                                               TZrUInt32 slot) {
    const SZrFunctionLocalVariable *activeLocalVariable;

    activeLocalVariable = compiler_quickening_find_active_local_variable(function, slot, instructionIndex);
    return compiler_quickening_slot_kind_is_integral(
                   compiler_quickening_slot_kind_for_slot(function, aliases, aliasCount, slotKinds, instructionIndex, slot)) ||
           compiler_quickening_resolve_index_access_int_constant(function, blockStarts, instructionIndex, slot) ||
           (activeLocalVariable != ZR_NULL &&
            compiler_quickening_slot_has_only_int_writers_in_range(function,
                                                                   (TZrUInt32)activeLocalVariable->offsetActivate,
                                                                   (TZrUInt32)activeLocalVariable->offsetDead,
                                                                   slot,
                                                                   0)) ||
           compiler_quickening_slot_has_only_int_writers_in_range(function,
                                                                  0,
                                                                  function->instructionsLength,
                                                                  slot,
                                                                  0);
}

static TZrBool compiler_quickening_slot_is_array_int(const SZrFunction *function,
                                                      const ZrCompilerQuickeningSlotAlias *aliases,
                                                      TZrUInt32 aliasCount,
                                                      const EZrCompilerQuickeningSlotKind *slotKinds,
                                                      TZrUInt32 instructionIndex,
                                                      TZrUInt32 slot) {
    return compiler_quickening_slot_kind_for_slot(function, aliases, aliasCount, slotKinds, instructionIndex, slot) ==
           ZR_COMPILER_QUICKENING_SLOT_KIND_ARRAY_INT;
}

static const TZrChar *compiler_quickening_member_entry_symbol_text(const SZrFunction *function, TZrUInt16 memberEntryIndex) {
    if (function == ZR_NULL || function->memberEntries == ZR_NULL || memberEntryIndex >= function->memberEntryLength) {
        return ZR_NULL;
    }

    return compiler_quickening_type_name_text(function->memberEntries[memberEntryIndex].symbol);
}

static EZrInstructionCode compiler_quickening_specialized_numeric_opcode(
        EZrInstructionCode opcode,
        EZrCompilerQuickeningSlotKind leftKind,
        EZrCompilerQuickeningSlotKind rightKind) {
    if (leftKind != rightKind) {
        return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD):
            switch (leftKind) {
                case ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT:
                    return ZR_INSTRUCTION_ENUM(ADD_SIGNED);
                case ZR_COMPILER_QUICKENING_SLOT_KIND_UNSIGNED_INT:
                    return ZR_INSTRUCTION_ENUM(ADD_UNSIGNED);
                case ZR_COMPILER_QUICKENING_SLOT_KIND_FLOAT:
                    return ZR_INSTRUCTION_ENUM(ADD_FLOAT);
                case ZR_COMPILER_QUICKENING_SLOT_KIND_STRING:
                    return ZR_INSTRUCTION_ENUM(ADD_STRING);
                default:
                    return ZR_INSTRUCTION_ENUM(ENUM_MAX);
            }
        case ZR_INSTRUCTION_ENUM(SUB):
            switch (leftKind) {
                case ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT:
                    return ZR_INSTRUCTION_ENUM(SUB_SIGNED);
                case ZR_COMPILER_QUICKENING_SLOT_KIND_UNSIGNED_INT:
                    return ZR_INSTRUCTION_ENUM(SUB_UNSIGNED);
                case ZR_COMPILER_QUICKENING_SLOT_KIND_FLOAT:
                    return ZR_INSTRUCTION_ENUM(SUB_FLOAT);
                default:
                    return ZR_INSTRUCTION_ENUM(ENUM_MAX);
            }
        case ZR_INSTRUCTION_ENUM(MUL):
            switch (leftKind) {
                case ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT:
                    return ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                case ZR_COMPILER_QUICKENING_SLOT_KIND_UNSIGNED_INT:
                    return ZR_INSTRUCTION_ENUM(MUL_UNSIGNED);
                case ZR_COMPILER_QUICKENING_SLOT_KIND_FLOAT:
                    return ZR_INSTRUCTION_ENUM(MUL_FLOAT);
                default:
                    return ZR_INSTRUCTION_ENUM(ENUM_MAX);
            }
        case ZR_INSTRUCTION_ENUM(DIV):
            switch (leftKind) {
                case ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT:
                    return ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                case ZR_COMPILER_QUICKENING_SLOT_KIND_UNSIGNED_INT:
                    return ZR_INSTRUCTION_ENUM(DIV_UNSIGNED);
                case ZR_COMPILER_QUICKENING_SLOT_KIND_FLOAT:
                    return ZR_INSTRUCTION_ENUM(DIV_FLOAT);
                default:
                    return ZR_INSTRUCTION_ENUM(ENUM_MAX);
            }
        case ZR_INSTRUCTION_ENUM(MOD):
            switch (leftKind) {
                case ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT:
                    return ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                case ZR_COMPILER_QUICKENING_SLOT_KIND_UNSIGNED_INT:
                    return ZR_INSTRUCTION_ENUM(MOD_UNSIGNED);
                case ZR_COMPILER_QUICKENING_SLOT_KIND_FLOAT:
                    return ZR_INSTRUCTION_ENUM(MOD_FLOAT);
                default:
                    return ZR_INSTRUCTION_ENUM(ENUM_MAX);
            }
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

static EZrInstructionCode compiler_quickening_specialized_equality_opcode(
        EZrInstructionCode opcode,
        EZrCompilerQuickeningSlotKind leftKind,
        EZrCompilerQuickeningSlotKind rightKind) {
    if (opcode != ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL) && opcode != ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL)) {
        return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
    if (leftKind != rightKind) {
        return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }

    switch (leftKind) {
        case ZR_COMPILER_QUICKENING_SLOT_KIND_BOOL:
            return opcode == ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL) ? ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL)
                                                                : ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL);
        case ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT:
            return opcode == ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL) ? ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED)
                                                                : ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED);
        case ZR_COMPILER_QUICKENING_SLOT_KIND_UNSIGNED_INT:
            return opcode == ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL) ? ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED)
                                                                : ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED);
        case ZR_COMPILER_QUICKENING_SLOT_KIND_FLOAT:
            return opcode == ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL) ? ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT)
                                                                : ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT);
        case ZR_COMPILER_QUICKENING_SLOT_KIND_STRING:
            return opcode == ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL) ? ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING)
                                                                : ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING);
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

static EZrInstructionCode compiler_quickening_specialized_arithmetic_const_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_INT):
            return ZR_INSTRUCTION_ENUM(ADD_INT_CONST);
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST);
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
            return ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST);
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
            return ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST);
        case ZR_INSTRUCTION_ENUM(SUB_INT):
            return ZR_INSTRUCTION_ENUM(SUB_INT_CONST);
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST);
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
            return ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST);
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
            return ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST);
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
            return ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST);
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
            return ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST);
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
            return ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
            return ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
            return ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
            return ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST);
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

static TZrBool compiler_quickening_constant_matches_arithmetic_opcode(const SZrFunction *function,
                                                                      EZrInstructionCode opcode,
                                                                      TZrUInt32 constantIndex) {
    EZrCompilerQuickeningSlotKind constantKind =
            compiler_quickening_function_constant_slot_kind(function, constantIndex);

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
            return compiler_quickening_slot_kind_is_integral(constantKind);
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
            return constantKind == ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT;
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
            return constantKind == ZR_COMPILER_QUICKENING_SLOT_KIND_UNSIGNED_INT;
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_quickening_resolve_index_access_int_constant(const SZrFunction *function,
                                                                     const TZrBool *blockStarts,
                                                                     TZrUInt32 instructionIndex,
                                                                     TZrUInt32 slot) {
    TZrUInt32 currentSlot;
    TZrUInt32 blockStartIndex = 0;
    TZrUInt32 hop;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    for (TZrUInt32 scan = instructionIndex + 1; scan > 0; scan--) {
        TZrUInt32 candidate = scan - 1;
        if (blockStarts[candidate]) {
            blockStartIndex = candidate;
            break;
        }
    }

    currentSlot = slot;
    for (hop = 0; hop < function->stackSize; hop++) {
        TZrBool foundWriter = ZR_FALSE;

        for (TZrUInt32 scan = instructionIndex; scan > blockStartIndex; scan--) {
            const TZrInstruction *writer = &function->instructionsList[scan - 1];
            EZrInstructionCode writerOpcode = (EZrInstructionCode)writer->instruction.operationCode;

            if (writer->instruction.operandExtra != currentSlot) {
                continue;
            }

            foundWriter = ZR_TRUE;
            if (writerOpcode == ZR_INSTRUCTION_ENUM(GET_STACK) || writerOpcode == ZR_INSTRUCTION_ENUM(SET_STACK)) {
                TZrUInt32 sourceSlot = (TZrUInt32)writer->instruction.operand.operand2[0];
                if (sourceSlot == currentSlot) {
                    return ZR_FALSE;
                }
                currentSlot = sourceSlot;
                break;
            }

            if (writerOpcode == ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
                return compiler_quickening_function_constant_is_int(function,
                                                                    (TZrUInt32)writer->instruction.operand.operand2[0]);
            }

            return ZR_FALSE;
        }

        if (!foundWriter) {
            return ZR_FALSE;
        }
    }

    return ZR_FALSE;
}

static TZrInstruction *compiler_quickening_find_latest_block_writer(SZrFunction *function,
                                                                    const TZrBool *blockStarts,
                                                                    TZrUInt32 instructionIndex,
                                                                    TZrUInt32 slot,
                                                                    TZrUInt32 *outWriterIndex) {
    TZrUInt32 blockStartIndex = 0;

    if (outWriterIndex != ZR_NULL) {
        *outWriterIndex = UINT32_MAX;
    }
    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex == 0 || instructionIndex > function->instructionsLength) {
        return ZR_NULL;
    }

    for (TZrUInt32 scan = instructionIndex; scan > 0; scan--) {
        TZrUInt32 candidate = scan - 1;
        if (blockStarts[candidate]) {
            blockStartIndex = candidate;
            break;
        }
    }

    for (TZrInt64 scan = (TZrInt64)instructionIndex - 1; scan >= (TZrInt64)blockStartIndex; scan--) {
        TZrInstruction *writer = &function->instructionsList[scan];
        if (writer->instruction.operandExtra != slot) {
            continue;
        }
        if (outWriterIndex != ZR_NULL) {
            *outWriterIndex = (TZrUInt32)scan;
        }
        return writer;
    }

    return ZR_NULL;
}

static TZrBool compiler_quickening_is_control_only_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP):
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(TRY):
        case ZR_INSTRUCTION_ENUM(END_TRY):
        case ZR_INSTRUCTION_ENUM(THROW):
        case ZR_INSTRUCTION_ENUM(CATCH):
        case ZR_INSTRUCTION_ENUM(END_FINALLY):
        case ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED):
        case ZR_INSTRUCTION_ENUM(CLOSE_SCOPE):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_BREAK):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_CONTINUE):
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static void compiler_quickening_mark_jump_target(TZrBool *blockStarts,
                                                 TZrUInt32 instructionLength,
                                                 TZrInt32 targetIndex) {
    if (blockStarts == ZR_NULL || targetIndex < 0) {
        return;
    }

    if ((TZrUInt32)targetIndex < instructionLength) {
        blockStarts[targetIndex] = ZR_TRUE;
    }
}

static TZrBool compiler_quickening_build_block_starts(const SZrFunction *function,
                                                      TZrBool *blockStarts) {
    TZrUInt32 index;

    if (function == ZR_NULL || blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(blockStarts, 0, sizeof(*blockStarts) * function->instructionsLength);
    if (function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    blockStarts[0] = ZR_TRUE;
    for (index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if (opcode == ZR_INSTRUCTION_ENUM(JUMP) || opcode == ZR_INSTRUCTION_ENUM(JUMP_IF)) {
            TZrInt32 targetIndex = (TZrInt32)index + instruction->instruction.operand.operand2[0] + 1;
            compiler_quickening_mark_jump_target(blockStarts, function->instructionsLength, targetIndex);
            if (index + 1 < function->instructionsLength) {
                blockStarts[index + 1] = ZR_TRUE;
            }
            continue;
        }

        if (opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST)) {
            TZrInt32 targetIndex = (TZrInt32)index + (TZrInt16)instruction->instruction.operand.operand1[1] + 1;
            compiler_quickening_mark_jump_target(blockStarts, function->instructionsLength, targetIndex);
            if (index + 1 < function->instructionsLength) {
                blockStarts[index + 1] = ZR_TRUE;
            }
            continue;
        }

        if (opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)) {
            TZrInt32 targetIndex = (TZrInt32)index + (TZrInt16)instruction->instruction.operand.operand1[1] + 1;
            compiler_quickening_mark_jump_target(blockStarts, function->instructionsLength, targetIndex);
            if (index + 1 < function->instructionsLength) {
                blockStarts[index + 1] = ZR_TRUE;
            }
            continue;
        }

        if (compiler_quickening_is_control_only_opcode(opcode) && index + 1 < function->instructionsLength) {
            blockStarts[index + 1] = ZR_TRUE;
        }
    }

    return ZR_TRUE;
}

static TZrBool compiler_quickening_instruction_destination_becomes_plain(const SZrFunction *function,
                                                                         const TZrInstruction *instruction,
                                                                         const TZrBool *plainSlots,
                                                                         TZrUInt32 slotCount) {
    EZrInstructionCode opcode;

    if (function == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK): {
            TZrUInt32 sourceSlot = (TZrUInt32)instruction->instruction.operand.operand2[0];
            return plainSlots != ZR_NULL && sourceSlot < slotCount && plainSlots[sourceSlot];
        }
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            return compiler_quickening_function_constant_is_plain_primitive(
                    function,
                    (TZrUInt32)instruction->instruction.operand.operand2[0]);
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
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
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_quickening_try_get_plain_destination_variant(EZrInstructionCode opcode,
                                                                     EZrInstructionCode *outVariant) {
    if (outVariant == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
            *outVariant = ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(ADD_INT):
            *outVariant = ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
            *outVariant = ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
            *outVariant = ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
            *outVariant = ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
            *outVariant = ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
            *outVariant = ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(SUB_INT):
            *outVariant = ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
            *outVariant = ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
            *outVariant = ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
            *outVariant = ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
            *outVariant = ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
            *outVariant = ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
            *outVariant = ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
            *outVariant = ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
            *outVariant = ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
            *outVariant = ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
            *outVariant = ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
            *outVariant = ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
            *outVariant = ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST);
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
            *outVariant = ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST);
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_quickening_promote_plain_destination_opcodes(SZrFunction *function,
                                                                     const TZrBool *blockStarts) {
    TZrUInt32 aliasCount;
    TZrUInt32 index;
    TZrBool *plainState = ZR_NULL;
    TZrBool *tempOnlySlots = ZR_NULL;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || blockStarts == ZR_NULL || function->instructionsList == ZR_NULL ||
        function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    aliasCount = function->stackSize;
    if (aliasCount == 0) {
        return ZR_TRUE;
    }

    plainState = (TZrBool *)malloc(sizeof(*plainState) * aliasCount);
    tempOnlySlots = (TZrBool *)malloc(sizeof(*tempOnlySlots) * aliasCount);
    if (plainState == ZR_NULL || tempOnlySlots == ZR_NULL) {
        goto cleanup;
    }

    for (index = 0; index < aliasCount; index++) {
        tempOnlySlots[index] = compiler_quickening_slot_has_any_binding(function, index) ? ZR_FALSE : ZR_TRUE;
    }
    memset(plainState, 0, sizeof(*plainState) * aliasCount);

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        EZrInstructionCode promotedOpcode;
        TZrUInt32 destinationSlot = instruction->instruction.operandExtra;

        if (blockStarts[index]) {
            memset(plainState, 0, sizeof(*plainState) * aliasCount);
        }

        if (compiler_quickening_try_get_plain_destination_variant(opcode, &promotedOpcode) &&
            destinationSlot < aliasCount &&
            tempOnlySlots[destinationSlot] &&
            plainState[destinationSlot]) {
            instruction->instruction.operationCode = (TZrUInt16)promotedOpcode;
            opcode = promotedOpcode;
        }

        if (opcode == ZR_INSTRUCTION_ENUM(NOP) ||
            compiler_quickening_is_control_only_opcode(opcode) ||
            destinationSlot >= aliasCount) {
            continue;
        }

        plainState[destinationSlot] =
                (TZrBool)(tempOnlySlots[destinationSlot] &&
                          compiler_quickening_instruction_destination_becomes_plain(function,
                                                                                  instruction,
                                                                                  plainState,
                                                                                  aliasCount));
        compiler_quickening_clear_temp_only_plain_tracking_after_call(instruction, plainState, tempOnlySlots, aliasCount);
    }

    success = ZR_TRUE;

cleanup:
    free(plainState);
    free(tempOnlySlots);
    return success;
}

static TZrBool compiler_quickening_can_skip_over_super_array_store_to_load_forward(
        const TZrInstruction *instruction,
        TZrUInt32 receiverSlot,
        TZrUInt32 indexSlot,
        TZrUInt32 valueSlot) {
    EZrInstructionCode opcode;
    TZrUInt32 destinationSlot;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    if (opcode == ZR_INSTRUCTION_ENUM(NOP)) {
        return ZR_TRUE;
    }

    if (compiler_quickening_is_control_only_opcode(opcode)) {
        return ZR_FALSE;
    }

    destinationSlot = instruction->instruction.operandExtra;
    if (destinationSlot == receiverSlot || destinationSlot == indexSlot || destinationSlot == valueSlot) {
        return ZR_FALSE;
    }

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_quickening_forward_super_array_store_to_load_reads(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index + 1 < function->instructionsLength; index++) {
        TZrInstruction *storeInstruction = &function->instructionsList[index];
        EZrInstructionCode storeOpcode = (EZrInstructionCode)storeInstruction->instruction.operationCode;
        TZrUInt32 receiverSlot;
        TZrUInt32 indexSlot;
        TZrUInt32 valueSlot;
        TZrUInt32 scan;

        if (storeOpcode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT)) {
            continue;
        }

        receiverSlot = storeInstruction->instruction.operand.operand1[0];
        indexSlot = storeInstruction->instruction.operand.operand1[1];
        valueSlot = storeInstruction->instruction.operandExtra;
        if (valueSlot == ZR_INSTRUCTION_USE_RET_FLAG) {
            continue;
        }

        for (scan = index + 1; scan < function->instructionsLength; scan++) {
            TZrInstruction *instruction = &function->instructionsList[scan];
            EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

            if (scan > index + 1 && blockStarts[scan]) {
                break;
            }

            if ((opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT) ||
                 opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST)) &&
                instruction->instruction.operand.operand1[0] == receiverSlot &&
                instruction->instruction.operand.operand1[1] == indexSlot) {
                instruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_STACK);
                instruction->instruction.operand.operand2[0] = (TZrInt32)valueSlot;
                continue;
            }

            if (!compiler_quickening_can_skip_over_super_array_store_to_load_forward(
                        instruction,
                        receiverSlot,
                        indexSlot,
                        valueSlot)) {
                break;
            }
        }
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static void compiler_quickening_write_nop(TZrInstruction *instruction) {
    if (instruction == ZR_NULL) {
        return;
    }

    instruction->value = 0;
    instruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(NOP);
}

static TZrBool compiler_quickening_fuse_known_native_member_calls(SZrFunction *function) {
    TZrBool *blockStarts;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)calloc(function->instructionsLength, sizeof(TZrBool));
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index + 1 < function->instructionsLength; index++) {
        TZrInstruction *loadInstruction = &function->instructionsList[index];
        EZrInstructionCode loadOpcode = (EZrInstructionCode)loadInstruction->instruction.operationCode;
        TZrUInt16 functionSlot;
        TZrUInt32 callIndex;

        if (loadOpcode != ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)) {
            continue;
        }

        functionSlot = loadInstruction->instruction.operandExtra;
        for (callIndex = index + 1; callIndex < function->instructionsLength; callIndex++) {
            TZrInstruction *candidate = &function->instructionsList[callIndex];
            EZrInstructionCode candidateOpcode = (EZrInstructionCode)candidate->instruction.operationCode;

            if (blockStarts[callIndex]) {
                break;
            }

            if (candidateOpcode == ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL)) {
                if (candidate->instruction.operand.operand1[0] == functionSlot &&
                    candidate->instruction.operandExtra == functionSlot) {
                    candidate->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL);
                    candidate->instruction.operand.operand1[0] = loadInstruction->instruction.operand.operand1[1];
                    compiler_quickening_write_nop(loadInstruction);
                    index = callIndex;
                }
                break;
            }

            if (candidateOpcode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
                candidate->instruction.operandExtra == functionSlot ||
                (TZrUInt32)candidate->instruction.operand.operand2[0] == functionSlot) {
                break;
            }
        }
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_fuse_known_vm_member_call_load1_u8(SZrFunction *function) {
    TZrBool *blockStarts;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 3) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)calloc(function->instructionsLength, sizeof(TZrBool));
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index + 2u < function->instructionsLength; index++) {
        TZrInstruction *receiverLoad = &function->instructionsList[index];
        TZrInstruction *argumentLoad = &function->instructionsList[index + 1u];
        TZrInstruction *callInstruction = &function->instructionsList[index + 2u];
        TZrUInt32 resultSlot;
        TZrUInt32 cacheIndex;
        TZrUInt32 receiverDestinationSlot;
        TZrUInt32 argumentDestinationSlot;
        TZrUInt32 receiverSourceSlot;
        TZrUInt32 argumentSourceSlot;

        if (blockStarts[index + 1u] || blockStarts[index + 2u] ||
            receiverLoad->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
            argumentLoad->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
            callInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)) {
            continue;
        }

        resultSlot = callInstruction->instruction.operandExtra;
        cacheIndex = (TZrUInt32)callInstruction->instruction.operand.operand1[0];
        receiverDestinationSlot = receiverLoad->instruction.operandExtra;
        argumentDestinationSlot = argumentLoad->instruction.operandExtra;
        receiverSourceSlot = (TZrUInt32)receiverLoad->instruction.operand.operand2[0];
        argumentSourceSlot = (TZrUInt32)argumentLoad->instruction.operand.operand2[0];

        if (cacheIndex >= function->callSiteCacheLength ||
            function->callSiteCaches == ZR_NULL ||
            function->callSiteCaches[cacheIndex].argumentCount != 2u ||
            receiverDestinationSlot != resultSlot + 1u ||
            argumentDestinationSlot != resultSlot + 2u ||
            resultSlot > UINT16_MAX ||
            cacheIndex > 0xFFu ||
            receiverSourceSlot > 0xFFu ||
            argumentSourceSlot > 0xFFu) {
            continue;
        }

        *callInstruction = create_instruction_4(
                ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8),
                (TZrUInt16)resultSlot,
                (TZrUInt8)cacheIndex,
                (TZrUInt8)receiverSourceSlot,
                (TZrUInt8)argumentSourceSlot,
                0u);
        compiler_quickening_write_nop(receiverLoad);
        compiler_quickening_write_nop(argumentLoad);
        index += 2u;
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_rewrite_null_constant_loads(SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *instruction = &function->instructionsList[index];
        TZrUInt32 constantIndex;

        if ((EZrInstructionCode)instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
            instruction->instruction.operandExtra == ZR_INSTRUCTION_USE_RET_FLAG) {
            continue;
        }

        constantIndex = (TZrUInt32)instruction->instruction.operand.operand2[0];
        if (constantIndex >= function->constantValueLength ||
            function->constantValueList == ZR_NULL ||
            !ZR_VALUE_IS_TYPE_NULL(function->constantValueList[constantIndex].type)) {
            continue;
        }

        *instruction = create_instruction_0(ZR_INSTRUCTION_ENUM(RESET_STACK_NULL),
                                            instruction->instruction.operandExtra);
    }

    return ZR_TRUE;
}

static TZrUInt32 compiler_quickening_remap_instruction_index(const TZrUInt32 *oldToNew,
                                                             TZrUInt32 oldLength,
                                                             TZrUInt32 newLength,
                                                             TZrMemoryOffset oldIndex) {
    if (oldToNew == ZR_NULL) {
        return newLength;
    }

    if (oldIndex < 0) {
        return newLength;
    }

    if (oldIndex >= oldLength) {
        return newLength;
    }

    while (oldIndex < oldLength) {
        TZrUInt32 oldIndex32 = (TZrUInt32)oldIndex;
        if (oldToNew[oldIndex32] != UINT32_MAX) {
            return oldToNew[oldIndex32];
        }
        oldIndex++;
    }

    return newLength;
}

static void compiler_quickening_remap_local_variable_instruction_offsets(
        SZrFunction *function,
        const TZrUInt32 *oldToNew,
        TZrUInt32 oldLength,
        TZrUInt32 newLength) {
    TZrUInt32 localIndex;

    if (function == ZR_NULL || function->localVariableList == ZR_NULL || function->localVariableLength == 0 ||
        oldToNew == ZR_NULL) {
        return;
    }

    for (localIndex = 0; localIndex < function->localVariableLength; localIndex++) {
        SZrFunctionLocalVariable *localVariable = &function->localVariableList[localIndex];
        TZrUInt32 remappedActivate = compiler_quickening_remap_instruction_index(
                oldToNew,
                oldLength,
                newLength,
                (TZrUInt32)localVariable->offsetActivate);
        TZrUInt32 remappedDead = compiler_quickening_remap_instruction_index(
                oldToNew,
                oldLength,
                newLength,
                (TZrUInt32)localVariable->offsetDead);

        if (remappedActivate > newLength) {
            remappedActivate = newLength;
        }
        if (remappedDead > newLength) {
            remappedDead = newLength;
        }
        if (remappedDead < remappedActivate) {
            remappedDead = remappedActivate;
        }

        localVariable->offsetActivate = (TZrMemoryOffset)remappedActivate;
        localVariable->offsetDead = (TZrMemoryOffset)remappedDead;
    }
}

static TZrBool compiler_quickening_rewrite_compacted_branches(TZrInstruction *instructions,
                                                              const TZrUInt32 *oldToNew,
                                                              TZrUInt32 oldLength,
                                                              TZrUInt32 newLength) {
    TZrUInt32 oldIndex;

    if (instructions == ZR_NULL || oldToNew == ZR_NULL) {
        return ZR_FALSE;
    }

    for (oldIndex = 0; oldIndex < oldLength; oldIndex++) {
        TZrUInt32 newIndex;
        TZrInstruction *instruction;
        EZrInstructionCode opcode;
        TZrInt64 targetIndex;
        TZrUInt32 remappedTarget;
        TZrInt64 newOffset;

        if (oldToNew[oldIndex] == UINT32_MAX) {
            continue;
        }

        newIndex = oldToNew[oldIndex];
        instruction = &instructions[newIndex];
        opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        if (opcode != ZR_INSTRUCTION_ENUM(JUMP) &&
            opcode != ZR_INSTRUCTION_ENUM(JUMP_IF) &&
            opcode != ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED) &&
            opcode != ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED) &&
            opcode != ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST) &&
            opcode != ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE) &&
            opcode != ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)) {
            continue;
        }

        targetIndex = (TZrInt64)oldIndex + 1;
        if (opcode == ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE) ||
            opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST)) {
            targetIndex += (TZrInt16)instruction->instruction.operand.operand1[1];
        } else {
            targetIndex += instruction->instruction.operand.operand2[0];
        }

        if (targetIndex < 0 || (TZrUInt64)targetIndex > (TZrUInt64)oldLength) {
            if (compiler_quickening_trace_enabled()) {
                fprintf(stderr,
                        "[zr-quickening] branch remap fail oldIndex=%u opcode=%u target=%lld oldLength=%u newLength=%u reason=target_out_of_range\n",
                        oldIndex,
                        (unsigned int)opcode,
                        (long long)targetIndex,
                        oldLength,
                        newLength);
            }
            return ZR_FALSE;
        }

        remappedTarget = compiler_quickening_remap_instruction_index(oldToNew,
                                                                     oldLength,
                                                                     newLength,
                                                                     (TZrUInt32)targetIndex);
        if (remappedTarget > newLength) {
            if (compiler_quickening_trace_enabled()) {
                fprintf(stderr,
                        "[zr-quickening] branch remap fail oldIndex=%u opcode=%u target=%lld remapped=%u oldLength=%u newLength=%u reason=remapped_target_out_of_range\n",
                        oldIndex,
                        (unsigned int)opcode,
                        (long long)targetIndex,
                        remappedTarget,
                        oldLength,
                        newLength);
            }
            return ZR_FALSE;
        }

        newOffset = (TZrInt64)remappedTarget - (TZrInt64)newIndex - 1;
        if (opcode == ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE) ||
            opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST)) {
            if (newOffset < INT16_MIN || newOffset > INT16_MAX) {
                if (compiler_quickening_trace_enabled()) {
                    fprintf(stderr,
                            "[zr-quickening] branch remap fail oldIndex=%u opcode=%u newIndex=%u remapped=%u newOffset=%lld reason=int16_overflow\n",
                            oldIndex,
                            (unsigned int)opcode,
                            newIndex,
                            remappedTarget,
                            (long long)newOffset);
                }
                return ZR_FALSE;
            }
            instruction->instruction.operand.operand1[1] = (TZrUInt16)((TZrInt16)newOffset);
        } else {
            instruction->instruction.operand.operand2[0] = (TZrInt32)newOffset;
        }
    }

    return ZR_TRUE;
}

static TZrBool compiler_quickening_rewrite_inserted_super_array_items_cache_branches(
        TZrInstruction *instructions,
        const TZrUInt32 *oldToNewEntry,
        const TZrUInt32 *oldToNewOriginal,
        const TZrUInt32 *skipPreheaderForBackedgeSource,
        TZrUInt32 oldLength,
        TZrUInt32 newLength) {
    TZrUInt32 oldIndex;

    if (instructions == ZR_NULL || oldToNewEntry == ZR_NULL || oldToNewOriginal == ZR_NULL ||
        skipPreheaderForBackedgeSource == ZR_NULL) {
        return ZR_FALSE;
    }

    for (oldIndex = 0; oldIndex < oldLength; oldIndex++) {
        TZrUInt32 newIndex;
        TZrInstruction *instruction;
        EZrInstructionCode opcode;
        TZrInt64 targetIndex;
        TZrUInt32 remappedTarget;
        TZrInt64 newOffset;

        if (oldToNewOriginal[oldIndex] == UINT32_MAX) {
            continue;
        }

        newIndex = oldToNewOriginal[oldIndex];
        instruction = &instructions[newIndex];
        opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        if (opcode != ZR_INSTRUCTION_ENUM(JUMP) &&
            opcode != ZR_INSTRUCTION_ENUM(JUMP_IF) &&
            opcode != ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED) &&
            opcode != ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED) &&
            opcode != ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST) &&
            opcode != ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)) {
            continue;
        }

        targetIndex = (TZrInt64)oldIndex + 1;
        if (opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST)) {
            targetIndex += (TZrInt16)instruction->instruction.operand.operand1[1];
        } else {
            targetIndex += instruction->instruction.operand.operand2[0];
        }

        if (targetIndex < 0 || (TZrUInt64)targetIndex > (TZrUInt64)oldLength) {
            return ZR_FALSE;
        }

        if (targetIndex == (TZrInt64)oldLength) {
            remappedTarget = newLength;
        } else if (skipPreheaderForBackedgeSource[oldIndex] == (TZrUInt32)targetIndex) {
            remappedTarget = compiler_quickening_remap_instruction_index(oldToNewOriginal,
                                                                         oldLength,
                                                                         newLength,
                                                                         (TZrUInt32)targetIndex);
        } else {
            remappedTarget = compiler_quickening_remap_instruction_index(oldToNewEntry,
                                                                         oldLength,
                                                                         newLength,
                                                                         (TZrUInt32)targetIndex);
        }

        if (remappedTarget > newLength) {
            return ZR_FALSE;
        }

        newOffset = (TZrInt64)remappedTarget - (TZrInt64)newIndex - 1;
        if (opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED) ||
            opcode == ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST)) {
            if (newOffset < INT16_MIN || newOffset > INT16_MAX) {
                return ZR_FALSE;
            }
            instruction->instruction.operand.operand1[1] = (TZrUInt16)((TZrInt16)newOffset);
        } else {
            instruction->instruction.operand.operand2[0] = (TZrInt32)newOffset;
        }
    }

    return ZR_TRUE;
}

static TZrBool compiler_quickening_compact_nops(SZrState *state, SZrFunction *function) {
    TZrUInt32 *oldToNew = ZR_NULL;
    TZrInstruction *newInstructions = ZR_NULL;
    TZrUInt32 *newLineInSourceList = ZR_NULL;
    SZrFunctionExecutionLocationInfo *newExecutionLocationInfoList = ZR_NULL;
    TZrUInt32 newExecutionLocationInfoLength = 0;
    TZrUInt32 liveCount = 0;
    TZrUInt32 oldIndex;
    TZrUInt32 executionInfoIndex;
    TZrBool hasNop = ZR_FALSE;
    TZrSize instructionBytes;
    SZrGlobalState *global;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL ||
        function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    global = state->global;
    for (oldIndex = 0; oldIndex < function->instructionsLength; oldIndex++) {
        if ((EZrInstructionCode)function->instructionsList[oldIndex].instruction.operationCode ==
            ZR_INSTRUCTION_ENUM(NOP)) {
            hasNop = ZR_TRUE;
            continue;
        }
        liveCount++;
    }

    if (!hasNop) {
        return ZR_TRUE;
    }

    oldToNew = (TZrUInt32 *)malloc(sizeof(*oldToNew) * function->instructionsLength);
    if (oldToNew == ZR_NULL) {
        return ZR_FALSE;
    }

    liveCount = 0;
    for (oldIndex = 0; oldIndex < function->instructionsLength; oldIndex++) {
        if ((EZrInstructionCode)function->instructionsList[oldIndex].instruction.operationCode ==
            ZR_INSTRUCTION_ENUM(NOP)) {
            oldToNew[oldIndex] = UINT32_MAX;
        } else {
            oldToNew[oldIndex] = liveCount++;
        }
    }

    if (liveCount == 0) {
        free(oldToNew);
        return ZR_FALSE;
    }

    instructionBytes = sizeof(*newInstructions) * liveCount;
    newInstructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(global,
                                                                        instructionBytes,
                                                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newInstructions == ZR_NULL) {
        free(oldToNew);
        return ZR_FALSE;
    }

    if (function->lineInSourceList != ZR_NULL) {
        newLineInSourceList = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(global,
                                                                           sizeof(*newLineInSourceList) * liveCount,
                                                                           ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newLineInSourceList == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          newInstructions,
                                          instructionBytes,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            free(oldToNew);
            return ZR_FALSE;
        }
    }

    if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        newExecutionLocationInfoList = (SZrFunctionExecutionLocationInfo *)ZrCore_Memory_RawMallocWithType(
                global,
                sizeof(*newExecutionLocationInfoList) * function->executionLocationInfoLength,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newExecutionLocationInfoList == ZR_NULL) {
            if (newLineInSourceList != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(global,
                                              newLineInSourceList,
                                              sizeof(*newLineInSourceList) * liveCount,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            ZrCore_Memory_RawFreeWithType(global,
                                          newInstructions,
                                          instructionBytes,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            free(oldToNew);
            return ZR_FALSE;
        }
    }

    for (oldIndex = 0; oldIndex < function->instructionsLength; oldIndex++) {
        TZrUInt32 newIndex;

        if (oldToNew[oldIndex] == UINT32_MAX) {
            continue;
        }

        newIndex = oldToNew[oldIndex];
        newInstructions[newIndex] = function->instructionsList[oldIndex];
        if (newLineInSourceList != ZR_NULL) {
            newLineInSourceList[newIndex] = function->lineInSourceList[oldIndex];
        }
    }

    if (!compiler_quickening_rewrite_compacted_branches(newInstructions,
                                                        oldToNew,
                                                        function->instructionsLength,
                                                        liveCount)) {
        if (newExecutionLocationInfoList != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          newExecutionLocationInfoList,
                                          sizeof(*newExecutionLocationInfoList) * function->executionLocationInfoLength,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (newLineInSourceList != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          newLineInSourceList,
                                          sizeof(*newLineInSourceList) * liveCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        ZrCore_Memory_RawFreeWithType(global,
                                      newInstructions,
                                      instructionBytes,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        free(oldToNew);
        return ZR_FALSE;
    }

    if (newExecutionLocationInfoList != ZR_NULL) {
        for (executionInfoIndex = 0; executionInfoIndex < function->executionLocationInfoLength; executionInfoIndex++) {
            const SZrFunctionExecutionLocationInfo *oldInfo = &function->executionLocationInfoList[executionInfoIndex];
            TZrUInt32 remappedIndex = compiler_quickening_remap_instruction_index(oldToNew,
                                                                                  function->instructionsLength,
                                                                                  liveCount,
                                                                                  oldInfo->currentInstructionOffset);

            if (remappedIndex >= liveCount) {
                continue;
            }
            if (newExecutionLocationInfoLength > 0) {
                const SZrFunctionExecutionLocationInfo *previousInfo =
                        &newExecutionLocationInfoList[newExecutionLocationInfoLength - 1];

                if (previousInfo->currentInstructionOffset == remappedIndex &&
                    previousInfo->lineInSource == oldInfo->lineInSource &&
                    previousInfo->columnInSourceStart == oldInfo->columnInSourceStart &&
                    previousInfo->lineInSourceEnd == oldInfo->lineInSourceEnd &&
                    previousInfo->columnInSourceEnd == oldInfo->columnInSourceEnd) {
                    continue;
                }
            }

            newExecutionLocationInfoList[newExecutionLocationInfoLength] = *oldInfo;
            newExecutionLocationInfoList[newExecutionLocationInfoLength].currentInstructionOffset = remappedIndex;
            newExecutionLocationInfoLength++;
        }
    }

    for (oldIndex = 0; oldIndex < function->catchClauseCount; oldIndex++) {
        function->catchClauseList[oldIndex].targetInstructionOffset =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->catchClauseList[oldIndex].targetInstructionOffset);
    }

    for (oldIndex = 0; oldIndex < function->exceptionHandlerCount; oldIndex++) {
        function->exceptionHandlerList[oldIndex].protectedStartInstructionOffset =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->exceptionHandlerList[oldIndex]
                                                                    .protectedStartInstructionOffset);
        function->exceptionHandlerList[oldIndex].finallyTargetInstructionOffset =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->exceptionHandlerList[oldIndex]
                                                                    .finallyTargetInstructionOffset);
        function->exceptionHandlerList[oldIndex].afterFinallyInstructionOffset =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->exceptionHandlerList[oldIndex]
                                                                    .afterFinallyInstructionOffset);
    }

    for (oldIndex = 0; oldIndex < function->semIrInstructionLength; oldIndex++) {
        function->semIrInstructions[oldIndex].execInstructionIndex =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->semIrInstructions[oldIndex].execInstructionIndex);
    }

    for (oldIndex = 0; oldIndex < function->semIrDeoptTableLength; oldIndex++) {
        function->semIrDeoptTable[oldIndex].execInstructionIndex =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->semIrDeoptTable[oldIndex].execInstructionIndex);
    }

    for (oldIndex = 0; oldIndex < function->callSiteCacheLength; oldIndex++) {
        function->callSiteCaches[oldIndex].instructionIndex =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            function->instructionsLength,
                                                            liveCount,
                                                            function->callSiteCaches[oldIndex].instructionIndex);
    }

    compiler_quickening_remap_local_variable_instruction_offsets(function,
                                                                 oldToNew,
                                                                 function->instructionsLength,
                                                                 liveCount);

    ZR_MEMORY_RAW_FREE_LIST(global, function->instructionsList, function->instructionsLength);
    if (function->lineInSourceList != ZR_NULL) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->lineInSourceList, function->instructionsLength);
    }
    if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->executionLocationInfoList, function->executionLocationInfoLength);
    }

    function->instructionsList = newInstructions;
    function->instructionsLength = liveCount;
    function->lineInSourceList = newLineInSourceList;
    function->executionLocationInfoList = newExecutionLocationInfoList;
    function->executionLocationInfoLength = newExecutionLocationInfoLength;

    free(oldToNew);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_is_super_array_items_cache_access(EZrInstructionCode opcode) {
    return opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT) ||
           opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST) ||
           opcode == ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT);
}

static EZrInstructionCode compiler_quickening_super_array_items_cache_variant(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST);
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
            return ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS);
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
            return ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS);
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

static TZrBool compiler_quickening_super_array_items_cache_access_receiver(const TZrInstruction *instruction,
                                                                           TZrUInt32 *outReceiverSlot) {
    EZrInstructionCode opcode;

    if (outReceiverSlot != ZR_NULL) {
        *outReceiverSlot = UINT32_MAX;
    }
    if (instruction == ZR_NULL || outReceiverSlot == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    if (!compiler_quickening_is_super_array_items_cache_access(opcode)) {
        return ZR_FALSE;
    }

    *outReceiverSlot = (TZrUInt32)instruction->instruction.operand.operand1[0];
    return ZR_TRUE;
}

static TZrBool compiler_quickening_super_array_items_cache_instruction_may_escape(const TZrInstruction *instruction) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_TRUE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):
        case ZR_INSTRUCTION_ENUM(META_SET):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED):
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(THROW):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static void compiler_quickening_clear_bound_super_array_items(TZrUInt32 *boundItemsSlotByReceiver,
                                                              TZrUInt32 receiverCount) {
    TZrUInt32 index;

    if (boundItemsSlotByReceiver == ZR_NULL) {
        return;
    }

    for (index = 0; index < receiverCount; index++) {
        boundItemsSlotByReceiver[index] = UINT32_MAX;
    }
}

static void compiler_quickening_invalidate_written_super_array_item_receivers(
        const TZrInstruction *instruction,
        TZrUInt32 *boundItemsSlotByReceiver,
        TZrUInt32 receiverCount) {
    TZrUInt32 receiverSlot;

    if (instruction == ZR_NULL || boundItemsSlotByReceiver == ZR_NULL) {
        return;
    }

    for (receiverSlot = 0; receiverSlot < receiverCount; receiverSlot++) {
        if (boundItemsSlotByReceiver[receiverSlot] == UINT32_MAX) {
            continue;
        }
        if (compiler_quickening_instruction_writes_slot(instruction, receiverSlot)) {
            boundItemsSlotByReceiver[receiverSlot] = UINT32_MAX;
        }
    }
}

typedef struct SZrSuperArrayItemsCacheBindInsertion {
    TZrUInt32 oldIndex;
    TZrUInt32 itemsSlot;
    TZrUInt32 receiverSlot;
    TZrUInt32 nextForOldIndex;
} SZrSuperArrayItemsCacheBindInsertion;

static TZrBool compiler_quickening_append_super_array_items_cache_bind(
        SZrSuperArrayItemsCacheBindInsertion **ioInsertions,
        TZrUInt32 *ioInsertionCount,
        TZrUInt32 *ioInsertionCapacity,
        TZrUInt32 *firstInsertionBefore,
        TZrUInt32 *lastInsertionBefore,
        TZrUInt32 oldLength,
        TZrUInt32 oldIndex,
        TZrUInt32 itemsSlot,
        TZrUInt32 receiverSlot) {
    SZrSuperArrayItemsCacheBindInsertion *insertions;
    TZrUInt32 insertionIndex;

    if (ioInsertions == ZR_NULL || ioInsertionCount == ZR_NULL || ioInsertionCapacity == ZR_NULL ||
        firstInsertionBefore == ZR_NULL || lastInsertionBefore == ZR_NULL || oldIndex >= oldLength) {
        return ZR_FALSE;
    }

    if (*ioInsertionCount == *ioInsertionCapacity) {
        TZrUInt32 newCapacity = *ioInsertionCapacity == 0 ? 8u : (*ioInsertionCapacity * 2u);
        SZrSuperArrayItemsCacheBindInsertion *newInsertions;

        if (newCapacity <= *ioInsertionCapacity) {
            return ZR_FALSE;
        }

        newInsertions = (SZrSuperArrayItemsCacheBindInsertion *)realloc(
                *ioInsertions,
                sizeof(**ioInsertions) * newCapacity);
        if (newInsertions == ZR_NULL) {
            return ZR_FALSE;
        }

        *ioInsertions = newInsertions;
        *ioInsertionCapacity = newCapacity;
    }

    insertions = *ioInsertions;
    insertionIndex = *ioInsertionCount;
    insertions[insertionIndex].oldIndex = oldIndex;
    insertions[insertionIndex].itemsSlot = itemsSlot;
    insertions[insertionIndex].receiverSlot = receiverSlot;
    insertions[insertionIndex].nextForOldIndex = UINT32_MAX;

    if (firstInsertionBefore[oldIndex] == UINT32_MAX) {
        firstInsertionBefore[oldIndex] = insertionIndex;
    } else {
        insertions[lastInsertionBefore[oldIndex]].nextForOldIndex = insertionIndex;
    }
    lastInsertionBefore[oldIndex] = insertionIndex;
    (*ioInsertionCount)++;
    return ZR_TRUE;
}

static TZrBool compiler_quickening_allocate_super_array_items_slot(TZrUInt32 receiverSlot,
                                                                   TZrUInt32 originalStackSize,
                                                                   TZrUInt32 *itemsSlotByReceiver,
                                                                   TZrUInt32 *ioItemsSlotCount,
                                                                   TZrUInt32 *outItemsSlot) {
    TZrUInt32 itemsSlot;

    if (itemsSlotByReceiver == ZR_NULL || ioItemsSlotCount == ZR_NULL || outItemsSlot == ZR_NULL ||
        receiverSlot >= originalStackSize) {
        return ZR_FALSE;
    }

    itemsSlot = itemsSlotByReceiver[receiverSlot];
    if (itemsSlot == UINT32_MAX) {
        if (originalStackSize + *ioItemsSlotCount > UINT16_MAX) {
            return ZR_FALSE;
        }
        itemsSlot = originalStackSize + *ioItemsSlotCount;
        itemsSlotByReceiver[receiverSlot] = itemsSlot;
        (*ioItemsSlotCount)++;
    }

    *outItemsSlot = itemsSlot;
    return itemsSlot <= UINT16_MAX;
}

static TZrBool compiler_quickening_plan_super_array_items_cache_bindings(
        const SZrFunction *function,
        const TZrBool *blockStarts,
        SZrSuperArrayItemsCacheBindInsertion **ioInsertions,
        TZrUInt32 *ioInsertionCount,
        TZrUInt32 *ioInsertionCapacity,
        TZrUInt32 *firstInsertionBefore,
        TZrUInt32 *lastInsertionBefore,
        EZrInstructionCode *rewriteOpcode,
        TZrUInt32 *rewriteItemsSlot,
        TZrUInt32 *itemsSlotByReceiver,
        TZrUInt32 *skipPreheaderForBackedgeSource,
        TZrUInt32 *ioItemsSlotCount,
        TZrUInt32 *outInsertCount) {
    TZrUInt32 originalStackSize;
    TZrUInt16 *accessCountByReceiver = ZR_NULL;
    TZrUInt32 *boundItemsSlotByReceiver = ZR_NULL;
    TZrUInt32 blockStart;
    TZrBool success = ZR_FALSE;

    if (outInsertCount != ZR_NULL) {
        *outInsertCount = 0;
    }
    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        ioInsertions == ZR_NULL || ioInsertionCount == ZR_NULL || ioInsertionCapacity == ZR_NULL ||
        firstInsertionBefore == ZR_NULL || lastInsertionBefore == ZR_NULL || rewriteOpcode == ZR_NULL ||
        rewriteItemsSlot == ZR_NULL || itemsSlotByReceiver == ZR_NULL || skipPreheaderForBackedgeSource == ZR_NULL ||
        ioItemsSlotCount == ZR_NULL || outInsertCount == ZR_NULL) {
        return ZR_FALSE;
    }

    originalStackSize = function->stackSize;
    if (originalStackSize == 0) {
        return ZR_TRUE;
    }

    accessCountByReceiver = (TZrUInt16 *)malloc(sizeof(*accessCountByReceiver) * originalStackSize);
    boundItemsSlotByReceiver = (TZrUInt32 *)malloc(sizeof(*boundItemsSlotByReceiver) * originalStackSize);
    if (accessCountByReceiver == ZR_NULL || boundItemsSlotByReceiver == ZR_NULL) {
        free(boundItemsSlotByReceiver);
        free(accessCountByReceiver);
        return ZR_FALSE;
    }

    for (blockStart = 0; blockStart < function->instructionsLength; blockStart++) {
        const TZrInstruction *backedge = &function->instructionsList[blockStart];
        EZrInstructionCode backedgeOpcode = (EZrInstructionCode)backedge->instruction.operationCode;
        TZrInt64 loopStartSigned;
        TZrUInt32 loopStart;
        TZrUInt32 index;
        TZrBool loopMayEscape = ZR_FALSE;
        TZrBool insertedLoopBind = ZR_FALSE;

        if (backedgeOpcode != ZR_INSTRUCTION_ENUM(JUMP)) {
            continue;
        }

        loopStartSigned = (TZrInt64)blockStart + 1 + backedge->instruction.operand.operand2[0];
        if (loopStartSigned < 0 || (TZrUInt64)loopStartSigned >= (TZrUInt64)blockStart) {
            continue;
        }

        loopStart = (TZrUInt32)loopStartSigned;
        memset(accessCountByReceiver, 0, sizeof(*accessCountByReceiver) * originalStackSize);

        for (index = loopStart; index <= blockStart; index++) {
            TZrUInt32 receiverSlot;

            if (compiler_quickening_super_array_items_cache_instruction_may_escape(
                        &function->instructionsList[index])) {
                loopMayEscape = ZR_TRUE;
                break;
            }

            if (rewriteOpcode[index] != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                continue;
            }

            if (!compiler_quickening_super_array_items_cache_access_receiver(&function->instructionsList[index],
                                                                             &receiverSlot) ||
                receiverSlot >= originalStackSize) {
                continue;
            }

            if (accessCountByReceiver[receiverSlot] < UINT16_MAX) {
                accessCountByReceiver[receiverSlot]++;
            }
        }

        if (loopMayEscape) {
            continue;
        }

        for (index = 0; index < originalStackSize; index++) {
            TZrUInt32 scanIndex;
            TZrUInt32 itemsSlot;
            TZrBool receiverWritten = ZR_FALSE;
            TZrBool rewroteLoopAccess = ZR_FALSE;

            if (accessCountByReceiver[index] == 0) {
                continue;
            }

            for (scanIndex = loopStart; scanIndex <= blockStart; scanIndex++) {
                if (compiler_quickening_instruction_writes_slot(&function->instructionsList[scanIndex], index)) {
                    receiverWritten = ZR_TRUE;
                    break;
                }
            }
            if (receiverWritten) {
                continue;
            }

            if (!compiler_quickening_allocate_super_array_items_slot(index,
                                                                     originalStackSize,
                                                                     itemsSlotByReceiver,
                                                                     ioItemsSlotCount,
                                                                     &itemsSlot)) {
                goto cleanup;
            }

            for (scanIndex = loopStart; scanIndex <= blockStart; scanIndex++) {
                const TZrInstruction *instruction = &function->instructionsList[scanIndex];
                EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
                TZrUInt32 receiverSlot;

                if (rewriteOpcode[scanIndex] != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                    continue;
                }
                if (!compiler_quickening_super_array_items_cache_access_receiver(instruction, &receiverSlot) ||
                    receiverSlot != index) {
                    continue;
                }

                rewriteOpcode[scanIndex] = compiler_quickening_super_array_items_cache_variant(opcode);
                rewriteItemsSlot[scanIndex] = itemsSlot;
                rewroteLoopAccess = ZR_TRUE;
            }

            if (rewroteLoopAccess) {
                if (!compiler_quickening_append_super_array_items_cache_bind(ioInsertions,
                                                                             ioInsertionCount,
                                                                             ioInsertionCapacity,
                                                                             firstInsertionBefore,
                                                                             lastInsertionBefore,
                                                                             function->instructionsLength,
                                                                             loopStart,
                                                                             itemsSlot,
                                                                             index)) {
                    goto cleanup;
                }
                (*outInsertCount)++;
                insertedLoopBind = ZR_TRUE;
            }
        }

        if (insertedLoopBind) {
            skipPreheaderForBackedgeSource[blockStart] = loopStart;
        }
    }

    for (blockStart = 0; blockStart < function->instructionsLength;) {
        TZrUInt32 blockEnd = blockStart + 1u;
        TZrUInt32 index;

        while (blockEnd < function->instructionsLength && !blockStarts[blockEnd]) {
            blockEnd++;
        }

        memset(accessCountByReceiver, 0, sizeof(*accessCountByReceiver) * originalStackSize);
        compiler_quickening_clear_bound_super_array_items(boundItemsSlotByReceiver, originalStackSize);

        for (index = blockStart; index < blockEnd; index++) {
            TZrUInt32 receiverSlot;
            if (rewriteOpcode[index] != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                continue;
            }
            if (!compiler_quickening_super_array_items_cache_access_receiver(&function->instructionsList[index],
                                                                             &receiverSlot) ||
                receiverSlot >= originalStackSize) {
                continue;
            }
            if (accessCountByReceiver[receiverSlot] < UINT16_MAX) {
                accessCountByReceiver[receiverSlot]++;
            }
        }

        for (index = blockStart; index < blockEnd; index++) {
            const TZrInstruction *instruction = &function->instructionsList[index];
            EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
            TZrUInt32 receiverSlot;

            if (rewriteOpcode[index] != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                continue;
            }

            if (compiler_quickening_super_array_items_cache_access_receiver(instruction, &receiverSlot) &&
                receiverSlot < originalStackSize &&
                accessCountByReceiver[receiverSlot] >= 2u) {
                TZrUInt32 itemsSlot;

                if (!compiler_quickening_allocate_super_array_items_slot(receiverSlot,
                                                                         originalStackSize,
                                                                         itemsSlotByReceiver,
                                                                         ioItemsSlotCount,
                                                                         &itemsSlot)) {
                    goto cleanup;
                }

                if (boundItemsSlotByReceiver[receiverSlot] == UINT32_MAX) {
                    if (!compiler_quickening_append_super_array_items_cache_bind(ioInsertions,
                                                                                 ioInsertionCount,
                                                                                 ioInsertionCapacity,
                                                                                 firstInsertionBefore,
                                                                                 lastInsertionBefore,
                                                                                 function->instructionsLength,
                                                                                 index,
                                                                                 itemsSlot,
                                                                                 receiverSlot)) {
                        goto cleanup;
                    }
                    boundItemsSlotByReceiver[receiverSlot] = itemsSlot;
                    (*outInsertCount)++;
                }

                rewriteOpcode[index] = compiler_quickening_super_array_items_cache_variant(opcode);
                rewriteItemsSlot[index] = itemsSlot;
            }

            if (compiler_quickening_super_array_items_cache_instruction_may_escape(instruction)) {
                compiler_quickening_clear_bound_super_array_items(boundItemsSlotByReceiver, originalStackSize);
            } else {
                compiler_quickening_invalidate_written_super_array_item_receivers(instruction,
                                                                                  boundItemsSlotByReceiver,
                                                                                  originalStackSize);
            }
        }

        blockStart = blockEnd;
    }

    success = ZR_TRUE;

cleanup:
    free(boundItemsSlotByReceiver);
    free(accessCountByReceiver);
    return success;
}

static void compiler_quickening_remap_metadata_after_instruction_insertion(SZrFunction *function,
                                                                           const TZrUInt32 *oldToNew,
                                                                           TZrUInt32 oldLength,
                                                                           TZrUInt32 newLength) {
    TZrUInt32 index;

    for (index = 0; index < function->catchClauseCount; index++) {
        function->catchClauseList[index].targetInstructionOffset =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            oldLength,
                                                            newLength,
                                                            function->catchClauseList[index].targetInstructionOffset);
    }

    for (index = 0; index < function->exceptionHandlerCount; index++) {
        function->exceptionHandlerList[index].protectedStartInstructionOffset =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            oldLength,
                                                            newLength,
                                                            function->exceptionHandlerList[index]
                                                                    .protectedStartInstructionOffset);
        function->exceptionHandlerList[index].finallyTargetInstructionOffset =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            oldLength,
                                                            newLength,
                                                            function->exceptionHandlerList[index]
                                                                    .finallyTargetInstructionOffset);
        function->exceptionHandlerList[index].afterFinallyInstructionOffset =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            oldLength,
                                                            newLength,
                                                            function->exceptionHandlerList[index]
                                                                    .afterFinallyInstructionOffset);
    }

    for (index = 0; index < function->semIrInstructionLength; index++) {
        function->semIrInstructions[index].execInstructionIndex =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            oldLength,
                                                            newLength,
                                                            function->semIrInstructions[index].execInstructionIndex);
    }

    for (index = 0; index < function->semIrDeoptTableLength; index++) {
        function->semIrDeoptTable[index].execInstructionIndex =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            oldLength,
                                                            newLength,
                                                            function->semIrDeoptTable[index].execInstructionIndex);
    }

    for (index = 0; index < function->callSiteCacheLength; index++) {
        function->callSiteCaches[index].instructionIndex =
                compiler_quickening_remap_instruction_index(oldToNew,
                                                            oldLength,
                                                            newLength,
                                                            function->callSiteCaches[index].instructionIndex);
    }

    compiler_quickening_remap_local_variable_instruction_offsets(function, oldToNew, oldLength, newLength);
}

static TZrBool compiler_quickening_insert_super_array_items_cache_bindings(SZrState *state, SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 *firstInsertionBefore = ZR_NULL;
    TZrUInt32 *lastInsertionBefore = ZR_NULL;
    TZrUInt32 *skipPreheaderForBackedgeSource = ZR_NULL;
    SZrSuperArrayItemsCacheBindInsertion *insertions = ZR_NULL;
    TZrUInt32 insertionCount = 0;
    TZrUInt32 insertionCapacity = 0;
    EZrInstructionCode *rewriteOpcode = ZR_NULL;
    TZrUInt32 *rewriteItemsSlot = ZR_NULL;
    TZrUInt32 *itemsSlotByReceiver = ZR_NULL;
    TZrUInt32 *oldToNewEntry = ZR_NULL;
    TZrUInt32 *oldToNewOriginal = ZR_NULL;
    TZrInstruction *newInstructions = ZR_NULL;
    TZrUInt32 *newLineInSourceList = ZR_NULL;
    SZrFunctionExecutionLocationInfo *newExecutionLocationInfoList = ZR_NULL;
    TZrUInt32 newExecutionLocationInfoLength = 0;
    TZrUInt32 oldLength;
    TZrUInt32 newLength;
    TZrUInt32 oldIndex;
    TZrUInt32 writeIndex = 0;
    TZrUInt32 insertCount = 0;
    TZrUInt32 itemsSlotCount = 0;
    TZrSize instructionBytes;
    SZrGlobalState *global;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL ||
        function->instructionsLength == 0 || function->stackSize == 0) {
        return ZR_TRUE;
    }

    global = state->global;
    oldLength = function->instructionsLength;

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * oldLength);
    firstInsertionBefore = (TZrUInt32 *)malloc(sizeof(*firstInsertionBefore) * oldLength);
    lastInsertionBefore = (TZrUInt32 *)malloc(sizeof(*lastInsertionBefore) * oldLength);
    skipPreheaderForBackedgeSource = (TZrUInt32 *)malloc(sizeof(*skipPreheaderForBackedgeSource) * oldLength);
    rewriteOpcode = (EZrInstructionCode *)malloc(sizeof(*rewriteOpcode) * oldLength);
    rewriteItemsSlot = (TZrUInt32 *)malloc(sizeof(*rewriteItemsSlot) * oldLength);
    itemsSlotByReceiver = (TZrUInt32 *)malloc(sizeof(*itemsSlotByReceiver) * function->stackSize);
    if (blockStarts == ZR_NULL || firstInsertionBefore == ZR_NULL || lastInsertionBefore == ZR_NULL ||
        skipPreheaderForBackedgeSource == ZR_NULL || rewriteOpcode == ZR_NULL || rewriteItemsSlot == ZR_NULL ||
        itemsSlotByReceiver == ZR_NULL) {
        goto cleanup;
    }

    for (oldIndex = 0; oldIndex < oldLength; oldIndex++) {
        firstInsertionBefore[oldIndex] = UINT32_MAX;
        lastInsertionBefore[oldIndex] = UINT32_MAX;
        skipPreheaderForBackedgeSource[oldIndex] = UINT32_MAX;
        rewriteOpcode[oldIndex] = ZR_INSTRUCTION_ENUM(ENUM_MAX);
        rewriteItemsSlot[oldIndex] = UINT32_MAX;
    }
    for (oldIndex = 0; oldIndex < function->stackSize; oldIndex++) {
        itemsSlotByReceiver[oldIndex] = UINT32_MAX;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts) ||
        !compiler_quickening_plan_super_array_items_cache_bindings(function,
                                                                   blockStarts,
                                                                   &insertions,
                                                                   &insertionCount,
                                                                   &insertionCapacity,
                                                                   firstInsertionBefore,
                                                                   lastInsertionBefore,
                                                                   rewriteOpcode,
                                                                   rewriteItemsSlot,
                                                                   itemsSlotByReceiver,
                                                                   skipPreheaderForBackedgeSource,
                                                                   &itemsSlotCount,
                                                                   &insertCount)) {
        goto cleanup;
    }

    if (insertCount == 0 || itemsSlotCount == 0 || insertionCount == 0) {
        success = ZR_TRUE;
        goto cleanup;
    }

    if (oldLength > UINT32_MAX - insertCount || function->stackSize + itemsSlotCount > UINT16_MAX) {
        success = ZR_TRUE;
        goto cleanup;
    }

    newLength = oldLength + insertCount;
    oldToNewEntry = (TZrUInt32 *)malloc(sizeof(*oldToNewEntry) * oldLength);
    oldToNewOriginal = (TZrUInt32 *)malloc(sizeof(*oldToNewOriginal) * oldLength);
    if (oldToNewEntry == ZR_NULL || oldToNewOriginal == ZR_NULL) {
        goto cleanup;
    }

    instructionBytes = sizeof(*newInstructions) * newLength;
    newInstructions = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(global,
                                                                        instructionBytes,
                                                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newInstructions == ZR_NULL) {
        goto cleanup;
    }

    if (function->lineInSourceList != ZR_NULL) {
        newLineInSourceList = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(global,
                                                                           sizeof(*newLineInSourceList) * newLength,
                                                                           ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newLineInSourceList == ZR_NULL) {
            goto cleanup;
        }
    }

    if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        newExecutionLocationInfoList = (SZrFunctionExecutionLocationInfo *)ZrCore_Memory_RawMallocWithType(
                global,
                sizeof(*newExecutionLocationInfoList) * function->executionLocationInfoLength,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newExecutionLocationInfoList == ZR_NULL) {
            goto cleanup;
        }
    }

    for (oldIndex = 0; oldIndex < oldLength; oldIndex++) {
        TZrInstruction copiedInstruction = function->instructionsList[oldIndex];
        TZrUInt32 insertionIndex;

        oldToNewEntry[oldIndex] = writeIndex;
        for (insertionIndex = firstInsertionBefore[oldIndex];
             insertionIndex != UINT32_MAX;
             insertionIndex = insertions[insertionIndex].nextForOldIndex) {
            TZrInstruction bindInstruction;
            bindInstruction.value = 0;
            bindInstruction.instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS);
            bindInstruction.instruction.operandExtra = (TZrUInt16)insertions[insertionIndex].itemsSlot;
            bindInstruction.instruction.operand.operand2[0] = (TZrInt32)insertions[insertionIndex].receiverSlot;
            newInstructions[writeIndex] = bindInstruction;
            if (newLineInSourceList != ZR_NULL) {
                newLineInSourceList[writeIndex] = function->lineInSourceList[oldIndex];
            }
            writeIndex++;
        }

        oldToNewOriginal[oldIndex] = writeIndex;

        if (rewriteOpcode[oldIndex] != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
            copiedInstruction.instruction.operationCode = (TZrUInt16)rewriteOpcode[oldIndex];
            copiedInstruction.instruction.operand.operand1[0] = (TZrUInt16)rewriteItemsSlot[oldIndex];
        }

        newInstructions[writeIndex] = copiedInstruction;
        if (newLineInSourceList != ZR_NULL) {
            newLineInSourceList[writeIndex] = function->lineInSourceList[oldIndex];
        }
        writeIndex++;
    }

    ZR_ASSERT(writeIndex == newLength);
    if (!compiler_quickening_rewrite_inserted_super_array_items_cache_branches(newInstructions,
                                                                               oldToNewEntry,
                                                                               oldToNewOriginal,
                                                                               skipPreheaderForBackedgeSource,
                                                                               oldLength,
                                                                               newLength)) {
        goto cleanup;
    }

    if (newExecutionLocationInfoList != ZR_NULL) {
        TZrUInt32 executionInfoIndex;
        for (executionInfoIndex = 0; executionInfoIndex < function->executionLocationInfoLength; executionInfoIndex++) {
            const SZrFunctionExecutionLocationInfo *oldInfo =
                    &function->executionLocationInfoList[executionInfoIndex];
            TZrUInt32 remappedIndex = compiler_quickening_remap_instruction_index(oldToNewEntry,
                                                                                  oldLength,
                                                                                  newLength,
                                                                                  oldInfo->currentInstructionOffset);
            if (remappedIndex >= newLength) {
                continue;
            }
            if (newExecutionLocationInfoLength > 0) {
                const SZrFunctionExecutionLocationInfo *previousInfo =
                        &newExecutionLocationInfoList[newExecutionLocationInfoLength - 1];
                if (previousInfo->currentInstructionOffset == remappedIndex &&
                    previousInfo->lineInSource == oldInfo->lineInSource &&
                    previousInfo->columnInSourceStart == oldInfo->columnInSourceStart &&
                    previousInfo->lineInSourceEnd == oldInfo->lineInSourceEnd &&
                    previousInfo->columnInSourceEnd == oldInfo->columnInSourceEnd) {
                    continue;
                }
            }
            newExecutionLocationInfoList[newExecutionLocationInfoLength] = *oldInfo;
            newExecutionLocationInfoList[newExecutionLocationInfoLength].currentInstructionOffset = remappedIndex;
            newExecutionLocationInfoLength++;
        }
    }

    compiler_quickening_remap_metadata_after_instruction_insertion(function, oldToNewEntry, oldLength, newLength);

    ZR_MEMORY_RAW_FREE_LIST(global, function->instructionsList, function->instructionsLength);
    if (function->lineInSourceList != ZR_NULL) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->lineInSourceList, function->instructionsLength);
    }
    if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        ZR_MEMORY_RAW_FREE_LIST(global, function->executionLocationInfoList, function->executionLocationInfoLength);
    }

    function->instructionsList = newInstructions;
    function->instructionsLength = newLength;
    function->lineInSourceList = newLineInSourceList;
    function->executionLocationInfoList = newExecutionLocationInfoList;
    function->executionLocationInfoLength = newExecutionLocationInfoLength;
    function->stackSize += itemsSlotCount;
    function->vmEntryClearStackSizePlusOne = 0;

    newInstructions = ZR_NULL;
    newLineInSourceList = ZR_NULL;
    newExecutionLocationInfoList = ZR_NULL;
    success = ZR_TRUE;

cleanup:
    if (newExecutionLocationInfoList != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global,
                                      newExecutionLocationInfoList,
                                      sizeof(*newExecutionLocationInfoList) * function->executionLocationInfoLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (newLineInSourceList != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global,
                                      newLineInSourceList,
                                      sizeof(*newLineInSourceList) * (oldLength + insertCount),
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (newInstructions != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global,
                                      newInstructions,
                                      sizeof(*newInstructions) * (oldLength + insertCount),
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    free(oldToNewOriginal);
    free(oldToNewEntry);
    free(itemsSlotByReceiver);
    free(rewriteItemsSlot);
    free(rewriteOpcode);
    free(insertions);
    free(skipPreheaderForBackedgeSource);
    free(lastInsertionBefore);
    free(firstInsertionBefore);
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_try_fold_dead_super_array_add_setup(SZrFunction *function,
                                                                       const TZrBool *blockStarts,
                                                                       TZrUInt32 instructionIndex) {
    TZrInstruction *addInstruction;
    TZrInstruction *constantInstruction;
    TZrInstruction *receiverReloadInstruction;
    TZrInstruction *receiverStageInstruction;
    TZrInstruction *receiverLoadInstruction;
    TZrUInt32 destinationSlot;
    TZrUInt32 receiverSlot;
    TZrUInt32 valueSlot;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex < 4 || instructionIndex + 1 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    if (blockStarts[instructionIndex] || blockStarts[instructionIndex - 1] || blockStarts[instructionIndex - 2] ||
        blockStarts[instructionIndex - 3] || blockStarts[instructionIndex + 1]) {
        return ZR_FALSE;
    }

    addInstruction = &function->instructionsList[instructionIndex];
    if ((EZrInstructionCode)addInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT) ||
        addInstruction->instruction.operandExtra == ZR_INSTRUCTION_USE_RET_FLAG) {
        return ZR_FALSE;
    }

    destinationSlot = addInstruction->instruction.operandExtra;
    receiverSlot = addInstruction->instruction.operand.operand1[0];
    valueSlot = addInstruction->instruction.operand.operand1[1];
    constantInstruction = &function->instructionsList[instructionIndex - 1];
    receiverReloadInstruction = &function->instructionsList[instructionIndex - 2];
    receiverStageInstruction = &function->instructionsList[instructionIndex - 3];
    receiverLoadInstruction = &function->instructionsList[instructionIndex - 4];

    if ((EZrInstructionCode)constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        constantInstruction->instruction.operandExtra != valueSlot) {
        return ZR_FALSE;
    }

    if ((EZrInstructionCode)receiverReloadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        receiverReloadInstruction->instruction.operandExtra != destinationSlot ||
        (TZrUInt32)receiverReloadInstruction->instruction.operand.operand2[0] != receiverSlot) {
        return ZR_FALSE;
    }

    if ((EZrInstructionCode)receiverStageInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
        receiverStageInstruction->instruction.operandExtra != receiverSlot ||
        (TZrUInt32)receiverStageInstruction->instruction.operand.operand2[0] != destinationSlot) {
        return ZR_FALSE;
    }

    if ((EZrInstructionCode)receiverLoadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        receiverLoadInstruction->instruction.operandExtra != destinationSlot) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_slot_is_overwritten_before_read(function, blockStarts, instructionIndex + 1, destinationSlot) &&
        !compiler_quickening_temp_slot_is_dead_until_block_end_with_terminal_exit(
                function,
                blockStarts,
                instructionIndex + 1,
                destinationSlot)) {
        return ZR_FALSE;
    }

    addInstruction->instruction.operand.operand1[0] =
            (TZrUInt16)receiverLoadInstruction->instruction.operand.operand2[0];
    addInstruction->instruction.operandExtra = ZR_INSTRUCTION_USE_RET_FLAG;
    compiler_quickening_write_nop(receiverLoadInstruction);
    compiler_quickening_write_nop(receiverStageInstruction);
    compiler_quickening_write_nop(receiverReloadInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fold_super_array_add_int4_burst(SZrFunction *function,
                                                                       const TZrBool *blockStarts,
                                                                       TZrUInt32 instructionIndex) {
    TZrInstruction *firstInstruction;
    TZrUInt32 receiverBaseSlot;
    TZrUInt32 valueSlot;
    TZrUInt32 burstIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex + 3 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    firstInstruction = &function->instructionsList[instructionIndex];
    if ((EZrInstructionCode)firstInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT) ||
        firstInstruction->instruction.operandExtra != ZR_INSTRUCTION_USE_RET_FLAG) {
        return ZR_FALSE;
    }

    receiverBaseSlot = firstInstruction->instruction.operand.operand1[0];
    valueSlot = firstInstruction->instruction.operand.operand1[1];
    for (burstIndex = 0; burstIndex < 4; burstIndex++) {
        TZrInstruction *instruction = &function->instructionsList[instructionIndex + burstIndex];

        if (burstIndex > 0 && blockStarts[instructionIndex + burstIndex]) {
            return ZR_FALSE;
        }
        if ((EZrInstructionCode)instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT) ||
            instruction->instruction.operandExtra != ZR_INSTRUCTION_USE_RET_FLAG ||
            instruction->instruction.operand.operand1[0] != receiverBaseSlot + burstIndex ||
            instruction->instruction.operand.operand1[1] != valueSlot) {
            return ZR_FALSE;
        }
    }

    firstInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4);
    firstInstruction->instruction.operand.operand1[0] = (TZrUInt16)receiverBaseSlot;
    firstInstruction->instruction.operand.operand1[1] = (TZrUInt16)valueSlot;
    for (burstIndex = 1; burstIndex < 4; burstIndex++) {
        compiler_quickening_write_nop(&function->instructionsList[instructionIndex + burstIndex]);
    }
    return ZR_TRUE;
}

static TZrBool compiler_quickening_instruction_may_read_slot(const TZrInstruction *instruction, TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(NOP):
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
        case ZR_INSTRUCTION_ENUM(JUMP):
            return ZR_FALSE;
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
            return (TZrUInt32)instruction->instruction.operand.operand2[0] == slot;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS):
            return (TZrUInt32)instruction->instruction.operand.operand2[0] == slot;
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
            return instruction->instruction.operandExtra == slot;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
            return instruction->instruction.operandExtra == slot ||
                   instruction->instruction.operand.operand1[0] == slot;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
            return instruction->instruction.operandExtra == slot ||
                   instruction->instruction.operand.operand1[0] == slot;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
            return instruction->instruction.operandExtra == slot;
        case ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            return instruction->instruction.operand.operand1[0] == slot;
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_STRING):
        case ZR_INSTRUCTION_ENUM(TO_STRUCT):
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
        case ZR_INSTRUCTION_ENUM(NEG):
        case ZR_INSTRUCTION_ENUM(TYPEOF):
            return instruction->instruction.operand.operand1[0] == slot ||
                   instruction->instruction.operand.operand1[1] == slot;
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS):
            return instruction->instruction.operandExtra == slot ||
                   instruction->instruction.operand.operand1[0] == slot ||
                   instruction->instruction.operand.operand1[1] == slot;
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
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
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
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
            return instruction->instruction.operand.operand1[0] == slot ||
                   instruction->instruction.operand.operand1[1] == slot;
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST):
            return instruction->instruction.operand.operand1[0] == slot;
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
            return instruction->instruction.operand.operand0[0] == slot;
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
            return instruction->instruction.operand.operand0[0] == slot ||
                   instruction->instruction.operand.operand0[1] == slot;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4):
            return instruction->instruction.operand.operand1[0] == slot ||
                   instruction->instruction.operand.operand1[0] + 1u == slot ||
                   instruction->instruction.operand.operand1[0] + 2u == slot ||
                   instruction->instruction.operand.operand1[0] + 3u == slot ||
                   instruction->instruction.operand.operand1[1] == slot;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST):
            return instruction->instruction.operand.operand1[0] == slot ||
                   instruction->instruction.operand.operand1[0] + 1u == slot ||
                   instruction->instruction.operand.operand1[0] + 2u == slot ||
                   instruction->instruction.operand.operand1[0] + 3u == slot;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST):
            return instruction->instruction.operand.operand1[0] == slot ||
                   instruction->instruction.operand.operand1[0] + 1u == slot ||
                   instruction->instruction.operand.operand1[0] + 2u == slot ||
                   instruction->instruction.operand.operand1[0] + 3u == slot ||
                   instruction->instruction.operand.operand1[1] == slot;
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL): {
            /*
             * operand1[1] is the argument count; it must not be treated as a stack slot index.
             * Callee and arguments occupy slots operand1[0] through operand1[0] + operand1[1] inclusive.
             */
            TZrUInt32 callStart;
            TZrUInt32 argumentCount;

            callStart = (TZrUInt32)instruction->instruction.operand.operand1[0];
            argumentCount = (TZrUInt32)instruction->instruction.operand.operand1[1];
            if (slot >= callStart && slot <= callStart + argumentCount) {
                return ZR_TRUE;
            }
            return ZR_FALSE;
        }
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL):
        case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL):
            return slot >= (TZrUInt32)instruction->instruction.operandExtra;
        case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8):
            return slot == (TZrUInt32)instruction->instruction.operand.operand0[1] ||
                   slot == (TZrUInt32)instruction->instruction.operand.operand0[2] ||
                   slot >= (TZrUInt32)instruction->instruction.operandExtra;
        case ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED): {
            /*
             * After compiler_quicken_cached_calls, operand1[1] holds a callsite cache index, not a stack slot.
             * Without decoding the callsite cache entry, conservatively treat any slot at or above the callee base
             * as potentially read (callee plus stacked arguments).
             */
            TZrUInt32 calleeSlot;

            calleeSlot = (TZrUInt32)instruction->instruction.operand.operand1[0];
            return slot >= calleeSlot;
        }
        default:
            if (instruction->instruction.operandExtra == slot ||
                instruction->instruction.operand.operand1[0] == slot ||
                instruction->instruction.operand.operand1[1] == slot ||
                (TZrUInt32)instruction->instruction.operand.operand2[0] == slot) {
                return ZR_TRUE;
            }
            return ZR_FALSE;
    }
}

static TZrBool compiler_quickening_instruction_writes_slot(const TZrInstruction *instruction, TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK):
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(RESET_STACK_NULL):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(SETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER):
        case ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT):
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_STRING):
        case ZR_INSTRUCTION_ENUM(TO_STRUCT):
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
            return instruction->instruction.operandExtra == slot ||
                   instruction->instruction.operand.operand0[1] == slot ||
                   instruction->instruction.operand.operand0[2] == slot;
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
            return instruction->instruction.operandExtra == slot;
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
            return instruction->instruction.operandExtra == slot ||
                   instruction->instruction.operand.operand0[1] == slot;
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
        case ZR_INSTRUCTION_ENUM(NEG):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
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
        case ZR_INSTRUCTION_ENUM(ITER_INIT):
        case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
        case ZR_INSTRUCTION_ENUM(TYPEOF):
            return instruction->instruction.operandExtra == slot;
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_quickening_instruction_replace_read_slot_if_supported(TZrInstruction *instruction,
                                                                              TZrUInt32 oldSlot,
                                                                              TZrUInt32 newSlot,
                                                                              TZrBool applyChanges,
                                                                              TZrBool *outReplaced) {
    EZrInstructionCode opcode;
    TZrBool replaced = ZR_FALSE;

    if (outReplaced != ZR_NULL) {
        *outReplaced = ZR_FALSE;
    }

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(NOP):
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
            break;
        case ZR_INSTRUCTION_ENUM(GET_STACK):
            /*
             * SET_STACK is intentionally not a supported rewrite target here.
             * This pass forwards a copied temp to its readers, but SET_STACK
             * materializes a temporary result and may transfer ownership from
             * its source slot. Replacing GET_STACK temp -> SET_STACK dst,temp
             * with SET_STACK dst,source changes copy semantics for ownership
             * values and can consume the original local.
             */
            if ((TZrUInt32)instruction->instruction.operand.operand2[0] == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operand.operand2[0] = (TZrInt32)newSlot;
                }
                replaced = ZR_TRUE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS):
            if ((TZrUInt32)instruction->instruction.operand.operand2[0] == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operand.operand2[0] = (TZrInt32)newSlot;
                }
                replaced = ZR_TRUE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
            if (instruction->instruction.operandExtra == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operandExtra = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
            if (instruction->instruction.operandExtra == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operandExtra = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            if (instruction->instruction.operand.operand1[0] == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operand.operand1[0] = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
            if (instruction->instruction.operandExtra == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operandExtra = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            if (instruction->instruction.operand.operand1[0] == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operand.operand1[0] = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
            if (instruction->instruction.operandExtra == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operandExtra = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            if (instruction->instruction.operand.operand1[0] == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operand.operand1[0] = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_STRING):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
            if (instruction->instruction.operand.operand1[0] == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operand.operand1[0] = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
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
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
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
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT):
            if (instruction->instruction.operand.operand1[0] == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operand.operand1[0] = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            if (instruction->instruction.operand.operand1[1] == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operand.operand1[1] = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS):
            if (instruction->instruction.operandExtra == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operandExtra = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            if (instruction->instruction.operand.operand1[0] == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operand.operand1[0] = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            if (instruction->instruction.operand.operand1[1] == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operand.operand1[1] = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
            if (instruction->instruction.operand.operand0[0] == oldSlot) {
                if (newSlot > UINT8_MAX) {
                    return ZR_FALSE;
                }
                if (applyChanges) {
                    instruction->instruction.operand.operand0[0] = (TZrUInt8)newSlot;
                }
                replaced = ZR_TRUE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST):
            if (instruction->instruction.operand.operand1[0] == oldSlot) {
                if (applyChanges) {
                    instruction->instruction.operand.operand1[0] = (TZrUInt16)newSlot;
                }
                replaced = ZR_TRUE;
            }
            break;
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
            if (instruction->instruction.operand.operand0[0] == oldSlot) {
                if (newSlot > UINT8_MAX) {
                    return ZR_FALSE;
                }
                if (applyChanges) {
                    instruction->instruction.operand.operand0[0] = (TZrUInt8)newSlot;
                }
                replaced = ZR_TRUE;
            }
            if (instruction->instruction.operand.operand0[1] == oldSlot) {
                if (newSlot > UINT8_MAX) {
                    return ZR_FALSE;
                }
                if (applyChanges) {
                    instruction->instruction.operand.operand0[1] = (TZrUInt8)newSlot;
                }
                replaced = ZR_TRUE;
            }
            break;
        default:
            if (compiler_quickening_instruction_may_read_slot(instruction, oldSlot)) {
                return ZR_FALSE;
            }
            break;
    }

    if (outReplaced != ZR_NULL) {
        *outReplaced = replaced;
    }
    return ZR_TRUE;
}

static TZrUInt32 compiler_quickening_find_block_end_index(const SZrFunction *function,
                                                          const TZrBool *blockStarts,
                                                          TZrUInt32 instructionIndex) {
    TZrUInt32 scan;

    if (function == ZR_NULL || blockStarts == ZR_NULL || function->instructionsList == ZR_NULL ||
        instructionIndex >= function->instructionsLength) {
        return 0;
    }

    for (scan = instructionIndex + 1; scan < function->instructionsLength; scan++) {
        if (blockStarts[scan]) {
            return scan - 1;
        }
    }

    return function->instructionsLength - 1;
}

static TZrBool compiler_quickening_block_ends_without_fallthrough(const TZrInstruction *instruction) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP):
        case ZR_INSTRUCTION_ENUM(THROW):
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_quickening_slot_is_overwritten_before_any_read_linear(const SZrFunction *function,
                                                                              TZrUInt32 instructionIndex,
                                                                              TZrUInt32 slot) {
    TZrUInt32 scan;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (scan = instructionIndex; scan < function->instructionsLength; scan++) {
        const TZrInstruction *instruction = &function->instructionsList[scan];

        if (compiler_quickening_instruction_may_read_slot(instruction, slot)) {
            return ZR_FALSE;
        }
        if (compiler_quickening_instruction_writes_slot(instruction, slot)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

typedef enum EZrQuickeningSlotPathState {
    ZR_QUICKENING_SLOT_PATH_UNKNOWN = 0,
    ZR_QUICKENING_SLOT_PATH_VISITING = 1,
    ZR_QUICKENING_SLOT_PATH_DEAD_BEFORE_READ = 2,
    ZR_QUICKENING_SLOT_PATH_READ_BEFORE_DEAD = 3
} EZrQuickeningSlotPathState;

static void compiler_quickening_append_unique_successor_index(const SZrFunction *function,
                                                              TZrUInt32 candidateIndex,
                                                              TZrUInt32 successorIndices[2],
                                                              TZrUInt32 *successorCount) {
    if (function == ZR_NULL || successorIndices == ZR_NULL || successorCount == ZR_NULL ||
        candidateIndex >= function->instructionsLength || *successorCount >= 2) {
        return;
    }

    for (TZrUInt32 index = 0; index < *successorCount; index++) {
        if (successorIndices[index] == candidateIndex) {
            return;
        }
    }

    successorIndices[*successorCount] = candidateIndex;
    (*successorCount)++;
}

static TZrUInt32 compiler_quickening_collect_block_successor_indices(const SZrFunction *function,
                                                                     TZrUInt32 blockEndIndex,
                                                                     TZrUInt32 successorIndices[2]) {
    const TZrInstruction *instruction;
    EZrInstructionCode opcode;
    TZrUInt32 successorCount = 0;
    TZrUInt32 fallthroughIndex;
    TZrInt64 targetIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || successorIndices == ZR_NULL ||
        blockEndIndex >= function->instructionsLength) {
        return 0;
    }

    instruction = &function->instructionsList[blockEndIndex];
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    fallthroughIndex = blockEndIndex + 1;

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(JUMP):
            targetIndex = (TZrInt64)blockEndIndex + 1 + instruction->instruction.operand.operand2[0];
            if (targetIndex >= 0) {
                compiler_quickening_append_unique_successor_index(function,
                                                                  (TZrUInt32)targetIndex,
                                                                  successorIndices,
                                                                  &successorCount);
            }
            return successorCount;
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
            targetIndex = (TZrInt64)blockEndIndex + 1 + instruction->instruction.operand.operand2[0];
            if (targetIndex >= 0) {
                compiler_quickening_append_unique_successor_index(function,
                                                                  (TZrUInt32)targetIndex,
                                                                  successorIndices,
                                                                  &successorCount);
            }
            compiler_quickening_append_unique_successor_index(function,
                                                              fallthroughIndex,
                                                              successorIndices,
                                                              &successorCount);
            return successorCount;
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
            targetIndex = (TZrInt64)blockEndIndex + 1 + (TZrInt16)instruction->instruction.operand.operand1[1];
            if (targetIndex >= 0) {
                compiler_quickening_append_unique_successor_index(function,
                                                                  (TZrUInt32)targetIndex,
                                                                  successorIndices,
                                                                  &successorCount);
            }
            compiler_quickening_append_unique_successor_index(function,
                                                              fallthroughIndex,
                                                              successorIndices,
                                                              &successorCount);
            return successorCount;
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE):
            targetIndex = (TZrInt64)blockEndIndex + 1 + (TZrInt16)instruction->instruction.operand.operand1[1];
            if (targetIndex >= 0) {
                compiler_quickening_append_unique_successor_index(function,
                                                                  (TZrUInt32)targetIndex,
                                                                  successorIndices,
                                                                  &successorCount);
            }
            compiler_quickening_append_unique_successor_index(function,
                                                              fallthroughIndex,
                                                              successorIndices,
                                                              &successorCount);
            return successorCount;
        case ZR_INSTRUCTION_ENUM(THROW):
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
        case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
        case ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS):
        case ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED):
        case ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED):
            return 0;
        default:
            compiler_quickening_append_unique_successor_index(function,
                                                              fallthroughIndex,
                                                              successorIndices,
                                                              &successorCount);
            return successorCount;
    }
}

static TZrBool compiler_quickening_slot_is_overwritten_before_read_from_block_cfg(const SZrFunction *function,
                                                                                  const TZrBool *blockStarts,
                                                                                  TZrUInt32 blockStartIndex,
                                                                                  TZrUInt32 slot,
                                                                                  TZrUInt8 *pathStates) {
    TZrUInt32 blockEndIndex;
    TZrUInt32 successorIndices[2];
    TZrUInt32 successorCount;
    TZrUInt32 scan;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL || pathStates == ZR_NULL ||
        blockStartIndex >= function->instructionsLength || !blockStarts[blockStartIndex]) {
        return ZR_FALSE;
    }

    if (compiler_quickening_find_active_typed_local_binding(function, slot, blockStartIndex) != ZR_NULL) {
        return ZR_FALSE;
    }

    if (pathStates[blockStartIndex] == ZR_QUICKENING_SLOT_PATH_VISITING) {
        return ZR_TRUE;
    }
    if (pathStates[blockStartIndex] == ZR_QUICKENING_SLOT_PATH_DEAD_BEFORE_READ) {
        return ZR_TRUE;
    }
    if (pathStates[blockStartIndex] == ZR_QUICKENING_SLOT_PATH_READ_BEFORE_DEAD) {
        return ZR_FALSE;
    }

    pathStates[blockStartIndex] = ZR_QUICKENING_SLOT_PATH_VISITING;
    blockEndIndex = compiler_quickening_find_block_end_index(function, blockStarts, blockStartIndex);
    for (scan = blockStartIndex; scan <= blockEndIndex; scan++) {
        const TZrInstruction *instruction = &function->instructionsList[scan];

        if (compiler_quickening_instruction_may_read_slot(instruction, slot)) {
            pathStates[blockStartIndex] = ZR_QUICKENING_SLOT_PATH_READ_BEFORE_DEAD;
            return ZR_FALSE;
        }
        if (compiler_quickening_instruction_writes_slot(instruction, slot)) {
            pathStates[blockStartIndex] = ZR_QUICKENING_SLOT_PATH_DEAD_BEFORE_READ;
            return ZR_TRUE;
        }
    }

    successorCount = compiler_quickening_collect_block_successor_indices(function, blockEndIndex, successorIndices);
    for (scan = 0; scan < successorCount; scan++) {
        if (!compiler_quickening_slot_is_overwritten_before_read_from_block_cfg(function,
                                                                                blockStarts,
                                                                                successorIndices[scan],
                                                                                slot,
                                                                                pathStates)) {
            pathStates[blockStartIndex] = ZR_QUICKENING_SLOT_PATH_READ_BEFORE_DEAD;
            return ZR_FALSE;
        }
    }

    pathStates[blockStartIndex] = ZR_QUICKENING_SLOT_PATH_DEAD_BEFORE_READ;
    return ZR_TRUE;
}

static TZrBool compiler_quickening_temp_slot_is_dead_after_instruction_cfg(const SZrFunction *function,
                                                                           const TZrBool *blockStarts,
                                                                           TZrUInt32 instructionIndex,
                                                                           TZrUInt32 slot) {
    TZrUInt32 blockEndIndex;
    TZrUInt32 successorIndices[2];
    TZrUInt32 successorCount;
    TZrUInt8 *pathStates = ZR_NULL;
    TZrBool success = ZR_FALSE;
    TZrUInt32 scan;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    if (instructionIndex + 1 >= function->instructionsLength) {
        return ZR_TRUE;
    }

    if (compiler_quickening_find_active_typed_local_binding(function, slot, instructionIndex + 1) != ZR_NULL) {
        return ZR_FALSE;
    }

    blockEndIndex = compiler_quickening_find_block_end_index(function, blockStarts, instructionIndex);
    for (scan = instructionIndex + 1; scan <= blockEndIndex; scan++) {
        const TZrInstruction *instruction = &function->instructionsList[scan];

        if (compiler_quickening_instruction_may_read_slot(instruction, slot)) {
            return ZR_FALSE;
        }
        if (compiler_quickening_instruction_writes_slot(instruction, slot)) {
            return ZR_TRUE;
        }
    }

    successorCount = compiler_quickening_collect_block_successor_indices(function, blockEndIndex, successorIndices);
    if (successorCount == 0) {
        return ZR_TRUE;
    }

    pathStates = (TZrUInt8 *)calloc(function->instructionsLength, sizeof(*pathStates));
    if (pathStates == ZR_NULL) {
        return ZR_FALSE;
    }

    success = ZR_TRUE;
    for (scan = 0; scan < successorCount; scan++) {
        if (!compiler_quickening_slot_is_overwritten_before_read_from_block_cfg(function,
                                                                                blockStarts,
                                                                                successorIndices[scan],
                                                                                slot,
                                                                                pathStates)) {
            success = ZR_FALSE;
            break;
        }
    }

    free(pathStates);
    return success;
}

static TZrBool compiler_quickening_temp_slot_is_dead_until_block_end_with_terminal_exit(
        const SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex,
        TZrUInt32 slot) {
    TZrUInt32 blockEndIndex;
    TZrUInt32 scan;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    if (compiler_quickening_find_active_typed_local_binding(function, slot, instructionIndex) != ZR_NULL) {
        return ZR_FALSE;
    }

    blockEndIndex = compiler_quickening_find_block_end_index(function, blockStarts, instructionIndex);
    for (scan = instructionIndex; scan <= blockEndIndex; scan++) {
        if (compiler_quickening_instruction_may_read_slot(&function->instructionsList[scan], slot)) {
            return ZR_FALSE;
        }
    }

    return compiler_quickening_block_ends_without_fallthrough(&function->instructionsList[blockEndIndex]);
}

static TZrBool compiler_quickening_slot_is_overwritten_before_read(const SZrFunction *function,
                                                                   const TZrBool *blockStarts,
                                                                   TZrUInt32 instructionIndex,
                                                                   TZrUInt32 slot) {
    TZrUInt32 scan;

    if (function == ZR_NULL || blockStarts == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (scan = instructionIndex; scan < function->instructionsLength; scan++) {
        const TZrInstruction *instruction = &function->instructionsList[scan];

        if (scan > instructionIndex && blockStarts[scan]) {
            return ZR_FALSE;
        }
        if (compiler_quickening_instruction_may_read_slot(instruction, slot)) {
            return ZR_FALSE;
        }
        if (compiler_quickening_instruction_writes_slot(instruction, slot)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool compiler_quickening_temp_slot_is_dead_after_instruction(const SZrFunction *function,
                                                                       const TZrBool *blockStarts,
                                                                       TZrUInt32 instructionIndex,
                                                                       TZrUInt32 slot) {
    if (function == ZR_NULL || blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (instructionIndex + 1 >= function->instructionsLength) {
        return ZR_TRUE;
    }

    if (compiler_quickening_slot_is_overwritten_before_read(function, blockStarts, instructionIndex + 1, slot)) {
        return ZR_TRUE;
    }

    return compiler_quickening_temp_slot_is_dead_until_block_end_with_terminal_exit(function,
                                                                                     blockStarts,
                                                                                     instructionIndex + 1,
                                                                                     slot);
}

static TZrBool compiler_quickening_opcode_supports_direct_result_store(const SZrFunction *function,
                                                                       const TZrInstruction *instruction) {
    EZrInstructionCode opcode;

    if (function == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            return compiler_quickening_function_constant_is_plain_primitive(
                    function,
                    (TZrUInt32)instruction->instruction.operand.operand2[0]);
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
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
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_quickening_try_fold_direct_result_store(SZrFunction *function,
                                                                const TZrBool *blockStarts,
                                                                TZrUInt32 instructionIndex) {
    TZrInstruction *producerInstruction;
    TZrInstruction *storeInstruction;
    TZrUInt32 temporarySlot;
    TZrUInt32 destinationSlot;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex + 1 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    producerInstruction = &function->instructionsList[instructionIndex];
    storeInstruction = &function->instructionsList[instructionIndex + 1];
    if ((EZrInstructionCode)storeInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
        blockStarts[instructionIndex + 1] ||
        !compiler_quickening_opcode_supports_direct_result_store(function, producerInstruction)) {
        return ZR_FALSE;
    }

    temporarySlot = producerInstruction->instruction.operandExtra;
    destinationSlot = storeInstruction->instruction.operandExtra;
    if ((TZrUInt32)storeInstruction->instruction.operand.operand2[0] != temporarySlot ||
        temporarySlot == destinationSlot) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_temp_slot_is_dead_after_instruction_cfg(function,
                                                                     blockStarts,
                                                                     instructionIndex + 1,
                                                                     temporarySlot)) {
        return ZR_FALSE;
    }

    producerInstruction->instruction.operandExtra = (TZrUInt16)destinationSlot;
    compiler_quickening_write_nop(storeInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_forward_get_stack_copy_reads(SZrFunction *function,
                                                                    const TZrBool *blockStarts,
                                                                    TZrUInt32 instructionIndex) {
    TZrInstruction *copyInstruction;
    TZrUInt32 sourceSlot;
    TZrUInt32 tempSlot;
    TZrUInt32 blockEndIndex;
    TZrUInt32 stopIndex = instructionIndex;
    TZrUInt32 scan;
    TZrBool replacedAny = ZR_FALSE;
    TZrBool stoppedBeforeCurrent = ZR_FALSE;
    TZrBool tempValueDiesByCurrentRewrite = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex + 1 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    copyInstruction = &function->instructionsList[instructionIndex];
    if ((EZrInstructionCode)copyInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK)) {
        return ZR_FALSE;
    }

    sourceSlot = (TZrUInt32)copyInstruction->instruction.operand.operand2[0];
    tempSlot = copyInstruction->instruction.operandExtra;
    if (sourceSlot == tempSlot ||
        compiler_quickening_find_active_typed_local_binding(function, tempSlot, instructionIndex + 1) != ZR_NULL) {
        return ZR_FALSE;
    }

    blockEndIndex = compiler_quickening_find_block_end_index(function, blockStarts, instructionIndex);
    for (scan = instructionIndex + 1; scan <= blockEndIndex; scan++) {
        TZrInstruction *instruction = &function->instructionsList[scan];
        TZrBool readsTemp = compiler_quickening_instruction_may_read_slot(instruction, tempSlot);
        TZrBool writesSource = compiler_quickening_instruction_writes_slot(instruction, sourceSlot);
        TZrBool writesTemp = compiler_quickening_instruction_writes_slot(instruction, tempSlot);
        TZrBool replacedThis = ZR_FALSE;

        if (!compiler_quickening_instruction_replace_read_slot_if_supported(
                    instruction,
                    tempSlot,
                    sourceSlot,
                    ZR_FALSE,
                    &replacedThis)) {
            return ZR_FALSE;
        }

        if (replacedThis) {
            replacedAny = ZR_TRUE;
            stopIndex = scan;
        }

        if (writesSource || writesTemp) {
            if ((writesSource || writesTemp) && !(readsTemp && replacedThis)) {
                stoppedBeforeCurrent = ZR_TRUE;
            } else {
                stopIndex = scan;
                tempValueDiesByCurrentRewrite = writesTemp;
            }
            break;
        }
    }

    if (!replacedAny || (stoppedBeforeCurrent && stopIndex == instructionIndex) ||
        (!tempValueDiesByCurrentRewrite &&
         !compiler_quickening_temp_slot_is_dead_after_instruction(function, blockStarts, stopIndex, tempSlot) &&
         !compiler_quickening_temp_slot_is_dead_after_instruction_cfg(function, blockStarts, stopIndex, tempSlot))) {
        return ZR_FALSE;
    }

    for (scan = instructionIndex + 1; scan <= stopIndex; scan++) {
        TZrBool replacedThis = ZR_FALSE;

        if (!compiler_quickening_instruction_replace_read_slot_if_supported(&function->instructionsList[scan],
                                                                            tempSlot,
                                                                            sourceSlot,
                                                                            ZR_TRUE,
                                                                            &replacedThis)) {
            return ZR_FALSE;
        }
    }

    compiler_quickening_write_nop(copyInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fold_stack_self_update_int_const(SZrFunction *function,
                                                                        const TZrBool *blockStarts,
                                                                        TZrUInt32 instructionIndex) {
    TZrInstruction *loadInstruction;
    TZrInstruction *arithmeticInstruction;
    TZrInstruction *storeInstruction;
    EZrInstructionCode arithmeticOpcode;
    TZrUInt32 sourceSlot;
    TZrUInt32 loadTempSlot;
    TZrUInt32 arithmeticTempSlot;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex + 2 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    loadInstruction = &function->instructionsList[instructionIndex];
    arithmeticInstruction = &function->instructionsList[instructionIndex + 1];
    storeInstruction = &function->instructionsList[instructionIndex + 2];
    arithmeticOpcode = (EZrInstructionCode)arithmeticInstruction->instruction.operationCode;
    if ((EZrInstructionCode)loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        ((EZrInstructionCode)storeInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK)) ||
        (arithmeticOpcode != ZR_INSTRUCTION_ENUM(ADD_INT_CONST) &&
         arithmeticOpcode != ZR_INSTRUCTION_ENUM(SUB_INT_CONST) &&
         arithmeticOpcode != ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST) &&
         arithmeticOpcode != ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST) &&
         arithmeticOpcode != ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST) &&
         arithmeticOpcode != ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST)) ||
        blockStarts[instructionIndex + 1] ||
        blockStarts[instructionIndex + 2]) {
        return ZR_FALSE;
    }

    sourceSlot = (TZrUInt32)loadInstruction->instruction.operand.operand2[0];
    loadTempSlot = loadInstruction->instruction.operandExtra;
    arithmeticTempSlot = arithmeticInstruction->instruction.operandExtra;
    if ((TZrUInt32)arithmeticInstruction->instruction.operand.operand1[0] != loadTempSlot ||
        (TZrUInt32)storeInstruction->instruction.operand.operand2[0] != arithmeticTempSlot ||
        storeInstruction->instruction.operandExtra != sourceSlot ||
        sourceSlot == loadTempSlot ||
        sourceSlot == arithmeticTempSlot) {
        return ZR_FALSE;
    }

    /*
     * This rewrite only bypasses a redundant GET_STACK/SET_STACK pair around an
     * existing *_INT_CONST op. The source slot type may be proven in a
     * predecessor block (for example loop counters initialized in a preheader),
     * so requiring an in-block int proof leaves valid self-updates behind
     * without improving safety.
     */
    if (!compiler_quickening_temp_slot_is_dead_after_instruction(function,
                                                                 blockStarts,
                                                                 instructionIndex + 1,
                                                                 loadTempSlot) ||
        !compiler_quickening_temp_slot_is_dead_after_instruction(function,
                                                                 blockStarts,
                                                                 instructionIndex + 2,
                                                                 arithmeticTempSlot)) {
        return ZR_FALSE;
    }

    arithmeticInstruction->instruction.operand.operand1[0] = (TZrUInt16)sourceSlot;
    arithmeticInstruction->instruction.operandExtra = (TZrUInt16)sourceSlot;
    compiler_quickening_write_nop(loadInstruction);
    compiler_quickening_write_nop(storeInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_fold_direct_result_stores(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index + 1 < function->instructionsLength; index++) {
        compiler_quickening_try_fold_direct_result_store(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_forward_get_stack_copy_reads(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index + 1 < function->instructionsLength; index++) {
        compiler_quickening_try_forward_get_stack_copy_reads(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_try_fuse_jump_if_greater_signed(SZrFunction *function,
                                                                   const TZrBool *blockStarts,
                                                                   TZrUInt32 instructionIndex) {
    TZrInstruction *compareInstruction;
    TZrInstruction *jumpIfInstruction;
    EZrInstructionCode compareOpcode;
    TZrUInt32 compareResultSlot;
    TZrInt64 fusedOffset;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex + 1 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    compareInstruction = &function->instructionsList[instructionIndex];
    jumpIfInstruction = &function->instructionsList[instructionIndex + 1];
    compareOpcode = (EZrInstructionCode)compareInstruction->instruction.operationCode;
    if ((compareOpcode != ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED) &&
         compareOpcode != ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED)) ||
        (EZrInstructionCode)jumpIfInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        blockStarts[instructionIndex + 1]) {
        return ZR_FALSE;
    }

    compareResultSlot = compareInstruction->instruction.operandExtra;
    if (jumpIfInstruction->instruction.operandExtra != compareResultSlot) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_temp_slot_is_dead_after_instruction_cfg(function,
                                                                     blockStarts,
                                                                     instructionIndex + 1,
                                                                     compareResultSlot)) {
        return ZR_FALSE;
    }

    fusedOffset = (TZrInt64)jumpIfInstruction->instruction.operand.operand2[0] + 1;
    if (fusedOffset < INT16_MIN || fusedOffset > INT16_MAX) {
        return ZR_FALSE;
    }

    compareInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED);
    if (compareOpcode == ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED)) {
        compareInstruction->instruction.operandExtra = compareInstruction->instruction.operand.operand1[0];
        compareInstruction->instruction.operand.operand1[0] = compareInstruction->instruction.operand.operand1[1];
    } else {
        compareInstruction->instruction.operandExtra = compareInstruction->instruction.operand.operand1[1];
    }
    compareInstruction->instruction.operand.operand1[1] = (TZrUInt16)((TZrInt16)fusedOffset);
    compiler_quickening_write_nop(jumpIfInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_fuse_jump_if_greater_signed(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index + 1 < function->instructionsLength; index++) {
        compiler_quickening_try_fuse_jump_if_greater_signed(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_try_fuse_jump_if_not_equal_signed_const(SZrFunction *function,
                                                                           const TZrBool *blockStarts,
                                                                           TZrUInt32 instructionIndex) {
    TZrInstruction *compareInstruction;
    TZrInstruction *jumpIfInstruction;
    TZrUInt32 compareResultSlot;
    TZrInt64 fusedOffset;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex + 1 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    compareInstruction = &function->instructionsList[instructionIndex];
    jumpIfInstruction = &function->instructionsList[instructionIndex + 1];
    if ((EZrInstructionCode)compareInstruction->instruction.operationCode !=
                ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST) ||
        (EZrInstructionCode)jumpIfInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        blockStarts[instructionIndex + 1]) {
        return ZR_FALSE;
    }

    compareResultSlot = compareInstruction->instruction.operandExtra;
    if (compareResultSlot == ZR_INSTRUCTION_USE_RET_FLAG || jumpIfInstruction->instruction.operandExtra != compareResultSlot) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_temp_slot_is_dead_after_instruction_cfg(function,
                                                                     blockStarts,
                                                                     instructionIndex + 1,
                                                                     compareResultSlot)) {
        return ZR_FALSE;
    }

    fusedOffset = (TZrInt64)jumpIfInstruction->instruction.operand.operand2[0] + 1;
    if (fusedOffset < INT16_MIN || fusedOffset > INT16_MAX) {
        return ZR_FALSE;
    }

    compareInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST);
    compareInstruction->instruction.operandExtra = compareInstruction->instruction.operand.operand1[0];
    compareInstruction->instruction.operand.operand1[0] = compareInstruction->instruction.operand.operand1[1];
    compareInstruction->instruction.operand.operand1[1] = (TZrUInt16)((TZrInt16)fusedOffset);
    compiler_quickening_write_nop(jumpIfInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fuse_jump_if_not_equal_signed(SZrFunction *function,
                                                                     const TZrBool *blockStarts,
                                                                     TZrUInt32 instructionIndex) {
    TZrInstruction *compareInstruction;
    TZrInstruction *jumpIfInstruction;
    TZrUInt32 compareResultSlot;
    TZrInt64 fusedOffset;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex + 1 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    compareInstruction = &function->instructionsList[instructionIndex];
    jumpIfInstruction = &function->instructionsList[instructionIndex + 1];
    if ((EZrInstructionCode)compareInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED) ||
        (EZrInstructionCode)jumpIfInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        blockStarts[instructionIndex + 1]) {
        return ZR_FALSE;
    }

    compareResultSlot = compareInstruction->instruction.operandExtra;
    if (compareResultSlot == ZR_INSTRUCTION_USE_RET_FLAG || jumpIfInstruction->instruction.operandExtra != compareResultSlot) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_temp_slot_is_dead_after_instruction_cfg(function,
                                                                     blockStarts,
                                                                     instructionIndex + 1,
                                                                     compareResultSlot)) {
        return ZR_FALSE;
    }

    fusedOffset = (TZrInt64)jumpIfInstruction->instruction.operand.operand2[0] + 1;
    if (fusedOffset < INT16_MIN || fusedOffset > INT16_MAX) {
        return ZR_FALSE;
    }

    compareInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED);
    compareInstruction->instruction.operandExtra = compareInstruction->instruction.operand.operand1[0];
    compareInstruction->instruction.operand.operand1[0] = compareInstruction->instruction.operand.operand1[1];
    compareInstruction->instruction.operand.operand1[1] = (TZrUInt16)((TZrInt16)fusedOffset);
    compiler_quickening_write_nop(jumpIfInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_fuse_jump_if_not_equal_signed(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index + 1 < function->instructionsLength; index++) {
        compiler_quickening_try_fuse_jump_if_not_equal_signed(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_fuse_jump_if_not_equal_signed_const(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index + 1 < function->instructionsLength; index++) {
        compiler_quickening_try_fuse_jump_if_not_equal_signed_const(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_try_fold_right_converted_uint_const_arithmetic(
        SZrState *state,
        SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex) {
    TZrInstruction *instruction;
    TZrInstruction *conversionInstruction;
    TZrInstruction *constantInstruction;
    EZrInstructionCode opcode;
    EZrInstructionCode constOpcode;
    TZrUInt32 convertedSlot;
    TZrUInt32 constantSlot;
    TZrUInt32 uintConstantIndex;
    TZrUInt32 sourceConstantIndex;
    TZrUInt64 uintConstantValue;

    if (state == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex < 2 || instructionIndex >= function->instructionsLength || blockStarts[instructionIndex] ||
        blockStarts[instructionIndex - 1]) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[instructionIndex];
    conversionInstruction = &function->instructionsList[instructionIndex - 1];
    constantInstruction = &function->instructionsList[instructionIndex - 2];
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    constOpcode = compiler_quickening_specialized_arithmetic_const_opcode(opcode);
    if ((EZrInstructionCode)conversionInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(TO_UINT) ||
        (EZrInstructionCode)constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        constOpcode == ZR_INSTRUCTION_ENUM(ENUM_MAX) ||
        (opcode != ZR_INSTRUCTION_ENUM(ADD_UNSIGNED) &&
         opcode != ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST) &&
         opcode != ZR_INSTRUCTION_ENUM(SUB_UNSIGNED) &&
         opcode != ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST) &&
         opcode != ZR_INSTRUCTION_ENUM(MUL_UNSIGNED) &&
         opcode != ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST) &&
         opcode != ZR_INSTRUCTION_ENUM(DIV_UNSIGNED) &&
         opcode != ZR_INSTRUCTION_ENUM(MOD_UNSIGNED))) {
        return ZR_FALSE;
    }

    convertedSlot = conversionInstruction->instruction.operandExtra;
    constantSlot = (TZrUInt32)conversionInstruction->instruction.operand.operand1[0];
    if (instruction->instruction.operand.operand1[1] != convertedSlot ||
        constantInstruction->instruction.operandExtra != constantSlot ||
        instruction->instruction.operand.operand1[0] == convertedSlot) {
        return ZR_FALSE;
    }

    sourceConstantIndex = (TZrUInt32)constantInstruction->instruction.operand.operand2[0];
    if (!compiler_quickening_function_constant_read_uint64(function, sourceConstantIndex, &uintConstantValue) ||
        !compiler_quickening_find_or_append_uint_constant(state,
                                                          function,
                                                          uintConstantValue,
                                                          &uintConstantIndex) ||
        uintConstantIndex > UINT16_MAX) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_temp_slot_is_dead_after_instruction(function, blockStarts, instructionIndex, convertedSlot) ||
        !compiler_quickening_temp_slot_is_dead_after_instruction(function, blockStarts, instructionIndex, constantSlot)) {
        return ZR_FALSE;
    }

    instruction->instruction.operationCode = (TZrUInt16)constOpcode;
    instruction->instruction.operand.operand1[1] = (TZrUInt16)uintConstantIndex;
    compiler_quickening_write_nop(conversionInstruction);
    compiler_quickening_write_nop(constantInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fold_right_const_arithmetic(SZrState *state,
                                                                   SZrFunction *function,
                                                                   const TZrBool *blockStarts,
                                                                   TZrUInt32 instructionIndex) {
    TZrInstruction *instruction;
    TZrInstruction *constantInstruction;
    EZrInstructionCode opcode;
    EZrInstructionCode constOpcode;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;
    TZrUInt32 constantSlot;
    TZrInt32 constantIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex == 0 || instructionIndex >= function->instructionsLength || blockStarts[instructionIndex]) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[instructionIndex];
    constantInstruction = &function->instructionsList[instructionIndex - 1];
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    constOpcode = compiler_quickening_specialized_arithmetic_const_opcode(opcode);
    if (constOpcode == ZR_INSTRUCTION_ENUM(ENUM_MAX) ||
        (EZrInstructionCode)constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
        return compiler_quickening_try_fold_right_converted_uint_const_arithmetic(state,
                                                                                  function,
                                                                                  blockStarts,
                                                                                  instructionIndex);
    }

    constantIndex = constantInstruction->instruction.operand.operand2[0];
    if (constantIndex < 0 || constantIndex > UINT16_MAX ||
        !compiler_quickening_constant_matches_arithmetic_opcode(function,
                                                                opcode,
                                                                (TZrUInt32)constantIndex)) {
        return compiler_quickening_try_fold_right_converted_uint_const_arithmetic(state,
                                                                                  function,
                                                                                  blockStarts,
                                                                                  instructionIndex);
    }

    leftSlot = instruction->instruction.operand.operand1[0];
    rightSlot = instruction->instruction.operand.operand1[1];
    constantSlot = constantInstruction->instruction.operandExtra;
    if (rightSlot != constantSlot || leftSlot == constantSlot) {
        return compiler_quickening_try_fold_right_converted_uint_const_arithmetic(state,
                                                                                  function,
                                                                                  blockStarts,
                                                                                  instructionIndex);
    }

    if (instruction->instruction.operandExtra != constantSlot) {
        if (!compiler_quickening_temp_slot_is_dead_after_instruction(function,
                                                                     blockStarts,
                                                                     instructionIndex,
                                                                     constantSlot) &&
            !compiler_quickening_temp_slot_is_dead_after_instruction_cfg(function,
                                                                         blockStarts,
                                                                         instructionIndex,
                                                                         constantSlot)) {
            return compiler_quickening_try_fold_right_converted_uint_const_arithmetic(state,
                                                                                      function,
                                                                                      blockStarts,
                                                                                      instructionIndex);
        }
    }

    instruction->instruction.operationCode = (TZrUInt16)constOpcode;
    instruction->instruction.operand.operand1[1] = (TZrUInt16)constantIndex;
    compiler_quickening_write_nop(constantInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_opcode_supports_left_const_commutative_fold(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static EZrInstructionCode compiler_quickening_plain_dest_const_opcode_to_reuse_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(ADD_INT_CONST);
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST);
        default:
            return opcode;
    }
}

static TZrBool compiler_quickening_try_fold_left_const_commutative_arithmetic(
        SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex) {
    TZrInstruction *instruction;
    TZrInstruction *constantInstruction;
    EZrInstructionCode opcode;
    EZrInstructionCode constOpcode;
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;
    TZrUInt32 constantSlot;
    TZrInt32 constantIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex == 0 || instructionIndex >= function->instructionsLength || blockStarts[instructionIndex]) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[instructionIndex];
    constantInstruction = &function->instructionsList[instructionIndex - 1];
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    constOpcode = compiler_quickening_specialized_arithmetic_const_opcode(opcode);
    if (!compiler_quickening_opcode_supports_left_const_commutative_fold(opcode) ||
        constOpcode == ZR_INSTRUCTION_ENUM(ENUM_MAX) ||
        (EZrInstructionCode)constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
        return ZR_FALSE;
    }

    constantIndex = constantInstruction->instruction.operand.operand2[0];
    if (constantIndex < 0 || constantIndex > UINT16_MAX ||
        !compiler_quickening_constant_matches_arithmetic_opcode(function,
                                                                opcode,
                                                                (TZrUInt32)constantIndex)) {
        return ZR_FALSE;
    }

    leftSlot = instruction->instruction.operand.operand1[0];
    rightSlot = instruction->instruction.operand.operand1[1];
    constantSlot = constantInstruction->instruction.operandExtra;
    if (leftSlot != constantSlot || rightSlot == constantSlot) {
        return ZR_FALSE;
    }

    if (instruction->instruction.operandExtra != constantSlot) {
        if (!compiler_quickening_temp_slot_is_dead_after_instruction(function,
                                                                     blockStarts,
                                                                     instructionIndex,
                                                                     constantSlot) &&
            !compiler_quickening_temp_slot_is_dead_after_instruction_cfg(function,
                                                                         blockStarts,
                                                                         instructionIndex,
                                                                         constantSlot)) {
            return ZR_FALSE;
        }
    }

    if (instruction->instruction.operandExtra == constantSlot) {
        constOpcode = compiler_quickening_plain_dest_const_opcode_to_reuse_opcode(constOpcode);
    }

    instruction->instruction.operationCode = (TZrUInt16)constOpcode;
    instruction->instruction.operand.operand1[0] = (TZrUInt16)rightSlot;
    instruction->instruction.operand.operand1[1] = (TZrUInt16)constantIndex;
    compiler_quickening_write_nop(constantInstruction);
    return ZR_TRUE;
}

static EZrInstructionCode compiler_quickening_signed_load_const_arithmetic_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST);
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST);
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST);
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
            return ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST);
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
            return ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST);
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

static EZrInstructionCode compiler_quickening_signed_load_stack_const_arithmetic_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST);
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST);
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST);
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST);
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST);
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

static EZrInstructionCode compiler_quickening_signed_load_stack_load_const_arithmetic_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
            return ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST);
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

static EZrInstructionCode compiler_quickening_signed_load_stack_arithmetic_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
            return ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK);
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

static EZrInstructionCode compiler_quickening_signed_load_const_to_const_opcode(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
            return ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
            return ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
            return ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
            return ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST);
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
            return ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST);
        default:
            return ZR_INSTRUCTION_ENUM(ENUM_MAX);
    }
}

static TZrBool compiler_quickening_try_dematerialize_dead_signed_load_const_arithmetic(
        SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex) {
    TZrInstruction *instruction;
    EZrInstructionCode opcode;
    EZrInstructionCode constOpcode;
    TZrUInt32 leftSlot;
    TZrUInt32 materializedSlot;
    TZrUInt32 constantIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[instructionIndex];
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    constOpcode = compiler_quickening_signed_load_const_to_const_opcode(opcode);
    if (constOpcode == ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
        return ZR_FALSE;
    }

    leftSlot = (TZrUInt32)instruction->instruction.operand.operand0[0];
    materializedSlot = (TZrUInt32)instruction->instruction.operand.operand0[1];
    constantIndex = (TZrUInt32)instruction->instruction.operand.operand1[1];
    if (constantIndex > UINT16_MAX ||
        !compiler_quickening_temp_slot_is_dead_after_instruction_cfg(function,
                                                                     blockStarts,
                                                                     instructionIndex,
                                                                     materializedSlot)) {
        return ZR_FALSE;
    }

    instruction->instruction.operationCode = (TZrUInt16)constOpcode;
    instruction->instruction.operand.operand1[0] = (TZrUInt16)leftSlot;
    instruction->instruction.operand.operand1[1] = (TZrUInt16)constantIndex;
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_dematerialize_dead_signed_load_stack_load_const_arithmetic(
        SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex) {
    TZrInstruction *instruction;
    TZrUInt32 sourceSlot;
    TZrUInt32 materializedStackSlot;
    TZrUInt32 materializedConstantSlot;
    TZrUInt32 constantIndex;
    TZrBool stackSlotDead;
    TZrBool constantSlotDead;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex >= function->instructionsLength) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[instructionIndex];
    if ((EZrInstructionCode)instruction->instruction.operationCode !=
        ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST)) {
        return ZR_FALSE;
    }

    sourceSlot = (TZrUInt32)instruction->instruction.operand.operand0[0];
    materializedStackSlot = (TZrUInt32)instruction->instruction.operand.operand0[1];
    materializedConstantSlot = (TZrUInt32)instruction->instruction.operand.operand0[2];
    constantIndex = (TZrUInt32)instruction->instruction.operand.operand1[1];
    if (constantIndex > UINT16_MAX) {
        return ZR_FALSE;
    }

    stackSlotDead = compiler_quickening_temp_slot_is_dead_after_instruction_cfg(function,
                                                                                blockStarts,
                                                                                instructionIndex,
                                                                                materializedStackSlot);
    constantSlotDead = compiler_quickening_temp_slot_is_dead_after_instruction_cfg(function,
                                                                                   blockStarts,
                                                                                   instructionIndex,
                                                                                   materializedConstantSlot);
    if (!constantSlotDead) {
        return ZR_FALSE;
    }

    if (stackSlotDead) {
        instruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST);
        instruction->instruction.operand.operand1[0] = (TZrUInt16)sourceSlot;
        instruction->instruction.operand.operand1[1] = (TZrUInt16)constantIndex;
        return ZR_TRUE;
    }

    instruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST);
    instruction->instruction.operand.operand0[0] = (TZrUInt8)sourceSlot;
    instruction->instruction.operand.operand0[1] = (TZrUInt8)materializedStackSlot;
    instruction->instruction.operand.operand1[1] = (TZrUInt16)constantIndex;
    return ZR_TRUE;
}

static TZrBool compiler_quickening_dematerialize_dead_signed_load_arithmetic(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        if (compiler_quickening_try_dematerialize_dead_signed_load_stack_load_const_arithmetic(function,
                                                                                                blockStarts,
                                                                                                index)) {
            continue;
        }
        compiler_quickening_try_dematerialize_dead_signed_load_const_arithmetic(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_try_fuse_materialized_const_signed_arithmetic(
        SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex) {
    TZrInstruction *instruction;
    TZrInstruction *constantInstruction;
    EZrInstructionCode fusedOpcode;
    EZrInstructionCode opcode;
    TZrUInt32 leftSlot;
    TZrUInt32 materializedSlot;
    TZrInt32 constantIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex == 0 || instructionIndex >= function->instructionsLength || blockStarts[instructionIndex]) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[instructionIndex];
    constantInstruction = &function->instructionsList[instructionIndex - 1];
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    fusedOpcode = compiler_quickening_signed_load_const_arithmetic_opcode(opcode);
    if (fusedOpcode == ZR_INSTRUCTION_ENUM(ENUM_MAX) ||
        (EZrInstructionCode)constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        constantInstruction->instruction.operandExtra == ZR_INSTRUCTION_USE_RET_FLAG) {
        return ZR_FALSE;
    }

    materializedSlot = constantInstruction->instruction.operandExtra;
    leftSlot = instruction->instruction.operand.operand1[0];
    constantIndex = constantInstruction->instruction.operand.operand2[0];
    if (constantIndex < 0 || constantIndex > UINT16_MAX ||
        leftSlot > UINT8_MAX ||
        materializedSlot > UINT8_MAX ||
        !compiler_quickening_constant_matches_arithmetic_opcode(function,
                                                                opcode,
                                                                (TZrUInt32)constantIndex) ||
        instruction->instruction.operand.operand1[1] != materializedSlot) {
        return ZR_FALSE;
    }

    instruction->instruction.operationCode = (TZrUInt16)fusedOpcode;
    instruction->instruction.operand.operand0[0] = (TZrUInt8)leftSlot;
    instruction->instruction.operand.operand0[1] = (TZrUInt8)materializedSlot;
    instruction->instruction.operand.operand1[1] = (TZrUInt16)constantIndex;
    compiler_quickening_write_nop(constantInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fuse_materialized_const_signed_equality(
        SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex) {
    TZrInstruction *instruction;
    TZrInstruction *constantInstruction;
    TZrUInt32 leftSlot;
    TZrUInt32 materializedSlot;
    TZrInt32 constantIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex == 0 || instructionIndex >= function->instructionsLength || blockStarts[instructionIndex]) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[instructionIndex];
    constantInstruction = &function->instructionsList[instructionIndex - 1];
    if ((EZrInstructionCode)instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED) ||
        (EZrInstructionCode)constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        constantInstruction->instruction.operandExtra == ZR_INSTRUCTION_USE_RET_FLAG) {
        return ZR_FALSE;
    }

    materializedSlot = constantInstruction->instruction.operandExtra;
    leftSlot = instruction->instruction.operand.operand1[0];
    constantIndex = constantInstruction->instruction.operand.operand2[0];
    if (constantIndex < 0 ||
        constantIndex > UINT16_MAX ||
        instruction->instruction.operand.operand1[1] != materializedSlot ||
        compiler_quickening_function_constant_slot_kind(function, (TZrUInt32)constantIndex) !=
                ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT ||
        (!compiler_quickening_temp_slot_is_dead_after_instruction(function, blockStarts, instructionIndex, materializedSlot) &&
         !compiler_quickening_temp_slot_is_dead_after_instruction_cfg(function,
                                                                      blockStarts,
                                                                      instructionIndex,
                                                                      materializedSlot))) {
        return ZR_FALSE;
    }

    instruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST);
    instruction->instruction.operand.operand1[0] = (TZrUInt16)leftSlot;
    instruction->instruction.operand.operand1[1] = (TZrUInt16)constantIndex;
    compiler_quickening_write_nop(constantInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fuse_materialized_stack_load_const_signed_arithmetic(
        SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex) {
    TZrInstruction *instruction;
    TZrInstruction *loadInstruction;
    EZrInstructionCode fusedOpcode;
    EZrInstructionCode opcode;
    TZrUInt32 sourceSlot;
    TZrUInt32 materializedStackSlot;
    TZrUInt32 materializedConstantSlot;
    TZrUInt32 constantIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex == 0 || instructionIndex >= function->instructionsLength || blockStarts[instructionIndex]) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[instructionIndex];
    loadInstruction = &function->instructionsList[instructionIndex - 1];
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    fusedOpcode = compiler_quickening_signed_load_stack_load_const_arithmetic_opcode(opcode);
    if (fusedOpcode == ZR_INSTRUCTION_ENUM(ENUM_MAX) ||
        (EZrInstructionCode)loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        loadInstruction->instruction.operandExtra == ZR_INSTRUCTION_USE_RET_FLAG) {
        return ZR_FALSE;
    }

    sourceSlot = (TZrUInt32)loadInstruction->instruction.operand.operand2[0];
    materializedStackSlot = loadInstruction->instruction.operandExtra;
    materializedConstantSlot = instruction->instruction.operand.operand0[1];
    constantIndex = (TZrUInt32)instruction->instruction.operand.operand1[1];
    if (sourceSlot > UINT8_MAX ||
        materializedStackSlot > UINT8_MAX ||
        materializedConstantSlot > UINT8_MAX ||
        constantIndex > UINT16_MAX ||
        sourceSlot == materializedStackSlot ||
        sourceSlot == materializedConstantSlot ||
        materializedStackSlot == materializedConstantSlot ||
        instruction->instruction.operandExtra == sourceSlot ||
        instruction->instruction.operand.operand0[0] != materializedStackSlot ||
        compiler_quickening_function_constant_slot_kind(function, constantIndex) !=
                ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT) {
        return ZR_FALSE;
    }

    instruction->instruction.operationCode = (TZrUInt16)fusedOpcode;
    instruction->instruction.operand.operand0[0] = (TZrUInt8)sourceSlot;
    instruction->instruction.operand.operand0[1] = (TZrUInt8)materializedStackSlot;
    instruction->instruction.operand.operand0[2] = (TZrUInt8)materializedConstantSlot;
    instruction->instruction.operand.operand1[1] = (TZrUInt16)constantIndex;
    compiler_quickening_write_nop(loadInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fuse_dead_stack_add_signed(
        SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex) {
    TZrInstruction *instruction;
    TZrInstruction *loadInstruction = ZR_NULL;
    EZrInstructionCode fusedOpcode;
    EZrInstructionCode opcode;
    TZrUInt32 sourceSlot;
    TZrUInt32 materializedSlot;
    TZrUInt32 rightSlot;
    TZrUInt32 scan;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex == 0 || instructionIndex >= function->instructionsLength || blockStarts[instructionIndex]) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[instructionIndex];
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    fusedOpcode = compiler_quickening_signed_load_stack_arithmetic_opcode(opcode);
    if (fusedOpcode == ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
        return ZR_FALSE;
    }

    materializedSlot = instruction->instruction.operand.operand1[0];
    rightSlot = instruction->instruction.operand.operand1[1];
    if (rightSlot > UINT8_MAX ||
        materializedSlot < function->localVariableLength ||
        instruction->instruction.operandExtra == materializedSlot ||
        (!compiler_quickening_temp_slot_is_dead_after_instruction(function, blockStarts, instructionIndex, materializedSlot) &&
         !compiler_quickening_temp_slot_is_dead_after_instruction_cfg(function,
                                                                      blockStarts,
                                                                      instructionIndex,
                                                                      materializedSlot))) {
        return ZR_FALSE;
    }

    for (scan = instructionIndex; scan > 0; scan--) {
        TZrInstruction *candidate = &function->instructionsList[scan - 1];

        if (blockStarts[scan]) {
            break;
        }

        if ((EZrInstructionCode)candidate->instruction.operationCode == ZR_INSTRUCTION_ENUM(GET_STACK) &&
            candidate->instruction.operandExtra == materializedSlot &&
            candidate->instruction.operandExtra != ZR_INSTRUCTION_USE_RET_FLAG) {
            loadInstruction = candidate;
            break;
        }

        if (compiler_quickening_instruction_may_read_slot(candidate, materializedSlot) ||
            compiler_quickening_instruction_writes_slot(candidate, materializedSlot)) {
            break;
        }
    }

    if (loadInstruction == ZR_NULL) {
        return ZR_FALSE;
    }

    sourceSlot = (TZrUInt32)loadInstruction->instruction.operand.operand2[0];
    if (sourceSlot > UINT8_MAX || sourceSlot == materializedSlot) {
        return ZR_FALSE;
    }

    for (scan = (TZrUInt32)(loadInstruction - function->instructionsList) + 1u; scan < instructionIndex; scan++) {
        if (compiler_quickening_instruction_writes_slot(&function->instructionsList[scan], sourceSlot)) {
            return ZR_FALSE;
        }
    }

    instruction->instruction.operationCode = (TZrUInt16)fusedOpcode;
    instruction->instruction.operand.operand0[0] = (TZrUInt8)sourceSlot;
    instruction->instruction.operand.operand0[1] = (TZrUInt8)rightSlot;
    compiler_quickening_write_nop(loadInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fuse_materialized_stack_const_signed_arithmetic(
        SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex) {
    TZrInstruction *instruction;
    TZrInstruction *loadInstruction;
    EZrInstructionCode fusedOpcode;
    EZrInstructionCode opcode;
    TZrUInt32 sourceSlot;
    TZrUInt32 materializedSlot;
    TZrUInt32 constantIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex == 0 || instructionIndex >= function->instructionsLength || blockStarts[instructionIndex]) {
        return ZR_FALSE;
    }

    instruction = &function->instructionsList[instructionIndex];
    loadInstruction = &function->instructionsList[instructionIndex - 1];
    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    fusedOpcode = compiler_quickening_signed_load_stack_const_arithmetic_opcode(opcode);
    if (fusedOpcode == ZR_INSTRUCTION_ENUM(ENUM_MAX) ||
        (EZrInstructionCode)loadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        loadInstruction->instruction.operandExtra == ZR_INSTRUCTION_USE_RET_FLAG) {
        return ZR_FALSE;
    }

    sourceSlot = (TZrUInt32)loadInstruction->instruction.operand.operand2[0];
    materializedSlot = loadInstruction->instruction.operandExtra;
    constantIndex = (TZrUInt32)instruction->instruction.operand.operand1[1];
    if (sourceSlot > UINT8_MAX ||
        materializedSlot > UINT8_MAX ||
        constantIndex > UINT16_MAX ||
        sourceSlot == materializedSlot ||
        instruction->instruction.operandExtra == sourceSlot ||
        instruction->instruction.operand.operand1[0] != materializedSlot ||
        compiler_quickening_function_constant_slot_kind(function, constantIndex) !=
                ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT) {
        return ZR_FALSE;
    }

    instruction->instruction.operationCode = (TZrUInt16)fusedOpcode;
    instruction->instruction.operand.operand0[0] = (TZrUInt8)sourceSlot;
    instruction->instruction.operand.operand0[1] = (TZrUInt8)materializedSlot;
    instruction->instruction.operand.operand1[1] = (TZrUInt16)constantIndex;
    compiler_quickening_write_nop(loadInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_fuse_materialized_const_signed_arithmetic(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 1; index < function->instructionsLength; index++) {
        compiler_quickening_try_fuse_materialized_const_signed_equality(function, blockStarts, index);
        compiler_quickening_try_fuse_materialized_const_signed_arithmetic(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_fuse_materialized_stack_const_signed_arithmetic(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 1; index < function->instructionsLength; index++) {
        compiler_quickening_try_fuse_dead_stack_add_signed(function, blockStarts, index);
        compiler_quickening_try_fuse_materialized_stack_load_const_signed_arithmetic(function, blockStarts, index);
        compiler_quickening_try_fuse_materialized_stack_const_signed_arithmetic(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_fold_right_const_arithmetic(SZrState *state, SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 1; index < function->instructionsLength; index++) {
        compiler_quickening_try_fold_right_const_arithmetic(state, function, blockStarts, index);
        compiler_quickening_try_fold_left_const_commutative_arithmetic(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_fold_stack_self_update_int_const(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 3) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index + 2 < function->instructionsLength; index++) {
        compiler_quickening_try_fold_stack_self_update_int_const(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static ZR_FORCE_INLINE TZrBool compiler_quickening_opcode_is_sub_int_family(EZrInstructionCode opcode) {
    return opcode == ZR_INSTRUCTION_ENUM(SUB_INT) || opcode == ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST) ||
           opcode == ZR_INSTRUCTION_ENUM(SUB_SIGNED) || opcode == ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST);
}

static ZR_FORCE_INLINE TZrBool compiler_quickening_opcode_is_add_int_family(EZrInstructionCode opcode) {
    return opcode == ZR_INSTRUCTION_ENUM(ADD_INT) || opcode == ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST) ||
           opcode == ZR_INSTRUCTION_ENUM(ADD_SIGNED) || opcode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST);
}

static ZR_FORCE_INLINE TZrBool compiler_quickening_opcode_is_add_int_const_family(EZrInstructionCode opcode) {
    return opcode == ZR_INSTRUCTION_ENUM(ADD_INT_CONST) ||
           opcode == ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST) ||
           opcode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST) ||
           opcode == ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST);
}

static TZrBool compiler_quickening_try_fold_super_array_fill_int4_const_loop_compact_self_update(
        SZrFunction *function,
        const TZrBool *blockStarts,
        TZrUInt32 instructionIndex) {
    TZrInstruction *initConstantInstruction;
    TZrInstruction *loopIndexLoadInstruction;
    TZrInstruction *countLoadInstruction;
    TZrInstruction *minusOneInstruction;
    TZrInstruction *subtractInstruction;
    TZrInstruction *compareInstruction;
    TZrInstruction *jumpIfInstruction;
    TZrInstruction *fillInstruction;
    TZrInstruction *incrementInstruction;
    TZrInstruction *jumpBackInstruction;
    TZrUInt32 indexSlot;
    TZrUInt32 countSlot;
    TZrUInt32 receiverBaseSlot;
    TZrUInt32 fillConstantIndex;
    TZrUInt32 minusOneConstantIndex;
    TZrUInt32 incrementOneConstantIndex;
    TZrInt64 constantValue;
    TZrUInt32 scan;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex < 3 || instructionIndex + 6 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    initConstantInstruction = &function->instructionsList[instructionIndex - 3];
    loopIndexLoadInstruction = &function->instructionsList[instructionIndex - 2];
    countLoadInstruction = &function->instructionsList[instructionIndex - 1];
    minusOneInstruction = &function->instructionsList[instructionIndex];
    subtractInstruction = &function->instructionsList[instructionIndex + 1];
    compareInstruction = &function->instructionsList[instructionIndex + 2];
    jumpIfInstruction = &function->instructionsList[instructionIndex + 3];
    fillInstruction = &function->instructionsList[instructionIndex + 4];
    incrementInstruction = &function->instructionsList[instructionIndex + 5];
    jumpBackInstruction = &function->instructionsList[instructionIndex + 6];

    for (scan = instructionIndex + 1; scan <= instructionIndex + 6; scan++) {
        if (blockStarts[scan]) {
            return ZR_FALSE;
        }
    }

    if ((EZrInstructionCode)initConstantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        (EZrInstructionCode)loopIndexLoadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        (EZrInstructionCode)countLoadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        (EZrInstructionCode)minusOneInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        !compiler_quickening_opcode_is_sub_int_family(
                (EZrInstructionCode)subtractInstruction->instruction.operationCode) ||
        (EZrInstructionCode)compareInstruction->instruction.operationCode !=
                ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED) ||
        (EZrInstructionCode)jumpIfInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        (EZrInstructionCode)fillInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST) ||
        !compiler_quickening_opcode_is_add_int_const_family(
                (EZrInstructionCode)incrementInstruction->instruction.operationCode) ||
        (EZrInstructionCode)jumpBackInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_function_constant_read_int64(function,
                                                          (TZrUInt32)initConstantInstruction->instruction.operand.operand2[0],
                                                          &constantValue) ||
        constantValue != 0) {
        return ZR_FALSE;
    }

    indexSlot = initConstantInstruction->instruction.operandExtra;
    if ((TZrUInt32)loopIndexLoadInstruction->instruction.operand.operand2[0] != indexSlot ||
        compareInstruction->instruction.operand.operand1[0] != loopIndexLoadInstruction->instruction.operandExtra ||
        incrementInstruction->instruction.operandExtra != indexSlot ||
        incrementInstruction->instruction.operand.operand1[0] != indexSlot) {
        return ZR_FALSE;
    }

    countSlot = (TZrUInt32)countLoadInstruction->instruction.operand.operand2[0];
    receiverBaseSlot = fillInstruction->instruction.operand.operand1[0];
    fillConstantIndex = fillInstruction->instruction.operand.operand1[1];
    minusOneConstantIndex = (TZrUInt32)minusOneInstruction->instruction.operand.operand2[0];
    incrementOneConstantIndex = (TZrUInt32)incrementInstruction->instruction.operand.operand1[1];
    if (!compiler_quickening_function_constant_read_int64(function, minusOneConstantIndex, &constantValue) ||
        constantValue != 1 ||
        !compiler_quickening_function_constant_read_int64(function, incrementOneConstantIndex, &constantValue) ||
        constantValue != 1) {
        return ZR_FALSE;
    }

    if (subtractInstruction->instruction.operand.operand1[0] != countLoadInstruction->instruction.operandExtra ||
        subtractInstruction->instruction.operand.operand1[1] != minusOneInstruction->instruction.operandExtra ||
        compareInstruction->instruction.operand.operand1[1] != subtractInstruction->instruction.operandExtra ||
        jumpIfInstruction->instruction.operandExtra != compareInstruction->instruction.operandExtra ||
        jumpIfInstruction->instruction.operand.operand2[0] != 3 ||
        jumpBackInstruction->instruction.operand.operand2[0] != -9) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_slot_is_overwritten_before_any_read_linear(function, instructionIndex + 7, indexSlot)) {
        return ZR_FALSE;
    }

    minusOneInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST);
    minusOneInstruction->instruction.operandExtra = (TZrUInt16)fillConstantIndex;
    minusOneInstruction->instruction.operand.operand1[0] = (TZrUInt16)receiverBaseSlot;
    minusOneInstruction->instruction.operand.operand1[1] = (TZrUInt16)countSlot;

    compiler_quickening_write_nop(initConstantInstruction);
    compiler_quickening_write_nop(loopIndexLoadInstruction);
    compiler_quickening_write_nop(countLoadInstruction);
    compiler_quickening_write_nop(subtractInstruction);
    compiler_quickening_write_nop(compareInstruction);
    compiler_quickening_write_nop(jumpIfInstruction);
    compiler_quickening_write_nop(fillInstruction);
    compiler_quickening_write_nop(incrementInstruction);
    compiler_quickening_write_nop(jumpBackInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fold_super_array_fill_int4_const_loop_compact(SZrFunction *function,
                                                                                      const TZrBool *blockStarts,
                                                                                      TZrUInt32 instructionIndex) {
    TZrInstruction *initConstantInstruction;
    TZrInstruction *initSetInstruction;
    TZrInstruction *minusOneInstruction;
    TZrInstruction *subtractInstruction;
    TZrInstruction *compareInstruction;
    TZrInstruction *jumpIfInstruction;
    TZrInstruction *fillInstruction;
    TZrInstruction *incrementOneInstruction;
    TZrInstruction *incrementInstruction;
    TZrInstruction *indexStoreInstruction;
    TZrInstruction *jumpBackInstruction;
    TZrUInt32 indexSlot;
    TZrUInt32 countSlot;
    TZrUInt32 receiverBaseSlot;
    TZrUInt32 fillConstantIndex;
    TZrUInt32 minusOneConstantIndex;
    TZrUInt32 incrementOneConstantIndex;
    TZrInt64 constantValue;
    TZrUInt32 scan;

    if (compiler_quickening_try_fold_super_array_fill_int4_const_loop_compact_self_update(function,
                                                                                           blockStarts,
                                                                                           instructionIndex)) {
        return ZR_TRUE;
    }

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex < 2 || instructionIndex + 8 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    initConstantInstruction = &function->instructionsList[instructionIndex - 2];
    initSetInstruction = &function->instructionsList[instructionIndex - 1];
    minusOneInstruction = &function->instructionsList[instructionIndex];
    subtractInstruction = &function->instructionsList[instructionIndex + 1];
    compareInstruction = &function->instructionsList[instructionIndex + 2];
    jumpIfInstruction = &function->instructionsList[instructionIndex + 3];
    fillInstruction = &function->instructionsList[instructionIndex + 4];
    incrementOneInstruction = &function->instructionsList[instructionIndex + 5];
    incrementInstruction = &function->instructionsList[instructionIndex + 6];
    indexStoreInstruction = &function->instructionsList[instructionIndex + 7];
    jumpBackInstruction = &function->instructionsList[instructionIndex + 8];

    for (scan = instructionIndex + 1; scan <= instructionIndex + 8; scan++) {
        if (blockStarts[scan] && scan != instructionIndex + 4) {
            return ZR_FALSE;
        }
    }

    if ((EZrInstructionCode)initConstantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        (EZrInstructionCode)initSetInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
        (EZrInstructionCode)minusOneInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        !compiler_quickening_opcode_is_sub_int_family(
                (EZrInstructionCode)subtractInstruction->instruction.operationCode) ||
        (EZrInstructionCode)compareInstruction->instruction.operationCode !=
                ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED) ||
        (EZrInstructionCode)jumpIfInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        (EZrInstructionCode)fillInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST) ||
        (EZrInstructionCode)incrementOneInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        !compiler_quickening_opcode_is_add_int_family(
                (EZrInstructionCode)incrementInstruction->instruction.operationCode) ||
        (EZrInstructionCode)indexStoreInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
        (EZrInstructionCode)jumpBackInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_function_constant_read_int64(function,
                                                          (TZrUInt32)initConstantInstruction->instruction.operand.operand2[0],
                                                          &constantValue) ||
        constantValue != 0) {
        return ZR_FALSE;
    }

    indexSlot = initSetInstruction->instruction.operandExtra;
    if ((TZrUInt32)initSetInstruction->instruction.operand.operand2[0] != initConstantInstruction->instruction.operandExtra ||
        compareInstruction->instruction.operand.operand1[0] != indexSlot ||
        incrementInstruction->instruction.operand.operand1[0] != indexSlot ||
        indexStoreInstruction->instruction.operandExtra != indexSlot ||
        (TZrUInt32)indexStoreInstruction->instruction.operand.operand2[0] != incrementInstruction->instruction.operandExtra) {
        return ZR_FALSE;
    }

    countSlot = subtractInstruction->instruction.operand.operand1[0];
    receiverBaseSlot = fillInstruction->instruction.operand.operand1[0];
    fillConstantIndex = fillInstruction->instruction.operand.operand1[1];
    minusOneConstantIndex = (TZrUInt32)minusOneInstruction->instruction.operand.operand2[0];
    incrementOneConstantIndex = (TZrUInt32)incrementOneInstruction->instruction.operand.operand2[0];
    if (!compiler_quickening_function_constant_read_int64(function, minusOneConstantIndex, &constantValue) ||
        constantValue != 1 ||
        !compiler_quickening_function_constant_read_int64(function, incrementOneConstantIndex, &constantValue) ||
        constantValue != 1) {
        return ZR_FALSE;
    }

    if (subtractInstruction->instruction.operand.operand1[1] != minusOneInstruction->instruction.operandExtra ||
        compareInstruction->instruction.operand.operand1[1] != subtractInstruction->instruction.operandExtra ||
        jumpIfInstruction->instruction.operandExtra != compareInstruction->instruction.operandExtra ||
        jumpIfInstruction->instruction.operand.operand2[0] != 6 ||
        incrementInstruction->instruction.operand.operand1[1] != incrementOneInstruction->instruction.operandExtra ||
        jumpBackInstruction->instruction.operand.operand2[0] != -9) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_slot_is_overwritten_before_any_read_linear(function, instructionIndex + 9, indexSlot)) {
        return ZR_FALSE;
    }

    minusOneInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST);
    minusOneInstruction->instruction.operandExtra = (TZrUInt16)fillConstantIndex;
    minusOneInstruction->instruction.operand.operand1[0] = (TZrUInt16)receiverBaseSlot;
    minusOneInstruction->instruction.operand.operand1[1] = (TZrUInt16)countSlot;

    compiler_quickening_write_nop(initConstantInstruction);
    compiler_quickening_write_nop(initSetInstruction);
    compiler_quickening_write_nop(subtractInstruction);
    compiler_quickening_write_nop(compareInstruction);
    compiler_quickening_write_nop(jumpIfInstruction);
    compiler_quickening_write_nop(fillInstruction);
    compiler_quickening_write_nop(incrementOneInstruction);
    compiler_quickening_write_nop(incrementInstruction);
    compiler_quickening_write_nop(indexStoreInstruction);
    compiler_quickening_write_nop(jumpBackInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fold_super_array_add_int4_const(SZrFunction *function,
                                                                       const TZrBool *blockStarts,
                                                                       TZrUInt32 instructionIndex) {
    TZrInstruction *constantInstruction;
    TZrInstruction *burstInstruction;
    TZrUInt32 valueSlot;
    TZrUInt32 constantIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex + 1 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    constantInstruction = &function->instructionsList[instructionIndex];
    burstInstruction = &function->instructionsList[instructionIndex + 1];
    if ((EZrInstructionCode)constantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        (EZrInstructionCode)burstInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4) ||
        blockStarts[instructionIndex + 1]) {
        return ZR_FALSE;
    }

    valueSlot = constantInstruction->instruction.operandExtra;
    constantIndex = (TZrUInt32)constantInstruction->instruction.operand.operand2[0];
    if (!compiler_quickening_function_constant_is_int(function, constantIndex) ||
        burstInstruction->instruction.operand.operand1[1] != valueSlot ||
        !compiler_quickening_slot_is_overwritten_before_read(function, blockStarts, instructionIndex + 2, valueSlot)) {
        return ZR_FALSE;
    }

    constantInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST);
    constantInstruction->instruction.operandExtra = ZR_INSTRUCTION_USE_RET_FLAG;
    constantInstruction->instruction.operand.operand1[0] = burstInstruction->instruction.operand.operand1[0];
    constantInstruction->instruction.operand.operand1[1] = (TZrUInt16)constantIndex;
    compiler_quickening_write_nop(burstInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quickening_try_fold_super_array_fill_int4_const_loop(SZrFunction *function,
                                                                             const TZrBool *blockStarts,
                                                                             TZrUInt32 instructionIndex) {
    TZrInstruction *initConstantInstruction;
    TZrInstruction *initSetInstruction;
    TZrInstruction *loopIndexLoadInstruction;
    TZrInstruction *countLoadInstruction;
    TZrInstruction *minusOneInstruction;
    TZrInstruction *subtractInstruction;
    TZrInstruction *compareInstruction;
    TZrInstruction *jumpIfInstruction;
    TZrInstruction *fillInstruction;
    TZrInstruction *incrementIndexLoadInstruction;
    TZrInstruction *incrementOneInstruction;
    TZrInstruction *incrementInstruction;
    TZrInstruction *indexStoreInstruction;
    TZrInstruction *jumpBackInstruction;
    TZrUInt32 indexSlot;
    TZrUInt32 countSlot;
    TZrUInt32 receiverBaseSlot;
    TZrUInt32 fillConstantIndex;
    TZrUInt32 minusOneConstantIndex;
    TZrUInt32 incrementOneConstantIndex;
    TZrInt64 constantValue;
    TZrUInt32 scan;

    if (compiler_quickening_try_fold_super_array_fill_int4_const_loop_compact(function,
                                                                              blockStarts,
                                                                              instructionIndex)) {
        return ZR_TRUE;
    }

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || blockStarts == ZR_NULL ||
        instructionIndex < 2 || instructionIndex + 11 >= function->instructionsLength) {
        return ZR_FALSE;
    }

    initConstantInstruction = &function->instructionsList[instructionIndex - 2];
    initSetInstruction = &function->instructionsList[instructionIndex - 1];
    loopIndexLoadInstruction = &function->instructionsList[instructionIndex];
    countLoadInstruction = &function->instructionsList[instructionIndex + 1];
    minusOneInstruction = &function->instructionsList[instructionIndex + 2];
    subtractInstruction = &function->instructionsList[instructionIndex + 3];
    compareInstruction = &function->instructionsList[instructionIndex + 4];
    jumpIfInstruction = &function->instructionsList[instructionIndex + 5];
    fillInstruction = &function->instructionsList[instructionIndex + 6];
    incrementIndexLoadInstruction = &function->instructionsList[instructionIndex + 7];
    incrementOneInstruction = &function->instructionsList[instructionIndex + 8];
    incrementInstruction = &function->instructionsList[instructionIndex + 9];
    indexStoreInstruction = &function->instructionsList[instructionIndex + 10];
    jumpBackInstruction = &function->instructionsList[instructionIndex + 11];

    for (scan = instructionIndex + 1; scan <= instructionIndex + 11; scan++) {
        if (blockStarts[scan] && scan != instructionIndex + 6) {
            return ZR_FALSE;
        }
    }

    if ((EZrInstructionCode)initConstantInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        (EZrInstructionCode)initSetInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
        (EZrInstructionCode)loopIndexLoadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        (EZrInstructionCode)countLoadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        (EZrInstructionCode)minusOneInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        !compiler_quickening_opcode_is_sub_int_family(
                (EZrInstructionCode)subtractInstruction->instruction.operationCode) ||
        (EZrInstructionCode)compareInstruction->instruction.operationCode !=
                ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED) ||
        (EZrInstructionCode)jumpIfInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF) ||
        (EZrInstructionCode)fillInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST) ||
        (EZrInstructionCode)incrementIndexLoadInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_STACK) ||
        (EZrInstructionCode)incrementOneInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_CONSTANT) ||
        !compiler_quickening_opcode_is_add_int_family(
                (EZrInstructionCode)incrementInstruction->instruction.operationCode) ||
        (EZrInstructionCode)indexStoreInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(SET_STACK) ||
        (EZrInstructionCode)jumpBackInstruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP)) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_function_constant_read_int64(function,
                                                          (TZrUInt32)initConstantInstruction->instruction.operand.operand2[0],
                                                          &constantValue) ||
        constantValue != 0) {
        return ZR_FALSE;
    }

    indexSlot = initSetInstruction->instruction.operandExtra;
    if ((TZrUInt32)initSetInstruction->instruction.operand.operand2[0] != initConstantInstruction->instruction.operandExtra ||
        (TZrUInt32)loopIndexLoadInstruction->instruction.operand.operand2[0] != indexSlot ||
        (TZrUInt32)incrementIndexLoadInstruction->instruction.operand.operand2[0] != indexSlot ||
        indexStoreInstruction->instruction.operandExtra != indexSlot ||
        (TZrUInt32)indexStoreInstruction->instruction.operand.operand2[0] != incrementInstruction->instruction.operandExtra) {
        return ZR_FALSE;
    }

    countSlot = (TZrUInt32)countLoadInstruction->instruction.operand.operand2[0];
    receiverBaseSlot = fillInstruction->instruction.operand.operand1[0];
    fillConstantIndex = fillInstruction->instruction.operand.operand1[1];
    minusOneConstantIndex = (TZrUInt32)minusOneInstruction->instruction.operand.operand2[0];
    incrementOneConstantIndex = (TZrUInt32)incrementOneInstruction->instruction.operand.operand2[0];
    if (!compiler_quickening_function_constant_read_int64(function, minusOneConstantIndex, &constantValue) ||
        constantValue != 1 ||
        !compiler_quickening_function_constant_read_int64(function, incrementOneConstantIndex, &constantValue) ||
        constantValue != 1) {
        return ZR_FALSE;
    }

    if (subtractInstruction->instruction.operand.operand1[0] != countLoadInstruction->instruction.operandExtra ||
        subtractInstruction->instruction.operand.operand1[1] != minusOneInstruction->instruction.operandExtra ||
        compareInstruction->instruction.operand.operand1[0] != loopIndexLoadInstruction->instruction.operandExtra ||
        compareInstruction->instruction.operand.operand1[1] != subtractInstruction->instruction.operandExtra ||
        jumpIfInstruction->instruction.operandExtra != compareInstruction->instruction.operandExtra ||
        jumpIfInstruction->instruction.operand.operand2[0] != 6 ||
        incrementInstruction->instruction.operand.operand1[0] != incrementIndexLoadInstruction->instruction.operandExtra ||
        incrementInstruction->instruction.operand.operand1[1] != incrementOneInstruction->instruction.operandExtra ||
        jumpBackInstruction->instruction.operand.operand2[0] != -12) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_slot_is_overwritten_before_any_read_linear(function, instructionIndex + 12, indexSlot)) {
        return ZR_FALSE;
    }

    loopIndexLoadInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST);
    loopIndexLoadInstruction->instruction.operandExtra = (TZrUInt16)fillConstantIndex;
    loopIndexLoadInstruction->instruction.operand.operand1[0] = (TZrUInt16)receiverBaseSlot;
    loopIndexLoadInstruction->instruction.operand.operand1[1] = (TZrUInt16)countSlot;

    compiler_quickening_write_nop(initConstantInstruction);
    compiler_quickening_write_nop(initSetInstruction);
    compiler_quickening_write_nop(countLoadInstruction);
    compiler_quickening_write_nop(minusOneInstruction);
    compiler_quickening_write_nop(subtractInstruction);
    compiler_quickening_write_nop(compareInstruction);
    compiler_quickening_write_nop(jumpIfInstruction);
    compiler_quickening_write_nop(fillInstruction);
    compiler_quickening_write_nop(incrementIndexLoadInstruction);
    compiler_quickening_write_nop(incrementOneInstruction);
    compiler_quickening_write_nop(incrementInstruction);
    compiler_quickening_write_nop(indexStoreInstruction);
    compiler_quickening_write_nop(jumpBackInstruction);
    return ZR_TRUE;
}

static TZrBool compiler_quicken_array_int_index_accesses(SZrState *state, SZrFunction *function) {
    ZrCompilerQuickeningSlotAlias *aliases = ZR_NULL;
    EZrCompilerQuickeningSlotKind *slotKinds = ZR_NULL;
    TZrBool *blockStarts = ZR_NULL;
    TZrBool *constantSlotsValid = ZR_NULL;
    TZrUInt32 *constantSlotIndices = ZR_NULL;
    TZrUInt32 aliasCount;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    aliasCount = function->stackSize;
    if (aliasCount > 0) {
        aliases = (ZrCompilerQuickeningSlotAlias *)malloc(sizeof(*aliases) * aliasCount);
        slotKinds = (EZrCompilerQuickeningSlotKind *)malloc(sizeof(*slotKinds) * aliasCount);
        constantSlotsValid = (TZrBool *)malloc(sizeof(*constantSlotsValid) * aliasCount);
        constantSlotIndices = (TZrUInt32 *)malloc(sizeof(*constantSlotIndices) * aliasCount);
        if (aliases == ZR_NULL || slotKinds == ZR_NULL || constantSlotsValid == ZR_NULL || constantSlotIndices == ZR_NULL) {
            free(constantSlotIndices);
            free(constantSlotsValid);
            free(slotKinds);
            free(aliases);
            return ZR_FALSE;
        }
        compiler_quickening_clear_aliases(aliases, aliasCount);
        compiler_quickening_clear_slot_kinds(slotKinds, aliasCount);
        memset(constantSlotsValid, 0, sizeof(*constantSlotsValid) * aliasCount);
        memset(constantSlotIndices, 0, sizeof(*constantSlotIndices) * aliasCount);
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        free(constantSlotIndices);
        free(constantSlotsValid);
        free(slotKinds);
        free(aliases);
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        free(constantSlotIndices);
        free(constantSlotsValid);
        free(slotKinds);
        free(aliases);
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        TZrUInt32 destinationSlot = instruction->instruction.operandExtra;

        if (blockStarts[index]) {
            compiler_quickening_clear_aliases(aliases, aliasCount);
            compiler_quickening_clear_slot_kinds(slotKinds, aliasCount);
        }

        if (opcode == ZR_INSTRUCTION_ENUM(GET_BY_INDEX) || opcode == ZR_INSTRUCTION_ENUM(SET_BY_INDEX)) {
            TZrUInt32 receiverSlot = instruction->instruction.operand.operand1[0];
            TZrUInt32 keySlot = instruction->instruction.operand.operand1[1];
            if (compiler_quickening_slot_is_array_int(function,
                                                      aliases,
                                                      aliasCount,
                                                      slotKinds,
                                                      index,
                                                      receiverSlot) &&
                compiler_quickening_slot_is_int(function,
                                                aliases,
                                                aliasCount,
                                                slotKinds,
                                                blockStarts,
                                                index,
                                                keySlot)) {
                instruction->instruction.operationCode =
                        (TZrUInt16)(opcode == ZR_INSTRUCTION_ENUM(GET_BY_INDEX)
                                            ? ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT)
                                            : ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT));
                opcode = (EZrInstructionCode)instruction->instruction.operationCode;
            }
        }

        if (opcode == ZR_INSTRUCTION_ENUM(FUNCTION_CALL) && index > 0) {
            TZrUInt32 getMemberInstructionIndex = UINT32_MAX;
            TZrInstruction *getMemberInstruction = compiler_quickening_find_latest_block_writer(function,
                                                                                                blockStarts,
                                                                                                index,
                                                                                                instruction->instruction.operand.operand1[0],
                                                                                                &getMemberInstructionIndex);
            EZrInstructionCode getMemberOpcode =
                    getMemberInstruction != ZR_NULL
                            ? (EZrInstructionCode)getMemberInstruction->instruction.operationCode
                            : ZR_INSTRUCTION_ENUM(ENUM_MAX);
            TZrUInt16 functionSlot = instruction->instruction.operand.operand1[0];
            TZrUInt16 parameterCount = instruction->instruction.operand.operand1[1];

            if (getMemberInstruction != ZR_NULL &&
                getMemberInstructionIndex < index &&
                getMemberOpcode == ZR_INSTRUCTION_ENUM(GET_MEMBER) &&
                getMemberInstruction->instruction.operandExtra == functionSlot &&
                parameterCount == 2) {
                const TZrChar *memberName = compiler_quickening_member_entry_symbol_text(
                        function,
                        getMemberInstruction->instruction.operand.operand1[1]);
                TZrUInt32 receiverArgumentSlot = (TZrUInt32)functionSlot + 1u;
                TZrUInt32 valueArgumentSlot = (TZrUInt32)functionSlot + 2u;

                if (memberName != ZR_NULL &&
                    strcmp(memberName, "add") == 0 &&
                    compiler_quickening_slot_is_array_int(function,
                                                          aliases,
                                                          aliasCount,
                                                          slotKinds,
                                                          index,
                                                          receiverArgumentSlot) &&
                    compiler_quickening_slot_is_int(function,
                                                    aliases,
                                                    aliasCount,
                                                    slotKinds,
                                                    blockStarts,
                                                    index,
                                                    valueArgumentSlot)) {
                    getMemberInstruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_STACK);
                    getMemberInstruction->instruction.operand.operand2[0] = (TZrInt32)receiverArgumentSlot;

                    instruction->instruction.operationCode = (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT);
                    instruction->instruction.operand.operand1[0] = (TZrUInt16)receiverArgumentSlot;
                    instruction->instruction.operand.operand1[1] = (TZrUInt16)valueArgumentSlot;
                    opcode = ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT);
                }
            }
        }

        if (opcode == ZR_INSTRUCTION_ENUM(ADD) ||
            opcode == ZR_INSTRUCTION_ENUM(SUB) ||
            opcode == ZR_INSTRUCTION_ENUM(MUL) ||
            opcode == ZR_INSTRUCTION_ENUM(DIV) ||
            opcode == ZR_INSTRUCTION_ENUM(MOD)) {
            TZrUInt32 leftSlot = instruction->instruction.operand.operand1[0];
            TZrUInt32 rightSlot = instruction->instruction.operand.operand1[1];
            EZrCompilerQuickeningSlotKind leftKind = compiler_quickening_slot_kind_for_slot(function,
                                                                                             aliases,
                                                                                             aliasCount,
                                                                                             slotKinds,
                                                                                             index,
                                                                                             leftSlot);
            EZrCompilerQuickeningSlotKind rightKind = compiler_quickening_slot_kind_for_slot(function,
                                                                                              aliases,
                                                                                              aliasCount,
                                                                                              slotKinds,
                                                                                              index,
                                                                                              rightSlot);
            EZrInstructionCode specializedOpcode =
                    compiler_quickening_specialized_numeric_opcode(opcode, leftKind, rightKind);

            if (specializedOpcode != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                instruction->instruction.operationCode = (TZrUInt16)specializedOpcode;
                opcode = (EZrInstructionCode)instruction->instruction.operationCode;
            }
        }

        if (opcode == ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL) || opcode == ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL)) {
            TZrUInt32 leftSlot = instruction->instruction.operand.operand1[0];
            TZrUInt32 rightSlot = instruction->instruction.operand.operand1[1];
            EZrCompilerQuickeningSlotKind leftKind = compiler_quickening_slot_kind_for_slot(function,
                                                                                             aliases,
                                                                                             aliasCount,
                                                                                             slotKinds,
                                                                                             index,
                                                                                             leftSlot);
            EZrCompilerQuickeningSlotKind rightKind = compiler_quickening_slot_kind_for_slot(function,
                                                                                              aliases,
                                                                                              aliasCount,
                                                                                              slotKinds,
                                                                                              index,
                                                                                              rightSlot);
            EZrInstructionCode specializedOpcode =
                    compiler_quickening_specialized_equality_opcode(opcode, leftKind, rightKind);

            if (specializedOpcode != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                instruction->instruction.operationCode = (TZrUInt16)specializedOpcode;
                opcode = (EZrInstructionCode)instruction->instruction.operationCode;
            }
        }

        if (opcode == ZR_INSTRUCTION_ENUM(GET_STACK) || opcode == ZR_INSTRUCTION_ENUM(SET_STACK)) {
            TZrUInt32 sourceSlot = (TZrUInt32)instruction->instruction.operand.operand2[0];
            TZrUInt32 rootSlot = 0;
            if (compiler_quickening_resolve_alias_slot(function,
                                                       aliases,
                                                       aliasCount,
                                                       index,
                                                       sourceSlot,
                                                       &rootSlot)) {
                if (aliases != ZR_NULL && destinationSlot < aliasCount) {
                    aliases[destinationSlot].valid = ZR_TRUE;
                    aliases[destinationSlot].rootSlot = rootSlot;
                }
            } else {
                compiler_quickening_clear_alias(aliases, aliasCount, destinationSlot);
            }
            if (slotKinds != ZR_NULL && destinationSlot < aliasCount) {
                slotKinds[destinationSlot] = compiler_quickening_slot_kind_for_slot(function,
                                                                                    aliases,
                                                                                    aliasCount,
                                                                                    slotKinds,
                                                                                    index,
                                                                                    sourceSlot);
            }
            continue;
        }

        compiler_quickening_clear_call_argument_tracking(function,
                                                         instruction,
                                                         aliases,
                                                         slotKinds,
                                                         ZR_NULL,
                                                         ZR_NULL,
                                                         aliasCount);

        if (slotKinds != ZR_NULL && destinationSlot < aliasCount) {
            switch (opcode) {
                case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
                    slotKinds[destinationSlot] = compiler_quickening_function_constant_slot_kind(
                            function,
                            (TZrUInt32)instruction->instruction.operand.operand2[0]);
                    break;
                case ZR_INSTRUCTION_ENUM(TO_BOOL):
                    slotKinds[destinationSlot] = ZR_COMPILER_QUICKENING_SLOT_KIND_BOOL;
                    break;
                case ZR_INSTRUCTION_ENUM(TO_INT):
                case ZR_INSTRUCTION_ENUM(ADD_INT):
                case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
                case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
                case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
                case ZR_INSTRUCTION_ENUM(SUB_INT):
                case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
                case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
                case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
                case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
                case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
                case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
                case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
                case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
                case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
                case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
                case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
                case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
                case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
                case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST):
                    slotKinds[destinationSlot] = ZR_COMPILER_QUICKENING_SLOT_KIND_SIGNED_INT;
                    break;
                case ZR_INSTRUCTION_ENUM(TO_UINT):
                case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
                case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
                case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
                case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
                case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
                case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
                case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
                    slotKinds[destinationSlot] = ZR_COMPILER_QUICKENING_SLOT_KIND_UNSIGNED_INT;
                    break;
                case ZR_INSTRUCTION_ENUM(TO_FLOAT):
                case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
                case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
                case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
                case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
                case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
                    slotKinds[destinationSlot] = ZR_COMPILER_QUICKENING_SLOT_KIND_FLOAT;
                    break;
                case ZR_INSTRUCTION_ENUM(TO_STRING):
                case ZR_INSTRUCTION_ENUM(ADD_STRING):
                    slotKinds[destinationSlot] = ZR_COMPILER_QUICKENING_SLOT_KIND_STRING;
                    break;
                case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
                case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
                case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
                case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
                case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL):
                case ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8):
                case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_MEMBER_CALL):
                case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS):
                case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS):
                case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS):
                case ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS):
                    slotKinds[destinationSlot] =
                            compiler_quickening_known_call_result_slot_kind(state, function, index, instruction);
                    break;
                case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
                case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
                case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
                case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
                case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
                case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST):
                case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
                case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
                case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
                case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
                case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
                case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
                case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
                case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
                case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
                case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
                case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
                case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
                case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
                case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
                case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
                case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
                case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
                case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
                case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
                case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
                case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
                    slotKinds[destinationSlot] = ZR_COMPILER_QUICKENING_SLOT_KIND_BOOL;
                    break;
                default:
                    slotKinds[destinationSlot] = ZR_COMPILER_QUICKENING_SLOT_KIND_UNKNOWN;
                    break;
            }
        }

        if (!compiler_quickening_is_control_only_opcode(opcode)) {
            compiler_quickening_clear_alias(aliases, aliasCount, destinationSlot);
        }
    }

    for (index = 0; index < function->instructionsLength; index++) {
        compiler_quickening_try_fold_dead_super_array_add_setup(function, blockStarts, index);
    }

    if (constantSlotsValid != ZR_NULL && constantSlotIndices != ZR_NULL && aliasCount > 0) {
        memset(constantSlotsValid, 0, sizeof(*constantSlotsValid) * aliasCount);
        memset(constantSlotIndices, 0, sizeof(*constantSlotIndices) * aliasCount);
        for (index = 0; index < function->instructionsLength; index++) {
            TZrInstruction *instruction = &function->instructionsList[index];
            EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
            TZrUInt32 destinationSlot = instruction->instruction.operandExtra;

            if (blockStarts[index]) {
                memset(constantSlotsValid, 0, sizeof(*constantSlotsValid) * aliasCount);
            }

            if (opcode == ZR_INSTRUCTION_ENUM(GET_CONSTANT) && destinationSlot < aliasCount) {
                TZrUInt32 constantIndex = (TZrUInt32)instruction->instruction.operand.operand2[0];

                if (constantSlotsValid[destinationSlot] && constantSlotIndices[destinationSlot] == constantIndex) {
                    compiler_quickening_write_nop(instruction);
                    continue;
                }

                constantSlotsValid[destinationSlot] = ZR_TRUE;
                constantSlotIndices[destinationSlot] = constantIndex;
                continue;
            }

            compiler_quickening_clear_call_argument_tracking(function,
                                                             instruction,
                                                             ZR_NULL,
                                                             ZR_NULL,
                                                             constantSlotsValid,
                                                             constantSlotIndices,
                                                             aliasCount);

            if (!compiler_quickening_is_control_only_opcode(opcode) && destinationSlot < aliasCount) {
                constantSlotsValid[destinationSlot] = ZR_FALSE;
                constantSlotIndices[destinationSlot] = 0;
            }
        }
    }

    if (!compiler_quickening_promote_plain_destination_opcodes(function, blockStarts)) {
        free(blockStarts);
        free(constantSlotIndices);
        free(constantSlotsValid);
        free(slotKinds);
        free(aliases);
        return ZR_FALSE;
    }

    success = ZR_TRUE;
    free(blockStarts);
    free(constantSlotIndices);
    free(constantSlotsValid);
    free(slotKinds);
    free(aliases);
    return success;
}

static TZrBool compiler_quickening_fold_super_array_add_int4_bursts(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 4) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        compiler_quickening_try_fold_super_array_add_int4_burst(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_fold_super_array_add_int4_const_bursts(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        compiler_quickening_try_fold_super_array_add_int4_const(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrBool compiler_quickening_fold_super_array_fill_int4_const_loops(SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 12) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        compiler_quickening_try_fold_super_array_fill_int4_const_loop(function, blockStarts, index);
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrUInt32 compiler_quickening_find_deopt_id(const SZrFunction *function, TZrUInt32 instructionIndex) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return ZR_RUNTIME_SEMIR_DEOPT_ID_NONE;
    }

    for (index = 0; index < function->semIrInstructionLength; index++) {
        const SZrSemIrInstruction *instruction = &function->semIrInstructions[index];
        if (instruction->execInstructionIndex == instructionIndex) {
            return instruction->deoptId;
        }
    }

    return ZR_RUNTIME_SEMIR_DEOPT_ID_NONE;
}

static TZrBool compiler_quicken_known_calls(SZrState *state, SZrFunction *function) {
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        EZrCompilerQuickeningCallableProvenanceKind provenance;

        switch (opcode) {
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
            case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
                break;
            case ZR_INSTRUCTION_ENUM(META_CALL):
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
                /*
                 * A meta-call's callee slot is the receiver object. Even when
                 * the receiver type has a VM @call target, the callable is not
                 * materialized in that slot, so known-call opcodes would call
                 * the receiver object itself.
                 */
                continue;
            default:
                continue;
        }

        provenance = compiler_quickening_resolve_callable_provenance_before_instruction(
                state,
                function,
                blockStarts,
                index,
                instruction->instruction.operand.operand1[0],
                0);
        if (provenance == ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_VM) {
            instruction->instruction.operationCode = (TZrUInt16)((opcode == ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL) ||
                                                                  opcode == ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL) ||
                                                                  opcode == ZR_INSTRUCTION_ENUM(META_TAIL_CALL))
                                                                         ? ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL)
                                                                         : ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL));
        } else if (provenance == ZR_COMPILER_QUICKENING_CALLABLE_PROVENANCE_NATIVE) {
            instruction->instruction.operationCode =
                    (TZrUInt16)((opcode == ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL) ||
                                 opcode == ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL) ||
                                 opcode == ZR_INSTRUCTION_ENUM(META_TAIL_CALL))
                                        ? ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL)
                                        : ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL));
        }
    }

    success = ZR_TRUE;
    free(blockStarts);
    return success;
}

static TZrUInt8 compiler_quickening_member_entry_flags(const SZrFunction *function, TZrUInt32 memberEntryIndex) {
    if (function == ZR_NULL || function->memberEntries == ZR_NULL || memberEntryIndex >= function->memberEntryLength) {
        return ZR_COMPILER_QUICKENING_MEMBER_FLAGS_NONE;
    }

    return function->memberEntries[memberEntryIndex].reserved0;
}

static TZrBool compiler_quickening_function_matches_inline_child(const SZrFunction *left, const SZrFunction *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    return left->functionName == right->functionName &&
           left->parameterCount == right->parameterCount &&
           left->instructionsLength == right->instructionsLength &&
           left->lineInSourceStart == right->lineInSourceStart &&
           left->lineInSourceEnd == right->lineInSourceEnd;
}

static SZrFunction *compiler_quickening_function_from_vm_constant_value(SZrTypeValue *constant) {
    SZrRawObject *rawObject;

    if (constant == ZR_NULL || constant->isNative || constant->value.object == ZR_NULL ||
        (constant->type != ZR_VALUE_TYPE_FUNCTION && constant->type != ZR_VALUE_TYPE_CLOSURE)) {
        return ZR_NULL;
    }

    rawObject = constant->value.object;
    if (rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_CAST(SZrFunction *, rawObject);
    }

    if (rawObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
        SZrClosure *closure = ZR_CAST(SZrClosure *, rawObject);
        return closure != ZR_NULL ? closure->function : ZR_NULL;
    }

    return ZR_NULL;
}

static const SZrFunction *compiler_quickening_function_from_const_vm_constant_value(const SZrTypeValue *constant) {
    return compiler_quickening_function_from_vm_constant_value((SZrTypeValue *)constant);
}

static void compiler_quickening_rebind_constant_function_values_to_children(SZrFunction *function) {
    if (function == ZR_NULL || function->constantValueList == ZR_NULL || function->childFunctionList == ZR_NULL ||
        function->childFunctionLength == 0) {
        return;
    }

    for (TZrUInt32 constantIndex = 0; constantIndex < function->constantValueLength; constantIndex++) {
        SZrTypeValue *constant = &function->constantValueList[constantIndex];
        SZrRawObject *rawObject;
        SZrFunction *constantFunction;

        if ((constant->type != ZR_VALUE_TYPE_FUNCTION && constant->type != ZR_VALUE_TYPE_CLOSURE) ||
            constant->value.object == ZR_NULL) {
            continue;
        }

        rawObject = constant->value.object;
        constantFunction = compiler_quickening_function_from_vm_constant_value(constant);
        if (constantFunction == ZR_NULL) {
            continue;
        }

        for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
            SZrFunction *childFunction = &function->childFunctionList[childIndex];
            if (!compiler_quickening_function_matches_inline_child(constantFunction, childFunction)) {
                continue;
            }

            if (rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                constant->value.object = ZR_CAST_RAW_OBJECT_AS_SUPER(childFunction);
            } else if (rawObject->type == ZR_RAW_OBJECT_TYPE_CLOSURE && !constant->isNative) {
                SZrClosure *closure = ZR_CAST(SZrClosure *, rawObject);
                if (closure != ZR_NULL) {
                    closure->function = childFunction;
                }
            }
            break;
        }
    }
}

static TZrBool compiler_quickening_constant_function_matches_child(const SZrFunction *function,
                                                                   const SZrFunction *constantFunction) {
    if (function == ZR_NULL || constantFunction == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (compiler_quickening_function_matches_inline_child(constantFunction,
                                                              &function->childFunctionList[childIndex])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool compiler_quickening_quicken_constant_function_values(SZrState *state, SZrFunction *function) {
    static TZrUInt32 recursionDepth = 0;
    TZrBool success = ZR_TRUE;

    if (state == ZR_NULL || function == ZR_NULL || function->constantValueList == ZR_NULL) {
        return ZR_TRUE;
    }

    if (recursionDepth > 0) {
        return ZR_TRUE;
    }

    recursionDepth++;
    for (TZrUInt32 constantIndex = 0; constantIndex < function->constantValueLength; constantIndex++) {
        SZrTypeValue *constant = &function->constantValueList[constantIndex];
        SZrFunction *constantFunction;

        if (constant == ZR_NULL || constant->isNative ||
            (constant->type != ZR_VALUE_TYPE_FUNCTION && constant->type != ZR_VALUE_TYPE_CLOSURE)) {
            continue;
        }

        constantFunction = compiler_quickening_function_from_vm_constant_value(constant);
        if (constantFunction == ZR_NULL ||
            constantFunction == function ||
            compiler_quickening_constant_function_matches_child(function, constantFunction)) {
            continue;
        }

        if (!compiler_quicken_child_functions(state, constantFunction, ZR_TRUE)) {
            success = ZR_FALSE;
            break;
        }
    }

    recursionDepth--;
    return success;
}

static TZrBool compiler_quickening_append_callsite_cache(SZrState *state,
                                                         SZrFunction *function,
                                                         EZrFunctionCallSiteCacheKind kind,
                                                         TZrUInt32 instructionIndex,
                                                         TZrUInt32 memberEntryIndex,
                                                         TZrUInt32 deoptId,
                                                         TZrUInt32 argumentCount,
                                                         TZrUInt16 *outCacheIndex) {
    SZrFunctionCallSiteCacheEntry *newEntries;
    TZrSize newCount;
    TZrSize copyBytes;

    if (outCacheIndex != ZR_NULL) {
        *outCacheIndex = 0;
    }
    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || outCacheIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    newCount = (TZrSize)function->callSiteCacheLength + 1;
    if (newCount > UINT16_MAX) {
        return ZR_FALSE;
    }

    newEntries = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionCallSiteCacheEntry) * newCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newEntries == ZR_NULL) {
        return ZR_FALSE;
    }

    copyBytes = sizeof(SZrFunctionCallSiteCacheEntry) * function->callSiteCacheLength;
    if (function->callSiteCaches != ZR_NULL && copyBytes > 0) {
        compiler_quickening_trace_callsite_aliases(state, function, function->callSiteCaches);
        memcpy(newEntries, function->callSiteCaches, copyBytes);
        ZrCore_Memory_RawFreeWithType(state->global,
                                      function->callSiteCaches,
                                      sizeof(SZrFunctionCallSiteCacheEntry) * function->callSiteCacheLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    memset(&newEntries[newCount - 1], 0, sizeof(SZrFunctionCallSiteCacheEntry));
    newEntries[newCount - 1].kind = (TZrUInt32)kind;
    newEntries[newCount - 1].instructionIndex = instructionIndex;
    newEntries[newCount - 1].memberEntryIndex = memberEntryIndex;
    newEntries[newCount - 1].deoptId = deoptId;
    newEntries[newCount - 1].argumentCount = argumentCount;

    function->callSiteCaches = newEntries;
    function->callSiteCacheLength = (TZrUInt32)newCount;
    *outCacheIndex = (TZrUInt16)(newCount - 1);
    return ZR_TRUE;
}

static TZrBool compiler_quicken_cached_calls(SZrState *state, SZrFunction *function) {
    TZrUInt32 index;

    if (state == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        EZrFunctionCallSiteCacheKind cacheKind;
        EZrInstructionCode quickenedOpcode;
        TZrUInt16 cacheIndex;
        TZrUInt32 argumentCount;

        switch (opcode) {
            case ZR_INSTRUCTION_ENUM(META_CALL):
                cacheKind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL;
                quickenedOpcode = ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED);
                break;
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
                cacheKind = ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL;
                quickenedOpcode = ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED);
                break;
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
                cacheKind = ZR_FUNCTION_CALLSITE_CACHE_KIND_META_TAIL_CALL;
                quickenedOpcode = ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED);
                break;
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
                cacheKind = ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_TAIL_CALL;
                quickenedOpcode = ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED);
                break;
            default:
                continue;
        }

        argumentCount = instruction->instruction.operand.operand1[1];
        if (argumentCount == 0) {
            continue;
        }

        if (!compiler_quickening_append_callsite_cache(state,
                                                       function,
                                                       cacheKind,
                                                       index,
                                                       ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE,
                                                       compiler_quickening_find_deopt_id(function, index),
                                                       argumentCount,
                                                       &cacheIndex)) {
            return ZR_FALSE;
        }

        instruction->instruction.operationCode = (TZrUInt16)quickenedOpcode;
        instruction->instruction.operand.operand1[1] = cacheIndex;
    }

    return ZR_TRUE;
}

static TZrBool compiler_quicken_member_slot_accesses(SZrState *state, SZrFunction *function) {
    ZrCompilerQuickeningSlotAlias *aliases = ZR_NULL;
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 aliasCount;
    TZrUInt32 index;
    TZrBool success = ZR_FALSE;

    if (state == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL ||
        function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    aliasCount = function->stackSize;
    if (aliasCount > 0) {
        aliases = (ZrCompilerQuickeningSlotAlias *)malloc(sizeof(*aliases) * aliasCount);
        if (aliases == ZR_NULL) {
            return ZR_FALSE;
        }
        compiler_quickening_clear_aliases(aliases, aliasCount);
    }

    blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
    if (blockStarts == ZR_NULL) {
        free(aliases);
        return ZR_FALSE;
    }

    if (!compiler_quickening_build_block_starts(function, blockStarts)) {
        free(blockStarts);
        free(aliases);
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        TZrUInt32 destinationSlot = instruction->instruction.operandExtra;

        if (blockStarts[index]) {
            compiler_quickening_clear_aliases(aliases, aliasCount);
        }

        if (opcode == ZR_INSTRUCTION_ENUM(GET_MEMBER) || opcode == ZR_INSTRUCTION_ENUM(SET_MEMBER)) {
            const SZrFunctionTypedLocalBinding *binding =
                    compiler_quickening_resolve_named_binding_for_slot(function,
                                                                       aliases,
                                                                       aliasCount,
                                                                       index,
                                                                       instruction->instruction.operand.operand1[0]);
            const SZrFunctionTypedTypeRef *receiverTypeRef = ZR_NULL;
            TZrUInt32 memberEntryIndex = instruction->instruction.operand.operand1[1];
            TZrUInt32 blockStartIndex = index;
            TZrUInt16 cacheIndex;
            SZrFunctionTypedTypeRef recoveredReceiverType;

            if (binding != ZR_NULL) {
                receiverTypeRef = &binding->type;
            } else {
                while (blockStartIndex > 0 && !blockStarts[blockStartIndex]) {
                    blockStartIndex--;
                }
                if (compiler_quickening_resolve_slot_type_ref_before_instruction_in_range(function,
                                                                                          blockStartIndex,
                                                                                          index,
                                                                                          instruction->instruction.operand.operand1[0],
                                                                                          0u,
                                                                                          &recoveredReceiverType)) {
                    receiverTypeRef = &recoveredReceiverType;
                }
            }

            if (compiler_quickening_type_ref_supports_static_member_slots(receiverTypeRef) &&
                compiler_quickening_member_entry_symbol_text(function, (TZrUInt16)memberEntryIndex) != ZR_NULL) {
                if (!compiler_quickening_append_callsite_cache(
                            state,
                            function,
                            opcode == ZR_INSTRUCTION_ENUM(GET_MEMBER)
                                    ? ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET
                                    : ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET,
                            index,
                            memberEntryIndex,
                            compiler_quickening_find_deopt_id(function, index),
                            0,
                            &cacheIndex)) {
                    free(blockStarts);
                    free(aliases);
                    return ZR_FALSE;
                }

                instruction->instruction.operationCode =
                        (TZrUInt16)(opcode == ZR_INSTRUCTION_ENUM(GET_MEMBER)
                                            ? ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)
                                            : ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT));
                instruction->instruction.operand.operand1[1] = cacheIndex;
                opcode = (EZrInstructionCode)instruction->instruction.operationCode;
            }
        }

        if (opcode == ZR_INSTRUCTION_ENUM(GET_STACK) || opcode == ZR_INSTRUCTION_ENUM(SET_STACK)) {
            TZrUInt32 sourceSlot = (TZrUInt32)instruction->instruction.operand.operand2[0];
            TZrUInt32 rootSlot = 0;

            if (compiler_quickening_resolve_alias_slot(function,
                                                       aliases,
                                                       aliasCount,
                                                       index,
                                                       sourceSlot,
                                                       &rootSlot)) {
                if (destinationSlot < aliasCount) {
                    aliases[destinationSlot].valid = ZR_TRUE;
                    aliases[destinationSlot].rootSlot = rootSlot;
                }
            } else {
                compiler_quickening_clear_alias(aliases, aliasCount, destinationSlot);
            }
            continue;
        }

        compiler_quickening_clear_call_argument_tracking(function,
                                                         instruction,
                                                         aliases,
                                                         ZR_NULL,
                                                         ZR_NULL,
                                                         ZR_NULL,
                                                         aliasCount);

        if (destinationSlot < aliasCount &&
            (opcode == ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT) ||
             opcode == ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT) ||
             compiler_quickening_instruction_writes_slot(instruction, destinationSlot))) {
            compiler_quickening_clear_alias(aliases, aliasCount, destinationSlot);
        }
    }

    success = ZR_TRUE;
    free(blockStarts);
    free(aliases);
    return success;
}

static TZrBool compiler_quicken_iter_loop_guards(SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength < 2) {
        return ZR_TRUE;
    }

    for (index = 0; index + 1 < function->instructionsLength; index++) {
        TZrInstruction *iterMoveNextInst = &function->instructionsList[index];
        TZrInstruction *jumpIfInst = &function->instructionsList[index + 1];
        TZrInt32 jumpOffset;

        EZrInstructionCode moveNextOpcode = (EZrInstructionCode)iterMoveNextInst->instruction.operationCode;
        if (moveNextOpcode != ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT) &&
            moveNextOpcode != ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT)) {
            continue;
        }
        if ((EZrInstructionCode)jumpIfInst->instruction.operationCode != ZR_INSTRUCTION_ENUM(JUMP_IF)) {
            continue;
        }
        if (jumpIfInst->instruction.operandExtra != iterMoveNextInst->instruction.operandExtra) {
            continue;
        }

        jumpOffset = jumpIfInst->instruction.operand.operand2[0];
        if (jumpOffset < INT16_MIN || jumpOffset > INT16_MAX) {
            continue;
        }

        iterMoveNextInst->instruction.operationCode =
                (TZrUInt16)(moveNextOpcode == ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT)
                                    ? ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE)
                                    : ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE));
        iterMoveNextInst->instruction.operand.operand1[1] = (TZrUInt16)((TZrInt16)jumpOffset);
    }

    return ZR_TRUE;
}

static TZrBool compiler_quicken_zero_arg_calls(SZrFunction *function) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *callInst = &function->instructionsList[index];

        if (callInst->instruction.operand.operand1[1] != 0) {
            continue;
        }

        switch ((EZrInstructionCode)callInst->instruction.operationCode) {
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(KNOWN_VM_TAIL_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_TAIL_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_KNOWN_NATIVE_TAIL_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(DYN_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(META_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS);
                break;
            case ZR_INSTRUCTION_ENUM(META_TAIL_CALL):
                callInst->instruction.operationCode =
                        (TZrUInt16)ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS);
                break;
            default:
                break;
        }
    }

    return ZR_TRUE;
}

static TZrBool compiler_quicken_meta_access(SZrState *state, SZrFunction *function) {
    TZrUInt32 index;

    if (state == ZR_NULL || function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        TZrInstruction *instruction = &function->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        EZrFunctionCallSiteCacheKind cacheKind;
        EZrInstructionCode quickenedOpcode;
        TZrUInt16 cacheIndex;
        TZrUInt32 memberEntryIndex;
        TZrUInt8 memberFlags;
        TZrBool isStaticAccessor;

        if (opcode == ZR_INSTRUCTION_ENUM(META_GET)) {
            memberEntryIndex = instruction->instruction.operand.operand1[1];
            memberFlags = compiler_quickening_member_entry_flags(function, memberEntryIndex);
            isStaticAccessor =
                    (TZrBool)((memberFlags & ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR) != 0);
            cacheKind = isStaticAccessor ? ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC
                                         : ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET;
            quickenedOpcode = isStaticAccessor ? ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED)
                                               : ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED);
        } else if (opcode == ZR_INSTRUCTION_ENUM(META_SET)) {
            memberEntryIndex = instruction->instruction.operand.operand1[1];
            memberFlags = compiler_quickening_member_entry_flags(function, memberEntryIndex);
            isStaticAccessor =
                    (TZrBool)((memberFlags & ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR) != 0);
            cacheKind = isStaticAccessor ? ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC
                                         : ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET;
            quickenedOpcode = isStaticAccessor ? ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED)
                                               : ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED);
        } else {
            continue;
        }

        if (!compiler_quickening_append_callsite_cache(state,
                                                       function,
                                                       cacheKind,
                                                       index,
                                                       memberEntryIndex,
                                                       compiler_quickening_find_deopt_id(function, index),
                                                       0,
                                                       &cacheIndex)) {
            return ZR_FALSE;
        }

        instruction->instruction.operationCode = (TZrUInt16)quickenedOpcode;
        instruction->instruction.operand.operand1[1] = cacheIndex;
    }

    return ZR_TRUE;
}

static TZrBool compiler_quickening_opcode_is_load_typed_arithmetic_probe_candidate(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_quickening_load_typed_arithmetic_candidate_reads_slot(const TZrInstruction *instruction,
                                                                              TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    if (!compiler_quickening_opcode_is_load_typed_arithmetic_probe_candidate(opcode)) {
        return ZR_FALSE;
    }

    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
            return instruction->instruction.operand.operand1[0] == slot;
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
            return ZR_FALSE;
        default:
            return instruction->instruction.operand.operand1[0] == slot ||
                   instruction->instruction.operand.operand1[1] == slot;
    }
}

static void compiler_quickening_collect_load_typed_arithmetic_probe_stats_in_function(
        const SZrFunction *function,
        SZrQuickeningLoadTypedArithmeticProbeStats *stats,
        TZrBool *blockStarts) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || stats == ZR_NULL || blockStarts == ZR_NULL ||
        function->instructionsLength == 0) {
        return;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *loadInstruction = &function->instructionsList[index];
        EZrInstructionCode loadOpcode = (EZrInstructionCode)loadInstruction->instruction.operationCode;
        TZrUInt32 loadedTempSlot;
        TZrUInt32 scan;

        if (loadOpcode != ZR_INSTRUCTION_ENUM(GET_STACK) && loadOpcode != ZR_INSTRUCTION_ENUM(GET_CONSTANT)) {
            continue;
        }
        if (loadInstruction->instruction.operandExtra == ZR_INSTRUCTION_USE_RET_FLAG) {
            continue;
        }

        loadedTempSlot = loadInstruction->instruction.operandExtra;
        for (scan = index + 1; scan < function->instructionsLength; scan++) {
            const TZrInstruction *scanInstruction = &function->instructionsList[scan];

            if (blockStarts[scan]) {
                break;
            }

            if (compiler_quickening_instruction_may_read_slot(scanInstruction, loadedTempSlot)) {
                if (compiler_quickening_load_typed_arithmetic_candidate_reads_slot(scanInstruction, loadedTempSlot)) {
                    if (loadOpcode == ZR_INSTRUCTION_ENUM(GET_STACK)) {
                        stats->getStackTypedArithmeticPairs++;
                    } else {
                        stats->getConstantTypedArithmeticPairs++;
                    }

                    if (compiler_quickening_instruction_writes_slot(scanInstruction, loadedTempSlot) ||
                        compiler_quickening_temp_slot_is_dead_after_instruction(function,
                                                                                blockStarts,
                                                                                scan,
                                                                                loadedTempSlot)) {
                        stats->safeFusionCandidates++;
                    } else {
                        stats->materializedLoadCandidates++;
                    }
                }
                break;
            }

            if (compiler_quickening_instruction_writes_slot(scanInstruction, loadedTempSlot)) {
                break;
            }
        }
    }
}

static TZrBool compiler_quickening_collect_load_typed_arithmetic_probe_stats_recursive(
        const SZrFunction *function,
        SZrQuickeningLoadTypedArithmeticProbeStats *stats) {
    static TZrUInt32 constantFunctionDepth = 0;
    TZrBool *blockStarts = ZR_NULL;
    TZrUInt32 childIndex;
    TZrBool success = ZR_FALSE;

    if (function == ZR_NULL) {
        return ZR_TRUE;
    }

    if (function->instructionsLength > 0) {
        blockStarts = (TZrBool *)malloc(sizeof(*blockStarts) * function->instructionsLength);
        if (blockStarts == ZR_NULL) {
            return ZR_FALSE;
        }
        if (!compiler_quickening_build_block_starts(function, blockStarts)) {
            free(blockStarts);
            return ZR_FALSE;
        }
        compiler_quickening_collect_load_typed_arithmetic_probe_stats_in_function(function, stats, blockStarts);
        free(blockStarts);
        blockStarts = ZR_NULL;
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (!compiler_quickening_collect_load_typed_arithmetic_probe_stats_recursive(&function->childFunctionList[childIndex],
                                                                                    stats)) {
            goto cleanup;
        }
    }

    if (constantFunctionDepth == 0 && function->constantValueList != ZR_NULL) {
        constantFunctionDepth++;
        for (TZrUInt32 constantIndex = 0; constantIndex < function->constantValueLength; constantIndex++) {
            const SZrFunction *constantFunction =
                    compiler_quickening_function_from_const_vm_constant_value(&function->constantValueList[constantIndex]);

            if (constantFunction == ZR_NULL ||
                constantFunction == function ||
                compiler_quickening_constant_function_matches_child(function, constantFunction)) {
                continue;
            }

            if (!compiler_quickening_collect_load_typed_arithmetic_probe_stats_recursive(constantFunction, stats)) {
                constantFunctionDepth--;
                goto cleanup;
            }
        }
        constantFunctionDepth--;
    }

    success = ZR_TRUE;

cleanup:
    free(blockStarts);
    return success;
}

TZrBool ZrParser_Quickening_CollectLoadTypedArithmeticProbeStats(
        const SZrFunction *function,
        SZrQuickeningLoadTypedArithmeticProbeStats *outStats) {
    if (outStats == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outStats, 0, sizeof(*outStats));
    if (function == ZR_NULL) {
        return ZR_TRUE;
    }

    return compiler_quickening_collect_load_typed_arithmetic_probe_stats_recursive(function, outStats);
}

static TZrBool compiler_quicken_child_functions(SZrState *state,
                                                SZrFunction *function,
                                                TZrBool recurseChildren) {
    TZrUInt32 childIndex;

    if (function == ZR_NULL) {
        return ZR_TRUE;
    }

    if (function->childFunctionList != ZR_NULL) {
        for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
            function->childFunctionList[childIndex].ownerFunction = function;
        }
    }

    ZR_QUICKENING_RUN_PASS("meta_access", compiler_quicken_meta_access(state, function));
    ZR_QUICKENING_RUN_PASS("known_calls", compiler_quicken_known_calls(state, function));
    ZR_QUICKENING_RUN_PASS("cached_calls", compiler_quicken_cached_calls(state, function));
    ZR_QUICKENING_RUN_PASS("iter_loop_guards", compiler_quicken_iter_loop_guards(function));
    ZR_QUICKENING_RUN_PASS("zero_arg_calls", compiler_quicken_zero_arg_calls(function));
    ZR_QUICKENING_RUN_PASS("array_int_index_accesses", compiler_quicken_array_int_index_accesses(state, function));
    ZR_QUICKENING_RUN_PASS("member_slot_accesses", compiler_quicken_member_slot_accesses(state, function));
    ZR_QUICKENING_RUN_PASS("compact_nops_1", compiler_quickening_compact_nops(state, function));
    ZR_QUICKENING_RUN_PASS("fold_super_array_add_int4_bursts",
                           compiler_quickening_fold_super_array_add_int4_bursts(function));
    ZR_QUICKENING_RUN_PASS("compact_nops_2", compiler_quickening_compact_nops(state, function));
    ZR_QUICKENING_RUN_PASS("fold_super_array_add_int4_const_bursts",
                           compiler_quickening_fold_super_array_add_int4_const_bursts(function));
    ZR_QUICKENING_RUN_PASS("compact_nops_3", compiler_quickening_compact_nops(state, function));
    ZR_QUICKENING_RUN_PASS("fold_super_array_fill_int4_const_loops",
                           compiler_quickening_fold_super_array_fill_int4_const_loops(function));
    ZR_QUICKENING_RUN_PASS("compact_nops_4", compiler_quickening_compact_nops(state, function));
    ZR_QUICKENING_RUN_PASS("fold_right_const_arithmetic",
                           compiler_quickening_fold_right_const_arithmetic(state, function));
    ZR_QUICKENING_RUN_PASS("fuse_materialized_const_signed_arithmetic",
                           compiler_quickening_fuse_materialized_const_signed_arithmetic(function));
    ZR_QUICKENING_RUN_PASS("fuse_materialized_stack_const_signed_arithmetic",
                           compiler_quickening_fuse_materialized_stack_const_signed_arithmetic(function));
    ZR_QUICKENING_RUN_PASS("dematerialize_dead_signed_load_arithmetic",
                           compiler_quickening_dematerialize_dead_signed_load_arithmetic(function));
    ZR_QUICKENING_RUN_PASS("compact_nops_5", compiler_quickening_compact_nops(state, function));
    ZR_QUICKENING_RUN_PASS("fold_stack_self_update_int_const",
                           compiler_quickening_fold_stack_self_update_int_const(function));
    ZR_QUICKENING_RUN_PASS("compact_nops_6", compiler_quickening_compact_nops(state, function));
    ZR_QUICKENING_RUN_PASS("fold_direct_result_stores_1",
                           compiler_quickening_fold_direct_result_stores(function));
    ZR_QUICKENING_RUN_PASS("compact_nops_7", compiler_quickening_compact_nops(state, function));
    ZR_QUICKENING_RUN_PASS("forward_super_array_store_to_load_reads",
                           compiler_quickening_forward_super_array_store_to_load_reads(function));
    ZR_QUICKENING_RUN_PASS("compact_nops_8", compiler_quickening_compact_nops(state, function));
    ZR_QUICKENING_RUN_PASS("forward_get_stack_copy_reads",
                           compiler_quickening_forward_get_stack_copy_reads(function));
    ZR_QUICKENING_RUN_PASS("compact_nops_9", compiler_quickening_compact_nops(state, function));
    ZR_QUICKENING_RUN_PASS("fuse_jump_if_greater_signed",
                           compiler_quickening_fuse_jump_if_greater_signed(function));
    ZR_QUICKENING_RUN_PASS("fuse_jump_if_not_equal_signed",
                           compiler_quickening_fuse_jump_if_not_equal_signed(function));
    ZR_QUICKENING_RUN_PASS("fuse_jump_if_not_equal_signed_const",
                           compiler_quickening_fuse_jump_if_not_equal_signed_const(function));
    ZR_QUICKENING_RUN_PASS("compact_nops_10", compiler_quickening_compact_nops(state, function));
    ZR_QUICKENING_RUN_PASS("fuse_jump_if_not_equal_signed_late",
                           compiler_quickening_fuse_jump_if_not_equal_signed(function));
    ZR_QUICKENING_RUN_PASS("fuse_jump_if_not_equal_signed_const_late",
                           compiler_quickening_fuse_jump_if_not_equal_signed_const(function));
    ZR_QUICKENING_RUN_PASS("compact_nops_10b", compiler_quickening_compact_nops(state, function));

    /*
     * Later peephole passes plus NOP compaction can expose fresh producer ->
     * SET_STACK adjacency that was not present during the first fold pass.
     * Run one final direct-result-store sweep on the stabilized stream so loop
     * zero-inits and similar pure stores land directly in their final locals.
     */
    ZR_QUICKENING_RUN_PASS("fold_direct_result_stores_2",
                           compiler_quickening_fold_direct_result_stores(function));
    ZR_QUICKENING_RUN_PASS("compact_nops_11", compiler_quickening_compact_nops(state, function));
    /*
     * Later store-folding and GET_STACK forwarding can expose fresh direct
     * callee provenance that was not visible during the first known_calls pass.
     * Re-run known call lowering on the stabilized stream so loop-local callable
     * copies such as exported child functions do not stay generic.
     */
    ZR_QUICKENING_RUN_PASS("known_calls_late", compiler_quicken_known_calls(state, function));
    ZR_QUICKENING_RUN_PASS("known_native_member_calls_late",
                           compiler_quickening_fuse_known_native_member_calls(function));
    ZR_QUICKENING_RUN_PASS("zero_arg_calls_late", compiler_quicken_zero_arg_calls(function));
    /*
     * member_slot_accesses and late known-call lowering can expose typed call
     * results that the first array_int_index_accesses pass could not see yet.
     * Re-run the slot-kind-driven specialization pass on the stabilized stream
     * so arithmetic/equality consumers of KNOWN_*_CALL results do not stay
     * generic.
     */
    ZR_QUICKENING_RUN_PASS("array_int_index_accesses_late",
                           compiler_quicken_array_int_index_accesses(state, function));
    ZR_QUICKENING_RUN_PASS("forward_get_stack_copy_reads_late",
                           compiler_quickening_forward_get_stack_copy_reads(function));
    ZR_QUICKENING_RUN_PASS("fuse_materialized_const_signed_arithmetic_late",
                           compiler_quickening_fuse_materialized_const_signed_arithmetic(function));
    ZR_QUICKENING_RUN_PASS("fuse_materialized_stack_const_signed_arithmetic_late",
                           compiler_quickening_fuse_materialized_stack_const_signed_arithmetic(function));
    ZR_QUICKENING_RUN_PASS("dematerialize_dead_signed_load_arithmetic_late",
                           compiler_quickening_dematerialize_dead_signed_load_arithmetic(function));
    ZR_QUICKENING_RUN_PASS("fuse_known_vm_member_call_load1_u8",
                           compiler_quickening_fuse_known_vm_member_call_load1_u8(function));
    ZR_QUICKENING_RUN_PASS("rewrite_null_constant_loads",
                           compiler_quickening_rewrite_null_constant_loads(function));
    ZR_QUICKENING_RUN_PASS("compact_nops_12", compiler_quickening_compact_nops(state, function));
    ZR_QUICKENING_RUN_PASS("super_array_items_cache_bindings",
                           compiler_quickening_insert_super_array_items_cache_bindings(state, function));

    if (recurseChildren && !function->childFunctionGraphIsBorrowed) {
        for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
            compiler_quickening_trace_pass("begin", "child_function", &function->childFunctionList[childIndex]);
            if (!compiler_quicken_child_functions(state, &function->childFunctionList[childIndex], ZR_TRUE)) {
                compiler_quickening_trace_pass("fail", "child_function", &function->childFunctionList[childIndex]);
                return ZR_FALSE;
            }
            compiler_quickening_trace_pass("done", "child_function", &function->childFunctionList[childIndex]);
        }
    }

    ZR_QUICKENING_RUN_PASS("constant_function_values",
                           compiler_quickening_quicken_constant_function_values(state, function));
    compiler_quickening_rebind_constant_function_values_to_children(function);

    return ZR_TRUE;
}

TZrBool compiler_quicken_execbc_function(SZrState *state, SZrFunction *function) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quicken_child_functions(state, function, ZR_TRUE)) {
        return ZR_FALSE;
    }

    ZrCore_Function_ClearChildOwnerLinks(function);
    return ZR_TRUE;
}

TZrBool compiler_quicken_execbc_function_shallow(SZrState *state, SZrFunction *function) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_quicken_child_functions(state, function, ZR_FALSE)) {
        return ZR_FALSE;
    }

    ZrCore_Function_ClearChildOwnerLinks(function);
    return ZR_TRUE;
}

#undef ZR_QUICKENING_RUN_PASS
