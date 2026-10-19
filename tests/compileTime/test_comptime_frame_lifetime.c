#include "unity.h"

#include <string.h>

#include "runtime_support.h"
#include "compiler/compile_time/compile_time_executor_internal.h"
#include "zr_vm_parser/compile_tool.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static SZrAstNode *g_ast;

typedef struct SDecoratorGcProbe {
    FZrAllocator allocator;
    TZrPtr allocatorUserData;
    SZrRawObject *firstPatch;
    TZrBool armed;
    TZrBool collected;
    TZrSize allocationCalls;
    TZrSize freeCalls;
    TZrSize patchObservations;
} SDecoratorGcProbe;

static SDecoratorGcProbe g_decoratorGcProbe;

void setUp(void) {
    memset(&g_decoratorGcProbe, 0, sizeof(g_decoratorGcProbe));
    g_ast = ZR_NULL;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compiler.suppressErrorOutput = ZR_TRUE;
}

void tearDown(void) {
    if (g_decoratorGcProbe.allocator != ZR_NULL) {
        g_state->global->allocator = g_decoratorGcProbe.allocator;
        g_state->global->userAllocationArguments =
                g_decoratorGcProbe.allocatorUserData;
    }
    ZrParser_CompilerState_Free(&g_compiler);
    if (g_ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_ast);
        g_ast = ZR_NULL;
    }
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static SZrString *make_string(const TZrChar *text) {
    SZrString *string = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)text);
    TEST_ASSERT_NOT_NULL(string);
    return string;
}

static SZrTypeValue string_value(const TZrChar *text) {
    SZrTypeValue value;
    ZrCore_Value_InitAsRawObject(
            g_state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(make_string(text)));
    value.type = ZR_VALUE_TYPE_STRING;
    return value;
}

static SZrCompileTimeBinding *binding_at(SZrCompileTimeFrame *frame,
                                         TZrSize index) {
    SZrCompileTimeBinding *binding =
            (SZrCompileTimeBinding *)ZrCore_Array_Get(&frame->bindings, index);
    TEST_ASSERT_NOT_NULL(binding);
    return binding;
}

