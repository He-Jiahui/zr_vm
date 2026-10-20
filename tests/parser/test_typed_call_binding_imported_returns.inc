#include "zr_vm_core/gc.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/parser.h"
#include "compiler/compiler_internal.h"

#include <errno.h>

static const ZrTestsFixtureSource imported_return_fixtures[] = {
    {.path = "typed_return_primitive", .source =
        "module typed_return_primitive; pub fn answer(): int { return 17; }"},
    {.path = "typed_return_nominal", .source =
        "module typed_return_nominal; pub class Token { pub @constructor() {} } "
        "pub fn keep(value: Token): Token { return value; }"},
    {.path = "typed_return_generic", .source =
        "module typed_return_generic; pub class Box<T> { pub @constructor() {} } "
        "pub fn keep(value: Box<u8>): Box<u8> { return value; }"},
    {.path = "typed_return_width", .source =
        "module typed_return_width; pub fn narrow(): u8 { return 7; }"}
};

typedef struct SZrImportedReturnObservation {
    TZrBool stateCreated;
    TZrBool providerFound;
    TZrBool parsed;
    TZrBool compiled;
    TZrBool memberFound;
    TZrBool aliasFound;
    TZrBool returnMatchesProvider;
    TZrBool canonicalFunctionFound;
    TZrBool providerHasTypeName;
    TZrBool providerIsNullable;
    TZrBool providerIsArray;
    EZrOwnershipQualifier providerOwnership;
    EZrValueType providerBaseType;
    EZrValueType aliasBaseType;
    TZrUInt32 returnKind;
    TZrUInt32 genericDefinitionKind;
    TZrSize genericArgumentCount;
    TZrUInt32 genericArgumentKind;
    EZrValueType genericArgumentValueType;
    TZrChar providerTypeName[96];
    TZrChar aliasTypeName[96];
    TZrChar displayTypeName[96];
    TZrChar compileError[256];
    TZrUInt64 providerHash;
    TZrBool runtimeCompiled;
    TZrBool runtimeExecuted;
    TZrBool siteFound;
    TZrBool signatureRecordMatches;
    TZrUInt32 siteKind;
    TZrUInt32 siteTargetToken;
    TZrUInt32 siteSignatureToken;
    TZrUInt32 siteRelocation;
    TZrUInt64 siteHash;
    EZrValueType resultType;
    TZrInt64 result;
    SZrCallBindingDiagnostic runtimeDiagnostic;
    TZrSize binaryCompileInfoLoads;
} SZrImportedReturnObservation;

typedef struct SZrImportedReturnBinaryFixture {
    ZrTestsFixtureSource fixture;
    TZrSize compileInfoLoads;
} SZrImportedReturnBinaryFixture;

static SZrImportedReturnBinaryFixture *imported_return_binary_fixture = ZR_NULL;

static TZrBool imported_return_binary_loader(SZrState *loaderState,
        TZrNativeString path, TZrNativeString hash, SZrIo *io) {
    SZrImportedReturnBinaryFixture *context = imported_return_binary_fixture;
    SZrString **stackModule;
    const TZrChar *stackModuleName;
    TZrBool loaded;

    if (context == ZR_NULL || context->fixture.source != ZR_NULL ||
            context->fixture.bytes == ZR_NULL || context->fixture.length == 0U ||
            !context->fixture.isBinary) return ZR_FALSE;
    loaded = ZrTests_Fixture_SourceLoaderFromArray(loaderState, path, hash, io,
            &context->fixture, 1U);
    if (!loaded || !io->isBinary ||
            loaderState->global->importCompileInfoStack.length == 0U) return loaded;
    stackModule = ZrCore_Array_Get(&loaderState->global->importCompileInfoStack,
            loaderState->global->importCompileInfoStack.length - 1U);
    if (stackModule == ZR_NULL || *stackModule == ZR_NULL) return loaded;
    stackModuleName = ZrCore_String_GetNativeString(*stackModule);
    if (stackModuleName != ZR_NULL && strcmp(stackModuleName, context->fixture.path) == 0)
        context->compileInfoLoads++;
    return loaded;
}

static TZrBool imported_return_source_loader(SZrState *loaderState,
        TZrNativeString path, TZrNativeString hash, SZrIo *io) {
    return ZrTests_Fixture_SourceLoaderFromArray(loaderState, path, hash, io,
            imported_return_fixtures,
            sizeof(imported_return_fixtures) / sizeof(imported_return_fixtures[0]));
}

