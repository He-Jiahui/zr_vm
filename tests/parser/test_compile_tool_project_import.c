#include "unity.h"

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/project.h"
#include "zr_vm_library/zrm.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compile_tool.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/project_imports.h"
#include "compiler/compile_time_import.h"
#include "compiler/module_init_analysis.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unity 每个用例重建运行时；项目通过 global->userData 供导入解析和编译阶段读取。 */
static SZrState *g_state;
static SZrLibrary_Project *g_project;

/* 提供只有 buildDependencies、没有 lock 的基线项目，用于区分路径规范化与工具装载。 */
void setUp(void) {
    static TZrChar manifest[] =
            "{"
            "\"manifestVersion\":2,"
            "\"name\":\"consumer\","
            "\"version\":\"1.0.0\","
            "\"kind\":\"executable\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\","
            "\"dependencies\":{},"
            "\"buildDependencies\":{"
            "\"@derive\":{\"version\":\"^1.0.0\",\"path\":\"./derive.zrm\"}"
            "}"
            "}";

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    g_project = ZrLibrary_Project_New(
            g_state,
            manifest,
            "E:/repo/compile-tool-import/consumer.zrp");
    TEST_ASSERT_NOT_NULL(g_project);
    g_state->global->userData = g_project;
}

/* 用例正常退出或 Unity 断言跳转后均由此释放基线项目及运行时。 */
/* BUG: 断言跳过用例尾部时，局部 AST/CompilerState 等未交给此钩子，
 * 因而其原生分配不会随基线项目的释放得到完整回收。 */
void tearDown(void) {
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
}

/* 让导入断言复用同一源模块身份；返回 AST 由用例负责释放。 */
static SZrAstNode *parse_import_source(const TZrChar *specifier) {
    TZrChar source[256];
    SZrString *sourceName;
    int written = snprintf(
            source,
            sizeof(source),
            "let tool = import(\"%s\");\nreturn 0;\n",
            specifier);

    TEST_ASSERT_TRUE(written > 0);
    TEST_ASSERT_TRUE((TZrSize)written < sizeof(source));
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "E:/repo/compile-tool-import/src/main.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Parse(
            g_state, source, (TZrSize)written, sourceName);
}

/* 只读取测试脚本首条导入，借此判断 canonicalize 是否改写编译工具别名。 */
static const TZrChar *first_import_specifier(SZrAstNode *ast) {
    SZrAstNode *statement;
    SZrAstNode *value;
    SZrAstNode *modulePath;

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count == 0U) {
        return ZR_NULL;
    }
    statement = ast->data.script.statements->nodes[0];
    if (statement == ZR_NULL || statement->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_NULL;
    }
    value = statement->data.variableDeclaration.value;
    if (value == ZR_NULL || value->type != ZR_AST_IMPORT_EXPRESSION) {
        return ZR_NULL;
    }
    modulePath = value->data.importExpression.modulePath;
    if (modulePath == ZR_NULL || modulePath->type != ZR_AST_STRING_LITERAL ||
        modulePath->data.stringLiteral.value == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_GetNativeString(
            modulePath->data.stringLiteral.value);
}

/* 锁解析前的编译工具别名必须留给 build-facts 阶段，不能变成运行时模块键。 */
static void test_build_dependency_import_preserves_compile_tool_specifier(void) {
    SZrAstNode *ast = parse_import_source("@derive");
    SZrString *sourceName;
    SZrString *currentModuleKey = ZR_NULL;
    SZrFileRange errorLocation;
    TZrChar error[256];

    TEST_ASSERT_NOT_NULL(ast);
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "E:/repo/compile-tool-import/src/main.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&errorLocation, 0, sizeof(errorLocation));
    memset(error, 0, sizeof(error));

    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_ProjectImports_CanonicalizeAst(
                    g_state,
                    ast,
                    sourceName,
                    &currentModuleKey,
                    error,
                    sizeof(error),
                    &errorLocation),
            error);
    TEST_ASSERT_NOT_NULL(currentModuleKey);
    TEST_ASSERT_EQUAL_STRING("main", ZrCore_String_GetNativeString(currentModuleKey));
    TEST_ASSERT_EQUAL_STRING("@derive", first_import_specifier(ast));

    ZrParser_Ast_Free(g_state, ast);
}

