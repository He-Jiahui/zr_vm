#ifndef ZR_VM_TEST_COMPILE_TIME_DECLARATION_PATCH_TRANSACTION_HASH_CASES_H
#define ZR_VM_TEST_COMPILE_TIME_DECLARATION_PATCH_TRANSACTION_HASH_CASES_H

typedef struct SDeclarationPatchHashPairFailure {
    FZrAllocator allocator;
    TZrPtr allocatorUserData;
    TZrSize allocationAttempt;
    TZrSize rejectAtAttempt;
    TZrBool rejected;
} SDeclarationPatchHashPairFailure;

static TZrPtr fail_declaration_patch_hash_pair_allocator(
        TZrPtr userData,
        TZrPtr pointer,
        TZrSize originalSize,
        TZrSize newSize,
        TZrInt64 flag) {
    SDeclarationPatchHashPairFailure *failure =
            (SDeclarationPatchHashPairFailure *)userData;

    if (failure != ZR_NULL && newSize > 0U &&
        flag == ZR_MEMORY_NATIVE_TYPE_HASH_PAIR) {
        failure->allocationAttempt++;
        if (!failure->rejected &&
            failure->allocationAttempt == failure->rejectAtAttempt) {
            failure->rejected = ZR_TRUE;
            return ZR_NULL;
        }
    }
    return failure->allocator(
            failure->allocatorUserData,
            pointer,
            originalSize,
            newSize,
            flag);
}

static void assert_generated_hash_pair_retry_preserves_metadata(
        TZrSize rejectAtAttempt) {
    SZrState *state = create_test_state();
    SZrCompilerState cs;
    SZrTypePrototypeInfo targetInfo;
    SZrParserGeneratedDeclaration addition;
    SZrString *canonicalTypeName;
    SDeclarationPatchHashPairFailure failure;
    SZrTypeMemberInfo *member;
    SZrString *generatedKeyString;
    SZrTypeValue generatedKey;
    TZrSize symbolsLengthBefore;
    TZrSymbolId nextSymbolIdBefore;
    TZrBool commitResult;

    TEST_ASSERT_NOT_NULL(state);
    ZrParser_CompilerState_Init(&cs, state);
    ZrCore_Memory_RawSet(&targetInfo, 0, sizeof(targetInfo));
    targetInfo.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    ZrCore_Array_Init(
            state, &targetInfo.members, sizeof(SZrTypeMemberInfo), 1U);
    canonicalTypeName = ZrCore_String_CreateFromNative(state, "bool");
    TEST_ASSERT_NOT_NULL(canonicalTypeName);
    ZrCore_Memory_RawSet(&addition, 0, sizeof(addition));
    addition.kind = ZR_PARSER_GENERATED_DECLARATION_FIELD;
    addition.name = "hashPairFailure";
    addition.typeId = 1U;
    addition.visibility = ZR_PARSER_GENERATED_VISIBILITY_PRIVATE;
    addition.mutability = ZR_PARSER_GENERATED_MUTABILITY_LET;
    symbolsLengthBefore = cs.semanticContext->symbols.length;
    nextSymbolIdBefore = cs.semanticContext->nextSymbolId;
    ZrCore_Memory_RawSet(&failure, 0, sizeof(failure));
    failure.allocator = state->global->allocator;
    failure.allocatorUserData = state->global->userAllocationArguments;
    failure.rejectAtAttempt = rejectAtAttempt;
    state->global->allocator = fail_declaration_patch_hash_pair_allocator;
    state->global->userAllocationArguments = &failure;

    commitResult = ZrParser_CompileTime_CommitGeneratedFieldsAtomic(
            &cs,
            &targetInfo,
            &addition,
            &canonicalTypeName,
            1U,
            101U,
            (SZrFileRange){0},
            ZR_NULL,
            ZR_NULL);

    state->global->allocator = failure.allocator;
    state->global->userAllocationArguments = failure.allocatorUserData;
    TEST_ASSERT_TRUE(commitResult);
    TEST_ASSERT_TRUE(failure.rejected);
    TEST_ASSERT_EQUAL_UINT64(1U, targetInfo.members.length);
    TEST_ASSERT_EQUAL_UINT64(
            symbolsLengthBefore + 1U, cs.semanticContext->symbols.length);
    TEST_ASSERT_EQUAL_UINT32(
            nextSymbolIdBefore + 1U, cs.semanticContext->nextSymbolId);
    member = (SZrTypeMemberInfo *)ZrCore_Array_Get(
            &targetInfo.members, 0U);
    TEST_ASSERT_NOT_NULL(member);
    TEST_ASSERT_TRUE(member->hasDecoratorMetadata);
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

    ZrCore_Array_Free(state, &targetInfo.members);
    ZrParser_CompilerState_Free(&cs);
    destroy_test_state(state);
}

