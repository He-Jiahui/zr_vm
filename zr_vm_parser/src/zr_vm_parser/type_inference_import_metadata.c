//
// Compile-time import metadata loading for native/source/binary modules.
//

#include "compiler_internal.h"
#include "type_inference_internal.h"
#include "zr_vm_core/io.h"

#include <stdio.h>

static TZrBool refill_import_io(SZrIo *io) {
    SZrState *state;
    TZrSize readSize = 0;
    TZrBytePtr buffer;

    if (io == ZR_NULL || io->state == ZR_NULL || io->read == ZR_NULL) {
        return ZR_FALSE;
    }

    state = io->state;
    ZR_THREAD_UNLOCK(state);
    buffer = io->read(state, io->customData, &readSize);
    ZR_THREAD_LOCK(state);
    if (buffer == ZR_NULL || readSize == 0) {
        return ZR_FALSE;
    }

    io->pointer = buffer;
    io->remained = readSize;
    return ZR_TRUE;
}

static TZrBytePtr read_all_import_bytes(SZrState *state, SZrIo *io, TZrSize *outSize) {
    SZrGlobalState *global;
    TZrSize capacity;
    TZrSize totalSize = 0;
    TZrBytePtr buffer;

    if (state == ZR_NULL || io == ZR_NULL || outSize == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    global = state->global;
    capacity = io->remained > 0 ? io->remained : ZR_VM_READ_ALL_IO_FALLBACK_CAPACITY;
    buffer = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    while (io->remained > 0 || refill_import_io(io)) {
        TZrSize chunkSize = io->remained;
        if (totalSize + chunkSize + 1 > capacity) {
            TZrSize newCapacity = capacity;
            TZrBytePtr resizedBuffer;

            while (totalSize + chunkSize + 1 > newCapacity) {
                newCapacity *= 2;
            }

            resizedBuffer = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global,
                                                                        newCapacity + 1,
                                                                        ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            if (resizedBuffer == ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                return ZR_NULL;
            }

            if (totalSize > 0) {
                ZrCore_Memory_RawCopy(resizedBuffer, buffer, totalSize);
            }
            ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            buffer = resizedBuffer;
            capacity = newCapacity;
        }

        ZrCore_Memory_RawCopy(buffer + totalSize, io->pointer, chunkSize);
        totalSize += chunkSize;
        io->pointer += chunkSize;
        io->remained = 0;
    }

    buffer[totalSize] = '\0';
    *outSize = totalSize;
    return buffer;
}

static void import_type_prototype_init(SZrState *state,
                                       SZrTypePrototypeInfo *info,
                                       SZrString *name,
                                       EZrObjectPrototypeType type) {
    if (state == ZR_NULL || info == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(info, 0, sizeof(*info));
    info->name = name;
    info->type = type;
    info->accessModifier = ZR_ACCESS_PUBLIC;
    info->isImportedNative = ZR_FALSE;
    info->allowValueConstruction = type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE &&
                                   type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
    info->allowBoxedConstruction = info->allowValueConstruction;
    ZrCore_Array_Init(state, &info->inherits, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state, &info->implements, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state,
                      &info->genericParameters,
                      sizeof(SZrTypeGenericParameterInfo),
                      ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state, &info->members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);
}

static TZrBool import_prototype_has_member(SZrTypePrototypeInfo *info, SZrString *memberName) {
    if (info == ZR_NULL || memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < info->members.length; index++) {
        SZrTypeMemberInfo *existing = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, index);
        if (existing != ZR_NULL && existing->name != ZR_NULL && ZrCore_String_Equal(existing->name, memberName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void register_imported_type_name(SZrCompilerState *cs, SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return;
    }

    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, typeName);
    }
    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->compileTimeTypeEnv, typeName);
    }
}

static void function_typed_type_ref_to_inferred(SZrCompilerState *cs,
                                                const SZrFunctionTypedTypeRef *typeRef,
                                                SZrInferredType *result) {
    if (cs == ZR_NULL || result == ZR_NULL) {
        return;
    }

    if (typeRef == ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return;
    }

    if (typeRef->isArray) {
        SZrInferredType elementType;

        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_ARRAY);
        result->ownershipQualifier = typeRef->ownershipQualifier;
        result->isNullable = typeRef->isNullable;
        ZrCore_Array_Init(cs->state, &result->elementTypes, sizeof(SZrInferredType), 1);
        ZrParser_InferredType_InitFull(cs->state,
                                       &elementType,
                                       typeRef->elementBaseType,
                                       ZR_FALSE,
                                       typeRef->elementTypeName);
        ZrCore_Array_Push(cs->state, &result->elementTypes, &elementType);
        return;
    }

    if (typeRef->typeName != ZR_NULL) {
        ZrParser_InferredType_InitFull(cs->state,
                                       result,
                                       typeRef->baseType,
                                       typeRef->isNullable,
                                       typeRef->typeName);
    } else {
        ZrParser_InferredType_Init(cs->state, result, typeRef->baseType);
        result->isNullable = typeRef->isNullable;
    }
    result->ownershipQualifier = typeRef->ownershipQualifier;
}