/* 未声明的 @ 别名仍需报普通导入错误，不能被 buildDependencies 路径吞掉。 */
static void test_unknown_package_import_is_not_treated_as_compile_tool(void) {
    SZrAstNode *ast = parse_import_source("@missing");
    SZrString *sourceName;
    SZrString *currentModuleKey = ZR_NULL;
    SZrFileRange errorLocation;
    TZrChar error[256];

    TEST_ASSERT_NOT_NULL(ast);
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "E:/repo/compile-tool-import/src/main.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&errorLocation, 0, sizeof(errorLocation));
    memset(error, 0, sizeof(error));

    TEST_ASSERT_FALSE(ZrParser_ProjectImports_CanonicalizeAst(
            g_state,
            ast,
            sourceName,
            &currentModuleKey,
            error,
            sizeof(error),
            &errorLocation));
    TEST_ASSERT_NOT_NULL(strstr(error, "unknown import alias '@missing'"));

    ZrParser_Ast_Free(g_state, ast);
}

/* 工具包子模块仍保持原始限定名，供后续锁与 provider 解析使用。 */
static void test_build_dependency_submodule_preserves_compile_tool_specifier(void) {
    SZrAstNode *ast = parse_import_source("@derive.tools.derive");
    SZrString *sourceName;
    SZrString *currentModuleKey = ZR_NULL;
    SZrFileRange errorLocation;
    TZrChar error[256];

    TEST_ASSERT_NOT_NULL(ast);
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "E:/repo/compile-tool-import/src/main.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&errorLocation, 0, sizeof(errorLocation));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_ProjectImports_CanonicalizeAst(
                    g_state,
                    ast,
                    sourceName,
                    &currentModuleKey,
                    error,
                    sizeof(error),
                    &errorLocation),
            error);
    TEST_ASSERT_EQUAL_STRING(
            "@derive.tools.derive", first_import_specifier(ast));

    ZrParser_Ast_Free(g_state, ast);
}

/* 编译期依赖不应污染运行时静态导入图；AST 身份需先从分析缓存撤销。 */
static void test_build_dependency_import_is_excluded_from_runtime_module_graph(void) {
    SZrAstNode *ast = parse_import_source("@derive");
    SZrString *sourceName;
    SZrString *moduleName;
    SZrString *currentModuleKey = ZR_NULL;
    const SZrParserModuleInitSummary *summary;
    SZrFileRange errorLocation;
    TZrChar error[256];

    TEST_ASSERT_NOT_NULL(ast);
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "E:/repo/compile-tool-import/src/main.zr");
    moduleName = ZrCore_String_CreateFromNative(g_state, "main");
    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_NOT_NULL(moduleName);
    memset(&errorLocation, 0, sizeof(errorLocation));
    memset(error, 0, sizeof(error));

    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_ProjectImports_CanonicalizeAst(
                    g_state,
                    ast,
                    sourceName,
                    &currentModuleKey,
                    error,
                    sizeof(error),
                    &errorLocation),
            error);
    TEST_ASSERT_TRUE(ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(
            g_state, moduleName, ast));
    summary = ZrParser_ModuleInitAnalysis_FindSummaryByAst(g_state->global, ast);
    TEST_ASSERT_NOT_NULL(summary);
    TEST_ASSERT_EQUAL_size_t(0U, summary->staticImports.length);

    ZrParser_ModuleInitAnalysis_ClearAstIdentity(g_state->global, ast);
    ZrParser_Ast_Free(g_state, ast);
}

/* build-facts 无锁时必须失败且不留下工具绑定，防止使用未经固定的构建输入。 */
static void test_build_dependency_import_requires_compile_tool_lock(void) {
    SZrAstNode *ast = parse_import_source("@derive");
    SZrString *sourceName;
    SZrString *currentModuleKey = ZR_NULL;
    SZrCompilerState compiler;
    SZrFileRange errorLocation;
    TZrChar error[256];

    TEST_ASSERT_NOT_NULL(ast);
    sourceName = ZrCore_String_CreateFromNative(
            g_state, "E:/repo/compile-tool-import/src/main.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&errorLocation, 0, sizeof(errorLocation));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_ProjectImports_CanonicalizeAst(
                    g_state,
                    ast,
                    sourceName,
                    &currentModuleKey,
                    error,
                    sizeof(error),
                    &errorLocation),
            error);

    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_FALSE(
            ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
                    &compiler, ast));
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL_MESSAGE(
            strstr(compiler.errorMessage, "compiletool.artifact.lock_missing"),
            compiler.errorMessage);
    TEST_ASSERT_EQUAL_size_t(0U, compiler.compileToolBindings.length);
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
}

