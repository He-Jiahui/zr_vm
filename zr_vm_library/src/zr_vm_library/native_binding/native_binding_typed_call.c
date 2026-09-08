#include "native_binding_internal.h"

static TZrBool typed_native_type(const TZrChar *name, SZrFunctionTypedTypeRef *type) {
    static const struct { const TZrChar *name; EZrValueType type; } primitives[] = {
        {"null", ZR_VALUE_TYPE_NULL}, {"bool", ZR_VALUE_TYPE_BOOL}, {"i8", ZR_VALUE_TYPE_INT8},
        {"i16", ZR_VALUE_TYPE_INT16}, {"i32", ZR_VALUE_TYPE_INT32}, {"int", ZR_VALUE_TYPE_INT64},
        {"u8", ZR_VALUE_TYPE_UINT8}, {"u16", ZR_VALUE_TYPE_UINT16}, {"u32", ZR_VALUE_TYPE_UINT32},
        {"uint", ZR_VALUE_TYPE_UINT64},
        {"float", ZR_VALUE_TYPE_FLOAT}, {"double", ZR_VALUE_TYPE_DOUBLE}, {"string", ZR_VALUE_TYPE_STRING},
        {"object", ZR_VALUE_TYPE_OBJECT}
    };
    memset(type, 0, sizeof(*type));
    type->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    if (name == ZR_NULL) return ZR_FALSE;
    for (TZrSize index = 0u; index < ZR_ARRAY_COUNT(primitives); ++index) {
        if (strcmp(name, primitives[index].name) == 0) {
            type->baseType = primitives[index].type;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool native_registry_resolve_typed_call_binding(SZrState *state,
        const SZrTypeValue *callable, SZrCallBindingTarget *target,
        TZrUInt64 *signatureHash, TZrPtr userData) {
    ZrLibrary_NativeRegistryState *registry = userData;
    ZrLibBindingEntry *entry;
    SZrClosureNative *closure;
    SZrFunction temporary = {0};
    SZrFunctionMetadataParameter *parameters = ZR_NULL;
    SZrFunctionTypedLocalBinding *locals = ZR_NULL;
    const ZrLibFunctionDescriptor *descriptor;
    TZrBool valid = ZR_FALSE;
    TZrSize count;
    if (registry == ZR_NULL || callable == ZR_NULL || !callable->isNative ||
        callable->type != ZR_VALUE_TYPE_CLOSURE) return ZR_FALSE;
    closure = ZR_CAST_NATIVE_CLOSURE(state, callable->value.object);
    entry = native_registry_find_binding(registry, closure);
    if (entry == ZR_NULL) {
        return registry->hostTypedCallBindingResolver != ZR_NULL &&
                registry->hostTypedCallBindingResolver(state, callable, target, signatureHash,
                        registry->hostTypedCallBindingResolverUserData);
    }
    if (entry->bindingKind != ZR_LIB_RESOLVED_BINDING_FUNCTION || closure->callBindingGeneration == 0u ||
        (descriptor = entry->descriptor.functionDescriptor) == ZR_NULL) return ZR_FALSE;
    count = descriptor->parameterCount;
    if (count > UINT16_MAX || (count != 0u && descriptor->parameters == ZR_NULL) ||
        descriptor->genericParameterCount != 0u || descriptor->minArgumentCount != count ||
        descriptor->maxArgumentCount != count ||
        !typed_native_type(descriptor->returnTypeName, &temporary.callableReturnType)) return ZR_FALSE;
    if (count != 0u) {
        parameters = ZrCore_Memory_RawMallocWithType(state->global, sizeof(*parameters) * count,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        locals = ZrCore_Memory_RawMallocWithType(state->global, sizeof(*locals) * count,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (parameters == ZR_NULL || locals == ZR_NULL) goto cleanup;
        memset(parameters, 0, sizeof(*parameters) * count);
        memset(locals, 0, sizeof(*locals) * count);
    }
    for (TZrSize index = 0u; index < count; ++index) {
        if (!typed_native_type(descriptor->parameters[index].typeName, &parameters[index].type)) goto cleanup;
        locals[index].stackSlot = (TZrUInt32)index;
        switch (descriptor->parameters[index].passingMode) {
            case ZR_LIB_PARAMETER_PASSING_MODE_VALUE: locals[index].roleFlags = ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE; break;
            case ZR_LIB_PARAMETER_PASSING_MODE_IN: locals[index].roleFlags = ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_IN; break;
            case ZR_LIB_PARAMETER_PASSING_MODE_OUT: locals[index].roleFlags = ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_OUT; break;
            case ZR_LIB_PARAMETER_PASSING_MODE_REF: locals[index].roleFlags = ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_REF; break;
            default: goto cleanup;
        }
    }
    temporary.parameterCount = (TZrUInt16)count;
    temporary.parameterMetadataCount = (TZrUInt32)count;
    temporary.parameterMetadata = parameters;
    temporary.typedLocalBindings = locals;
    temporary.typedLocalBindingLength = (TZrUInt32)count;
    temporary.hasCallableReturnType = ZR_TRUE;
    *signatureHash = ZrCore_CallBinding_FunctionSignatureHash(&temporary);
    target->targetKind = ZR_CALL_BINDING_TARGET_NATIVE;
    target->native.function = closure->nativeFunction;
    target->callableObject = callable->value.object;
    target->targetGeneration = closure->callBindingGeneration;
    valid = *signatureHash != 0u && target->native.function != ZR_NULL;
cleanup:
    if (parameters != ZR_NULL) ZrCore_Memory_RawFreeWithType(state->global,
            parameters, sizeof(*parameters) * count, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (locals != ZR_NULL) ZrCore_Memory_RawFreeWithType(state->global,
            locals, sizeof(*locals) * count, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return valid;
}
