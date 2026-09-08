#include "native_binding_internal.h"

#include "zr_vm_core/hash.h"

static const TZrByte CZrNativeContractHashPrefix[] = {
        'z', 'r', '.', 'n', 'a', 't', 'i', 'v', 'e', '.', 'c', 'o', 'n', 't', 'r', 'a', 'c', 't', '.', 'v', '1', '\0'
};

typedef struct SZrNativeContractHash {
    TZrUInt64 value;
    TZrBool valid;
} SZrNativeContractHash;

static void native_contract_hash_bytes(SZrNativeContractHash *hash,
                                       const void *bytes, TZrSize length) {
    TZrByte prefix[sizeof(CZrNativeContractHashPrefix) + sizeof(TZrUInt64) * 2u];
    TZrSize offset = sizeof(CZrNativeContractHashPrefix);
    if (!hash->valid || (bytes == ZR_NULL && length != 0u)) {
        hash->valid = ZR_FALSE;
        return;
    }
    memcpy(prefix, CZrNativeContractHashPrefix, sizeof(CZrNativeContractHashPrefix));
    for (TZrUInt32 index = 0u; index < 8u; ++index) {
        prefix[offset + index] = (TZrByte)(hash->value >> (index * 8u));
        prefix[offset + 8u + index] = (TZrByte)((TZrUInt64)length >> (index * 8u));
    }
    hash->value = ZrCore_Hash_CreateStable64WithPrefix(prefix, sizeof(prefix), bytes, length);
}

static void native_contract_hash_u64(SZrNativeContractHash *hash, TZrUInt64 value) {
    TZrByte bytes[8];
    for (TZrUInt32 index = 0u; index < 8u; ++index) bytes[index] = (TZrByte)(value >> (index * 8u));
    native_contract_hash_bytes(hash, bytes, sizeof(bytes));
}

static void native_contract_hash_string(SZrNativeContractHash *hash, const TZrChar *value) {
    native_contract_hash_bytes(hash, value, value != ZR_NULL ? strlen(value) : 0u);
}

static TZrBool native_contract_hash_array(SZrNativeContractHash *hash,
                                         const void *array, TZrSize count,
                                         TZrSize elementSize) {
    if (!hash->valid || count > (TZrSize)UINT32_MAX ||
        (count != 0u && (array == ZR_NULL || count > ZR_MAX_SIZE / elementSize))) {
        hash->valid = ZR_FALSE;
        return ZR_FALSE;
    }
    native_contract_hash_u64(hash, (TZrUInt64)count);
    return ZR_TRUE;
}

static void native_contract_hash_parameters(SZrNativeContractHash *hash,
                                            const ZrLibParameterDescriptor *parameters,
                                            TZrSize count) {
    if (!native_contract_hash_array(hash, parameters, count, sizeof(*parameters))) return;
    for (TZrSize index = 0u; index < count; ++index) {
        native_contract_hash_string(hash, parameters[index].name);
        native_contract_hash_string(hash, parameters[index].typeName);
        native_contract_hash_u64(hash, parameters[index].passingMode);
    }
}

static void native_contract_hash_generics(SZrNativeContractHash *hash,
                                          const ZrLibGenericParameterDescriptor *parameters,
                                          TZrSize count) {
    if (!native_contract_hash_array(hash, parameters, count, sizeof(*parameters))) return;
    for (TZrSize index = 0u; index < count; ++index) {
        const ZrLibGenericParameterDescriptor *parameter = &parameters[index];
        native_contract_hash_string(hash, parameter->name);
        if (!native_contract_hash_array(hash, parameter->constraintTypeNames,
                parameter->constraintTypeCount, sizeof(*parameter->constraintTypeNames))) return;
        for (TZrSize constraint = 0u; constraint < parameter->constraintTypeCount; ++constraint)
            native_contract_hash_string(hash, parameter->constraintTypeNames[constraint]);
    }
}

static void native_contract_hash_function(SZrNativeContractHash *hash,
                                          const ZrLibFunctionDescriptor *function) {
    native_contract_hash_string(hash, function->name);
    native_contract_hash_u64(hash, function->minArgumentCount);
    native_contract_hash_u64(hash, function->maxArgumentCount);
    native_contract_hash_string(hash, function->returnTypeName);
    native_contract_hash_parameters(hash, function->parameters, function->parameterCount);
    native_contract_hash_generics(hash, function->genericParameters, function->genericParameterCount);
    native_contract_hash_u64(hash, function->contractRole);
    native_contract_hash_u64(hash, function->dispatchFlags);
}

