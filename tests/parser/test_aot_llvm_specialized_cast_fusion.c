/* 钉住 LLVM AOT 对专用数值转换、ADD_SIGNED_MOD_CONST 和 MUL_SIGNED_LOAD_STACK 的降级。 */
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

#if defined(ZR_PLATFORM_UNIX)
#include "harness/aot_c_link_support.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_library/project.h"
#endif

#ifndef ZR_VM_TESTS_LLVM_COMPILER
#define ZR_VM_TESTS_LLVM_COMPILER "clang"
#endif

#ifndef ZR_VM_TESTS_REPO_ROOT
#define ZR_VM_TESTS_REPO_ROOT "."
#endif

#ifndef ZR_VM_TESTS_BUILD_LIB_DIR
#define ZR_VM_TESTS_BUILD_LIB_DIR "lib"
#endif

/* 20*3 + (20+3)%7 + (int)1.5 + (int)(uint)20 + (int)(uint)3
 * + (int)(uint)1.5 + (int)(float)20 + (int)(float)(uint)20 */
#define ZR_AOT_LLVM_CAST_FUSION_MIX_RESULT ((TZrInt64)127)

void setUp(void) {}

void tearDown(void) {}

static const char *mix_source(void) {
    return "fn mix(left: int, right: int): int {\n"
           "    var unsignedValue: uint = <uint>left;\n"
           "    var floatValue: float = 1.5;\n"
           "    var product: int = left * right;\n"
           "    var wrapped: int = (left + right) % 7;\n"
           "    var fromFloat: int = <int>floatValue;\n"
           "    var fromUnsigned: int = <int>unsignedValue;\n"
           "    var asUnsigned: uint = <uint>right;\n"
           "    var unsignedFromFloat: uint = <uint>floatValue;\n"
           "    var asFloat: float = <float>left;\n"
           "    var unsignedAsFloat: float = <float>unsignedValue;\n"
           "    return product + wrapped + fromFloat + fromUnsigned + <int>asUnsigned +\n"
           "        <int>unsignedFromFloat + <int>asFloat + <int>unsignedAsFloat;\n"
           "}\n"
           "return mix(20, 3);\n";
}

static const EZrInstructionCode kGapCOpcodes[] = {
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

static const EZrInstructionCode kPinnedOpcodes[] = {
        ZR_INSTRUCTION_ENUM(TO_INT_FLOAT),
        ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED),
        ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT),
        ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED),
        ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED),
        ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED),
        ZR_INSTRUCTION_ENUM(ADD_SIGNED_MOD_CONST),
        ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK),
};

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

static const char *opcode_name(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(TO_INT):
            return "TO_INT";
        case ZR_INSTRUCTION_ENUM(TO_UINT):
            return "TO_UINT";
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
            return "TO_FLOAT";
        case ZR_INSTRUCTION_ENUM(TO_INT_FLOAT):
            return "TO_INT_FLOAT";
        case ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED):
            return "TO_INT_UNSIGNED";
        case ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT):
            return "TO_UINT_FLOAT";
        case ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED):
            return "TO_UINT_SIGNED";
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED):
            return "TO_FLOAT_SIGNED";
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED):
            return "TO_FLOAT_UNSIGNED";
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
            return "ADD_SIGNED";
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
            return "ADD_SIGNED_LOAD_STACK";
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_MOD_CONST):
            return "ADD_SIGNED_MOD_CONST";
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
            return "MUL_SIGNED";
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK):
            return "MUL_SIGNED_LOAD_STACK";
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
            return "MOD_SIGNED_CONST";
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
            return "ADD_SIGNED_LOAD_CONST";
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
            return "ADD_SIGNED_LOAD_STACK_CONST";
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
            return "ADD_SIGNED_LOAD_STACK_LOAD_CONST";
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
            return "MUL_SIGNED_LOAD_CONST";
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
            return "MUL_SIGNED_LOAD_STACK_CONST";
        default:
            return ZR_NULL;
    }
}

