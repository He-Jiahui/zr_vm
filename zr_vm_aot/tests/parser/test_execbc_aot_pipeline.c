#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "path_support.h"
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
#include "compiler/compiler_internal.h"
#include "zr_vm_parser/writer.h"
#include "test_support.h"

// 主测试文件的计时状态只用于报告耗时，不影响语义断言。
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrExecBcAotTestTimer;

// 将内存中的二进制 fixture 暴露给核心 IO 回调；bytes 由测试持有，读取只消费一次。
typedef struct {
    TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} SZrBinaryFixtureReader;

// 收集行事件供 AOT 调试信号用例断言；测试入口必须先重置全局捕获状态。
// TODO: lines[] 已记录前八个事件但现有用例只断言计数/最后行；增加顺序断言后再依赖它验证事件序列。
typedef struct {
    TZrUInt32 eventCount;
    TZrUInt32 lastLine;
    TZrUInt32 lines[8];
} SZrAotDebugHookCapture;

// 源码同步矩阵绑定 opcode 和预期后端/运行时入口；可选 C 直接 lowering 允许空值。
typedef struct {
    const char *opcodeName;
    const char *runtimeHelperName;
    const char *cDirectLoweringName;
} SZrAotSourceSyncExpectation;

// 模拟当前根库的私有项目记录，供生成函数帧测试注入临时项目。
// 当前字段次序与根库 aot_runtime.c 的记录一致，包括 codeRegistration、functionPins 和 modulePin。
// TODO: 类型未公开；库内字段变更时同步此镜像，并以 sizeof/offsetof 检查阻止静默漂移。
typedef struct {
    EZrAotBackendKind backendKind;
    TZrChar *moduleName;
    TZrChar *sourcePath;
    TZrChar *zroPath;
    TZrChar *libraryPath;
    void *libraryHandle;
    const ZrAotCompiledModule *descriptor;
    const SZrAotCodeRegistration *codeRegistration;
    SZrFunction *moduleFunction;
    SZrFunction **functionTable;
    SZrGcNativeCallPin *functionPins;
    TZrUInt32 functionCount;
    TZrUInt32 functionCapacity;
    TZrUInt32 *generatedFrameSlotCounts;
    SZrObjectModule *module;
    SZrGcNativeCallPin modulePin;
    TZrBool moduleExecuted;
} SZrExecBcAotTestLoadedModule;

// 项目 AOT 状态镜像供测试直接装配 records；当前与根库私有状态同布局。
// TODO: 此跨模块私有 ABI 依赖无编译期校验；同步根库记录结构变化。
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

// 调试回调共享捕获槽；相关 Unity 用例串行执行并在注册回调前清零。
static SZrAotDebugHookCapture g_aotDebugHookCapture;

// 与指令声明表同步生成诊断名称，避免 trace 为新 opcode 维护第二份枚举表。
#define ZR_EXECBC_TEST_OPCODE_NAME_CASE(INSTRUCTION)                                                                  \
    case ZR_INSTRUCTION_ENUM(INSTRUCTION):                                                                            \
        return #INSTRUCTION;

// 将指令声明表映射为可读诊断名，供可选树形 trace 使用。
static const char *execbc_test_instruction_opcode_name(EZrInstructionCode opcode) {
    switch (opcode) {
        ZR_INSTRUCTION_DECLARE(ZR_EXECBC_TEST_OPCODE_NAME_CASE)
        default:
            return "UNKNOWN_OPCODE";
    }
}

// 按进程环境变量控制测试 trace；首次查询后缓存结果，运行期间修改环境不会生效。
static TZrBool execbc_test_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        enabled = getenv("ZR_VM_TRACE_EXECBC_AOT_TEST") != ZR_NULL ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

// 只在 trace 开启时递归打印函数树与中间码，帮助定位 quickening 断言失败。
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

// 为手工项目记录提供最小可调用入口，使生成帧测试不依赖真实动态库。
static TZrInt64 aot_runtime_test_dummy_entry_thunk(struct SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
    return 1;
}

// 在每个调试事件用例前清空共享捕获槽，避免上个测试的行事件污染断言。
static void aot_debug_hook_capture_reset(void) {
    memset(&g_aotDebugHookCapture, 0, sizeof(g_aotDebugHookCapture));
    g_aotDebugHookCapture.lastLine = ZR_RUNTIME_DEBUG_HOOK_LINE_NONE;
}

// 作为运行时 debug hook 接收行事件，仅保留计数、最后行号和前八次样本。
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

// 读取生成工件的文本快照；调用测试持有并释放返回缓冲，失败时返回 NULL。
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

// 集中约束生成 C 文本采用新的函数表与入口形态，供多组 fixture 复用。
static void assert_generated_aot_c_text_uses_new_code_shape(const char *text) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NULL(strstr(text, "if (zr_aot_direct_call.prepared) {"));
    TEST_ASSERT_NULL(strstr(text, "goto zr_aot_fail"));
    TEST_ASSERT_NULL(strstr(text, "return 0;"));
    TEST_ASSERT_NULL(strstr(text, "ZrLibrary_AotRuntime_InvokeActiveShim"));
    TEST_ASSERT_TRUE(strstr(text, "ZrLibrary_AotRuntime_ResolveGeneratedModuleContext") != ZR_NULL ||
                     strstr(text, "ZrLibrary_AotRuntime_BeginGeneratedFunction") != ZR_NULL);
    TEST_ASSERT_TRUE(strstr(text, "zr_aot_function_exit:") != ZR_NULL ||
                     strstr(text, "ZrLibrary_AotRuntime_FailGeneratedFunction") != ZR_NULL);
}

// 核对 LLVM 输出为独立后端产物，避免退回嵌入式或占位实现。
static void assert_generated_aot_llvm_text_uses_true_backend_shape(const char *text) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NULL(strstr(text, "ZrLibrary_AotRuntime_InvokeActiveShim"));
    TEST_ASSERT_NOT_NULL(strstr(text, "define internal i64 @zr_aot_fn_0(ptr %state)"));
}

// 守护 LLVM 的栈值布局与运行时 ABI 对齐，防止文本工件仍能生成却访问错位。
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

// 用最短返回路径确认 LLVM 正常发出可执行入口与返回约定。
static void assert_generated_aot_llvm_text_lowers_simple_return_path(const char *text) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(strstr(text, "@ZrLibrary_AotRuntime_BeginGeneratedFunction"));
    TEST_ASSERT_NOT_NULL(strstr(text, "@ZrLibrary_AotRuntime_BeginInstruction"));
    TEST_ASSERT_NOT_NULL(strstr(text, "@ZrLibrary_AotRuntime_CopyConstant"));
    TEST_ASSERT_NOT_NULL(strstr(text, "@ZrLibrary_AotRuntime_Return"));
}

// 检查 LLVM 静态目标调用遵守直达帧协议，供多个调用测试复用。
static void assert_generated_aot_llvm_text_lowers_static_direct_call_path(const char *text) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(strstr(text, "@ZrLibrary_AotRuntime_PrepareStaticDirectCall"));
    TEST_ASSERT_NOT_NULL(strstr(text, "call i64 @zr_aot_fn_1(ptr %state)"));
    TEST_ASSERT_NOT_NULL(strstr(text, "@ZrLibrary_AotRuntime_FinishDirectCall"));
}

// 检测 LLVM 工件对指定 opcode 的 unsupported 标记；文本匹配不等于执行验证。
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

// 检测 C 工件对指定 opcode 的 unsupported 标记；需结合运行测试解释结果。
static TZrBool aot_c_text_contains_unsupported_opcode(const char *text, TZrUInt32 opcode) {
    const char *unsupportedPrefix = "ZR_AOT_C_RETURN(ZrLibrary_AotRuntime_ReportUnsupportedInstruction(state,";
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

// 限定无观察需求时的 C 后端输出，防止每条指令都引入调试开销。
static void assert_generated_aot_c_elides_instruction_observation(const char *text) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NULL(strstr(text, "/* zr_aot_begin_instruction */"));
    TEST_ASSERT_NULL(strstr(text, "ZrLibrary_AotRuntime_BeginInstruction"));
}

// 核对 LLVM 指令观察标记映射，确保异常、调用等观察请求未被静默丢弃。
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

// 手工 opcode fixture 的基础构造器，用于隔离后端 lowering 与源编译器选择。
static TZrInstruction make_instruction_no_operands(EZrInstructionCode opcode, TZrUInt16 operandExtra) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    return instruction;
}

// 构造引用常量表的手工指令；调用方负责保证常量索引有效。
static TZrInstruction make_instruction_constant_operand(EZrInstructionCode opcode,
                                                        TZrUInt16 operandExtra,
                                                        TZrInt32 operandA2) {
    TZrInstruction instruction = make_instruction_no_operands(opcode, operandExtra);

    instruction.instruction.operand.operand2[0] = operandA2;
    return instruction;
}

// 构造带栈槽操作数的手工指令；槽布局由 fixture 初始化函数提供。
static TZrInstruction make_instruction_slot_operands(EZrInstructionCode opcode,
                                                     TZrUInt16 operandExtra,
                                                     TZrUInt16 operandA1,
                                                     TZrUInt16 operandB1) {
    TZrInstruction instruction = make_instruction_no_operands(opcode, operandExtra);

    instruction.instruction.operand.operand1[0] = operandA1;
    instruction.instruction.operand.operand1[1] = operandB1;
    return instruction;
}

// 为手工函数追加可观测返回，使执行路径能够终止并比对结果。
static TZrInstruction make_instruction_return(TZrUInt16 sourceSlot) {
    return make_instruction_slot_operands(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, sourceSlot, 0);
}

// 保留源编译阶段的原始中间码，供测试对照 quickening 前后的契约。
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

// 使用 state 所在 global 的项目分配器建立镜像，使根库项目释放路径接管这些缓冲。
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

// 为四个生成帧/索引用例按当前根库布局注入项目记录并挂到项目对象。
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
    generatedFrameSlotCounts[0] =
            (TZrUInt32)ZrCore_Function_GetFrameStorageSlotCount(function);
    for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
        functionTable[index + 1u] = &function->childFunctionList[index];
        generatedFrameSlotCounts[index + 1u] =
                (TZrUInt32)ZrCore_Function_GetFrameStorageSlotCount(
                        &function->childFunctionList[index]);
    }

    records[0].backendKind = backendKind;
    records[0].moduleFunction = function;
    records[0].functionTable = functionTable;
    records[0].functionCount = functionCount;
    records[0].functionCapacity = functionCount;
    records[0].generatedFrameSlotCounts = generatedFrameSlotCounts;

    // TODO: records 被根库按私有布局解释；库内类型变化时须同步镜像并校验偏移。
    runtimeState->records = records;
    runtimeState->recordCount = 1;
    runtimeState->recordCapacity = 1;
    runtimeState->activeRecord = ZR_NULL;
    return runtimeState;
}

// 测试结束时让根库释放项目状态，再断开 global 对项目对象的借用引用。
static void aot_runtime_test_remove_project_record(SZrState *state, SZrLibrary_Project *project) {
    if (state == ZR_NULL || state->global == ZR_NULL || project == ZR_NULL) {
        return;
    }

    if (project->aotRuntime != ZR_NULL) {
        ZrLibrary_AotRuntime_FreeProjectState(state, project);
    }
    state->global->userData = ZR_NULL;
}

// 建立可供 VM 与后端共用的最小手工函数；调用方须随后填充有效指令及常量。
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

// 对双后端工件做共同的 opcode 支持性门禁，防止静默生成占位路径。
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

// 经 fixture 路径辅助层读取仓库参考文件，返回堆缓冲由调用方释放。
static char *read_reference_file(const char *relativePath, size_t *outSize) {
    return ZrTests_Reference_ReadFixture(relativePath, outSize);
}

// 将仓库相对路径转成可读取文本，供源码拆分与同步契约测试使用。
static char *read_repo_file_owned(const char *repoRelativePath) {
    char pathBuffer[1024];
    char *text;

    if (repoRelativePath == ZR_NULL) {
        return ZR_NULL;
    }

    snprintf(pathBuffer, sizeof(pathBuffer), "%s/../%s", ZR_VM_TESTS_SOURCE_DIR, repoRelativePath);
    text = read_text_file_owned(pathBuffer);
    if (text == ZR_NULL) {
        snprintf(pathBuffer,
                 sizeof(pathBuffer),
                 "%s/../zr_vm_aot/%s",
                 ZR_VM_TESTS_SOURCE_DIR,
                 repoRelativePath);
        text = read_text_file_owned(pathBuffer);
    }
    return text;
}

// 统计文本锚点次数，源码拆分测试借此约束唯一归属而非仅检查存在。
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

// 为跨文件源码同步断言拼接多个实现文件，并返回调用方持有的缓冲。BUG: 两遍读取之间任一文件增长时，第二遍 memcpy 可超出首次统计的分配长度。
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
    // BUG: 第二次读取未校验首次分配容量；并发编辑使文件变长时可写过 joinedText。
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

// 读取二进制 fixture 并返回长度；调用方释放缓冲，禁止按 C 字符串使用。
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

// 从仓库相对路径获取二进制 fixture，供加载器与 roundtrip 测试共用。
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

// 生成携带二进制模块的 C 工件，以验证嵌入路径仍保留直达 lowering。
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

// 用无嵌入模块的严格模式生成 C 工件，防止运行时 shim 掩盖 opcode 缺口。
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

// 为相同 fixture 生成严格 LLVM 工件，与 C 后端支持矩阵对照。
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

// 向核心 IO 提供一次性借用的二进制视图；bytes 仍归测试所有，后续读取返回 EOF。
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

// 核心 IO 的 close 回调不释放借用的字节缓冲；实际所有权仍归调用测试。
static void binary_fixture_reader_close(struct SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(customData);
}

// 只查当前函数的 ExecBC 指令；嵌套 fixture 必须改用函数树查询。
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

// 汇总整数数组读取快路径家族，允许优化选择等价变体。
static TZrBool function_contains_super_array_get_int_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST));
}

// 汇总整数数组写入快路径家族，避免对具体变体编码过严。
static TZrBool function_contains_super_array_set_int_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS));
}

// 将整数加法的多个快路径视为同一语义族，供优化用例断言。
static TZrBool function_contains_add_int_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_INT)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_INT_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST));
}

// 识别有符号加法族，覆盖类型专门化后可能不同的寄存器形态。
static TZrBool function_contains_add_signed_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_MOD_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST));
}

// 识别无符号加法族，供数值 lowering 矩阵检查。
static TZrBool function_contains_add_unsigned_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST));
}

// 汇总整数减法优化族，避免用例依赖单一编码变体。
static TZrBool function_contains_sub_int_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_INT)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_INT_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST));
}

// 识别有符号减法族，核对后端与 quickening 的类型选择。
static TZrBool function_contains_sub_signed_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST));
}

// 识别无符号减法族，核对后端与 quickening 的类型选择。
static TZrBool function_contains_sub_unsigned_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST));
}

// 识别有符号乘法族，供 benchmark 风格算术路径用例使用。
static TZrBool function_contains_mul_signed_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK));
}

// 识别有符号除法族，供除法快路径与回退检查。
static TZrBool function_contains_div_signed_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DIV_SIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST));
}

