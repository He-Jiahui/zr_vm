#include "unity.h"

#include "harness/path_support.h"
#include "harness/module_fixture_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/project.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_parser/compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TZrUInt64 metadata_signature_hash_v1(const TZrByte *signatureBlob, TZrSize signatureBlobLength);
typedef struct SZrModuleImportSignatureMismatch SZrModuleImportSignatureMismatch;
TZrBool zr_module_import_signature_verify(SZrState *state,
                                           SZrFunction *callerFunction,
                                           SZrString *path,
                                           SZrObjectModule *module,
                                           SZrModuleImportSignatureMismatch *outMismatch);
TZrBool compiler_build_function_metadata_tokens(SZrCompilerState *cs, SZrFunction *function);

typedef struct SZrProjectImportFixture {
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar mainPath[ZR_TESTS_PATH_MAX];
    TZrChar helperPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedPath[ZR_TESTS_PATH_MAX];
} SZrProjectImportFixture;

typedef struct SZrProjectDependencyImportFixture {
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar rootMainPath[ZR_TESTS_PATH_MAX];
    TZrChar mathMainPath[ZR_TESTS_PATH_MAX];
    TZrChar mathBinaryPath[ZR_TESTS_PATH_MAX];
} SZrProjectDependencyImportFixture;

void setUp(void) {}

void tearDown(void) {}

static const TZrChar *test_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

static TZrChar *find_last_path_separator(TZrChar *path) {
    TZrChar *forwardSlash;
    TZrChar *backSlash;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    forwardSlash = strrchr(path, '/');
    backSlash = strrchr(path, '\\');
    if (forwardSlash == ZR_NULL) {
        return backSlash;
    }
    if (backSlash == ZR_NULL) {
        return forwardSlash;
    }

    return forwardSlash > backSlash ? forwardSlash : backSlash;
}

static TZrBool write_text_file(const TZrChar *path, const TZrChar *content) {
    FILE *file;
    size_t contentLength;
    size_t written;

    if (path == ZR_NULL || content == ZR_NULL || !ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }

    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    contentLength = strlen(content);
    written = fwrite(content, 1, contentLength, file);
    fclose(file);
    return written == contentLength;
}

static TZrBool compile_project_source_to_zro(const TZrChar *projectPath,
                                             const TZrChar *sourcePath,
                                             const TZrChar *binaryPath,
                                             const TZrChar *moduleName) {
    SZrGlobalState *global = ZR_NULL;
    SZrState *state = ZR_NULL;
    SZrString *sourceName = ZR_NULL;
    SZrFunction *function = ZR_NULL;
    SZrBinaryWriterOptions options;
    TZrSize sourceLength = 0;
    TZrChar *sourceContent = ZR_NULL;
    TZrBool success = ZR_FALSE;

    if (projectPath == ZR_NULL || sourcePath == ZR_NULL || binaryPath == ZR_NULL ||
        moduleName == ZR_NULL || !ZrTests_Path_EnsureParentDirectory(binaryPath)) {
        return ZR_FALSE;
    }

    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        goto cleanup;
    }
    state = global->mainThreadState;
    ZrParser_ToGlobalState_Register(state);

    sourceContent = ZrTests_ReadTextFile(sourcePath, &sourceLength);
    if (sourceContent == ZR_NULL) {
        goto cleanup;
    }

    sourceName = ZrCore_String_CreateFromNative(state, (TZrNativeString)sourcePath);
    if (sourceName == ZR_NULL) {
        goto cleanup;
    }

    function = ZrParser_Source_Compile(state, sourceContent, sourceLength, sourceName);
    if (function == ZR_NULL) {
        goto cleanup;
    }

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    success = ZrParser_Writer_WriteBinaryFileWithOptions(state, function, binaryPath, &options);

cleanup:
    if (sourceContent != ZR_NULL) {
        free(sourceContent);
    }
    if (function != ZR_NULL && state != ZR_NULL) {
        ZrCore_Function_Free(state, function);
    }
    if (global != ZR_NULL) {
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
    }
    return success;
}