static SZrString *primitive_type_name(SZrCompilerState *cs, EZrValueType baseType) {
    const TZrChar *name = "object";

    if (cs == ZR_NULL) {
        return ZR_NULL;
    }

    switch (baseType) {
        case ZR_VALUE_TYPE_NULL:
            name = "null";
            break;
        case ZR_VALUE_TYPE_BOOL:
            name = "bool";
            break;
        case ZR_VALUE_TYPE_INT8:
            name = "i8";
            break;
        case ZR_VALUE_TYPE_INT16:
            name = "i16";
            break;
        case ZR_VALUE_TYPE_INT32:
            name = "i32";
            break;
        case ZR_VALUE_TYPE_INT64:
            name = "int";
            break;
        case ZR_VALUE_TYPE_UINT8:
            name = "u8";
            break;
        case ZR_VALUE_TYPE_UINT16:
            name = "u16";
            break;
        case ZR_VALUE_TYPE_UINT32:
            name = "u32";
            break;
        case ZR_VALUE_TYPE_UINT64:
            name = "uint";
            break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            name = "float";
            break;
        case ZR_VALUE_TYPE_STRING:
            name = "string";
            break;
        case ZR_VALUE_TYPE_CLOSURE:
        case ZR_VALUE_TYPE_FUNCTION:
            name = "function";
            break;
        case ZR_VALUE_TYPE_ARRAY:
            name = "array";
            break;
        default:
            break;
    }

    return ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)name);
}

static SZrString *typed_type_ref_to_type_name(SZrCompilerState *cs, const SZrFunctionTypedTypeRef *typeRef) {
    TZrNativeString elementName;
    TZrSize elementLength;
    TZrSize totalLength;
    TZrChar *buffer;
    SZrString *result;
    SZrString *elementType;

    if (cs == ZR_NULL || typeRef == ZR_NULL) {
        return ZR_NULL;
    }

    if (!typeRef->isArray) {
        return typeRef->typeName != ZR_NULL ? typeRef->typeName : primitive_type_name(cs, typeRef->baseType);
    }

    if (typeRef->elementTypeName != ZR_NULL) {
        elementType = typeRef->elementTypeName;
    } else {
        elementType = primitive_type_name(cs, typeRef->elementBaseType);
    }
    if (elementType == ZR_NULL) {
        return ZR_NULL;
    }

    elementName = ZrCore_String_GetNativeString(elementType);
    if (elementName == ZR_NULL) {
        return ZR_NULL;
    }

    elementLength = strlen(elementName);
    totalLength = elementLength + 2;
    buffer = (TZrChar *)ZrCore_Memory_RawMalloc(cs->state->global, totalLength + 1);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(buffer, elementName, elementLength);
    buffer[elementLength] = '[';
    buffer[elementLength + 1] = ']';
    buffer[elementLength + 2] = '\0';
    result = ZrCore_String_Create(cs->state, buffer, totalLength);
    ZrCore_Memory_RawFree(cs->state->global, buffer, totalLength + 1);
    return result;
}

