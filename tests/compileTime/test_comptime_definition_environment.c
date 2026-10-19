#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler/compile_time/compile_time_import.h"
#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/project.h"
#include "zr_vm_library/zrm.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compile_tool.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/project_imports.h"

static const TZrChar g_providerSource[] =
        "comptime fn ownHelper(): int { return 11; }\n"
        "pub comptime fn pick(): int { return ownHelper(); }\n"
        "pub comptime fn missing(): int { return consumerHelper(); }\n"
        "pub comptime fn defaultPick(value: int = ownHelper()): int { return value; }\n"
        "pub comptime fn invoke(callback: fn() -> int): int { return callback(); }\n"
        "pub comptime fn readUnbound(): int { return consumerOnlyValue; }\n"
        "pub comptime fn localBlock(): int {\n"
        "    let value = 55; { let nestedValue = value + 1; return nestedValue; }\n"
        "}\n"
        "pub comptime fn defaultFromEarlier(first: int, second: int = first + 1): int { return second; }\n"
        "pub comptime fn echo(first: int, second: int): int { return second; }\n"
        "pub comptime fn readArray(first: int[], second: int[]): int { return first[0]; }\n"
        "pub comptime fn localCallback(): int { let callback = ownHelper; return callback(); }\n"
        "pub comptime fn makeCallback(): fn() -> int { return ownHelper; }\n"
        "pub comptime fn invalidCallback(): int { let callback = 1; return callback(); }\n"
        "pub comptime fn invokeRuntime(callback: fn(float) -> float): float { return callback(-1.0); }\n";

static const TZrChar g_consumerPrefix[] =
        "let derive = import(\"@derive\");\n"
        "comptime fn ownHelper(): int { return 22; }\n"
        "comptime fn consumerHelper(): int { return 33; }\n"
        "comptime fn callbackCaller(): int {\n"
        "    let callback = consumerHelper; return derive.invoke(callback);\n"
        "}\n"
        "comptime fn localCaller(): int {\n"
        "    { let consumerOnlyValue = 44; return derive.readUnbound(); }\n"
        "}\n"
        "comptime fn shadowCaller(): int {\n"
        "    let first = 77; return derive.echo(1, first);\n"
        "}\n"
        "comptime fn namedShadowCaller(): int {\n"
        "    let first = 77; return derive.echo(first: 1, second: first);\n"
        "}\n"
        "comptime fn cacheCaller(): int { return derive.defaultPick() + derive.defaultPick(11); }\n"
        "comptime fn returnedCallbackCaller(): int { let callback = derive.makeCallback(); return callback(); }\n";

static SZrState *g_state;
static SZrLibrary_Project *g_project;
static SZrCompilerState g_compiler;
static SZrCompilerState g_childCompiler;
static TZrBool g_compilerInitialized;
static TZrBool g_childCompilerInitialized;
static SZrAstNode *g_ast;
static SZrImportedCompileTimeModule *g_providerModule;
static TZrByte *g_archiveBytes;
static TZrChar g_sourcePath[ZR_TESTS_PATH_MAX];
static TZrChar g_archivePath[ZR_TESTS_PATH_MAX];
static TZrBool g_ownedSource;
static TZrBool g_ownedArchive;
static TZrSize g_archiveByteCount;
static TZrSize g_removedFixtureBytes;
static TZrSize g_removedFixtureFiles;
static TZrSize g_cleanupFailures;

typedef struct SArgumentGcProbe {
    FZrAllocator allocator;
    TZrPtr allocatorUserData;
    TZrSize objectAllocations;
    TZrBool collected;
    TZrBool beforeCollection;
    TZrBool afterCollection;
} SArgumentGcProbe;

static SArgumentGcProbe g_argumentGcProbe;

static void restore_allocator(void) {
    if (g_argumentGcProbe.allocator != ZR_NULL && g_state != ZR_NULL) {
        g_state->global->allocator = g_argumentGcProbe.allocator;
        g_state->global->userAllocationArguments = g_argumentGcProbe.allocatorUserData;
        g_argumentGcProbe.allocator = ZR_NULL;
    }
}

void setUp(void) {
    memset(&g_argumentGcProbe, 0, sizeof(g_argumentGcProbe));
    g_project = ZR_NULL;
    g_ast = ZR_NULL;
    g_providerModule = ZR_NULL;
    g_archiveBytes = ZR_NULL;
    g_archiveByteCount = 0U;
    g_sourcePath[0] = '\0';
    g_archivePath[0] = '\0';
    g_ownedSource = ZR_FALSE;
    g_ownedArchive = ZR_FALSE;
    g_compilerInitialized = ZR_FALSE;
    g_childCompilerInitialized = ZR_FALSE;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    g_compiler.suppressErrorOutput = ZR_TRUE;
}