// 识别有符号取模族，供数值综合用例检查。
static TZrBool function_contains_mod_signed_family(const SZrFunction *function) {
    return function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MOD_SIGNED)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST)) ||
           function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST));
}

// 按成员名确认访问指令实际关联目标字段，防止只见 opcode 却指向错符号。
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

// 把 ExecBC 指令索引映射回调试行号，供二进制 roundtrip 对照。
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

// 为加载前后字符串字段提供空值安全比较，避免指针身份替代内容比较。
static TZrBool execbc_aot_pipeline_string_equal(const SZrString *string1, const SZrString *string2) {
    return ZrCore_String_Equal((SZrString *)string1, (SZrString *)string2);
}

// 递归比较源函数与二进制加载后函数的关键元数据，防止 roundtrip 丢失运行时契约。
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

// 比较可空字符串字段，供加载后函数元数据断言复用。
static void assert_optional_string_equal(const SZrString *expected, const SZrString *actual) {
    if (expected == ZR_NULL || actual == ZR_NULL) {
        TEST_ASSERT_EQUAL_PTR(expected, actual);
        return;
    }

    TEST_ASSERT_TRUE(execbc_aot_pipeline_string_equal(expected, actual));
}

// 比较类型引用元数据，确保二进制回读保留类型化调用信息。
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

// 从模块入口递归扫描子函数的 ExecBC opcode，覆盖实际执行体。
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

// 只查当前函数调用点缓存类型，供本层 quickening 断言。
static TZrBool function_contains_callsite_cache_kind(const SZrFunction *function,
                                                     EZrFunctionCallSiteCacheKind kind) {
    return function != ZR_NULL && function->callSiteCaches != ZR_NULL &&
           function_count_callsite_cache_kind(function, kind) > 0;
}

// 递归查找调用缓存类型，覆盖闭包与成员方法子函数。
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

// 统计当前函数的目标缓存数量，用来约束 quickening 没有重复建项。
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

// 查询本函数的 SemIR opcode，供编译前后契约对照。
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

// 递归查询函数树的 SemIR 和 deopt 备份路径，避免忽略嵌套函数。
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

// 把 SemIR 正常路径与 deopt 路径一同检查，保证优化回退契约。
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

// 递归返回首个指定缓存；调用方若使用索引，必须同步识别其所属函数。
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

// 撤销 fixture 中的元访问缓存 quickening，以隔离测试纯元访问 helper lowering。
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

// 按符号名查函数成员表，为类型化成员缓存断言定位元数据。
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

// 核对符号与成员标志位，避免仅凭 opcode 判断接收者绑定正确。
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

// 把缓存项与成员表符号绑定作交叉断言。TODO: 树搜索可返回子函数缓存，但这里按根函数成员表解释索引；新增子函数调用场景前需传回所属函数。
static void assert_first_callsite_cache_member_binding(const SZrFunction *function,
                                                       EZrFunctionCallSiteCacheKind kind,
                                                       const char *expectedSymbol,
                                                       TZrUInt8 expectedFlags) {
    const SZrFunctionCallSiteCacheEntry *entry = function_tree_find_first_callsite_cache_kind(function, kind);

    TEST_ASSERT_NOT_NULL(entry);
    // TODO: entry 可能来自子函数，此处只能在缓存确属根函数时使用根成员表。
    TEST_ASSERT_TRUE(entry->memberEntryIndex < function->memberEntryLength);
    TEST_ASSERT_NOT_NULL(function->memberEntries);
    TEST_ASSERT_NOT_NULL(function->memberEntries[entry->memberEntryIndex].symbol);
    TEST_ASSERT_EQUAL_STRING(expectedSymbol,
                             ZrCore_String_GetNativeString(function->memberEntries[entry->memberEntryIndex].symbol));
    TEST_ASSERT_EQUAL_UINT8(expectedFlags, function->memberEntries[entry->memberEntryIndex].reserved0);
}

// 从访问语法到 ExecBC 再到 VM 结果核对成员、索引和类型查询仍走独立指令。
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
                "var reflection = typeof(obj);\n"
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

// 同一访问 fixture 在严格 C/LLVM 后端均保留运行时契约，防止借嵌入执行隐藏缺失的 lowering。
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
                "var reflection = typeof(obj);\n"
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

// 确保局部函数常量对应可调用子函数入口，防止生成的模块只保留根函数。
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
                "pub var greet = fn() => {\n"
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
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(state, 0,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(state, 1,"));
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

// 验证 LLVM 最简入口/返回工件结构，作为更复杂 lowering 的基线。
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

// 确保已知静态目标按 LLVM 直达调用帧协议生成，而非走动态分派。
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
                "var greet = fn() => {\n"
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

// 检查 LLVM 闭包捕获读取保留环境访问契约。
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
                "var readCapture = fn() => {\n"
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

// 检查 LLVM 新对象创建将类型与运行时分配路径正确连接。
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

// 检查 LLVM 对对象转换保留必要运行时辅助调用。
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

// 确认 C 模块描述符暴露本机入口，避免回退到解释器 shim。
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
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_fn_0_ins_0:"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_constant_i64_local */"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_s0 = (TZrInt64)7;"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_direct_return_i64_local */"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ResolveGeneratedModuleContext"));
        TEST_ASSERT_NULL(strstr(cText, "ZrAotGeneratedFrame frame"));
        TEST_ASSERT_NULL(strstr(cText, "/* zr_aot_generated_frame_setup */"));
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

// 用静态槽位与整数运算 fixture 守护 C 后端直接 lowering。
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

        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_constant_i64_local */"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_stack_copy_i64"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_exec_i64_binary"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_s4 = ZrCore_Integer_Add(state, zr_aot_s1, (TZrInt64)2);"));
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

// 用基础字面量矩阵核对 C 后端直接构造原始值。
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

        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_constant_i64_local */"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_s0 = (TZrInt64)42;"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_constant_bool_local */"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_b1 = ZR_TRUE;"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_constant_f64_local */"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_f2 = (TZrFloat64)3.5;"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ResetStackNull(state, &frame, 3)"));
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

// 让泛型加法同时保留快路径与类型不匹配时的 helper 回退。
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
                "pub var left = fn() => {\n"
                "    return 15;\n"
                "};\n"
                "pub var right = fn() => {\n"
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
        TEST_ASSERT_TRUE(function_contains_add_int_family(func) ||
                         function_contains_add_signed_family(func));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS)) ||
                         function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_generic_add_test",
                                                            cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_typed_i64_fn_1"));
        TEST_ASSERT_NOT_NULL(strstr(cText,
                                    "zr_aot_result = ZrCore_Integer_Add(state, zr_aot_left_scalar, zr_aot_right_scalar);"));
        TEST_ASSERT_NULL(strstr(cText, "aot_c lowering unsupported"));
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

// 与 C 后端同场景验证 LLVM 的静态槽位及整数运算直接 lowering。
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
        TEST_ASSERT_NULL(strstr(llvmText, " = add i64"));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_CopyConstant("));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_CopyStack("));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_AddInt("));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_AddIntConst("));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_AddSigned("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_AddSignedConst("));
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

// 确保 LLVM 泛型加法兼有数值快路径与运行时回退。
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
                "pub var left = fn() => {\n"
                "    return 15;\n"
                "};\n"
                "pub var right = fn() => {\n"
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
        TEST_ASSERT_TRUE(function_contains_add_int_family(func) ||
                         function_contains_add_signed_family(func));

        remove(llvmPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_llvm_file(state,
                                                               func,
                                                               "execbc_aot_llvm_generic_add_test",
                                                               llvmPath));

        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NULL(strstr(llvmText, " = add i64"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_AddSigned("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(
                llvmText, ZR_INSTRUCTION_ENUM(ADD_SIGNED)));

        free(llvmText);
        remove(llvmPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 确认局部可调用常量在 C 工件中形成直达函数入口。
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
                "pub var greet = fn() => {\n"
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

// 核对仓库中已生成模块图 fixture 的本地 AOT 函数调用形态。TODO: 当前读取静态工件，需用即时生成工件补证。
static void test_aot_c_backend_directly_lowers_local_aot_function_calls(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Directly Lowers Local AOT Function Calls";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c direct function call lowering",
                 "Testing that the representative module-graph AOT C fixture still preserves the runtime direct-call fast path for call targets that are not statically bound by the emitter");

    {
        char *cText = read_repo_file_owned(
                "zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/main.c");
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_TRUE(strlen(cText) > 0);

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

// 对来源已证明的本地函数调用检查静态直达 lowering，防止退回通用调用。
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
                "pub var greet = fn() => {\n"
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

        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_direct_static_function_call */"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_closure->nativeFunction = zr_aot_fn_1;"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_PrepareStaticDirectCall(state,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_static_direct_succeeded = (TZrBool)(zr_aot_fn_1(state) != 0);"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CompletePreparedDirectCallWithResume("));
        TEST_ASSERT_NOT_NULL(strstr(cText, "goto zr_aot_fn_0_dispatch;"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_gc_safepoint_call */"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "frame.observationMask & (ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW | ZR_AOT_GENERATED_STEP_FLAG_CALL | ZR_AOT_GENERATED_STEP_FLAG_RETURN)"));
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

// 检验缓存元调用在 C 后端采用直达帧协议而不丢失元方法接收者。
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
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(func, ZR_SEMIR_OPCODE_META_CALL, ZR_FALSE));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_cached_meta_call_test",
                                                            cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_meta_call_prepare_and_dispatch */"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_PrepareMetaCall(state,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CallPreparedOrGenericWithResume(state,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "goto zr_aot_fn_0_dispatch;"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_UnsupportedMetaCall(state,"));
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

// 检验 LLVM 缓存元调用的直达帧协议，与 C 后端交叉约束。
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
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(func, ZR_SEMIR_OPCODE_META_CALL, ZR_FALSE));

        remove(llvmPath);
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, func, llvmPath));

        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(llvmText);
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_PrepareMetaCall("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_CallPreparedOrGenericWithResume("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "%resume_instruction"));
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

// 先撤销缓存化再生成 C，隔离纯元属性读写所需的运行时 helper。
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
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_value_dynamic_get_member_slot_boundary */"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_value_dynamic_set_member_slot_boundary */"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(state,"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
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

// 验证 LLVM 元尾调用保留直达帧和尾调用控制流。
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
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED)));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_TAIL_CALL, ZR_FALSE));

        remove(llvmPath);
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));

        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(llvmText);
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_PrepareMetaCall("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_CallPreparedOrGenericWithResume("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "%resume_instruction"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i64 @ZrLibrary_AotRuntime_Return("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED)));
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

// 确认 LLVM 异常控制转移与运行时异常辅助层接通。
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

// 检查 C 缓存动态尾调用在转移前准备运行时调用帧。
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
                "fn apply(callable, value: int): int {\n"
                "    return callable(value);\n"
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
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_direct_dynamic_function_call */"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CallDynamicDeoptBridge(state,"));
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

// 确认非导出子函数返回在 C 工件内闭合，不误走模块导出返回协议。
static void test_aot_c_backend_directly_lowers_non_export_return(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Backend Directly Lowers Non Export Return";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot c direct return lowering",
                 "Testing that pure scalar functions use the narrow return helper without materializing an AOT frame");

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

        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_direct_return_i64_local */"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ReturnI64(state,"));
        TEST_ASSERT_NULL(strstr(cText, "SZrCallInfo *zr_aot_call_info"));
        TEST_ASSERT_NULL(strstr(cText, "ZrAotGeneratedFrame frame"));
        TEST_ASSERT_NULL(strstr(cText, "/* zr_aot_generated_frame_setup */"));

        free(cText);
        remove(cPath);
        ZrCore_Function_Free(state, func);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 检查 C 对象转换保留运行时辅助路径。
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

// 检查 C 结构化转换保留运行时辅助路径。
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

// 覆盖有符号比较、除法和取负在 C 后端的专门化路径。
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
        TEST_ASSERT_TRUE(function_contains_sub_int_family(func) ||
                         function_contains_sub_signed_family(func));
        TEST_ASSERT_TRUE(function_contains_div_signed_family(func));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(NEG)) ||
                         function_contains_opcode(func, ZR_INSTRUCTION_ENUM(NEG_SIGNED)));

        remove(cPath);
        TEST_ASSERT_TRUE(write_standalone_strict_aot_c_file(state,
                                                            func,
                                                            "execbc_aot_signed_numeric_paths_test",
                                                            cPath));

        cText = read_text_file_owned(cPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_exec_i64_compare"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_exec_i64_binary"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_exec_i64_unary"));
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

// 用 benchmark 风格组合表达式交叉检查取模、字符串与比较 lowering，并执行解释器结果。
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
            "let container = import(\"zr.container\");\n"
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
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(MOD)) ||
                         function_contains_mod_signed_family(function));
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

        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_exec_i64_binary"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ToString(state, &frame,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_exec_i64_compare"));
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

// 手工构造目标槽与左操作数重叠的字符串相等指令，捕获覆盖顺序错误。
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

// 用通用减法 workload 检验两个后端的快路径和回退保真。
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
            "let container = import(\"zr.container\");\n"
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

        TEST_ASSERT_TRUE(strstr(cText, "/* zr_aot_scalar_exec_i64_binary") != ZR_NULL ||
                         strstr(cText, "ZrLibrary_AotRuntime_Sub(state, &frame,") != ZR_NULL ||
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

// 用通用乘法 workload 检验两个后端的快路径和回退保真。
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
                "    pub fn read(): int {\n"
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
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(MUL)) ||
                         function_contains_mul_signed_family(function));

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

        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_exec_i64_binary"));
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

// 用通用除法 workload 检验两个后端的快路径和回退保真。
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
            "let container = import(\"zr.container\");\n"
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
                         function_contains_div_signed_family(function));

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
                         strstr(cText, "ZrLibrary_AotRuntime_DivSignedConst(state, &frame,") != ZR_NULL ||
                         strstr(cText, "zr_aot_result = zr_aot_left_scalar / zr_aot_right_literal;") != ZR_NULL);
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

// 用位异或 workload 检验有符号/无符号分派后的双后端支持。
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
                "    pub fn step(delta: int): int {\n"
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

        TEST_ASSERT_TRUE(strstr(cText, "ZrLibrary_AotRuntime_BitwiseXor(state, &frame,") != ZR_NULL ||
                         strstr(cText, "/* zr_aot_scalar_exec_i64_bitwise semirOpcode=44") != ZR_NULL);
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