static void io_typed_type_ref_to_inferred(SZrCompilerState *cs,
                                          const SZrIoFunctionTypedTypeRef *typeRef,
                                          SZrInferredType *result) {
    SZrFunctionTypedTypeRef bridge;

    if (result == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(&bridge, 0, sizeof(bridge));
    if (typeRef != ZR_NULL) {
        bridge.baseType = typeRef->baseType;
        bridge.isNullable = typeRef->isNullable;
        bridge.ownershipQualifier = typeRef->ownershipQualifier;
        bridge.isArray = typeRef->isArray;
        bridge.typeName = typeRef->typeName;
        bridge.elementBaseType = typeRef->elementBaseType;
        bridge.elementTypeName = typeRef->elementTypeName;
    }
    function_typed_type_ref_to_inferred(cs, &bridge, result);
}

static SZrString *io_typed_type_ref_to_type_name(SZrCompilerState *cs, const SZrIoFunctionTypedTypeRef *typeRef) {
    SZrFunctionTypedTypeRef bridge;

    ZrCore_Memory_RawSet(&bridge, 0, sizeof(bridge));
    if (typeRef != ZR_NULL) {
        bridge.baseType = typeRef->baseType;
        bridge.isNullable = typeRef->isNullable;
        bridge.ownershipQualifier = typeRef->ownershipQualifier;
        bridge.isArray = typeRef->isArray;
        bridge.typeName = typeRef->typeName;
        bridge.elementBaseType = typeRef->elementBaseType;
        bridge.elementTypeName = typeRef->elementTypeName;
    }
    return typed_type_ref_to_type_name(cs, &bridge);
}

static void import_add_field_member(SZrState *state,
                                    SZrTypePrototypeInfo *info,
                                    SZrString *memberName,
                                    SZrString *fieldTypeName,
                                    TZrBool isStatic) {
    SZrTypeMemberInfo memberInfo;

    if (state == ZR_NULL || info == ZR_NULL || memberName == ZR_NULL || import_prototype_has_member(info, memberName)) {
        return;
    }

    ZrCore_Memory_RawSet(&memberInfo, 0, sizeof(memberInfo));
    memberInfo.memberType = info->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT ? ZR_AST_STRUCT_FIELD : ZR_AST_CLASS_FIELD;
    memberInfo.name = memberName;
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.isStatic = isStatic;
    memberInfo.fieldTypeName = fieldTypeName;
    ZrCore_Array_Push(state, &info->members, &memberInfo);
}

static void import_add_function_member_from_symbol(SZrCompilerState *cs,
                                                   SZrTypePrototypeInfo *modulePrototype,
                                                   const SZrFunctionTypedExportSymbol *symbol) {
    SZrTypeMemberInfo memberInfo;

    if (cs == ZR_NULL || modulePrototype == ZR_NULL || symbol == ZR_NULL || symbol->name == ZR_NULL ||
        import_prototype_has_member(modulePrototype, symbol->name)) {
        return;
    }

    ZrCore_Memory_RawSet(&memberInfo, 0, sizeof(memberInfo));
    memberInfo.memberType = ZR_AST_CLASS_METHOD;
    memberInfo.name = symbol->name;
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.isStatic = ZR_TRUE;
    memberInfo.parameterCount = symbol->parameterCount;
    memberInfo.returnTypeName = typed_type_ref_to_type_name(cs, &symbol->valueType);
    if (symbol->parameterCount > 0 && symbol->parameterTypes != ZR_NULL) {
        ZrCore_Array_Init(cs->state, &memberInfo.parameterTypes, sizeof(SZrInferredType), symbol->parameterCount);
        for (TZrUInt32 paramIndex = 0; paramIndex < symbol->parameterCount; paramIndex++) {
            SZrInferredType paramType;

            ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            function_typed_type_ref_to_inferred(cs, &symbol->parameterTypes[paramIndex], &paramType);
            ZrCore_Array_Push(cs->state, &memberInfo.parameterTypes, &paramType);
        }
    }

    ZrCore_Array_Push(cs->state, &modulePrototype->members, &memberInfo);
}

static void import_add_function_member_from_io_symbol(SZrCompilerState *cs,
                                                      SZrTypePrototypeInfo *modulePrototype,
                                                      const SZrIoFunctionTypedExportSymbol *symbol) {
    SZrTypeMemberInfo memberInfo;

    if (cs == ZR_NULL || modulePrototype == ZR_NULL || symbol == ZR_NULL || symbol->name == ZR_NULL ||
        import_prototype_has_member(modulePrototype, symbol->name)) {
        return;
    }

    ZrCore_Memory_RawSet(&memberInfo, 0, sizeof(memberInfo));
    memberInfo.memberType = ZR_AST_CLASS_METHOD;
    memberInfo.name = symbol->name;
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.isStatic = ZR_TRUE;
    memberInfo.parameterCount = (TZrUInt32)symbol->parameterCount;
    memberInfo.returnTypeName = io_typed_type_ref_to_type_name(cs, &symbol->valueType);
    if (symbol->parameterCount > 0 && symbol->parameterTypes != ZR_NULL) {
        ZrCore_Array_Init(cs->state, &memberInfo.parameterTypes, sizeof(SZrInferredType), symbol->parameterCount);
        for (TZrSize paramIndex = 0; paramIndex < symbol->parameterCount; paramIndex++) {
            SZrInferredType paramType;

            ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            io_typed_type_ref_to_inferred(cs, &symbol->parameterTypes[paramIndex], &paramType);
            ZrCore_Array_Push(cs->state, &memberInfo.parameterTypes, &paramType);
        }
    }

    ZrCore_Array_Push(cs->state, &modulePrototype->members, &memberInfo);
}

static SZrString *function_constant_string(SZrState *state, const SZrFunction *function, TZrUInt32 index) {
    const SZrTypeValue *constant;

    if (state == ZR_NULL || function == ZR_NULL || function->constantValueList == ZR_NULL ||
        index >= function->constantValueLength) {
        return ZR_NULL;
    }

    constant = &function->constantValueList[index];
    if (constant->type != ZR_VALUE_TYPE_STRING || constant->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_STRING(state, constant->value.object);
}

static TZrBool register_runtime_prototypes_from_function(SZrCompilerState *cs, const SZrFunction *function) {
    const TZrByte *currentPos;
    TZrSize remainingDataSize;
    TZrUInt32 prototypeCount;

    if (cs == ZR_NULL || function == ZR_NULL || function->prototypeData == ZR_NULL || function->prototypeDataLength <= sizeof(TZrUInt32) ||
        function->prototypeCount == 0) {
        return ZR_TRUE;
    }

    prototypeCount = function->prototypeCount;
    currentPos = function->prototypeData + sizeof(TZrUInt32);
    remainingDataSize = function->prototypeDataLength - sizeof(TZrUInt32);
    for (TZrUInt32 index = 0; index < prototypeCount; index++) {
        const SZrCompiledPrototypeInfo *protoInfo;
        TZrSize currentPrototypeSize;
        TZrUInt32 inheritsCount;
        TZrUInt32 membersCount;
        SZrString *prototypeName;
        SZrTypePrototypeInfo typePrototype;

        if (remainingDataSize < sizeof(SZrCompiledPrototypeInfo)) {
            return ZR_FALSE;
        }

        protoInfo = (const SZrCompiledPrototypeInfo *)currentPos;
        inheritsCount = protoInfo->inheritsCount;
        membersCount = protoInfo->membersCount;
        currentPrototypeSize = sizeof(SZrCompiledPrototypeInfo) +
                               inheritsCount * sizeof(TZrUInt32) +
                               membersCount * sizeof(SZrCompiledMemberInfo);
        if (remainingDataSize < currentPrototypeSize) {
            return ZR_FALSE;
        }

        prototypeName = function_constant_string(cs->state, function, protoInfo->nameStringIndex);
        if (prototypeName != ZR_NULL && find_compiler_type_prototype_inference(cs, prototypeName) == ZR_NULL) {
            import_type_prototype_init(cs->state,
                                       &typePrototype,
                                       prototypeName,
                                       (EZrObjectPrototypeType)protoInfo->type);
            typePrototype.accessModifier = (EZrAccessModifier)protoInfo->accessModifier;

            if (inheritsCount > 0) {
                const TZrUInt32 *inheritIndices =
                        (const TZrUInt32 *)(currentPos + sizeof(SZrCompiledPrototypeInfo));
                for (TZrUInt32 inheritIndex = 0; inheritIndex < inheritsCount; inheritIndex++) {
                    SZrString *inheritTypeName =
                            function_constant_string(cs->state, function, inheritIndices[inheritIndex]);
                    if (inheritTypeName != ZR_NULL) {
                        ZrCore_Array_Push(cs->state, &typePrototype.inherits, &inheritTypeName);
                        if (typePrototype.extendsTypeName == ZR_NULL) {
                            typePrototype.extendsTypeName = inheritTypeName;
                        }
                    }
                }
            }

            if (membersCount > 0) {
                const SZrCompiledMemberInfo *memberInfos = (const SZrCompiledMemberInfo *)(currentPos +
                                                                                           sizeof(SZrCompiledPrototypeInfo) +
                                                                                           inheritsCount * sizeof(TZrUInt32));
                for (TZrUInt32 memberIndex = 0; memberIndex < membersCount; memberIndex++) {
                    const SZrCompiledMemberInfo *compiledMember = &memberInfos[memberIndex];
                    SZrTypeMemberInfo memberInfo;

                    ZrCore_Memory_RawSet(&memberInfo, 0, sizeof(memberInfo));
                    memberInfo.memberType = (EZrAstNodeType)compiledMember->memberType;
                    memberInfo.name = function_constant_string(cs->state, function, compiledMember->nameStringIndex);
                    memberInfo.accessModifier = (EZrAccessModifier)compiledMember->accessModifier;
                    memberInfo.isStatic = compiledMember->isStatic ? ZR_TRUE : ZR_FALSE;
                    memberInfo.isConst = compiledMember->isConst ? ZR_TRUE : ZR_FALSE;
                    memberInfo.fieldTypeName =
                            function_constant_string(cs->state, function, compiledMember->fieldTypeNameStringIndex);
                    memberInfo.fieldOffset = compiledMember->fieldOffset;
                    memberInfo.fieldSize = compiledMember->fieldSize;
                    memberInfo.isMetaMethod = compiledMember->isMetaMethod ? ZR_TRUE : ZR_FALSE;
                    memberInfo.metaType = (EZrMetaType)compiledMember->metaType;
                    memberInfo.functionConstantIndex = compiledMember->functionConstantIndex;
                    memberInfo.parameterCount = compiledMember->parameterCount;
                    memberInfo.returnTypeName = function_constant_string(cs->state,
                                                                         function,
                                                                         compiledMember->returnTypeNameStringIndex);
                    memberInfo.isUsingManaged = compiledMember->isUsingManaged ? ZR_TRUE : ZR_FALSE;
                    memberInfo.ownershipQualifier = (EZrOwnershipQualifier)compiledMember->ownershipQualifier;
                    memberInfo.callsClose = compiledMember->callsClose ? ZR_TRUE : ZR_FALSE;
                    memberInfo.callsDestructor = compiledMember->callsDestructor ? ZR_TRUE : ZR_FALSE;
                    memberInfo.declarationOrder = compiledMember->declarationOrder;
                    ZrCore_Array_Push(cs->state, &typePrototype.members, &memberInfo);
                }
            }

            register_imported_type_name(cs, prototypeName);
            ZrCore_Array_Push(cs->state, &cs->typePrototypes, &typePrototype);
        }

        currentPos += currentPrototypeSize;
        remainingDataSize -= currentPrototypeSize;
    }

    return ZR_TRUE;
}

static TZrBool register_runtime_import_metadata(SZrCompilerState *cs,
                                                SZrString *moduleName,
                                                const SZrFunction *function) {
    SZrTypePrototypeInfo modulePrototype;
    TZrSize initialTypeCount;

    if (cs == ZR_NULL || moduleName == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    import_type_prototype_init(cs->state, &modulePrototype, moduleName, ZR_OBJECT_PROTOTYPE_TYPE_MODULE);
    initialTypeCount = cs->typePrototypes.length;
    if (!register_runtime_prototypes_from_function(cs, function)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; index++) {
        const SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];

        if (symbol->name == ZR_NULL) {
            continue;
        }

        if (symbol->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
            import_add_function_member_from_symbol(cs, &modulePrototype, symbol);
        } else {
            import_add_field_member(cs->state,
                                    &modulePrototype,
                                    symbol->name,
                                    typed_type_ref_to_type_name(cs, &symbol->valueType),
                                    ZR_TRUE);
        }
    }

    for (TZrSize index = initialTypeCount; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, index);
        if (info != ZR_NULL && info->name != ZR_NULL && info->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
            import_add_field_member(cs->state, &modulePrototype, info->name, info->name, ZR_TRUE);
        }
    }

    register_imported_type_name(cs, moduleName);
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &modulePrototype);
    return ZR_TRUE;
}