static void remove_owned_fixture(const TZrChar *path, TZrBool owned) {
    FILE *file;
    long byteCount = -1L;
    if (!owned || !ZrTests_File_Exists(path)) {
        return;
    }
    file = fopen(path, "rb");
    if (file != ZR_NULL) {
        if (fseek(file, 0L, SEEK_END) == 0) {
            byteCount = ftell(file);
        }
        fclose(file);
    }
    if (byteCount < 0L) {
        g_cleanupFailures++;
        printf("FIXTURE_CLEANUP_SIZE_FAILED=%s\n", path);
        return;
    }
    if (remove(path) == 0) {
        g_removedFixtureFiles++;
        g_removedFixtureBytes += (TZrSize)byteCount;
    } else {
        g_cleanupFailures++;
        printf("FIXTURE_CLEANUP_FAILED=%s\n", path);
    }
}

void tearDown(void) {
    restore_allocator();
    if (g_childCompilerInitialized) {
        ZrParser_CompilerState_Free(&g_childCompiler);
        g_childCompilerInitialized = ZR_FALSE;
    }
    if (g_compilerInitialized) {
        ZrParser_CompilerState_Free(&g_compiler);
        g_compilerInitialized = ZR_FALSE;
    }
    if (g_ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_ast);
        g_ast = ZR_NULL;
    }
    if (g_state != ZR_NULL && g_state->global != ZR_NULL) {
        g_state->global->userData = ZR_NULL;
    }
    if (g_project != ZR_NULL) {
        ZrLibrary_Project_Free(g_state, g_project);
        g_project = ZR_NULL;
    }
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
    free(g_archiveBytes);
    g_archiveBytes = ZR_NULL;
    g_providerModule = ZR_NULL;
    remove_owned_fixture(g_archivePath, g_ownedArchive);
    remove_owned_fixture(g_sourcePath, g_ownedSource);
    g_ownedSource = ZR_FALSE;
    g_ownedArchive = ZR_FALSE;
}

static void generated_path(
        const TZrChar *baseName, const TZrChar *extension, TZrChar *path) {
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "parser", "comptime_definition_environment", baseName, extension,
            path, ZR_TESTS_PATH_MAX));
    for (TZrSize i = 0U; path[i] != '\0'; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
}

static TZrBool write_provider_source(void) {
    FILE *file = fopen(g_sourcePath, "wb");
    TZrBool success;
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    g_ownedSource = ZR_TRUE;
    success = fwrite(g_providerSource, 1U, sizeof(g_providerSource) - 1U, file) ==
              sizeof(g_providerSource) - 1U;
    return fclose(file) == 0 && success;
}

static void create_locked_project(void) {
    const SZrParserCompileToolModuleDescriptor *descriptor =
            ZrParser_CompileTool_FindModule(ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
    SZrLibrary_ZrmPackModule module = {0};
    SZrLibrary_ZrmPackRequest request = {0};
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar moduleHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar archiveHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar manifest[4096];
    TZrChar lock[2048];
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH] = {0};
    int written;

    TEST_ASSERT_NOT_NULL(descriptor);
    generated_path("provider", ".zrs", g_sourcePath);
    generated_path("provider", ".zrm", g_archivePath);
    generated_path("consumer", ".zrp", projectPath);
    TEST_ASSERT_FALSE_MESSAGE(ZrTests_File_Exists(g_sourcePath), "Unowned source fixture exists");
    TEST_ASSERT_FALSE_MESSAGE(ZrTests_File_Exists(g_archivePath), "Unowned archive fixture exists");
    TEST_ASSERT_TRUE(write_provider_source());
    TEST_ASSERT_TRUE(ZrParser_CompileToolContentHash_Bytes(
            (const TZrByte *)g_providerSource, sizeof(g_providerSource) - 1U,
            moduleHash, sizeof(moduleHash)));
    module.moduleKey = "main";
    module.sourcePath = g_sourcePath;
    module.hash = moduleHash;
    module.compileToolExecutableSourcePath = g_sourcePath;
    module.compileToolExecutableHash = moduleHash;
    request.outputPath = g_archivePath;
    request.assembly.name = "derive";
    request.assembly.version = "1.4.0";
    request.assembly.kind = "compile-tool";
    request.assembly.entryModule = "main";
    request.assembly.providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL;
    request.assembly.publicContractHash = descriptor->publicContractHash;
    request.modules = &module;
    request.moduleCount = 1U;
    g_ownedArchive = ZR_TRUE;
    TEST_ASSERT_TRUE_MESSAGE(
            ZrLibrary_Zrm_WriteArchive(&request, error, sizeof(error)), error);
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(
            g_archivePath, &g_archiveBytes, &g_archiveByteCount));
    TEST_ASSERT_TRUE(ZrParser_CompileToolContentHash_Bytes(
            g_archiveBytes, g_archiveByteCount, archiveHash, sizeof(archiveHash)));
    free(g_archiveBytes);
    g_archiveBytes = ZR_NULL;
    written = snprintf(manifest, sizeof(manifest),
            "{\"manifestVersion\":2,\"name\":\"consumer\","
            "\"version\":\"1.0.0\",\"kind\":\"executable\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\","
            "\"dependencies\":{},\"buildDependencies\":{"
            "\"@derive\":{\"version\":\"^1.0.0\",\"path\":\"%s\"}}}",
            g_archivePath);
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(manifest));
    g_project = ZrLibrary_Project_New(g_state, manifest, projectPath);
    TEST_ASSERT_NOT_NULL(g_project);
    written = snprintf(lock, sizeof(lock),
            "{\"lockVersion\":1,\"dependencies\":{},\"buildDependencies\":{"
            "\"@derive\":{\"version\":\"1.4.0\",\"contentHash\":\"%s\","
            "\"transitiveIdentity\":\"sha256:uZUQq8UmbWYobrUOMIYjSt_JG2D7HnXK8ukUtWWZfpA\","
            "\"provider\":\"path\"}}}", archiveHash);
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(lock));
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_ProjectManifestV2_ReadDependencyLock(
            g_state, g_project, lock, error, sizeof(error)), error);
    g_state->global->userData = g_project;
}

