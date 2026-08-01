#include "commands/test_command.h"

#include "project/project.h"
#include "testing/test_process.h"
#include "testing/test_runner.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_testing/module.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/test_contract.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZR_CLI_TEST_WORKER_CASE_ENV "ZR_VM_TEST_CASE_ID"

typedef struct SZrCliTestTextBuilder {
    TZrChar *data;
    TZrSize length;
    TZrSize capacity;
} SZrCliTestTextBuilder;

typedef struct SZrCliTestSourceRecord {
    SZrGlobalState *discoveryGlobal;
    TZrChar sourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar *source;
    TZrSize sourceLength;
    SZrParserTestManifest manifest;
} SZrCliTestSourceRecord;

typedef struct SZrCliTestCommandContext {
    const SZrCliCommand *command;
    const TZrChar *executablePath;
    FILE *output;
    FILE *errorOutput;
    SZrCliTestSourceRecord *sources;
    TZrSize sourceCount;
    TZrSize sourceCapacity;
    SZrParserTestManifest *manifests;
    TZrBool projectTarget;
} SZrCliTestCommandContext;

typedef struct SZrCliTestExecutionRequest {
    SZrState *state;
    SZrTypeValue callable;
    SZrTypeValue returnValue;
    TZrBool returned;
} SZrCliTestExecutionRequest;

