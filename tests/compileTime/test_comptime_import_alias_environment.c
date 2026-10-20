#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler/compile_time/compile_time_import.h"
#include "compiler/tool/compile_tool_binding.h"
#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/project.h"
#include "zr_vm_library/zrm.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compile_tool.h"
#include "zr_vm_parser/compiler.h"
#include "contract/declaration_transform_contract.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/project_imports.h"

static const TZrChar g_buildProviderSource[] =
        "let tool = import(\"zr.compile\");\n"
        "pub comptime fn ownFeature(): bool { return tool.build.feature(\"trace\"); }\n"
        "pub comptime fn ownDisabled(): bool { return tool.build.feature(\"disabled\"); }\n"
        "pub comptime fn ownUnknown(): bool { return tool.build.feature(\"missing\"); }\n"
        "pub comptime fn missingAlias(): bool { return consumerTool.build.feature(\"trace\"); }\n"
        "pub comptime fn sameOwnerCaller(): bool { let tool = 44; return ownFeature(); }\n"
        "pub comptime fn localValue(): int { let tool = 44; return tool; }\n"
        "pub comptime fn localShadow(): bool { let tool = 44; return tool.build.feature(\"trace\"); }\n";

static const TZrChar g_declarationProviderSource[] =
        "let tool = import(\"zr.compile.declaration\");\n"
        "pub comptime fn ownVisibility(): int { return tool.Visibility.public; }\n"
        "pub comptime fn invokeThenOwner(callback: fn() -> bool): int {\n"
        "    let enabled = callback();\n"
        "    if (enabled) { return tool.Visibility.public; }\n"
        "    return tool.Visibility.private;\n"
        "}\n";

static const TZrChar g_consumerPrefix[] = "let derive = import(\"@derive\");\n";

static SZrState *g_state;
static SZrLibrary_Project *g_project;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrAstNode *g_probeBlock;
static TZrByte *g_archiveBytes;
static TZrSize g_archiveByteCount;
static TZrChar g_sourcePath[ZR_TESTS_PATH_MAX];
static TZrChar g_archivePath[ZR_TESTS_PATH_MAX];
static TZrBool g_ownedSource;
static TZrBool g_ownedArchive;
static TZrBool g_sourcePinned;
static TZrBool g_archivePinned;
static TZrChar g_sourceHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
static TZrChar g_archiveHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
static TZrSize g_removedFixtureBytes;
static TZrSize g_removedFixtureFiles;
static TZrSize g_cleanupFailures;

static void cleanup_owner_case_resources(void);

void setUp(void) {
    g_state = ZR_NULL;
    g_project = ZR_NULL;
    g_ast = ZR_NULL;
    g_probeBlock = ZR_NULL;
    g_archiveBytes = ZR_NULL;
    g_archiveByteCount = 0U;
    g_sourcePath[0] = '\0';
    g_archivePath[0] = '\0';
    g_ownedSource = ZR_FALSE;
    g_ownedArchive = ZR_FALSE;
    g_sourcePinned = ZR_FALSE;
    g_archivePinned = ZR_FALSE;
    g_sourceHash[0] = '\0';
    g_archiveHash[0] = '\0';
    g_compilerInitialized = ZR_FALSE;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    g_compiler.suppressErrorOutput = ZR_TRUE;
}

static TZrBool record_fixture_hash(const TZrChar *path, TZrChar *hash) {
    TZrByte *bytes = ZR_NULL;
    TZrSize byteCount = 0U;
    TZrBool success = ZrTests_ReadFileBytes(path, &bytes, &byteCount) &&
            ZrParser_CompileToolContentHash_Bytes(
                    bytes, byteCount, hash, ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH);
    free(bytes);
    return success;
}

static void remove_owned_fixture(
        const TZrChar *path, TZrBool owned, TZrBool pinned, const TZrChar *expectedHash) {
    TZrByte *bytes = ZR_NULL;
    TZrSize byteCount = 0U;
    TZrChar currentHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH] = {0};
    TZrBool verified;
    if (!owned) {
        return;
    }
    if (!pinned) {
        g_cleanupFailures++;
        printf("FIXTURE_CLEANUP_UNPINNED=%s\n", path);
        return;
    }
    verified = ZrTests_ReadFileBytes(path, &bytes, &byteCount) &&
            ZrParser_CompileToolContentHash_Bytes(
                    bytes, byteCount, currentHash, sizeof(currentHash));
    free(bytes);
    if (!verified || strcmp(expectedHash, currentHash) != 0) {
        g_cleanupFailures++;
        printf("FIXTURE_CLEANUP_HASH_FAILED=%s expected=%s actual=%s\n", path, expectedHash, currentHash);
        return;
    }
    if (remove(path) != 0) {
        g_cleanupFailures++;
        printf("FIXTURE_CLEANUP_FAILED=%s\n", path);
        return;
    }
    g_removedFixtureFiles++;
    g_removedFixtureBytes += byteCount;
}