static SZrCompileTimeFunction *record_named(SZrArray *records, const TZrChar *name) {
    for (TZrSize i = 0U; i < records->length; i++) {
        SZrCompileTimeFunction **record =
                (SZrCompileTimeFunction **)ZrCore_Array_Get(records, i);
        if (record != ZR_NULL && *record != ZR_NULL && (*record)->name != ZR_NULL &&
            strcmp(name, ZrCore_String_GetNativeString((*record)->name)) == 0) {
            return *record;
        }
    }
    return ZR_NULL;
}

static void assert_provider_identity(void) {
    const SZrCompileToolBinding *binding;
    SZrCompileTimeFunction *record;
    TEST_ASSERT_EQUAL_UINT64(1U, g_compiler.compileToolBindings.length);
    TEST_ASSERT_EQUAL_UINT64(1U, g_compiler.ownedCompileToolProviders.length);
    TEST_ASSERT_EQUAL_UINT64(1U, g_compiler.importedCompileTimeModules.length);
    binding = (const SZrCompileToolBinding *)ZrCore_Array_Get(&g_compiler.compileToolBindings, 0U);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_NOT_NULL(binding->provider);
    TEST_ASSERT_NOT_NULL(binding->resolvedArtifact);
    TEST_ASSERT_TRUE(ZrParser_CompileToolArtifact_IsOpen(binding->resolvedArtifact));
    TEST_ASSERT_EQUAL_STRING("@derive", binding->provider->moduleName);
    g_providerModule = *(SZrImportedCompileTimeModule **)ZrCore_Array_Get(
            &g_compiler.importedCompileTimeModules, 0U);
    TEST_ASSERT_NOT_NULL(g_providerModule);
    TEST_ASSERT_EQUAL_STRING("@derive", ZrCore_String_GetNativeString(g_providerModule->moduleName));
    TEST_ASSERT_NOT_NULL(g_providerModule->scriptAst);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_providerModule->scriptAst->type);
    for (TZrSize i = 0U; i < g_providerModule->compileTimeFunctions.length; i++) {
        record = *(SZrCompileTimeFunction **)ZrCore_Array_Get(
                &g_providerModule->compileTimeFunctions, i);
        TEST_ASSERT_NOT_NULL(record);
        TEST_ASSERT_EQUAL_PTR(g_providerModule, record->ownerModule);
        TEST_ASSERT_FALSE(record->isRuntimeProjection);
        TEST_ASSERT_NOT_NULL(record->declaration);
        TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, record->declaration->type);
        TEST_ASSERT_NOT_NULL(record->location.source);
        TEST_ASSERT_EQUAL_STRING("@derive", ZrCore_String_GetNativeString(record->location.source));
    }
    record = record_named(&g_providerModule->compileTimeFunctions, "ownHelper");
    TEST_ASSERT_NOT_NULL(record);
    TEST_ASSERT_FALSE(record->isExported);
    record = record_named(&g_providerModule->compileTimeFunctions, "pick");
    TEST_ASSERT_NOT_NULL(record);
    TEST_ASSERT_TRUE(record->isExported);
    TEST_ASSERT_NULL(record_named(&g_providerModule->compileTimeFunctions, "consumerHelper"));
    record = record_named(&g_compiler.compileTimeFunctions, "ownHelper");
    TEST_ASSERT_NOT_NULL(record);
    TEST_ASSERT_NULL(record->ownerModule);
    TEST_ASSERT_EQUAL_UINT64(0U, g_project->dependencyPackageCount);
}

static void capture_parse_error(
        TZrPtr userData, const SZrFileRange *location,
        const TZrChar *message, EZrToken token) {
    TZrChar *error = (TZrChar *)userData;
    (void)location;
    (void)token;
    if (error != ZR_NULL && error[0] == '\0') {
        snprintf(error, 512U, "Consumer parse: %s", message != ZR_NULL ? message : "unknown error");
    }
}