static void register_io_type_prototype_stub(SZrCompilerState *cs,
                                            SZrTypePrototypeInfo *modulePrototype,
                                            SZrString *typeName,
                                            EZrObjectPrototypeType type) {
    SZrTypePrototypeInfo typePrototype;

    if (cs == ZR_NULL || modulePrototype == ZR_NULL || typeName == ZR_NULL ||
        find_compiler_type_prototype_inference(cs, typeName) != ZR_NULL) {
        return;
    }

    import_type_prototype_init(cs->state, &typePrototype, typeName, type);
    register_imported_type_name(cs, typeName);
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &typePrototype);
    import_add_field_member(cs->state, modulePrototype, typeName, typeName, ZR_TRUE);
}

static TZrBool register_binary_import_metadata(SZrCompilerState *cs,
                                               SZrString *moduleName,
                                               const SZrIoFunction *function) {
    SZrTypePrototypeInfo modulePrototype;

    if (cs == ZR_NULL || moduleName == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    import_type_prototype_init(cs->state, &modulePrototype, moduleName, ZR_OBJECT_PROTOTYPE_TYPE_MODULE);
    for (TZrSize index = 0; index < function->typedExportedSymbolsLength; index++) {
        const SZrIoFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];

        if (symbol->name == ZR_NULL) {
            continue;
        }

        if (symbol->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
            import_add_function_member_from_io_symbol(cs, &modulePrototype, symbol);
        } else {
            import_add_field_member(cs->state,
                                    &modulePrototype,
                                    symbol->name,
                                    io_typed_type_ref_to_type_name(cs, &symbol->valueType),
                                    ZR_TRUE);
        }
    }

    if (function->classes != ZR_NULL) {
        for (TZrSize index = 0; index < function->classesLength; index++) {
            register_io_type_prototype_stub(cs,
                                            &modulePrototype,
                                            function->classes[index].name,
                                            ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        }
    }
    if (function->structs != ZR_NULL) {
        for (TZrSize index = 0; index < function->structsLength; index++) {
            register_io_type_prototype_stub(cs,
                                            &modulePrototype,
                                            function->structs[index].name,
                                            ZR_OBJECT_PROTOTYPE_TYPE_STRUCT);
        }
    }

    register_imported_type_name(cs, moduleName);
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &modulePrototype);
    return ZR_TRUE;
}

