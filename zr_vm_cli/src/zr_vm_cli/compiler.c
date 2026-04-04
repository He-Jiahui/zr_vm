#include "zr_vm_cli/compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_cli/conf.h"
#include "zr_vm_cli/project.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/file.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/writer.h"

typedef struct SZrCliModuleRecord {
    TZrChar moduleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar zroPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar zriPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar aotCSourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar aotCLibraryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar aotLlvmIrPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar aotLlvmLibraryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sourceHash[ZR_CLI_SOURCE_HASH_HEX_LENGTH];
    TZrChar zroHash[ZR_CLI_SOURCE_HASH_HEX_LENGTH];
    TZrChar aotCInputHash[ZR_CLI_SOURCE_HASH_HEX_LENGTH];
    TZrUInt32 aotCInputKind;
    TZrUInt32 aotCAbiVersion;
    TZrBool hasSourceInput;
    TZrBool hasBinaryInput;
    SZrCliStringList imports;
    TZrBool dirty;
} SZrCliModuleRecord;

typedef struct SZrCliModuleCollection {
    SZrCliModuleRecord *records;
    TZrSize count;
    TZrSize capacity;
} SZrCliModuleCollection;

typedef struct SZrCliCompileSummary {
    TZrSize compiledCount;
    TZrSize skippedCount;
    TZrSize removedCount;
} SZrCliCompileSummary;

#ifndef ZR_VM_SOURCE_ROOT
#define ZR_VM_SOURCE_ROOT ""
#endif

#ifndef ZR_VM_HOST_LIB_DIR
#define ZR_VM_HOST_LIB_DIR ""
#endif

static TZrBool zr_cli_command_exists(const TZrChar *commandName);

static void zr_cli_module_collection_init(SZrCliModuleCollection *collection) {
    if (collection == ZR_NULL) {
        return;
    }

    collection->records = ZR_NULL;
    collection->count = 0;
    collection->capacity = 0;
}

static void zr_cli_module_collection_free(SZrCliModuleCollection *collection) {
    if (collection == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < collection->count; index++) {
        ZrCli_Project_StringList_Free(&collection->records[index].imports);
    }
    free(collection->records);
    collection->records = ZR_NULL;
    collection->count = 0;
    collection->capacity = 0;
}

static SZrCliModuleRecord *zr_cli_find_module(SZrCliModuleCollection *collection, const TZrChar *moduleName) {
    if (collection == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < collection->count; index++) {
        if (strcmp(collection->records[index].moduleName, moduleName) == 0) {
            return &collection->records[index];
        }
    }

    return ZR_NULL;
}

static const SZrCliModuleRecord *zr_cli_find_module_const(const SZrCliModuleCollection *collection,
                                                          const TZrChar *moduleName) {
    if (collection == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < collection->count; index++) {
        if (strcmp(collection->records[index].moduleName, moduleName) == 0) {
            return &collection->records[index];
        }
    }

    return ZR_NULL;
}

static SZrCliModuleRecord *zr_cli_append_module(SZrCliModuleCollection *collection, const SZrCliModuleRecord *record) {
    SZrCliModuleRecord *newRecords;
    TZrSize newCapacity;

    if (collection == ZR_NULL || record == ZR_NULL) {
        return ZR_NULL;
    }

    if (collection->count == collection->capacity) {
        newCapacity = collection->capacity == 0 ? ZR_CLI_COLLECTION_INITIAL_CAPACITY
                                                : collection->capacity * ZR_CLI_COLLECTION_GROWTH_FACTOR;
        newRecords = (SZrCliModuleRecord *) realloc(collection->records, newCapacity * sizeof(*newRecords));
        if (newRecords == ZR_NULL) {
            return ZR_NULL;
        }
        collection->records = newRecords;
        collection->capacity = newCapacity;
    }

    collection->records[collection->count] = *record;
    collection->count++;
    return &collection->records[collection->count - 1];
}

static TZrBool zr_cli_manifest_append_entry(SZrCliIncrementalManifest *manifest, const SZrCliManifestEntry *entry) {
    SZrCliManifestEntry *newEntries;
    TZrSize newCapacity;

    if (manifest == ZR_NULL || entry == ZR_NULL) {
        return ZR_FALSE;
    }

    if (manifest->count == manifest->capacity) {
        newCapacity = manifest->capacity == 0 ? ZR_CLI_COLLECTION_INITIAL_CAPACITY
                                              : manifest->capacity * ZR_CLI_COLLECTION_GROWTH_FACTOR;
        newEntries = (SZrCliManifestEntry *) realloc(manifest->entries, newCapacity * sizeof(*newEntries));
        if (newEntries == ZR_NULL) {
            return ZR_FALSE;
        }
        manifest->entries = newEntries;
        manifest->capacity = newCapacity;
    }

    manifest->entries[manifest->count] = *entry;
    manifest->count++;
    return ZR_TRUE;
}