// 检查两个 state 的观察策略互不污染。TODO: 当前两对象仍在同一线程顺序调用，尚未验证线程局部隔离；需增加并发交错测试。
static void test_aot_runtime_observation_policy_is_thread_local(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Runtime Observation Policy Is Thread Local";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot runtime thread local observation policy",
                 "Testing that each state resolves its own AOT observation/debug policy without leaking publish-all or mask overrides across threads");

    {
        // TODO: 两个 state 在同一线程顺序使用；要验证线程隔离需跨线程交错读写。
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

// 注入项目记录后检验生成函数帧的槽数缓存与栈重定位后边界。
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

        nativeClosure->aotShimFunction = ZR_NULL;
        aot_runtime_test_remove_project_record(state, &project);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 验证静态直达调用按被调函数缓存槽数备帧。
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
        SZrAotCodeRegistration codeRegistration;
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
        memset(&codeRegistration, 0, sizeof(codeRegistration));
        memset(&descriptor, 0, sizeof(descriptor));
        memset(&frame, 0, sizeof(frame));
        memset(&directCall, 0, sizeof(directCall));

        childInstructions[0] = make_instruction_constant_operand(ZR_INSTRUCTION_ENUM(GET_CONSTANT), 5, 0);
        childInstructions[1] = make_instruction_return(5);
        init_manual_test_function(&childFunctions[0], childInstructions, 2, ZR_NULL, 0, ZR_NULL, 0, 0);
        childFunctions[0].frameByteSize =
                (TZrUInt32)(sizeof(SZrTypeValueOnStack) * 9u);

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
        TEST_ASSERT_EQUAL_UINT32(9u, runtimeState->records[0].generatedFrameSlotCounts[1]);

        codeRegistration.functionCount = 2;
        codeRegistration.functionPointers = functionThunks;
        descriptor.codeRegistration = &codeRegistration;
        runtimeState->records[0].descriptor = &descriptor;
        runtimeState->records[0].codeRegistration = &codeRegistration;

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
        childFunctions[0].frameByteSize = 0u;

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_PrepareStaticDirectCall(state, &frame, 1, 0, 0, 1, &directCall));
        TEST_ASSERT_TRUE(directCall.prepared);
        TEST_ASSERT_EQUAL_UINT32(1u, directCall.calleeFunctionIndex);
        TEST_ASSERT_EQUAL_PTR(frame.slotBase + frame.generatedFrameSlotCount,
                              directCall.calleeCallInfo->functionBase.valuePointer);
        TEST_ASSERT_EQUAL_PTR(directCall.calleeCallInfo->functionBase.valuePointer + 10,
                              directCall.calleeCallInfo->functionTop.valuePointer);
        TEST_ASSERT_EQUAL_PTR(frame.slotBase + 1, directCall.calleeCallInfo->returnDestination);
        TEST_ASSERT_EQUAL_PTR(childInstructions, directCall.calleeCallInfo->context.context.programCounter);
        TEST_ASSERT_EQUAL_PTR(directCall.calleeCallInfo->functionBase.valuePointer + 1,
                              state->stackTop.valuePointer);

        TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_FinishDirectCall(state, &frame, &directCall, 0));
        TEST_ASSERT_EQUAL_PTR(&state->baseCallInfo, state->callInfoList);
        TEST_ASSERT_EQUAL_PTR(functionBase + 1, frame.slotBase);

        nativeClosure->aotShimFunction = ZR_NULL;
        aot_runtime_test_remove_project_record(state, &project);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 验证整数数组追加可忽略目标槽而仍保持副作用。
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
            "let container = import(\"zr.container\");\n"
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

        nativeClosure->aotShimFunction = ZR_NULL;
        aot_runtime_test_remove_project_record(state, &project);
        ZrLib_TempValueRoot_End(&receiverRoot);
        ZrCore_Function_Free(state, factoryFunction);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 检验原生绑定导致栈重定位后索引 helper 刷新帧。
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
            "let container = import(\"zr.container\");\n"
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

        nativeClosure->aotShimFunction = ZR_NULL;
        aot_runtime_test_remove_project_record(state, &project);
        ZrLib_TempValueRoot_End(&mapRoot);
        ZrCore_Function_Free(state, factoryFunction);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 交叉检查生成指令入口的观察掩码与全量发布策略。
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

// 通过实际 debug hook 观察生成指令入口发布的行事件及最后行号。
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

// 验证运行期动态开启行信号后生成函数继续发布调试事件。
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

// 在有嵌入模块时检查入口函数索引访问仍被 C 后端直接 lowering。
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

// 在无嵌入模块时检查入口函数索引访问仍被 C 后端直接 lowering。
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

// 在有嵌入模块时检查子函数索引访问的 C 后端路径。
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
                "var build = fn() => {\n"
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

// 在无嵌入模块时检查子函数索引访问的 C 后端路径。
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
                "var build = fn() => {\n"
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

// 扫描已提交 C 工件的入口与函数表形态，作为生成代码回归哨兵。
static void test_checked_in_aot_c_fixtures_use_new_code_shape(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Checked In AOT C Fixtures Use New Code Shape";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("checked-in aot c fixtures",
                 "Testing that the committed AOT C fixtures no longer keep legacy prepared-branch, goto-fail, return-0, or shim-entry code shape");

    {
        const char *fixtureFiles[] = {
                "zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_c/src/main.c",
                "zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/bin/aot_c/src/main.c",
                "zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/main.c",
                "zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_stage_a.c",
                "zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_stage_b.c",
                "zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_c/src/graph_binary_stage.c"};
        TZrUInt32 index;

        for (index = 0; index < (TZrUInt32)(sizeof(fixtureFiles) / sizeof(fixtureFiles[0])); index++) {
            char *cText = read_repo_file_owned(fixtureFiles[index]);
            TEST_ASSERT_NOT_NULL(cText);
            TEST_ASSERT_TRUE(strlen(cText) > 0);
            assert_generated_aot_c_text_uses_new_code_shape(cText);
            TEST_ASSERT_NULL(strstr(cText, "ownership.weak"));
            TEST_ASSERT_NULL(strstr(cText, "ownership.upgrade"));
            TEST_ASSERT_NULL(strstr(cText, "ownership.detach"));
            TEST_ASSERT_NULL(strstr(cText, "ownership.release"));
            free(cText);
        }
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 扫描已提交 LLVM 工件，防止真实后端退化为占位或嵌入执行。
static void test_checked_in_aot_llvm_fixtures_use_true_backend_shape(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Checked In AOT LLVM Fixtures Use True Backend Shape";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("checked-in aot llvm fixtures",
                 "Testing that the committed AOT LLVM fixtures no longer keep shim-entry wrappers and instead commit real generated function bodies plus thunk tables");

    {
        const char *fixtureFiles[] = {
                "zr_vm_aot/tests/fixtures/projects/aot_dynamic_meta_ownership_lab/bin/aot_llvm/ir/main.ll",
                "zr_vm_aot/tests/fixtures/projects/aot_eh_tail_gc_stress/bin/aot_llvm/ir/main.ll",
                "zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_llvm/ir/main.ll",
                "zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_llvm/ir/graph_stage_a.ll",
                "zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_llvm/ir/graph_stage_b.ll",
                "zr_vm_aot/tests/fixtures/projects/aot_module_graph_pipeline/bin/aot_llvm/ir/graph_binary_stage.ll"};
        TZrUInt32 index;

        for (index = 0; index < (TZrUInt32)(sizeof(fixtureFiles) / sizeof(fixtureFiles[0])); index++) {
            char *llvmText = read_repo_file_owned(fixtureFiles[index]);
            TEST_ASSERT_NOT_NULL(llvmText);
            TEST_ASSERT_TRUE(strlen(llvmText) > 0);
            assert_generated_aot_llvm_text_uses_true_backend_shape(llvmText);
            TEST_ASSERT_NOT_NULL(strstr(llvmText, "@zr_aot_function_thunks = private constant"));
            TEST_ASSERT_NOT_NULL(strstr(llvmText, "define i64 @zr_aot_entry(ptr %state)"));
            TEST_ASSERT_NULL(strstr(llvmText, "ownership.weak"));
            TEST_ASSERT_NULL(strstr(llvmText, "ownership.upgrade"));
            TEST_ASSERT_NULL(strstr(llvmText, "ownership.detach"));
            TEST_ASSERT_NULL(strstr(llvmText, "ownership.release"));
            TEST_ASSERT_NULL(strstr(llvmText, "ZrCore_Ownership_UpgradeValue"));
            TEST_ASSERT_NULL(strstr(llvmText, "ZrLibrary_AotRuntime_OwnUpgrade"));
            TEST_ASSERT_NOT_NULL(strstr(llvmText, "ZrLibrary_AotRuntime_OwnWake"));
            free(llvmText);
        }
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 扫描源目录归属，约束执行 IR 已按函数持有 CFG 与帧布局。
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

// 检查运行时源码不再依赖后端特例描述符门禁。
static void test_aot_runtime_source_removes_backend_specific_true_aot_descriptor_gate(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT Runtime Source Removes Backend Specific True AOT Descriptor Gate";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot runtime descriptor strictness",
                 "Testing that runtime descriptor strict validation for true AOT payload no longer special-cases C only, so LLVM must satisfy the same embedded-module and thunk-table contract");

    {
        char *runtimeText = read_repo_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c");

        TEST_ASSERT_NOT_NULL(runtimeText);
        TEST_ASSERT_NULL(strstr(runtimeText, "if (backendKind == ZR_AOT_BACKEND_KIND_C &&"));
        TEST_ASSERT_NOT_NULL(strstr(runtimeText, "aot_runtime_validate_descriptor("));
        TEST_ASSERT_NOT_NULL(strstr(runtimeText, "descriptor->embeddedModuleBlob == ZR_NULL"));
        TEST_ASSERT_NOT_NULL(strstr(runtimeText, "descriptor->functionThunks == ZR_NULL"));

        free(runtimeText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 扫描生成器源文件，约束 LLVM 发射器不回流到 C 后端主体。
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

// 约束 C/LLVM writer 入口归属独立单元，防止主体再次膨胀。
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

// 约束共享函数表及可调用来源分析归属公共层。
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

// 按 lowering 家族检查 C 发射器拆分后的职责边界。
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
        char *cTypedArithmeticText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c");
        char *cCallBoundariesText =
                read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_boundaries.c");
        char *cLoweringControlText = read_repo_file_owned("zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c");

        TEST_ASSERT_NOT_NULL(cEmitterText);
        TEST_ASSERT_NOT_NULL(cFunctionBodyText);
        TEST_ASSERT_NOT_NULL(cFunctionBodyHeaderText);
        TEST_ASSERT_NOT_NULL(cTypedArithmeticText);
        TEST_ASSERT_NOT_NULL(cCallBoundariesText);
        TEST_ASSERT_NOT_NULL(cLoweringControlText);

        TEST_ASSERT_NULL(strstr(cEmitterText, "static void backend_aot_write_c_function_body("));
        TEST_ASSERT_NULL(strstr(cEmitterText, "void backend_aot_write_c_direct_meta_get("));
        TEST_ASSERT_NULL(strstr(cEmitterText, "void backend_aot_write_c_direct_mul_signed("));
        TEST_ASSERT_NULL(strstr(cEmitterText, "void backend_aot_write_c_try("));
        TEST_ASSERT_NULL(strstr(cEmitterText, "void backend_aot_write_c_dispatch_loop("));

        TEST_ASSERT_NOT_NULL(strstr(cFunctionBodyHeaderText, "backend_aot_write_c_function_body("));
        TEST_ASSERT_NOT_NULL(strstr(cFunctionBodyText, "backend_aot_write_c_function_body("));
        TEST_ASSERT_NOT_NULL(strstr(cFunctionBodyText, "case ZR_INSTRUCTION_ENUM(META_GET):"));
        TEST_ASSERT_NOT_NULL(strstr(cTypedArithmeticText, "backend_aot_write_c_direct_mul_signed("));
        TEST_ASSERT_NOT_NULL(strstr(cCallBoundariesText, "backend_aot_write_c_direct_function_call("));
        TEST_ASSERT_NOT_NULL(strstr(cCallBoundariesText, "backend_aot_write_c_static_direct_function_call("));
        TEST_ASSERT_NOT_NULL(strstr(cLoweringControlText, "backend_aot_write_c_dispatch_loop("));
        TEST_ASSERT_NOT_NULL(strstr(cLoweringControlText, "backend_aot_write_c_try("));
        TEST_ASSERT_NOT_NULL(strstr(cLoweringControlText, "backend_aot_write_c_set_pending_continue("));

        free(cEmitterText);
        free(cFunctionBodyText);
        free(cFunctionBodyHeaderText);
        free(cTypedArithmeticText);
        free(cCallBoundariesText);
        free(cLoweringControlText);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 按 lowering 家族检查 LLVM 发射器拆分后的职责边界。
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
        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsHeaderText, "#include \"backend_aot/backend_aot_llvm_module_prelude.h\""));
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

// 约束 LLVM 对象 lowering 的细分归属，避免单文件重新承载全部对象操作。
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

// 约束 LLVM 成员与索引 lowering 的细分归属。
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

// 约束 LLVM 调用 lowering 各子家族的文件归属。
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

// 检查 LLVM 文本发射 helper 已脱离控制流 lowering 文件。
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
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "#include \"backend_aot/backend_aot_llvm_text_call_result.h\""));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "#include \"backend_aot/backend_aot_llvm_text_terminal.h\""));
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

// 约束 LLVM 模块工件生成与核心 emitter 的职责分离。
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

        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsHeaderText, "#include \"backend_aot/backend_aot_llvm_module_prelude.h\""));
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

// 约束 LLVM 模块前导 helper 与工件组装的职责分离。
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

        TEST_ASSERT_NOT_NULL(strstr(llvmModuleArtifactsHeaderText, "#include \"backend_aot/backend_aot_llvm_module_prelude.h\""));
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

// 约束 LLVM 流程文本 helper 与一般文本发射的职责分离。
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
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "#include \"backend_aot/backend_aot_llvm_text_call_result.h\""));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "#include \"backend_aot/backend_aot_llvm_text_terminal.h\""));
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

// 检查 LLVM 流程文本已分到具体子家族。
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

        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "#include \"backend_aot/backend_aot_llvm_text_call_result.h\""));
        TEST_ASSERT_NOT_NULL(strstr(llvmTextFlowHeaderText, "#include \"backend_aot/backend_aot_llvm_text_terminal.h\""));
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

// 检查 LLVM 控制流 lowering 的子家族归属。
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

// 检查 LLVM 闭包 lowering 的子家族归属。
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

// 以零参调用验证 ExecBC 可专门化而 SemIR 原始语义保持稳定。
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
                "fn answer(): int {\n"
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
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED)));
        TEST_ASSERT_FALSE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(META_CALL)));
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(func, ZR_SEMIR_OPCODE_META_CALL, ZR_FALSE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, func, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SEMIR"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_KNOWN_VM_CALL_NO_ARGS"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_CALL_CACHED"));

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

// 用类型化数值及相等矩阵检验 quickening 与严格双后端覆盖一致。
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

// 覆盖已知 native 调用族在 ExecBC 与双后端的目标来源和调用协议。
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
        const char *source =
                "let system = import(\"zr.system\");\n"
                "system.console.printLine(\"KNOWN_NATIVE_CALL_CURRENT_SURFACE\");\n"
                "return 1;\n";
        char *intermediateText;
        char *cText;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);
        TEST_ASSERT_TRUE(ZrVmLibSystem_Register(state->global));

        sourceName = ZR_STRING_LITERAL(state, "known_native_call_current_surface.zr");
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
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_direct_dynamic_function_call */"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CallDynamicDeoptBridge(state,"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "@ZrLibrary_AotRuntime_PrepareDirectCall"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "@ZrLibrary_AotRuntime_CallPreparedOrGeneric"));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL)));

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

// 把元/动态调用缓存 quickening 与二进制回读并测，防止序列化丢失缓存元数据。
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
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_CALL, ZR_FALSE));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_DYN_CALL, ZR_FALSE));

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

// 序列化并重载固定数组 helper fixture，核对参数值未被索引重映射。
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

// 跨二进制 roundtrip 核对容器矩阵的 native 调用参数顺序和值。
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