void tearDown(void) {
    cleanup_owner_case_resources();
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
    g_probeBlock = ZR_NULL;
    remove_owned_fixture(g_archivePath, g_ownedArchive, g_archivePinned, g_archiveHash);
    remove_owned_fixture(g_sourcePath, g_ownedSource, g_sourcePinned, g_sourceHash);
    g_ownedSource = ZR_FALSE;
    g_ownedArchive = ZR_FALSE;
}

static void generated_path(
        const TZrChar *baseName, const TZrChar *extension, TZrChar *path) {
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "compileTime", "comptime_import_alias_environment", baseName, extension,
            path, ZR_TESTS_PATH_MAX));
    for (TZrSize i = 0U; path[i] != '\0'; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
}

static TZrBool write_provider_source(const TZrChar *source) {
    FILE *file = fopen(g_sourcePath, "wb");
    TZrSize length = strlen(source);
    TZrBool success;
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    g_ownedSource = ZR_TRUE;
    success = fwrite(source, 1U, length, file) == length;
    return fclose(file) == 0 && success;
}

static void create_locked_project(const TZrChar *providerSource) {
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
    generated_path("m1_import_alias_environment_provider", ".zr", g_sourcePath);
    generated_path("m1_import_alias_environment_provider", ".zrm", g_archivePath);
    generated_path("m1_import_alias_environment_consumer", ".zrp", projectPath);
    TEST_ASSERT_FALSE_MESSAGE(ZrTests_File_Exists(g_sourcePath), "Unowned provider source exists");
    TEST_ASSERT_FALSE_MESSAGE(ZrTests_File_Exists(g_archivePath), "Unowned provider archive exists");
    printf("FIXTURE_SOURCE=%s\nFIXTURE_ARCHIVE=%s\n", g_sourcePath, g_archivePath);
    TEST_ASSERT_TRUE(write_provider_source(providerSource));
    g_sourcePinned = record_fixture_hash(g_sourcePath, g_sourceHash);
    TEST_ASSERT_TRUE_MESSAGE(g_sourcePinned, "Created provider source could not be pinned");
    printf("FIXTURE_PIN file=%s hash=%s\n", g_sourcePath, g_sourceHash);
    TEST_ASSERT_TRUE(ZrParser_CompileToolContentHash_Bytes(
            (const TZrByte *)providerSource, strlen(providerSource),
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
    memcpy(g_archiveHash, archiveHash, strlen(archiveHash) + 1U);
    g_archivePinned = ZR_TRUE;
    printf("FIXTURE_PIN file=%s hash=%s\n", g_archivePath, g_archiveHash);
    free(g_archiveBytes);
    g_archiveBytes = ZR_NULL;
    written = snprintf(manifest, sizeof(manifest),
            "{\"manifestVersion\":2,\"name\":\"consumer\","
            "\"version\":\"1.0.0\",\"kind\":\"executable\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\","
            "\"features\":{\"trace\":true,\"disabled\":false},"
            "\"dependencies\":{},\"buildDependencies\":{"
            "\"@derive\":{\"version\":\"^1.0.0\",\"path\":\"%s\"}}}", g_archivePath);
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

static SZrAstNode *script_declaration(SZrAstNode *script, const TZrChar *name) {
    SZrAstNodeArray *statements;
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    statements = script->data.script.statements;
    TEST_ASSERT_NOT_NULL(statements);
    for (TZrSize i = 0U; i < statements->count; i++) {
        SZrAstNode *node = statements->nodes[i];
        SZrString *nodeName = ZR_NULL;
        if (node != ZR_NULL && node->type == ZR_AST_COMPILE_TIME_DECLARATION &&
            node->data.compileTimeDeclaration.declarationType == ZR_COMPILE_TIME_FUNCTION) {
            node = node->data.compileTimeDeclaration.declaration;
        }
        if (node != ZR_NULL && node->type == ZR_AST_VARIABLE_DECLARATION &&
            node->data.variableDeclaration.pattern != ZR_NULL &&
            node->data.variableDeclaration.pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
            nodeName = node->data.variableDeclaration.pattern->data.identifier.name;
        } else if (node != ZR_NULL && node->type == ZR_AST_FUNCTION_DECLARATION &&
                   node->data.functionDeclaration.name != ZR_NULL) {
            nodeName = node->data.functionDeclaration.name->name;
        }
        if (nodeName != ZR_NULL && strcmp(name, ZrCore_String_GetNativeString(nodeName)) == 0) {
            return node;
        }
    }
    return ZR_NULL;
}

static SZrAstNode *assert_import(SZrAstNode *script, const TZrChar *alias, const TZrChar *moduleName) {
    SZrAstNode *declaration = script_declaration(script, alias);
    SZrAstNode *value;
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    value = declaration->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IMPORT_EXPRESSION, value->type);
    TEST_ASSERT_NOT_NULL(value->data.importExpression.modulePath);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRING_LITERAL, value->data.importExpression.modulePath->type);
    TEST_ASSERT_EQUAL_STRING(moduleName, ZrCore_String_GetNativeString(
            value->data.importExpression.modulePath->data.stringLiteral.value));
    return declaration;
}

static SZrImportedCompileTimeModule *provider_module(void) {
    for (TZrSize i = 0U; i < g_compiler.importedCompileTimeModules.length; i++) {
        SZrImportedCompileTimeModule *module = *(SZrImportedCompileTimeModule **)ZrCore_Array_Get(
                &g_compiler.importedCompileTimeModules, i);
        if (module != ZR_NULL && module->moduleName != ZR_NULL &&
            strcmp("@derive", ZrCore_String_GetNativeString(module->moduleName)) == 0) {
            return module;
        }
    }
    return ZR_NULL;
}

static SZrCompileTimeFunction *record_named(SZrArray *records, const TZrChar *name) {
    for (TZrSize i = 0U; i < records->length; i++) {
        SZrCompileTimeFunction *record = *(SZrCompileTimeFunction **)ZrCore_Array_Get(records, i);
        if (record != ZR_NULL && record->name != ZR_NULL &&
            strcmp(name, ZrCore_String_GetNativeString(record->name)) == 0) {
            return record;
        }
    }
    return ZR_NULL;
}

static void assert_source_node(SZrAstNode *node, const TZrChar *source) {
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_NOT_NULL(node->location.source);
    TEST_ASSERT_EQUAL_STRING(source, ZrCore_String_GetNativeString(node->location.source));
    TEST_ASSERT_TRUE(node->location.start.line > 0);
    TEST_ASSERT_TRUE(node->location.end.offset > node->location.start.offset);
}

static SZrCompileTimeFunction *assert_provider_function(const TZrChar *name) {
    SZrImportedCompileTimeModule *module = provider_module();
    SZrCompileTimeFunction *record;
    TEST_ASSERT_NOT_NULL(module);
    record = record_named(&module->compileTimeFunctions, name);
    TEST_ASSERT_NOT_NULL(record);
    TEST_ASSERT_TRUE(record->isExported);
    TEST_ASSERT_FALSE(record->isRuntimeProjection);
    TEST_ASSERT_FALSE(record->isDeclarationTransform);
    TEST_ASSERT_EQUAL_PTR(module, record->ownerModule);
    TEST_ASSERT_NOT_NULL(record->declaration);
    TEST_ASSERT_EQUAL_PTR(script_declaration(module->scriptAst, name), record->declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, record->declaration->type);
    TEST_ASSERT_NOT_NULL(record->location.source);
    TEST_ASSERT_EQUAL_STRING("@derive", ZrCore_String_GetNativeString(record->location.source));
    assert_source_node(record->declaration, "@derive");
    return record;
}

static void assert_binding(SZrAstNode *declaration, EZrCompileToolBindingKind kind, const TZrChar *moduleName) {
    const SZrCompileToolBinding *binding = ZrParser_CompileToolBinding_Resolve(
            &g_compiler, declaration->data.variableDeclaration.pattern->data.identifier.name);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_INT(kind, binding->kind);
    if (kind == ZR_COMPILE_TOOL_BINDING_PROVIDER) {
        TEST_ASSERT_NOT_NULL(binding->provider);
        TEST_ASSERT_EQUAL_STRING(moduleName, binding->provider->moduleName);
    } else {
        TEST_ASSERT_NULL(binding->provider);
    }
}

static void assert_project_and_provider(const TZrChar *providerImport) {
    const SZrCompileToolBinding *binding;
    SZrAstNode *derive = assert_import(g_ast, "derive", "@derive");
    SZrImportedCompileTimeModule *module = provider_module();
    TZrBool foundTrace = ZR_FALSE;
    TZrBool foundDisabled = ZR_FALSE;
    TEST_ASSERT_EQUAL_UINT64(2U, g_project->featureSwitchCount);
    for (TZrSize i = 0U; i < g_project->featureSwitchCount; i++) {
        const SZrLibrary_ProjectFeatureSwitch *feature = &g_project->featureSwitches[i];
        TEST_ASSERT_NOT_NULL(feature->name);
        if (strcmp("trace", ZrCore_String_GetNativeString(feature->name)) == 0) {
            TEST_ASSERT_TRUE(feature->value);
            foundTrace = ZR_TRUE;
        } else if (strcmp("disabled", ZrCore_String_GetNativeString(feature->name)) == 0) {
            TEST_ASSERT_FALSE(feature->value);
            foundDisabled = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE(foundTrace);
    TEST_ASSERT_TRUE(foundDisabled);
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_NOT_NULL(module->scriptAst);
    assert_import(module->scriptAst, "tool", providerImport);
    TEST_ASSERT_EQUAL_UINT64(1U, g_compiler.ownedCompileToolProviders.length);
    TEST_ASSERT_EQUAL_UINT64(1U, g_compiler.importedCompileTimeModules.length);
    assert_binding(derive, ZR_COMPILE_TOOL_BINDING_PROVIDER, "@derive");
    binding = ZrParser_CompileToolBinding_Resolve(
            &g_compiler, derive->data.variableDeclaration.pattern->data.identifier.name);
    TEST_ASSERT_NOT_NULL(binding->resolvedArtifact);
    TEST_ASSERT_TRUE(ZrParser_CompileToolArtifact_IsOpen(binding->resolvedArtifact));
    TEST_ASSERT_NOT_NULL(binding->providerContentHash);
    TEST_ASSERT_NULL(g_compiler.activeImportedCompileTimeModule);
    TEST_ASSERT_EQUAL_UINT64(0U, g_compiler.comptimeCache.length);
}

static void capture_parse_error(
        TZrPtr userData, const SZrFileRange *location, const TZrChar *message, EZrToken token) {
    TZrChar *error = (TZrChar *)userData;
    (void)location;
    (void)token;
    if (error != ZR_NULL && error[0] == '\0') {
        snprintf(error, 512U, "Consumer parse: %s", message != ZR_NULL ? message : "unknown error");
    }
}

static void prepare_source(
        const TZrChar *providerSource, const TZrChar *prefix, const TZrChar *statements,
        const TZrChar *providerImport) {
    TZrChar source[4096];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar error[512] = {0};
    SZrFileRange errorLocation = {{0, 0, 0}, {0, 0, 0}, ZR_NULL};
    SZrString *sourceName;
    SZrString *moduleKey = ZR_NULL;
    SZrParserState parser;
    SZrAstNode *wrapper;
    TZrBool hadError;
    int written;

    create_locked_project(providerSource);
    written = snprintf(sourcePath, sizeof(sourcePath), "%s/src/main.zr",
            ZrCore_String_GetNativeString(g_project->directory));
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(sourcePath));
    written = snprintf(source, sizeof(source), "%scomptime { %s }\n", prefix, statements);
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
    TEST_ASSERT_EQUAL_PTR(g_ast, g_compiler.scriptAst);
    assert_project_and_provider(providerImport);
    wrapper = g_ast->data.script.statements->nodes[g_ast->data.script.statements->count - 1U];
    TEST_ASSERT_NOT_NULL(wrapper);
    TEST_ASSERT_EQUAL_INT(ZR_AST_COMPILE_TIME_DECLARATION, wrapper->type);
    TEST_ASSERT_EQUAL_INT(ZR_COMPILE_TIME_STATEMENT, wrapper->data.compileTimeDeclaration.declarationType);
    TEST_ASSERT_FALSE(wrapper->data.compileTimeDeclaration.isConditionalPruning);
    g_probeBlock = wrapper->data.compileTimeDeclaration.declaration;
    TEST_ASSERT_NOT_NULL(g_probeBlock);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, g_probeBlock->type);
    TEST_ASSERT_NOT_NULL(g_probeBlock->data.block.body);
    g_compiler.compilePhase = ZR_PARSER_COMPILE_PHASE_EXPANSION;
    g_compiler.comptimeContext = ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE;
}

static SZrAstNode *probe_expression(const TZrChar *name) {
    SZrAstNodeArray *body = g_probeBlock->data.block.body;
    for (TZrSize i = 0U; i < body->count; i++) {
        SZrAstNode *node = body->nodes[i];
        SZrAstNode *pattern;
        TEST_ASSERT_NOT_NULL(node);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, node->type);
        pattern = node->data.variableDeclaration.pattern;
        TEST_ASSERT_NOT_NULL(pattern);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, pattern->type);
        if (strcmp(name, ZrCore_String_GetNativeString(pattern->data.identifier.name)) == 0) {
            TEST_ASSERT_NOT_NULL(node->data.variableDeclaration.value);
            return node->data.variableDeclaration.value;
        }
    }
    TEST_FAIL_MESSAGE("Missing real comptime probe initializer");
    return ZR_NULL;
}

