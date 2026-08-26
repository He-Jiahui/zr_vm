//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"
#include "compiler/compile_time_executor_internal.h"
#include "compiler/compiler_attribute_binding.h"
#include "zr_vm_parser/semantic_calls.h"
#include "semantic/semantic_scope_facts.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "module_init_analysis.h"
#include "type_inference_internal.h"
#include "zr_vm_library/native_registry.h"

static TZrBool zr_parser_compile_trace_enabled(void);
static void zr_parser_compile_trace(const TZrChar *format, ...);
static void zr_parser_source_compile_capture_error(TZrPtr userData,
                                                   const SZrFileRange *location,
                                                   const TZrChar *message,
                                                   EZrToken token);

static void zr_parser_source_compile_discard_cache(
        SZrState *state,
        SZrParserSourceComptimeCache *cache) {
    if (state == ZR_NULL || cache == ZR_NULL || cache->outputSnapshot == ZR_NULL) {
        return;
    }
    ZrParser_ComptimeCache_FreeSnapshot(
            state, cache->outputSnapshot, cache->outputSnapshotSize);
    cache->outputSnapshot = ZR_NULL;
    cache->outputSnapshotSize = 0U;
}

static TZrBool compiler_refresh_borrowed_child_function_graph(SZrState *state,
                                                              SZrFunction *function,
                                                              const SZrFunction *sourceRoot) {
    SZrGlobalState *global;
    TZrSize childFuncSize;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL || sourceRoot == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!function->childFunctionGraphIsBorrowed) {
        return ZR_TRUE;
    }

    global = state->global;
    if (sourceRoot->childFunctionList == ZR_NULL || sourceRoot->childFunctionLength == 0) {
        if (function->childFunctionList != ZR_NULL && function->childFunctionLength > 0) {
            ZrCore_Memory_RawFreeWithType(global,
                                          function->childFunctionList,
                                          sizeof(SZrFunction) * function->childFunctionLength,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        function->childFunctionList = ZR_NULL;
        function->childFunctionLength = 0;
        return ZR_TRUE;
    }

    childFuncSize = sizeof(SZrFunction) * sourceRoot->childFunctionLength;
    if (function->childFunctionList == ZR_NULL || function->childFunctionLength != sourceRoot->childFunctionLength) {
        if (function->childFunctionList != ZR_NULL && function->childFunctionLength > 0) {
            ZrCore_Memory_RawFreeWithType(global,
                                          function->childFunctionList,
                                          sizeof(SZrFunction) * function->childFunctionLength,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }

        function->childFunctionList =
                (SZrFunction *)ZrCore_Memory_RawMallocWithType(global, childFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->childFunctionList == ZR_NULL) {
            function->childFunctionLength = 0;
            return ZR_FALSE;
        }
    }

    memcpy(function->childFunctionList, sourceRoot->childFunctionList, childFuncSize);
    function->childFunctionLength = sourceRoot->childFunctionLength;
    function->childFunctionGraphIsBorrowed = ZR_TRUE;
    ZrCore_Function_RebindConstantFunctionValuesToChildren(function);
    ZrCore_Function_ClearChildOwnerLinks(function);
    return ZR_TRUE;
}

// 初始化编译器状态

static void compiler_accumulate_protocol_mask_from_type_names(SZrCompilerState *cs,
                                                              const SZrArray *typeNames,
                                                              TZrUInt64 *protocolMask) {
    if (cs == ZR_NULL || typeNames == ZR_NULL || protocolMask == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < typeNames->length; index++) {
        SZrString **typeNamePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)typeNames, index);
        SZrTypePrototypeInfo *prototype;

        if (typeNamePtr == ZR_NULL || *typeNamePtr == ZR_NULL) {
            continue;
        }

        ensure_generic_instance_type_prototype(cs, *typeNamePtr);
        prototype = find_compiler_type_prototype_inference(cs, *typeNamePtr);
        if (prototype != ZR_NULL) {
            *protocolMask |= prototype->protocolMask;
        }
    }
}

static TZrUInt64 compiler_protocol_mask_from_prototype_info(SZrCompilerState *cs, SZrTypePrototypeInfo *info) {
    TZrUInt64 protocolMask = 0;

    if (cs == ZR_NULL || info == ZR_NULL) {
        return 0;
    }

    protocolMask = info->protocolMask;
    compiler_accumulate_protocol_mask_from_type_names(cs, &info->inherits, &protocolMask);
    compiler_accumulate_protocol_mask_from_type_names(cs, &info->implements, &protocolMask);
    return protocolMask;
}

static TZrUInt32 compiler_member_contract_role_from_member_info(const SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL) {
        return ZR_MEMBER_CONTRACT_ROLE_NONE;
    }

    return memberInfo->contractRole;
}

