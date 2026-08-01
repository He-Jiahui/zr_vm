#ifndef ZR_VM_TEST_COMPILE_TIME_DECLARATION_PATCH_TRANSACTION_CASES_H
#define ZR_VM_TEST_COMPILE_TIME_DECLARATION_PATCH_TRANSACTION_CASES_H

#include "zr_vm_core/gc_domain.h"

static TZrBool fail_declaration_patch_after_first_commit(
        TZrSize committedAdditionCount,
        TZrPtr userData) {
    ZR_UNUSED_PARAMETER(userData)
    return committedAdditionCount < 1U;
}

typedef struct SDeclarationPatchAllocationFailure {
    FZrAllocator allocator;
    TZrPtr allocatorUserData;
    TZrBool rejectedArrayGrowth;
} SDeclarationPatchAllocationFailure;

static TZrPtr fail_declaration_patch_array_growth_allocator(
        TZrPtr userData,
        TZrPtr pointer,
        TZrSize originalSize,
        TZrSize newSize,
        TZrInt64 flag) {
    SDeclarationPatchAllocationFailure *failure =
            (SDeclarationPatchAllocationFailure *)userData;

    if (failure != ZR_NULL && !failure->rejectedArrayGrowth &&
        newSize > originalSize &&
        flag == ZR_MEMORY_NATIVE_TYPE_ARRAY) {
        failure->rejectedArrayGrowth = ZR_TRUE;
        return ZR_NULL;
    }
    return failure->allocator(
            failure->allocatorUserData,
            pointer,
            originalSize,
            newSize,
            flag);
}