static TZrBool zr_cli_hash_file(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    FILE *file;
    TZrByte chunk[ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH];
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    TZrSize readSize;

    if (path == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    while ((readSize = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        for (TZrSize index = 0; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }

    fclose(file);
    ZrCli_Project_HashToHex(hash, buffer, bufferSize);
    return ZR_TRUE;
}

static TZrBool zr_cli_read_binary_file(const TZrChar *path, TZrByte **outBuffer, TZrSize *outLength) {
    FILE *file;
    long fileLength;
    TZrByte *buffer;
    TZrSize readLength;

    if (outBuffer != ZR_NULL) {
        *outBuffer = ZR_NULL;
    }
    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (path == ZR_NULL || outBuffer == ZR_NULL || outLength == ZR_NULL) {
        return ZR_FALSE;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_FALSE;
    }

    fileLength = ftell(file);
    if (fileLength <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_FALSE;
    }

    buffer = (TZrByte *)malloc((size_t)fileLength);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_FALSE;
    }

    readLength = (TZrSize)fread(buffer, 1, (size_t)fileLength, file);
    fclose(file);
    if (readLength != (TZrSize)fileLength) {
        free(buffer);
        return ZR_FALSE;
    }

    *outBuffer = buffer;
    *outLength = readLength;
    return ZR_TRUE;
}

static TZrBool zr_cli_load_binary_function(SZrState *state, const TZrChar *path, SZrFunction **outFunction) {
    SZrIo io;
    SZrIoSource *ioSource;

    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }
    if (state == ZR_NULL || path == ZR_NULL || outFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCli_Project_OpenFileIo(state, path, ZR_TRUE, &io)) {
        return ZR_FALSE;
    }

    ioSource = ZrCore_Io_ReadSourceNew(&io);
    if (io.close != ZR_NULL) {
        io.close(state, io.customData);
    }
    if (ioSource == ZR_NULL) {
        return ZR_FALSE;
    }

    *outFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, ioSource);
    ZrCore_Io_ReadSourceFree(state->global, ioSource);
    return *outFunction != ZR_NULL;
}

static TZrBool zr_cli_compile_aot_c_shared_library(const SZrCliModuleRecord *record) {
    TZrChar command[ZR_CLI_REPL_LINE_BUFFER_LENGTH * 8];

    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

#if defined(_WIN32)
    if (!zr_cli_command_exists("cl")) {
        fprintf(stderr, "AOT C host adapter unavailable: cl host toolchain not found\n");
        return ZR_FALSE;
    }

    if (snprintf(command,
                 sizeof(command),
                 "cl /nologo /LD /utf-8 /wd4819 /wd4103 /DZR_PLATFORM_WIN /DZR_PLATFORM_WIN_USE_MSVC /D_CRT_SECURE_NO_WARNINGS "
                 "/I\"%s\\zr_vm_common\\include\" /I\"%s\\zr_vm_core\\include\" /I\"%s\\zr_vm_library\\include\" "
                 "\"%s\" /link /OUT:\"%s\" /LIBPATH:\"%s\" zr_vm_library.lib zr_vm_core.lib",
                 ZR_VM_SOURCE_ROOT,
                 ZR_VM_SOURCE_ROOT,
                 ZR_VM_SOURCE_ROOT,
                 record->aotCSourcePath,
                 record->aotCLibraryPath,
                 ZR_VM_HOST_LIB_DIR) >= (int)sizeof(command)) {
        return ZR_FALSE;
    }
#else
    if (snprintf(command,
                 sizeof(command),
                 "cc -shared -fPIC -DZR_PLATFORM_UNIX -I\"%s/zr_vm_common/include\" -I\"%s/zr_vm_core/include\" -I\"%s/zr_vm_library/include\" "
                 "\"%s\" -L\"%s\" -Wl,-rpath,\"%s\" -lzr_vm_library -lzr_vm_core -o \"%s\"",
                 ZR_VM_SOURCE_ROOT,
                 ZR_VM_SOURCE_ROOT,
                 ZR_VM_SOURCE_ROOT,
                 record->aotCSourcePath,
                 ZR_VM_HOST_LIB_DIR,
                 ZR_VM_HOST_LIB_DIR,
                 record->aotCLibraryPath) >= (int)sizeof(command)) {
        return ZR_FALSE;
    }
#endif

    return system(command) == 0;
}

static TZrBool zr_cli_command_exists(const TZrChar *commandName) {
    TZrChar probeCommand[ZR_CLI_REPL_LINE_BUFFER_LENGTH];

    if (commandName == ZR_NULL || commandName[0] == '\0') {
        return ZR_FALSE;
    }

#if defined(_WIN32)
    if (snprintf(probeCommand, sizeof(probeCommand), "where /Q %s", commandName) >= (int)sizeof(probeCommand)) {
        return ZR_FALSE;
    }
#else
    if (snprintf(probeCommand, sizeof(probeCommand), "command -v %s >/dev/null 2>&1", commandName) >=
        (int)sizeof(probeCommand)) {
        return ZR_FALSE;
    }
#endif

    return system(probeCommand) == 0;
}

static TZrBool zr_cli_resolve_aot_llvm_toolchain(TZrChar *toolBuffer,
                                                 TZrSize toolBufferSize,
                                                 TZrBool *outUseClangCl) {
    if (toolBuffer == ZR_NULL || toolBufferSize == 0 || outUseClangCl == ZR_NULL) {
        return ZR_FALSE;
    }

    toolBuffer[0] = '\0';
    *outUseClangCl = ZR_FALSE;

#if defined(_WIN32)
    if (zr_cli_command_exists("clang-cl")) {
        snprintf(toolBuffer, toolBufferSize, "%s", "clang-cl");
        *outUseClangCl = ZR_TRUE;
        return ZR_TRUE;
    }
    if (zr_cli_command_exists("clang")) {
        snprintf(toolBuffer, toolBufferSize, "%s", "clang");
        return ZR_TRUE;
    }
    return ZR_FALSE;
#else
    if (!zr_cli_command_exists("clang")) {
        return ZR_FALSE;
    }
    snprintf(toolBuffer, toolBufferSize, "%s", "clang");
    return ZR_TRUE;
#endif
}