static const SZrTypeValue *get_object_field_value(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrString *fieldNameString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldNameString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldNameString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static const TZrChar *current_exception_message(SZrState *state) {
    SZrObject *errorObject;
    const SZrTypeValue *messageValue;

    if (state == ZR_NULL || !state->hasCurrentException ||
        state->currentException.type != ZR_VALUE_TYPE_OBJECT ||
        state->currentException.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    errorObject = ZR_CAST_OBJECT(state, state->currentException.value.object);
    if (errorObject == ZR_NULL) {
        return ZR_NULL;
    }

    messageValue = get_object_field_value(state, errorObject, "message");
    if (messageValue == ZR_NULL ||
        messageValue->type != ZR_VALUE_TYPE_STRING ||
        messageValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, messageValue->value.object));
}

static void normalize_test_path_to_forward_slashes(TZrChar *path) {
    if (path == ZR_NULL) {
        return;
    }

    for (; *path != '\0'; path++) {
        if (*path == '\\') {
            *path = '/';
        }
    }
}

static void inject_windows_mixed_separator(TZrChar *path) {
    normalize_test_path_to_forward_slashes(path);

#ifdef ZR_VM_PLATFORM_IS_WIN
    TZrChar *featureSeparator;

    if (path == ZR_NULL) {
        return;
    }

    featureSeparator = strstr(path, "/feature/");
    if (featureSeparator != ZR_NULL) {
        *featureSeparator = '\\';
    }
#endif
}

static void project_import_init_named_object_type_ref(SZrState *state,
                                                      SZrFunctionTypedTypeRef *typeRef,
                                                      const TZrChar *typeName) {
    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->typeName = ZrCore_String_CreateFromNative(state, (TZrNativeString)typeName);
    TEST_ASSERT_NOT_NULL(typeRef->typeName);
}

static SZrFunction *create_project_import_typespec_entry_function(SZrState *state,
                                                                  const TZrChar *returnTypeName) {
    SZrFunction *function;
    SZrFunctionTypedExportSymbol *symbol;
    SZrCompilerState compilerState;

    if (state == ZR_NULL || state->global == ZR_NULL || returnTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    function->functionName = ZrCore_String_Create(state, "__entry", strlen("__entry"));
    function->typedExportedSymbols = (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedExportSymbol),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->functionName == ZR_NULL || function->typedExportedSymbols == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(function->typedExportedSymbols, 0, sizeof(SZrFunctionTypedExportSymbol));
    function->typedExportedSymbolLength = 1u;
    symbol = &function->typedExportedSymbols[0];
    symbol->name = ZrCore_String_Create(state, "make", strlen("make"));
    symbol->accessModifier = ZR_ACCESS_PUBLIC;
    symbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    symbol->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    symbol->readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
    symbol->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
    if (symbol->name == ZR_NULL) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }
    project_import_init_named_object_type_ref(state, &symbol->valueType, returnTypeName);

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = function;
    if (!compiler_build_function_metadata_tokens(&compilerState, function)) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    return function;
}

static TZrBool attach_project_import_typespec_effect(SZrState *state,
                                                     SZrFunction *callerFunction,
                                                     SZrString *moduleName,
                                                     const SZrFunction *providerFunction) {
    SZrFunctionModuleEffect *effect;
    const SZrFunctionTypedExportSymbol *providerSymbol;

    if (state == ZR_NULL || state->global == ZR_NULL || callerFunction == ZR_NULL ||
        moduleName == ZR_NULL || providerFunction == ZR_NULL ||
        providerFunction->typedExportedSymbols == ZR_NULL ||
        providerFunction->typedExportedSymbolLength == 0u) {
        return ZR_FALSE;
    }

    providerSymbol = &providerFunction->typedExportedSymbols[0];
    effect = (SZrFunctionModuleEffect *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                       sizeof(SZrFunctionModuleEffect),
                                                                       ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (effect == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(effect, 0, sizeof(*effect));
    effect->kind = ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL;
    effect->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    effect->readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
    effect->moduleName = moduleName;
    effect->symbolName = providerSymbol->name;
    effect->targetMetadataToken = providerSymbol->metadataToken;
    effect->targetSignatureToken = providerSymbol->signatureToken;
    effect->targetSignatureHash = providerSymbol->signatureHash;
    effect->targetModuleSignatureHash = providerFunction->moduleSignatureHash;

    callerFunction->moduleEntryEffects = effect;
    callerFunction->moduleEntryEffectLength = 1u;
    return ZR_TRUE;
}

static const TZrChar *native_plugin_extension_for_test(void) {
#ifdef ZR_VM_PLATFORM_IS_WIN
    return ".dll";
#elif defined(ZR_PLATFORM_DARWIN)
    return ".dylib";
#else
    return ".so";
#endif
}

static TZrBool prepare_project_import_fixture(SZrProjectImportFixture *fixture) {
    static const TZrChar *projectContent =
            "{\n"
            "  \"name\": \"parser_project_import_canonicalization\",\n"
            "  \"version\": \"1.0.0\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"feature/app/main\",\n"
            "  \"pathAliases\": {\n"
            "    \"@shared\": \"common/shared\"\n"
            "  }\n"
            "}\n";
    static const TZrChar *mainContent =
            "var localMath = %import(\".helper.math\");\n"
            "var localMathAgain = %import(\"feature/app/helper/math\");\n"
            "var sharedHash = %import(\"@shared.crypto.hash\");\n"
            "\n"
            "pub func run(): i32 {\n"
            "    return localMath.answer + localMathAgain.answer + sharedHash.seed;\n"
            "}\n";
    static const TZrChar *helperContent =
            "pub var answer = 40;\n";
    static const TZrChar *sharedContent =
            "pub var seed = 2;\n";
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;

    if (fixture == ZR_NULL ||
        !ZrTests_Path_GetGeneratedArtifact("parser",
                                           "project_import_canonicalization",
                                           "import_relative_alias",
                                           ".zrp",
                                           fixture->projectPath,
                                           sizeof(fixture->projectPath))) {
        return ZR_FALSE;
    }

    memset(fixture->mainPath, 0, sizeof(fixture->mainPath));
    memset(fixture->helperPath, 0, sizeof(fixture->helperPath));
    memset(fixture->sharedPath, 0, sizeof(fixture->sharedPath));

    snprintf(rootPath, sizeof(rootPath), "%s", fixture->projectPath);
    lastSeparator = find_last_path_separator(rootPath);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    ZrLibrary_File_PathJoin(rootPath, "src", sourceRoot);
    ZrLibrary_File_PathJoin(sourceRoot, "feature/app/main.zr", fixture->mainPath);
    ZrLibrary_File_PathJoin(sourceRoot, "feature/app/helper/math.zr", fixture->helperPath);
    ZrLibrary_File_PathJoin(sourceRoot, "common/shared/crypto/hash.zr", fixture->sharedPath);

    return write_text_file(fixture->projectPath, projectContent) &&
           write_text_file(fixture->mainPath, mainContent) &&
           write_text_file(fixture->helperPath, helperContent) &&
           write_text_file(fixture->sharedPath, sharedContent);
}

static TZrBool prepare_project_dependency_import_fixture(SZrProjectDependencyImportFixture *fixture) {
    static const TZrChar *projectContent =
            "{\n"
            "  \"name\": \"dependency_import_root\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"dependencies\": {\n"
            "    \"$math\": { \"path\": \"deps/math/math.zrp\", \"version\": \"1.0.0\" }\n"
            "  }\n"
            "}\n";
    static const TZrChar *mathProjectContent =
            "{\n"
            "  \"name\": \"math\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"feature/main\",\n"
            "  \"version\": \"1.0.0\",\n"
            "  \"pathAliases\": { \"@core\": \"core\" },\n"
            "  \"dependencies\": { \"$trig\": \"../trig/trig.zrp\" }\n"
            "}\n";
    static const TZrChar *trigProjectContent =
            "{\n"
            "  \"name\": \"trig\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"version\": \"1.0.0\"\n"
            "}\n";
    static const TZrChar *rootMainContent =
            "var math = %import(\"&math.ops.sum\");\n"
            "pub func run(): i32 { return 1; }\n";
    static const TZrChar *mathMainContent =
            "var core = %import(\"@core.util\");\n"
            "var helper = %import(\".helper\");\n"
            "var bare = %import(\"bare.local\");\n"
            "var trig = %import(\"&trig.wave\");\n"
            "pub func run(): i32 { return 2; }\n";
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mathProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar trigProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar mathSourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar trigSourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mathOpsPath[ZR_TESTS_PATH_MAX];
    TZrChar mathCorePath[ZR_TESTS_PATH_MAX];
    TZrChar mathHelperPath[ZR_TESTS_PATH_MAX];
    TZrChar mathBarePath[ZR_TESTS_PATH_MAX];
    TZrChar trigWavePath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;

    if (fixture == ZR_NULL ||
        !ZrTests_Path_GetGeneratedArtifact("parser",
                                           "project_import_canonicalization",
                                           "dependency_imports",
                                           ".zrp",
                                           fixture->projectPath,
                                           sizeof(fixture->projectPath))) {
        return ZR_FALSE;
    }

    memset(fixture->rootMainPath, 0, sizeof(fixture->rootMainPath));
    memset(fixture->mathMainPath, 0, sizeof(fixture->mathMainPath));

    snprintf(rootPath, sizeof(rootPath), "%s", fixture->projectPath);
    lastSeparator = find_last_path_separator(rootPath);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    ZrLibrary_File_PathJoin(rootPath, "src", sourceRoot);
    ZrLibrary_File_PathJoin(sourceRoot, "main.zr", fixture->rootMainPath);
    ZrLibrary_File_PathJoin(rootPath, "deps/math/math.zrp", mathProjectPath);
    ZrLibrary_File_PathJoin(rootPath, "deps/trig/trig.zrp", trigProjectPath);
    ZrLibrary_File_PathJoin(rootPath, "deps/math/src", mathSourceRoot);
    ZrLibrary_File_PathJoin(rootPath, "deps/trig/src", trigSourceRoot);
    ZrLibrary_File_PathJoin(mathSourceRoot, "feature/main.zr", fixture->mathMainPath);
    ZrLibrary_File_PathJoin(mathSourceRoot, "ops/sum.zr", mathOpsPath);
    ZrLibrary_File_PathJoin(mathSourceRoot, "core/util.zr", mathCorePath);
    ZrLibrary_File_PathJoin(mathSourceRoot, "feature/helper.zr", mathHelperPath);
    ZrLibrary_File_PathJoin(mathSourceRoot, "bare/local.zr", mathBarePath);
    ZrLibrary_File_PathJoin(trigSourceRoot, "wave.zr", trigWavePath);

    return write_text_file(fixture->projectPath, projectContent) &&
           write_text_file(fixture->rootMainPath, rootMainContent) &&
           write_text_file(mathProjectPath, mathProjectContent) &&
           write_text_file(trigProjectPath, trigProjectContent) &&
           write_text_file(fixture->mathMainPath, mathMainContent) &&
           write_text_file(mathOpsPath, "pub var value = 1;\n") &&
           write_text_file(mathCorePath, "pub var value = 2;\n") &&
           write_text_file(mathHelperPath, "pub var value = 3;\n") &&
           write_text_file(mathBarePath, "pub var value = 4;\n") &&
           write_text_file(trigWavePath, "pub var value = 5;\n");
}

static TZrBool prepare_project_dependency_import_version_range_fixture(SZrProjectDependencyImportFixture *fixture) {
    static const TZrChar *projectContent =
            "{\n"
            "  \"name\": \"dependency_import_range_root\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"dependencies\": {\n"
            "    \"$math\": {\n"
            "      \"path\": \"deps/math/math.zrp\",\n"
            "      \"version\": \"1.2.3\",\n"
            "      \"minVersionInclusive\": \"1.2.0\",\n"
            "      \"maxVersionExclusive\": \"1.3.0\"\n"
            "    }\n"
            "  }\n"
            "}\n";
    static const TZrChar *mathProjectContent =
            "{\n"
            "  \"name\": \"math\",\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"ops/sum\",\n"
            "  \"version\": \"1.2.3\"\n"
            "}\n";
    static const TZrChar *rootMainContent =
            "var math = %import(\"&math.ops.sum\");\n"
            "return math.value;\n";
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mathProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar mathSourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mathOpsPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;

    if (fixture == ZR_NULL ||
        !ZrTests_Path_GetGeneratedArtifact("parser",
                                           "project_import_canonicalization",
                                           "dependency_import_range",
                                           ".zrp",
                                           fixture->projectPath,
                                           sizeof(fixture->projectPath))) {
        return ZR_FALSE;
    }

    memset(fixture->rootMainPath, 0, sizeof(fixture->rootMainPath));
    memset(fixture->mathMainPath, 0, sizeof(fixture->mathMainPath));

    snprintf(rootPath, sizeof(rootPath), "%s", fixture->projectPath);
    lastSeparator = find_last_path_separator(rootPath);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    ZrLibrary_File_PathJoin(rootPath, "src", sourceRoot);
    ZrLibrary_File_PathJoin(sourceRoot, "main.zr", fixture->rootMainPath);
    ZrLibrary_File_PathJoin(rootPath, "deps/math/math.zrp", mathProjectPath);
    ZrLibrary_File_PathJoin(rootPath, "deps/math/src", mathSourceRoot);
    ZrLibrary_File_PathJoin(mathSourceRoot, "ops/sum.zr", mathOpsPath);

    return write_text_file(fixture->projectPath, projectContent) &&
           write_text_file(fixture->rootMainPath, rootMainContent) &&
           write_text_file(mathProjectPath, mathProjectContent) &&
           write_text_file(mathOpsPath, "pub var value = 42;\n");
}

static TZrBool prepare_project_assembly_reference_import_fixture(SZrProjectDependencyImportFixture *fixture) {
    static const TZrChar *projectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"app.render\", \"version\": \"3.4.5\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\",\n"
            "  \"references\": {\n"
            "    \"mathLocal\": {\n"
            "      \"assembly\": \"zr.math\",\n"
            "      \"version\": \"2.1.0\",\n"
            "      \"path\": \"deps/math/math.zrp\",\n"
            "      \"minVersionInclusive\": \"2.0.0\",\n"
            "      \"maxVersionExclusive\": \"3.0.0\"\n"
            "    }\n"
            "  }\n"
            "}\n";
    static const TZrChar *mathProjectContent =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"assembly\": { \"name\": \"zr.math\", \"version\": \"2.1.0\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"ops/sum\"\n"
            "}\n";
    static const TZrChar *rootMainContent =
            "var math = %import(\"&mathLocal.ops.sum\");\n"
            "return math.value;\n";
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mathProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar mathSourceRoot[ZR_TESTS_PATH_MAX];
    TZrChar mathBinaryRoot[ZR_TESTS_PATH_MAX];
    TZrChar mathOpsPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;

    if (fixture == ZR_NULL ||
        !ZrTests_Path_GetGeneratedArtifact("parser",
                                           "project_import_canonicalization",
                                           "assembly_references",
                                           ".zrp",
                                           fixture->projectPath,
                                           sizeof(fixture->projectPath))) {
        return ZR_FALSE;
    }

    memset(fixture->rootMainPath, 0, sizeof(fixture->rootMainPath));
    memset(fixture->mathMainPath, 0, sizeof(fixture->mathMainPath));
    memset(fixture->mathBinaryPath, 0, sizeof(fixture->mathBinaryPath));

    snprintf(rootPath, sizeof(rootPath), "%s", fixture->projectPath);
    lastSeparator = find_last_path_separator(rootPath);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    ZrLibrary_File_PathJoin(rootPath, "src", sourceRoot);
    ZrLibrary_File_PathJoin(sourceRoot, "main.zr", fixture->rootMainPath);
    ZrLibrary_File_PathJoin(rootPath, "deps/math/math.zrp", mathProjectPath);
    ZrLibrary_File_PathJoin(rootPath, "deps/math/src", mathSourceRoot);
    ZrLibrary_File_PathJoin(rootPath, "deps/math/bin", mathBinaryRoot);
    ZrLibrary_File_PathJoin(mathSourceRoot, "ops/sum.zr", mathOpsPath);
    snprintf(fixture->mathMainPath, sizeof(fixture->mathMainPath), "%s", mathOpsPath);
    ZrLibrary_File_PathJoin(mathBinaryRoot, "ops/sum.zro", fixture->mathBinaryPath);

    return write_text_file(fixture->projectPath, projectContent) &&
           write_text_file(fixture->rootMainPath, rootMainContent) &&
           write_text_file(mathProjectPath, mathProjectContent) &&
           write_text_file(mathOpsPath, "pub var value = 42;\n");
}

static TZrSize count_static_imports_named(const SZrFunction *function, const TZrChar *moduleName) {
    TZrSize count = 0;

    if (function == ZR_NULL || moduleName == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < function->staticImportLength; index++) {
        const TZrChar *text = test_string_text(function->staticImports[index]);
        if (text != ZR_NULL && strcmp(text, moduleName) == 0) {
            count++;
        }
    }

    return count;
}

static TZrBool function_contains_module_effect_named(const SZrFunction *function, const TZrChar *moduleName) {
    if (function == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->moduleEntryEffectLength; index++) {
        const TZrChar *text = test_string_text(function->moduleEntryEffects[index].moduleName);
        if (text != ZR_NULL && strcmp(text, moduleName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_contains_module_effect_with_assembly(const SZrFunction *function,
                                                             const TZrChar *moduleName,
                                                             const TZrChar *assemblyName) {
    if (function == ZR_NULL || moduleName == ZR_NULL || assemblyName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->moduleEntryEffectLength; index++) {
        const SZrFunctionModuleEffect *effect = &function->moduleEntryEffects[index];
        const TZrChar *effectModuleName = test_string_text(effect->moduleName);
        const TZrChar *effectAssemblyName = test_string_text(effect->assemblyName);

        if (effectModuleName != ZR_NULL &&
            effectAssemblyName != ZR_NULL &&
            strcmp(effectModuleName, moduleName) == 0 &&
            strcmp(effectAssemblyName, assemblyName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool io_function_contains_module_effect_with_assembly(const SZrIoFunction *function,
                                                                const TZrChar *moduleName,
                                                                const TZrChar *assemblyName) {
    if (function == ZR_NULL || moduleName == ZR_NULL || assemblyName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < function->moduleEntryEffectsLength; index++) {
        const SZrIoFunctionModuleEffect *effect = &function->moduleEntryEffects[index];
        const TZrChar *effectModuleName = test_string_text(effect->moduleName);
        const TZrChar *effectAssemblyName = test_string_text(effect->assemblyName);

        if (effectModuleName != ZR_NULL &&
            effectAssemblyName != ZR_NULL &&
            strcmp(effectModuleName, moduleName) == 0 &&
            strcmp(effectAssemblyName, assemblyName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static const SZrFunctionCallableSummary *find_exported_callable_summary_named(const SZrFunction *function,
                                                                              const TZrChar *callableName) {
    if (function == ZR_NULL || callableName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->exportedCallableSummaryLength; index++) {
        const SZrFunctionCallableSummary *summary = &function->exportedCallableSummaries[index];
        const TZrChar *text = test_string_text(summary->name);
        if (text != ZR_NULL && strcmp(text, callableName) == 0) {
            return summary;
        }
    }

    return ZR_NULL;
}

static TZrBool callable_summary_contains_module_effect_named(const SZrFunctionCallableSummary *summary,
                                                             const TZrChar *moduleName) {
    if (summary == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < summary->effectCount; index++) {
        const TZrChar *text = test_string_text(summary->effects[index].moduleName);
        if (text != ZR_NULL && strcmp(text, moduleName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrSize count_metadata_records_with_table(const SZrFunction *function, TZrUInt32 tableTag) {
    TZrSize count = 0;

    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        if (ZR_METADATA_TOKEN_TABLE(function->metadataTokenRecords[index].token) == tableTag) {
            count++;
        }
    }

    return count;
}

static TZrBool function_has_member_ref_signature_blob(const SZrFunction *function) {
    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF ||
            record->signatureBlobLength == 0 ||
            record->signatureBlobOffset >= function->signatureBlobHeapLength ||
            (TZrUInt32)record->signatureBlobLength >
                    function->signatureBlobHeapLength - record->signatureBlobOffset) {
            continue;
        }

        if (function->signatureBlobHeap[record->signatureBlobOffset] == ZR_METADATA_SIGNATURE_NODE_MEMBER_REF) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_has_table_signature_blob_node(const SZrFunction *function,
                                                      TZrUInt32 tableTag,
                                                      TZrUInt8 nodeKind) {
    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(record->token) != tableTag ||
            record->signatureBlobLength == 0 ||
            record->signatureBlobOffset >= function->signatureBlobHeapLength ||
            (TZrUInt32)record->signatureBlobLength >
                    function->signatureBlobHeapLength - record->signatureBlobOffset) {
            continue;
        }

        if (function->signatureBlobHeap[record->signatureBlobOffset] == nodeKind) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_has_member_ref_signature_hash(const SZrFunction *function) {
    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF ||
            record->signatureHash == 0) {
            continue;
        }
        for (TZrUInt32 signatureIndex = 0; signatureIndex < function->metadataTokenRecordLength; signatureIndex++) {
            const SZrMetadataTokenRecord *signatureRecord = &function->metadataTokenRecords[signatureIndex];

            if (signatureRecord->token == record->relatedToken &&
                ZR_METADATA_TOKEN_TABLE(signatureRecord->token) == ZR_METADATA_TABLE_SIGNATURE &&
                signatureRecord->relatedToken == record->token &&
                signatureRecord->signatureHash == record->signatureHash) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool read_metadata_blob_u32(const TZrByte *blob,
                                      TZrUInt32 blobLength,
                                      TZrUInt32 *ioOffset,
                                      TZrUInt32 *outValue) {
    TZrUInt32 offset;

    if (blob == ZR_NULL || ioOffset == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    offset = *ioOffset;
    if (offset > blobLength || blobLength - offset < 4u) {
        return ZR_FALSE;
    }

    *outValue = ((TZrUInt32)blob[offset]) |
                ((TZrUInt32)blob[offset + 1u] << 8u) |
                ((TZrUInt32)blob[offset + 2u] << 16u) |
                ((TZrUInt32)blob[offset + 3u] << 24u);
    *ioOffset = offset + 4u;
    return ZR_TRUE;
}

static SZrString *metadata_string_heap_lookup(const SZrFunction *function, TZrUInt32 stringIndex) {
    if (function == ZR_NULL || stringIndex == 0u ||
        function->metadataStringHeap == ZR_NULL || function->metadataStringHeapLength == 0u) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->metadataStringHeapLength; index++) {
        if (function->metadataStringHeap[index].stringIndex == stringIndex) {
            return function->metadataStringHeap[index].value;
        }
    }

    return ZR_NULL;
}

static TZrBool read_metadata_blob_string_matches(const SZrFunction *function,
                                                 const TZrByte *blob,
                                                 TZrUInt32 blobLength,
                                                 TZrUInt32 *ioOffset,
                                                 const TZrChar *expected) {
    TZrUInt32 length;
    TZrUInt32 offset;

    if (!read_metadata_blob_u32(blob, blobLength, ioOffset, &length) ||
        expected == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function != ZR_NULL && function->metadataStringHeap != ZR_NULL &&
        function->metadataStringHeapLength > 0u) {
        SZrString *value = metadata_string_heap_lookup(function, length);
        const TZrChar *text = test_string_text(value);
        return text != ZR_NULL && strcmp(text, expected) == 0 ? ZR_TRUE : ZR_FALSE;
    }

    offset = *ioOffset;
    if (offset > blobLength || length > blobLength - offset ||
        strlen(expected) != length ||
        memcmp(blob + offset, expected, length) != 0) {
        return ZR_FALSE;
    }

    *ioOffset = offset + length;
    return ZR_TRUE;
}

static TZrBool skip_metadata_blob_string(const SZrFunction *function,
                                         const TZrByte *blob,
                                         TZrUInt32 blobLength,
                                         TZrUInt32 *ioOffset) {
    TZrUInt32 length;
    TZrUInt32 offset;

    if (!read_metadata_blob_u32(blob, blobLength, ioOffset, &length)) {
        return ZR_FALSE;
    }

    if (function != ZR_NULL && function->metadataStringHeap != ZR_NULL &&
        function->metadataStringHeapLength > 0u) {
        return length == 0u || metadata_string_heap_lookup(function, length) != ZR_NULL ? ZR_TRUE : ZR_FALSE;
    }

    offset = *ioOffset;
    if (offset > blobLength || length > blobLength - offset) {
        return ZR_FALSE;
    }

    *ioOffset = offset + length;
    return ZR_TRUE;
}

static TZrBool skip_metadata_blob_type_signature(const SZrFunction *function,
                                                 const TZrByte *blob,
                                                 TZrUInt32 blobLength,
                                                 TZrUInt32 *ioOffset) {
    TZrUInt8 nodeKind;
    TZrUInt32 value;
    TZrUInt32 count;

    if (blob == ZR_NULL || ioOffset == ZR_NULL || *ioOffset >= blobLength) {
        return ZR_FALSE;
    }

    nodeKind = blob[(*ioOffset)++];
    switch (nodeKind) {
        case ZR_METADATA_SIGNATURE_NODE_PRIMITIVE:
            return read_metadata_blob_u32(blob, blobLength, ioOffset, &value);
        case ZR_METADATA_SIGNATURE_NODE_TYPE_REF:
        case ZR_METADATA_SIGNATURE_NODE_TYPE_DEF:
            return read_metadata_blob_u32(blob, blobLength, ioOffset, &value) &&
                   skip_metadata_blob_string(function, blob, blobLength, ioOffset);
        case ZR_METADATA_SIGNATURE_NODE_ARRAY:
            return read_metadata_blob_u32(blob, blobLength, ioOffset, &value) &&
                   skip_metadata_blob_type_signature(function, blob, blobLength, ioOffset);
        case ZR_METADATA_SIGNATURE_NODE_OWNERSHIP:
            return read_metadata_blob_u32(blob, blobLength, ioOffset, &value) &&
                   skip_metadata_blob_type_signature(function, blob, blobLength, ioOffset);
        case ZR_METADATA_SIGNATURE_NODE_UNION:
            if (!read_metadata_blob_u32(blob, blobLength, ioOffset, &value) ||
                !skip_metadata_blob_string(function, blob, blobLength, ioOffset) ||
                !read_metadata_blob_u32(blob, blobLength, ioOffset, &count)) {
                return ZR_FALSE;
            }
            for (TZrUInt32 index = 0; index < count; index++) {
                if (!skip_metadata_blob_type_signature(function, blob, blobLength, ioOffset)) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        case ZR_METADATA_SIGNATURE_NODE_NULLABLE:
            return skip_metadata_blob_type_signature(function, blob, blobLength, ioOffset);
        case ZR_METADATA_SIGNATURE_NODE_FUNC:
            if (!read_metadata_blob_u32(blob, blobLength, ioOffset, &count) ||
                !skip_metadata_blob_type_signature(function, blob, blobLength, ioOffset)) {
                return ZR_FALSE;
            }
            for (TZrUInt32 index = 0; index < count; index++) {
                if (!skip_metadata_blob_type_signature(function, blob, blobLength, ioOffset)) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool function_has_member_ref_method_signature(const SZrFunction *function,
                                                        const TZrChar *symbolName,
                                                        TZrUInt32 expectedParameterCount) {
    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        const TZrByte *blob;
        TZrUInt32 offset = 0;
        TZrUInt32 effectKind = 0;
        TZrUInt32 genericArity = 0;
        TZrUInt32 parameterCount = 0;
        TZrUInt8 signatureVersion;
        TZrUInt8 callConvention;

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF ||
            record->signatureBlobLength == 0 ||
            record->signatureBlobOffset >= function->signatureBlobHeapLength ||
            (TZrUInt32)record->signatureBlobLength >
                    function->signatureBlobHeapLength - record->signatureBlobOffset) {
            continue;
        }

        blob = function->signatureBlobHeap + record->signatureBlobOffset;
        if (blob[offset++] != ZR_METADATA_SIGNATURE_NODE_MEMBER_REF) {
            continue;
        }
        offset = 1;
        if (!read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, "feature/app/helper/math") ||
            !read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, symbolName) ||
            !read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &effectKind) ||
            offset >= record->signatureBlobLength ||
            blob[offset++] != ZR_METADATA_SIGNATURE_NODE_METHOD_SIG ||
            offset + 2u > record->signatureBlobLength) {
            continue;
        }

        signatureVersion = blob[offset++];
        callConvention = blob[offset++];
        (void)signatureVersion;
        (void)callConvention;
        if (!read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &genericArity) ||
            genericArity != 0u ||
            !skip_metadata_blob_type_signature(function, blob, record->signatureBlobLength, &offset) ||
            !read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &parameterCount)) {
            continue;
        }

        for (TZrUInt32 paramIndex = 0; paramIndex < parameterCount; paramIndex++) {
            if (offset >= record->signatureBlobLength) {
                return ZR_FALSE;
            }
            offset++;
            if (!skip_metadata_blob_type_signature(function, blob, record->signatureBlobLength, &offset)) {
                return ZR_FALSE;
            }
        }

        if (parameterCount == expectedParameterCount) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_has_member_ref_target_method_signature_hash(const SZrFunction *function,
                                                                    const TZrChar *symbolName,
                                                                    TZrUInt32 expectedParameterCount) {
    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        const TZrByte *blob;
        TZrUInt32 offset = 0;
        TZrUInt32 effectKind = 0;
        TZrUInt32 genericArity = 0;
        TZrUInt32 parameterCount = 0;

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF ||
            record->targetSignatureHash == 0 ||
            record->signatureBlobLength == 0 ||
            record->signatureBlobOffset >= function->signatureBlobHeapLength ||
            (TZrUInt32)record->signatureBlobLength >
                    function->signatureBlobHeapLength - record->signatureBlobOffset) {
            continue;
        }

        blob = function->signatureBlobHeap + record->signatureBlobOffset;
        if (blob[offset++] != ZR_METADATA_SIGNATURE_NODE_MEMBER_REF ||
            !read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, "feature/app/helper/math") ||
            !read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, symbolName) ||
            !read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &effectKind) ||
            offset >= record->signatureBlobLength ||
            blob[offset] != ZR_METADATA_SIGNATURE_NODE_METHOD_SIG) {
            continue;
        }

        offset++;
        if (offset + 2u > record->signatureBlobLength) {
            continue;
        }
        offset += 2u;
        if (!read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &genericArity) ||
            genericArity != 0u ||
            !skip_metadata_blob_type_signature(function, blob, record->signatureBlobLength, &offset) ||
            !read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &parameterCount)) {
            continue;
        }

        for (TZrUInt32 paramIndex = 0; paramIndex < parameterCount; paramIndex++) {
            if (offset >= record->signatureBlobLength) {
                return ZR_FALSE;
            }
            offset++;
            if (!skip_metadata_blob_type_signature(function, blob, record->signatureBlobLength, &offset)) {
                return ZR_FALSE;
            }
        }

        if (parameterCount == expectedParameterCount &&
            offset == record->signatureBlobLength &&
            record->targetSignatureHash != 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_has_member_ref_target_signature_hash(const SZrFunction *function) {
    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF ||
            record->targetSignatureHash == 0) {
            continue;
        }
        for (TZrUInt32 signatureIndex = 0; signatureIndex < function->metadataTokenRecordLength; signatureIndex++) {
            const SZrMetadataTokenRecord *signatureRecord = &function->metadataTokenRecords[signatureIndex];

            if (signatureRecord->token == record->relatedToken &&
                ZR_METADATA_TOKEN_TABLE(signatureRecord->token) == ZR_METADATA_TABLE_SIGNATURE &&
                signatureRecord->targetSignatureHash == record->targetSignatureHash) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool function_corrupt_module_effect_target_signature_hash(SZrFunction *function,
                                                                    const TZrChar *moduleName,
                                                                    const TZrChar *symbolName) {
    if (function == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL ||
        function->moduleEntryEffects == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->moduleEntryEffectLength; index++) {
        SZrFunctionModuleEffect *effect = &function->moduleEntryEffects[index];
        const TZrChar *effectModuleName = test_string_text(effect->moduleName);
        const TZrChar *effectSymbolName = test_string_text(effect->symbolName);

        if (effectModuleName == ZR_NULL || effectSymbolName == ZR_NULL ||
            strcmp(effectModuleName, moduleName) != 0 ||
            strcmp(effectSymbolName, symbolName) != 0 ||
            effect->targetSignatureHash == 0u) {
            continue;
        }

        effect->targetSignatureHash ^= 0x9E3779B97F4A7C15ull;
        if (effect->targetSignatureHash == 0u) {
            effect->targetSignatureHash = 1u;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrMetadataToken corrupt_metadata_token_rid(TZrMetadataToken token) {
    TZrUInt32 table = ZR_METADATA_TOKEN_TABLE(token);
    TZrUInt32 rid = ZR_METADATA_TOKEN_RID(token);

    if (token == 0u || table == 0u) {
        return 0u;
    }

    rid = (rid + 1u) & ZR_METADATA_TOKEN_RID_MASK;
    if (rid == 0u) {
        rid = 1u;
    }
    return ZR_METADATA_TOKEN_MAKE(table, rid);
}

static SZrMetadataTokenRecord *find_metadata_token_record(SZrMetadataTokenRecord *records,
                                                          TZrUInt32 recordCount,
                                                          TZrMetadataToken token) {
    if (records == ZR_NULL || token == 0u) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < recordCount; index++) {
        if (records[index].token == token) {
            return &records[index];
        }
    }

    return ZR_NULL;
}

static const SZrMetadataTokenRecord *function_find_module_member_ref_record(const SZrFunction *function,
                                                                            const TZrChar *moduleName,
                                                                            const TZrChar *symbolName) {
    if (function == ZR_NULL || function->moduleMetadataTokenRecords == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->moduleMetadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->moduleMetadataTokenRecords[index];
        const TZrByte *blob;
        TZrUInt32 offset = 0;
        TZrUInt32 effectKind = 0;

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF ||
            record->signatureBlobLength == 0 ||
            record->signatureBlobOffset >= function->signatureBlobHeapLength ||
            (TZrUInt32)record->signatureBlobLength >
                    function->signatureBlobHeapLength - record->signatureBlobOffset) {
            continue;
        }

        blob = function->signatureBlobHeap + record->signatureBlobOffset;
        if (blob[offset++] == ZR_METADATA_SIGNATURE_NODE_MEMBER_REF &&
            read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, moduleName) &&
            read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, symbolName) &&
            read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &effectKind)) {
            return record;
        }
    }

    return ZR_NULL;
}

static TZrBool function_corrupt_module_effect_target_metadata_token(SZrFunction *function,
                                                                    const TZrChar *moduleName,
                                                                    const TZrChar *symbolName) {
    if (function == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL ||
        function->moduleEntryEffects == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->moduleEntryEffectLength; index++) {
        SZrFunctionModuleEffect *effect = &function->moduleEntryEffects[index];
        const TZrChar *effectModuleName = test_string_text(effect->moduleName);
        const TZrChar *effectSymbolName = test_string_text(effect->symbolName);
        TZrMetadataToken corruptedToken;

        if (effectModuleName == ZR_NULL || effectSymbolName == ZR_NULL ||
            strcmp(effectModuleName, moduleName) != 0 ||
            strcmp(effectSymbolName, symbolName) != 0 ||
            effect->targetMetadataToken == 0u) {
            continue;
        }

        corruptedToken = corrupt_metadata_token_rid(effect->targetMetadataToken);
        if (corruptedToken == 0u || corruptedToken == effect->targetMetadataToken) {
            continue;
        }
        effect->targetMetadataToken = corruptedToken;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool function_corrupt_module_effect_target_signature_token(SZrFunction *function,
                                                                     const TZrChar *moduleName,
                                                                     const TZrChar *symbolName) {
    if (function == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL ||
        function->moduleEntryEffects == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->moduleEntryEffectLength; index++) {
        SZrFunctionModuleEffect *effect = &function->moduleEntryEffects[index];
        const TZrChar *effectModuleName = test_string_text(effect->moduleName);
        const TZrChar *effectSymbolName = test_string_text(effect->symbolName);
        TZrMetadataToken corruptedToken;

        if (effectModuleName == ZR_NULL || effectSymbolName == ZR_NULL ||
            strcmp(effectModuleName, moduleName) != 0 ||
            strcmp(effectSymbolName, symbolName) != 0 ||
            effect->targetSignatureToken == 0u) {
            continue;
        }

        corruptedToken = corrupt_metadata_token_rid(effect->targetSignatureToken);
        if (corruptedToken == 0u || corruptedToken == effect->targetSignatureToken) {
            continue;
        }
        effect->targetSignatureToken = corruptedToken;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool function_corrupt_module_effect_target_module_signature_hash(SZrFunction *function,
                                                                           const TZrChar *moduleName,
                                                                           const TZrChar *symbolName) {
    if (function == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL ||
        function->moduleEntryEffects == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->moduleEntryEffectLength; index++) {
        SZrFunctionModuleEffect *effect = &function->moduleEntryEffects[index];
        const TZrChar *effectModuleName = test_string_text(effect->moduleName);
        const TZrChar *effectSymbolName = test_string_text(effect->symbolName);

        if (effectModuleName == ZR_NULL || effectSymbolName == ZR_NULL ||
            strcmp(effectModuleName, moduleName) != 0 ||
            strcmp(effectSymbolName, symbolName) != 0 ||
            effect->targetModuleSignatureHash == 0u) {
            continue;
        }

        effect->targetModuleSignatureHash ^= 0xD6E8FEB86659FD93ull;
        if (effect->targetModuleSignatureHash == 0u) {
            effect->targetModuleSignatureHash = 1u;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool function_clear_module_effect_target_identity(SZrFunction *function,
                                                            const TZrChar *moduleName,
                                                            const TZrChar *symbolName) {
    if (function == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL ||
        function->moduleEntryEffects == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->moduleEntryEffectLength; index++) {
        SZrFunctionModuleEffect *effect = &function->moduleEntryEffects[index];
        const TZrChar *effectModuleName = test_string_text(effect->moduleName);
        const TZrChar *effectSymbolName = test_string_text(effect->symbolName);

        if (effectModuleName == ZR_NULL || effectSymbolName == ZR_NULL ||
            strcmp(effectModuleName, moduleName) != 0 ||
            strcmp(effectSymbolName, symbolName) != 0 ||
            effect->targetSignatureHash == 0u) {
            continue;
        }

        effect->targetMetadataToken = 0u;
        effect->targetSignatureToken = 0u;
        effect->targetSignatureHash = 0u;
        effect->targetModuleSignatureHash = 0u;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool function_drop_module_entry_effects(SZrFunction *function) {
    if (function == ZR_NULL || function->moduleEntryEffects == ZR_NULL ||
        function->moduleEntryEffectLength == 0u) {
        return ZR_FALSE;
    }

    function->moduleEntryEffectLength = 0u;
    return ZR_TRUE;
}

static TZrBool function_has_module_effect_target_signature_hash(const SZrFunction *function,
                                                                const TZrChar *moduleName,
                                                                const TZrChar *symbolName) {
    if (function == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL ||
        function->moduleEntryEffects == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->moduleEntryEffectLength; index++) {
        const SZrFunctionModuleEffect *effect = &function->moduleEntryEffects[index];
        const TZrChar *effectModuleName = test_string_text(effect->moduleName);
        const TZrChar *effectSymbolName = test_string_text(effect->symbolName);

        if (effectModuleName != ZR_NULL &&
            effectSymbolName != ZR_NULL &&
            strcmp(effectModuleName, moduleName) == 0 &&
            strcmp(effectSymbolName, symbolName) == 0 &&
            effect->targetSignatureHash != 0u) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrUInt32 count_typed_exported_symbols_named(const SZrFunction *function, const TZrChar *symbolName) {
    TZrUInt32 count = 0;

    if (function == ZR_NULL || function->typedExportedSymbols == ZR_NULL || symbolName == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; index++) {
        const SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];
        const TZrChar *name = test_string_text(symbol->name);

        if (name != ZR_NULL && strcmp(name, symbolName) == 0) {
            count++;
        }
    }

    return count;
}

static TZrBool function_corrupt_child_module_effect_target_signature_hash(SZrFunction *function,
                                                                          const TZrChar *moduleName,
                                                                          const TZrChar *symbolName) {
    if (function == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
        SZrFunction *child = &function->childFunctionList[index];

        if (function_corrupt_module_effect_target_signature_hash(child, moduleName, symbolName) ||
            function_corrupt_child_module_effect_target_signature_hash(child, moduleName, symbolName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_corrupt_member_ref_target_signature_blob_byte(SZrFunction *function,
                                                                      const TZrChar *moduleName,
                                                                      const TZrChar *symbolName) {
    if (function == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL ||
        function->metadataTokenRecords == ZR_NULL || function->signatureBlobHeap == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        TZrByte *blob;
        TZrUInt32 offset = 0;
        TZrUInt32 effectKind = 0;
        TZrUInt32 genericArity = 0;
        TZrUInt32 parameterCount = 0;
        TZrUInt32 targetOffset;

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF ||
            record->targetSignatureHash == 0 ||
            record->signatureBlobLength == 0 ||
            record->signatureBlobOffset >= function->signatureBlobHeapLength ||
            (TZrUInt32)record->signatureBlobLength >
                    function->signatureBlobHeapLength - record->signatureBlobOffset) {
            continue;
        }

        blob = function->signatureBlobHeap + record->signatureBlobOffset;
        if (blob[offset++] != ZR_METADATA_SIGNATURE_NODE_MEMBER_REF ||
            !read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, moduleName) ||
            !read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, symbolName) ||
            !read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &effectKind) ||
            offset >= record->signatureBlobLength) {
            continue;
        }

        targetOffset = offset;
        if (blob[offset] == ZR_METADATA_SIGNATURE_NODE_FIELD_SIG) {
            offset++;
            if (offset >= record->signatureBlobLength) {
                continue;
            }
            offset++;
            if (!skip_metadata_blob_type_signature(function, blob, record->signatureBlobLength, &offset)) {
                continue;
            }
        } else if (blob[offset] == ZR_METADATA_SIGNATURE_NODE_METHOD_SIG) {
            offset++;
            if (offset + 2u > record->signatureBlobLength) {
                continue;
            }
            offset += 2u;
            if (!read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &genericArity) ||
                genericArity != 0u ||
                !skip_metadata_blob_type_signature(function, blob, record->signatureBlobLength, &offset) ||
                !read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &parameterCount)) {
                continue;
            }

            for (TZrUInt32 paramIndex = 0; paramIndex < parameterCount; paramIndex++) {
                if (offset >= record->signatureBlobLength) {
                    return ZR_FALSE;
                }
                offset++;
                if (!skip_metadata_blob_type_signature(function, blob, record->signatureBlobLength, &offset)) {
                    return ZR_FALSE;
                }
            }
        } else {
            continue;
        }

        if (offset == record->signatureBlobLength && targetOffset + 1u < record->signatureBlobLength) {
            blob[targetOffset + 1u] ^= 0x7Fu;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_corrupt_module_ref_target_signature_blob_byte_only(SZrState *state,
                                                                           SZrFunction *function,
                                                                           const TZrChar *moduleName,
                                                                           const TZrChar *symbolName) {
    TZrByte *newHeap;
    TZrUInt32 originalHeapLength;

    if (state == ZR_NULL || state->global == ZR_NULL ||
        function == ZR_NULL || function->moduleMetadataTokenRecords == ZR_NULL ||
        function->moduleMetadataTokenRecordLength == 0u || function->signatureBlobHeap == ZR_NULL ||
        function->signatureBlobHeapLength == 0u || moduleName == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_FALSE;
    }

    originalHeapLength = function->signatureBlobHeapLength;
    if (originalHeapLength > (TZrUInt32)0x7FFFFFFFu) {
        return ZR_FALSE;
    }

    newHeap = (TZrByte *)ZrCore_Memory_RawMallocWithType(state->global,
                                                        (TZrSize)originalHeapLength * 2u,
                                                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newHeap == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawCopy(newHeap, function->signatureBlobHeap, originalHeapLength);
    ZrCore_Memory_RawCopy(newHeap + originalHeapLength, function->signatureBlobHeap, originalHeapLength);

    for (TZrUInt32 index = 0; index < function->moduleMetadataTokenRecordLength; index++) {
        SZrMetadataTokenRecord *record = &function->moduleMetadataTokenRecords[index];
        TZrByte *blob;
        TZrUInt32 copiedOffset;
        TZrUInt32 offset = 0;
        TZrUInt32 effectKind = 0;
        TZrUInt32 genericArity = 0;
        TZrUInt32 parameterCount = 0;
        TZrUInt32 targetOffset;

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF ||
            record->targetSignatureHash == 0 ||
            record->signatureBlobLength == 0 ||
            record->signatureBlobOffset >= originalHeapLength ||
            (TZrUInt32)record->signatureBlobLength > originalHeapLength - record->signatureBlobOffset) {
            continue;
        }

        copiedOffset = record->signatureBlobOffset + originalHeapLength;
        blob = newHeap + copiedOffset;
        if (blob[offset++] != ZR_METADATA_SIGNATURE_NODE_MEMBER_REF ||
            !read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, moduleName) ||
            !read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, symbolName) ||
            !read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &effectKind) ||
            offset >= record->signatureBlobLength) {
            continue;
        }

        targetOffset = offset;
        if (blob[offset] == ZR_METADATA_SIGNATURE_NODE_FIELD_SIG) {
            offset++;
            if (offset >= record->signatureBlobLength) {
                continue;
            }
            offset++;
            if (!skip_metadata_blob_type_signature(function, blob, record->signatureBlobLength, &offset)) {
                continue;
            }
        } else if (blob[offset] == ZR_METADATA_SIGNATURE_NODE_METHOD_SIG) {
            offset++;
            if (offset + 2u > record->signatureBlobLength) {
                continue;
            }
            offset += 2u;
            if (!read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &genericArity) ||
                genericArity != 0u ||
                !skip_metadata_blob_type_signature(function, blob, record->signatureBlobLength, &offset) ||
                !read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &parameterCount)) {
                continue;
            }

            for (TZrUInt32 paramIndex = 0; paramIndex < parameterCount; paramIndex++) {
                if (offset >= record->signatureBlobLength) {
                    return ZR_FALSE;
                }
                offset++;
                if (!skip_metadata_blob_type_signature(function, blob, record->signatureBlobLength, &offset)) {
                    return ZR_FALSE;
                }
            }
        } else {
            continue;
        }

        if (offset == record->signatureBlobLength && targetOffset + 1u < record->signatureBlobLength) {
            blob[targetOffset + 1u] ^= 0x7Fu;
            ZrCore_Memory_RawFreeWithType(state->global,
                                          function->signatureBlobHeap,
                                          originalHeapLength,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            function->signatureBlobHeap = newHeap;
            function->signatureBlobHeapLength = originalHeapLength * 2u;
            record->signatureBlobOffset = copiedOffset;
            return ZR_TRUE;
        }
    }

    ZrCore_Memory_RawFreeWithType(state->global,
                                  newHeap,
                                  (TZrSize)originalHeapLength * 2u,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return ZR_FALSE;
}

static TZrBool function_corrupt_module_ref_target_metadata_token_only(SZrState *state,
                                                                      SZrFunction *function,
                                                                      const TZrChar *moduleName,
                                                                      const TZrChar *symbolName) {
    SZrMetadataTokenRecord *newRecords;
    TZrSize recordBytes;

    if (state == ZR_NULL || state->global == ZR_NULL ||
        function == ZR_NULL || function->moduleMetadataTokenRecords == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_FALSE;
    }

    recordBytes = sizeof(SZrMetadataTokenRecord) * function->moduleMetadataTokenRecordLength;
    newRecords = (SZrMetadataTokenRecord *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                           recordBytes,
                                                                           ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newRecords == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawCopy(newRecords, function->moduleMetadataTokenRecords, recordBytes);

    for (TZrUInt32 index = 0; index < function->moduleMetadataTokenRecordLength; index++) {
        SZrMetadataTokenRecord *record = &newRecords[index];
        const TZrByte *blob;
        TZrUInt32 offset = 0;
        TZrUInt32 effectKind = 0;
        TZrMetadataToken corruptedToken;

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF ||
            record->targetMetadataToken == 0 ||
            record->signatureBlobLength == 0 ||
            record->signatureBlobOffset >= function->signatureBlobHeapLength ||
            (TZrUInt32)record->signatureBlobLength >
                    function->signatureBlobHeapLength - record->signatureBlobOffset) {
            continue;
        }

        blob = function->signatureBlobHeap + record->signatureBlobOffset;
        if (blob[offset++] != ZR_METADATA_SIGNATURE_NODE_MEMBER_REF ||
            !read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, moduleName) ||
            !read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, symbolName) ||
            !read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &effectKind)) {
            continue;
        }

        corruptedToken = corrupt_metadata_token_rid(record->targetMetadataToken);
        if (corruptedToken == 0u || corruptedToken == record->targetMetadataToken) {
            continue;
        }
        record->targetMetadataToken = corruptedToken;
        if (function->moduleMetadataTokenRecords != function->metadataTokenRecords) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          function->moduleMetadataTokenRecords,
                                          recordBytes,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        function->moduleMetadataTokenRecords = newRecords;
        return ZR_TRUE;
    }

    ZrCore_Memory_RawFreeWithType(state->global,
                                  newRecords,
                                  recordBytes,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return ZR_FALSE;
}

static TZrBool function_corrupt_module_ref_type_owner_token_only(SZrState *state,
                                                                 SZrFunction *function,
                                                                 const TZrChar *moduleName,
                                                                 const TZrChar *symbolName) {
    SZrMetadataTokenRecord *newRecords;
    TZrSize recordBytes;

    if (state == ZR_NULL || state->global == ZR_NULL ||
        function == ZR_NULL || function->moduleMetadataTokenRecords == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL || moduleName == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_FALSE;
    }

    recordBytes = sizeof(SZrMetadataTokenRecord) * function->moduleMetadataTokenRecordLength;
    newRecords = (SZrMetadataTokenRecord *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                           recordBytes,
                                                                           ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newRecords == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawCopy(newRecords, function->moduleMetadataTokenRecords, recordBytes);

    for (TZrUInt32 index = 0; index < function->moduleMetadataTokenRecordLength; index++) {
        SZrMetadataTokenRecord *record = &newRecords[index];
        const TZrByte *blob;
        TZrUInt32 offset = 0;
        TZrUInt32 effectKind = 0;
        SZrMetadataTokenRecord *typeRecord;
        TZrMetadataToken corruptedOwnerToken;

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_MEMBER_REF ||
            record->signatureBlobLength == 0 ||
            record->signatureBlobOffset >= function->signatureBlobHeapLength ||
            (TZrUInt32)record->signatureBlobLength >
                    function->signatureBlobHeapLength - record->signatureBlobOffset) {
            continue;
        }

        blob = function->signatureBlobHeap + record->signatureBlobOffset;
        if (blob[offset++] != ZR_METADATA_SIGNATURE_NODE_MEMBER_REF ||
            !read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, moduleName) ||
            !read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, symbolName) ||
            !read_metadata_blob_u32(blob, record->signatureBlobLength, &offset, &effectKind)) {
            continue;
        }

        typeRecord = find_metadata_token_record(newRecords,
                                                function->moduleMetadataTokenRecordLength,
                                                record->ownerToken);
        if (typeRecord == ZR_NULL ||
            ZR_METADATA_TOKEN_TABLE(typeRecord->token) != ZR_METADATA_TABLE_TYPE_REF) {
            continue;
        }
        corruptedOwnerToken = corrupt_metadata_token_rid(typeRecord->ownerToken);
        if (corruptedOwnerToken == 0u || corruptedOwnerToken == typeRecord->ownerToken) {
            continue;
        }

        typeRecord->ownerToken = corruptedOwnerToken;
        if (function->moduleMetadataTokenRecords != function->metadataTokenRecords) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          function->moduleMetadataTokenRecords,
                                          recordBytes,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        function->moduleMetadataTokenRecords = newRecords;
        return ZR_TRUE;
    }

    ZrCore_Memory_RawFreeWithType(state->global,
                                  newRecords,
                                  recordBytes,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return ZR_FALSE;
}

static TZrBool function_corrupt_module_ref_assembly_version_range_only(SZrState *state,
                                                                       SZrFunction *function,
                                                                       const TZrChar *moduleName) {
    SZrString *requestedVersion;
    SZrString *minVersion;
    SZrString *maxVersion;

    if (state == ZR_NULL || function == ZR_NULL || function->moduleMetadataTokenRecords == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    requestedVersion = ZrCore_String_CreateFromNative(state, "2.0.0");
    minVersion = ZrCore_String_CreateFromNative(state, "2.0.0");
    maxVersion = ZrCore_String_CreateFromNative(state, "3.0.0");
    if (requestedVersion == ZR_NULL || minVersion == ZR_NULL || maxVersion == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->moduleMetadataTokenRecordLength; index++) {
        SZrMetadataTokenRecord *record = &function->moduleMetadataTokenRecords[index];
        SZrMetadataTokenRecord *signatureRecord;
        const TZrByte *blob;
        TZrUInt32 offset = 0;

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_ASSEMBLY_REF ||
            record->signatureBlobLength == 0 ||
            record->signatureBlobOffset >= function->signatureBlobHeapLength ||
            record->signatureBlobLength > function->signatureBlobHeapLength - record->signatureBlobOffset) {
            continue;
        }

        blob = function->signatureBlobHeap + record->signatureBlobOffset;
        if (blob[offset++] != ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF ||
            !read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, moduleName)) {
            continue;
        }

        record->requestedModuleVersion = requestedVersion;
        record->minModuleVersionInclusive = minVersion;
        record->maxModuleVersionExclusive = maxVersion;
        signatureRecord = find_metadata_token_record(function->moduleMetadataTokenRecords,
                                                     function->moduleMetadataTokenRecordLength,
                                                     record->relatedToken);
        if (signatureRecord != ZR_NULL) {
            signatureRecord->requestedModuleVersion = requestedVersion;
            signatureRecord->minModuleVersionInclusive = minVersion;
            signatureRecord->maxModuleVersionExclusive = maxVersion;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static const SZrMetadataTokenRecord *find_assembly_ref_record_named(const SZrFunction *function,
                                                                    const TZrChar *moduleName) {
    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL ||
        function->signatureBlobHeap == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        const SZrMetadataTokenRecord *record = &function->metadataTokenRecords[index];
        const TZrByte *blob;
        TZrUInt32 offset = 0;

        if (ZR_METADATA_TOKEN_TABLE(record->token) != ZR_METADATA_TABLE_ASSEMBLY_REF ||
            record->signatureBlobLength == 0 ||
            record->signatureBlobOffset >= function->signatureBlobHeapLength ||
            record->signatureBlobLength > function->signatureBlobHeapLength - record->signatureBlobOffset) {
            continue;
        }

        blob = function->signatureBlobHeap + record->signatureBlobOffset;
        if (blob[offset++] == ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF &&
            read_metadata_blob_string_matches(function, blob, record->signatureBlobLength, &offset, moduleName)) {
            return record;
        }
    }

    return ZR_NULL;
}

static const SZrMetadataTokenRecord *find_module_ref_owner_record(const SZrFunction *function,
                                                                  const SZrMetadataTokenRecord *record,
                                                                  EZrMetadataTableTag expectedTable) {
    const SZrMetadataTokenRecord *ownerRecord;

    if (function == ZR_NULL || record == ZR_NULL ||
        function->moduleMetadataTokenRecords == ZR_NULL ||
        record->ownerToken == 0u) {
        return ZR_NULL;
    }

    ownerRecord = find_metadata_token_record(function->moduleMetadataTokenRecords,
                                             function->moduleMetadataTokenRecordLength,
                                             record->ownerToken);
    if (ownerRecord == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(ownerRecord->token) != (TZrUInt32)expectedTable) {
        return ZR_NULL;
    }

    return ownerRecord;
}

static TZrBool metadata_token_record_module_versions_match(const SZrMetadataTokenRecord *record,
                                                           const TZrChar *requestedVersion,
                                                           const TZrChar *minVersionInclusive,
                                                           const TZrChar *maxVersionExclusive) {
    const TZrChar *actualRequestedVersion;
    const TZrChar *actualMinVersionInclusive;
    const TZrChar *actualMaxVersionExclusive;

    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

    actualRequestedVersion = test_string_text(record->requestedModuleVersion);
    actualMinVersionInclusive = test_string_text(record->minModuleVersionInclusive);
    actualMaxVersionExclusive = test_string_text(record->maxModuleVersionExclusive);
    return actualRequestedVersion != ZR_NULL &&
           actualMinVersionInclusive != ZR_NULL &&
           actualMaxVersionExclusive != ZR_NULL &&
           strcmp(actualRequestedVersion, requestedVersion) == 0 &&
           strcmp(actualMinVersionInclusive, minVersionInclusive) == 0 &&
           strcmp(actualMaxVersionExclusive, maxVersionExclusive) == 0;
}

static void test_project_compile_canonicalizes_relative_and_alias_imports(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    const SZrFunctionCallableSummary *runSummary;
    SZrString *sourceName;
    TZrSize mainLength = 0;
    TZrChar *mainContent;

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    mainContent = ZrTests_ReadTextFile(fixture.mainPath, &mainLength);
    TEST_ASSERT_NOT_NULL(mainContent);
    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, mainContent, mainLength, sourceName);
    free(mainContent);
    TEST_ASSERT_NOT_NULL(function);
    runSummary = find_exported_callable_summary_named(function, "run");
    TEST_ASSERT_NOT_NULL(runSummary);

    TEST_ASSERT_EQUAL_UINT32(2u, function->staticImportLength);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(function, "feature/app/helper/math"));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(function, "common/shared/crypto/hash"));
    TEST_ASSERT_TRUE(callable_summary_contains_module_effect_named(runSummary, "feature/app/helper/math"));
    TEST_ASSERT_TRUE(callable_summary_contains_module_effect_named(runSummary, "common/shared/crypto/hash"));
    TEST_ASSERT_FALSE(function_contains_module_effect_named(function, ".helper.math"));
    TEST_ASSERT_FALSE(function_contains_module_effect_named(function, "@shared.crypto.hash"));
    TEST_ASSERT_FALSE(callable_summary_contains_module_effect_named(runSummary, ".helper.math"));
    TEST_ASSERT_FALSE(callable_summary_contains_module_effect_named(runSummary, "@shared.crypto.hash"));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_project_compile_records_using_import_guard_dependencies(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    const SZrFunctionCallableSummary *runSummary;
    SZrString *sourceName;
    static const TZrChar *source =
            "pub func run(): i32 {\n"
            "    using (var helper = %import(\".helper.math\")) {\n"
            "        return helper.answer;\n"
            "    } else {\n"
            "        return 0;\n"
            "    }\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    runSummary = find_exported_callable_summary_named(function, "run");
    TEST_ASSERT_NOT_NULL(runSummary);

    TEST_ASSERT_TRUE(callable_summary_contains_module_effect_named(runSummary, "feature/app/helper/math"));
    TEST_ASSERT_FALSE(callable_summary_contains_module_effect_named(runSummary, ".helper.math"));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_metadata_records_with_table(function,
                                                                           ZR_METADATA_TABLE_MODULE));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_metadata_records_with_table(function,
                                                                           ZR_METADATA_TABLE_ASSEMBLY_REF));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_metadata_records_with_table(function,
                                                                           ZR_METADATA_TABLE_TYPE_REF));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_metadata_records_with_table(function,
                                                                           ZR_METADATA_TABLE_MEMBER_REF));
    TEST_ASSERT_EQUAL_UINT32(5u,
                             (TZrUInt32)count_metadata_records_with_table(function,
                                                                           ZR_METADATA_TABLE_SIGNATURE));
    TEST_ASSERT_TRUE(function_has_table_signature_blob_node(function,
                                                            ZR_METADATA_TABLE_MODULE,
                                                            ZR_METADATA_SIGNATURE_NODE_MODULE));
    TEST_ASSERT_TRUE(function_has_table_signature_blob_node(function,
                                                            ZR_METADATA_TABLE_ASSEMBLY_REF,
                                                            ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF));
    TEST_ASSERT_TRUE(function_has_table_signature_blob_node(function,
                                                            ZR_METADATA_TABLE_TYPE_REF,
                                                            ZR_METADATA_SIGNATURE_NODE_TYPE_REF));
    TEST_ASSERT_TRUE(function_has_member_ref_signature_blob(function));
    TEST_ASSERT_TRUE(function_has_member_ref_signature_hash(function));
    TEST_ASSERT_TRUE(function_has_member_ref_target_signature_hash(function));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_project_compile_records_using_import_guard_method_signature(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    static const TZrChar *source =
            "pub func run(): i32 {\n"
            "    using (var helper = %import(\".helper.math\")) {\n"
            "        return helper.add(1, 2);\n"
            "    } else {\n"
            "        return 0;\n"
            "    }\n"
            "}\n";
    static const TZrChar *helperContent =
            "pub func add(left: i32, right: i32): i32 {\n"
            "    return left + right;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));
    TEST_ASSERT_TRUE(write_text_file(fixture.helperPath, helperContent));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(function_has_member_ref_method_signature(function, "add", 2u));
    TEST_ASSERT_TRUE(function_has_member_ref_target_method_signature_hash(function, "add", 2u));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_runtime_rejects_signature_hash_mismatch(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "using (var helper = %import(\".helper.math\")) {\n"
            "    return helper.answer;\n"
            "} else {\n"
            "    return 77;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_module_effect_target_signature_hash(function,
                                                                         "feature/app/helper/math",
                                                                         "answer"));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(77, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_runtime_rejects_target_token_mismatch(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "using (var helper = %import(\".helper.math\")) {\n"
            "    return helper.answer;\n"
            "} else {\n"
            "    return 77;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_module_effect_target_metadata_token(function,
                                                                         "feature/app/helper/math",
                                                                         "answer"));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(77, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_runtime_rejects_target_module_hash_mismatch(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "using (var helper = %import(\".helper.math\")) {\n"
            "    return helper.answer;\n"
            "} else {\n"
            "    return 77;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_module_effect_target_module_signature_hash(function,
                                                                                "feature/app/helper/math",
                                                                                "answer"));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(77, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_required_import_runtime_rejects_signature_hash_mismatch(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    const TZrChar *message;
    SZrTypeValue resultValue;
    static const TZrChar *source =
            "var helper = %import(\".helper.math\");\n"
            "return helper.answer;\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_module_effect_target_signature_hash(function,
                                                                         "feature/app/helper/math",
                                                                         "answer"));

    ZrCore_Value_ResetAsNull(&resultValue);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &resultValue));
    message = current_exception_message(state);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_NOT_NULL(strstr(message, "import signature mismatch"));
    TEST_ASSERT_NOT_NULL(strstr(message, "feature/app/helper/math"));
    TEST_ASSERT_NOT_NULL(strstr(message, "answer"));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_required_import_runtime_reports_target_token_mismatch(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    const TZrChar *message;
    SZrTypeValue resultValue;
    static const TZrChar *source =
            "var helper = %import(\".helper.math\");\n"
            "return helper.answer;\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_module_effect_target_metadata_token(function,
                                                                         "feature/app/helper/math",
                                                                         "answer"));
    TEST_ASSERT_TRUE(function_corrupt_module_effect_target_signature_token(function,
                                                                          "feature/app/helper/math",
                                                                          "answer"));

    ZrCore_Value_ResetAsNull(&resultValue);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &resultValue));
    message = current_exception_message(state);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_NOT_NULL(strstr(message, "import signature mismatch"));
    TEST_ASSERT_NOT_NULL(strstr(message, "feature/app/helper/math"));
    TEST_ASSERT_NOT_NULL(strstr(message, "answer"));
    TEST_ASSERT_NOT_NULL(strstr(message, "expectedMetadataToken="));
    TEST_ASSERT_NOT_NULL(strstr(message, "actualMetadataToken="));
    TEST_ASSERT_NOT_NULL(strstr(message, "expectedSignatureToken="));
    TEST_ASSERT_NOT_NULL(strstr(message, "actualSignatureToken="));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_required_import_runtime_rejects_target_module_hash_mismatch(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    const TZrChar *message;
    SZrTypeValue resultValue;
    static const TZrChar *source =
            "var helper = %import(\".helper.math\");\n"
            "return helper.answer;\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_module_effect_target_module_signature_hash(function,
                                                                                "feature/app/helper/math",
                                                                                "answer"));

    ZrCore_Value_ResetAsNull(&resultValue);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &resultValue));
    message = current_exception_message(state);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_NOT_NULL(strstr(message, "assembly_signature_mismatch"));
    TEST_ASSERT_NOT_NULL(strstr(message, "feature/app/helper/math"));
    TEST_ASSERT_NOT_NULL(strstr(message, "answer"));
    TEST_ASSERT_NOT_NULL(strstr(message, "expectedModuleHash="));
    TEST_ASSERT_NOT_NULL(strstr(message, "actualModuleHash="));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_required_import_runtime_rejects_assembly_ref_version_mismatch(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    const TZrChar *message;
    SZrTypeValue resultValue;
    static const TZrChar *source =
            "var helper = %import(\".helper.math\");\n"
            "return helper.answer;\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_module_ref_assembly_version_range_only(state,
                                                                             function,
                                                                             "feature/app/helper/math"));
    TEST_ASSERT_TRUE(function_drop_module_entry_effects(function));

    ZrCore_Value_ResetAsNull(&resultValue);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &resultValue));
    message = current_exception_message(state);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_NOT_NULL(strstr(message, "assembly_version_mismatch"));
    TEST_ASSERT_NOT_NULL(strstr(message, "feature/app/helper/math"));
    TEST_ASSERT_NOT_NULL(strstr(message, "answer"));
    TEST_ASSERT_NOT_NULL(strstr(message, "minVersionInclusive=2.0.0"));
    TEST_ASSERT_NOT_NULL(strstr(message, "maxVersionExclusive=3.0.0"));
    TEST_ASSERT_NOT_NULL(strstr(message, "actualVersion=1.0.0"));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_required_import_runtime_resolves_same_name_signature_candidate(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *helperFunction;
    SZrFunction *function;
    SZrString *helperSourceName;
    SZrString *sourceName;
    TZrChar *helperSource;
    TZrSize helperLength = 0;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "var helper = %import(\".helper.math\");\n"
            "return helper.pick(true);\n";
    static const TZrChar *helperContent =
            "pub func pick(value: int): int {\n"
            "    return value;\n"
            "}\n"
            "pub func pick(value: bool): int {\n"
            "    return 42;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));
    TEST_ASSERT_TRUE(write_text_file(fixture.helperPath, helperContent));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    helperSource = ZrTests_ReadTextFile(fixture.helperPath, &helperLength);
    TEST_ASSERT_NOT_NULL(helperSource);
    helperSourceName = ZrCore_String_CreateFromNative(state, fixture.helperPath);
    TEST_ASSERT_NOT_NULL(helperSourceName);
    helperFunction = ZrParser_Source_Compile(state, helperSource, helperLength, helperSourceName);
    free(helperSource);
    TEST_ASSERT_NOT_NULL(helperFunction);
    TEST_ASSERT_EQUAL_UINT32(2u, count_typed_exported_symbols_named(helperFunction, "pick"));
    ZrCore_Function_Free(state, helperFunction);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(42, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_runtime_rejects_signature_blob_mismatch_with_matching_hash(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "using (var helper = %import(\".helper.math\")) {\n"
            "    return helper.answer;\n"
            "} else {\n"
            "    return 77;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_member_ref_target_signature_blob_byte(function,
                                                                            "feature/app/helper/math",
                                                                            "answer"));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(77, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_runtime_consumes_module_ref_signature_blob(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "using (var helper = %import(\".helper.math\")) {\n"
            "    return helper.answer;\n"
            "} else {\n"
            "    return 77;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_module_ref_target_signature_blob_byte_only(state,
                                                                                function,
                                                                                "feature/app/helper/math",
                                                                                "answer"));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(77, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_runtime_rejects_module_ref_target_token_mismatch(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "using (var helper = %import(\".helper.math\")) {\n"
            "    return helper.answer;\n"
            "} else {\n"
            "    return 77;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_module_ref_target_metadata_token_only(state,
                                                                           function,
                                                                           "feature/app/helper/math",
                                                                           "answer"));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(77, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_runtime_rejects_module_ref_owner_chain_mismatch(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "using (var helper = %import(\".helper.math\")) {\n"
            "    return helper.answer;\n"
            "} else {\n"
            "    return 77;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_module_ref_type_owner_token_only(state,
                                                                       function,
                                                                       "feature/app/helper/math",
                                                                       "answer"));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(77, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_runtime_uses_module_ref_identity_when_effect_targets_are_missing(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "using (var helper = %import(\".helper.math\")) {\n"
            "    return helper.answer;\n"
            "} else {\n"
            "    return 77;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_clear_module_effect_target_identity(function,
                                                                 "feature/app/helper/math",
                                                                 "answer"));
    TEST_ASSERT_TRUE(function_corrupt_module_ref_target_metadata_token_only(state,
                                                                           function,
                                                                           "feature/app/helper/math",
                                                                           "answer"));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(77, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_signature_reports_typespec_mismatch_diagnostic(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *callerFunction;
    SZrFunction *providerFunction;
    SZrObjectModule *providerModule;
    SZrString *moduleName;
    const TZrChar *diagnostic;

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    moduleName = ZrCore_String_CreateFromNative(state, "typespec.provider");
    TEST_ASSERT_NOT_NULL(moduleName);
    callerFunction = create_project_import_typespec_entry_function(state, "Box<int>");
    providerFunction = create_project_import_typespec_entry_function(state, "Box<float>");
    providerModule = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(callerFunction);
    TEST_ASSERT_NOT_NULL(providerFunction);
    TEST_ASSERT_NOT_NULL(providerModule);

    ZrCore_Module_SetInfo(state,
                          providerModule,
                          moduleName,
                          ZrCore_Module_CalculatePathHash(state, moduleName),
                          moduleName);
    ZrCore_Reflection_AttachModuleRuntimeMetadata(state, providerModule, providerFunction);
    TEST_ASSERT_TRUE(attach_project_import_typespec_effect(state,
                                                          callerFunction,
                                                          moduleName,
                                                          providerFunction));
    ZrCore_GlobalState_ClearModuleLoadDiagnostic(global);

    TEST_ASSERT_TRUE(zr_module_import_signature_verify(state,
                                                      callerFunction,
                                                      moduleName,
                                                      providerModule,
                                                      ZR_NULL));
    diagnostic = ZrCore_GlobalState_GetModuleLoadDiagnostic(global);
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "type_spec_mismatch"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "typespec.provider"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "make"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "unmatchedTypeSpecs=1"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "firstUnmatchedTypeSpecToken="));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "firstUnmatchedSignatureHash="));

    ZrCore_Function_Free(state, callerFunction);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_runtime_verifies_module_ref_table_without_entry_effects(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "using (var helper = %import(\".helper.math\")) {\n"
            "    return helper.answer;\n"
            "} else {\n"
            "    return 77;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_module_ref_target_signature_blob_byte_only(state,
                                                                                function,
                                                                                "feature/app/helper/math",
                                                                                "answer"));
    TEST_ASSERT_TRUE(function_drop_module_entry_effects(function));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(77, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_runtime_records_module_ref_binding_result(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    const SZrMetadataTokenRecord *memberRefRecord;
    const SZrMetadataTokenRecord *typeRefRecord;
    const SZrMetadataTokenRecord *assemblyRefRecord;
    const SZrMetadataTokenBinding *binding;
    const SZrMetadataTokenBinding *assemblyBinding;
    TZrMetadataToken memberRefToken;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "using (var helper = %import(\".helper.math\")) {\n"
            "    return helper.answer;\n"
            "} else {\n"
            "    return 77;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    memberRefRecord = function_find_module_member_ref_record(function, "feature/app/helper/math", "answer");
    TEST_ASSERT_NOT_NULL(memberRefRecord);
    typeRefRecord = find_module_ref_owner_record(function, memberRefRecord, ZR_METADATA_TABLE_TYPE_REF);
    TEST_ASSERT_NOT_NULL(typeRefRecord);
    assemblyRefRecord = find_module_ref_owner_record(function, typeRefRecord, ZR_METADATA_TABLE_ASSEMBLY_REF);
    TEST_ASSERT_NOT_NULL(assemblyRefRecord);
    memberRefToken = memberRefRecord->token;
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_MEMBER_REF, ZR_METADATA_TOKEN_TABLE(memberRefToken));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(40, result);
    binding = ZrCore_Function_FindModuleMetadataBinding(function, memberRefToken);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(memberRefToken, binding->refToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_SIGNATURE, ZR_METADATA_TOKEN_TABLE(binding->refSignatureToken));
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, binding->refSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_MEMBER_DEF, ZR_METADATA_TOKEN_TABLE(binding->resolvedMetadataToken));
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_SIGNATURE, ZR_METADATA_TOKEN_TABLE(binding->resolvedSignatureToken));
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, binding->resolvedSignatureHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, binding->resolvedModuleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_MEMBER_REF, ZR_METADATA_TOKEN_TABLE(memberRefRecord->token));
    assemblyBinding = ZrCore_Function_FindModuleMetadataBinding(function, assemblyRefRecord->token);
    TEST_ASSERT_NOT_NULL(assemblyBinding);
    TEST_ASSERT_EQUAL_UINT32(assemblyRefRecord->token, assemblyBinding->refToken);
    TEST_ASSERT_EQUAL_UINT32(assemblyRefRecord->relatedToken, assemblyBinding->refSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(assemblyRefRecord->signatureHash, assemblyBinding->refSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(assemblyRefRecord->token, assemblyBinding->expectedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(assemblyRefRecord->relatedToken, assemblyBinding->expectedSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(assemblyRefRecord->signatureHash, assemblyBinding->expectedSignatureHash);
    TEST_ASSERT_EQUAL_UINT64(assemblyRefRecord->targetModuleSignatureHash,
                             assemblyBinding->expectedModuleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_MODULE,
                             ZR_METADATA_TOKEN_TABLE(assemblyBinding->resolvedMetadataToken));
    TEST_ASSERT_EQUAL_UINT32(1u, ZR_METADATA_TOKEN_RID(assemblyBinding->resolvedMetadataToken));
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TABLE_SIGNATURE,
                             ZR_METADATA_TOKEN_TABLE(assemblyBinding->resolvedSignatureToken));
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, assemblyBinding->resolvedSignatureHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, assemblyBinding->resolvedModuleSignatureHash);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_runtime_unavailable_provider_falls_back_to_else(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "using (var helper = %import(\".helper.math\")) {\n"
            "    return helper.answer;\n"
            "} else {\n"
            "    return 77;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_INT(0, remove(fixture.helperPath));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(77, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_runtime_checks_nested_caller_signature_hash(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "pub func run(): i32 {\n"
            "    using (var helper = %import(\".helper.math\")) {\n"
            "        return helper.answer;\n"
            "    } else {\n"
            "        return 77;\n"
            "    }\n"
            "}\n"
            "return run();\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_child_module_effect_target_signature_hash(function,
                                                                               "feature/app/helper/math",
                                                                               "answer"));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(77, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_records_native_target_signature_hash(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    static const TZrChar *source =
            "using (var math = %import(\"zr.math\")) {\n"
            "    return math.abs(-3.0);\n"
            "} else {\n"
            "    return 0.0;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(global));
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(function_has_module_effect_target_signature_hash(function, "zr.math", "abs"));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_using_import_guard_runtime_rejects_native_target_signature_hash_mismatch(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "using (var math = %import(\"zr.math\")) {\n"
            "    math.abs(-3.0);\n"
            "    return 40;\n"
            "} else {\n"
            "    return 77;\n"
            "}\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(global));
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_corrupt_module_effect_target_signature_hash(function, "zr.math", "abs"));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(77, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_required_import_runtime_accepts_native_module_link_signature(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    TZrInt64 result = 0;
    static const TZrChar *source =
            "var system = %import(\"zr.system\");\n"
            "var console = system.console;\n"
            "return 1;\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_has_module_effect_target_signature_hash(function, "zr.system", "console"));

    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_required_import_runtime_reports_native_provider_unavailable(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    const TZrChar *message;
    SZrTypeValue resultValue;
    static const TZrChar *source =
            "var math = %import(\"zr.math\");\n"
            "return math.abs(-3.0);\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrVmLibMath_Register(global));
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    ZrLibrary_NativeRegistry_Free(global);
    ZrCore_Module_RemoveFromCache(state, ZrCore_String_CreateFromNative(state, "zr.math"));
    global->sourceLoader = ZR_NULL;
    ZrCore_Value_ResetAsNull(&resultValue);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &resultValue));
    message = current_exception_message(state);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_NOT_NULL(strstr(message, "import_load_unavailable"));
    TEST_ASSERT_NOT_NULL(strstr(message, "zr.math"));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_required_import_runtime_reports_source_loader_attempts(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    const TZrChar *message;
    SZrTypeValue resultValue;
    static const TZrChar *source =
            "var helper = %import(\".helper.math\");\n"
            "return helper.answer;\n";

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_EQUAL_INT(0, remove(fixture.helperPath));
    ZrCore_Module_RemoveFromCache(state, ZrCore_String_CreateFromNative(state, "feature/app/helper/math"));
    ZrCore_Value_ResetAsNull(&resultValue);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &resultValue));
    message = current_exception_message(state);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_NOT_NULL(strstr(message, "import_load_unavailable"));
    TEST_ASSERT_NOT_NULL(strstr(message, "feature/app/helper/math"));
    TEST_ASSERT_NOT_NULL(strstr(message, "loader=project-source"));
    TEST_ASSERT_NOT_NULL(strstr(message, "source="));
    TEST_ASSERT_NOT_NULL(strstr(message, "feature/app/helper/math.zr"));
    TEST_ASSERT_NOT_NULL(strstr(message, "binary="));
    TEST_ASSERT_NOT_NULL(strstr(message, "feature/app/helper/math.zro"));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_required_import_runtime_reports_descriptor_plugin_load_error(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrString *moduleName;
    const TZrChar *diagnostic;
    const TZrChar *lastError;
    TZrChar projectDirectory[ZR_TESTS_PATH_MAX];
    TZrChar nativeDirectory[ZR_TESTS_PATH_MAX];
    TZrChar pluginName[ZR_TESTS_PATH_MAX];
    TZrChar pluginPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    snprintf(projectDirectory, sizeof(projectDirectory), "%s", fixture.projectPath);
    lastSeparator = find_last_path_separator(projectDirectory);
    TEST_ASSERT_NOT_NULL(lastSeparator);
    *lastSeparator = '\0';
    ZrLibrary_File_PathJoin(projectDirectory, "native", nativeDirectory);
    snprintf(pluginName,
             sizeof(pluginName),
             "zrvm_native_acme_bad%s",
             native_plugin_extension_for_test());
    ZrLibrary_File_PathJoin(nativeDirectory, pluginName, pluginPath);
    TEST_ASSERT_TRUE(write_text_file(pluginPath, "not a dynamic library\n"));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    moduleName = ZrCore_String_CreateFromNative(state, "acme.bad");
    TEST_ASSERT_NOT_NULL(moduleName);
    TEST_ASSERT_NULL(ZrCore_Module_ImportByPath(state, moduleName));
    diagnostic = ZrCore_GlobalState_GetModuleLoadDiagnostic(global);
    lastError = ZrLibrary_NativeRegistry_GetLastErrorMessage(global);
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_NOT_NULL(lastError);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "loader=native-plugin"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "descriptor-load-failed"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "acme.bad"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "failed to load native plugin"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "zrvm_native_acme_bad"));
    TEST_ASSERT_NOT_NULL(strstr(lastError, "failed to load native plugin"));
    TEST_ASSERT_NOT_NULL(strstr(lastError, "zrvm_native_acme_bad"));

    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_project_import_reports_aot_descriptor_load_error(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrString *moduleName;
    const TZrChar *diagnostic;
    const TZrChar *lastError;

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_TRUE));

    moduleName = ZrCore_String_CreateFromNative(state, "feature/app/helper/math");
    TEST_ASSERT_NOT_NULL(moduleName);
    TEST_ASSERT_NULL(ZrCore_Module_ImportByPath(state, moduleName));
    diagnostic = ZrCore_GlobalState_GetModuleLoadDiagnostic(global);
    lastError = ZrLibrary_AotRuntime_GetLastError(global);
    TEST_ASSERT_NOT_NULL(diagnostic);
    TEST_ASSERT_NOT_NULL(lastError);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "loader=aot-runtime"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "backend=aot-c"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "descriptor-load-failed"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "feature/app/helper/math"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic, "missing AOT artifacts"));
    TEST_ASSERT_NOT_NULL(strstr(lastError, "missing AOT artifacts"));

    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_project_compile_applies_dependency_import_version_range_to_assembly_ref(void) {
    SZrProjectDependencyImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    const SZrMetadataTokenRecord *assemblyRef;
    TZrSize sourceLength = 0;
    TZrChar *sourceContent;

    TEST_ASSERT_TRUE(prepare_project_dependency_import_version_range_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceContent = ZrTests_ReadTextFile(fixture.rootMainPath, &sourceLength);
    TEST_ASSERT_NOT_NULL(sourceContent);
    sourceName = ZrCore_String_CreateFromNative(state, fixture.rootMainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, sourceContent, sourceLength, sourceName);
    free(sourceContent);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_EQUAL_UINT32(1u, function->staticImportLength);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(function, "$math@1.2.3/ops/sum"));
    assemblyRef = find_assembly_ref_record_named(function, "$math@1.2.3/ops/sum");
    TEST_ASSERT_NOT_NULL(assemblyRef);
    TEST_ASSERT_TRUE(metadata_token_record_module_versions_match(assemblyRef,
                                                                 "1.2.3",
                                                                 "1.2.0",
                                                                 "1.3.0"));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_project_compile_emits_assembly_ref_identity_from_zrp_references(void) {
    SZrProjectDependencyImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    const SZrMetadataTokenRecord *assemblyRef;
    TZrSize sourceLength = 0;
    TZrChar *sourceContent;

    TEST_ASSERT_TRUE(prepare_project_assembly_reference_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceContent = ZrTests_ReadTextFile(fixture.rootMainPath, &sourceLength);
    TEST_ASSERT_NOT_NULL(sourceContent);
    sourceName = ZrCore_String_CreateFromNative(state, fixture.rootMainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, sourceContent, sourceLength, sourceName);
    free(sourceContent);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_EQUAL_UINT32(1u, function->staticImportLength);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(function, "$mathLocal@2.1.0/ops/sum"));
    TEST_ASSERT_NULL(find_assembly_ref_record_named(function, "$mathLocal@2.1.0/ops/sum"));
    assemblyRef = find_assembly_ref_record_named(function, "zr.math");
    TEST_ASSERT_NOT_NULL(assemblyRef);
    TEST_ASSERT_TRUE(metadata_token_record_module_versions_match(assemblyRef,
                                                                 "2.1.0",
                                                                 "2.0.0",
                                                                 "3.0.0"));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_project_compile_reads_referenced_assembly_from_zro_without_source(void) {
    SZrProjectDependencyImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;
    const SZrMetadataTokenRecord *assemblyRef;
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar mathProjectPath[ZR_TESTS_PATH_MAX];
    TZrChar *lastSeparator;
    TZrSize sourceLength = 0;
    TZrChar *sourceContent;

    TEST_ASSERT_TRUE(prepare_project_assembly_reference_import_fixture(&fixture));

    snprintf(rootPath, sizeof(rootPath), "%s", fixture.projectPath);
    lastSeparator = find_last_path_separator(rootPath);
    TEST_ASSERT_NOT_NULL(lastSeparator);
    *lastSeparator = '\0';
    ZrLibrary_File_PathJoin(rootPath, "deps/math/math.zrp", mathProjectPath);

    TEST_ASSERT_TRUE(compile_project_source_to_zro(mathProjectPath,
                                                   fixture.mathMainPath,
                                                   fixture.mathBinaryPath,
                                                   "$mathLocal@2.1.0/ops/sum"));
    TEST_ASSERT_EQUAL_INT(0, remove(fixture.mathMainPath));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceContent = ZrTests_ReadTextFile(fixture.rootMainPath, &sourceLength);
    TEST_ASSERT_NOT_NULL(sourceContent);
    sourceName = ZrCore_String_CreateFromNative(state, fixture.rootMainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, sourceContent, sourceLength, sourceName);
    free(sourceContent);
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(function, "$mathLocal@2.1.0/ops/sum"));
    TEST_ASSERT_TRUE(function_has_module_effect_target_signature_hash(function,
                                                                      "$mathLocal@2.1.0/ops/sum",
                                                                      "value"));
    assemblyRef = find_assembly_ref_record_named(function, "zr.math");
    TEST_ASSERT_NOT_NULL(assemblyRef);
    TEST_ASSERT_TRUE(metadata_token_record_module_versions_match(assemblyRef,
                                                                 "2.1.0",
                                                                 "2.0.0",
                                                                 "3.0.0"));

    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_project_assembly_ref_identity_roundtrips_through_zro_module_effects(void) {
    SZrProjectDependencyImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrFunction *runtimeFunction = ZR_NULL;
    SZrString *sourceName;
    ZrTestsFixtureReader reader;
    SZrIo io;
    SZrIoSource *sourceObject;
    const SZrIoFunction *binaryEntry;
    TZrChar binaryPath[ZR_TESTS_PATH_MAX];
    TZrByte *binaryBytes = ZR_NULL;
    TZrSize binaryLength = 0;
    TZrSize sourceLength = 0;
    TZrChar *sourceContent;

    TEST_ASSERT_TRUE(prepare_project_assembly_reference_import_fixture(&fixture));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("parser",
                                                       "project_import_canonicalization",
                                                       "assembly_reference_effect_roundtrip",
                                                       ".zro",
                                                       binaryPath,
                                                       sizeof(binaryPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(binaryPath));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceContent = ZrTests_ReadTextFile(fixture.rootMainPath, &sourceLength);
    TEST_ASSERT_NOT_NULL(sourceContent);
    sourceName = ZrCore_String_CreateFromNative(state, fixture.rootMainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, sourceContent, sourceLength, sourceName);
    free(sourceContent);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(function_contains_module_effect_with_assembly(function,
                                                                   "$mathLocal@2.1.0/ops/sum",
                                                                   "zr.math"));

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
    binaryBytes = ZrTests_Fixture_ReadFileBytes(binaryPath, &binaryLength);
    TEST_ASSERT_NOT_NULL(binaryBytes);
    TEST_ASSERT_TRUE(binaryLength > 0);

    memset(&reader, 0, sizeof(reader));
    reader.bytes = binaryBytes;
    reader.length = binaryLength;
    memset(&io, 0, sizeof(io));
    ZrCore_Io_Init(state, &io, ZrTests_Fixture_ReaderRead, ZR_NULL, &reader);
    io.isBinary = ZR_TRUE;

    sourceObject = ZrCore_Io_ReadSourceNew(&io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    TEST_ASSERT_EQUAL_UINT32(1u, sourceObject->modulesLength);
    binaryEntry = sourceObject->modules[0].entryFunction;
    TEST_ASSERT_NOT_NULL(binaryEntry);
    TEST_ASSERT_TRUE(io_function_contains_module_effect_with_assembly(binaryEntry,
                                                                       "$mathLocal@2.1.0/ops/sum",
                                                                       "zr.math"));

    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    TEST_ASSERT_TRUE(function_contains_module_effect_with_assembly(runtimeFunction,
                                                                   "$mathLocal@2.1.0/ops/sum",
                                                                   "zr.math"));

    remove(binaryPath);
    free(binaryBytes);
    ZrCore_Function_Free(state, runtimeFunction);
    ZrCore_Function_Free(state, function);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_project_compile_canonicalizes_dependency_imports(void) {
    SZrProjectDependencyImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *rootFunction;
    SZrFunction *mathFunction;
    SZrString *rootSourceName;
    SZrString *mathSourceName;
    TZrSize rootLength = 0;
    TZrSize mathLength = 0;
    TZrChar *rootContent;
    TZrChar *mathContent;

    TEST_ASSERT_TRUE(prepare_project_dependency_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    rootContent = ZrTests_ReadTextFile(fixture.rootMainPath, &rootLength);
    TEST_ASSERT_NOT_NULL(rootContent);
    rootSourceName = ZrCore_String_CreateFromNative(state, fixture.rootMainPath);
    TEST_ASSERT_NOT_NULL(rootSourceName);
    rootFunction = ZrParser_Source_Compile(state, rootContent, rootLength, rootSourceName);
    free(rootContent);
    TEST_ASSERT_NOT_NULL(rootFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, rootFunction->staticImportLength);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(rootFunction, "$math@1.0.0/ops/sum"));

    mathContent = ZrTests_ReadTextFile(fixture.mathMainPath, &mathLength);
    TEST_ASSERT_NOT_NULL(mathContent);
    mathSourceName = ZrCore_String_CreateFromNative(state, fixture.mathMainPath);
    TEST_ASSERT_NOT_NULL(mathSourceName);
    mathFunction = ZrParser_Source_Compile(state, mathContent, mathLength, mathSourceName);
    free(mathContent);
    TEST_ASSERT_NOT_NULL(mathFunction);
    TEST_ASSERT_EQUAL_UINT32(4u, mathFunction->staticImportLength);
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(mathFunction, "$math@1.0.0/core/util"));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(mathFunction, "$math@1.0.0/feature/helper"));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(mathFunction, "$math@1.0.0/bare/local"));
    TEST_ASSERT_EQUAL_UINT32(1u,
                             (TZrUInt32)count_static_imports_named(mathFunction, "$trig@1.0.0/wave"));

    ZrCore_Function_Free(state, mathFunction);
    ZrCore_Function_Free(state, rootFunction);
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_project_derive_current_module_key_accepts_mixed_windows_separators(void) {
    SZrProjectImportFixture fixture;
    SZrGlobalState *global;
    SZrState *state;
    const SZrLibrary_Project *project;
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar currentModuleKey[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar errorBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_GetFromGlobal(global);
    TEST_ASSERT_NOT_NULL(project);

    snprintf(sourcePath, sizeof(sourcePath), "%s", fixture.mainPath);
    inject_windows_mixed_separator(sourcePath);
    memset(currentModuleKey, 0, sizeof(currentModuleKey));
    memset(errorBuffer, 0, sizeof(errorBuffer));

    TEST_ASSERT_TRUE(ZrLibrary_Project_DeriveCurrentModuleKey(project,
                                                              sourcePath,
                                                              ZR_NULL,
                                                              currentModuleKey,
                                                              sizeof(currentModuleKey),
                                                              errorBuffer,
                                                              sizeof(errorBuffer)));
    TEST_ASSERT_EQUAL_STRING("feature/app/main", currentModuleKey);

    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_project_compile_rejects_explicit_module_key_path_mismatch(void) {
    SZrProjectImportFixture fixture;
    static const TZrChar *mismatchSource =
            "%module(\"feature/other/main\");\n"
            "pub var value = 1;\n";
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
    SZrString *sourceName;

    TEST_ASSERT_TRUE(prepare_project_import_fixture(&fixture));

    global = ZrLibrary_CommonState_CommonGlobalState_New(fixture.projectPath);
    TEST_ASSERT_NOT_NULL(global);
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);
    ZrParser_ToGlobalState_Register(state);

    sourceName = ZrCore_String_CreateFromNative(state, fixture.mainPath);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, mismatchSource, strlen(mismatchSource), sourceName);
    TEST_ASSERT_NULL(function);

    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_project_compile_canonicalizes_relative_and_alias_imports);
    RUN_TEST(test_project_compile_records_using_import_guard_dependencies);
    RUN_TEST(test_project_compile_records_using_import_guard_method_signature);
    RUN_TEST(test_using_import_guard_runtime_rejects_signature_hash_mismatch);
    RUN_TEST(test_using_import_guard_runtime_rejects_target_token_mismatch);
    RUN_TEST(test_using_import_guard_runtime_rejects_target_module_hash_mismatch);
    RUN_TEST(test_required_import_runtime_rejects_signature_hash_mismatch);
    RUN_TEST(test_required_import_runtime_reports_target_token_mismatch);
    RUN_TEST(test_required_import_runtime_rejects_target_module_hash_mismatch);
    RUN_TEST(test_required_import_runtime_rejects_assembly_ref_version_mismatch);
    RUN_TEST(test_required_import_runtime_resolves_same_name_signature_candidate);
    RUN_TEST(test_using_import_guard_runtime_rejects_signature_blob_mismatch_with_matching_hash);
    RUN_TEST(test_using_import_guard_runtime_consumes_module_ref_signature_blob);
    RUN_TEST(test_using_import_guard_runtime_rejects_module_ref_target_token_mismatch);
    RUN_TEST(test_using_import_guard_runtime_rejects_module_ref_owner_chain_mismatch);
    RUN_TEST(test_using_import_guard_runtime_uses_module_ref_identity_when_effect_targets_are_missing);
    RUN_TEST(test_using_import_signature_reports_typespec_mismatch_diagnostic);
    RUN_TEST(test_using_import_guard_runtime_verifies_module_ref_table_without_entry_effects);
    RUN_TEST(test_using_import_guard_runtime_records_module_ref_binding_result);
    RUN_TEST(test_using_import_guard_runtime_unavailable_provider_falls_back_to_else);
    RUN_TEST(test_using_import_guard_runtime_checks_nested_caller_signature_hash);
    RUN_TEST(test_using_import_guard_records_native_target_signature_hash);
    RUN_TEST(test_using_import_guard_runtime_rejects_native_target_signature_hash_mismatch);
    RUN_TEST(test_required_import_runtime_accepts_native_module_link_signature);
    RUN_TEST(test_required_import_runtime_reports_native_provider_unavailable);
    RUN_TEST(test_required_import_runtime_reports_source_loader_attempts);
    RUN_TEST(test_required_import_runtime_reports_descriptor_plugin_load_error);
    RUN_TEST(test_project_import_reports_aot_descriptor_load_error);
    RUN_TEST(test_project_compile_applies_dependency_import_version_range_to_assembly_ref);
    RUN_TEST(test_project_compile_emits_assembly_ref_identity_from_zrp_references);
    RUN_TEST(test_project_compile_reads_referenced_assembly_from_zro_without_source);
    RUN_TEST(test_project_assembly_ref_identity_roundtrips_through_zro_module_effects);
    RUN_TEST(test_project_compile_canonicalizes_dependency_imports);
    RUN_TEST(test_project_derive_current_module_key_accepts_mixed_windows_separators);
    RUN_TEST(test_project_compile_rejects_explicit_module_key_path_mismatch);

    return UNITY_END();
}
