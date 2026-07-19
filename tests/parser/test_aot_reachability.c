#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "backend_aot_function_table.h"
#include "backend_aot_reachability.h"
#include "backend_aot_reachability_function_graph.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "harness/runtime_support.h"

void setUp(void) {}

void tearDown(void) {}

static TZrInstruction test_create_instruction_2(EZrInstructionCode opcode,
                                                TZrUInt16 operandExtra,
                                                TZrUInt16 operandA,
                                                TZrUInt16 operandB) {
    TZrInstruction instruction;

    instruction.value = 0u;
    instruction.instruction.operationCode = (TZrUInt16)opcode;
    instruction.instruction.operandExtra = operandExtra;
    instruction.instruction.operand.operand1[0] = operandA;
    instruction.instruction.operand.operand1[1] = operandB;
    return instruction;
}

static SZrObject *get_or_create_function_metadata_object(SZrState *state, SZrFunction *function) {
    SZrObject *metadataObject;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);

    if (function->hasDecoratorMetadata &&
        function->decoratorMetadataValue.type == ZR_VALUE_TYPE_OBJECT &&
        function->decoratorMetadataValue.value.object != ZR_NULL) {
        metadataObject = ZR_CAST_OBJECT(state, function->decoratorMetadataValue.value.object);
        if (metadataObject != ZR_NULL) {
            return metadataObject;
        }
    }

    metadataObject = ZrCore_Object_New(state, ZR_NULL);
    TEST_ASSERT_NOT_NULL(metadataObject);
    ZrCore_Value_InitAsRawObject(state,
                                 &function->decoratorMetadataValue,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(metadataObject));
    function->hasDecoratorMetadata = ZR_TRUE;
    return metadataObject;
}

static void mark_function_metadata_uint(SZrState *state,
                                        SZrFunction *function,
                                        const TZrChar *fieldName,
                                        TZrUInt64 fieldValue) {
    SZrObject *metadataObject;
    SZrString *fieldString;
    SZrTypeValue key;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(fieldName);

    metadataObject = get_or_create_function_metadata_object(state, function);
    TEST_ASSERT_NOT_NULL(metadataObject);
    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    TEST_ASSERT_NOT_NULL(fieldString);
    ZrCore_Value_InitAsRawObject(state,
                                 &key,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    ZrCore_Value_InitAsUInt(state, &value, fieldValue);
    ZrCore_Object_SetValue(state, metadataObject, &key, &value);
}

static void mark_function_metadata_string(SZrState *state,
                                          SZrFunction *function,
                                          const TZrChar *fieldName,
                                          const TZrChar *fieldValue) {
    SZrObject *metadataObject;
    SZrString *fieldString;
    SZrString *valueString;
    SZrTypeValue key;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(fieldName);
    TEST_ASSERT_NOT_NULL(fieldValue);

    metadataObject = get_or_create_function_metadata_object(state, function);
    TEST_ASSERT_NOT_NULL(metadataObject);
    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    valueString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldValue);
    TEST_ASSERT_NOT_NULL(fieldString);
    TEST_ASSERT_NOT_NULL(valueString);
    ZrCore_Value_InitAsRawObject(state,
                                 &key,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    ZrCore_Value_InitAsRawObject(state,
                                 &value,
                                 ZR_CAST_RAW_OBJECT_AS_SUPER(valueString));
    value.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, metadataObject, &key, &value);
}

static void attach_typed_method_token(SZrFunction *rootFunction,
                                      SZrFunctionTypedExportSymbol *symbol,
                                      TZrUInt32 callableChildIndex,
                                      TZrMetadataToken metadataToken,
                                      TZrUInt8 exportKind) {
    memset(symbol, 0, sizeof(*symbol));
    symbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    symbol->exportKind = exportKind;
    symbol->callableChildIndex = callableChildIndex;
    symbol->metadataToken = metadataToken;
    rootFunction->typedExportedSymbols = symbol;
    rootFunction->typedExportedSymbolLength = 1u;
}

static void attach_typed_exported_method_token(SZrFunction *rootFunction,
                                               SZrFunctionTypedExportSymbol *symbol,
                                               TZrUInt32 callableChildIndex,
                                               TZrMetadataToken metadataToken) {
    attach_typed_method_token(rootFunction,
                              symbol,
                              callableChildIndex,
                              metadataToken,
                              ZR_MODULE_EXPORT_KIND_FUNCTION);
}

