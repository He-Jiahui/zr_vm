#include "compile_time_declaration_patch_transaction.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdint.h>

typedef struct SZrGeneratedCommitObserverAdapter {
    FZrParserDeclarationPatchCommitObserver observer;
    TZrPtr userData;
} SZrGeneratedCommitObserverAdapter;

static TZrBool patch_transaction_clone_array(
        SZrCompilerState *cs,
        const SZrArray *source,
        TZrSize additionalCount,
        SZrArray *detached) {
    TZrSize requiredCapacity;
    TZrSize byteCapacity;

    if (cs == ZR_NULL || cs->state == ZR_NULL || source == ZR_NULL ||
        detached == ZR_NULL || !source->isValid || source->head == ZR_NULL ||
        source->elementSize == 0U || source->length > source->capacity ||
        additionalCount > SIZE_MAX - source->length) {
        return ZR_FALSE;
    }
    ZrCore_Array_Construct(detached);
    requiredCapacity = source->length + additionalCount;
    if (requiredCapacity == 0U) {
        requiredCapacity = 1U;
    }
    if (requiredCapacity > SIZE_MAX / source->elementSize) {
        return ZR_FALSE;
    }
    byteCapacity = requiredCapacity * source->elementSize;
    detached->head = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            byteCapacity,
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (detached->head == ZR_NULL) {
        ZrCore_Array_Construct(detached);
        return ZR_FALSE;
    }
    if (source->length > 0U) {
        ZrCore_Memory_RawCopy(
                detached->head,
                source->head,
                source->length * source->elementSize);
    }
    detached->elementSize = source->elementSize;
    detached->length = source->length;
    detached->capacity = requiredCapacity;
    detached->isValid = ZR_TRUE;
    return ZR_TRUE;
}

static void patch_transaction_publish_array(
        SZrState *state,
        SZrArray *target,
        SZrArray *detached) {
    SZrArray previous = *target;

    *target = *detached;
    ZrCore_Array_Construct(detached);
    ZrCore_Array_Free(state, &previous);
}

static TZrBool patch_transaction_set_object_field(
        SZrCompilerState *cs,
        SZrObject *object,
        const TZrChar *name,
        const SZrTypeValue *value) {
    ZrExternCompilerTempRoot keyRoot = {0};
    SZrString *keyString;
    SZrTypeValue key;
    TZrBool result = ZR_FALSE;

    if (cs == ZR_NULL || object == ZR_NULL || name == ZR_NULL ||
        value == ZR_NULL ||
        !extern_compiler_temp_root_begin(cs, &keyRoot)) {
        return ZR_FALSE;
    }
    keyString = ZrCore_String_CreateFromNative(
            cs->state, (TZrNativeString)name);
    if (keyString == ZR_NULL) {
        goto cleanup;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    if (!extern_compiler_temp_root_set_value(&keyRoot, &key)) {
        goto cleanup;
    }
    ZrCore_Object_SetValue(cs->state, object, &key, value);
    result = ZrCore_Object_GetValue(cs->state, object, &key) != ZR_NULL;

cleanup:
    extern_compiler_temp_root_end(&keyRoot);
    return result;
}

static TZrBool patch_transaction_push_root_value(
        SZrCompilerState *cs,
        SZrObject *roots,
        const SZrTypeValue *value) {
    const TZrSize index = roots != ZR_NULL
                                  ? roots->nodeMap.elementCount
                                  : 0U;
    SZrTypeValue key;

    if (cs == ZR_NULL || roots == ZR_NULL || value == ZR_NULL ||
        index > (TZrSize)INT64_MAX || index == SIZE_MAX ||
        !extern_compiler_push_array_value(cs, roots, value)) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsInt(cs->state, &key, (TZrInt64)index);
    return roots->nodeMap.elementCount == index + 1U &&
           ZrCore_Object_GetValue(cs->state, roots, &key) != ZR_NULL;
}

static TZrBool patch_transaction_hold_string(
        SZrCompilerState *cs,
        SZrObject *roots,
        SZrString *string) {
    ZrExternCompilerTempRoot stringRoot = {0};
    SZrTypeValue value;
    TZrBool result = ZR_FALSE;

    if (cs == ZR_NULL || roots == ZR_NULL || string == ZR_NULL ||
        !extern_compiler_temp_root_begin(cs, &stringRoot)) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(string));
    value.type = ZR_VALUE_TYPE_STRING;
    if (extern_compiler_temp_root_set_value(&stringRoot, &value)) {
        result = patch_transaction_push_root_value(cs, roots, &value);
    }
    extern_compiler_temp_root_end(&stringRoot);
    return result;
}