static TZrBool compiler_add_decorator_name_array_constant(SZrCompilerState *cs,
                                                          const SZrArray *decorators,
                                                          TZrUInt32 *outConstantIndex) {
    SZrObject *decoratorArray;
    SZrTypeValue arrayValue;

    if (outConstantIndex != ZR_NULL) {
        *outConstantIndex = 0;
    }

    if (cs == ZR_NULL || decorators == ZR_NULL || outConstantIndex == ZR_NULL || decorators->length == 0) {
        return ZR_TRUE;
    }

    decoratorArray = ZrCore_Object_NewCustomized(cs->state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (decoratorArray == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(cs->state, decoratorArray);

    for (TZrSize index = 0; index < decorators->length; index++) {
        SZrTypeDecoratorInfo *decoratorInfo = (SZrTypeDecoratorInfo *)ZrCore_Array_Get((SZrArray *)decorators, index);
        SZrTypeValue keyValue;
        SZrTypeValue entryValue;

        if (decoratorInfo == ZR_NULL || decoratorInfo->name == ZR_NULL) {
            continue;
        }

        ZrCore_Value_InitAsInt(cs->state, &keyValue, (TZrInt64)index);
        ZrCore_Value_InitAsRawObject(cs->state,
                                     &entryValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(decoratorInfo->name));
        entryValue.type = ZR_VALUE_TYPE_STRING;
        ZrCore_Object_SetValue(cs->state, decoratorArray, &keyValue, &entryValue);
    }

    ZrCore_Value_InitAsRawObject(cs->state, &arrayValue, ZR_CAST_RAW_OBJECT_AS_SUPER(decoratorArray));
    arrayValue.type = ZR_VALUE_TYPE_ARRAY;
    *outConstantIndex = add_constant(cs, &arrayValue);
    return ZR_TRUE;
}

static TZrUInt32 compiler_resolve_serialized_member_function_constant_index(SZrCompilerState *cs,
                                                                            const SZrTypeMemberInfo *memberInfo) {
    SZrTypeValue functionValue;
    SZrTypeValue *existingValue;

    if (cs == ZR_NULL || memberInfo == ZR_NULL || memberInfo->compiledFunction == ZR_NULL) {
        return memberInfo != ZR_NULL ? memberInfo->functionConstantIndex : 0;
    }

    /*
     * Member functions are already registered in the enclosing compiler state's
     * constant pool when the member is compiled. Reusing that canonical index
     * keeps prototype serialization deterministic across toolchains instead of
     * depending on a second add_constant() pass to rediscover the same object.
     */
    if (memberInfo->functionConstantIndex < cs->constants.length) {
        existingValue = (SZrTypeValue *)ZrCore_Array_Get(&cs->constants, memberInfo->functionConstantIndex);
        if (existingValue != ZR_NULL &&
            (existingValue->type == ZR_VALUE_TYPE_FUNCTION || existingValue->type == ZR_VALUE_TYPE_CLOSURE)) {
            return memberInfo->functionConstantIndex;
        }
    }

    ZrCore_Value_InitAsRawObject(cs->state,
                                 &functionValue,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(memberInfo->compiledFunction));
    return add_constant(cs, &functionValue);
}

static TZrUInt32 compiler_add_serialized_string_constant(SZrCompilerState *cs, SZrString *value) {
    SZrTypeValue stringValue;
    TZrUInt32 index;

    if (cs == ZR_NULL || value == ZR_NULL) {
        return 0;
    }

    ZrCore_Value_InitAsRawObject(cs->state, &stringValue, ZR_CAST_RAW_OBJECT_AS_SUPER(value));
    stringValue.type = ZR_VALUE_TYPE_STRING;

    /* Serialized prototype fields reserve index zero for "not present". */
    for (TZrSize constantIndex = 1u; constantIndex < cs->constants.length; constantIndex++) {
        SZrTypeValue *existingValue =
                (SZrTypeValue *)ZrCore_Array_Get(&cs->constants, constantIndex);
        if (existingValue != ZR_NULL &&
            ZrCore_Value_Equal(cs->state, existingValue, &stringValue)) {
            return (TZrUInt32)constantIndex;
        }
    }

    index = add_constant(cs, &stringValue);
    if (index != 0u) {
        return index;
    }

    ZrCore_Array_Push(cs->state, &cs->constants, &stringValue);
    cs->constantCount = cs->constants.length;
    return (TZrUInt32)(cs->constants.length - 1u);
}

static void compiler_log_failure_summary(SZrCompilerState *cs) {
    const TZrChar *reason = "Unknown error";

    if (cs != ZR_NULL && cs->errorMessage != ZR_NULL) {
        reason = cs->errorMessage;
    }

    ZrCore_Log_Diagnosticf(cs != ZR_NULL ? cs->state : ZR_NULL,
                           ZR_LOG_LEVEL_ERROR,
                           ZR_OUTPUT_CHANNEL_STDERR,
                           "\n=== Compilation Summary ===\n"
                           "Status: FAILED\n"
                           "Reason: %s\n",
                           reason);
}

static TZrBool compiler_prototype_serializes_to_function_metadata(
        const SZrTypePrototypeInfo *info) {
    const TZrUInt64 contiguousViewProtocols =
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_VIEW_MUTABLE) |
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_VIEW_READONLY);

    if (info == ZR_NULL || info->name == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!info->isImportedNative) {
        return ZR_TRUE;
    }
    return (TZrBool)((info->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT ||
                      info->type == ZR_OBJECT_PROTOTYPE_TYPE_UNION) &&
                     info->layoutByteSize > 0u &&
                     info->layoutByteAlign > 0u &&
                     (info->protocolMask & contiguousViewProtocols) != 0u);
}