static void init_typed_exported_method_name(SZrState *state,
                                            SZrFunctionTypedExportSymbol *symbol,
                                            TZrUInt32 callableChildIndex,
                                            const TZrChar *methodName,
                                            TZrUInt64 signatureHash) {
    memset(symbol, 0, sizeof(*symbol));
    symbol->name = ZrCore_String_CreateFromNative(state, (TZrNativeString)methodName);
    TEST_ASSERT_NOT_NULL(symbol->name);
    symbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    symbol->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    symbol->callableChildIndex = callableChildIndex;
    symbol->signatureHash = signatureHash;
}

static void attach_typed_exported_method_name(SZrState *state,
                                              SZrFunction *rootFunction,
                                              SZrFunctionTypedExportSymbol *symbol,
                                              TZrUInt32 callableChildIndex,
                                              const TZrChar *methodName) {
    init_typed_exported_method_name(state, symbol, callableChildIndex, methodName, 0u);
    rootFunction->typedExportedSymbols = symbol;
    rootFunction->typedExportedSymbolLength = 1u;
}

static void test_reachability_marks_roots_and_direct_dependencies(void) {
    static const TZrUInt32 roots[] = {0u};
    static const EZrAotReachabilityReason rootReasons[] = {ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY};
    static const SZrAotReachabilityEdge edges[] = {
            {0u, 1u, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL},
            {1u, 3u, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL},
            {0u, 2u, ZR_AOT_REACHABILITY_REASON_FIELD_ACCESS},
            {4u, 5u, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL},
    };
    SZrAotReachabilityMark marks[6];
    TZrUInt32 queue[6];
    TZrUInt32 markedCount = 0u;

    TEST_ASSERT_TRUE(backend_aot_reachability_compute(marks,
                                                      6u,
                                                      roots,
                                                      rootReasons,
                                                      1u,
                                                      edges,
                                                      4u,
                                                      queue,
                                                      6u,
                                                      &markedCount));

    TEST_ASSERT_EQUAL_UINT32(4u, markedCount);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[0].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY, marks[0].reason);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_REACHABILITY_NO_NODE, marks[0].predecessor);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[1].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, marks[1].reason);
    TEST_ASSERT_EQUAL_UINT32(0u, marks[1].predecessor);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[2].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_FIELD_ACCESS, marks[2].reason);
    TEST_ASSERT_EQUAL_UINT32(0u, marks[2].predecessor);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[3].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, marks[3].reason);
    TEST_ASSERT_EQUAL_UINT32(1u, marks[3].predecessor);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_UNMARKED, marks[4].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_UNMARKED, marks[5].state);
}

static void test_reachability_preserves_root_reason_and_rejects_invalid_graphs(void) {
    static const TZrUInt32 roots[] = {0u, 2u};
    static const EZrAotReachabilityReason rootReasons[] = {
            ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY,
            ZR_AOT_REACHABILITY_REASON_MANIFEST,
    };
    static const SZrAotReachabilityEdge edges[] = {
            {0u, 1u, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL},
            {1u, 2u, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL},
    };
    static const SZrAotReachabilityEdge invalidEdges[] = {
            {0u, 4u, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL},
    };
    SZrAotReachabilityMark marks[3];
    TZrUInt32 queue[3];
    TZrUInt32 markedCount = 0u;

    TEST_ASSERT_TRUE(backend_aot_reachability_compute(marks,
                                                      3u,
                                                      roots,
                                                      rootReasons,
                                                      2u,
                                                      edges,
                                                      2u,
                                                      queue,
                                                      3u,
                                                      &markedCount));

    TEST_ASSERT_EQUAL_UINT32(3u, markedCount);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_MANIFEST, marks[2].reason);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_REACHABILITY_NO_NODE, marks[2].predecessor);
    TEST_ASSERT_FALSE(backend_aot_reachability_compute(marks,
                                                       3u,
                                                       roots,
                                                       rootReasons,
                                                       2u,
                                                       edges,
                                                       2u,
                                                       queue,
                                                       2u,
                                                       &markedCount));
    TEST_ASSERT_FALSE(backend_aot_reachability_compute(marks,
                                                       3u,
                                                       roots,
                                                       rootReasons,
                                                       1u,
                                                       invalidEdges,
                                                       1u,
                                                       queue,
                                                       3u,
                                                       &markedCount));
}

static char *test_read_stream(FILE *file) {
    long length;
    char *text;

    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT(0, fflush(file));
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0L, SEEK_END));
    length = ftell(file);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, length);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0L, SEEK_SET));

    text = (char *)malloc((size_t)length + 1u);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_EQUAL_size_t((size_t)length, fread(text, 1u, (size_t)length, file));
    text[length] = '\0';
    return text;
}