static SZrAstNode *prepare_probe(const TZrChar *prefix, const TZrChar *expression) {
    TZrChar statements[512];
    int written = snprintf(statements, sizeof(statements), "let probe = %s;", expression);
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(statements));
    prepare_source(g_buildProviderSource, prefix, statements, ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
    TEST_ASSERT_EQUAL_UINT64(1U, g_probeBlock->data.block.body->count);
    return probe_expression("probe");
}

typedef struct SEnvironmentMark {
    TZrSize bindings;
    TZrSize aliases;
    TZrSize roots;
    TZrUInt32 depth;
    SZrImportedCompileTimeModule *owner;
    EZrParserCompilePhase phase;
    EZrParserComptimeContext context;
} SEnvironmentMark;

static SEnvironmentMark environment_mark(void) {
    SEnvironmentMark mark;
    mark.bindings = g_compiler.compileToolBindings.length;
    mark.aliases = g_compiler.importedCompileTimeModuleAliases.length;
    mark.roots = ZrCore_GcDomain_GetRootCount(g_state);
    mark.depth = g_compiler.comptimeCallDepth;
    mark.owner = g_compiler.activeImportedCompileTimeModule;
    mark.phase = g_compiler.compilePhase;
    mark.context = g_compiler.comptimeContext;
    return mark;
}