static TZrBool patch_transaction_create_rooted_string(
        SZrCompilerState *cs,
        SZrObject *roots,
        const TZrChar *text,
        SZrString **result) {
    ZrExternCompilerTempRoot candidateRoot = {0};
    SZrString *string = ZR_NULL;
    SZrTypeValue value;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || roots == ZR_NULL || text == ZR_NULL ||
        result == ZR_NULL ||
        !extern_compiler_temp_root_begin(cs, &candidateRoot)) {
        return ZR_FALSE;
    }
    string = ZrCore_String_CreateFromNative(
            cs->state, (TZrNativeString)text);
    if (string == ZR_NULL) {
        goto cleanup;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(string));
    value.type = ZR_VALUE_TYPE_STRING;
    if (!extern_compiler_temp_root_set_value(&candidateRoot, &value) ||
        !patch_transaction_push_root_value(cs, roots, &value)) {
        goto cleanup;
    }
    *result = string;
    success = ZR_TRUE;

cleanup:
    extern_compiler_temp_root_end(&candidateRoot);
    return success;
}

static TZrUInt32 patch_transaction_generated_field_size(
        SZrCompilerState *cs,
        TZrTypeId typeId) {
    const SZrCanonicalTypeNode *type =
            ZrParser_CanonicalType_Find(cs->semanticContext, typeId);

    if (type == ZR_NULL || type->kind != ZR_CANONICAL_TYPE_PRIMITIVE) {
        return sizeof(SZrTypeValue);
    }
    switch (type->data.primitive.valueType) {
        case ZR_VALUE_TYPE_INT8: return sizeof(TZrInt8);
        case ZR_VALUE_TYPE_INT16: return sizeof(TZrInt16);
        case ZR_VALUE_TYPE_INT32: return sizeof(TZrInt32);
        case ZR_VALUE_TYPE_INT64: return sizeof(TZrInt64);
        case ZR_VALUE_TYPE_UINT8: return sizeof(TZrUInt8);
        case ZR_VALUE_TYPE_UINT16: return sizeof(TZrUInt16);
        case ZR_VALUE_TYPE_UINT32: return sizeof(TZrUInt32);
        case ZR_VALUE_TYPE_UINT64: return sizeof(TZrUInt64);
        case ZR_VALUE_TYPE_FLOAT: return sizeof(TZrFloat32);
        case ZR_VALUE_TYPE_DOUBLE: return sizeof(TZrDouble);
        case ZR_VALUE_TYPE_BOOL: return sizeof(TZrBool);
        default: return sizeof(SZrTypeValue);
    }
}