static void test_reachability_function_manifest_is_stable_and_preserves_reason_chain(void) {
    static const SZrAotReachabilityMark marks[] = {
            {ZR_AOT_REACHABILITY_STATE_PROCESSED,
             ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY,
             ZR_AOT_REACHABILITY_NO_NODE},
            {ZR_AOT_REACHABILITY_STATE_PROCESSED,
             ZR_AOT_REACHABILITY_REASON_DIRECT_CALL,
             0u},
            {ZR_AOT_REACHABILITY_STATE_UNMARKED,
             ZR_AOT_REACHABILITY_REASON_NONE,
             ZR_AOT_REACHABILITY_NO_NODE},
            {ZR_AOT_REACHABILITY_STATE_PROCESSED,
             ZR_AOT_REACHABILITY_REASON_FIELD_ACCESS,
             1u},
            {ZR_AOT_REACHABILITY_STATE_PROCESSED,
             ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION,
             ZR_AOT_REACHABILITY_NO_NODE},
    };
    static const char expected[] =
            "/* reachability.functionManifest.version = 1 */\n"
            "/* reachability.functionManifest.count = 4 */\n"
            "/* reachability.functionManifest.node[0] = reason=root.entry predecessor=none */\n"
            "/* reachability.functionManifest.node[1] = reason=edge.direct_call predecessor=0 */\n"
            "/* reachability.functionManifest.node[3] = reason=edge.field_access predecessor=1 */\n"
            "/* reachability.functionManifest.node[4] = reason=root.reflection_annotation predecessor=none */\n";
    FILE *file = tmpfile();
    char *text;

    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_TRUE(backend_aot_reachability_write_function_manifest(file, marks, 5u));
    text = test_read_stream(file);
    TEST_ASSERT_EQUAL_STRING(expected, text);

    free(text);
    fclose(file);
}

static void test_reachability_function_manifest_rejects_malformed_reason_chains(void) {
    static const SZrAotReachabilityMark pendingMarks[] = {
            {ZR_AOT_REACHABILITY_STATE_MARKED_PENDING,
             ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY,
             ZR_AOT_REACHABILITY_NO_NODE},
    };
    static const SZrAotReachabilityMark edgeAsRootMarks[] = {
            {ZR_AOT_REACHABILITY_STATE_PROCESSED,
             ZR_AOT_REACHABILITY_REASON_DIRECT_CALL,
             ZR_AOT_REACHABILITY_NO_NODE},
    };
    static const SZrAotReachabilityMark outOfRangeMarks[] = {
            {ZR_AOT_REACHABILITY_STATE_PROCESSED,
             ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY,
             ZR_AOT_REACHABILITY_NO_NODE},
            {ZR_AOT_REACHABILITY_STATE_PROCESSED,
             ZR_AOT_REACHABILITY_REASON_DIRECT_CALL,
             2u},
    };
    static const SZrAotReachabilityMark cyclicMarks[] = {
            {ZR_AOT_REACHABILITY_STATE_PROCESSED,
             ZR_AOT_REACHABILITY_REASON_DIRECT_CALL,
             1u},
            {ZR_AOT_REACHABILITY_STATE_PROCESSED,
             ZR_AOT_REACHABILITY_REASON_FIELD_ACCESS,
             0u},
    };
    static const SZrAotReachabilityMark dirtyUnmarkedMarks[] = {
            {ZR_AOT_REACHABILITY_STATE_UNMARKED,
             ZR_AOT_REACHABILITY_REASON_DIRECT_CALL,
             ZR_AOT_REACHABILITY_NO_NODE},
    };
    FILE *file = tmpfile();

    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_FALSE(backend_aot_reachability_write_function_manifest(file, pendingMarks, 1u));
    TEST_ASSERT_FALSE(backend_aot_reachability_write_function_manifest(file, edgeAsRootMarks, 1u));
    TEST_ASSERT_FALSE(backend_aot_reachability_write_function_manifest(file, outOfRangeMarks, 2u));
    TEST_ASSERT_FALSE(backend_aot_reachability_write_function_manifest(file, cyclicMarks, 2u));
    TEST_ASSERT_FALSE(backend_aot_reachability_write_function_manifest(file, dirtyUnmarkedMarks, 1u));
    TEST_ASSERT_EQUAL_INT(0, (int)ftell(file));

    fclose(file);
}

