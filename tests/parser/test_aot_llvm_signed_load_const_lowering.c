/* C-U1：钉住 LLVM AOT 对 11 个融合“物化 + 有符号常量运算”指令的降级、故障 PC 步进标志与
 * DivSignedConst 在 INT64_MIN / -1 上与解释器一致的报错（决定记录见 docs/core-runtime/aot-fused-signed-load-const.md）。 */
#include "unity.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

#if defined(ZR_PLATFORM_UNIX)
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "harness/aot_c_link_support.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_library/project.h"
#endif

#ifndef ZR_VM_TESTS_LLVM_COMPILER
#define ZR_VM_TESTS_LLVM_COMPILER "clang"
#endif

#ifndef ZR_VM_TESTS_BUILD_LIB_DIR
#define ZR_VM_TESTS_BUILD_LIB_DIR "lib"
#endif

#define ZR_C_U1_SUITE "aot_llvm_signed_load_const_lowering"

/* 与 backend_aot_internal.h 的 ZR_PARSER_API 声明一致；测试直接检查共享步进分类器。 */
extern TZrUInt32 backend_aot_c_step_flags_for_instruction(const SZrFunction *function,
                                                          const TZrInstruction *instruction);

#define ZR_C_U1_STEP_FLAG_NONE 0u
#define ZR_C_U1_STEP_FLAG_MAY_THROW 1u

void setUp(void) {}

void tearDown(void) {}

typedef enum EZrCU1Shape {
    ZR_C_U1_SHAPE_LOAD_CONST = 0,
    ZR_C_U1_SHAPE_LOAD_STACK_CONST = 1,
    ZR_C_U1_SHAPE_LOAD_STACK_LOAD_CONST = 2,
    ZR_C_U1_SHAPE_PLAIN_CONST = 3
} EZrCU1Shape;

typedef struct SZrCU1Row {
    EZrInstructionCode opcode;
    EZrCU1Shape shape;
    TZrUInt16 destination;
    TZrUInt8 a;
    TZrUInt8 b;
    TZrUInt8 c;
    TZrUInt16 constantIndex;
    const char *helper;
    TZrBool keepLeftTag;
    TZrUInt32 expectedStepFlag;
} SZrCU1Row;

/* 常量表：k0=3, k1=-1, k2=7, k3=0, k4=2, k5=5（176 需要 operand0[2] 与 k 的低字节相同，故 C=5,k=5）。 */
#define ZR_C_U1_CONSTANT_COUNT 6u
static const TZrInt64 kConstants[ZR_C_U1_CONSTANT_COUNT] = {3, -1, 7, 0, 2, 5};