static void assert_environment_restored(SEnvironmentMark mark) {
    TEST_ASSERT_EQUAL_UINT64(mark.bindings, g_compiler.compileToolBindings.length);
    TEST_ASSERT_EQUAL_UINT64(mark.aliases, g_compiler.importedCompileTimeModuleAliases.length);
    TEST_ASSERT_EQUAL_UINT64(mark.roots, ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_EQUAL_UINT32(mark.depth, g_compiler.comptimeCallDepth);
    TEST_ASSERT_EQUAL_PTR(mark.owner, g_compiler.activeImportedCompileTimeModule);
    TEST_ASSERT_EQUAL_INT(mark.phase, g_compiler.compilePhase);
    TEST_ASSERT_EQUAL_INT(mark.context, g_compiler.comptimeContext);
}

static void assert_value(SZrAstNode *probe, EZrValueType type, TZrInt64 expected) {
    SZrTypeValue result;
    SEnvironmentMark mark = environment_mark();
    TZrBool success;
    ZrCore_Value_ResetAsNull(&result);
    success = ZrParser_Compiler_EvaluateCompileTimeExpression(&g_compiler, probe, &result);
    assert_environment_restored(mark);
    TEST_ASSERT_TRUE_MESSAGE(success,
            g_compiler.errorMessage != ZR_NULL ? g_compiler.errorMessage : "evaluation failed");
    TEST_ASSERT_FALSE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_INT(type, result.type);
    if (type == ZR_VALUE_TYPE_BOOL) {
        TEST_ASSERT_EQUAL_INT(expected, result.value.nativeObject.nativeBool != 0U);
    } else {
        TEST_ASSERT_EQUAL_INT64(expected, result.value.nativeObject.nativeInt64);
    }
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

static void assert_failure(SZrAstNode *probe, SZrAstNode *location, const TZrChar *message) {
    SZrTypeValue result;
    SEnvironmentMark mark = environment_mark();
    TZrBool success;
    assert_source_node(location, "@derive");
    ZrCore_Value_ResetAsNull(&result);
    success = ZrParser_Compiler_EvaluateCompileTimeExpression(&g_compiler, probe, &result);
    assert_environment_restored(mark);
    TEST_ASSERT_FALSE_MESSAGE(success, "CompileTool lookup must respect the defining environment and locals");
    TEST_ASSERT_TRUE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result.type));
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_EQUAL_STRING(message, g_compiler.errorMessage);
    assert_error_range(location);
}

