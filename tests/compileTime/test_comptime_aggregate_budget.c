#include "unity.h"

#include <string.h>

#include "compiler/tool/compile_tool_binding.h"
#include "compiler/compile_time/compile_time_aggregate.h"
#include "runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static SZrAstNode *g_ast;

void setUp(void) {
    g_ast = ZR_NULL;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compiler.suppressErrorOutput = ZR_TRUE;
}

void tearDown(void) {
    ZrParser_CompilerState_Free(&g_compiler);
    if (g_ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_ast);
        g_ast = ZR_NULL;
    }
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static SZrAstNode *parse_literal(TZrBool isArray) {
    const TZrChar *source = isArray ? "var probe = [];" : "var probe = {};";
    SZrString *name = ZrCore_String_CreateFromNative(
            g_state, "comptime_aggregate_budget.zr");
    SZrParserState parser;
    TZrBool hadError;
    SZrAstNode *declaration;

    TEST_ASSERT_NOT_NULL(name);
    ZrParser_State_Init(&parser, g_state, source, strlen(source), name);
    parser.suppressErrorOutput = ZR_TRUE;
    g_ast = ZrParser_ParseWithState(&parser);
    hadError = parser.hasError;
    ZrParser_State_Free(&parser);
    TEST_ASSERT_FALSE_MESSAGE(hadError, "Aggregate budget fixture must parse");
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(1U, g_ast->data.script.statements->count);
    declaration = g_ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(isArray ? ZR_AST_ARRAY_LITERAL : ZR_AST_OBJECT_LITERAL,
                          declaration->data.variableDeclaration.value->type);
    return declaration->data.variableDeclaration.value;
}

static TZrSize object_count(void) {
    TZrSize count = 0U;
    for (SZrRawObject *raw = g_state->global->garbageCollector->gcObjectList;
         raw != ZR_NULL; raw = raw->next) {
        if (raw->type == ZR_RAW_OBJECT_TYPE_OBJECT ||
            raw->type == ZR_RAW_OBJECT_TYPE_ARRAY) {
            count++;
        }
    }
    return count;
}

static void assert_rejected_reservation(TZrBool isArray, TZrBool rejectHeap) {
    SZrAstNode *literal = parse_literal(isArray);
    SZrTypeValue result;
    TZrUInt64 beforeHeap;
    TZrSize beforeRoots = ZrCore_GcDomain_GetRootCount(g_state);
    TZrSize beforeObjects = object_count();
    EZrParserComptimeBudgetResource resource = rejectHeap
            ? ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES
            : ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT;

    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsume(
            &g_compiler.comptimeBudget, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
            7U));
    beforeHeap = g_compiler.comptimeBudget.usage.heapBytes;
    g_compiler.comptimeBudget.limits.heapBytes =
            beforeHeap + sizeof(SZrObject) - (rejectHeap ? 1U : 0U);
    g_compiler.comptimeBudget.limits.aggregateCount = rejectHeap ? 2U : 0U;
    ZrCore_Value_ResetAsNull(&result);

    TEST_ASSERT_FALSE(ZrParser_Compiler_EvaluateCompileTimeExpression(
            &g_compiler, literal, &result));
    TEST_ASSERT_TRUE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_INT(resource, g_compiler.comptimeBudget.exceededResource);
    TEST_ASSERT_EQUAL_UINT64(rejectHeap ? beforeHeap + sizeof(SZrObject) : 1U,
                             g_compiler.comptimeBudget.requestedUsage);
    TEST_ASSERT_EQUAL_UINT64(rejectHeap
            ? g_compiler.comptimeBudget.limits.heapBytes : 0U,
            g_compiler.comptimeBudget.exceededLimit);
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage,
            rejectHeap ? "heap_bytes requested=" : "aggregate_count requested=1 limit=0"));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result.type));
    TEST_ASSERT_EQUAL_UINT64(beforeRoots, ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_EQUAL_UINT64(beforeObjects, object_count());
    TEST_ASSERT_EQUAL_UINT64(0U, g_compiler.comptimeBudget.usage.aggregateCount);
    TEST_ASSERT_EQUAL_UINT64(1U, g_compiler.comptimeBudget.usage.fuel);
    TEST_ASSERT_EQUAL_UINT64(0U, g_compiler.comptimeBudget.usage.diagnosticCount);
    TEST_ASSERT_EQUAL_UINT64(beforeHeap, g_compiler.comptimeBudget.usage.heapBytes);
}

