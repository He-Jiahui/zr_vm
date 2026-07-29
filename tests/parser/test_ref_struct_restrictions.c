#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;

ZR_PARSER_API TZrBool compiler_validate_ref_struct_rules(
        SZrCompilerState *compiler,
        SZrAstNode *node);
ZR_PARSER_API TZrBool compiler_validate_reference_escapes(
        SZrCompilerState *compiler,
        SZrAstNode *node);

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static SZrAstNode *parse_source(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "ref_struct_restrictions.zr");
    SZrParserState parser;
    SZrAstNode *script;

    ZrParser_State_Init(&parser, g_state, source, strlen(source), sourceName);
    parser.suppressErrorOutput = ZR_TRUE;
    script = ZrParser_ParseWithState(&parser);
    TEST_ASSERT_FALSE_MESSAGE(parser.hasError, parser.errorMessage);
    ZrParser_State_Free(&parser);
    return script;
}

static SZrAstNode *script_statement(SZrAstNode *script, TZrSize index) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(
            (TZrUInt32)index,
            (TZrUInt32)script->data.script.statements->count);
    return script->data.script.statements->nodes[index];
}

static SZrFunction *find_named_function_recursive(
        SZrFunction *function,
        const TZrChar *name,
        TZrUInt32 depth) {
    TZrNativeString functionName;
    TZrUInt32 index;

    if (function == ZR_NULL || name == ZR_NULL || depth > 32U) {
        return ZR_NULL;
    }
    functionName = function->functionName != ZR_NULL
                           ? ZrCore_String_GetNativeString(function->functionName)
                           : ZR_NULL;
    if (functionName != ZR_NULL && strcmp(functionName, name) == 0) {
        return function;
    }
    for (index = 0U; index < function->childFunctionLength; index++) {
        SZrFunction *match = find_named_function_recursive(
                &function->childFunctionList[index], name, depth + 1U);
        if (match != ZR_NULL) {
            return match;
        }
    }
    for (index = 0U; index < function->constantValueLength; index++) {
        SZrTypeValue *constant = &function->constantValueList[index];
        SZrFunction *candidate;
        SZrFunction *match;

        if (constant->type != ZR_VALUE_TYPE_FUNCTION ||
            constant->value.object == ZR_NULL || constant->isNative) {
            continue;
        }
        candidate = ZR_CAST_FUNCTION(g_state, constant->value.object);
        if (candidate == function) {
            continue;
        }
        match = find_named_function_recursive(candidate, name, depth + 1U);
        if (match != ZR_NULL) {
            return match;
        }
    }
    return ZR_NULL;
}