static const SZrCU1Row kRows[] = {
        {ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST), ZR_C_U1_SHAPE_LOAD_CONST, 10u, 1u, 2u, 0u, 0u,
         "ZrLibrary_AotRuntime_AddSignedConst", ZR_TRUE, ZR_C_U1_STEP_FLAG_MAY_THROW},
        {ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST), ZR_C_U1_SHAPE_LOAD_STACK_CONST, 11u, 1u, 3u, 0u, 0u,
         "ZrLibrary_AotRuntime_AddSignedConst", ZR_TRUE, ZR_C_U1_STEP_FLAG_MAY_THROW},
        {ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST), ZR_C_U1_SHAPE_LOAD_STACK_LOAD_CONST, 12u, 1u, 4u, 5u,
         5u, "ZrLibrary_AotRuntime_AddSignedConst", ZR_TRUE, ZR_C_U1_STEP_FLAG_MAY_THROW},
        {ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST), ZR_C_U1_SHAPE_LOAD_CONST, 13u, 1u, 2u, 0u, 0u,
         "ZrLibrary_AotRuntime_SubSignedConst", ZR_TRUE, ZR_C_U1_STEP_FLAG_MAY_THROW},
        {ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST), ZR_C_U1_SHAPE_LOAD_STACK_CONST, 14u, 1u, 3u, 0u, 0u,
         "ZrLibrary_AotRuntime_SubSignedConst", ZR_TRUE, ZR_C_U1_STEP_FLAG_MAY_THROW},
        {ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST), ZR_C_U1_SHAPE_LOAD_CONST, 15u, 1u, 2u, 0u, 0u,
         "ZrLibrary_AotRuntime_MulSignedConst", ZR_FALSE, ZR_C_U1_STEP_FLAG_MAY_THROW},
        {ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST), ZR_C_U1_SHAPE_LOAD_STACK_CONST, 16u, 1u, 3u, 0u, 0u,
         "ZrLibrary_AotRuntime_MulSignedConst", ZR_FALSE, ZR_C_U1_STEP_FLAG_MAY_THROW},
        /* 除数 7：既非 0 也非 -1，保持无需发布 PC 的快路径。 */
        {ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST), ZR_C_U1_SHAPE_LOAD_CONST, 17u, 1u, 2u, 0u, 2u,
         "ZrLibrary_AotRuntime_DivSignedConst", ZR_FALSE, ZR_C_U1_STEP_FLAG_NONE},
        /* 除数 -1：INT64_MIN / -1 会报 signed integer division overflow。 */
        {ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST), ZR_C_U1_SHAPE_LOAD_STACK_CONST, 18u, 1u, 3u, 0u, 1u,
         "ZrLibrary_AotRuntime_DivSignedConst", ZR_FALSE, ZR_C_U1_STEP_FLAG_MAY_THROW},
        {ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST), ZR_C_U1_SHAPE_LOAD_CONST, 19u, 1u, 2u, 0u, 2u,
         "ZrLibrary_AotRuntime_ModSignedConst", ZR_FALSE, ZR_C_U1_STEP_FLAG_NONE},
        /* 除数 0：modulo by zero。 */
        {ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST), ZR_C_U1_SHAPE_LOAD_STACK_CONST, 20u, 1u, 3u, 0u, 3u,
         "ZrLibrary_AotRuntime_ModSignedConst", ZR_FALSE, ZR_C_U1_STEP_FLAG_MAY_THROW},
        /* 非融合 DIV_SIGNED_CONST：R1 之后 -1 除数也可能报错，0/-1 之外仍为 NONE。 */
        {ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST), ZR_C_U1_SHAPE_PLAIN_CONST, 21u, 1u, 0u, 0u, 1u,
         "ZrLibrary_AotRuntime_DivSignedConst", ZR_FALSE, ZR_C_U1_STEP_FLAG_MAY_THROW},
        {ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST), ZR_C_U1_SHAPE_PLAIN_CONST, 22u, 1u, 0u, 0u, 1u,
         ZR_NULL, ZR_FALSE, ZR_C_U1_STEP_FLAG_MAY_THROW},
        {ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST), ZR_C_U1_SHAPE_PLAIN_CONST, 23u, 1u, 0u, 0u, 2u,
         "ZrLibrary_AotRuntime_DivSignedConst", ZR_FALSE, ZR_C_U1_STEP_FLAG_NONE},
};

#define ZR_C_U1_ROW_COUNT ((TZrUInt32)(sizeof(kRows) / sizeof(kRows[0])))

static TZrInstruction make_row_instruction(const SZrCU1Row *row) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)row->opcode;
    instruction.instruction.operandExtra = row->destination;
    if (row->shape == ZR_C_U1_SHAPE_PLAIN_CONST) {
        instruction.instruction.operand.operand1[0] = row->a;
        instruction.instruction.operand.operand1[1] = row->constantIndex;
        return instruction;
    }
    instruction.instruction.operand.operand0[0] = row->a;
    instruction.instruction.operand.operand0[1] = row->b;
    /* operand1[1] 覆盖 operand0[2..3]；176 的物化槽 C 由 SLOT_FITS 保证与 k 的低字节一致。 */
    instruction.instruction.operand.operand1[1] = row->constantIndex;
    if (row->shape == ZR_C_U1_SHAPE_LOAD_STACK_LOAD_CONST) {
        TEST_ASSERT_TRUE(ZR_INSTRUCTION_LOAD_STACK_LOAD_CONST_SLOT_FITS(row->c, row->constantIndex));
        TEST_ASSERT_EQUAL_UINT8(row->c, instruction.instruction.operand.operand0[2]);
    }
    return instruction;
}