static void assert_success_keeps_cumulative_usage(TZrBool isArray) {
    SZrAstNode *literal = parse_literal(isArray);
    SZrTypeValue result;
    TZrSize beforeRoots = ZrCore_GcDomain_GetRootCount(g_state);
    const TZrUInt64 frameBytes =
            ZR_PARSER_INITIAL_CAPACITY_TINY * sizeof(SZrGcRootHandle);
    const TZrUInt64 evaluationBytes = sizeof(SZrObject) + frameBytes;

    g_compiler.comptimeBudget.limits.heapBytes = 2U * evaluationBytes;
    g_compiler.comptimeBudget.limits.aggregateCount = 4U;
    for (TZrUInt64 index = 1U; index <= 2U; index++) {
        ZrCore_Value_ResetAsNull(&result);
        TEST_ASSERT_TRUE(ZrParser_Compiler_EvaluateCompileTimeExpression(
                &g_compiler, literal, &result));
        TEST_ASSERT_EQUAL_INT(isArray ? ZR_VALUE_TYPE_ARRAY : ZR_VALUE_TYPE_OBJECT,
                              result.type);
        TEST_ASSERT_NOT_NULL(result.value.object);
        TEST_ASSERT_EQUAL_UINT64(index * evaluationBytes,
                                 g_compiler.comptimeBudget.usage.heapBytes);
        TEST_ASSERT_EQUAL_UINT64(index * sizeof(SZrObject),
                                 g_compiler.comptimeBudget.usage.heapBytes - index * frameBytes);
        TEST_ASSERT_EQUAL_UINT64(2U * index,
                                 g_compiler.comptimeBudget.usage.aggregateCount);
        TEST_ASSERT_EQUAL_UINT64(index, g_compiler.comptimeBudget.usage.fuel);
        TEST_ASSERT_EQUAL_UINT64(beforeRoots,
                                 ZrCore_GcDomain_GetRootCount(g_state));
    }
    TEST_ASSERT_FALSE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_UINT64(0U, g_compiler.comptimeBudget.usage.diagnosticCount);
    ZrCore_Value_ResetAsNull(&result);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT64(2U * evaluationBytes,
                             g_compiler.comptimeBudget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(4U, g_compiler.comptimeBudget.usage.aggregateCount);
    TEST_ASSERT_EQUAL_UINT64(beforeRoots, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_rejected_object_aggregate_slot_has_no_partial_heap_charge(void) {
    assert_rejected_reservation(ZR_FALSE, ZR_FALSE);
}

static void test_rejected_array_aggregate_slot_has_no_partial_heap_charge(void) {
    assert_rejected_reservation(ZR_TRUE, ZR_FALSE);
}

static void test_rejected_object_heap_has_no_partial_aggregate_charge(void) {
    assert_rejected_reservation(ZR_FALSE, ZR_TRUE);
}

static void test_rejected_array_heap_has_no_partial_aggregate_charge(void) {
    assert_rejected_reservation(ZR_TRUE, ZR_TRUE);
}

static void test_successful_objects_keep_usage_after_scratch_free_and_gc(void) {
    assert_success_keeps_cumulative_usage(ZR_FALSE);
}

static void test_successful_arrays_keep_usage_after_scratch_free_and_gc(void) {
    assert_success_keeps_cumulative_usage(ZR_TRUE);
}

static void assert_identifier(SZrAstNode *node, const TZrChar *expected) {
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, node->type);
    TEST_ASSERT_NOT_NULL(node->data.identifier.name);
    TEST_ASSERT_EQUAL_STRING(expected,
            ZrCore_String_GetNativeString(node->data.identifier.name));
}

static SZrAstNode *parse_typed_initializer(TZrBool isPatch) {
    const TZrChar *source = isPatch
            ? "let declaration = import(\"zr.compile.declaration\");\n"
              "var probe = init declaration.Patch(target: 1);"
            : "let declaration = import(\"zr.compile.declaration\");\n"
              "var probe = init declaration.CompileDiagnostic("
              "isError: false, message: \"probe\", target: 1);";
    const TZrChar *typeName = isPatch ? "Patch" : "CompileDiagnostic";
    SZrString *name = ZrCore_String_CreateFromNative(
            g_state, "comptime_typed_aggregate_budget.zr");
    SZrParserState parser;
    TZrBool hadError;
    SZrAstNode *importDeclaration;
    SZrAstNode *importExpression;
    SZrAstNode *declaration;
    SZrAstNode *node;
    SZrStructInitExpression *init;
    const SZrCompileToolBinding *binding;
    const SZrParserCompileToolTypeDescriptor *descriptor;

    TEST_ASSERT_NOT_NULL(name);
    ZrParser_State_Init(&parser, g_state, source, strlen(source), name);
    parser.suppressErrorOutput = ZR_TRUE;
    g_ast = ZrParser_ParseWithState(&parser);
    hadError = parser.hasError;
    ZrParser_State_Free(&parser);
    TEST_ASSERT_FALSE_MESSAGE(hadError, "Typed aggregate fixture must parse");
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_NOT_NULL(g_ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT64(2U, g_ast->data.script.statements->count);
    importDeclaration = g_ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(importDeclaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, importDeclaration->type);
    assert_identifier(importDeclaration->data.variableDeclaration.pattern,
                      "declaration");
    importExpression = importDeclaration->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(importExpression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IMPORT_EXPRESSION, importExpression->type);
    TEST_ASSERT_NOT_NULL(importExpression->data.importExpression.modulePath);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRING_LITERAL,
            importExpression->data.importExpression.modulePath->type);
    TEST_ASSERT_EQUAL_STRING("zr.compile.declaration",
            ZrCore_String_GetNativeString(importExpression->data.importExpression
                    .modulePath->data.stringLiteral.value));
    declaration = g_ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    assert_identifier(declaration->data.variableDeclaration.pattern, "probe");
    node = declaration->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_INIT_EXPRESSION, node->type);
    TEST_ASSERT_EQUAL_PTR(name, node->location.source);
    TEST_ASSERT_GREATER_THAN_INT(0, node->location.start.line);
    init = &node->data.structInitExpression;
    TEST_ASSERT_NOT_NULL(init->typeInfo);
    assert_identifier(init->typeInfo->name, "declaration");
    TEST_ASSERT_NOT_NULL(init->typeInfo->subType);
    assert_identifier(init->typeInfo->subType->name, typeName);
    TEST_ASSERT_NULL(init->typeInfo->subType->subType);
    TEST_ASSERT_TRUE(init->hasNamedArgs);
    TEST_ASSERT_NOT_NULL(init->args);
    TEST_ASSERT_NOT_NULL(init->argNames);
    TEST_ASSERT_EQUAL_UINT64(isPatch ? 1U : 3U, init->args->count);
    TEST_ASSERT_EQUAL_UINT64(init->args->count, init->argNames->length);
    for (TZrSize index = 0U; index < init->args->count; index++) {
        SZrString **argName = (SZrString **)ZrCore_Array_Get(init->argNames, index);
        SZrAstNode *argument = init->args->nodes[index];
        const TZrChar *expectedName = !isPatch && index == 0U ? "isError"
                : !isPatch && index == 1U ? "message" : "target";
        TEST_ASSERT_NOT_NULL(argName);
        TEST_ASSERT_NOT_NULL(*argName);
        TEST_ASSERT_EQUAL_STRING(expectedName, ZrCore_String_GetNativeString(*argName));
        TEST_ASSERT_NOT_NULL(argument);
        if (!isPatch && index == 0U) {
            TEST_ASSERT_EQUAL_INT(ZR_AST_BOOLEAN_LITERAL, argument->type);
            TEST_ASSERT_FALSE(argument->data.booleanLiteral.value);
        } else if (!isPatch && index == 1U) {
            TEST_ASSERT_EQUAL_INT(ZR_AST_STRING_LITERAL, argument->type);
            TEST_ASSERT_EQUAL_STRING("probe", ZrCore_String_GetNativeString(
                    argument->data.stringLiteral.value));
        } else {
            TEST_ASSERT_EQUAL_INT(ZR_AST_INTEGER_LITERAL, argument->type);
            TEST_ASSERT_EQUAL_INT64(1, argument->data.integerLiteral.value);
        }
    }

    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &g_compiler, g_ast));
    binding = ZrParser_CompileToolBinding_Resolve(
            &g_compiler, init->typeInfo->name->data.identifier.name);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_INT(ZR_COMPILE_TOOL_BINDING_PROVIDER, binding->kind);
    TEST_ASSERT_NOT_NULL(binding->provider);
    TEST_ASSERT_EQUAL_STRING("zr.compile.declaration", binding->provider->moduleName);
    descriptor = ZrParser_CompileTool_FindType(binding->provider, typeName);
    TEST_ASSERT_NOT_NULL(descriptor);
    TEST_ASSERT_EQUAL_INT(isPatch ? ZR_PARSER_COMPILE_TOOL_TYPE_PATCH
                                 : ZR_PARSER_COMPILE_TOOL_TYPE_DIAGNOSTIC,
                          descriptor->role);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPILE_PHASE_EXPANSION,
                          descriptor->minimumPhase);
    g_compiler.comptimeContext = ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM;
    g_compiler.compilePhase = ZR_PARSER_COMPILE_PHASE_EXPANSION;
    return node;
}