static void test_function_table_filter_keeps_reachable_entries_without_renumbering(void) {
    SZrFunction functions[4];
    SZrAotFunctionEntry entries[4] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
            {&functions[3], 3u},
    };
    SZrAotFunctionTable table = {
            entries,
            4u,
            4u,
            4u,
    };
    SZrAotReachabilityMark marks[4] = {
            {ZR_AOT_REACHABILITY_STATE_PROCESSED, ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY, ZR_AOT_REACHABILITY_NO_NODE},
            {ZR_AOT_REACHABILITY_STATE_UNMARKED, ZR_AOT_REACHABILITY_REASON_NONE, ZR_AOT_REACHABILITY_NO_NODE},
            {ZR_AOT_REACHABILITY_STATE_PROCESSED, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, 0u},
            {ZR_AOT_REACHABILITY_STATE_UNMARKED, ZR_AOT_REACHABILITY_REASON_NONE, ZR_AOT_REACHABILITY_NO_NODE},
    };
    SZrAotFunctionEntry invalidEntries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 9u},
    };
    SZrAotFunctionTable invalidTable = {
            invalidEntries,
            3u,
            3u,
            3u,
    };
    SZrAotReachabilityMark invalidMarks[3] = {
            {ZR_AOT_REACHABILITY_STATE_UNMARKED, ZR_AOT_REACHABILITY_REASON_NONE, ZR_AOT_REACHABILITY_NO_NODE},
            {ZR_AOT_REACHABILITY_STATE_PROCESSED, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, 0u},
            {ZR_AOT_REACHABILITY_STATE_PROCESSED, ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, 1u},
    };

    memset(functions, 0, sizeof(functions));
    TEST_ASSERT_TRUE(backend_aot_filter_function_table_by_reachability(&table, marks, 4u));
    TEST_ASSERT_EQUAL_UINT32(2u, table.count);
    TEST_ASSERT_EQUAL_PTR(&functions[0], table.entries[0].function);
    TEST_ASSERT_EQUAL_UINT32(0u, table.entries[0].flatIndex);
    TEST_ASSERT_EQUAL_PTR(&functions[2], table.entries[1].function);
    TEST_ASSERT_EQUAL_UINT32(2u, table.entries[1].flatIndex);
    TEST_ASSERT_EQUAL_UINT32(4u, backend_aot_function_table_index_space(&table));
    TEST_ASSERT_FALSE(backend_aot_filter_function_table_by_reachability(&table, marks, 1u));
    TEST_ASSERT_FALSE(backend_aot_filter_function_table_by_reachability(&invalidTable, invalidMarks, 3u));
    TEST_ASSERT_EQUAL_UINT32(3u, invalidTable.count);
    TEST_ASSERT_EQUAL_PTR(&functions[0], invalidTable.entries[0].function);
    TEST_ASSERT_EQUAL_UINT32(0u, invalidTable.entries[0].flatIndex);
}

static void test_static_callable_reachability_marks_get_sub_function_target(void) {
    TZrInstruction rootInstructions[1];
    SZrFunction functions[3];
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    SZrAotReachabilityMark marks[3];
    SZrAotReachabilityEdge edges[3];
    TZrUInt32 roots[3];
    EZrAotReachabilityReason rootReasons[3];
    TZrUInt32 queue[3];
    TZrUInt32 markedCount = 0u;
    TZrUInt32 edgeCount = 0u;

    memset(functions, 0, sizeof(functions));
    rootInstructions[0] = test_create_instruction_2(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), 0u, 0u, 0u);
    functions[0].instructionsList = rootInstructions;
    functions[0].instructionsLength = 1u;
    functions[0].childFunctionList = &functions[1];
    functions[0].childFunctionLength = 1u;

    TEST_ASSERT_TRUE(backend_aot_compute_static_callable_reachability(ZR_NULL,
                                                                      &table,
                                                                      ZR_NULL,
                                                                      0u,
                                                                      ZR_NULL,
                                                                      0u,
                                                                      roots,
                                                                      rootReasons,
                                                                      3u,
                                                                      marks,
                                                                      3u,
                                                                      edges,
                                                                      3u,
                                                                      queue,
                                                                      3u,
                                                                      &markedCount,
                                                                      &edgeCount));

    TEST_ASSERT_EQUAL_UINT32(1u, edgeCount);
    TEST_ASSERT_EQUAL_UINT32(0u, edges[0].source);
    TEST_ASSERT_EQUAL_UINT32(1u, edges[0].target);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, edges[0].reason);
    TEST_ASSERT_EQUAL_UINT32(2u, markedCount);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[0].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY, marks[0].reason);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[1].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_DIRECT_CALL, marks[1].reason);
    TEST_ASSERT_EQUAL_UINT32(0u, marks[1].predecessor);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_UNMARKED, marks[2].state);
    TEST_ASSERT_FALSE(backend_aot_compute_static_callable_reachability(ZR_NULL,
                                                                       &table,
                                                                       ZR_NULL,
                                                                       0u,
                                                                       ZR_NULL,
                                                                       0u,
                                                                       roots,
                                                                       rootReasons,
                                                                       3u,
                                                                       marks,
                                                                       3u,
                                                                       edges,
                                                                       0u,
                                                                       queue,
                                                                       3u,
                                                                       &markedCount,
                                                                       &edgeCount));
}