static TZrBool zr_cli_compile_aot_llvm_shared_library(const SZrCliModuleRecord *record) {
    TZrChar toolCommand[64];
    TZrChar command[ZR_CLI_REPL_LINE_BUFFER_LENGTH * 8];
    TZrBool useClangCl = ZR_FALSE;

    if (record == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!zr_cli_resolve_aot_llvm_toolchain(toolCommand, sizeof(toolCommand), &useClangCl)) {
        fprintf(stderr, "AOT LLVM host adapter unavailable: clang host toolchain not found\n");
        return ZR_FALSE;
    }

#if defined(_WIN32)
    if (useClangCl) {
        if (snprintf(command,
                     sizeof(command),
                     "%s /nologo /LD \"%s\" /link /OUT:\"%s\" /LIBPATH:\"%s\" "
                     "/EXPORT:ZrVm_GetAotCompiledModule zr_vm_library.lib zr_vm_core.lib",
                     toolCommand,
                     record->aotLlvmIrPath,
                     record->aotLlvmLibraryPath,
                     ZR_VM_HOST_LIB_DIR) >= (int)sizeof(command)) {
            return ZR_FALSE;
        }
    } else {
        if (snprintf(command,
                     sizeof(command),
                     "%s -Wno-override-module -shared \"%s\" -L\"%s\" -lzr_vm_library -lzr_vm_core "
                     "-Wl,/EXPORT:ZrVm_GetAotCompiledModule -o \"%s\"",
                     toolCommand,
                     record->aotLlvmIrPath,
                     ZR_VM_HOST_LIB_DIR,
                     record->aotLlvmLibraryPath) >= (int)sizeof(command)) {
            return ZR_FALSE;
        }
    }
#else
    if (snprintf(command,
                 sizeof(command),
                 "%s -Wno-override-module -shared -fPIC \"%s\" -L\"%s\" -Wl,-rpath,\"%s\" -lzr_vm_library -lzr_vm_core -o \"%s\"",
                 toolCommand,
                 record->aotLlvmIrPath,
                 ZR_VM_HOST_LIB_DIR,
                 ZR_VM_HOST_LIB_DIR,
                 record->aotLlvmLibraryPath) >= (int)sizeof(command)) {
        return ZR_FALSE;
    }
#endif

    if (system(command) != 0) {
        fprintf(stderr, "failed to compile AOT LLVM shared library: %s\n", record->moduleName);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool zr_cli_collect_imports_from_ast(SZrAstNode *node, SZrCliStringList *imports);

static TZrBool zr_cli_collect_imports_from_array(SZrAstNodeArray *nodes, SZrCliStringList *imports) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (!zr_cli_collect_imports_from_ast(nodes->nodes[index], imports)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool zr_cli_collect_imports_from_ast(SZrAstNode *node, SZrCliStringList *imports) {
    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (node == ZR_NULL || imports == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return zr_cli_collect_imports_from_array(node->data.script.statements, imports);

        case ZR_AST_STRUCT_DECLARATION:
            return zr_cli_collect_imports_from_array(node->data.structDeclaration.members, imports);

        case ZR_AST_CLASS_DECLARATION:
            return zr_cli_collect_imports_from_array(node->data.classDeclaration.members, imports);

        case ZR_AST_ENUM_DECLARATION:
            return zr_cli_collect_imports_from_array(node->data.enumDeclaration.members, imports);

        case ZR_AST_INTERFACE_DECLARATION:
            return zr_cli_collect_imports_from_array(node->data.interfaceDeclaration.members, imports);

        case ZR_AST_FUNCTION_DECLARATION:
            return zr_cli_collect_imports_from_ast(node->data.functionDeclaration.body, imports);

        case ZR_AST_TEST_DECLARATION:
            return zr_cli_collect_imports_from_ast(node->data.testDeclaration.body, imports);

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return zr_cli_collect_imports_from_ast(node->data.compileTimeDeclaration.declaration, imports);

        case ZR_AST_EXTERN_BLOCK:
            return zr_cli_collect_imports_from_array(node->data.externBlock.declarations, imports);

        case ZR_AST_STRUCT_FIELD:
            return zr_cli_collect_imports_from_ast(node->data.structField.init, imports);

        case ZR_AST_STRUCT_METHOD:
            return zr_cli_collect_imports_from_ast(node->data.structMethod.body, imports);

        case ZR_AST_STRUCT_META_FUNCTION:
            return zr_cli_collect_imports_from_ast(node->data.structMetaFunction.body, imports);

        case ZR_AST_ENUM_MEMBER:
            return zr_cli_collect_imports_from_ast(node->data.enumMember.value, imports);

        case ZR_AST_CLASS_FIELD:
            return zr_cli_collect_imports_from_ast(node->data.classField.init, imports);

        case ZR_AST_CLASS_METHOD:
            return zr_cli_collect_imports_from_ast(node->data.classMethod.body, imports);

        case ZR_AST_CLASS_PROPERTY:
            return zr_cli_collect_imports_from_ast(node->data.classProperty.modifier, imports);

        case ZR_AST_CLASS_META_FUNCTION:
            return zr_cli_collect_imports_from_array(node->data.classMetaFunction.superArgs, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.classMetaFunction.body, imports);

        case ZR_AST_PROPERTY_GET:
            return zr_cli_collect_imports_from_ast(node->data.propertyGet.body, imports);

        case ZR_AST_PROPERTY_SET:
            return zr_cli_collect_imports_from_ast(node->data.propertySet.body, imports);

        case ZR_AST_BLOCK:
            return zr_cli_collect_imports_from_array(node->data.block.body, imports);

        case ZR_AST_EXPRESSION_STATEMENT:
            return zr_cli_collect_imports_from_ast(node->data.expressionStatement.expr, imports);

        case ZR_AST_VARIABLE_DECLARATION:
            return zr_cli_collect_imports_from_ast(node->data.variableDeclaration.value, imports);

        case ZR_AST_USING_STATEMENT:
            return zr_cli_collect_imports_from_ast(node->data.usingStatement.resource, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.usingStatement.body, imports);

        case ZR_AST_RETURN_STATEMENT:
            return zr_cli_collect_imports_from_ast(node->data.returnStatement.expr, imports);

        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return zr_cli_collect_imports_from_ast(node->data.breakContinueStatement.expr, imports);

        case ZR_AST_THROW_STATEMENT:
            return zr_cli_collect_imports_from_ast(node->data.throwStatement.expr, imports);

        case ZR_AST_OUT_STATEMENT:
            return zr_cli_collect_imports_from_ast(node->data.outStatement.expr, imports);

        case ZR_AST_CATCH_CLAUSE:
            return zr_cli_collect_imports_from_ast(node->data.catchClause.block, imports);

        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return zr_cli_collect_imports_from_ast(node->data.tryCatchFinallyStatement.block, imports) &&
                   zr_cli_collect_imports_from_array(node->data.tryCatchFinallyStatement.catchClauses, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.tryCatchFinallyStatement.finallyBlock, imports);

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return zr_cli_collect_imports_from_ast(node->data.assignmentExpression.left, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.assignmentExpression.right, imports);

        case ZR_AST_BINARY_EXPRESSION:
            return zr_cli_collect_imports_from_ast(node->data.binaryExpression.left, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.binaryExpression.right, imports);

        case ZR_AST_LOGICAL_EXPRESSION:
            return zr_cli_collect_imports_from_ast(node->data.logicalExpression.left, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.logicalExpression.right, imports);

        case ZR_AST_CONDITIONAL_EXPRESSION:
            return zr_cli_collect_imports_from_ast(node->data.conditionalExpression.test, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.conditionalExpression.consequent, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.conditionalExpression.alternate, imports);

        case ZR_AST_UNARY_EXPRESSION:
            return zr_cli_collect_imports_from_ast(node->data.unaryExpression.argument, imports);

        case ZR_AST_TYPE_CAST_EXPRESSION:
            return zr_cli_collect_imports_from_ast(node->data.typeCastExpression.expression, imports);

        case ZR_AST_LAMBDA_EXPRESSION:
            return zr_cli_collect_imports_from_ast(node->data.lambdaExpression.block, imports);

        case ZR_AST_IF_EXPRESSION:
            return zr_cli_collect_imports_from_ast(node->data.ifExpression.condition, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.ifExpression.thenExpr, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.ifExpression.elseExpr, imports);

        case ZR_AST_SWITCH_EXPRESSION:
            return zr_cli_collect_imports_from_ast(node->data.switchExpression.expr, imports) &&
                   zr_cli_collect_imports_from_array(node->data.switchExpression.cases, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.switchExpression.defaultCase, imports);

        case ZR_AST_SWITCH_CASE:
            return zr_cli_collect_imports_from_ast(node->data.switchCase.value, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.switchCase.block, imports);

        case ZR_AST_SWITCH_DEFAULT:
            return zr_cli_collect_imports_from_ast(node->data.switchDefault.block, imports);

        case ZR_AST_FUNCTION_CALL:
            return zr_cli_collect_imports_from_array(node->data.functionCall.args, imports);

        case ZR_AST_MEMBER_EXPRESSION:
            return !node->data.memberExpression.computed ||
                   zr_cli_collect_imports_from_ast(node->data.memberExpression.property, imports);

        case ZR_AST_PRIMARY_EXPRESSION:
            return zr_cli_collect_imports_from_ast(node->data.primaryExpression.property, imports) &&
                   zr_cli_collect_imports_from_array(node->data.primaryExpression.members, imports);

        case ZR_AST_IMPORT_EXPRESSION:
            if (node->data.importExpression.modulePath != ZR_NULL &&
                node->data.importExpression.modulePath->type == ZR_AST_STRING_LITERAL &&
                node->data.importExpression.modulePath->data.stringLiteral.value != ZR_NULL &&
                ZrCli_Project_NormalizeModuleName(
                        ZrCore_String_GetNativeString(node->data.importExpression.modulePath->data.stringLiteral.value),
                        normalizedModule,
                        sizeof(normalizedModule))) {
                return ZrCli_Project_StringList_AppendUnique(imports, normalizedModule);
            }
            return ZR_TRUE;

        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return zr_cli_collect_imports_from_ast(node->data.prototypeReferenceExpression.target, imports);

        case ZR_AST_CONSTRUCT_EXPRESSION:
            return zr_cli_collect_imports_from_ast(node->data.constructExpression.target, imports) &&
                   zr_cli_collect_imports_from_array(node->data.constructExpression.args, imports);

        case ZR_AST_ARRAY_LITERAL:
            return zr_cli_collect_imports_from_array(node->data.arrayLiteral.elements, imports);

        case ZR_AST_OBJECT_LITERAL:
            return zr_cli_collect_imports_from_array(node->data.objectLiteral.properties, imports);

        case ZR_AST_KEY_VALUE_PAIR:
            return zr_cli_collect_imports_from_ast(node->data.keyValuePair.key, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.keyValuePair.value, imports);

        case ZR_AST_UNPACK_LITERAL:
            return zr_cli_collect_imports_from_ast(node->data.unpackLiteral.element, imports);

        case ZR_AST_GENERATOR_EXPRESSION:
            return zr_cli_collect_imports_from_ast(node->data.generatorExpression.block, imports);

        case ZR_AST_WHILE_LOOP:
            return zr_cli_collect_imports_from_ast(node->data.whileLoop.cond, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.whileLoop.block, imports);

        case ZR_AST_FOR_LOOP:
            return zr_cli_collect_imports_from_ast(node->data.forLoop.init, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.forLoop.cond, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.forLoop.step, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.forLoop.block, imports);

        case ZR_AST_FOREACH_LOOP:
            return zr_cli_collect_imports_from_ast(node->data.foreachLoop.expr, imports) &&
                   zr_cli_collect_imports_from_ast(node->data.foreachLoop.block, imports);

        case ZR_AST_INTERMEDIATE_STATEMENT:
            return zr_cli_collect_imports_from_ast(node->data.intermediateStatement.declaration, imports) &&
                   zr_cli_collect_imports_from_array(node->data.intermediateStatement.instructions, imports);

        case ZR_AST_INTERMEDIATE_DECLARATION:
            return zr_cli_collect_imports_from_array(node->data.intermediateDeclaration.constants, imports);

        case ZR_AST_INTERMEDIATE_CONSTANT:
            return zr_cli_collect_imports_from_ast(node->data.intermediateConstant.value, imports);

        default:
            return ZR_TRUE;
    }
}

static TZrBool zr_cli_collect_module_recursive(const SZrCliProjectContext *project,
                                               SZrState *state,
                                               SZrCliModuleCollection *collection,
                                               const TZrChar *moduleName,
                                               TZrBool includeBinaryModules,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize) {
    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar *source = ZR_NULL;
    TZrSize sourceLength = 0;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrCliModuleRecord record;
    TZrUInt64 sourceHash;
    SZrCliModuleRecord *recordSlot;
    TZrBool sourceExists;
    TZrBool binaryExists;

    if (project == ZR_NULL || state == ZR_NULL || collection == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrCli_Project_NormalizeModuleName(moduleName, normalizedModule, sizeof(normalizedModule))) {
        snprintf(errorBuffer, errorBufferSize, "invalid module name: %s", moduleName);
        return ZR_FALSE;
    }

    if (zr_cli_find_module(collection, normalizedModule) != ZR_NULL) {
        return ZR_TRUE;
    }

    memset(&record, 0, sizeof(record));
    ZrCli_Project_StringList_Init(&record.imports);
    snprintf(record.moduleName, sizeof(record.moduleName), "%s", normalizedModule);
    if (!ZrCli_Project_ResolveSourcePath(project, record.moduleName, record.sourcePath, sizeof(record.sourcePath))) {
        snprintf(errorBuffer, errorBufferSize, "invalid source path for module: %s", record.moduleName);
        ZrCli_Project_StringList_Free(&record.imports);
        return ZR_FALSE;
    }
    ZrCli_Project_ResolveBinaryPath(project, record.moduleName, record.zroPath, sizeof(record.zroPath));
    ZrCli_Project_ResolveIntermediatePath(project, record.moduleName, record.zriPath, sizeof(record.zriPath));
    ZrCli_Project_ResolveAotCSourcePath(project, record.moduleName, record.aotCSourcePath, sizeof(record.aotCSourcePath));
    ZrCli_Project_ResolveAotCLibraryPath(project, record.moduleName, record.aotCLibraryPath, sizeof(record.aotCLibraryPath));
    ZrCli_Project_ResolveAotLlvmIrPath(project, record.moduleName, record.aotLlvmIrPath, sizeof(record.aotLlvmIrPath));
    ZrCli_Project_ResolveAotLlvmLibraryPath(project,
                                            record.moduleName,
                                            record.aotLlvmLibraryPath,
                                            sizeof(record.aotLlvmLibraryPath));
    sourceExists = ZrLibrary_File_Exist(record.sourcePath) == ZR_LIBRARY_FILE_IS_FILE;
    binaryExists = ZrLibrary_File_Exist(record.zroPath) == ZR_LIBRARY_FILE_IS_FILE;
    if (!sourceExists && (!includeBinaryModules || !binaryExists)) {
        snprintf(errorBuffer, errorBufferSize, "missing source module: %s", record.moduleName);
        ZrCli_Project_StringList_Free(&record.imports);
        return ZR_FALSE;
    }
    record.hasSourceInput = sourceExists;
    record.hasBinaryInput = binaryExists;
    record.aotCAbiVersion = ZR_VM_AOT_ABI_VERSION;

    recordSlot = zr_cli_append_module(collection, &record);
    if (recordSlot == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize, "out of memory while collecting modules");
        ZrCli_Project_StringList_Free(&record.imports);
        return ZR_FALSE;
    }

    if (recordSlot->hasSourceInput) {
        if (!ZrCli_Project_ReadTextFile(recordSlot->sourcePath, &source, &sourceLength)) {
            snprintf(errorBuffer, errorBufferSize, "failed to read source: %s", recordSlot->sourcePath);
            return ZR_FALSE;
        }

        sourceHash = ZrCli_Project_StableHashBytes((const TZrByte *) source, sourceLength);
        ZrCli_Project_HashToHex(sourceHash, recordSlot->sourceHash, sizeof(recordSlot->sourceHash));
        snprintf(recordSlot->aotCInputHash, sizeof(recordSlot->aotCInputHash), "%s", recordSlot->sourceHash);
        recordSlot->aotCInputKind = ZR_AOT_INPUT_KIND_SOURCE;

        sourceName = ZrCore_String_CreateFromNative(state, recordSlot->sourcePath);
        ast = ZrParser_Parse(state, source, sourceLength, sourceName);
        free(source);
        if (ast == ZR_NULL) {
            snprintf(errorBuffer, errorBufferSize, "failed to parse source: %s", recordSlot->sourcePath);
            return ZR_FALSE;
        }

        if (!zr_cli_collect_imports_from_ast(ast, &recordSlot->imports)) {
            ZrParser_Ast_Free(state, ast);
            snprintf(errorBuffer, errorBufferSize, "failed to collect imports for: %s", recordSlot->moduleName);
            return ZR_FALSE;
        }

        ZrParser_Ast_Free(state, ast);

        for (TZrSize index = 0; index < recordSlot->imports.count; index++) {
            TZrChar resolvedSourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
            TZrChar resolvedBinaryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
            const TZrChar *importName = recordSlot->imports.items[index];
            TZrBool importHasSource = ZR_FALSE;
            TZrBool importHasBinary = ZR_FALSE;

            if (ZrCli_Project_ResolveSourcePath(project, importName, resolvedSourcePath, sizeof(resolvedSourcePath))) {
                importHasSource = ZrLibrary_File_Exist(resolvedSourcePath) == ZR_LIBRARY_FILE_IS_FILE;
            }
            if (includeBinaryModules &&
                ZrCli_Project_ResolveBinaryPath(project, importName, resolvedBinaryPath, sizeof(resolvedBinaryPath))) {
                importHasBinary = ZrLibrary_File_Exist(resolvedBinaryPath) == ZR_LIBRARY_FILE_IS_FILE;
            }
            if (!importHasSource && !importHasBinary) {
                continue;
            }

            if (!zr_cli_collect_module_recursive(project,
                                                 state,
                                                 collection,
                                                 importName,
                                                 includeBinaryModules,
                                                 errorBuffer,
                                                 errorBufferSize)) {
                return ZR_FALSE;
            }
        }
    } else {
        if (!zr_cli_hash_file(recordSlot->zroPath, (TZrChar *)recordSlot->zroHash, sizeof(recordSlot->zroHash))) {
            snprintf(errorBuffer, errorBufferSize, "failed to hash binary module: %s", recordSlot->zroPath);
            return ZR_FALSE;
        }
        snprintf(recordSlot->aotCInputHash, sizeof(recordSlot->aotCInputHash), "%s", recordSlot->zroHash);
        recordSlot->aotCInputKind = ZR_AOT_INPUT_KIND_BINARY;
    }

    return ZR_TRUE;
}

static TZrBool zr_cli_compile_one_module(const SZrCliProjectContext *project,
                                         SZrCliModuleRecord *record,
                                         TZrBool emitIntermediate,
                                         TZrBool emitAotC,
                                         TZrBool emitAotLlvm) {
    SZrGlobalState *global;
    SZrState *state;
    TZrChar *source = ZR_NULL;
    TZrSize sourceLength = 0;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    TZrByte *embeddedModuleBlob = ZR_NULL;
    TZrSize embeddedModuleBlobLength = 0;
    TZrBool success = ZR_FALSE;
    SZrAotWriterOptions aotOptions;

    if (project == ZR_NULL || record == ZR_NULL) {
        return ZR_FALSE;
    }

    global = ZrCli_Project_CreateProjectGlobal(project->projectPath);
    if (global == ZR_NULL) {
        fprintf(stderr, "failed to load project: %s\n", project->projectPath);
        return ZR_FALSE;
    }

    if (!ZrCli_Project_RegisterStandardModules(global)) {
        fprintf(stderr, "failed to register standard modules\n");
        ZrLibrary_CommonState_CommonGlobalState_Free(global);
        return ZR_FALSE;
    }

    state = global->mainThreadState;
    memset(&aotOptions, 0, sizeof(aotOptions));

    if (record->hasSourceInput) {
        if (!ZrCli_Project_ReadTextFile(record->sourcePath, &source, &sourceLength)) {
            fprintf(stderr, "failed to read source: %s\n", record->sourcePath);
            ZrLibrary_CommonState_CommonGlobalState_Free(global);
            return ZR_FALSE;
        }

        sourceName = ZrCore_String_CreateFromNative(state, (TZrNativeString)record->sourcePath);
        function = ZrParser_Source_Compile(state, source, sourceLength, sourceName);
        free(source);
        source = ZR_NULL;

        if (function == ZR_NULL) {
            fprintf(stderr, "failed to compile module: %s\n", record->moduleName);
            ZrLibrary_CommonState_CommonGlobalState_Free(global);
            return ZR_FALSE;
        }

        success = ZrCli_Project_EnsureParentDirectory(record->zroPath) &&
                  ZrParser_Writer_WriteBinaryFile(state, function, record->zroPath);
        if (success && emitIntermediate) {
            success = ZrCli_Project_EnsureParentDirectory(record->zriPath) &&
                      ZrParser_Writer_WriteIntermediateFile(state, function, record->zriPath);
        }
        if (success && !zr_cli_hash_file(record->zroPath, (TZrChar *)record->zroHash, sizeof(record->zroHash))) {
            success = ZR_FALSE;
        }
    } else {
        if (!zr_cli_load_binary_function(state, record->zroPath, &function)) {
            fprintf(stderr, "failed to load binary module: %s\n", record->zroPath);
            ZrLibrary_CommonState_CommonGlobalState_Free(global);
            return ZR_FALSE;
        }
        success = zr_cli_hash_file(record->zroPath, (TZrChar *)record->zroHash, sizeof(record->zroHash));
    }

    if (success && emitAotC) {
        success = zr_cli_read_binary_file(record->zroPath, &embeddedModuleBlob, &embeddedModuleBlobLength);
    }
    if (success && (emitAotC || emitAotLlvm)) {
        aotOptions.moduleName = record->moduleName;
        aotOptions.sourceHash = record->sourceHash;
        aotOptions.zroHash = record->zroHash;
        aotOptions.inputKind = record->aotCInputKind;
        aotOptions.inputHash = record->aotCInputHash;
        aotOptions.embeddedModuleBlob = embeddedModuleBlob;
        aotOptions.embeddedModuleBlobLength = embeddedModuleBlobLength;
        aotOptions.requireExecutableLowering = emitAotC;
    }
    if (success && emitAotC) {
        success = ZrCli_Project_EnsureParentDirectory(record->aotCSourcePath) &&
                  ZrParser_Writer_WriteAotCFileWithOptions(state, function, record->aotCSourcePath, &aotOptions) &&
                  ZrCli_Project_EnsureParentDirectory(record->aotCLibraryPath) &&
                  zr_cli_compile_aot_c_shared_library(record);
    }
    if (success && emitAotLlvm) {
        success = ZrCli_Project_EnsureParentDirectory(record->aotLlvmIrPath) &&
                  ZrParser_Writer_WriteAotLlvmFileWithOptions(state, function, record->aotLlvmIrPath, &aotOptions) &&
                  ZrCli_Project_EnsureParentDirectory(record->aotLlvmLibraryPath) &&
                  zr_cli_compile_aot_llvm_shared_library(record);
    }

    if (embeddedModuleBlob != ZR_NULL) {
        free(embeddedModuleBlob);
    }
    if (function != ZR_NULL) {
        ZrCore_Function_Free(state, function);
    }
    ZrLibrary_CommonState_CommonGlobalState_Free(global);
    if (!success) {
        fprintf(stderr, "failed to write outputs for module: %s\n", record->moduleName);
    }
    return success;
}

static TZrBool zr_cli_module_depends_on_dirty(const SZrCliModuleCollection *collection, const SZrCliModuleRecord *record) {
    if (collection == ZR_NULL || record == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < record->imports.count; index++) {
        const SZrCliModuleRecord *dependency = zr_cli_find_module_const(collection, record->imports.items[index]);
        if (dependency != ZR_NULL && dependency->dirty) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool zr_cli_module_has_required_outputs(const SZrCliModuleRecord *record,
                                                  TZrBool emitIntermediate,
                                                  TZrBool emitAotC,
                                                  TZrBool emitAotLlvm) {
    if (record == ZR_NULL || ZrLibrary_File_Exist((TZrNativeString)record->zroPath) != ZR_LIBRARY_FILE_IS_FILE) {
        return ZR_FALSE;
    }

    if (record->hasSourceInput &&
        emitIntermediate &&
        ZrLibrary_File_Exist((TZrNativeString)record->zriPath) != ZR_LIBRARY_FILE_IS_FILE) {
        return ZR_FALSE;
    }

    if (emitAotC &&
        (ZrLibrary_File_Exist((TZrNativeString)record->aotCSourcePath) != ZR_LIBRARY_FILE_IS_FILE ||
         ZrLibrary_File_Exist((TZrNativeString)record->aotCLibraryPath) != ZR_LIBRARY_FILE_IS_FILE)) {
        return ZR_FALSE;
    }

    if (emitAotLlvm &&
        (ZrLibrary_File_Exist((TZrNativeString)record->aotLlvmIrPath) != ZR_LIBRARY_FILE_IS_FILE ||
         ZrLibrary_File_Exist((TZrNativeString)record->aotLlvmLibraryPath) != ZR_LIBRARY_FILE_IS_FILE)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool zr_cli_manifest_entry_matches_record(const SZrCliManifestEntry *entry,
                                                    const SZrCliModuleRecord *record,
                                                    TZrBool emitAotC) {
    if (entry == ZR_NULL || record == ZR_NULL) {
        return ZR_FALSE;
    }

    if (record->hasSourceInput) {
        if (strcmp(entry->sourceHash, record->sourceHash) != 0) {
            return ZR_FALSE;
        }
    } else if (record->hasBinaryInput) {
        if (strcmp(entry->zroHash, record->zroHash) != 0) {
            return ZR_FALSE;
        }
    } else {
        return ZR_FALSE;
    }

    if (!ZrCli_Project_StringList_Equals(&entry->imports, &record->imports)) {
        return ZR_FALSE;
    }

    if (emitAotC &&
        (entry->aotCInputKind != record->aotCInputKind ||
         entry->aotCAbiVersion != record->aotCAbiVersion ||
         strcmp(entry->aotCInputHash, record->aotCInputHash) != 0)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool zr_cli_build_next_manifest(const SZrCliModuleCollection *collection,
                                          SZrCliIncrementalManifest *manifest) {
    if (collection == ZR_NULL || manifest == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCli_Project_Manifest_Init(manifest);
    for (TZrSize index = 0; index < collection->count; index++) {
        const SZrCliModuleRecord *record = &collection->records[index];
        SZrCliManifestEntry entry;

        memset(&entry, 0, sizeof(entry));
        ZrCli_Project_StringList_Init(&entry.imports);
        snprintf(entry.moduleName, sizeof(entry.moduleName), "%s", record->moduleName);
        snprintf(entry.sourceHash, sizeof(entry.sourceHash), "%s", record->sourceHash);
        snprintf(entry.zroHash, sizeof(entry.zroHash), "%s", record->zroHash);
        snprintf(entry.aotCInputHash, sizeof(entry.aotCInputHash), "%s", record->aotCInputHash);
        snprintf(entry.zroPath, sizeof(entry.zroPath), "%s", record->zroPath);
        snprintf(entry.zriPath, sizeof(entry.zriPath), "%s", record->zriPath);
        snprintf(entry.aotCSourcePath, sizeof(entry.aotCSourcePath), "%s", record->aotCSourcePath);
        snprintf(entry.aotCLibraryPath, sizeof(entry.aotCLibraryPath), "%s", record->aotCLibraryPath);
        snprintf(entry.aotLlvmIrPath, sizeof(entry.aotLlvmIrPath), "%s", record->aotLlvmIrPath);
        snprintf(entry.aotLlvmLibraryPath, sizeof(entry.aotLlvmLibraryPath), "%s", record->aotLlvmLibraryPath);
        entry.aotCInputKind = record->aotCInputKind;
        entry.aotCAbiVersion = record->aotCAbiVersion;
        if (!ZrCli_Project_StringList_Copy(&entry.imports, &record->imports) ||
            !zr_cli_manifest_append_entry(manifest, &entry)) {
            ZrCli_Project_StringList_Free(&entry.imports);
            ZrCli_Project_Manifest_Free(manifest);
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

int ZrCli_Compiler_CompileProject(const SZrCliCommand *command) {
    SZrGlobalState *scanGlobal;
    SZrCliProjectContext project;
    SZrCliModuleCollection modules;
    SZrCliIncrementalManifest previousManifest;
    SZrCliIncrementalManifest nextManifest;
    SZrCliCompileSummary summary = {0};
    TZrChar error[ZR_CLI_ERROR_BUFFER_LENGTH];
    TZrBool success = ZR_TRUE;
    TZrBool emitAotC;
    TZrBool emitAotLlvm;

    if (command == ZR_NULL || command->projectPath == ZR_NULL) {
        fprintf(stderr, "compile mode requires a project path\n");
        return 1;
    }

    emitAotC = command->emitAotC ||
               (command->runAfterCompile && command->executionMode == ZR_CLI_EXECUTION_MODE_AOT_C);
    emitAotLlvm = command->emitAotLlvm ||
                  (command->runAfterCompile && command->executionMode == ZR_CLI_EXECUTION_MODE_AOT_LLVM);

    scanGlobal = ZrCli_Project_CreateProjectGlobal(command->projectPath);
    if (scanGlobal == ZR_NULL) {
        fprintf(stderr, "failed to load project: %s\n", command->projectPath);
        return 1;
    }

    if (!ZrCli_ProjectContext_FromGlobal(&project, scanGlobal, command->projectPath)) {
        fprintf(stderr, "failed to resolve project context: %s\n", command->projectPath);
        ZrLibrary_CommonState_CommonGlobalState_Free(scanGlobal);
        return 1;
    }

    zr_cli_module_collection_init(&modules);
    ZrCli_Project_Manifest_Init(&previousManifest);
    ZrCli_Project_Manifest_Init(&nextManifest);
    error[0] = '\0';

    if (!zr_cli_collect_module_recursive(&project,
                                         scanGlobal->mainThreadState,
                                         &modules,
                                         project.entryModule,
                                         emitAotC,
                                         error,
                                         sizeof(error))) {
        fprintf(stderr, "%s\n", error[0] != '\0' ? error : "failed to collect compile graph");
        success = ZR_FALSE;
        goto cleanup;
    }

    if (command->incremental && !ZrCli_Project_LoadManifest(&project, &previousManifest)) {
        fprintf(stderr, "failed to load manifest: %s\n", project.manifestPath);
        success = ZR_FALSE;
        goto cleanup;
    }

    for (TZrSize index = 0; index < modules.count; index++) {
        SZrCliModuleRecord *record = &modules.records[index];

        if (!command->incremental) {
            record->dirty = ZR_TRUE;
            continue;
        }

        {
            const SZrCliManifestEntry *manifestEntry =
                    ZrCli_Project_FindManifestEntryConst(&previousManifest, record->moduleName);

            record->dirty = manifestEntry == ZR_NULL ||
                            !zr_cli_manifest_entry_matches_record(manifestEntry, record, emitAotC) ||
                            !zr_cli_module_has_required_outputs(record,
                                                                command->emitIntermediate,
                                                                emitAotC,
                                                                emitAotLlvm);
        }
    }

    if (command->incremental) {
        TZrBool changed;
        do {
            changed = ZR_FALSE;
            for (TZrSize index = 0; index < modules.count; index++) {
                SZrCliModuleRecord *record = &modules.records[index];
                if (!record->dirty && zr_cli_module_depends_on_dirty(&modules, record)) {
                    record->dirty = ZR_TRUE;
                    changed = ZR_TRUE;
                }
            }
        } while (changed);
    }

    for (TZrSize index = 0; index < modules.count; index++) {
        SZrCliModuleRecord *record = &modules.records[index];

        if (!record->dirty) {
            summary.skippedCount++;
            continue;
        }

        if (!zr_cli_compile_one_module(&project, record, command->emitIntermediate, emitAotC, emitAotLlvm)) {
            success = ZR_FALSE;
            goto cleanup;
        }

        summary.compiledCount++;
    }

    if (command->incremental) {
        for (TZrSize index = 0; index < previousManifest.count; index++) {
            const SZrCliManifestEntry *entry = &previousManifest.entries[index];
            if (zr_cli_find_module_const(&modules, entry->moduleName) != ZR_NULL) {
                continue;
            }

            ZrCli_Project_RemoveFileIfExists(entry->zroPath);
            ZrCli_Project_RemoveFileIfExists(entry->zriPath);
            ZrCli_Project_RemoveFileIfExists(entry->aotCSourcePath);
            ZrCli_Project_RemoveFileIfExists(entry->aotCLibraryPath);
            ZrCli_Project_RemoveFileIfExists(entry->aotLlvmIrPath);
            ZrCli_Project_RemoveFileIfExists(entry->aotLlvmLibraryPath);
            summary.removedCount++;
        }

        if (!zr_cli_build_next_manifest(&modules, &nextManifest) || !ZrCli_Project_SaveManifest(&project, &nextManifest)) {
            fprintf(stderr, "failed to update manifest: %s\n", project.manifestPath);
            success = ZR_FALSE;
            goto cleanup;
        }
    }

cleanup:
    if (success) {
        fprintf(stdout,
                "compile summary: compiled=%llu skipped=%llu removed=%llu\n",
                (unsigned long long) summary.compiledCount,
                (unsigned long long) summary.skippedCount,
                (unsigned long long) summary.removedCount);
    }

    zr_cli_module_collection_free(&modules);
    ZrCli_Project_Manifest_Free(&previousManifest);
    ZrCli_Project_Manifest_Free(&nextManifest);
    ZrLibrary_CommonState_CommonGlobalState_Free(scanGlobal);
    return success ? 0 : 1;
}