static void append_opcode_inventory(const SZrFunction *function, char *buffer, size_t bufferSize) {
    TZrUInt32 opcode;
    size_t used = strlen(buffer);

    if (used + 1u < bufferSize) {
        snprintf(buffer + used, bufferSize - used, " opcodes:");
    }
    for (opcode = 0; opcode < (TZrUInt32)ZR_INSTRUCTION_ENUM(ENUM_MAX); opcode++) {
        TZrUInt32 count = count_opcode_recursive(function, (EZrInstructionCode)opcode, 0u);
        const char *name;

        if (count == 0u) {
            continue;
        }
        name = opcode_name((EZrInstructionCode)opcode);
        used = strlen(buffer);
        if (used + 1u >= bufferSize) {
            break;
        }
        if (name != ZR_NULL) {
            snprintf(buffer + used, bufferSize - used, " %s=%u", name, (unsigned)count);
        } else {
            snprintf(buffer + used, bufferSize - used, " op%u=%u", (unsigned)opcode, (unsigned)count);
        }
    }
}

static const TZrInstruction *find_opcode_recursive(const SZrFunction *function,
                                                   EZrInstructionCode opcode,
                                                   TZrUInt32 *seen,
                                                   TZrUInt32 target,
                                                   TZrUInt32 depth) {
    TZrUInt32 index;

    TEST_ASSERT_TRUE(depth < 64u);
    for (index = 0; index < function->instructionsLength; index++) {
        if ((EZrInstructionCode)function->instructionsList[index].instruction.operationCode == opcode) {
            if (*seen == target) {
                return &function->instructionsList[index];
            }
            *seen += 1u;
        }
    }

    if (function->childFunctionList != ZR_NULL) {
        for (index = 0; index < function->childFunctionLength; index++) {
            const TZrInstruction *match = find_opcode_recursive(&function->childFunctionList[index],
                                                                opcode,
                                                                seen,
                                                                target,
                                                                depth + 1u);
            if (match != ZR_NULL) {
                return match;
            }
        }
    }

    return ZR_NULL;
}

static void assert_text_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(text, needle), needle);
}

static const char *find_in_range(const char *start, const char *end, const char *needle) {
    size_t needleLength;
    const char *cursor;

    TEST_ASSERT_NOT_NULL(start);
    TEST_ASSERT_NOT_NULL(end);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_TRUE(end >= start);
    needleLength = strlen(needle);
    if (needleLength == 0u || (size_t)(end - start) < needleLength) {
        return ZR_NULL;
    }

    for (cursor = start; cursor + needleLength <= end; cursor++) {
        if (memcmp(cursor, needle, needleLength) == 0) {
            return cursor;
        }
    }
    return ZR_NULL;
}

static void assert_cast_region(const char *text,
                               const char *marker,
                               const char *conversionOp,
                               unsigned minType,
                               unsigned maxType,
                               unsigned resultType) {
    const char *start = strstr(text, marker);
    const char *end;
    char lowerNeedle[32];
    char upperNeedle[32];
    char storeNeedle[64];

    TEST_ASSERT_NOT_NULL_MESSAGE(start, marker);
    end = strstr(start + 1, "; zr_aot_llvm_");
    if (end == ZR_NULL) {
        end = start + strlen(start);
    }

    snprintf(lowerNeedle, sizeof(lowerNeedle), ", %u\n", minType);
    snprintf(upperNeedle, sizeof(upperNeedle), ", %u\n", maxType);
    snprintf(storeNeedle, sizeof(storeNeedle), "store i32 %u, ptr", resultType);
    TEST_ASSERT_NOT_NULL_MESSAGE(find_in_range(start, end, "icmp uge i32"), marker);
    TEST_ASSERT_NOT_NULL_MESSAGE(find_in_range(start, end, "icmp ule i32"), marker);
    TEST_ASSERT_NOT_NULL_MESSAGE(find_in_range(start, end, lowerNeedle), marker);
    TEST_ASSERT_NOT_NULL_MESSAGE(find_in_range(start, end, upperNeedle), marker);
    TEST_ASSERT_NOT_NULL_MESSAGE(find_in_range(start, end, storeNeedle), marker);
    TEST_ASSERT_NOT_NULL_MESSAGE(find_in_range(start, end, "zr_aot_cast_fast_"), marker);
    if (conversionOp != ZR_NULL) {
        TEST_ASSERT_NOT_NULL_MESSAGE(find_in_range(start, end, conversionOp), marker);
    } else {
        TEST_ASSERT_NULL_MESSAGE(find_in_range(start, end, "sitofp"), marker);
        TEST_ASSERT_NULL_MESSAGE(find_in_range(start, end, "uitofp"), marker);
        TEST_ASSERT_NULL_MESSAGE(find_in_range(start, end, "fptosi"), marker);
        TEST_ASSERT_NULL_MESSAGE(find_in_range(start, end, "fptoui"), marker);
    }
}