/* provider 的公开入口筛选依赖 AST 可见性，覆盖显式和默认私有修饰符。 */
static void test_function_access_modifiers_are_retained_in_ast(void) {
    static const TZrChar source[] =
            "pub fn exported(): int { return 1; }\n"
            "pri fn hidden(): int { return 2; }\n"
            "fn localOnly(): int { return 3; }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "E:/repo/compile-tool-import/src/visibility.zr");
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(
            g_state, source, sizeof(source) - 1U, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_size_t(3U, ast->data.script.statements->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_ACCESS_PUBLIC,
            ast->data.script.statements->nodes[0]
                    ->data.functionDeclaration.accessModifier);
    TEST_ASSERT_EQUAL_INT(
            ZR_ACCESS_PRIVATE,
            ast->data.script.statements->nodes[1]
                    ->data.functionDeclaration.accessModifier);
    TEST_ASSERT_EQUAL_INT(
            ZR_ACCESS_PRIVATE,
            ast->data.script.statements->nodes[2]
                    ->data.functionDeclaration.accessModifier);
    ZrParser_Ast_Free(g_state, ast);
}

/* provider 源码的传递依赖失败时，编译状态四类导入/拥有权记录都要回滚。 */
static void test_provider_source_rolls_back_failed_transitive_build_dependency(void) {
    static const TZrByte source[] =
            "let nested = import(\"@derive\");\n"
            "pub comptime fn exported(): int { return 1; }\n";
    SZrCompilerState compiler;
    SZrString *moduleName = ZrCore_String_CreateFromNative(
            g_state, "@provider-with-transitive-import");

    TEST_ASSERT_NOT_NULL(moduleName);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_NULL(ZrParser_CompileTimeImport_LoadSourceModule(
            &compiler,
            moduleName,
            source,
            sizeof(source) - 1U,
            ZR_FALSE));
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL_MESSAGE(
            strstr(
                    compiler.errorMessage,
                    "compiletool.artifact.lock_missing"),
            compiler.errorMessage);
    TEST_ASSERT_EQUAL_size_t(0U, compiler.importedCompileTimeModules.length);
    TEST_ASSERT_EQUAL_size_t(0U, compiler.ownedCompileToolProviders.length);
    TEST_ASSERT_EQUAL_size_t(0U, compiler.compileToolBindings.length);
    TEST_ASSERT_EQUAL_size_t(0U, compiler.importedCompileTimeModuleAliases.length);
    ZrParser_CompilerState_Free(&compiler);
}

/* 供真实 .zrs 与 .zrm 夹具落盘；调用方负责检查返回值。 */
static TZrBool write_bytes(
        const TZrChar *path,
        const TZrByte *bytes,
        TZrSize byteCount) {
    FILE *file;
    TZrBool ok;

    if (!ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }
    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    ok = (TZrBool)(fwrite(bytes, 1U, byteCount, file) == byteCount);
    fclose(file);
    return ok;
}

/* 清单和锁使用可移植正斜杠路径，避免 Windows 转义改变 JSON 中的路径。 */
static void normalize_json_path(TZrChar *path) {
    if (path == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0U; path[index] != '\0'; index++) {
        if (path[index] == '\\') {
            path[index] = '/';
        }
    }
}

/* 同时保留源路径、归档路径与内容哈希，供 manifest/lock 构造同一制品身份。 */
typedef struct STestCompileToolArchive {
    TZrChar modulePath[ZR_TESTS_PATH_MAX];
    TZrChar archivePath[ZR_TESTS_PATH_MAX];
    TZrChar archiveHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
} STestCompileToolArchive;

/* 构造带公开契约哈希的真实工具归档；输出路径/哈希由调用方用于锁定。 */
static void create_compile_tool_archive(
        const TZrChar *stem,
        const TZrChar *packageName,
        const TZrChar *version,
        const TZrByte *source,
        TZrSize sourceByteCount,
        const TZrChar *publicContractHash,
        STestCompileToolArchive *outArchive) {
    SZrLibrary_ZrmPackModule module = {0};
    SZrLibrary_ZrmPackRequest request = {0};
    TZrByte *archiveBytes = ZR_NULL;
    TZrSize archiveByteCount = 0U;
    TZrChar moduleStem[ZR_TESTS_PATH_MAX];
    TZrChar moduleHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH] = {0};
    int written;

    TEST_ASSERT_NOT_NULL(stem);
    TEST_ASSERT_NOT_NULL(packageName);
    TEST_ASSERT_NOT_NULL(version);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(publicContractHash);
    TEST_ASSERT_NOT_NULL(outArchive);
    written = snprintf(moduleStem, sizeof(moduleStem), "%s_main", stem);
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(moduleStem));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "parser",
            "compile_tool_project_import",
            moduleStem,
            ".zrs",
            outArchive->modulePath,
            sizeof(outArchive->modulePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "parser",
            "compile_tool_project_import",
            stem,
            ".zrm",
            outArchive->archivePath,
            sizeof(outArchive->archivePath)));
    normalize_json_path(outArchive->modulePath);
    normalize_json_path(outArchive->archivePath);
    TEST_ASSERT_TRUE(write_bytes(
            outArchive->modulePath, source, sourceByteCount));
    TEST_ASSERT_TRUE(ZrParser_CompileToolContentHash_Bytes(
            source,
            sourceByteCount,
            moduleHash,
            sizeof(moduleHash)));

    module.moduleKey = "main";
    module.sourcePath = outArchive->modulePath;
    module.hash = moduleHash;
    module.compileToolExecutableSourcePath = outArchive->modulePath;
    module.compileToolExecutableHash = moduleHash;
    request.outputPath = outArchive->archivePath;
    request.assembly.name = packageName;
    request.assembly.version = version;
    request.assembly.kind = "compile-tool";
    request.assembly.entryModule = "main";
    request.assembly.providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL;
    request.assembly.publicContractHash = publicContractHash;
    request.modules = &module;
    request.moduleCount = 1U;
    TEST_ASSERT_TRUE_MESSAGE(
            ZrLibrary_Zrm_WriteArchive(&request, error, sizeof(error)),
            error);
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(
            outArchive->archivePath,
            &archiveBytes,
            &archiveByteCount));
    /* BUG: ReadFileBytes 后若哈希断言失败，Unity 长跳转会越过 free(archiveBytes)。 */
    TEST_ASSERT_TRUE(ZrParser_CompileToolContentHash_Bytes(
            archiveBytes,
            archiveByteCount,
            outArchive->archiveHash,
            sizeof(outArchive->archiveHash)));
    free(archiveBytes);
}