static SZrAstNode *prepare_probe(const TZrChar *expression) {
    TZrChar source[4096];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar error[512] = {0};
    SZrFileRange errorLocation = {{0, 0, 0}, {0, 0, 0}, ZR_NULL};
    SZrString *sourceName;
    SZrString *moduleKey = ZR_NULL;
    SZrParserState parser;
    TZrBool hadError;
    SZrAstNode *declaration;
    int written;

    create_locked_project();
    written = snprintf(sourcePath, sizeof(sourcePath), "%s/src/main.zr",
            ZrCore_String_GetNativeString(g_project->directory));
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(sourcePath));
    written = snprintf(source, sizeof(source), "%scomptime { let probe = %s; }\n", g_consumerPrefix, expression);
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(source));
    sourceName = ZrCore_String_CreateFromNative(g_state, sourcePath);
    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(&parser, g_state, source, strlen(source), sourceName);
    parser.suppressErrorOutput = ZR_TRUE;
    parser.errorCallback = capture_parse_error;
    parser.errorUserData = error;
    g_ast = ZrParser_ParseWithState(&parser);
    hadError = parser.hasError;
    ZrParser_State_Free(&parser);
    TEST_ASSERT_FALSE_MESSAGE(hadError, error);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ProjectImports_CanonicalizeAst(
            g_state, g_ast, sourceName, &moduleKey, error, sizeof(error), &errorLocation), error);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &g_compiler, g_ast),
            g_compiler.errorMessage != ZR_NULL ? g_compiler.errorMessage : "build facts failed");
    assert_provider_identity();
    TEST_ASSERT_EQUAL_UINT64(0U, g_compiler.comptimeCache.length);
    TEST_ASSERT_NULL(g_compiler.activeImportedCompileTimeModule);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    declaration = g_ast->data.script.statements->nodes[g_ast->data.script.statements->count - 1U];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_COMPILE_TIME_DECLARATION, declaration->type);
    TEST_ASSERT_EQUAL_INT(ZR_COMPILE_TIME_STATEMENT, declaration->data.compileTimeDeclaration.declarationType);
    TEST_ASSERT_FALSE(declaration->data.compileTimeDeclaration.isConditionalPruning);
    declaration = declaration->data.compileTimeDeclaration.declaration;
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(1U, declaration->data.block.body->count);
    declaration = declaration->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.value);
    g_compiler.comptimeContext = ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE;
    g_compiler.compilePhase = ZR_PARSER_COMPILE_PHASE_EXPANSION;
    return declaration->data.variableDeclaration.value;
}

typedef struct SEnvironmentMark {
    TZrSize bindings;
    TZrSize aliases;
    TZrSize roots;
    TZrUInt32 depth;
    SZrImportedCompileTimeModule *owner;
} SEnvironmentMark;

static SEnvironmentMark environment_mark(void) {
    SEnvironmentMark mark;
    mark.bindings = g_compiler.compileToolBindings.length;
    mark.aliases = g_compiler.importedCompileTimeModuleAliases.length;
    mark.roots = ZrCore_GcDomain_GetRootCount(g_state);
    mark.depth = g_compiler.comptimeCallDepth;
    mark.owner = g_compiler.activeImportedCompileTimeModule;
    return mark;
}

static void assert_environment_restored(SEnvironmentMark mark) {
    TEST_ASSERT_EQUAL_UINT64(mark.bindings, g_compiler.compileToolBindings.length);
    TEST_ASSERT_EQUAL_UINT64(mark.aliases, g_compiler.importedCompileTimeModuleAliases.length);
    TEST_ASSERT_EQUAL_UINT64(mark.roots, ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_EQUAL_UINT32(mark.depth, g_compiler.comptimeCallDepth);
    TEST_ASSERT_EQUAL_PTR(mark.owner, g_compiler.activeImportedCompileTimeModule);
}

static void assert_scalar(SZrAstNode *probe, TZrInt64 expected) {
    SZrTypeValue result;
    SEnvironmentMark mark = environment_mark();
    TZrBool success;
    ZrCore_Value_ResetAsNull(&result);
    success = ZrParser_Compiler_EvaluateCompileTimeExpression(&g_compiler, probe, &result);
    assert_environment_restored(mark);
    TEST_ASSERT_TRUE_MESSAGE(success,
            g_compiler.errorMessage != ZR_NULL ? g_compiler.errorMessage : "evaluation failed");
    TEST_ASSERT_FALSE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(expected, result.value.nativeObject.nativeInt64);
}

static SZrAstNode *provider_return_expression(const TZrChar *name) {
    SZrCompileTimeFunction *record = record_named(&g_providerModule->compileTimeFunctions, name);
    SZrAstNode *body;
    SZrAstNode *statement;
    TEST_ASSERT_NOT_NULL(record);
    body = record->declaration->data.functionDeclaration.body;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(1U, body->data.block.body->count);
    statement = body->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, statement->type);
    TEST_ASSERT_NOT_NULL(statement->data.returnStatement.expr);
    return statement->data.returnStatement.expr;
}

static void assert_error_range(SZrAstNode *node) {
    TEST_ASSERT_EQUAL_PTR(node->location.source, g_compiler.errorLocation.source);
    TEST_ASSERT_EQUAL_UINT64(node->location.start.offset, g_compiler.errorLocation.start.offset);
    TEST_ASSERT_EQUAL_INT(node->location.start.line, g_compiler.errorLocation.start.line);
    TEST_ASSERT_EQUAL_INT(node->location.start.column, g_compiler.errorLocation.start.column);
    TEST_ASSERT_EQUAL_UINT64(node->location.end.offset, g_compiler.errorLocation.end.offset);
    TEST_ASSERT_EQUAL_INT(node->location.end.line, g_compiler.errorLocation.end.line);
    TEST_ASSERT_EQUAL_INT(node->location.end.column, g_compiler.errorLocation.end.column);
}

