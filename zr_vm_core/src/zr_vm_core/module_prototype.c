//
// Prototype materialization and registration helpers for modules.
//

#include "module_internal.h"
#include "zr_vm_core/reflection.h"
#include <string.h>

typedef struct SZrPrototypeMetadataFieldMapping {
    const TZrChar *metadataFieldName;
    const TZrChar *hiddenFieldName;
} SZrPrototypeMetadataFieldMapping;

static const SZrPrototypeMetadataFieldMapping kModulePrototypeFfiMetadataFieldMappings[] = {
        {"ffiLoweringKind", "__zr_ffiLoweringKind"},
        {"ffiViewTypeName", "__zr_ffiViewTypeName"},
        {"ffiUnderlyingTypeName", "__zr_ffiUnderlyingTypeName"},
        {"ffiOwnerMode", "__zr_ffiOwnerMode"},
        {"ffiReleaseHook", "__zr_ffiReleaseHook"},
};
static const TZrChar *kModulePrototypeEnumMembersFieldName = "__zr_enumMembers";
static const TZrChar *kModulePrototypeEnumValueTypeFieldName = "__zr_enumValueTypeName";
static const TZrChar *kModulePrototypeEnumValueFieldName = "__zr_enumValue";
static const TZrChar *kModulePrototypeEnumNameFieldName = "__zr_enumName";

static void register_prototype_in_global_scope(SZrState *state,
                                               SZrString *typeName,
                                               const SZrTypeValue *prototypeValue) {
    SZrObject *globalObject;
    SZrTypeValue key;

    if (state == ZR_NULL || state->global == ZR_NULL || typeName == ZR_NULL || prototypeValue == ZR_NULL) {
        return;
    }

    if (state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return;
    }

    globalObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    if (globalObject == ZR_NULL) {
        return;
    }

    zr_module_init_string_key(state, &key, typeName);
    ZrCore_Object_SetValue(state, globalObject, &key, prototypeValue);
}