static SZrAstNode *provider_return_expression(const TZrChar *name, TZrSize statementIndex) {
    SZrCompileTimeFunction *record = assert_provider_function(name);
    SZrAstNode *body = record->declaration->data.functionDeclaration.body;
    SZrAstNode *statement;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_TRUE(statementIndex < body->data.block.body->count);
    statement = body->data.block.body->nodes[statementIndex];
    TEST_ASSERT_NOT_NULL(statement);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, statement->type);
    TEST_ASSERT_NOT_NULL(statement->data.returnStatement.expr);
    return statement->data.returnStatement.expr;
}

static SZrAstNode *assert_feature_expression(SZrAstNode *expression, const TZrChar *alias, const TZrChar *feature) {
    SZrPrimaryExpression *primary;
    SZrAstNode *call;
    SZrAstNode *argument;
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    primary = &expression->data.primaryExpression;
    TEST_ASSERT_NOT_NULL(primary->property);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, primary->property->type);
    TEST_ASSERT_EQUAL_STRING(alias, ZrCore_String_GetNativeString(primary->property->data.identifier.name));
    TEST_ASSERT_NOT_NULL(primary->members);
    TEST_ASSERT_EQUAL_UINT64(3U, primary->members->count);
    for (TZrSize i = 0U; i < 2U; i++) {
        SZrAstNode *member = primary->members->nodes[i];
        TEST_ASSERT_NOT_NULL(member);
        TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, member->type);
        TEST_ASSERT_FALSE(member->data.memberExpression.computed);
        TEST_ASSERT_NOT_NULL(member->data.memberExpression.property);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, member->data.memberExpression.property->type);
        TEST_ASSERT_EQUAL_STRING(i == 0U ? "build" : "feature", ZrCore_String_GetNativeString(
                member->data.memberExpression.property->data.identifier.name));
    }
    call = primary->members->nodes[2];
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, call->type);
    TEST_ASSERT_NOT_NULL(call->data.functionCall.args);
    TEST_ASSERT_EQUAL_UINT64(1U, call->data.functionCall.args->count);
    argument = call->data.functionCall.args->nodes[0];
    TEST_ASSERT_NOT_NULL(argument);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRING_LITERAL, argument->type);
    TEST_ASSERT_EQUAL_STRING(feature, ZrCore_String_GetNativeString(argument->data.stringLiteral.value));
    return argument;
}