// 加载序列化模块后核对指令调试行号与调用缓存信息。
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

// 让 Map 内的数组经历编译/序列化/加载/执行，保护动态迭代器契约。
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

// 核对链式 pair helper 在二进制回读后仍传递正确值。
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

// 核对 Set 中 pair 的哈希及迭代结果在回读后保持。
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

// 核对 Set 转 Map 的 bucket 值在回读与执行后保持。
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

// 为元属性 get/set 与原始字段访问构造统一源码，供缓存和纯 helper 两条路径对照。
static SZrFunction *compile_meta_access_fixture(SZrState *state) {
    const char *source =
            "class Box {\n"
            "    pub var raw: int;\n"
            "    pub @constructor(raw: int) {\n"
            "        this.raw = raw;\n"
            "    }\n"
            "    pub property value: int {\n"
            "        get { return this.raw + 1; }\n"
            "        set { this.raw = value + 2; }\n"
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

// 构造类型化成员槽访问，检验缓存项和槽 opcode 的编译期选择。
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

// 构造类型化成员方法调用，连接成员缓存与已知 VM 调用族。
static SZrFunction *compile_typed_member_known_call_fixture(SZrState *state) {
    const char *source =
            "class Counter {\n"
            "    pub var value: int;\n"
            "    pub fn step(delta: int): int {\n"
            "        this.value = this.value + delta;\n"
            "        return this.value;\n"
            "    }\n"
            "    pub fn read(): int {\n"
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

// 构造接收者类型不确定的成员访问，检验编译器保留通用路径。
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

// 构造连续类型化成员访问，检查嵌套链每一级的槽选择。
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

// 构造对象解构 fixture，检验解构读成员仍使用类型化槽。
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

// 构造基类成员查找与元调用组合，区分直接基类查找和动态分派。
static SZrFunction *compile_super_member_dispatch_fixture(SZrState *state) {
    const char *source =
            "class BaseCounter {\n"
            "    pub var baseValue: int;\n"
            "    pub virtual fn bump(): int {\n"
            "        return this.baseValue + 1;\n"
            "    }\n"
            "    pub virtual property score: int {\n"
            "        get { return this.baseValue + 10; }\n"
            "    }\n"
            "    pub virtual @call(): int {\n"
            "        return this.baseValue + 20;\n"
            "    }\n"
            "}\n"
            "class DerivedCounter: BaseCounter {\n"
            "    pub override fn bump(): int {\n"
            "        return super.bump() + 1;\n"
            "    }\n"
            "    pub override property score: int {\n"
            "        get { return super.score + 2; }\n"
            "    }\n"
            "    pub override @call(): int {\n"
            "        return super.call() + 5;\n"
            "    }\n"
            "    pub fn total(): int {\n"
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

// 构造静态已知元属性访问，供调用点缓存专门化用例。
static SZrFunction *compile_static_meta_access_fixture(SZrState *state) {
    const char *source =
            "class Counter {\n"
            "    pub static property count: int {\n"
            "        get { return 5; }\n"
            "        set { var sink = value; }\n"
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

// 按参考脚本路径编译源码，为已提交语义样例的工件检查提供统一入口。
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

// 构造零参尾调用，供 SemIR/ExecBC 与后端协议对照。
static SZrFunction *compile_zero_arg_tail_quickening_fixture(SZrState *state) {
    const char *source =
            "fn answer(): int {\n"
            "    return 11;\n"
            "}\n"
            "fn callDirectTail(): int {\n"
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
            "fn callMetaTail(counter: Counter): int {\n"
            "    return counter();\n"
            "}\n"
            "fn callDynTail(callable): int {\n"
            "    return callable();\n"
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

// 构造异常控制转移 fixture，检验后端异常 helper 路径。
static SZrFunction *compile_exception_control_fixture(SZrState *state) {
    const char *source =
            "fn guarded(flag: int): int {\n"
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

// 构造共享的元/动态缓存调用点，供 quickening 与二进制元数据回读测试。
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
            "fn apply(callable, value: int): int {\n"
            "    var result = callable(value);\n"
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

// 构造拥有者与引用视图场景，检查专用 ownership 指令贯穿后端。
static SZrFunction *compile_ownership_intrinsics_reference_views_fixture(SZrState *state) {
    const char *source =
            "resource class Box {}\n"
            "fn run(): int {\n"
            "var owner = own Box();\n"
            "var shared = share(owner);\n"
            "var watcher = degrade(shared);\n"
            "var borrowAlive = false;\n"
            "{\n"
            "    var borrowed: ref readonly Box = ref shared;\n"
            "    borrowAlive = borrowed != null;\n"
            "}\n"
            "var awakened = wake(watcher);\n"
            "var loanSource = own Box();\n"
            "var loanAlive = false;\n"
            "{\n"
            "    var loaned: ref Box = ref loanSource;\n"
            "    loanAlive = loaned != null;\n"
            "}\n"
            "var loanSourceRestored = loanSource != null;\n"
            "var gcBoxSource = own Box();\n"
            "var gcBox = intoGc(gcBoxSource);\n"
            "var gcBoxAlive = gcBox != null;\n"
            "var droppedShared = drop(shared);\n"
            "var droppedAwakened = drop(awakened);\n"
            "var after = wake(watcher);\n"
            "if (borrowAlive && loanAlive && loanSourceRestored && gcBoxAlive && droppedAwakened == null && droppedShared == null && after == null) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

        sourceName = ZR_STRING_LITERAL(state, "ownership_intrinsics_reference_views_pipeline_test.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

// 构造资源唯一持有与释放顺序场景，供 VM 和 AOT 对照。
static SZrFunction *compile_resource_unique_drop_fixture(SZrState *state) {
    const char *source =
            "resource class Tracer {\n"
            "    pub static var dropLog: int = 0;\n"
            "    var id: int;\n"
            "    pub @constructor(id: int) { this.id = id; }\n"
            "    pub @destructor() { Tracer.dropLog = Tracer.dropLog * 10 + this.id; }\n"
            "}\n"
            "fn run(): int {\n"
            "    {\n"
            "        var first: Unique<Tracer> = own Tracer(1);\n"
            "        var moved: Unique<Tracer> = first;\n"
            "        var second: Unique<Tracer> = own Tracer(2);\n"
            "        drop(second);\n"
            "    }\n"
            "    return Tracer.dropLog;\n"
            "}\n"
            "return run();\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZR_STRING_LITERAL(state, "resource_unique_drop_aot_pipeline_test.zr");
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

// 构造固定数组 helper 参数场景，突出序列化后的实参一致性。
static SZrFunction *compile_fixed_array_helper_roundtrip_fixture(SZrState *state) {
    const char *source =
            "fn labelFor(value: int) {\n"
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

// 构造容器矩阵与 native 调用参数场景，跨二进制回读验证。
static SZrFunction *compile_container_matrix_roundtrip_fixture(SZrState *state) {
    const char *source =
            "let container = import(\"zr.container\");\n"
            "let {Array, Map, Set, LinkedList, Pair} = import(\"zr.container\");\n"
            "fn labelFor(value: int) {\n"
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

// 构造整数数组读写源码，观察索引快路径选择。
static SZrFunction *compile_array_int_index_quickening_fixture(SZrState *state) {
    const char *source =
            "let container = import(\"zr.container\");\n"
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

// 构造连续整数数组追加，检验批量指令专门化。
static SZrFunction *compile_array_int_add_burst_fixture(SZrState *state) {
    const char *source =
            "let container = import(\"zr.container\");\n"
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

// 构造整数数组填充循环，检验循环到批量派发的专门化。
static SZrFunction *compile_array_int_fill_loop_fixture(SZrState *state) {
    const char *source =
            "let container = import(\"zr.container\");\n"
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

// 构造 Map 内数组可迭代场景，隔离容器嵌套后的协议保持。
static SZrFunction *compile_map_array_roundtrip_fixture(SZrState *state) {
    const char *source =
            "let container = import(\"zr.container\");\n"
            "let {Array, Map} = import(\"zr.container\");\n"
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

// 构造链式 pair helper 输入，检验二进制回读的值传递。
static SZrFunction *compile_linked_pair_roundtrip_fixture(SZrState *state) {
    const char *source =
            "let container = import(\"zr.container\");\n"
            "let {LinkedList, Pair} = import(\"zr.container\");\n"
            "fn labelFor(value: int) {\n"
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

// 构造 Set 中 pair 哈希/迭代场景，检验回读保持集合契约。
static SZrFunction *compile_set_pair_roundtrip_fixture(SZrState *state) {
    const char *source =
            "let container = import(\"zr.container\");\n"
            "let {Set, Pair} = import(\"zr.container\");\n"
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

// 构造 Set 转 Map bucket 场景，核对回读后键值对应关系。
static SZrFunction *compile_set_to_map_roundtrip_fixture(SZrState *state) {
    const char *source =
            "let container = import(\"zr.container\");\n"
            "let {Array, Map, Set, Pair} = import(\"zr.container\");\n"
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

// 从元属性源码核对 SemIR、ExecBC 与严格 C 工件的专用 get/set 协议。
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
        assert_member_entry_symbol_flags(function, "value", 0);
        assert_first_callsite_cache_member_binding(function,
                                                   ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET,
                                                   "value",
                                                   0);
        assert_first_callsite_cache_member_binding(function,
                                                   ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET,
                                                   "value",
                                                   0);
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_GET, ZR_FALSE));
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_SET, ZR_FALSE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));

        intermediateText = read_text_file_owned(intermediatePath);
        {
            char *cText = read_text_file_owned(cPath);
            TEST_ASSERT_NOT_NULL(cText);
            TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(state,"));
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
                                                   "value",
                                                   0);
        assert_first_callsite_cache_member_binding(runtimeFunction,
                                                   ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET,
                                                   "value",
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

// 检查静态元访问 quickening 分配相应调用点缓存而非通用路径。
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
        assert_member_entry_symbol_flags(function, "count", ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR);
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
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_GET, ZR_FALSE));
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_SET, ZR_FALSE));

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
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(state,"));
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

// 验证 super 的方法、属性和元调用都从基类直接查找，避免回到子类分派。
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

// 核对类型化成员访问的缓存与符号绑定，防止索引存在却指向错成员。
static void test_typed_member_access_allocates_member_callsite_cache_entries(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Typed Member Access Allocates Member Callsite Cache Entries";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("member slot quickening",
                 "Testing that typed class field access stops staying on plain GET_MEMBER/SET_MEMBER and starts allocating dedicated member callsite cache metadata");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        TZrChar binaryPath[ZR_TESTS_PATH_MAX];
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("execbc_aot_pipeline",
                                                           "binary",
                                                           "member_slot_quickening_test",
                                                           ".zro",
                                                           binaryPath,
                                                           sizeof(binaryPath)));
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
        remove(binaryPath);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 让类型化成员方法走已知 VM 调用族，同时保留成员接收者信息。
static void test_typed_member_calls_quicken_to_known_vm_call_family(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Typed Member Calls Quicken To Known VM Call Family";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("member call quickening",
                 "Testing that typed instance method calls stop staying on generic FUNCTION_CALL and instead lower into GET_MEMBER_SLOT plus the KNOWN_VM_CALL family, including zero-arg variants.");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        TZrChar binaryPath[ZR_TESTS_PATH_MAX];
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("execbc_aot_pipeline",
                                                           "binary",
                                                           "typed_member_known_call_test",
                                                           ".zro",
                                                           binaryPath,
                                                           sizeof(binaryPath)));
        function = compile_typed_member_known_call_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)) ||
                         function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS)) ||
                         function_contains_opcode(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)) ||
                         function_contains_opcode(function, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8)));
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
        TEST_ASSERT_TRUE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL)) ||
                         function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SUPER_KNOWN_VM_CALL_NO_ARGS)) ||
                         function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL)) ||
                         function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(KNOWN_VM_MEMBER_CALL_LOAD1_U8)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(FUNCTION_CALL)));
        TEST_ASSERT_FALSE(function_contains_opcode(runtimeFunction, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS)));
        TEST_ASSERT_TRUE(function_count_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_MEMBER_GET) >= 2);

        result = 0;
        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, runtimeFunction, &result));
        TEST_ASSERT_EQUAL_INT64(14, result);

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

// 动态对象接收者应保留通用成员 opcode，防止过早套用类型化槽。
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

// 逐层核对嵌套类型化成员链的槽访问选择。
static void test_nested_typed_member_chain_emits_member_slot_opcodes(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Nested Typed Member Chain Emits Member Slot Opcodes";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("member slot nested chain quickening",
                 "Testing that strongly typed chained field access lowers all the way to member-slot opcodes instead of falling back to generic GET_MEMBER/SET_MEMBER on temporary receivers");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        TZrChar binaryPath[ZR_TESTS_PATH_MAX];
        SZrFunction *function;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("execbc_aot_pipeline",
                                                           "binary",
                                                           "nested_member_slot_quickening_test",
                                                           ".zro",
                                                           binaryPath,
                                                           sizeof(binaryPath)));
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

// 检验类型化对象解构的读成员路径复用专用槽指令。
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

// 用仓库属性参考样例校验元访问工件和严格 C lowering 的一致性。
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
        assert_member_entry_symbol_flags(function, "value", 0);
        assert_first_callsite_cache_member_binding(function,
                                                   ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET,
                                                   "value",
                                                   0);
        assert_first_callsite_cache_member_binding(function,
                                                   ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET,
                                                   "value",
                                                   0);
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_GET, ZR_FALSE));
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_SET, ZR_FALSE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));

        intermediateText = read_text_file_owned(intermediatePath);
        {
            char *cText = read_text_file_owned(cPath);
            TEST_ASSERT_NOT_NULL(cText);
            TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(state,"));
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

// 用仓库参考样例守护成员/索引访问拆分后的工件形态。
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
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER)) ||
                         function_contains_opcode(function, ZR_INSTRUCTION_ENUM(GET_MEMBER_SLOT)));
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

// 用仓库 foreach 参考样例守护迭代协议的双后端工件。
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
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ITER_INIT)) ||
                         function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DYN_ITER_INIT)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT)) ||
                         function_contains_opcode(function, ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT)) ||
                         function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_ITER_MOVE_NEXT_JUMP_IF_FALSE)) ||
                         function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE)));
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

// 零参尾调用 quickening 只改变 ExecBC 形态，SemIR 原始契约须稳定。
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
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_TAIL_CALL_CACHED)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(META_TAIL_CALL)));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_DYN_TAIL_CALL, ZR_FALSE));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_TAIL_CALL, ZR_FALSE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_FUNCTION_TAIL_CALL_NO_ARGS"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_DYN_TAIL_CALL_NO_ARGS"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_TAIL_CALL_CACHED"));
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

// 数组整数索引快路径须被 quicken，并得到严格 C/LLVM helper 支持。
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
        TEST_ASSERT_TRUE(function_contains_super_array_set_int_family(function));
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
        TEST_ASSERT_TRUE(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SuperArrayGetInt(") != ZR_NULL ||
                         strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SuperArrayGetIntBoundItems(") != ZR_NULL);
        TEST_ASSERT_TRUE(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SuperArraySetInt(") != ZR_NULL ||
                         strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SuperArraySetIntBoundItems(") != ZR_NULL);
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_ADD_INT)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT)));
        TEST_ASSERT_FALSE(
                aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST)));
        TEST_ASSERT_FALSE(
                aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(
                llvmText, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT)));
        TEST_ASSERT_FALSE(
                aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(SUPER_ARRAY_SET_INT_ITEMS)));
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
        TEST_ASSERT_TRUE(function_contains_super_array_set_int_family(runtimeFunction));
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