static SZrFunction *create_row_function(SZrState *state) {
    SZrFunction *function;
    TZrUInt32 index;

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global, sizeof(TZrInstruction) * (ZR_C_U1_ROW_COUNT + 1u), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    for (index = 0; index < ZR_C_U1_ROW_COUNT; index++) {
        function->instructionsList[index] = make_row_instruction(&kRows[index]);
    }
    memset(&function->instructionsList[ZR_C_U1_ROW_COUNT], 0, sizeof(TZrInstruction));
    function->instructionsList[ZR_C_U1_ROW_COUNT].instruction.operationCode =
            (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    function->instructionsList[ZR_C_U1_ROW_COUNT].instruction.operandExtra = 1u;
    function->instructionsList[ZR_C_U1_ROW_COUNT].instruction.operand.operand1[0] = 10u;
    function->instructionsLength = ZR_C_U1_ROW_COUNT + 1u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global, sizeof(SZrTypeValue) * ZR_C_U1_CONSTANT_COUNT, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    for (index = 0; index < ZR_C_U1_CONSTANT_COUNT; index++) {
        ZrCore_Value_InitAsInt(state, &function->constantValueList[index], kConstants[index]);
    }
    function->constantValueLength = ZR_C_U1_CONSTANT_COUNT;
    function->stackSize = 32u;
    function->parameterCount = 0u;
    function->lineInSourceStart = 1u;
    function->lineInSourceEnd = ZR_C_U1_ROW_COUNT + 1u;
    return function;
}

static void assert_text_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(text, needle), needle);
}

static const char *find_in_range(const char *start, const char *end, const char *needle) {
    size_t needleLength = strlen(needle);
    const char *cursor;

    if (start == ZR_NULL || end == ZR_NULL || end < start || needleLength == 0u ||
        (size_t)(end - start) < needleLength) {
        return ZR_NULL;
    }
    for (cursor = start; cursor + needleLength <= end; cursor++) {
        if (memcmp(cursor, needle, needleLength) == 0) {
            return cursor;
        }
    }
    return ZR_NULL;
}

static void begin_instruction_needle(char *buffer, size_t size, TZrUInt32 index, TZrUInt32 flag) {
    snprintf(buffer,
             size,
             "call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %%state, ptr %%frame, i32 %u, i32 %u)",
             (unsigned)index,
             (unsigned)flag);
}