static void assert_local_tool(const TZrChar *name) {
    SZrCompileTimeFunction *record = assert_provider_function(name);
    SZrAstNode *body = record->declaration->data.functionDeclaration.body;
    SZrAstNode *local;
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, body->type);
    TEST_ASSERT_NOT_NULL(body->data.block.body);
    TEST_ASSERT_EQUAL_UINT64(2U, body->data.block.body->count);
    local = body->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(local);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, local->type);
    TEST_ASSERT_NOT_NULL(local->data.variableDeclaration.pattern);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, local->data.variableDeclaration.pattern->type);
    TEST_ASSERT_EQUAL_STRING("tool", ZrCore_String_GetNativeString(
            local->data.variableDeclaration.pattern->data.identifier.name));
    TEST_ASSERT_NOT_NULL(local->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, local->data.variableDeclaration.value->type);
    TEST_ASSERT_EQUAL_INT64(44, local->data.variableDeclaration.value->data.integerLiteral.value);
}

static void test_provider_own_import_reads_declared_true_feature(void) {
    SZrAstNode *probe = prepare_probe(g_consumerPrefix, "derive.ownFeature()");
    assert_feature_expression(provider_return_expression("ownFeature", 0U), "tool", "trace");
    assert_value(probe, ZR_VALUE_TYPE_BOOL, 1);
}

static void test_provider_own_import_reads_declared_false_feature(void) {
    SZrAstNode *probe = prepare_probe(g_consumerPrefix, "derive.ownDisabled()");
    assert_feature_expression(provider_return_expression("ownDisabled", 0U), "tool", "disabled");
    assert_value(probe, ZR_VALUE_TYPE_BOOL, 0);
}

static void test_provider_unknown_feature_preserves_diagnostic_range(void) {
    SZrAstNode *probe = prepare_probe(g_consumerPrefix, "derive.ownUnknown()");
    SZrAstNode *argument = assert_feature_expression(
            provider_return_expression("ownUnknown", 0U), "tool", "missing");
    assert_failure(probe, argument, "Unknown project feature in compile.build.feature");
}