// 连续整数数组追加应融合为专用派发 opcode，并保持结果。
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

// 填充循环应被识别为批量派发路径，同时保留循环结果。
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

// 交叉检查引用视图 ownership 指令从 SemIR 到严格双后端未退化。
static void test_ownership_intrinsics_reference_views_semir_and_true_aot_preserve_dedicated_opcodes(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary =
            "Ownership Intrinsics Reference Views SemIR And True AOT Preserve Dedicated Opcodes";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("ownership intrinsics, reference views, and GC bridge semantic pipeline",
                 "Testing that share/degrade/ref/ref readonly/intoGc/wake/drop keep dedicated ExecBC and SemIR ownership opcodes, reject legacy borrow/loan/detach emission, and lower through explicit runtime helpers without shim fallback");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "ownership_intrinsics_reference_views_pipeline_test.zri";
        const char *cPath = "ownership_intrinsics_reference_views_pipeline_test.c";
        const char *llvmPath = "ownership_intrinsics_reference_views_pipeline_test.ll";
        SZrFunction *function;
        char *intermediateText;
        char *cText;
        char *llvmText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_ownership_intrinsics_reference_views_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_SHARE)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_DEGRADE)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_VIEW_SHARED)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_VIEW_MUT)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_RETURN_LOAN)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_BORROW)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_LOAN)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_INTO_GC_BOX)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_DETACH)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_WAKE)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_DROP)));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_OWN_SHARE, ZR_FALSE));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_OWN_DEGRADE, ZR_FALSE));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_OWN_VIEW_SHARED, ZR_FALSE));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_OWN_VIEW_MUT, ZR_FALSE));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_OWN_RETURN_LOAN, ZR_FALSE));
        TEST_ASSERT_FALSE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_OWN_BORROW, ZR_FALSE));
        TEST_ASSERT_FALSE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_OWN_LOAN, ZR_FALSE));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_OWN_INTO_GC_BOX, ZR_FALSE));
        TEST_ASSERT_FALSE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_OWN_DETACH, ZR_FALSE));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_OWN_WAKE, ZR_FALSE));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_OWN_DROP, ZR_FALSE));

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
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_DEGRADE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_VIEW_SHARED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_VIEW_MUT"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_RETURN_LOAN"));
        TEST_ASSERT_NULL(strstr(intermediateText, "OWN_BORROW"));
        TEST_ASSERT_NULL(strstr(intermediateText, "OWN_LOAN"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_INTO_GC_BOX"));
        TEST_ASSERT_NULL(strstr(intermediateText, "OWN_DETACH"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_WAKE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_DROP"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnShare"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnDegrade"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnBorrow"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnLoan"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnReturnLoan"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnIntoGcBox"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnDetach"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnWake"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnDrop"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnShare("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnDegrade("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnBorrow("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnLoan("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnReturnLoan("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnIntoGcBox("));
        TEST_ASSERT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnDetach("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnWake("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnDrop("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_SHARE)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_DEGRADE)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_VIEW_SHARED)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_VIEW_MUT)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_RETURN_LOAN)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_INTO_GC_BOX)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_WAKE)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText, ZR_INSTRUCTION_ENUM(OWN_DROP)));

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

// 验证可选/直接弱接收者守卫在真实 AOT 中保留失效分支。
static void test_receiver_guards_preserve_optional_and_direct_weak_contracts_in_true_aot(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Receiver Guards Preserve Optional And Direct Weak Contracts In True AOT";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("weak receiver guard AOT parity",
                 "Testing optional success, optional argument skipping after expiry, and catchable direct NullReferenceError across ExecBC, SemIR, AOT C, and AOT LLVM");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *source =
                "resource class Service {\n"
                "    pub const fn add(value: int): int { return value + 10; }\n"
                "    pub const @call(value: int): int {\n"
                "        if (value == 99) { throw \"receiver guard callable failure\"; }\n"
                "        return value + 20;\n"
                "    }\n"
                "}\n"
                "fn failIfEvaluated(): int { throw \"receiver guard evaluated arguments\"; }\n"
                "fn tailCall(target: Weak<Service>, value: int): int { return target(value); }\n"
                "fn run(): int {\n"
                "    var seed = own Service();\n"
                "    var shared = share(seed);\n"
                "    var weak = degrade(shared);\n"
                "    var live = weak?.add(1);\n"
                "    var liveCallable = weak?.(1);\n"
                "    var observed = 0;\n"
                "    if (live != null) { observed = 10; }\n"
                "    if (liveCallable == 21) { observed = observed + 100; }\n"
                "    var callableSuffixCaught = false;\n"
                "    try { weak(99); }\n"
                "    catch (error) { callableSuffixCaught = true; }\n"
                "    var tailCallableCaught = false;\n"
                "    try { tailCall(weak, 99); }\n"
                "    catch (error) { tailCallableCaught = true; }\n"
                "    drop(shared);\n"
                "    var expired = weak?.add(failIfEvaluated());\n"
                "    var expiredCallable = weak?.(failIfEvaluated());\n"
                "    var caught = 0;\n"
                "    try { weak.add(failIfEvaluated()); }\n"
                "    catch (error: NullReferenceError) { caught = 1; }\n"
                "    var callableCaught = 0;\n"
                "    try { weak(failIfEvaluated()); }\n"
                "    catch (error: NullReferenceError) { callableCaught = 1; }\n"
                "    if (callableSuffixCaught && tailCallableCaught &&\n"
                "        expired == null && expiredCallable == null) {\n"
                "        return observed + caught + callableCaught;\n"
                "    }\n"
                "    return 0;\n"
                "}\n"
                "return run();\n";
        const char *intermediatePath = "receiver_guard_aot_parity_test.zri";
        const char *cPath = "receiver_guard_aot_parity_test.c";
        const char *llvmPath = "receiver_guard_aot_parity_test.ll";
        SZrString *sourceName;
        SZrFunction *function;
        char *intermediateText;
        char *cText;
        char *llvmText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);
        ZrParser_ToGlobalState_Register(state);
        TEST_ASSERT_TRUE(ZrVmLibSystem_Register(state->global));
        sourceName = ZR_STRING_LITERAL(state, "receiver_guard_aot_parity_test.zr");
        TEST_ASSERT_NOT_NULL(sourceName);
        function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_WAKE)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(JUMP_IF_NULL)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(REQUIRE_NON_NULL)));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function,
                                                               ZR_SEMIR_OPCODE_REQUIRE_NON_NULL,
                                                               ZR_FALSE));

        remove(intermediatePath);
        remove(cPath);
        remove(llvmPath);
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));

        intermediateText = read_text_file_owned(intermediatePath);
        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);
        TEST_ASSERT_TRUE_MESSAGE(
                count_substring_occurrences(
                        intermediateText, "JUMP_IF_NULL") >= 2u,
                "method and callable optional guards must reach ExecBC");
        TEST_ASSERT_TRUE_MESSAGE(
                count_substring_occurrences(
                        intermediateText, "REQUIRE_NON_NULL") >= 2u,
                "method and callable direct guards must reach ExecBC");
        TEST_ASSERT_TRUE_MESSAGE(
                count_substring_occurrences(
                        cText, "/* zr_aot_jump_if_null */") >= 2u,
                "method and callable optional guards must reach AOT C");
        TEST_ASSERT_TRUE_MESSAGE(
                count_substring_occurrences(
                        cText,
                        "ZrLibrary_AotRuntime_RequireNonNull(state, &frame,") >= 2u,
                "method and callable direct guards must reach AOT C");
        TEST_ASSERT_NOT_NULL(strstr(cText,
                                    "ZrLibrary_AotRuntime_PrepareMetaCall(state,"));
        TEST_ASSERT_NOT_NULL(strstr(cText,
                                    "ZrLibrary_AotRuntime_CallPreparedOrGenericWithResume(state,"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText,
                                    "call i1 @ZrLibrary_AotRuntime_CallPreparedOrGenericWithResume("));
        TEST_ASSERT_NULL(strstr(cText,
                                "ZrLibrary_AotRuntime_UnsupportedMetaCall(state,"));
        TEST_ASSERT_TRUE_MESSAGE(
                count_substring_occurrences(
                        llvmText,
                        "call i1 @ZrLibrary_AotRuntime_IsNull(") >= 2u,
                "method and callable optional guards must reach AOT LLVM");
        TEST_ASSERT_TRUE_MESSAGE(
                count_substring_occurrences(
                        llvmText,
                        "call i1 @ZrLibrary_AotRuntime_RequireNonNull(") >= 2u,
                "method and callable direct guards must reach AOT LLVM");
        TEST_ASSERT_FALSE(aot_c_text_contains_unsupported_opcode(cText,
                                                                 ZR_INSTRUCTION_ENUM(JUMP_IF_NULL)));
        TEST_ASSERT_FALSE(aot_c_text_contains_unsupported_opcode(cText,
                                                                 ZR_INSTRUCTION_ENUM(REQUIRE_NON_NULL)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(JUMP_IF_NULL)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(llvmText,
                                                                    ZR_INSTRUCTION_ENUM(REQUIRE_NON_NULL)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(112, result);

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

// 比较 VM 与 AOT 的唯一资源释放顺序，防止优化改变清理时序。
static void test_resource_unique_drop_vm_and_aot_preserve_cleanup_order_contract(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Resource Unique Drop VM And AOT Preserve Cleanup Order Contract";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("resource Unique deterministic cleanup",
                 "Testing resource construction, move, explicit drop, and reverse scope cleanup through canonical ExecBC/SemIR plus true AOT C/LLVM ownership helpers");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "resource_unique_drop_aot_pipeline_test.zri";
        const char *cPath = "resource_unique_drop_aot_pipeline_test.c";
        const char *llvmPath = "resource_unique_drop_aot_pipeline_test.ll";
        SZrFunction *function;
        char *intermediateText;
        char *cText;
        char *llvmText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);
        function = compile_resource_unique_drop_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function,
                                                        ZR_INSTRUCTION_ENUM(OWN_UNIQUE)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function,
                                                        ZR_INSTRUCTION_ENUM(OWN_DROP)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function,
                                                        ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED)));
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function,
                                                        ZR_INSTRUCTION_ENUM(CLOSE_SCOPE)));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function,
                                                               ZR_SEMIR_OPCODE_OWN_UNIQUE,
                                                               ZR_FALSE));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function,
                                                               ZR_SEMIR_OPCODE_OWN_DROP,
                                                               ZR_FALSE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state,
                                                               function,
                                                               intermediatePath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));

        intermediateText = read_text_file_owned(intermediatePath);
        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_UNIQUE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_DROP"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnUnique"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_OwnDrop"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_MarkToBeClosed"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CloseScope"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_InvokeActiveShim"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnUnique("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_OwnDrop("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_MarkToBeClosed("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_CloseScope("));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(
                llvmText, ZR_INSTRUCTION_ENUM(OWN_UNIQUE)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(
                llvmText, ZR_INSTRUCTION_ENUM(OWN_DROP)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(
                llvmText, ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED)));
        TEST_ASSERT_FALSE(aot_llvm_text_contains_unsupported_opcode(
                llvmText, ZR_INSTRUCTION_ENUM(CLOSE_SCOPE)));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(21, result);

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

// 只验证 benchmark 字符串构建模块能序列化并加载入口。TODO: 当前不执行基准主体，不能证明运行时拼接行为。
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

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_TRUE(ZrVmLibContainer_Register(state->global));

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
        TEST_ASSERT_TRUE(runtimeFunction->instructionsLength > 0u);

        ZrCore_Function_Free(state, runtimeFunction);
        ZrCore_Io_ReadSourceFree(state->global, sourceObject);
        free(binaryBytes);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

// 手工构造扩展数值 opcode 并检查严格双后端均无 unsupported 标记；运行时语义仍需独立执行验证。
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
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_ConvertGenericToBool"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_exec_to_u64 opcode="));
        TEST_ASSERT_NOT_NULL(strstr(cText, "/* zr_aot_scalar_exec_to_f64 opcode="));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_GenericPower"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_GenericPrimitiveLogicalNot"));
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

// 用常量除/模场景核对 C 观察消除与 LLVM 观察标记，不掩盖零除异常。
static void test_aot_c_observation_elision_and_llvm_flags_preserve_const_div_mod_contract(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C Observation Elision And LLVM Flags Preserve Const Div Mod Contract";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot const div mod observation flags",
                 "Testing that AOT C omits per-instruction observation while LLVM uses NONE for non-zero signed constants and MAY_THROW for zero constants");

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
        assert_generated_aot_c_elides_instruction_observation(cText);
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
        assert_generated_aot_c_elides_instruction_observation(cText);
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

// 手工构造状态和作用域管理 opcode，覆盖源码通常难以稳定产生的 lowering 分支。
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
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_value_exec_set_constant"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "*zr_aot_constant = *zr_aot_source;"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_SetConstant"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_value_get_sub_function_native_closure_boundary"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_GetSubFunctionNativeClosure(state,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_scope_mark_to_be_closed"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_MarkToBeClosed(state, &frame,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "zr_aot_scope_close_scope"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CloseScope(state, &frame,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_PrepareStaticDirectCall(state,"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrLibrary_AotRuntime_CompletePreparedDirectCallWithResume("));
        TEST_ASSERT_NOT_NULL(strstr(cText, "goto zr_aot_fn_"));
        TEST_ASSERT_NULL(strstr(cText, "ZrLibrary_AotRuntime_PrepareDirectCall(state,"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_SetConstant("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_GetSubFunction("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_MarkToBeClosed("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_CloseScope("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_PrepareStaticDirectCall("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i64 @zr_aot_fn_1(ptr %state)"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "call i1 @ZrLibrary_AotRuntime_CompletePreparedDirectCallWithResume("));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "%resume_instruction"));
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

// 跨运行时与双后端源码扫描扩展 opcode 支持入口。TODO: 字符串存在性易受无关注释/死代码影响，需以构建或执行检查补证。
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
            {"MARK_TO_BE_CLOSED",
             "ZrLibrary_AotRuntime_MarkToBeClosed",
             "backend_aot_write_c_direct_mark_to_be_closed"},
            {"CLOSE_SCOPE", "ZrLibrary_AotRuntime_CloseScope", "backend_aot_write_c_direct_close_scope"}};
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
    runtimeSourceText = read_repo_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c");

    TEST_ASSERT_NOT_NULL(backendSupportText);
    TEST_ASSERT_NOT_NULL(cLoweringText);
    TEST_ASSERT_NOT_NULL(llvmLoweringText);
    TEST_ASSERT_NOT_NULL(llvmPreludeText);
    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);

    // TODO: 全文存在性断言可能命中注释或未使用分支；需要对生成工件的可达路径补证。
    for (index = 0; index < (TZrSize)(sizeof(expectations) / sizeof(expectations[0])); index++) {
        char opcodeToken[96];

        snprintf(opcodeToken,
                 sizeof(opcodeToken),
                 "ZR_INSTRUCTION_ENUM(%s)",
                 expectations[index].opcodeName);
        TEST_ASSERT_TRUE_MESSAGE(count_substring_occurrences(backendSupportText, opcodeToken) >= 2, opcodeToken);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(cLoweringText, opcodeToken), opcodeToken);
        if (expectations[index].cDirectLoweringName != ZR_NULL) {
            TEST_ASSERT_NOT_NULL_MESSAGE(strstr(cLoweringText, expectations[index].cDirectLoweringName),
                                         expectations[index].cDirectLoweringName);
            TEST_ASSERT_NULL_MESSAGE(strstr(cLoweringText, expectations[index].runtimeHelperName),
                                     expectations[index].runtimeHelperName);
        }
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