static void imported_return_copy_name(TZrChar *target, TZrSize capacity,
        const SZrString *name) {
    snprintf(target, capacity, "%s", name != ZR_NULL
            ? ZrCore_String_GetNativeString(name) : "");
}

static SZrImportedReturnObservation observe_imported_return_fixture(
        const ZrTestsFixtureSource *fixture,
        TZrBool (*sourceLoader)(SZrState *, TZrNativeString, TZrNativeString, SZrIo *),
        const TZrSize *binaryCompileInfoLoads,
        const TZrChar *source, const TZrChar *memberName, TZrBool execute) {
    SZrImportedReturnObservation observed = {0};
    SZrState *caseState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrCompilerState compiler = {0};
    SZrAstNode *ast = ZR_NULL;
    SZrFunction *compilerRoot = ZR_NULL;
    SZrFunction *provider = ZR_NULL;
    SZrFunction *runtimeRoot = ZR_NULL;
    TZrBool compilerInitialized = ZR_FALSE;
    TZrBool compilerRootIgnored = ZR_FALSE;
    TZrBool providerIgnored = ZR_FALSE;
    TZrBool runtimeRootIgnored = ZR_FALSE;
    TZrBool (*previousLoader)(SZrState *, TZrNativeString,
            TZrNativeString, SZrIo *) = ZR_NULL;
    TZrSize binaryLoadsBeforeCompile = 0U;
    SZrObjectModule *module;
    const SZrTypeValue *providerValue;
    SZrString *sourceName;

    observed.returnKind = UINT32_MAX;
    observed.genericDefinitionKind = UINT32_MAX;
    observed.genericArgumentKind = UINT32_MAX;
    if (caseState == ZR_NULL) return observed;
    observed.stateCreated = ZR_TRUE;
    previousLoader = caseState->global->sourceLoader;
    caseState->global->sourceLoader = sourceLoader;
    ZrParser_ToGlobalState_Register(caseState);
    module = ZrCore_Module_ImportByPath(caseState,
            ZrCore_String_CreateFromNative(caseState, (TZrNativeString)fixture->path));
    if (module == ZR_NULL) goto cleanup;
    providerValue = ZrCore_Module_GetPubExport(caseState, module,
            ZrCore_String_CreateFromNative(caseState, (TZrNativeString)memberName));
    provider = ZrCore_Closure_GetMetadataFunctionFromValue(caseState, providerValue);
    if (provider == ZR_NULL || !provider->hasCallableReturnType ||
            !ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
            caseState->global, caseState, ZR_CAST_RAW_OBJECT_AS_SUPER(provider),
            &providerIgnored)) goto cleanup;
    observed.providerFound = ZR_TRUE;
    observed.providerBaseType = provider->callableReturnType.baseType;
    observed.providerHasTypeName = provider->callableReturnType.typeName != ZR_NULL;
    observed.providerIsNullable = provider->callableReturnType.isNullable;
    observed.providerIsArray = provider->callableReturnType.isArray;
    observed.providerOwnership = (EZrOwnershipQualifier)provider->callableReturnType.ownershipQualifier;
    imported_return_copy_name(observed.providerTypeName,
            sizeof(observed.providerTypeName), provider->callableReturnType.typeName);
    observed.providerHash = ZrCore_CallBinding_FunctionSignatureHash(provider);

    sourceName = ZrCore_String_CreateFromNative(caseState, "imported_return_consumer.zr");
    if (sourceName == ZR_NULL) goto cleanup;
    ast = ZrParser_Parse(caseState, source, strlen(source), sourceName);
    if (ast == ZR_NULL) goto cleanup;
    observed.parsed = ZR_TRUE;
    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, caseState);
    compilerInitialized = ZR_TRUE;
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = ast;
    compiler.currentFunction = ZrCore_Function_New(caseState);
    compilerRoot = compiler.currentFunction;
    if (compilerRoot == ZR_NULL || !ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
            caseState->global, caseState, ZR_CAST_RAW_OBJECT_AS_SUPER(compilerRoot),
            &compilerRootIgnored)) goto cleanup;
    if (binaryCompileInfoLoads != ZR_NULL) binaryLoadsBeforeCompile = *binaryCompileInfoLoads;
    compile_script(&compiler, ast);
    if (binaryCompileInfoLoads != ZR_NULL)
        observed.binaryCompileInfoLoads = *binaryCompileInfoLoads - binaryLoadsBeforeCompile;
    if (compiler.errorMessage != ZR_NULL)
        snprintf(observed.compileError, sizeof(observed.compileError), "%s",
                compiler.errorMessage);
    observed.compiled = !compiler.hasError;
    if (!observed.compiled) goto cleanup;

    for (TZrSize index = 0U; index < compiler.typePrototypes.length; index++) {
        SZrTypePrototypeInfo *prototype = ZrCore_Array_Get(&compiler.typePrototypes, index);
        if (prototype == ZR_NULL || prototype->name == ZR_NULL ||
                prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE ||
                strcmp(ZrCore_String_GetNativeString(prototype->name), fixture->path) != 0)
            continue;
        for (TZrSize memberIndex = 0U; memberIndex < prototype->members.length; memberIndex++) {
            SZrTypeMemberInfo *member = ZrCore_Array_Get(&prototype->members, memberIndex);
            if (member != ZR_NULL && member->name != ZR_NULL &&
                    strcmp(ZrCore_String_GetNativeString(member->name), memberName) == 0) {
                observed.memberFound = ZR_TRUE;
                imported_return_copy_name(observed.displayTypeName,
                        sizeof(observed.displayTypeName), member->returnTypeName);
                break;
            }
        }
    }
    for (SZrTypeEnvironment *environment = compiler.typeEnv;
            environment != ZR_NULL; environment = environment->parent) {
        for (TZrSize index = 0U; index < environment->functionReturnTypes.length; index++) {
            SZrFunctionTypeInfo **entry = ZrCore_Array_Get(&environment->functionReturnTypes, index);
            SZrFunctionTypeInfo *alias = entry != ZR_NULL ? *entry : ZR_NULL;
            const SZrCanonicalTypeNode *callable;
            const SZrCanonicalTypeNode *returnType;
            if (alias == ZR_NULL || !alias->isCallableValueBinding || alias->name == ZR_NULL ||
                    strcmp(ZrCore_String_GetNativeString(alias->name), "callback") != 0)
                continue;
            observed.aliasFound = ZR_TRUE;
            observed.aliasBaseType = alias->returnType.baseType;
            imported_return_copy_name(observed.aliasTypeName,
                    sizeof(observed.aliasTypeName), alias->returnType.typeName);
            observed.returnMatchesProvider = !observed.providerIsArray &&
                    alias->returnType.baseType == observed.providerBaseType &&
                    alias->returnType.isNullable == observed.providerIsNullable &&
                    alias->returnType.ownershipQualifier == observed.providerOwnership &&
                    (alias->returnType.typeName != ZR_NULL) == observed.providerHasTypeName &&
                    strcmp(observed.aliasTypeName, observed.providerTypeName) == 0;
            callable = ZrParser_CanonicalType_Find(compiler.semanticContext, alias->typeId);
            if (callable == ZR_NULL || callable->kind != ZR_CANONICAL_TYPE_FUNCTION) continue;
            observed.canonicalFunctionFound = ZR_TRUE;
            returnType = ZrParser_CanonicalType_Find(compiler.semanticContext,
                    callable->data.function.returnTypeId);
            if (returnType == ZR_NULL) continue;
            observed.returnKind = returnType->kind;
            if (returnType->kind == ZR_CANONICAL_TYPE_GENERIC_INSTANCE) {
                const SZrCanonicalTypeNode *definition = ZrParser_CanonicalType_Find(
                        compiler.semanticContext, returnType->data.genericInstance.definitionTypeId);
                const SZrCanonicalGenericArgument *argument;
                if (definition != ZR_NULL) observed.genericDefinitionKind = definition->kind;
                observed.genericArgumentCount = returnType->data.genericInstance.arguments.length;
                argument = ZrCore_Array_Get((SZrArray *)&returnType->data.genericInstance.arguments, 0U);
                if (argument != ZR_NULL) {
                    observed.genericArgumentKind = argument->kind;
                    if (argument->kind == ZR_CANONICAL_GENERIC_ARGUMENT_TYPE) {
                        const SZrCanonicalTypeNode *argumentType = ZrParser_CanonicalType_Find(
                                compiler.semanticContext, argument->data.typeId);
                        if (argumentType != ZR_NULL && argumentType->kind == ZR_CANONICAL_TYPE_PRIMITIVE)
                            observed.genericArgumentValueType = argumentType->data.primitive.valueType;
                    }
                }
            }
        }
    }

    if (execute) {
        SZrFunction *owner = ZR_NULL;
        SZrFunctionCallSiteCacheEntry *site;
        SZrTypeValue result;
        runtimeRoot = ZrParser_Source_Compile(caseState, source, strlen(source),
                ZrCore_String_CreateFromNative(caseState, "imported_return_runtime.zr"));
        if (runtimeRoot == ZR_NULL || !ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
                caseState->global, caseState, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeRoot),
                &runtimeRootIgnored)) goto cleanup;
        observed.runtimeCompiled = ZR_TRUE;
        site = find_typed(runtimeRoot, &owner);
        if (site != ZR_NULL) {
            observed.siteFound = ZR_TRUE;
            observed.siteKind = site->kind;
            observed.siteTargetToken = site->binding.contract.targetMetadataToken;
            observed.siteSignatureToken = site->binding.contract.signatureToken;
            observed.siteRelocation = site->bindingLocation.kind;
            observed.siteHash = site->binding.contract.signatureHash;
            for (TZrUInt32 index = 0U; index < owner->metadataTokenRecordLength; index++) {
                const SZrMetadataTokenRecord *record = &owner->metadataTokenRecords[index];
                if (record->token == site->binding.contract.signatureToken &&
                        ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_SIGNATURE &&
                        record->signatureHash == observed.siteHash)
                    observed.signatureRecordMatches = ZR_TRUE;
            }
        }
        observed.runtimeExecuted = ZrTests_Runtime_Function_ExecuteCaptureFailure(
                caseState, runtimeRoot, &result);
        observed.runtimeDiagnostic = caseState->lastCallBindingError;
        observed.resultType = result.type;
        if (ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)) observed.result = result.value.nativeObject.nativeInt64;
    }

