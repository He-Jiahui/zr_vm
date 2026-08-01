#ifndef TEST_PROPERTY_CONSUMER_RUNTIME_BOOTSTRAP_CASES_H
#define TEST_PROPERTY_CONSUMER_RUNTIME_BOOTSTRAP_CASES_H

static SZrTypePrototypeInfo *property_consumer_find_prototype(
        SZrCompilerState *cs,
        const TZrChar *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *prototype =
                (SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        &cs->typePrototypes,
                        index);
        if (prototype != ZR_NULL && prototype->name != ZR_NULL &&
            strcmp(
                    ZrCore_String_GetNativeString(prototype->name),
                    name) == 0) {
            return prototype;
        }
    }
    return ZR_NULL;
}

static void property_consumer_add_empty_imported_placeholder(
        SZrCompilerState *cs,
        TZrNativeString name,
        TZrNativeString moduleName) {
    SZrTypePrototypeInfo prototype;

    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_NOT_NULL(moduleName);
    memset(&prototype, 0, sizeof(prototype));
    prototype.name = ZrCore_String_CreateFromNative(g_state, name);
    prototype.importModuleName =
            ZrCore_String_CreateFromNative(g_state, moduleName);
    TEST_ASSERT_NOT_NULL(prototype.name);
    TEST_ASSERT_NOT_NULL(prototype.importModuleName);
    prototype.type = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    prototype.accessModifier = ZR_ACCESS_PUBLIC;
    prototype.isImportedNative = ZR_TRUE;
    ZrCore_Array_Init(
            g_state,
            &prototype.inherits,
            sizeof(SZrString *),
            1U);
    ZrCore_Array_Init(
            g_state,
            &prototype.implements,
            sizeof(SZrString *),
            1U);
    ZrCore_Array_Init(
            g_state,
            &prototype.genericParameters,
            sizeof(SZrTypeGenericParameterInfo),
            1U);
    ZrCore_Array_Init(
            g_state,
            &prototype.members,
            sizeof(SZrTypeMemberInfo),
            1U);
    ZrCore_Array_Init(
            g_state,
            &prototype.decorators,
            sizeof(SZrTypeDecoratorInfo),
            1U);
    ZrCore_Value_ResetAsNull(&prototype.decoratorMetadataValue);
    ZrCore_Array_Push(g_state, &cs->typePrototypes, &prototype);
}

static void property_consumer_assert_public_runtime_bootstrap(
        const SZrFunction *function) {
    SZrCompilerState cs;
    SZrTypePrototypeInfo *prototype;
    SZrTypeMemberInfo *visible = ZR_NULL;
    SZrTypeMemberInfo *getter = ZR_NULL;
    SZrTypeMemberInfo *ordinaryFake = ZR_NULL;
    SZrParserSemanticPropertyQuery query;

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    property_consumer_add_empty_imported_placeholder(
            &cs,
            "Meter",
            "property_bootstrap_provider");
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)cs.typePrototypes.length);
    TEST_ASSERT_TRUE(
            ZrParser_TypeInference_RegisterRuntimePrototypes(&cs, function));
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)cs.typePrototypes.length);

    prototype = property_consumer_find_prototype(&cs, "Meter");
    TEST_ASSERT_NOT_NULL(prototype);
    TEST_ASSERT_TRUE(prototype->isImportedNative);
    TEST_ASSERT_NOT_NULL(prototype->importModuleName);
    TEST_ASSERT_EQUAL_STRING(
            "property_bootstrap_provider",
            ZrCore_String_GetNativeString(prototype->importModuleName));
    for (TZrSize index = 0U; index < prototype->members.length; index++) {
        SZrTypeMemberInfo *member =
                (SZrTypeMemberInfo *)ZrCore_Array_Get(
                        &prototype->members,
                        index);
        if (member == ZR_NULL) {
            continue;
        }
        if (member->memberType == ZR_AST_PROPERTY_DECLARATION &&
            member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_NONE) {
            visible = member;
        } else if (member->accessorRole ==
                   ZR_PROPERTY_ACCESSOR_ROLE_GET) {
            getter = member;
        } else if (member->name != ZR_NULL &&
                   strcmp(
                           ZrCore_String_GetNativeString(member->name),
                           "__get_fake") == 0) {
            ordinaryFake = member;
        }
    }

    TEST_ASSERT_NOT_NULL(visible);
    TEST_ASSERT_NOT_NULL(getter);
    TEST_ASSERT_NOT_NULL(ordinaryFake);
    TEST_ASSERT_EQUAL_UINT32(
            visible->propertyIdentity,
            getter->propertyIdentity);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID,
            visible->propertySymbolId);
    TEST_ASSERT_NOT_EQUAL(
            ZR_SEMANTIC_ID_INVALID,
            visible->propertyValueTypeId);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_REFERENCE_ACCESS_READONLY,
            visible->metaType);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_PROPERTY_ACCESSOR_ROLE_NONE,
            ordinaryFake->accessorRole);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, ordinaryFake->propertyIdentity);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ordinaryFake->propertySymbolId);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_PropertyBySymbolId(
            cs.semanticContext,
            visible->propertySymbolId,
            &query));
    TEST_ASSERT_EQUAL_UINT32(
            visible->propertyValueTypeId,
            query.propertyTypeId);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_REFERENCE_ACCESS_READONLY,
            query.referenceAccess);
    TEST_ASSERT_EQUAL_UINT32(
            getter->symbolId,
            query.getterSymbolId);

    ZrParser_CompilerState_Free(&cs);
}