/* 以本指令与下一条指令的 BeginInstruction 为界截取该指令的 IR 区段。 */
static void instruction_region(const char *text, TZrUInt32 index, const char **start, const char **end) {
    char needle[160];
    const char *cursor;

    snprintf(needle, sizeof(needle), "@ZrLibrary_AotRuntime_BeginInstruction(ptr %%state, ptr %%frame, i32 %u, i32 ",
             (unsigned)index);
    cursor = strstr(text, needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(cursor, needle);
    *start = cursor;
    snprintf(needle, sizeof(needle), "@ZrLibrary_AotRuntime_BeginInstruction(ptr %%state, ptr %%frame, i32 %u, i32 ",
             (unsigned)(index + 1u));
    cursor = strstr(*start, needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(cursor, needle);
    *end = cursor;
}

static char *write_llvm_for_function(SZrState *state, SZrFunction *function, const char *artifactStem) {
    SZrAotWriterOptions options;
    TZrChar generatedPath[ZR_TESTS_PATH_MAX];
    char *text;

    memset(&options, 0, sizeof(options));
    options.moduleName = artifactStem;
    options.sourceHash = artifactStem;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = artifactStem;
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(ZR_C_U1_SUITE,
                                                       "generated",
                                                       artifactStem,
                                                       ".ll",
                                                       generatedPath,
                                                       sizeof(generatedPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFileWithOptions(state, function, generatedPath, &options));
    text = ZrTests_ReadTextFile(generatedPath, ZR_NULL);
    TEST_ASSERT_NOT_NULL(text);
    return text;
}

/* D3：共享分类器对融合指令与 -1 除数给出 MAY_THROW，使 BeginInstruction 发布故障 PC（C 与 LLVM 共用）。 */
static void test_step_flags_publish_faulting_pc_for_fused_signed_const_ops(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(state);
    function = create_row_function(state);
    for (index = 0; index < ZR_C_U1_ROW_COUNT; index++) {
        char message[96];

        snprintf(message, sizeof(message), "row %u opcode %u", (unsigned)index, (unsigned)kRows[index].opcode);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(kRows[index].expectedStepFlag,
                                         backend_aot_c_step_flags_for_instruction(function,
                                                                                  &function->instructionsList[index]),
                                         message);
    }
    ZrTests_Runtime_State_Destroy(state);
}

/* C4：11 个融合指令不再落到 ReportUnsupportedInstruction；按解释器顺序物化后调用受检 helper。 */
static void test_llvm_lowers_fused_signed_load_const_family_in_interpreter_order(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    char *text;
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(state);
    function = create_row_function(state);
    text = write_llvm_for_function(state, function, "hand_built_rows");

    for (index = 0; index < ZR_C_U1_ROW_COUNT; index++) {
        const SZrCU1Row *row = &kRows[index];
        const char *start;
        const char *end;
        const char *materialize;
        const char *call;
        char needle[256];
        TZrUInt32 leftSlot;

        begin_instruction_needle(needle, sizeof(needle), index, row->expectedStepFlag);
        assert_text_contains(text, needle);
        instruction_region(text, index, &start, &end);
        TEST_ASSERT_NULL_MESSAGE(find_in_range(start, end, "ReportUnsupportedInstruction"), needle);
        if (row->shape == ZR_C_U1_SHAPE_PLAIN_CONST) {
            if (row->helper != ZR_NULL) {
                snprintf(needle, sizeof(needle), "call i1 @%s(ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u)",
                         row->helper, (unsigned)row->destination, (unsigned)row->a, (unsigned)row->constantIndex);
                TEST_ASSERT_NOT_NULL_MESSAGE(find_in_range(start, end, needle), needle);
            }
            continue;
        }

        snprintf(needle, sizeof(needle), "; zr_aot_llvm_signed_load_const op=%u", (unsigned)row->opcode);
        TEST_ASSERT_NOT_NULL_MESSAGE(find_in_range(start, end, needle), needle);

        if (row->shape == ZR_C_U1_SHAPE_LOAD_CONST) {
            snprintf(needle, sizeof(needle), "@ZrLibrary_AotRuntime_ResetStackNull(ptr %%state, ptr %%frame, i32 %u)",
                     (unsigned)row->b);
            leftSlot = row->a;
        } else {
            snprintf(needle, sizeof(needle), "@ZrLibrary_AotRuntime_GetStack(ptr %%state, ptr %%frame, i32 %u, i32 %u)",
                     (unsigned)row->b, (unsigned)row->a);
            leftSlot = row->b;
        }
        materialize = find_in_range(start, end, needle);
        TEST_ASSERT_NOT_NULL_MESSAGE(materialize, needle);
        if (row->shape == ZR_C_U1_SHAPE_LOAD_STACK_LOAD_CONST) {
            const char *constantStore;
            snprintf(needle, sizeof(needle), "@ZrLibrary_AotRuntime_ResetStackNull(ptr %%state, ptr %%frame, i32 %u)",
                     (unsigned)row->c);
            constantStore = find_in_range(materialize, end, needle);
            TEST_ASSERT_NOT_NULL_MESSAGE(constantStore, needle);
            materialize = constantStore;
        }

        snprintf(needle, sizeof(needle), "call i1 @%s(ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u)",
                 row->helper, (unsigned)row->destination, (unsigned)leftSlot, (unsigned)row->constantIndex);
        call = find_in_range(start, end, needle);
        TEST_ASSERT_NOT_NULL_MESSAGE(call, needle);
        /* 物化必须先于运算：LOAD_CONST 的 A==B 读到常量、D==B/C 最终保存结果。 */
        TEST_ASSERT_TRUE_MESSAGE(materialize < call, needle);

        if (row->keepLeftTag) {
            TEST_ASSERT_NOT_NULL_MESSAGE(find_in_range(call, end, "zr_aot_signed_load_const_tag_"), needle);
        } else {
            TEST_ASSERT_NULL_MESSAGE(find_in_range(start, end, "zr_aot_signed_load_const_tag_"), needle);
        }
    }

    free(text);
    ZrTests_Runtime_State_Destroy(state);
}


static const TZrChar *current_error_message(SZrState *state);

static const EZrInstructionCode kFusedOpcodes[] = {
        ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST),
        ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST),
        ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST),
        ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST),
        ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST),
        ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST),
        ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST),
        ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST),
        ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST),
        ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST),
        ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST),
};

#define ZR_C_U1_FUSED_COUNT ((TZrUInt32)(sizeof(kFusedOpcodes) / sizeof(kFusedOpcodes[0])))

typedef struct SZrCU1SourceCase {
    const char *name;
    const char *source;
} SZrCU1SourceCase;