cleanup:
    caseState->global->sourceLoader = previousLoader;
    if (runtimeRootIgnored)
        ZrCore_GarbageCollector_UnignoreObject(caseState->global, ZR_CAST_RAW_OBJECT_AS_SUPER(runtimeRoot));
    if (runtimeRoot != ZR_NULL) ZrCore_Function_Free(caseState, runtimeRoot);
    if (compilerInitialized) {
        SZrFunction *currentCompilerRoot = compiler.currentFunction;
        SZrFunction *topLevelCompilerRoot = compiler.topLevelFunction;
        if (compilerRootIgnored)
            ZrCore_GarbageCollector_UnignoreObject(caseState->global, ZR_CAST_RAW_OBJECT_AS_SUPER(compilerRoot));
        if (compilerRoot != ZR_NULL) ZrCore_Function_Free(caseState, compilerRoot);
        if (currentCompilerRoot != ZR_NULL && currentCompilerRoot != compilerRoot)
            ZrCore_Function_Free(caseState, currentCompilerRoot);
        if (topLevelCompilerRoot != ZR_NULL && topLevelCompilerRoot != compilerRoot &&
                topLevelCompilerRoot != currentCompilerRoot)
            ZrCore_Function_Free(caseState, topLevelCompilerRoot);
        ZrParser_CompilerState_Free(&compiler);
    }
    if (ast != ZR_NULL) ZrParser_Ast_Free(caseState, ast);
    if (providerIgnored)
        ZrCore_GarbageCollector_UnignoreObject(caseState->global, ZR_CAST_RAW_OBJECT_AS_SUPER(provider));
    ZrTests_Runtime_State_Destroy(caseState);
    return observed;
}