static void test_provider_alias_miss_does_not_capture_consumer_import(void) {
    SZrAstNode *probe = prepare_probe(
            "let consumerTool = import(\"zr.compile\");\nlet derive = import(\"@derive\");\n",
            "derive.missingAlias()");
    SZrAstNode *expression = provider_return_expression("missingAlias", 0U);
    TEST_ASSERT_NULL(script_declaration(provider_module()->scriptAst, "consumerTool"));
    assert_binding(assert_import(g_ast, "consumerTool", ZR_PARSER_COMPILE_TOOL_MODULE_BUILD),
            ZR_COMPILE_TOOL_BINDING_PROVIDER, ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
    assert_feature_expression(expression, "consumerTool", "trace");
    assert_failure(probe, expression->data.primaryExpression.property,
            "Unknown compile-time identifier: consumerTool");
}

static void test_consumer_shadow_does_not_suppress_provider_own_import(void) {
    SZrAstNode *probe = prepare_probe("let tool = 44;\nlet derive = import(\"@derive\");\n", "derive.ownFeature()");
    SZrAstNode *shadow = script_declaration(g_ast, "tool");
    TEST_ASSERT_NOT_NULL(shadow);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, shadow->type);
    TEST_ASSERT_NOT_NULL(shadow->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, shadow->data.variableDeclaration.value->type);
    TEST_ASSERT_EQUAL_INT64(44, shadow->data.variableDeclaration.value->data.integerLiteral.value);
    assert_binding(shadow, ZR_COMPILE_TOOL_BINDING_SHADOW, ZR_NULL);
    assert_feature_expression(provider_return_expression("ownFeature", 0U), "tool", "trace");
    assert_value(probe, ZR_VALUE_TYPE_BOOL, 1);
    assert_binding(shadow, ZR_COMPILE_TOOL_BINDING_SHADOW, ZR_NULL);
}

static void test_same_owner_caller_local_does_not_shadow_sibling_import(void) {
    SZrAstNode *probe = prepare_probe(g_consumerPrefix, "derive.sameOwnerCaller()");
    assert_local_tool("sameOwnerCaller");
    assert_feature_expression(provider_return_expression("ownFeature", 0U), "tool", "trace");
    assert_value(probe, ZR_VALUE_TYPE_BOOL, 1);
}

static void test_provider_local_value_resolves_real_local_shadow(void) {
    SZrAstNode *probe = prepare_probe(g_consumerPrefix, "derive.localValue()");
    SZrAstNode *identifier = provider_return_expression("localValue", 1U);
    assert_local_tool("localValue");
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, identifier->type);
    TEST_ASSERT_EQUAL_STRING("tool", ZrCore_String_GetNativeString(identifier->data.identifier.name));
    assert_value(probe, ZR_VALUE_TYPE_INT64, 44);
}

static void test_provider_direct_local_shadow_blocks_compile_tool_member_dispatch(void) {
    SZrAstNode *probe = prepare_probe(g_consumerPrefix, "derive.localShadow()");
    SZrAstNode *expression = provider_return_expression("localShadow", 1U);
    assert_local_tool("localShadow");
    assert_feature_expression(expression, "tool", "trace");
    assert_failure(probe, expression->data.primaryExpression.members->nodes[0],
            "Compile-time member access requires object or array value");
}