static void test_declaration_transform_generated_multi_add_failure_rolls_back(void) {
    SZrState *state = create_test_state();
    SZrCompilerState cs;
    SZrTypePrototypeInfo targetInfo;
    SZrTypePrototypeInfo allocationFailureTarget;
    SZrParserGeneratedDeclaration additions[2];
    SZrString *canonicalTypeNames[2];
    SZrString *firstName;
    SZrString *secondName;
    SZrFileRange location;
    TZrSize memberCountBefore;
    TZrSize symbolCountBefore;
    TZrSymbolId nextSymbolIdBefore;
    SDeclarationPatchAllocationFailure allocationFailure;
    TZrBool allocationCommitResult;

    TEST_ASSERT_NOT_NULL(state);
    ZrParser_CompilerState_Init(&cs, state);
    cs.suppressErrorOutput = ZR_TRUE;
    ZrCore_Memory_RawSet(&targetInfo, 0, sizeof(targetInfo));
    targetInfo.name = ZrCore_String_CreateFromNative(state, "AtomicTarget");
    targetInfo.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    ZrCore_Array_Init(
            state,
            &targetInfo.members,
            sizeof(SZrTypeMemberInfo),
            1U);
    TEST_ASSERT_NOT_NULL(targetInfo.name);
    firstName = ZrCore_String_CreateFromNative(state, "firstGenerated");
    secondName = ZrCore_String_CreateFromNative(state, "secondGenerated");
    canonicalTypeNames[0] = ZrCore_String_CreateFromNative(state, "bool");
    canonicalTypeNames[1] = ZrCore_String_CreateFromNative(state, "int");
    TEST_ASSERT_NOT_NULL(firstName);
    TEST_ASSERT_NOT_NULL(secondName);
    TEST_ASSERT_NOT_NULL(canonicalTypeNames[0]);
    TEST_ASSERT_NOT_NULL(canonicalTypeNames[1]);
    ZrCore_Memory_RawSet(additions, 0, sizeof(additions));
    additions[0].kind = ZR_PARSER_GENERATED_DECLARATION_FIELD;
    additions[0].name = "firstGenerated";
    additions[0].typeId = 1U;
    additions[0].visibility = ZR_PARSER_GENERATED_VISIBILITY_PUBLIC;
    additions[0].mutability = ZR_PARSER_GENERATED_MUTABILITY_LET;
    additions[1].kind = ZR_PARSER_GENERATED_DECLARATION_FIELD;
    additions[1].name = "secondGenerated";
    additions[1].typeId = 2U;
    additions[1].visibility = ZR_PARSER_GENERATED_VISIBILITY_PRIVATE;
    additions[1].mutability = ZR_PARSER_GENERATED_MUTABILITY_VAR;
    ZrCore_Memory_RawSet(&location, 0, sizeof(location));
    location.start.line = 7U;
    location.end.line = 9U;
    memberCountBefore = targetInfo.members.length;
    symbolCountBefore = cs.semanticContext->symbols.length;
    nextSymbolIdBefore = cs.semanticContext->nextSymbolId;

    TEST_ASSERT_FALSE(ZrParser_CompileTime_CommitGeneratedFieldsAtomic(
            &cs,
            &targetInfo,
            additions,
            canonicalTypeNames,
            ZR_ARRAY_COUNT(additions),
            41U,
            location,
            fail_declaration_patch_after_first_commit,
            ZR_NULL));
    TEST_ASSERT_EQUAL_UINT64(memberCountBefore, targetInfo.members.length);
    TEST_ASSERT_EQUAL_UINT64(
            symbolCountBefore, cs.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT32(
            nextSymbolIdBefore, cs.semanticContext->nextSymbolId);
    TEST_ASSERT_NULL(ZrParser_Semantic_FindSymbolByNameAndKind(
            cs.semanticContext,
            firstName,
            ZR_SEMANTIC_SYMBOL_KIND_FIELD));
    TEST_ASSERT_NULL(ZrParser_Semantic_FindSymbolByNameAndKind(
            cs.semanticContext,
            secondName,
            ZR_SEMANTIC_SYMBOL_KIND_FIELD));

    ZrCore_Memory_RawSet(
            &allocationFailureTarget, 0, sizeof(allocationFailureTarget));
    allocationFailureTarget.name = ZrCore_String_CreateFromNative(
            state, "AllocationFailureTarget");
    allocationFailureTarget.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    ZrCore_Array_Init(
            state,
            &allocationFailureTarget.members,
            sizeof(SZrTypeMemberInfo),
            1U);
    TEST_ASSERT_NOT_NULL(allocationFailureTarget.name);
    ZrCore_Memory_RawSet(
            &allocationFailure, 0, sizeof(allocationFailure));
    allocationFailure.allocator = state->global->allocator;
    allocationFailure.allocatorUserData =
            state->global->userAllocationArguments;
    state->global->allocator =
            fail_declaration_patch_array_growth_allocator;
    state->global->userAllocationArguments = &allocationFailure;
    allocationCommitResult =
            ZrParser_CompileTime_CommitGeneratedFieldsAtomic(
                    &cs,
                    &allocationFailureTarget,
                    additions,
                    canonicalTypeNames,
                    ZR_ARRAY_COUNT(additions),
                    43U,
                    location,
                    ZR_NULL,
                    ZR_NULL);
    state->global->allocator = allocationFailure.allocator;
    state->global->userAllocationArguments =
            allocationFailure.allocatorUserData;
    TEST_ASSERT_FALSE(allocationCommitResult);
    TEST_ASSERT_TRUE(allocationFailure.rejectedArrayGrowth);
    TEST_ASSERT_EQUAL_UINT64(
            0U, allocationFailureTarget.members.length);
    TEST_ASSERT_EQUAL_UINT64(
            symbolCountBefore, cs.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT32(
            nextSymbolIdBefore, cs.semanticContext->nextSymbolId);

    TEST_ASSERT_TRUE(ZrParser_CompileTime_CommitGeneratedFieldsAtomic(
            &cs,
            &targetInfo,
            additions,
            canonicalTypeNames,
            ZR_ARRAY_COUNT(additions),
            47U,
            location,
            ZR_NULL,
            ZR_NULL));
    TEST_ASSERT_EQUAL_UINT64(
            memberCountBefore + ZR_ARRAY_COUNT(additions),
            targetInfo.members.length);
    TEST_ASSERT_EQUAL_UINT64(
            symbolCountBefore + ZR_ARRAY_COUNT(additions),
            cs.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT32(
            nextSymbolIdBefore + ZR_ARRAY_COUNT(additions),
            cs.semanticContext->nextSymbolId);
    TEST_ASSERT_NOT_NULL(ZrParser_Semantic_FindSymbolByNameAndKind(
            cs.semanticContext,
            firstName,
            ZR_SEMANTIC_SYMBOL_KIND_FIELD));
    TEST_ASSERT_NOT_NULL(ZrParser_Semantic_FindSymbolByNameAndKind(
            cs.semanticContext,
            secondName,
            ZR_SEMANTIC_SYMBOL_KIND_FIELD));

    ZrCore_Array_Free(state, &allocationFailureTarget.members);
    ZrCore_Array_Free(state, &targetInfo.members);
    ZrParser_CompilerState_Free(&cs);
    destroy_test_state(state);
}

static TZrBool fail_declaration_patch_after_attribute_commit(
        EZrParserDeclarationPatchCommitStage stage,
        TZrSize committedCount,
        TZrPtr userData) {
    ZR_UNUSED_PARAMETER(committedCount)
    ZR_UNUSED_PARAMETER(userData)
    return stage < ZR_PARSER_DECLARATION_PATCH_COMMIT_ATTRIBUTES;
}

static TZrBool collect_declaration_patch_after_attribute_stage(
        EZrParserDeclarationPatchCommitStage stage,
        TZrSize committedCount,
        TZrPtr userData) {
    ZR_UNUSED_PARAMETER(committedCount)
    if (stage == ZR_PARSER_DECLARATION_PATCH_COMMIT_ATTRIBUTES &&
        userData != ZR_NULL) {
        ZrCore_GarbageCollector_GcFull((SZrState *)userData, ZR_TRUE);
    }
    return ZR_TRUE;
}

static void test_declaration_transform_cross_kind_failure_rolls_back(void) {
    SZrState *state = create_test_state();
    SZrCompilerState cs;
    SZrTypePrototypeInfo targetInfo;
    SZrParserGeneratedDeclaration addition;
    SZrString *canonicalTypeName;
    SZrString *interfaceName;
    SZrString *transformDecoratorName;
    SZrParserCompileTimePatchInterfaceAdds interfaceAdds;
    SZrCompilerAttributeSchemaBinding schema;
    SZrParserCompileTimePatchAttributeAdd attributeEntry;
    SZrParserCompileTimePatchAttributeAdds attributeAdds;
    SZrObject *existingMetadata;
    SZrString *existingMetadataKeyString;
    SZrString *generatedAttributeKeyString;
    SZrTypeValue existingMetadataKey;
    SZrTypeValue generatedAttributeKey;
    SZrTypeValue existingMetadataField;
    SZrGcRootHandle existingMetadataRoot;
    SZrGcRootHandle generatedAttributeKeyRoot;
    SZrFileRange location;
    TZrSize symbolCountBefore;
    TZrSymbolId nextSymbolIdBefore;

    TEST_ASSERT_NOT_NULL(state);
    ZrParser_CompilerState_Init(&cs, state);
    cs.suppressErrorOutput = ZR_TRUE;
    ZrCore_Memory_RawSet(&targetInfo, 0, sizeof(targetInfo));
    targetInfo.name = ZrCore_String_CreateFromNative(
            state, "CrossKindAtomicTarget");
    targetInfo.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    ZrCore_Array_Init(
            state, &targetInfo.members, sizeof(SZrTypeMemberInfo), 1U);
    ZrCore_Array_Init(
            state, &targetInfo.inherits, sizeof(SZrString *), 1U);
    ZrCore_Array_Init(
            state, &targetInfo.implements, sizeof(SZrString *), 1U);
    ZrCore_Array_Init(
            state,
            &targetInfo.decorators,
            sizeof(SZrTypeDecoratorInfo),
            1U);
    TEST_ASSERT_NOT_NULL(targetInfo.name);
    existingMetadata = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(existingMetadata);
    ZrCore_Object_Init(state, existingMetadata);
    existingMetadataKeyString = ZrCore_String_CreateFromNative(
            state, "existingMetadata");
    generatedAttributeKeyString = ZrCore_String_CreateFromNative(
            state, "attribute:00000047:0");
    TEST_ASSERT_NOT_NULL(existingMetadataKeyString);
    TEST_ASSERT_NOT_NULL(generatedAttributeKeyString);
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(existingMetadata),
            &existingMetadataRoot));
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Create(
            state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(generatedAttributeKeyString),
            &generatedAttributeKeyRoot));
    ZrCore_Value_InitAsRawObject(
            state,
            &existingMetadataKey,
            ZR_CAST_RAW_OBJECT_AS_SUPER(existingMetadataKeyString));
    existingMetadataKey.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(
            state,
            &generatedAttributeKey,
            ZR_CAST_RAW_OBJECT_AS_SUPER(generatedAttributeKeyString));
    generatedAttributeKey.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsInt(state, &existingMetadataField, 17);
    ZrCore_Object_SetValue(
            state,
            existingMetadata,
            &existingMetadataKey,
            &existingMetadataField);
    ZrCore_Value_InitAsRawObject(
            state,
            &targetInfo.decoratorMetadataValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(existingMetadata));
    targetInfo.decoratorMetadataValue.type = ZR_VALUE_TYPE_OBJECT;
    targetInfo.hasDecoratorMetadata = ZR_TRUE;

    ZrCore_Memory_RawSet(&addition, 0, sizeof(addition));
    addition.kind = ZR_PARSER_GENERATED_DECLARATION_FIELD;
    addition.name = "generated";
    addition.typeId = 1U;
    addition.visibility = ZR_PARSER_GENERATED_VISIBILITY_PUBLIC;
    addition.mutability = ZR_PARSER_GENERATED_MUTABILITY_LET;
    canonicalTypeName = ZrCore_String_CreateFromNative(state, "bool");
    interfaceName = ZrCore_String_CreateFromNative(state, "Readable");
    transformDecoratorName = ZrCore_String_CreateFromNative(
            state, "generateCrossKind");
    TEST_ASSERT_NOT_NULL(canonicalTypeName);
    TEST_ASSERT_NOT_NULL(interfaceName);
    TEST_ASSERT_NOT_NULL(transformDecoratorName);
    ZrCore_Memory_RawSet(&interfaceAdds, 0, sizeof(interfaceAdds));
    interfaceAdds.typeNames = &interfaceName;
    interfaceAdds.count = 1U;

    ZrCore_Memory_RawSet(&schema, 0, sizeof(schema));
    schema.name = ZrCore_String_CreateFromNative(state, "GeneratedLabel");
    schema.attributeId = 71U;
    schema.typeId = 73U;
    schema.usage.retention = ZR_PARSER_ATTRIBUTE_RETENTION_ARTIFACT;
    ZrCore_Array_Construct(&schema.fields);
    TEST_ASSERT_NOT_NULL(schema.name);
    ZrCore_Memory_RawSet(&attributeEntry, 0, sizeof(attributeEntry));
    attributeEntry.schema = &schema;
    attributeEntry.data.attributeId = schema.attributeId;
    attributeEntry.data.typeId = schema.typeId;
    attributeEntry.data.retention = schema.usage.retention;
    ZrCore_Memory_RawSet(&attributeAdds, 0, sizeof(attributeAdds));
    attributeAdds.entries = &attributeEntry;
    attributeAdds.count = 1U;

    ZrCore_Memory_RawSet(&location, 0, sizeof(location));
    location.start.line = 11U;
    location.end.line = 13U;
    symbolCountBefore = cs.semanticContext->symbols.length;
    nextSymbolIdBefore = cs.semanticContext->nextSymbolId;

    TEST_ASSERT_FALSE(ZrParser_CompileTime_CommitDeclarationPatchAtomic(
            &cs,
            &targetInfo,
            &addition,
            &canonicalTypeName,
            1U,
            &interfaceAdds,
            &attributeAdds,
            transformDecoratorName,
            79U,
            location,
            fail_declaration_patch_after_attribute_commit,
            ZR_NULL));
    TEST_ASSERT_EQUAL_UINT64(0U, targetInfo.members.length);
    TEST_ASSERT_EQUAL_UINT64(0U, targetInfo.inherits.length);
    TEST_ASSERT_EQUAL_UINT64(0U, targetInfo.implements.length);
    TEST_ASSERT_EQUAL_UINT64(0U, targetInfo.decorators.length);
    TEST_ASSERT_TRUE(targetInfo.hasDecoratorMetadata);
    TEST_ASSERT_EQUAL_PTR(
            existingMetadata, targetInfo.decoratorMetadataValue.value.object);
    TEST_ASSERT_NOT_NULL(ZrCore_Object_GetValue(
            state, existingMetadata, &existingMetadataKey));
    TEST_ASSERT_NULL(ZrCore_Object_GetValue(
            state, existingMetadata, &generatedAttributeKey));
    TEST_ASSERT_EQUAL_UINT64(
            symbolCountBefore, cs.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT32(
            nextSymbolIdBefore, cs.semanticContext->nextSymbolId);

    TEST_ASSERT_TRUE(ZrParser_CompileTime_CommitDeclarationPatchAtomic(
            &cs,
            &targetInfo,
            &addition,
            &canonicalTypeName,
            1U,
            &interfaceAdds,
            &attributeAdds,
            transformDecoratorName,
            83U,
            location,
            collect_declaration_patch_after_attribute_stage,
            state));
    TEST_ASSERT_EQUAL_UINT64(1U, targetInfo.members.length);
    TEST_ASSERT_EQUAL_UINT64(1U, targetInfo.inherits.length);
    TEST_ASSERT_EQUAL_UINT64(1U, targetInfo.implements.length);
    TEST_ASSERT_EQUAL_UINT64(2U, targetInfo.decorators.length);
    TEST_ASSERT_EQUAL_PTR(
            schema.name,
            ((SZrTypeDecoratorInfo *)ZrCore_Array_Get(
                    &targetInfo.decorators, 0U))->name);
    TEST_ASSERT_EQUAL_PTR(
            transformDecoratorName,
            ((SZrTypeDecoratorInfo *)ZrCore_Array_Get(
                    &targetInfo.decorators, 1U))->name);
    TEST_ASSERT_TRUE(targetInfo.hasDecoratorMetadata);
    TEST_ASSERT_TRUE(
            existingMetadata != ZR_CAST_OBJECT(
                                        state,
                                        targetInfo.decoratorMetadataValue
                                                .value.object));
    TEST_ASSERT_NOT_NULL(ZrCore_Object_GetValue(
            state,
            ZR_CAST_OBJECT(
                    state, targetInfo.decoratorMetadataValue.value.object),
            &existingMetadataKey));
    TEST_ASSERT_NOT_NULL(ZrCore_Object_GetValue(
            state,
            ZR_CAST_OBJECT(
                    state, targetInfo.decoratorMetadataValue.value.object),
            &generatedAttributeKey));
    TEST_ASSERT_NULL(ZrCore_Object_GetValue(
            state, existingMetadata, &generatedAttributeKey));
    TEST_ASSERT_EQUAL_UINT64(
            symbolCountBefore + 1U, cs.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT32(
            nextSymbolIdBefore + 1U, cs.semanticContext->nextSymbolId);

    ZrCore_GcRootHandle_Release(state, &generatedAttributeKeyRoot);
    ZrCore_GcRootHandle_Release(state, &existingMetadataRoot);
    ZrCore_Array_Free(state, &targetInfo.decorators);
    ZrCore_Array_Free(state, &targetInfo.implements);
    ZrCore_Array_Free(state, &targetInfo.inherits);
    ZrCore_Array_Free(state, &targetInfo.members);
    ZrParser_CompilerState_Free(&cs);
    destroy_test_state(state);
}

typedef struct SDeclarationPatchPrepareAllocationFailure {
    FZrAllocator allocator;
    TZrPtr allocatorUserData;
    TZrSize functionAllocationCount;
    TZrPtr initializedEntries;
    TZrSize unexpectedFreeCount;
    TZrBool rejectedContractData;
} SDeclarationPatchPrepareAllocationFailure;

static TZrPtr fail_declaration_patch_contract_data_allocator(
        TZrPtr userData,
        TZrPtr pointer,
        TZrSize originalSize,
        TZrSize newSize,
        TZrInt64 flag) {
    SDeclarationPatchPrepareAllocationFailure *failure =
            (SDeclarationPatchPrepareAllocationFailure *)userData;
    TZrPtr result;

    if (failure == ZR_NULL) {
        return ZR_NULL;
    }
    if (newSize == 0U && pointer != ZR_NULL &&
        flag == ZR_MEMORY_NATIVE_TYPE_FUNCTION) {
        if (pointer != failure->initializedEntries) {
            failure->unexpectedFreeCount++;
            return ZR_NULL;
        }
        failure->initializedEntries = ZR_NULL;
        return failure->allocator(
                failure->allocatorUserData,
                pointer,
                originalSize,
                newSize,
                flag);
    }
    if (pointer == ZR_NULL && newSize > 0U &&
        flag == ZR_MEMORY_NATIVE_TYPE_FUNCTION) {
        failure->functionAllocationCount++;
        if (failure->functionAllocationCount == 2U) {
            failure->rejectedContractData = ZR_TRUE;
            return ZR_NULL;
        }
        result = failure->allocator(
                failure->allocatorUserData,
                pointer,
                originalSize,
                newSize,
                flag);
        failure->initializedEntries = result;
        if (result != ZR_NULL) {
            ZrCore_Memory_RawSet(result, 0xa5U, newSize);
        }
        return result;
    }
    return failure->allocator(
            failure->allocatorUserData,
            pointer,
            originalSize,
            newSize,
            flag);
}

static void test_declaration_transform_attribute_prepare_oom_frees_only_initialized_entries(void) {
    SZrState *state = create_test_state();
    SZrCompilerState cs;
    SZrTypePrototypeInfo targetInfo;
    SZrObject *arrayObject;
    SZrTypeValue arrayValue;
    SZrTypeValue elementKey;
    SZrTypeValue elementValue;
    SZrParserCompileTimePatchAttributeAdds prepared;
    SDeclarationPatchPrepareAllocationFailure failure;
    TZrBool prepareResult;

    TEST_ASSERT_NOT_NULL(state);
    ZrParser_CompilerState_Init(&cs, state);
    ZrCore_Memory_RawSet(&targetInfo, 0, sizeof(targetInfo));
    arrayObject = ZrCore_Object_NewCustomized(
            state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    TEST_ASSERT_NOT_NULL(arrayObject);
    ZrCore_Object_Init(state, arrayObject);
    ZrCore_Value_InitAsInt(state, &elementKey, 0);
    ZrCore_Value_InitAsInt(state, &elementValue, 1);
    ZrCore_Object_SetValue(state, arrayObject, &elementKey, &elementValue);
    ZrCore_Value_InitAsRawObject(
            state, &arrayValue, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayObject));
    arrayValue.type = ZR_VALUE_TYPE_ARRAY;
    ZrCore_Memory_RawSet(&prepared, 0, sizeof(prepared));
    ZrCore_Memory_RawSet(&failure, 0, sizeof(failure));
    failure.allocator = state->global->allocator;
    failure.allocatorUserData = state->global->userAllocationArguments;
    state->global->allocator = fail_declaration_patch_contract_data_allocator;
    state->global->userAllocationArguments = &failure;

    prepareResult = ZrParser_CompileTime_PreparePatchAttributeAdds(
            &cs, &targetInfo, &arrayValue, (SZrFileRange){0}, &prepared);

    state->global->allocator = failure.allocator;
    state->global->userAllocationArguments = failure.allocatorUserData;
    TEST_ASSERT_FALSE(prepareResult);
    TEST_ASSERT_TRUE(failure.rejectedContractData);
    TEST_ASSERT_EQUAL_UINT64(0U, failure.unexpectedFreeCount);
    TEST_ASSERT_NULL(prepared.entries);
    TEST_ASSERT_NULL(prepared.contractData);
    TEST_ASSERT_EQUAL_UINT64(0U, prepared.count);

    ZrParser_CompilerState_Free(&cs);
    destroy_test_state(state);
}

typedef struct SDeclarationPatchStageFailure {
    EZrParserDeclarationPatchCommitStage stage;
    SZrState *gcState;
} SDeclarationPatchStageFailure;

static TZrBool fail_or_collect_declaration_patch_stage(
        EZrParserDeclarationPatchCommitStage stage,
        TZrSize committedCount,
        TZrPtr userData) {
    SDeclarationPatchStageFailure *failure =
            (SDeclarationPatchStageFailure *)userData;

    ZR_UNUSED_PARAMETER(committedCount)
    if (failure != ZR_NULL && failure->gcState != ZR_NULL &&
        stage == ZR_PARSER_DECLARATION_PATCH_COMMIT_GENERATED) {
        ZrCore_GarbageCollector_GcFull(failure->gcState, ZR_TRUE);
    }
    return failure == ZR_NULL || stage != failure->stage;
}

static void test_declaration_transform_failed_stage_preserves_array_identity(void) {
    SZrState *state = create_test_state();
    SZrCompilerState cs;
    SZrTypePrototypeInfo targetInfo;
    SZrTypeMemberInfo existingMember;
    SZrParserGeneratedDeclaration addition;
    SZrString *canonicalTypeName;
    SZrString *existingInterfaceName;
    SZrString *addedInterfaceName;
    SZrString *transformDecoratorName;
    SZrTypeDecoratorInfo existingDecorator;
    SZrParserCompileTimePatchInterfaceAdds interfaceAdds;
    SZrParserCompileTimePatchAttributeAdds attributeAdds;
    SDeclarationPatchStageFailure failure;
    TZrBytePtr membersHeadBefore;
    TZrBytePtr inheritsHeadBefore;
    TZrBytePtr implementsHeadBefore;
    TZrBytePtr decoratorsHeadBefore;
    TZrBytePtr symbolsHeadBefore;
    TZrSize membersCapacityBefore;
    TZrSize inheritsCapacityBefore;
    TZrSize implementsCapacityBefore;
    TZrSize decoratorsCapacityBefore;
    TZrSize symbolsCapacityBefore;
    TZrSize symbolsLengthBefore;
    TZrSymbolId existingSymbolId;
    TZrSymbolId nextSymbolIdBefore;
    const SZrSemanticSymbolRecord *existingSymbolBefore;

    TEST_ASSERT_NOT_NULL(state);
    ZrParser_CompilerState_Init(&cs, state);
    ZrCore_Memory_RawSet(&targetInfo, 0, sizeof(targetInfo));
    targetInfo.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    ZrCore_Array_Init(
            state, &targetInfo.members, sizeof(SZrTypeMemberInfo), 1U);
    ZrCore_Array_Init(
            state, &targetInfo.inherits, sizeof(SZrString *), 1U);
    ZrCore_Array_Init(
            state, &targetInfo.implements, sizeof(SZrString *), 1U);
    ZrCore_Array_Init(
            state, &targetInfo.decorators, sizeof(SZrTypeDecoratorInfo), 1U);
    ZrCore_Memory_RawSet(&existingMember, 0, sizeof(existingMember));
    existingMember.name = ZrCore_String_CreateFromNative(state, "existing");
    existingInterfaceName = ZrCore_String_CreateFromNative(state, "Base");
    addedInterfaceName = ZrCore_String_CreateFromNative(state, "Readable");
    existingDecorator.name = ZrCore_String_CreateFromNative(
            state, "existingDecorator");
    transformDecoratorName = ZrCore_String_CreateFromNative(
            state, "transformDecorator");
    canonicalTypeName = ZrCore_String_CreateFromNative(state, "bool");
    TEST_ASSERT_NOT_NULL(existingMember.name);
    TEST_ASSERT_NOT_NULL(existingInterfaceName);
    TEST_ASSERT_NOT_NULL(addedInterfaceName);
    TEST_ASSERT_NOT_NULL(existingDecorator.name);
    TEST_ASSERT_NOT_NULL(transformDecoratorName);
    TEST_ASSERT_NOT_NULL(canonicalTypeName);
    existingSymbolId = ZrParser_Semantic_RegisterSymbol(
            cs.semanticContext,
            existingMember.name,
            ZR_SEMANTIC_SYMBOL_KIND_FIELD,
            1U,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            (SZrFileRange){0});
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, existingSymbolId);
    existingMember.symbolId = existingSymbolId;
    existingSymbolBefore = ZrParser_Semantic_FindSymbolByNameAndKind(
            cs.semanticContext,
            existingMember.name,
            ZR_SEMANTIC_SYMBOL_KIND_FIELD);
    TEST_ASSERT_NOT_NULL(existingSymbolBefore);
    ZrCore_Array_Push(state, &targetInfo.members, &existingMember);
    ZrCore_Array_Push(state, &targetInfo.inherits, &existingInterfaceName);
    ZrCore_Array_Push(state, &targetInfo.implements, &existingInterfaceName);
    ZrCore_Array_Push(state, &targetInfo.decorators, &existingDecorator);
    membersHeadBefore = targetInfo.members.head;
    inheritsHeadBefore = targetInfo.inherits.head;
    implementsHeadBefore = targetInfo.implements.head;
    decoratorsHeadBefore = targetInfo.decorators.head;
    symbolsHeadBefore = cs.semanticContext->symbols.head;
    membersCapacityBefore = targetInfo.members.capacity;
    inheritsCapacityBefore = targetInfo.inherits.capacity;
    implementsCapacityBefore = targetInfo.implements.capacity;
    decoratorsCapacityBefore = targetInfo.decorators.capacity;
    symbolsCapacityBefore = cs.semanticContext->symbols.capacity;
    symbolsLengthBefore = cs.semanticContext->symbols.length;
    nextSymbolIdBefore = cs.semanticContext->nextSymbolId;

    ZrCore_Memory_RawSet(&addition, 0, sizeof(addition));
    addition.kind = ZR_PARSER_GENERATED_DECLARATION_FIELD;
    addition.name = "generated";
    addition.typeId = 1U;
    addition.visibility = ZR_PARSER_GENERATED_VISIBILITY_PRIVATE;
    addition.mutability = ZR_PARSER_GENERATED_MUTABILITY_LET;
    ZrCore_Memory_RawSet(&interfaceAdds, 0, sizeof(interfaceAdds));
    interfaceAdds.typeNames = &addedInterfaceName;
    interfaceAdds.count = 1U;
    ZrCore_Memory_RawSet(&attributeAdds, 0, sizeof(attributeAdds));
    failure.stage = ZR_PARSER_DECLARATION_PATCH_COMMIT_INTERFACES;
    failure.gcState = ZR_NULL;

    TEST_ASSERT_FALSE(ZrParser_CompileTime_CommitDeclarationPatchAtomic(
            &cs,
            &targetInfo,
            &addition,
            &canonicalTypeName,
            1U,
            &interfaceAdds,
            &attributeAdds,
            transformDecoratorName,
            91U,
            (SZrFileRange){0},
            fail_or_collect_declaration_patch_stage,
            &failure));
    TEST_ASSERT_EQUAL_PTR(membersHeadBefore, targetInfo.members.head);
    TEST_ASSERT_EQUAL_PTR(inheritsHeadBefore, targetInfo.inherits.head);
    TEST_ASSERT_EQUAL_PTR(implementsHeadBefore, targetInfo.implements.head);
    TEST_ASSERT_EQUAL_PTR(decoratorsHeadBefore, targetInfo.decorators.head);
    TEST_ASSERT_EQUAL_PTR(symbolsHeadBefore, cs.semanticContext->symbols.head);
    TEST_ASSERT_EQUAL_UINT64(membersCapacityBefore, targetInfo.members.capacity);
    TEST_ASSERT_EQUAL_UINT64(inheritsCapacityBefore, targetInfo.inherits.capacity);
    TEST_ASSERT_EQUAL_UINT64(
            implementsCapacityBefore, targetInfo.implements.capacity);
    TEST_ASSERT_EQUAL_UINT64(
            decoratorsCapacityBefore, targetInfo.decorators.capacity);
    TEST_ASSERT_EQUAL_UINT64(
            symbolsCapacityBefore, cs.semanticContext->symbols.capacity);
    TEST_ASSERT_EQUAL_UINT64(1U, targetInfo.members.length);
    TEST_ASSERT_EQUAL_UINT64(1U, targetInfo.inherits.length);
    TEST_ASSERT_EQUAL_UINT64(1U, targetInfo.implements.length);
    TEST_ASSERT_EQUAL_UINT64(1U, targetInfo.decorators.length);
    TEST_ASSERT_EQUAL_UINT64(
            symbolsLengthBefore, cs.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT32(
            nextSymbolIdBefore, cs.semanticContext->nextSymbolId);
    TEST_ASSERT_EQUAL_PTR(
            existingMember.name,
            ((SZrTypeMemberInfo *)ZrCore_Array_Get(
                    &targetInfo.members, 0U))->name);
    TEST_ASSERT_EQUAL_UINT32(
            existingSymbolId,
            ((SZrTypeMemberInfo *)ZrCore_Array_Get(
                    &targetInfo.members, 0U))->symbolId);
    TEST_ASSERT_EQUAL_PTR(
            existingDecorator.name,
            ((SZrTypeDecoratorInfo *)ZrCore_Array_Get(
                    &targetInfo.decorators, 0U))->name);
    TEST_ASSERT_EQUAL_PTR(
            existingSymbolBefore,
            ZrParser_Semantic_FindSymbolByNameAndKind(
                    cs.semanticContext,
                    existingMember.name,
                    ZR_SEMANTIC_SYMBOL_KIND_FIELD));
    TEST_ASSERT_EQUAL_UINT32(existingSymbolId, existingSymbolBefore->id);

    ZrCore_Array_Free(state, &targetInfo.decorators);
    ZrCore_Array_Free(state, &targetInfo.implements);
    ZrCore_Array_Free(state, &targetInfo.inherits);
    ZrCore_Array_Free(state, &targetInfo.members);
    ZrParser_CompilerState_Free(&cs);
    destroy_test_state(state);
}

static void test_declaration_transform_generated_metadata_survives_commit_gc(void) {
    SZrState *state = create_test_state();
    SZrCompilerState cs;
    SZrTypePrototypeInfo targetInfo;
    SZrParserGeneratedDeclaration addition;
    SZrString *canonicalTypeName;
    SZrParserCompileTimePatchInterfaceAdds interfaceAdds;
    SZrParserCompileTimePatchAttributeAdds attributeAdds;
    SDeclarationPatchStageFailure observer;
    SZrTypeMemberInfo *member;
    SZrString *generatedKeyString;
    SZrTypeValue generatedKey;

    TEST_ASSERT_NOT_NULL(state);
    ZrParser_CompilerState_Init(&cs, state);
    ZrCore_Memory_RawSet(&targetInfo, 0, sizeof(targetInfo));
    targetInfo.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    ZrCore_Array_Init(
            state, &targetInfo.members, sizeof(SZrTypeMemberInfo), 1U);
    ZrCore_Array_Init(
            state, &targetInfo.inherits, sizeof(SZrString *), 1U);
    ZrCore_Array_Init(
            state, &targetInfo.implements, sizeof(SZrString *), 1U);
    ZrCore_Array_Init(
            state, &targetInfo.decorators, sizeof(SZrTypeDecoratorInfo), 1U);
    ZrCore_Memory_RawSet(&addition, 0, sizeof(addition));
    addition.kind = ZR_PARSER_GENERATED_DECLARATION_FIELD;
    addition.name = "gcGenerated";
    addition.typeId = 1U;
    addition.visibility = ZR_PARSER_GENERATED_VISIBILITY_PRIVATE;
    addition.mutability = ZR_PARSER_GENERATED_MUTABILITY_LET;
    canonicalTypeName = ZrCore_String_CreateFromNative(state, "bool");
    TEST_ASSERT_NOT_NULL(canonicalTypeName);
    ZrCore_Memory_RawSet(&interfaceAdds, 0, sizeof(interfaceAdds));
    ZrCore_Memory_RawSet(&attributeAdds, 0, sizeof(attributeAdds));
    observer.stage = (EZrParserDeclarationPatchCommitStage)0;
    observer.gcState = state;

    TEST_ASSERT_TRUE(ZrParser_CompileTime_CommitDeclarationPatchAtomic(
            &cs,
            &targetInfo,
            &addition,
            &canonicalTypeName,
            1U,
            &interfaceAdds,
            &attributeAdds,
            ZR_NULL,
            97U,
            (SZrFileRange){0},
            fail_or_collect_declaration_patch_stage,
            &observer));
    TEST_ASSERT_EQUAL_UINT64(1U, targetInfo.members.length);
    member = (SZrTypeMemberInfo *)ZrCore_Array_Get(&targetInfo.members, 0U);
    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_NOT_NULL(member->name);
    TEST_ASSERT_EQUAL_STRING(
            "gcGenerated", ZrCore_String_GetNativeString(member->name));
    TEST_ASSERT_TRUE(member->hasDecoratorMetadata);
    TEST_ASSERT_EQUAL_INT(
            ZR_VALUE_TYPE_OBJECT, member->decoratorMetadataValue.type);
    generatedKeyString = ZrCore_String_CreateFromNative(state, "generated");
    TEST_ASSERT_NOT_NULL(generatedKeyString);
    ZrCore_Value_InitAsRawObject(
            state,
            &generatedKey,
            ZR_CAST_RAW_OBJECT_AS_SUPER(generatedKeyString));
    generatedKey.type = ZR_VALUE_TYPE_STRING;
    TEST_ASSERT_NOT_NULL(ZrCore_Object_GetValue(
            state,
            ZR_CAST_OBJECT(
                    state, member->decoratorMetadataValue.value.object),
            &generatedKey));

    ZrCore_Array_Free(state, &targetInfo.decorators);
    ZrCore_Array_Free(state, &targetInfo.implements);
    ZrCore_Array_Free(state, &targetInfo.inherits);
    ZrCore_Array_Free(state, &targetInfo.members);
    ZrParser_CompilerState_Free(&cs);
    destroy_test_state(state);
}

#endif // ZR_VM_TEST_COMPILE_TIME_DECLARATION_PATCH_TRANSACTION_CASES_H
