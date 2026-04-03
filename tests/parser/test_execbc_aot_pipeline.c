#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/state.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_parser.h"
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

static TZrBool semir_contains_opcode_with_deopt(const SZrFunction *function,
                                                EZrSemIrOpcode opcode,
                                                TZrBool requireDeopt);
static TZrUInt32 function_count_callsite_cache_kind(const SZrFunction *function,
                                                    EZrFunctionCallSiteCacheKind kind);
static SZrFunction *compile_cached_meta_and_dynamic_callsite_fixture(SZrState *state);

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

static char *read_reference_file(const char *relativePath, size_t *outSize) {
    return ZrTests_Reference_ReadFixture(relativePath, outSize);
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
        sourceName = ZrCore_String_Create(state, "execbc_aot_dynamic_test.zr", 26);
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);

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

static void test_aot_c_and_llvm_backends_emit_runtime_contract_artifacts(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "AOT C And LLVM Backends Emit Runtime Contract Artifacts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("aot artifact emission",
                 "Testing that AOT artifacts only advertise the runtime contracts still carried by SemIR and no longer mention legacy dynamic quickening opcodes");

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
        sourceName = ZrCore_String_Create(state, "execbc_aot_backend_test.zr", 26);
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_MEMBER)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(GET_BY_INDEX)));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, func, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, func, llvmPath));

        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(cText, "ZR AOT C Backend"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrCore_Reflection_TypeOfValue"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "TYPEOF"));
        TEST_ASSERT_NULL(strstr(cText, "DYN_GET"));
        TEST_ASSERT_NULL(strstr(cText, "ZrCore_Object_GetByIndex"));

        TEST_ASSERT_NOT_NULL(strstr(llvmText, "ZR AOT LLVM Backend"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "declare i1 @ZrCore_Reflection_TypeOfValue"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "define i1 @zr_aot_entry"));
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
        sourceName = ZrCore_String_Create(state, "execbc_call_site_quickening_test.zr", 35);
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);

        func = ZrParser_Compiler_Compile(state, ast);
        ZrParser_Ast_Free(state, ast);
        TEST_ASSERT_NOT_NULL(func);

        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_FUNCTION_CALL_NO_ARGS)));
        TEST_ASSERT_TRUE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(SUPER_META_CALL_NO_ARGS)));
        TEST_ASSERT_FALSE(function_contains_opcode(func, ZR_INSTRUCTION_ENUM(META_CALL)));
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(func, ZR_SEMIR_OPCODE_META_CALL, ZR_TRUE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, func, intermediatePath));
        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SEMIR"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_CALL"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_FUNCTION_CALL_NO_ARGS"));
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

    sourceName = ZrCore_String_Create(state, "meta_access_pipeline_test.zr", 28);
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

    sourceName = ZrCore_String_Create(state, "static_meta_access_pipeline_test.zr", 35);
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

    sourceName = ZrCore_String_Create(state, "zero_arg_tail_call_site_quickening_test.zr", 38);
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

    sourceName = ZrCore_String_Create(state, "cached_meta_dyn_callsite_test.zr", 31);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static SZrFunction *compile_ownership_upgrade_release_fixture(SZrState *state) {
    const char *source =
            "class Box {}\n"
            "var owner = %shared new Box();\n"
            "var watcher = %weak(owner);\n"
            "var alias = %upgrade(watcher);\n"
            "var droppedOwner = %release(owner);\n"
            "var stillAlive = %upgrade(watcher);\n"
            "var droppedAlias = %release(alias);\n"
            "var droppedStillAlive = %release(stillAlive);\n"
            "var after = %upgrade(watcher);\n"
            "if (droppedOwner == null && droppedAlias == null && droppedStillAlive == null && owner == null && alias == null && stillAlive == null && after == null) {\n"
            "    return 1;\n"
            "}\n"
            "return 0;\n";
    SZrString *sourceName;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_Create(state, "ownership_upgrade_release_pipeline_test.zr", 41);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static void test_meta_access_semir_and_aot_use_dedicated_meta_get_set_opcodes(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Meta Access SemIR And AOT Use Dedicated Meta Get Set Opcodes";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("meta get/set semantic pipeline",
                 "Testing that property getter/setter lowering emits direct META_GET and META_SET in ExecBC while keeping the same SemIR and AOT contracts");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "meta_access_pipeline_test.zri";
        const char *cPath = "meta_access_pipeline_test.c";
        const char *llvmPath = "meta_access_pipeline_test.ll";
        const char *binaryPath = "meta_access_pipeline_test.zro";
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

        function = compile_meta_access_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(SUPER_META_SET_CACHED)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(META_GET)));
        TEST_ASSERT_FALSE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(META_SET)));
        TEST_ASSERT_EQUAL_UINT32(2, function->callSiteCacheLength);
        TEST_ASSERT_TRUE(function_contains_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET));
        TEST_ASSERT_TRUE(function_contains_callsite_cache_kind(function, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET));
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
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_GET_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_SET_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_GET"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_SET"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "META_GET"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "META_SET"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrCore_Function_PreCall"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "META_GET"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "META_SET"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "declare ptr @ZrCore_Function_PreCall"));

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
        TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)sourceObject->modules[0].entryFunction->callSiteCacheLength);

        runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
        TEST_ASSERT_NOT_NULL(runtimeFunction);
        TEST_ASSERT_EQUAL_UINT32(2, runtimeFunction->callSiteCacheLength);
        TEST_ASSERT_TRUE(function_contains_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_GET));
        TEST_ASSERT_TRUE(function_contains_callsite_cache_kind(runtimeFunction, ZR_FUNCTION_CALLSITE_CACHE_KIND_META_SET));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(16, result);

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
        const char *binaryPath = "static_meta_access_pipeline_test.zro";
        SZrFunction *function;
        char *intermediateText;
        TZrSize binaryLength = 0;
        TZrByte *binaryBytes;
        SZrBinaryFixtureReader reader;
        SZrIo *io;
        SZrIoSource *sourceObject;
        SZrFunction *runtimeFunction;

        TEST_ASSERT_NOT_NULL(state);

        function = compile_static_meta_access_fixture(state);
        TEST_ASSERT_NOT_NULL(function);
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
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, binaryPath));

        intermediateText = read_text_file_owned(intermediatePath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "CALLSITE_CACHE_TABLE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_GET_STATIC"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_SET_STATIC"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_GET_STATIC_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_SET_STATIC_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_GET"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_SET"));

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
        remove(intermediatePath);
        remove(binaryPath);
        ZrCore_Function_Free(state, function);
        ZrTests_Runtime_State_Destroy(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

static void test_reference_property_fixture_preserves_meta_access_artifacts(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Reference Property Fixture Preserves Meta Access Artifacts";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("reference property artifact pipeline",
                 "Testing that the reference property precedence fixture keeps quickened ExecBC meta access while SemIR and AOT artifacts preserve META_GET and META_SET contracts");

    {
        SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
        const char *intermediatePath = "reference_property_getter_setter_precedence.zri";
        const char *cPath = "reference_property_getter_setter_precedence.c";
        const char *llvmPath = "reference_property_getter_setter_precedence.ll";
        SZrFunction *function;
        char *intermediateText;
        char *cText;
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
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_GET, ZR_TRUE));
        TEST_ASSERT_TRUE(semir_contains_opcode_with_deopt(function, ZR_SEMIR_OPCODE_META_SET, ZR_TRUE));

        TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, intermediatePath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFile(state, function, cPath));
        TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFile(state, function, llvmPath));

        intermediateText = read_text_file_owned(intermediatePath);
        cText = read_text_file_owned(cPath);
        llvmText = read_text_file_owned(llvmPath);
        TEST_ASSERT_NOT_NULL(intermediateText);
        TEST_ASSERT_NOT_NULL(cText);
        TEST_ASSERT_NOT_NULL(llvmText);

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_GET_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "SUPER_META_SET_CACHED"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_GET"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "META_SET"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "META_GET"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "META_SET"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "META_GET"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "META_SET"));

        TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, function, &result));
        TEST_ASSERT_EQUAL_INT64(4142, result);

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