TZrBool serialize_prototype_info_to_binary(SZrCompilerState *cs, SZrTypePrototypeInfo *info, 
                                                 TZrByte **outData, TZrSize *outSize) {
    if (cs == ZR_NULL || info == ZR_NULL || info->name == ZR_NULL || outData == ZR_NULL || outSize == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 注意：为了保持格式兼容，我们仍然使用字符串索引
    // 但这些索引现在指向 prototype 数据内部的字符串表，而不是常量池
    // TODO: 为了简化实现，我们暂时仍然使用常量池索引，但后续会改为内部字符串表
    
    // 1. 使用C原生结构收集数据，避免创建VM对象
    // 先将所有字符串添加到常量池，获取索引（临时方案，后续改为内部字符串表）
    TZrUInt32 nameStringIndex = compiler_add_serialized_string_constant(cs, info->name);
    
    // 2. 添加继承类型名称字符串到常量池
    TZrUInt32 *inheritStringIndices = ZR_NULL;
    TZrUInt32 *decoratorNameIndices = ZR_NULL;
    TZrUInt32 inheritsCount = (TZrUInt32)info->inherits.length;
    TZrUInt32 decoratorsCount = (TZrUInt32)info->decorators.length;
    if (inheritsCount > 0) {
        inheritStringIndices = (TZrUInt32 *)ZrCore_Memory_RawMalloc(cs->state->global, inheritsCount * sizeof(TZrUInt32));
        if (inheritStringIndices == ZR_NULL) {
            return ZR_FALSE;
        }
        
        for (TZrSize i = 0; i < info->inherits.length; i++) {
            SZrString **inheritTypeNamePtr = (SZrString **)ZrCore_Array_Get(&info->inherits, i);
            if (inheritTypeNamePtr != ZR_NULL && *inheritTypeNamePtr != ZR_NULL) {
                SZrTypeValue inheritValue;
                ZrCore_Value_InitAsRawObject(cs->state, &inheritValue, ZR_CAST_RAW_OBJECT_AS_SUPER(*inheritTypeNamePtr));
                inheritValue.type = ZR_VALUE_TYPE_STRING;
                inheritStringIndices[i] = add_constant(cs, &inheritValue);
            } else {
                inheritStringIndices[i] = 0;
            }
        }
    }

    if (decoratorsCount > 0) {
        decoratorNameIndices =
                (TZrUInt32 *)ZrCore_Memory_RawMalloc(cs->state->global, decoratorsCount * sizeof(TZrUInt32));
        if (decoratorNameIndices == ZR_NULL) {
            if (inheritStringIndices != ZR_NULL) {
                ZrCore_Memory_RawFree(cs->state->global, inheritStringIndices, inheritsCount * sizeof(TZrUInt32));
            }
            return ZR_FALSE;
        }

        for (TZrSize i = 0; i < info->decorators.length; i++) {
            SZrTypeDecoratorInfo *decoratorInfo =
                    (SZrTypeDecoratorInfo *)ZrCore_Array_Get(&info->decorators, i);
            if (decoratorInfo != ZR_NULL && decoratorInfo->name != ZR_NULL) {
                SZrTypeValue decoratorNameValue;
                ZrCore_Value_InitAsRawObject(cs->state,
                                            &decoratorNameValue,
                                            ZR_CAST_RAW_OBJECT_AS_SUPER(decoratorInfo->name));
                decoratorNameValue.type = ZR_VALUE_TYPE_STRING;
                decoratorNameIndices[i] = add_constant(cs, &decoratorNameValue);
            } else {
                decoratorNameIndices[i] = 0;
            }
        }
    }
    
    // 3. 计算序列化数据大小（使用C原生结构）
    TZrUInt32 membersCount = (TZrUInt32)info->members.length;
    TZrSize serializedSize = sizeof(SZrCompiledPrototypeInfo) + 
                             (inheritsCount > 0 ? inheritsCount * sizeof(TZrUInt32) : 0) +
                             (decoratorsCount > 0 ? decoratorsCount * sizeof(TZrUInt32) : 0) +
                             membersCount * sizeof(SZrCompiledMemberInfo);
    
    // 4. 分配序列化数据缓冲区（C原生内存，非VM对象）
    TZrByte *serializedData = (TZrByte *)ZrCore_Memory_RawMalloc(cs->state->global, serializedSize);
    if (serializedData == ZR_NULL) {
        if (inheritStringIndices != ZR_NULL) {
            ZrCore_Memory_RawFree(cs->state->global, inheritStringIndices, inheritsCount * sizeof(TZrUInt32));
        }
        if (decoratorNameIndices != ZR_NULL) {
            ZrCore_Memory_RawFree(cs->state->global, decoratorNameIndices, decoratorsCount * sizeof(TZrUInt32));
        }
        return ZR_FALSE;
    }
    
    // 5. 填充序列化数据（使用C原生结构，避免指针，所有数据直接嵌入）
    SZrCompiledPrototypeInfo *protoInfo = (SZrCompiledPrototypeInfo *)serializedData;
    ZrCore_Memory_RawSet(protoInfo, 0, sizeof(*protoInfo));
    protoInfo->nameStringIndex = nameStringIndex;
    protoInfo->type = (TZrUInt32)info->type;
    protoInfo->accessModifier = (TZrUInt32)info->accessModifier;
    protoInfo->inheritsCount = inheritsCount;
    protoInfo->membersCount = membersCount;
    protoInfo->protocolMask = compiler_protocol_mask_from_prototype_info(cs, info);
    protoInfo->hasDecoratorMetadata = info->hasDecoratorMetadata ? ZR_TRUE : ZR_FALSE;
    protoInfo->decoratorMetadataConstantIndex =
            info->hasDecoratorMetadata ? add_constant(cs, &info->decoratorMetadataValue) : 0;
    protoInfo->decoratorsCount = decoratorsCount;
    protoInfo->modifierFlags =
            info->modifierFlags |
            (info->isImportedNative
                     ? ZR_TYPE_MODIFIER_FLAG_IMPORTED_LAYOUT_ONLY
                     : 0u);
    protoInfo->nextVirtualSlotIndex = info->nextVirtualSlotIndex;
    protoInfo->nextPropertyIdentity = info->nextPropertyIdentity;
    protoInfo->layoutByteSize = info->layoutByteSize;
    protoInfo->layoutByteAlign = info->layoutByteAlign;
    
    // 复制继承类型索引数组到序列化数据中（紧跟在结构体后面）
    TZrUInt32 *embeddedInheritIndices = (TZrUInt32 *)(serializedData + sizeof(SZrCompiledPrototypeInfo));
    if (inheritsCount > 0 && inheritStringIndices != ZR_NULL) {
        memcpy(embeddedInheritIndices, inheritStringIndices, inheritsCount * sizeof(TZrUInt32));
        ZrCore_Memory_RawFree(cs->state->global, inheritStringIndices, inheritsCount * sizeof(TZrUInt32));
    }

    TZrUInt32 *embeddedDecoratorIndices = embeddedInheritIndices + inheritsCount;
    if (decoratorsCount > 0 && decoratorNameIndices != ZR_NULL) {
        memcpy(embeddedDecoratorIndices, decoratorNameIndices, decoratorsCount * sizeof(TZrUInt32));
        ZrCore_Memory_RawFree(cs->state->global, decoratorNameIndices, decoratorsCount * sizeof(TZrUInt32));
    }

    // 序列化成员信息（紧跟在继承数组后面）
    SZrCompiledMemberInfo *members = (SZrCompiledMemberInfo *)(serializedData + 
                                                                 sizeof(SZrCompiledPrototypeInfo) +
                                                                 inheritsCount * sizeof(TZrUInt32) +
                                                                 decoratorsCount * sizeof(TZrUInt32));
    for (TZrSize i = 0; i < info->members.length; i++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, i);
        if (memberInfo == ZR_NULL) {
            continue;
        }
        
        SZrCompiledMemberInfo *compiledMember = &members[i];
        ZrCore_Memory_RawSet(compiledMember, 0, sizeof(*compiledMember));
        compiledMember->memberType = (TZrUInt32)memberInfo->memberType;
        compiledMember->accessModifier = (TZrUInt32)memberInfo->accessModifier;
        compiledMember->isStatic = memberInfo->isStatic ? ZR_TRUE : ZR_FALSE;
        compiledMember->isConst =
                memberInfo->isConst ||
                        memberInfo->receiverEffect ==
                                ZR_CANONICAL_RECEIVER_READONLY
                        ? ZR_TRUE
                        : ZR_FALSE;
        compiledMember->reservedRemovedUsingManaged = 0u;
        compiledMember->ownershipQualifier = (TZrUInt32)memberInfo->ownershipQualifier;
        compiledMember->callsClose = memberInfo->callsClose ? ZR_TRUE : ZR_FALSE;
        compiledMember->callsDestructor = memberInfo->callsDestructor ? ZR_TRUE : ZR_FALSE;
        compiledMember->declarationOrder = memberInfo->declarationOrder;
        compiledMember->contractRole = compiler_member_contract_role_from_member_info(memberInfo);
        compiledMember->hasDecoratorMetadata = memberInfo->hasDecoratorMetadata ? ZR_TRUE : ZR_FALSE;
        compiledMember->decoratorMetadataConstantIndex =
                memberInfo->hasDecoratorMetadata ? add_constant(cs, &memberInfo->decoratorMetadataValue) : 0;
        compiledMember->hasDecoratorNames = memberInfo->decorators.length > 0 ? ZR_TRUE : ZR_FALSE;
        compiledMember->decoratorNamesConstantIndex = 0;
        compiledMember->modifierFlags = memberInfo->modifierFlags;
        compiledMember->ownerTypeNameStringIndex =
                compiler_add_serialized_string_constant(cs, memberInfo->ownerTypeName);
        compiledMember->baseDefinitionOwnerTypeNameStringIndex =
                compiler_add_serialized_string_constant(cs, memberInfo->baseDefinitionOwnerTypeName);
        compiledMember->baseDefinitionNameStringIndex =
                compiler_add_serialized_string_constant(cs, memberInfo->baseDefinitionName);
        compiledMember->virtualSlotIndex = memberInfo->virtualSlotIndex;
        compiledMember->interfaceContractSlot = memberInfo->interfaceContractSlot;
        compiledMember->propertyIdentity = memberInfo->propertyIdentity;
        compiledMember->accessorRole = memberInfo->accessorRole;
        if (compiledMember->hasDecoratorNames &&
            !compiler_add_decorator_name_array_constant(cs,
                                                        &memberInfo->decorators,
                                                        &compiledMember->decoratorNamesConstantIndex)) {
            ZrCore_Memory_RawFree(cs->state->global, serializedData, serializedSize);
            return ZR_FALSE;
        }

        // 添加成员名称字符串到常量池（临时方案）
        compiledMember->nameStringIndex = compiler_add_serialized_string_constant(cs, memberInfo->name);
        
        // 字段特定信息
        if (memberInfo->memberType == ZR_AST_STRUCT_FIELD || memberInfo->memberType == ZR_AST_CLASS_FIELD) {
            compiledMember->fieldTypeNameStringIndex =
                    compiler_add_serialized_string_constant(cs, memberInfo->fieldTypeName);
            compiledMember->fieldOffset = memberInfo->fieldOffset;
            compiledMember->fieldSize = memberInfo->fieldSize;
            // 方法字段清零
            compiledMember->isMetaMethod = ZR_FALSE;
            compiledMember->metaType = 0;
            compiledMember->functionConstantIndex = 0;
            compiledMember->parameterCount = 0;
            compiledMember->returnTypeNameStringIndex = 0;
        }

        if (memberInfo->memberType == ZR_AST_UNION_VARIANT) {
            compiledMember->fieldTypeNameStringIndex =
                    compiler_add_serialized_string_constant(cs, memberInfo->fieldTypeName);
            compiledMember->fieldOffset = memberInfo->fieldOffset;
            compiledMember->fieldSize = memberInfo->fieldSize;
            compiledMember->parameterCount = memberInfo->parameterCount;
            compiledMember->returnTypeNameStringIndex =
                    compiler_add_serialized_string_constant(cs, memberInfo->returnTypeName);
        }

        if (memberInfo->memberType == ZR_AST_PROPERTY_DECLARATION) {
            compiledMember->fieldTypeNameStringIndex =
                    compiler_add_serialized_string_constant(cs, memberInfo->fieldTypeName);
            compiledMember->parameterCount = memberInfo->propertyValueTypeId;
            compiledMember->metaType =
                    memberInfo->fieldType != ZR_NULL
                            ? (TZrUInt32)memberInfo->fieldType->referenceAccess
                            : (TZrUInt32)ZR_REFERENCE_ACCESS_NONE;
            compiledMember->isMetaMethod =
                    memberInfo->exportsWritableRef ? ZR_TRUE : ZR_FALSE;
        }
        
        // 方法特定信息
        if (memberInfo->memberType == ZR_AST_STRUCT_METHOD || 
            memberInfo->memberType == ZR_AST_STRUCT_META_FUNCTION ||
            memberInfo->memberType == ZR_AST_CLASS_METHOD ||
            memberInfo->memberType == ZR_AST_CLASS_META_FUNCTION) {
            compiledMember->isMetaMethod = memberInfo->isMetaMethod ? ZR_TRUE : ZR_FALSE;
            compiledMember->metaType = (TZrUInt32)memberInfo->metaType;
            if (memberInfo->compiledFunction != ZR_NULL) {
                compiledMember->functionConstantIndex =
                        compiler_resolve_serialized_member_function_constant_index(cs, memberInfo);
            } else {
                compiledMember->functionConstantIndex = memberInfo->functionConstantIndex;
            }
            compiledMember->parameterCount = memberInfo->parameterCount;
            // 处理返回类型名称
            compiledMember->returnTypeNameStringIndex =
                    compiler_add_serialized_string_constant(cs, memberInfo->returnTypeName);
            // 字段字段清零
            compiledMember->fieldTypeNameStringIndex = 0;
            compiledMember->fieldOffset = 0;
            compiledMember->fieldSize = 0;
        } else if (memberInfo->memberType != ZR_AST_UNION_VARIANT) {
            // 非方法成员，返回类型字段清零
            compiledMember->returnTypeNameStringIndex = 0;
        }
    }
    
    // 6. 返回序列化数据（不存储到常量池）
    *outData = serializedData;
    *outSize = serializedSize;
    
    return ZR_TRUE;
}