static void assert_error_location(SZrAstNode *node) {
    TEST_ASSERT_EQUAL_PTR(node->location.source, g_compiler.errorLocation.source);
    TEST_ASSERT_EQUAL_UINT64(node->location.start.offset, g_compiler.errorLocation.start.offset);
    TEST_ASSERT_EQUAL_INT(node->location.start.line, g_compiler.errorLocation.start.line);
    TEST_ASSERT_EQUAL_INT(node->location.start.column, g_compiler.errorLocation.start.column);
    TEST_ASSERT_EQUAL_UINT64(node->location.end.offset, g_compiler.errorLocation.end.offset);
    TEST_ASSERT_EQUAL_INT(node->location.end.line, g_compiler.errorLocation.end.line);
    TEST_ASSERT_EQUAL_INT(node->location.end.column, g_compiler.errorLocation.end.column);
}

static const SZrTypeValue *get_field_without_allocation(
        const SZrObject *object, const TZrChar *name) {
    for (TZrSize index = 0U; index < object->nodeMap.capacity; index++) {
        for (const SZrHashKeyValuePair *pair = object->nodeMap.buckets[index];
             pair != ZR_NULL; pair = pair->next) {
            if (pair->key.type == ZR_VALUE_TYPE_STRING &&
                pair->key.value.object != ZR_NULL &&
                strcmp(ZrCore_String_GetNativeString(
                        (SZrString *)pair->key.value.object), name) == 0) {
                return &pair->value;
            }
        }
    }
    return ZR_NULL;
}