static TZrBool patch_transaction_prepare_generated_field(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *targetInfo,
        const SZrParserGeneratedDeclaration *addition,
        SZrString *canonicalTypeName,
        TZrSize declarationOrder,
        TZrSymbolId originTargetSymbolId,
        SZrFileRange location,
        SZrObject *roots,
        SZrTypeMemberInfo *member) {
    ZrExternCompilerTempRoot metadataRoot = {0};
    SZrObject *metadataObject;
    SZrTypeValue metadataValue;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || targetInfo == ZR_NULL || addition == ZR_NULL ||
        canonicalTypeName == ZR_NULL || roots == ZR_NULL || member == ZR_NULL ||
        addition->kind != ZR_PARSER_GENERATED_DECLARATION_FIELD ||
        addition->name == ZR_NULL ||
        !patch_transaction_hold_string(cs, roots, canonicalTypeName)) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(member, 0, sizeof(*member));
    member->memberType = targetInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT
                                 ? ZR_AST_STRUCT_FIELD
                                 : ZR_AST_CLASS_FIELD;
    if (!patch_transaction_create_rooted_string(
                cs, roots, addition->name, &member->name)) {
        return ZR_FALSE;
    }
    member->accessModifier =
            addition->visibility == ZR_PARSER_GENERATED_VISIBILITY_PUBLIC
                    ? ZR_ACCESS_PUBLIC
                    : addition->visibility ==
                                      ZR_PARSER_GENERATED_VISIBILITY_PROTECTED
                              ? ZR_ACCESS_PROTECTED
                              : ZR_ACCESS_PRIVATE;
    member->isConst = addition->mutability ==
                      ZR_PARSER_GENERATED_MUTABILITY_LET;
    member->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    member->gcBridgeKind = ZR_GC_BRIDGE_NONE;
    member->receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    member->receiverEffect = ZR_CANONICAL_RECEIVER_NONE;
    member->declarationOrder = (TZrUInt32)declarationOrder;
    member->fieldTypeName = canonicalTypeName;
    member->fieldSize =
            patch_transaction_generated_field_size(cs, addition->typeId);
    member->virtualSlotIndex = UINT32_MAX;
    member->interfaceContractSlot = UINT32_MAX;
    member->propertyIdentity = UINT32_MAX;
    member->propertySymbolId = ZR_SEMANTIC_ID_INVALID;
    member->getterAccessorSymbolId = ZR_SEMANTIC_ID_INVALID;
    member->setterAccessorSymbolId = ZR_SEMANTIC_ID_INVALID;
    member->initAccessorSymbolId = ZR_SEMANTIC_ID_INVALID;
    ZrCore_Array_Construct(&member->parameterTypes);
    ZrCore_Array_Construct(&member->parameterNames);
    ZrCore_Array_Construct(&member->parameterHasDefaultValues);
    ZrCore_Array_Construct(&member->parameterDefaultValues);
    ZrCore_Array_Construct(&member->genericParameters);
    ZrCore_Array_Construct(&member->parameterPassingModes);
    ZrCore_Array_Construct(&member->decorators);
    ZrCore_Value_ResetAsNull(&member->decoratorMetadataValue);

    if (!extern_compiler_temp_root_begin(cs, &metadataRoot)) {
        return ZR_FALSE;
    }
    metadataObject = extern_compiler_new_object_constant(cs);
    if (metadataObject == ZR_NULL ||
        !extern_compiler_temp_root_set_object(
                &metadataRoot, metadataObject, ZR_VALUE_TYPE_OBJECT)) {
        goto cleanup;
    }
    ZrCore_Value_InitAsInt(cs->state, &metadataValue, 1);
    if (!patch_transaction_set_object_field(
                cs, metadataObject, "generated", &metadataValue)) {
        goto cleanup;
    }
    ZrCore_Value_InitAsUInt(
            cs->state, &metadataValue, originTargetSymbolId);
    if (!patch_transaction_set_object_field(
                cs,
                metadataObject,
                "originTargetSymbolId",
                &metadataValue)) {
        goto cleanup;
    }
    ZrCore_Value_InitAsInt(
            cs->state, &metadataValue, location.start.line);
    if (!patch_transaction_set_object_field(
                cs, metadataObject, "sourceLineStart", &metadataValue)) {
        goto cleanup;
    }
    ZrCore_Value_InitAsInt(
            cs->state, &metadataValue, location.end.line);
    if (!patch_transaction_set_object_field(
                cs, metadataObject, "sourceLineEnd", &metadataValue)) {
        goto cleanup;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state,
            &member->decoratorMetadataValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(metadataObject));
    member->decoratorMetadataValue.type = ZR_VALUE_TYPE_OBJECT;
    member->hasDecoratorMetadata = ZR_TRUE;
    if (!patch_transaction_push_root_value(
                cs, roots, &member->decoratorMetadataValue)) {
        goto cleanup;
    }
    success = ZR_TRUE;

