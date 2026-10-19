#include "compiler/module_init_analysis.h"

static const ZrTestsFixtureSource cyclic_return_fixtures[] = {
    {.path = "typed_return_cycle_a", .source =
        "module typed_return_cycle_a; var b = import(\"typed_return_cycle_b\"); "
        "pub fn answer(): int { return 17; } pub fn relay(): int { return b.invoke(); }"},
    {.path = "typed_return_cycle_b", .source =
        "module typed_return_cycle_b; var a = import(\"typed_return_cycle_a\"); "
        "var callback = a.answer; pub fn invoke(): int { return callback(); }"}
};

typedef struct SZrCyclicReturnSite {
    TZrBool found, recordMatches;
    TZrBool returned, invokeFound;
    TZrUInt32 childCount, cacheCount, typedCount, firstKind, firstBinding;
    TZrUInt32 kind, targetToken, signatureToken, relocation;
    TZrUInt64 hash;
} SZrCyclicReturnSite;

typedef struct SZrCyclicReturnContext {
    SZrFunction *(*compileSource)(SZrState *, const TZrChar *, TZrSize, SZrString *);
    TZrSize summaryCalls, runtimeCalls;
    TZrBool summaryDeclarationReady, summaryPrimitive;
    TZrUInt32 summaryOwnership;
    SZrCyclicReturnSite summarySite, runtimeSite;
} SZrCyclicReturnContext;

static SZrCyclicReturnContext *cyclic_return_context = ZR_NULL;

static TZrBool cyclic_return_loader(SZrState *loaderState,
        TZrNativeString path, TZrNativeString hash, SZrIo *io) {
    return ZrTests_Fixture_SourceLoaderFromArray(loaderState, path, hash, io,
            cyclic_return_fixtures, 2U);
}

static void cyclic_return_copy_site(SZrFunction *function, SZrCyclicReturnSite *observed) {
    SZrFunction *owner = ZR_NULL;
    SZrFunctionCallSiteCacheEntry *site = ZR_NULL;
    memset(observed, 0, sizeof(*observed));
    observed->returned = function != ZR_NULL;
    if (function == ZR_NULL) return;
    observed->childCount = function->childFunctionLength;
    for (TZrUInt32 index = 0U; index < function->childFunctionLength; index++) {
        SZrFunction *child = &function->childFunctionList[index];
        if (child->functionName != ZR_NULL &&
                strcmp(ZrCore_String_GetNativeString(child->functionName), "invoke") == 0) {
            observed->invokeFound = ZR_TRUE;
            observed->cacheCount = child->callSiteCacheLength;
            for (TZrUInt32 cacheIndex = 0U; cacheIndex < child->callSiteCacheLength; cacheIndex++) {
                const SZrFunctionCallSiteCacheEntry *cache = &child->callSiteCaches[cacheIndex];
                if (cacheIndex == 0U) observed->firstKind = cache->kind;
                if (cacheIndex == 0U) observed->firstBinding = cache->binding.contract.bindingKind;
                if (cache->binding.contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION) observed->typedCount++;
            }
            site = find_typed(child, &owner);
            break;
        }
    }
    if (site == ZR_NULL || owner == ZR_NULL || owner->functionName == ZR_NULL ||
            strcmp(ZrCore_String_GetNativeString(owner->functionName), "invoke") != 0) return;
    observed->found = ZR_TRUE;
    observed->kind = site->kind;
    observed->targetToken = site->binding.contract.targetMetadataToken;
    observed->signatureToken = site->binding.contract.signatureToken;
    observed->relocation = site->bindingLocation.kind;
    observed->hash = site->binding.contract.signatureHash;
    for (TZrUInt32 index = 0U; index < owner->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &owner->metadataTokenRecords[index];
        if (record->token == observed->signatureToken &&
                ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_SIGNATURE &&
                record->signatureHash == observed->hash) observed->recordMatches = ZR_TRUE;
    }
}

static TZrBool cyclic_return_stack_matches(SZrState *caseState) {
    SZrArray *stack = &caseState->global->importCompileInfoStack;
    if (stack->length != 2U) return ZR_FALSE;
    for (TZrSize index = 0U; index < 2U; index++) {
        SZrString **module = ZrCore_Array_Get(stack, index);
        if (module == ZR_NULL || *module == ZR_NULL ||
                strcmp(ZrCore_String_GetNativeString(*module), cyclic_return_fixtures[index].path) != 0)
            return ZR_FALSE;
    }
    return ZR_TRUE;
}