/* 源码用例：每个都至少产生一个融合指令（由 count_opcode_recursive 断言），结果由解释器给出。 */
static const SZrCU1SourceCase kSourceCases[] = {
        {"c01", "fn addFour(value: int): int { return value + 4; }\n"
                "fn apply(cb) { return cb(3); }\n"
                "return apply(addFour);\n"},
        {"c05", "fn addFour(value: int): int { return value + 4; }\n"
                "fn apply(cb, x: int) { var r = cb(x + 1); return r; }\n"
                "return apply(addFour, 2);\n"},
        {"c07", "fn addFour(value: int): int { return value + 4; }\n"
                "fn apply(cb) { var i = 0; var last = cb(0); while (i < 3) { last = cb(i); i = i + 1; } return last; }\n"
                "return apply(addFour);\n"},
        {"f05", "fn addFour(value: int): int { return value + 4; }\n"
                "fn apply(cb, n: int) { var i = 0; var r = cb(0); while (i < n) { r = cb(i + 1); i = i + 1; } return r; }\n"
                "return apply(addFour, 3);\n"},
        {"f07", "fn addFour(value: int): int { return value + 4; }\n"
                "fn apply(cb, x: int) { var a = cb(x * 1); var b = cb(3); return b; }\n"
                "return apply(addFour, 2);\n"},
        {"stack_forms",
         "fn s(v: int): int { return v - 4; }\n"
         "fn m(v: int): int { return v * 4; }\n"
         "fn d(v: int): int { return v / 4; }\n"
         "fn r(v: int): int { return v % 4; }\n"
         "return s(50) + m(3) + d(41) + r(43);\n"},
        {"const_forms",
         "fn id(value: int): int { return value; }\n"
         "fn apply(cb, x: int) { var a = cb(x - 1); var b = cb(x / 2); var c = cb(x % 3); var e = cb(x * 5); return a + b + c + e; }\n"
         "return apply(id, 17);\n"},
        {"nested_args",
         "fn addFour(value: int): int { return value + 4; }\n"
         "fn apply(cb, x: int) { var y: int = x + 1; return cb(y + 4) + y; }\n"
         "return apply(addFour, 2);\n"},
};

#define ZR_C_U1_SOURCE_COUNT ((TZrUInt32)(sizeof(kSourceCases) / sizeof(kSourceCases[0])))

static TZrUInt32 count_opcode_recursive(const SZrFunction *function, EZrInstructionCode opcode, TZrUInt32 depth) {
    TZrUInt32 count = 0;
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(depth < 64u);
    for (index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            count++;
        }
    }
    if (function->childFunctionList != ZR_NULL) {
        for (index = 0; index < function->childFunctionLength; index++) {
            count += count_opcode_recursive(&function->childFunctionList[index], opcode, depth + 1u);
        }
    }
    return count;
}

static SZrFunction *compile_case(SZrState *state, const SZrCU1SourceCase *sourceCase) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(state, (TZrNativeString)sourceCase->name);

    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, sourceCase->source, strlen(sourceCase->source), sourceName);
}

static TZrUInt32 fused_inventory(const SZrFunction *function, char *buffer, size_t size, TZrUInt32 *mask) {
    TZrUInt32 total = 0;
    TZrUInt32 index;
    size_t used;

    snprintf(buffer, size, "fused:");
    for (index = 0; index < ZR_C_U1_FUSED_COUNT; index++) {
        TZrUInt32 count = count_opcode_recursive(function, kFusedOpcodes[index], 0u);
        if (count == 0u) {
            continue;
        }
        total += count;
        if (mask != ZR_NULL) {
            *mask |= 1u << index;
        }
        used = strlen(buffer);
        if (used + 1u < size) {
            snprintf(buffer + used, size - used, " op%u=%u", (unsigned)kFusedOpcodes[index], (unsigned)count);
        }
    }
    return total;
}

/* 源码用例的 IR 不再含这些融合指令的 unsupported 报告（不需要 clang）。 */
static void test_llvm_source_cases_emit_and_lower_fused_ops(void) {
    TZrUInt32 caseIndex;
    TZrUInt32 coverage = 0u;

    for (caseIndex = 0; caseIndex < ZR_C_U1_SOURCE_COUNT; caseIndex++) {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        char inventory[512];
        char *text;
        TZrUInt32 index;

        TEST_ASSERT_NOT_NULL(state);
        function = compile_case(state, &kSourceCases[caseIndex]);
        TEST_ASSERT_NOT_NULL_MESSAGE(function, kSourceCases[caseIndex].name);
        TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u,
                                                fused_inventory(function, inventory, sizeof(inventory), &coverage),
                                                kSourceCases[caseIndex].name);
        printf("C-U1 source %s %s\n", kSourceCases[caseIndex].name, inventory);
        text = write_llvm_for_function(state, function, kSourceCases[caseIndex].name);
        for (index = 0; index < ZR_C_U1_FUSED_COUNT; index++) {
            char needle[64];
            const char *cursor = text;

            snprintf(needle, sizeof(needle), ", i32 %u)\n", (unsigned)kFusedOpcodes[index]);
            while ((cursor = strstr(cursor, "ReportUnsupportedInstruction")) != ZR_NULL) {
                const char *lineEnd = strchr(cursor, '\n');
                TEST_ASSERT_NOT_NULL(lineEnd);
                TEST_ASSERT_NULL_MESSAGE(find_in_range(cursor, lineEnd + 1, needle), kSourceCases[caseIndex].name);
                cursor = lineEnd + 1;
            }
        }
        free(text);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }
    printf("C-U1 source coverage mask 0x%03x\n", (unsigned)coverage);
    /* 源码覆盖 10 个融合指令；ADD_SIGNED_LOAD_STACK_LOAD_CONST（下标 2）只在常量槽之后仍活跃时保留，
     * 普通源码会被反物化（见 test_compiler_w2_performance_quickening 的 dispatch_loops 断言），
     * 因此只由手写指令行覆盖。 */
    TEST_ASSERT_EQUAL_HEX32(((1u << ZR_C_U1_FUSED_COUNT) - 1u) & ~(1u << 2u), coverage);
}