static void assert_attribute_hash_pair_retry_preserves_metadata(
        TZrSize rejectAtAttempt) {
    SZrState *state = create_test_state();
    SZrCompilerState cs;
    SZrTypePrototypeInfo targetInfo;
    SZrCompilerAttributeSchemaBinding schema;
    SZrParserCompileTimePatchAttributeAdd attributeEntry;
    SZrParserCompileTimePatchAttributeAdds attributeAdds;
    SZrParserCompileTimePatchInterfaceAdds interfaceAdds;
    SDeclarationPatchHashPairFailure failure;
    SZrString *attributeKeyString;
    SZrString *attributeIdKeyString;
    SZrTypeValue attributeKey;
    SZrTypeValue attributeIdKey;
    const SZrTypeValue *entryValue;
    TZrBool commitResult;

    TEST_ASSERT_NOT_NULL(state);
    ZrParser_CompilerState_Init(&cs, state);
    ZrCore_Memory_RawSet(&targetInfo, 0, sizeof(targetInfo));
    targetInfo.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    ZrCore_Array_Init(
            state,
            &targetInfo.decorators,
            sizeof(SZrTypeDecoratorInfo),
            1U);
    ZrCore_Memory_RawSet(&schema, 0, sizeof(schema));
    schema.name = ZrCore_String_CreateFromNative(state, "HashRetryAttribute");
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
    ZrCore_Memory_RawSet(&interfaceAdds, 0, sizeof(interfaceAdds));
    ZrCore_Memory_RawSet(&failure, 0, sizeof(failure));
    failure.allocator = state->global->allocator;
    failure.allocatorUserData = state->global->userAllocationArguments;
    failure.rejectAtAttempt = rejectAtAttempt;
    state->global->allocator = fail_declaration_patch_hash_pair_allocator;
    state->global->userAllocationArguments = &failure;

    commitResult = ZrParser_CompileTime_CommitDeclarationPatchAtomic(
            &cs,
            &targetInfo,
            ZR_NULL,
            ZR_NULL,
            0U,
            &interfaceAdds,
            &attributeAdds,
            ZR_NULL,
            103U,
            (SZrFileRange){0},
            ZR_NULL,
            ZR_NULL);

    state->global->allocator = failure.allocator;
    state->global->userAllocationArguments = failure.allocatorUserData;
    TEST_ASSERT_TRUE(commitResult);
    TEST_ASSERT_TRUE(failure.rejected);
    TEST_ASSERT_EQUAL_UINT64(1U, targetInfo.decorators.length);
    TEST_ASSERT_TRUE(targetInfo.hasDecoratorMetadata);
    attributeKeyString = ZrCore_String_CreateFromNative(
            state, "attribute:00000047:0");
    TEST_ASSERT_NOT_NULL(attributeKeyString);
    ZrCore_Value_InitAsRawObject(
            state,
            &attributeKey,
            ZR_CAST_RAW_OBJECT_AS_SUPER(attributeKeyString));
    attributeKey.type = ZR_VALUE_TYPE_STRING;
    entryValue = ZrCore_Object_GetValue(
            state,
            ZR_CAST_OBJECT(
                    state, targetInfo.decoratorMetadataValue.value.object),
            &attributeKey);
    TEST_ASSERT_NOT_NULL(entryValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, entryValue->type);
    attributeIdKeyString = ZrCore_String_CreateFromNative(state, "attributeId");
    TEST_ASSERT_NOT_NULL(attributeIdKeyString);
    ZrCore_Value_InitAsRawObject(
            state,
            &attributeIdKey,
            ZR_CAST_RAW_OBJECT_AS_SUPER(attributeIdKeyString));
    attributeIdKey.type = ZR_VALUE_TYPE_STRING;
    TEST_ASSERT_NOT_NULL(ZrCore_Object_GetValue(
            state,
            ZR_CAST_OBJECT(state, entryValue->value.object),
            &attributeIdKey));

    ZrCore_Array_Free(state, &targetInfo.decorators);
    ZrParser_CompilerState_Free(&cs);
    destroy_test_state(state);
}

static void test_declaration_transform_hash_pair_retry_preserves_metadata(void) {
    assert_generated_hash_pair_retry_preserves_metadata(1U);
    assert_generated_hash_pair_retry_preserves_metadata(3U);
}

static void test_declaration_transform_attribute_hash_pair_retry_preserves_metadata(void) {
    /* Attempts 2 and 7 are the first entry field and the metadata envelope. */
    assert_attribute_hash_pair_retry_preserves_metadata(2U);
    assert_attribute_hash_pair_retry_preserves_metadata(7U);
}

#endif // ZR_VM_TEST_COMPILE_TIME_DECLARATION_PATCH_TRANSACTION_HASH_CASES_H