static void test_frame_roots_name_and_value_until_free(void) {
    SZrCompileTimeFrame frame;
    TZrSize baseline = ZrCore_GcDomain_GetRootCount(g_state);
    SZrString *name = make_string("rootedBinding");
    SZrTypeValue value = string_value("a frame-owned string");
    SZrTypeValue result;
    SZrRawObject *resolvedName = ZR_NULL;
    ct_frame_init(&g_compiler, &frame, ZR_NULL);
    TEST_ASSERT_TRUE(ct_frame_set(&g_compiler, &frame, name, &value));
    TEST_ASSERT_EQUAL_UINT64(baseline + 2U, ZrCore_GcDomain_GetRootCount(g_state));
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(
            g_state, &binding_at(&frame, 0U)->nameRoot, &resolvedName));
    TEST_ASSERT_NOT_NULL(resolvedName);
    TEST_ASSERT_TRUE(ct_frame_get(&frame, (SZrString *)resolvedName, &result));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, result.type);
    TEST_ASSERT_EQUAL_STRING("a frame-owned string",
            ZrCore_String_GetNativeString((SZrString *)result.value.object));
    ct_frame_free(&g_compiler, &frame);
    TEST_ASSERT_EQUAL_UINT64(baseline, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_value_update_releases_obsolete_object_root(void) {
    SZrCompileTimeFrame frame;
    TZrSize baseline = ZrCore_GcDomain_GetRootCount(g_state);
    SZrString *name = make_string("replaceableBinding");
    SZrTypeValue value = string_value("obsolete");
    SZrTypeValue result;
    ct_frame_init(&g_compiler, &frame, ZR_NULL);
    TEST_ASSERT_TRUE(ct_frame_set(&g_compiler, &frame, name, &value));
    ZrCore_Value_InitAsInt(g_state, &value, 17);
    TEST_ASSERT_TRUE(ct_frame_update(&g_compiler, &frame, name, &value,
            (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL}));
    TEST_ASSERT_EQUAL_UINT64(baseline + 1U, ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_TRUE(ct_frame_get(&frame, name, &result));
    TEST_ASSERT_EQUAL_INT64(17, result.value.nativeObject.nativeInt64);
    ct_frame_free(&g_compiler, &frame);
    TEST_ASSERT_EQUAL_UINT64(baseline, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_nested_frame_free_keeps_outer_value_alive(void) {
    SZrCompileTimeFrame outer;
    SZrCompileTimeFrame inner;
    TZrSize baseline = ZrCore_GcDomain_GetRootCount(g_state);
    SZrString *name = make_string("shadowedBinding");
    SZrTypeValue value = string_value("outer value");
    SZrTypeValue result;
    SZrRawObject *resolvedName = ZR_NULL;
    ct_frame_init(&g_compiler, &outer, ZR_NULL);
    ct_frame_init(&g_compiler, &inner, &outer);
    TEST_ASSERT_TRUE(ct_frame_set(&g_compiler, &outer, name, &value));
    value = string_value("inner value");
    TEST_ASSERT_TRUE(ct_frame_set(&g_compiler, &inner, name, &value));
    TEST_ASSERT_EQUAL_UINT64(baseline + 4U, ZrCore_GcDomain_GetRootCount(g_state));
    ct_frame_free(&g_compiler, &inner);
    TEST_ASSERT_EQUAL_UINT64(baseline + 2U, ZrCore_GcDomain_GetRootCount(g_state));
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_TRUE(ZrCore_GcRootHandle_Resolve(
            g_state, &binding_at(&outer, 0U)->nameRoot, &resolvedName));
    TEST_ASSERT_TRUE(ct_frame_get(&outer, (SZrString *)resolvedName, &result));
    TEST_ASSERT_EQUAL_STRING("outer value",
            ZrCore_String_GetNativeString((SZrString *)result.value.object));
    ct_frame_free(&g_compiler, &outer);
    TEST_ASSERT_EQUAL_UINT64(baseline, ZrCore_GcDomain_GetRootCount(g_state));
}

static void test_rejected_frame_binding_has_no_partial_charge_or_roots(void) {
    SZrCompileTimeFrame frame;
    TZrSize baseline = ZrCore_GcDomain_GetRootCount(g_state);
    SZrString *name = make_string("rejectedBinding");
    SZrTypeValue value = string_value("rejected value");
    TZrUInt64 beforeHeap = g_compiler.comptimeBudget.usage.heapBytes;
    g_compiler.comptimeBudget.limits.aggregateCount = 0U;
    ct_frame_init(&g_compiler, &frame, ZR_NULL);
    TEST_ASSERT_FALSE(ct_frame_set(&g_compiler, &frame, name, &value));
    TEST_ASSERT_EQUAL_UINT64(beforeHeap, g_compiler.comptimeBudget.usage.heapBytes);
    TEST_ASSERT_EQUAL_UINT64(0U, frame.bindings.length);
    TEST_ASSERT_EQUAL_UINT64(baseline, ZrCore_GcDomain_GetRootCount(g_state));
    ct_frame_free(&g_compiler, &frame);
}

static SZrRawObject *find_completed_patch(void) {
    SZrRawObject *raw = g_state->global->garbageCollector->gcObjectList;

    for (; raw != ZR_NULL; raw = raw->next) {
        SZrObject *object;
        if (raw->type != ZR_RAW_OBJECT_TYPE_OBJECT) {
            continue;
        }
        object = (SZrObject *)raw;
        if (!object->nodeMap.isValid || object->nodeMap.elementCount != 6U) {
            continue;
        }
        for (TZrSize index = 0U; index < object->nodeMap.capacity; index++) {
            const SZrHashKeyValuePair *pair = object->nodeMap.buckets[index];
            for (; pair != ZR_NULL; pair = pair->next) {
                if (pair->key.type == ZR_VALUE_TYPE_STRING &&
                    pair->key.value.object != ZR_NULL &&
                    strcmp(ZrCore_String_GetNativeString(
                                   (SZrString *)pair->key.value.object),
                           "__zrCompileToolTypeRole") == 0 &&
                    ZR_VALUE_IS_TYPE_INT(pair->value.type) &&
                    pair->value.value.nativeObject.nativeInt64 ==
                            ZR_PARSER_COMPILE_TOOL_TYPE_PATCH) {
                    return raw;
                }
            }
        }
    }
    return ZR_NULL;
}

static TZrBool gc_list_contains(const SZrRawObject *expected) {
    SZrRawObject *raw = g_state->global->garbageCollector->gcObjectList;
    for (; raw != ZR_NULL; raw = raw->next) {
        if (raw == expected) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrPtr collect_between_decorator_allocations(
        TZrPtr userData,
        TZrPtr pointer,
        TZrSize originalSize,
        TZrSize newSize,
        TZrInt64 flag) {
    SDecoratorGcProbe *probe = (SDecoratorGcProbe *)userData;

    probe->allocationCalls++;
    if (newSize == 0U) {
        probe->freeCalls++;
    }
    if (find_completed_patch() != ZR_NULL) {
        probe->patchObservations++;
    }

    if (!probe->armed && newSize == 0U &&
        flag == ZR_MEMORY_NATIVE_TYPE_ARRAY &&
        g_compiler.comptimeContext !=
                ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM) {
        probe->firstPatch = find_completed_patch();
        probe->armed = probe->firstPatch != ZR_NULL;
    }
    if (probe->armed && !probe->collected && newSize > 0U) {
        probe->collected = ZR_TRUE;
        ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
        /* Fail before the decorator caller can dereference a reclaimed patch. */
        TEST_ASSERT_TRUE_MESSAGE(
                gc_list_contains(probe->firstPatch),
                "Earlier declaration.Patch was collected before later decorators completed");
    }
    return probe->allocator(
            probe->allocatorUserData,
            pointer,
            originalSize,
            newSize,
            flag);
}

static SZrAstNode *prepare_parameter_decorator_fixture(
        const TZrChar *source,
        SZrFunctionMetadataParameter *parameterInfo) {
    SZrAstNode *parameterNode = ZR_NULL;

    g_ast = ZrParser_Parse(
            g_state,
            source,
            strlen(source),
            make_string("parameter_decorator_lifetime.zr"));
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &g_compiler, g_ast));
    g_compiler.compilePhase = ZR_PARSER_COMPILE_PHASE_EXPANSION;

    for (TZrSize index = 0U;
         index < g_ast->data.script.statements->count;
         index++) {
        SZrAstNode *statement = g_ast->data.script.statements->nodes[index];
        if (statement == ZR_NULL ||
            statement->type != ZR_AST_FUNCTION_DECLARATION ||
            statement->data.functionDeclaration.name == ZR_NULL ||
            statement->data.functionDeclaration.name->name == ZR_NULL ||
            strcmp(ZrCore_String_GetNativeString(
                           statement->data.functionDeclaration.name->name),
                   "target") != 0 ||
            statement->data.functionDeclaration.params == ZR_NULL ||
            statement->data.functionDeclaration.params->count == 0U) {
            continue;
        }
        parameterNode = statement->data.functionDeclaration.params->nodes[0];
        break;
    }

    TEST_ASSERT_NOT_NULL(parameterNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PARAMETER, parameterNode->type);
    TEST_ASSERT_NOT_NULL(parameterNode->data.parameter.name);
    TEST_ASSERT_NOT_NULL(parameterNode->data.parameter.name->name);
    memset(parameterInfo, 0, sizeof(*parameterInfo));
    parameterInfo->name = parameterNode->data.parameter.name->name;
    parameterInfo->type.baseType = ZR_VALUE_TYPE_INT64;
    return parameterNode;
}

static void free_parameter_decorator_names(
        SZrFunctionMetadataParameter *parameterInfo) {
    if (parameterInfo->decoratorNames != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                g_state->global,
                parameterInfo->decoratorNames,
                sizeof(SZrString *) * parameterInfo->decoratorCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        parameterInfo->decoratorNames = ZR_NULL;
        parameterInfo->decoratorCount = 0U;
    }
}

static void test_parameter_decorator_entry_keeps_patch_rooted(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn first(target: declaration.Parameter): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn second(target: declaration.Parameter): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n"
            "fn target(#first# #second# value: int): int { return value; }\n"
            "return 0;\n";
    SZrFunctionMetadataParameter parameterInfo;
    SZrAstNode *parameterNode;
    TZrSize baseline;
    TZrBool success;
    TZrBool hasDecoratorNames;
    TZrUInt32 decoratorCount;

    parameterNode = prepare_parameter_decorator_fixture(source, &parameterInfo);
    baseline = ZrCore_GcDomain_GetRootCount(g_state);
    g_decoratorGcProbe.allocator = g_state->global->allocator;
    g_decoratorGcProbe.allocatorUserData =
            g_state->global->userAllocationArguments;
    g_state->global->allocator = collect_between_decorator_allocations;
    g_state->global->userAllocationArguments = &g_decoratorGcProbe;
    success = ZrParser_CompileTime_ApplyParameterDecorators(
            &g_compiler, parameterNode, 0U, &parameterInfo);
    g_state->global->allocator = g_decoratorGcProbe.allocator;
    g_state->global->userAllocationArguments =
            g_decoratorGcProbe.allocatorUserData;
    hasDecoratorNames = parameterInfo.decoratorNames != ZR_NULL;
    decoratorCount = parameterInfo.decoratorCount;
    free_parameter_decorator_names(&parameterInfo);

    TEST_ASSERT_EQUAL_UINT64(baseline, ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_TRUE(success);
    TEST_ASSERT_TRUE(g_decoratorGcProbe.collected);
    TEST_ASSERT_TRUE(g_decoratorGcProbe.patchObservations > 0U);
    TEST_ASSERT_EQUAL_UINT32(2U, decoratorCount);
    TEST_ASSERT_TRUE(hasDecoratorNames);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_FALSE(g_compiler.hasCompileTimeError);
}

static void test_parameter_decorator_missing_return_releases_frame_roots(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn first(target: declaration.Parameter): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn missing(target: declaration.Parameter): declaration.Patch {\n"
            "    let discarded = init declaration.Patch(target: target.symbolId);\n"
            "}\n"
            "fn target(#first# #missing# value: int): int { return value; }\n"
            "return 0;\n";
    SZrFunctionMetadataParameter parameterInfo;
    SZrAstNode *parameterNode;
    TZrSize baseline;
    TZrBool success;

    parameterNode = prepare_parameter_decorator_fixture(source, &parameterInfo);
    baseline = ZrCore_GcDomain_GetRootCount(g_state);
    success = ZrParser_CompileTime_ApplyParameterDecorators(
            &g_compiler, parameterNode, 0U, &parameterInfo);

    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT64(baseline, ZrCore_GcDomain_GetRootCount(g_state));
    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_TRUE(g_compiler.hasCompileTimeError);
    TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage,
                                "declaration_transform.return"));
    TEST_ASSERT_NULL(parameterInfo.decoratorNames);
    TEST_ASSERT_EQUAL_UINT32(0U, parameterInfo.decoratorCount);
}

static void test_returned_decorator_patch_survives_later_transform_gc(void) {
    static const TZrChar *source =
            "let declaration = import(\"zr.compile.declaration\");\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn first(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n"
            "#zr.compile.declarationTransform#\n"
            "pub comptime fn second(target: declaration.Struct): declaration.Patch {\n"
            "    return init declaration.Patch(target: target.symbolId);\n"
            "}\n"
            "#first#\n"
            "#second#\n"
            "pub struct Meter { pub let value: int; }\n"
            "return 0;\n";
    SZrAstNode *structNode = ZR_NULL;

    g_ast = ZrParser_Parse(
            g_state, source, strlen(source), make_string("decorator_patch_gc.zr"));
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    TEST_ASSERT_TRUE(ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
            &g_compiler, g_ast));
    for (TZrSize index = 0U; index < g_ast->data.script.statements->count; index++) {
        SZrAstNode *statement = g_ast->data.script.statements->nodes[index];
        if (statement != ZR_NULL && statement->type == ZR_AST_STRUCT_DECLARATION) {
            structNode = statement;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(structNode);
    g_compiler.compilePhase = ZR_PARSER_COMPILE_PHASE_EXPANSION;

    g_decoratorGcProbe.allocator = g_state->global->allocator;
    g_decoratorGcProbe.allocatorUserData = g_state->global->userAllocationArguments;
    g_state->global->allocator = collect_between_decorator_allocations;
    g_state->global->userAllocationArguments = &g_decoratorGcProbe;
    ZrParser_Compiler_CompileStructDeclaration(&g_compiler, structNode);
    g_state->global->allocator = g_decoratorGcProbe.allocator;
    g_state->global->userAllocationArguments = g_decoratorGcProbe.allocatorUserData;

    TEST_ASSERT_TRUE_MESSAGE(g_decoratorGcProbe.collected,
            "Fixture must force GC while an earlier decorator patch is pending");
    TEST_ASSERT_FALSE(g_compiler.hasError);
    TEST_ASSERT_FALSE(g_compiler.hasCompileTimeError);
}

#include "test_comptime_frame_budget_cases.h"
#include "test_comptime_frame_transaction_cases.h"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_frame_roots_name_and_value_until_free);
    RUN_TEST(test_value_update_releases_obsolete_object_root);
    RUN_TEST(test_nested_frame_free_keeps_outer_value_alive);
    RUN_TEST(test_rejected_frame_binding_has_no_partial_charge_or_roots);
    RUN_TEST(test_returned_decorator_patch_survives_later_transform_gc);
    RUN_TEST(test_parameter_decorator_entry_keeps_patch_rooted);
    RUN_TEST(test_parameter_decorator_missing_return_releases_frame_roots);
    RUN_TEST(test_public_scalar_evaluation_needs_no_frame_backing);
    RUN_TEST(test_root_growth_requires_full_candidate_heap_admission);
    RUN_TEST(test_binding_growth_requires_full_candidate_heap_admission);
    RUN_TEST(test_rejected_binding_value_keeps_mutability);
    RUN_TEST(test_binding_candidate_oom_is_atomic);
    RUN_TEST(test_root_candidate_oom_is_atomic);
    RUN_TEST(test_binding_candidate_gc_preserves_inputs);
    RUN_TEST(test_root_candidate_gc_preserves_inputs);
    RUN_TEST(test_binding_capacity_growth_and_reuse_are_exact);
    RUN_TEST(test_root_capacity_growth_and_reuse_are_exact);
    RUN_TEST(test_binding_commit_rejects_live_heap_interleave);
    RUN_TEST(test_root_commit_rejects_live_heap_interleave);
    RUN_TEST(test_binding_commit_preserves_live_heap_interleave);
    RUN_TEST(test_root_commit_preserves_live_heap_interleave);
    return UNITY_END();
}