static void assert_opcode_not_reported_unsupported(const char *text, EZrInstructionCode opcode) {
    char needle[64];
    const char *cursor = text;

    snprintf(needle, sizeof(needle), ", i32 %u)\n", (unsigned)opcode);
    while ((cursor = strstr(cursor, "ReportUnsupportedInstruction")) != ZR_NULL) {
        const char *lineEnd = strchr(cursor, '\n');
        TEST_ASSERT_NOT_NULL(lineEnd);
        TEST_ASSERT_NULL_MESSAGE(find_in_range(cursor, lineEnd + 1, needle), needle);
        cursor = lineEnd + 1;
    }
}

static TZrInstruction make_pair_instruction(EZrInstructionCode opcode,
                                            TZrUInt16 destinationSlot,
                                            TZrUInt16 sourceSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand1[0] = sourceSlot;
    return instruction;
}

static TZrInstruction make_u8_pair_instruction(EZrInstructionCode opcode,
                                               TZrUInt16 destinationSlot,
                                               TZrUInt8 leftSlot,
                                               TZrUInt8 rightSlot) {
    TZrInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = destinationSlot;
    instruction.instruction.operand.operand0[0] = leftSlot;
    instruction.instruction.operand.operand0[1] = rightSlot;
    return instruction;
}

static SZrFunction *create_hand_built_function(SZrState *state) {
    SZrFunction *function;
    TZrInstruction modulo;

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global, sizeof(TZrInstruction) * 9u, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = make_pair_instruction(ZR_INSTRUCTION_ENUM(TO_INT_FLOAT), 1u, 0u);
    function->instructionsList[1] = make_pair_instruction(ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED), 2u, 3u);
    function->instructionsList[2] = make_pair_instruction(ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT), 4u, 5u);
    function->instructionsList[3] = make_pair_instruction(ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED), 6u, 7u);
    function->instructionsList[4] = make_pair_instruction(ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED), 8u, 9u);
    function->instructionsList[5] = make_pair_instruction(ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED), 10u, 11u);
    function->instructionsList[6] = make_u8_pair_instruction(ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK), 12u, 1u, 2u);
    modulo = make_u8_pair_instruction(ZR_INSTRUCTION_ENUM(ADD_SIGNED_MOD_CONST), 13u, 4u, 5u);
    modulo.instruction.operand.operand1[1] = 0u;
    function->instructionsList[7] = modulo;
    function->instructionsList[8] = make_pair_instruction(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1u, 13u);
    function->instructionsLength = 9u;

    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global, sizeof(SZrTypeValue), ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 7);
    function->constantValueLength = 1u;
    function->stackSize = 16u;
    function->parameterCount = 0u;
    function->lineInSourceStart = 1u;
    function->lineInSourceEnd = 9u;
    return function;
}

