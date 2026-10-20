#ifndef ZR_TEST_COMPTIME_IMPORT_ALIAS_OWNER_CASES_H
#define ZR_TEST_COMPTIME_IMPORT_ALIAS_OWNER_CASES_H

#include "compiler/tool/compile_tool_execution_scope.h"
#include "compiler/compile_time/compile_time_call.h"
#include "harness/module_fixture_support.h"

static SZrCompilerState g_ownerChildCompiler;
static TZrBool g_ownerChildInitialized;
static SZrAstNode *g_ownerChildAst;
static TZrByte *g_ownerBindingRows;
static TZrByte *g_ownerAliasRows;

static FZrIoLoadSource g_ownerPreviousSourceLoader;
static TZrPtr g_ownerPreviousSourceLoaderUserData;
static TZrBool g_ownerSourceLoaderInstalled;
static TZrBool g_ownerSourceFunctionsOwned;
static TZrSize g_ownerSourceLoaderCalls;
static SZrGcRootHandle g_ownerCurrentFunctionRoot;
static TZrBool g_ownerCurrentFunctionRooted;

static const ZrTestsFixtureSource g_ownerSourceFixtures[] = {
        ZR_TESTS_FIXTURE_SOURCE_TEXT(
                "sizing",
                "module sizing;\n"
                "pub comptime fn staticPlanSize(seed: int, factor: int): int {\n"
                "    return seed * factor + 3;\n"
                "}\n"),
};

static TZrBool owner_source_loader(
        SZrState *state, TZrNativeString path, TZrNativeString md5, SZrIo *io) {
    g_ownerSourceLoaderCalls++;
    return ZrTests_Fixture_SourceLoaderFromArray(
            state, path, md5, io, g_ownerSourceFixtures, ZR_ARRAY_COUNT(g_ownerSourceFixtures));
}

static void restore_owner_source_loader(void) {
    if (g_ownerSourceLoaderInstalled && g_state != ZR_NULL && g_state->global != ZR_NULL) {
        g_state->global->sourceLoader = g_ownerPreviousSourceLoader;
        g_state->global->sourceLoaderUserData = g_ownerPreviousSourceLoaderUserData;
    }
    g_ownerPreviousSourceLoader = ZR_NULL;
    g_ownerPreviousSourceLoaderUserData = ZR_NULL;
    g_ownerSourceLoaderInstalled = ZR_FALSE;
}

static void cleanup_owner_source_resources(void) {
    restore_owner_source_loader();
    if (g_ownerCurrentFunctionRooted) {
        SZrRawObject *functionObject = ZR_NULL;
        if (ZrCore_GcRootHandle_Resolve(g_state, &g_ownerCurrentFunctionRoot, &functionObject)) {
            g_compiler.currentFunction = ZR_CAST_FUNCTION(g_state, functionObject);
        } else {
            g_cleanupFailures++;
            fprintf(stderr, "Owned source function root could not be resolved during cleanup\n");
            g_compiler.currentFunction = ZR_NULL;
        }
        ZrCore_GcRootHandle_Release(g_state, &g_ownerCurrentFunctionRoot);
        g_ownerCurrentFunctionRooted = ZR_FALSE;
    }
    if (g_ownerSourceFunctionsOwned) {
        if (g_compiler.topLevelFunction != ZR_NULL &&
            g_compiler.topLevelFunction != g_compiler.currentFunction) {
            ZrCore_Function_Free(g_state, g_compiler.topLevelFunction);
        }
        if (g_compiler.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(g_state, g_compiler.currentFunction);
        }
        g_compiler.topLevelFunction = ZR_NULL;
        g_compiler.currentFunction = ZR_NULL;
        g_ownerSourceFunctionsOwned = ZR_FALSE;
    }
}