/* 两个已锁定 provider 互相导入时，错误链应可诊断且加载事务不得残留。 */
static void test_transitive_build_dependency_cycle_reports_chain_and_rolls_back(void) {
    static const TZrByte cycleAModuleBytes[] =
            "let cycleb = import(\"@cycleb\");\n"
            "pub comptime fn value(): int { return cycleb.value(); }\n";
    static const TZrByte cycleBModuleBytes[] =
            "let cyclea = import(\"@cyclea\");\n"
            "pub comptime fn value(): int { return cyclea.value(); }\n";
    const SZrParserCompileToolModuleDescriptor *builtinDescriptor =
            ZrParser_CompileTool_FindModule(ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
    STestCompileToolArchive cycleA = {0};
    STestCompileToolArchive cycleB = {0};
    SZrLibrary_Project *project;
    SZrAstNode *ast;
    SZrCompilerState compiler;
    SZrString *sourceName;
    SZrString *currentModuleKey = ZR_NULL;
    SZrFileRange errorLocation = {{0, 0, 0}, {0, 0, 0}, ZR_NULL};
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar manifest[4096];
    TZrChar lock[2048];
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH] = {0};
    int written;

    TEST_ASSERT_NOT_NULL(builtinDescriptor);
    create_compile_tool_archive(
            "cyclea",
            "cyclea",
            "1.0.0",
            cycleAModuleBytes,
            sizeof(cycleAModuleBytes) - 1U,
            builtinDescriptor->publicContractHash,
            &cycleA);
    create_compile_tool_archive(
            "cycleb",
            "cycleb",
            "1.0.0",
            cycleBModuleBytes,
            sizeof(cycleBModuleBytes) - 1U,
            builtinDescriptor->publicContractHash,
            &cycleB);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "parser",
            "compile_tool_project_import",
            "cycle_consumer",
            ".zrp",
            projectPath,
            sizeof(projectPath)));
    normalize_json_path(projectPath);
    written = snprintf(
            manifest,
            sizeof(manifest),
            "{\"manifestVersion\":2,\"name\":\"cycle-consumer\","
            "\"version\":\"1.0.0\",\"kind\":\"executable\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\","
            "\"dependencies\":{},\"buildDependencies\":{"
            "\"@cyclea\":{\"version\":\"^1.0.0\",\"path\":\"%s\"},"
            "\"@cycleb\":{\"version\":\"^1.0.0\",\"path\":\"%s\"}}}",
            cycleA.archivePath,
            cycleB.archivePath);
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(manifest));
    project = ZrLibrary_Project_New(g_state, manifest, projectPath);
    TEST_ASSERT_NOT_NULL(project);
    written = snprintf(
            sourcePath,
            sizeof(sourcePath),
            "%s/src/main.zr",
            ZrCore_String_GetNativeString(project->directory));
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(sourcePath));
    written = snprintf(
            lock,
            sizeof(lock),
            "{\"lockVersion\":1,\"dependencies\":{},\"buildDependencies\":{"
            "\"@cyclea\":{\"version\":\"1.0.0\",\"contentHash\":\"%s\","
            "\"transitiveIdentity\":\"sha256:uZUQq8UmbWYobrUOMIYjSt_JG2D7HnXK8ukUtWWZfpA\","
            "\"provider\":\"path\"},"
            "\"@cycleb\":{\"version\":\"1.0.0\",\"contentHash\":\"%s\","
            "\"transitiveIdentity\":\"sha256:VfjnMCaYkN5eJPeoTId1YBuK5y0FzGf3hQmQhhdNJ8E\","
            "\"provider\":\"path\"}}}",
            cycleA.archiveHash,
            cycleB.archiveHash);
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(lock));
    TEST_ASSERT_TRUE_MESSAGE(
            ZrLibrary_ProjectManifestV2_ReadDependencyLock(
                    g_state, project, lock, error, sizeof(error)),
            error);
    /* BUG: 此后任一 Unity 断言失败会跳过本用例尾部恢复/释放，tearDown 仅回收
     * g_project；临时 project 与可能已建的编译状态因此泄漏。证据见本用例尾部。 */
    g_state->global->userData = project;

    ast = parse_import_source("@cyclea");
    TEST_ASSERT_NOT_NULL(ast);
    sourceName = ZrCore_String_CreateFromNative(g_state, sourcePath);
    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_ProjectImports_CanonicalizeAst(
                    g_state,
                    ast,
                    sourceName,
                    &currentModuleKey,
                    error,
                    sizeof(error),
                    &errorLocation),
            error);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_FALSE(
            ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
                    &compiler, ast));
    TEST_ASSERT_NOT_NULL(compiler.errorMessage);
    TEST_ASSERT_NOT_NULL_MESSAGE(
            strstr(
                    compiler.errorMessage,
                    "comptime.phase_cycle: compile-tool provider graph @cyclea -> @cycleb -> @cyclea"),
            compiler.errorMessage);
    TEST_ASSERT_EQUAL_size_t(0U, compiler.compileToolBindings.length);
    TEST_ASSERT_EQUAL_size_t(0U, compiler.importedCompileTimeModuleAliases.length);
    TEST_ASSERT_EQUAL_size_t(0U, compiler.importedCompileTimeModules.length);
    TEST_ASSERT_EQUAL_size_t(0U, compiler.ownedCompileToolProviders.length);
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
    g_state->global->userData = g_project;
    ZrLibrary_Project_Free(g_state, project);
}