static char *write_llvm_for_function(SZrState *state,
                                     SZrFunction *function,
                                     const char *suiteName,
                                     const char *artifactStem) {
    SZrAotWriterOptions options;
    TZrChar generatedPath[ZR_TESTS_PATH_MAX];
    char *text;

    memset(&options, 0, sizeof(options));
    options.moduleName = artifactStem;
    options.sourceHash = artifactStem;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = artifactStem;
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(suiteName,
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

static void assert_pinned_ir(const char *text, TZrBool requireMulCall) {
    TZrUInt32 index;

    assert_text_contains(text, "declare i1 @ZrLibrary_AotRuntime_AddSignedModConst(ptr, ptr, i32, i32, i32, i32)");
    assert_cast_region(text,
                       "; zr_aot_llvm_to_int_float",
                       "fptosi double",
                       (unsigned)ZR_VALUE_TYPE_FLOAT,
                       (unsigned)ZR_VALUE_TYPE_DOUBLE,
                       (unsigned)ZR_VALUE_TYPE_INT64);
    assert_cast_region(text,
                       "; zr_aot_llvm_to_uint_float",
                       "fptoui double",
                       (unsigned)ZR_VALUE_TYPE_FLOAT,
                       (unsigned)ZR_VALUE_TYPE_DOUBLE,
                       (unsigned)ZR_VALUE_TYPE_UINT64);
    assert_cast_region(text,
                       "; zr_aot_llvm_to_float_signed",
                       "sitofp i64",
                       (unsigned)ZR_VALUE_TYPE_INT8,
                       (unsigned)ZR_VALUE_TYPE_INT64,
                       (unsigned)ZR_VALUE_TYPE_DOUBLE);
    assert_cast_region(text,
                       "; zr_aot_llvm_to_float_unsigned",
                       "uitofp i64",
                       (unsigned)ZR_VALUE_TYPE_UINT8,
                       (unsigned)ZR_VALUE_TYPE_UINT64,
                       (unsigned)ZR_VALUE_TYPE_DOUBLE);
    assert_cast_region(text,
                       "; zr_aot_llvm_to_int_unsigned",
                       ZR_NULL,
                       (unsigned)ZR_VALUE_TYPE_UINT8,
                       (unsigned)ZR_VALUE_TYPE_UINT64,
                       (unsigned)ZR_VALUE_TYPE_INT64);
    assert_cast_region(text,
                       "; zr_aot_llvm_to_uint_signed",
                       ZR_NULL,
                       (unsigned)ZR_VALUE_TYPE_INT8,
                       (unsigned)ZR_VALUE_TYPE_INT64,
                       (unsigned)ZR_VALUE_TYPE_UINT64);
    if (requireMulCall) {
        assert_text_contains(text, "; zr_aot_llvm_mul_signed_load_stack");
        assert_text_contains(text,
                             "call i1 @ZrLibrary_AotRuntime_MulSigned(ptr %state, ptr %frame, i32 12, i32 1, i32 2)");
        /* 两条融合指令都会报整数溢出，步进点须带 MAY_THROW(1) 才会发布 PC；除数 7 非零也不例外。 */
        assert_text_contains(text, "call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 6, i32 1)");
        assert_text_contains(text, "call i1 @ZrLibrary_AotRuntime_BeginInstruction(ptr %state, ptr %frame, i32 7, i32 1)");
    }
    assert_text_contains(text, "; zr_aot_llvm_add_signed_mod_const");
    assert_text_contains(
            text,
            "call i1 @ZrLibrary_AotRuntime_AddSignedModConst(ptr %state, ptr %frame, i32 13, i32 4, i32 5, i32 0)");
    for (index = 0; index < (TZrUInt32)(sizeof(kPinnedOpcodes) / sizeof(kPinnedOpcodes[0])); index++) {
        assert_opcode_not_reported_unsupported(text, kPinnedOpcodes[index]);
    }
}

static void test_llvm_hand_built_casts_and_fusions_lower_exact_operands(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    char *text;

    TEST_ASSERT_NOT_NULL(state);
    function = create_hand_built_function(state);
    text = write_llvm_for_function(state, function, "aot_llvm_specialized_cast_fusion", "hand_built");
    assert_pinned_ir(text, ZR_TRUE);
#if defined(ZR_PLATFORM_UNIX)
    {
        TZrChar objectPath[ZR_TESTS_PATH_MAX];
        TZrChar llvmPath[ZR_TESTS_PATH_MAX];
        char command[4096];
        int status;

        TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_llvm_specialized_cast_fusion",
                                                           "generated",
                                                           "hand_built",
                                                           ".ll",
                                                           llvmPath,
                                                           sizeof(llvmPath)));
        TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_llvm_specialized_cast_fusion",
                                                           "generated",
                                                           "hand_built",
                                                           ".o",
                                                           objectPath,
                                                           sizeof(objectPath)));
        snprintf(command,
                 sizeof(command),
                 "\"%s\" " ZR_VM_TESTS_LLVM_POINTER_FLAGS " -c -o \"%s\" \"%s\"",
                 ZR_VM_TESTS_LLVM_COMPILER,
                 objectPath,
                 llvmPath);
        status = system(command);
        if (status != 0) {
            printf("Command failed with status %d:\n%s\n", status, command);
        }
        TEST_ASSERT_EQUAL_INT(0, status);
    }