static void native_contract_hash_method(SZrNativeContractHash *hash,
                                        const ZrLibMethodDescriptor *method) {
    native_contract_hash_string(hash, method->name);
    native_contract_hash_u64(hash, method->minArgumentCount);
    native_contract_hash_u64(hash, method->maxArgumentCount);
    native_contract_hash_string(hash, method->returnTypeName);
    native_contract_hash_u64(hash, method->isStatic);
    native_contract_hash_parameters(hash, method->parameters, method->parameterCount);
    native_contract_hash_generics(hash, method->genericParameters, method->genericParameterCount);
    native_contract_hash_u64(hash, method->contractRole);
    native_contract_hash_u64(hash, method->dispatchFlags);
    native_contract_hash_string(hash, method->propertyName);
    native_contract_hash_u64(hash, method->propertyReferenceAccess);
    native_contract_hash_u64(hash, method->propertyExportsWritableRef);
}

static void native_contract_hash_meta(SZrNativeContractHash *hash,
                                      const ZrLibMetaMethodDescriptor *method) {
    native_contract_hash_u64(hash, method->metaType);
    native_contract_hash_u64(hash, method->minArgumentCount);
    native_contract_hash_u64(hash, method->maxArgumentCount);
    native_contract_hash_string(hash, method->returnTypeName);
    native_contract_hash_parameters(hash, method->parameters, method->parameterCount);
    native_contract_hash_generics(hash, method->genericParameters, method->genericParameterCount);
    native_contract_hash_u64(hash, method->contractRole);
    native_contract_hash_u64(hash, method->dispatchFlags);
}

static void native_contract_hash_type(SZrNativeContractHash *hash,
                                      const ZrLibTypeDescriptor *type) {
    native_contract_hash_string(hash, type->name);
    native_contract_hash_u64(hash, type->prototypeType);
    native_contract_hash_string(hash, type->extendsTypeName);
    if (!native_contract_hash_array(hash, type->implementsTypeNames,
            type->implementsTypeCount, sizeof(*type->implementsTypeNames))) return;
    for (TZrSize index = 0u; index < type->implementsTypeCount; ++index)
        native_contract_hash_string(hash, type->implementsTypeNames[index]);
    native_contract_hash_generics(hash, type->genericParameters, type->genericParameterCount);
    native_contract_hash_u64(hash, type->protocolMask);
    native_contract_hash_u64(hash, type->allowValueConstruction);
    native_contract_hash_u64(hash, type->allowBoxedConstruction);
    native_contract_hash_string(hash, type->constructorSignature);
    native_contract_hash_string(hash, type->enumValueTypeName);
    native_contract_hash_string(hash, type->ffiLoweringKind);
    native_contract_hash_string(hash, type->ffiViewTypeName);
    native_contract_hash_string(hash, type->ffiUnderlyingTypeName);
    native_contract_hash_string(hash, type->ffiOwnerMode);
    native_contract_hash_string(hash, type->ffiReleaseHook);
    if (!native_contract_hash_array(hash, type->fields, type->fieldCount, sizeof(*type->fields))) return;
    for (TZrSize index = 0u; index < type->fieldCount; ++index) {
        const ZrLibFieldDescriptor *field = &type->fields[index];
        native_contract_hash_string(hash, field->name);
        native_contract_hash_string(hash, field->typeName);
        native_contract_hash_u64(hash, field->contractRole);
        native_contract_hash_u64(hash, field->runtimeOnly);
        native_contract_hash_u64(hash, field->isReadonly);
    }
    if (!native_contract_hash_array(hash, type->methods, type->methodCount, sizeof(*type->methods))) return;
    for (TZrSize index = 0u; index < type->methodCount; ++index)
        native_contract_hash_method(hash, &type->methods[index]);
    if (!native_contract_hash_array(hash, type->metaMethods, type->metaMethodCount, sizeof(*type->metaMethods))) return;
    for (TZrSize index = 0u; index < type->metaMethodCount; ++index)
        native_contract_hash_meta(hash, &type->metaMethods[index]);
    if (!native_contract_hash_array(hash, type->enumMembers, type->enumMemberCount, sizeof(*type->enumMembers))) return;
    for (TZrSize index = 0u; index < type->enumMemberCount; ++index) {
        const ZrLibEnumMemberDescriptor *member = &type->enumMembers[index];
        TZrUInt64 floatBits = 0u;
        memcpy(&floatBits, &member->floatValue, sizeof(floatBits));
        native_contract_hash_string(hash, member->name);
        native_contract_hash_u64(hash, member->kind);
        native_contract_hash_u64(hash, (TZrUInt64)member->intValue);
        native_contract_hash_u64(hash, floatBits);
        native_contract_hash_string(hash, member->stringValue);
        native_contract_hash_u64(hash, member->boolValue);
    }
}