// 完整 Unity 矩阵入口；环境开关可选 ownership/resource 子集，独立手工 opcode 可执行文件复用本文件静态用例。
int main(void) {
    UNITY_BEGIN();
    if (getenv("ZR_VM_OWNERSHIP_OPCODE_FOCUSED") != ZR_NULL) {
        RUN_TEST(test_ownership_intrinsics_reference_views_semir_and_true_aot_preserve_dedicated_opcodes);
        return UNITY_END();
    }
    if (getenv("ZR_VM_RESOURCE_UNIQUE_DROP_FOCUSED") != ZR_NULL) {
        RUN_TEST(test_resource_unique_drop_vm_and_aot_preserve_cleanup_order_contract);
        return UNITY_END();
    }
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
    RUN_TEST(test_aot_c_observation_elision_and_llvm_flags_preserve_const_div_mod_contract);
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
    RUN_TEST(test_ownership_intrinsics_reference_views_semir_and_true_aot_preserve_dedicated_opcodes);
    RUN_TEST(test_receiver_guards_preserve_optional_and_direct_weak_contracts_in_true_aot);
    RUN_TEST(test_resource_unique_drop_vm_and_aot_preserve_cleanup_order_contract);
    RUN_TEST(test_aot_backends_lower_manual_extended_numeric_opcode_fixture);
    RUN_TEST(test_aot_backends_lower_manual_state_and_scope_opcode_fixture);
    RUN_TEST(test_aot_source_sync_keeps_extended_opcode_surfaces_aligned);
    return UNITY_END();
}

/**
 * @unit EXBCFIX-001
 * @typedef SZrExecBcAotTestTimer
 *
 * ## Summary
 * 保存用例开始、结束两次 clock() 读数，供测试打印耗时。
 *
 * ## Fields
 * - startTime：用例开始时的 clock() 读数。
 * - endTime：用例结束时的 clock() 读数；与开始读数取差仅供诊断。
 *
 * ## Value model
 * 结构体值；无独立资源释放。
 *
 * ## Invariants and lifecycle
 * 两个 clock_t 字段由用例栈上局部实例持有；不传入编译器、VM 或 AOT 模块。
 *
 * 只在同一用例正常路径取差；计时不是正确性、性能阈值或线程同步的证明。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:28
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1428
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1482
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1554
 */

/**
 * @unit EXBCFIX-002
 * @typedef SZrBinaryFixtureReader
 *
 * ## Summary
 * 把调用测试拥有的二进制缓冲包装成一次性核心 IO 输入。
 *
 * ## Fields
 * - bytes：借用的测试 heap 缓冲，不由 IO close 释放。
 * - length：首轮交付的字节数，由分配/读取测试保证有效。
 * - consumed：首次交付后为真；后续 callback 返回 EOF。
 *
 * ## Value model
 * 结构体值；缓冲仍由测试 free。
 *
 * ## Invariants and lifecycle
 * bytes 是借用的原始字节，length 是可读字节数，consumed 记录是否已交付；核心 IO 可在随后读取中继续引用该缓冲。
 *
 * 缓冲必须活到 IO 最后一次使用之后；close 回调不 free。长度与实际分配由构造测试保证，不补 NUL。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:34
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:861
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:6032
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:6131
 * - zr_vm_core/src/zr_vm_core/io/io.c:52
 */

/**
 * @unit EXBCFIX-003
 * @typedef SZrAotDebugHookCapture
 *
 * ## Summary
 * 记录调试行事件总数、最后行号及前八个行号样本。
 *
 * ## Fields
 * - eventCount：收到的有效 LINE 事件总数。
 * - lastLine：最近一次 LINE 事件行号；reset 时为 NONE。
 * - lines：只保存最初八次 LINE 行号；尚无顺序断言。
 *
 * ## Value model
 * 结构体值；不拥有 heap 或回调参数。
 *
 * ## Invariants and lifecycle
 * eventCount/lastLine/lines[8] 由静态捕获槽及回调共同使用；注册回调前由 reset 清零。
 *
 * 串行测试状态，不提供并发隔离；超过八次仍更新计数与末行但不再写样本。TODO: 当前用例只断言计数和最后行，尚未以 lines[] 断言完整事件顺序。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:42
 */

/**
 * @unit EXBCFIX-004
 * @typedef SZrAotSourceSyncExpectation
 *
 * ## Summary
 * 为源码同步测试关联 opcode 名、运行时 helper 名和可选 C 直接 lowering 名。
 *
 * ## Fields
 * - opcodeName：指令表中的借用字面名字。
 * - runtimeHelperName：源码中应匹配的 runtime helper 名字。
 * - cDirectLoweringName：可选的 C 直接 lowering 锚点，允许 NULL。
 *
 * ## Value model
 * 只读期望项；不转移字符串所有权。
 *
 * ## Invariants and lifecycle
 * 三个 const char* 借用静态字符串，按测试矩阵逐项在源码文本中检索。
 *
 * cDirectLoweringName 可为空；文本出现次数仅证明约定锚点，不证明对应分支执行或语义完备。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:49
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9651
 */

/**
 * @unit EXBCFIX-005
 * @typedef SZrExecBcAotTestLoadedModule
 *
 * ## Summary
 * 镜像当前根库私有 AOT 已加载记录，供少量生成帧测试注入项目状态。
 *
 * ## Fields
 * - backendKind：本记录所属的请求后端。
 * - moduleName：真实记录拥有的模块名；当前手工 fixture 留零。
 * - sourcePath：真实记录拥有的源码路径；手工 fixture 留零。
 * - zroPath：真实记录拥有的二进制路径；手工 fixture 留零。
 * - libraryPath：真实记录拥有的动态库路径；手工 fixture 留零。
 * - libraryHandle：真实动态库句柄；手工 fixture 不装载库。
 * - descriptor：借用库内 descriptor；本手工记录初始为空。
 * - codeRegistration：借用库内注册元信息；本手工记录初始为空。
 * - moduleFunction：借用的根函数，须活到记录不再使用。
 * - functionTable：记录拥有的 native 指针数组，其中函数对象只借用。
 * - functionPins：真实记录用于逐函数 GC pin 的凭据数组；手工 fixture 清零。
 * - functionCount：有效函数数：本 fixture 为根加直接子函数数。
 * - functionCapacity：functionTable 分配容量，释放按此计。
 * - generatedFrameSlotCounts：记录拥有的逐函数物理槽数表。
 * - module：真实加载模块的借用指针，当前手工 fixture 清零。
 * - modulePin：真实模块 pin 凭据，当前手工 fixture 清零。
 * - moduleExecuted：记录执行状态，手工 fixture 初始为假。
 *
 * ## Value model
 * 记录镜像；安装器分配的表由根库项目释放链接管。
 *
 * ## Invariants and lifecycle
 * backendKind、路径、句柄、descriptor、注册信息、函数表/pin/槽数及模块状态的字段次序对应当前 SZrLibraryAotLoadedModule。测试安装器只填所需字段并将表交项目释放链。
 *
 * 不能作为公开稳定 ABI；真实动态库记录中的指针与 pin 各有专门生命周期，测试零值不代表装载成功。TODO: 尚无跨模块 sizeof/offsetof 编译期校验，库内字段变化须同步镜像。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:58
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:510
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:531
 * - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c:64
 */

/**
 * @unit EXBCFIX-006
 * @typedef SZrExecBcAotTestRuntimeState
 *
 * ## Summary
 * 镜像项目 AOT 状态，供测试直接配置记录数组并检查生成帧缓存。
 *
 * ## Fields
 * - configuredExecutionMode：最近配置的项目执行模式。
 * - executedVia：实际后端报告值，配置时由根库清回 NONE。
 * - requireAotPath：保存的配置请求，不据此推断当前消费者强制策略。
 * - strictProjectAot：根库从 AOT_C/AOT_LLVM 模式导出的严格标记。
 * - lastError：状态内固定错误缓冲，借用内容可被后续操作覆盖。
 * - records：项目拥有的记录数组；本 fixture 发布一条。
 * - recordCount：当前已发布的记录数。
 * - recordCapacity：记录数组分配容量。
 * - activeRecord：同步派发过程中的借用活动记录；安装时为空。
 *
 * ## Value model
 * 借用的状态视图；不得另行 free 或跨项目释放保存。
 *
 * ## Invariants and lifecycle
 * 模式、实际后端、严格标记、错误缓冲、records/数量/容量/activeRecord 对应当前根库私有状态；cast 借用 project->aotRuntime。
 *
 * records 元素和本结构均依赖私有布局，activeRecord 只是借用。TODO: 缺少编译期布局证明；当前字段逐项一致不等于长期 ABI 保障。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:80
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:509
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:528
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3749
 * - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c:102
 */

/**
 * @unit EXBCFIX-007
 * @var g_aotDebugHookCapture
 *
 * ## Summary
 * 为两个行调试事件用例提供共享捕获槽。
 *
 * ## Value model
 * 静态存储，无调用方释放。
 *
 * ## Invariants and lifecycle
 * reset 写初始值，debug hook 更新计数、最后行和有限样本，用例在回调运行后读取。
 *
 * 静态进程状态；每个相关用例必须先 reset，不声称可并行使用或保证全部行事件顺序。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:93
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:173
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:174
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:185
 */

/**
 * @unit EXBCFIX-008
 * @def ZR_EXECBC_TEST_OPCODE_NAME_CASE(INSTRUCTION)
 *
 * ## Summary
 * 把指令声明表中的每个名字展开为诊断 switch 的 case 和字符串。
 *
 * ## Parameters
 * @param[in] INSTRUCTION 指令声明表提供的单个符号名；须能用于枚举拼接与字符串化。
 *
 * ## Expansion
 * 展开后的 case 返回静态名字字符串。
 *
 * ## Constraints and lifecycle
 * 与 ZR_INSTRUCTION_DECLARE 共同使用，枚举值来自 ZR_INSTRUCTION_ENUM，字符串来自预处理器 #INSTRUCTION。
 *
 * 仅在 opcode-name switch 中展开；不维护第二份 opcode 表，不产生指令执行逻辑。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:96
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:103
 * - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h:26
 */

/**
 * @unit EXBCFIX-009
 * @fn static const char *execbc_test_instruction_opcode_name(EZrInstructionCode opcode)
 *
 * ## Summary
 * 将指令枚举映射为可读诊断名，供可选函数树 trace 使用。
 *
 * ## Parameters
 * @param[in] opcode 待打印的指令枚举；未知值走默认分支。
 *
 * ## Returns
 * @return 借用的静态 C 字符串，调用者不得 free。
 *
 * ## Constraints and lifecycle
 * switch 由统一指令声明表生成；未知枚举返回静态 UNKNOWN_OPCODE。
 *
 * 返回值只读、进程期有效；名称查询不校验指令操作数，也不执行 opcode。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:101
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:146
 */

/**
 * @unit EXBCFIX-010
 * @fn static TZrBool execbc_test_trace_enabled(void)
 *
 * ## Summary
 * 按环境变量是否存在决定是否启用 ExecBC 测试 trace，并缓存首次结果。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 存在变量时真，否则假；不分配资源。
 *
 * ## Constraints and lifecycle
 * 首次调用 getenv("ZR_VM_TRACE_EXECBC_AOT_TEST")，之后使用两个函数内静态布尔值。
 *
 * 空字符串或值 "0" 也算存在；首次调用后再改环境不会生效。此静态缓存无并发初始化协议。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:110
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:127
 */

/**
 * @unit EXBCFIX-011
 * @fn static void execbc_test_dump_function_tree(const SZrFunction *function, TZrUInt32 depth)
 *
 * ## Summary
 * 在 trace 开启时递归输出函数、字节码、SemIR 和直接子函数诊断。
 *
 * ## Parameters
 * @param[in] function 借用的函数树根；允许 NULL。
 * @param[in] depth 仅用于打印的当前递归深度，子结点加一。
 *
 * ## Returns
 * 无返回值，不取得函数、名字或指令数组所有权。
 *
 * ## Constraints and lifecycle
 * 借用完整函数树，从 stderr 打印当前结点和指令，再对 childFunctionList 深度递归。
 *
 * NULL 或未开 trace 直接返回；有效数组、计数、无环树及生命周期由调用方保证，没有损坏树或深度防护。打印不是 AOT 执行验证。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:123
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:161
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1458
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1514
 */

/**
 * @unit EXBCFIX-012
 * @fn static TZrInt64 aot_runtime_test_dummy_entry_thunk(struct SZrState *state)
 *
 * ## Summary
 * 为手工项目记录提供固定成功值的最小 native 入口。
 *
 * ## Parameters
 * @param[in] state 入口 ABI 要求的状态参数，本桩不读取。
 *
 * ## Returns
 * @return 固定 TZrInt64 值 1。
 *
 * ## Constraints and lifecycle
 * 忽略 state 并返回整数 1，供生成帧/静态直调用协议测试绑定。
 *
 * 不装载动态库，不执行 ZR 程序，也不能证明真实生成入口 ABI 或模块初始化。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:166
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3745
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3746
 */

/**
 * @unit EXBCFIX-013
 * @fn static void aot_debug_hook_capture_reset(void)
 *
 * ## Summary
 * 在行事件测试前清空共享捕获槽并设置无行号哨兵。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；静态槽仍由本测试翻译单元持有。
 *
 * ## Constraints and lifecycle
 * memset 清零计数和样本，再把 lastLine 设为 ZR_RUNTIME_DEBUG_HOOK_LINE_NONE。
 *
 * 需在安装/触发行事件之前调用；不注销已有 hook，不提供并行隔离。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:172
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:4203
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:4293
 */

/**
 * @unit EXBCFIX-014
 * @fn static void aot_debug_hook_capture(struct SZrState *state, SZrDebugInfo *debugInfo)
 *
 * ## Summary
 * 作为 runtime debug hook 接收 LINE 事件并更新共享捕获状态。
 *
 * ## Parameters
 * @param[in] state 回调 ABI 参数，此捕获器不读取。
 * @param[in] debugInfo 本次事件信息的借用指针；NULL 或非 LINE 事件不计入。
 *
 * ## Returns
 * 无返回值，不分配或释放资源。
 *
 * ## Constraints and lifecycle
 * NULL 调试信息或非 LINE 事件被忽略；前八次写 lines[]，每次有效事件均更新 lastLine 并增加 eventCount。
 *
 * state 未使用；debugInfo 仅在调用期间借用，不保存其地址。样本容量与事件总数分开，未证明事件序列完整性。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:178
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:4201
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:4291
 */