static void test_ownership_upgrade_release_semir_and_aot_use_dedicated_opcodes(void) {
    SZrExecBcAotTestTimer timer;
    const char *testSummary = "Ownership Upgrade Release SemIR And AOT Use Dedicated Opcodes";

    timer.startTime = clock();
    ZR_TEST_START(testSummary);
    ZR_TEST_INFO("ownership upgrade/release semantic pipeline",
                 "Testing that %upgrade/%release lower to dedicated ExecBC and SemIR opcodes and that AOT artifacts advertise the ownership runtime contracts");

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
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_UPGRADE)));
        TEST_ASSERT_TRUE(function_contains_opcode(function, ZR_INSTRUCTION_ENUM(OWN_RELEASE)));
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

        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_UPGRADE"));
        TEST_ASSERT_NOT_NULL(strstr(intermediateText, "OWN_RELEASE"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "OWN_UPGRADE"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "OWN_RELEASE"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrCore_Ownership_UpgradeValue"));
        TEST_ASSERT_NOT_NULL(strstr(cText, "ZrCore_Ownership_ReleaseValue"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "OWN_UPGRADE"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "OWN_RELEASE"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "declare i1 @ZrCore_Ownership_UpgradeValue"));
        TEST_ASSERT_NOT_NULL(strstr(llvmText, "declare void @ZrCore_Ownership_ReleaseValue"));

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

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_access_lowering_preserves_explicit_member_and_index_ops);
    RUN_TEST(test_aot_c_and_llvm_backends_emit_runtime_contract_artifacts);
    RUN_TEST(test_execbc_quickens_zero_arg_call_sites_without_changing_semir_contracts);
    RUN_TEST(test_execbc_quickens_cached_meta_and_dynamic_call_sites_and_preserves_binary_metadata);
    RUN_TEST(test_execbc_quickens_zero_arg_tail_call_sites_without_changing_semir_contracts);
    RUN_TEST(test_meta_access_semir_and_aot_use_dedicated_meta_get_set_opcodes);
    RUN_TEST(test_static_meta_access_quickens_to_static_callsite_cache_variants);
    RUN_TEST(test_reference_property_fixture_preserves_meta_access_artifacts);
    RUN_TEST(test_reference_member_index_fixture_preserves_split_access_artifacts);
    RUN_TEST(test_reference_foreach_fixture_preserves_iter_contract_artifacts);
    RUN_TEST(test_ownership_upgrade_release_semir_and_aot_use_dedicated_opcodes);
    return UNITY_END();
}