static SZrImportedCompileTimeModule *owner_source_alias_module(SZrString *name) {
    for (TZrSize index = g_compiler.importedCompileTimeModuleAliases.length; index > 0U; index--) {
        SZrImportedCompileTimeModuleAlias *alias =
                (SZrImportedCompileTimeModuleAlias *)ZrCore_Array_Get(
                        &g_compiler.importedCompileTimeModuleAliases, index - 1U);
        if (alias != ZR_NULL && alias->aliasName != ZR_NULL && ZrCore_String_Equal(alias->aliasName, name)) {
            return alias->module;
        }
    }
    return ZR_NULL;
}

static void cleanup_owner_case_resources(void) {
    cleanup_owner_source_resources();
    free(g_ownerBindingRows);
    free(g_ownerAliasRows);
    g_ownerBindingRows = ZR_NULL;
    g_ownerAliasRows = ZR_NULL;
    if (g_ownerChildInitialized) {
        ZrParser_CompilerState_Free(&g_ownerChildCompiler);
        g_ownerChildInitialized = ZR_FALSE;
    }
    if (g_ownerChildAst != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_ownerChildAst);
        g_ownerChildAst = ZR_NULL;
    }
}

static SZrCompileTimeFunction *assert_owner_root_function(void) {
    SZrCompileTimeFunction *function = record_named(&g_compiler.compileTimeFunctions, "rootFeature");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NULL(function->ownerModule);
    TEST_ASSERT_FALSE(function->isExported);
    TEST_ASSERT_FALSE(function->isRuntimeProjection);
    TEST_ASSERT_FALSE(function->isDeclarationTransform);
    TEST_ASSERT_NOT_NULL(function->declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, function->declaration->type);
    TEST_ASSERT_EQUAL_PTR(script_declaration(g_ast, "rootFeature"), function->declaration);
    TEST_ASSERT_EQUAL_PTR(g_ast, g_compiler.scriptAst);
    TEST_ASSERT_EQUAL_PTR(function->declaration->location.source, function->location.source);
    return function;
}

static TZrBool owner_shadow_is_present(SZrAstNode *declaration) {
    const SZrCompileToolBinding *binding = ZrParser_CompileToolBinding_Resolve(
            &g_compiler, declaration->data.variableDeclaration.pattern->data.identifier.name);
    return binding != ZR_NULL && binding->kind == ZR_COMPILE_TOOL_BINDING_SHADOW &&
           binding->provider == ZR_NULL;
}