static SZrObjectPrototype *find_prototype_in_global_scope(SZrState *state, SZrString *typeName) {
    SZrObject *globalObject;
    SZrTypeValue key;
    const SZrTypeValue *registeredValue;
    SZrObjectPrototype *prototype;

    if (state == ZR_NULL || state->global == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global->zrObject.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    globalObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
    if (globalObject == ZR_NULL) {
        return ZR_NULL;
    }

    zr_module_init_string_key(state, &key, typeName);
    registeredValue = ZrCore_Object_GetValue(state, globalObject, &key);
    if (registeredValue == ZR_NULL || registeredValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }

    prototype = (SZrObjectPrototype *)ZR_CAST_OBJECT(state, registeredValue->value.object);
    if (prototype == ZR_NULL || prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_INVALID) {
        return ZR_NULL;
    }

    return prototype;
}

static SZrObjectPrototype *find_prototype_in_qualified_module(SZrState *state, const TZrChar *typeNameText) {
    const TZrChar *genericStart;
    const TZrChar *lastDot;
    TZrSize moduleNameLength;
    TZrSize exportNameLength;
    TZrChar moduleNameBuffer[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
    TZrChar exportNameBuffer[ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH];
    SZrString *moduleName;
    SZrString *exportName;
    struct SZrObjectModule *qualifiedModule;
    const SZrTypeValue *exportedValue;

    if (state == ZR_NULL || typeNameText == ZR_NULL) {
        return ZR_NULL;
    }

    genericStart = strchr(typeNameText, '<');
    lastDot = strrchr(typeNameText, '.');
    if (lastDot == ZR_NULL || lastDot == typeNameText || lastDot[1] == '\0' ||
        (genericStart != ZR_NULL && lastDot > genericStart)) {
        return ZR_NULL;
    }

    moduleNameLength = (TZrSize)(lastDot - typeNameText);
    exportNameLength = genericStart != ZR_NULL
                               ? (TZrSize)(genericStart - (lastDot + 1))
                               : strlen(lastDot + 1);
    if (moduleNameLength == 0 || exportNameLength == 0 ||
        moduleNameLength >= sizeof(moduleNameBuffer) ||
        exportNameLength >= sizeof(exportNameBuffer)) {
        return ZR_NULL;
    }

    memcpy(moduleNameBuffer, typeNameText, moduleNameLength);
    moduleNameBuffer[moduleNameLength] = '\0';
    memcpy(exportNameBuffer, lastDot + 1, exportNameLength);
    exportNameBuffer[exportNameLength] = '\0';

    moduleName = ZrCore_String_Create(state, moduleNameBuffer, moduleNameLength);
    exportName = ZrCore_String_Create(state, exportNameBuffer, exportNameLength);
    if (moduleName == ZR_NULL || exportName == ZR_NULL) {
        return ZR_NULL;
    }

    qualifiedModule = ZrCore_Module_GetFromCache(state, moduleName);
    if (qualifiedModule == ZR_NULL) {
        qualifiedModule = ZrCore_Module_ImportByPath(state, moduleName);
    }
    if (qualifiedModule == ZR_NULL) {
        return ZR_NULL;
    }

    exportedValue = ZrCore_Module_GetPubExport(state, qualifiedModule, exportName);
    if (exportedValue == ZR_NULL || exportedValue->type != ZR_VALUE_TYPE_OBJECT || exportedValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return (SZrObjectPrototype *)ZR_CAST_OBJECT(state, exportedValue->value.object);
}

static TZrBool ensure_prototype_instance_storage(SZrState *state, SZrFunction *entryFunction) {
    struct SZrObjectPrototype **newStorage;
    TZrSize storageBytes;

    if (state == ZR_NULL || state->global == ZR_NULL || entryFunction == ZR_NULL || entryFunction->prototypeCount == 0) {
        return ZR_FALSE;
    }

    if (entryFunction->prototypeInstances != ZR_NULL &&
        entryFunction->prototypeInstancesLength >= entryFunction->prototypeCount) {
        return ZR_TRUE;
    }

    storageBytes = entryFunction->prototypeCount * sizeof(struct SZrObjectPrototype *);
    newStorage = (struct SZrObjectPrototype **)ZrCore_Memory_RawMalloc(state->global, storageBytes);
    if (newStorage == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(newStorage, 0, storageBytes);
    if (entryFunction->prototypeInstances != ZR_NULL && entryFunction->prototypeInstancesLength > 0) {
        TZrSize copyCount = entryFunction->prototypeInstancesLength;
        if (copyCount > entryFunction->prototypeCount) {
            copyCount = entryFunction->prototypeCount;
        }
        ZrCore_Memory_RawCopy(
                newStorage, entryFunction->prototypeInstances, copyCount * sizeof(struct SZrObjectPrototype *));
        ZrCore_Memory_RawFree(state->global,
                              entryFunction->prototypeInstances,
                              entryFunction->prototypeInstancesLength * sizeof(struct SZrObjectPrototype *));
    }

    entryFunction->prototypeInstances = newStorage;
    entryFunction->prototypeInstancesLength = entryFunction->prototypeCount;
    return ZR_TRUE;
}

static SZrFunction *get_function_from_constant(SZrState *state, const SZrTypeValue *constant) {
    if (state == ZR_NULL || constant == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Closure_GetMetadataFunctionFromValue(state, constant);
}

static SZrString *module_prototype_get_string_constant(SZrState *state,
                                                       SZrFunction *entryFunction,
                                                       TZrUInt32 constantIndex) {
    const SZrTypeValue *constantValue;

    if (state == ZR_NULL || entryFunction == ZR_NULL || constantIndex >= entryFunction->constantValueLength) {
        return ZR_NULL;
    }

    constantValue = &entryFunction->constantValueList[constantIndex];
    if (constantValue->type != ZR_VALUE_TYPE_STRING || constantValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_STRING(state, constantValue->value.object);
}

static void module_prototype_apply_protocol_mask(SZrObjectPrototype *prototype, TZrUInt64 protocolMask) {
    if (prototype == ZR_NULL || protocolMask == 0) {
        return;
    }

    for (EZrProtocolId protocolId = (EZrProtocolId)(ZR_PROTOCOL_ID_NONE + 1);
         protocolId <= ZR_PROTOCOL_ID_ARRAY_LIKE;
         protocolId = (EZrProtocolId)(protocolId + 1)) {
        if ((protocolMask & ZR_PROTOCOL_BIT(protocolId)) != 0) {
            ZrCore_ObjectPrototype_AddProtocol(prototype, protocolId);
        }
    }
}

static void module_prototype_add_runtime_descriptor(SZrState *state,
                                                    SZrObjectPrototype *prototype,
                                                    SZrFunction *entryFunction,
                                                    SZrString *memberName,
                                                    const SZrCompiledMemberInfo *member,
                                                    SZrFunction *function) {
    SZrMemberDescriptor descriptor;

    if (state == ZR_NULL || prototype == ZR_NULL || memberName == ZR_NULL || member == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(&descriptor, 0, sizeof(descriptor));
    descriptor.name = memberName;
    descriptor.isStatic = member->isStatic ? ZR_TRUE : ZR_FALSE;
    descriptor.isWritable = member->isConst ? ZR_FALSE : ZR_TRUE;
    descriptor.contractRole = member->contractRole;
    descriptor.modifierFlags = member->modifierFlags;
    descriptor.ownerTypeName =
            module_prototype_get_string_constant(state, entryFunction, member->ownerTypeNameStringIndex);
    descriptor.baseDefinitionOwnerTypeName =
            module_prototype_get_string_constant(state,
                                                 entryFunction,
                                                 member->baseDefinitionOwnerTypeNameStringIndex);
    descriptor.baseDefinitionName =
            module_prototype_get_string_constant(state, entryFunction, member->baseDefinitionNameStringIndex);
    descriptor.virtualSlotIndex = member->virtualSlotIndex;
    descriptor.interfaceContractSlot = member->interfaceContractSlot;
    descriptor.propertyIdentity = member->propertyIdentity;
    descriptor.accessorRole = member->accessorRole;

    switch (member->memberType) {
        case ZR_AST_CONSTANT_STRUCT_FIELD:
        case ZR_AST_CONSTANT_CLASS_FIELD:
            descriptor.kind = descriptor.isStatic ? ZR_MEMBER_DESCRIPTOR_KIND_STATIC_MEMBER
                                                  : ZR_MEMBER_DESCRIPTOR_KIND_FIELD;
            break;
        case ZR_AST_CONSTANT_STRUCT_METHOD:
        case ZR_AST_CONSTANT_CLASS_METHOD:
            descriptor.kind = descriptor.isStatic ? ZR_MEMBER_DESCRIPTOR_KIND_STATIC_MEMBER
                                                  : ZR_MEMBER_DESCRIPTOR_KIND_METHOD;
            break;
        default:
            return;
    }

    ZrCore_ObjectPrototype_AddMemberDescriptor(state, prototype, &descriptor);

    if (function != ZR_NULL && !descriptor.isStatic) {
        switch ((EZrMemberContractRole)member->contractRole) {
            case ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT:
                prototype->iterableContract.iterInitFunction = function;
                break;
            case ZR_MEMBER_CONTRACT_ROLE_ITERATOR_MOVE_NEXT:
                prototype->iteratorContract.moveNextFunction = function;
                break;
            case ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_METHOD:
                prototype->iteratorContract.currentFunction = function;
                break;
            default:
                break;
        }
    } else if (!descriptor.isStatic &&
               member->contractRole == ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_FIELD) {
        prototype->iteratorContract.currentMemberName = memberName;
    }
}

static SZrObjectPrototype *find_prototype_by_name(SZrState *state,
                                                  struct SZrObjectModule *module,
                                                  SZrString *typeName) {
    const TZrChar *typeNameText;

    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    typeNameText = ZrCore_String_GetNativeString(typeName);
    if (typeNameText != ZR_NULL && strchr(typeNameText, '.') != ZR_NULL) {
        SZrObjectPrototype *qualifiedPrototype = find_prototype_in_qualified_module(state, typeNameText);
        if (qualifiedPrototype != ZR_NULL) {
            return qualifiedPrototype;
        }
    }

    if (module != ZR_NULL) {
        const SZrTypeValue *exported = ZrCore_Module_GetProExport(state, module, typeName);
        if (exported != ZR_NULL && exported->type == ZR_VALUE_TYPE_OBJECT) {
            SZrObjectPrototype *proto = (SZrObjectPrototype *)ZR_CAST_OBJECT(state, exported->value.object);
            if (proto != ZR_NULL && proto->type != ZR_OBJECT_PROTOTYPE_TYPE_INVALID) {
                return proto;
            }
        }
    }

    {
        SZrObjectPrototype *globalPrototype = find_prototype_in_global_scope(state, typeName);
        if (globalPrototype != ZR_NULL) {
            return globalPrototype;
        }
    }

    {
        SZrObject *registry = zr_module_get_loaded_modules_registry(state);
        if (registry != ZR_NULL && registry->nodeMap.isValid && registry->nodeMap.buckets != ZR_NULL) {
            for (TZrSize i = 0; i < registry->nodeMap.capacity; i++) {
                SZrHashKeyValuePair *pair = registry->nodeMap.buckets[i];
                while (pair != ZR_NULL) {
                    if (pair->value.type == ZR_VALUE_TYPE_OBJECT) {
                        SZrObject *cachedObject = ZR_CAST_OBJECT(state, pair->value.value.object);
                        if (cachedObject != ZR_NULL &&
                            cachedObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                            struct SZrObjectModule *importedModule = (struct SZrObjectModule *)cachedObject;

                            if (importedModule == module) {
                                pair = pair->next;
                                continue;
                            }

                            {
                                const SZrTypeValue *importedExported =
                                        ZrCore_Module_GetPubExport(state, importedModule, typeName);
                                if (importedExported != ZR_NULL && importedExported->type == ZR_VALUE_TYPE_OBJECT) {
                                    SZrObjectPrototype *proto =
                                            (SZrObjectPrototype *)ZR_CAST_OBJECT(state, importedExported->value.object);
                                    if (proto != ZR_NULL && proto->type != ZR_OBJECT_PROTOTYPE_TYPE_INVALID) {
                                        return proto;
                                    }
                                }
                            }
                        }
                    }
                    pair = pair->next;
                }
            }
        }
    }

    return ZR_NULL;
}

static SZrString *module_prototype_default_builtin_super_name(SZrState *state, EZrObjectPrototypeType type) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    if (type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
        return ZrCore_String_Create(state, "zr.builtin.Module", strlen("zr.builtin.Module"));
    }
    if (type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        return ZrCore_String_Create(state, "zr.builtin.Object", strlen("zr.builtin.Object"));
    }

    return ZR_NULL;
}

static SZrObjectPrototype *find_local_created_prototype_by_name(SZrArray *prototypeInfos, SZrString *typeName) {
    if (prototypeInfos == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < prototypeInfos->length; i++) {
        SZrPrototypeCreationInfo *protoInfo =
                (SZrPrototypeCreationInfo *)ZrCore_Array_Get(prototypeInfos, i);
        if (protoInfo == ZR_NULL || protoInfo->prototype == ZR_NULL || protoInfo->typeName == ZR_NULL) {
            continue;
        }

        if (ZrCore_String_Equal(protoInfo->typeName, typeName)) {
            return protoInfo->prototype;
        }
    }

    return ZR_NULL;
}

static SZrString *module_prototype_extract_open_generic_base_name(SZrState *state, SZrString *typeName) {
    TZrNativeString typeNameText;
    TZrSize typeNameLength;
    const TZrChar *genericStart;

    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        typeNameText = (TZrNativeString)ZrCore_String_GetNativeStringShort(typeName);
        typeNameLength = typeName->shortStringLength;
    } else {
        typeNameText = (TZrNativeString)ZrCore_String_GetNativeString(typeName);
        typeNameLength = typeName->longStringLength;
    }

    if (typeNameText == ZR_NULL || typeNameLength == 0) {
        return ZR_NULL;
    }

    genericStart = (const TZrChar *)memchr(typeNameText, '<', typeNameLength);
    if (genericStart == ZR_NULL || genericStart == typeNameText) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, typeNameText, (TZrSize)(genericStart - typeNameText));
}

static void module_prototype_attach_open_generic_super(SZrState *state,
                                                       struct SZrObjectModule *module,
                                                       SZrPrototypeCreationInfo *protoInfo,
                                                       SZrArray *prototypeInfos) {
    SZrString *openGenericBaseName;
    SZrObjectPrototype *superPrototype;

    if (state == ZR_NULL || protoInfo == ZR_NULL || protoInfo->prototype == ZR_NULL || protoInfo->typeName == ZR_NULL ||
        protoInfo->inheritTypeNames.length > 0 || protoInfo->prototype->superPrototype != ZR_NULL) {
        return;
    }

    openGenericBaseName = module_prototype_extract_open_generic_base_name(state, protoInfo->typeName);
    if (openGenericBaseName == ZR_NULL || ZrCore_String_Equal(openGenericBaseName, protoInfo->typeName)) {
        return;
    }

    superPrototype = find_local_created_prototype_by_name(prototypeInfos, openGenericBaseName);
    if (superPrototype == ZR_NULL) {
        superPrototype = find_prototype_by_name(state, module, openGenericBaseName);
    }

    if (superPrototype == ZR_NULL || superPrototype == protoInfo->prototype ||
        superPrototype->type != protoInfo->prototypeType) {
        return;
    }

    ZrCore_ObjectPrototype_SetSuper(state, protoInfo->prototype, superPrototype);
}

static TZrBool module_prototype_read_metadata_string_field(SZrState *state,
                                                           const SZrTypeValue *metadataValue,
                                                           const TZrChar *fieldName,
                                                           const TZrChar **outText) {
    SZrObject *metadataObject;
    SZrString *fieldNameString;
    SZrTypeValue key;
    const SZrTypeValue *fieldValue;

    if (outText != ZR_NULL) {
        *outText = ZR_NULL;
    }

    if (state == ZR_NULL || metadataValue == ZR_NULL || fieldName == ZR_NULL || outText == ZR_NULL ||
        metadataValue->type != ZR_VALUE_TYPE_OBJECT || metadataValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    metadataObject = ZR_CAST_OBJECT(state, metadataValue->value.object);
    if (metadataObject == ZR_NULL) {
        return ZR_FALSE;
    }

    fieldNameString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldNameString == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_module_init_string_key(state, &key, fieldNameString);
    fieldValue = ZrCore_Object_GetValue(state, metadataObject, &key);
    if (fieldValue == ZR_NULL || fieldValue->type != ZR_VALUE_TYPE_STRING || fieldValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    *outText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, fieldValue->value.object));
    return *outText != ZR_NULL;
}

static void module_prototype_set_hidden_string_metadata(SZrState *state,
                                                        SZrObjectPrototype *prototype,
                                                        const TZrChar *fieldName,
                                                        const TZrChar *value) {
    SZrString *fieldNameString;
    SZrString *valueString;
    SZrTypeValue key;
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || prototype == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    fieldNameString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    valueString = ZrCore_String_CreateFromNative(state, (TZrNativeString)value);
    if (fieldNameString == ZR_NULL || valueString == ZR_NULL) {
        return;
    }

    zr_module_init_string_key(state, &key, fieldNameString);
    ZrCore_Value_InitAsRawObject(state, &fieldValue, ZR_CAST_RAW_OBJECT_AS_SUPER(valueString));
    fieldValue.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, &prototype->super, &key, &fieldValue);
}

static const SZrTypeValue *module_prototype_get_object_field_value(SZrState *state,
                                                                   SZrObject *object,
                                                                   SZrString *fieldName) {
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    zr_module_init_string_key(state, &key, fieldName);
    return ZrCore_Object_GetValue(state, object, &key);
}

static const SZrTypeValue *module_prototype_get_metadata_field_value(SZrState *state,
                                                                     const SZrTypeValue *metadataValue,
                                                                     const TZrChar *fieldName) {
    SZrObject *metadataObject;
    SZrString *fieldNameString;

    if (state == ZR_NULL || metadataValue == ZR_NULL || fieldName == ZR_NULL ||
        metadataValue->type != ZR_VALUE_TYPE_OBJECT || metadataValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    metadataObject = ZR_CAST_OBJECT(state, metadataValue->value.object);
    fieldNameString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (metadataObject == ZR_NULL || fieldNameString == ZR_NULL) {
        return ZR_NULL;
    }

    return module_prototype_get_object_field_value(state, metadataObject, fieldNameString);
}

static void module_prototype_attach_enum_hidden_metadata(SZrState *state,
                                                         SZrObjectPrototype *prototype,
                                                         const SZrPrototypeCreationInfo *protoInfo) {
    const TZrChar *enumValueTypeName = ZR_NULL;

    if (state == ZR_NULL || prototype == ZR_NULL || protoInfo == ZR_NULL ||
        protoInfo->prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_ENUM || !protoInfo->hasDecoratorMetadata) {
        return;
    }

    if (module_prototype_read_metadata_string_field(state,
                                                    &protoInfo->decoratorMetadataValue,
                                                    kModulePrototypeEnumValueTypeFieldName,
                                                    &enumValueTypeName) &&
        enumValueTypeName != ZR_NULL) {
        module_prototype_set_hidden_string_metadata(state,
                                                    prototype,
                                                    kModulePrototypeEnumValueTypeFieldName,
                                                    enumValueTypeName);
    }
}

static const SZrTypeValue *module_prototype_get_enum_member_scalar_value(SZrState *state,
                                                                         const SZrPrototypeCreationInfo *protoInfo,
                                                                         SZrString *memberName) {
    const SZrTypeValue *membersValue;
    SZrObject *membersObject;

    if (state == ZR_NULL || protoInfo == ZR_NULL || memberName == ZR_NULL || !protoInfo->hasDecoratorMetadata) {
        return ZR_NULL;
    }

    membersValue = module_prototype_get_metadata_field_value(state,
                                                             &protoInfo->decoratorMetadataValue,
                                                             kModulePrototypeEnumMembersFieldName);
    if (membersValue == ZR_NULL || membersValue->type != ZR_VALUE_TYPE_OBJECT || membersValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    membersObject = ZR_CAST_OBJECT(state, membersValue->value.object);
    if (membersObject == ZR_NULL) {
        return ZR_NULL;
    }

    return module_prototype_get_object_field_value(state, membersObject, memberName);
}

static TZrBool module_prototype_set_object_field_string(SZrState *state,
                                                        SZrObject *object,
                                                        SZrString *fieldName,
                                                        const SZrTypeValue *fieldValue) {
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || fieldValue == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_module_init_string_key(state, &key, fieldName);
    ZrCore_Object_SetValue(state, object, &key, fieldValue);
    return ZR_TRUE;
}

static TZrBool module_prototype_add_enum_member_instance(SZrState *state,
                                                         SZrObjectPrototype *prototype,
                                                         SZrString *memberName,
                                                         const SZrTypeValue *underlyingValue) {
    SZrObject *enumObject;
    SZrString *valueFieldName;
    SZrString *nameFieldName;
    SZrTypeValue enumObjectValue;
    SZrTypeValue nameValue;

    if (state == ZR_NULL || prototype == ZR_NULL || memberName == ZR_NULL || underlyingValue == ZR_NULL) {
        return ZR_FALSE;
    }

    enumObject = ZrCore_Object_New(state, prototype);
    if (enumObject == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(state, enumObject);

    valueFieldName = ZrCore_String_CreateFromNative(state, (TZrNativeString)kModulePrototypeEnumValueFieldName);
    nameFieldName = ZrCore_String_CreateFromNative(state, (TZrNativeString)kModulePrototypeEnumNameFieldName);
    if (valueFieldName == ZR_NULL || nameFieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!module_prototype_set_object_field_string(state, enumObject, valueFieldName, underlyingValue)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &nameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
    nameValue.type = ZR_VALUE_TYPE_STRING;
    if (!module_prototype_set_object_field_string(state, enumObject, nameFieldName, &nameValue)) {
        return ZR_FALSE;
    }

    zr_module_init_object_value(state, &enumObjectValue, ZR_CAST_RAW_OBJECT_AS_SUPER(enumObject));
    return module_prototype_set_object_field_string(state, &prototype->super, memberName, &enumObjectValue);
}

static void module_prototype_attach_ffi_wrapper_hidden_metadata(SZrState *state,
                                                                SZrObjectPrototype *prototype,
                                                                const SZrPrototypeCreationInfo *protoInfo) {
    if (state == ZR_NULL || prototype == ZR_NULL || protoInfo == ZR_NULL || !protoInfo->hasDecoratorMetadata) {
        return;
    }

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(kModulePrototypeFfiMetadataFieldMappings); index++) {
        const SZrPrototypeMetadataFieldMapping *mapping = &kModulePrototypeFfiMetadataFieldMappings[index];
        const TZrChar *fieldValue = ZR_NULL;

        if (mapping->metadataFieldName == ZR_NULL || mapping->hiddenFieldName == ZR_NULL) {
            continue;
        }

        if (module_prototype_read_metadata_string_field(state,
                                                        &protoInfo->decoratorMetadataValue,
                                                        mapping->metadataFieldName,
                                                        &fieldValue) &&
            fieldValue != ZR_NULL) {
            module_prototype_set_hidden_string_metadata(state, prototype, mapping->hiddenFieldName, fieldValue);
        }
    }
}

TZrInt64 ZrCore_PrototypeNativeFunction_Create(SZrState *state) {
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argBase;
    SZrTypeValue *moduleValue;
    SZrObject *moduleObject;
    struct SZrObjectModule *module;
    SZrTypeValue *typeNameValue;
    SZrString *typeName;
    SZrTypeValue *typeValue;
    EZrObjectPrototypeType prototypeType;
    SZrTypeValue *accessModifierValue;
    EZrAccessModifier accessModifier;
    SZrObjectPrototype *prototype;
    SZrTypeValue prototypeValue;
    SZrTypeValue *result;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    functionBase = state->callInfoList->functionBase.valuePointer;
    argBase = functionBase + 1;
#define ZR_RETURN_CREATE_PROTOTYPE_RESULT() \
    do {                                    \
        state->stackTop.valuePointer = functionBase + 1; \
        return 1;                           \
    } while (0)

    if (state->stackTop.valuePointer <= argBase + 3) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }

    moduleValue = ZrCore_Stack_GetValue(argBase);
    if (moduleValue->type != ZR_VALUE_TYPE_OBJECT) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }

    moduleObject = ZR_CAST_OBJECT(state, moduleValue->value.object);
    if (moduleObject == ZR_NULL || moduleObject->internalType != ZR_OBJECT_INTERNAL_TYPE_MODULE) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    module = (struct SZrObjectModule *)moduleObject;

    typeNameValue = ZrCore_Stack_GetValue(argBase + 1);
    if (typeNameValue->type != ZR_VALUE_TYPE_STRING) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }

    typeName = ZR_CAST_STRING(state, typeNameValue->value.object);
    if (typeName == ZR_NULL) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }

    typeValue = ZrCore_Stack_GetValue(argBase + 2);
    if (!ZR_VALUE_IS_TYPE_INT(typeValue->type)) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }

    prototypeType = (EZrObjectPrototypeType)typeValue->value.nativeObject.nativeUInt64;
    if (prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT && prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }

    accessModifierValue = ZrCore_Stack_GetValue(argBase + 3);
    if (!ZR_VALUE_IS_TYPE_INT(accessModifierValue->type)) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }
    accessModifier = (EZrAccessModifier)accessModifierValue->value.nativeObject.nativeUInt64;

    prototype = ZR_NULL;
    if (prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        prototype = (SZrObjectPrototype *)ZrCore_StructPrototype_New(state, typeName);
    } else {
        prototype = ZrCore_ObjectPrototype_New(state, typeName, prototypeType);
    }

    if (prototype == ZR_NULL) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(functionBase));
        ZR_RETURN_CREATE_PROTOTYPE_RESULT();
    }

    zr_module_init_object_value(state, &prototypeValue, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    if (accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
        ZrCore_Module_AddPubExport(state, module, typeName, &prototypeValue);
    } else if (accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
        ZrCore_Module_AddProExport(state, module, typeName, &prototypeValue);
    }

    result = ZrCore_Stack_GetValue(functionBase);
    ZrCore_Value_Copy(state, result, &prototypeValue);

    ZR_RETURN_CREATE_PROTOTYPE_RESULT();
#undef ZR_RETURN_CREATE_PROTOTYPE_RESULT
}

