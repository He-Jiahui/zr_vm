#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"
#include "zr_vm_parser/writer.h"
#include "test_support.h"

typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrExecBcAotTestTimer;

typedef struct {
    TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} SZrBinaryFixtureReader;

typedef struct {
    TZrUInt32 eventCount;
    TZrUInt32 lastLine;
    TZrUInt32 lines[8];
} SZrAotDebugHookCapture;

typedef struct {
    const char *opcodeName;
    const char *runtimeHelperName;
} SZrAotSourceSyncExpectation;

typedef struct {
    EZrAotBackendKind backendKind;
    TZrChar *moduleName;
    TZrChar *sourcePath;
    TZrChar *zroPath;
    TZrChar *libraryPath;
    void *libraryHandle;
    const ZrAotCompiledModule *descriptor;
    SZrFunction *moduleFunction;
    SZrFunction **functionTable;
    TZrUInt32 functionCount;
    TZrUInt32 functionCapacity;
    TZrUInt32 *generatedFrameSlotCounts;
    SZrObjectModule *module;
    TZrBool moduleExecuted;
} SZrExecBcAotTestLoadedModule;

typedef struct {
    EZrLibraryProjectExecutionMode configuredExecutionMode;
    EZrLibraryExecutedVia executedVia;
    TZrBool requireAotPath;
    TZrBool strictProjectAot;
    TZrChar lastError[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrExecBcAotTestLoadedModule *records;
    TZrSize recordCount;
    TZrSize recordCapacity;
    SZrExecBcAotTestLoadedModule *activeRecord;
} SZrExecBcAotTestRuntimeState;

static SZrAotDebugHookCapture g_aotDebugHookCapture;

#define ZR_EXECBC_TEST_OPCODE_NAME_CASE(INSTRUCTION)                                                                  \
    case ZR_INSTRUCTION_ENUM(INSTRUCTION):                                                                            \
        return #INSTRUCTION;

static const char *execbc_test_instruction_opcode_name(EZrInstructionCode opcode) {
    switch (opcode) {
        ZR_INSTRUCTION_DECLARE(ZR_EXECBC_TEST_OPCODE_NAME_CASE)
        default:
            return "UNKNOWN_OPCODE";
    }
}

static TZrBool execbc_test_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        enabled = getenv("ZR_VM_TRACE_EXECBC_AOT_TEST") != ZR_NULL ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void execbc_test_dump_function_tree(const SZrFunction *function, TZrUInt32 depth) {
    const char *functionName;
    TZrUInt32 index;

    if (!execbc_test_trace_enabled() || function == ZR_NULL) {
        return;
    }

    functionName = function->functionName != ZR_NULL ? ZrCore_String_GetNativeString(function->functionName) : ZR_NULL;
    fprintf(stderr,
            "[execbc-trace] depth=%u function=%s instructions=%u semir=%u children=%u\n",
            (unsigned int)depth,
            functionName != ZR_NULL ? functionName : "<anonymous>",
            (unsigned int)function->instructionsLength,
            (unsigned int)function->semIrInstructionLength,
            (unsigned int)function->childFunctionLength);

    for (index = 0; index < function->instructionsLength; index++) {
        EZrInstructionCode opcode = (EZrInstructionCode)function->instructionsList[index].instruction.operationCode;

        fprintf(stderr,
                "[execbc-trace]   ins[%u]=%s (%u)\n",
                (unsigned int)index,
                execbc_test_instruction_opcode_name(opcode),
                (unsigned int)opcode);
    }

    for (index = 0; index < function->semIrInstructionLength; index++) {
        const SZrSemIrInstruction *instruction = &function->semIrInstructions[index];

        fprintf(stderr,
                "[execbc-trace]   semir[%u]=%u deopt=%u\n",
                (unsigned int)index,
                (unsigned int)instruction->opcode,
                (unsigned int)instruction->deoptId);
    }

    for (index = 0; index < function->childFunctionLength; index++) {
        execbc_test_dump_function_tree(&function->childFunctionList[index], depth + 1u);
    }
}

static TZrInt64 aot_runtime_test_dummy_entry_thunk(struct SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    return 1;
}

static void aot_debug_hook_capture_reset(void) {
    memset(&g_aotDebugHookCapture, 0, sizeof(g_aotDebugHookCapture));
    g_aotDebugHookCapture.lastLine = ZR_RUNTIME_DEBUG_HOOK_LINE_NONE;
}

static void aot_debug_hook_capture(struct SZrState *state, SZrDebugInfo *debugInfo) {
    ZR_UNUSED_PARAMETER(state);

    if (debugInfo == ZR_NULL || debugInfo->event != ZR_DEBUG_HOOK_EVENT_LINE) {
        return;
    }

    if (g_aotDebugHookCapture.eventCount < (TZrUInt32)(sizeof(g_aotDebugHookCapture.lines) /
                                                       sizeof(g_aotDebugHookCapture.lines[0]))) {
        g_aotDebugHookCapture.lines[g_aotDebugHookCapture.eventCount] = (TZrUInt32)debugInfo->currentLine;
    }
    g_aotDebugHookCapture.lastLine = (TZrUInt32)debugInfo->currentLine;
    g_aotDebugHookCapture.eventCount++;
}

static TZrBool semir_contains_opcode_with_deopt(const SZrFunction *function,
                                                EZrSemIrOpcode opcode,
                                                TZrBool requireDeopt);
static TZrUInt32 function_count_callsite_cache_kind(const SZrFunction *function,
                                                    EZrFunctionCallSiteCacheKind kind);
static TZrUInt32 function_find_debug_line_for_instruction(const SZrFunction *function, TZrUInt32 instructionIndex);
static TZrBool execbc_aot_pipeline_string_equal(const SZrString *string1, const SZrString *string2);
static void assert_runtime_function_matches_source_function(const SZrFunction *expected,
                                                            const SZrFunction *actual);
static void assert_optional_string_equal(const SZrString *expected, const SZrString *actual);
static void assert_typed_type_ref_equal(const SZrFunctionTypedTypeRef *expected, const SZrFunctionTypedTypeRef *actual);
static SZrFunction *compile_cached_meta_and_dynamic_callsite_fixture(SZrState *state);
static SZrFunction *compile_fixed_array_helper_roundtrip_fixture(SZrState *state);
static SZrFunction *compile_container_matrix_roundtrip_fixture(SZrState *state);
static SZrFunction *compile_array_int_index_quickening_fixture(SZrState *state);
static SZrFunction *compile_array_int_add_burst_fixture(SZrState *state);
static SZrFunction *compile_array_int_fill_loop_fixture(SZrState *state);
static SZrFunction *compile_map_array_roundtrip_fixture(SZrState *state);
static SZrFunction *compile_linked_pair_roundtrip_fixture(SZrState *state);
static SZrFunction *compile_typed_destructuring_member_slot_fixture(SZrState *state);
static SZrFunction *compile_set_pair_roundtrip_fixture(SZrState *state);
static SZrFunction *compile_set_to_map_roundtrip_fixture(SZrState *state);
static SZrFunction *compile_meta_access_fixture(SZrState *state);
static SZrFunction *compile_member_slot_quickening_fixture(SZrState *state);
static SZrFunction *compile_typed_member_known_call_fixture(SZrState *state);
static SZrFunction *compile_dynamic_object_member_fallback_fixture(SZrState *state);
static SZrFunction *compile_super_member_dispatch_fixture(SZrState *state);
static SZrFunction *compile_zero_arg_tail_quickening_fixture(SZrState *state);
static SZrFunction *compile_exception_control_fixture(SZrState *state);

static char *read_text_file_owned(const TZrChar *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

static void assert_generated_aot_c_text_uses_new_code_shape(const char *text) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NULL(strstr(text, "if (zr_aot_direct_call.prepared) {"));
    TEST_ASSERT_NULL(strstr(text, "goto zr_aot_fail"));
    TEST_ASSERT_NULL(strstr(text, "return 0;"));
    TEST_ASSERT_NULL(strstr(text, "ZrLibrary_AotRuntime_InvokeActiveShim"));
    TEST_ASSERT_NOT_NULL(strstr(text, "ZrLibrary_AotRuntime_FailGeneratedFunction"));
}

static void assert_generated_aot_llvm_text_uses_true_backend_shape(const char *text) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NULL(strstr(text, "ZrLibrary_AotRuntime_InvokeActiveShim"));
    TEST_ASSERT_NOT_NULL(strstr(text, "define internal i64 @zr_aot_fn_0(ptr %state)"));
}

static void assert_generated_aot_llvm_text_uses_aligned_stack_value_layout(const char *text) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(strstr(text, "%SZrTypeValue = type { ["));
    TEST_ASSERT_NOT_NULL(strstr(text, "%SZrTypeValueOnStack = type { ["));
    TEST_ASSERT_NULL(strstr(text, "%SZrTypeValue = type { i32, i64, i8, i8, i32, ptr, ptr }"));
    TEST_ASSERT_NULL(strstr(text, "%SZrTypeValueOnStack = type { %SZrTypeValue, i32 }"));
    TEST_ASSERT_NULL(strstr(text, "getelementptr %SZrTypeValue, ptr"));
    TEST_ASSERT_NULL(strstr(text, "getelementptr %SZrTypeValueOnStack, ptr"));
    TEST_ASSERT_NOT_NULL(strstr(text, "getelementptr i8, ptr"));
}

static void assert_generated_aot_llvm_text_lowers_simple_return_path(const char *text) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(strstr(text, "@ZrLibrary_AotRuntime_BeginGeneratedFunction"));
    TEST_ASSERT_NOT_NULL(strstr(text, "@ZrLibrary_AotRuntime_BeginInstruction"));
    TEST_ASSERT_NOT_NULL(strstr(text, "@ZrLibrary_AotRuntime_CopyConstant"));
    TEST_ASSERT_NOT_NULL(strstr(text, "@ZrLibrary_AotRuntime_Return"));
}

static void assert_generated_aot_llvm_text_lowers_static_direct_call_path(const char *text) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(strstr(text, "@ZrLibrary_AotRuntime_PrepareStaticDirectCall"));
    TEST_ASSERT_NOT_NULL(strstr(text, "call i64 @zr_aot_fn_1(ptr %state)"));
    TEST_ASSERT_NOT_NULL(strstr(text, "@ZrLibrary_AotRuntime_FinishDirectCall"));
}

static TZrBool aot_llvm_text_contains_unsupported_opcode(const char *text, TZrUInt32 opcode) {
    const char *unsupportedPrefix = "call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state";
    const char *cursor = text;
    char opcodeSuffix[32];

    if (text == ZR_NULL) {
        return ZR_FALSE;
    }

    snprintf(opcodeSuffix, sizeof(opcodeSuffix), ", i32 %u)", (unsigned)opcode);
    while ((cursor = strstr(cursor, unsupportedPrefix)) != ZR_NULL) {
        const char *lineEnd = strchr(cursor, '\n');
        const char *suffix = strstr(cursor, opcodeSuffix);
        if (suffix != ZR_NULL && (lineEnd == ZR_NULL || suffix < lineEnd)) {
            return ZR_TRUE;
        }
        cursor += strlen(unsupportedPrefix);
    }

    return ZR_FALSE;
}

static TZrBool aot_c_text_contains_unsupported_opcode(const char *text, TZrUInt32 opcode) {
    const char *unsupportedPrefix = "return ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state,";
    const char *cursor = text;
    char opcodeSuffix[32];

    if (text == ZR_NULL) {
        return ZR_FALSE;
    }

    snprintf(opcodeSuffix, sizeof(opcodeSuffix), ", %u);", (unsigned)opcode);
    while ((cursor = strstr(cursor, unsupportedPrefix)) != ZR_NULL) {
        const char *lineEnd = strchr(cursor, '\n');
        const char *suffix = strstr(cursor, opcodeSuffix);
        if (suffix != ZR_NULL && (lineEnd == ZR_NULL || suffix < lineEnd)) {
            return ZR_TRUE;
        }
        cursor += strlen(unsupportedPrefix);
    }

    return ZR_FALSE;
}

static void assert_generated_aot_c_begin_instruction_step_flags(const char *text,
                                                                TZrUInt32 instructionIndex,
                                                                const char *stepFlagsText) {
    char expected[192];

    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(stepFlagsText);

    snprintf(expected,
             sizeof(expected),
             "ZrLibrary_AotRuntime_BeginInstruction(state, &frame, %u, %s)",
             (unsigned)instructionIndex,
             stepFlagsText);
    TEST_ASSERT_NOT_NULL(strstr(text, expected));
}

static void assert_generated_aot_llvm_begin_instruction_step_flags(const char *text,
                                                                   TZrUInt32 instructionIndex,
                                                                   TZrUInt32 stepFlags) {
    char expected[192];

    TEST_ASSERT_NOT_NULL(text);

    snprintf(expected,
             sizeof(expected),
             "@ZrLibrary_AotRuntime_BeginInstruction(ptr %%state, ptr %%frame, i32 %u, i32 %u)",
             (unsigned)instructionIndex,
             (unsigned)stepFlags);
    TEST_ASSERT_NOT_NULL(strstr(text, expected));
}

static TZrInstruction make_instruction_no_operands(EZrInstructionCode opcode, TZrUInt16 operandExtra) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    return instruction;
}

static TZrInstruction make_instruction_constant_operand(EZrInstructionCode opcode,
                                                        TZrUInt16 operandExtra,
                                                        TZrInt32 operandA2) {
    TZrInstruction instruction = make_instruction_no_operands(opcode, operandExtra);

    instruction.instruction.operand.operand2[0] = operandA2;
    return instruction;
}

static TZrInstruction make_instruction_slot_operands(EZrInstructionCode opcode,
                                                     TZrUInt16 operandExtra,
                                                     TZrUInt16 operandA1,
                                                     TZrUInt16 operandB1) {
    TZrInstruction instruction = make_instruction_no_operands(opcode, operandExtra);

    instruction.instruction.operand.operand1[0] = operandA1;
    instruction.instruction.operand.operand1[1] = operandB1;
    return instruction;
}

static TZrInstruction make_instruction_return(TZrUInt16 sourceSlot) {
    return make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, sourceSlot, 0);
}

static SZrFunction *compile_source_without_quickening(SZrState *state,
                                                      const char *source,
                                                      const char *sourcePath) {
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFunction *func;

    if (state == ZR_NULL || source == ZR_NULL || sourcePath == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourcePath, strlen(sourcePath));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_CompilerState_Init(&cs, state);
    cs.currentAst = ast;

    if (!compiler_validate_task_effects(&cs, ast)) {
        ZrParser_Ast_Free(state, ast);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    compile_script(&cs, ast);
    if (cs.hasError) {
        if (cs.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(state, cs.currentFunction);
        }
        if (cs.topLevelFunction != ZR_NULL) {
            ZrCore_Function_Free(state, cs.topLevelFunction);
        }
        ZrParser_Ast_Free(state, ast);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    func = cs.topLevelFunction != ZR_NULL ? cs.topLevelFunction : cs.currentFunction;
    optimize_instructions(&cs);
    if (!compiler_assemble_final_function(&cs,
                                          func,
                                          ast,
                                          func == cs.currentFunction,
                                          cs.topLevelFunction != ZR_NULL)) {
        ZrCore_Function_Free(state, func);
        ZrParser_Ast_Free(state, ast);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    ZrParser_Ast_Free(state, ast);
    ZrParser_CompilerState_Free(&cs);
    return func;
}

static void *aot_runtime_test_project_alloc(SZrState *state, TZrSize size) {
    if (state == ZR_NULL || state->global == ZR_NULL || state->global->allocator == ZR_NULL) {
        return ZR_NULL;
    }

    return state->global->allocator(state->global->userAllocationArguments,
                                    ZR_NULL,
                                    0,
                                    size,
                                    ZR_MEMORY_NATIVE_TYPE_PROJECT);
}

static SZrExecBcAotTestRuntimeState *aot_runtime_test_install_project_record(SZrState *state,
                                                                              SZrLibrary_Project *project,
                                                                              SZrFunction *function,
                                                                              EZrAotBackendKind backendKind) {
    SZrExecBcAotTestRuntimeState *runtimeState;
    SZrExecBcAotTestLoadedModule *records;
    SZrFunction **functionTable;
    TZrUInt32 *generatedFrameSlotCounts;
    TZrUInt32 functionCount;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_NOT_NULL(function);

    memset(project, 0, sizeof(*project));
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          backendKind == ZR_AOT_BACKEND_KIND_LLVM
                                                                  ? ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM
                                                                  : ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
                                                          ZR_FALSE));

    runtimeState = (SZrExecBcAotTestRuntimeState *)project->aotRuntime;
    TEST_ASSERT_NOT_NULL(runtimeState);

    records = (SZrExecBcAotTestLoadedModule *)aot_runtime_test_project_alloc(state, sizeof(*records));
    TEST_ASSERT_NOT_NULL(records);
    memset(records, 0, sizeof(*records));

    functionCount = 1u + function->childFunctionLength;
    TEST_ASSERT_TRUE(functionCount > 0u);
    if (function->childFunctionLength > 0u) {
        TEST_ASSERT_NOT_NULL(function->childFunctionList);
    }

    functionTable = (SZrFunction **)aot_runtime_test_project_alloc(state, sizeof(*functionTable) * functionCount);
    TEST_ASSERT_NOT_NULL(functionTable);
    generatedFrameSlotCounts =
            (TZrUInt32 *)aot_runtime_test_project_alloc(state, sizeof(*generatedFrameSlotCounts) * functionCount);
    TEST_ASSERT_NOT_NULL(generatedFrameSlotCounts);
    functionTable[0] = function;
    generatedFrameSlotCounts[0] = ZrCore_Function_GetGeneratedFrameSlotCount(function);
    for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
        functionTable[index + 1u] = &function->childFunctionList[index];
        generatedFrameSlotCounts[index + 1u] =
                ZrCore_Function_GetGeneratedFrameSlotCount(&function->childFunctionList[index]);
    }

    records[0].backendKind = backendKind;
    records[0].moduleFunction = function;
    records[0].functionTable = functionTable;
    records[0].functionCount = functionCount;
    records[0].functionCapacity = functionCount;
    records[0].generatedFrameSlotCounts = generatedFrameSlotCounts;

    runtimeState->records = records;
    runtimeState->recordCount = 1;
    runtimeState->recordCapacity = 1;
    runtimeState->activeRecord = ZR_NULL;
    return runtimeState;
}

static void aot_runtime_test_remove_project_record(SZrState *state, SZrLibrary_Project *project) {
    if (state == ZR_NULL || state->global == ZR_NULL || project == ZR_NULL) {
        return;
    }

    if (project->aotRuntime != ZR_NULL) {
        ZrLibrary_AotRuntime_FreeProjectState(state, project);
    }
    state->global->userData = ZR_NULL;
}

static void init_manual_test_function(SZrFunction *function,
                                      TZrInstruction *instructions,
                                      TZrUInt32 instructionCount,
                                      SZrTypeValue *constants,
                                      TZrUInt32 constantCount,
                                      SZrFunction *childFunctions,
                                      TZrUInt32 childFunctionCount,
                                      TZrUInt32 stackSize) {
    TEST_ASSERT_NOT_NULL(function);

    memset(function, 0, sizeof(*function));
    function->instructionsList = instructions;
    function->instructionsLength = instructionCount;
    function->constantValueList = constants;
    function->constantValueLength = constantCount;
    function->childFunctionList = childFunctions;
    function->childFunctionLength = childFunctionCount;
    function->stackSize = stackSize;
    function->lineInSourceStart = 1;
    function->lineInSourceEnd = 1;
}

static void assert_generated_aot_texts_do_not_report_unsupported_opcodes(const char *cText,
                                                                         const char *llvmText,
                                                                         const EZrInstructionCode *opcodes,
                                                                         TZrSize opcodeCount) {
    TZrSize index;

    TEST_ASSERT_NOT_NULL(cText);
    TEST_ASSERT_NOT_NULL(llvmText);
    TEST_ASSERT_NOT_NULL(opcodes);

    for (index = 0; index < opcodeCount; index++) {
        TEST_ASSERT_FALSE(aot_c_text_contains_unsupported_opcode(cText, (TZrUInt32)opcodes[index]));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, (TZrUInt32)opcodes[index]));
    }
}

static char *read_reference_file(const char *relativePath, size_t *outSize) {
    return ZrTests_Reference_ReadFixture(relativePath, outSize);
}

static char *read_repo_file_owned(const char *repoRelativePath) {
    char pathBuffer[1024];

    if (repoRelativePath == ZR_NULL) {
        return ZR_NULL;
    }

    snprintf(pathBuffer, sizeof(pathBuffer), "%s/../%s", ZR_VM_TESTS_SOURCE_DIR, repoRelativePath);
    return read_text_file_owned(pathBuffer);
}

static TZrUInt32 count_substring_occurrences(const char *text, const char *needle) {
    TZrUInt32 count = 0;
    const char *cursor;
    size_t needleLength;

    if (text == ZR_NULL || needle == ZR_NULL || needle[0] == '\0') {
        return 0;
    }

    needleLength = strlen(needle);
    cursor = text;
    while ((cursor = strstr(cursor, needle)) != ZR_NULL) {
        count++;
        cursor += needleLength;
    }

    return count;
}

static char *join_repo_files_owned(const char *const *repoRelativePaths, TZrSize pathCount) {
    TZrSize index;
    size_t totalLength = 1;
    char *joinedText;
    char *writeCursor;

    if (repoRelativePaths == ZR_NULL || pathCount == 0) {
        return ZR_NULL;
    }

    for (index = 0; index < pathCount; index++) {
        char *fileText = read_repo_file_owned(repoRelativePaths[index]);

        if (fileText == ZR_NULL) {
            free(fileText);
            return ZR_NULL;
        }
        totalLength += strlen(fileText) + 1;
        free(fileText);
    }

    joinedText = (char *)malloc(totalLength);
    if (joinedText == ZR_NULL) {
        return ZR_NULL;
    }

    writeCursor = joinedText;
    for (index = 0; index < pathCount; index++) {
        char *fileText = read_repo_file_owned(repoRelativePaths[index]);
        size_t fileLength;

        if (fileText == ZR_NULL) {
            free(joinedText);
            return ZR_NULL;
        }

        fileLength = strlen(fileText);
        memcpy(writeCursor, fileText, fileLength);
        writeCursor += fileLength;
        *writeCursor++ = '\n';
        free(fileText);
    }

    *writeCursor = '\0';
    return joinedText;
}

static TZrByte *read_binary_file_owned(const TZrChar *path, TZrSize *outLength) {
    FILE *file;
    long fileSize;
    TZrByte *buffer;

    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (TZrByte *)malloc((size_t)fileSize);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    fclose(file);
    if (outLength != ZR_NULL) {
        *outLength = (TZrSize)fileSize;
    }
    return buffer;
}

static TZrByte *read_repo_binary_file_owned(const char *repoRelativePath, TZrSize *outLength) {
    char pathBuffer[1024];

    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (repoRelativePath == ZR_NULL) {
        return ZR_NULL;
    }

    snprintf(pathBuffer, sizeof(pathBuffer), "%s/../%s", ZR_VM_TESTS_SOURCE_DIR, repoRelativePath);
    return read_binary_file_owned(pathBuffer, outLength);
}

static TZrBool write_embedded_aot_c_file(SZrState *state,
                                         SZrFunction *function,
                                         const TZrChar *moduleName,
                                         const TZrChar *filename,
                                         const TZrChar *binaryPath) {
    SZrAotWriterOptions options;
    TZrByte *embeddedModuleBlob = ZR_NULL;
    TZrSize embeddedModuleBlobLength = 0;
    TZrBool success;

    if (state == ZR_NULL || function == ZR_NULL || moduleName == ZR_NULL || filename == ZR_NULL || binaryPath == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrParser_Writer_WriteBinaryFile(state, function, binaryPath)) {
        return ZR_FALSE;
    }

    embeddedModuleBlob = read_binary_file_owned(binaryPath, &embeddedModuleBlobLength);
    remove(binaryPath);
    if (embeddedModuleBlob == ZR_NULL || embeddedModuleBlobLength == 0) {
        free(embeddedModuleBlob);
        return ZR_FALSE;
    }

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = moduleName;
    options.embeddedModuleBlob = embeddedModuleBlob;
    options.embeddedModuleBlobLength = embeddedModuleBlobLength;
    options.requireExecutableLowering = ZR_TRUE;

    success = ZrParser_Writer_WriteAotCFileWithOptions(state, function, filename, &options);
    free(embeddedModuleBlob);
    return success;
}

static TZrBool write_standalone_strict_aot_c_file(SZrState *state,
                                                  SZrFunction *function,
                                                  const TZrChar *moduleName,
                                                  const TZrChar *filename) {
    SZrAotWriterOptions options;

    if (state == ZR_NULL || function == ZR_NULL || moduleName == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = moduleName;
    options.requireExecutableLowering = ZR_TRUE;
    return ZrParser_Writer_WriteAotCFileWithOptions(state, function, filename, &options);
}

static TZrBool write_standalone_strict_aot_llvm_file(SZrState *state,
                                                     SZrFunction *function,
                                                     const TZrChar *moduleName,
                                                     const TZrChar *filename) {
    SZrAotWriterOptions options;

    if (state == ZR_NULL || function == ZR_NULL || moduleName == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&options, 0, sizeof(options));
    options.moduleName = moduleName;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = moduleName;
    options.requireExecutableLowering = ZR_TRUE;
    return ZrParser_Writer_WriteAotLlvmFileWithOptions(state, function, filename, &options);
}

static TZrBytePtr binary_fixture_reader_read(struct SZrState *state, TZrPtr customData, ZR_OUT TZrSize *size) {
    SZrBinaryFixtureReader *reader = (SZrBinaryFixtureReader *)customData;

    ZR_UNUSED_PARAMETER(state);

    if (reader == ZR_NULL || size == ZR_NULL || reader->consumed || reader->bytes == ZR_NULL) {
        return ZR_NULL;
    }

    reader->consumed = ZR_TRUE;
    *size = reader->length;
    return reader->bytes;
}

static void binary_fixture_reader_close(struct SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

static TZrBool function_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool function_contains_super_array_get_int_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST));
}

static TZrBool function_contains_add_int_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_INT)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_INT_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST));
}

static TZrBool function_contains_add_signed_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST));
}

static TZrBool function_contains_add_unsigned_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST));
}

static TZrBool function_contains_sub_int_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_INT)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_INT_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST));
}

static TZrBool function_contains_sub_signed_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST));
}

static TZrBool function_contains_sub_unsigned_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST));
}

static TZrBool function_contains_mul_signed_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST));
}

static TZrBool function_contains_div_signed_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DIV_SIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST));
}

static TZrBool function_contains_mod_signed_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MOD_SIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST));
}

