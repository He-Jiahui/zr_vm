#ifndef ZR_TEST_AOT_C_DYNAMIC_RETURN_REPRESENTATION_CASES_H
#define ZR_TEST_AOT_C_DYNAMIC_RETURN_REPRESENTATION_CASES_H

#include "zr_vm_core/gc.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_common/zr_instruction_conf.h"

typedef struct SZrTestDynamicReturnRepresentationFixture {
    SZrState *state;
    SZrFunction *function;
    SZrGcRootLease root;
    char *text;
    TZrChar path[ZR_TESTS_PATH_MAX];
} SZrTestDynamicReturnRepresentationFixture;

typedef struct SZrTestDynamicReturnMetadata {
    TZrBool hasReturnType;
    SZrFunctionTypedTypeRef returnType;
    TZrBool hasIdentity;
    SZrFunctionSourceCallableIdentity identity;
} SZrTestDynamicReturnMetadata;

/* Unity may longjmp from any prerequisite; ownership stays outside the case. */
static SZrTestDynamicReturnRepresentationFixture g_dynamicReturnRepresentation;

static void dynamic_return_representation_cleanup(void) {
    free(g_dynamicReturnRepresentation.text);
    if (g_dynamicReturnRepresentation.state != ZR_NULL) {
        ZrCore_Gc_RootLeaseRelease(g_dynamicReturnRepresentation.state->global,
                                 &g_dynamicReturnRepresentation.root);
        if (g_dynamicReturnRepresentation.function != ZR_NULL) {
            ZrCore_Function_Free(g_dynamicReturnRepresentation.state,
                                 g_dynamicReturnRepresentation.function);
        }
        ZrTests_Runtime_State_Destroy(g_dynamicReturnRepresentation.state);
    }
    memset(&g_dynamicReturnRepresentation, 0, sizeof(g_dynamicReturnRepresentation));
}

static void dynamic_return_representation_compile(const char *source) {
    SZrString *sourceName;

    dynamic_return_representation_cleanup();
    g_dynamicReturnRepresentation.state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_dynamicReturnRepresentation.state,
                                "fixture prerequisite: dynamic return runtime is unavailable");
    sourceName = ZrCore_String_CreateFromNative(
            g_dynamicReturnRepresentation.state, "dynamic_return_representation.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName,
                                "fixture prerequisite: dynamic return source name is unavailable");
    g_dynamicReturnRepresentation.function = ZrParser_Source_Compile(
            g_dynamicReturnRepresentation.state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_dynamicReturnRepresentation.function,
                                "fixture prerequisite: dynamic return source did not compile");
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_Gc_RootLeaseAcquire(
                                     g_dynamicReturnRepresentation.state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(g_dynamicReturnRepresentation.function),
                                     &g_dynamicReturnRepresentation.root),
                             "fixture prerequisite: dynamic return function could not be rooted");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            0u, g_dynamicReturnRepresentation.function->childFunctionLength,
            "fixture prerequisite: dynamic return source must have only a script entry");
}

static SZrTestDynamicReturnMetadata dynamic_return_representation_snapshot(void) {
    const SZrFunction *function = g_dynamicReturnRepresentation.function;
    SZrTestDynamicReturnMetadata snapshot;

    snapshot.hasReturnType = function->hasCallableReturnType;
    snapshot.returnType = function->callableReturnType;
    snapshot.hasIdentity = function->hasSourceCallableIdentity;
    snapshot.identity = function->sourceCallableIdentity;
    return snapshot;
}

static TZrBool dynamic_return_representation_type_equal(
        const SZrFunctionTypedTypeRef *left, const SZrFunctionTypedTypeRef *right) {
    return (TZrBool)(left->baseType == right->baseType &&
            left->isNullable == right->isNullable &&
            left->ownershipQualifier == right->ownershipQualifier &&
            left->isArray == right->isArray && left->typeName == right->typeName &&
            left->elementBaseType == right->elementBaseType &&
            left->elementTypeName == right->elementTypeName &&
            left->staticCType == right->staticCType &&
            left->staticCTypeId == right->staticCTypeId);
}