static TZrSize field_count(const SZrObject *object) {
    TZrSize count = 0U;
    for (TZrSize index = 0U; index < object->nodeMap.capacity; index++) {
        for (const SZrHashKeyValuePair *pair = object->nodeMap.buckets[index];
             pair != ZR_NULL; pair = pair->next) {
            count++;
        }
    }
    return count;
}

static void assert_typed_rejected_reservation(TZrBool isPatch) {
    SZrAstNode *node = parse_typed_initializer(isPatch);
    SZrParserComptimeBudgetUsage before;
    SZrTypeValue result;
    TZrSize beforeRoots = ZrCore_GcDomain_GetRootCount(g_state);
    TZrSize beforeObjects = object_count();
    TZrUInt64 successfulHeaders = isPatch ? 1U : 0U;

    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsume(
            &g_compiler.comptimeBudget, ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES, 7U));
    TEST_ASSERT_TRUE(ZrParser_ComptimeBudget_TryConsume(
            &g_compiler.comptimeBudget, ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT, 3U));
    before = g_compiler.comptimeBudget.usage;
    g_compiler.comptimeBudget.limits.heapBytes = before.heapBytes + 2U * sizeof(SZrObject);
    g_compiler.comptimeBudget.limits.aggregateCount = before.aggregateCount + successfulHeaders;
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_FALSE(ZrParser_Compiler_EvaluateCompileTimeExpression(
            &g_compiler, node, &result));
    TEST_ASSERT_TRUE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT,
                          g_compiler.comptimeBudget.exceededResource);
    TEST_ASSERT_EQUAL_UINT64(before.aggregateCount + successfulHeaders + 1U,
                             g_compiler.comptimeBudget.requestedUsage);
    TEST_ASSERT_EQUAL_UINT64(before.aggregateCount + successfulHeaders,
                             g_compiler.comptimeBudget.exceededLimit);
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, "aggregate_count requested="));
    assert_error_location(node);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result.type));
    TEST_ASSERT_EQUAL_UINT64(beforeRoots, ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_EQUAL_UINT64(beforeObjects + successfulHeaders, object_count());
    TEST_ASSERT_EQUAL_UINT64(before.aggregateCount + successfulHeaders,
                             g_compiler.comptimeBudget.usage.aggregateCount);
    TEST_ASSERT_EQUAL_UINT64(before.fuel + (isPatch ? 2U : 1U),
                             g_compiler.comptimeBudget.usage.fuel);
    TEST_ASSERT_EQUAL_UINT64(before.diagnosticCount,
                             g_compiler.comptimeBudget.usage.diagnosticCount);
    TEST_ASSERT_EQUAL_UINT64(before.heapBytes + successfulHeaders * sizeof(SZrObject),
                             g_compiler.comptimeBudget.usage.heapBytes);
}