static void compiler_compile_top_level_statement(SZrCompilerState *cs, SZrAstNode *statement) {
    if (cs == ZR_NULL || statement == ZR_NULL || cs->hasError) {
        return;
    }

    if (compiler_is_compile_tool_import_declaration(cs->state, statement)) {
        return;
    }

    if (statement->type == ZR_AST_COMPILE_TIME_DECLARATION) {
        SZrCompileTimeDeclaration *declaration = &statement->data.compileTimeDeclaration;
        if (declaration->isConditionalPruning && declaration->selectedBranch != ZR_NULL) {
            SZrAstNode *selectedBranch = declaration->selectedBranch;
            if (selectedBranch->type == ZR_AST_BLOCK && selectedBranch->data.block.body != ZR_NULL) {
                for (TZrSize index = 0; index < selectedBranch->data.block.body->count; index++) {
                    compiler_compile_top_level_statement(cs, selectedBranch->data.block.body->nodes[index]);
                    if (cs->hasError) {
                        return;
                    }
                }
            } else {
                compiler_compile_top_level_statement(cs, selectedBranch);
            }
            return;
        }
        return;
    }

    switch (statement->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            compile_function_declaration(cs, statement);
            break;
        case ZR_AST_VARIABLE_DECLARATION:
        case ZR_AST_EXPRESSION_STATEMENT:
        case ZR_AST_USING_STATEMENT:
        case ZR_AST_BLOCK:
        case ZR_AST_RETURN_STATEMENT:
        case ZR_AST_THROW_STATEMENT:
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
        case ZR_AST_IF_EXPRESSION:
        case ZR_AST_SWITCH_EXPRESSION:
        case ZR_AST_WHILE_LOOP:
        case ZR_AST_FOR_LOOP:
        case ZR_AST_FOREACH_LOOP:
            ZrParser_Statement_Compile(cs, statement);
            break;
        case ZR_AST_STRUCT_DECLARATION:
            compile_struct_declaration(cs, statement);
            break;
        case ZR_AST_EXTERN_BLOCK:
            compile_extern_block_declaration(cs, statement);
            break;
        case ZR_AST_CLASS_DECLARATION:
            compile_class_declaration(cs, statement);
            break;
        case ZR_AST_INTERFACE_DECLARATION:
            for (TZrSize index = 0;
                 index < cs->signatureCompiledInterfaceNodes.length;
                 index++) {
                SZrAstNode **compiledNode =
                        (SZrAstNode **)ZrCore_Array_Get(
                                &cs->signatureCompiledInterfaceNodes, index);
                if (compiledNode != ZR_NULL && *compiledNode == statement) {
                    compiler_finalize_interface_decorators(cs, statement);
                    return;
                }
            }
            compile_interface_declaration(cs, statement);
            break;
        case ZR_AST_ENUM_DECLARATION:
            compile_enum_declaration(cs, statement);
            break;
        case ZR_AST_UNION_DECLARATION:
            compile_union_declaration(cs, statement);
            break;
        default:
            ZrCore_Log_Diagnosticf(cs->state,
                                   ZR_LOG_LEVEL_WARNING,
                                   ZR_OUTPUT_CHANNEL_STDERR,
                                   "    Skipping statement type %d (not implemented yet)\n",
                                   statement->type);
            break;
    }
}

static void compiler_compile_interface_signatures(
        SZrCompilerState *cs,
        SZrAstNodeArray *statements) {
    if (cs == ZR_NULL || statements == ZR_NULL || cs->hasError) {
        return;
    }
    for (TZrSize index = 0; index < statements->count; index++) {
        SZrAstNode *statement = statements->nodes[index];

        if (statement == ZR_NULL) {
            continue;
        }
        if (statement->type == ZR_AST_COMPILE_TIME_DECLARATION &&
            statement->data.compileTimeDeclaration.isConditionalPruning &&
            statement->data.compileTimeDeclaration.selectedBranch != ZR_NULL) {
            SZrAstNode *selected =
                    statement->data.compileTimeDeclaration.selectedBranch;
            if (selected->type == ZR_AST_BLOCK) {
                compiler_compile_interface_signatures(
                        cs, selected->data.block.body);
            } else {
                SZrAstNode *selectedNodes[] = {selected};
                SZrAstNodeArray selectedArray = {
                        .nodes = selectedNodes,
                        .count = 1U,
                        .capacity = 1U,
                };
                compiler_compile_interface_signatures(cs, &selectedArray);
            }
            if (cs->hasError) {
                return;
            }
            continue;
        }
        if (statement->type != ZR_AST_INTERFACE_DECLARATION) {
            continue;
        }
        compile_interface_declaration(cs, statement);
        if (cs->hasError) {
            return;
        }
        ZrCore_Array_Push(
                cs->state,
                &cs->signatureCompiledInterfaceNodes,
                &statement);
    }
}