/**
 * @unit EXBCFIX-015
 * @fn static char *read_text_file_owned(const TZrChar *path)
 *
 * ## Summary
 * 以二进制方式完整读取文件并补一个 NUL，返回调用方拥有的文本缓冲。
 *
 * ## Parameters
 * @param[in] path 借用的文件路径，函数不修改；允许 NULL 并返回失败。
 *
 * ## Returns
 * @return 成功返回 malloc 缓冲，由调用方 free；失败返回 NULL。
 *
 * ## Constraints and lifecycle
 * 打开后 seek/ftell 获取长度，分配长度加一，短读时 free 并关闭，成功时补 NUL、关闭并返回。
 *
 * NULL 路径及已检查的打开/定位/分配/短读失败返回 NULL；长度来自同一次打开，但不保证并发文件变更下的原子快照或文本编码有效。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:224
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:638
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:645
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1521
 */

/**
 * @unit EXBCFIX-016
 * @fn static void assert_generated_aot_c_text_uses_new_code_shape(const char *text)
 *
 * ## Summary
 * 断言生成 C 文本使用当前函数入口和统一退出形态。
 *
 * ## Parameters
 * @param[in] text 调用方持有的生成 C 文本，要求非 NULL。
 *
 * ## Returns
 * 无返回值；text 保持调用方所有。
 *
 * ## Constraints and lifecycle
 * 检查旧 prepared/shim/goto/return-0 形态不存在，并要求上下文解析或 BeginGeneratedFunction、退出或失败 helper 的约定标记。
 *
 * 输入须为 NUL 终止文本；Unity 断言失败可非局部退出当前用例。仅字符串形状验证，不编译或执行生成 C。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:267
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1531
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:2379
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:4574
 */

/**
 * @unit EXBCFIX-017
 * @fn static void assert_generated_aot_llvm_text_uses_true_backend_shape(const char *text)
 *
 * ## Summary
 * 断言 LLVM 文本包含独立内部函数入口且不调用活动 shim。
 *
 * ## Parameters
 * @param[in] text NUL 终止的生成 LLVM 文本，要求非 NULL。
 *
 * ## Returns
 * 无返回值，不转移文本所有权。
 *
 * ## Constraints and lifecycle
 * 检查 InvokeActiveShim 不存在，并匹配 define internal i64 @zr_aot_fn_0(ptr %state)。
 *
 * 只约束具体文本形状；不证明 LLVM 解析、链接、ABI 或入口执行。Unity 失败不保证调用方尾部清理。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:280
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1536
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1656
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1709
 */

/**
 * @unit EXBCFIX-018
 * @fn static void assert_generated_aot_llvm_text_uses_aligned_stack_value_layout(const char *text)
 *
 * ## Summary
 * 检查 LLVM 输出采用字节布局寻址并拒绝旧的未对齐栈值文本形态。
 *
 * ## Parameters
 * @param[in] text NUL 终止的生成 LLVM 文本，要求非 NULL。
 *
 * ## Returns
 * 无返回值，所有权不变。
 *
 * ## Constraints and lifecycle
 * 要求两种 value 类型的数组形态及 getelementptr i8，排除旧结构字面量和按值结构 GEP。
 *
 * 这些标记对应布局策略的文本契约；并未进行 sizeof/offsetof、LLVM 编译或运行时地址验证。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:287
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:2208
 */

/**
 * @unit EXBCFIX-019
 * @fn static void assert_generated_aot_llvm_text_lowers_simple_return_path(const char *text)
 *
 * ## Summary
 * 检查最短 LLVM 返回路径引用生成函数、指令、常量复制和返回 helper。
 *
 * ## Parameters
 * @param[in] text 调用方持有的 LLVM 文本，要求非 NULL 且 NUL 终止。
 *
 * ## Returns
 * 无返回值；Unity 断言失败终止当前用例路径。
 *
 * ## Constraints and lifecycle
 * 逐项 strstr 检查 BeginGeneratedFunction、BeginInstruction、CopyConstant、Return 四个符号。
 *
 * 符号出现不等于其顺序、控制流可达或被执行；该 helper 本身不运行 LLVM。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:299
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1657
 */

/**
 * @unit EXBCFIX-020
 * @fn static void assert_generated_aot_llvm_text_lowers_static_direct_call_path(const char *text)
 *
 * ## Summary
 * 检查 LLVM 静态直调用文本具备准备、调用子函数和完成协议标记。
 *
 * ## Parameters
 * @param[in] text NUL 终止的 LLVM 文本，要求非 NULL。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 要求 PrepareStaticDirectCall、call i64 @zr_aot_fn_1(ptr %state) 和 FinishDirectCall。
 *
 * 只检查约定片段存在，不证明运行帧、返回值或清理顺序；文本保持借用。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:308
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1710
 */

/**
 * @unit EXBCFIX-021
 * @fn static TZrBool aot_llvm_text_contains_unsupported_opcode(const char *text, TZrUInt32 opcode)
 *
 * ## Summary
 * 检测 LLVM 文本中指定 opcode 的 unsupported 报告调用。
 *
 * ## Parameters
 * @param[in] text 只读 NUL 终止文本；可为 NULL。
 * @param[in] opcode 要匹配的无符号 opcode 数值，用 %u 写入固定后缀。
 *
 * ## Returns
 * @return 检测到同一行匹配时真，否则假。
 *
 * ## Constraints and lifecycle
 * 扫描精确 ReportUnsupportedInstruction 前缀，在同一换行前寻找格式化的 i32 opcode 后缀，命中即返回真。
 *
 * NULL 返回假；这是生成文本模式查询，既不解码完整 IR，也不能把假值当作后端执行成功。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:316
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:619
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:2281
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:2578
 */

/**
 * @unit EXBCFIX-022
 * @fn static TZrBool aot_c_text_contains_unsupported_opcode(const char *text, TZrUInt32 opcode)
 *
 * ## Summary
 * 检测 C 文本中指定 opcode 的 unsupported 返回包装。
 *
 * ## Parameters
 * @param[in] text 只读 NUL 终止的 C 文本；可为 NULL。
 * @param[in] opcode 要查询的无符号 opcode 数值。
 *
 * ## Returns
 * @return 同一行前缀与数值后缀都匹配时真，否则假。
 *
 * ## Constraints and lifecycle
 * 扫描精确 ZR_AOT_C_RETURN(ReportUnsupportedInstruction...) 前缀，并限制数值后缀在当前行内。
 *
 * NULL 返回假；仅文本匹配，不保证该行可达或其余 opcode 全部受支持。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:339
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:618
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9096
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9098
 */

/**
 * @unit EXBCFIX-023
 * @fn static void assert_generated_aot_c_elides_instruction_observation(const char *text)
 *
 * ## Summary
 * 断言无观察需求的 C 文本没有逐指令观察标记或调用。
 *
 * ## Parameters
 * @param[in] text 调用方拥有的 C 文本，须非 NULL、NUL 终止。
 *
 * ## Returns
 * 无返回值；不释放输入。
 *
 * ## Constraints and lifecycle
 * 排除 zr_aot_begin_instruction 注释标记和 BeginInstruction 名字。
 *
 * 仅验证该生成文本的观察代码消去形态，不代表调试事件、性能或异常路径已运行。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:362
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9491
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9525
 */

/**
 * @unit EXBCFIX-024
 * @fn static void assert_generated_aot_llvm_begin_instruction_step_flags(const char *text, TZrUInt32 instructionIndex, TZrUInt32 stepFlags)
 *
 * ## Summary
 * 断言 LLVM 文本中存在指定指令下标和 stepFlags 的观察 helper 调用。
 *
 * ## Parameters
 * @param[in] text NUL 终止的 LLVM 文本，要求非 NULL。
 * @param[in] instructionIndex 预期写入调用串的指令下标。
 * @param[in] stepFlags 预期写入调用串的观察位集合。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 把 instructionIndex 和 stepFlags 格式化进固定调用串，在借用文本中检索。
 *
 * 不执行观察回调，不验证其出现次数或控制流位置；两个数值由 fixture 负责匹配生成上下文。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:369
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9492
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9493
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9526
 */

/**
 * @unit EXBCFIX-025
 * @fn static TZrInstruction make_instruction_no_operands(EZrInstructionCode opcode, TZrUInt16 operandExtra)
 *
 * ## Summary
 * 构造其余位清零的手工指令，仅填写 opcode 和 operandExtra。
 *
 * ## Parameters
 * @param[in] opcode 要写入 operationCode 的枚举。
 * @param[in] operandExtra 直接写入额外位字段的值。
 *
 * ## Returns
 * @return 初始化后的指令值，不拥有堆资源。
 *
 * ## Constraints and lifecycle
 * 按值返回完整 TZrInstruction，供另外两个构造器与手工 opcode fixture 复用。
 *
 * opcode 转为 TZrUInt16；不校验枚举、位宽截断或 opcode 对额外位的语义要求。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:385
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:398
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:409
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9582
 */

/**
 * @unit EXBCFIX-026
 * @fn static TZrInstruction make_instruction_constant_operand(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrInt32 operandA2)
 *
 * ## Summary
 * 在基础零初始化指令上写入一个 32 位 operand2 槽。
 *
 * ## Parameters
 * @param[in] opcode 目标 opcode。
 * @param[in] operandExtra 原样传入基础构造器的额外位。
 * @param[in] operandA2 写入首个 32 位操作数的索引或数值。
 *
 * ## Returns
 * @return 指令值；无资源转移。
 *
 * ## Constraints and lifecycle
 * 复用 no_operands，再设置 operand.operand2[0]；常用于常量索引 fixture。
 *
 * 不验证常量表长度或 opcode 的操作数格式，索引有效性由调用方保证。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:395
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3196
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3197
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3769
 */

/**
 * @unit EXBCFIX-027
 * @fn static TZrInstruction make_instruction_slot_operands(EZrInstructionCode opcode, TZrUInt16 operandExtra, TZrUInt16 operandA1, TZrUInt16 operandB1)
 *
 * ## Summary
 * 在基础零初始化指令上写入两个 16 位 operand1 槽。
 *
 * ## Parameters
 * @param[in] opcode 目标 opcode。
 * @param[in] operandExtra 原样写入的额外位。
 * @param[in] operandA1 第一个 16 位操作数。
 * @param[in] operandB1 第二个 16 位操作数。
 *
 * ## Returns
 * @return 指令值；不分配资源。
 *
 * ## Constraints and lifecycle
 * 依次设置 operand1[0] 和 operand1[1]，用于 fixture 的目标/源栈槽等编码。
 *
 * 不校验栈槽范围、读写方向或 opcode 格式；手工函数布局必须与编码一致。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:405
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:418
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3198
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9337
 */

/**
 * @unit EXBCFIX-028
 * @fn static TZrInstruction make_instruction_return(TZrUInt16 sourceSlot)
 *
 * ## Summary
 * 构造携带一个返回值源槽的 FUNCTION_RETURN 指令。
 *
 * ## Parameters
 * @param[in] sourceSlot 返回值所在的源槽索引。
 *
 * ## Returns
 * @return 按值返回单条返回指令。
 *
 * ## Constraints and lifecycle
 * 调用槽构造器，固定 operandExtra=1、第二操作数=0。
 *
 * sourceSlot 必须是该手工函数的有效返回源槽；本函数不追加数组或检查容量。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:417
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3199
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3657
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3770
 */

/**
 * @unit EXBCFIX-029
 * @fn static SZrFunction *compile_source_without_quickening(SZrState *state, const char *source, const char *sourcePath)
 *
 * ## Summary
 * 走内部解析、编译、优化和装配链取得未走公开 quickening 路径的函数。
 *
 * ## Parameters
 * @param[in,out] state 有效 VM 状态；允许 NULL 并返回失败。
 * @param[in] source 借用的 NUL 终止源码，长度用 strlen 计算。
 * @param[in] sourcePath 借用的源名字/路径文本，转为 VM 字符串；要求非 NULL。
 *
 * ## Returns
 * @return 成功返回生成函数；已检查的参数、解析、验证、分配、编译或装配失败返回 NULL。
 *
 * ## Constraints and lifecycle
 * 建立源名字和 AST，初始化 compiler state，验证 task effects、compile_script、optimize_instructions，再 assemble；普通失败分支显式释放 AST/compiler state，部分分支释放函数。
 *
 * 名称不意味着完全未优化：这里仍调用 optimize_instructions。只描述显式普通返回清理，不保证 Throw 或 Unity 非局部退出清理。成功函数由调用测试管理。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:422
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:8193
 */

/**
 * @unit EXBCFIX-030
 * @fn static void *aot_runtime_test_project_alloc(SZrState *state, TZrSize size)
 *
 * ## Summary
 * 通过当前 global 的 allocator 分配 PROJECT 类别的 native 内存。
 *
 * ## Parameters
 * @param[in] state 提供 global/allocator/用户分配参数的借用状态。
 * @param[in] size 请求的字节数，原样传递给 allocator。
 *
 * ## Returns
 * @return allocator 返回的原始指针或 NULL。
 *
 * ## Constraints and lifecycle
 * 向 allocator 传入旧指针 NULL、旧大小 0、请求 size 和 ZR_MEMORY_NATIVE_TYPE_PROJECT。
 *
 * state/global/allocator 任一空则返回 NULL；返回内存未初始化，调用方须按同一分配器类别与项目生命周期归还。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:492
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:531
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:541
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:544
 */

/**
 * @unit EXBCFIX-031
 * @fn static SZrExecBcAotTestRuntimeState *aot_runtime_test_install_project_record(SZrState *state, SZrLibrary_Project *project, SZrFunction *function, EZrAotBackendKind backendKind)
 *
 * ## Summary
 * 为四个生成帧/索引测试配置临时项目并安装一个手工 AOT 记录。
 *
 * ## Parameters
 * @param[in,out] state 具有有效 global 的测试状态。
 * @param[out] project 调用方持有的临时项目对象，将被 memset 并供 global 借用。
 * @param[in] function 在项目记录使用期间存活的根函数，子函数数组及槽数须有效。
 * @param[in] backendKind 决定请求 AOT_C 或 AOT_LLVM 模式并写入记录的后端值。
 *
 * ## Returns
 * @return 借用 project->aotRuntime 的镜像指针，项目释放后失效。
 *
 * ## Constraints and lifecycle
 * 清空调用方 project，挂到 global->userData，ConfigureGlobal 后分配 records、根和直接子函数表、对应 frame-slot 表，填完后发布单条记录。
 *
 * project 必须是可覆盖的临时对象；函数树借用，三张 native 表交根库释放。Unity 分配断言可非局部退出，未提供通用 OOM 回滚。只列根与直接子函数，不声称递归装载或真实 DLL。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:505
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3666
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3785
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3905
 * - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c:3375
 * - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c:3398
 */

/**
 * @unit EXBCFIX-032
 * @brief aot_runtime_test_install_project_record::records publication（所属函数关键块）
 *
 * ## Summary
 * 把填充完成的测试记录数组发布到私有运行时状态。
 *
 * ## Control flow
 * 所属安装函数随后返回借用状态；此块不独立返回。
 *
 * ## Constraints and lifecycle
 * 设置 records、recordCount=1、recordCapacity=1、activeRecord=NULL，使根库生成函数消费者可查该记录。
 *
 * 发布后表由项目释放链负责；此前分配断言失败并无完整回滚。TODO: 该 cast 和 records 元素依赖私有布局，需同步类型并增加编译期偏移校验。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:563
 * - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c:102
 * - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c:3398
 */

