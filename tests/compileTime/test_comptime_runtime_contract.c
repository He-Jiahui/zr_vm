#include "unity.h"

#include <string.h>

#include "runtime_support.h"
#include "compile_tool_binding.h"
#include "compile_time_declaration_patch_diagnostics.h"
#include "compile_time_executor_internal.h"
#include "comptime_runtime_contract.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;

typedef struct STestLogCapture {
    TZrBool called;
    EZrLogLevel level;
    EZrOutputChannel channel;
    EZrOutputKind kind;
    TZrChar message[256];
} STestLogCapture;

static STestLogCapture g_logCapture;

static void capture_log(
        SZrState *state,
        EZrLogLevel level,
        EZrOutputChannel channel,
        EZrOutputKind kind,
        TZrNativeString message) {
    ZR_UNUSED_PARAMETER(state);

    g_logCapture.called = ZR_TRUE;
    g_logCapture.level = level;
    g_logCapture.channel = channel;
    g_logCapture.kind = kind;
    strncpy(
            g_logCapture.message,
            message != ZR_NULL ? message : "",
            sizeof(g_logCapture.message) - 1U);
    g_logCapture.message[sizeof(g_logCapture.message) - 1U] = '\0';
}

void setUp(void) {
    memset(&g_logCapture, 0, sizeof(g_logCapture));
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        g_state->global->logFunction = ZR_NULL;
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrAstNode *parse_source(const TZrChar *source, const TZrChar *sourceNameText) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)sourceNameText);
    SZrAstNode *ast = ZrParser_Parse(
            g_state, source, strlen(source), sourceName);

    TEST_ASSERT_NOT_NULL(ast);
    return ast;
}

static TZrBool prepare_and_run_late_checks(
        SZrCompilerState *compiler,
        SZrAstNode *ast) {
    if (!ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
                compiler, ast)) {
        return ZR_FALSE;
    }
    compiler->compilePhase = ZR_PARSER_COMPILE_PHASE_LATE_CHECK;
    return ZrParser_CompileTime_ExecuteLateChecksInCompilerState(
            compiler, ast);
}

static void set_object_field(
        SZrObject *object,
        const TZrChar *name,
        const SZrTypeValue *value) {
    SZrString *keyString = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)name);
    SZrTypeValue key;

    TEST_ASSERT_NOT_NULL(keyString);
    ZrCore_Value_InitAsRawObject(
            g_state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(g_state, object, &key, value);
}

static SZrObject *make_patch_diagnostic(
        TZrBool isError,
        const TZrChar *message,
        TZrSymbolId targetSymbolId) {
    SZrObject *diagnostic = ZrCore_Object_New(g_state, ZR_NULL);
    SZrString *messageString;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(diagnostic);
    ZrCore_Object_Init(g_state, diagnostic);

    ZrCore_Value_InitAsInt(
            g_state, &value, ZR_PARSER_COMPILE_TOOL_TYPE_DIAGNOSTIC);
    set_object_field(diagnostic, "__zrCompileToolTypeRole", &value);
    ZrCore_Value_InitAsBool(g_state, &value, isError);
    set_object_field(diagnostic, "isError", &value);
    messageString = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)message);
    TEST_ASSERT_NOT_NULL(messageString);
    ZrCore_Value_InitAsRawObject(
            g_state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(messageString));
    value.type = ZR_VALUE_TYPE_STRING;
    set_object_field(diagnostic, "message", &value);
    ZrCore_Value_InitAsInt(g_state, &value, (TZrInt64)targetSymbolId);
    set_object_field(diagnostic, "target", &value);
    return diagnostic;
}