static void assert_unknown_provider_identifier(
        SZrAstNode *probe, SZrAstNode *identifier, const TZrChar *name) {
    SZrTypeValue result;
    SEnvironmentMark mark = environment_mark();
    TZrBool success;
    TEST_ASSERT_NOT_NULL(identifier);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, identifier->type);
    TEST_ASSERT_EQUAL_STRING(name, ZrCore_String_GetNativeString(identifier->data.identifier.name));
    TEST_ASSERT_NOT_NULL(identifier->location.source);
    TEST_ASSERT_EQUAL_STRING("@derive", ZrCore_String_GetNativeString(identifier->location.source));
    TEST_ASSERT_TRUE(identifier->location.start.line > 0);
    TEST_ASSERT_TRUE(identifier->location.end.offset > identifier->location.start.offset);
    ZrCore_Value_ResetAsNull(&result);
    success = ZrParser_Compiler_EvaluateCompileTimeExpression(&g_compiler, probe, &result);
    assert_environment_restored(mark);
    TEST_ASSERT_FALSE_MESSAGE(success, "Provider must not capture consumer bindings");
    TEST_ASSERT_TRUE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result.type));
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, "Unknown compile-time identifier: "));
    TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, name));
    assert_error_range(identifier);
}

static void test_provider_private_helper_wins_over_consumer_homonym(void) {
    assert_scalar(prepare_probe("derive.pick()"), 11);
}

static void test_provider_miss_does_not_capture_consumer_function(void) {
    SZrAstNode *probe = prepare_probe("derive.missing()");
    SZrAstNode *call = provider_return_expression("missing");
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, call->type);
    TEST_ASSERT_NOT_NULL(call->data.primaryExpression.members);
    TEST_ASSERT_TRUE(call->data.primaryExpression.members->count > 0U);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, call->data.primaryExpression.members->nodes[0]->type);
    assert_unknown_provider_identifier(probe, call->data.primaryExpression.property, "consumerHelper");
}

static void test_omitted_default_uses_provider_environment(void) {
    assert_scalar(prepare_probe("derive.defaultPick()"), 11);
}

static void test_positional_argument_uses_consumer_environment(void) {
    assert_scalar(prepare_probe("derive.defaultPick(ownHelper())"), 22);
}

static void test_named_argument_uses_consumer_environment(void) {
    assert_scalar(prepare_probe("derive.defaultPick(value: ownHelper())"), 22);
}

static void test_explicit_consumer_callback_remains_callable(void) {
    assert_scalar(prepare_probe("derive.invoke(consumerHelper)"), 33);
}

static void test_prebound_consumer_local_callback_remains_callable(void) {
    assert_scalar(prepare_probe("callbackCaller()"), 33);
}

static void test_provider_body_does_not_capture_consumer_nested_local(void) {
    SZrAstNode *probe = prepare_probe("localCaller()");
    assert_unknown_provider_identifier(probe, provider_return_expression("readUnbound"), "consumerOnlyValue");
}

static void test_provider_nested_block_keeps_lexical_parent(void) {
    assert_scalar(prepare_probe("derive.localBlock()"), 56);
}

static void test_provider_definition_survives_full_gc_after_import(void) {
    SZrAstNode *probe = prepare_probe("derive.pick()");
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    assert_provider_identity();
    assert_scalar(probe, 11);
}

static void test_explicit_callback_survives_full_gc_after_import(void) {
    SZrAstNode *probe = prepare_probe("callbackCaller()");
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    assert_provider_identity();
    assert_scalar(probe, 33);
}

static void test_private_provider_function_is_not_external_member(void) {
    SZrAstNode *probe = prepare_probe("derive.ownHelper()");
    SZrAstNode *identifier;
    SEnvironmentMark mark = environment_mark();
    SZrTypeValue result;
    TZrBool success;
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, probe->type);
    identifier = probe->data.primaryExpression.property;
    TEST_ASSERT_NOT_NULL(identifier);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, identifier->type);
    TEST_ASSERT_EQUAL_STRING("derive", ZrCore_String_GetNativeString(identifier->data.identifier.name));
    ZrCore_Value_ResetAsNull(&result);
    success = ZrParser_Compiler_EvaluateCompileTimeExpression(&g_compiler, probe, &result);
    assert_environment_restored(mark);
    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_TRUE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result.type));
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_EQUAL_STRING("Unknown compile-time identifier: derive", g_compiler.errorMessage);
    assert_error_range(identifier);
}

static void test_omitted_default_reads_earlier_parameter(void) {
    assert_scalar(prepare_probe("derive.defaultFromEarlier(7)"), 8);
}

static void test_omitted_default_reads_named_earlier_parameter(void) {
    assert_scalar(prepare_probe("derive.defaultFromEarlier(first: 7)"), 8);
}