/**
 * @unit EXBCFIX-033
 * @fn static void aot_runtime_test_remove_project_record(SZrState *state, SZrLibrary_Project *project)
 *
 * ## Summary
 * 经根库项目释放入口清理临时 AOT 状态，再断开 global 对项目的借用。
 *
 * ## Parameters
 * @param[in,out] state 有效 state/global；为空时无操作。
 * @param[in,out] project 调用方的临时项目；为空时无操作。
 *
 * ## Returns
 * 无返回值；项目状态与表由被调用释放链管理。
 *
 * ## Constraints and lifecycle
 * 参数有效且 aotRuntime 非空时调用 FreeProjectState，随后把 global->userData 清空。
 *
 * 不释放调用方栈上的 project 或借用函数；真实根库释放包含撤 pin 和延迟动态库处理，不能把返回解释为全部 GC 后关闭完成。无效参数直接返回。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:572
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3719
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3841
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3951
 * - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c:3398
 */

/**
 * @unit EXBCFIX-034
 * @fn static void init_manual_test_function(SZrFunction *function, TZrInstruction *instructions, TZrUInt32 instructionCount, SZrTypeValue *constants, TZrUInt32 constantCount, SZrFunction *childFunctions, TZrUInt32 childFunctionCount, TZrUInt32 stackSize)
 *
 * ## Summary
 * 用调用方数组组装最小手工 SZrFunction，供 VM 与两个生成后端共用。
 *
 * ## Parameters
 * @param[out] function 待覆盖初始化的函数对象，须非 NULL 且无待释放旧资源。
 * @param[in] instructions 借用的指令数组。
 * @param[in] instructionCount 有效指令条数。
 * @param[in] constants 借用的常量值数组。
 * @param[in] constantCount 常量条数。
 * @param[in] childFunctions 借用的直接子函数数组。
 * @param[in] childFunctionCount 直接子函数数。
 * @param[in] stackSize 手工函数声明的栈槽大小。
 *
 * ## Returns
 * 无返回值；所有数组所有权仍在构造测试。
 *
 * ## Constraints and lifecycle
 * 清零函数对象后借用 instructions/constants/childFunctions，设置各长度、stackSize 及源行范围 1..1。
 *
 * 只断言 function 非空；数组、计数和指令语义由 fixture 保证。清零会覆盖旧内容，不能用于仍拥有资源的活动函数；栈数组不能据此交给常规拥有式释放。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:584
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3200
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3658
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3771
 */

/**
 * @unit EXBCFIX-035
 * @fn static void assert_generated_aot_texts_do_not_report_unsupported_opcodes(const char *cText, const char *llvmText, const EZrInstructionCode *opcodes, TZrSize opcodeCount)
 *
 * ## Summary
 * 逐项断言 C 和 LLVM 文本未报告指定 opcode 为 unsupported。
 *
 * ## Parameters
 * @param[in] cText NUL 终止的 C 文本。
 * @param[in] llvmText NUL 终止的 LLVM 文本。
 * @param[in] opcodes 要逐项检查的 opcode 数组。
 * @param[in] opcodeCount 数组有效元素数。
 *
 * ## Returns
 * 无返回值；所有输入只借用。
 *
 * ## Constraints and lifecycle
 * 遍历 opcodes[0..opcodeCount)，调用两个同一行文本检测器并要求假。
 *
 * 三个指针须非 NULL，列表长度由调用方保证；不覆盖列表外 opcode，也不证明生成工件被编译或执行。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:607
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9419
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9631
 */

/**
 * @unit EXBCFIX-036
 * @fn static char *read_reference_file(const char *relativePath, size_t *outSize)
 *
 * ## Summary
 * 通过 reference fixture 支持层读取文本，供参考语言用例编译。
 *
 * ## Parameters
 * @param[in] relativePath 相对 reference fixture 根的路径，借用。
 * @param[out] outSize 可选长度接收指针，原样转交支持层。
 *
 * ## Returns
 * @return 成功为调用方需 free 的文本，失败 NULL；outSize 由支持层处理。
 *
 * ## Constraints and lifecycle
 * 直接转发 relativePath/outSize；支持层把路径解析到 tests/fixtures/reference 并调用文本读取器。
 *
 * 路径按 reference fixture 根解释，非当前工作目录；失败及长度规则沿被调用接口，不扩大为任意仓库文件读取。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:624
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:6888
 * - tests/harness/reference_support.c:51
 */

/**
 * @unit EXBCFIX-037
 * @fn static char *read_repo_file_owned(const char *repoRelativePath)
 *
 * ## Summary
 * 尝试按仓库根及 zr_vm_aot 回退根读取源码文本。
 *
 * ## Parameters
 * @param[in] repoRelativePath 借用的仓库相对路径，不取得所有权。
 *
 * ## Returns
 * @return 返回 malloc 文本或 NULL；成功由调用方 free。
 *
 * ## Constraints and lifecycle
 * 用 1024 字节路径缓冲拼接 tests/../relativePath；首次读取失败才尝试 tests/../zr_vm_aot/relativePath。
 *
 * 相对根来自编译期 ZR_VM_TESTS_SOURCE_DIR，不随 cwd；NULL 返回 NULL。snprintf 截断结果未单独诊断，现有固定路径须适合缓冲；不保证并发快照。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:629
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:682
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:700
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:2365
 * - tests/harness/path_support.c:18
 */

/**
 * @unit EXBCFIX-038
 * @fn static TZrUInt32 count_substring_occurrences(const char *text, const char *needle)
 *
 * ## Summary
 * 统计 NUL 终止文本中互不重叠的字面子串出现次数。
 *
 * ## Parameters
 * @param[in] text 只读 NUL 终止文本，可为 NULL。
 * @param[in] needle 只读匹配串；NULL 或空串得到零。
 *
 * ## Returns
 * @return TZrUInt32 次数。
 *
 * ## Constraints and lifecycle
 * 每次 strstr 命中后前移 needleLength；源码同步断言用它检查锚点数量。
 *
 * NULL 或空 needle 返回 0；不解析 C token，注释和字符串中的字面内容也可能计入，不等价于语义调用次数。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:651
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9062
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9066
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9070
 */

/**
 * @unit EXBCFIX-039
 * @fn static char *join_repo_files_owned(const char *const *repoRelativePaths, TZrSize pathCount)
 *
 * ## Summary
 * 两遍读取多个仓库源码并用换行拼接为调用方拥有的文本。
 *
 * ## Parameters
 * @param[in] repoRelativePaths 借用的路径指针数组；不可为 NULL。
 * @param[in] pathCount 要读取的数组元素数；零返回失败。
 *
 * ## Returns
 * @return 成功返回需 free 的拼接文本，参数、读取或分配失败返回 NULL。
 *
 * ## Constraints and lifecycle
 * 第一遍累加每份 strlen+换行并释放临时文本，分配总缓冲；第二遍重新读取、memcpy、写换行并释放，最后补 NUL。
 *
 * 输入数组和路径在调用期有效；普通读失败释放已有 joined 缓冲。BUG: 两遍之间文件增长时，第二遍没有容量复核，可越过第一遍分配的 joinedText；未做运行复现。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:671
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9724
 */

/**
 * @unit EXBCFIX-040
 * @brief join_repo_files_owned::second read and copy（所属函数关键块）
 *
 * ## Summary
 * 第二遍重读并把每份当前文件内容复制到首次统计的拼接缓冲。
 *
 * ## Control flow
 * 所属函数在该块后返回 joinedText；此块读失败走所属函数 NULL 返回。
 *
 * ## Constraints and lifecycle
 * 对每个路径重新取得文本，按当次 strlen 复制并写换行，释放当次临时缓冲；读失败释放 joinedText 后返回。
 *
 * BUG: writeCursor 不带剩余容量校验；合法文件在两遍间增长即可超过首次估计。与所属函数是同一既有问题的块镜像，不新增缺陷家族。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:698
 */

/**
 * @unit EXBCFIX-041
 * @fn static TZrByte *read_binary_file_owned(const TZrChar *path, TZrSize *outLength)
 *
 * ## Summary
 * 以二进制方式完整读取文件并返回原始字节及长度。
 *
 * ## Parameters
 * @param[in] path 借用的文件路径，NULL 返回失败。
 * @param[out] outLength 可选输出；入口先置零，成功写原始字节数。
 *
 * ## Returns
 * @return 成功返回调用方需 free 的字节缓冲，失败 NULL。
 *
 * ## Constraints and lifecycle
 * 先把可选 outLength 置零，rb 打开/seek/ftell/分配/完整读，成功关闭后写入长度。
 *
 * 不补 NUL，不能作为 C 字符串；空文件 malloc(0) 的结果依实现，可能返回 NULL。已检查失败关闭文件并释放缓冲；无原子快照承诺。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:720
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:779
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:801
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:6070
 */

/**
 * @unit EXBCFIX-042
 * @fn static TZrByte *read_repo_binary_file_owned(const char *repoRelativePath, TZrSize *outLength)
 *
 * ## Summary
 * 从编译期 tests 根的父目录解析仓库相对二进制路径并读取。
 *
 * ## Parameters
 * @param[in] repoRelativePath 借用的仓库相对二进制路径。
 * @param[out] outLength 可选长度输出，沿底层读取器规则。
 *
 * ## Returns
 * @return 成功返回需 free 的字节缓冲，失败 NULL。
 *
 * ## Constraints and lifecycle
 * 用 1024 字节缓冲拼接 tests/../relativePath，随后调用二进制读取器；不同于文本 helper，这里没有 zr_vm_aot 回退。
 *
 * outLength 可选且入口清零；NULL 路径失败。路径必须适合固定缓冲，原始字节无 NUL 保证。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:768
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:9235
 * - tests/harness/path_support.c:18
 */

/**
 * @unit EXBCFIX-043
 * @fn static TZrBool write_embedded_aot_c_file(SZrState *state, SZrFunction *function, const TZrChar *moduleName, const TZrChar *filename, const TZrChar *binaryPath)
 *
 * ## Summary
 * 先生成并读取二进制模块，再以借用 blob 生成严格嵌入式 C 文本。
 *
 * ## Parameters
 * @param[in,out] state 有效测试 VM 状态。
 * @param[in] function 调用期间存活的借用函数。
 * @param[in] moduleName 借用的模块名，同时用作 inputHash 标识。
 * @param[in] filename C 文本输出路径，借用。
 * @param[in] binaryPath 临时二进制写入/读取/尝试删除路径，须由任务登记。
 *
 * ## Returns
 * @return 返回 C writer 的布尔结果或前置失败假；blob 不逃逸到返回调用方。
 *
 * ## Constraints and lifecycle
 * WriteBinaryFile 成功后读取 binaryPath 并尝试 remove；设置 SOURCE、moduleName/inputHash、blob/长度及 requireExecutableLowering，再同步调用 C writer 并 free blob。
 *
 * 所有输入须非 NULL；二进制写失败、读取失败或零长返回假。remove 返回未检查，写失败可能留文件；只保证普通返回后的本地 blob 释放，不承诺异常清理或外部 C 编译/DLL 装载。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:783
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1586
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1916
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:4357
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:29
 */

/**
 * @unit EXBCFIX-044
 * @fn static TZrBool write_standalone_strict_aot_c_file(SZrState *state, SZrFunction *function, const TZrChar *moduleName, const TZrChar *filename)
 *
 * ## Summary
 * 不提供嵌入 blob，以要求可执行 lowering 的选项生成独立 C 文本。
 *
 * ## Parameters
 * @param[in,out] state 有效 VM 状态。
 * @param[in] function 调用期间借用的待生成函数。
 * @param[in] moduleName 借用模块名并用作输入标识。
 * @param[in] filename C 文本输出路径，借用且须登记。
 *
 * ## Returns
 * @return C writer 布尔结果或参数失败假。
 *
 * ## Constraints and lifecycle
 * 零初始化 options，设置 moduleName、SOURCE、inputHash=moduleName 和 requireExecutableLowering=TRUE 后调用 C writer。
 *
 * 四个参数任一 NULL 返回假；不运行外部 C 编译器、链接器或 DLL，返回真也不是持久化/执行证明。文件及函数由调用方管理。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:822
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:1995
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:2063
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:2133
 */

/**
 * @unit EXBCFIX-045
 * @fn static TZrBool write_standalone_strict_aot_llvm_file(SZrState *state, SZrFunction *function, const TZrChar *moduleName, const TZrChar *filename)
 *
 * ## Summary
 * 不提供嵌入 blob，以严格 lowering 选项生成独立 LLVM 文本。
 *
 * ## Parameters
 * @param[in,out] state 有效 VM 状态。
 * @param[in] function 调用期间存活的借用函数。
 * @param[in] moduleName 借用模块名兼输入标识。
 * @param[in] filename LLVM 文本输出路径，须由任务登记。
 *
 * ## Returns
 * @return LLVM writer 的布尔结果或参数失败假。
 *
 * ## Constraints and lifecycle
 * 零初始化 options，填写 SOURCE、moduleName/inputHash 及 requireExecutableLowering，然后同步调用 LLVM writer。
 *
 * 参数 NULL 返回假；只生成文件，不执行 LLVM 编译、链接或入口。普通布尔结果不覆盖 Throw 或调用方 Unity 清理。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:841
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:2200
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:2271
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:3123
 */

/**
 * @unit EXBCFIX-046
 * @fn static TZrBytePtr binary_fixture_reader_read(struct SZrState *state, TZrPtr customData, ZR_OUT TZrSize *size)
 *
 * ## Summary
 * 向核心 IO 一次性交付测试拥有的二进制缓冲。
 *
 * ## Parameters
 * @param[in] state 回调 ABI 状态参数，本实现不读取。
 * @param[in,out] customData 借用的 SZrBinaryFixtureReader，需活到 IO 关闭。
 * @param[out] size 核心 IO 提供的长度输出指针；正常交付时写入 length。
 *
 * ## Returns
 * @return 首次有效调用返回借用 bytes，以后或无效输入返回 NULL。
 *
 * ## Constraints and lifecycle
 * customData 转成 reader；有效未消费且 bytes 非空时先置 consumed，写 length 并返回 bytes。
 *
 * 无效 reader/size、已消费或空 bytes 返回 NULL，早退不写 size；当前 io_refill 在回调前置 readSize=0 并按 NULL/零判断 EOF。借用缓冲须覆盖后续 IO 使用。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:860
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:6080
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:6156
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:6221
 * - zr_vm_core/src/zr_vm_core/io/io.c:52
 */

/**
 * @unit EXBCFIX-047
 * @fn static void binary_fixture_reader_close(struct SZrState *state, TZrPtr customData)
 *
 * ## Summary
 * 作为核心 IO close 回调保留外部二进制缓冲所有权。
 *
 * ## Parameters
 * @param[in] state 回调 ABI 参数，未使用。
 * @param[in] customData 调用方 reader 指针，未使用且不释放。
 *
 * ## Returns
 * 无返回值，不释放资源。
 *
 * ## Constraints and lifecycle
 * state/customData 均不读取，函数无操作；调用测试在 IO 不再使用 bytes 后负责 free。
 *
 * 不能把 close 当作缓冲释放或 reader 销毁；不保存指针，也不承担重复关闭的额外状态转换。
 *
 * ## References
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:875
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:6080
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:6156
 * - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c:6221
 * - zr_vm_core/src/zr_vm_core/io/io.c:52
 */