static void init_object_value(SZrTypeValue *value, SZrObject *object) {
    ZrCore_Value_InitAsRawObject(
            g_state, value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    value->type = ZR_VALUE_TYPE_OBJECT;
}

static void init_array_value_with_repeated_entry(
        SZrTypeValue *arrayValue,
        const SZrTypeValue *entry,
        TZrSize count) {
    SZrObject *array = ZrCore_Object_NewCustomized(
            g_state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    SZrTypeValue key;

    TEST_ASSERT_NOT_NULL(array);
    ZrCore_Object_Init(g_state, array);
    for (TZrSize index = 0; index < count; index++) {
        ZrCore_Value_InitAsInt(g_state, &key, (TZrInt64)index);
        ZrCore_Object_SetValue(g_state, array, &key, entry);
    }
    ZrCore_Value_InitAsRawObject(
            g_state, arrayValue, ZR_CAST_RAW_OBJECT_AS_SUPER(array));
    arrayValue->type = ZR_VALUE_TYPE_ARRAY;
}

static void test_typed_compile_tool_assert_and_warning_run_in_check_context(void) {
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "comptime {\n"
            "    compile.assert(true, \"layout ok\");\n"
            "    compile.warning(\"alignment warning\");\n"
            "}\n"
            "return 42;\n";
    SZrAstNode *ast = parse_source(source, "comptime_typed_diagnostics.zr");
    SZrCompilerState compiler;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(prepare_and_run_late_checks(&compiler, ast));
    TEST_ASSERT_FALSE(compiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_UINT64(
            1U, compiler.comptimeBudget.usage.diagnosticCount);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_compile_tool_error_stops_build_with_typed_diagnostic(void) {
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "comptime { compile.error(\"schema mismatch\"); }\n"
            "return 0;\n";
    SZrAstNode *ast = parse_source(source, "comptime_typed_error.zr");
    SZrCompilerState compiler;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_FALSE(prepare_and_run_late_checks(&compiler, ast));
    TEST_ASSERT_TRUE(compiler.hasCompileTimeError);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(compiler.errorMessage, "schema mismatch"));
    TEST_ASSERT_EQUAL_UINT64(
            1U, compiler.comptimeBudget.usage.diagnosticCount);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_diagnostic_effect_is_rejected_in_pure_build_predicate(void) {
    static const TZrChar *source =
            "let compile = import(\"zr.compile\");\n"
            "comptime if (compile.assert(true, \"not pure\")) {\n"
            "    fn selected(): int { return 1; }\n"
            "}\n"
            "return 0;\n";
    SZrAstNode *ast = parse_source(source, "comptime_effect_violation.zr");
    SZrCompilerState compiler;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_FALSE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &compiler, ast));
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(
            compiler.errorMessage, "comptime.effect_violation"));

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_fuel_budget_stops_evaluation_without_partial_overrun(void) {
    static const TZrChar *source =
            "comptime if ((((1 + 2) + 3) + 4) > 0) {\n"
            "    fn selected(): int { return 1; }\n"
            "}\n"
            "return 0;\n";
    SZrAstNode *ast = parse_source(source, "comptime_fuel_budget.zr");
    SZrCompilerState compiler;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.comptimeBudget.limits.fuel = 4U;
    TEST_ASSERT_FALSE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &compiler, ast));
    TEST_ASSERT_EQUAL_UINT64(4U, compiler.comptimeBudget.usage.fuel);
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPTIME_BUDGET_FUEL,
            compiler.comptimeBudget.exceededResource);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(
            compiler.errorMessage, "comptime.budget_exceeded"));

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_call_depth_budget_stops_recursive_comptime_function(void) {
    static const TZrChar *source =
            "comptime fn descend(value: int): int {\n"
            "    if (value == 0) { return 0; }\n"
            "    return descend(value - 1);\n"
            "}\n"
            "comptime { let result = descend(8); }\n"
            "return 0;\n";
    SZrAstNode *ast = parse_source(source, "comptime_call_depth.zr");
    SZrCompilerState compiler;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.comptimeBudget.limits.callDepth = 3U;
    TEST_ASSERT_FALSE(prepare_and_run_late_checks(&compiler, ast));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPTIME_BUDGET_CALL_DEPTH,
            compiler.comptimeBudget.exceededResource);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(
            compiler.errorMessage, "comptime.budget_exceeded"));

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_pure_comptime_function_results_use_deterministic_cache(void) {
    static const TZrChar *source =
            "comptime fn add(left: int, right: int): int {\n"
            "    return left + right;\n"
            "}\n"
            "comptime {\n"
            "    let first = add(20, 22);\n"
            "    let second = add(20, 22);\n"
            "    let third = add(21, 21);\n"
            "}\n"
            "return 0;\n";
    SZrAstNode *ast = parse_source(source, "comptime_deterministic_cache.zr");
    SZrCompilerState compiler;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE(prepare_and_run_late_checks(&compiler, ast));
    TEST_ASSERT_EQUAL_UINT64(1U, compiler.comptimeCacheHitCount);
    TEST_ASSERT_EQUAL_UINT64(2U, compiler.comptimeCacheMissCount);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_comptime_cache_key_includes_compile_tool_provider_contract(void) {
    const SZrParserCompileToolModuleDescriptor *builtin =
            ZrParser_CompileTool_FindModule(ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
    SZrParserCompileToolModuleDescriptor baselineProvider;
    SZrParserCompileToolModuleDescriptor publishedHashProvider;
    SZrParserCompileToolModuleDescriptor computedHashProvider;
    SZrCompileTimeFunction function = {0};
    SZrCompilerState compiler;
    SZrString *alias;
    TZrUInt64 baselineKey;
    TZrUInt64 publishedHashKey;
    TZrUInt64 computedHashKey;
    TZrUInt64 firstContentKey;
    TZrUInt64 secondContentKey;

    TEST_ASSERT_NOT_NULL(builtin);
    baselineProvider = *builtin;
    publishedHashProvider = *builtin;
    computedHashProvider = *builtin;
    publishedHashProvider.publicContractHash = "sha256:provider-contract-changed";
    computedHashProvider.computedPublicContractHash = 0x2222222222222222ULL;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.currentModuleKey = ZrCore_String_CreateFromNative(
            g_state, "@workspace/cache-contract");
    function.name = ZrCore_String_CreateFromNative(g_state, "cachedValue");
    alias = ZrCore_String_CreateFromNative(g_state, "compile");
    TEST_ASSERT_NOT_NULL(compiler.currentModuleKey);
    TEST_ASSERT_NOT_NULL(function.name);
    TEST_ASSERT_NOT_NULL(alias);

    TEST_ASSERT_TRUE(ZrParser_CompileToolBinding_DeclareProvider(
            &compiler, alias, &baselineProvider));
    baselineKey = ZrParser_ComptimeCache_BeginKey(&compiler, &function);
    ZrParser_CompileToolBinding_Reset(&compiler);
    TEST_ASSERT_TRUE(ZrParser_CompileToolBinding_DeclareProvider(
            &compiler, alias, &publishedHashProvider));
    publishedHashKey = ZrParser_ComptimeCache_BeginKey(&compiler, &function);
    ZrParser_CompileToolBinding_Reset(&compiler);
    TEST_ASSERT_TRUE(ZrParser_CompileToolBinding_DeclareProvider(
            &compiler, alias, &computedHashProvider));
    computedHashKey = ZrParser_ComptimeCache_BeginKey(&compiler, &function);

    TEST_ASSERT_NOT_EQUAL_UINT64(0U, baselineKey);
    TEST_ASSERT_NOT_EQUAL_UINT64(0U, publishedHashKey);
    TEST_ASSERT_NOT_EQUAL_UINT64(0U, computedHashKey);
    TEST_ASSERT_NOT_EQUAL_UINT64(baselineKey, publishedHashKey);
    TEST_ASSERT_NOT_EQUAL_UINT64(baselineKey, computedHashKey);

    ZrParser_CompileToolBinding_Reset(&compiler);
    TEST_ASSERT_TRUE(ZrParser_CompileToolBinding_DeclareProviderWithContentHash(
            &compiler, alias, &baselineProvider, "sha256:provider-content-a"));
    firstContentKey = ZrParser_ComptimeCache_BeginKey(&compiler, &function);
    ZrParser_CompileToolBinding_Reset(&compiler);
    TEST_ASSERT_TRUE(ZrParser_CompileToolBinding_DeclareProviderWithContentHash(
            &compiler, alias, &baselineProvider, "sha256:provider-content-b"));
    secondContentKey = ZrParser_ComptimeCache_BeginKey(&compiler, &function);

    TEST_ASSERT_NOT_EQUAL_UINT64(0U, firstContentKey);
    TEST_ASSERT_NOT_EQUAL_UINT64(0U, secondContentKey);
    TEST_ASSERT_NOT_EQUAL_UINT64(firstContentKey, secondContentKey);
    ZrParser_CompilerState_Free(&compiler);
}

static void test_removed_global_assert_and_fatal_error_are_not_builtins(void) {
    static const TZrChar *sources[] = {
            "comptime { Assert(true, \"legacy\"); }\nreturn 0;\n",
            "comptime { FatalError(\"legacy\"); }\nreturn 0;\n"};

    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(sources); index++) {
        SZrAstNode *ast = parse_source(sources[index], "comptime_removed_builtin.zr");
        SZrCompilerState compiler;

        ZrParser_CompilerState_Init(&compiler, g_state);
        compiler.suppressErrorOutput = ZR_TRUE;
        TEST_ASSERT_FALSE(prepare_and_run_late_checks(&compiler, ast));
        TEST_ASSERT_NOT_NULL(compiler.errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(
                compiler.errorMessage, "Unknown compile-time identifier"));
        ZrParser_CompilerState_Free(&compiler);
        ZrParser_Ast_Free(g_state, ast);
    }
}

static void test_patch_diagnostics_preserve_error_message_severity_and_location(void) {
    SZrCompilerState compiler;
    SZrObject *warning;
    SZrObject *error;
    SZrObject *array;
    SZrTypeValue arrayValue;
    SZrTypeValue warningValue;
    SZrTypeValue errorValue;
    SZrTypeValue key;
    SZrFileRange location = {0};
    TZrBool hasErrorDiagnostic = ZR_FALSE;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    location.source = ZrCore_String_CreateFromNative(
            g_state, "typed_patch_diagnostic.zr");
    location.start = ZrParser_FilePosition_Create(19U, 7, 11);
    location.end = ZrParser_FilePosition_Create(27U, 7, 19);
    warning = make_patch_diagnostic(
            ZR_FALSE, "generated member warning", 41U);
    error = make_patch_diagnostic(
            ZR_TRUE, "generated member rejected", 41U);
    init_object_value(&warningValue, warning);
    init_object_value(&errorValue, error);
    array = ZrCore_Object_NewCustomized(
            g_state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    TEST_ASSERT_NOT_NULL(array);
    ZrCore_Object_Init(g_state, array);
    ZrCore_Value_InitAsInt(g_state, &key, 0);
    ZrCore_Object_SetValue(g_state, array, &key, &warningValue);
    ZrCore_Value_InitAsInt(g_state, &key, 1);
    ZrCore_Object_SetValue(g_state, array, &key, &errorValue);
    ZrCore_Value_InitAsRawObject(
            g_state, &arrayValue, ZR_CAST_RAW_OBJECT_AS_SUPER(array));
    arrayValue.type = ZR_VALUE_TYPE_ARRAY;

    TEST_ASSERT_TRUE(ZrParser_CompileTime_ProcessPatchDiagnostics(
            &compiler, &arrayValue, 41U, location, &hasErrorDiagnostic));
    TEST_ASSERT_TRUE(hasErrorDiagnostic);
    TEST_ASSERT_TRUE(compiler.hasCompileTimeError);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_EQUAL_STRING("generated member rejected", compiler.errorMessage);
    TEST_ASSERT_EQUAL_PTR(location.source, compiler.errorLocation.source);
    TEST_ASSERT_EQUAL_INT32(7, compiler.errorLocation.start.line);
    TEST_ASSERT_EQUAL_INT32(11, compiler.errorLocation.start.column);
    TEST_ASSERT_EQUAL_INT32(7, compiler.errorLocation.end.line);
    TEST_ASSERT_EQUAL_INT32(19, compiler.errorLocation.end.column);
    TEST_ASSERT_EQUAL_UINT64(2U, compiler.comptimeBudget.usage.diagnosticCount);

    ZrParser_CompilerState_Free(&compiler);
}

static void test_patch_warning_uses_warning_log_severity(void) {
    SZrCompilerState compiler;
    SZrObject *warning;
    SZrTypeValue warningValue;
    SZrTypeValue arrayValue;
    SZrFileRange location = {0};
    TZrBool hasErrorDiagnostic = ZR_FALSE;

    ZrParser_CompilerState_Init(&compiler, g_state);
    warning = make_patch_diagnostic(
            ZR_FALSE, "generated severity warning", 41U);
    init_object_value(&warningValue, warning);
    init_array_value_with_repeated_entry(&arrayValue, &warningValue, 1U);
    g_state->global->logFunction = capture_log;

    TEST_ASSERT_TRUE(ZrParser_CompileTime_ProcessPatchDiagnostics(
            &compiler, &arrayValue, 41U, location, &hasErrorDiagnostic));
    TEST_ASSERT_FALSE(hasErrorDiagnostic);
    TEST_ASSERT_FALSE(compiler.hasCompileTimeError);
    TEST_ASSERT_TRUE(g_logCapture.called);
    TEST_ASSERT_EQUAL_INT(ZR_LOG_LEVEL_WARNING, g_logCapture.level);
    TEST_ASSERT_EQUAL_INT(ZR_OUTPUT_CHANNEL_STDERR, g_logCapture.channel);
    TEST_ASSERT_EQUAL_INT(ZR_OUTPUT_KIND_DIAGNOSTIC, g_logCapture.kind);
    TEST_ASSERT_NOT_NULL(strstr(
            g_logCapture.message, "[CompileTime WARNING]"));
    TEST_ASSERT_NOT_NULL(strstr(
            g_logCapture.message, "generated severity warning"));

    g_state->global->logFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
}

static void test_patch_diagnostic_default_budget_accepts_1024_and_rejects_1025(void) {
    SZrCompilerState compiler;
    SZrObject *warning;
    SZrTypeValue warningValue;
    SZrTypeValue acceptedArray;
    SZrTypeValue rejectedArray;
    SZrFileRange location = {0};
    TZrBool hasErrorDiagnostic = ZR_FALSE;

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_EQUAL_UINT64(
            1024U, compiler.comptimeBudget.limits.diagnosticCount);
    warning = make_patch_diagnostic(ZR_FALSE, "boundary warning", 41U);
    init_object_value(&warningValue, warning);
    init_array_value_with_repeated_entry(&acceptedArray, &warningValue, 1024U);
    TEST_ASSERT_TRUE(ZrParser_CompileTime_ProcessPatchDiagnostics(
            &compiler, &acceptedArray, 41U, location, &hasErrorDiagnostic));
    TEST_ASSERT_FALSE(hasErrorDiagnostic);
    TEST_ASSERT_FALSE(compiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_UINT64(
            1024U, compiler.comptimeBudget.usage.diagnosticCount);
    ZrParser_CompilerState_Free(&compiler);

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    init_array_value_with_repeated_entry(&rejectedArray, &warningValue, 1025U);
    TEST_ASSERT_FALSE(ZrParser_CompileTime_ProcessPatchDiagnostics(
            &compiler, &rejectedArray, 41U, location, &hasErrorDiagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT,
            compiler.comptimeBudget.exceededResource);
    TEST_ASSERT_EQUAL_UINT64(0U, compiler.comptimeBudget.usage.diagnosticCount);
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(
            compiler.errorMessage, "comptime.budget_exceeded"));

    ZrParser_CompilerState_Free(&compiler);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_typed_compile_tool_assert_and_warning_run_in_check_context);
    RUN_TEST(test_compile_tool_error_stops_build_with_typed_diagnostic);
    RUN_TEST(test_diagnostic_effect_is_rejected_in_pure_build_predicate);
    RUN_TEST(test_fuel_budget_stops_evaluation_without_partial_overrun);
    RUN_TEST(test_call_depth_budget_stops_recursive_comptime_function);
    RUN_TEST(test_pure_comptime_function_results_use_deterministic_cache);
    RUN_TEST(test_comptime_cache_key_includes_compile_tool_provider_contract);
    RUN_TEST(test_removed_global_assert_and_fatal_error_are_not_builtins);
    RUN_TEST(test_patch_diagnostics_preserve_error_message_severity_and_location);
    RUN_TEST(test_patch_warning_uses_warning_log_severity);
    RUN_TEST(test_patch_diagnostic_default_budget_accepts_1024_and_rejects_1025);
    return UNITY_END();
}