static SZrImportedReturnObservation observe_imported_return(TZrSize fixtureIndex,
        const TZrChar *source, const TZrChar *memberName, TZrBool execute) {
    return observe_imported_return_fixture(&imported_return_fixtures[fixtureIndex],
            imported_return_source_loader, ZR_NULL, source, memberName, execute);
}

static void assert_imported_return_observed(const SZrImportedReturnObservation *observed) {
    TEST_ASSERT_TRUE(observed->stateCreated);
    TEST_ASSERT_TRUE(observed->providerFound);
    TEST_ASSERT_TRUE(observed->parsed);
    TEST_ASSERT_TRUE_MESSAGE(observed->compiled, observed->compileError);
    TEST_ASSERT_TRUE(observed->memberFound);
    TEST_ASSERT_TRUE(observed->aliasFound);
    TEST_ASSERT_TRUE(observed->canonicalFunctionFound);
}

static void test_imported_primitive_return_matches_live_typed_call(void) {
    const SZrImportedReturnObservation observed = observe_imported_return(0U,
            "let provider = import(\"typed_return_primitive\"); var callback = provider.answer; "
            "fn invoke(): int { return callback(); } return invoke();", "answer", ZR_TRUE);
    TZrChar failure[192];
    assert_imported_return_observed(&observed);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, observed.providerBaseType);
    TEST_ASSERT_FALSE(observed.providerHasTypeName);
    TEST_ASSERT_EQUAL_STRING("int", observed.displayTypeName);
    TEST_ASSERT_TRUE(observed.runtimeCompiled);
    TEST_ASSERT_TRUE(observed.siteFound);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL, observed.siteKind);
    TEST_ASSERT_EQUAL_UINT32(0U, observed.siteTargetToken);
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, observed.siteSignatureToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_RELOCATION_NONE, observed.siteRelocation);
    TEST_ASSERT_TRUE(observed.signatureRecordMatches);
    snprintf(failure, sizeof(failure), "actual VM executed=%u binding=%u expected=%llu actual=%llu",
            (unsigned int)observed.runtimeExecuted, (unsigned int)observed.runtimeDiagnostic.status,
            (unsigned long long)observed.runtimeDiagnostic.expected,
            (unsigned long long)observed.runtimeDiagnostic.actual);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(observed.providerHash, observed.siteHash, failure);
    TEST_ASSERT_TRUE(observed.returnMatchesProvider);
    TEST_ASSERT_TRUE_MESSAGE(observed.runtimeExecuted, failure);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, observed.resultType);
    TEST_ASSERT_EQUAL_INT64(17, observed.result);
}