static SZrFunction *cyclic_return_compile(SZrState *caseState,
        const TZrChar *source, TZrSize length, SZrString *sourceName) {
    SZrCyclicReturnContext *context = cyclic_return_context;
    SZrFunction *function;
    TZrBool isB = length == strlen(cyclic_return_fixtures[1].source) &&
            memcmp(source, cyclic_return_fixtures[1].source, length) == 0;
    TZrBool summaryFrame = isB && cyclic_return_stack_matches(caseState);
    TZrBool runtimeFrame = isB && caseState->global->importCompileInfoStack.length == 0U;
    if (context == ZR_NULL || context->compileSource == ZR_NULL) return ZR_NULL;
    if (summaryFrame) {
        SZrString **module = ZrCore_Array_Get(&caseState->global->importCompileInfoStack, 0U);
        const SZrParserModuleInitSummary *summary =
                ZrParser_ModuleInitAnalysis_FindSummary(caseState->global, *module);
        context->summaryCalls++;
        context->summaryDeclarationReady = context->summaryPrimitive = ZR_FALSE;
        if (summary != ZR_NULL && summary->hasPrescan && !summary->isBinary &&
                summary->state != ZR_PARSER_MODULE_INIT_SUMMARY_FAILED) {
            for (TZrSize index = 0U; index < summary->exports.length; index++) {
                const SZrModuleInitExportInfo *info = ZrCore_Array_Get((SZrArray *)&summary->exports, index);
                if (info == ZR_NULL || info->name == ZR_NULL ||
                        strcmp(ZrCore_String_GetNativeString(info->name), "answer") != 0) continue;
                context->summaryDeclarationReady = info->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION &&
                        info->readiness == ZR_MODULE_EXPORT_READY_DECLARATION;
                context->summaryPrimitive = info->valueType.baseType == ZR_VALUE_TYPE_INT64 &&
                        info->valueType.typeName == ZR_NULL && !info->valueType.isNullable &&
                        !info->valueType.isArray;
                context->summaryOwnership = info->valueType.ownershipQualifier;
            }
        }
    }
    function = context->compileSource(caseState, source, length, sourceName);
    if (summaryFrame) cyclic_return_copy_site(function, &context->summarySite);
    if (runtimeFrame) {
        context->runtimeCalls++;
        cyclic_return_copy_site(function, &context->runtimeSite);
    }
    return function;
}

static void assert_cyclic_return_site(const SZrCyclicReturnSite *site, TZrUInt64 providerHash, const char *diagnostic) {
    TEST_ASSERT_TRUE_MESSAGE(site->found, diagnostic);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_CALLSITE_CACHE_KIND_KNOWN_CALL, site->kind);
    TEST_ASSERT_EQUAL_UINT32(0U, site->targetToken);
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, site->signatureToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_CALL_BINDING_RELOCATION_NONE, site->relocation);
    TEST_ASSERT_TRUE(site->recordMatches);
    TEST_ASSERT_EQUAL_UINT64(providerHash, site->hash);
}