static void test_static_callable_reachability_keeps_exported_child_roots(void) {
    SZrFunction functions[3];
    SZrFunctionTopLevelCallableBinding exportedCallable;
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    SZrAotReachabilityMark marks[3];
    SZrAotReachabilityEdge edges[1];
    TZrUInt32 roots[3];
    EZrAotReachabilityReason rootReasons[3];
    TZrUInt32 queue[3];
    TZrUInt32 markedCount = 0u;
    TZrUInt32 edgeCount = 0u;

    memset(functions, 0, sizeof(functions));
    memset(&exportedCallable, 0, sizeof(exportedCallable));
    functions[0].childFunctionList = &functions[1];
    functions[0].childFunctionLength = 2u;
    functions[1].lineInSourceStart = 10u;
    functions[1].lineInSourceEnd = 10u;
    functions[2].lineInSourceStart = 20u;
    functions[2].lineInSourceEnd = 20u;
    exportedCallable.callableChildIndex = 1u;
    exportedCallable.exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    functions[0].topLevelCallableBindings = &exportedCallable;
    functions[0].topLevelCallableBindingLength = 1u;

    TEST_ASSERT_TRUE(backend_aot_compute_static_callable_reachability(ZR_NULL,
                                                                      &table,
                                                                      ZR_NULL,
                                                                      0u,
                                                                      ZR_NULL,
                                                                      0u,
                                                                      roots,
                                                                      rootReasons,
                                                                      3u,
                                                                      marks,
                                                                      3u,
                                                                      edges,
                                                                      1u,
                                                                      queue,
                                                                      3u,
                                                                      &markedCount,
                                                                      &edgeCount));

    TEST_ASSERT_EQUAL_UINT32(0u, edgeCount);
    TEST_ASSERT_EQUAL_UINT32(2u, markedCount);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[0].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY, marks[0].reason);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_UNMARKED, marks[1].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[2].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_ROOT_EXPORT, marks[2].reason);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_REACHABILITY_NO_NODE, marks[2].predecessor);
}

static void test_static_callable_reachability_keeps_reflection_annotation_roots(void) {
    SZrFunction functions[3];
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    static const TZrUInt32 annotationRoots[] = {2u};
    SZrAotReachabilityMark marks[3];
    SZrAotReachabilityEdge edges[1];
    TZrUInt32 roots[3];
    EZrAotReachabilityReason rootReasons[3];
    TZrUInt32 queue[3];
    TZrUInt32 markedCount = 0u;
    TZrUInt32 edgeCount = 0u;

    memset(functions, 0, sizeof(functions));

    TEST_ASSERT_TRUE(backend_aot_compute_static_callable_reachability(ZR_NULL,
                                                                      &table,
                                                                      annotationRoots,
                                                                      1u,
                                                                      ZR_NULL,
                                                                      0u,
                                                                      roots,
                                                                      rootReasons,
                                                                      3u,
                                                                      marks,
                                                                      3u,
                                                                      edges,
                                                                      1u,
                                                                      queue,
                                                                      3u,
                                                                      &markedCount,
                                                                      &edgeCount));

    TEST_ASSERT_EQUAL_UINT32(0u, edgeCount);
    TEST_ASSERT_EQUAL_UINT32(2u, markedCount);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[0].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY, marks[0].reason);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_UNMARKED, marks[1].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[2].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION, marks[2].reason);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_REACHABILITY_NO_NODE, marks[2].predecessor);
}