static TZrBool dynamic_return_representation_unchanged(
        const SZrTestDynamicReturnMetadata *snapshot) {
    const SZrFunction *function = g_dynamicReturnRepresentation.function;
    const SZrFunctionSourceCallableIdentity *actual = &function->sourceCallableIdentity;
    const SZrFunctionSourceCallableIdentity *expected = &snapshot->identity;

    return (TZrBool)(function->hasCallableReturnType == snapshot->hasReturnType &&
            dynamic_return_representation_type_equal(&function->callableReturnType,
                                                      &snapshot->returnType) &&
            function->hasSourceCallableIdentity == snapshot->hasIdentity &&
            actual->schemaVersion == expected->schemaVersion &&
            actual->symbolId == expected->symbolId && actual->typeId == expected->typeId &&
            actual->canonicalSignatureHash == expected->canonicalSignatureHash &&
            actual->returnPrimitive == expected->returnPrimitive &&
            actual->parameterCount == expected->parameterCount &&
            actual->receiverFlags == expected->receiverFlags &&
            actual->effectFlags == expected->effectFlags &&
            actual->declarationRange.startLine == expected->declarationRange.startLine &&
            actual->declarationRange.startColumn == expected->declarationRange.startColumn &&
            actual->declarationRange.endLine == expected->declarationRange.endLine &&
            actual->declarationRange.endColumn == expected->declarationRange.endColumn &&
            actual->hasExplicitNoArgsI64 == expected->hasExplicitNoArgsI64);
}

static void dynamic_return_representation_write(const char *caseName) {
    SZrAotWriterOptions options;

    TEST_ASSERT_TRUE_MESSAGE(ZrTests_Path_GetGeneratedArtifact(
                                     "aot_c_method_info_signature", caseName, "main", ".c",
                                     g_dynamicReturnRepresentation.path,
                                     sizeof(g_dynamicReturnRepresentation.path)),
                             "fixture prerequisite: dynamic return output path is unavailable");
    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_Path_EnsureParentDirectory(g_dynamicReturnRepresentation.path),
            "fixture prerequisite: dynamic return output directory is unavailable");
    memset(&options, 0, sizeof(options));
    options.moduleName = caseName;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.requireExecutableLowering = ZR_TRUE;
    if (!ZrParser_Writer_WriteAotCFileWithOptions(
                g_dynamicReturnRepresentation.state, g_dynamicReturnRepresentation.function,
                g_dynamicReturnRepresentation.path, &options)) {
        printf("dynamic return fixture: writer rejected case=%s\n", caseName);
        TEST_FAIL_MESSAGE("fixture prerequisite: public AOT writer rejected the dynamic return shape");
    }
    g_dynamicReturnRepresentation.text = ZrTests_ReadTextFile(
            g_dynamicReturnRepresentation.path, ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_dynamicReturnRepresentation.text,
                                "fixture prerequisite: dynamic return output could not be read");
}

static TZrBool dynamic_return_representation_signature_matches(
        const SZrFunctionTypedTypeRef *expected) {
    static const char tablePrefix[] =
            "static const SZrAotSignatureType zr_aot_signature_0_types[] = {\n";
    const char *table = strstr(g_dynamicReturnRepresentation.text, tablePrefix);
    const char *signature = strstr(g_dynamicReturnRepresentation.text,
                                   "static const SZrAotSignature zr_aot_signature_0 = {\n");
    const char *signatureEnd = signature != ZR_NULL ? strstr(signature, "};\n") : ZR_NULL;
    const char *returnType = signature != ZR_NULL
            ? strstr(signature, "    .returnType = &zr_aot_signature_0_types[0],\n") : ZR_NULL;
    const char *hasReturn = signature != ZR_NULL
            ? strstr(signature, "    .hasReturnValue = (TZrUInt8)1u,\n") : ZR_NULL;
    char row[512];
    int length = snprintf(
            row, sizeof(row),
            "    {\n"
            "        .baseType = (TZrUInt16)%uu,\n"
            "        .staticCType = (TZrUInt16)%uu,\n"
            "        .staticCTypeId = %uu,\n"
            "        .ownershipQualifier = %uu,\n"
            "        .elementBaseType = (TZrUInt16)%uu,\n"
            "        .isNullable = (TZrUInt8)%uu,\n"
            "        .isArray = (TZrUInt8)%uu,\n"
            "        .passingMode = (TZrUInt8)%uu,\n"
            "    },\n",
            (unsigned)expected->baseType, (unsigned)expected->staticCType,
            (unsigned)expected->staticCTypeId, (unsigned)expected->ownershipQualifier,
            (unsigned)expected->elementBaseType, expected->isNullable ? 1u : 0u,
            expected->isArray ? 1u : 0u, (unsigned)ZR_AOT_PARAMETER_PASSING_UNKNOWN);

    return (TZrBool)(table != ZR_NULL && signatureEnd != ZR_NULL &&
            returnType != ZR_NULL && returnType < signatureEnd &&
            hasReturn != ZR_NULL && hasReturn < signatureEnd &&
            length > 0 && (size_t)length < sizeof(row) &&
            strncmp(table + strlen(tablePrefix), row, (size_t)length) == 0);
}