#endif
    free(text);
    ZrTests_Runtime_State_Destroy(state);
}

static SZrFunction *compile_mix(SZrState *state) {
    SZrString *sourceName;
    const char *source = mix_source();

    sourceName = ZrCore_String_CreateFromNative(state, "aot_llvm_specialized_cast_fusion.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static void assert_source_opcode_contract(const SZrFunction *function) {
    char inventory[4096];
    TZrUInt32 index;

    inventory[0] = '\0';
    append_opcode_inventory(function, inventory, sizeof(inventory));
    for (index = 0; index < (TZrUInt32)(sizeof(kGapCOpcodes) / sizeof(kGapCOpcodes[0])); index++) {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u,
                                         count_opcode_recursive(function, kGapCOpcodes[index], 0u),
                                         inventory);
    }

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u,
                                            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT_FLOAT), 0u),
                                            inventory);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u,
                                            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED), 0u),
                                            inventory);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u,
                                            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT), 0u),
                                            inventory);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u,
                                            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED), 0u),
                                            inventory);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u,
                                            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED), 0u),
                                            inventory);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(0u,
                                            count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED), 0u),
                                            inventory);
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(
            0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_MOD_CONST), 0u), inventory);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_INT), 0u), inventory);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_UINT), 0u), inventory);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(TO_FLOAT), 0u), inventory);
}

static void assert_source_fusion_calls(const SZrFunction *function, const char *text) {
    TZrUInt32 count;
    TZrUInt32 index;

    count = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_MOD_CONST), 0u);
    for (index = 0; index < count; index++) {
        TZrUInt32 seen = 0u;
        const TZrInstruction *instruction =
                find_opcode_recursive(function, ZR_INSTRUCTION_ENUM(ADD_SIGNED_MOD_CONST), &seen, index, 0u);
        char needle[192];

        TEST_ASSERT_NOT_NULL(instruction);
        snprintf(needle,
                 sizeof(needle),
                 "call i1 @ZrLibrary_AotRuntime_AddSignedModConst(ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u, i32 %u)",
                 (unsigned)instruction->instruction.operandExtra,
                 (unsigned)instruction->instruction.operand.operand0[0],
                 (unsigned)instruction->instruction.operand.operand0[1],
                 (unsigned)instruction->instruction.operand.operand1[1]);
        assert_text_contains(text, needle);
    }

    count = count_opcode_recursive(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK), 0u);
    for (index = 0; index < count; index++) {
        TZrUInt32 seen = 0u;
        const TZrInstruction *instruction =
                find_opcode_recursive(function, ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK), &seen, index, 0u);
        char needle[192];

        TEST_ASSERT_NOT_NULL(instruction);
        snprintf(needle,
                 sizeof(needle),
                 "call i1 @ZrLibrary_AotRuntime_MulSigned(ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u)",
                 (unsigned)instruction->instruction.operandExtra,
                 (unsigned)instruction->instruction.operand.operand0[0],
                 (unsigned)instruction->instruction.operand.operand0[1]);
        assert_text_contains(text, needle);
        assert_text_contains(text, "; zr_aot_llvm_mul_signed_load_stack");
    }
}