static void test_root_owner_view_preserves_real_shadow_and_saved_rows_across_gc(void) {
    static const TZrChar *prefix =
            "let tool = import(\"zr.compile\");\n"
            "let derive = import(\"@derive\");\n"
            "let rootValue = 44;\n"
            "comptime fn rootFeature(): bool { return tool.build.feature(\"trace\"); }\n";
    SZrCompileToolExecutionScope scope = {0};
    SZrCompileTimeFunction *function;
    SZrAstNode *shadow;
    SEnvironmentMark mark;
    TZrSize bindingBytes;
    TZrSize aliasBytes;
    TZrBool entered;
    TZrBool shadowBeforeGc = ZR_FALSE;
    TZrBool shadowAfterGc = ZR_FALSE;
    TZrBool restoredRows;
    (void)prepare_probe(prefix, "rootFeature()");
    function = assert_owner_root_function();
    shadow = script_declaration(g_ast, "rootValue");
    TEST_ASSERT_NOT_NULL(shadow);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, shadow->type);
    TEST_ASSERT_NOT_NULL(shadow->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, shadow->data.variableDeclaration.value->type);
    TEST_ASSERT_EQUAL_INT64(44, shadow->data.variableDeclaration.value->data.integerLiteral.value);
    assert_binding(shadow, ZR_COMPILE_TOOL_BINDING_SHADOW, ZR_NULL);
    mark = environment_mark();
    bindingBytes = mark.bindings * g_compiler.compileToolBindings.elementSize;
    aliasBytes = mark.aliases * g_compiler.importedCompileTimeModuleAliases.elementSize;
    TEST_ASSERT_TRUE(bindingBytes > 0U && aliasBytes > 0U);
    g_ownerBindingRows = (TZrByte *)malloc(bindingBytes);
    g_ownerAliasRows = (TZrByte *)malloc(aliasBytes);
    TEST_ASSERT_NOT_NULL(g_ownerBindingRows);
    TEST_ASSERT_NOT_NULL(g_ownerAliasRows);
    memcpy(g_ownerBindingRows, g_compiler.compileToolBindings.head, bindingBytes);
    memcpy(g_ownerAliasRows, g_compiler.importedCompileTimeModuleAliases.head, aliasBytes);
    /* Leave and release snapshots before Unity can abort the scope. */
    entered = ZrParser_CompileToolExecutionScope_EnterFunction(&g_compiler, function, &scope);
    if (entered) {
        shadowBeforeGc = owner_shadow_is_present(shadow);
        ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
        shadowAfterGc = owner_shadow_is_present(shadow);
    }
    ZrParser_CompileToolExecutionScope_Leave(&g_compiler, &scope);
    restoredRows = mark.bindings == g_compiler.compileToolBindings.length &&
            mark.aliases == g_compiler.importedCompileTimeModuleAliases.length &&
            memcmp(g_ownerBindingRows, g_compiler.compileToolBindings.head, bindingBytes) == 0 &&
            memcmp(g_ownerAliasRows, g_compiler.importedCompileTimeModuleAliases.head, aliasBytes) == 0;
    cleanup_owner_case_resources();
    assert_environment_restored(mark);
    TEST_ASSERT_TRUE_MESSAGE(entered, g_compiler.errorMessage);
    TEST_ASSERT_TRUE(shadowBeforeGc);
    TEST_ASSERT_TRUE(shadowAfterGc);
    TEST_ASSERT_TRUE_MESSAGE(restoredRows, "Owner scope must restore the caller's live rows byte-for-byte");
    assert_binding(shadow, ZR_COMPILE_TOOL_BINDING_SHADOW, ZR_NULL);
}