static void property_consumer_break_getter_property_identity(
        SZrFunction *function) {
    SZrCompiledPrototypeInfo *prototype;
    SZrCompiledMemberInfo *members;
    TZrBool found = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(function->prototypeData);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, function->prototypeCount);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
            sizeof(TZrUInt32) + sizeof(SZrCompiledPrototypeInfo),
            function->prototypeDataLength);
    prototype = (SZrCompiledPrototypeInfo *)(
            function->prototypeData + sizeof(TZrUInt32));
    members = (SZrCompiledMemberInfo *)(
            (TZrByte *)prototype + sizeof(*prototype) +
            prototype->inheritsCount * sizeof(TZrUInt32) +
            prototype->decoratorsCount * sizeof(TZrUInt32));
    for (TZrUInt32 index = 0U; index < prototype->membersCount; index++) {
        if (members[index].accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_GET) {
            members[index].propertyIdentity = UINT32_MAX;
            found = ZR_TRUE;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);
}

static void property_consumer_assert_incomplete_property_stays_unavailable(
        const SZrFunction *function) {
    SZrCompilerState cs;
    SZrTypePrototypeInfo *prototype;
    SZrTypeMemberInfo *visible = ZR_NULL;
    SZrTypeMemberInfo *getter = ZR_NULL;
    SZrParserSemanticPropertyQuery query;

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    property_consumer_add_empty_imported_placeholder(
            &cs,
            "Meter",
            "property_bootstrap_provider");
    TEST_ASSERT_TRUE(
            ZrParser_TypeInference_RegisterRuntimePrototypes(&cs, function));
    prototype = property_consumer_find_prototype(&cs, "Meter");
    TEST_ASSERT_NOT_NULL(prototype);
    for (TZrSize index = 0U; index < prototype->members.length; index++) {
        SZrTypeMemberInfo *member =
                (SZrTypeMemberInfo *)ZrCore_Array_Get(
                        &prototype->members,
                        index);
        if (member == ZR_NULL) {
            continue;
        }
        if (member->memberType == ZR_AST_PROPERTY_DECLARATION) {
            visible = member;
        } else if (member->accessorRole ==
                   ZR_PROPERTY_ACCESSOR_ROLE_GET) {
            getter = member;
        }
    }
    TEST_ASSERT_NOT_NULL(visible);
    TEST_ASSERT_NOT_NULL(getter);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, getter->propertyIdentity);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            visible->propertySymbolId);
    TEST_ASSERT_FALSE(ZrParser_SemanticQuery_PropertyBySymbolId(
            cs.semanticContext,
            visible->propertySymbolId,
            &query));
    ZrParser_CompilerState_Free(&cs);
}

static void test_public_runtime_prototype_bootstrap_preserves_property_contract(
        void) {
    static const TZrChar binaryPath[] =
            "property_consumer_public_bootstrap.zro";
    static const TZrChar source[] =
            "class Meter {\n"
            "  pri var stored: int = 7;\n"
            "  pub property value: ref readonly int {\n"
            "    get => ref this.stored;\n"
            "  }\n"
            "  pub fn __get_fake(): int { return 9; }\n"
            "}\n"
            "return 0;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state,
            "property_consumer_public_bootstrap.zr");
    SZrCompilerState emptyCompiler;
    SZrFunction emptyFunction;
    SZrFunction *function;
    SZrFunction *loadedFunction;

    memset(&emptyCompiler, 0, sizeof(emptyCompiler));
    memset(&emptyFunction, 0, sizeof(emptyFunction));
    ZrParser_CompilerState_Init(&emptyCompiler, g_state);
    TEST_ASSERT_FALSE(
            ZrParser_TypeInference_RegisterRuntimePrototypes(
                    ZR_NULL,
                    &emptyFunction));
    TEST_ASSERT_FALSE(
            ZrParser_TypeInference_RegisterRuntimePrototypes(
                    &emptyCompiler,
                    ZR_NULL));
    TEST_ASSERT_TRUE(
            ZrParser_TypeInference_RegisterRuntimePrototypes(
                    &emptyCompiler,
                    &emptyFunction));
    ZrParser_CompilerState_Free(&emptyCompiler);

    function = ZrParser_Source_Compile(
            g_state,
            source,
            strlen(source),
            sourceName);
    TEST_ASSERT_NOT_NULL(function);
    property_consumer_assert_public_runtime_bootstrap(function);
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(
            g_state,
            function,
            binaryPath));
    loadedFunction = property_consumer_load_binary_entry(binaryPath);
    property_consumer_assert_public_runtime_bootstrap(loadedFunction);
    property_consumer_break_getter_property_identity(loadedFunction);
    property_consumer_assert_incomplete_property_stays_unavailable(
            loadedFunction);

    remove(binaryPath);
    ZrCore_Function_Free(g_state, loadedFunction);
    ZrCore_Function_Free(g_state, function);
}

#endif