static void test_later_argument_uses_caller_variable_over_callee_parameter(void) {
    assert_scalar(prepare_probe("shadowCaller()"), 77);
}

static void test_named_argument_uses_caller_variable_over_callee_parameter(void) {
    assert_scalar(prepare_probe("namedShadowCaller()"), 77);
}

static void test_explicit_and_default_scalar_cache_replay_survives_gc(void) {
    SZrAstNode *probe = prepare_probe("cacheCaller()");
    TZrUInt64 hits;
    assert_scalar(probe, 22);
    TEST_ASSERT_TRUE(g_compiler.comptimeCache.length > 0U);
    TEST_ASSERT_TRUE(g_compiler.comptimeCacheHitCount > 0U);
    hits = g_compiler.comptimeCacheHitCount;
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    assert_provider_identity();
    assert_scalar(probe, 22);
    TEST_ASSERT_TRUE(g_compiler.comptimeCacheHitCount > hits);
}

static TZrBool first_array_is_live(void) {
    for (SZrRawObject *raw = g_state->global->garbageCollector->gcObjectList;
         raw != ZR_NULL; raw = raw->next) {
        SZrObject *object;
        if (raw->type != ZR_RAW_OBJECT_TYPE_ARRAY) {
            continue;
        }
        object = (SZrObject *)raw;
        if (!object->nodeMap.isValid || object->nodeMap.elementCount != 1U) {
            continue;
        }
        for (TZrSize i = 0U; i < object->nodeMap.capacity; i++) {
            const SZrHashKeyValuePair *pair = object->nodeMap.buckets[i];
            for (; pair != ZR_NULL; pair = pair->next) {
                if (ZR_VALUE_IS_TYPE_INT(pair->key.type) &&
                    pair->key.value.nativeObject.nativeInt64 == 0 &&
                    ZR_VALUE_IS_TYPE_INT(pair->value.type) &&
                    pair->value.value.nativeObject.nativeInt64 == 77) {
                    return ZR_TRUE;
                }
            }
        }
    }
    return ZR_FALSE;
}

static TZrPtr collect_during_second_array_argument(
        TZrPtr userData, TZrPtr pointer, TZrSize originalSize,
        TZrSize newSize, TZrInt64 flag) {
    SArgumentGcProbe *probe = (SArgumentGcProbe *)userData;
    if (pointer == ZR_NULL && newSize == sizeof(SZrObject) &&
        flag == ZR_MEMORY_NATIVE_TYPE_OBJECT) {
        probe->objectAllocations++;
        if (!probe->collected && probe->objectAllocations == 2U) {
            probe->collected = ZR_TRUE;
            probe->beforeCollection = first_array_is_live();
            ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
            probe->afterCollection = first_array_is_live();
        }
    }
    return probe->allocator(
            probe->allocatorUserData, pointer, originalSize, newSize, flag);
}

static void test_earlier_aggregate_argument_survives_gc_during_later_argument(void) {
    SZrAstNode *probe = prepare_probe("derive.readArray([77], [88])");
    SEnvironmentMark mark = environment_mark();
    SZrTypeValue result;
    TZrBool success;
    g_argumentGcProbe.allocator = g_state->global->allocator;
    g_argumentGcProbe.allocatorUserData = g_state->global->userAllocationArguments;
    g_state->global->allocator = collect_during_second_array_argument;
    g_state->global->userAllocationArguments = &g_argumentGcProbe;
    ZrCore_Value_ResetAsNull(&result);
    success = ZrParser_Compiler_EvaluateCompileTimeExpression(&g_compiler, probe, &result);
    restore_allocator();
    assert_environment_restored(mark);
    TEST_ASSERT_TRUE_MESSAGE(success,
            g_compiler.errorMessage != ZR_NULL ? g_compiler.errorMessage : "evaluation failed");
    TEST_ASSERT_FALSE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_TRUE(g_argumentGcProbe.collected);
    TEST_ASSERT_TRUE(g_argumentGcProbe.beforeCollection);
    TEST_ASSERT_TRUE(g_argumentGcProbe.afterCollection);
    TEST_ASSERT_EQUAL_UINT64(2U, g_argumentGcProbe.objectAllocations);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(77, result.value.nativeObject.nativeInt64);
}

static void test_private_provider_function_value_is_callable_locally(void) {
    assert_scalar(prepare_probe("derive.localCallback()"), 11);
}

static void test_private_provider_function_value_can_be_returned_to_consumer(void) {
    assert_scalar(prepare_probe("returnedCallbackCaller()"), 11);
}

static void test_returned_private_callback_can_be_passed_to_provider(void) {
    assert_scalar(prepare_probe("derive.invoke(derive.makeCallback())"), 11);
}

static void test_private_provider_function_value_survives_full_gc(void) {
    SZrAstNode *probe = prepare_probe("derive.localCallback()");
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    assert_provider_identity();
    assert_scalar(probe, 11);
}