static void test_root_owner_view_keeps_selected_conditional_import(void) {
    static const TZrChar *prefix =
            "comptime if (true) { let tool = import(\"zr.compile\"); }\n"
            "let derive = import(\"@derive\");\n"
            "comptime fn rootFeature(): bool { return tool.build.feature(\"trace\"); }\n";
    SZrAstNode *probe = prepare_probe(prefix, "rootFeature()");
    SZrAstNode *conditional = g_ast->data.script.statements->nodes[0];
    SZrAstNode *branch;
    SZrAstNode *import;
    TEST_ASSERT_NOT_NULL(conditional);
    TEST_ASSERT_EQUAL_INT(ZR_AST_COMPILE_TIME_DECLARATION, conditional->type);
    TEST_ASSERT_TRUE(conditional->data.compileTimeDeclaration.isConditionalPruning);
    TEST_ASSERT_TRUE(conditional->data.compileTimeDeclaration.buildFactsEvaluated);
    branch = conditional->data.compileTimeDeclaration.selectedBranch;
    TEST_ASSERT_NOT_NULL(branch);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, branch->type);
    TEST_ASSERT_NOT_NULL(branch->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(1U, branch->data.block.body->count);
    import = branch->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(import);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, import->type);
    TEST_ASSERT_NOT_NULL(import->data.variableDeclaration.pattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, import->data.variableDeclaration.pattern->type);
    TEST_ASSERT_EQUAL_STRING("tool", ZrCore_String_GetNativeString(
            import->data.variableDeclaration.pattern->data.identifier.name));
    TEST_ASSERT_NOT_NULL(import->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IMPORT_EXPRESSION, import->data.variableDeclaration.value->type);
    TEST_ASSERT_NOT_NULL(import->data.variableDeclaration.value->data.importExpression.modulePath);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRING_LITERAL,
            import->data.variableDeclaration.value->data.importExpression.modulePath->type);
    TEST_ASSERT_EQUAL_STRING(ZR_PARSER_COMPILE_TOOL_MODULE_BUILD, ZrCore_String_GetNativeString(
            import->data.variableDeclaration.value->data.importExpression.modulePath->data.stringLiteral.value));
    assert_binding(import, ZR_COMPILE_TOOL_BINDING_PROVIDER, ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
    assert_owner_root_function();
    assert_value(probe, ZR_VALUE_TYPE_BOOL, 1);
    assert_binding(import, ZR_COMPILE_TOOL_BINDING_PROVIDER, ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
}

static void test_validated_ancestor_function_value_uses_owner_import_in_source_call(void) {
    static const TZrChar *prefix =
            "let tool = import(\"zr.compile\");\n"
            "let derive = import(\"@derive\");\n"
            "comptime fn rootFeature(): bool { return tool.build.feature(\"trace\"); }\n";
    static const TZrChar *childSource =
            "let tool = import(\"zr.compile.declaration\");\n"
            "comptime { let probe = rootFeature(); }\n";
    SZrAstNode *parentProbe = prepare_probe(prefix, "rootFeature()");
    SZrCompileTimeFunction *function = assert_owner_root_function();
    SZrParserState parser;
    SZrFileRange errorLocation = {{0, 0, 0}, {0, 0, 0}, ZR_NULL};
    SZrString *sourceName;
    SZrString *moduleKey = ZR_NULL;
    SZrAstNode *wrapper;
    SZrAstNode *block;
    SZrAstNode *variable;
    SZrAstNode *probe;
    SZrAstNode *call;
    SZrAstNode *childImport;
    const SZrCompileToolBinding *binding;
    SZrCompileTimeFunction *selectedFunction = ZR_NULL;
    SZrTypeValue functionValue;
    SZrTypeValue result;
    SEnvironmentMark parentMark;
    TZrChar childPath[ZR_TESTS_PATH_MAX];
    TZrChar error[512] = {0};
    TZrSize roots;
    TZrSize bindings;
    TZrSize aliases;
    TZrBool parseError;
    TZrBool success;
    TZrBool restored;
    TZrBool hasError;
    int written;
    assert_value(parentProbe, ZR_VALUE_TYPE_BOOL, 1);
    parentMark = environment_mark();
    written = snprintf(childPath, sizeof(childPath), "%s/src/owner_child.zr",
            ZrCore_String_GetNativeString(g_project->directory));
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(childPath));
    sourceName = ZrCore_String_CreateFromNative(g_state, childPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(&parser, g_state, childSource, strlen(childSource), sourceName);
    parser.suppressErrorOutput = ZR_TRUE;
    parser.errorCallback = capture_parse_error;
    parser.errorUserData = error;
    g_ownerChildAst = ZrParser_ParseWithState(&parser);
    parseError = parser.hasError;
    ZrParser_State_Free(&parser);
    TEST_ASSERT_FALSE_MESSAGE(parseError, error);
    TEST_ASSERT_NOT_NULL(g_ownerChildAst);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ownerChildAst->type);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ProjectImports_CanonicalizeAst(
            g_state, g_ownerChildAst, sourceName, &moduleKey, error, sizeof(error), &errorLocation), error);
    ZrParser_CompilerState_Init(&g_ownerChildCompiler, g_state);
    g_ownerChildInitialized = ZR_TRUE;
    g_ownerChildCompiler.suppressErrorOutput = ZR_TRUE;
    g_ownerChildCompiler.compileToolProviderParent = &g_compiler;
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &g_ownerChildCompiler, g_ownerChildAst), g_ownerChildCompiler.errorMessage);
    TEST_ASSERT_EQUAL_PTR(g_ownerChildAst, g_ownerChildCompiler.scriptAst);
    TEST_ASSERT_EQUAL_PTR(&g_compiler, g_ownerChildCompiler.compileToolProviderParent);
    TEST_ASSERT_NULL(record_named(&g_ownerChildCompiler.compileTimeFunctions, "rootFeature"));
    TEST_ASSERT_EQUAL_PTR(function, record_named(&g_compiler.compileTimeFunctions, "rootFeature"));
    childImport = assert_import(g_ownerChildAst, "tool", ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION);
    binding = ZrParser_CompileToolBinding_Resolve(&g_ownerChildCompiler,
            childImport->data.variableDeclaration.pattern->data.identifier.name);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_INT(ZR_COMPILE_TOOL_BINDING_PROVIDER, binding->kind);
    TEST_ASSERT_NOT_NULL(binding->provider);
    TEST_ASSERT_EQUAL_STRING(ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION, binding->provider->moduleName);
    wrapper = g_ownerChildAst->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(wrapper);
    TEST_ASSERT_EQUAL_INT(ZR_AST_COMPILE_TIME_DECLARATION, wrapper->type);
    TEST_ASSERT_EQUAL_INT(ZR_COMPILE_TIME_STATEMENT, wrapper->data.compileTimeDeclaration.declarationType);
    TEST_ASSERT_FALSE(wrapper->data.compileTimeDeclaration.isConditionalPruning);
    block = wrapper->data.compileTimeDeclaration.declaration;
    TEST_ASSERT_NOT_NULL(block);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, block->type);
    TEST_ASSERT_NOT_NULL(block->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(1U, block->data.block.body->count);
    variable = block->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(variable);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, variable->type);
    probe = variable->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(probe);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, probe->type);
    TEST_ASSERT_NOT_NULL(probe->data.primaryExpression.property);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, probe->data.primaryExpression.property->type);
    TEST_ASSERT_EQUAL_STRING("rootFeature", ZrCore_String_GetNativeString(
            probe->data.primaryExpression.property->data.identifier.name));
    TEST_ASSERT_NOT_NULL(probe->data.primaryExpression.members);
    TEST_ASSERT_EQUAL_UINT64(1U, probe->data.primaryExpression.members->count);
    call = probe->data.primaryExpression.members->nodes[0];
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, call->type);
    TEST_ASSERT_TRUE(call->data.functionCall.args == ZR_NULL || call->data.functionCall.args->count == 0U);
    TEST_ASSERT_EQUAL_UINT64(0U, function->paramTypes.length);
    g_ownerChildCompiler.compilePhase = ZR_PARSER_COMPILE_PHASE_EXPANSION;
    g_ownerChildCompiler.comptimeContext = ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE;
    ZrCore_Value_ResetAsNull(&functionValue);
    TEST_ASSERT_TRUE(ct_value_from_compile_time_function(&g_ownerChildCompiler, function, &functionValue));
    TEST_ASSERT_TRUE(ct_value_try_get_compile_time_function(
            &g_ownerChildCompiler, &functionValue, &selectedFunction));
    TEST_ASSERT_EQUAL_PTR(function, selectedFunction);
    roots = ZrCore_GcDomain_GetRootCount(g_state);
    bindings = g_ownerChildCompiler.compileToolBindings.length;
    aliases = g_ownerChildCompiler.importedCompileTimeModuleAliases.length;
    ZrCore_Value_ResetAsNull(&result);
    /* Bare ancestor-name lookup is separate; this invokes the validated real callable. */
    success = ct_call_function(&g_ownerChildCompiler, call, selectedFunction,
            &call->data.functionCall, ZR_NULL, &result);
    hasError = g_ownerChildCompiler.hasCompileTimeError || g_ownerChildCompiler.hasError ||
            g_ownerChildCompiler.hasFatalError;
    snprintf(error, sizeof(error), "%s", g_ownerChildCompiler.errorMessage != ZR_NULL
            ? g_ownerChildCompiler.errorMessage : "Ancestor Root function evaluation failed");
    restored = roots == ZrCore_GcDomain_GetRootCount(g_state) &&
            bindings == g_ownerChildCompiler.compileToolBindings.length &&
            aliases == g_ownerChildCompiler.importedCompileTimeModuleAliases.length &&
            g_ownerChildCompiler.activeImportedCompileTimeModule == ZR_NULL &&
            g_ownerChildCompiler.comptimeCallDepth == 0U &&
            g_ownerChildCompiler.compilePhase == ZR_PARSER_COMPILE_PHASE_EXPANSION &&
            g_ownerChildCompiler.comptimeContext == ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE;
    cleanup_owner_case_resources();
    assert_environment_restored(parentMark);
    TEST_ASSERT_TRUE(restored);
    TEST_ASSERT_TRUE_MESSAGE(success, error);
    TEST_ASSERT_FALSE(hasError);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, result.type);
    TEST_ASSERT_EQUAL_INT(1, result.value.nativeObject.nativeBool != 0U);
    assert_owner_root_function();
    assert_binding(assert_import(g_ast, "tool", ZR_PARSER_COMPILE_TOOL_MODULE_BUILD),
            ZR_COMPILE_TOOL_BINDING_PROVIDER, ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
}

