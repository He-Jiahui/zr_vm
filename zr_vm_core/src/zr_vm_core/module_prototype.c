//
// Prototype materialization and registration helpers for modules.
//

#include "module_internal.h"
#include "zr_vm_core/reflection.h"
#include <string.h>

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
    SZrRawObject *rawObject;

    if (state == ZR_NULL || constant == ZR_NULL) {
        return ZR_NULL;
    }

    if (constant->type != ZR_VALUE_TYPE_FUNCTION && constant->type != ZR_VALUE_TYPE_CLOSURE) {
        return ZR_NULL;
    }

    rawObject = constant->value.object;
    if (rawObject == ZR_NULL || rawObject->type != ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_NULL;
    }

    return ZR_CAST_FUNCTION(state, rawObject);
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
    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
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

    expectedSize =
            sizeof(SZrCompiledPrototypeInfo) + inheritsCount * sizeof(TZrUInt32) +
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
    protoInfo->prototype = ZR_NULL;
    protoInfo->membersCount = membersCount;
    protoInfo->needsPostCreateSetup = ZR_FALSE;

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
                                            inheritsCount * sizeof(TZrUInt32));

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
            TZrSize membersArraySize = membersCount * sizeof(SZrCompiledMemberInfo);
            TZrSize currentPrototypeSize =
                    sizeof(SZrCompiledPrototypeInfo) + inheritArraySize + membersArraySize;

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
                            } else if (protoInfoData.prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
                                prototype = ZrCore_ObjectPrototype_New(state,
                                                                       protoInfoData.typeName,
                                                                       protoInfoData.prototypeType);
                                ZrCore_ObjectPrototype_InitMetaTable(state, prototype);
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
                            ZrCore_Reflection_AttachPrototypeRuntimeMetadata(state, prototype, module, entryFunction);

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
                            module_prototype_add_runtime_descriptor(state,
                                                                    protoInfo->prototype,
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

                            if (member->isUsingManaged) {
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
                                        SZrTypeValue constructorValue;
                                        zr_module_init_string_key(state, &constructorKey, constructorName);
                                        ZrCore_Value_InitAsRawObject(
                                                state, &constructorValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
                                        constructorValue.type = functionConstant->type;
                                        ZrCore_Object_SetValue(state,
                                                               &protoInfo->prototype->super,
                                                               &constructorKey,
                                                               &constructorValue);
                                    }
                                }
                                continue;
                            }

                            module_prototype_add_runtime_descriptor(state,
                                                                    protoInfo->prototype,
                                                                    memberName,
                                                                    member,
                                                                    function);

                            {
                                SZrTypeValue methodValue;
                                SZrTypeValue methodKey;
                                ZrCore_Value_InitAsRawObject(
                                        state, &methodValue, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
                                methodValue.type = functionConstant->type;
                                zr_module_init_string_key(state, &methodKey, memberName);
                                ZrCore_Object_SetValue(
                                        state, &protoInfo->prototype->super, &methodKey, &methodValue);
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