cleanup:
    extern_compiler_temp_root_end(&metadataRoot);
    return success;
}

static TZrBool patch_transaction_generated_observer_adapter(
        EZrParserDeclarationPatchCommitStage stage,
        TZrSize committedCount,
        TZrPtr userData) {
    SZrGeneratedCommitObserverAdapter *adapter =
            (SZrGeneratedCommitObserverAdapter *)userData;

    if (stage != ZR_PARSER_DECLARATION_PATCH_COMMIT_GENERATED ||
        adapter == ZR_NULL || adapter->observer == ZR_NULL) {
        return ZR_TRUE;
    }
    return adapter->observer(committedCount, adapter->userData);
}

TZrBool ZrParser_CompileTime_CommitGeneratedFieldsAtomic(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *targetInfo,
        const SZrParserGeneratedDeclaration *additions,
        SZrString *const *canonicalTypeNames,
        TZrSize additionCount,
        TZrSymbolId originTargetSymbolId,
        SZrFileRange location,
        FZrParserDeclarationPatchCommitObserver observer,
        TZrPtr observerUserData) {
    SZrParserCompileTimePatchInterfaceAdds interfaceAdds = {0};
    SZrParserCompileTimePatchAttributeAdds attributeAdds = {0};
    SZrGeneratedCommitObserverAdapter adapter;

    adapter.observer = observer;
    adapter.userData = observerUserData;
    return ZrParser_CompileTime_CommitDeclarationPatchAtomic(
            cs,
            targetInfo,
            additions,
            canonicalTypeNames,
            additionCount,
            &interfaceAdds,
            &attributeAdds,
            ZR_NULL,
            originTargetSymbolId,
            location,
            observer != ZR_NULL
                    ? patch_transaction_generated_observer_adapter
                    : ZR_NULL,
            &adapter);
}