static void test_collect_reflection_annotation_roots_keeps_dynamic_dependency_function_index(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction functions[3];
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    TZrUInt32 annotationRoots[3];
    TZrUInt32 annotationRootCount = 0u;

    TEST_ASSERT_NOT_NULL(state);
    memset(functions, 0, sizeof(functions));
    mark_function_metadata_uint(state,
                                &functions[0],
                                "dynamicDependencyFunctionIndex",
                                2u);

    TEST_ASSERT_TRUE(backend_aot_collect_reflection_annotation_roots(state,
                                                                     &table,
                                                                     annotationRoots,
                                                                     3u,
                                                                     &annotationRootCount));
    TEST_ASSERT_EQUAL_UINT32(1u, annotationRootCount);
    TEST_ASSERT_EQUAL_UINT32(2u, annotationRoots[0]);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_collect_reflection_annotation_roots_keeps_dynamic_dependency_method_token(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const TZrMetadataToken methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7u);
    SZrFunction functions[3];
    SZrFunctionTypedExportSymbol exportedSymbol;
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    TZrUInt32 annotationRoots[3];
    TZrUInt32 annotationRootCount = 0u;

    TEST_ASSERT_NOT_NULL(state);
    memset(functions, 0, sizeof(functions));
    functions[0].childFunctionList = &functions[1];
    functions[0].childFunctionLength = 2u;
    functions[0].lineInSourceStart = 1u;
    functions[0].lineInSourceEnd = 1u;
    functions[2].lineInSourceStart = 20u;
    functions[2].lineInSourceEnd = 20u;
    attach_typed_exported_method_token(&functions[0], &exportedSymbol, 1u, methodToken);
    mark_function_metadata_uint(state,
                                &functions[0],
                                "dynamicDependencyMethodToken",
                                methodToken);

    TEST_ASSERT_TRUE(backend_aot_collect_reflection_annotation_roots(state,
                                                                     &table,
                                                                     annotationRoots,
                                                                     3u,
                                                                     &annotationRootCount));
    TEST_ASSERT_EQUAL_UINT32(1u, annotationRootCount);
    TEST_ASSERT_EQUAL_UINT32(2u, annotationRoots[0]);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_collect_reflection_annotation_roots_keeps_non_exported_dynamic_dependency_method_token(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    const TZrMetadataToken methodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 8u);
    SZrFunction functions[3];
    SZrFunctionTypedExportSymbol methodSymbol;
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    TZrUInt32 annotationRoots[3];
    TZrUInt32 annotationRootCount = 0u;

    TEST_ASSERT_NOT_NULL(state);
    memset(functions, 0, sizeof(functions));
    functions[0].childFunctionList = &functions[1];
    functions[0].childFunctionLength = 2u;
    functions[0].lineInSourceStart = 1u;
    functions[0].lineInSourceEnd = 1u;
    functions[2].lineInSourceStart = 20u;
    functions[2].lineInSourceEnd = 20u;
    attach_typed_method_token(&functions[0],
                              &methodSymbol,
                              1u,
                              methodToken,
                              ZR_MODULE_EXPORT_KIND_VALUE);
    mark_function_metadata_uint(state,
                                &functions[0],
                                "dynamicDependencyMethodToken",
                                methodToken);

    TEST_ASSERT_TRUE(backend_aot_collect_reflection_annotation_roots(state,
                                                                     &table,
                                                                     annotationRoots,
                                                                     3u,
                                                                     &annotationRootCount));
    TEST_ASSERT_EQUAL_UINT32(1u, annotationRootCount);
    TEST_ASSERT_EQUAL_UINT32(2u, annotationRoots[0]);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_collect_reflection_annotation_roots_keeps_dynamic_dependency_method_name(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction functions[3];
    SZrFunctionTypedExportSymbol exportedSymbol;
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    TZrUInt32 annotationRoots[3];
    TZrUInt32 annotationRootCount = 0u;

    TEST_ASSERT_NOT_NULL(state);
    memset(functions, 0, sizeof(functions));
    functions[0].childFunctionList = &functions[1];
    functions[0].childFunctionLength = 2u;
    functions[0].lineInSourceStart = 1u;
    functions[0].lineInSourceEnd = 1u;
    functions[2].lineInSourceStart = 20u;
    functions[2].lineInSourceEnd = 20u;
    attach_typed_exported_method_name(state, &functions[0], &exportedSymbol, 1u, "target");
    mark_function_metadata_string(state,
                                  &functions[0],
                                  "dynamicDependencyMethodName",
                                  "target");

    TEST_ASSERT_TRUE(backend_aot_collect_reflection_annotation_roots(state,
                                                                     &table,
                                                                     annotationRoots,
                                                                     3u,
                                                                     &annotationRootCount));
    TEST_ASSERT_EQUAL_UINT32(1u, annotationRootCount);
    TEST_ASSERT_EQUAL_UINT32(2u, annotationRoots[0]);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_collect_reflection_annotation_roots_keeps_dynamic_dependency_method_name_signature_hash(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction functions[3];
    SZrFunctionTypedExportSymbol exportedSymbols[2];
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    TZrUInt32 annotationRoots[3];
    TZrUInt32 annotationRootCount = 0u;

    TEST_ASSERT_NOT_NULL(state);
    memset(functions, 0, sizeof(functions));
    functions[0].childFunctionList = &functions[1];
    functions[0].childFunctionLength = 2u;
    functions[0].lineInSourceStart = 1u;
    functions[0].lineInSourceEnd = 1u;
    functions[1].lineInSourceStart = 10u;
    functions[1].lineInSourceEnd = 10u;
    functions[2].lineInSourceStart = 20u;
    functions[2].lineInSourceEnd = 20u;
    init_typed_exported_method_name(state, &exportedSymbols[0], 0u, "target", 0x1111u);
    init_typed_exported_method_name(state, &exportedSymbols[1], 1u, "target", 0x2222u);
    functions[0].typedExportedSymbols = exportedSymbols;
    functions[0].typedExportedSymbolLength = 2u;
    mark_function_metadata_string(state,
                                  &functions[0],
                                  "dynamicDependencyMethodName",
                                  "target");
    mark_function_metadata_uint(state,
                                &functions[0],
                                "dynamicDependencyMethodSignatureHash",
                                0x2222u);

    TEST_ASSERT_TRUE(backend_aot_collect_reflection_annotation_roots(state,
                                                                     &table,
                                                                     annotationRoots,
                                                                     3u,
                                                                     &annotationRootCount));
    TEST_ASSERT_EQUAL_UINT32(1u, annotationRootCount);
    TEST_ASSERT_EQUAL_UINT32(2u, annotationRoots[0]);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_collect_reflection_annotation_roots_rejects_ambiguous_dynamic_dependency_method_name(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction functions[3];
    SZrFunctionTypedExportSymbol exportedSymbols[2];
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    TZrUInt32 annotationRoots[3];
    TZrUInt32 annotationRootCount = 0u;

    TEST_ASSERT_NOT_NULL(state);
    memset(functions, 0, sizeof(functions));
    functions[0].childFunctionList = &functions[1];
    functions[0].childFunctionLength = 2u;
    init_typed_exported_method_name(state, &exportedSymbols[0], 0u, "target", 0x1111u);
    init_typed_exported_method_name(state, &exportedSymbols[1], 1u, "target", 0x2222u);
    functions[0].typedExportedSymbols = exportedSymbols;
    functions[0].typedExportedSymbolLength = 2u;
    mark_function_metadata_string(state,
                                  &functions[0],
                                  "dynamicDependencyMethodName",
                                  "target");

    TEST_ASSERT_FALSE(backend_aot_collect_reflection_annotation_roots(state,
                                                                      &table,
                                                                      annotationRoots,
                                                                      3u,
                                                                      &annotationRootCount));

    ZrTests_Runtime_State_Destroy(state);
}

static void test_collect_reflection_annotation_roots_keeps_zero_dynamic_dependency_method_signature_hash(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction functions[3];
    SZrFunctionTypedExportSymbol exportedSymbols[2];
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    TZrUInt32 annotationRoots[3];
    TZrUInt32 annotationRootCount = 0u;

    TEST_ASSERT_NOT_NULL(state);
    memset(functions, 0, sizeof(functions));
    functions[0].childFunctionList = &functions[1];
    functions[0].childFunctionLength = 2u;
    functions[0].lineInSourceStart = 1u;
    functions[0].lineInSourceEnd = 1u;
    functions[1].lineInSourceStart = 10u;
    functions[1].lineInSourceEnd = 10u;
    functions[2].lineInSourceStart = 20u;
    functions[2].lineInSourceEnd = 20u;
    init_typed_exported_method_name(state, &exportedSymbols[0], 0u, "target", 0x1111u);
    init_typed_exported_method_name(state, &exportedSymbols[1], 1u, "target", 0u);
    functions[0].typedExportedSymbols = exportedSymbols;
    functions[0].typedExportedSymbolLength = 2u;
    mark_function_metadata_string(state,
                                  &functions[0],
                                  "dynamicDependencyMethodName",
                                  "target");
    mark_function_metadata_uint(state,
                                &functions[0],
                                "dynamicDependencyMethodSignatureHash",
                                0u);

    TEST_ASSERT_TRUE(backend_aot_collect_reflection_annotation_roots(state,
                                                                     &table,
                                                                     annotationRoots,
                                                                     3u,
                                                                     &annotationRootCount));
    TEST_ASSERT_EQUAL_UINT32(1u, annotationRootCount);
    TEST_ASSERT_EQUAL_UINT32(2u, annotationRoots[0]);

    ZrTests_Runtime_State_Destroy(state);
}

static void test_static_callable_reachability_keeps_manifest_function_roots(void) {
    SZrFunction functions[3];
    SZrAotFunctionEntry entries[3] = {
            {&functions[0], 0u},
            {&functions[1], 1u},
            {&functions[2], 2u},
    };
    SZrAotFunctionTable table = {
            entries,
            3u,
            3u,
            3u,
    };
    static const TZrUInt32 manifestRoots[] = {2u};
    static const TZrUInt32 invalidManifestRoots[] = {7u};
    SZrAotReachabilityMark marks[3];
    SZrAotReachabilityEdge edges[1];
    TZrUInt32 roots[3];
    EZrAotReachabilityReason rootReasons[3];
    TZrUInt32 queue[3];
    TZrUInt32 markedCount = 0u;
    TZrUInt32 edgeCount = 0u;

    memset(functions, 0, sizeof(functions));

    TEST_ASSERT_TRUE(backend_aot_compute_static_callable_reachability(ZR_NULL,
                                                                      &table,
                                                                      ZR_NULL,
                                                                      0u,
                                                                      manifestRoots,
                                                                      1u,
                                                                      roots,
                                                                      rootReasons,
                                                                      3u,
                                                                      marks,
                                                                      3u,
                                                                      edges,
                                                                      1u,
                                                                      queue,
                                                                      3u,
                                                                      &markedCount,
                                                                      &edgeCount));

    TEST_ASSERT_EQUAL_UINT32(0u, edgeCount);
    TEST_ASSERT_EQUAL_UINT32(2u, markedCount);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[0].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY, marks[0].reason);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_UNMARKED, marks[1].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_STATE_PROCESSED, marks[2].state);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_REACHABILITY_REASON_MANIFEST, marks[2].reason);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_REACHABILITY_NO_NODE, marks[2].predecessor);
    TEST_ASSERT_FALSE(backend_aot_compute_static_callable_reachability(ZR_NULL,
                                                                       &table,
                                                                       ZR_NULL,
                                                                       0u,
                                                                       invalidManifestRoots,
                                                                       1u,
                                                                       roots,
                                                                       rootReasons,
                                                                       3u,
                                                                       marks,
                                                                       3u,
                                                                       edges,
                                                                       1u,
                                                                       queue,
                                                                       3u,
                                                                       &markedCount,
                                                                       &edgeCount));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reachability_marks_roots_and_direct_dependencies);
    RUN_TEST(test_reachability_preserves_root_reason_and_rejects_invalid_graphs);
    RUN_TEST(test_reachability_function_manifest_is_stable_and_preserves_reason_chain);
    RUN_TEST(test_reachability_function_manifest_rejects_malformed_reason_chains);
    RUN_TEST(test_function_table_filter_keeps_reachable_entries_without_renumbering);
    RUN_TEST(test_static_callable_reachability_marks_get_sub_function_target);
    RUN_TEST(test_static_callable_reachability_keeps_exported_child_roots);
    RUN_TEST(test_static_callable_reachability_keeps_reflection_annotation_roots);
    RUN_TEST(test_collect_reflection_annotation_roots_keeps_dynamic_dependency_function_index);
    RUN_TEST(test_collect_reflection_annotation_roots_keeps_dynamic_dependency_method_token);
    RUN_TEST(test_collect_reflection_annotation_roots_keeps_non_exported_dynamic_dependency_method_token);
    RUN_TEST(test_collect_reflection_annotation_roots_keeps_dynamic_dependency_method_name);
    RUN_TEST(test_collect_reflection_annotation_roots_keeps_dynamic_dependency_method_name_signature_hash);
    RUN_TEST(test_collect_reflection_annotation_roots_rejects_ambiguous_dynamic_dependency_method_name);
    RUN_TEST(test_collect_reflection_annotation_roots_keeps_zero_dynamic_dependency_method_signature_hash);
    RUN_TEST(test_static_callable_reachability_keeps_manifest_function_roots);
    return UNITY_END();
}