TZrBool ensure_import_module_compile_info(SZrCompilerState *cs, SZrString *moduleName) {
    SZrGlobalState *global;
    TZrNativeString moduleNameText;
    TZrSize moduleNameLength;
    SZrIo io;
    TZrBool loaderSuccess;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (find_compiler_type_prototype_inference(cs, moduleName) != ZR_NULL) {
        return ZR_TRUE;
    }

    if (ensure_native_module_compile_info(cs, moduleName)) {
        return ZR_TRUE;
    }

    global = cs->state->global;
    if (global->sourceLoader == ZR_NULL) {
        return ZR_FALSE;
    }

    moduleNameText = ZrCore_String_GetNativeString(moduleName);
    if (moduleNameText == ZR_NULL) {
        return ZR_FALSE;
    }
    moduleNameLength = strlen(moduleNameText);
    ZrCore_Io_Init(cs->state, &io, ZR_NULL, ZR_NULL, ZR_NULL);
    loaderSuccess = global->sourceLoader(cs->state, moduleNameText, ZR_NULL, &io);
    if (!loaderSuccess) {
        return ZR_FALSE;
    }

    if (io.isBinary) {
        SZrIoSource *source = ZrCore_Io_ReadSourceNew(&io);
        TZrBool success = ZR_FALSE;

        if (io.close != ZR_NULL) {
            io.close(cs->state, io.customData);
        }

        if (source != ZR_NULL &&
            source->modulesLength > 0 &&
            source->modules != ZR_NULL &&
            source->modules[0].entryFunction != ZR_NULL) {
            success = register_binary_import_metadata(cs, moduleName, source->modules[0].entryFunction);
        }

        return success;
    }

    if (global->compileSource != ZR_NULL) {
        TZrSize sourceSize = 0;
        TZrBytePtr sourceBuffer = read_all_import_bytes(cs->state, &io, &sourceSize);
        SZrFunction *compiledFunction = ZR_NULL;
        TZrBool success = ZR_FALSE;

        if (io.close != ZR_NULL) {
            io.close(cs->state, io.customData);
        }

        if (sourceBuffer == ZR_NULL || sourceSize == 0) {
            if (sourceBuffer != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(global,
                                              sourceBuffer,
                                              sourceSize + 1,
                                              ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
            return ZR_FALSE;
        }

        compiledFunction = global->compileSource(cs->state,
                                                 (const TZrChar *)sourceBuffer,
                                                 sourceSize,
                                                 ZrCore_String_Create(cs->state, moduleNameText, moduleNameLength));
        ZrCore_Memory_RawFreeWithType(global,
                                      sourceBuffer,
                                      sourceSize + 1,
                                      ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        if (compiledFunction == ZR_NULL) {
            return ZR_FALSE;
        }

        success = register_runtime_import_metadata(cs, moduleName, compiledFunction);
        ZrCore_Function_Free(cs->state, compiledFunction);
        return success;
    }

    if (io.close != ZR_NULL) {
        io.close(cs->state, io.customData);
    }
    return ZR_FALSE;
}