static void compiler_predeclare_attribute_schemas(
        SZrCompilerState *cs,
        SZrAstNodeArray *statements) {
    if (cs == ZR_NULL || statements == ZR_NULL || cs->hasError) {
        return;
    }
    for (TZrSize index = 0U; index < statements->count; index++) {
        SZrAstNode *statement = statements->nodes[index];

        if (statement == ZR_NULL) {
            continue;
        }
        if (statement->type == ZR_AST_COMPILE_TIME_DECLARATION &&
            statement->data.compileTimeDeclaration.isConditionalPruning &&
            statement->data.compileTimeDeclaration.selectedBranch != ZR_NULL) {
            SZrAstNode *selected =
                    statement->data.compileTimeDeclaration.selectedBranch;
            if (selected->type == ZR_AST_BLOCK) {
                compiler_predeclare_attribute_schemas(
                        cs, selected->data.block.body);
            } else {
                SZrAstNode *selectedNodes[] = {selected};
                SZrAstNodeArray selectedArray = {
                        .nodes = selectedNodes,
                        .count = 1U,
                        .capacity = 1U,
                };
                compiler_predeclare_attribute_schemas(cs, &selectedArray);
            }
            if (cs->hasError) {
                return;
            }
            continue;
        }
        if (statement->type == ZR_AST_STRUCT_DECLARATION &&
            !ZrParser_Metadata_RegisterAttributeSchema(cs, statement)) {
            return;
        }
    }
}