static void test_imported_nominal_return_keeps_provider_identity(void) {
    const SZrImportedReturnObservation observed = observe_imported_return(1U,
            "let provider = import(\"typed_return_nominal\"); var callback = provider.keep;",
            "keep", ZR_FALSE);
    assert_imported_return_observed(&observed);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, observed.providerBaseType);
    TEST_ASSERT_TRUE(observed.providerHasTypeName);
    TEST_ASSERT_EQUAL_STRING("Token", observed.providerTypeName);
    TEST_ASSERT_TRUE(observed.returnMatchesProvider);
    TEST_ASSERT_EQUAL_STRING(observed.providerTypeName, observed.displayTypeName);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_NOMINAL, observed.returnKind);
}

static void test_imported_closed_generic_return_keeps_canonical_arguments(void) {
    const SZrImportedReturnObservation observed = observe_imported_return(2U,
            "let provider = import(\"typed_return_generic\"); var callback = provider.keep;",
            "keep", ZR_FALSE);
    assert_imported_return_observed(&observed);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, observed.providerBaseType);
    TEST_ASSERT_TRUE(observed.providerHasTypeName);
    TEST_ASSERT_TRUE(observed.returnMatchesProvider);
    TEST_ASSERT_EQUAL_STRING(observed.providerTypeName, observed.displayTypeName);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_GENERIC_INSTANCE, observed.returnKind);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_NOMINAL, observed.genericDefinitionKind);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)observed.genericArgumentCount);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_GENERIC_ARGUMENT_TYPE, observed.genericArgumentKind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT8, observed.genericArgumentValueType);
}

static void test_imported_width_return_keeps_provider_spelling(void) {
    const SZrImportedReturnObservation observed = observe_imported_return(3U,
            "let provider = import(\"typed_return_width\"); var callback = provider.narrow;",
            "narrow", ZR_FALSE);
    assert_imported_return_observed(&observed);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT8, observed.providerBaseType);
    TEST_ASSERT_TRUE(observed.providerHasTypeName);
    TEST_ASSERT_EQUAL_STRING("u8", observed.providerTypeName);
    TEST_ASSERT_TRUE(observed.returnMatchesProvider);
    TEST_ASSERT_EQUAL_STRING(observed.providerTypeName, observed.displayTypeName);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_PRIMITIVE, observed.returnKind);
}