static void test_cyclic_summary_primitive_return_matches_live_typed_call(void) {
    const TZrChar *source =
            "let provider = import(\"typed_return_cycle_a\"); var callback = provider.answer; "
            "fn invoke(): int { return callback(); } "
            "if (provider.relay() != 17) { return -1; } return invoke();";
    SZrCyclicReturnContext context = {0};
    SZrCyclicReturnContext *previousContext = cyclic_return_context;
    SZrCyclicReturnSite consumerSite = {0};
    char diagnostic[512];
    SZrState *caseState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *consumer = ZR_NULL, *provider = ZR_NULL;
    SZrObjectModule *module;
    const SZrTypeValue *providerValue;
    SZrTypeValue result;
    TZrBool (*previousLoader)(SZrState *, TZrNativeString, TZrNativeString, SZrIo *) = ZR_NULL;
    TZrBool stateCreated = caseState != ZR_NULL, hooksInstalled = ZR_FALSE;
    TZrBool consumerIgnored = ZR_FALSE, providerIgnored = ZR_FALSE;
    TZrBool compiled = ZR_FALSE, providerMatchesSummary = ZR_FALSE, executed = ZR_FALSE;
    TZrUInt64 providerHash = 0U;
    EZrValueType resultType = ZR_VALUE_TYPE_NULL;
    TZrInt64 resultValue = 0;
    if (!stateCreated) goto cleanup;
    ZrParser_ToGlobalState_Register(caseState);
    previousLoader = caseState->global->sourceLoader;
    context.compileSource = caseState->global->compileSource;
    cyclic_return_context = &context;
    caseState->global->sourceLoader = cyclic_return_loader;
    ZrCore_GlobalState_SetCompileSource(caseState->global, cyclic_return_compile);
    hooksInstalled = ZR_TRUE;
    consumer = ZrParser_Source_Compile(caseState, source, strlen(source),
            ZrCore_String_CreateFromNative(caseState, "typed_return_cycle_consumer.zr"));
    if (consumer == ZR_NULL || !ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(caseState->global,
            caseState, ZR_CAST_RAW_OBJECT_AS_SUPER(consumer), &consumerIgnored)) goto cleanup;
    compiled = ZR_TRUE;
    cyclic_return_copy_site(consumer, &consumerSite);
    module = ZrCore_Module_ImportByPath(caseState,
            ZrCore_String_CreateFromNative(caseState, "typed_return_cycle_a"));
    if (module == ZR_NULL) goto cleanup;
    providerValue = ZrCore_Module_GetPubExport(caseState, module,
            ZrCore_String_CreateFromNative(caseState, "answer"));
    provider = ZrCore_Closure_GetMetadataFunctionFromValue(caseState, providerValue);
    if (provider == ZR_NULL || !provider->hasCallableReturnType ||
            !ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(caseState->global, caseState,
            ZR_CAST_RAW_OBJECT_AS_SUPER(provider), &providerIgnored)) goto cleanup;
    providerHash = ZrCore_CallBinding_FunctionSignatureHash(provider);
    providerMatchesSummary = context.summaryPrimitive &&
            provider->callableReturnType.baseType == ZR_VALUE_TYPE_INT64 &&
            provider->callableReturnType.typeName == ZR_NULL && !provider->callableReturnType.isNullable &&
            !provider->callableReturnType.isArray &&
            provider->callableReturnType.ownershipQualifier == context.summaryOwnership;
    executed = ZrTests_Runtime_Function_ExecuteCaptureFailure(caseState, consumer, &result);
    resultType = result.type;
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(result.type)) resultValue = result.value.nativeObject.nativeInt64;
cleanup:
    if (hooksInstalled) {
        caseState->global->sourceLoader = previousLoader;
        ZrCore_GlobalState_SetCompileSource(caseState->global, context.compileSource);
    }
    cyclic_return_context = previousContext;
    if (consumerIgnored)
        ZrCore_GarbageCollector_UnignoreObject(caseState->global, ZR_CAST_RAW_OBJECT_AS_SUPER(consumer));
    if (consumer != ZR_NULL) ZrCore_Function_Free(caseState, consumer);
    if (providerIgnored)
        ZrCore_GarbageCollector_UnignoreObject(caseState->global, ZR_CAST_RAW_OBJECT_AS_SUPER(provider));
    ZrTests_Runtime_State_Destroy(caseState);
    snprintf(diagnostic, sizeof(diagnostic),
            "returned/invoke/children/caches/typed/first-kind/first-binding summary=%u/%u/%u/%u/%u/%u/%u; runtime=%u/%u/%u/%u/%u/%u/%u; consumer=%u/%u/%u/%u/%u/%u/%u; executed=%u result=%lld",
            (unsigned)context.summarySite.returned, (unsigned)context.summarySite.invokeFound, context.summarySite.childCount, context.summarySite.cacheCount, context.summarySite.typedCount, context.summarySite.firstKind, context.summarySite.firstBinding,
            (unsigned)context.runtimeSite.returned, (unsigned)context.runtimeSite.invokeFound, context.runtimeSite.childCount, context.runtimeSite.cacheCount, context.runtimeSite.typedCount, context.runtimeSite.firstKind, context.runtimeSite.firstBinding,
            (unsigned)consumerSite.returned, (unsigned)consumerSite.invokeFound, consumerSite.childCount, consumerSite.cacheCount, consumerSite.typedCount, consumerSite.firstKind, consumerSite.firstBinding,
            (unsigned)executed, (long long)resultValue);
    TEST_ASSERT_TRUE(stateCreated);
    TEST_ASSERT_TRUE(compiled);
    TEST_ASSERT_TRUE(context.summaryCalls > 0U);
    TEST_ASSERT_TRUE(context.runtimeCalls > 0U);
    TEST_ASSERT_TRUE(context.summaryDeclarationReady);
    TEST_ASSERT_TRUE(providerMatchesSummary);
    assert_cyclic_return_site(&context.summarySite, providerHash, diagnostic);
    assert_cyclic_return_site(&context.runtimeSite, providerHash, diagnostic);
    assert_cyclic_return_site(&consumerSite, providerHash, diagnostic);
    TEST_ASSERT_TRUE(executed);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, resultType);
    TEST_ASSERT_EQUAL_INT64(17, resultValue);
}