TZrUInt64 native_registry_call_binding_type_hash(const ZrLibTypeDescriptor *type) {
    SZrNativeContractHash hash = {0u, ZR_TRUE};
    if (type == ZR_NULL) return 0u;
    native_contract_hash_string(&hash, "type");
    native_contract_hash_type(&hash, type);
    return hash.valid ? hash.value : 0u;
}

TZrUInt64 ZrLibrary_NativeRegistry_ComputeModuleSignatureHash(const ZrLibModuleDescriptor *module) {
    SZrNativeContractHash hash = {0u, ZR_TRUE};
    if (module == ZR_NULL || module->moduleName == ZR_NULL) return 0u;
    native_contract_hash_string(&hash, "module");
    native_contract_hash_string(&hash, module->moduleName);
    native_contract_hash_string(&hash, module->moduleVersion);
    native_contract_hash_string(&hash, module->publicContractHash);
    native_contract_hash_u64(&hash, module->abiVersion);
    native_contract_hash_u64(&hash, module->minRuntimeAbi);
    native_contract_hash_u64(&hash, module->requiredCapabilities);
    native_contract_hash_u64(&hash, module->providerPhase);
    native_contract_hash_u64(&hash, module->providerContractRole);
    native_contract_hash_u64(&hash, module->isContractOnly);
    if (!native_contract_hash_array(&hash, module->functions, module->functionCount, sizeof(*module->functions))) return 0u;
    for (TZrSize index = 0u; index < module->functionCount; ++index)
        native_contract_hash_function(&hash, &module->functions[index]);
    if (!native_contract_hash_array(&hash, module->constants, module->constantCount, sizeof(*module->constants))) return 0u;
    for (TZrSize index = 0u; index < module->constantCount; ++index) {
        native_contract_hash_string(&hash, module->constants[index].name);
        native_contract_hash_string(&hash, module->constants[index].typeName);
        native_contract_hash_u64(&hash, module->constants[index].kind);
    }
    if (!native_contract_hash_array(&hash, module->moduleLinks, module->moduleLinkCount, sizeof(*module->moduleLinks))) return 0u;
    for (TZrSize index = 0u; index < module->moduleLinkCount; ++index) {
        native_contract_hash_string(&hash, module->moduleLinks[index].name);
        native_contract_hash_string(&hash, module->moduleLinks[index].moduleName);
    }
    if (!native_contract_hash_array(&hash, module->types, module->typeCount, sizeof(*module->types))) return 0u;
    for (TZrSize index = 0u; index < module->typeCount; ++index)
        native_contract_hash_type(&hash, &module->types[index]);
    if (!native_contract_hash_array(&hash, module->attributeRoles, module->attributeRoleCount, sizeof(*module->attributeRoles))) return 0u;
    for (TZrSize index = 0u; index < module->attributeRoleCount; ++index) {
        const ZrLibAttributeRoleDescriptor *role = &module->attributeRoles[index];
        native_contract_hash_string(&hash, role->qualifiedName);
        native_contract_hash_u64(&hash, role->attributeId);
        native_contract_hash_u64(&hash, role->role);
        native_contract_hash_u64(&hash, role->targetFlags);
        native_contract_hash_u64(&hash, role->retention);
        native_contract_hash_u64(&hash, role->repeatable);
        native_contract_hash_string(&hash, role->typeName);
    }
    if (!native_contract_hash_array(&hash, module->canonicalTypeRoles, module->canonicalTypeRoleCount, sizeof(*module->canonicalTypeRoles))) return 0u;
    for (TZrSize index = 0u; index < module->canonicalTypeRoleCount; ++index) {
        const ZrLibCanonicalTypeRoleDescriptor *role = &module->canonicalTypeRoles[index];
        native_contract_hash_string(&hash, role->canonicalName);
        native_contract_hash_u64(&hash, role->role);
        native_contract_hash_u64(&hash, role->parentRole);
        native_contract_hash_u64(&hash, role->surfaceFlags);
        native_contract_hash_u64(&hash, role->projectionKind);
    }
    return hash.valid ? hash.value : 0u;
}