static void test_returned_private_callback_value_preserves_provider_identity(void) {
    SZrAstNode *probe = prepare_probe("derive.makeCallback()");
    SZrCompileTimeFunction *providerFunction = record_named(
            &g_providerModule->compileTimeFunctions, "ownHelper");
    SZrCompileTimeFunction *consumerFunction = record_named(
            &g_compiler.compileTimeFunctions, "ownHelper");
    SZrTypeValue result;
    SEnvironmentMark mark = environment_mark();
    TZrBool success;
    TEST_ASSERT_NOT_NULL(providerFunction);
    TEST_ASSERT_NOT_NULL(consumerFunction);
    ZrCore_Value_ResetAsNull(&result);
    success = ZrParser_Compiler_EvaluateCompileTimeExpression(&g_compiler, probe, &result);
    assert_environment_restored(mark);
    TEST_ASSERT_TRUE_MESSAGE(success, g_compiler.errorMessage);
    TEST_ASSERT_FALSE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NATIVE_POINTER, result.type);
    TEST_ASSERT_EQUAL_PTR(providerFunction, result.value.nativeObject.nativePointer);
    TEST_ASSERT_TRUE(consumerFunction != result.value.nativeObject.nativePointer);
    TEST_ASSERT_EQUAL_PTR(g_providerModule, providerFunction->ownerModule);
    TEST_ASSERT_FALSE(providerFunction->isExported);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    assert_provider_identity();
    TEST_ASSERT_EQUAL_PTR(providerFunction, record_named(
            &g_providerModule->compileTimeFunctions, "ownHelper"));
    TEST_ASSERT_EQUAL_PTR(providerFunction, result.value.nativeObject.nativePointer);
}

static void assert_runtime_callable_rejected(SZrAstNode *probe, SZrAstNode *callSite) {
    SEnvironmentMark mark = environment_mark();
    SZrTypeValue result;
    TZrBool success;
    TEST_ASSERT_NOT_NULL(callSite);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, callSite->type);
    TEST_ASSERT_NOT_NULL(callSite->data.primaryExpression.members);
    TEST_ASSERT_EQUAL_UINT64(1U, callSite->data.primaryExpression.members->count);
    callSite = callSite->data.primaryExpression.members->nodes[0];
    TEST_ASSERT_NOT_NULL(callSite);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, callSite->type);
    TEST_ASSERT_EQUAL_STRING("@derive", ZrCore_String_GetNativeString(callSite->location.source));
    ZrCore_Value_ResetAsNull(&result);
    success = ZrParser_Compiler_EvaluateCompileTimeExpression(&g_compiler, probe, &result);
    assert_environment_restored(mark);
    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_TRUE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result.type));
    TEST_ASSERT_EQUAL_STRING(
            "comptime.effect_violation: runtime callable invocation is not allowed in compile-time evaluation",
            g_compiler.errorMessage);
    assert_error_range(callSite);
}

static void test_noncallable_value_keeps_effect_rejection(void) {
    SZrAstNode *probe = prepare_probe("derive.invalidCallback()");
    SZrCompileTimeFunction *record = record_named(
            &g_providerModule->compileTimeFunctions, "invalidCallback");
    SZrAstNodeArray *statements;
    TEST_ASSERT_NOT_NULL(record);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, record->declaration->data.functionDeclaration.body->type);
    statements = record->declaration->data.functionDeclaration.body->data.block.body;
    TEST_ASSERT_NOT_NULL(statements);
    TEST_ASSERT_EQUAL_UINT64(2U, statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, statements->nodes[1]->type);
    assert_runtime_callable_rejected(probe, statements->nodes[1]->data.returnStatement.expr);
}

static void test_real_native_closure_keeps_effect_rejection(void) {
    SZrAstNode *probe;
    SZrAstNode *call;
    SZrTypeValue nativeValue;
    SEnvironmentMark mark;
    TZrBool success;
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(g_state->global));
    probe = prepare_probe("derive.invokeRuntime(import(\"zr.math\").abs)");
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, probe->type);
    TEST_ASSERT_EQUAL_UINT64(2U, probe->data.primaryExpression.members->count);
    call = probe->data.primaryExpression.members->nodes[1];
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, call->type);
    TEST_ASSERT_EQUAL_UINT64(1U, call->data.functionCall.args->count);
    mark = environment_mark();
    ZrCore_Value_ResetAsNull(&nativeValue);
    success = ZrParser_Compiler_EvaluateCompileTimeExpression(
            &g_compiler, call->data.functionCall.args->nodes[0], &nativeValue);
    assert_environment_restored(mark);
    TEST_ASSERT_TRUE_MESSAGE(success, g_compiler.errorMessage);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, nativeValue.type);
    TEST_ASSERT_TRUE(nativeValue.isNative);
    TEST_ASSERT_NOT_NULL(nativeValue.value.object);
    TEST_ASSERT_EQUAL_INT(ZR_RAW_OBJECT_TYPE_CLOSURE, nativeValue.value.object->type);
    TEST_ASSERT_NOT_NULL(((SZrClosureNative *)nativeValue.value.object)->nativeFunction);
    assert_runtime_callable_rejected(probe, provider_return_expression("invokeRuntime"));
}