#if defined(ZR_PLATFORM_UNIX)
static void write_text_file_or_fail(const TZrChar *path, const char *text) {
    FILE *file;

    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(path));
    file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static void hash_file_or_fail(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    FILE *file;
    TZrByte chunk[ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH];
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    TZrSize readSize;

    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    while ((readSize = fread(chunk, 1, sizeof(chunk), file)) > 0u) {
        TZrSize index;
        for (index = 0u; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    TEST_ASSERT_TRUE(feof(file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    snprintf(buffer, bufferSize, ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long)hash);
}

/* 解释器为准：值与标签都必须与 LLVM 共享库执行结果一致，且确实经由 LLVM 执行。 */
static void execute_case_via_llvm(const SZrCU1SourceCase *sourceCase, const char *expectedError) {
    const char *projectJson = "{\"name\":\"aot-llvm-c-u1\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue interpreted;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0u;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    TZrChar caseDir[ZR_TESTS_PATH_MAX];
    char command[8192];
    int status;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_case(state, sourceCase);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, sourceCase->name);
    ZrCore_Value_ResetAsNull(&interpreted);
    if (expectedError != ZR_NULL) {
        /* 解释器先给出同一条运行时错误，作为 LLVM 终止性失败消息的参照。 */
        TEST_ASSERT_FALSE_MESSAGE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, function, &interpreted),
                                  sourceCase->name);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(expectedError, current_error_message(state), sourceCase->name);
        state->hasCurrentException = ZR_FALSE;
        state->threadStatus = ZR_THREAD_STATUS_FINE;
    } else {
        TEST_ASSERT_TRUE_MESSAGE(ZrTests_Runtime_Function_Execute(state, function, &interpreted), sourceCase->name);
        TEST_ASSERT_TRUE_MESSAGE(ZR_VALUE_IS_TYPE_INT(interpreted.type), sourceCase->name);
    }

    snprintf(caseDir, sizeof(caseDir), "project_%s", sourceCase->name);
#define ZR_C_U1_ARTIFACT(SUBDIR, STEM, EXT, OUT)                                                                       \
    do {                                                                                                               \
        TZrChar dir__[ZR_TESTS_PATH_MAX];                                                                              \
        snprintf(dir__, sizeof(dir__), "%s%s", caseDir, SUBDIR);                                                       \
        TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(ZR_C_U1_SUITE, dir__, STEM, EXT, OUT, sizeof(OUT)));        \
    } while (0)
    ZR_C_U1_ARTIFACT("", "c_u1", ".zrp", projectPath);
    ZR_C_U1_ARTIFACT("/src", "main", ".zr", sourcePath);
    ZR_C_U1_ARTIFACT("/bin", "main", ".zro", zroPath);
    ZR_C_U1_ARTIFACT("/bin/aot_llvm/src", "main", ".ll", generatedPath);
    ZR_C_U1_ARTIFACT("/bin/aot_llvm/lib", "zrvm_aot_main", ".so", sharedLibraryPath);