TZrBool ZrParser_CompileTime_CommitDeclarationPatchAtomic(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *targetInfo,
        const SZrParserGeneratedDeclaration *additions,
        SZrString *const *canonicalTypeNames,
        TZrSize additionCount,
        const SZrParserCompileTimePatchInterfaceAdds *interfaceAdds,
        const SZrParserCompileTimePatchAttributeAdds *attributeAdds,
        SZrString *transformDecoratorName,
        TZrSymbolId originTargetSymbolId,
        SZrFileRange location,
        FZrParserDeclarationPatchTransactionObserver observer,
        TZrPtr observerUserData) {
    const TZrBool hasGenerated = additionCount > 0U;
    const TZrBool hasInterfaces =
            interfaceAdds != ZR_NULL && interfaceAdds->count > 0U;
    const TZrBool hasAttributes =
            attributeAdds != ZR_NULL && attributeAdds->count > 0U;
    const TZrBool hasDecorators =
            hasAttributes || transformDecoratorName != ZR_NULL;
    const TZrSize decoratorAdditionCount =
            (hasAttributes ? attributeAdds->count : 0U) +
            (transformDecoratorName != ZR_NULL ? 1U : 0U);
    SZrArray detachedMembers;
    SZrArray detachedSymbols;
    SZrArray detachedInherits;
    SZrArray detachedImplements;
    SZrArray detachedDecorators;
    SZrSemanticContext detachedContext = {0};
    ZrExternCompilerTempRoot rootsRoot = {0};
    ZrExternCompilerTempRoot metadataRoot = {0};
    SZrObject *roots = ZR_NULL;
    SZrTypeValue preparedDecoratorMetadata;
    TZrBool result = ZR_FALSE;

    ZrCore_Array_Construct(&detachedMembers);
    ZrCore_Array_Construct(&detachedSymbols);
    ZrCore_Array_Construct(&detachedInherits);
    ZrCore_Array_Construct(&detachedImplements);
    ZrCore_Array_Construct(&detachedDecorators);
    ZrCore_Value_ResetAsNull(&preparedDecoratorMetadata);
    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        cs->semanticContext == ZR_NULL || targetInfo == ZR_NULL ||
        interfaceAdds == ZR_NULL || attributeAdds == ZR_NULL ||
        (hasGenerated &&
         (additions == ZR_NULL || canonicalTypeNames == ZR_NULL)) ||
        (hasInterfaces && interfaceAdds->typeNames == ZR_NULL) ||
        (hasAttributes && attributeAdds->entries == ZR_NULL) ||
        additionCount > ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS ||
        interfaceAdds->count > ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS ||
        attributeAdds->count > ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS ||
        (hasAttributes && attributeAdds->count == SIZE_MAX) ||
        targetInfo->members.length > UINT32_MAX ||
        additionCount > (TZrSize)UINT32_MAX - targetInfo->members.length ||
        additionCount > UINT32_MAX - cs->semanticContext->nextSymbolId) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < interfaceAdds->count; index++) {
        if (interfaceAdds->typeNames[index] == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    for (TZrSize index = 0; index < attributeAdds->count; index++) {
        if (attributeAdds->entries[index].schema == ZR_NULL ||
            attributeAdds->entries[index].schema->name == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    if (!hasGenerated && !hasInterfaces && !hasDecorators) {
        return ZR_TRUE;
    }

    if ((hasGenerated &&
         (!patch_transaction_clone_array(
                  cs, &targetInfo->members, additionCount, &detachedMembers) ||
          !patch_transaction_clone_array(
                  cs,
                  &cs->semanticContext->symbols,
                  additionCount,
                  &detachedSymbols))) ||
        (hasInterfaces &&
         (!patch_transaction_clone_array(
                  cs,
                  &targetInfo->inherits,
                  interfaceAdds->count,
                  &detachedInherits) ||
          !patch_transaction_clone_array(
                  cs,
                  &targetInfo->implements,
                  interfaceAdds->count,
                  &detachedImplements))) ||
        (hasDecorators &&
         !patch_transaction_clone_array(
                 cs,
                 &targetInfo->decorators,
                 decoratorAdditionCount,
                 &detachedDecorators))) {
        goto cleanup;
    }
    if (!extern_compiler_temp_root_begin(cs, &rootsRoot)) {
        goto cleanup;
    }
    roots = extern_compiler_new_array_constant(cs);
    if (roots == ZR_NULL ||
        !extern_compiler_temp_root_set_object(
                &rootsRoot, roots, ZR_VALUE_TYPE_ARRAY)) {
        goto cleanup;
    }
    if (transformDecoratorName != ZR_NULL &&
        !patch_transaction_hold_string(cs, roots, transformDecoratorName)) {
        goto cleanup;
    }
    for (TZrSize index = 0; index < interfaceAdds->count; index++) {
        if (!patch_transaction_hold_string(
                    cs, roots, interfaceAdds->typeNames[index])) {
            goto cleanup;
        }
    }
    for (TZrSize index = 0; index < attributeAdds->count; index++) {
        if (!patch_transaction_hold_string(
                    cs, roots, attributeAdds->entries[index].schema->name)) {
            goto cleanup;
        }
    }

    if (hasAttributes) {
        if (!extern_compiler_temp_root_begin(cs, &metadataRoot) ||
            !ZrParser_CompileTime_BuildPatchAttributeMetadata(
                    cs,
                    targetInfo,
                    attributeAdds,
                    &preparedDecoratorMetadata) ||
            !extern_compiler_temp_root_set_value(
                    &metadataRoot, &preparedDecoratorMetadata)) {
            goto cleanup;
        }
    }
    if (hasGenerated) {
        detachedContext = *cs->semanticContext;
        detachedContext.symbols = detachedSymbols;
        for (TZrSize index = 0; index < additionCount; index++) {
            SZrTypeMemberInfo member;
            TZrSymbolId symbolId;

            if (!patch_transaction_prepare_generated_field(
                        cs,
                        targetInfo,
                        &additions[index],
                        canonicalTypeNames[index],
                        targetInfo->members.length + index,
                        originTargetSymbolId,
                        location,
                        roots,
                        &member)) {
                goto cleanup;
            }
            symbolId = ZrParser_Semantic_ReserveSymbolId(&detachedContext);
            if (symbolId == ZR_SEMANTIC_ID_INVALID ||
                ZrParser_Semantic_RegisterSymbolWithId(
                        &detachedContext,
                        symbolId,
                        member.name,
                        ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                        additions[index].typeId,
                        ZR_SEMANTIC_ID_INVALID,
                        ZR_NULL,
                        location) == ZR_SEMANTIC_ID_INVALID) {
                goto cleanup;
            }
            member.symbolId = symbolId;
            ZrCore_Array_Push(cs->state, &detachedMembers, &member);
        }
        detachedSymbols = detachedContext.symbols;
        if (observer != ZR_NULL &&
            !observer(
                    ZR_PARSER_DECLARATION_PATCH_COMMIT_GENERATED,
                    additionCount,
                    observerUserData)) {
            goto cleanup;
        }
    }
    if (hasInterfaces) {
        for (TZrSize index = 0; index < interfaceAdds->count; index++) {
            SZrString *name = interfaceAdds->typeNames[index];

            ZrCore_Array_Push(cs->state, &detachedInherits, &name);
            ZrCore_Array_Push(cs->state, &detachedImplements, &name);
        }
        if (observer != ZR_NULL &&
            !observer(
                    ZR_PARSER_DECLARATION_PATCH_COMMIT_INTERFACES,
                    interfaceAdds->count,
                    observerUserData)) {
            goto cleanup;
        }
    }
    if (hasAttributes) {
        for (TZrSize index = 0; index < attributeAdds->count; index++) {
            SZrTypeDecoratorInfo decoratorInfo;

            decoratorInfo.name = attributeAdds->entries[index].schema->name;
            ZrCore_Array_Push(
                    cs->state, &detachedDecorators, &decoratorInfo);
        }
        if (observer != ZR_NULL &&
            !observer(
                    ZR_PARSER_DECLARATION_PATCH_COMMIT_ATTRIBUTES,
                    attributeAdds->count,
                    observerUserData)) {
            goto cleanup;
        }
    }
    if (transformDecoratorName != ZR_NULL) {
        SZrTypeDecoratorInfo decoratorInfo;

        decoratorInfo.name = transformDecoratorName;
        ZrCore_Array_Push(cs->state, &detachedDecorators, &decoratorInfo);
    }

    if (hasGenerated) {
        patch_transaction_publish_array(
                cs->state, &targetInfo->members, &detachedMembers);
        patch_transaction_publish_array(
                cs->state,
                &cs->semanticContext->symbols,
                &detachedSymbols);
        cs->semanticContext->nextSymbolId = detachedContext.nextSymbolId;
    }
    if (hasInterfaces) {
        patch_transaction_publish_array(
                cs->state, &targetInfo->inherits, &detachedInherits);
        patch_transaction_publish_array(
                cs->state, &targetInfo->implements, &detachedImplements);
    }
    if (hasDecorators) {
        patch_transaction_publish_array(
                cs->state, &targetInfo->decorators, &detachedDecorators);
    }
    if (hasAttributes) {
        targetInfo->decoratorMetadataValue = preparedDecoratorMetadata;
        targetInfo->hasDecoratorMetadata = ZR_TRUE;
    }
    result = ZR_TRUE;

cleanup:
    extern_compiler_temp_root_end(&metadataRoot);
    extern_compiler_temp_root_end(&rootsRoot);
    ZrCore_Array_Free(cs->state, &detachedDecorators);
    ZrCore_Array_Free(cs->state, &detachedImplements);
    ZrCore_Array_Free(cs->state, &detachedInherits);
    ZrCore_Array_Free(cs->state, &detachedSymbols);
    ZrCore_Array_Free(cs->state, &detachedMembers);
    return result;
}