// 编译脚本
ZR_PARSER_API void compile_script(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_SCRIPT) {
        ZrParser_Compiler_Error(cs, "Expected script node", node->location);
        return;
    }

    SZrScript *script = &node->data.script;

    if (cs->semanticContext != ZR_NULL) {
        ZrParser_SemanticContext_Reset(cs->semanticContext);
        compiler_semantic_ir_reset(cs);
        if (cs->hirModule != ZR_NULL) {
            ZrParser_HirModule_Free(cs->state, cs->hirModule);
            cs->hirModule = ZR_NULL;
        }
        cs->hirModule = ZrParser_HirModule_New(cs->state, cs->semanticContext, node);
    }

    // 设置脚本级别标志（用于区分脚本级变量和函数内变量）
    cs->isScriptLevel = ZR_TRUE;
    
    // 保存脚本 AST 引用（用于类型查找）
    cs->scriptAst = node;

    // 1. 编译模块声明（如果有）
    if (script->moduleName != ZR_NULL) {
        // 处理模块声明（注册模块到全局模块表）
        // 注意：模块注册在运行时进行，编译器只需要记录模块名称
        // 模块名称可以通过entry function的常量池或元数据存储
        // 运行时加载模块时会创建模块对象并注册到全局模块注册表
        // TODO: 这里暂时不生成特殊指令，模块注册在模块加载时自动进行
    }

    // 2. 首先收集并执行所有编译期声明
    if (script->statements != ZR_NULL) {
        enter_scope(cs);

        cs->compilePhase = ZR_PARSER_COMPILE_PHASE_SIGNATURE;
        ZrParser_Compiler_PredeclareExternBindings(cs, script->statements);
        if (cs->hasError) {
            return;
        }

        ZrParser_Compiler_PredeclareFunctionBindings(cs, script->statements);
        if (cs->hasError) {
            return;
        }
        compiler_predeclare_attribute_schemas(cs, script->statements);
        if (cs->hasError) {
            return;
        }
        compiler_compile_interface_signatures(cs, script->statements);
        if (cs->hasError) {
            return;
        }
        if (!compiler_validate_task_effects(cs, node)) {
            return;
        }

        cs->compilePhase = ZR_PARSER_COMPILE_PHASE_EXPANSION;
        // 第二遍：编译运行时代码
        for (TZrSize i = 0; i < script->statements->count; i++) {
            SZrAstNode *stmt = script->statements->nodes[i];
            if (stmt != ZR_NULL) {
                zr_parser_compile_trace("compile_script stmt[%llu] type=%d",
                                        (unsigned long long)i,
                                        (int)stmt->type);
                compiler_compile_top_level_statement(cs, stmt);

                if (cs->hasCompileTimeError) {
                    cs->hasError = ZR_TRUE;
                    return;
                }

                // 即使有错误，也继续编译后续语句（除非是致命错误）
                // 这样可以尽可能多地编译成功的语句
                if (cs->hasError && !cs->hasFatalError) {
                    ZrCore_Log_Diagnosticf(cs->state,
                                           ZR_LOG_LEVEL_WARNING,
                                           ZR_OUTPUT_CHANNEL_STDERR,
                                           "    Compilation error at statement %zu, resetting error and continuing...\n",
                                           i);
                    // 清除当前语句的阻塞状态，保留总体失败标记以继续收集后续错误
                    cs->hasError = ZR_FALSE;
                } else if (cs->hasFatalError) {
                    ZrCore_Log_Diagnosticf(cs->state,
                                           ZR_LOG_LEVEL_ERROR,
                                           ZR_OUTPUT_CHANNEL_STDERR,
                                           "  Fatal error encountered, stopping compilation\n");
                    return;
                }
            }
        }
        if (cs->hadRecoverableError) {
            cs->hasError = ZR_TRUE;
            return;
        }
        cs->compilePhase = ZR_PARSER_COMPILE_PHASE_LAYOUT;
        if (!cs->hasError && !compiler_build_script_typed_metadata(cs)) {
            ZrParser_Compiler_Error(cs, "Failed to build typed metadata for compiled script", node->location);
            return;
        }
        cs->compilePhase = ZR_PARSER_COMPILE_PHASE_LATE_CHECK;
        if (!ZrParser_CompileTime_ExecuteLateChecksInCompilerState(cs, node)) {
            cs->hasError = ZR_TRUE;
            return;
        }
        if (!cs->hasError) {
            (void)ZrParser_Compiler_PublishSemanticQueryDiagnostics(cs);
        }
        if (!cs->hasError && cs->semanticContext != ZR_NULL &&
            !ZrParser_SemanticRelations_PublishReferenceDefinitions(cs->semanticContext)) {
            ZrParser_Compiler_Error(
                    cs, "Failed to publish semantic declaration definition relations", node->location);
            return;
        }
        if (!cs->hasError && cs->semanticContext != ZR_NULL &&
            !ZrParser_SemanticRelations_PublishPropertyContracts(cs->semanticContext)) {
            ZrParser_Compiler_Error(
                    cs, "Failed to publish semantic property accessor relations", node->location);
            return;
        }
        if (!cs->hasError && cs->semanticContext != ZR_NULL &&
            !ZrParser_Semantic_BuildSourceScopeFacts(cs->semanticContext, node)) {
            ZrParser_Compiler_Error(
                    cs, "Failed to publish source semantic scope facts", node->location);
            return;
        }
        if (!cs->hasError && cs->semanticContext != ZR_NULL &&
            !compiler_publish_source_constructor_relations(cs)) {
            ZrParser_Compiler_Error(
                    cs, "Failed to publish source semantic constructor relations", node->location);
            return;
        }
        if (!cs->hasError && cs->semanticContext != ZR_NULL &&
            !ZrParser_SemanticCalls_Publish(cs->semanticContext)) {
            ZrParser_Compiler_Error(
                    cs, "Failed to publish semantic call edges", node->location);
            return;
        }
        if (!cs->hasError && cs->semanticContext != ZR_NULL &&
            !ZrParser_SemanticRelations_PublishImportOrigins(cs->semanticContext)) {
            ZrParser_Compiler_Error(
                    cs, "Failed to publish semantic import origin relations", node->location);
            return;
        }

        exit_scope(cs);
    }

    // 3. 在返回前添加导出收集代码（如果有导出的变量）
    // 导出收集在运行时进行（在内部模块导入 helper 执行完 __entry 后）
    // 这里只需要确保导出信息被正确记录到函数中
    // 导出的变量信息已存储在 cs->pubVariables 和 cs->proVariables 中
    // 这些信息将在编译完成后复制到函数的 exportedVariables 字段中
    
    // 4. 如果没有显式返回，添加隐式返回
    if (!cs->hasError) {
        // 使用 instructions.length 而不是 instructionCount，确保同步
        if (cs->instructions.length == 0) {
            // 如果没有任何指令，添加隐式返回 null
            TZrUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrCore_Value_ResetAsNull(&nullValue);
            TZrUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                       (TZrInt32) constantIndex);
            emit_instruction(cs, inst);

            TZrInstruction returnInst =
                    create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            // 检查最后一条指令是否是 RETURN
            if (cs->instructions.length > 0) {
                // 使用 length 而不是 instructionCount，因为 length 是数组的实际长度
                TZrInstruction *lastInst =
                        (TZrInstruction *) ZrCore_Array_Get(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode) lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        // 添加隐式返回 null
                        TZrUInt32 resultSlot = allocate_stack_slot(cs);
                        SZrTypeValue nullValue;
                        ZrCore_Value_ResetAsNull(&nullValue);
                        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                   (TZrUInt16) resultSlot, (TZrInt32) constantIndex);
                        emit_instruction(cs, inst);

                        TZrInstruction returnInst =
                                create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            } else {
                // 如果没有任何指令，添加隐式返回 null
                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                SZrTypeValue nullValue;
                ZrCore_Value_ResetAsNull(&nullValue);
                TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                           (TZrInt32) constantIndex);
                emit_instruction(cs, inst);

                TZrInstruction returnInst =
                        create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
                emit_instruction(cs, returnInst);
            }
        }
    }
    
    // 5. 将 prototype 信息序列化为二进制数据并存储到 function->prototypeData
    // 运行时创建逻辑将在内部模块导入 helper 中实现（在创建模块后）
    // 使用紧凑二进制格式存储，不再使用常量池
    
    if (cs->typePrototypes.length > 0) {
        // 计算所有 prototype 数据的总大小
        TZrSize totalPrototypeDataSize = 0;
        TZrSize serializablePrototypeCount = 0;
        TZrByte **prototypeDataArray = (TZrByte **)ZrCore_Memory_RawMalloc(cs->state->global, cs->typePrototypes.length * sizeof(TZrByte *));
        TZrSize *prototypeDataSizes = (TZrSize *)ZrCore_Memory_RawMalloc(cs->state->global, cs->typePrototypes.length * sizeof(TZrSize));
        
        if (prototypeDataArray == ZR_NULL || prototypeDataSizes == ZR_NULL) {
            if (prototypeDataArray != ZR_NULL) {
                ZrCore_Memory_RawFree(cs->state->global, prototypeDataArray, cs->typePrototypes.length * sizeof(TZrByte *));
            }
            if (prototypeDataSizes != ZR_NULL) {
                ZrCore_Memory_RawFree(cs->state->global, prototypeDataSizes, cs->typePrototypes.length * sizeof(TZrSize));
            }
        } else {
            // 序列化每个 prototype 信息
            for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
                SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
                if (!compiler_prototype_serializes_to_function_metadata(info)) {
                    prototypeDataArray[i] = ZR_NULL;
                    prototypeDataSizes[i] = 0;
                    continue;
                }
                
                // 序列化prototype信息为二进制数据（不存储到常量池）
                TZrByte *prototypeData = ZR_NULL;
                TZrSize prototypeDataSize = 0;
                if (serialize_prototype_info_to_binary(cs, info, &prototypeData, &prototypeDataSize)) {
                    prototypeDataArray[i] = prototypeData;
                    prototypeDataSizes[i] = prototypeDataSize;
                    totalPrototypeDataSize += prototypeDataSize;
                    serializablePrototypeCount++;
                } else {
                    prototypeDataArray[i] = ZR_NULL;
                    prototypeDataSizes[i] = 0;
                }
            }
            
            // 将所有 prototype 数据合并到一个连续的缓冲区中
            if (totalPrototypeDataSize > 0) {
                // 在数据前添加一个头部：prototype 数量（TZrUInt32）
                TZrSize finalDataSize = sizeof(TZrUInt32) + totalPrototypeDataSize;
                TZrByte *finalPrototypeData = (TZrByte *)ZrCore_Memory_RawMalloc(cs->state->global, finalDataSize);
                if (finalPrototypeData != ZR_NULL) {
                    // 写入 prototype 数量
                    TZrUInt32 *prototypeCountPtr = (TZrUInt32 *)finalPrototypeData;
                    *prototypeCountPtr = (TZrUInt32)serializablePrototypeCount;
                    
                    // 复制每个 prototype 的数据
                    TZrByte *currentPos = finalPrototypeData + sizeof(TZrUInt32);
                    for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
                        if (prototypeDataArray[i] != ZR_NULL && prototypeDataSizes[i] > 0) {
                            memcpy(currentPos, prototypeDataArray[i], prototypeDataSizes[i]);
                            currentPos += prototypeDataSizes[i];
                            // 释放单个 prototype 数据
                            ZrCore_Memory_RawFree(cs->state->global, prototypeDataArray[i], prototypeDataSizes[i]);
                        }
                    }
                    
                    // 存储到 function
                    cs->currentFunction->prototypeData = finalPrototypeData;
                    cs->currentFunction->prototypeDataLength = (TZrUInt32)finalDataSize;
                    cs->currentFunction->prototypeCount = (TZrUInt32)serializablePrototypeCount;
                }
            } else {
                cs->currentFunction->prototypeData = ZR_NULL;
                cs->currentFunction->prototypeDataLength = 0;
                cs->currentFunction->prototypeCount = 0;
            }
            
            // 释放临时数组
            ZrCore_Memory_RawFree(cs->state->global, prototypeDataArray, cs->typePrototypes.length * sizeof(TZrByte *));
            ZrCore_Memory_RawFree(cs->state->global, prototypeDataSizes, cs->typePrototypes.length * sizeof(TZrSize));
        }
    } else {
        cs->currentFunction->prototypeData = ZR_NULL;
        cs->currentFunction->prototypeDataLength = 0;
        cs->currentFunction->prototypeCount = 0;
    }
    
    if (!cs->hasError) {
        TZrBool preSemanticIrIsValid =
                ZrParser_Compiler_ValidatePreSemanticIr(cs);
        if (!preSemanticIrIsValid && !cs->hasError) {
            ZrParser_Compiler_Error(
                    cs,
                    "Pre-execution Semantic IR validation failed",
                    node->location);
        }
    }

    // 重置脚本级别标志
    cs->isScriptLevel = ZR_FALSE;
}