#undef ZR_C_U1_ARTIFACT
    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, sourceCase->source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath, &embeddedBlob, &embeddedBlobLength));

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFileWithOptions(state, function, generatedPath, &aotOptions));
    snprintf(command,
             sizeof(command),
             "\"%s\" " ZR_VM_TESTS_LLVM_POINTER_FLAGS " -fPIC -shared \"%s\" "
             "-L\"%s\" -Wl,-rpath,\"%s\" -Wl,--no-undefined "
             ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS
             "-lzr_xx_hash -lzr_utf8proc -lm -o \"%s\"",
             ZR_VM_TESTS_LLVM_COMPILER,
             generatedPath,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             ZR_VM_TESTS_BUILD_LIB_DIR,
             sharedLibraryPath);
    status = system(command);
    if (status != 0) {
        printf("Command failed with status %d:\n%s\n", status, command);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, status, sourceCase->name);

    ZrCore_Function_Free(state, function);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM,
                                                          ZR_TRUE));
    ZrCore_Value_ResetAsNull(&result);
    if (expectedError != ZR_NULL) {
        /* R2：AOT 算术错误目前是终止性的，ExecuteEntry 返回失败并保留解释器的消息。 */
        TEST_ASSERT_FALSE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_LLVM, &result),
                                  sourceCase->name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_THREAD_STATUS_RUNTIME_ERROR, state->threadStatus, sourceCase->name);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(expectedError, current_error_message(state), sourceCase->name);
        state->global->userData = ZR_NULL;
        ZrLibrary_Project_Free(state, project);
        free(embeddedBlob);
        ZrTests_Runtime_State_Destroy(state);
        return;
    }
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_LLVM, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM,
                                  ZrLibrary_AotRuntime_GetExecutedVia(state->global),
                                  sourceCase->name);
    TEST_ASSERT_EQUAL_INT_MESSAGE(interpreted.type, result.type, sourceCase->name);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(interpreted.value.nativeObject.nativeInt64,
                                    result.value.nativeObject.nativeInt64,
                                    sourceCase->name);

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_llvm_source_cases_execute_like_interpreter(void) {
    TZrUInt32 caseIndex;

    for (caseIndex = 0; caseIndex < ZR_C_U1_SOURCE_COUNT; caseIndex++) {
        execute_case_via_llvm(&kSourceCases[caseIndex], ZR_NULL);
    }
}

/* 融合指令上的溢出：值来自参数，避免常量折叠；LLVM 与解释器报同一消息。 */
static const SZrCU1SourceCase kFaultCases[] = {
        {"fault_mul_stack", "fn m(v: int): int { return v * 4; }\nreturn m(4611686018427387904);\n"},
        {"fault_sub_stack", "fn s(v: int): int { return v - 4; }\nreturn s(-9223372036854775807);\n"},
        {"fault_add_stack", "fn addFour(value: int): int { return value + 4; }\nreturn addFour(9223372036854775807);\n"},
};

static const char *const kFaultMessages[] = {
        "signed integer multiplication overflow",
        "signed integer subtraction overflow",
        "signed integer addition overflow",
};

static void test_llvm_fused_faults_are_terminal_with_interpreter_message(void) {
    TZrUInt32 caseIndex;

    for (caseIndex = 0; caseIndex < (TZrUInt32)(sizeof(kFaultCases) / sizeof(kFaultCases[0])); caseIndex++) {
        execute_case_via_llvm(&kFaultCases[caseIndex], kFaultMessages[caseIndex]);
    }
}
#endif

/* 读取 RunError 规范化后异常对象上的 message 字段。 */
static const TZrChar *current_error_message(SZrState *state) {
    SZrString *fieldName;
    SZrTypeValue key;
    const SZrTypeValue *field;

    if (!state->hasCurrentException || state->currentException.type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }
    fieldName = ZrCore_String_CreateFromNative(state, "message");
    if (fieldName == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldName));
    key.type = ZR_VALUE_TYPE_STRING;
    field = ZrCore_Object_GetValue(state, ZR_CAST_OBJECT(state, state->currentException.value.object), &key);
    return field != ZR_NULL && field->type == ZR_VALUE_TYPE_STRING
                   ? ZrCore_String_GetNativeString(ZR_CAST_STRING(state, field->value.object))
                   : ZR_NULL;
}

typedef struct SZrCU1DivideCall {
    ZrAotGeneratedFrame *frame;
    TZrUInt32 constantIndex;
    TZrBool returned;
} SZrCU1DivideCall;

static void call_div_signed_const(SZrState *state, TZrPtr arguments) {
    SZrCU1DivideCall *call = (SZrCU1DivideCall *)arguments;

    call->returned = ZrLibrary_AotRuntime_DivSignedConst(state, call->frame, 0u, 1u, call->constantIndex);
}