static void assert_typed_success_keeps_cumulative_usage(TZrBool isPatch) {
    static const TZrChar *const arrayFields[] = {
            "additions", "interfaceAdds", "attributeAdds", "diagnostics"};
    SZrAstNode *node = parse_typed_initializer(isPatch);
    SZrParserComptimeBudgetUsage before = g_compiler.comptimeBudget.usage;
    SZrTypeValue result;
    TZrSize beforeRoots = ZrCore_GcDomain_GetRootCount(g_state);
    TZrUInt64 headers = isPatch ? 5U : 1U;
    TZrUInt64 aggregates = isPatch ? 6U : 3U;
    TZrUInt64 fuel = isPatch ? 2U : 4U;
    const TZrUInt64 frameBytes =
            ZR_PARSER_INITIAL_CAPACITY_TINY * sizeof(SZrGcRootHandle);
    const TZrUInt64 evaluationBytes = headers * sizeof(SZrObject) + frameBytes;

    g_compiler.comptimeBudget.limits.heapBytes = before.heapBytes + 2U * evaluationBytes;
    g_compiler.comptimeBudget.limits.aggregateCount = before.aggregateCount + 2U * aggregates;
    for (TZrUInt64 index = 1U; index <= 2U; index++) {
        const SZrTypeValue *value;
        const SZrObject *object;
        ZrCore_Value_ResetAsNull(&result);
        TEST_ASSERT_TRUE(ZrParser_Compiler_EvaluateCompileTimeExpression(
                &g_compiler, node, &result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, result.type);
        TEST_ASSERT_NOT_NULL(result.value.object);
        TEST_ASSERT_EQUAL_INT(ZR_RAW_OBJECT_TYPE_OBJECT, result.value.object->type);
        object = (const SZrObject *)result.value.object;
        TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_OBJECT, object->internalType);
        TEST_ASSERT_EQUAL_UINT64(isPatch ? 6U : 4U, field_count(object));
        value = get_field_without_allocation(object, "__zrCompileToolTypeRole");
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(value->type));
        TEST_ASSERT_EQUAL_INT64(isPatch ? ZR_PARSER_COMPILE_TOOL_TYPE_PATCH
                                       : ZR_PARSER_COMPILE_TOOL_TYPE_DIAGNOSTIC,
                                value->value.nativeObject.nativeInt64);
        value = get_field_without_allocation(object, "target");
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(value->type));
        TEST_ASSERT_EQUAL_INT64(1, value->value.nativeObject.nativeInt64);
        if (isPatch) {
            for (TZrSize field = 0U; field < sizeof(arrayFields) / sizeof(arrayFields[0]); field++) {
                const SZrObject *array;
                value = get_field_without_allocation(object, arrayFields[field]);
                TEST_ASSERT_NOT_NULL(value);
                TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, value->type);
                TEST_ASSERT_NOT_NULL(value->value.object);
                TEST_ASSERT_EQUAL_INT(ZR_RAW_OBJECT_TYPE_ARRAY, value->value.object->type);
                array = (const SZrObject *)value->value.object;
                TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_ARRAY, array->internalType);
                TEST_ASSERT_EQUAL_UINT64(0U, field_count(array));
            }
        } else {
            value = get_field_without_allocation(object, "isError");
            TEST_ASSERT_NOT_NULL(value);
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, value->type);
            TEST_ASSERT_FALSE(value->value.nativeObject.nativeBool);
            value = get_field_without_allocation(object, "message");
            TEST_ASSERT_NOT_NULL(value);
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, value->type);
            TEST_ASSERT_NOT_NULL(value->value.object);
            TEST_ASSERT_EQUAL_STRING("probe", ZrCore_String_GetNativeString(
                    (SZrString *)value->value.object));
        }
        TEST_ASSERT_EQUAL_UINT64(before.heapBytes + index * evaluationBytes,
                                 g_compiler.comptimeBudget.usage.heapBytes);
        TEST_ASSERT_EQUAL_UINT64(index * headers * sizeof(SZrObject),
                                 g_compiler.comptimeBudget.usage.heapBytes -
                                 before.heapBytes - index * frameBytes);
        TEST_ASSERT_EQUAL_UINT64(before.aggregateCount + index * aggregates,
                                 g_compiler.comptimeBudget.usage.aggregateCount);
        TEST_ASSERT_EQUAL_UINT64(before.fuel + index * fuel, g_compiler.comptimeBudget.usage.fuel);
        TEST_ASSERT_EQUAL_UINT64(beforeRoots, ZrCore_GcDomain_GetRootCount(g_state));
    }
    TEST_ASSERT_FALSE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_EQUAL_UINT64(before.diagnosticCount,
                             g_compiler.comptimeBudget.usage.diagnosticCount);
    ZrCore_Value_ResetAsNull(&result);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT64(before.heapBytes + 2U * evaluationBytes,
                             g_compiler.comptimeBudget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(before.aggregateCount + 2U * aggregates,
                             g_compiler.comptimeBudget.usage.aggregateCount);
    TEST_ASSERT_EQUAL_UINT64(beforeRoots, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_typed_diagnostic_rejection_has_no_partial_heap_charge(void) {
    assert_typed_rejected_reservation(ZR_FALSE);
}

static void test_typed_patch_array_rejection_keeps_only_successful_header_charge(void) {
    assert_typed_rejected_reservation(ZR_TRUE);
}

static void test_typed_diagnostics_keep_usage_after_scratch_free_and_gc(void) {
    assert_typed_success_keeps_cumulative_usage(ZR_FALSE);
}

static void test_typed_patches_keep_usage_after_scratch_free_and_gc(void) {
    assert_typed_success_keeps_cumulative_usage(ZR_TRUE);
}

static void assert_typed_effect_rejection(TZrBool earlyPhase) {
    SZrAstNode *node = parse_typed_initializer(ZR_FALSE);
    SZrParserComptimeBudgetUsage before = g_compiler.comptimeBudget.usage;
    SZrTypeValue result;
    TZrSize beforeRoots = ZrCore_GcDomain_GetRootCount(g_state);
    TZrSize beforeObjects = object_count();

    g_compiler.comptimeContext = earlyPhase
            ? ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM
            : ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE;
    g_compiler.compilePhase = earlyPhase ? ZR_PARSER_COMPILE_PHASE_SIGNATURE
                                       : ZR_PARSER_COMPILE_PHASE_EXPANSION;
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_FALSE(ZrParser_Compiler_EvaluateCompileTimeExpression(
            &g_compiler, node, &result));
    TEST_ASSERT_TRUE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage, "comptime.effect_violation"));
    assert_error_location(node);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result.type));
    TEST_ASSERT_EQUAL_UINT64(beforeRoots, ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_EQUAL_UINT64(beforeObjects, object_count());
    TEST_ASSERT_EQUAL_UINT64(before.heapBytes, g_compiler.comptimeBudget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(before.aggregateCount, g_compiler.comptimeBudget.usage.aggregateCount);
    TEST_ASSERT_EQUAL_UINT64(before.diagnosticCount, g_compiler.comptimeBudget.usage.diagnosticCount);
    TEST_ASSERT_EQUAL_UINT64(before.fuel + 1U, g_compiler.comptimeBudget.usage.fuel);
}

static void test_typed_constructor_in_pure_context_rejects_before_allocation(void) {
    assert_typed_effect_rejection(ZR_FALSE);
}

static void test_typed_constructor_before_expansion_rejects_before_allocation(void) {
    assert_typed_effect_rejection(ZR_TRUE);
}

static void assert_private_aggregate_header_charge(TZrBool isArray) {
    TZrSize beforeRoots = ZrCore_GcDomain_GetRootCount(g_state);
    const TZrUInt64 headerBytes = sizeof(SZrObject);

    g_compiler.comptimeBudget.limits.heapBytes = 2U * headerBytes;
    g_compiler.comptimeBudget.limits.aggregateCount = 2U;
    for (TZrUInt64 index = 1U; index <= 2U; index++) {
        SZrObject *object = isArray ? ct_new_array(&g_compiler)
                                   : ct_new_object(&g_compiler);
        TEST_ASSERT_NOT_NULL(object);
        TEST_ASSERT_EQUAL_INT(isArray ? ZR_OBJECT_INTERNAL_TYPE_ARRAY
                                     : ZR_OBJECT_INTERNAL_TYPE_OBJECT,
                              object->internalType);
        TEST_ASSERT_EQUAL_UINT64(index * headerBytes,
                                 g_compiler.comptimeBudget.usage.heapBytes);
        TEST_ASSERT_EQUAL_UINT64(index, g_compiler.comptimeBudget.usage.aggregateCount);
        TEST_ASSERT_EQUAL_UINT64(beforeRoots, ZrCore_GcDomain_GetRootCount(g_state));
    }
    TEST_ASSERT_FALSE(g_compiler.hasCompileTimeError);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT64(2U * headerBytes, g_compiler.comptimeBudget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(2U, g_compiler.comptimeBudget.usage.aggregateCount);
    TEST_ASSERT_EQUAL_UINT64(beforeRoots, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_private_object_construction_keeps_exact_header_charge(void) {
    assert_private_aggregate_header_charge(ZR_FALSE);
}

static void test_private_array_construction_keeps_exact_header_charge(void) {
    assert_private_aggregate_header_charge(ZR_TRUE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rejected_object_aggregate_slot_has_no_partial_heap_charge);
    RUN_TEST(test_rejected_array_aggregate_slot_has_no_partial_heap_charge);
    RUN_TEST(test_rejected_object_heap_has_no_partial_aggregate_charge);
    RUN_TEST(test_rejected_array_heap_has_no_partial_aggregate_charge);
    RUN_TEST(test_successful_objects_keep_usage_after_scratch_free_and_gc);
    RUN_TEST(test_successful_arrays_keep_usage_after_scratch_free_and_gc);
    RUN_TEST(test_typed_diagnostic_rejection_has_no_partial_heap_charge);
    RUN_TEST(test_typed_patch_array_rejection_keeps_only_successful_header_charge);
    RUN_TEST(test_typed_diagnostics_keep_usage_after_scratch_free_and_gc);
    RUN_TEST(test_typed_patches_keep_usage_after_scratch_free_and_gc);
    RUN_TEST(test_typed_constructor_in_pure_context_rejects_before_allocation);
    RUN_TEST(test_typed_constructor_before_expansion_rejects_before_allocation);
    RUN_TEST(test_private_object_construction_keeps_exact_header_charge);
    RUN_TEST(test_private_array_construction_keeps_exact_header_charge);
    return UNITY_END();
}
