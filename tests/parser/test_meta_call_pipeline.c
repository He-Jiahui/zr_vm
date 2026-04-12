#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/writer.h"

typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrMetaCallPipelineTimer;

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

static TZrBool function_tree_contains_opcode(const SZrFunction *function, EZrInstructionCode opcode) {
    TZrUInt32 childIndex;

    if (function_contains_opcode(function, opcode)) {
        return ZR_TRUE;
    }
    if (function == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (function_tree_contains_opcode(&function->childFunctionList[childIndex], opcode)) {
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

static TZrBool function_tree_contains_callsite_cache_kind(const SZrFunction *function,
                                                          EZrFunctionCallSiteCacheKind kind) {
    TZrUInt32 index;
    TZrUInt32 childIndex;

    if (function != ZR_NULL && function->callSiteCaches != ZR_NULL) {
        for (index = 0; index < function->callSiteCacheLength; index++) {
            if ((EZrFunctionCallSiteCacheKind)function->callSiteCaches[index].kind == kind) {
                return ZR_TRUE;
            }
        }
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

static SZrFunction *compile_meta_call_fixture(SZrState *state) {
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
            "return result;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, "meta_call_pipeline_test.zr", 26);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_dynamic_call_fixture(SZrState *state) {
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
            "return apply(adder, 5);\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, "dynamic_call_pipeline_test.zr", 29);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static void test_meta_call_emits_semir_meta_runtime_contracts(void) {
    SZrMetaCallPipelineTimer timer = {0};
    const char *testSummary = "Meta Call Emits SemIR Meta Runtime Contracts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("meta call semir pipeline",
                 "Testing that one-argument @call sites quicken to cached ExecBC opcodes while SemIR keeps the META_CALL runtime contract");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        const SZrFunctionCallSiteCacheEntry *metaCache;
        const char *intermediatePath = "meta_call_pipeline_test.zri";
        char *intermediateText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_meta_call_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_CALL_CACHED)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(DYN_CALL)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(META_CALL)));
        TEST_ASSERT_TRUE(function_tree_contains_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_CALL, ZR_TRUE));
        metaCache = function_tree_find_first_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL);
        TEST_ASSERT_NOT_NULL(metaCache);
        TEST_ASSERT_EQUAL_UINT32(1, metaCache->argumentCount);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SEMIR"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "CALLSITE_CACHE_TABLE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_CALL_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "DYNAMIC_RUNTIME"));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(12, result);

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_dynamic_call_emits_semir_runtime_contracts_and_cached_execbc_variants(void) {
    SZrMetaCallPipelineTimer timer = {0};
    const char *testSummary = "Dynamic Call Emits SemIR Runtime Contracts And Cached ExecBC Variants";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("dynamic call semir pipeline",
                 "Testing that unresolved callable values lower to DYN_CALL in SemIR while ExecBC quickens them to cached dynamic call sites");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        const char *intermediatePath = "dynamic_call_pipeline_test.zri";
        char *intermediateText;
        TZrInt64 result = 0;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_dynamic_call_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED)));
        TEST_ASSERT_FALSE(function_tree_contains_opcode(function, ZR_INSTRUCTION_ENUM(DYN_CALL)));
        TEST_ASSERT_TRUE(function_tree_contains_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_DYN_CALL));
        TEST_ASSERT_TRUE(semir_tree_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_DYN_CALL, ZR_TRUE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "CALLSITE_CACHE_TABLE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "DYN_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_DYN_CALL_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "DYNAMIC_RUNTIME"));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(12, result);

        free(intermediateText);
        remove(intermediatePath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_meta_call_aot_backends_emit_runtime_contracts(void) {
    SZrMetaCallPipelineTimer timer = {0};
    const char *testSummary = "Meta Call AOT Backends Emit Runtime Contracts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("meta call aot pipeline",
                 "Testing that SemIR-driven AOT artifacts retain META_CALL and the shared function-precall runtime contract");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        const char *cPath = "meta_call_pipeline_test.c";
        const char *llvmPath = "meta_call_pipeline_test.ll";
        char *cText;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_meta_call_fixture(state);
        TEST_ASSERT_NOT_NULL(function);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR AOT C Backend"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "META_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrCore_Function_PreCall"));

        TEST_ASSERT_NOT_NULL(strstr(llvmText, "ZR AOT LLVM Backend"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "META_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "declare ptr @ZrCore_Function_PreCall"));

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

static void test_dynamic_call_aot_backends_emit_runtime_contracts(void) {
    SZrMetaCallPipelineTimer timer = {0};
    const char *testSummary = "Dynamic Call AOT Backends Emit Runtime Contracts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("dynamic call aot pipeline",
                 "Testing that SemIR-driven AOT artifacts retain DYN_CALL and the shared function-precall runtime contract");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        SZrFunction *function;
        const char *cPath = "dynamic_call_pipeline_test.c";
        const char *llvmPath = "dynamic_call_pipeline_test.ll";
        char *cText;
        char *llvmText;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_dynamic_call_fixture(state);
        TEST_ASSERT_NOT_NULL(function);

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR AOT C Backend"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "DYN_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrCore_Function_PreCall"));

        TEST_ASSERT_NOT_NULL(strstr(llvmText, "ZR AOT LLVM Backend"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "DYN_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "declare ptr @ZrCore_Function_PreCall"));

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

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_meta_call_emits_semir_meta_runtime_contracts);
    RUN_TEST(test_dynamic_call_emits_semir_runtime_contracts_and_cached_execbc_variants);
    RUN_TEST(test_meta_call_aot_backends_emit_runtime_contracts);
    RUN_TEST(test_dynamic_call_aot_backends_emit_runtime_contracts);
    return UNITY_END();
}