static void test_llvm_source_casts_and_add_mod_match_interpreter(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    TZrInt64 interpreted = 0;
    char *text;
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_mix(state);
    TEST_ASSERT_NOT_NULL(function);
    assert_source_opcode_contract(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &interpreted));
    TEST_ASSERT_EQUAL_INT64(ZR_AOT_LLVM_CAST_FUSION_MIX_RESULT, interpreted);

    text = write_llvm_for_function(state, function, "aot_llvm_specialized_cast_fusion", "mix_source");
    assert_text_contains(text, "; zr_aot_llvm_to_int_float");
    assert_text_contains(text, "; zr_aot_llvm_to_int_unsigned");
    assert_text_contains(text, "; zr_aot_llvm_to_uint_float");
    assert_text_contains(text, "; zr_aot_llvm_to_uint_signed");
    assert_text_contains(text, "; zr_aot_llvm_to_float_signed");
    assert_text_contains(text, "; zr_aot_llvm_to_float_unsigned");
    assert_text_contains(text, "fptosi double");
    assert_text_contains(text, "fptoui double");
    assert_text_contains(text, "sitofp i64");
    assert_text_contains(text, "uitofp i64");
    assert_text_contains(text, "; zr_aot_llvm_add_signed_mod_const");
    assert_source_fusion_calls(function, text);
    for (index = 0; index < (TZrUInt32)(sizeof(kPinnedOpcodes) / sizeof(kPinnedOpcodes[0])); index++) {
        if (count_opcode_recursive(function, kPinnedOpcodes[index], 0u) > 0u) {
            assert_opcode_not_reported_unsupported(text, kPinnedOpcodes[index]);
        }
    }
    free(text);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void set_signed_slot(SZrState *state, SZrTypeValueOnStack *slot, TZrInt64 value, EZrValueType type) {
    ZrCore_Value_InitAsInt(state, &slot->value, value);
    slot->value.type = type;
}

static void expect_add_mod(SZrState *state,
                           ZrAotGeneratedFrame *frame,
                           SZrTypeValueOnStack *slots,
                           TZrInt64 left,
                           EZrValueType leftType,
                           TZrInt64 right,
                           TZrUInt32 constantIndex,
                           TZrBool expectedSuccess,
                           TZrInt64 expectedValue) {
    TZrBool success;

    set_signed_slot(state, &slots[0], 42, ZR_VALUE_TYPE_INT64);
    set_signed_slot(state, &slots[1], left, leftType);
    set_signed_slot(state, &slots[2], right, ZR_VALUE_TYPE_INT64);
    success = ZrLibrary_AotRuntime_AddSignedModConst(state, frame, 0u, 1u, 2u, constantIndex);
    TEST_ASSERT_EQUAL(expectedSuccess ? ZR_TRUE : ZR_FALSE, success);
    if (expectedSuccess) {
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, slots[0].value.type);
        TEST_ASSERT_EQUAL_INT64(expectedValue, slots[0].value.value.nativeObject.nativeInt64);
    } else {
        TEST_ASSERT_EQUAL_INT64(42, slots[0].value.value.nativeObject.nativeInt64);
    }
}