static void assert_ref_struct_rules(
        const TZrChar *source,
        TZrBool expectedSuccess,
        const TZrChar *expectedMessage) {
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    TZrBool success;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    success = ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
                      &compiler, script) &&
              compiler_validate_ref_struct_rules(&compiler, script);

    TEST_ASSERT_EQUAL_INT(expectedSuccess, success);
    TEST_ASSERT_EQUAL_INT(!expectedSuccess, compiler.hasError);
    if (!expectedSuccess) {
        TEST_ASSERT_NOT_NULL(compiler.errorMessage);
        TEST_ASSERT_NOT_NULL_MESSAGE(
                strstr(compiler.errorMessage, expectedMessage),
                compiler.errorMessage);
        TEST_ASSERT_TRUE(compiler.hasStructuredError);
    }

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void assert_ref_struct_escape(
        const TZrChar *source,
        TZrBool expectedSuccess,
        const TZrChar *expectedMessage) {
    SZrAstNode *script = parse_source(source);
    SZrCompilerState compiler;
    TZrBool success;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    success = ZrParser_CompileTime_PrepareBuildFactsInCompilerState(
                      &compiler, script) &&
              compiler_validate_ref_struct_rules(&compiler, script) &&
              compiler_validate_reference_escapes(&compiler, script);

    TEST_ASSERT_EQUAL_INT(expectedSuccess, success);
    TEST_ASSERT_EQUAL_INT(!expectedSuccess, compiler.hasError);
    if (!expectedSuccess) {
        TEST_ASSERT_NOT_NULL(compiler.errorMessage);
        TEST_ASSERT_NOT_NULL_MESSAGE(
                strstr(compiler.errorMessage, expectedMessage),
                compiler.errorMessage);
        TEST_ASSERT_TRUE(compiler.hasStructuredError);
    }

    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_ref_struct_surface_preserves_readonly_and_ref_like_flags(void) {
    SZrAstNode *script = parse_source(
            "ref struct View { var value: int; }\n"
            "readonly ref struct ReadOnlyView { var const value: int; }\n");
    const SZrStructDeclaration *view =
            &script_statement(script, 0U)->data.structDeclaration;
    const SZrStructDeclaration *readOnlyView =
            &script_statement(script, 1U)->data.structDeclaration;

    TEST_ASSERT_TRUE(view->isRefLike);
    TEST_ASSERT_FALSE(view->isReadonly);
    TEST_ASSERT_TRUE(readOnlyView->isRefLike);
    TEST_ASSERT_TRUE(readOnlyView->isReadonly);

    ZrParser_Ast_Free(g_state, script);
}

static void test_ref_struct_projects_canonical_ref_like_capability(void) {
    SZrAstNode *script = parse_source("ref struct View { var value: int; }\n");
    SZrAstNode *declaration = script_statement(script, 0U);
    SZrCompilerState compiler;
    TZrTypeId typeId;

    memset(&compiler, 0, sizeof(compiler));
    ZrParser_CompilerState_Init(&compiler, g_state);
    compiler.suppressErrorOutput = ZR_TRUE;
    compiler.currentAst = script;
    compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);

    ZrParser_Compiler_CompileStructDeclaration(&compiler, declaration);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)compiler.typePrototypes.length);
    {
        const SZrTypePrototypeInfo *prototype =
                (const SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        &compiler.typePrototypes, 0U);
        TEST_ASSERT_NOT_NULL(prototype);
        TEST_ASSERT_TRUE(prototype->allowValueConstruction);
        TEST_ASSERT_FALSE(prototype->allowBoxedConstruction);
    }
    typeId = ZrParser_CanonicalType_FromName(
            compiler.semanticContext,
            declaration->data.structDeclaration.name->name);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, typeId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_HasCapabilities(
            compiler.semanticContext,
            typeId,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
                    ZR_CANONICAL_TYPE_CAPABILITY_REF_LIKE));

    ZrCore_Function_Free(g_state, compiler.currentFunction);
    compiler.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compiler);
    ZrParser_Ast_Free(g_state, script);
}