/* 子进程退出码：0 通过；其余数值标识失败的行，便于父进程报告。 */
static int run_div_signed_const_rows(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrTypeValueOnStack slots[4];
    ZrAotGeneratedFrame frame;
    SZrCU1DivideCall call;
    EZrThreadStatus status;
    const TZrChar *message;

    if (state == ZR_NULL) {
        return 2;
    }
    memset(slots, 0, sizeof(slots));
    memset(&frame, 0, sizeof(frame));
    function = create_row_function(state);
    frame.function = function;
    frame.slotBase = slots;
    frame.generatedFrameSlotCount = 4u;

    /* (INT64_MIN + 1) / -1 = INT64_MAX，结果为 INT64。 */
    ZrCore_Value_InitAsInt(state, &slots[0].value, 42);
    ZrCore_Value_InitAsInt(state, &slots[1].value, INT64_MIN + 1);
    call.frame = &frame;
    call.constantIndex = 1u;
    call.returned = ZR_FALSE;
    status = ZrCore_Exception_TryRun(state, call_div_signed_const, &call);
    if (status != ZR_THREAD_STATUS_FINE || !call.returned || slots[0].value.type != ZR_VALUE_TYPE_INT64 ||
        slots[0].value.value.nativeObject.nativeInt64 != INT64_MAX) {
        return 10;
    }

    /* INT64_MIN / -1：与解释器同样报 signed integer division overflow，且不写目标槽。 */
    ZrCore_Value_InitAsInt(state, &slots[0].value, 42);
    ZrCore_Value_InitAsInt(state, &slots[1].value, INT64_MIN);
    call.returned = ZR_FALSE;
    status = ZrCore_Exception_TryRun(state, call_div_signed_const, &call);
    if (status != ZR_THREAD_STATUS_RUNTIME_ERROR) {
        return slots[0].value.value.nativeObject.nativeInt64 == INT64_MIN ? 20 : 21;
    }
    message = current_error_message(state);
    if (message == ZR_NULL || strcmp(message, "signed integer division overflow") != 0) {
        return 22;
    }
    if (slots[0].value.value.nativeObject.nativeInt64 != 42) {
        return 23;
    }

    /* 除零消息不变。 */
    ZrCore_Value_InitAsInt(state, &slots[0].value, 42);
    ZrCore_Value_InitAsInt(state, &slots[1].value, 7);
    call.constantIndex = 3u;
    state->hasCurrentException = ZR_FALSE;
    status = ZrCore_Exception_TryRun(state, call_div_signed_const, &call);
    if (status != ZR_THREAD_STATUS_RUNTIME_ERROR) {
        return 30;
    }
    message = current_error_message(state);
    if (message == ZR_NULL || strcmp(message, "divide by zero") != 0) {
        return 31;
    }
    if (slots[0].value.value.nativeObject.nativeInt64 != 42) {
        return 32;
    }

    ZrTests_Runtime_State_Destroy(state);
    return 0;
}

/* R1：在子进程中执行，旧实现的宿主 UB（x86-64 idiv 触发 SIGFPE）只会使子进程失败而不拖垮测试。 */
static void test_div_signed_const_runtime_rejects_int64_min_by_minus_one(void) {
#if defined(ZR_PLATFORM_UNIX)
    pid_t child;
    int waitStatus = 0;
    char message[128];

    fflush(stdout);
    child = fork();
    TEST_ASSERT_TRUE(child >= 0);
    if (child == 0) {
        _exit(run_div_signed_const_rows());
    }
    TEST_ASSERT_EQUAL_INT(child, waitpid(child, &waitStatus, 0));
    if (WIFSIGNALED(waitStatus)) {
        snprintf(message, sizeof(message), "DivSignedConst child died with signal %d", WTERMSIG(waitStatus));
        TEST_FAIL_MESSAGE(message);
    }
    TEST_ASSERT_TRUE(WIFEXITED(waitStatus));
    snprintf(message, sizeof(message), "DivSignedConst row failure code %d", WEXITSTATUS(waitStatus));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, WEXITSTATUS(waitStatus), message);
#else
    TEST_IGNORE_MESSAGE("fork-isolated DivSignedConst rows need a POSIX host");
#endif
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_step_flags_publish_faulting_pc_for_fused_signed_const_ops);
    RUN_TEST(test_llvm_lowers_fused_signed_load_const_family_in_interpreter_order);
    RUN_TEST(test_div_signed_const_runtime_rejects_int64_min_by_minus_one);
    RUN_TEST(test_llvm_source_cases_emit_and_lower_fused_ops);
#if defined(ZR_PLATFORM_UNIX)
    RUN_TEST(test_llvm_source_cases_execute_like_interpreter);
    RUN_TEST(test_llvm_fused_faults_are_terminal_with_interpreter_message);
#endif
    return UNITY_END();
}