static TZrBool test_command_path_has_suffix(const TZrChar *path, const TZrChar *suffix) {
    TZrSize pathLength;
    TZrSize suffixLength;

    if (path == ZR_NULL || suffix == ZR_NULL) return ZR_FALSE;
    pathLength = strlen(path);
    suffixLength = strlen(suffix);
    if (suffixLength > pathLength) return ZR_FALSE;
    for (TZrSize index = 0U; index < suffixLength; index++) {
        if (tolower((unsigned char)path[pathLength - suffixLength + index]) !=
            tolower((unsigned char)suffix[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool test_command_register_testing(SZrGlobalState *global, TZrPtr userData) {
    ZR_UNUSED_PARAMETER(userData);
    return ZrVmLibTesting_Register(global);
}

static SZrGlobalState *test_command_create_global(
        const SZrCliTestCommandContext *context) {
    SZrGlobalState *global;

    if (context == ZR_NULL || context->command == ZR_NULL) return ZR_NULL;
    global = context->projectTarget
             ? ZrCli_Project_CreateProjectGlobal(context->command->testPath)
             : ZrCli_Project_CreateBareGlobal();
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL ||
        !ZrCli_Project_RegisterStandardModulesWithBootstrap(
                global, test_command_register_testing, ZR_NULL)) {
        if (global != ZR_NULL) ZrCore_GlobalState_Free(global);
        return ZR_NULL;
    }
    ZrLibrary_State_SetProviderPhase(
            global->mainThreadState, ZR_LIBRARY_PROVIDER_PHASE_TEST);
    return global;
}

static TZrBool test_command_builder_reserve(
        SZrCliTestTextBuilder *builder,
        TZrSize additional) {
    TZrSize required;
    TZrSize capacity;
    TZrChar *newData;

    if (builder == ZR_NULL || additional > (TZrSize)-1 - builder->length - 1U) {
        return ZR_FALSE;
    }
    required = builder->length + additional + 1U;
    if (required <= builder->capacity) return ZR_TRUE;
    capacity = builder->capacity > 0U ? builder->capacity : 256U;
    while (capacity < required) {
        if (capacity > (TZrSize)-1 / 2U) {
            capacity = required;
            break;
        }
        capacity *= 2U;
    }
    newData = (TZrChar *)realloc(builder->data, capacity);
    if (newData == ZR_NULL) return ZR_FALSE;
    builder->data = newData;
    builder->capacity = capacity;
    if (builder->length == 0U) builder->data[0] = '\0';
    return ZR_TRUE;
}

static TZrBool test_command_builder_append_bytes(
        SZrCliTestTextBuilder *builder,
        const TZrChar *text,
        TZrSize length) {
    if (builder == ZR_NULL || text == ZR_NULL ||
        !test_command_builder_reserve(builder, length)) {
        return ZR_FALSE;
    }
    memcpy(builder->data + builder->length, text, length);
    builder->length += length;
    builder->data[builder->length] = '\0';
    return ZR_TRUE;
}

static TZrBool test_command_builder_append(
        SZrCliTestTextBuilder *builder,
        const TZrChar *text) {
    return text != ZR_NULL
           ? test_command_builder_append_bytes(builder, text, strlen(text))
           : ZR_FALSE;
}

static TZrBool test_command_builder_append_format(
        SZrCliTestTextBuilder *builder,
        const TZrChar *format,
        ...) {
    va_list arguments;
    va_list measuredArguments;
    int length;

    if (builder == ZR_NULL || format == ZR_NULL) return ZR_FALSE;
    va_start(arguments, format);
    va_copy(measuredArguments, arguments);
    length = vsnprintf(ZR_NULL, 0, format, measuredArguments);
    va_end(measuredArguments);
    if (length < 0 || !test_command_builder_reserve(builder, (TZrSize)length)) {
        va_end(arguments);
        return ZR_FALSE;
    }
    vsnprintf(
            builder->data + builder->length,
            builder->capacity - builder->length,
            format,
            arguments);
    va_end(arguments);
    builder->length += (TZrSize)length;
    return ZR_TRUE;
}

static TZrBool test_command_builder_append_string_literal(
        SZrCliTestTextBuilder *builder,
        const TZrChar *value) {
    const unsigned char *cursor = (const unsigned char *)(value != ZR_NULL ? value : "");

    if (!test_command_builder_append(builder, "\"")) return ZR_FALSE;
    while (*cursor != 0U) {
        switch (*cursor) {
            case '\\':
                if (!test_command_builder_append(builder, "\\\\")) return ZR_FALSE;
                break;
            case '"':
                if (!test_command_builder_append(builder, "\\\"")) return ZR_FALSE;
                break;
            case '\n':
                if (!test_command_builder_append(builder, "\\n")) return ZR_FALSE;
                break;
            case '\r':
                if (!test_command_builder_append(builder, "\\r")) return ZR_FALSE;
                break;
            case '\t':
                if (!test_command_builder_append(builder, "\\t")) return ZR_FALSE;
                break;
            default:
                if (*cursor < 0x20U || *cursor == 0x7fU) {
                    if (!test_command_builder_append_format(builder, "\\x%02x", *cursor)) {
                        return ZR_FALSE;
                    }
                } else if (!test_command_builder_append_bytes(
                                   builder, (const TZrChar *)cursor, 1U)) {
                    return ZR_FALSE;
                }
                break;
        }
        cursor++;
    }
    return test_command_builder_append(builder, "\"");
}

static TZrBool test_command_builder_append_constant(
        SZrCliTestTextBuilder *builder,
        const SZrParserTestConstant *constant) {
    TZrChar numeric[128];

    if (builder == ZR_NULL || constant == ZR_NULL) return ZR_FALSE;
    switch (constant->kind) {
        case ZR_PARSER_TEST_CONSTANT_NULL:
            return test_command_builder_append(builder, "null");
        case ZR_PARSER_TEST_CONSTANT_BOOL:
            return test_command_builder_append(
                    builder, constant->value.boolValue ? "true" : "false");
        case ZR_PARSER_TEST_CONSTANT_INT:
            snprintf(
                    numeric,
                    sizeof(numeric),
                    "%lld",
                    (long long)constant->value.intValue);
            return test_command_builder_append(builder, numeric);
        case ZR_PARSER_TEST_CONSTANT_UINT:
            snprintf(
                    numeric,
                    sizeof(numeric),
                    "<uint>%llu",
                    (unsigned long long)constant->value.uintValue);
            return test_command_builder_append(builder, numeric);
        case ZR_PARSER_TEST_CONSTANT_FLOAT:
            snprintf(numeric, sizeof(numeric), "%.17g", constant->value.floatValue);
            if (strchr(numeric, '.') == ZR_NULL && strchr(numeric, 'e') == ZR_NULL &&
                strchr(numeric, 'E') == ZR_NULL) {
                return test_command_builder_append_format(builder, "%s.0", numeric);
            }
            return test_command_builder_append(builder, numeric);
        case ZR_PARSER_TEST_CONSTANT_STRING:
            return test_command_builder_append_string_literal(
                    builder, constant->value.stringValue);
        default:
            return ZR_FALSE;
    }
}

static TZrChar *test_command_build_case_source(
        const SZrCliTestSourceRecord *source,
        const SZrCliTestCaseReference *reference,
        TZrSize *outLength) {
    SZrCliTestTextBuilder builder = {ZR_NULL, 0U, 0U};

    if (outLength != ZR_NULL) *outLength = 0U;
    if (source == ZR_NULL || reference == ZR_NULL || reference->entry == ZR_NULL ||
        !test_command_builder_append_bytes(&builder, source->source, source->sourceLength) ||
        !test_command_builder_append(&builder, "\n") ||
        (reference->entry->isAsync &&
         !test_command_builder_append(&builder, "let __zrCliTestTask = ")) ||
        !test_command_builder_append(&builder, reference->entry->qualifiedName) ||
        !test_command_builder_append(&builder, "(")) {
        free(builder.data);
        return ZR_NULL;
    }
    if (reference->testCase != ZR_NULL) {
        for (TZrUInt32 index = 0U;
             index < reference->testCase->argumentCount;
             index++) {
            if ((index > 0U && !test_command_builder_append(&builder, ", ")) ||
                !test_command_builder_append_constant(
                        &builder, &reference->testCase->arguments[index])) {
                free(builder.data);
                return ZR_NULL;
            }
        }
    }
    if (!test_command_builder_append(
                &builder,
                reference->entry->isAsync
                        ? ");\n__zrCliTestTask.result();\n"
                        : ");\n")) {
        free(builder.data);
        return ZR_NULL;
    }
    if (outLength != ZR_NULL) *outLength = builder.length;
    return builder.data;
}

static int test_command_compare_file_entries(const void *left, const void *right) {
    const SZrLibrary_File_ListEntry *a = (const SZrLibrary_File_ListEntry *)left;
    const SZrLibrary_File_ListEntry *b = (const SZrLibrary_File_ListEntry *)right;
    return strcmp(a->path, b->path);
}

static TZrBool test_command_append_source(
        SZrCliTestCommandContext *context,
        const TZrChar *sourcePath) {
    SZrGlobalState *global = ZR_NULL;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    TZrChar *source = ZR_NULL;
    TZrSize sourceLength = 0U;
    SZrParserTestManifest manifest;
    SZrCliTestSourceRecord *newSources;
    TZrSize newCapacity;

    memset(&manifest, 0, sizeof(manifest));
    if (!ZrCli_Project_ReadTextFile(sourcePath, &source, &sourceLength)) {
        fprintf(context->errorOutput, "failed to read test module: %s\n", sourcePath);
        return ZR_FALSE;
    }
    if (sourceLength == 0U) {
        free(source);
        return ZR_TRUE;
    }
    global = test_command_create_global(context);
    if (global == ZR_NULL) {
        fprintf(context->errorOutput, "failed to create test discovery isolate: %s\n", sourcePath);
        free(source);
        return ZR_FALSE;
    }
    state = global->mainThreadState;
    sourceName = ZrCore_String_CreateFromNative(state, sourcePath);
    function = sourceName != ZR_NULL
               ? ZrParser_Source_CompileTest(state, source, sourceLength, sourceName)
               : ZR_NULL;
    if (function == ZR_NULL) {
        fprintf(context->errorOutput, "test discovery compile failed: %s\n", sourcePath);
        ZrCore_GlobalState_Free(global);
        free(source);
        return ZR_FALSE;
    }
    if (function->testManifestDataLength == 0U) {
        ZrCore_Function_Free(state, function);
        ZrCore_GlobalState_Free(global);
        free(source);
        return ZR_TRUE;
    }
    if (!ZrParser_TestManifest_Decode(
                state,
                function->testManifestData,
                function->testManifestDataLength,
                &manifest)) {
        fprintf(context->errorOutput, "invalid TestManifest: %s\n", sourcePath);
        ZrCore_Function_Free(state, function);
        ZrCore_GlobalState_Free(global);
        free(source);
        return ZR_FALSE;
    }
    ZrCore_Function_Free(state, function);

    if (context->sourceCount == context->sourceCapacity) {
        newCapacity = context->sourceCapacity > 0U ? context->sourceCapacity * 2U : 8U;
        if (newCapacity < context->sourceCapacity ||
            newCapacity > (TZrSize)-1 / sizeof(SZrCliTestSourceRecord)) {
            ZrParser_TestManifest_Free(state, &manifest);
            ZrCore_GlobalState_Free(global);
            free(source);
            return ZR_FALSE;
        }
        newSources = (SZrCliTestSourceRecord *)realloc(
                context->sources, newCapacity * sizeof(SZrCliTestSourceRecord));
        if (newSources == ZR_NULL) {
            ZrParser_TestManifest_Free(state, &manifest);
            ZrCore_GlobalState_Free(global);
            free(source);
            return ZR_FALSE;
        }
        context->sources = newSources;
        context->sourceCapacity = newCapacity;
    }
    memset(&context->sources[context->sourceCount], 0, sizeof(SZrCliTestSourceRecord));
    context->sources[context->sourceCount].discoveryGlobal = global;
    context->sources[context->sourceCount].source = source;
    context->sources[context->sourceCount].sourceLength = sourceLength;
    context->sources[context->sourceCount].manifest = manifest;
    snprintf(
            context->sources[context->sourceCount].sourcePath,
            sizeof(context->sources[context->sourceCount].sourcePath),
            "%s",
            sourcePath);
    context->sourceCount++;
    return ZR_TRUE;
}

static TZrBool test_command_discover(SZrCliTestCommandContext *context) {
    SZrGlobalState *projectGlobal = ZR_NULL;
    SZrCliProjectContext project;
    SZrLibrary_File_List files;
    EZrLibrary_File_Exist existence;

    if (context == ZR_NULL || context->command == ZR_NULL ||
        context->command->testPath == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&files, 0, sizeof(files));
    existence = ZrLibrary_File_Exist(context->command->testPath);
    context->projectTarget = test_command_path_has_suffix(
            context->command->testPath, ".zrp");
    if (context->projectTarget) {
        if (existence != ZR_LIBRARY_FILE_IS_FILE) {
            fprintf(context->errorOutput, "test project is not a readable .zrp file: %s\n",
                    context->command->testPath);
            return ZR_FALSE;
        }
        projectGlobal = ZrCli_Project_CreateProjectGlobal(context->command->testPath);
        if (projectGlobal == ZR_NULL ||
            !ZrCli_ProjectContext_FromGlobal(
                    &project, projectGlobal, context->command->testPath) ||
            !ZrLibrary_File_ListDirectory(project.sourceRoot, ZR_TRUE, &files)) {
            fprintf(context->errorOutput, "failed to enumerate test project: %s\n",
                    context->command->testPath);
            if (projectGlobal != ZR_NULL) ZrCore_GlobalState_Free(projectGlobal);
            return ZR_FALSE;
        }
        ZrCore_GlobalState_Free(projectGlobal);
        qsort(files.entries, files.count, sizeof(files.entries[0]), test_command_compare_file_entries);
        for (TZrSize index = 0U; index < files.count; index++) {
            if (files.entries[index].existence == ZR_LIBRARY_FILE_IS_FILE &&
                test_command_path_has_suffix(files.entries[index].path, ".zr") &&
                !test_command_append_source(context, files.entries[index].path)) {
                ZrLibrary_File_List_Free(&files);
                return ZR_FALSE;
            }
        }
        ZrLibrary_File_List_Free(&files);
    } else {
        if (existence != ZR_LIBRARY_FILE_IS_FILE ||
            !test_command_path_has_suffix(context->command->testPath, ".zr")) {
            fprintf(context->errorOutput, "test target must be a .zrp project or .zr module: %s\n",
                    context->command->testPath);
            return ZR_FALSE;
        }
        if (!test_command_append_source(context, context->command->testPath)) {
            return ZR_FALSE;
        }
    }
    if (context->sourceCount > 0U) {
        context->manifests = (SZrParserTestManifest *)calloc(
                context->sourceCount, sizeof(SZrParserTestManifest));
        if (context->manifests == ZR_NULL) return ZR_FALSE;
        for (TZrSize index = 0U; index < context->sourceCount; index++) {
            context->manifests[index] = context->sources[index].manifest;
        }
    }
    return ZR_TRUE;
}

static const SZrCliTestSourceRecord *test_command_find_source(
        const SZrCliTestCommandContext *context,
        const SZrParserTestEntry *entry) {
    if (context == ZR_NULL || entry == ZR_NULL) return ZR_NULL;
    for (TZrSize sourceIndex = 0U; sourceIndex < context->sourceCount; sourceIndex++) {
        const SZrParserTestManifest *manifest = &context->sources[sourceIndex].manifest;
        for (TZrUInt32 entryIndex = 0U; entryIndex < manifest->entryCount; entryIndex++) {
            if (&manifest->entries[entryIndex] == entry) {
                return &context->sources[sourceIndex];
            }
        }
    }
    return ZR_NULL;
}

static void test_command_execute_body(SZrState *state, TZrPtr userData) {
    SZrCliTestExecutionRequest *request = (SZrCliTestExecutionRequest *)userData;

    request->returned = ZrLib_CallValue(
            state,
            &request->callable,
            ZR_NULL,
            ZR_NULL,
            0U,
            &request->returnValue);
}

static TZrBool test_command_execute_in_process(
        const SZrCliTestCaseReference *reference,
        TZrUInt64 timeoutMilliseconds,
        SZrCliTestCaseResult *result,
        TZrPtr userData) {
    SZrCliTestCommandContext *context = (SZrCliTestCommandContext *)userData;
    const SZrCliTestSourceRecord *source = test_command_find_source(
            context, reference != ZR_NULL ? reference->entry : ZR_NULL);
    SZrGlobalState *global = ZR_NULL;
    SZrState *state;
    SZrString *sourceName;
    SZrFunction *function = ZR_NULL;
    TZrChar *caseSource = ZR_NULL;
    TZrSize caseSourceLength = 0U;
    TZrSize rootsBefore;
    TZrSize rootsAfter;
    EZrThreadStatus executionStatus;
    SZrCliTestExecutionRequest request;
    SZrTestingAssertionFailure assertionFailure;
    TZrBool success = ZR_FALSE;

    ZR_UNUSED_PARAMETER(timeoutMilliseconds);
    if (source == ZR_NULL || result == ZR_NULL) return ZR_FALSE;
    caseSource = test_command_build_case_source(source, reference, &caseSourceLength);
    global = test_command_create_global(context);
    if (caseSource == ZR_NULL || global == ZR_NULL) goto cleanup;
    state = global->mainThreadState;
    sourceName = ZrCore_String_CreateFromNative(state, source->sourcePath);
    function = sourceName != ZR_NULL
               ? ZrParser_Source_CompileTest(
                       state, caseSource, caseSourceLength, sourceName)
               : ZR_NULL;
    if (function == ZR_NULL) {
        snprintf(result->message, sizeof(result->message), "case compile failed");
        goto cleanup;
    }
    memset(&request, 0, sizeof(request));
    ZrCore_Value_InitAsRawObject(
            state, &request.callable, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    request.callable.type = ZR_VALUE_TYPE_FUNCTION;
    request.callable.isGarbageCollectable = ZR_TRUE;
    request.callable.isNative = ZR_FALSE;
    rootsBefore = ZrCore_GcDomain_GetRootCount(state);
    ZrVmLibTesting_ClearLastFailure();
    executionStatus = ZrCore_Exception_TryRun(
            state, test_command_execute_body, &request);
    rootsAfter = ZrCore_GcDomain_GetRootCount(state);
    if (executionStatus == ZR_THREAD_STATUS_FINE && request.returned &&
        !state->hasCurrentException && rootsAfter == rootsBefore) {
        result->status = ZR_CLI_TEST_STATUS_PASSED;
        success = ZR_TRUE;
        goto cleanup;
    }
    result->status = ZR_CLI_TEST_STATUS_FAILED;
    if (rootsAfter != rootsBefore) {
        snprintf(
                result->message,
                sizeof(result->message),
                "isolate leaked %llu external root(s)",
                (unsigned long long)(rootsAfter > rootsBefore
                                     ? rootsAfter - rootsBefore
                                     : rootsBefore - rootsAfter));
    } else if (ZrVmLibTesting_GetLastFailure(&assertionFailure)) {
        snprintf(
                result->message,
                sizeof(result->message),
                "%s",
                assertionFailure.message[0] != '\0'
                        ? assertionFailure.message
                        : "assertion failed");
    } else if (state->hasCurrentException) {
        SZrString *exceptionText = ZrCore_Value_ToDebugString(
                state, &state->currentException);
        snprintf(
                result->message,
                sizeof(result->message),
                "%s",
                exceptionText != ZR_NULL
                        ? ZrCore_String_GetNativeString(exceptionText)
                        : "unhandled test exception");
    } else {
        snprintf(result->message, sizeof(result->message), "unhandled test exception");
    }
    success = ZR_TRUE;

cleanup:
    if (function != ZR_NULL && global != ZR_NULL) {
        ZrCore_Function_Free(global->mainThreadState, function);
    }
    if (global != ZR_NULL) ZrCore_GlobalState_Free(global);
    free(caseSource);
    return success;
}

static TZrBool test_command_execute_isolated(
        const SZrCliTestCaseReference *reference,
        TZrUInt64 timeoutMilliseconds,
        SZrCliTestCaseResult *result,
        TZrPtr userData) {
    SZrCliTestCommandContext *context = (SZrCliTestCommandContext *)userData;
    SZrCliTestProcessRequest request;
    SZrCliTestProcessResult processResult;

    if (context == ZR_NULL || reference == ZR_NULL || result == ZR_NULL) return ZR_FALSE;
    memset(&request, 0, sizeof(request));
    request.executablePath = context->executablePath;
    request.targetPath = context->command->testPath;
    request.caseId = reference->id;
    request.timeoutMilliseconds = timeoutMilliseconds;
    if (!ZrCli_TestProcess_Run(&request, &processResult)) {
        snprintf(result->message, sizeof(result->message), "failed to launch test isolate");
        return ZR_FALSE;
    }
    result->durationMilliseconds = processResult.durationMilliseconds;
    snprintf(result->output, sizeof(result->output), "%s", processResult.output);
    if (processResult.timedOut) {
        result->status = ZR_CLI_TEST_STATUS_TIMED_OUT;
        snprintf(result->message, sizeof(result->message), "test isolate timed out");
        return ZR_TRUE;
    }
    if (processResult.exitCode == 0) {
        result->status = ZR_CLI_TEST_STATUS_PASSED;
        return ZR_TRUE;
    }
    if (processResult.exitCode == 1) {
        result->status = ZR_CLI_TEST_STATUS_FAILED;
        snprintf(result->message, sizeof(result->message), "test isolate reported failure");
        return ZR_TRUE;
    }
    snprintf(
            result->message,
            sizeof(result->message),
            "test isolate exited with code %d",
            processResult.exitCode);
    return ZR_FALSE;
}

static void test_command_cleanup(SZrCliTestCommandContext *context) {
    if (context == ZR_NULL) return;
    for (TZrSize index = 0U; index < context->sourceCount; index++) {
        SZrCliTestSourceRecord *source = &context->sources[index];
        if (source->discoveryGlobal != ZR_NULL &&
            source->discoveryGlobal->mainThreadState != ZR_NULL) {
            ZrParser_TestManifest_Free(
                    source->discoveryGlobal->mainThreadState, &source->manifest);
        }
        if (source->discoveryGlobal != ZR_NULL) {
            ZrCore_GlobalState_Free(source->discoveryGlobal);
        }
        free(source->source);
    }
    free(context->manifests);
    free(context->sources);
    context->manifests = ZR_NULL;
    context->sources = ZR_NULL;
    context->sourceCount = 0U;
    context->sourceCapacity = 0U;
}

static void test_command_print_result(
        const SZrCliTestCommandContext *context,
        const SZrCliTestRunResult *runResult,
        TZrBool listOnly) {
    TZrUInt64 moduleGraphHash = 0U;

    for (TZrSize index = 0U; index < context->sourceCount; index++) {
        moduleGraphHash ^= context->sources[index].manifest.moduleGraphHash;
    }
    for (TZrSize index = 0U; index < runResult->caseCount; index++) {
        const SZrCliTestCaseResult *testCase = &runResult->cases[index];
        if (listOnly) {
            fprintf(context->output, "%s\n", testCase->reference.id);
            continue;
        }
        if (testCase->output[0] != '\0') {
            fputs(testCase->output, context->output);
            if (testCase->output[strlen(testCase->output) - 1U] != '\n') {
                fputc('\n', context->output);
            }
        }
        fprintf(
                context->output,
                "[%s] %s (%llums)%s%s\n",
                ZrCli_TestRunner_StatusName(testCase->status),
                testCase->reference.id,
                (unsigned long long)testCase->durationMilliseconds,
                testCase->message[0] != '\0' ? ": " : "",
                testCase->message);
    }
    if (!listOnly) {
        fprintf(
                context->output,
                "test-result: passed=%llu failed=%llu skipped=%llu timedout=%llu crashed=%llu "
                "seed=%llu jobs=%u timeout_ms=%llu target=%s module_graph_hash=%llu\n",
                (unsigned long long)runResult->passedCount,
                (unsigned long long)runResult->failedCount,
                (unsigned long long)runResult->skippedCount,
                (unsigned long long)runResult->timedOutCount,
                (unsigned long long)runResult->crashedCount,
                (unsigned long long)runResult->seed,
                (unsigned)runResult->jobsUsed,
                (unsigned long long)context->command->testTimeoutMilliseconds,
                context->command->testPath,
                (unsigned long long)moduleGraphHash);
    }
}

int ZrCli_TestCommand_Run(
        const SZrCliCommand *command,
        const TZrChar *executablePath,
        FILE *output,
        FILE *errorOutput) {
    SZrCliTestCommandContext context;
    SZrCliTestRunnerOptions options;
    SZrCliTestRunResult runResult;
    const TZrChar *exactCaseId;
    FZrCliTestCaseExecutor executor;
    int exitCode = 2;

    if (command == ZR_NULL || command->mode != ZR_CLI_MODE_TEST ||
        command->testPath == ZR_NULL || executablePath == ZR_NULL ||
        output == ZR_NULL || errorOutput == ZR_NULL) {
        return 2;
    }
    memset(&context, 0, sizeof(context));
    context.command = command;
    context.executablePath = executablePath;
    context.output = output;
    context.errorOutput = errorOutput;
    if (!test_command_discover(&context)) goto cleanup;

    exactCaseId = getenv(ZR_CLI_TEST_WORKER_CASE_ENV);
    if (exactCaseId != ZR_NULL && exactCaseId[0] == '\0') exactCaseId = ZR_NULL;
    memset(&options, 0, sizeof(options));
    options.filterPattern = exactCaseId == ZR_NULL ? command->testFilter : ZR_NULL;
    options.exactCaseId = exactCaseId;
    options.jobs = exactCaseId != ZR_NULL ? 1U : command->testJobs;
    options.timeoutMilliseconds = exactCaseId != ZR_NULL
                                  ? 0U
                                  : command->testTimeoutMilliseconds;
    options.listOnly = command->testList;
    executor = exactCaseId != ZR_NULL
               ? test_command_execute_in_process
               : test_command_execute_isolated;
    if (!ZrCli_TestRunner_Run(
                context.manifests,
                context.sourceCount,
                &options,
                executor,
                &context,
                &runResult)) {
        fprintf(errorOutput, "test runner failed\n");
        exitCode = 3;
        goto cleanup;
    }
    if (exactCaseId != ZR_NULL && runResult.caseCount != 1U) {
        fprintf(errorOutput, "test worker case was not found: %s\n", exactCaseId);
        ZrCli_TestRunner_Free(&runResult);
        exitCode = 2;
        goto cleanup;
    }
    test_command_print_result(&context, &runResult, options.listOnly);
    exitCode = ZrCli_TestRunner_ExitCode(&runResult);
    ZrCli_TestRunner_Free(&runResult);

cleanup:
    test_command_cleanup(&context);
    return exitCode;
}