static void test_aot_c_known_dynamic_i64_return_refines_only_c_signature(void) {
    SZrTestDynamicReturnMetadata snapshot;
    SZrFunctionTypedTypeRef expected;
    SZrFunctionTypedTypeRef plainDynamic;
    TZrBool passed;

    dynamic_return_representation_compile("var value: int = 7;\nreturn -value;\n");
    TEST_ASSERT_TRUE(g_dynamicReturnRepresentation.function->hasCallableReturnType);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64,
                         g_dynamicReturnRepresentation.function->callableReturnType.baseType);
    /* Exercise the backend's dynamic representation refinement explicitly;
     * the compiler can already prove this source's scalar representation. */
    init_signature_type_ref(&plainDynamic, ZR_VALUE_TYPE_INT64, ZR_STATIC_C_TYPE_DYNAMIC);
    plainDynamic.staticCTypeId = 0u;
    g_dynamicReturnRepresentation.function->callableReturnType = plainDynamic;
    snapshot = dynamic_return_representation_snapshot();
    expected = snapshot.returnType;
    expected.staticCType = ZR_STATIC_C_TYPE_I64;
    dynamic_return_representation_write("known_dynamic_i64_return");
    passed = (TZrBool)(dynamic_return_representation_signature_matches(&expected) &&
            dynamic_return_representation_unchanged(&snapshot) &&
            strstr(g_dynamicReturnRepresentation.text,
                   "ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s") != ZR_NULL &&
            strstr(g_dynamicReturnRepresentation.text,
                   "ZrLibrary_AotRuntime_Return(state, &frame,") == ZR_NULL);
    if (!passed) {
        TEST_FAIL_MESSAGE("A known dynamic i64 return must refine only its C signature representation");
    }
    dynamic_return_representation_cleanup();
}

static void test_aot_c_known_dynamic_return_preserves_unproven_semantics(void) {
    static const struct {
        const char *caseName;
        EZrValueType baseType;
        TZrBool isNullable;
        TZrBool isArray;
        TZrUInt32 ownershipQualifier;
        TZrBool partialProof;
    } cases[] = {
            {"dynamic_return_base_mismatch", ZR_VALUE_TYPE_BOOL,
             ZR_FALSE, ZR_FALSE, ZR_OWNERSHIP_QUALIFIER_NONE, ZR_FALSE},
            {"dynamic_return_nullable", ZR_VALUE_TYPE_INT64,
             ZR_TRUE, ZR_FALSE, ZR_OWNERSHIP_QUALIFIER_NONE, ZR_FALSE},
            {"dynamic_return_array", ZR_VALUE_TYPE_ARRAY,
             ZR_FALSE, ZR_TRUE, ZR_OWNERSHIP_QUALIFIER_NONE, ZR_FALSE},
            {"dynamic_return_owned", ZR_VALUE_TYPE_OBJECT,
             ZR_FALSE, ZR_FALSE, ZR_OWNERSHIP_QUALIFIER_SHARED, ZR_FALSE},
            {"dynamic_return_partial_proof", ZR_VALUE_TYPE_INT64,
             ZR_FALSE, ZR_FALSE, ZR_OWNERSHIP_QUALIFIER_NONE, ZR_TRUE},
    };

    for (size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        SZrTestDynamicReturnMetadata snapshot;
        SZrFunction *function;

        dynamic_return_representation_compile(cases[index].partialProof
                ? "var value: int = 7;\nif (value > 0) { return value; }\nreturn true;\n"
                : "var value: int = 7;\nreturn -value;\n");
        function = g_dynamicReturnRepresentation.function;
        if (cases[index].partialProof) {
            TZrUInt32 returnCount = 0u;
            for (TZrUInt32 instruction = 0u; instruction < function->instructionsLength; instruction++) {
                if (function->instructionsList[instruction].instruction.operationCode ==
                    ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                    returnCount++;
                }
            }
            TEST_ASSERT_TRUE_MESSAGE(returnCount >= 2u,
                                     "fixture prerequisite: partial proof source must retain both returns");
        }
        /* Valid metadata shapes test the writer boundary, not compiler inference. */
        function->hasCallableReturnType = ZR_TRUE;
        init_signature_type_ref(&function->callableReturnType,
                                cases[index].baseType, ZR_STATIC_C_TYPE_DYNAMIC);
        function->callableReturnType.isNullable = cases[index].isNullable;
        function->callableReturnType.isArray = cases[index].isArray;
        function->callableReturnType.ownershipQualifier = cases[index].ownershipQualifier;
        if (cases[index].isArray) {
            function->callableReturnType.elementBaseType = ZR_VALUE_TYPE_INT64;
        }
        snapshot = dynamic_return_representation_snapshot();
        dynamic_return_representation_write(cases[index].caseName);
        if (!dynamic_return_representation_signature_matches(&snapshot.returnType) ||
            !dynamic_return_representation_unchanged(&snapshot)) {
            printf("dynamic return preservation failed: case=%s\n", cases[index].caseName);
            TEST_FAIL_MESSAGE("Unproven or nonplain dynamic returns must retain their semantic C signature fields");
        }
        dynamic_return_representation_cleanup();
    }
}

#endif