static void test_add_signed_mod_const_runtime_matches_integer_contract(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrTypeValueOnStack slots[4];
    ZrAotGeneratedFrame frame;

    TEST_ASSERT_NOT_NULL(state);
    memset(slots, 0, sizeof(slots));
    memset(&frame, 0, sizeof(frame));
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->constantValueList = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            state->global, sizeof(SZrTypeValue) * 5u, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->constantValueList);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[0], 7);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[1], -1);
    ZrCore_Value_InitAsInt(state, &function->constantValueList[2], 5);
    ZrCore_Value_InitAsUInt(state, &function->constantValueList[3], 7u);
    ZrCore_Value_InitAsFloat(state, &function->constantValueList[4], 1.5);
    function->constantValueLength = 5u;
    frame.function = function;
    frame.slotBase = slots;
    frame.generatedFrameSlotCount = 4u;

    expect_add_mod(state, &frame, slots, 8, ZR_VALUE_TYPE_INT64, -3, 0u, ZR_TRUE, 5);
    expect_add_mod(state, &frame, slots, 5, ZR_VALUE_TYPE_INT64, 3, 1u, ZR_TRUE, 0);
    expect_add_mod(state, &frame, slots, -8, ZR_VALUE_TYPE_INT64, 1, 2u, ZR_TRUE, -2);
    expect_add_mod(state, &frame, slots, 8, ZR_VALUE_TYPE_INT32, -3, 0u, ZR_TRUE, 5);
    expect_add_mod(state, &frame, slots, 8, ZR_VALUE_TYPE_INT64, -3, 3u, ZR_TRUE, 5);

    set_signed_slot(state, &slots[0], 42, ZR_VALUE_TYPE_INT64);
    ZrCore_Value_InitAsUInt(state, &slots[1].value, 8u);
    set_signed_slot(state, &slots[2], -3, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_AddSignedModConst(state, &frame, 0u, 1u, 2u, 0u));
    TEST_ASSERT_EQUAL_INT64(42, slots[0].value.value.nativeObject.nativeInt64);

    set_signed_slot(state, &slots[1], 8, ZR_VALUE_TYPE_INT64);
    ZrCore_Value_InitAsUInt(state, &slots[2].value, 3u);
    TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_AddSignedModConst(state, &frame, 0u, 1u, 2u, 0u));
    TEST_ASSERT_EQUAL_INT64(42, slots[0].value.value.nativeObject.nativeInt64);

    set_signed_slot(state, &slots[2], -3, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_AddSignedModConst(state, &frame, 0u, 1u, 2u, 4u));
    TEST_ASSERT_EQUAL_INT64(42, slots[0].value.value.nativeObject.nativeInt64);
    TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_AddSignedModConst(state, &frame, 0u, 1u, 2u, 9u));
    TEST_ASSERT_EQUAL_INT64(42, slots[0].value.value.nativeObject.nativeInt64);
    TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_AddSignedModConst(state, &frame, 4u, 1u, 2u, 0u));
    TEST_ASSERT_FALSE(ZrLibrary_AotRuntime_AddSignedModConst(state, ZR_NULL, 0u, 1u, 2u, 0u));

    ZrTests_Runtime_State_Destroy(state);
}

#if defined(ZR_PLATFORM_UNIX)
static int run_command_expect_success(const char *command) {
    int result = system(command);

    if (result != 0) {
        printf("Command failed with status %d:\n%s\n", result, command);
    }
    return result;
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

static void write_text_file_or_fail(const TZrChar *path, const char *text) {
    FILE *file;

    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(path));
    file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static void test_llvm_shared_library_executes_mix_source(void) {
    const char *projectJson =
            "{"
            "\"name\":\"aot-llvm-cast-fusion\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue result;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0u;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char command[8192];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_mix(state);
    TEST_ASSERT_NOT_NULL(function);
    assert_source_opcode_contract(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_llvm_specialized_cast_fusion",
                                                       "runtime_project",
                                                       "cast_fusion",
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_llvm_specialized_cast_fusion",
                                                       "runtime_project/src",
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_llvm_specialized_cast_fusion",
                                                       "runtime_project/bin",
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_llvm_specialized_cast_fusion",
                                                       "runtime_project/bin/aot_llvm/src",
                                                       "main",
                                                       ".ll",
                                                       generatedPath,
                                                       sizeof(generatedPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_llvm_specialized_cast_fusion",
                                                       "runtime_project/bin/aot_llvm/lib",
                                                       "zrvm_aot_main",
                                                       ".so",
                                                       sharedLibraryPath,
                                                       sizeof(sharedLibraryPath)));
    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, mix_source());

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
    TEST_ASSERT_EQUAL_INT(0, run_command_expect_success(command));

    ZrCore_Function_Free(state, function);
    project = ZrLibrary_Project_New(state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(state->global,
                                                          ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_LLVM,
                                                          ZR_TRUE));
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_AotRuntime_ExecuteEntry(state, ZR_AOT_BACKEND_KIND_LLVM, &result),
                             ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(result.type));
    TEST_ASSERT_EQUAL_INT64(ZR_AOT_LLVM_CAST_FUSION_MIX_RESULT, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_EXECUTED_VIA_AOT_LLVM, ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrTests_Runtime_State_Destroy(state);
}
#endif

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_llvm_hand_built_casts_and_fusions_lower_exact_operands);
    RUN_TEST(test_llvm_source_casts_and_add_mod_match_interpreter);
    RUN_TEST(test_add_signed_mod_const_runtime_matches_integer_contract);
#if defined(ZR_PLATFORM_UNIX)
    RUN_TEST(test_llvm_shared_library_executes_mix_source);
#endif
    return UNITY_END();
}