/* 锁定工具及其 helper 应成为编译状态拥有的 provider，仅公开 transform 可供消费。 */
static void test_locked_build_dependency_import_executes_owned_compile_tool_transform(void) {
    static const TZrByte helperModuleBytes[] =
            "pub comptime fn generatedName(): string { return \"generated\"; }\n";
    static const TZrByte moduleBytes[] =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "let helper = import(\"@helper\");\n"
            "#zr.compile.declarationTransform#\n"
            "comptime fn privateDerive(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn derive(target: declaration.Struct): declaration.Patch {\n"
            "    let field = init declaration.GeneratedField(\n"
            "        name: helper.generatedName(), type: typeid(bool),\n"
            "        visibility: declaration.Visibility.public,\n"
            "        mutability: declaration.Mutability.let);\n"
            "    return init declaration.Patch(\n"
            "        target: target.symbolId, additions: [field]);\n"
            "}\n";
    static const TZrChar consumerSource[] =
            "let derive = import(\"@derive\");\n"
            "#derive.derive#\n"
            "pub struct Meter { pub let value: int; }\n"
            "fn readGenerated(): bool {\n"
            "    let meter = init Meter();\n"
            "    return meter.generated;\n"
            "}\n"
            "return readGenerated() ? 1 : 0;\n";
    static const TZrChar privateConsumerSource[] =
            "let derive = import(\"@derive\");\n"
            "#derive.privateDerive#\n"
            "pub struct HiddenMeter { pub let value: int; }\n"
            "return 0;\n";
    const SZrParserCompileToolModuleDescriptor *builtinDescriptor =
            ZrParser_CompileTool_FindModule(ZR_PARSER_COMPILE_TOOL_MODULE_BUILD);
    SZrLibrary_ZrmPackModule module = {0};
    SZrLibrary_ZrmPackModule helperModule = {0};
    SZrLibrary_ZrmPackRequest request = {0};
    SZrLibrary_ZrmPackRequest helperRequest = {0};
    SZrLibrary_Project *project;
    SZrAstNode *ast;
    SZrCompilerState compiler;
    SZrFunction *compiledConsumer;
    const SZrCompileToolBinding *binding;
    SZrString *sourceName;
    SZrString *currentModuleKey = ZR_NULL;
    TZrByte *archiveBytes = ZR_NULL;
    TZrSize archiveByteCount = 0U;
    TZrByte *helperArchiveBytes = ZR_NULL;
    TZrSize helperArchiveByteCount = 0U;
    SZrFileRange errorLocation;
    TZrChar modulePath[ZR_TESTS_PATH_MAX];
    TZrChar archivePath[ZR_TESTS_PATH_MAX];
    TZrChar helperModulePath[ZR_TESTS_PATH_MAX];
    TZrChar helperArchivePath[ZR_TESTS_PATH_MAX];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar moduleHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar archiveHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar helperModuleHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar helperArchiveHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar manifest[4096];
    TZrChar lock[2048];
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH];
    int written;

    TEST_ASSERT_NOT_NULL(builtinDescriptor);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "parser",
            "compile_tool_project_import",
            "derive_main",
            ".zrs",
            modulePath,
            sizeof(modulePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "parser",
            "compile_tool_project_import",
            "derive",
            ".zrm",
            archivePath,
            sizeof(archivePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "parser",
            "compile_tool_project_import",
            "helper_main",
            ".zrs",
            helperModulePath,
            sizeof(helperModulePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "parser",
            "compile_tool_project_import",
            "helper",
            ".zrm",
            helperArchivePath,
            sizeof(helperArchivePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "parser",
            "compile_tool_project_import",
            "consumer",
            ".zrp",
            projectPath,
            sizeof(projectPath)));
    normalize_json_path(modulePath);
    normalize_json_path(archivePath);
    normalize_json_path(helperModulePath);
    normalize_json_path(helperArchivePath);
    normalize_json_path(projectPath);
    TEST_ASSERT_TRUE(write_bytes(
            modulePath, moduleBytes, sizeof(moduleBytes) - 1U));
    TEST_ASSERT_TRUE(ZrParser_CompileToolContentHash_Bytes(
            moduleBytes,
            sizeof(moduleBytes) - 1U,
            moduleHash,
            sizeof(moduleHash)));
    TEST_ASSERT_TRUE(write_bytes(
            helperModulePath,
            helperModuleBytes,
            sizeof(helperModuleBytes) - 1U));
    TEST_ASSERT_TRUE(ZrParser_CompileToolContentHash_Bytes(
            helperModuleBytes,
            sizeof(helperModuleBytes) - 1U,
            helperModuleHash,
            sizeof(helperModuleHash)));

    module.moduleKey = "main";
    module.sourcePath = modulePath;
    module.hash = moduleHash;
    module.compileToolExecutableSourcePath = modulePath;
    module.compileToolExecutableHash = moduleHash;
    request.outputPath = archivePath;
    request.assembly.name = "derive";
    request.assembly.version = "1.4.0";
    request.assembly.kind = "compile-tool";
    request.assembly.entryModule = "main";
    request.assembly.providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL;
    request.assembly.publicContractHash = builtinDescriptor->publicContractHash;
    request.modules = &module;
    request.moduleCount = 1U;
    TEST_ASSERT_TRUE_MESSAGE(
            ZrLibrary_Zrm_WriteArchive(&request, error, sizeof(error)),
            error);
    helperModule.moduleKey = "main";
    helperModule.sourcePath = helperModulePath;
    helperModule.hash = helperModuleHash;
    helperModule.compileToolExecutableSourcePath = helperModulePath;
    helperModule.compileToolExecutableHash = helperModuleHash;
    helperRequest.outputPath = helperArchivePath;
    helperRequest.assembly.name = "helper";
    helperRequest.assembly.version = "1.1.0";
    helperRequest.assembly.kind = "compile-tool";
    helperRequest.assembly.entryModule = "main";
    helperRequest.assembly.providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL;
    helperRequest.assembly.publicContractHash = builtinDescriptor->publicContractHash;
    helperRequest.modules = &helperModule;
    helperRequest.moduleCount = 1U;
    TEST_ASSERT_TRUE_MESSAGE(
            ZrLibrary_Zrm_WriteArchive(&helperRequest, error, sizeof(error)),
            error);
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(
            archivePath, &archiveBytes, &archiveByteCount));
    /* BUG: 两次归档读取后的哈希断言都可能长跳转并泄漏各自 malloc 缓冲区。 */
    TEST_ASSERT_TRUE(ZrParser_CompileToolContentHash_Bytes(
            archiveBytes,
            archiveByteCount,
            archiveHash,
            sizeof(archiveHash)));
    free(archiveBytes);
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(
            helperArchivePath, &helperArchiveBytes, &helperArchiveByteCount));
    /* BUG: helper 哈希断言失败时 helperArchiveBytes 同样越过尾部 free。 */
    TEST_ASSERT_TRUE(ZrParser_CompileToolContentHash_Bytes(
            helperArchiveBytes,
            helperArchiveByteCount,
            helperArchiveHash,
            sizeof(helperArchiveHash)));
    free(helperArchiveBytes);

    written = snprintf(
            manifest,
            sizeof(manifest),
            "{\"manifestVersion\":2,\"name\":\"consumer\","
            "\"version\":\"1.0.0\",\"kind\":\"executable\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\","
            "\"dependencies\":{},\"buildDependencies\":{"
            "\"@derive\":{\"version\":\"^1.0.0\",\"path\":\"%s\"},"
            "\"@helper\":{\"version\":\"^1.0.0\",\"path\":\"%s\"}}}",
            archivePath,
            helperArchivePath);
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(manifest));
    project = ZrLibrary_Project_New(g_state, manifest, projectPath);
    TEST_ASSERT_NOT_NULL(project);
    written = snprintf(
            sourcePath,
            sizeof(sourcePath),
            "%s/src/main.zr",
            ZrCore_String_GetNativeString(project->directory));
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(sourcePath));
    written = snprintf(
            lock,
            sizeof(lock),
            "{\"lockVersion\":1,\"dependencies\":{},"
            "\"buildDependencies\":{\"@derive\":{"
            "\"version\":\"1.4.0\",\"contentHash\":\"%s\","
            "\"transitiveIdentity\":\"sha256:uZUQq8UmbWYobrUOMIYjSt_JG2D7HnXK8ukUtWWZfpA\","
            "\"provider\":\"path\"},\"@helper\":{"
            "\"version\":\"1.1.0\",\"contentHash\":\"%s\","
            "\"transitiveIdentity\":\"sha256:VfjnMCaYkN5eJPeoTId1YBuK5y0FzGf3hQmQhhdNJ8E\","
            "\"provider\":\"path\"}}}",
            archiveHash,
            helperArchiveHash);
    TEST_ASSERT_TRUE(written > 0 && (TZrSize)written < sizeof(lock));
    TEST_ASSERT_TRUE_MESSAGE(
            ZrLibrary_ProjectManifestV2_ReadDependencyLock(
                    g_state, project, lock, error, sizeof(error)),
            error);
    /* BUG: 切换项目后断言失败会绕过本用例尾部的编译状态、AST、临时项目清理；
     * Unity 只调用 tearDown，而 tearDown 不拥有这里新建的 project。 */
    g_state->global->userData = project;

    ast = parse_import_source("@derive");
    TEST_ASSERT_NOT_NULL(ast);
    sourceName = ZrCore_String_CreateFromNative(g_state, sourcePath);
    TEST_ASSERT_NOT_NULL(sourceName);
    memset(&errorLocation, 0, sizeof(errorLocation));
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_ProjectImports_CanonicalizeAst(
                    g_state,
                    ast,
                    sourceName,
                    &currentModuleKey,
                    error,
                    sizeof(error),
                    &errorLocation),
            error);
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
                    &compiler, ast),
            compiler.errorMessage != ZR_NULL ? compiler.errorMessage : "build facts failed");
    TEST_ASSERT_EQUAL_size_t(1U, compiler.compileToolBindings.length);
    TEST_ASSERT_EQUAL_size_t(2U, compiler.ownedCompileToolProviders.length);
    TEST_ASSERT_EQUAL_size_t(2U, compiler.importedCompileTimeModules.length);
    binding = (const SZrCompileToolBinding *)ZrCore_Array_Get(
            &compiler.compileToolBindings, 0U);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_NOT_NULL(binding->provider);
    TEST_ASSERT_NOT_NULL(binding->resolvedArtifact);
    TEST_ASSERT_TRUE(ZrParser_CompileToolArtifact_IsOpen(
            binding->resolvedArtifact));
    TEST_ASSERT_EQUAL_STRING("@derive", binding->provider->moduleName);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)project->dependencyPackageCount);

    compiledConsumer = ZrParser_Source_Compile(
            g_state,
            privateConsumerSource,
            sizeof(privateConsumerSource) - 1U,
            sourceName);
    TEST_ASSERT_NULL(compiledConsumer);

    compiledConsumer = ZrParser_Source_Compile(
            g_state,
            consumerSource,
            sizeof(consumerSource) - 1U,
            sourceName);
    TEST_ASSERT_NOT_NULL(compiledConsumer);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)project->dependencyPackageCount);
    ZrCore_Function_Free(g_state, compiledConsumer);

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, ast);
    g_state->global->userData = g_project;
    ZrLibrary_Project_Free(g_state, project);
}

/* Unity 入口覆盖别名、锁、循环和工具执行；可执行目标由 tests/CMakeLists.txt 构建。 */
/* TODO: 当前 CMake 未把此目标注册为 CTest 用例；核对是否预期只手工执行。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_build_dependency_import_preserves_compile_tool_specifier);
    RUN_TEST(test_unknown_package_import_is_not_treated_as_compile_tool);
    RUN_TEST(test_build_dependency_submodule_preserves_compile_tool_specifier);
    RUN_TEST(test_build_dependency_import_is_excluded_from_runtime_module_graph);
    RUN_TEST(test_build_dependency_import_requires_compile_tool_lock);
    RUN_TEST(test_function_access_modifiers_are_retained_in_ast);
    RUN_TEST(test_provider_source_rolls_back_failed_transitive_build_dependency);
    RUN_TEST(test_transitive_build_dependency_cycle_reports_chain_and_rolls_back);
    RUN_TEST(test_locked_build_dependency_import_executes_owned_compile_tool_transform);
    return UNITY_END();
}