static TZrBool parse_compiled_prototype_info(SZrState *state,
                                             SZrFunction *entryFunction,
                                             const TZrByte *serializedData,
                                             TZrSize dataSize,
                                             SZrPrototypeCreationInfo *protoInfo) {
    const SZrCompiledPrototypeInfo *protoInfoHeader;
    TZrUInt32 nameStringIndex;
    TZrUInt32 type;
    TZrUInt32 accessModifier;
    TZrUInt32 inheritsCount;
    TZrUInt32 membersCount;
    TZrUInt32 decoratorsCount;
    TZrSize expectedSize;
    const SZrTypeValue *nameConstant;

    if (state == ZR_NULL || entryFunction == ZR_NULL || serializedData == ZR_NULL ||
        dataSize < sizeof(SZrCompiledPrototypeInfo) || protoInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    protoInfoHeader = (const SZrCompiledPrototypeInfo *)serializedData;
    nameStringIndex = protoInfoHeader->nameStringIndex;
    type = protoInfoHeader->type;
    accessModifier = protoInfoHeader->accessModifier;
    inheritsCount = protoInfoHeader->inheritsCount;
    membersCount = protoInfoHeader->membersCount;
    decoratorsCount = protoInfoHeader->decoratorsCount;

    expectedSize =
            sizeof(SZrCompiledPrototypeInfo) + inheritsCount * sizeof(TZrUInt32) +
            decoratorsCount * sizeof(TZrUInt32) +
            membersCount * sizeof(SZrCompiledMemberInfo);
    if (dataSize < expectedSize) {
        return ZR_FALSE;
    }

    if (nameStringIndex >= entryFunction->constantValueLength) {
        return ZR_FALSE;
    }
    nameConstant = &entryFunction->constantValueList[nameStringIndex];
    if (nameConstant->type != ZR_VALUE_TYPE_STRING) {
        return ZR_FALSE;
    }

    protoInfo->typeName = ZR_CAST_STRING(state, nameConstant->value.object);
    if (protoInfo->typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    protoInfo->prototypeType = (EZrObjectPrototypeType)type;
    protoInfo->accessModifier = (EZrAccessModifier)accessModifier;
    protoInfo->protocolMask = protoInfoHeader->protocolMask;
    protoInfo->modifierFlags = protoInfoHeader->modifierFlags;
    protoInfo->nextVirtualSlotIndex = protoInfoHeader->nextVirtualSlotIndex;
    protoInfo->nextPropertyIdentity = protoInfoHeader->nextPropertyIdentity;
    protoInfo->hasDecoratorMetadata = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&protoInfo->decoratorMetadataValue);
    protoInfo->prototype = ZR_NULL;
    protoInfo->membersCount = membersCount;
    protoInfo->needsPostCreateSetup = ZR_FALSE;

    if (protoInfoHeader->hasDecoratorMetadata &&
        protoInfoHeader->decoratorMetadataConstantIndex < entryFunction->constantValueLength) {
        protoInfo->decoratorMetadataValue =
                entryFunction->constantValueList[protoInfoHeader->decoratorMetadataConstantIndex];
        protoInfo->hasDecoratorMetadata = ZR_TRUE;
    }

    ZrCore_Array_Init(state, &protoInfo->inheritTypeNames, sizeof(SZrString *), inheritsCount);

    {
        const TZrUInt32 *inheritIndices =
                (const TZrUInt32 *)(serializedData + sizeof(SZrCompiledPrototypeInfo));
        for (TZrUInt32 i = 0; i < inheritsCount; i++) {
            TZrUInt32 inheritStringIndex = inheritIndices[i];
            if (inheritStringIndex > 0 && inheritStringIndex < entryFunction->constantValueLength) {
                const SZrTypeValue *inheritConstant = &entryFunction->constantValueList[inheritStringIndex];
                if (inheritConstant->type == ZR_VALUE_TYPE_STRING) {
                    SZrString *inheritTypeName = ZR_CAST_STRING(state, inheritConstant->value.object);
                    if (inheritTypeName != ZR_NULL) {
                        ZrCore_Array_Push(state, &protoInfo->inheritTypeNames, &inheritTypeName);
                    }
                }
            }
        }
    }

    protoInfo->members =
            (const SZrCompiledMemberInfo *)(serializedData + sizeof(SZrCompiledPrototypeInfo) +
                                            inheritsCount * sizeof(TZrUInt32) +
                                            decoratorsCount * sizeof(TZrUInt32));

    return ZR_TRUE;
}

TZrSize ZrCore_Module_CreatePrototypesFromData(SZrState *state,
                                               struct SZrObjectModule *module,
                                               SZrFunction *entryFunction) {
    TZrUInt32 prototypeCount;
    SZrArray prototypeInfos;
    TZrSize createdCount;
    const TZrByte *prototypeData;
    TZrSize remainingDataSize;
    const TZrByte *currentPos;

    if (state == ZR_NULL || entryFunction == ZR_NULL) {
        return 0;
    }

    if (entryFunction->prototypeData == ZR_NULL || entryFunction->prototypeDataLength == 0 ||
        entryFunction->prototypeCount == 0) {
        return 0;
    }

    if (entryFunction->constantValueList == ZR_NULL || entryFunction->constantValueLength == 0) {
        return 0;
    }

    prototypeCount = entryFunction->prototypeCount;
    if (!ensure_prototype_instance_storage(state, entryFunction)) {
        return 0;
    }

    ZrCore_Array_Init(state, &prototypeInfos, sizeof(SZrPrototypeCreationInfo), prototypeCount);
    createdCount = 0;

    prototypeData = entryFunction->prototypeData + sizeof(TZrUInt32);
    remainingDataSize = entryFunction->prototypeDataLength - sizeof(TZrUInt32);
    currentPos = prototypeData;

    for (TZrUInt32 i = 0; i < prototypeCount; i++) {
        if (remainingDataSize < sizeof(SZrCompiledPrototypeInfo)) {
            break;
        }

        {
            const SZrCompiledPrototypeInfo *protoInfo = (const SZrCompiledPrototypeInfo *)currentPos;
            TZrUInt32 inheritsCount = protoInfo->inheritsCount;
            TZrUInt32 membersCount = protoInfo->membersCount;
            TZrSize inheritArraySize = inheritsCount * sizeof(TZrUInt32);
            TZrSize decoratorArraySize = protoInfo->decoratorsCount * sizeof(TZrUInt32);
            TZrSize membersArraySize = membersCount * sizeof(SZrCompiledMemberInfo);
            TZrSize currentPrototypeSize =
                    sizeof(SZrCompiledPrototypeInfo) + inheritArraySize + decoratorArraySize + membersArraySize;

            if (remainingDataSize < currentPrototypeSize) {
                break;
            }

            {
                SZrPrototypeCreationInfo protoInfoData;
                protoInfoData.prototype = ZR_NULL;
                protoInfoData.typeName = ZR_NULL;
                protoInfoData.prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
                protoInfoData.accessModifier = ZR_ACCESS_CONSTANT_PRIVATE;
                protoInfoData.protocolMask = 0;
                protoInfoData.modifierFlags = 0;
                protoInfoData.nextVirtualSlotIndex = 0;
                protoInfoData.nextPropertyIdentity = 0;
                protoInfoData.hasDecoratorMetadata = ZR_FALSE;
                ZrCore_Value_ResetAsNull(&protoInfoData.decoratorMetadataValue);
                ZrCore_Array_Init(state,
                                  &protoInfoData.inheritTypeNames,
                                  sizeof(SZrString *),
                                  ZR_RUNTIME_PROTOTYPE_INHERIT_INITIAL_CAPACITY);
                protoInfoData.members = ZR_NULL;
                protoInfoData.membersCount = 0;

                if (parse_compiled_prototype_info(
                            state, entryFunction, currentPos, currentPrototypeSize, &protoInfoData)) {
                    if (protoInfoData.typeName != ZR_NULL &&
                        protoInfoData.prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INVALID) {
                        TZrBool prototypeWasCreated = ZR_FALSE;
                        SZrObjectPrototype *prototype = entryFunction->prototypeInstances[i];
                        if (prototype == ZR_NULL) {
                            if (protoInfoData.prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
                                prototype =
                                        (SZrObjectPrototype *)ZrCore_StructPrototype_New(state, protoInfoData.typeName);
                            } else if (protoInfoData.prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS ||
                                       protoInfoData.prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
                                prototype = ZrCore_ObjectPrototype_New(state,
                                                                       protoInfoData.typeName,
                                                                       protoInfoData.prototypeType);
                                if (protoInfoData.prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
                                    ZrCore_ObjectPrototype_InitMetaTable(state, prototype);
                                }
                            }
                            if (prototype != ZR_NULL) {
                                entryFunction->prototypeInstances[i] = prototype;
                                prototypeWasCreated = ZR_TRUE;
                            }
                        }

                        if (prototype != ZR_NULL) {
                            SZrTypeValue prototypeValue;
                            protoInfoData.prototype = prototype;
                            protoInfoData.needsPostCreateSetup = prototypeWasCreated;
                            prototype->modifierFlags = protoInfoData.modifierFlags;
                            prototype->nextVirtualSlotIndex = protoInfoData.nextVirtualSlotIndex;
                            prototype->nextPropertyIdentity = protoInfoData.nextPropertyIdentity;
                            ZrCore_Reflection_AttachPrototypeRuntimeMetadata(state, prototype, module, entryFunction);
                            module_prototype_attach_ffi_wrapper_hidden_metadata(state, prototype, &protoInfoData);
                            module_prototype_attach_enum_hidden_metadata(state, prototype, &protoInfoData);

                            zr_module_init_object_value(
                                    state, &prototypeValue, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
                            if (module != ZR_NULL) {
                                if (protoInfoData.accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
                                    ZrCore_Module_AddPubExport(state, module, protoInfoData.typeName, &prototypeValue);
                                } else if (protoInfoData.accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
                                    ZrCore_Module_AddProExport(state, module, protoInfoData.typeName, &prototypeValue);
                                }
                            }
                            register_prototype_in_global_scope(state, protoInfoData.typeName, &prototypeValue);
                            ZrCore_Array_Push(state, &prototypeInfos, &protoInfoData);

                            if (prototypeWasCreated) {
                                createdCount++;
                            }
                        } else {
                            ZrCore_Array_Free(state, &protoInfoData.inheritTypeNames);
                        }
                    } else {
                        ZrCore_Array_Free(state, &protoInfoData.inheritTypeNames);
                    }
                }
            }

            currentPos += currentPrototypeSize;
            remainingDataSize -= currentPrototypeSize;
        }
    }

    for (TZrSize i = 0; i < prototypeInfos.length; i++) {
        SZrPrototypeCreationInfo *protoInfo = (SZrPrototypeCreationInfo *)ZrCore_Array_Get(&prototypeInfos, i);
        if (protoInfo == ZR_NULL || protoInfo->prototype == ZR_NULL) {
            continue;
        }

        module_prototype_attach_open_generic_super(state, module, protoInfo, &prototypeInfos);

        if (!protoInfo->needsPostCreateSetup) {
            ZrCore_Array_Free(state, &protoInfo->inheritTypeNames);
            continue;
        }

        if (protoInfo->inheritTypeNames.length > 0) {
            SZrString **inheritTypeNamePtr = (SZrString **)ZrCore_Array_Get(&protoInfo->inheritTypeNames, 0);
            if (inheritTypeNamePtr != ZR_NULL && *inheritTypeNamePtr != ZR_NULL) {
                SZrObjectPrototype *superPrototype =
                        find_local_created_prototype_by_name(&prototypeInfos, *inheritTypeNamePtr);
                if (superPrototype == ZR_NULL) {
                    superPrototype = find_prototype_by_name(state, module, *inheritTypeNamePtr);
                }
                if (superPrototype != ZR_NULL) {
                    ZrCore_ObjectPrototype_SetSuper(state, protoInfo->prototype, superPrototype);
                }
            }
        }
        if (protoInfo->prototype->superPrototype == ZR_NULL) {
            SZrString *defaultSuperName =
                    module_prototype_default_builtin_super_name(state, protoInfo->prototypeType);
            if (defaultSuperName != ZR_NULL) {
                SZrObjectPrototype *defaultSuperPrototype = find_prototype_by_name(state, module, defaultSuperName);
                if (defaultSuperPrototype != ZR_NULL && defaultSuperPrototype != protoInfo->prototype) {
                    ZrCore_ObjectPrototype_SetSuper(state, protoInfo->prototype, defaultSuperPrototype);
                }
            }
        }

        module_prototype_apply_protocol_mask(protoInfo->prototype, protoInfo->protocolMask);

        if (protoInfo->members != ZR_NULL && entryFunction->constantValueList != ZR_NULL) {
            for (TZrUInt32 memberIndex = 0; memberIndex < protoInfo->membersCount; memberIndex++) {
                const SZrCompiledMemberInfo *member = &protoInfo->members[memberIndex];
                if (member == ZR_NULL || member->nameStringIndex >= entryFunction->constantValueLength) {
                    continue;
                }

                {
                    const SZrTypeValue *nameConstant = &entryFunction->constantValueList[member->nameStringIndex];
                    if (nameConstant->type != ZR_VALUE_TYPE_STRING) {
                        continue;
                    }

                    {
                        SZrString *memberName = ZR_CAST_STRING(state, nameConstant->value.object);
                        if (memberName == ZR_NULL) {
                            continue;
                        }

                        if (member->memberType == ZR_AST_CONSTANT_STRUCT_FIELD ||
                            member->memberType == ZR_AST_CONSTANT_CLASS_FIELD) {
                            if (protoInfo->prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_ENUM &&
                                member->isStatic) {
                                const SZrTypeValue *enumScalarValue =
                                        module_prototype_get_enum_member_scalar_value(state, protoInfo, memberName);

                                module_prototype_add_runtime_descriptor(state,
                                                                        protoInfo->prototype,
                                                                        entryFunction,
                                                                        memberName,
                                                                        member,
                                                                        ZR_NULL);
                                if (enumScalarValue != ZR_NULL) {
                                    module_prototype_add_enum_member_instance(state,
                                                                              protoInfo->prototype,
                                                                              memberName,
                                                                              enumScalarValue);
                                }
                                continue;
                            }

                            module_prototype_add_runtime_descriptor(state,
                                                                    protoInfo->prototype,
                                                                    entryFunction,
                                                                    memberName,
                                                                    member,
                                                                    ZR_NULL);
                            if (protoInfo->prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT &&
                                member->memberType == ZR_AST_CONSTANT_STRUCT_FIELD) {
                                ZrCore_StructPrototype_AddField(state,
                                                                (SZrStructPrototype *)protoInfo->prototype,
                                                                memberName,
                                                                member->fieldOffset);
                            }

                            if (member->ownershipQualifier != 0) {
                                ZrCore_ObjectPrototype_AddManagedField(state,
                                                                       protoInfo->prototype,
                                                                       memberName,
                                                                       member->fieldOffset,
                                                                       member->fieldSize,
                                                                       member->ownershipQualifier,
                                                                       member->callsClose ? ZR_TRUE : ZR_FALSE,
                                                                       member->callsDestructor ? ZR_TRUE : ZR_FALSE,
                                                                       member->declarationOrder);
                            }
                            continue;
                        }

                        if ((member->memberType == ZR_AST_CONSTANT_CLASS_METHOD ||
                             member->memberType == ZR_AST_CONSTANT_STRUCT_METHOD ||
                             member->memberType == ZR_AST_CONSTANT_CLASS_META_FUNCTION ||
                             member->memberType == ZR_AST_CONSTANT_STRUCT_META_FUNCTION) &&
                            member->functionConstantIndex < entryFunction->constantValueLength) {
                            const SZrTypeValue *functionConstant =
                                    &entryFunction->constantValueList[member->functionConstantIndex];
                            SZrFunction *function = get_function_from_constant(state, functionConstant);
                            if (function == ZR_NULL) {
                                continue;
                            }

                            if (member->isMetaMethod) {
                                ZrCore_ObjectPrototype_AddMeta(
                                        state, protoInfo->prototype, (EZrMetaType)member->metaType, function);
                                if (member->metaType == ZR_META_GET_ITEM) {
                                    protoInfo->prototype->indexContract.getByIndexFunction = function;
                                } else if (member->metaType == ZR_META_SET_ITEM) {
                                    protoInfo->prototype->indexContract.setByIndexFunction = function;
                                }
                                if (member->metaType == ZR_META_CONSTRUCTOR) {
                                    SZrString *constructorName =
                                            ZrCore_String_CreateFromNative(state, "__constructor");
                                    if (constructorName != ZR_NULL) {
                                        SZrTypeValue constructorKey;
                                        zr_module_init_string_key(state, &constructorKey, constructorName);
                                        ZrCore_Object_SetValue(state,
                                                               &protoInfo->prototype->super,
                                                               &constructorKey,
                                                               functionConstant);
                                    }
                                }
                                continue;
                            }

                            module_prototype_add_runtime_descriptor(state,
                                                                    protoInfo->prototype,
                                                                    entryFunction,
                                                                    memberName,
                                                                    member,
                                                                    function);

                            {
                                SZrTypeValue methodKey;
                                zr_module_init_string_key(state, &methodKey, memberName);
                                ZrCore_Object_SetValue(
                                        state, &protoInfo->prototype->super, &methodKey, functionConstant);
                            }
                        }
                    }
                }
            }
        }

        ZrCore_Array_Free(state, &protoInfo->inheritTypeNames);
    }

    ZrCore_Array_Free(state, &prototypeInfos);
    return createdCount;
}

TZrSize ZrCore_Module_CreatePrototypesFromConstants(SZrState *state,
                                                    struct SZrObjectModule *module,
                                                    SZrFunction *entryFunction) {
    if (entryFunction->prototypeData != ZR_NULL && entryFunction->prototypeDataLength > 0 &&
        entryFunction->prototypeCount > 0) {
        return ZrCore_Module_CreatePrototypesFromData(state, module, entryFunction);
    }

    if (entryFunction->constantValueList == ZR_NULL || entryFunction->constantValueLength == 0) {
        return 0;
    }

    return 0;
}