static void test_root_owner_view_preserves_ordinary_source_import_alias_across_gc(void) {
    static const TZrChar *source =
            "let sizing = import(\"sizing\");\n"
            "comptime fn readOwned(): int { return sizing.staticPlanSize(seed: 4, factor: 2); }\n";
    SZrCompileToolExecutionScope scope = {0};
    SZrParserState parser;
    SZrString *sourceName;
    SZrString *aliasName;
    SZrAstNode *import;
    SZrAstNode *body;
    SZrAstNode *statement;
    SZrCompileTimeFunction *function;
    SZrCompileTimeFunction *importedFunction;
    SZrImportedCompileTimeModule *module;
    SZrArray bindingHeader;
    SZrArray aliasHeader;
    SEnvironmentMark mark;
    TZrChar error[512] = {0};
    TZrSize bindingBytes;
    TZrSize aliasBytes;
    TZrSize loaderCalls;
    TZrBool parseError;
    TZrBool snapshotsReady;
    TZrBool entered = ZR_FALSE;
    TZrBool aliasBeforeGc = ZR_FALSE;
    TZrBool aliasAfterGc = ZR_FALSE;
    TZrBool restoredHeaders;
    TZrBool restoredRows;
    TZrBool loaderUnchanged;
    TZrBool functionRootResolved = ZR_FALSE;

    ZrParser_ToGlobalState_Register(g_state);
    sourceName = ZrCore_String_CreateFromNative(g_state, "ordinary_source_alias_owner.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(&parser, g_state, source, strlen(source), sourceName);
    parser.suppressErrorOutput = ZR_TRUE;
    parser.errorCallback = capture_parse_error;
    parser.errorUserData = error;
    g_ast = ZrParser_ParseWithState(&parser);
    parseError = parser.hasError;
    ZrParser_State_Free(&parser);
    TEST_ASSERT_FALSE_MESSAGE(parseError, error);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);

    g_ownerSourceLoaderCalls = 0U;
    g_ownerPreviousSourceLoader = g_state->global->sourceLoader;
    g_ownerPreviousSourceLoaderUserData = g_state->global->sourceLoaderUserData;
    g_ownerSourceLoaderInstalled = ZR_TRUE;
    g_state->global->sourceLoader = owner_source_loader;
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &g_compiler, g_ast), g_compiler.errorMessage);
    g_ownerSourceFunctionsOwned = ZR_TRUE;
    g_compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(g_compiler.currentFunction);
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_GcRootHandle_Create(g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(g_compiler.currentFunction), &g_ownerCurrentFunctionRoot),
            "Could not retain the actual source function across full GC");
    g_ownerCurrentFunctionRooted = ZR_TRUE;
    compile_script(&g_compiler, g_ast);
    TEST_ASSERT_FALSE_MESSAGE(g_compiler.hasError, g_compiler.errorMessage);
    TEST_ASSERT_FALSE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_FALSE(g_compiler.hasFatalError);
    TEST_ASSERT_NULL(g_compiler.topLevelFunction);
    TEST_ASSERT_TRUE(g_ownerSourceLoaderCalls > 0U);
    TEST_ASSERT_EQUAL_PTR(g_ast, g_compiler.scriptAst);

    import = assert_import(g_ast, "sizing", "sizing");
    aliasName = import->data.variableDeclaration.pattern->data.identifier.name;
    function = record_named(&g_compiler.compileTimeFunctions, "readOwned");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NULL(function->ownerModule);
    TEST_ASSERT_FALSE(function->isExported);
    TEST_ASSERT_FALSE(function->isRuntimeProjection);
    TEST_ASSERT_FALSE(function->isDeclarationTransform);
    TEST_ASSERT_EQUAL_PTR(script_declaration(g_ast, "readOwned"), function->declaration);
    TEST_ASSERT_NOT_NULL(function->declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, function->declaration->type);
    TEST_ASSERT_EQUAL_PTR(import->location.source, function->location.source);
    TEST_ASSERT_EQUAL_PTR(function->declaration->location.source, function->location.source);
    assert_source_node(import, "ordinary_source_alias_owner.zr");
    assert_source_node(function->declaration, "ordinary_source_alias_owner.zr");
    module = owner_source_alias_module(aliasName);
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_UINT64(1U, g_compiler.importedCompileTimeModules.length);
    TEST_ASSERT_EQUAL_UINT64(1U, g_compiler.importedCompileTimeModuleAliases.length);
    TEST_ASSERT_EQUAL_PTR(module, *(SZrImportedCompileTimeModule **)ZrCore_Array_Get(
            &g_compiler.importedCompileTimeModules, 0U));
    TEST_ASSERT_NOT_NULL(module->moduleName);
    TEST_ASSERT_TRUE(ZrCore_String_Equal(module->moduleName,
            import->data.variableDeclaration.value->data.importExpression.modulePath->data.stringLiteral.value));
    TEST_ASSERT_NOT_NULL(module->scriptAst);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, module->scriptAst->type);
    importedFunction = record_named(&module->compileTimeFunctions, "staticPlanSize");
    TEST_ASSERT_NOT_NULL(importedFunction);
    TEST_ASSERT_TRUE(importedFunction->isExported);
    TEST_ASSERT_FALSE(importedFunction->isRuntimeProjection);
    TEST_ASSERT_FALSE(importedFunction->isDeclarationTransform);
    TEST_ASSERT_EQUAL_PTR(module, importedFunction->ownerModule);
    TEST_ASSERT_EQUAL_PTR(script_declaration(module->scriptAst, "staticPlanSize"), importedFunction->declaration);
    TEST_ASSERT_NOT_NULL(importedFunction->declaration);
    TEST_ASSERT_EQUAL_PTR(importedFunction->declaration->location.source, importedFunction->location.source);
    assert_source_node(importedFunction->declaration, "sizing");
    body = function->declaration->data.functionDeclaration.body;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(1U, body->data.block.body->count);
    statement = body->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, statement->type);
    TEST_ASSERT_NOT_NULL(statement->data.returnStatement.expr);

    g_compiler.compilePhase = ZR_PARSER_COMPILE_PHASE_EXPANSION;
    g_compiler.comptimeContext = ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE;
    mark = environment_mark();
    memcpy(&bindingHeader, &g_compiler.compileToolBindings, sizeof(bindingHeader));
    memcpy(&aliasHeader, &g_compiler.importedCompileTimeModuleAliases, sizeof(aliasHeader));
    bindingBytes = mark.bindings * bindingHeader.elementSize;
    aliasBytes = mark.aliases * aliasHeader.elementSize;
    loaderCalls = g_ownerSourceLoaderCalls;
    if (bindingBytes > 0U) {
        g_ownerBindingRows = (TZrByte *)malloc(bindingBytes);
    }
    g_ownerAliasRows = (TZrByte *)malloc(aliasBytes);
    snapshotsReady = (bindingBytes == 0U || g_ownerBindingRows != ZR_NULL) && g_ownerAliasRows != ZR_NULL;
    if (snapshotsReady) {
        if (bindingBytes > 0U) {
            memcpy(g_ownerBindingRows, bindingHeader.head, bindingBytes);
        }
        memcpy(g_ownerAliasRows, aliasHeader.head, aliasBytes);
        entered = ZrParser_CompileToolExecutionScope_EnterFunction(&g_compiler, function, &scope);
        if (entered) {
            aliasBeforeGc = owner_source_alias_module(aliasName) == module;
            ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
            aliasAfterGc = owner_source_alias_module(aliasName) == module;
            {
                SZrRawObject *functionObject = ZR_NULL;
                functionRootResolved = ZrCore_GcRootHandle_Resolve(
                        g_state, &g_ownerCurrentFunctionRoot, &functionObject);
                if (functionRootResolved) {
                    g_compiler.currentFunction = ZR_CAST_FUNCTION(g_state, functionObject);
                }
            }
        }
    }
    /* Leave and release snapshots before Unity can abort the scope. */
    ZrParser_CompileToolExecutionScope_Leave(&g_compiler, &scope);
    restoredHeaders = memcmp(&bindingHeader, &g_compiler.compileToolBindings, sizeof(bindingHeader)) == 0 &&
            memcmp(&aliasHeader, &g_compiler.importedCompileTimeModuleAliases, sizeof(aliasHeader)) == 0;
    restoredRows = snapshotsReady && restoredHeaders &&
            (bindingBytes == 0U ||
             memcmp(g_ownerBindingRows, g_compiler.compileToolBindings.head, bindingBytes) == 0) &&
            memcmp(g_ownerAliasRows, g_compiler.importedCompileTimeModuleAliases.head, aliasBytes) == 0;
    loaderUnchanged = loaderCalls == g_ownerSourceLoaderCalls;
    free(g_ownerBindingRows);
    free(g_ownerAliasRows);
    g_ownerBindingRows = ZR_NULL;
    g_ownerAliasRows = ZR_NULL;
    restore_owner_source_loader();
    assert_environment_restored(mark);
    TEST_ASSERT_TRUE_MESSAGE(snapshotsReady, "Could not save the actual caller's live import rows");
    TEST_ASSERT_TRUE_MESSAGE(entered, g_compiler.errorMessage);
    TEST_ASSERT_TRUE_MESSAGE(aliasBeforeGc, "The owner's ordinary source import must resolve inside its view");
    TEST_ASSERT_TRUE_MESSAGE(aliasAfterGc, "The owner's ordinary source import must survive full GC");
    TEST_ASSERT_TRUE_MESSAGE(functionRootResolved, "The actual source function must remain rooted across full GC");
    TEST_ASSERT_TRUE_MESSAGE(restoredHeaders, "Owner scope must restore the complete caller array headers");
    TEST_ASSERT_TRUE_MESSAGE(restoredRows, "Owner scope must restore the caller's live rows byte-for-byte");
    TEST_ASSERT_TRUE_MESSAGE(loaderUnchanged, "Owner views must reuse the retained source module without loading");
    TEST_ASSERT_EQUAL_PTR(module, owner_source_alias_module(aliasName));
    assert_value(statement->data.returnStatement.expr, ZR_VALUE_TYPE_INT64, 11);
}

#endif