static void test_root_callback_uses_own_import_and_restores_provider_across_cache_and_gc(void) {
    static const TZrChar *prefix =
            "let tool = import(\"zr.compile\");\n"
            "let derive = import(\"@derive\");\n"
            "comptime fn rootFeature(): bool { return tool.build.feature(\"trace\"); }\n";
    static const TZrChar *statements =
            "let rootControl = rootFeature();\n"
            "let ownerControl = derive.ownVisibility();\n"
            "let probe = derive.invokeThenOwner(rootFeature);\n";
    SZrCompileTimeFunction *root;
    SZrAstNode *probe;
    SZrAstNode *call;
    SZrAstNode *argument;
    TZrUInt64 hits;
    SEnvironmentMark mark;
    prepare_source(g_declarationProviderSource, prefix, statements, ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION);
    TEST_ASSERT_EQUAL_UINT64(3U, g_probeBlock->data.block.body->count);
    assert_binding(assert_import(g_ast, "tool", ZR_PARSER_COMPILE_TOOL_MODULE_BUILD),
            ZR_COMPILE_TOOL_BINDING_PROVIDER, ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
    root = record_named(&g_compiler.compileTimeFunctions, "rootFeature");
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NULL(root->ownerModule);
    TEST_ASSERT_FALSE(root->isRuntimeProjection);
    TEST_ASSERT_FALSE(root->isExported);
    TEST_ASSERT_FALSE(root->isDeclarationTransform);
    TEST_ASSERT_NOT_NULL(root->declaration);
    TEST_ASSERT_EQUAL_PTR(script_declaration(g_ast, "rootFeature"), root->declaration);
    TEST_ASSERT_EQUAL_PTR(root->declaration->location.source, root->location.source);
    TEST_ASSERT_EQUAL_PTR(assert_import(g_ast, "tool", ZR_PARSER_COMPILE_TOOL_MODULE_BUILD)->location.source,
            root->location.source);
    assert_provider_function("invokeThenOwner");
    assert_provider_function("ownVisibility");
    assert_value(probe_expression("rootControl"), ZR_VALUE_TYPE_BOOL, 1);
    printf("CALLBACK_PREREQUISITE root_feature=PASS\n");
    assert_value(probe_expression("ownerControl"), ZR_VALUE_TYPE_INT64, ZR_PARSER_GENERATED_VISIBILITY_PUBLIC);
    printf("CALLBACK_PREREQUISITE provider_visibility=PASS\n");
    probe = probe_expression("probe");
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, probe->type);
    TEST_ASSERT_NOT_NULL(probe->data.primaryExpression.members);
    TEST_ASSERT_EQUAL_UINT64(2U, probe->data.primaryExpression.members->count);
    call = probe->data.primaryExpression.members->nodes[1];
    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, call->type);
    TEST_ASSERT_NOT_NULL(call->data.functionCall.args);
    TEST_ASSERT_EQUAL_UINT64(1U, call->data.functionCall.args->count);
    argument = call->data.functionCall.args->nodes[0];
    TEST_ASSERT_NOT_NULL(argument);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, argument->type);
    TEST_ASSERT_EQUAL_STRING("rootFeature", ZrCore_String_GetNativeString(argument->data.identifier.name));
    assert_value(probe, ZR_VALUE_TYPE_INT64, ZR_PARSER_GENERATED_VISIBILITY_PUBLIC);
    /* The function-valued outer argument disables its cache; the Root callback can hit. */
    hits = g_compiler.comptimeCacheHitCount;
    assert_value(probe, ZR_VALUE_TYPE_INT64, ZR_PARSER_GENERATED_VISIBILITY_PUBLIC);
    TEST_ASSERT_TRUE(g_compiler.comptimeCacheHitCount > hits);
    hits = g_compiler.comptimeCacheHitCount;
    mark = environment_mark();
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    assert_environment_restored(mark);
    assert_provider_function("invokeThenOwner");
    root = record_named(&g_compiler.compileTimeFunctions, "rootFeature");
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NULL(root->ownerModule);
    assert_value(probe, ZR_VALUE_TYPE_INT64, ZR_PARSER_GENERATED_VISIBILITY_PUBLIC);
    TEST_ASSERT_TRUE(g_compiler.comptimeCacheHitCount > hits);
    assert_binding(assert_import(g_ast, "tool", ZR_PARSER_COMPILE_TOOL_MODULE_BUILD),
            ZR_COMPILE_TOOL_BINDING_PROVIDER, ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
}

#include "test_comptime_import_alias_owner_cases.h"

int main(void) {
    int result;
    UNITY_BEGIN();
    RUN_TEST(test_provider_own_import_reads_declared_true_feature);
    RUN_TEST(test_provider_own_import_reads_declared_false_feature);
    RUN_TEST(test_provider_unknown_feature_preserves_diagnostic_range);
    RUN_TEST(test_provider_alias_miss_does_not_capture_consumer_import);
    RUN_TEST(test_consumer_shadow_does_not_suppress_provider_own_import);
    RUN_TEST(test_same_owner_caller_local_does_not_shadow_sibling_import);
    RUN_TEST(test_provider_local_value_resolves_real_local_shadow);
    RUN_TEST(test_provider_direct_local_shadow_blocks_compile_tool_member_dispatch);
    RUN_TEST(test_root_callback_uses_own_import_and_restores_provider_across_cache_and_gc);
    RUN_TEST(test_root_owner_view_preserves_real_shadow_and_saved_rows_across_gc);
    RUN_TEST(test_root_owner_view_keeps_selected_conditional_import);
    RUN_TEST(test_validated_ancestor_function_value_uses_owner_import_in_source_call);
    RUN_TEST(test_root_owner_view_preserves_ordinary_source_import_alias_across_gc);
    result = UNITY_END();
    printf("FIXTURE_CLEANUP files=%zu bytes=%zu errors=%zu\n",
            (size_t)g_removedFixtureFiles, (size_t)g_removedFixtureBytes, (size_t)g_cleanupFailures);
    return g_cleanupFailures == 0U ? result : 1;
}