static void test_imported_binary_primitive_return_matches_live_typed_call(void) {
    const TZrChar *providerSource =
            "module typed_return_binary_primitive; pub fn answer(): int { return 17; }";
    const TZrChar *source =
            "let provider = import(\"typed_return_binary_primitive\"); var callback = provider.answer; "
            "fn invoke(): int { return callback(); } return invoke();";
    SZrImportedReturnObservation observed = {0};
    SZrImportedReturnBinaryFixture context = {0};
    SZrImportedReturnBinaryFixture *previousFixture = imported_return_binary_fixture;
    SZrState *builder = ZR_NULL;
    TZrByte *binaryBytes = ZR_NULL;
    TZrChar binaryPath[512] = {0};
    TZrChar failure[192];
    FILE *existingFile;
    TZrBool pathValid;
    TZrBool pathAbsent = ZR_FALSE;
    TZrBool buildAttempted = ZR_FALSE;
    TZrBool binaryBuilt = ZR_FALSE;
    TZrBool binaryRemoved = ZR_TRUE;
    int pathLength = snprintf(binaryPath, sizeof(binaryPath),
            "%s/typed_call_binding_imported_return_provider.zro", ZR_VM_TESTS_BINARY_DIR);

    pathValid = pathLength >= 0 && (TZrSize)pathLength < sizeof(binaryPath);
    if (!pathValid) goto cleanup;
    errno = 0;
    existingFile = fopen(binaryPath, "rb");
    if (existingFile != ZR_NULL) {
        fclose(existingFile);
        goto cleanup;
    }
    if (errno != ENOENT) goto cleanup;
    pathAbsent = ZR_TRUE;
    builder = ZrTests_Runtime_State_Create(ZR_NULL);
    if (builder == ZR_NULL) goto cleanup;
    ZrParser_ToGlobalState_Register(builder);
    buildAttempted = ZR_TRUE;
    binaryBytes = ZrTests_Fixture_BuildBinaryFile(builder, providerSource, binaryPath,
            ZR_FALSE, &context.fixture.length);
    ZrTests_Runtime_State_Destroy(builder);
    builder = ZR_NULL;
    binaryBuilt = binaryBytes != ZR_NULL && context.fixture.length > 0U;
    if (!binaryBuilt) goto cleanup;
    context.fixture.path = "typed_return_binary_primitive";
    context.fixture.bytes = binaryBytes;
    context.fixture.isBinary = ZR_TRUE;
    imported_return_binary_fixture = &context;
    observed = observe_imported_return_fixture(&context.fixture, imported_return_binary_loader,
            &context.compileInfoLoads, source, "answer", ZR_TRUE);

cleanup:
    imported_return_binary_fixture = previousFixture;
    ZrTests_Runtime_State_Destroy(builder);
    free(binaryBytes);
    if (buildAttempted) {
        errno = 0;
        binaryRemoved = remove(binaryPath) == 0 || errno == ENOENT;
    }
    TEST_ASSERT_TRUE_MESSAGE(pathValid, "binary provider path exceeds capacity");
    TEST_ASSERT_TRUE_MESSAGE(pathAbsent, "binary provider path must not pre-exist");
    TEST_ASSERT_TRUE_MESSAGE(binaryBuilt, "binary provider fixture build failed");
    TEST_ASSERT_TRUE_MESSAGE(binaryRemoved, "binary provider fixture cleanup failed");
    assert_imported_return_observed(&observed);
    TEST_ASSERT_TRUE_MESSAGE(observed.binaryCompileInfoLoads > 0U,
            "manual compile did not load binary provider metadata on the import stack");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, observed.providerBaseType);
    TEST_ASSERT_FALSE(observed.providerHasTypeName);
    TEST_ASSERT_EQUAL_STRING("int", observed.displayTypeName);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_PRIMITIVE, observed.returnKind);
    TEST_ASSERT_TRUE(observed.runtimeCompiled);
    TEST_ASSERT_TRUE(observed.siteFound);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL, observed.siteKind);
    TEST_ASSERT_EQUAL_UINT32(0U, observed.siteTargetToken);
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, observed.siteSignatureToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_RELOCATION_NONE, observed.siteRelocation);
    TEST_ASSERT_TRUE(observed.signatureRecordMatches);
    snprintf(failure, sizeof(failure), "actual VM executed=%u binding=%u expected=%llu actual=%llu",
            (unsigned int)observed.runtimeExecuted, (unsigned int)observed.runtimeDiagnostic.status,
            (unsigned long long)observed.runtimeDiagnostic.expected,
            (unsigned long long)observed.runtimeDiagnostic.actual);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(observed.providerHash, observed.siteHash, failure);
    TEST_ASSERT_TRUE(observed.returnMatchesProvider);
    TEST_ASSERT_TRUE_MESSAGE(observed.runtimeExecuted, failure);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, observed.resultType);
    TEST_ASSERT_EQUAL_INT64(17, observed.result);
}