static void test_private_callback_from_borrowed_parent_provider_is_callable(void) {
    SZrAstNode *probe = prepare_probe("returnedCallbackCaller()");
    const SZrImportedCompileTimeModuleAlias *alias;
    SZrTypeValue result;
    TZrSize roots;
    TZrSize bindings;
    TZrSize aliases;
    TZrBool success;
    ZrParser_CompilerState_Init(&g_childCompiler, g_state);
    g_childCompilerInitialized = ZR_TRUE;
    g_childCompiler.suppressErrorOutput = ZR_TRUE;
    g_childCompiler.compileToolProviderParent = &g_compiler;
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &g_childCompiler, g_ast), g_childCompiler.errorMessage);
    TEST_ASSERT_EQUAL_UINT64(0U, g_childCompiler.ownedCompileToolProviders.length);
    TEST_ASSERT_EQUAL_UINT64(0U, g_childCompiler.importedCompileTimeModules.length);
    TEST_ASSERT_EQUAL_UINT64(1U, g_childCompiler.compileToolBindings.length);
    TEST_ASSERT_EQUAL_UINT64(1U, g_childCompiler.importedCompileTimeModuleAliases.length);
    alias = (const SZrImportedCompileTimeModuleAlias *)ZrCore_Array_Get(
            &g_childCompiler.importedCompileTimeModuleAliases, 0U);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_EQUAL_PTR(g_providerModule, alias->module);
    TEST_ASSERT_NOT_NULL(record_named(&g_childCompiler.compileTimeFunctions, "returnedCallbackCaller"));
    g_childCompiler.comptimeContext = ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE;
    g_childCompiler.compilePhase = ZR_PARSER_COMPILE_PHASE_EXPANSION;
    roots = ZrCore_GcDomain_GetRootCount(g_state);
    bindings = g_childCompiler.compileToolBindings.length;
    aliases = g_childCompiler.importedCompileTimeModuleAliases.length;
    ZrCore_Value_ResetAsNull(&result);
    success = ZrParser_Compiler_EvaluateCompileTimeExpression(&g_childCompiler, probe, &result);
    TEST_ASSERT_EQUAL_UINT64(roots, ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_EQUAL_UINT64(bindings, g_childCompiler.compileToolBindings.length);
    TEST_ASSERT_EQUAL_UINT64(aliases, g_childCompiler.importedCompileTimeModuleAliases.length);
    TEST_ASSERT_NULL(g_childCompiler.activeImportedCompileTimeModule);
    TEST_ASSERT_EQUAL_UINT32(0U, g_childCompiler.comptimeCallDepth);
    TEST_ASSERT_TRUE_MESSAGE(success, g_childCompiler.errorMessage);
    TEST_ASSERT_FALSE(g_childCompiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(11, result.value.nativeObject.nativeInt64);
    assert_provider_identity();
}

int main(void) {
    int result;
    UNITY_BEGIN();
    RUN_TEST(test_provider_private_helper_wins_over_consumer_homonym);
    RUN_TEST(test_provider_miss_does_not_capture_consumer_function);
    RUN_TEST(test_omitted_default_uses_provider_environment);
    RUN_TEST(test_positional_argument_uses_consumer_environment);
    RUN_TEST(test_named_argument_uses_consumer_environment);
    RUN_TEST(test_explicit_consumer_callback_remains_callable);
    RUN_TEST(test_prebound_consumer_local_callback_remains_callable);
    RUN_TEST(test_provider_body_does_not_capture_consumer_nested_local);
    RUN_TEST(test_provider_nested_block_keeps_lexical_parent);
    RUN_TEST(test_provider_definition_survives_full_gc_after_import);
    RUN_TEST(test_explicit_callback_survives_full_gc_after_import);
    RUN_TEST(test_private_provider_function_is_not_external_member);
    RUN_TEST(test_omitted_default_reads_earlier_parameter);
    RUN_TEST(test_omitted_default_reads_named_earlier_parameter);
    RUN_TEST(test_later_argument_uses_caller_variable_over_callee_parameter);
    RUN_TEST(test_named_argument_uses_caller_variable_over_callee_parameter);
    RUN_TEST(test_explicit_and_default_scalar_cache_replay_survives_gc);
    RUN_TEST(test_earlier_aggregate_argument_survives_gc_during_later_argument);
    RUN_TEST(test_private_provider_function_value_is_callable_locally);
    RUN_TEST(test_private_provider_function_value_can_be_returned_to_consumer);
    RUN_TEST(test_returned_private_callback_can_be_passed_to_provider);
    RUN_TEST(test_private_provider_function_value_survives_full_gc);
    RUN_TEST(test_returned_private_callback_value_preserves_provider_identity);
    RUN_TEST(test_noncallable_value_keeps_effect_rejection);
    RUN_TEST(test_real_native_closure_keeps_effect_rejection);
    RUN_TEST(test_private_callback_from_borrowed_parent_provider_is_callable);
    result = UNITY_END();
    printf("FIXTURE_CLEANUP files=%zu bytes=%zu errors=%zu\n",
            (size_t)g_removedFixtureFiles, (size_t)g_removedFixtureBytes, (size_t)g_cleanupFailures);
    return g_cleanupFailures == 0U ? result : 1;
}