static void test_ref_struct_accepts_legal_storage_and_safe_return_surfaces(void) {
    assert_ref_struct_rules(
            "class Resource {}\n"
            "ref struct Inner { var value: int; }\n"
            "ref struct View<T> {\n"
            "  var borrowed: ref T;\n"
            "  var observed: ref readonly T;\n"
            "  var nested: Inner;\n"
            "  var label: string;\n"
            "  var owner: %unique Resource;\n"
            "}\n"
            "fn use(view: View<int>): View<int> {\n"
            "  var local: View<int> = view;\n"
            "  return local;\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL);
    assert_ref_struct_escape(
            "ref struct View { var value: int; }\n"
            "fn forward(view: View): View { return view; }\n",
            ZR_TRUE,
            ZR_NULL);
}

static void test_ref_struct_rejects_heap_fields_and_plain_struct_ref_fields(void) {
    assert_ref_struct_rules(
            "ref struct View { var value: int; }\n"
            "class Holder { var view: View; }\n",
            ZR_FALSE,
            "cannot be stored in a class field");
    assert_ref_struct_rules(
            "ref struct View { var value: int; }\n"
            "%owned class Holder { var view: View; }\n",
            ZR_FALSE,
            "cannot be stored in a resource class field");
    assert_ref_struct_rules(
            "struct Invalid { var borrowed: ref int; }\n",
            ZR_FALSE,
            "Only a ref struct may contain a ref field");
    assert_ref_struct_rules(
            "ref struct View { var value: int; }\n"
            "struct Invalid { var view: View; }\n",
            ZR_FALSE,
            "Only a ref struct may contain a ref-like field");
}

static void test_ref_struct_rejects_array_global_and_generic_storage(void) {
    assert_ref_struct_rules(
            "ref struct View { var value: int; }\n"
            "fn invalid(): void { var views: View[1]; }\n",
            ZR_FALSE,
            "cannot be an array element");
    assert_ref_struct_rules(
            "ref struct View { var value: int; }\n"
            "var globalView: View;\n",
            ZR_FALSE,
            "cannot be stored in module/global storage");
    assert_ref_struct_rules(
            "ref struct View { var value: int; }\n"
            "class Store { static var current: View; }\n",
            ZR_FALSE,
            "cannot be stored in module/global storage");
    assert_ref_struct_rules(
            "class Box<T> {}\n"
            "ref struct View { var value: int; }\n"
            "fn invalid(): void { var boxed: Box<View>; }\n",
            ZR_FALSE,
            "cannot be used as an unconstrained generic argument");
    assert_ref_struct_escape(
            "ref struct View { var value: int; }\n"
            "var globalView = init View();\n",
            ZR_FALSE,
            "module/global store");
    assert_ref_struct_escape(
            "ref struct View { var value: int; }\n"
            "fn invalid(view: View): void { var views = [view]; }\n",
            ZR_FALSE,
            "cannot be stored in an array");
}

static void test_ref_struct_rules_follow_active_comptime_branch(void) {
    assert_ref_struct_rules(
            "ref struct View { var value: int; }\n"
            "comptime if (true) { var globalView: View; }\n",
            ZR_FALSE,
            "cannot be stored in module/global storage");
    assert_ref_struct_rules(
            "ref struct View { var value: int; }\n"
            "comptime if (false) { var globalView: View; }\n"
            "var result: int = 0;\n",
            ZR_TRUE,
            ZR_NULL);
}

static void test_ref_struct_rejects_boxing_and_native_opaque_storage(void) {
    assert_ref_struct_escape(
            "ref struct View { var value: int; }\n"
            "fn invalid(view: View): object { return <object> view; }\n",
            ZR_FALSE,
            "cannot be boxed as object");
    assert_ref_struct_escape(
            "interface Display {}\n"
            "ref struct View { var value: int; }\n"
            "fn invalid(view: View): Display { return <Display> view; }\n",
            ZR_FALSE,
            "cannot be boxed as object, dynamic, or interface");
    assert_ref_struct_escape(
            "ref struct View { var value: int; }\n"
            "fn invalid(view: View): dynamic { return <dynamic> view; }\n",
            ZR_FALSE,
            "cannot be boxed as object, dynamic, or interface");
    assert_ref_struct_rules(
            "ref struct View { var value: int; }\n"
            "native extern(\"sample\") { fn store(view: View): void; }\n",
            ZR_FALSE,
            "cannot cross a native opaque ABI boundary");
    assert_ref_struct_rules(
            "ref struct View { var value: int; }\n"
            "native extern(\"sample\") { fn load(): View; }\n",
            ZR_FALSE,
            "cannot cross a native opaque ABI boundary");
}

static void test_ref_struct_return_tracks_internal_reference_origin(void) {
    assert_ref_struct_escape(
            "ref struct View { var borrowed: ref int; }\n"
            "fn invalid(): View {\n"
            "  var value: int = 1;\n"
            "  return init View(ref value);\n"
            "}\n",
            ZR_FALSE,
            "cannot escape to caller through return");
    assert_ref_struct_escape(
            "ref struct EmptyView { var value: int; }\n"
            "fn make(): EmptyView { return init EmptyView(); }\n",
            ZR_TRUE,
            ZR_NULL);
}

static void test_ref_struct_rejects_closure_capture(void) {
    assert_ref_struct_escape(
            "ref struct View { var value: int; }\n"
            "fn invalid(view: View): int {\n"
            "  var read = fn(): int => view.value;\n"
            "  return 0;\n"
            "}\n",
            ZR_FALSE,
            "cannot be captured by a closure");
}

static void test_ref_struct_rejects_await_and_yield_suspension(void) {
    assert_ref_struct_escape(
            "ref struct View { var value: int; }\n"
            "async fn invalid(view: View): Task<int> {\n"
            "  var task = pause().start();\n"
            "  await task;\n"
            "  return view.value;\n"
            "}\n",
            ZR_FALSE,
            "cannot cross an await suspension");
    assert_ref_struct_escape(
            "ref struct View { var value: int; }\n"
            "fn invalid(view: View): int {\n"
            "  var sequence = {{\n"
            "    out 1;\n"
            "    out view.value;\n"
            "  }};\n"
            "  return 0;\n"
            "}\n",
            ZR_FALSE,
            "cannot cross a yield suspension");
}

static void test_ref_struct_frame_layout_preserves_ref_and_owner_maps(void) {
    SZrAstNode *script = parse_source(
            "%owned class Resource {}\n"
            "ref struct RefOwner {\n"
            "  var borrowed: ref int;\n"
            "  var owner: %unique Resource;\n"
            "}\n"
            "fn probe(value: ref int): int {\n"
            "  var frame: RefOwner;\n"
            "  return value;\n"
            "}\n"
            "var source: int = 1;\n"
            "return probe(ref source);\n");
    SZrFunction *entry = ZrParser_Compiler_Compile(g_state, script);
    SZrFunction *probe;
    TZrUInt32 inlineSlotCount = 0U;
    TZrUInt32 resolvedLayoutCount = 0U;
    TZrUInt32 maxRefFieldCount = 0U;
    TZrUInt32 maxOwnershipFieldCount = 0U;
    TZrUInt32 maxDropKind = ZR_TYPE_LAYOUT_DROP_KIND_NONE;
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL(entry);
    probe = find_named_function_recursive(entry, "probe", 0U);
    TEST_ASSERT_NOT_NULL(probe);
    for (index = 0U; index < probe->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slot = &probe->frameSlotLayouts[index];
        const SZrTypeLayout *layout;

        if (slot->slotKind != ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT) {
            continue;
        }
        inlineSlotCount++;
        layout = ZrCore_Function_ResolvePrototypeFrameTypeLayout(
                probe, slot->typeLayoutId, g_state);
        if (layout != ZR_NULL) {
            resolvedLayoutCount++;
            if (layout->refFieldCount > maxRefFieldCount) {
                maxRefFieldCount = layout->refFieldCount;
            }
            if (layout->ownershipFieldCount > maxOwnershipFieldCount) {
                maxOwnershipFieldCount = layout->ownershipFieldCount;
            }
            if (layout->dropKind > maxDropKind) {
                maxDropKind = layout->dropKind;
            }
        }
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0U, inlineSlotCount);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, resolvedLayoutCount);
    TEST_ASSERT_EQUAL_UINT32(1U, maxRefFieldCount);
    TEST_ASSERT_EQUAL_UINT32(1U, maxOwnershipFieldCount);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE, maxDropKind);

    ZrCore_Function_Free(g_state, entry);
    ZrParser_Ast_Free(g_state, script);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ref_struct_surface_preserves_readonly_and_ref_like_flags);
    RUN_TEST(test_ref_struct_projects_canonical_ref_like_capability);
    RUN_TEST(test_ref_struct_accepts_legal_storage_and_safe_return_surfaces);
    RUN_TEST(test_ref_struct_rejects_heap_fields_and_plain_struct_ref_fields);
    RUN_TEST(test_ref_struct_rejects_array_global_and_generic_storage);
    RUN_TEST(test_ref_struct_rules_follow_active_comptime_branch);
    RUN_TEST(test_ref_struct_rejects_boxing_and_native_opaque_storage);
    RUN_TEST(test_ref_struct_return_tracks_internal_reference_origin);
    RUN_TEST(test_ref_struct_rejects_closure_capture);
    RUN_TEST(test_ref_struct_rejects_await_and_yield_suspension);
    RUN_TEST(test_ref_struct_frame_layout_preserves_ref_and_owner_maps);
    return UNITY_END();
}