static TZrBool function_contains_get_member_name(const SZrFunction *function, const TZrChar *memberName) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL || memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        TZrUInt16 memberEntryIndex;
        SZrString *symbol;

        if ((EZrInstructionCode)instruction->instruction.operationCode != ZR_INSTRUCTION_ENUM(GET_MEMBER)) {
            continue;
        }

        memberEntryIndex = instruction->instruction.operand.operand1[1];
        if (memberEntryIndex >= function->memberEntryLength) {
            continue;
        }

        symbol = function->memberEntries[memberEntryIndex].symbol;
        if (symbol != ZR_NULL && strcmp(ZrCore_String_GetNativeString(symbol), memberName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrUInt32 function_find_debug_line_for_instruction(const SZrFunction *function, TZrUInt32 instructionIndex) {
    TZrUInt32 bestLine = 0;
    TZrUInt32 index;

    if (function == ZR_NULL || function->executionLocationInfoList == ZR_NULL || function->executionLocationInfoLength == 0) {
        return 0;
    }

    for (index = 0; index < function->executionLocationInfoLength; index++) {
        const SZrFunctionExecutionLocationInfo *info = &function->executionLocationInfoList[index];
        if (info->currentInstructionOffset > instructionIndex) {
            break;
        }
        bestLine = info->lineInSource;
    }

    if (bestLine == 0 && function->lineInSourceStart > 0) {
        bestLine = function->lineInSourceStart;
    }

    return bestLine;
}

static TZrBool execbc_aot_pipeline_string_equal(const SZrString *string1, const SZrString *string2) {
    return ZrCore_String_Equal((SZrString *)string1, (SZrString *)string2);
}

static void assert_runtime_function_matches_source_function(const SZrFunction *expected,
                                                            const SZrFunction *actual) {
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(expected);
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_UINT32(expected->stackSize, actual->stackSize);
    TEST_ASSERT_EQUAL_UINT32(expected->instructionsLength, actual->instructionsLength);
    TEST_ASSERT_EQUAL_UINT32(expected->parameterCount, actual->parameterCount);
    TEST_ASSERT_EQUAL_UINT32(expected->childFunctionLength, actual->childFunctionLength);
    TEST_ASSERT_EQUAL_UINT32(expected->memberEntryLength, actual->memberEntryLength);
    TEST_ASSERT_EQUAL_UINT32(expected->callSiteCacheLength, actual->callSiteCacheLength);

    for (index = 0; index < expected->instructionsLength; index++) {
        const TZrInstruction *left = &expected->instructionsList[index];
        const TZrInstruction *right = &actual->instructionsList[index];
        TEST_ASSERT_EQUAL_UINT16(left->instruction.operationCode, right->instruction.operationCode);
        TEST_ASSERT_EQUAL_UINT16(left->instruction.operandExtra, right->instruction.operandExtra);
        TEST_ASSERT_EQUAL_UINT16(left->instruction.operand.operand1[0], right->instruction.operand.operand1[0]);
        TEST_ASSERT_EQUAL_UINT16(left->instruction.operand.operand1[1], right->instruction.operand.operand1[1]);
        TEST_ASSERT_EQUAL_INT32(left->instruction.operand.operand2[0], right->instruction.operand.operand2[0]);
    }

    if (expected->instructionsLength > 0 && expected->executionLocationInfoLength > 0) {
        TEST_ASSERT_EQUAL_UINT32(actual->instructionsLength, actual->executionLocationInfoLength);
        TEST_ASSERT_NOT_NULL(actual->executionLocationInfoList);
        TEST_ASSERT_NOT_NULL(actual->lineInSourceList);
        for (index = 0; index < expected->instructionsLength; index++) {
            TEST_ASSERT_EQUAL_UINT32(index, actual->executionLocationInfoList[index].currentInstructionOffset);
            TEST_ASSERT_EQUAL_UINT32(function_find_debug_line_for_instruction(expected, index),
                                     actual->executionLocationInfoList[index].lineInSource);
            TEST_ASSERT_EQUAL_UINT32(function_find_debug_line_for_instruction(expected, index),
                                     actual->lineInSourceList[index]);
        }
    } else {
        TEST_ASSERT_EQUAL_UINT32(0, actual->executionLocationInfoLength);
        TEST_ASSERT_NULL(actual->executionLocationInfoList);
        TEST_ASSERT_NULL(actual->lineInSourceList);
    }

    for (index = 0; index < expected->memberEntryLength; index++) {
        const SZrFunctionMemberEntry *left = &expected->memberEntries[index];
        const SZrFunctionMemberEntry *right = &actual->memberEntries[index];
        TEST_ASSERT_NOT_NULL(left->symbol);
        TEST_ASSERT_NOT_NULL(right->symbol);
        TEST_ASSERT_TRUE(execbc_aot_pipeline_string_equal(left->symbol, right->symbol));
        TEST_ASSERT_EQUAL_UINT8(left->entryKind, right->entryKind);
        TEST_ASSERT_EQUAL_UINT8(left->reserved0, right->reserved0);
        TEST_ASSERT_EQUAL_UINT16(left->reserved1, right->reserved1);
        TEST_ASSERT_EQUAL_UINT32(left->prototypeIndex, right->prototypeIndex);
        TEST_ASSERT_EQUAL_UINT32(left->descriptorIndex, right->descriptorIndex);
    }

    for (index = 0; index < expected->childFunctionLength; index++) {
        assert_runtime_function_matches_source_function(&expected->childFunctionList[index],
                                                        &actual->childFunctionList[index]);
    }
}

static void assert_optional_string_equal(const SZrString *expected, const SZrString *actual) {
    if (expected == ZR_NULL || actual == ZR_NULL) {
        TEST_ASSERT_EQUAL_PTR(expected, actual);
        return;
    }

    TEST_ASSERT_TRUE(execbc_aot_pipeline_string_equal(expected, actual));
}

static void assert_typed_type_ref_equal(const SZrFunctionTypedTypeRef *expected, const SZrFunctionTypedTypeRef *actual) {
    TEST_ASSERT_NOT_NULL(expected);
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_INT(expected->baseType, actual->baseType);
    TEST_ASSERT_EQUAL_UINT8(expected->isNullable, actual->isNullable);
    TEST_ASSERT_EQUAL_UINT32(expected->ownershipQualifier, actual->ownershipQualifier);
    TEST_ASSERT_EQUAL_UINT8(expected->isArray, actual->isArray);
    assert_optional_string_equal(expected->typeName, actual->typeName);
    TEST_ASSERT_EQUAL_INT(expected->elementBaseType, actual->elementBaseType);
    assert_optional_string_equal(expected->elementTypeName, actual->elementTypeName);
}

static TZrBool function_tree_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 childIndex;
    TZrUInt32 constantIndex;

    if (function_contains_opcode(function, opcode)) {
        return ZR_TRUE;
    }
    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->childFunctionList != ZR_NULL) {
        for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
            if (function_tree_contains_opcode(&function->childFunctionList[childIndex], opcode)) {
                return ZR_TRUE;
            }
        }
    }

    if (function->constantValueList != ZR_NULL) {
        for (constantIndex = 0; constantIndex < function->constantValueLength; constantIndex++) {
            const SZrTypeValue *constantValue = &function->constantValueList[constantIndex];

            if ((constantValue->type == ZR_VALUE_TYPE_FUNCTION || constantValue->type == ZR_VALUE_TYPE_CLOSURE) &&
                constantValue->value.object != ZR_NULL &&
                constantValue->value.object->type == ZR_RAW_OBJECT_TYPE_FUNCTION &&
                function_tree_contains_opcode((const SZrFunction *)constantValue->value.object, opcode)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool function_contains_callsite_cache_kind(const SZrFunction *function,
                                                     EZrFunctionCallSiteCacheKind kind) {
    return function != ZR_NULL && function->callSiteCaches != ZR_NULL &&
           function_count_callsite_cache_kind(function, kind) > 0;
}

static TZrBool function_tree_contains_callsite_cache_kind(const SZrFunction *function,
                                                          EZrFunctionCallSiteCacheKind kind) {
    TZrUInt32 childIndex;

    if (function_contains_callsite_cache_kind(function, kind)) {
        return ZR_TRUE;
    }
    if (function == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (function_tree_contains_callsite_cache_kind(&function->childFunctionList[childIndex], kind)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrUInt32 function_count_callsite_cache_kind(const SZrFunction *function,
                                                    EZrFunctionCallSiteCacheKind kind) {
    TZrUInt32 index;
    TZrUInt32 count = 0;

    if (function == ZR_NULL || function->callSiteCaches == ZR_NULL) {
        return 0;
    }

    for (index = 0; index < function->callSiteCacheLength; index++) {
        if ((EZrFunctionCallSiteCacheKind)function->callSiteCaches[index].kind == kind) {
            count++;
        }
    }

    return count;
}

static TZrBool semir_contains_opcode(const SZrFunction *function, EZrSemIrOpcode opcode) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->semIrInstructionLength; index++) {
        if ((EZrSemIrOpcode)function->semIrInstructions[index].opcode == opcode) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool semir_tree_contains_opcode_with_deopt(const SZrFunction *function,
                                                     EZrSemIrOpcode opcode,
                                                     TZrBool requireDeopt) {
    TZrUInt32 childIndex;

    if (semir_contains_opcode_with_deopt(function, opcode, requireDeopt)) {
        return ZR_TRUE;
    }
    if (function == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (semir_tree_contains_opcode_with_deopt(&function->childFunctionList[childIndex], opcode, requireDeopt)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool semir_contains_opcode_with_deopt(const SZrFunction *function,
                                                EZrSemIrOpcode opcode,
                                                TZrBool requireDeopt) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->semIrInstructions == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < function->semIrInstructionLength; index++) {
        const SZrSemIrInstruction *instruction = &function->semIrInstructions[index];
        if ((EZrSemIrOpcode)instruction->opcode != opcode) {
            continue;
        }
        if (!requireDeopt || instruction->deoptId != 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static const SZrFunctionCallSiteCacheEntry *function_tree_find_first_callsite_cache_kind(
        const SZrFunction *function,
        EZrFunctionCallSiteCacheKind kind) {
    TZrUInt32 index;
    TZrUInt32 childIndex;
    const SZrFunctionCallSiteCacheEntry *found;

    if (function != ZR_NULL && function->callSiteCaches != ZR_NULL) {
        for (index = 0; index < function->callSiteCacheLength; index++) {
            if ((EZrFunctionCallSiteCacheKind)function->callSiteCaches[index].kind == kind) {
                return &function->callSiteCaches[index];
            }
        }
    }
    if (function == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return ZR_NULL;
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        found = function_tree_find_first_callsite_cache_kind(&function->childFunctionList[childIndex], kind);
        if (found != ZR_NULL) {
            return found;
        }
    }

    return ZR_NULL;
}

static TZrUInt32 rewrite_cached_meta_access_tree_to_plain(SZrFunction *function) {
    TZrUInt32 index;
    TZrUInt32 childIndex;
    TZrUInt32 rewritten = 0;

    if (function == ZR_NULL) {
        return 0;
    }

    if (function->instructionsList != ZR_NULL) {
        for (index = 0; index < function->instructionsLength; index++) {
            TZrInstruction *instruction = &function->instructionsList[index];
            EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
            TZrUInt32 cacheIndex = instruction->instruction.operand.operand1[1];
            const SZrFunctionCallSiteCacheEntry *cacheEntry;

            if ((opcode != ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED) &&
                 opcode != ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED)) ||
                function->callSiteCaches == ZR_NULL || cacheIndex >= function->callSiteCacheLength) {
                continue;
            }

            cacheEntry = &function->callSiteCaches[cacheIndex];
            if (opcode == ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED) &&
                (EZrFunctionCallSiteCacheKind)cacheEntry->kind == ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET) {
                instruction->instruction.operationCode = ZR_INSTRUCTION_ENUM(META_GET);
                instruction->instruction.operand.operand1[1] = (TZrUInt16)cacheEntry->memberEntryIndex;
                rewritten++;
            } else if (opcode == ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED) &&
                       (EZrFunctionCallSiteCacheKind)cacheEntry->kind == ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET) {
                instruction->instruction.operationCode = ZR_INSTRUCTION_ENUM(META_SET);
                instruction->instruction.operand.operand1[1] = (TZrUInt16)cacheEntry->memberEntryIndex;
                rewritten++;
            }
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
            rewritten += rewrite_cached_meta_access_tree_to_plain(&function->childFunctionList[childIndex]);
        }
    }

    return rewritten;
}

static const SZrFunctionMemberEntry *find_member_entry_by_symbol(const SZrFunction *function,
                                                                 const char *expectedSymbol) {
    TZrUInt32 index;

    if (function == ZR_NULL || function->memberEntries == ZR_NULL || expectedSymbol == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < function->memberEntryLength; index++) {
        const SZrFunctionMemberEntry *entry = &function->memberEntries[index];
        const char *actualSymbol = entry->symbol != ZR_NULL ? ZrCore_String_GetNativeString(entry->symbol) : ZR_NULL;

        if (actualSymbol != ZR_NULL && strcmp(actualSymbol, expectedSymbol) == 0) {
            return entry;
        }
    }

    return ZR_NULL;
}

static void assert_member_entry_symbol_flags(const SZrFunction *function,
                                             const char *expectedSymbol,
                                             TZrUInt8 expectedFlags) {
    const SZrFunctionMemberEntry *entry = find_member_entry_by_symbol(function, expectedSymbol);
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(function);
    if (entry == ZR_NULL && function->memberEntries != ZR_NULL) {
        fprintf(stderr,
                "missing member entry '%s' in function; memberEntryLength=%u\n",
                expectedSymbol,
                (unsigned int)function->memberEntryLength);
        for (index = 0; index < function->memberEntryLength; index++) {
            const SZrFunctionMemberEntry *current = &function->memberEntries[index];
            fprintf(stderr,
                    "  member[%u] symbol=%s flags=%u kind=%u\n",
                    (unsigned int)index,
                    current->symbol != ZR_NULL ? ZrCore_String_GetNativeString(current->symbol) : "<null>",
                    (unsigned int)current->reserved0,
                    (unsigned int)current->entryKind);
        }
        fflush(stderr);
    }
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_NOT_NULL(entry->symbol);
    TEST_ASSERT_EQUAL_UINT8(expectedFlags, entry->reserved0);
}

static void assert_first_callsite_cache_member_binding(const SZrFunction *function,
                                                       EZrFunctionCallSiteCacheKind kind,
                                                       const char *expectedSymbol,
                                                       TZrUInt8 expectedFlags) {
    const SZrFunctionCallSiteCacheEntry *entry = function_tree_find_first_callsite_cache_kind(function, kind);

    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_TRUE(entry->memberEntryIndex < function->memberEntryLength);
    TEST_ASSERT_NOT_NULL(function->memberEntries);
    TEST_ASSERT_NOT_NULL(function->memberEntries[entry->memberEntryIndex].symbol);
    TEST_ASSERT_EQUAL_STRING(expectedSymbol,
                             ZrCore_String_GetNativeString(function->memberEntries[entry->memberEntryIndex].symbol));
    TEST_ASSERT_EQUAL_UINT8(expectedFlags, function->memberEntries[entry->memberEntryIndex].reserved0);
}

static void test_access_lowering_preserves_explicit_member_and_index_ops(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Access Lowering Preserves Explicit Member And Index Ops";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("access/meta execbc pipeline",
                 "Testing that dot and bracket access stay as explicit member/index opcodes and do not quicken back into legacy dynamic instructions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var obj = { count: 41 };\n"
                "obj.count = obj.count + 1;\n"
                "obj[\"count\"] = obj[\"count\"] + 1;\n"
                "var reflection = %type(obj);\n"
                "return obj.count + obj[\"count\"];";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_dynamic_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        execbc_test_dump_function_tree(func, 0u);

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(TYPEOF)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SET_MEMBER)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SET_BY_INDEX)));

        TEST_ASSERT_TRUE(semir_contains_opcode(func, ZR_SEMIR_OPCODE_TYPEOF));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result));
        TEST_ASSERT_EQUAL_INT64(86, result);

        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_backends_preserve_runtime_contract_artifacts_under_strict_aot_c(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Backends Preserve Runtime Contract Artifacts Under Strict AOT C";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot artifact emission",
                 "Testing that SemIR, AOT C, and AOT LLVM all keep runtime contract evidence while strict AOT C lowers indexed access directly without shim-backed fallback");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var obj = { count: 1 };\n"
                "var next = obj.count + obj[\"count\"] + 2;\n"
                "var reflection = %type(obj);\n"
                "return next;";
        const char *cPath = "execbc_aot_backend_test.c";
        const char *llvmPath = "execbc_aot_backend_test.ll";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_backend_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        execbc_test_dump_function_tree(func, 0u);
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, func, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, func, llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR AOT C Backend"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_GetMember"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_GetByIndex"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_TypeOf"));
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
        assert_generated_aot_c_text_uses_new_code_shape(cText);
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "ZR AOT LLVM Backend"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "%ZrAotCompiledModule = type"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "ZrVm_GetAotCompiledModule"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "declare i1 @ZrCore_Reflection_TypeOfValue"));
        assert_generated_aot_llvm_text_uses_true_backend_shape(llvmText);
        TEST_ASSERT_NULL(strstr(llvmText, "declare i1 @ZrCore_Object_GetByIndex"));

        free(cText);
        free(llvmText);
        remove(cPath);
        remove(llvmPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_emits_child_thunks_for_callable_constants(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Emits Child Thunks For Callable Constants";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c child thunk emission",
                 "Testing that callable constants such as exported lambdas receive their own generated AOT thunks instead of being folded into the entry thunk only");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "pub var greet = () => {\n"
                "    return \"hello from import\";\n"
                "};\n"
                "return greet();";
        const char *cPath = "execbc_aot_child_thunk_test.c";
        const char *binaryPath = "execbc_aot_child_thunk_test.zro";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_child_thunk_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);

        TEST_ASSERT_TRUE(write_embedded_aot_c_file(state,
                                                   func,
                                                   "execbc_aot_child_thunk_test",
                                                   cPath,
                                                   binaryPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrAotCompiledModule"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "static const TZrByte zr_aot_embedded_module_blob[] = {"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "static const FZrAotEntryThunk zr_aot_function_thunks[] = {"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "static TZrInt64 zr_aot_fn_0(struct SZrState *state) {"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "static TZrInt64 zr_aot_fn_1"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_BeginGeneratedFunction(state, 0, &frame)"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_BeginGeneratedFunction(state, 1, &frame)"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_fn_0_ins_0:"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_fn_1_ins_0:"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "    zr_aot_fn_1,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "    2,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "#include \"zr_vm_core/closure.h\""));
        TEST_ASSERT_NOT_NULL(strstr(cText, "SZrClosureNative *zr_aot_closure = ZrCore_ClosureNative_New(state, 0);"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_closure->nativeFunction = zr_aot_fn_1;"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeCurrentClosureShim"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CreateClosure"));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_llvm_backend_lowers_simple_entry_execution_path(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT LLVM Backend Lowers Simple Entry Execution Path";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot llvm simple lowering",
                 "Testing that simple LLVM AOT output lowers the actual constant+return execution path instead of routing the generated function body to unsupported placeholders");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source = "return \"hello world\";";
        const char *llvmPath = "execbc_aot_llvm_simple_entry.ll";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_llvm_simple_entry.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, func, llvmPath));
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(llvmText);
        assert_generated_aot_llvm_text_uses_true_backend_shape(llvmText);
        assert_generated_aot_llvm_text_lowers_simple_return_path(llvmText);
        TEST_ASSERT_NULL(strstr(llvmText,
                                "call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 0, i32 0, i32 2)"));
        TEST_ASSERT_NULL(strstr(llvmText, "zr_aot_fn_0_bb0:"));

        free(llvmText);
        remove(llvmPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_llvm_backend_lowers_static_direct_call_protocol(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT LLVM Backend Lowers Static Direct Call Protocol";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot llvm static direct calls",
                 "Testing that LLVM AOT lowers callable constants through the formal static direct-call frame protocol instead of generic unsupported placeholders");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var greet = () => {\n"
                "    return \"hello from llvm\";\n"
                "};\n"
                "return greet();";
        const char *llvmPath = "execbc_aot_llvm_static_direct_call.ll";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_llvm_static_direct_call.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, func, llvmPath));
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(llvmText);
        assert_generated_aot_llvm_text_uses_true_backend_shape(llvmText);
        assert_generated_aot_llvm_text_lowers_static_direct_call_path(llvmText);

        free(llvmText);
        remove(llvmPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_llvm_backend_lowers_closure_capture_access(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT LLVM Backend Lowers Closure Capture Access";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot llvm closure captures",
                 "Testing that LLVM AOT lowers closure-capture reads through the generated closure-value runtime protocol instead of immediately reporting unsupported GETUPVAL instructions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var captured = 15;\n"
                "var readCapture = () => {\n"
                "    return captured;\n"
                "};\n"
                "return readCapture();";
        const char *llvmPath = "execbc_aot_llvm_closure_capture.ll";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_llvm_closure_capture.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, func, llvmPath));
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(llvmText);
        assert_generated_aot_llvm_text_uses_true_backend_shape(llvmText);
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "@ZrLibrary_AotRuntime_GetClosureValue"));
        TEST_ASSERT_NULL(strstr(llvmText,
                                "call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 1, i32 0, i32 6)"));

        free(llvmText);
        remove(llvmPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_llvm_backend_lowers_object_creation_path(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT LLVM Backend Lowers Object Creation Path";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot llvm create object",
                 "Testing that LLVM AOT lowers CREATE_OBJECT through the generated object-allocation runtime helper instead of emitting an immediate unsupported sink");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var obj = {};\n"
                "return obj;";
        const char *llvmPath = "execbc_aot_llvm_create_object.ll";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_llvm_create_object.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, func, llvmPath));
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(llvmText);
        assert_generated_aot_llvm_text_uses_true_backend_shape(llvmText);
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "@ZrLibrary_AotRuntime_CreateObject"));
        TEST_ASSERT_NULL(strstr(llvmText,
                                "call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 0, i32 0, i32 81)"));

        free(llvmText);
        remove(llvmPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_llvm_backend_lowers_to_object_with_runtime_helper(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT LLVM Backend Lowers ToObject With Runtime Helper";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot llvm to_object lowering",
                 "Testing that LLVM true AOT lowers TO_OBJECT instructions through the dedicated runtime helper instead of emitting unsupported sinks for class/object conversions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "class Person {\n"
                "    pub var id: string;\n"
                "    pub var name: string;\n"
                "    pub @constructor(id: string, name: string) {\n"
                "        this.id = id;\n"
                "        this.name = name;\n"
                "    }\n"
                "}\n"
                "var obj = {id: \"123\", name: \"John\"};\n"
                "var person = <Person> obj;\n"
                "return person.id + person.name;";
        const char *llvmPath = "execbc_aot_llvm_to_object.ll";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_llvm_to_object.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(TO_OBJECT)));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, func, llvmPath));
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(llvmText);
        assert_generated_aot_llvm_text_uses_true_backend_shape(llvmText);
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_ToObject("));
        TEST_ASSERT_NULL(strstr(llvmText,
                                "call i64 @ZrLibrary_AotRuntime_ReportUnsupportedInstruction(ptr %state, i32 0, i32 0, i32 21)"));

        free(llvmText);
        remove(llvmPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_emits_native_entry_descriptor_instead_of_shim_invoke(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Emits Native Entry Descriptor Instead Of Shim Invoke";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c native entry",
                 "Testing that AOT C output exports the formal descriptor and no longer routes entry execution through InvokeActiveShim");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source = "return 7;";
        const char *cPath = "execbc_aot_native_entry_test.c";
        const char *binaryPath = "execbc_aot_native_entry_test.zro";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_native_entry_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);

        TEST_ASSERT_TRUE(write_embedded_aot_c_file(state,
                                                   func,
                                                   "execbc_aot_native_entry_test",
                                                   cPath,
                                                   binaryPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrAotCompiledModule"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrVm_GetAotCompiledModule"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_BeginGeneratedFunction(state, 0, &frame)"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_fn_0_ins_0:"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "SZrTypeValue zr_aot_constant;"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrCore_Value_ResetAsNull(&zr_aot_constant);"));
        TEST_ASSERT_NOT_NULL(
                strstr(cText, "ZR_VALUE_FAST_SET(&zr_aot_constant, nativeInt64, (TZrInt64)7, ZR_VALUE_TYPE_INT64);"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrCore_Value_Copy(state, zr_aot_destination, &zr_aot_constant);"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CopyConstant"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeCurrentClosureShim"));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_directly_lowers_static_slot_and_int_ops(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Directly Lowers Static Slot And Int Ops";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c direct static lowering",
                 "Testing that simple constant loads, local copies, and integer adds lower to direct generated C instead of routing through AOT runtime opcode helpers");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var left = 40;\n"
                "var copied = left;\n"
                "return copied + 2;";
        const char *cPath = "execbc_aot_direct_static_ops_test.c";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_direct_static_ops_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        {
            TZrBool hasGetConstant = function_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_CONSTANT));
            TEST_ASSERT_TRUE(hasGetConstant);
        }
        {
            TZrBool hasGetStack = function_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_STACK));
            TZrBool hasSetStack = function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SET_STACK));
            TEST_ASSERT_TRUE(hasGetStack || hasSetStack);
        }
        {
            TEST_ASSERT_TRUE(function_contains_add_int_family(func) ||
                             function_contains_add_signed_family(func));
        }

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_direct_static_ops_test",
                                                            cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "nativeInt64"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrCore_Stack_GetValue(frame.slotBase +"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR_VALUE_FAST_SET("));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CopyConstant"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CopyStack"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_AddInt("));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_AddIntConst("));
        TEST_ASSERT_NULL(strstr(cText, "&frame.function->constantValueList["));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_directly_lowers_primitive_literal_constants(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Directly Lowers Primitive Literal Constants";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c immediate constant lowering",
                 "Testing that primitive literals lower to direct immediate writes in generated C instead of copying from the function constant table");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var integer = 42;\n"
                "var flag = true;\n"
                "var decimal = 3.5;\n"
                "var nothing = null;\n"
                "var keepFlag = flag;\n"
                "var keepDecimal = decimal;\n"
                "var keepNothing = nothing;\n"
                "return integer;";
        const char *cPath = "execbc_aot_direct_literal_constants_test.c";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_direct_literal_constants_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_CONSTANT)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_direct_literal_constants_test",
                                                            cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "SZrTypeValue *zr_aot_destination = ZrCore_Stack_GetValue(frame.slotBase + "));
        TEST_ASSERT_NOT_NULL(strstr(cText, "SZrTypeValue zr_aot_constant;"));
        TEST_ASSERT_NOT_NULL(
                strstr(cText, "ZR_VALUE_FAST_SET(&zr_aot_constant, nativeInt64, (TZrInt64)42, ZR_VALUE_TYPE_INT64);"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrCore_Value_Copy(state, zr_aot_destination, &zr_aot_constant);"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR_VALUE_TYPE_INT64"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR_VALUE_TYPE_BOOL"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "nativeDouble"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrCore_Value_ResetAsNull(&zr_aot_constant);"));
        TEST_ASSERT_NULL(strstr(cText, "&frame.function->constantValueList["));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CopyConstant"));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_lowers_generic_add_with_fast_path_and_helper_fallback(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Lowers Generic Add With Fast Path And Helper Fallback";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c generic add lowering",
                 "Testing that generic ADD instructions from local AOT function call results compile under strict AOT C, keep an inline numeric fast path, and fall back to the runtime helper only for the non-trivial cases");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "pub var left = () => {\n"
                "    return 15;\n"
                "};\n"
                "pub var right = () => {\n"
                "    return 16;\n"
                "};\n"
                "return left() + right();";
        const char *cPath = "execbc_aot_generic_add_test.c";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_generic_add_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(ADD)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS)) ||
                         function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_generic_add_test",
                                                            cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR_VALUE_IS_TYPE_NUMBER(zr_aot_left->type)"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR_VALUE_IS_TYPE_STRING(zr_aot_left->type)"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_Add(state, &frame, 4, 2, 3)"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_llvm_backend_directly_lowers_static_slot_and_int_ops(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT LLVM Backend Directly Lowers Static Slot And Int Ops";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot llvm direct static lowering",
                 "Testing that simple constant loads, local copies, and integer adds lower to direct LLVM IR instead of routing through AOT runtime opcode helpers");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var left = 40;\n"
                "var copied = left;\n"
                "return copied + 2;";
        const char *llvmPath = "execbc_aot_llvm_direct_static_ops_test.ll";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_llvm_direct_static_ops_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_CONSTANT)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_STACK)) ||
                         function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SET_STACK)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(ADD_INT)) ||
                         function_contains_opcode(func, ZR_INSTRUCTION_ENUM(ADD_INT_CONST)) ||
                         function_contains_opcode(func, ZR_INSTRUCTION_ENUM(ADD_SIGNED)) ||
                         function_contains_opcode(func, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST)) ||
                         function_contains_opcode(func, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED)) ||
                         function_contains_opcode(func, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST)));

        remove(llvmPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               func,
                                                               "execbc_aot_llvm_direct_static_ops_test",
                                                               llvmPath));

        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(llvmText);

        assert_generated_aot_llvm_text_uses_aligned_stack_value_layout(llvmText);
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "store i64 40"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "load %SZrTypeValue, ptr"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "add i64"));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_CopyConstant("));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_CopyStack("));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_AddInt("));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_AddIntConst("));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_AddSigned("));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_AddSignedConst("));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_AddUnsigned("));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_AddUnsignedConst("));

        free(llvmText);
        remove(llvmPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_llvm_backend_lowers_generic_add_with_fast_path_and_helper_fallback(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT LLVM Backend Lowers Generic Add With Fast Path And Helper Fallback";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot llvm generic add lowering",
                 "Testing that generic ADD keeps an inline numeric fast path in LLVM AOT while still falling back to the runtime helper for non-trivial cases");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "pub var left = () => {\n"
                "    return 15;\n"
                "};\n"
                "pub var right = () => {\n"
                "    return 16;\n"
                "};\n"
                "return left() + right();";
        const char *llvmPath = "execbc_aot_llvm_generic_add_test.ll";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_llvm_generic_add_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(ADD)));

        remove(llvmPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               func,
                                                               "execbc_aot_llvm_generic_add_test",
                                                               llvmPath));

        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_Add("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "fadd double"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "add i64"));

        free(llvmText);
        remove(llvmPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_directly_lowers_local_callable_constants(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Directly Lowers Local Callable Constants";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c callable constant lowering",
                 "Testing that local callable constants and closure creation lower to direct native-closure construction instead of routing through the AOT runtime create-closure helper");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "pub var greet = () => {\n"
                "    return 7;\n"
                "};\n"
                "return greet();";
        const char *cPath = "execbc_aot_direct_callable_constant_test.c";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_direct_callable_constant_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(CREATE_CLOSURE)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_direct_callable_constant_test",
                                                            cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "SZrClosureNative *zr_aot_closure = ZrCore_ClosureNative_New(state, 0);"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_closure->nativeFunction = zr_aot_fn_1;"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_closure->aotShimFunction ="));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CreateClosure"));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_directly_lowers_local_aot_function_calls(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Directly Lowers Local AOT Function Calls";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c direct function call lowering",
                 "Testing that the representative module-graph AOT C fixture still preserves the runtime direct-call fast path for call targets that are not statically bound by the emitter");

    {
        char *cText;
        TZrChar cPath[ZR_TESTS_PATH_MAX];
        TZrSize cTextLength = 0;

        TEST_ASSERT_TRUE(ZrTests_Path_GetProjectFile("aot_module_graph_pipeline",
                                                     "bin/aot_c/src/main.c",
                                                     cPath,
                                                     sizeof(cPath)));
        cText = ZrTests_ReadTextFile(cPath, &cTextLength);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_TRUE(cTextLength > 0);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrAotGeneratedDirectCall zr_aot_direct_call;"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_PrepareDirectCall(state,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CallPreparedOrGeneric(state,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_BeginInstruction(state, &frame,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR_AOT_GENERATED_STEP_FLAG_CALL"));
        TEST_ASSERT_NULL(strstr(cText, "ZR_AOT_C_GUARD(zr_aot_direct_call.nativeFunction(state));"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_FinishDirectCall(state, &frame, &zr_aot_direct_call, 1)"));
        TEST_ASSERT_NULL(strstr(cText, "if (!zr_aot_direct_call.nativeFunction(state))"));
        TEST_ASSERT_NULL(strstr(cText, "state->callInfoList->context.context.programCounter = frame.function->instructionsList +"));
        assert_generated_aot_c_text_uses_new_code_shape(cText);

        free(cText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_statically_lowers_proven_local_aot_function_calls(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Statically Lowers Proven Local AOT Function Calls";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c static direct function call lowering",
                 "Testing that local call targets with compile-time-proven AOT provenance lower to static thunk calls with explicit VM call-frame setup instead of runtime closure probing");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "pub var greet = () => {\n"
                "    return 7;\n"
                "};\n"
                "var local = greet;\n"
                "return local();";
        const char *cPath = "execbc_aot_static_direct_function_call_test.c";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_static_direct_function_call_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_static_direct_function_call_test",
                                                            cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_PrepareStaticDirectCall(state,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR_AOT_C_GUARD(zr_aot_fn_1(state));"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_FinishDirectCall(state, &frame, &zr_aot_direct_call, 1)"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_BeginInstruction(state, &frame,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR_AOT_GENERATED_STEP_FLAG_CALL"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_PrepareDirectCall(state,"));
        TEST_ASSERT_NULL(strstr(cText, "zr_aot_direct_call.nativeFunction(state)"));
        TEST_ASSERT_NULL(strstr(cText, "if (!zr_aot_fn_1(state))"));
        TEST_ASSERT_NULL(strstr(cText, "state->callInfoList->context.context.programCounter = frame.function->instructionsList +"));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_lowers_cached_meta_calls_with_direct_call_frames(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Lowers Cached Meta Calls With Direct Call Frames";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c meta call lowering",
                 "Testing that cached @call dispatch sites lower to explicit AOT meta-call preparation plus VM-visible direct-call frames instead of rejecting the site or falling back to a shim");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "class Adder {\n"
                "    pub var base: int;\n"
                "    pub @constructor(base: int) {\n"
                "        this.base = base;\n"
                "    }\n"
                "    pub @call(value: int): int {\n"
                "        return this.base + value;\n"
                "    }\n"
                "}\n"
                "var adder = new Adder(7);\n"
                "var result = adder(5);\n"
                "return result;";
        const char *cPath = "execbc_aot_cached_meta_call_test.c";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_cached_meta_call_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED)));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(func, ZR_SEMIR_OPCODE_META_CALL, ZR_TRUE));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_cached_meta_call_test",
                                                            cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_PrepareMetaCall"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrAotGeneratedDirectCall zr_aot_direct_call;"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CallPreparedOrGeneric(state,"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
        TEST_ASSERT_NULL(strstr(cText, "ZR_AOT_C_GUARD(zr_aot_direct_call.nativeFunction(state));"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_FinishDirectCall(state, &frame, &zr_aot_direct_call, 1)"));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_llvm_backend_lowers_cached_meta_calls_with_direct_call_frames(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT LLVM Backend Lowers Cached Meta Calls With Direct Call Frames";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot llvm meta call lowering",
                 "Testing that cached @call dispatch sites lower to explicit LLVM meta-call preparation plus VM-visible direct-call frames instead of emitting unsupported instruction sinks");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "class Adder {\n"
                "    pub var base: int;\n"
                "    pub @constructor(base: int) {\n"
                "        this.base = base;\n"
                "    }\n"
                "    pub @call(value: int): int {\n"
                "        return this.base + value;\n"
                "    }\n"
                "}\n"
                "var adder = new Adder(7);\n"
                "var result = adder(5);\n"
                "return result;";
        const char *llvmPath = "execbc_aot_cached_meta_call_test.ll";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_cached_meta_call_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED)));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(func, ZR_SEMIR_OPCODE_META_CALL, ZR_TRUE));

        remove(llvmPath);
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, func, llvmPath));

        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(llvmText);
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_PrepareMetaCall("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED)));
        TEST_ASSERT_NULL(strstr(llvmText, "call i64 @ZrLibrary_AotRuntime_InvokeActiveShim("));

        free(llvmText);
        remove(llvmPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_lowers_plain_meta_get_set_with_runtime_helpers(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Lowers Plain Meta Get Set With Runtime Helpers";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c plain meta get/set lowering",
                 "Testing that non-cached property getter/setter instructions lower to dedicated runtime helpers instead of being rejected when they survive quickening inside child methods");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *cPath = "execbc_aot_plain_meta_get_set_test.c";
        const char *llvmPath = "execbc_aot_plain_meta_get_set_test.ll";
        SZrFunction *func;
        char *cText;
        char *llvmText;
        TZrUInt32 rewritten;

        TEST_ASSERT_NOT_NULL(state);
        func = compile_meta_access_fixture(state);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED)));
        rewritten = rewrite_cached_meta_access_tree_to_plain(func);
        TEST_ASSERT_TRUE(rewritten >= 2);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(META_GET)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(META_SET)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_plain_meta_get_set_test",
                                                            cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, func, llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_MetaGet(state, &frame,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_MetaSet(state, &frame,"));
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_MetaGet("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_MetaSet("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(META_GET)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(META_SET)));

        free(cText);
        free(llvmText);
        remove(cPath);
        remove(llvmPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_llvm_backend_lowers_meta_tail_calls_with_direct_call_frames(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT LLVM Backend Lowers Meta Tail Calls With Direct Call Frames";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot llvm meta tail-call lowering",
                 "Testing that zero-argument meta tail calls lower through the shared generated call-frame protocol and return path instead of reporting unsupported instructions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *llvmPath = "execbc_aot_meta_tail_call_test.ll";
        SZrFunction *function;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_zero_arg_tail_quickening_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS)));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_TAIL_CALL, ZR_TRUE));

        remove(llvmPath);
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));

        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(llvmText);
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_PrepareMetaCall("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_CallPreparedOrGeneric("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i64 @ZrLibrary_AotRuntime_Return("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS)));
        TEST_ASSERT_NULL(strstr(llvmText, "call i64 @ZrLibrary_AotRuntime_InvokeActiveShim("));

        free(llvmText);
        remove(llvmPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_llvm_backend_lowers_exception_control_transfer_helpers(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT LLVM Backend Lowers Exception Control Transfer Helpers";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot llvm eh/control lowering",
                 "Testing that TRY/END_TRY/THROW/CATCH/END_FINALLY and pending-return control transfer lower to dedicated runtime helpers instead of unsupported instruction sinks");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *llvmPath = "execbc_aot_exception_control_test.ll";
        SZrFunction *function;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_exception_control_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(TRY)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(END_TRY)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(THROW)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(CATCH)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(END_FINALLY)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN)));

        remove(llvmPath);
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));

        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(llvmText);
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_Try("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_EndTry("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_Throw("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_Catch("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_EndFinally("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SetPendingReturn("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(TRY)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(END_TRY)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(THROW)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(CATCH)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(END_FINALLY)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN)));

        free(llvmText);
        remove(llvmPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_lowers_cached_dynamic_tail_calls_with_runtime_prepare(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Lowers Cached Dynamic Tail Calls With Runtime Prepare";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c cached dynamic tail-call lowering",
                 "Testing that quickened dynamic tail-call sites lower through the runtime dynamic-call preparation helper instead of remaining unsupported in child functions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "class Adder {\n"
                "    pri var base: int = 0;\n"
                "    pub @constructor(base: int) {\n"
                "        this.base = base;\n"
                "    }\n"
                "    pub @call(delta: int): int {\n"
                "        return this.base + delta;\n"
                "    }\n"
                "}\n"
                "func apply(fn, value: int): int {\n"
                "    return fn(value);\n"
                "}\n"
                "var adder = new Adder(4);\n"
                "return apply(adder, 3);";
        const char *cPath = "execbc_aot_cached_dynamic_tail_call_test.c";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_cached_dynamic_tail_call_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_cached_dynamic_tail_call_test",
                                                            cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_Call(state, &frame,"));
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_directly_lowers_non_export_return(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Directly Lowers Non Export Return";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c direct return lowering",
                 "Testing that functions without export publication inline their return writeback in generated C instead of routing through the AOT runtime return helper");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var left = 40;\n"
                "var copied = left;\n"
                "return copied + 2;";
        const char *cPath = "execbc_aot_direct_return_test.c";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_direct_return_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)));
        TEST_ASSERT_EQUAL_UINT32(0, func->exportedVariableLength);

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state, func, "execbc_aot_direct_return_test", cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "SZrCallInfo *zr_aot_call_info = frame.callInfo;"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrCore_Value_Copy(state,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_call_info->functionBase.valuePointer"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "state->stackTop.valuePointer = zr_aot_call_info->functionBase.valuePointer + 1;"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_Return"));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_lowers_to_object_with_runtime_helper(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Lowers ToObject With Runtime Helper";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c to_object lowering",
                 "Testing that strict AOT C lowers TO_OBJECT instructions through the dedicated runtime helper instead of rejecting class/object conversions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "class Person {\n"
                "    pub var id: string;\n"
                "    pub var name: string;\n"
                "    pub @constructor(id: string, name: string) {\n"
                "        this.id = id;\n"
                "        this.name = name;\n"
                "    }\n"
                "}\n"
                "var obj = {id: \"123\", name: \"John\"};\n"
                "var person = <Person> obj;\n"
                "return person.id + person.name;";
        const char *cPath = "execbc_aot_to_object_test.c";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_to_object_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(TO_OBJECT)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state, func, "execbc_aot_to_object_test", cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ToObject(state, &frame,"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_lowers_to_struct_with_runtime_helper(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Lowers ToStruct With Runtime Helper";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c to_struct lowering",
                 "Testing that strict AOT C lowers TO_STRUCT instructions through the dedicated runtime helper instead of rejecting struct conversions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "struct Person {\n"
                "    pub var id: string;\n"
                "    pub var name: string;\n"
                "}\n"
                "var obj = {id: \"123\", name: \"John\"};\n"
                "var person = <Person> obj;\n"
                "return person.id + person.name;";
        const char *cPath = "execbc_aot_to_struct_test.c";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_to_struct_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(TO_STRUCT)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state, func, "execbc_aot_to_struct_test", cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ToStruct(state, &frame,"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_lowers_signed_compare_div_and_neg_paths(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Lowers Signed Compare Div And Neg Paths";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c signed arithmetic lowering",
                 "Testing that strict AOT C lowers signed compare/div/neg instruction sequences into generated C control flow plus dedicated helpers instead of rejecting them as unsupported");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var left = 4;\n"
                "var right = 8;\n"
                "var flag = left < right;\n"
                "var delta = right - left;\n"
                "var quotient = 12 / (delta - 2);\n"
                "if (flag) {\n"
                "    return -quotient;\n"
                "}\n"
                "return quotient;";
        const char *cPath = "execbc_aot_signed_numeric_paths_test.c";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_signed_numeric_paths_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUB_INT)) ||
                         function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUB_SIGNED)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(DIV_SIGNED)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(NEG)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_signed_numeric_paths_test",
                                                            cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_left_int < zr_aot_right_int"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_SubSigned(state, &frame,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_DivSigned(state, &frame,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_Neg(state, &frame,"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_backends_lower_benchmark_style_mod_string_and_compare_paths(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Backends Lower Benchmark Style Mod String And Compare Paths";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("benchmark style aot lowering",
                 "Testing that strict AOT C and LLVM lower benchmark-like generic modulo, <= compare, TO_STRING, and string concatenation paths instead of reporting unsupported instructions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var container = %import(\"zr.container\");\n"
                "var items = new container.Array<int>();\n"
                "items.add(5);\n"
                "items.add(8);\n"
                "var slot = items.length % 3;\n"
                "var label = <string> slot + \":\" + <string> (slot + 1);\n"
                "var inRange = slot <= 2;\n"
                "if (inRange) {\n"
                "    if (label == \"2:3\") {\n"
                "        return 1;\n"
                "    }\n"
                "}\n"
                "return 0;";
        const char *cPath = "execbc_aot_benchmark_style_string_numeric_test.c";
        const char *llvmPath = "execbc_aot_benchmark_style_string_numeric_test.ll";
        SZrString *sourceName;
        SZrFunction *function;
        char *cText;
        char *llvmText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(ZrVmLibContainer_Register(state->global));

        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_benchmark_style_string_numeric_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MOD)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(TO_STRING)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_STRING)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED)));

        remove(cPath);
        remove(llvmPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            function,
                                                            "execbc_aot_benchmark_style_string_numeric_test",
                                                            cPath));
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               function,
                                                               "execbc_aot_benchmark_style_string_numeric_test",
                                                               llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_Mod(state, &frame,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ToString(state, &frame,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_Add(state, &frame,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_LogicalLessEqualSigned(state, &frame,"));
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));

        TEST_ASSERT_NOT_NULL(strstr(llvmText, "@ZrLibrary_AotRuntime_Mod("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "@ZrLibrary_AotRuntime_ToString("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "@ZrLibrary_AotRuntime_Add("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "@ZrLibrary_AotRuntime_LogicalLessEqualSigned("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(MOD)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(TO_STRING)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(ADD_STRING)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(1, result);

        free(cText);
        free(llvmText);
        remove(cPath);
        remove(llvmPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_string_equality_allows_destination_aliasing_left_operand(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "String Equality Allows Destination Aliasing Left Operand";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("string equality aliasing",
                 "Testing that LOGICAL_EQUAL_STRING reads both string operands before it overwrites a destination slot that aliases the left operand");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction function;
        SZrTypeValue constants[2];
        TZrInstruction instructions[4];
        SZrString *leftText;
        SZrString *rightText;
        SZrTypeValue resultValue;

        TEST_ASSERT_NOT_NULL(state);

        leftText = ZR_STRING_LITERAL(state, "2:3");
        rightText = ZR_STRING_LITERAL(state, "2:3");
        TEST_ASSERT_NOT_NULL(leftText);
        TEST_ASSERT_NOT_NULL(rightText);

        ZrCore_Value_InitAsRawObject(state, &constants[0], ZR_CAST_RAW_OBJECT_AS_SUPER(leftText));
        constants[0].type = ZR_VALUE_TYPE_STRING;
        ZrCore_Value_InitAsRawObject(state, &constants[1], ZR_CAST_RAW_OBJECT_AS_SUPER(rightText));
        constants[1].type = ZR_VALUE_TYPE_STRING;

        instructions[0] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 0);
        instructions[1] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 1);
        instructions[2] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING), 1, 1, 2);
        instructions[3] = make_instruction_return(1);
        init_manual_test_function(&function,
                                  instructions,
                                  (TZrUInt32)(sizeof(instructions) / sizeof(instructions[0])),
                                  constants,
                                  (TZrUInt32)(sizeof(constants) / sizeof(constants[0])),
                                  ZR_NULL,
                                  0,
                                  3);

        ZrCore_Value_ResetAsNull(&resultValue);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, &function, &resultValue));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, resultValue.type);
        TEST_ASSERT_TRUE(resultValue.value.nativeObject.nativeBool);

        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_backends_lower_benchmark_style_generic_sub_paths(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Backends Lower Benchmark Style Generic Sub Paths";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("benchmark style generic subtraction lowering",
                 "Testing that strict AOT C and LLVM lower benchmark-like generic SUB paths sourced from indexed container values instead of reporting unsupported instructions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var container = %import(\"zr.container\");\n"
                "var lhs = new container.Array<int>();\n"
                "var rhs = new container.Array<int>();\n"
                "var dst = new container.Array<int>();\n"
                "lhs.add(7);\n"
                "rhs.add(3);\n"
                "dst.add(0);\n"
                "dst[0] = lhs[0] + rhs[0];\n"
                "var scratch = dst[0] - lhs[0] / 3 + (rhs[0] % 11);\n"
                "return scratch;";
        const char *cPath = "execbc_aot_benchmark_style_generic_sub_test.c";
        const char *llvmPath = "execbc_aot_benchmark_style_generic_sub_test.ll";
        SZrString *sourceName;
        SZrFunction *function;
        char *cText;
        char *llvmText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(ZrVmLibContainer_Register(state->global));

        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_benchmark_style_generic_sub_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB)) ||
                         function_contains_sub_int_family(function) ||
                         function_contains_sub_signed_family(function));

        remove(cPath);
        remove(llvmPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            function,
                                                            "execbc_aot_benchmark_style_generic_sub_test",
                                                            cPath));
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               function,
                                                               "execbc_aot_benchmark_style_generic_sub_test",
                                                               llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_TRUE(strstr(cText, "ZrLibrary_AotRuntime_Sub(state, &frame,") != ZR_NULL ||
                         strstr(cText, "ZrLibrary_AotRuntime_SubInt(state, &frame,") != ZR_NULL ||
                         strstr(cText, "ZrLibrary_AotRuntime_SubIntConst(state, &frame,") != ZR_NULL ||
                         strstr(cText, "ZrLibrary_AotRuntime_SubSigned(state, &frame,") != ZR_NULL ||
                         strstr(cText, "ZrLibrary_AotRuntime_SubSignedConst(state, &frame,") != ZR_NULL);
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));

        TEST_ASSERT_TRUE(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_Sub(") != ZR_NULL ||
                         strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SubInt(") != ZR_NULL ||
                         strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SubIntConst(") != ZR_NULL ||
                         strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SubSigned(") != ZR_NULL ||
                         strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SubSignedConst(") != ZR_NULL);
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUB)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUB_INT)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(SUB_INT_CONST)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUB_SIGNED)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(11, result);

        free(cText);
        free(llvmText);
        remove(cPath);
        remove(llvmPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_backends_lower_benchmark_style_generic_mul_paths(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Backends Lower Benchmark Style Generic Mul Paths";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("benchmark style generic multiplication lowering",
                 "Testing that strict AOT C and LLVM lower benchmark-like generic MUL paths sourced from method results instead of reporting unsupported instructions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "class MultiplyWorker {\n"
                "    pri var state: int;\n"
                "    pub @constructor(seed: int) {\n"
                "        this.state = seed;\n"
                "    }\n"
                "    pub read(): int {\n"
                "        return this.state;\n"
                "    }\n"
                "}\n"
                "var worker = new MultiplyWorker(11);\n"
                "var outer = 5;\n"
                "return 7 + worker.read() * (outer + 1);";
        const char *cPath = "execbc_aot_benchmark_style_generic_mul_test.c";
        const char *llvmPath = "execbc_aot_benchmark_style_generic_mul_test.ll";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *function;
        char *cText;
        char *llvmText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_benchmark_style_generic_mul_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        function = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL)));

        remove(cPath);
        remove(llvmPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            function,
                                                            "execbc_aot_benchmark_style_generic_mul_test",
                                                            cPath));
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               function,
                                                               "execbc_aot_benchmark_style_generic_mul_test",
                                                               llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_Mul(state, &frame,"));
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));

        TEST_ASSERT_NOT_NULL(strstr(llvmText, "@ZrLibrary_AotRuntime_Mul("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(MUL)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(73, result);

        free(cText);
        free(llvmText);
        remove(cPath);
        remove(llvmPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_backends_lower_benchmark_style_generic_div_paths(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Backends Lower Benchmark Style Generic Div Paths";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("benchmark style generic division lowering",
                 "Testing that strict AOT C and LLVM lower benchmark-like generic DIV paths sourced from indexed container values instead of reporting unsupported instructions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var container = %import(\"zr.container\");\n"
                "var lhs = new container.Array<int>();\n"
                "lhs.add(21);\n"
                "return lhs[0] / 3;";
        const char *cPath = "execbc_aot_benchmark_style_generic_div_test.c";
        const char *llvmPath = "execbc_aot_benchmark_style_generic_div_test.ll";
        SZrString *sourceName;
        SZrFunction *function;
        char *cText;
        char *llvmText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(ZrVmLibContainer_Register(state->global));

        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_benchmark_style_generic_div_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DIV)) ||
                         function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DIV_SIGNED)) ||
                         function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST)));

        remove(cPath);
        remove(llvmPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            function,
                                                            "execbc_aot_benchmark_style_generic_div_test",
                                                            cPath));
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               function,
                                                               "execbc_aot_benchmark_style_generic_div_test",
                                                               llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_TRUE(strstr(cText, "ZrLibrary_AotRuntime_Div(state, &frame,") != ZR_NULL ||
                         strstr(cText, "ZrLibrary_AotRuntime_DivSigned(state, &frame,") != ZR_NULL ||
                         strstr(cText, "ZrLibrary_AotRuntime_DivSignedConst(state, &frame,") != ZR_NULL);
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));

        TEST_ASSERT_TRUE(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_Div(") != ZR_NULL ||
                         strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_DivSigned(") != ZR_NULL ||
                         strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_DivSignedConst(") != ZR_NULL);
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(DIV)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(DIV_SIGNED)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(7, result);

        free(cText);
        free(llvmText);
        remove(cPath);
        remove(llvmPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_backends_lower_benchmark_style_bitwise_xor_paths(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Backends Lower Benchmark Style Bitwise Xor Paths";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("benchmark style xor lowering",
                 "Testing that strict AOT C and LLVM lower benchmark-like BITWISE_XOR paths in child methods instead of reporting unsupported instructions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "class XorWorker {\n"
                "    pri var state: int;\n"
                "    pub @constructor(seed: int) {\n"
                "        this.state = seed;\n"
                "    }\n"
                "    pub step(delta: int): int {\n"
                "        this.state = ((this.state ^ (delta + 31)) + delta * 5 + 19) % 10037;\n"
                "        return this.state;\n"
                "    }\n"
                "}\n"
                "var worker = new XorWorker(43);\n"
                "return worker.step(5);";
        const char *cPath = "execbc_aot_benchmark_style_bitwise_xor_test.c";
        const char *llvmPath = "execbc_aot_benchmark_style_bitwise_xor_test.ll";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *function;
        char *cText;
        char *llvmText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_benchmark_style_bitwise_xor_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        function = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(BITWISE_XOR)));

        remove(cPath);
        remove(llvmPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            function,
                                                            "execbc_aot_benchmark_style_bitwise_xor_test",
                                                            cPath));
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               function,
                                                               "execbc_aot_benchmark_style_bitwise_xor_test",
                                                               llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_BitwiseXor(state, &frame,"));
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));

        TEST_ASSERT_NOT_NULL(strstr(llvmText, "@ZrLibrary_AotRuntime_BitwiseXor("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(BITWISE_XOR)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(59, result);

        free(cText);
        free(llvmText);
        remove(cPath);
        remove(llvmPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_runtime_observation_policy_is_thread_local(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Runtime Observation Policy Is Thread Local";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot runtime thread local observation policy",
                 "Testing that each state resolves its own AOT observation/debug policy without leaking publish-all or mask overrides across threads");

    {
        SZrState *left = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrState *right = ZrTests_Runtime_State_Create(ZR_NULL);
        TZrUInt32 leftMask = 0;
        TZrUInt32 rightMask = 0;
        TZrBool leftPublishAll = ZR_FALSE;
        TZrBool rightPublishAll = ZR_FALSE;

        TEST_ASSERT_NOT_NULL(left);
        TEST_ASSERT_NOT_NULL(right);

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_GetObservationPolicy(left, &leftMask, &leftPublishAll));
        TEST_ASSERT_EQUAL_UINT32(ZrLibrary_AotRuntime_DefaultObservationMask(), leftMask);
        TEST_ASSERT_FALSE(leftPublishAll);

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_SetObservationPolicy(left,
                                                                   ZR_AOT_GENERATED_STEP_FLAG_CALL,
                                                                   ZR_FALSE));
        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_GetObservationPolicy(left, &leftMask, &leftPublishAll));
        TEST_ASSERT_EQUAL_UINT32(ZR_AOT_GENERATED_STEP_FLAG_CALL, leftMask);
        TEST_ASSERT_FALSE(leftPublishAll);

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_GetObservationPolicy(right, &rightMask, &rightPublishAll));
        TEST_ASSERT_EQUAL_UINT32(ZrLibrary_AotRuntime_DefaultObservationMask(), rightMask);
        TEST_ASSERT_FALSE(rightPublishAll);

        right->debugHookSignal = ZR_DEBUG_HOOK_MASK_LINE;
        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_GetObservationPolicy(right, &rightMask, &rightPublishAll));
        TEST_ASSERT_EQUAL_UINT32(ZrLibrary_AotRuntime_DefaultObservationMask(), rightMask);
        TEST_ASSERT_TRUE(rightPublishAll);

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ResetObservationPolicy(left));
        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_GetObservationPolicy(left, &leftMask, &leftPublishAll));
        TEST_ASSERT_EQUAL_UINT32(ZrLibrary_AotRuntime_DefaultObservationMask(), leftMask);
        TEST_ASSERT_FALSE(leftPublishAll);

        ZrTests_Runtime_State_Destroy(left);
        ZrTests_Runtime_State_Destroy(right);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_runtime_begin_generated_function_caches_slot_count_and_preserves_bounds_after_refresh(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary =
            "AOT Runtime BeginGeneratedFunction Caches Slot Count And Preserves Bounds After Refresh";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot runtime cached frame slot count",
                 "Testing that generated frame slot bounds come from the cached BeginGeneratedFunction slot count and remain valid after the frame refreshes onto a relocated stack base");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrLibrary_Project project;
        SZrFunction function;
        TZrInstruction instructions[1];
        ZrAotGeneratedFrame frame;
        TZrStackValuePointer functionBase;
        TZrStackValuePointer relocatedBase;
        SZrTypeValue *functionBaseValue;
        SZrTypeValue *slot0Value;
        SZrTypeValue *slot1Value;
        SZrClosureNative *nativeClosure;

        TEST_ASSERT_NOT_NULL(state);
        memset(&project, 0, sizeof(project));
        memset(&function, 0, sizeof(function));
        memset(&frame, 0, sizeof(frame));

        instructions[0] = make_instruction_return(1);
        init_manual_test_function(&function,
                                  instructions,
                                  (TZrUInt32)(sizeof(instructions) / sizeof(instructions[0])),
                                  ZR_NULL,
                                  0,
                                  ZR_NULL,
                                  0,
                                  2);
        aot_runtime_test_install_project_record(state, &project, &function, ZR_AOT_BACKEND_KIND_C);

        nativeClosure = ZrCore_ClosureNative_New(state, 0);
        TEST_ASSERT_NOT_NULL(nativeClosure);
        nativeClosure->aotShimFunction = &function;

        functionBase = state->stackTop.valuePointer;
        functionBase = ZrCore_Function_CheckStackAndGc(state, 3, functionBase);
        TEST_ASSERT_NOT_NULL(functionBase);
        functionBaseValue = ZrCore_Stack_GetValue(functionBase);
        slot0Value = ZrCore_Stack_GetValue(functionBase + 1);
        slot1Value = ZrCore_Stack_GetValue(functionBase + 2);
        TEST_ASSERT_NOT_NULL(functionBaseValue);
        TEST_ASSERT_NOT_NULL(slot0Value);
        TEST_ASSERT_NOT_NULL(slot1Value);
        ZrCore_Value_InitAsRawObject(state, functionBaseValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));
        ZrCore_Value_InitAsInt(state, slot0Value, 7);
        ZrCore_Value_ResetAsNull(slot1Value);

        memset(&state->baseCallInfo, 0, sizeof(state->baseCallInfo));
        state->baseCallInfo.functionBase.valuePointer = functionBase;
        state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
        state->callInfoList = &state->baseCallInfo;
        state->stackTop.valuePointer = functionBase + 3;

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginGeneratedFunction(state, 0, &frame));
        TEST_ASSERT_EQUAL_UINT32(2u, frame.generatedFrameSlotCount);
        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_CopyStack(state, &frame, 1, 0));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, slot1Value->type);
        TEST_ASSERT_EQUAL_INT64(7, slot1Value->value.nativeObject.nativeInt64);
        TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_CopyStack(state, &frame, 2, 0));

        relocatedBase = state->stackTop.valuePointer;
        relocatedBase = ZrCore_Function_CheckStackAndGc(state, 3, relocatedBase);
        TEST_ASSERT_NOT_NULL(relocatedBase);
        ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(relocatedBase), functionBaseValue);
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(relocatedBase + 1), 9);
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(relocatedBase + 2));
        state->baseCallInfo.functionBase.valuePointer = relocatedBase;
        state->baseCallInfo.functionTop.valuePointer = relocatedBase + 3;
        state->stackTop.valuePointer = relocatedBase + 3;

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginInstruction(state,
                                                               &frame,
                                                               0,
                                                               ZR_AOT_GENERATED_STEP_FLAG_NONE));
        TEST_ASSERT_EQUAL_PTR(relocatedBase + 1, frame.slotBase);
        TEST_ASSERT_EQUAL_UINT32(2u, frame.generatedFrameSlotCount);
        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_CopyStack(state, &frame, 1, 0));
        TEST_ASSERT_EQUAL_INT64(9, ZrCore_Stack_GetValue(relocatedBase + 2)->value.nativeObject.nativeInt64);
        TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_CopyStack(state, &frame, 2, 0));

        aot_runtime_test_remove_project_record(state, &project);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_runtime_prepare_static_direct_call_uses_cached_callee_slot_count(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Runtime PrepareStaticDirectCall Uses Cached Callee Slot Count";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot runtime direct-call slot cache",
                 "Testing that static direct-call frame sizing comes from the cached per-function slot count even if the callee metadata changes after the record is installed");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrLibrary_Project project;
        SZrFunction rootFunction;
        SZrFunction childFunctions[1];
        TZrInstruction rootInstructions[1];
        TZrInstruction childInstructions[2];
        const FZrAotEntryThunk functionThunks[2] = {aot_runtime_test_dummy_entry_thunk,
                                                    aot_runtime_test_dummy_entry_thunk};
        ZrAotCompiledModule descriptor;
        SZrExecBcAotTestRuntimeState *runtimeState;
        ZrAotGeneratedFrame frame;
        ZrAotGeneratedDirectCall directCall;
        TZrStackValuePointer functionBase;
        SZrTypeValue *functionBaseValue;
        SZrTypeValue *slot0Value;
        SZrTypeValue *slot1Value;
        SZrClosureNative *nativeClosure;

        TEST_ASSERT_NOT_NULL(state);
        memset(&project, 0, sizeof(project));
        memset(&rootFunction, 0, sizeof(rootFunction));
        memset(childFunctions, 0, sizeof(childFunctions));
        memset(rootInstructions, 0, sizeof(rootInstructions));
        memset(childInstructions, 0, sizeof(childInstructions));
        memset(&descriptor, 0, sizeof(descriptor));
        memset(&frame, 0, sizeof(frame));
        memset(&directCall, 0, sizeof(directCall));

        childInstructions[0] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 5, 0);
        childInstructions[1] = make_instruction_return(5);
        init_manual_test_function(&childFunctions[0], childInstructions, 2, ZR_NULL, 0, ZR_NULL, 0, 0);

        rootInstructions[0] = make_instruction_return(1);
        init_manual_test_function(&rootFunction,
                                  rootInstructions,
                                  1,
                                  ZR_NULL,
                                  0,
                                  childFunctions,
                                  (TZrUInt32)(sizeof(childFunctions) / sizeof(childFunctions[0])),
                                  2);

        runtimeState = aot_runtime_test_install_project_record(state, &project, &rootFunction, ZR_AOT_BACKEND_KIND_C);
        TEST_ASSERT_NOT_NULL(runtimeState);
        TEST_ASSERT_EQUAL_UINT32(2u, runtimeState->records[0].functionCount);
        TEST_ASSERT_EQUAL_UINT32(6u, runtimeState->records[0].generatedFrameSlotCounts[1]);

        descriptor.functionThunks = functionThunks;
        descriptor.functionThunkCount = 2;
        runtimeState->records[0].descriptor = &descriptor;

        nativeClosure = ZrCore_ClosureNative_New(state, 0);
        TEST_ASSERT_NOT_NULL(nativeClosure);
        nativeClosure->aotShimFunction = &rootFunction;

        functionBase = state->stackTop.valuePointer;
        functionBase = ZrCore_Function_CheckStackAndGc(state, 3, functionBase);
        TEST_ASSERT_NOT_NULL(functionBase);
        functionBaseValue = ZrCore_Stack_GetValue(functionBase);
        slot0Value = ZrCore_Stack_GetValue(functionBase + 1);
        slot1Value = ZrCore_Stack_GetValue(functionBase + 2);
        TEST_ASSERT_NOT_NULL(functionBaseValue);
        TEST_ASSERT_NOT_NULL(slot0Value);
        TEST_ASSERT_NOT_NULL(slot1Value);
        ZrCore_Value_InitAsRawObject(state, functionBaseValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));
        ZrCore_Value_InitAsInt(state, slot0Value, 41);
        ZrCore_Value_InitAsInt(state, slot1Value, 99);

        memset(&state->baseCallInfo, 0, sizeof(state->baseCallInfo));
        state->baseCallInfo.functionBase.valuePointer = functionBase;
        state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
        state->callInfoList = &state->baseCallInfo;
        state->stackTop.valuePointer = functionBase + 3;

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginGeneratedFunction(state, 0, &frame));

        childFunctions[0].instructionsLength = 0;

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_PrepareStaticDirectCall(state, &frame, 1, 0, 0, 1, &directCall));
        TEST_ASSERT_TRUE(directCall.prepared);
        TEST_ASSERT_EQUAL_UINT32(1u, directCall.calleeFunctionIndex);
        TEST_ASSERT_EQUAL_PTR(frame.slotBase, directCall.calleeCallInfo->functionBase.valuePointer);
        TEST_ASSERT_EQUAL_PTR(frame.slotBase + 7, directCall.calleeCallInfo->functionTop.valuePointer);
        TEST_ASSERT_EQUAL_PTR(frame.slotBase + 1, directCall.calleeCallInfo->returnDestination);
        TEST_ASSERT_EQUAL_PTR(childInstructions, directCall.calleeCallInfo->context.context.programCounter);
        TEST_ASSERT_EQUAL_PTR(frame.slotBase + 1, state->stackTop.valuePointer);

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_FinishDirectCall(state, &frame, &directCall, 0));
        TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
        TEST_ASSERT_EQUAL_PTR(functionBase + 1, frame.slotBase);

        aot_runtime_test_remove_project_record(state, &project);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_runtime_super_array_add_int_allows_ignored_destination_slot(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Runtime SuperArrayAddInt Allows Ignored Destination Slot";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot runtime ignored add destination",
                 "Testing that SUPER_ARRAY_ADD_INT accepts ZR_INSTRUCTION_USE_RET_FLAG for ignored add-call results and still appends the int payload into the typed array");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var container = %import(\"zr.container\");\n"
                "return new container.Array<int>();\n";
        SZrString *sourceName;
        SZrFunction *factoryFunction;
        SZrTypeValue receiverValue;
        ZrLibTempValueRoot receiverRoot;
        SZrFunction function;
        TZrInstruction instructions[1];
        SZrLibrary_Project project;
        ZrAotGeneratedFrame frame;
        TZrStackValuePointer functionBase;
        SZrTypeValue *functionBaseValue;
        SZrTypeValue *slot0Value;
        SZrTypeValue *slot1Value;
        SZrClosureNative *nativeClosure;
        SZrTypeValue keyValue;
        SZrTypeValue elementValue;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(ZrVmLibContainer_Register(state->global));

        memset(&receiverValue, 0, sizeof(receiverValue));
        memset(&receiverRoot, 0, sizeof(receiverRoot));
        memset(&function, 0, sizeof(function));
        memset(instructions, 0, sizeof(instructions));
        memset(&project, 0, sizeof(project));
        memset(&frame, 0, sizeof(frame));
        memset(&keyValue, 0, sizeof(keyValue));
        memset(&elementValue, 0, sizeof(elementValue));

        sourceName = ZR_STRING_LITERAL(state, "aot_runtime_super_array_add_int_ret_flag_factory.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        factoryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(factoryFunction);

        TEST_ASSERT_TRUE(ZrLib_TempValueRoot_Begin(state, &receiverRoot));
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, factoryFunction, &receiverValue));
        TEST_ASSERT_TRUE(ZrLib_TempValueRoot_SetValue(&receiverRoot, &receiverValue));
        TEST_ASSERT_NOT_NULL(ZrLib_TempValueRoot_Value(&receiverRoot));

        instructions[0] = make_instruction_return(1);
        init_manual_test_function(&function, instructions, 1, ZR_NULL, 0, ZR_NULL, 0, 2);
        aot_runtime_test_install_project_record(state, &project, &function, ZR_AOT_BACKEND_KIND_C);

        nativeClosure = ZrCore_ClosureNative_New(state, 0);
        TEST_ASSERT_NOT_NULL(nativeClosure);
        nativeClosure->aotShimFunction = &function;

        functionBase = state->stackTop.valuePointer;
        functionBase = ZrCore_Function_CheckStackAndGc(state, 3, functionBase);
        TEST_ASSERT_NOT_NULL(functionBase);
        functionBaseValue = ZrCore_Stack_GetValue(functionBase);
        slot0Value = ZrCore_Stack_GetValue(functionBase + 1);
        slot1Value = ZrCore_Stack_GetValue(functionBase + 2);
        TEST_ASSERT_NOT_NULL(functionBaseValue);
        TEST_ASSERT_NOT_NULL(slot0Value);
        TEST_ASSERT_NOT_NULL(slot1Value);
        ZrCore_Value_InitAsRawObject(state, functionBaseValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));
        ZrCore_Value_Copy(state, slot0Value, ZrLib_TempValueRoot_Value(&receiverRoot));
        ZrCore_Value_InitAsInt(state, slot1Value, 7);

        memset(&state->baseCallInfo, 0, sizeof(state->baseCallInfo));
        state->baseCallInfo.functionBase.valuePointer = functionBase;
        state->baseCallInfo.functionTop.valuePointer = functionBase + 3;
        state->callInfoList = &state->baseCallInfo;
        state->stackTop.valuePointer = functionBase + 3;

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginGeneratedFunction(state, 0, &frame));
        TEST_ASSERT_TRUE(
                ZrLibrary_AotRuntime_SuperArrayAddInt(state, &frame, ZR_INSTRUCTION_USE_RET_FLAG, 0, 1));

        ZrCore_Value_InitAsInt(state, slot1Value, 9);
        TEST_ASSERT_TRUE(
                ZrLibrary_AotRuntime_SuperArrayAddInt(state, &frame, ZR_INSTRUCTION_USE_RET_FLAG, 0, 1));

        ZrCore_Value_InitAsInt(state, &keyValue, 0);
        ZrCore_Value_ResetAsNull(&elementValue);
        TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayGetInt(state, slot0Value, &keyValue, &elementValue));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(elementValue.type));
        TEST_ASSERT_EQUAL_INT64(7, elementValue.value.nativeObject.nativeInt64);

        ZrCore_Value_InitAsInt(state, &keyValue, 1);
        ZrCore_Value_ResetAsNull(&elementValue);
        TEST_ASSERT_TRUE(ZrCore_Object_SuperArrayGetInt(state, slot0Value, &keyValue, &elementValue));
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(elementValue.type));
        TEST_ASSERT_EQUAL_INT64(9, elementValue.value.nativeObject.nativeInt64);

        aot_runtime_test_remove_project_record(state, &project);
        ZrLib_TempValueRoot_End(&receiverRoot);
        ZrCore_Function_Free(state, factoryFunction);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_runtime_index_helpers_refresh_frame_for_native_binding_paths(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Runtime Index Helpers Refresh Frame For Native Binding Paths";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot runtime native-binding index helpers",
                 "Testing that GET_BY_INDEX and SET_BY_INDEX refresh the generated frame and restore stack-top semantics before native-binding map index paths allocate temp roots");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var container = %import(\"zr.container\");\n"
                "return new container.Map<string, int>();\n";
        SZrString *sourceName;
        SZrFunction *factoryFunction;
        SZrTypeValue mapValue;
        ZrLibTempValueRoot mapRoot;
        SZrFunction function;
        TZrInstruction instructions[1];
        SZrLibrary_Project project;
        ZrAotGeneratedFrame frame;
        TZrStackValuePointer functionBase;
        SZrTypeValue *functionBaseValue;
        SZrTypeValue *slot0Value;
        SZrTypeValue *slot1Value;
        SZrTypeValue *slot2Value;
        SZrClosureNative *nativeClosure;
        SZrString *keyString;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(ZrVmLibContainer_Register(state->global));

        memset(&mapValue, 0, sizeof(mapValue));
        memset(&mapRoot, 0, sizeof(mapRoot));
        memset(&function, 0, sizeof(function));
        memset(instructions, 0, sizeof(instructions));
        memset(&project, 0, sizeof(project));
        memset(&frame, 0, sizeof(frame));

        sourceName = ZR_STRING_LITERAL(state, "aot_runtime_map_index_helper_factory.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        factoryFunction = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(factoryFunction);

        TEST_ASSERT_TRUE(ZrLib_TempValueRoot_Begin(state, &mapRoot));
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_Execute(state, factoryFunction, &mapValue));
        TEST_ASSERT_TRUE(ZrLib_TempValueRoot_SetValue(&mapRoot, &mapValue));
        TEST_ASSERT_NOT_NULL(ZrLib_TempValueRoot_Value(&mapRoot));

        instructions[0] = make_instruction_return(2);
        init_manual_test_function(&function, instructions, 1, ZR_NULL, 0, ZR_NULL, 0, 3);
        aot_runtime_test_install_project_record(state, &project, &function, ZR_AOT_BACKEND_KIND_C);

        nativeClosure = ZrCore_ClosureNative_New(state, 0);
        TEST_ASSERT_NOT_NULL(nativeClosure);
        nativeClosure->aotShimFunction = &function;

        functionBase = state->stackTop.valuePointer;
        functionBase = ZrCore_Function_CheckStackAndGc(state, 4, functionBase);
        TEST_ASSERT_NOT_NULL(functionBase);
        functionBaseValue = ZrCore_Stack_GetValue(functionBase);
        slot0Value = ZrCore_Stack_GetValue(functionBase + 1);
        slot1Value = ZrCore_Stack_GetValue(functionBase + 2);
        slot2Value = ZrCore_Stack_GetValue(functionBase + 3);
        TEST_ASSERT_NOT_NULL(functionBaseValue);
        TEST_ASSERT_NOT_NULL(slot0Value);
        TEST_ASSERT_NOT_NULL(slot1Value);
        TEST_ASSERT_NOT_NULL(slot2Value);

        keyString = ZR_STRING_LITERAL(state, "alpha");
        TEST_ASSERT_NOT_NULL(keyString);

        ZrCore_Value_InitAsRawObject(state, functionBaseValue, ZR_CAST_RAW_OBJECT_AS_SUPER(nativeClosure));
        ZrCore_Value_Copy(state, slot0Value, ZrLib_TempValueRoot_Value(&mapRoot));
        ZrCore_Value_InitAsRawObject(state, slot1Value, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
        slot1Value->type = ZR_VALUE_TYPE_STRING;
        ZrCore_Value_InitAsInt(state, slot2Value, 41);

        memset(&state->baseCallInfo, 0, sizeof(state->baseCallInfo));
        state->baseCallInfo.functionBase.valuePointer = functionBase;
        state->baseCallInfo.functionTop.valuePointer = functionBase + 4;
        state->callInfoList = &state->baseCallInfo;
        state->stackTop.valuePointer = functionBase + 4;

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginGeneratedFunction(state, 0, &frame));
        TEST_ASSERT_EQUAL_PTR(functionBase + 1, frame.slotBase);
        TEST_ASSERT_EQUAL_PTR(functionBase + 4, state->stackTop.valuePointer);

        state->stackTop.valuePointer = functionBase + 2;
        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_SetByIndex(state, &frame, 2, 0, 1));
        TEST_ASSERT_NOT_NULL(frame.callInfo);
        TEST_ASSERT_EQUAL_PTR(frame.callInfo->functionBase.valuePointer + 1, frame.slotBase);
        TEST_ASSERT_EQUAL_PTR(frame.callInfo->functionTop.valuePointer, state->stackTop.valuePointer);
        TEST_ASSERT_TRUE(frame.callInfo->functionTop.valuePointer >= functionBase + 4);

        ZrCore_Value_ResetAsNull(slot2Value);
        state->stackTop.valuePointer = functionBase + 2;
        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_GetByIndex(state, &frame, 2, 0, 1));
        TEST_ASSERT_NOT_NULL(frame.callInfo);
        TEST_ASSERT_EQUAL_PTR(frame.callInfo->functionBase.valuePointer + 1, frame.slotBase);
        TEST_ASSERT_EQUAL_PTR(frame.callInfo->functionTop.valuePointer, state->stackTop.valuePointer);
        TEST_ASSERT_TRUE(frame.callInfo->functionTop.valuePointer >= functionBase + 4);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_SIGNED_INT(slot2Value->type));
        TEST_ASSERT_EQUAL_INT64(41, slot2Value->value.nativeObject.nativeInt64);

        aot_runtime_test_remove_project_record(state, &project);
        ZrLib_TempValueRoot_End(&mapRoot);
        ZrCore_Function_Free(state, factoryFunction);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_runtime_begin_instruction_respects_resolved_observation_policy(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Runtime BeginInstruction Respects Resolved Observation Policy";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot runtime instruction observation policy",
                 "Testing that generated AOT instruction observation updates the VM-visible program counter only for selected steps unless publish-all is enabled");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        TZrInstruction instructions[4];
        SZrFunction function;
        ZrAotGeneratedFrame frame;
        SZrCallInfo callInfo;
        TZrUInt32 mask = 0;
        TZrBool publishAll = ZR_FALSE;

        TEST_ASSERT_NOT_NULL(state);

        memset(instructions, 0, sizeof(instructions));
        memset(&function, 0, sizeof(function));
        memset(&frame, 0, sizeof(frame));
        memset(&callInfo, 0, sizeof(callInfo));

        function.instructionsList = instructions;
        function.instructionsLength = (TZrUInt32)(sizeof(instructions) / sizeof(instructions[0]));

        callInfo.context.context.programCounter = function.instructionsList;
        state->callInfoList = &callInfo;

        frame.function = &function;
        frame.callInfo = &callInfo;
        frame.lastObservedInstructionIndex = UINT32_MAX;

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_SetObservationPolicy(state,
                                                                   ZR_AOT_GENERATED_STEP_FLAG_CALL,
                                                                   ZR_FALSE));
        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_GetObservationPolicy(state, &mask, &publishAll));
        frame.observationMask = mask;
        frame.publishAllInstructions = publishAll;

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginInstruction(state,
                                                               &frame,
                                                               1,
                                                               ZR_AOT_GENERATED_STEP_FLAG_NONE));
        TEST_ASSERT_EQUAL_UINT32(1u, frame.currentInstructionIndex);
        TEST_ASSERT_EQUAL_PTR(function.instructionsList, callInfo.context.context.programCounter);
        TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, frame.lastObservedInstructionIndex);

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginInstruction(state,
                                                               &frame,
                                                               2,
                                                               ZR_AOT_GENERATED_STEP_FLAG_CALL));
        TEST_ASSERT_EQUAL_PTR(function.instructionsList + 2, callInfo.context.context.programCounter);
        TEST_ASSERT_EQUAL_UINT32(2, frame.lastObservedInstructionIndex);

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_SetObservationPolicy(state,
                                                                   ZR_AOT_GENERATED_STEP_FLAG_NONE,
                                                                   ZR_TRUE));
        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_GetObservationPolicy(state, &mask, &publishAll));
        frame.observationMask = mask;
        frame.publishAllInstructions = publishAll;

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginInstruction(state,
                                                               &frame,
                                                               3,
                                                               ZR_AOT_GENERATED_STEP_FLAG_NONE));
        TEST_ASSERT_EQUAL_PTR(function.instructionsList + 3, callInfo.context.context.programCounter);
        TEST_ASSERT_EQUAL_UINT32(3, frame.lastObservedInstructionIndex);

        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_runtime_begin_instruction_publishes_line_debug_events(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Runtime BeginInstruction Publishes Line Debug Events";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot runtime line debug publishing",
                 "Testing that generated AOT instruction observation emits VM line-debug hooks when line stepping is enabled and only reports line transitions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        TZrInstruction instructions[4];
        SZrFunctionExecutionLocationInfo locations[4];
        SZrFunction function;
        ZrAotGeneratedFrame frame;
        TZrUInt32 mask = 0;
        TZrBool publishAll = ZR_FALSE;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(state->callInfoList);

        memset(instructions, 0, sizeof(instructions));
        memset(locations, 0, sizeof(locations));
        memset(&function, 0, sizeof(function));
        memset(&frame, 0, sizeof(frame));

        function.instructionsList = instructions;
        function.instructionsLength = (TZrUInt32)(sizeof(instructions) / sizeof(instructions[0]));
        function.executionLocationInfoList = locations;
        function.executionLocationInfoLength = (TZrUInt32)(sizeof(locations) / sizeof(locations[0]));
        locations[0].currentInstructionOffset = 0;
        locations[0].lineInSource = 10;
        locations[1].currentInstructionOffset = 1;
        locations[1].lineInSource = 10;
        locations[2].currentInstructionOffset = 2;
        locations[2].lineInSource = 11;
        locations[3].currentInstructionOffset = 3;
        locations[3].lineInSource = 11;

        state->debugHook = aot_debug_hook_capture;
        state->debugHookSignal = ZR_DEBUG_HOOK_MASK_LINE;
        aot_debug_hook_capture_reset();

        frame.function = &function;
        frame.callInfo = state->callInfoList;
        frame.lastObservedInstructionIndex = UINT32_MAX;

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_GetObservationPolicy(state, &mask, &publishAll));
        TEST_ASSERT_TRUE(publishAll);
        frame.observationMask = mask;
        frame.publishAllInstructions = publishAll;

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginInstruction(state,
                                                               &frame,
                                                               0,
                                                               ZR_AOT_GENERATED_STEP_FLAG_NONE));
        TEST_ASSERT_EQUAL_UINT32(1, g_aotDebugHookCapture.eventCount);
        TEST_ASSERT_EQUAL_UINT32(10, g_aotDebugHookCapture.lastLine);

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginInstruction(state,
                                                               &frame,
                                                               1,
                                                               ZR_AOT_GENERATED_STEP_FLAG_NONE));
        TEST_ASSERT_EQUAL_UINT32(1, g_aotDebugHookCapture.eventCount);

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginInstruction(state,
                                                               &frame,
                                                               2,
                                                               ZR_AOT_GENERATED_STEP_FLAG_NONE));
        TEST_ASSERT_EQUAL_UINT32(2, g_aotDebugHookCapture.eventCount);
        TEST_ASSERT_EQUAL_UINT32(11, g_aotDebugHookCapture.lastLine);

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginInstruction(state,
                                                               &frame,
                                                               3,
                                                               ZR_AOT_GENERATED_STEP_FLAG_NONE));
        TEST_ASSERT_EQUAL_UINT32(2, g_aotDebugHookCapture.eventCount);

        state->debugHook = ZR_NULL;
        state->debugHookSignal = 0;
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_runtime_begin_instruction_honors_dynamic_line_signal_enablement(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Runtime BeginInstruction Honors Dynamic Line Signal Enablement";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot runtime dynamic line signal",
                 "Testing that generated AOT observation reacts when line-debug stepping is enabled after the frame has already been entered");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        TZrInstruction instructions[2];
        SZrFunctionExecutionLocationInfo locations[2];
        SZrFunction function;
        ZrAotGeneratedFrame frame;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(state->callInfoList);

        memset(instructions, 0, sizeof(instructions));
        memset(locations, 0, sizeof(locations));
        memset(&function, 0, sizeof(function));
        memset(&frame, 0, sizeof(frame));

        function.instructionsList = instructions;
        function.instructionsLength = 2;
        function.executionLocationInfoList = locations;
        function.executionLocationInfoLength = 2;
        locations[0].currentInstructionOffset = 0;
        locations[0].lineInSource = 20;
        locations[1].currentInstructionOffset = 1;
        locations[1].lineInSource = 21;

        frame.function = &function;
        frame.callInfo = state->callInfoList;
        frame.lastObservedInstructionIndex = UINT32_MAX;
        frame.lastObservedLine = ZR_RUNTIME_DEBUG_HOOK_LINE_NONE;
        frame.observationMask = ZR_AOT_GENERATED_STEP_FLAG_NONE;
        frame.publishAllInstructions = ZR_FALSE;

        state->debugHook = aot_debug_hook_capture;
        state->debugHookSignal = 0;
        aot_debug_hook_capture_reset();
        state->callInfoList->context.context.programCounter = function.instructionsList;

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginInstruction(state,
                                                               &frame,
                                                               0,
                                                               ZR_AOT_GENERATED_STEP_FLAG_NONE));
        TEST_ASSERT_EQUAL_UINT32(0, g_aotDebugHookCapture.eventCount);
        TEST_ASSERT_EQUAL_PTR(function.instructionsList, state->callInfoList->context.context.programCounter);

        state->debugHookSignal = ZR_DEBUG_HOOK_MASK_LINE;
        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_BeginInstruction(state,
                                                               &frame,
                                                               1,
                                                               ZR_AOT_GENERATED_STEP_FLAG_NONE));
        TEST_ASSERT_EQUAL_UINT32(1, g_aotDebugHookCapture.eventCount);
        TEST_ASSERT_EQUAL_UINT32(21, g_aotDebugHookCapture.lastLine);
        TEST_ASSERT_EQUAL_PTR(function.instructionsList + 1, state->callInfoList->context.context.programCounter);

        state->debugHook = ZR_NULL;
        state->debugHookSignal = 0;
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_lowers_indexed_entry_access_even_with_embedded_blob(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Lowers Indexed Entry Access Even With Embedded Blob";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c strict entry lowering",
                 "Testing that indexed entry access now lowers under embedded strict AOT C without falling back to shim-based execution");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var items = [7, 8];\n"
                "return items[0];";
        const char *cPath = "execbc_aot_root_shim_fallback_test.c";
        const char *binaryPath = "execbc_aot_root_shim_fallback_test.zro";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_root_shim_fallback_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(CREATE_ARRAY)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_embedded_aot_c_file(state,
                                                   func,
                                                   "execbc_aot_root_shim_fallback_test",
                                                   cPath,
                                                   binaryPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_GetByIndex"));
        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_lowers_indexed_entry_access_without_embedded_blob(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Lowers Indexed Entry Access Without Embedded Blob";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c strict standalone entry lowering",
                 "Testing that standalone strict AOT C generation lowers indexed entry access without requiring an embedded blob");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var items = [7, 8];\n"
                "return items[0];";
        const char *cPath = "execbc_aot_root_shim_fallback_test.c";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_root_shim_fallback_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(CREATE_ARRAY)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_root_shim_fallback_test",
                                                            cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_GetByIndex"));
        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_lowers_indexed_child_access_even_with_embedded_blob(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Lowers Indexed Child Access Even With Embedded Blob";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c strict child lowering",
                 "Testing that indexed child functions now lower under embedded strict AOT C without falling back to shim-based execution");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var build = () => {\n"
                "    var items = [9, 8];\n"
                "    return items[0];\n"
                "};\n"
                "return build();";
        const char *cPath = "execbc_aot_child_shim_fallback_test.c";
        const char *binaryPath = "execbc_aot_child_shim_fallback_test.zro";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_child_shim_fallback_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_embedded_aot_c_file(state,
                                                   func,
                                                   "execbc_aot_child_shim_fallback_test",
                                                   cPath,
                                                   binaryPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_GetByIndex"));
        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_c_backend_lowers_indexed_child_access_without_embedded_blob(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Lowers Indexed Child Access Without Embedded Blob";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c strict standalone child lowering",
                 "Testing that standalone strict AOT C generation lowers indexed child functions without requiring an embedded blob");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var build = () => {\n"
                "    var items = [9, 8];\n"
                "    return items[0];\n"
                "};\n"
                "return build();";
        const char *cPath = "execbc_aot_child_shim_fallback_test.c";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *cText;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_aot_child_shim_fallback_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_child_shim_fallback_test",
                                                            cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_GetByIndex"));
        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_checked_in_aot_c_fixtures_use_new_code_shape(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Checked In AOT C Fixtures Use New Code Shape";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("checked-in aot c fixtures",
                 "Testing that the committed AOT C fixtures no longer keep legacy prepared-branch, goto-fail, return-0, or shim-entry code shape");

    {
        const char *projects[] = {
                "hello_world",
                "import_basic",
                "aot_module_graph_pipeline",
                "decorator_compile_time",
                "decorator_compile_time_import",
                "decorator_compile_time_deep_import"};
        const char *relativePath = "bin/aot_c/src/main.c";
        TZrUInt32 index;

        for (index = 0; index < (TZrUInt32)(sizeof(projects) / sizeof(projects[0])); index++) {
            char *cText;
            TZrChar cPath[ZR_TESTS_PATH_MAX];
            TZrSize cTextLength = 0;

            TEST_ASSERT_TRUE(ZrTests_Path_GetProjectFile(projects[index], relativePath, cPath, sizeof(cPath)));
            cText = ZrTests_ReadTextFile(cPath, &cTextLength);
            TEST_ASSERT_NOT_NULL(cText);
            TEST_ASSERT_TRUE(cTextLength > 0);
            assert_generated_aot_c_text_uses_new_code_shape(cText);
            free(cText);
        }
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_checked_in_aot_llvm_fixtures_use_true_backend_shape(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Checked In AOT LLVM Fixtures Use True Backend Shape";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("checked-in aot llvm fixtures",
                 "Testing that the committed AOT LLVM fixtures no longer keep shim-entry wrappers and instead commit real generated function bodies plus thunk tables");

    {
        static const struct {
            const char *project;
            const char *relativePath;
        } fixtureFiles[] = {
                {"hello_world", "bin/aot_llvm/ir/main.ll"},
                {"import_basic", "bin/aot_llvm/ir/main.ll"},
                {"import_basic", "bin/aot_llvm/ir/greet.ll"},
                {"decorator_compile_time_import", "bin/aot_llvm/ir/main.ll"},
                {"decorator_compile_time_import", "bin/aot_llvm/ir/decorators.ll"},
                {"decorator_compile_time_import", "bin/aot_llvm/ir/decorated_user.ll"},
                {"aot_module_graph_pipeline", "bin/aot_llvm/ir/main.ll"},
                {"aot_module_graph_pipeline", "bin/aot_llvm/ir/graph_stage_a.ll"},
                {"aot_module_graph_pipeline", "bin/aot_llvm/ir/graph_stage_b.ll"},
                {"aot_module_graph_pipeline", "bin/aot_llvm/ir/graph_binary_stage.ll"},
                {"aot_dynamic_meta_ownership_lab", "bin/aot_llvm/ir/main.ll"},
                {"aot_eh_tail_gc_stress", "bin/aot_llvm/ir/main.ll"}};
        TZrUInt32 index;

        for (index = 0; index < (TZrUInt32)(sizeof(fixtureFiles) / sizeof(fixtureFiles[0])); index++) {
            char *llvmText;
            TZrChar llvmPath[ZR_TESTS_PATH_MAX];
            TZrSize llvmTextLength = 0;

            TEST_ASSERT_TRUE(ZrTests_Path_GetProjectFile(fixtureFiles[index].project,
                                                         fixtureFiles[index].relativePath,
                                                         llvmPath,
                                                         sizeof(llvmPath)));
            llvmText = ZrTests_ReadTextFile(llvmPath, &llvmTextLength);
            TEST_ASSERT_NOT_NULL(llvmText);
            TEST_ASSERT_TRUE(llvmTextLength > 0);
            assert_generated_aot_llvm_text_uses_true_backend_shape(llvmText);
            TEST_ASSERT_NOT_NULL(strstr(llvmText, "@zr_aot_function_thunks = private constant"));
            TEST_ASSERT_NOT_NULL(strstr(llvmText, "define i64 @zr_aot_entry(ptr %state)"));
            free(llvmText);
        }
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_promotes_exec_ir_to_per_function_cfg_and_frame_layout(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Backend AOT Source Split Promotes ExecIR To Per Function CFG And Frame Layout";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot execir structuralization",
                 "Testing that backend_aot_exec_ir now carries per-function CFG, basic-block, and frame-layout records instead of only a flat instruction list");

    {
        char *execIrHeaderText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h");
        char *execIrSourceText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c");

        TEST_ASSERT_NOT_NULL(execIrHeaderText);
        TEST_ASSERT_NOT_NULL(execIrSourceText);

        TEST_ASSERT_NOT_NULL(strstr(execIrHeaderText, "typedef struct SZrAotExecIrBasicBlock"));
        TEST_ASSERT_NOT_NULL(strstr(execIrHeaderText, "typedef struct SZrAotExecIrFrameLayout"));
        TEST_ASSERT_NOT_NULL(strstr(execIrHeaderText, "typedef struct SZrAotExecIrFunction"));
        TEST_ASSERT_NOT_NULL(strstr(execIrHeaderText, "SZrAotExecIrFunction *functions;"));
        TEST_ASSERT_NOT_NULL(strstr(execIrHeaderText, "TZrUInt32 functionCount;"));
        TEST_ASSERT_NOT_NULL(strstr(execIrHeaderText, "SZrAotExecIrBasicBlock *basicBlocks;"));
        TEST_ASSERT_NOT_NULL(strstr(execIrHeaderText, "TZrUInt32 basicBlockCount;"));
        TEST_ASSERT_NOT_NULL(strstr(execIrHeaderText, "SZrAotExecIrFrameLayout frameLayout;"));

        TEST_ASSERT_NOT_NULL(strstr(execIrSourceText, "backend_aot_exec_ir_build_function("));
        TEST_ASSERT_NOT_NULL(strstr(execIrSourceText, "backend_aot_exec_ir_build_basic_blocks("));
        TEST_ASSERT_NOT_NULL(strstr(execIrSourceText, "backend_aot_exec_ir_build_frame_layout("));
        TEST_ASSERT_NOT_NULL(strstr(execIrSourceText, "backend_aot_exec_ir_find_block_successors("));
        TEST_ASSERT_NOT_NULL(strstr(execIrSourceText, "module->functions"));

        free(execIrHeaderText);
        free(execIrSourceText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_runtime_source_removes_backend_specific_true_aot_descriptor_gate(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Runtime Source Removes Backend Specific True AOT Descriptor Gate";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot runtime descriptor strictness",
                 "Testing that runtime descriptor strict validation for true AOT payload no longer special-cases C only, so LLVM must satisfy the same embedded-module and thunk-table contract");

    {
        char *runtimeText = read_repo_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");

        TEST_ASSERT_NOT_NULL(runtimeText);
        TEST_ASSERT_NULL(strstr(runtimeText, "if (backendKind == ZR_AOT_BACKEND_KIND_C &&"));
        TEST_ASSERT_NOT_NULL(strstr(runtimeText, "aot_runtime_descriptor_has_true_aot_payload("));
        TEST_ASSERT_NOT_NULL(strstr(runtimeText, "descriptor->embeddedModuleBlob == ZR_NULL"));
        TEST_ASSERT_NOT_NULL(strstr(runtimeText, "descriptor->functionThunks == ZR_NULL"));

        free(runtimeText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_llvm_emitter_out_of_backend_aot_c(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Backend AOT Source Split Moves LLVM Emitter Out Of Backend AOT C";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot llvm modularization",
                 "Testing that LLVM emitter helpers and function-body emission live in a dedicated backend_aot_llvm_emitter module instead of remaining implemented inside backend_aot.c");

    {
        char *backendAotText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c");
        char *llvmEmitterText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_emitter.c");
        char *llvmFunctionBodyText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_function_body.c");
        char *llvmFunctionBodyHeaderText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_function_body.h");

        TEST_ASSERT_NOT_NULL(backendAotText);
        TEST_ASSERT_NOT_NULL(llvmEmitterText);
        TEST_ASSERT_NOT_NULL(llvmFunctionBodyText);
        TEST_ASSERT_NOT_NULL(llvmFunctionBodyHeaderText);

        TEST_ASSERT_NULL(strstr(backendAotText, "#include \"backend_aot_llvm_emitter.h\""));
        TEST_ASSERT_NULL(strstr(backendAotText, "static void backend_aot_write_llvm_function_body("));
        TEST_ASSERT_NULL(strstr(backendAotText, "static void backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NULL(strstr(llvmEmitterText, "void backend_aot_write_llvm_function_body("));
        TEST_ASSERT_NOT_NULL(strstr(llvmFunctionBodyText, "void backend_aot_write_llvm_function_body("));
        TEST_ASSERT_NOT_NULL(strstr(llvmFunctionBodyHeaderText, "void backend_aot_write_llvm_function_body("));

        free(backendAotText);
        free(llvmEmitterText);
        free(llvmFunctionBodyText);
        free(llvmFunctionBodyHeaderText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_backend_writer_entrypoints_out_of_backend_aot_c(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Backend AOT Source Split Moves Backend Writer Entrypoints Out Of Backend AOT C";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot writer modularization",
                 "Testing that backend_aot.c no longer defines public AOT C/LLVM writer entrypoints and that backend-specific modules own those writer surfaces");

    {
        char *backendAotText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c");
        char *cEmitterText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
        char *llvmEmitterText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_emitter.c");

        TEST_ASSERT_NOT_NULL(backendAotText);
        TEST_ASSERT_NOT_NULL(cEmitterText);
        TEST_ASSERT_NOT_NULL(llvmEmitterText);

        TEST_ASSERT_NULL(strstr(backendAotText, "ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFileWithOptions("));
        TEST_ASSERT_NULL(strstr(backendAotText, "ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFile("));
        TEST_ASSERT_NULL(strstr(backendAotText, "ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFileWithOptions("));
        TEST_ASSERT_NULL(strstr(backendAotText, "ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFile("));
        TEST_ASSERT_NULL(strstr(backendAotText, "static void backend_aot_write_c_function_forward_decls("));
        TEST_ASSERT_NULL(strstr(backendAotText, "static void backend_aot_write_c_guard_macro("));
        TEST_ASSERT_NULL(strstr(backendAotText, "static void backend_aot_write_embedded_blob_c("));
        TEST_ASSERT_NULL(strstr(backendAotText, "void backend_aot_write_c_function_body("));
        TEST_ASSERT_NOT_NULL(strstr(cEmitterText, "ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFileWithOptions("));
        TEST_ASSERT_NOT_NULL(strstr(cEmitterText, "ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotCFile("));
        TEST_ASSERT_NOT_NULL(strstr(cEmitterText, "static void backend_aot_write_c_function_forward_decls("));
        TEST_ASSERT_NOT_NULL(strstr(cEmitterText, "static void backend_aot_write_c_guard_macro("));
        TEST_ASSERT_NOT_NULL(strstr(cEmitterText, "static void backend_aot_write_embedded_blob_c("));
        TEST_ASSERT_NULL(strstr(cEmitterText, "static void backend_aot_write_c_function_body("));
        TEST_ASSERT_NOT_NULL(strstr(llvmEmitterText, "ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFileWithOptions("));
        TEST_ASSERT_NOT_NULL(strstr(llvmEmitterText, "ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFile("));

        free(backendAotText);
        free(cEmitterText);
        free(llvmEmitterText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_shared_function_table_and_callable_provenance_out_of_backend_aot_c(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Backend AOT Source Split Moves Shared Function Table And Callable Provenance Out Of Backend AOT C";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot shared util modularization",
                 "Testing that backend_aot.c no longer owns c-named function-table or callable-provenance helpers and that neutral shared util modules own those boundaries");

    {
        char *backendAotText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c");
        char *internalHeaderText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_internal.h");
        char *functionTableText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.c");
        char *functionTableHeaderText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.h");
        char *callableProvenanceText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_callable_provenance.c");
        char *callableProvenanceHeaderText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_callable_provenance.h");

        TEST_ASSERT_NOT_NULL(backendAotText);
        TEST_ASSERT_NOT_NULL(internalHeaderText);
        TEST_ASSERT_NOT_NULL(functionTableText);
        TEST_ASSERT_NOT_NULL(functionTableHeaderText);
        TEST_ASSERT_NOT_NULL(callableProvenanceText);
        TEST_ASSERT_NOT_NULL(callableProvenanceHeaderText);

        TEST_ASSERT_NULL(strstr(backendAotText, "backend_aot_build_c_function_table("));
        TEST_ASSERT_NULL(strstr(backendAotText, "backend_aot_release_c_function_table("));
        TEST_ASSERT_NULL(strstr(backendAotText, "backend_aot_resolve_callable_constant_flat_index("));
        TEST_ASSERT_NULL(strstr(backendAotText, "backend_aot_allocate_callable_slot_provenance("));
        TEST_ASSERT_NULL(strstr(backendAotText, "backend_aot_release_callable_slot_provenance("));
        TEST_ASSERT_NULL(strstr(backendAotText, "backend_aot_get_callable_slot_provenance("));
        TEST_ASSERT_NULL(strstr(backendAotText, "backend_aot_set_callable_slot_provenance("));
        TEST_ASSERT_NULL(strstr(backendAotText, "backend_aot_resolve_callable_slot_before_instruction("));

        TEST_ASSERT_NULL(strstr(internalHeaderText, "typedef struct SZrAotCFunctionEntry"));
        TEST_ASSERT_NULL(strstr(internalHeaderText, "typedef struct SZrAotCFunctionTable"));
        TEST_ASSERT_NULL(strstr(internalHeaderText, "backend_aot_build_c_function_table("));
        TEST_ASSERT_NULL(strstr(internalHeaderText, "backend_aot_release_c_function_table("));
        TEST_ASSERT_NULL(strstr(internalHeaderText, "backend_aot_allocate_callable_slot_provenance("));

        TEST_ASSERT_NOT_NULL(strstr(functionTableHeaderText, "typedef struct SZrAotFunctionEntry"));
        TEST_ASSERT_NOT_NULL(strstr(functionTableHeaderText, "typedef struct SZrAotFunctionTable"));
        TEST_ASSERT_NOT_NULL(strstr(functionTableHeaderText, "backend_aot_build_function_table("));
        TEST_ASSERT_NOT_NULL(strstr(functionTableHeaderText, "backend_aot_release_function_table("));
        TEST_ASSERT_NOT_NULL(strstr(functionTableHeaderText, "backend_aot_resolve_callable_constant_function_index("));
        TEST_ASSERT_NOT_NULL(strstr(functionTableText, "backend_aot_build_function_table("));
        TEST_ASSERT_NOT_NULL(strstr(functionTableText, "backend_aot_release_function_table("));
        TEST_ASSERT_NOT_NULL(strstr(functionTableText, "backend_aot_resolve_callable_constant_function_index("));

        TEST_ASSERT_NOT_NULL(strstr(callableProvenanceHeaderText, "backend_aot_allocate_callable_slot_function_indices("));
        TEST_ASSERT_NOT_NULL(strstr(callableProvenanceHeaderText, "backend_aot_release_callable_slot_function_indices("));
        TEST_ASSERT_NOT_NULL(strstr(callableProvenanceHeaderText, "backend_aot_get_callable_slot_function_index("));
        TEST_ASSERT_NOT_NULL(strstr(callableProvenanceHeaderText, "backend_aot_set_callable_slot_function_index("));
        TEST_ASSERT_NOT_NULL(strstr(callableProvenanceHeaderText, "backend_aot_resolve_callable_slot_function_index_before_instruction("));
        TEST_ASSERT_NOT_NULL(strstr(callableProvenanceText, "backend_aot_allocate_callable_slot_function_indices("));
        TEST_ASSERT_NOT_NULL(strstr(callableProvenanceText, "backend_aot_resolve_callable_slot_function_index_before_instruction("));

        free(backendAotText);
        free(internalHeaderText);
        free(functionTableText);
        free(functionTableHeaderText);
        free(callableProvenanceText);
        free(callableProvenanceHeaderText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_aot_c_lowering_families_out_of_backend_aot_c_emitter(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Backend AOT Source Split Moves AOT C Lowering Families Out Of Backend AOT C Emitter";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot c family modularization",
                 "Testing that backend_aot_c_emitter.c keeps writer orchestration only while direct-lowering families and function-body lowering move into dedicated AOT C modules");

    {
        char *cEmitterText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
        char *cFunctionBodyText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
        char *cFunctionBodyHeaderText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.h");
        char *cLoweringValuesText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c");
        char *cLoweringCallsText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c");
        char *cLoweringControlText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c");

        TEST_ASSERT_NOT_NULL(cEmitterText);
        TEST_ASSERT_NOT_NULL(cFunctionBodyText);
        TEST_ASSERT_NOT_NULL(cFunctionBodyHeaderText);
        TEST_ASSERT_NOT_NULL(cLoweringValuesText);
        TEST_ASSERT_NOT_NULL(cLoweringCallsText);
        TEST_ASSERT_NOT_NULL(cLoweringControlText);

        TEST_ASSERT_NULL(strstr(cEmitterText, "static void backend_aot_write_c_function_body("));
        TEST_ASSERT_NULL(strstr(cEmitterText, "void backend_aot_write_c_direct_meta_get("));
        TEST_ASSERT_NULL(strstr(cEmitterText, "void backend_aot_write_c_direct_mul_signed("));
        TEST_ASSERT_NULL(strstr(cEmitterText, "void backend_aot_write_c_try("));
        TEST_ASSERT_NULL(strstr(cEmitterText, "void backend_aot_write_c_dispatch_loop("));

        TEST_ASSERT_NOT_NULL(strstr(cFunctionBodyHeaderText, "backend_aot_write_c_function_body("));
        TEST_ASSERT_NOT_NULL(strstr(cFunctionBodyText, "backend_aot_write_c_function_body("));
        TEST_ASSERT_NOT_NULL(strstr(cLoweringValuesText, "backend_aot_write_c_direct_meta_get("));
        TEST_ASSERT_NOT_NULL(strstr(cLoweringValuesText, "backend_aot_write_c_direct_mul_signed("));
        TEST_ASSERT_NOT_NULL(strstr(cLoweringCallsText, "backend_aot_write_c_direct_function_call("));
        TEST_ASSERT_NOT_NULL(strstr(cLoweringCallsText, "backend_aot_write_c_static_direct_function_call("));
        TEST_ASSERT_NOT_NULL(strstr(cLoweringControlText, "backend_aot_write_c_dispatch_loop("));
        TEST_ASSERT_NOT_NULL(strstr(cLoweringControlText, "backend_aot_write_c_try("));
        TEST_ASSERT_NOT_NULL(strstr(cLoweringControlText, "backend_aot_write_c_set_pending_continue("));

        free(cEmitterText);
        free(cFunctionBodyText);
        free(cFunctionBodyHeaderText);
        free(cLoweringValuesText);
        free(cLoweringCallsText);
        free(cLoweringControlText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_aot_llvm_lowering_families_out_of_backend_aot_llvm_emitter(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Backend AOT Source Split Moves AOT LLVM Lowering Families Out Of Backend AOT LLVM Emitter";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot llvm family modularization",
                 "Testing that backend_aot_llvm_emitter.c keeps writer orchestration only while LLVM function-body emission and lowering helpers move into dedicated LLVM AOT modules");

    {
        char *llvmEmitterText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_emitter.c");
        char *llvmFunctionBodyText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_function_body.c");
        char *llvmFunctionBodyHeaderText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_function_body.h");
        char *llvmModuleArtifactsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.c");
        char *llvmModuleArtifactsHeaderText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.h");
        char *llvmModulePreludeText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.c");
        char *llvmModulePreludeHeaderText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.h");
        char *llvmLoweringValuesText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_values.c");
        char *llvmLoweringConstantsText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_constants.c");
        char *llvmLoweringClosureSlotsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_closure_slots.c");
        char *llvmLoweringClosuresText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_closures.c");
        char *llvmLoweringStackSlotsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_stack_slots.c");
        char *llvmLoweringObjectMetaOwningText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_object_meta_owning.c");
        char *llvmLoweringObjectsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_objects.c");
        char *llvmLoweringIteratorsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_iterators.c");
        char *llvmLoweringMetaAccessText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_meta_access.c");
        char *llvmLoweringOwnershipText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_ownership.c");
        char *llvmLoweringArithmeticText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_arithmetic.c");
        char *llvmLoweringCallsText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_calls.c");
        char *llvmLoweringFunctionCallsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_function_calls.c");
        char *llvmLoweringMetaCallsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_meta_calls.c");
        char *llvmLoweringControlText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_control.c");
        char *llvmLoweringExceptionControlText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_exception_control.c");
        char *llvmLoweringBranchControlText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_branch_control.c");

        TEST_ASSERT_NOT_NULL(llvmEmitterText);
        TEST_ASSERT_NOT_NULL(llvmFunctionBodyText);
        TEST_ASSERT_NOT_NULL(llvmFunctionBodyHeaderText);
        TEST_ASSERT_NOT_NULL(llvmModuleArtifactsText);
        TEST_ASSERT_NOT_NULL(llvmModuleArtifactsHeaderText);
        TEST_ASSERT_NOT_NULL(llvmModulePreludeText);
        TEST_ASSERT_NOT_NULL(llvmModulePreludeHeaderText);
        TEST_ASSERT_NOT_NULL(llvmLoweringValuesText);
        TEST_ASSERT_NOT_NULL(llvmLoweringConstantsText);
        TEST_ASSERT_NOT_NULL(llvmLoweringClosureSlotsText);
        TEST_ASSERT_NOT_NULL(llvmLoweringClosuresText);
        TEST_ASSERT_NOT_NULL(llvmLoweringStackSlotsText);
        TEST_ASSERT_NOT_NULL(llvmLoweringObjectMetaOwningText);
        TEST_ASSERT_NOT_NULL(llvmLoweringObjectsText);
        TEST_ASSERT_NOT_NULL(llvmLoweringIteratorsText);
        TEST_ASSERT_NOT_NULL(llvmLoweringMetaAccessText);
        TEST_ASSERT_NOT_NULL(llvmLoweringOwnershipText);
        TEST_ASSERT_NOT_NULL(llvmLoweringArithmeticText);
        TEST_ASSERT_NOT_NULL(llvmLoweringCallsText);
        TEST_ASSERT_NOT_NULL(llvmLoweringFunctionCallsText);
        TEST_ASSERT_NOT_NULL(llvmLoweringMetaCallsText);
        TEST_ASSERT_NOT_NULL(llvmLoweringControlText);
        TEST_ASSERT_NOT_NULL(llvmLoweringExceptionControlText);
        TEST_ASSERT_NOT_NULL(llvmLoweringBranchControlText);

        TEST_ASSERT_NOT_NULL(strstr(llvmEmitterText, "ZR_PARSER_API TZrBool ZrParser_Writer_WriteAotLlvmFileWithOptions("));
        TEST_ASSERT_NOT_NULL(strstr(llvmEmitterText, "backend_aot_write_llvm_function_body("));
        TEST_ASSERT_NOT_NULL(strstr(llvmEmitterText, "backend_aot_llvm_write_module_prelude("));
        TEST_ASSERT_NOT_NULL(strstr(llvmEmitterText, "backend_aot_llvm_write_function_thunk_table("));
        TEST_ASSERT_NOT_NULL(strstr(llvmEmitterText, "backend_aot_llvm_write_module_exports("));
        TEST_ASSERT_NULL(strstr(llvmEmitterText, "void backend_aot_write_llvm_function_body("));
        TEST_ASSERT_NULL(strstr(llvmEmitterText, "static void backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NULL(strstr(llvmEmitterText, "static void backend_aot_llvm_write_return_call("));
        TEST_ASSERT_NULL(strstr(llvmEmitterText, "void backend_aot_write_llvm_contracts("));
        TEST_ASSERT_NULL(strstr(llvmEmitterText, "void backend_aot_write_llvm_runtime_helper_decls("));
        TEST_ASSERT_NULL(strstr(llvmEmitterText, "@zr_aot_module = private constant"));
        TEST_ASSERT_NULL(strstr(llvmEmitterText, "static const TZrChar *backend_aot_llvm_meta_access_helper_name("));

        TEST_ASSERT_NOT_NULL(strstr(llvmFunctionBodyHeaderText, "backend_aot_write_llvm_function_body("));
        TEST_ASSERT_NOT_NULL(strstr(llvmFunctionBodyText, "backend_aot_write_llvm_function_body("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsHeaderText, "#include \"backend_aot_llvm_module_prelude.h\""));
        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsHeaderText, "backend_aot_llvm_write_module_exports("));
        TEST_ASSERT_NULL(strstr(llvmModuleArtifactsText, "backend_aot_llvm_write_module_prelude("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsText, "backend_aot_llvm_write_function_thunk_table("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsText, "backend_aot_llvm_write_module_exports("));
        TEST_ASSERT_NULL(strstr(llvmModuleArtifactsText, "void backend_aot_write_llvm_contracts("));
        TEST_ASSERT_NULL(strstr(llvmModuleArtifactsText, "void backend_aot_write_llvm_runtime_helper_decls("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeHeaderText, "backend_aot_llvm_write_module_prelude("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeText, "backend_aot_llvm_write_module_prelude("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeText, "backend_aot_write_llvm_contracts("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeText, "backend_aot_write_llvm_runtime_helper_decls("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringValuesText, "TZrBool backend_aot_llvm_lower_value_instruction("));
        TEST_ASSERT_NULL(strstr(llvmLoweringValuesText, "backend_aot_llvm_meta_access_helper_name("));
        TEST_ASSERT_NULL(strstr(llvmLoweringValuesText, "backend_aot_llvm_ownership_helper_name("));
        TEST_ASSERT_NULL(strstr(llvmLoweringValuesText, "backend_aot_llvm_lower_constant_instruction("));
        TEST_ASSERT_NULL(strstr(llvmLoweringValuesText, "backend_aot_llvm_lower_create_closure_instruction("));
        TEST_ASSERT_NULL(strstr(llvmLoweringValuesText, "backend_aot_llvm_binary_value_helper_name("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringConstantsText, "backend_aot_llvm_lower_constant_instruction("));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmLoweringClosureSlotsText, "backend_aot_llvm_lower_closure_value_subfamily(context, instruction)"));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmLoweringClosureSlotsText, "backend_aot_llvm_lower_stack_slot_value_subfamily(context, instruction)"));
        TEST_ASSERT_NULL(strstr(llvmLoweringClosureSlotsText, "backend_aot_llvm_lower_create_closure_instruction("));
        TEST_ASSERT_NULL(strstr(llvmLoweringClosureSlotsText, "backend_aot_llvm_lower_stack_copy_instruction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringClosuresText, "backend_aot_llvm_lower_closure_value_subfamily("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringClosuresText, "backend_aot_llvm_lower_create_closure_instruction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringClosuresText, "ZrLibrary_AotRuntime_CreateClosure"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringStackSlotsText, "backend_aot_llvm_lower_stack_slot_value_subfamily("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringStackSlotsText, "backend_aot_llvm_lower_stack_copy_instruction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringStackSlotsText, "ZrCore_Value_CopySlow"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringStackSlotsText, "ZrCore_Ownership_ReleaseValue"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringObjectMetaOwningText,
                                    "backend_aot_llvm_lower_object_value_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringObjectMetaOwningText,
                                    "backend_aot_llvm_lower_iterator_value_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringObjectMetaOwningText,
                                    "backend_aot_llvm_lower_meta_access_value_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringObjectMetaOwningText,
                                    "backend_aot_llvm_lower_ownership_value_family(context, instruction)"));
        TEST_ASSERT_NULL(strstr(llvmLoweringObjectMetaOwningText, "backend_aot_llvm_meta_access_helper_name("));
        TEST_ASSERT_NULL(strstr(llvmLoweringObjectMetaOwningText, "backend_aot_llvm_ownership_helper_name("));
        TEST_ASSERT_NULL(strstr(llvmLoweringObjectMetaOwningText, "ZrLibrary_AotRuntime_GetGlobal"));
        TEST_ASSERT_NULL(strstr(llvmLoweringObjectMetaOwningText, "ZrLibrary_AotRuntime_IterInit"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringObjectsText, "backend_aot_llvm_lower_object_value_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringObjectsText, "backend_aot_llvm_lower_global_value_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringObjectsText,
                                    "backend_aot_llvm_lower_member_index_value_family(context, instruction)"));
        TEST_ASSERT_NULL(strstr(llvmLoweringObjectsText, "ZrLibrary_AotRuntime_GetGlobal"));
        TEST_ASSERT_NULL(strstr(llvmLoweringObjectsText, "ZrLibrary_AotRuntime_SetByIndex"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringIteratorsText, "backend_aot_llvm_lower_iterator_value_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringIteratorsText, "ZrLibrary_AotRuntime_IterInit"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringIteratorsText, "ZrLibrary_AotRuntime_IterCurrent"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMetaAccessText, "backend_aot_llvm_meta_access_helper_name("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMetaAccessText, "backend_aot_llvm_lower_meta_access_value_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringOwnershipText, "backend_aot_llvm_ownership_helper_name("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringOwnershipText, "backend_aot_llvm_lower_ownership_value_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringArithmeticText, "backend_aot_llvm_binary_value_helper_name("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringArithmeticText, "backend_aot_llvm_unary_value_helper_name("));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmLoweringCallsText, "backend_aot_llvm_lower_function_call_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmLoweringCallsText, "backend_aot_llvm_lower_meta_call_family(context, instruction)"));
        TEST_ASSERT_NULL(strstr(llvmLoweringCallsText, "static TZrUInt32 backend_aot_llvm_function_call_argument_count("));
        TEST_ASSERT_NULL(strstr(llvmLoweringCallsText, "static TZrBool backend_aot_llvm_lower_function_call("));
        TEST_ASSERT_NULL(strstr(llvmLoweringCallsText, "static TZrBool backend_aot_llvm_lower_meta_call("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringFunctionCallsText, "backend_aot_llvm_lower_function_call_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringFunctionCallsText, "backend_aot_llvm_function_call_argument_count("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringFunctionCallsText, "ZrLibrary_AotRuntime_PrepareStaticDirectCall"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMetaCallsText, "backend_aot_llvm_lower_meta_call_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMetaCallsText, "backend_aot_llvm_meta_call_argument_count("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMetaCallsText, "ZrLibrary_AotRuntime_PrepareMetaCall"));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmLoweringControlText, "backend_aot_llvm_lower_exception_control_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmLoweringControlText, "backend_aot_llvm_lower_branch_control_family(context, instruction)"));
        TEST_ASSERT_NULL(strstr(llvmLoweringControlText, "static TZrBool backend_aot_llvm_lower_resume_control_instruction("));
        TEST_ASSERT_NULL(strstr(llvmLoweringControlText, "static TZrBool backend_aot_llvm_lower_jump_instruction("));
        TEST_ASSERT_NULL(strstr(llvmLoweringControlText, "static TZrBool backend_aot_llvm_lower_jump_if_instruction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringExceptionControlText, "backend_aot_llvm_lower_exception_control_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringExceptionControlText, "backend_aot_llvm_lower_resume_control_instruction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringExceptionControlText, "ZrLibrary_AotRuntime_SetPendingReturn"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringBranchControlText, "backend_aot_llvm_lower_branch_control_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringBranchControlText, "backend_aot_llvm_lower_jump_if_instruction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringBranchControlText, "backend_aot_llvm_write_return_call("));

        free(llvmEmitterText);
        free(llvmFunctionBodyText);
        free(llvmFunctionBodyHeaderText);
        free(llvmModuleArtifactsText);
        free(llvmModuleArtifactsHeaderText);
        free(llvmModulePreludeText);
        free(llvmModulePreludeHeaderText);
        free(llvmLoweringValuesText);
        free(llvmLoweringConstantsText);
        free(llvmLoweringClosureSlotsText);
        free(llvmLoweringClosuresText);
        free(llvmLoweringStackSlotsText);
        free(llvmLoweringObjectMetaOwningText);
        free(llvmLoweringObjectsText);
        free(llvmLoweringIteratorsText);
        free(llvmLoweringMetaAccessText);
        free(llvmLoweringOwnershipText);
        free(llvmLoweringArithmeticText);
        free(llvmLoweringCallsText);
        free(llvmLoweringFunctionCallsText);
        free(llvmLoweringMetaCallsText);
        free(llvmLoweringControlText);
        free(llvmLoweringExceptionControlText);
        free(llvmLoweringBranchControlText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_aot_llvm_object_subfamilies_out_of_object_lowering(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary =
            "Backend AOT Source Split Moves AOT LLVM Object Subfamilies Out Of Object Lowering";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot llvm object subfamily modularization",
                 "Testing that backend_aot_llvm_lowering_objects.c keeps object-family orchestration only while "
                 "global/create/type-conversion/member-index lowering move into dedicated LLVM AOT modules");

    {
        char *llvmLoweringObjectsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_objects.c");
        char *llvmLoweringGlobalsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_globals.c");
        char *llvmLoweringCreatesText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_creations.c");
        char *llvmLoweringTypeConversionsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_type_conversions.c");
        char *llvmLoweringMemberIndexText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_member_index.c");
        char *llvmLoweringMemberText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_member_access.c");
        char *llvmLoweringIndexText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_index_access.c");

        TEST_ASSERT_NOT_NULL(llvmLoweringObjectsText);
        TEST_ASSERT_NOT_NULL(llvmLoweringGlobalsText);
        TEST_ASSERT_NOT_NULL(llvmLoweringCreatesText);
        TEST_ASSERT_NOT_NULL(llvmLoweringTypeConversionsText);
        TEST_ASSERT_NOT_NULL(llvmLoweringMemberIndexText);
        TEST_ASSERT_NOT_NULL(llvmLoweringMemberText);
        TEST_ASSERT_NOT_NULL(llvmLoweringIndexText);

        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringObjectsText, "backend_aot_llvm_lower_global_value_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringObjectsText, "backend_aot_llvm_lower_creation_value_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmLoweringObjectsText, "backend_aot_llvm_lower_type_conversion_value_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmLoweringObjectsText, "backend_aot_llvm_lower_member_index_value_family(context, instruction)"));
        TEST_ASSERT_NULL(strstr(llvmLoweringObjectsText, "ZrLibrary_AotRuntime_GetGlobal"));
        TEST_ASSERT_NULL(strstr(llvmLoweringObjectsText, "ZrLibrary_AotRuntime_CreateObject"));
        TEST_ASSERT_NULL(strstr(llvmLoweringObjectsText, "ZrLibrary_AotRuntime_ToObject"));
        TEST_ASSERT_NULL(strstr(llvmLoweringObjectsText, "ZrLibrary_AotRuntime_SetByIndex"));

        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringGlobalsText, "backend_aot_llvm_lower_global_value_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringGlobalsText, "ZrLibrary_AotRuntime_GetGlobal"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringCreatesText, "backend_aot_llvm_lower_creation_value_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringCreatesText, "ZrLibrary_AotRuntime_CreateObject"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringCreatesText, "ZrLibrary_AotRuntime_CreateArray"));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmLoweringTypeConversionsText, "backend_aot_llvm_lower_type_conversion_value_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringTypeConversionsText, "ZrLibrary_AotRuntime_TypeOf"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringTypeConversionsText, "ZrLibrary_AotRuntime_ToStruct"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringTypeConversionsText, "ZrLibrary_AotRuntime_ToObject"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMemberIndexText, "backend_aot_llvm_lower_member_index_value_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMemberIndexText,
                                    "backend_aot_llvm_lower_member_value_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMemberIndexText,
                                    "backend_aot_llvm_lower_index_value_family(context, instruction)"));
        TEST_ASSERT_NULL(strstr(llvmLoweringMemberIndexText, "ZrLibrary_AotRuntime_GetMember"));
        TEST_ASSERT_NULL(strstr(llvmLoweringMemberIndexText, "ZrLibrary_AotRuntime_SetByIndex"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMemberText, "backend_aot_llvm_lower_member_value_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMemberText, "ZrLibrary_AotRuntime_GetMember"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMemberText, "ZrLibrary_AotRuntime_SetMember"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringIndexText, "backend_aot_llvm_lower_index_value_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringIndexText, "ZrLibrary_AotRuntime_GetByIndex"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringIndexText, "ZrLibrary_AotRuntime_SetByIndex"));

        free(llvmLoweringObjectsText);
        free(llvmLoweringGlobalsText);
        free(llvmLoweringCreatesText);
        free(llvmLoweringTypeConversionsText);
        free(llvmLoweringMemberIndexText);
        free(llvmLoweringMemberText);
        free(llvmLoweringIndexText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_aot_llvm_member_index_subfamilies_out_of_member_index_lowering(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary =
            "Backend AOT Source Split Moves AOT LLVM Member Index Subfamilies Out Of Member Index Lowering";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot llvm member/index subfamily modularization",
                 "Testing that backend_aot_llvm_lowering_member_index.c keeps member-index orchestration only while "
                 "member and index lowering move into dedicated LLVM AOT modules");

    {
        char *llvmLoweringMemberIndexText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_member_index.c");
        char *llvmLoweringMemberText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_member_access.c");
        char *llvmLoweringIndexText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_index_access.c");

        TEST_ASSERT_NOT_NULL(llvmLoweringMemberIndexText);
        TEST_ASSERT_NOT_NULL(llvmLoweringMemberText);
        TEST_ASSERT_NOT_NULL(llvmLoweringIndexText);

        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMemberIndexText,
                                    "backend_aot_llvm_lower_member_value_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMemberIndexText,
                                    "backend_aot_llvm_lower_index_value_family(context, instruction)"));
        TEST_ASSERT_NULL(strstr(llvmLoweringMemberIndexText, "ZrLibrary_AotRuntime_GetMember"));
        TEST_ASSERT_NULL(strstr(llvmLoweringMemberIndexText, "ZrLibrary_AotRuntime_SetByIndex"));

        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMemberText, "backend_aot_llvm_lower_member_value_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMemberText, "ZrLibrary_AotRuntime_GetMember"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMemberText, "ZrLibrary_AotRuntime_SetMember"));

        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringIndexText, "backend_aot_llvm_lower_index_value_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringIndexText, "ZrLibrary_AotRuntime_GetByIndex"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringIndexText, "ZrLibrary_AotRuntime_SetByIndex"));

        free(llvmLoweringMemberIndexText);
        free(llvmLoweringMemberText);
        free(llvmLoweringIndexText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_aot_llvm_call_subfamilies_out_of_call_lowering(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary =
            "Backend AOT Source Split Moves AOT LLVM Call Subfamilies Out Of Call Lowering";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot llvm call subfamily modularization",
                 "Testing that backend_aot_llvm_lowering_calls.c keeps call orchestration only while function-call "
                 "and meta-call lowering move into dedicated LLVM AOT modules");

    {
        char *llvmLoweringCallsText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_calls.c");
        char *llvmLoweringFunctionCallsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_function_calls.c");
        char *llvmLoweringMetaCallsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_meta_calls.c");

        TEST_ASSERT_NOT_NULL(llvmLoweringCallsText);
        TEST_ASSERT_NOT_NULL(llvmLoweringFunctionCallsText);
        TEST_ASSERT_NOT_NULL(llvmLoweringMetaCallsText);

        TEST_ASSERT_NOT_NULL(
                strstr(llvmLoweringCallsText, "backend_aot_llvm_lower_function_call_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmLoweringCallsText, "backend_aot_llvm_lower_meta_call_family(context, instruction)"));
        TEST_ASSERT_NULL(strstr(llvmLoweringCallsText, "backend_aot_llvm_function_call_argument_count("));
        TEST_ASSERT_NULL(strstr(llvmLoweringCallsText, "backend_aot_llvm_meta_call_argument_count("));
        TEST_ASSERT_NULL(strstr(llvmLoweringCallsText, "static TZrBool backend_aot_llvm_lower_function_call("));
        TEST_ASSERT_NULL(strstr(llvmLoweringCallsText, "static TZrBool backend_aot_llvm_lower_meta_call("));

        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringFunctionCallsText, "backend_aot_llvm_lower_function_call_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringFunctionCallsText, "backend_aot_llvm_function_call_argument_count("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringFunctionCallsText, "ZrLibrary_AotRuntime_PrepareDirectCall"));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringFunctionCallsText, "ZrLibrary_AotRuntime_PrepareStaticDirectCall"));

        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMetaCallsText, "backend_aot_llvm_lower_meta_call_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMetaCallsText, "backend_aot_llvm_meta_call_argument_count("));
        TEST_ASSERT_NOT_NULL(strstr(llvmLoweringMetaCallsText, "ZrLibrary_AotRuntime_PrepareMetaCall"));

        free(llvmLoweringCallsText);
        free(llvmLoweringFunctionCallsText);
        free(llvmLoweringMetaCallsText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_aot_llvm_text_emit_helpers_out_of_control_lowering(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Backend AOT Source Split Moves AOT LLVM Text Emit Helpers Out Of Control Lowering";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot llvm text emit split",
                 "Testing that backend_aot_llvm_lowering_control.c keeps only control-family orchestration while "
                 "shared LLVM text-emission helpers stay in a dedicated helper module");

    {
        char *llvmControlText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_control.c");
        char *llvmTextEmitText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_emit.c");
        char *llvmTextFlowText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_flow.c");
        char *llvmTextEmitHeaderText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_emit.h");
        char *llvmTextFlowHeaderText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_flow.h");

        TEST_ASSERT_NOT_NULL(llvmControlText);
        TEST_ASSERT_NOT_NULL(llvmTextEmitText);
        TEST_ASSERT_NOT_NULL(llvmTextFlowText);
        TEST_ASSERT_NOT_NULL(llvmTextEmitHeaderText);
        TEST_ASSERT_NOT_NULL(llvmTextFlowHeaderText);

        TEST_ASSERT_NULL(strstr(llvmControlText, "TZrUInt32 backend_aot_llvm_next_temp("));
        TEST_ASSERT_NULL(strstr(llvmControlText, "void backend_aot_llvm_make_function_label("));
        TEST_ASSERT_NULL(strstr(llvmControlText, "void backend_aot_llvm_make_instruction_label("));
        TEST_ASSERT_NULL(strstr(llvmControlText, "void backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NULL(strstr(llvmControlText, "void backend_aot_llvm_write_nonzero_call_text("));
        TEST_ASSERT_NULL(strstr(llvmControlText, "void backend_aot_llvm_write_begin_instruction("));
        TEST_ASSERT_NULL(strstr(llvmControlText, "void backend_aot_llvm_write_report_unsupported_return("));
        TEST_ASSERT_NULL(strstr(llvmControlText, "void backend_aot_llvm_write_report_unsupported_value_return("));
        TEST_ASSERT_NULL(strstr(llvmControlText, "void backend_aot_llvm_write_resume_dispatch("));
        TEST_ASSERT_NULL(strstr(llvmControlText, "void backend_aot_llvm_write_return_call("));
        TEST_ASSERT_NOT_NULL(strstr(llvmControlText, "TZrBool backend_aot_llvm_lower_control_instruction("));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmControlText, "backend_aot_llvm_lower_exception_control_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmControlText, "backend_aot_llvm_lower_branch_control_family(context, instruction)"));

        TEST_ASSERT_NULL(strstr(llvmTextEmitHeaderText, "backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NULL(strstr(llvmTextEmitHeaderText, "backend_aot_llvm_write_resume_dispatch("));
        TEST_ASSERT_NULL(strstr(llvmTextEmitHeaderText, "backend_aot_llvm_write_return_call("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "#include \"backend_aot_llvm_text_call_result.h\""));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "#include \"backend_aot_llvm_text_terminal.h\""));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "backend_aot_llvm_write_resume_dispatch("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextEmitText, "TZrUInt32 backend_aot_llvm_next_temp("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextEmitText, "void backend_aot_llvm_make_instruction_label("));
        TEST_ASSERT_NULL(strstr(llvmTextEmitText, "void backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NULL(strstr(llvmTextEmitText, "void backend_aot_llvm_write_resume_dispatch("));
        TEST_ASSERT_NULL(strstr(llvmTextEmitText, "void backend_aot_llvm_write_return_call("));
        TEST_ASSERT_NULL(strstr(llvmTextFlowText, "void backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowText, "void backend_aot_llvm_write_resume_dispatch("));
        TEST_ASSERT_NULL(strstr(llvmTextFlowText, "void backend_aot_llvm_write_return_call("));

        free(llvmControlText);
        free(llvmTextEmitText);
        free(llvmTextFlowText);
        free(llvmTextEmitHeaderText);
        free(llvmTextFlowHeaderText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_aot_llvm_module_artifacts_out_of_emitter(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary =
            "Backend AOT Source Split Moves AOT LLVM Module Artifacts Out Of Emitter";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot llvm module artifact modularization",
                 "Testing that backend_aot_llvm_emitter.c keeps writer orchestration only while LLVM module "
                 "prelude, thunk-table, and export/descriptor emission move into dedicated module-artifact helpers");

    {
        char *llvmEmitterText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_emitter.c");
        char *llvmModuleArtifactsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.c");
        char *llvmModuleArtifactsHeaderText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.h");
        char *llvmModulePreludeText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.c");
        char *llvmModulePreludeHeaderText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.h");

        TEST_ASSERT_NOT_NULL(llvmEmitterText);
        TEST_ASSERT_NOT_NULL(llvmModuleArtifactsText);
        TEST_ASSERT_NOT_NULL(llvmModuleArtifactsHeaderText);
        TEST_ASSERT_NOT_NULL(llvmModulePreludeText);
        TEST_ASSERT_NOT_NULL(llvmModulePreludeHeaderText);

        TEST_ASSERT_NOT_NULL(strstr(llvmEmitterText, "backend_aot_llvm_write_module_prelude("));
        TEST_ASSERT_NOT_NULL(strstr(llvmEmitterText, "backend_aot_llvm_write_function_thunk_table("));
        TEST_ASSERT_NOT_NULL(strstr(llvmEmitterText, "backend_aot_llvm_write_module_exports("));
        TEST_ASSERT_NULL(strstr(llvmEmitterText, "void backend_aot_write_llvm_contracts("));
        TEST_ASSERT_NULL(strstr(llvmEmitterText, "void backend_aot_write_llvm_runtime_helper_decls("));
        TEST_ASSERT_NULL(strstr(llvmEmitterText, "define i64 @zr_aot_entry("));
        TEST_ASSERT_NULL(strstr(llvmEmitterText, "@zr_aot_function_thunks = private constant"));
        TEST_ASSERT_NULL(strstr(llvmEmitterText, "@zr_aot_module = private constant"));

        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsHeaderText, "#include \"backend_aot_llvm_module_prelude.h\""));
        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsHeaderText, "backend_aot_llvm_write_module_exports("));
        TEST_ASSERT_NULL(strstr(llvmModuleArtifactsText, "backend_aot_llvm_write_module_prelude("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsText, "backend_aot_llvm_write_function_thunk_table("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsText, "backend_aot_llvm_write_module_exports("));
        TEST_ASSERT_NULL(strstr(llvmModuleArtifactsText, "void backend_aot_write_llvm_contracts("));
        TEST_ASSERT_NULL(strstr(llvmModuleArtifactsText, "void backend_aot_write_llvm_runtime_helper_decls("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsText, "define i64 @zr_aot_entry("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsText, "@zr_aot_module = private constant"));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeHeaderText, "backend_aot_llvm_write_module_prelude("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeText, "backend_aot_llvm_write_module_prelude("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeText, "void backend_aot_write_llvm_contracts("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeText, "void backend_aot_write_llvm_runtime_helper_decls("));

        free(llvmEmitterText);
        free(llvmModuleArtifactsText);
        free(llvmModuleArtifactsHeaderText);
        free(llvmModulePreludeText);
        free(llvmModulePreludeHeaderText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_aot_llvm_module_prelude_helpers_out_of_module_artifacts(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary =
            "Backend AOT Source Split Moves AOT LLVM Module Prelude Helpers Out Of Module Artifacts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot llvm module prelude modularization",
                 "Testing that backend_aot_llvm_module_artifacts.c keeps thunk/export descriptor emission only "
                 "while prelude contracts, helper declarations, and embedded blob emission move into a dedicated "
                 "LLVM module-prelude module");

    {
        char *llvmModuleArtifactsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.c");
        char *llvmModuleArtifactsHeaderText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.h");
        char *llvmModulePreludeText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.c");
        char *llvmModulePreludeHeaderText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.h");

        TEST_ASSERT_NOT_NULL(llvmModuleArtifactsText);
        TEST_ASSERT_NOT_NULL(llvmModuleArtifactsHeaderText);
        TEST_ASSERT_NOT_NULL(llvmModulePreludeText);
        TEST_ASSERT_NOT_NULL(llvmModulePreludeHeaderText);

        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsHeaderText, "#include \"backend_aot_llvm_module_prelude.h\""));
        TEST_ASSERT_NULL(strstr(llvmModuleArtifactsText, "void backend_aot_write_llvm_contracts("));
        TEST_ASSERT_NULL(strstr(llvmModuleArtifactsText, "void backend_aot_write_runtime_contract_array_llvm("));
        TEST_ASSERT_NULL(strstr(llvmModuleArtifactsText, "void backend_aot_write_runtime_contract_globals_llvm("));
        TEST_ASSERT_NULL(strstr(llvmModuleArtifactsText, "void backend_aot_write_embedded_blob_llvm("));
        TEST_ASSERT_NULL(strstr(llvmModuleArtifactsText, "void backend_aot_write_llvm_runtime_helper_decls("));
        TEST_ASSERT_NULL(strstr(llvmModuleArtifactsText, "void backend_aot_llvm_write_module_prelude("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeHeaderText, "backend_aot_llvm_write_module_prelude("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeText, "void backend_aot_write_llvm_contracts("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeText, "void backend_aot_write_runtime_contract_array_llvm("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeText, "void backend_aot_write_runtime_contract_globals_llvm("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeText, "void backend_aot_write_embedded_blob_llvm("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeText, "void backend_aot_write_llvm_runtime_helper_decls("));
        TEST_ASSERT_NOT_NULL(strstr(llvmModulePreludeText, "void backend_aot_llvm_write_module_prelude("));

        free(llvmModuleArtifactsText);
        free(llvmModuleArtifactsHeaderText);
        free(llvmModulePreludeText);
        free(llvmModulePreludeHeaderText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_aot_llvm_flow_text_helpers_out_of_text_emit(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary =
            "Backend AOT Source Split Moves AOT LLVM Flow Text Helpers Out Of Text Emit";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot llvm flow text modularization",
                 "Testing that backend_aot_llvm_text_emit.c keeps label/temp helper emission only while "
                 "guarded-call, begin-instruction, unsupported, resume, and return text helpers move into a "
                 "dedicated LLVM flow-text module");

    {
        char *llvmTextEmitText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_emit.c");
        char *llvmTextFlowText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_flow.c");
        char *llvmTextEmitHeaderText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_emit.h");
        char *llvmTextFlowHeaderText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_flow.h");
        char *llvmTextCallResultText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_call_result.c");
        char *llvmTextCallResultHeaderText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_call_result.h");
        char *llvmTextTerminalText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_terminal.c");
        char *llvmTextTerminalHeaderText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_terminal.h");

        TEST_ASSERT_NOT_NULL(llvmTextEmitText);
        TEST_ASSERT_NOT_NULL(llvmTextFlowText);
        TEST_ASSERT_NOT_NULL(llvmTextEmitHeaderText);
        TEST_ASSERT_NOT_NULL(llvmTextFlowHeaderText);
        TEST_ASSERT_NOT_NULL(llvmTextCallResultText);
        TEST_ASSERT_NOT_NULL(llvmTextCallResultHeaderText);
        TEST_ASSERT_NOT_NULL(llvmTextTerminalText);
        TEST_ASSERT_NOT_NULL(llvmTextTerminalHeaderText);

        TEST_ASSERT_NOT_NULL(strstr(llvmTextEmitText, "TZrUInt32 backend_aot_llvm_next_temp("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextEmitText, "void backend_aot_llvm_make_instruction_label("));
        TEST_ASSERT_NULL(strstr(llvmTextEmitText, "void backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NULL(strstr(llvmTextEmitText, "void backend_aot_llvm_write_begin_instruction("));
        TEST_ASSERT_NULL(strstr(llvmTextEmitText, "void backend_aot_llvm_write_resume_dispatch("));
        TEST_ASSERT_NULL(strstr(llvmTextEmitText, "void backend_aot_llvm_write_return_call("));

        TEST_ASSERT_NULL(strstr(llvmTextEmitHeaderText, "backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NULL(strstr(llvmTextEmitHeaderText, "backend_aot_llvm_write_begin_instruction("));
        TEST_ASSERT_NULL(strstr(llvmTextEmitHeaderText, "backend_aot_llvm_write_resume_dispatch("));
        TEST_ASSERT_NULL(strstr(llvmTextEmitHeaderText, "backend_aot_llvm_write_return_call("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "#include \"backend_aot_llvm_text_call_result.h\""));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "#include \"backend_aot_llvm_text_terminal.h\""));
        TEST_ASSERT_NULL(strstr(llvmTextFlowHeaderText, "backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "backend_aot_llvm_write_begin_instruction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "backend_aot_llvm_write_resume_dispatch("));
        TEST_ASSERT_NULL(strstr(llvmTextFlowHeaderText, "backend_aot_llvm_write_return_call("));
        TEST_ASSERT_NULL(strstr(llvmTextFlowText, "void backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowText, "void backend_aot_llvm_write_begin_instruction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowText, "void backend_aot_llvm_write_resume_dispatch("));
        TEST_ASSERT_NULL(strstr(llvmTextFlowText, "void backend_aot_llvm_write_return_call("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextCallResultHeaderText, "backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextCallResultHeaderText, "backend_aot_llvm_write_nonzero_call_text("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextCallResultText, "void backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextCallResultText, "void backend_aot_llvm_write_nonzero_call_text("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextTerminalHeaderText, "backend_aot_llvm_write_report_unsupported_return("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextTerminalHeaderText, "backend_aot_llvm_write_return_call("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextTerminalText, "void backend_aot_llvm_write_report_unsupported_return("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextTerminalText, "void backend_aot_llvm_write_return_call("));

        free(llvmTextEmitText);
        free(llvmTextFlowText);
        free(llvmTextEmitHeaderText);
        free(llvmTextFlowHeaderText);
        free(llvmTextCallResultText);
        free(llvmTextCallResultHeaderText);
        free(llvmTextTerminalText);
        free(llvmTextTerminalHeaderText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_aot_llvm_flow_subfamilies_out_of_text_flow(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary =
            "Backend AOT Source Split Moves AOT LLVM Flow Subfamilies Out Of Text Flow";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot llvm flow subfamily modularization",
                 "Testing that backend_aot_llvm_text_flow.c keeps begin/resume orchestration only while "
                 "call-result and terminal return helpers move into dedicated LLVM text-flow subfamily modules");

    {
        char *llvmTextFlowText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_flow.c");
        char *llvmTextFlowHeaderText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_flow.h");
        char *llvmTextCallResultText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_call_result.c");
        char *llvmTextCallResultHeaderText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_call_result.h");
        char *llvmTextTerminalText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_terminal.c");
        char *llvmTextTerminalHeaderText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_terminal.h");

        TEST_ASSERT_NOT_NULL(llvmTextFlowText);
        TEST_ASSERT_NOT_NULL(llvmTextFlowHeaderText);
        TEST_ASSERT_NOT_NULL(llvmTextCallResultText);
        TEST_ASSERT_NOT_NULL(llvmTextCallResultHeaderText);
        TEST_ASSERT_NOT_NULL(llvmTextTerminalText);
        TEST_ASSERT_NOT_NULL(llvmTextTerminalHeaderText);

        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "#include \"backend_aot_llvm_text_call_result.h\""));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "#include \"backend_aot_llvm_text_terminal.h\""));
        TEST_ASSERT_NULL(strstr(llvmTextFlowText, "void backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NULL(strstr(llvmTextFlowText, "void backend_aot_llvm_write_nonzero_call_text("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowText, "void backend_aot_llvm_write_begin_instruction("));
        TEST_ASSERT_NULL(strstr(llvmTextFlowText, "void backend_aot_llvm_write_report_unsupported_return("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowText, "void backend_aot_llvm_write_resume_dispatch("));
        TEST_ASSERT_NULL(strstr(llvmTextFlowText, "void backend_aot_llvm_write_return_call("));

        TEST_ASSERT_NOT_NULL(strstr(llvmTextCallResultHeaderText, "backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextCallResultHeaderText, "backend_aot_llvm_write_nonzero_call_text("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextCallResultText, "void backend_aot_llvm_write_guarded_call_text("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextCallResultText, "void backend_aot_llvm_write_nonzero_call_text("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextTerminalHeaderText, "backend_aot_llvm_write_report_unsupported_return("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextTerminalHeaderText, "backend_aot_llvm_write_report_unsupported_value_return("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextTerminalHeaderText, "backend_aot_llvm_write_return_call("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextTerminalText, "void backend_aot_llvm_write_report_unsupported_return("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextTerminalText, "void backend_aot_llvm_write_report_unsupported_value_return("));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextTerminalText, "void backend_aot_llvm_write_return_call("));

        free(llvmTextFlowText);
        free(llvmTextFlowHeaderText);
        free(llvmTextCallResultText);
        free(llvmTextCallResultHeaderText);
        free(llvmTextTerminalText);
        free(llvmTextTerminalHeaderText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_aot_llvm_control_subfamilies_out_of_control_lowering(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary =
            "Backend AOT Source Split Moves AOT LLVM Control Subfamilies Out Of Control Lowering";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot llvm control subfamily modularization",
                 "Testing that backend_aot_llvm_lowering_control.c keeps control orchestration only while "
                 "exception/resume control and branch/return lowering move into dedicated LLVM AOT modules");

    {
        char *llvmControlText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_control.c");
        char *llvmExceptionControlText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_exception_control.c");
        char *llvmBranchControlText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_branch_control.c");

        TEST_ASSERT_NOT_NULL(llvmControlText);
        TEST_ASSERT_NOT_NULL(llvmExceptionControlText);
        TEST_ASSERT_NOT_NULL(llvmBranchControlText);

        TEST_ASSERT_NOT_NULL(
                strstr(llvmControlText, "backend_aot_llvm_lower_exception_control_family(context, instruction)"));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmControlText, "backend_aot_llvm_lower_branch_control_family(context, instruction)"));
        TEST_ASSERT_NULL(strstr(llvmControlText, "backend_aot_llvm_lower_resume_control_instruction("));
        TEST_ASSERT_NULL(strstr(llvmControlText, "backend_aot_llvm_lower_jump_instruction("));
        TEST_ASSERT_NULL(strstr(llvmControlText, "backend_aot_llvm_lower_jump_if_instruction("));

        TEST_ASSERT_NOT_NULL(strstr(llvmExceptionControlText, "backend_aot_llvm_lower_exception_control_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmExceptionControlText, "backend_aot_llvm_lower_resume_control_instruction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmExceptionControlText, "ZrLibrary_AotRuntime_Try"));
        TEST_ASSERT_NOT_NULL(strstr(llvmExceptionControlText, "ZrLibrary_AotRuntime_SetPendingContinue"));

        TEST_ASSERT_NOT_NULL(strstr(llvmBranchControlText, "backend_aot_llvm_lower_branch_control_family("));
        TEST_ASSERT_NOT_NULL(strstr(llvmBranchControlText, "backend_aot_llvm_lower_jump_if_instruction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmBranchControlText, "ZrLibrary_AotRuntime_IsTruthy"));
        TEST_ASSERT_NOT_NULL(strstr(llvmBranchControlText, "backend_aot_llvm_write_return_call("));

        free(llvmControlText);
        free(llvmExceptionControlText);
        free(llvmBranchControlText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_backend_aot_source_split_moves_aot_llvm_closure_subfamilies_out_of_closure_lowering(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary =
            "Backend AOT Source Split Moves AOT LLVM Closure Subfamilies Out Of Closure Lowering";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("backend aot llvm closure subfamily modularization",
                 "Testing that backend_aot_llvm_lowering_closure_slots.c keeps closure-slot orchestration only while "
                 "closure lowering and stack-slot lowering move into dedicated LLVM AOT modules");

    {
        char *llvmClosureSlotsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_closure_slots.c");
        char *llvmClosuresText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_closures.c");
        char *llvmStackSlotsText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_stack_slots.c");

        TEST_ASSERT_NOT_NULL(llvmClosureSlotsText);
        TEST_ASSERT_NOT_NULL(llvmClosuresText);
        TEST_ASSERT_NOT_NULL(llvmStackSlotsText);

        TEST_ASSERT_NOT_NULL(
                strstr(llvmClosureSlotsText, "backend_aot_llvm_lower_closure_value_subfamily(context, instruction)"));
        TEST_ASSERT_NOT_NULL(
                strstr(llvmClosureSlotsText, "backend_aot_llvm_lower_stack_slot_value_subfamily(context, instruction)"));
        TEST_ASSERT_NULL(strstr(llvmClosureSlotsText, "backend_aot_llvm_lower_create_closure_instruction("));
        TEST_ASSERT_NULL(strstr(llvmClosureSlotsText, "backend_aot_llvm_lower_stack_copy_instruction("));

        TEST_ASSERT_NOT_NULL(strstr(llvmClosuresText, "backend_aot_llvm_lower_closure_value_subfamily("));
        TEST_ASSERT_NOT_NULL(strstr(llvmClosuresText, "backend_aot_llvm_lower_create_closure_instruction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmClosuresText, "ZrLibrary_AotRuntime_CreateClosure"));
        TEST_ASSERT_NOT_NULL(strstr(llvmClosuresText, "ZrLibrary_AotRuntime_GetClosureValue"));

        TEST_ASSERT_NOT_NULL(strstr(llvmStackSlotsText, "backend_aot_llvm_lower_stack_slot_value_subfamily("));
        TEST_ASSERT_NOT_NULL(strstr(llvmStackSlotsText, "backend_aot_llvm_lower_stack_copy_instruction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmStackSlotsText, "ZrCore_Value_CopySlow"));
        TEST_ASSERT_NOT_NULL(strstr(llvmStackSlotsText, "ZrCore_Ownership_ReleaseValue"));

        free(llvmClosureSlotsText);
        free(llvmClosuresText);
        free(llvmStackSlotsText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_execbc_quickens_zero_arg_call_sites_without_changing_semir_contracts(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "ExecBC Quickens Zero Arg Call Sites Without Changing SemIR Contracts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("execbc call-site quickening",
                 "Testing that zero-argument direct and meta call sites quicken only in ExecBC while SemIR keeps the original semantic runtime opcode");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "answer(): int {\n"
                "    return 11;\n"
                "}\n"
                "class Counter {\n"
                "    pub var base: int;\n"
                "    pub @constructor(base: int) {\n"
                "        this.base = base;\n"
                "    }\n"
                "    pub @call(): int {\n"
                "        return this.base + 1;\n"
                "    }\n"
                "}\n"
                "var counter = new Counter(7);\n"
                "return answer() + counter();";
        const char *intermediatePath = "execbc_call_site_quickening_test.zri";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *func;
        char *intermediateText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_call_site_quickening_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS)));
        TEST_ASSERT_FALSE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(META_CALL)));
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(func, ZR_SEMIR_OPCODE_META_CALL, ZR_TRUE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, func, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SEMIR"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_KNOWN_VM_CALL_NO_ARGS"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_CALL_NO_ARGS"));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, func, &result));
        TEST_ASSERT_EQUAL_INT64(19, result);

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_execbc_true_aot_lowers_typed_signed_unsigned_and_equality_opcodes(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "ExecBC True AOT Lowers Typed Signed Unsigned And Equality Opcodes";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("typed arithmetic and equality aot pipeline",
                 "Testing that strongly typed signed/unsigned arithmetic and bool/string/float equality preserve dedicated ExecBC opcodes and remain supported by intermediate, strict AOT C, and LLVM lowering.");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "var si: int = 7;\n"
                "var sj: int = 2;\n"
                "var su: uint = 9;\n"
                "var sv: uint = 4;\n"
                "var sb: bool = true;\n"
                "var ss: string = \"left\";\n"
                "var st: string = \"left\";\n"
                "var sf: float = 1.5;\n"
                "var sg: float = 1.5;\n"
                "var signedSum: int = si + sj;\n"
                "var signedDiff: int = si - sj;\n"
                "var unsignedSum: uint = su + sv;\n"
                "var unsignedDiff: uint = su - sv;\n"
                "var boolEq: bool = sb == true;\n"
                "var signedNe: bool = si != sj;\n"
                "var unsignedNe: bool = su != sv;\n"
                "var floatEq: bool = sf == sg;\n"
                "var stringEq: bool = ss == st;\n"
                "return signedSum + signedDiff + <int> unsignedSum + <int> unsignedDiff +\n"
                "       (boolEq ? 1 : 0) + (signedNe ? 1 : 0) + (unsignedNe ? 1 : 0) +\n"
                "       (floatEq ? 1 : 0) + (stringEq ? 1 : 0);\n";
        const char *intermediatePath = "execbc_typed_numeric_equality_test.zri";
        const char *cPath = "execbc_typed_numeric_equality_test.c";
        const char *llvmPath = "execbc_typed_numeric_equality_test.ll";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *function;
        char *intermediateText;
        char *cText;
        char *llvmText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZR_STRING_LITERAL(state, "execbc_typed_numeric_equality_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        function = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(function);

        TEST_ASSERT_TRUE(function_contains_add_signed_family(function));
        TEST_ASSERT_TRUE(function_contains_sub_signed_family(function));
        TEST_ASSERT_TRUE(function_contains_add_unsigned_family(function));
        TEST_ASSERT_TRUE(function_contains_sub_unsigned_family(function));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING)));
        TEST_ASSERT_FALSE(function_contains_add_int_family(function));
        TEST_ASSERT_FALSE(function_contains_sub_int_family(function));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL)));

        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            function,
                                                            "execbc_typed_numeric_equality_test",
                                                            cPath));
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               function,
                                                               "execbc_typed_numeric_equality_test",
                                                               llvmPath));
        intermediateText = read_text_file_owned(intermediatePath);
        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "ADD_SIGNED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "ADD_UNSIGNED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUB_SIGNED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUB_UNSIGNED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "LOGICAL_EQUAL_BOOL"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "LOGICAL_NOT_EQUAL_SIGNED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "LOGICAL_NOT_EQUAL_UNSIGNED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "LOGICAL_EQUAL_FLOAT"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "LOGICAL_EQUAL_STRING"));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(ADD_SIGNED)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUB_SIGNED)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(37, result);

        free(intermediateText);
        free(cText);
        free(llvmText);
        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_execbc_true_aot_lowers_known_native_call_family(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "ExecBC True AOT Lowers Known Native Call Family";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("known native call aot pipeline",
                 "Testing that direct native module call sites quicken to KNOWN_NATIVE_CALL and SUPER_KNOWN_NATIVE_CALL_NO_ARGS, survive intermediate emission, and stay supported by strict AOT C and LLVM lowering.");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "execbc_known_native_call_test.zri";
        const char *cPath = "execbc_known_native_call_test.c";
        const char *llvmPath = "execbc_known_native_call_test.ll";
        SZrString *sourceName;
        SZrAstNode *ast;
        SZrFunction *function;
        char *source;
        char *intermediateText;
        char *cText;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);
        source = read_repo_file_owned("tests/fixtures/scripts/decorator_artifact_baseline.zr");
        TEST_ASSERT_NOT_NULL(source);

        sourceName = ZR_STRING_LITERAL(state, "decorator_artifact_baseline.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        function = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(function);

        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_TAIL_CALL)));

        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state, function, "execbc_known_native_call_test", cPath));
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               function,
                                                               "execbc_known_native_call_test",
                                                               llvmPath));
        intermediateText = read_text_file_owned(intermediatePath);
        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "KNOWN_NATIVE_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_PrepareDirectCall"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CallPreparedOrGeneric"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "@ZrLibrary_AotRuntime_PrepareDirectCall"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "@ZrLibrary_AotRuntime_CallPreparedOrGeneric"));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL)));

        free(intermediateText);
        free(cText);
        free(llvmText);
        free(source);
        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_execbc_quickens_cached_meta_and_dynamic_call_sites_and_preserves_binary_metadata(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "ExecBC Quickens Cached Meta And Dynamic Call Sites And Preserves Binary Metadata";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("execbc cached callsite pipeline",
                 "Testing that one-argument META_CALL and DYN_CALL sites quicken to cached ExecBC variants while SemIR, binary metadata, and runtime loading keep the original contracts");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "cached_meta_dyn_callsite_test.zri";
        const char *binaryPath = "cached_meta_dyn_callsite_test.zro";
        SZrFunction *function;
        char *intermediateText;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        const SZrFunctionCallSiteCacheEntry *metaCache;
        const SZrFunctionCallSiteCacheEntry *dynCache;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_cached_meta_and_dynamic_callsite_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(META_CALL)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(DYN_CALL)));
        TEST_ASSERT_TRUE(function_tree_contains_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL));
        TEST_ASSERT_TRUE(function_tree_contains_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_CALL, ZR_TRUE));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_DYN_CALL, ZR_TRUE));

        metaCache = function_tree_find_first_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL);
        dynCache = function_tree_find_first_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL);
        TEST_ASSERT_NOT_NULL(metaCache);
        TEST_ASSERT_NOT_NULL(dynCache);
        TEST_ASSERT_EQUAL_UINT32(1, metaCache->argumentCount);
        TEST_ASSERT_EQUAL_UINT32(1, dynCache->argumentCount);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "CALLSITE_CACHE_TABLE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_CALL_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_DYN_CALL_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "DYN_CALL"));

        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_EQUAL_UINT32(function->memberEntryLength, runtimeFunction->memberEntryLength);
        assert_runtime_function_matches_source_function(function, runtimeFunction);

        metaCache = function_tree_find_first_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL);
        dynCache = function_tree_find_first_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL);
        TEST_ASSERT_NOT_NULL(metaCache);
        TEST_ASSERT_NOT_NULL(dynCache);
        TEST_ASSERT_EQUAL_UINT32(1, metaCache->argumentCount);
        TEST_ASSERT_EQUAL_UINT32(1, dynCache->argumentCount);

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(25, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        free(intermediateText);
        remove(intermediatePath);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_binary_roundtrip_preserves_fixed_array_helper_argument_values(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Binary Roundtrip Preserves Fixed Array Helper Argument Values";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("binary roundtrip helper call",
                 "Testing that .zro roundtrip keeps foreach item values intact when a helper function consumes the current fixed-array element");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *binaryPath = "fixed_array_helper_roundtrip_test.zro";
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_fixed_array_helper_roundtrip_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(16, result);
        result = 0;

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(16, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_binary_roundtrip_preserves_container_matrix_native_call_arguments(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Binary Roundtrip Preserves Container Matrix Native Call Arguments";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("binary roundtrip container/native path",
                 "Testing that .zro roundtrip keeps helper-call arguments stable across imported container constructors, native methods, and nested generic values");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *binaryPath = "container_matrix_roundtrip_test.zro";
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_container_matrix_roundtrip_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(417, result);
        result = 0;

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        assert_runtime_function_matches_source_function(function, runtimeFunction);

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(417, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_binary_roundtrip_preserves_debug_line_metadata_for_runtime_loading(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Binary Roundtrip Preserves Debug Line Metadata For Runtime Loading";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("binary debug line metadata",
                 "Testing that .zro roundtrip keeps per-instruction debug line data available to both the IO source model and the runtime-loaded SZrFunction");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *binaryPath = "binary_roundtrip_debug_line_runtime.zro";
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        const SZrIoFunction *ioFunction;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_cached_meta_and_dynamic_callsite_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function->instructionsLength > 0);
        TEST_ASSERT_TRUE(function->executionLocationInfoLength > 0);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));

        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        TEST_ASSERT_TRUE(sourceObject->isDebug);
        TEST_ASSERT_TRUE(sourceObject->modulesLength > 0);
        TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);
        ioFunction = sourceObject->modules[0].entryFunction;
        TEST_ASSERT_TRUE(ioFunction->debugInfosLength > 0);
        TEST_ASSERT_NOT_NULL(ioFunction->debugInfos);
        TEST_ASSERT_EQUAL_UINT32(function->instructionsLength, (TZrUInt32)ioFunction->debugInfos[0].instructionsLength);
        TEST_ASSERT_NOT_NULL(ioFunction->debugInfos[0].instructionsLine);
        TEST_ASSERT_EQUAL_UINT32(function_find_debug_line_for_instruction(function, 0),
                                 (TZrUInt32)ioFunction->debugInfos[0].instructionsLine[0]);

        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        assert_runtime_function_matches_source_function(function, runtimeFunction);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(25, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_binary_roundtrip_preserves_map_stored_array_iterable_contract(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Binary Roundtrip Preserves Map Stored Array Iterable Contract";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("binary roundtrip map/value iteration",
                 "Testing that an imported container.Array stored inside container.Map still iterates after .zro roundtrip");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *binaryPath = "map_array_roundtrip_test.zro";
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_map_array_roundtrip_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(23, result);
        result = 0;

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        assert_runtime_function_matches_source_function(function, runtimeFunction);
        TEST_ASSERT_EQUAL_UINT32(function->localVariableLength, runtimeFunction->localVariableLength);
        TEST_ASSERT_EQUAL_UINT32(function->constantValueLength, runtimeFunction->constantValueLength);
        TEST_ASSERT_EQUAL_UINT32(function->typedLocalBindingLength, runtimeFunction->typedLocalBindingLength);
        TEST_ASSERT_EQUAL_UINT32(function->typedExportedSymbolLength, runtimeFunction->typedExportedSymbolLength);
        TEST_ASSERT_EQUAL_UINT32(function->prototypeCount, runtimeFunction->prototypeCount);
        TEST_ASSERT_EQUAL_UINT32(function->prototypeDataLength, runtimeFunction->prototypeDataLength);
        TEST_ASSERT_EQUAL_UINT32(function->semIrTypeTableLength, runtimeFunction->semIrTypeTableLength);
        TEST_ASSERT_EQUAL_UINT32(function->staticImportLength, runtimeFunction->staticImportLength);
        TEST_ASSERT_EQUAL_UINT32(function->topLevelCallableBindingLength, runtimeFunction->topLevelCallableBindingLength);
        if (function->prototypeDataLength > 0) {
            TEST_ASSERT_EQUAL_MEMORY(function->prototypeData, runtimeFunction->prototypeData, function->prototypeDataLength);
        }
        for (TZrUInt32 index = 0; index < function->staticImportLength; ++index) {
            assert_optional_string_equal(function->staticImports[index], runtimeFunction->staticImports[index]);
        }
        for (TZrUInt32 index = 0; index < function->topLevelCallableBindingLength; ++index) {
            const SZrFunctionTopLevelCallableBinding *expectedBinding = &function->topLevelCallableBindings[index];
            const SZrFunctionTopLevelCallableBinding *actualBinding = &runtimeFunction->topLevelCallableBindings[index];

            assert_optional_string_equal(expectedBinding->name, actualBinding->name);
            TEST_ASSERT_EQUAL_UINT32(expectedBinding->stackSlot, actualBinding->stackSlot);
            TEST_ASSERT_EQUAL_UINT32(expectedBinding->callableChildIndex, actualBinding->callableChildIndex);
            TEST_ASSERT_EQUAL_UINT8(expectedBinding->accessModifier, actualBinding->accessModifier);
            TEST_ASSERT_EQUAL_UINT8(expectedBinding->exportKind, actualBinding->exportKind);
            TEST_ASSERT_EQUAL_UINT8(expectedBinding->readiness, actualBinding->readiness);
            TEST_ASSERT_EQUAL_UINT16(expectedBinding->reserved0, actualBinding->reserved0);
        }
        for (TZrUInt32 index = 0; index < function->typedLocalBindingLength; ++index) {
            const SZrFunctionTypedLocalBinding *expectedBinding = &function->typedLocalBindings[index];
            const SZrFunctionTypedLocalBinding *actualBinding = &runtimeFunction->typedLocalBindings[index];

            assert_optional_string_equal(expectedBinding->name, actualBinding->name);
            TEST_ASSERT_EQUAL_UINT32(expectedBinding->stackSlot, actualBinding->stackSlot);
            assert_typed_type_ref_equal(&expectedBinding->type, &actualBinding->type);
        }
        for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; ++index) {
            const SZrFunctionTypedExportSymbol *expectedSymbol = &function->typedExportedSymbols[index];
            const SZrFunctionTypedExportSymbol *actualSymbol = &runtimeFunction->typedExportedSymbols[index];

            assert_optional_string_equal(expectedSymbol->name, actualSymbol->name);
            TEST_ASSERT_EQUAL_UINT32(expectedSymbol->stackSlot, actualSymbol->stackSlot);
            TEST_ASSERT_EQUAL_UINT8(expectedSymbol->accessModifier, actualSymbol->accessModifier);
            TEST_ASSERT_EQUAL_UINT8(expectedSymbol->symbolKind, actualSymbol->symbolKind);
            TEST_ASSERT_EQUAL_UINT8(expectedSymbol->exportKind, actualSymbol->exportKind);
            TEST_ASSERT_EQUAL_UINT8(expectedSymbol->readiness, actualSymbol->readiness);
            TEST_ASSERT_EQUAL_UINT16(expectedSymbol->reserved0, actualSymbol->reserved0);
            TEST_ASSERT_EQUAL_UINT32(expectedSymbol->callableChildIndex, actualSymbol->callableChildIndex);
            TEST_ASSERT_EQUAL_UINT32(expectedSymbol->parameterCount, actualSymbol->parameterCount);
            assert_typed_type_ref_equal(&expectedSymbol->valueType, &actualSymbol->valueType);
        }
        for (TZrUInt32 index = 0; index < function->semIrTypeTableLength; ++index) {
            assert_typed_type_ref_equal(&function->semIrTypeTable[index], &runtimeFunction->semIrTypeTable[index]);
        }

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(23, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_binary_roundtrip_preserves_linked_pair_helper_values(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Binary Roundtrip Preserves Linked Pair Helper Values";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("binary roundtrip linked-list pair path",
                 "Testing that nested Pair construction and LinkedList storage keep helper-call values intact after .zro roundtrip");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *binaryPath = "linked_pair_roundtrip_test.zro";
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_linked_pair_roundtrip_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(16, result);
        result = 0;

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(16, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_binary_roundtrip_preserves_set_pair_hash_and_iteration(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Binary Roundtrip Preserves Set Pair Hash And Iteration";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("binary roundtrip set/pair path",
                 "Testing that Set<Pair<int,string>> keeps duplicate elimination and iteration results after .zro roundtrip");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *binaryPath = "set_pair_roundtrip_test.zro";
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_set_pair_roundtrip_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(410, result);
        result = 0;

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(410, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_binary_roundtrip_preserves_set_to_map_bucket_values(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Binary Roundtrip Preserves Set To Map Bucket Values";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("binary roundtrip set->map bridge",
                 "Testing that Set<Pair<int,string>> iteration can feed Map<string,Array<int>> buckets without corrupting numeric payloads after .zro roundtrip");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *binaryPath = "set_to_map_roundtrip_test.zro";
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_set_to_map_roundtrip_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(122, result);
        result = 0;

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        assert_runtime_function_matches_source_function(function, runtimeFunction);

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(122, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static SZrFunction *compile_meta_access_fixture(SZrState *state) {
    const char *source =
            "class Box {\n"
            "    pub var raw: int;\n"
            "    pub @constructor(raw: int) {\n"
            "        this.raw = raw;\n"
            "    }\n"
            "    pub get value: int {\n"
            "        return this.raw + 1;\n"
            "    }\n"
            "    pub set value(next: int) {\n"
            "        this.raw = next + 2;\n"
            "    }\n"
            "}\n"
            "var box = new Box(3);\n"
            "var first = box.value;\n"
            "box.value = 10;\n"
            "return first + box.raw;";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

        sourceName = ZR_STRING_LITERAL(state, "meta_access_pipeline_test.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_member_slot_quickening_fixture(SZrState *state) {
    const char *source =
            "class Counter {\n"
            "    pub var value: int;\n"
            "}\n"
            "var counter = new Counter();\n"
            "counter.value = 3;\n"
            "counter.value = counter.value + 4;\n"
            "return counter.value + counter.value;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZR_STRING_LITERAL(state, "member_slot_quickening_fixture.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_typed_member_known_call_fixture(SZrState *state) {
    const char *source =
            "class Counter {\n"
            "    pub var value: int;\n"
            "    pub step(delta: int): int {\n"
            "        this.value = this.value + delta;\n"
            "        return this.value;\n"
            "    }\n"
            "    pub read(): int {\n"
            "        return this.value;\n"
            "    }\n"
            "}\n"
            "var counter = new Counter();\n"
            "counter.value = 3;\n"
            "var first = counter.step(4);\n"
            "var second = counter.read();\n"
            "return first + second;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZR_STRING_LITERAL(state, "typed_member_known_call_fixture.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_dynamic_object_member_fallback_fixture(SZrState *state) {
    const char *source =
            "var payload = {a: 1, b: 1.0};\n"
            "payload.a = payload.a + 4;\n"
            "return payload.a;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZR_STRING_LITERAL(state, "dynamic_object_member_fallback_fixture.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_nested_member_slot_quickening_fixture(SZrState *state) {
    const char *source =
            "class Counter {\n"
            "    pub var value: int;\n"
            "}\n"
            "class Holder {\n"
            "    pub var counter: Counter;\n"
            "}\n"
            "var holder = new Holder();\n"
            "holder.counter = new Counter();\n"
            "holder.counter.value = 3;\n"
            "holder.counter.value = holder.counter.value + 4;\n"
            "return holder.counter.value;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZR_STRING_LITERAL(state, "nested_member_slot_quickening_fixture.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_typed_destructuring_member_slot_fixture(SZrState *state) {
    const char *source =
            "class Point {\n"
            "    pub var x: int;\n"
            "    pub var y: int;\n"
            "}\n"
            "var point = new Point();\n"
            "point.x = 2;\n"
            "point.y = 5;\n"
            "var {x, y} = point;\n"
            "return x + y;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZR_STRING_LITERAL(state, "typed_destructuring_member_slot_fixture.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_super_member_dispatch_fixture(SZrState *state) {
    const char *source =
            "class BaseCounter {\n"
            "    pub var baseValue: int;\n"
            "    pub virtual bump(): int {\n"
            "        return this.baseValue + 1;\n"
            "    }\n"
            "    pub virtual get score: int {\n"
            "        return this.baseValue + 10;\n"
            "    }\n"
            "    pub virtual @call(): int {\n"
            "        return this.baseValue + 20;\n"
            "    }\n"
            "}\n"
            "class DerivedCounter: BaseCounter {\n"
            "    pub override bump(): int {\n"
            "        return super.bump() + 1;\n"
            "    }\n"
            "    pub override get score: int {\n"
            "        return super.score + 2;\n"
            "    }\n"
            "    pub override @call(): int {\n"
            "        return super.call() + 5;\n"
            "    }\n"
            "    pub total(): int {\n"
            "        return this.bump() + this.score + this();\n"
            "    }\n"
            "}\n"
            "var counter = new DerivedCounter();\n"
            "counter.baseValue = 1;\n"
            "return counter.total();\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

        sourceName = ZR_STRING_LITERAL(state, "super_member_dispatch_fixture.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_static_meta_access_fixture(SZrState *state) {
    const char *source =
            "class Counter {\n"
            "    pub static get count: int {\n"
            "        return 5;\n"
            "    }\n"
            "    pub static set count(v: int) {\n"
            "        var sink = v;\n"
            "    }\n"
            "}\n"
            "Counter.count = Counter.count + 4;\n"
            "return Counter.count;";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

        sourceName = ZR_STRING_LITERAL(state, "static_meta_access_pipeline_test.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_reference_fixture(SZrState *state,
                                              const char *relativePath,
                                              const char *sourceLabel) {
    SZrString *sourceName;
    size_t sourceSize = 0;
    char *source;
    SZrFunction *function;

    if (state == ZR_NULL || relativePath == ZR_NULL || sourceLabel == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global == ZR_NULL || !ZrVmLibContainer_Register(state->global) || !ZrVmLibMath_Register(state->global) ||
        !ZrVmLibSystem_Register(state->global) || !ZrVmLibFfi_Register(state->global)) {
        return ZR_NULL;
    }

    source = read_reference_file(relativePath, &sourceSize);
    if (source == ZR_NULL || sourceSize == 0) {
        free(source);
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceLabel, strlen(sourceLabel));
    if (sourceName == ZR_NULL) {
        free(source);
        return ZR_NULL;
    }

    function = ZrParser_Source_Compile(state, source, sourceSize, sourceName);
    free(source);
    return function;
}

static SZrFunction *compile_zero_arg_tail_quickening_fixture(SZrState *state) {
    const char *source =
            "func answer(): int {\n"
            "    return 11;\n"
            "}\n"
            "func callDirectTail(): int {\n"
            "    return answer();\n"
            "}\n"
            "class Counter {\n"
            "    pub var base: int;\n"
            "    pub @constructor(base: int) {\n"
            "        this.base = base;\n"
            "    }\n"
            "    pub @call(): int {\n"
            "        return this.base + 1;\n"
            "    }\n"
            "}\n"
            "func callMetaTail(counter: Counter): int {\n"
            "    return counter();\n"
            "}\n"
            "func callDynTail(fn): int {\n"
            "    return fn();\n"
            "}\n"
            "var counter = new Counter(7);\n"
            "return callDirectTail() + callMetaTail(counter) + callDynTail(answer);\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

        sourceName = ZR_STRING_LITERAL(state, "zero_arg_tail_call_site_quickening_test.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_exception_control_fixture(SZrState *state) {
    const char *source =
            "func guarded(flag: int): int {\n"
            "    var marker = 0;\n"
            "    try {\n"
            "        try {\n"
            "            if (flag != 0) {\n"
            "                throw \"boom\";\n"
            "            }\n"
            "            return 0;\n"
            "        } finally {\n"
            "            marker = marker + 7;\n"
            "        }\n"
            "    } catch (e) {\n"
            "        return marker + 1;\n"
            "    }\n"
            "}\n"
            "return guarded(1);\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

        sourceName = ZR_STRING_LITERAL(state, "exception_control_transfer_test.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_cached_meta_and_dynamic_callsite_fixture(SZrState *state) {
    const char *source =
            "class Adder {\n"
            "    pub var base: int;\n"
            "    pub @constructor(base: int) {\n"
            "        this.base = base;\n"
            "    }\n"
            "    pub @call(value: int): int {\n"
            "        return this.base + value;\n"
            "    }\n"
            "}\n"
            "func apply(fn, value: int): int {\n"
            "    var result = fn(value);\n"
            "    return result;\n"
            "}\n"
            "var adder = new Adder(7);\n"
            "return adder(5) + apply(adder, 6);\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

        sourceName = ZR_STRING_LITERAL(state, "cached_meta_dyn_callsite_test.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_ownership_upgrade_release_fixture(SZrState *state) {
    const char *source =
            "class Box {}\n"
            "var owner = %unique new Box();\n"
            "var shared = %shared(owner);\n"
            "var watcher = %weak(shared);\n"
            "var borrowed = %borrow(shared);\n"
            "var borrowedAlive = borrowed != null;\n"
            "var upgraded = %upgrade(watcher);\n"
            "var droppedUpgraded = %release(upgraded);\n"
            "var loanSource = %unique new Box();\n"
            "var loaned = %loan(loanSource);\n"
            "var loanedAlive = loaned != null;\n"
            "var detachSource = %unique new Box();\n"
            "var detached = %detach(detachSource);\n"
            "var detachedAlive = detached != null;\n"
            "var droppedShared = %release(shared);\n"
            "var after = %upgrade(watcher);\n"
            "if (borrowedAlive && loanedAlive && detachedAlive && droppedUpgraded == null && droppedShared == null && after == null) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

        sourceName = ZR_STRING_LITERAL(state, "ownership_upgrade_release_pipeline_test.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_fixed_array_helper_roundtrip_fixture(SZrState *state) {
    const char *source =
            "labelFor(value: int) {\n"
            "    if (value % 2 == 0) {\n"
            "        return \"even\";\n"
            "    }\n"
            "    return \"odd\";\n"
            "}\n"
            "var xs: int[3] = [1, 2, 3];\n"
            "var total = 0;\n"
            "for (var item in xs) {\n"
            "    var label = labelFor(item);\n"
            "    if (label == \"even\") {\n"
            "        total = total + 10;\n"
            "    }\n"
            "    total = total + item;\n"
            "}\n"
            "return total;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state,
                                      "fixed_array_helper_roundtrip_fixture.zr",
                                      strlen("fixed_array_helper_roundtrip_fixture.zr"));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_container_matrix_roundtrip_fixture(SZrState *state) {
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {Array, Map, Set, LinkedList, Pair} = %import(\"zr.container\");\n"
            "labelFor(value: int) {\n"
            "    if (value % 2 == 0) {\n"
            "        return \"even\";\n"
            "    }\n"
            "    if (value > 2) {\n"
            "        return \"odd_hi\";\n"
            "    }\n"
            "    return \"odd_lo\";\n"
            "}\n"
            "var fixedValues: int[6] = [3, 1, 3, 2, 4, 2];\n"
            "var queue = new container.LinkedList<Pair<string, int>>();\n"
            "var seen = new container.Set<Pair<int, string>>();\n"
            "var buckets = new container.Map<string, Array<int>>();\n"
            "for (var item in fixedValues) {\n"
            "    var numeric = <int> item;\n"
            "    queue.addLast(new container.Pair<string, int>(labelFor(numeric), numeric));\n"
            "}\n"
            "while (queue.count > 0) {\n"
            "    var head = queue.first;\n"
            "    var task = head.value;\n"
            "    var next = head.next;\n"
            "    if (next != null) {\n"
            "        next.previous = null;\n"
            "        queue.first = next;\n"
            "    } else {\n"
            "        queue.first = null;\n"
            "        queue.last = null;\n"
            "    }\n"
            "    head.next = null;\n"
            "    head.previous = null;\n"
            "    queue.count = queue.count - 1;\n"
            "    seen.add(new container.Pair<int, string>(<int> task.second, <string> task.first));\n"
            "}\n"
            "for (var pair in seen) {\n"
            "    var label = <string> pair.second;\n"
            "    var value = <int> pair.first;\n"
            "    var bucket = buckets[label];\n"
            "    if (bucket == null) {\n"
            "        bucket = new container.Array<int>();\n"
            "        buckets[label] = bucket;\n"
            "    }\n"
            "    bucket.add(value);\n"
            "}\n"
            "var oddLo: Array<int> = buckets[\"odd_lo\"];\n"
            "var oddHi: Array<int> = buckets[\"odd_hi\"];\n"
            "var even: Array<int> = buckets[\"even\"];\n"
            "var oddLoLen = oddLo == null ? 0 : oddLo.length;\n"
            "var oddHiLen = oddHi == null ? 0 : oddHi.length;\n"
            "var evenLen = even == null ? 0 : even.length;\n"
            "var oddLoSum = 0;\n"
            "var oddHiSum = 0;\n"
            "if (oddLo != null) {\n"
            "    for (var item in oddLo) {\n"
            "        oddLoSum = oddLoSum + <int> item;\n"
            "    }\n"
            "}\n"
            "if (oddHi != null) {\n"
            "    for (var item in oddHi) {\n"
            "        oddHiSum = oddHiSum + <int> item;\n"
            "    }\n"
            "}\n"
            "return seen.count * 100 + oddLoLen * 10 + oddHiLen + evenLen + oddLoSum + oddHiSum;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global == ZR_NULL || !ZrVmLibContainer_Register(state->global) || !ZrVmLibMath_Register(state->global) ||
        !ZrVmLibSystem_Register(state->global) || !ZrVmLibFfi_Register(state->global)) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state,
                                      "container_matrix_roundtrip_fixture.zr",
                                      strlen("container_matrix_roundtrip_fixture.zr"));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_array_int_index_quickening_fixture(SZrState *state) {
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var xs = new container.Array<int>();\n"
            "xs.add(10);\n"
            "xs.add(20);\n"
            "xs.add(30);\n"
            "var index = 1;\n"
            "var value = xs[index];\n"
            "xs[index] = value + 5;\n"
            "var scaled = xs[index] * 3;\n"
            "var reduced = xs[0] / 2;\n"
            "var mixed = xs[2] % 7;\n"
            "return scaled - reduced + mixed + xs[0];\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global == ZR_NULL || !ZrVmLibContainer_Register(state->global) || !ZrVmLibMath_Register(state->global) ||
        !ZrVmLibSystem_Register(state->global) || !ZrVmLibFfi_Register(state->global)) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state,
                                      "array_int_index_quickening_fixture.zr",
                                      strlen("array_int_index_quickening_fixture.zr"));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_array_int_add_burst_fixture(SZrState *state) {
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var a = new container.Array<int>();\n"
            "var b = new container.Array<int>();\n"
            "var c = new container.Array<int>();\n"
            "var d = new container.Array<int>();\n"
            "a.add(0);\n"
            "b.add(0);\n"
            "c.add(0);\n"
            "d.add(0);\n"
            "a[0] = 10;\n"
            "b[0] = 20;\n"
            "c[0] = 30;\n"
            "d[0] = 40;\n"
            "return a[0] + b[0] + c[0] + d[0];\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global == ZR_NULL || !ZrVmLibContainer_Register(state->global) || !ZrVmLibMath_Register(state->global) ||
        !ZrVmLibSystem_Register(state->global) || !ZrVmLibFfi_Register(state->global)) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state,
                                      "array_int_add_burst_fixture.zr",
                                      strlen("array_int_add_burst_fixture.zr"));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_array_int_fill_loop_fixture(SZrState *state) {
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var a = new container.Array<int>();\n"
            "var b = new container.Array<int>();\n"
            "var c = new container.Array<int>();\n"
            "var d = new container.Array<int>();\n"
            "var count = 6;\n"
            "var index = 0;\n"
            "while (index <= count - 1) {\n"
            "    a.add(0);\n"
            "    b.add(0);\n"
            "    c.add(0);\n"
            "    d.add(0);\n"
            "    index = index + 1;\n"
            "}\n"
            "index = 0;\n"
            "a[0] = 1;\n"
            "b[5] = 2;\n"
            "c[3] = 4;\n"
            "d[2] = 8;\n"
            "return a.length + b.length + c.length + d.length + a[0] + b[5] + c[3] + d[2];\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global == ZR_NULL || !ZrVmLibContainer_Register(state->global) || !ZrVmLibMath_Register(state->global) ||
        !ZrVmLibSystem_Register(state->global) || !ZrVmLibFfi_Register(state->global)) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state,
                                      "array_int_fill_loop_fixture.zr",
                                      strlen("array_int_fill_loop_fixture.zr"));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_map_array_roundtrip_fixture(SZrState *state) {
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {Array, Map} = %import(\"zr.container\");\n"
            "var buckets = new container.Map<string, Array<int>>();\n"
            "var bucket = new container.Array<int>();\n"
            "bucket.add(1);\n"
            "bucket.add(2);\n"
            "buckets[\"a\"] = bucket;\n"
            "var fetched: Array<int> = buckets[\"a\"];\n"
            "var length = fetched == null ? 0 : fetched.length;\n"
            "var sum = 0;\n"
            "if (fetched != null) {\n"
            "    for (var item in fetched) {\n"
            "        sum = sum + <int> item;\n"
            "    }\n"
            "}\n"
            "return length * 10 + sum;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global == ZR_NULL || !ZrVmLibContainer_Register(state->global) || !ZrVmLibMath_Register(state->global) ||
        !ZrVmLibSystem_Register(state->global) || !ZrVmLibFfi_Register(state->global)) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state,
                                      "map_array_roundtrip_fixture.zr",
                                      strlen("map_array_roundtrip_fixture.zr"));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_linked_pair_roundtrip_fixture(SZrState *state) {
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {LinkedList, Pair} = %import(\"zr.container\");\n"
            "labelFor(value: int) {\n"
            "    if (value % 2 == 0) {\n"
            "        return \"even\";\n"
            "    }\n"
            "    return \"odd\";\n"
            "}\n"
            "var xs: int[3] = [1, 2, 3];\n"
            "var queue = new container.LinkedList<Pair<string, int>>();\n"
            "for (var item in xs) {\n"
            "    queue.addLast(new container.Pair<string, int>(labelFor(item), item));\n"
            "}\n"
            "var total = 0;\n"
            "while (queue.count > 0) {\n"
            "    var head = queue.first;\n"
            "    var pair = head.value;\n"
            "    if (<string> pair.first == \"even\") {\n"
            "        total = total + 10;\n"
            "    }\n"
            "    total = total + <int> pair.second;\n"
            "    queue.removeFirst();\n"
            "}\n"
            "return total;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global == ZR_NULL || !ZrVmLibContainer_Register(state->global) || !ZrVmLibMath_Register(state->global) ||
        !ZrVmLibSystem_Register(state->global) || !ZrVmLibFfi_Register(state->global)) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state,
                                      "linked_pair_roundtrip_fixture.zr",
                                      strlen("linked_pair_roundtrip_fixture.zr"));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_set_pair_roundtrip_fixture(SZrState *state) {
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {Set, Pair} = %import(\"zr.container\");\n"
            "var seen = new container.Set<Pair<int, string>>();\n"
            "seen.add(new container.Pair<int, string>(3, \"odd_hi\"));\n"
            "seen.add(new container.Pair<int, string>(1, \"odd_lo\"));\n"
            "seen.add(new container.Pair<int, string>(2, \"even\"));\n"
            "seen.add(new container.Pair<int, string>(4, \"even\"));\n"
            "seen.add(new container.Pair<int, string>(2, \"even\"));\n"
            "var total = seen.count * 100;\n"
            "for (var pair in seen) {\n"
            "    total = total + <int> pair.first;\n"
            "}\n"
            "return total;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global == ZR_NULL || !ZrVmLibContainer_Register(state->global) || !ZrVmLibMath_Register(state->global) ||
        !ZrVmLibSystem_Register(state->global) || !ZrVmLibFfi_Register(state->global)) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state,
                                      "set_pair_roundtrip_fixture.zr",
                                      strlen("set_pair_roundtrip_fixture.zr"));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_set_to_map_roundtrip_fixture(SZrState *state) {
    const char *source =
            "var container = %import(\"zr.container\");\n"
            "var {Array, Map, Set, Pair} = %import(\"zr.container\");\n"
            "var seen = new container.Set<Pair<int, string>>();\n"
            "seen.add(new container.Pair<int, string>(3, \"odd_hi\"));\n"
            "seen.add(new container.Pair<int, string>(1, \"odd_lo\"));\n"
            "seen.add(new container.Pair<int, string>(2, \"even\"));\n"
            "seen.add(new container.Pair<int, string>(4, \"even\"));\n"
            "var buckets = new container.Map<string, Array<int>>();\n"
            "for (var pair in seen) {\n"
            "    var label = <string> pair.second;\n"
            "    var value = <int> pair.first;\n"
            "    var bucket = buckets[label];\n"
            "    if (bucket == null) {\n"
            "        bucket = new container.Array<int>();\n"
            "        buckets[label] = bucket;\n"
            "    }\n"
            "    bucket.add(value);\n"
            "}\n"
            "var oddLo: Array<int> = buckets[\"odd_lo\"];\n"
            "var oddHi: Array<int> = buckets[\"odd_hi\"];\n"
            "var even: Array<int> = buckets[\"even\"];\n"
            "var total = 0;\n"
            "if (oddLo != null) {\n"
            "    for (var item in oddLo) {\n"
            "        total = total + <int> item;\n"
            "    }\n"
            "}\n"
            "if (oddHi != null) {\n"
            "    for (var item in oddHi) {\n"
            "        total = total + <int> item;\n"
            "    }\n"
            "}\n"
            "if (even != null) {\n"
            "    for (var item in even) {\n"
            "        total = total + <int> item;\n"
            "    }\n"
            "}\n"
            "return total + (oddLo == null ? 0 : oddLo.length) * 100 + (oddHi == null ? 0 : oddHi.length) * 10 + (even == null ? 0 : even.length);\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    if (state->global == ZR_NULL || !ZrVmLibContainer_Register(state->global) || !ZrVmLibMath_Register(state->global) ||
        !ZrVmLibSystem_Register(state->global) || !ZrVmLibFfi_Register(state->global)) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state,
                                      "set_to_map_roundtrip_fixture.zr",
                                      strlen("set_to_map_roundtrip_fixture.zr"));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static void test_meta_access_semir_and_true_aot_c_preserve_dedicated_meta_get_set_opcodes(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Meta Access SemIR And True AOT C Preserve Dedicated Meta Get Set Opcodes";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("meta get/set semantic pipeline",
                 "Testing that property getter/setter lowering keeps direct META_GET and META_SET contracts in SemIR while true AOT C and AOT LLVM preserve the same callable metadata contract");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "meta_access_pipeline_test.zri";
        const char *cPath = "meta_access_pipeline_test.c";
        const char *llvmPath = "meta_access_pipeline_test.ll";
        const char *binaryPath = "meta_access_pipeline_test.zro";
        SZrFunction *function;
        char *intermediateText;
        char *llvmText;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_meta_access_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(META_GET)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(META_SET)));
        TEST_ASSERT_TRUE(function->callSiteCacheLength >= 2);
        TEST_ASSERT_EQUAL_UINT32(1u, function_count_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET));
        TEST_ASSERT_EQUAL_UINT32(1u, function_count_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET));
        TEST_ASSERT_TRUE(function_contains_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET));
        TEST_ASSERT_TRUE(function_contains_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET));
        assert_member_entry_symbol_flags(function, "__get_value", 0);
        assert_member_entry_symbol_flags(function, "__set_value", 0);
        assert_first_callsite_cache_member_binding(function,
                                                   ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET,
                                                   "__get_value",
                                                   0);
        assert_first_callsite_cache_member_binding(function,
                                                   ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET,
                                                   "__set_value",
                                                   0);
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_GET, ZR_TRUE));
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_SET, ZR_TRUE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));

        intermediateText = read_text_file_owned(intermediatePath);
        {
            char *cText = read_text_file_owned(cPath);
            TEST_ASSERT_NOT_NULL(cText);
            TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_MetaGetCached(state, &frame,"));
            TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_MetaSetCached(state, &frame,"));
            TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
            free(cText);
        }
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "CALLSITE_CACHE_TABLE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_GET_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_SET_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_GET"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_SET"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "META_GET"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "META_SET"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "declare ptr @ZrCore_Function_PreCall"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_MetaGetCached("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_MetaSetCached("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED)));

        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        TEST_ASSERT_TRUE(sourceObject->modulesLength > 0);
        TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);
        TEST_ASSERT_EQUAL_UINT32(function->callSiteCacheLength,
                                 (TZrUInt32)sourceObject->modules[0].entryFunction->callSiteCacheLength);

        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_EQUAL_UINT32(function->callSiteCacheLength, runtimeFunction->callSiteCacheLength);
        TEST_ASSERT_EQUAL_UINT32(1u,
                                 function_count_callsite_cache_kind(runtimeFunction,
                                                                    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET));
        TEST_ASSERT_EQUAL_UINT32(1u,
                                 function_count_callsite_cache_kind(runtimeFunction,
                                                                    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET));
        TEST_ASSERT_TRUE(function_contains_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET));
        TEST_ASSERT_TRUE(function_contains_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET));
        assert_first_callsite_cache_member_binding(runtimeFunction,
                                                   ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET,
                                                   "__get_value",
                                                   0);
        assert_first_callsite_cache_member_binding(runtimeFunction,
                                                   ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET,
                                                   "__set_value",
                                                   0);

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(16, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        free(intermediateText);
        free(llvmText);
        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_static_meta_access_quickens_to_static_callsite_cache_variants(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Static Meta Access Quickens To Static Callsite Cache Variants";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("static meta access quickening",
                 "Testing that static property getter/setter sites lower to dedicated static cached ExecBC opcodes while SemIR and AOT keep META_GET/META_SET contracts");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "static_meta_access_pipeline_test.zri";
        const char *cPath = "static_meta_access_pipeline_test.c";
        const char *llvmPath = "static_meta_access_pipeline_test.ll";
        const char *binaryPath = "static_meta_access_pipeline_test.zro";
        SZrFunction *function;
        char *intermediateText;
        char *cText;
        char *llvmText;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_static_meta_access_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        assert_member_entry_symbol_flags(function, "__get_count", ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR);
        assert_member_entry_symbol_flags(function, "__set_count", ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED)));
        TEST_ASSERT_EQUAL_UINT32(3, function->callSiteCacheLength);
        TEST_ASSERT_EQUAL_UINT32(2,
                                 function_count_callsite_cache_kind(function,
                                                                    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC));
        TEST_ASSERT_EQUAL_UINT32(1,
                                 function_count_callsite_cache_kind(function,
                                                                    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC));
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_GET, ZR_TRUE));
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_SET, ZR_TRUE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));

        intermediateText = read_text_file_owned(intermediatePath);
        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "CALLSITE_CACHE_TABLE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_GET_STATIC"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_SET_STATIC"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_GET_STATIC_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_SET_STATIC_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_GET"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_SET"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_MetaGetStaticCached(state, &frame,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_MetaSetStaticCached(state, &frame,"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_MetaGetStaticCached("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_MetaSetStaticCached("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(
                llvmText,
                ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(
                llvmText,
                ZR_INSTRUCTION_ENUM(SUPER_META_SET_STATIC_CACHED)));

        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        TEST_ASSERT_TRUE(sourceObject->modulesLength > 0);
        TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);
        TEST_ASSERT_EQUAL_UINT32(3, (TZrUInt32)sourceObject->modules[0].entryFunction->callSiteCacheLength);

        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_EQUAL_UINT32(3, runtimeFunction->callSiteCacheLength);
        TEST_ASSERT_EQUAL_UINT32(2,
                                 function_count_callsite_cache_kind(runtimeFunction,
                                                                    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET_STATIC));
        TEST_ASSERT_EQUAL_UINT32(1,
                                 function_count_callsite_cache_kind(runtimeFunction,
                                                                    ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET_STATIC));

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        free(intermediateText);
        free(cText);
        free(llvmText);
        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_super_member_dispatch_uses_direct_base_lookup_for_methods_properties_and_meta_calls(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Super Member Dispatch Uses Direct Base Lookup For Methods Properties And Meta Calls";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("super member lowering",
                 "Testing that super.method, super.property, and super meta calls lower without a runtime 'super' lookup and execute against the direct base implementation");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_super_member_dispatch_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_FALSE(function_contains_get_member_name(function, "super"));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(42, result);

        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_typed_member_access_allocates_member_callsite_cache_entries(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Typed Member Access Allocates Member Callsite Cache Entries";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("member slot quickening",
                 "Testing that typed class field access stops staying on plain GET_MEMBER/SET_MEMBER and starts allocating dedicated member callsite cache metadata");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *binaryPath = "member_slot_quickening_test.zro";
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_member_slot_quickening_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SET_MEMBER)));
        TEST_ASSERT_TRUE(function_count_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET) > 0);
        TEST_ASSERT_TRUE(function_count_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET) > 0);
        TEST_ASSERT_TRUE(function->callSiteCacheLength >= 2);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(14, result);

        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);

        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SET_MEMBER)));
        TEST_ASSERT_TRUE(function_count_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET) > 0);
        TEST_ASSERT_TRUE(function_count_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET) > 0);
        TEST_ASSERT_TRUE(runtimeFunction->callSiteCacheLength >= 2);

        result = 0;
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(14, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_typed_member_calls_quicken_to_known_vm_call_family(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Typed Member Calls Quicken To Known VM Call Family";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("member call quickening",
                 "Testing that typed instance method calls stop staying on generic FUNCTION_CALL and instead lower into GET_MEMBER_SLOT plus the KNOWN_VM_CALL family, including zero-arg variants.");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *binaryPath = "typed_member_known_call_test.zro";
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_typed_member_known_call_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(FUNCTION_CALL)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS)));
        TEST_ASSERT_TRUE(function_count_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET) >= 2);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(14, result);

        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);

        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(FUNCTION_CALL)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS)));
        TEST_ASSERT_TRUE(function_count_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET) >= 2);

        result = 0;
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(14, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_dynamic_object_member_access_stays_on_generic_member_opcodes(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Dynamic Object Member Access Stays On Generic Member Opcodes";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("dynamic object member fallback",
                 "Testing that plain object literals keep GET_MEMBER/SET_MEMBER access and do not get incorrectly quickened into typed member-slot opcodes.");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *binaryPath = "dynamic_object_member_fallback_test.zro";
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_dynamic_object_member_fallback_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SET_MEMBER)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT)));
        TEST_ASSERT_EQUAL_UINT32(0, function_count_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET));
        TEST_ASSERT_EQUAL_UINT32(0, function_count_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(5, result);

        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);

        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SET_MEMBER)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT)));
        TEST_ASSERT_EQUAL_UINT32(0, function_count_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET));
        TEST_ASSERT_EQUAL_UINT32(0, function_count_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET));

        result = 0;
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(5, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_nested_typed_member_chain_emits_member_slot_opcodes(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Nested Typed Member Chain Emits Member Slot Opcodes";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("member slot nested chain quickening",
                 "Testing that strongly typed chained field access lowers all the way to member-slot opcodes instead of falling back to generic GET_MEMBER/SET_MEMBER on temporary receivers");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *binaryPath = "nested_member_slot_quickening_test.zro";
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_nested_member_slot_quickening_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SET_MEMBER)));
        TEST_ASSERT_TRUE(function_count_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET) >= 2);
        TEST_ASSERT_TRUE(function_count_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET) >= 2);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(7, result);

        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);

        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SET_MEMBER)));
        TEST_ASSERT_TRUE(function_count_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET) >= 2);
        TEST_ASSERT_TRUE(function_count_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_SET) >= 2);

        result = 0;
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(7, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        remove(binaryPath);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_typed_object_destructuring_emits_member_slot_opcodes(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Typed Object Destructuring Emits Member Slot Opcodes";
    const char *fixtureSource =
            "class Point {\n"
            "    pub var x: int;\n"
            "    pub var y: int;\n"
            "}\n"
            "var point = new Point();\n"
            "point.x = 2;\n"
            "point.y = 5;\n"
            "var {x, y} = point;\n"
            "return x + y;\n";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("member slot destructuring",
                 "Testing that strongly typed object destructuring lowers field extraction through member-slot opcodes before quickening, without relying on generic GET_MEMBER fallback");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *binaryPath = "typed_destructuring_member_slot_test.zro";
        SZrFunction *rawFunction;
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        rawFunction = compile_source_without_quickening(state,
                                                        fixtureSource,
                                                        "typed_destructuring_member_slot_fixture_raw.zr");
        TEST_ASSERT_NOT_NULL(rawFunction);
        TEST_ASSERT_TRUE(function_contains_opcode(rawFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_FALSE(function_contains_opcode(rawFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        ZrCore_Function_Free(state, rawFunction);

        function = compile_typed_destructuring_member_slot_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_TRUE(function_count_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET) >= 2);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(7, result);

        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);

        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_TRUE(function_count_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET) >= 2);

        result = 0;
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(7, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        remove(binaryPath);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_reference_property_fixture_preserves_meta_access_artifacts_with_true_aot_c_lowering(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Reference Property Fixture Preserves Meta Access Artifacts With True AOT C Lowering";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("reference property artifact pipeline",
                 "Testing that the reference property precedence fixture keeps quickened ExecBC meta access while SemIR, AOT C, and AOT LLVM preserve META_GET and META_SET contracts");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "reference_property_getter_setter_precedence.zri";
        const char *cPath = "reference_property_getter_setter_precedence.c";
        const char *llvmPath = "reference_property_getter_setter_precedence.ll";
        SZrFunction *function;
        char *intermediateText;
        char *llvmText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_reference_fixture(state,
                                             "core_semantics/object_member_index_construct_target/property_getter_setter_precedence.zr",
                                             "reference_property_getter_setter_precedence.zr");
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED)));
        TEST_ASSERT_TRUE(function_contains_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET));
        TEST_ASSERT_TRUE(function_contains_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET));
        assert_member_entry_symbol_flags(function, "__get_value", 0);
        assert_member_entry_symbol_flags(function, "__set_value", 0);
        assert_first_callsite_cache_member_binding(function,
                                                   ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET,
                                                   "__get_value",
                                                   0);
        assert_first_callsite_cache_member_binding(function,
                                                   ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET,
                                                   "__set_value",
                                                   0);
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_GET, ZR_TRUE));
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_SET, ZR_TRUE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));

        intermediateText = read_text_file_owned(intermediatePath);
        {
            char *cText = read_text_file_owned(cPath);
            TEST_ASSERT_NOT_NULL(cText);
            TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_MetaGetCached(state, &frame,"));
            TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_MetaSetCached(state, &frame,"));
            TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
            free(cText);
        }
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_GET_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_SET_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_GET"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_SET"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "META_GET"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "META_SET"));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(4142, result);

        free(intermediateText);
        free(llvmText);
        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_reference_member_index_fixture_preserves_split_access_artifacts(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Reference Member Index Fixture Preserves Split Access Artifacts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("reference member/index artifact pipeline",
                 "Testing that the reference member-vs-index fixture preserves distinct access opcodes in intermediate artifacts without legacy table fallback");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "reference_member_vs_string_index_split.zri";
        SZrFunction *function;
        char *intermediateText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_reference_fixture(state,
                                             "core_semantics/object_member_index_construct_target/member_vs_string_index_split.zr",
                                             "reference_member_vs_string_index_split.zr");
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SET_BY_INDEX)));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "GET_MEMBER"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "GET_BY_INDEX"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SET_BY_INDEX"));
        TEST_ASSERT_NULL(strstr(intermediateText, "GETTABLE"));
        TEST_ASSERT_NULL(strstr(intermediateText, "SETTABLE"));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(17, result);

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_reference_foreach_fixture_preserves_iter_contract_artifacts(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Reference Foreach Fixture Preserves Iterator Contract Artifacts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("reference foreach artifact pipeline",
                 "Testing that the reference foreach fixture preserves iterator contract opcodes and does not regress to named member probes");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "reference_foreach_contract_lowering.zri";
        SZrFunction *function;
        char *intermediateText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_reference_fixture(state,
                                             "core_semantics/protocols_iteration_comparable/foreach_contract_lowering.zr",
                                             "reference_foreach_contract_lowering.zr");
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ITER_INIT)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ITER_CURRENT)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "ITER_INIT"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "ITER_MOVE_NEXT"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "ITER_CURRENT"));
        TEST_ASSERT_NULL(strstr(intermediateText, "getIterator"));
        TEST_ASSERT_NULL(strstr(intermediateText, "moveNext"));
        TEST_ASSERT_NULL(strstr(intermediateText, "current"));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(6, result);

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_execbc_quickens_zero_arg_tail_call_sites_without_changing_semir_contracts(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "ExecBC Quickens Zero Arg Tail Call Sites Without Changing SemIR Contracts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("execbc tail call-site quickening",
                 "Testing that zero-argument direct, dynamic, and meta tail call sites quicken only in ExecBC while SemIR keeps the tail semantic runtime opcodes");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "execbc_tail_call_site_quickening_test.zri";
        SZrFunction *function;
        char *intermediateText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_zero_arg_tail_quickening_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_TAIL_CALL_NO_ARGS)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_NO_ARGS)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(META_TAIL_CALL)));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_DYN_TAIL_CALL, ZR_TRUE));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_TAIL_CALL, ZR_TRUE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_FUNCTION_TAIL_CALL_NO_ARGS"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_DYN_TAIL_CALL_NO_ARGS"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_TAIL_CALL_NO_ARGS"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "DYN_TAIL_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_TAIL_CALL"));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(30, result);

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_execbc_quickens_array_int_index_sites_and_true_aot_lowers_specialized_helpers(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "ExecBC Quickens Array Int Add Index Sites And True AOT Lowers Specialized Helpers";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("array int index quickening",
                 "Testing that statically typed container.Array<int> add/index sites quicken to SUPER_ARRAY_* helpers, downstream int arithmetic stays specialized in ExecBC, and AOT C plus LLVM lower those sites to specialized runtime helpers");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "array_int_index_quickening_test.zri";
        const char *cPath = "array_int_index_quickening_test.c";
        const char *llvmPath = "array_int_index_quickening_test.ll";
        const char *binaryPath = "array_int_index_quickening_test.zro";
        SZrFunction *function;
        char *intermediateText;
        char *cText;
        char *llvmText;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_array_int_index_quickening_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT)));
        TEST_ASSERT_TRUE(function_contains_super_array_get_int_family(function));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT)));
        TEST_ASSERT_TRUE(function_contains_add_int_family(function) ||
                         function_contains_add_signed_family(function));
        TEST_ASSERT_TRUE(function_contains_sub_int_family(function) ||
                         function_contains_sub_signed_family(function));
        TEST_ASSERT_TRUE(function_contains_mul_signed_family(function));
        TEST_ASSERT_TRUE(function_contains_div_signed_family(function));
        TEST_ASSERT_TRUE(function_contains_mod_signed_family(function));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DIV)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MOD)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SET_BY_INDEX)));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));

        intermediateText = read_text_file_owned(intermediatePath);
        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_ARRAY_ADD_INT"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_ARRAY_GET_INT"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_ARRAY_SET_INT"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_SuperArrayAddInt"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_SuperArrayGetInt"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_SuperArraySetInt"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SuperArrayAddInt("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SuperArrayGetInt("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SuperArraySetInt("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT)));
        TEST_ASSERT_FALSE(
                aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST)));
        TEST_ASSERT_FALSE(
                aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST)));
        TEST_ASSERT_FALSE(
                aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST)));
        TEST_ASSERT_FALSE(
                aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST)));
        TEST_ASSERT_FALSE(
                aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST)));
        TEST_ASSERT_FALSE(
                aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST)));
        TEST_ASSERT_FALSE(
                aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(82, result);

        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT)));
        TEST_ASSERT_TRUE(function_contains_super_array_get_int_family(runtimeFunction));
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT)));
        TEST_ASSERT_TRUE(function_contains_add_int_family(runtimeFunction) ||
                         function_contains_add_signed_family(runtimeFunction));
        TEST_ASSERT_TRUE(function_contains_sub_int_family(runtimeFunction) ||
                         function_contains_sub_signed_family(runtimeFunction));
        TEST_ASSERT_TRUE(function_contains_mul_signed_family(runtimeFunction));
        TEST_ASSERT_TRUE(function_contains_div_signed_family(runtimeFunction));
        TEST_ASSERT_TRUE(function_contains_mod_signed_family(runtimeFunction));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(ADD)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SUB)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(MUL)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(DIV)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(MOD)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SET_BY_INDEX)));

        result = 0;
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(82, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        free(intermediateText);
        free(cText);
        free(llvmText);
        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_execbc_quickens_array_int_add_bursts_to_dedicated_dispatch_opcode(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "ExecBC Quickens Consecutive Array Int Adds To Burst Opcode And AOT Lowers The Helper";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("array int add burst quickening",
                 "Testing that four consecutive container.Array<int>.add(value) sites sharing one payload quicken into SUPER_ARRAY_ADD_INT4 and that binary plus AOT outputs preserve the burst helper");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "array_int_add_burst_test.zri";
        const char *cPath = "array_int_add_burst_test.c";
        const char *llvmPath = "array_int_add_burst_test.ll";
        const char *binaryPath = "array_int_add_burst_test.zro";
        SZrFunction *function;
        char *intermediateText;
        char *cText;
        char *llvmText;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_array_int_add_burst_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4)) ||
                         function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST)));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));

        intermediateText = read_text_file_owned(intermediatePath);
        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_ARRAY_ADD_INT4"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_SuperArrayAddInt4"));
        TEST_ASSERT_TRUE(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SuperArrayAddInt4(") != ZR_NULL ||
                         strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SuperArrayAddInt4Const(") != ZR_NULL);
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(100, result);

        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4)) ||
                         function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT4_CONST)));

        result = 0;
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(100, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        free(intermediateText);
        free(cText);
        free(llvmText);
        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_execbc_quickens_array_int_fill_loops_to_bulk_dispatch_opcode(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "ExecBC Quickens Array Int Fill Loops To Bulk Opcode And AOT Lowers The Helper";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("array int fill loop quickening",
                 "Testing that counted loops which append the same int constant into four container.Array<int> values quicken into SUPER_ARRAY_FILL_INT4_CONST and that binary plus AOT outputs preserve the bulk helper");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "array_int_fill_loop_test.zri";
        const char *cPath = "array_int_fill_loop_test.c";
        const char *llvmPath = "array_int_fill_loop_test.ll";
        const char *binaryPath = "array_int_fill_loop_test.zro";
        SZrFunction *function;
        char *intermediateText;
        char *cText;
        char *llvmText;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_array_int_fill_loop_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST)));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));

        intermediateText = read_text_file_owned(intermediatePath);
        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_ARRAY_FILL_INT4_CONST"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_SuperArrayFillInt4Const"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SuperArrayFillInt4Const("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(39, result);

        binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        reader.consumed = ZR_FALSE;

        io = ZrCore_Io_New(state->global);
        TEST_ASSERT_NOT_NULL(io);
        ZrCore_Io_Init(state, io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io->isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_FILL_INT4_CONST)));

        result = 0;
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(39, result);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_Free(state->global, io);
        free(binaryBytes);
        free(intermediateText);
        free(cText);
        free(llvmText);
        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_ownership_upgrade_release_semir_and_true_aot_c_preserve_dedicated_opcodes(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Ownership Borrow Loan Detach Upgrade Release SemIR And True AOT C Preserve Dedicated Opcodes";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("ownership borrow/loan/detach semantic pipeline",
                 "Testing that %shared/%weak/%borrow/%loan/%detach/%upgrade/%release keep dedicated ExecBC and SemIR ownership opcodes, and that true AOT C plus AOT LLVM lower them to explicit runtime helpers without shim fallback");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "ownership_upgrade_release_pipeline_test.zri";
        const char *cPath = "ownership_upgrade_release_pipeline_test.c";
        const char *llvmPath = "ownership_upgrade_release_pipeline_test.ll";
        SZrFunction *function;
        char *intermediateText;
        char *cText;
        char *llvmText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_ownership_upgrade_release_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_SHARE)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_WEAK)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_BORROW)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_LOAN)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_DETACH)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_UPGRADE)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));
        TEST_ASSERT_TRUE(semir_contains_opcode(function, ZR_SEMIR_OPCODE_OWN_SHARE));
        TEST_ASSERT_TRUE(semir_contains_opcode(function, ZR_SEMIR_OPCODE_OWN_WEAK));
        TEST_ASSERT_TRUE(semir_contains_opcode(function, ZR_SEMIR_OPCODE_OWN_BORROW));
        TEST_ASSERT_TRUE(semir_contains_opcode(function, ZR_SEMIR_OPCODE_OWN_LOAN));
        TEST_ASSERT_TRUE(semir_contains_opcode(function, ZR_SEMIR_OPCODE_OWN_DETACH));
        TEST_ASSERT_TRUE(semir_contains_opcode(function, ZR_SEMIR_OPCODE_OWN_UPGRADE));
        TEST_ASSERT_TRUE(semir_contains_opcode(function, ZR_SEMIR_OPCODE_OWN_RELEASE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));

        intermediateText = read_text_file_owned(intermediatePath);
        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_SHARE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_WEAK"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_BORROW"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_LOAN"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_DETACH"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_UPGRADE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_RELEASE"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnShare"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnWeak"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnBorrow"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnLoan"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnDetach"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnUpgrade"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnRelease"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnShare("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnWeak("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnBorrow("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnLoan("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnDetach("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnUpgrade("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnRelease("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_SHARE)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_WEAK)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_BORROW)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_LOAN)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_DETACH)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_UPGRADE)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(1, result);

        free(intermediateText);
        free(cText);
        free(llvmText);
        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_benchmark_string_build_binary_roundtrip_loads_runtime_entry(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Benchmark String Build Binary Roundtrip Loads Runtime Entry";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("benchmark binary roundtrip loading",
                 "Testing that the checked-in string_build benchmark main.zro parses and reloads into a runtime function instead of failing at the project entry load step");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrBinaryFixtureReader reader;
        TZrByte *binaryBytes = ZR_NULL;
        TZrSize binaryLength = 0;
        SZrIo io;
        SZrIoSource *sourceObject = ZR_NULL;
        SZrFunction *runtimeFunction = ZR_NULL;
        TZrBool hasToString;
        TZrBool hasAddString;

        TEST_ASSERT_NOT_NULL(state);

        binaryBytes = read_repo_binary_file_owned("tests/benchmarks/cases/string_build/zr/bin/main.zro", &binaryLength);
        TEST_ASSERT_NOT_NULL(binaryBytes);
        TEST_ASSERT_TRUE(binaryLength > 0);

        memset(&reader, 0, sizeof(reader));
        reader.bytes = binaryBytes;
        reader.length = binaryLength;
        ZrCore_Io_Init(state, &io, binary_fixture_reader_read, binary_fixture_reader_close, &reader);
        io.isBinary = ZR_TRUE;

        sourceObject = ZrCore_Io_ReadSourceNew(&io);
        TEST_ASSERT_NOT_NULL(sourceObject);
        TEST_ASSERT_EQUAL_UINT32(1u, sourceObject->modulesLength);
        TEST_ASSERT_NOT_NULL(sourceObject->modules[0].entryFunction);

        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        hasToString = function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(TO_STRING));
        hasAddString = function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(ADD_STRING));
        TEST_ASSERT_TRUE(hasToString);
        TEST_ASSERT_TRUE(hasAddString);

        ZrCore_Function_Free(state, runtimeFunction);
        free(binaryBytes);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_backends_lower_manual_extended_numeric_opcode_fixture(void) {
    static const EZrInstructionCode coveredOpcodes[] = {
            ZR_INSTRUCTION_ENUM(TO_BOOL),
            ZR_INSTRUCTION_ENUM(TO_UINT),
            ZR_INSTRUCTION_ENUM(TO_FLOAT),
            ZR_INSTRUCTION_ENUM(ADD_INT_CONST),
            ZR_INSTRUCTION_ENUM(ADD_FLOAT),
            ZR_INSTRUCTION_ENUM(SUB_INT_CONST),
            ZR_INSTRUCTION_ENUM(SUB_FLOAT),
            ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST),
            ZR_INSTRUCTION_ENUM(MUL_UNSIGNED),
            ZR_INSTRUCTION_ENUM(MUL_FLOAT),
            ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST),
            ZR_INSTRUCTION_ENUM(DIV_UNSIGNED),
            ZR_INSTRUCTION_ENUM(DIV_FLOAT),
            ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST),
            ZR_INSTRUCTION_ENUM(MOD_UNSIGNED),
            ZR_INSTRUCTION_ENUM(MOD_FLOAT),
            ZR_INSTRUCTION_ENUM(POW),
            ZR_INSTRUCTION_ENUM(POW_SIGNED),
            ZR_INSTRUCTION_ENUM(POW_UNSIGNED),
            ZR_INSTRUCTION_ENUM(POW_FLOAT),
            ZR_INSTRUCTION_ENUM(SHIFT_LEFT),
            ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT),
            ZR_INSTRUCTION_ENUM(SHIFT_RIGHT),
            ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT),
            ZR_INSTRUCTION_ENUM(LOGICAL_NOT),
            ZR_INSTRUCTION_ENUM(LOGICAL_AND),
            ZR_INSTRUCTION_ENUM(LOGICAL_OR),
            ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED),
            ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT),
            ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED),
            ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT),
            ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED),
            ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT),
            ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED),
            ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT),
            ZR_INSTRUCTION_ENUM(BITWISE_NOT),
            ZR_INSTRUCTION_ENUM(BITWISE_AND),
            ZR_INSTRUCTION_ENUM(BITWISE_OR),
            ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT),
            ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT)};
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Backends Lower Manual Extended Numeric Opcode Fixture";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot extended numeric lowering",
                 "Testing that strict AOT C and LLVM both lower the expanded numeric, conversion, logical, and bitwise opcode families from a manual function fixture without reporting unsupported instructions");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction function;
        SZrTypeValue constants[4];
        TZrInstruction instructions[45];
        const char *cPath = "execbc_aot_manual_extended_numeric_fixture.c";
        const char *llvmPath = "execbc_aot_manual_extended_numeric_fixture.ll";
        char *cText;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);

        ZrCore_Value_InitAsInt(state, &constants[0], 9);
        ZrCore_Value_InitAsInt(state, &constants[1], 4);
        ZrCore_Value_InitAsFloat(state, &constants[2], 2.5);
        ZrCore_Value_InitAsFloat(state, &constants[3], 1.5);

        instructions[0] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 2, 2);
        instructions[3] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 3, 3);
        instructions[4] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(TO_BOOL), 4, 0, 0);
        instructions[5] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(TO_UINT), 5, 0, 0);
        instructions[6] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(TO_FLOAT), 6, 0, 0);
        instructions[7] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(ADD_FLOAT), 7, 2, 3);
        instructions[8] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(SUB_FLOAT), 8, 2, 3);
        instructions[9] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(MUL_UNSIGNED), 9, 0, 1);
        instructions[10] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(MUL_FLOAT), 10, 2, 3);
        instructions[11] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(DIV_UNSIGNED), 11, 0, 1);
        instructions[12] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(DIV_FLOAT), 12, 2, 3);
        instructions[13] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(MOD_UNSIGNED), 13, 0, 1);
        instructions[14] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(MOD_FLOAT), 14, 2, 3);
        instructions[15] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(POW), 15, 0, 1);
        instructions[16] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(POW_SIGNED), 16, 0, 1);
        instructions[17] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(POW_UNSIGNED), 17, 0, 1);
        instructions[18] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(POW_FLOAT), 18, 2, 3);
        instructions[19] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(SHIFT_LEFT), 19, 0, 1);
        instructions[20] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT), 20, 0, 1);
        instructions[21] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(SHIFT_RIGHT), 21, 0, 1);
        instructions[22] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT), 22, 0, 1);
        instructions[23] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(LOGICAL_NOT), 23, 4, 0);
        instructions[24] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(LOGICAL_AND), 24, 4, 4);
        instructions[25] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(LOGICAL_OR), 25, 4, 4);
        instructions[26] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED), 26, 0, 1);
        instructions[27] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT), 27, 2, 3);
        instructions[28] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED), 28, 0, 1);
        instructions[29] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT), 29, 2, 3);
        instructions[30] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED), 30, 0, 1);
        instructions[31] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT), 31, 2, 3);
        instructions[32] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED), 32, 0, 1);
        instructions[33] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT), 33, 2, 3);
        instructions[34] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(BITWISE_NOT), 34, 0, 0);
        instructions[35] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(BITWISE_AND), 35, 0, 1);
        instructions[36] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(BITWISE_OR), 36, 0, 1);
        instructions[37] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT), 37, 0, 1);
        instructions[38] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT), 38, 0, 1);
        instructions[39] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(ADD_INT_CONST), 39, 0, 1);
        instructions[40] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(SUB_INT_CONST), 40, 0, 1);
        instructions[41] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST), 41, 0, 1);
        instructions[42] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST), 42, 0, 1);
        instructions[43] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST), 43, 0, 1);
        instructions[44] = make_instruction_return(43);
        init_manual_test_function(&function,
                                  instructions,
                                  (TZrUInt32)(sizeof(instructions) / sizeof(instructions[0])),
                                  constants,
                                  (TZrUInt32)(sizeof(constants) / sizeof(constants[0])),
                                  ZR_NULL,
                                  0,
                                  44);
        remove(cPath);
        remove(llvmPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            &function,
                                                            "execbc_aot_manual_extended_numeric_fixture",
                                                            cPath));
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               &function,
                                                               "execbc_aot_manual_extended_numeric_fixture",
                                                               llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ToBool"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ToUInt"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ToFloat"));
        TEST_ASSERT_TRUE(strstr(cText, "ZrLibrary_AotRuntime_AddIntConst") != ZR_NULL ||
                         strstr(cText, "zr_aot_left_int + (TZrInt64)4") != ZR_NULL);
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_AddFloat"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_SubIntConst"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_MulSignedConst"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_DivSignedConst"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ModSignedConst"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_PowFloat"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_LogicalOr"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_BitwiseShiftRight"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_ToBool("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_ToUInt("));
        TEST_ASSERT_TRUE(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_AddIntConst(") != ZR_NULL ||
                         strstr(llvmText, "add i64") != ZR_NULL);
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_AddFloat("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SubIntConst("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_MulSignedConst("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_DivSignedConst("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_ModSignedConst("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_PowFloat("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_LogicalOr("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_BitwiseShiftRight("));
        assert_generated_aot_texts_do_not_report_unsupported_opcodes(cText,
                                                                      llvmText,
                                                                      coveredOpcodes,
                                                                      sizeof(coveredOpcodes) /
                                                                              sizeof(coveredOpcodes[0]));

        free(cText);
        free(llvmText);
        remove(cPath);
        remove(llvmPath);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_backends_emit_instruction_aware_step_flags_for_const_div_mod(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Backends Emit Instruction-Aware Step Flags For Const Div Mod";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot const div mod observation flags",
                 "Testing that strict AOT C and LLVM downgrade non-zero signed const DIV/MOD observation to the fast NONE path while preserving MAY_THROW for zero constants");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction function;
        SZrTypeValue constants[2];
        TZrInstruction instructions[5];
        const char *cPath = "execbc_aot_const_div_mod_step_flags.c";
        const char *llvmPath = "execbc_aot_const_div_mod_step_flags.ll";
        char *cText;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);

        instructions[0] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        instructions[1] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 1, 1);
        instructions[2] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST), 2, 0, 1);
        instructions[3] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST), 3, 0, 1);
        instructions[4] = make_instruction_return(3);

        ZrCore_Value_InitAsInt(state, &constants[0], 9);
        ZrCore_Value_InitAsInt(state, &constants[1], 4);
        init_manual_test_function(&function,
                                  instructions,
                                  (TZrUInt32)(sizeof(instructions) / sizeof(instructions[0])),
                                  constants,
                                  (TZrUInt32)(sizeof(constants) / sizeof(constants[0])),
                                  ZR_NULL,
                                  0,
                                  4);

        remove(cPath);
        remove(llvmPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            &function,
                                                            "execbc_aot_const_div_mod_step_flags_nonzero",
                                                            cPath));
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               &function,
                                                               "execbc_aot_const_div_mod_step_flags_nonzero",
                                                               llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);
        assert_generated_aot_c_begin_instruction_step_flags(cText, 2, "ZR_AOT_GENERATED_STEP_FLAG_NONE");
        assert_generated_aot_c_begin_instruction_step_flags(cText, 3, "ZR_AOT_GENERATED_STEP_FLAG_NONE");
        assert_generated_aot_llvm_begin_instruction_step_flags(llvmText, 2, ZR_AOT_GENERATED_STEP_FLAG_NONE);
        assert_generated_aot_llvm_begin_instruction_step_flags(llvmText, 3, ZR_AOT_GENERATED_STEP_FLAG_NONE);
        free(cText);
        free(llvmText);
        remove(cPath);
        remove(llvmPath);

        ZrCore_Value_InitAsInt(state, &constants[0], 9);
        ZrCore_Value_InitAsInt(state, &constants[1], 0);
        init_manual_test_function(&function,
                                  instructions,
                                  (TZrUInt32)(sizeof(instructions) / sizeof(instructions[0])),
                                  constants,
                                  (TZrUInt32)(sizeof(constants) / sizeof(constants[0])),
                                  ZR_NULL,
                                  0,
                                  4);

        remove(cPath);
        remove(llvmPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            &function,
                                                            "execbc_aot_const_div_mod_step_flags_zero",
                                                            cPath));
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               &function,
                                                               "execbc_aot_const_div_mod_step_flags_zero",
                                                               llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);
        assert_generated_aot_c_begin_instruction_step_flags(cText, 2, "ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW");
        assert_generated_aot_c_begin_instruction_step_flags(cText, 3, "ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW");
        assert_generated_aot_llvm_begin_instruction_step_flags(llvmText, 2, ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW);
        assert_generated_aot_llvm_begin_instruction_step_flags(llvmText, 3, ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW);
        free(cText);
        free(llvmText);
        remove(cPath);
        remove(llvmPath);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_backends_lower_manual_state_and_scope_opcode_fixture(void) {
    static const EZrInstructionCode coveredOpcodes[] = {
            ZR_INSTRUCTION_ENUM(SET_CONSTANT),
            ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION),
            ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED),
            ZR_INSTRUCTION_ENUM(CLOSE_SCOPE)};
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Backends Lower Manual State And Scope Opcode Fixture";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot state and scope lowering",
                 "Testing that strict AOT C and LLVM both lower SET_CONSTANT, GET_SUB_FUNCTION, and using-scope cleanup opcodes from a manual function tree, including static direct-call provenance from GET_SUB_FUNCTION");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction rootFunction;
        SZrFunction childFunctions[1];
        SZrTypeValue rootConstants[2];
        SZrTypeValue childConstants[1];
        TZrInstruction rootInstructions[7];
        TZrInstruction childInstructions[2];
        const char *cPath = "execbc_aot_manual_state_scope_fixture.c";
        const char *llvmPath = "execbc_aot_manual_state_scope_fixture.ll";
        char *cText;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);

        ZrCore_Value_InitAsInt(state, &rootConstants[0], 5);
        ZrCore_Value_InitAsInt(state, &rootConstants[1], 0);
        ZrCore_Value_InitAsInt(state, &childConstants[0], 42);

        childInstructions[0] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        childInstructions[1] = make_instruction_return(0);
        init_manual_test_function(&childFunctions[0], childInstructions, 2, childConstants, 1, ZR_NULL, 0, 1);

        rootInstructions[0] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 0, 0);
        rootInstructions[1] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(SET_CONSTANT), 0, 1);
        rootInstructions[2] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), 1, 0, 0);
        rootInstructions[3] = make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(FUNCTION_CALL), 2, 1, 0);
        rootInstructions[4] = make_instruction_no_operands(ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED), 0);
        rootInstructions[5] = make_instruction_no_operands(ZR_INSTRUCTION_ENUM(CLOSE_SCOPE), 1);
        rootInstructions[6] = make_instruction_return(2);
        init_manual_test_function(&rootFunction,
                                  rootInstructions,
                                  (TZrUInt32)(sizeof(rootInstructions) / sizeof(rootInstructions[0])),
                                  rootConstants,
                                  (TZrUInt32)(sizeof(rootConstants) / sizeof(rootConstants[0])),
                                  childFunctions,
                                  (TZrUInt32)(sizeof(childFunctions) / sizeof(childFunctions[0])),
                                  3);

        remove(cPath);
        remove(llvmPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            &rootFunction,
                                                            "execbc_aot_manual_state_scope_fixture",
                                                            cPath));
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               &rootFunction,
                                                               "execbc_aot_manual_state_scope_fixture",
                                                               llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_SetConstant"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_GetSubFunction"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_MarkToBeClosed"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CloseScope"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_PrepareStaticDirectCall(state,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR_AOT_C_GUARD(zr_aot_fn_1(state));"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_PrepareDirectCall(state,"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SetConstant("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_GetSubFunction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_MarkToBeClosed("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_CloseScope("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_PrepareStaticDirectCall("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i64 @zr_aot_fn_1(ptr %state)"));
        assert_generated_aot_texts_do_not_report_unsupported_opcodes(cText,
                                                                      llvmText,
                                                                      coveredOpcodes,
                                                                      sizeof(coveredOpcodes) /
                                                                              sizeof(coveredOpcodes[0]));

        free(cText);
        free(llvmText);
        remove(cPath);
        remove(llvmPath);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_aot_source_sync_keeps_extended_opcode_surfaces_aligned(void) {
    static const SZrAotSourceSyncExpectation expectations[] = {
            {"SET_CONSTANT", "ZrLibrary_AotRuntime_SetConstant"},
            {"GET_SUB_FUNCTION", "ZrLibrary_AotRuntime_GetSubFunction"},
            {"TO_BOOL", "ZrLibrary_AotRuntime_ToBool"},
            {"TO_UINT", "ZrLibrary_AotRuntime_ToUInt"},
            {"TO_FLOAT", "ZrLibrary_AotRuntime_ToFloat"},
            {"ADD_INT_CONST", "ZrLibrary_AotRuntime_AddIntConst"},
            {"ADD_FLOAT", "ZrLibrary_AotRuntime_AddFloat"},
            {"SUB_INT_CONST", "ZrLibrary_AotRuntime_SubIntConst"},
            {"SUB_FLOAT", "ZrLibrary_AotRuntime_SubFloat"},
            {"MUL_SIGNED_CONST", "ZrLibrary_AotRuntime_MulSignedConst"},
            {"MUL_UNSIGNED", "ZrLibrary_AotRuntime_MulUnsigned"},
            {"MUL_FLOAT", "ZrLibrary_AotRuntime_MulFloat"},
            {"DIV_SIGNED_CONST", "ZrLibrary_AotRuntime_DivSignedConst"},
            {"DIV_UNSIGNED", "ZrLibrary_AotRuntime_DivUnsigned"},
            {"DIV_FLOAT", "ZrLibrary_AotRuntime_DivFloat"},
            {"MOD_SIGNED_CONST", "ZrLibrary_AotRuntime_ModSignedConst"},
            {"MOD_UNSIGNED", "ZrLibrary_AotRuntime_ModUnsigned"},
            {"MOD_FLOAT", "ZrLibrary_AotRuntime_ModFloat"},
            {"POW", "ZrLibrary_AotRuntime_Pow"},
            {"POW_SIGNED", "ZrLibrary_AotRuntime_PowSigned"},
            {"POW_UNSIGNED", "ZrLibrary_AotRuntime_PowUnsigned"},
            {"POW_FLOAT", "ZrLibrary_AotRuntime_PowFloat"},
            {"SHIFT_LEFT", "ZrLibrary_AotRuntime_ShiftLeft"},
            {"SHIFT_LEFT_INT", "ZrLibrary_AotRuntime_ShiftLeftInt"},
            {"SHIFT_RIGHT", "ZrLibrary_AotRuntime_ShiftRight"},
            {"SHIFT_RIGHT_INT", "ZrLibrary_AotRuntime_ShiftRightInt"},
            {"LOGICAL_NOT", "ZrLibrary_AotRuntime_LogicalNot"},
            {"LOGICAL_AND", "ZrLibrary_AotRuntime_LogicalAnd"},
            {"LOGICAL_OR", "ZrLibrary_AotRuntime_LogicalOr"},
            {"LOGICAL_GREATER_UNSIGNED", "ZrLibrary_AotRuntime_LogicalGreaterUnsigned"},
            {"LOGICAL_GREATER_FLOAT", "ZrLibrary_AotRuntime_LogicalGreaterFloat"},
            {"LOGICAL_LESS_UNSIGNED", "ZrLibrary_AotRuntime_LogicalLessUnsigned"},
            {"LOGICAL_LESS_FLOAT", "ZrLibrary_AotRuntime_LogicalLessFloat"},
            {"LOGICAL_GREATER_EQUAL_UNSIGNED", "ZrLibrary_AotRuntime_LogicalGreaterEqualUnsigned"},
            {"LOGICAL_GREATER_EQUAL_FLOAT", "ZrLibrary_AotRuntime_LogicalGreaterEqualFloat"},
            {"LOGICAL_LESS_EQUAL_UNSIGNED", "ZrLibrary_AotRuntime_LogicalLessEqualUnsigned"},
            {"LOGICAL_LESS_EQUAL_FLOAT", "ZrLibrary_AotRuntime_LogicalLessEqualFloat"},
            {"BITWISE_NOT", "ZrLibrary_AotRuntime_BitwiseNot"},
            {"BITWISE_AND", "ZrLibrary_AotRuntime_BitwiseAnd"},
            {"BITWISE_OR", "ZrLibrary_AotRuntime_BitwiseOr"},
            {"BITWISE_SHIFT_LEFT", "ZrLibrary_AotRuntime_BitwiseShiftLeft"},
            {"BITWISE_SHIFT_RIGHT", "ZrLibrary_AotRuntime_BitwiseShiftRight"},
            {"GET_MEMBER_SLOT", "ZrLibrary_AotRuntime_GetMemberSlot"},
            {"SET_MEMBER_SLOT", "ZrLibrary_AotRuntime_SetMemberSlot"},
            {"MARK_TO_BE_CLOSED", "ZrLibrary_AotRuntime_MarkToBeClosed"},
            {"CLOSE_SCOPE", "ZrLibrary_AotRuntime_CloseScope"}};
    static const char *const llvmLoweringPaths[] = {
            "zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_constants.c",
            "zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_closures.c",
            "zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_type_conversions.c",
            "zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_arithmetic.c",
            "zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_control.c",
            "zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_member_access.c"};
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Source Sync Keeps Extended Opcode Surfaces Aligned";
    char *backendSupportText;
    char *cLoweringText;
    char *llvmLoweringText;
    char *llvmPreludeText;
    char *runtimeHeaderText;
    char *runtimeSourceText;
    TZrSize index;

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot source sync guard",
                 "Testing that the expanded opcode sync stays aligned across backend_aot support tables, C lowering, LLVM lowering, and runtime helper declarations so future opcode growth cannot silently strand true AOT behind older source maps");

    backendSupportText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c");
    cLoweringText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    llvmLoweringText = join_repo_files_owned(llvmLoweringPaths,
                                             (TZrSize)(sizeof(llvmLoweringPaths) / sizeof(llvmLoweringPaths[0])));
    llvmPreludeText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.c");
    runtimeHeaderText = read_repo_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    runtimeSourceText = read_repo_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");

    TEST_ASSERT_NOT_NULL(backendSupportText);
    TEST_ASSERT_NOT_NULL(cLoweringText);
    TEST_ASSERT_NOT_NULL(llvmLoweringText);
    TEST_ASSERT_NOT_NULL(llvmPreludeText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);

    for (index = 0; index < (TZrSize)(sizeof(expectations) / sizeof(expectations[0])); index++) {
        char opcodeToken[96];

        snprintf(opcodeToken,
                 sizeof(opcodeToken),
                 "ZR_INSTRUCTION_ENUM(%s)",
                 expectations[index].opcodeName);
        TEST_ASSERT_TRUE_MESSAGE(count_substring_occurrences(backendSupportText, opcodeToken) >= 2, opcodeToken);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(cLoweringText, opcodeToken), opcodeToken);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(llvmLoweringText, opcodeToken), opcodeToken);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(llvmPreludeText, expectations[index].runtimeHelperName),
                                     expectations[index].runtimeHelperName);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(runtimeHeaderText, expectations[index].runtimeHelperName),
                                     expectations[index].runtimeHelperName);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(runtimeSourceText, expectations[index].runtimeHelperName),
                                     expectations[index].runtimeHelperName);
    }

    TEST_ASSERT_NOT_NULL(strstr(cLoweringText, "backend_aot_find_function_table_index"));
    TEST_ASSERT_NOT_NULL(strstr(llvmLoweringText, "backend_aot_find_function_table_index"));

    free(backendSupportText);
    free(cLoweringText);
    free(llvmLoweringText);
    free(llvmPreludeText);
    free(runtimeHeaderText);
    free(runtimeSourceText);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_access_lowering_preserves_explicit_member_and_index_ops);
    RUN_TEST(test_aot_backends_preserve_runtime_contract_artifacts_under_strict_aot_c);
    RUN_TEST(test_aot_c_backend_emits_child_thunks_for_callable_constants);
    RUN_TEST(test_aot_llvm_backend_lowers_simple_entry_execution_path);
    RUN_TEST(test_aot_llvm_backend_lowers_static_direct_call_protocol);
    RUN_TEST(test_aot_llvm_backend_lowers_closure_capture_access);
    RUN_TEST(test_aot_llvm_backend_lowers_object_creation_path);
    RUN_TEST(test_aot_llvm_backend_lowers_to_object_with_runtime_helper);
    RUN_TEST(test_aot_c_backend_emits_native_entry_descriptor_instead_of_shim_invoke);
    RUN_TEST(test_aot_c_backend_directly_lowers_static_slot_and_int_ops);
    RUN_TEST(test_aot_llvm_backend_directly_lowers_static_slot_and_int_ops);
    RUN_TEST(test_aot_c_backend_directly_lowers_primitive_literal_constants);
    RUN_TEST(test_aot_c_backend_lowers_generic_add_with_fast_path_and_helper_fallback);
    RUN_TEST(test_aot_llvm_backend_lowers_generic_add_with_fast_path_and_helper_fallback);
    RUN_TEST(test_aot_c_backend_directly_lowers_local_callable_constants);
    RUN_TEST(test_aot_c_backend_directly_lowers_local_aot_function_calls);
    RUN_TEST(test_aot_c_backend_statically_lowers_proven_local_aot_function_calls);
    RUN_TEST(test_aot_c_backend_lowers_cached_meta_calls_with_direct_call_frames);
    RUN_TEST(test_aot_llvm_backend_lowers_cached_meta_calls_with_direct_call_frames);
    RUN_TEST(test_aot_c_backend_lowers_plain_meta_get_set_with_runtime_helpers);
    RUN_TEST(test_aot_llvm_backend_lowers_meta_tail_calls_with_direct_call_frames);
    RUN_TEST(test_aot_llvm_backend_lowers_exception_control_transfer_helpers);
    RUN_TEST(test_aot_c_backend_lowers_cached_dynamic_tail_calls_with_runtime_prepare);
    RUN_TEST(test_aot_c_backend_directly_lowers_non_export_return);
    RUN_TEST(test_aot_c_backend_lowers_to_object_with_runtime_helper);
    RUN_TEST(test_aot_c_backend_lowers_to_struct_with_runtime_helper);
    RUN_TEST(test_aot_c_backend_lowers_signed_compare_div_and_neg_paths);
    RUN_TEST(test_aot_backends_lower_benchmark_style_mod_string_and_compare_paths);
    RUN_TEST(test_string_equality_allows_destination_aliasing_left_operand);
    RUN_TEST(test_aot_backends_lower_benchmark_style_generic_sub_paths);
    RUN_TEST(test_aot_backends_lower_benchmark_style_generic_mul_paths);
    RUN_TEST(test_aot_backends_lower_benchmark_style_generic_div_paths);
    RUN_TEST(test_aot_backends_lower_benchmark_style_bitwise_xor_paths);
    RUN_TEST(test_aot_backends_emit_instruction_aware_step_flags_for_const_div_mod);
    RUN_TEST(test_aot_runtime_observation_policy_is_thread_local);
    RUN_TEST(test_aot_runtime_begin_generated_function_caches_slot_count_and_preserves_bounds_after_refresh);
    RUN_TEST(test_aot_runtime_prepare_static_direct_call_uses_cached_callee_slot_count);
    RUN_TEST(test_aot_runtime_super_array_add_int_allows_ignored_destination_slot);
    RUN_TEST(test_aot_runtime_index_helpers_refresh_frame_for_native_binding_paths);
    RUN_TEST(test_aot_runtime_begin_instruction_respects_resolved_observation_policy);
    RUN_TEST(test_aot_runtime_begin_instruction_publishes_line_debug_events);
    RUN_TEST(test_aot_runtime_begin_instruction_honors_dynamic_line_signal_enablement);
    RUN_TEST(test_aot_c_backend_lowers_indexed_entry_access_even_with_embedded_blob);
    RUN_TEST(test_aot_c_backend_lowers_indexed_entry_access_without_embedded_blob);
    RUN_TEST(test_aot_c_backend_lowers_indexed_child_access_even_with_embedded_blob);
    RUN_TEST(test_aot_c_backend_lowers_indexed_child_access_without_embedded_blob);
    RUN_TEST(test_checked_in_aot_c_fixtures_use_new_code_shape);
    RUN_TEST(test_checked_in_aot_llvm_fixtures_use_true_backend_shape);
    RUN_TEST(test_typed_member_calls_quicken_to_known_vm_call_family);
    RUN_TEST(test_backend_aot_source_split_promotes_exec_ir_to_per_function_cfg_and_frame_layout);
    RUN_TEST(test_aot_runtime_source_removes_backend_specific_true_aot_descriptor_gate);
    RUN_TEST(test_backend_aot_source_split_moves_llvm_emitter_out_of_backend_aot_c);
    RUN_TEST(test_backend_aot_source_split_moves_backend_writer_entrypoints_out_of_backend_aot_c);
    RUN_TEST(test_backend_aot_source_split_moves_shared_function_table_and_callable_provenance_out_of_backend_aot_c);
    RUN_TEST(test_backend_aot_source_split_moves_aot_c_lowering_families_out_of_backend_aot_c_emitter);
    RUN_TEST(test_backend_aot_source_split_moves_aot_llvm_lowering_families_out_of_backend_aot_llvm_emitter);
    RUN_TEST(test_backend_aot_source_split_moves_aot_llvm_module_artifacts_out_of_emitter);
    RUN_TEST(test_backend_aot_source_split_moves_aot_llvm_module_prelude_helpers_out_of_module_artifacts);
    RUN_TEST(test_backend_aot_source_split_moves_aot_llvm_object_subfamilies_out_of_object_lowering);
    RUN_TEST(test_backend_aot_source_split_moves_aot_llvm_member_index_subfamilies_out_of_member_index_lowering);
    RUN_TEST(test_backend_aot_source_split_moves_aot_llvm_call_subfamilies_out_of_call_lowering);
    RUN_TEST(test_backend_aot_source_split_moves_aot_llvm_text_emit_helpers_out_of_control_lowering);
    RUN_TEST(test_backend_aot_source_split_moves_aot_llvm_control_subfamilies_out_of_control_lowering);
    RUN_TEST(test_backend_aot_source_split_moves_aot_llvm_closure_subfamilies_out_of_closure_lowering);
    RUN_TEST(test_backend_aot_source_split_moves_aot_llvm_flow_text_helpers_out_of_text_emit);
    RUN_TEST(test_backend_aot_source_split_moves_aot_llvm_flow_subfamilies_out_of_text_flow);
    RUN_TEST(test_execbc_quickens_zero_arg_call_sites_without_changing_semir_contracts);
    RUN_TEST(test_execbc_true_aot_lowers_typed_signed_unsigned_and_equality_opcodes);
    RUN_TEST(test_execbc_true_aot_lowers_known_native_call_family);
    RUN_TEST(test_execbc_quickens_cached_meta_and_dynamic_call_sites_and_preserves_binary_metadata);
    RUN_TEST(test_binary_roundtrip_preserves_debug_line_metadata_for_runtime_loading);
    RUN_TEST(test_binary_roundtrip_preserves_fixed_array_helper_argument_values);
    RUN_TEST(test_binary_roundtrip_preserves_map_stored_array_iterable_contract);
    RUN_TEST(test_binary_roundtrip_preserves_linked_pair_helper_values);
    RUN_TEST(test_binary_roundtrip_preserves_set_pair_hash_and_iteration);
    RUN_TEST(test_binary_roundtrip_preserves_set_to_map_bucket_values);
    RUN_TEST(test_binary_roundtrip_preserves_container_matrix_native_call_arguments);
    RUN_TEST(test_benchmark_string_build_binary_roundtrip_loads_runtime_entry);
    RUN_TEST(test_execbc_quickens_zero_arg_tail_call_sites_without_changing_semir_contracts);
    RUN_TEST(test_execbc_quickens_array_int_index_sites_and_true_aot_lowers_specialized_helpers);
    RUN_TEST(test_execbc_quickens_array_int_add_bursts_to_dedicated_dispatch_opcode);
    RUN_TEST(test_execbc_quickens_array_int_fill_loops_to_bulk_dispatch_opcode);
    RUN_TEST(test_meta_access_semir_and_true_aot_c_preserve_dedicated_meta_get_set_opcodes);
    RUN_TEST(test_typed_member_access_allocates_member_callsite_cache_entries);
    RUN_TEST(test_dynamic_object_member_access_stays_on_generic_member_opcodes);
    RUN_TEST(test_nested_typed_member_chain_emits_member_slot_opcodes);
    RUN_TEST(test_typed_object_destructuring_emits_member_slot_opcodes);
    RUN_TEST(test_static_meta_access_quickens_to_static_callsite_cache_variants);
    RUN_TEST(test_super_member_dispatch_uses_direct_base_lookup_for_methods_properties_and_meta_calls);
    RUN_TEST(test_reference_property_fixture_preserves_meta_access_artifacts_with_true_aot_c_lowering);
    RUN_TEST(test_reference_member_index_fixture_preserves_split_access_artifacts);
    RUN_TEST(test_reference_foreach_fixture_preserves_iter_contract_artifacts);
    RUN_TEST(test_ownership_upgrade_release_semir_and_true_aot_c_preserve_dedicated_opcodes);
    RUN_TEST(test_aot_backends_lower_manual_extended_numeric_opcode_fixture);
    RUN_TEST(test_aot_backends_lower_manual_state_and_scope_opcode_fixture);
    RUN_TEST(test_aot_source_sync_keeps_extended_opcode_surfaces_aligned);
    return UNITY_END();
}