static SZrFunction *zr_parser_compiler_compile_mode_active(
        SZrState *state,
        SZrAstNode *ast,
        SZrString *currentModuleKey,
        TZrBool emitTestManifest,
        const SZrParserSubmissionContext *submissionContext,
        SZrParserSubmissionResult *outSubmissionResult) {
    if (state == ZR_NULL || ast == ZR_NULL) {
        return ZR_NULL;
    }

    SZrCompilerState cs;
    ZrParser_CompilerState_Init(&cs, state);
    cs.emitTestManifest = emitTestManifest;
    cs.currentAst = ast;
    cs.currentModuleKey = currentModuleKey;
    cs.submissionContext = submissionContext;
    if (!compiler_submission_seed_context(&cs, submissionContext)) {
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }
    zr_parser_compile_trace("compiler compile core init ast=%p", (void *)ast);

    if (!ZrParser_CompileTime_PrepareBuildFactsInCompilerState(&cs, ast)) {
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    if (!compiler_validate_ref_struct_rules(&cs, ast)) {
        zr_parser_compile_trace("compiler validate ref struct rules failed ast=%p", (void *)ast);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }
    if (!compiler_validate_reference_escapes(&cs, ast)) {
        zr_parser_compile_trace("compiler validate reference escapes failed ast=%p", (void *)ast);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }
    if (!compiler_validate_task_effects(&cs, ast)) {
        zr_parser_compile_trace("compiler validate task effects failed ast=%p", (void *)ast);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }
    zr_parser_compile_trace("compiler validate task effects ok ast=%p", (void *)ast);

    // 创建新函数
    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }
    cs.submissionEntryFunction = submissionContext != ZR_NULL ? cs.currentFunction : ZR_NULL;
    zr_parser_compile_trace("compiler current function=%p", (void *)cs.currentFunction);

    // 编译脚本
    zr_parser_compile_trace("compile_script start ast=%p", (void *)ast);
    compile_script(&cs, ast);
    zr_parser_compile_trace("compile_script done ast=%p hasError=%d", (void *)ast, (int)cs.hasError);

    if (!cs.hasError && !compiler_test_finalize_manifest(&cs)) {
        cs.hasError = ZR_TRUE;
    }

    if (cs.hasError) {
        // 错误信息已在 ZrParser_Compiler_Error 中输出（包含行列号）
        compiler_log_failure_summary(&cs);
        if (cs.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(state, cs.currentFunction);
        }
        if (cs.topLevelFunction != ZR_NULL) {
            ZrCore_Function_Free(state, cs.topLevelFunction);
        }
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    // 如果有顶层函数声明，返回它；否则返回脚本函数
    SZrFunction *func = (cs.topLevelFunction != ZR_NULL) ? cs.topLevelFunction : cs.currentFunction;
    zr_parser_compile_trace("optimize instructions func=%p", (void *)func);
    optimize_instructions(&cs);
    if (!compiler_assemble_final_function(&cs,
                                          func,
                                          ast,
                                          func == cs.currentFunction,
                                          cs.topLevelFunction != ZR_NULL)) {
        ZrCore_Function_Free(state, func);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }
    zr_parser_compile_trace("assemble final function ok func=%p", (void *)func);

    if (func == cs.currentFunction && cs.closureVars.length > 0u) {
        TZrUInt32 typedClosureBindingCount = 0u;

        if (!compiler_build_typed_closure_bindings(
                    &cs,
                    &func->typedClosureBindings,
                    &typedClosureBindingCount)) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_NULL;
        }
        func->typedClosureBindingLength = typedClosureBindingCount;
    }

    if (!compiler_build_function_semir_metadata(state, func)) {
        ZrCore_Function_Free(state, func);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }
    zr_parser_compile_trace("build semir metadata ok func=%p", (void *)func);

    if (!compiler_quicken_execbc_function(state, func)) {
        ZrCore_Function_Free(state, func);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }
    zr_parser_compile_trace("quicken execbc ok func=%p", (void *)func);

    if (!ZrParser_ModuleInitAnalysis_FinalizeCurrentSourceModule(&cs, ZR_NULL, func)) {
        zr_parser_compile_trace("finalize current source module failed func=%p", (void *)func);
        ZrCore_Function_Free(state, func);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }
    zr_parser_compile_trace("finalize current source module ok func=%p", (void *)func);

    if (!compiler_submission_publish_result(&cs, outSubmissionResult)) {
        ZrCore_Function_Free(state, func);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    ZrParser_CompilerState_Free(&cs);
    return func;
}

static SZrFunction *zr_parser_compiler_compile_mode(
        SZrState *state,
        SZrAstNode *ast,
        SZrString *currentModuleKey,
        TZrBool emitTestManifest,
        const SZrParserSubmissionContext *submissionContext,
        SZrParserSubmissionResult *outSubmissionResult) {
    EZrLibrary_ProviderPhase previousPhase;
    SZrFunction *result;

    if (state == ZR_NULL || ast == ZR_NULL) {
        return ZR_NULL;
    }

    previousPhase = ZrLibrary_State_GetProviderPhase(state);
    ZrLibrary_State_SetProviderPhase(
            state,
            emitTestManifest
                    ? ZR_LIBRARY_PROVIDER_PHASE_TEST
                    : ZR_LIBRARY_PROVIDER_PHASE_RUNTIME);
    result = zr_parser_compiler_compile_mode_active(
            state,
            ast,
            currentModuleKey,
            emitTestManifest,
            submissionContext,
            outSubmissionResult);
    ZrLibrary_State_SetProviderPhase(state, previousPhase);
    return result;
}

ZR_PARSER_API SZrFunction *ZrParser_Compiler_CompileWithCurrentModuleKey(
        SZrState *state,
        SZrAstNode *ast,
        SZrString *currentModuleKey) {
    return zr_parser_compiler_compile_mode(
            state, ast, currentModuleKey, ZR_FALSE, ZR_NULL, ZR_NULL);
}

// 主编译入口（占位实现）
SZrFunction *ZrParser_Compiler_Compile(SZrState *state, SZrAstNode *ast) {
    return ZrParser_Compiler_CompileWithCurrentModuleKey(state, ast, ZR_NULL);
}

SZrFunction *ZrParser_Compiler_CompileTest(SZrState *state, SZrAstNode *ast) {
    return zr_parser_compiler_compile_mode(
            state, ast, ZR_NULL, ZR_TRUE, ZR_NULL, ZR_NULL);
}

ZR_PARSER_API void ZrParser_Compiler_CompileStructDeclaration(SZrCompilerState *cs, SZrAstNode *node) {
    compile_struct_declaration(cs, node);
}

ZR_PARSER_API void ZrParser_Compiler_CompileClassDeclaration(SZrCompilerState *cs, SZrAstNode *node) {
    compile_class_declaration(cs, node);
}

ZR_PARSER_API void ZrParser_Compiler_CompileInterfaceDeclaration(SZrCompilerState *cs, SZrAstNode *node) {
    compile_interface_declaration(cs, node);
}

// 编译源代码为函数（封装了从解析到编译的全流程）
static struct SZrFunction *zr_parser_source_compile_mode(
        struct SZrState *state,
        const TZrChar *source,
        TZrSize sourceLength,
        struct SZrString *sourceName,
        TZrBool emitTestManifest,
        SZrParserSourceComptimeCache *comptimeCache,
        const SZrParserSubmissionContext *submissionContext,
        SZrParserSubmissionResult *outSubmissionResult) {
    TZrChar importError[ZR_PARSER_ERROR_BUFFER_LENGTH];
    SZrFileRange importErrorLocation;
    SZrString *currentModuleKey = ZR_NULL;
    SZrParserState parserState;
    TZrBool hadParserError = ZR_FALSE;
    SZrAstNode *ast;
    SZrFunction *func;

    if (comptimeCache != ZR_NULL) {
        comptimeCache->outputSnapshot = ZR_NULL;
        comptimeCache->outputSnapshotSize = 0U;
        comptimeCache->inputSnapshotAccepted = ZR_FALSE;
        comptimeCache->hitCount = 0U;
        comptimeCache->missCount = 0U;
    }
    if (state == ZR_NULL || source == ZR_NULL || sourceLength == 0) {
        return ZR_NULL;
    }
    zr_parser_compile_trace("source compile start name='%s' len=%llu",
                            sourceName != ZR_NULL ? ZrCore_String_GetNativeString(sourceName) : "<null>",
                            (unsigned long long)sourceLength);

    ZrParser_State_Init(&parserState, state, source, sourceLength, sourceName);
    parserState.errorCallback = zr_parser_source_compile_capture_error;
    parserState.errorUserData = &hadParserError;
    ast = ZrParser_ParseWithState(&parserState);
    if (parserState.hasError || hadParserError) {
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        ZrParser_State_Free(&parserState);
        zr_parser_compile_trace("parse reported error name='%s'",
                                sourceName != ZR_NULL ? ZrCore_String_GetNativeString(sourceName) : "<null>");
        return ZR_NULL;
    }
    ZrParser_State_Free(&parserState);
    if (ast == ZR_NULL) {
        zr_parser_compile_trace("parse failed name='%s'",
                                sourceName != ZR_NULL ? ZrCore_String_GetNativeString(sourceName) : "<null>");
        return ZR_NULL;
    }
    zr_parser_compile_trace("parse ok name='%s' ast=%p",
                            sourceName != ZR_NULL ? ZrCore_String_GetNativeString(sourceName) : "<null>",
                            (void *)ast);

    if (!ZrParser_CompileTime_PrepareBuildFactsWithCache(
                state,
                ast,
                comptimeCache,
                source,
                sourceLength)) {
        ZrParser_ModuleInitAnalysis_ClearAstIdentity(state->global, ast);
        ZrParser_Ast_Free(state, ast);
        return ZR_NULL;
    }

    if (!ZrParser_ProjectImports_CanonicalizeAst(state,
                                                 ast,
                                                 sourceName,
                                                 &currentModuleKey,
                                                 importError,
                                                 sizeof(importError),
                                                 &importErrorLocation)) {
        ZrCore_Log_Diagnosticf(state,
                               ZR_LOG_LEVEL_ERROR,
                               ZR_OUTPUT_CHANNEL_STDERR,
                               "%s\n",
                               importError[0] != '\0' ? importError : "failed to canonicalize project imports");
        zr_parser_source_compile_discard_cache(state, comptimeCache);
        ZrParser_Ast_Free(state, ast);
        return ZR_NULL;
    }

    if (!ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(state, currentModuleKey, ast)) {
        zr_parser_compile_trace("prepare current source module failed name='%s'",
                                sourceName != ZR_NULL ? ZrCore_String_GetNativeString(sourceName) : "<null>");
        zr_parser_source_compile_discard_cache(state, comptimeCache);
        ZrParser_ModuleInitAnalysis_ClearAstIdentity(state->global, ast);
        ZrParser_Ast_Free(state, ast);
        return ZR_NULL;
    }
    zr_parser_compile_trace("prepare current source module ok name='%s'",
                            sourceName != ZR_NULL ? ZrCore_String_GetNativeString(sourceName) : "<null>");
    
    // 编译AST为函数
    func = zr_parser_compiler_compile_mode(
            state,
            ast,
            currentModuleKey,
            emitTestManifest,
            submissionContext,
            outSubmissionResult);
    zr_parser_compile_trace("compiler compile finished name='%s' func=%p",
                            sourceName != ZR_NULL ? ZrCore_String_GetNativeString(sourceName) : "<null>",
                            (void *)func);
    
    // 释放AST
    ZrParser_ModuleInitAnalysis_ClearAstIdentity(state->global, ast);
    ZrParser_Ast_Free(state, ast);
    zr_parser_compile_trace("source compile cleanup name='%s'",
                            sourceName != ZR_NULL ? ZrCore_String_GetNativeString(sourceName) : "<null>");

    if (func == ZR_NULL) {
        zr_parser_source_compile_discard_cache(state, comptimeCache);
    }
    
    return func;
}

struct SZrFunction *ZrParser_Source_Compile(
        struct SZrState *state,
        const TZrChar *source,
        TZrSize sourceLength,
        struct SZrString *sourceName) {
    return zr_parser_source_compile_mode(
            state, source, sourceLength, sourceName, ZR_FALSE, ZR_NULL, ZR_NULL, ZR_NULL);
}

struct SZrFunction *ZrParser_Source_CompileWithComptimeCache(
        struct SZrState *state,
        const TZrChar *source,
        TZrSize sourceLength,
        struct SZrString *sourceName,
        SZrParserSourceComptimeCache *cache) {
    return zr_parser_source_compile_mode(
            state, source, sourceLength, sourceName, ZR_FALSE, cache, ZR_NULL, ZR_NULL);
}

struct SZrFunction *ZrParser_Source_CompileTest(
        struct SZrState *state,
        const TZrChar *source,
        TZrSize sourceLength,
        struct SZrString *sourceName) {
    return zr_parser_source_compile_mode(
            state, source, sourceLength, sourceName, ZR_TRUE, ZR_NULL, ZR_NULL, ZR_NULL);
}

struct SZrFunction *ZrParser_Source_CompileSubmission(
        struct SZrState *state,
        const TZrChar *source,
        TZrSize sourceLength,
        struct SZrString *sourceName,
        const SZrParserSubmissionContext *context,
    SZrParserSubmissionResult *outResult) {
    if (outResult != ZR_NULL) {
        ZrCore_Memory_RawSet(outResult, 0, sizeof(*outResult));
    }

    return zr_parser_source_compile_mode(
            state, source, sourceLength, sourceName, ZR_FALSE, ZR_NULL, context, outResult);
}

TZrBool ZrParser_CompilerState_SeedSubmissionContext(
        SZrCompilerState *cs,
        const SZrParserSubmissionContext *context) {
    if (cs == ZR_NULL || context == ZR_NULL) {
        return ZR_FALSE;
    }

    cs->submissionContext = context;
    return compiler_submission_seed_context(cs, context);
}

// 注册 compileSource 函数到 globalState
void ZrParser_ToGlobalState_Register(struct SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }
    
    // 使用 API 设置 compileSource 函数指针，避免直接访问内部结构
    ZrCore_GlobalState_SetCompileSource(state->global, ZrParser_Source_Compile);
}

static void zr_parser_source_compile_capture_error(TZrPtr userData,
                                                   const SZrFileRange *location,
                                                   const TZrChar *message,
                                                   EZrToken token) {
    TZrBool *hadParserError = (TZrBool *)userData;

    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(message);
    ZR_UNUSED_PARAMETER(token);

    if (hadParserError != ZR_NULL) {
        *hadParserError = ZR_TRUE;
    }
}

static TZrBool zr_parser_compile_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_PROJECT_STARTUP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void zr_parser_compile_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!zr_parser_compile_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-parser-compile] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}
