#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "semantic/bound_expression.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"

static SZrState *g_state;
static SZrCompilerState g_compiler;
static SZrFunction *g_provider;

void setUp(void) {
    static const char source[] =
            "pub struct Point {\n"
            "    pub var x: int;\n"
            "    pub var y: int;\n"
            "    pub @constructor() { this.x = 10; this.y = 20; }\n"
            "    pub @constructor(x: int, y: int = 7) { this.x = x; this.y = y; }\n"
            "}\n"
            "pub struct Empty { pub var value: int; }\n"
            "pub struct Secret {\n"
            "    pub var value: int;\n"
            "    pri @constructor() { this.value = 1; }\n"
            "}\n";
    SZrAstNode *script;

    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    script = ZrParser_Parse(g_state, source, strlen(source),
                           ZrCore_String_CreateFromNative(g_state, "provider.zr"));
    TEST_ASSERT_NOT_NULL(script);
    g_provider = ZrParser_Compiler_Compile(g_state, script);
    ZrParser_Ast_Free(g_state, script);
    TEST_ASSERT_NOT_NULL(g_provider);
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    TEST_ASSERT_TRUE(ZrParser_TypeInference_RegisterRuntimePrototypes(
            &g_compiler, g_provider));
}

void tearDown(void) {
    ZrParser_CompilerState_Free(&g_compiler);
    ZrCore_Function_Free(g_state, g_provider);
    g_provider = ZR_NULL;
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static TZrTypeId type_id(const char *name) {
    return ZrParser_CanonicalType_FromName(
            g_compiler.semanticContext,
            ZrCore_String_CreateFromNative(g_state, (TZrNativeString)name));
}

static TZrSymbolId bind_constructor(const char *name,
                                   SZrBoundValueConstructArgumentInput *arguments,
                                   TZrSize argumentCount,
                                   EZrValueConstructorResolution expected) {
    SZrBoundValueConstruct bound;
    TZrSymbolId symbolId;

    ZrParser_BoundValueConstruct_Init(g_state, &bound);
    TEST_ASSERT_EQUAL_INT(expected, ZrParser_BoundValueConstruct_Bind(
            g_compiler.semanticContext, type_id(name), arguments, argumentCount,
            (SZrFileRange){0}, &bound));
    symbolId = bound.constructorId;
    ZrParser_BoundValueConstruct_Free(g_state, &bound);
    return symbolId;
}

static void test_import_restores_overloads_names_and_defaults(void) {
    SZrBoundValueConstructArgumentInput arguments[2];
    TZrSymbolId explicitDefault;
    TZrSymbolId withDefault;

    memset(arguments, 0, sizeof(arguments));
    arguments[0].typeId = ZrParser_CanonicalType_InternPrimitive(
            g_compiler.semanticContext, ZR_VALUE_TYPE_INT64);
    arguments[1].typeId = arguments[0].typeId;
    arguments[0].name = ZrCore_String_CreateFromNative(g_state, "x");
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_HasCapabilities(
            g_compiler.semanticContext, type_id("Point"),
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
                    ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE));
    explicitDefault = bind_constructor("Point", ZR_NULL, 0U,
                                      ZR_VALUE_CONSTRUCTOR_RESOLVED);
    TEST_ASSERT_NOT_EQUAL(ZR_CANONICAL_SYNTHESIZED_DEFAULT_CONSTRUCTOR_ID,
                          explicitDefault);
    withDefault = bind_constructor("Point", arguments, 1U,
                                  ZR_VALUE_CONSTRUCTOR_RESOLVED);
    TEST_ASSERT_NOT_EQUAL(explicitDefault, withDefault);
    arguments[0].name = ZrCore_String_CreateFromNative(g_state, "y");
    arguments[1].name = ZrCore_String_CreateFromNative(g_state, "x");
    TEST_ASSERT_EQUAL_UINT32(withDefault, bind_constructor(
            "Point", arguments, 2U, ZR_VALUE_CONSTRUCTOR_RESOLVED));
    arguments[1].name = arguments[0].name;
    (void)bind_constructor("Point", arguments, 2U,
                           ZR_VALUE_CONSTRUCTOR_NO_MATCH);
}

static void test_import_preserves_private_and_synthesized_default_rules(void) {
    (void)bind_constructor("Secret", ZR_NULL, 0U,
                           ZR_VALUE_CONSTRUCTOR_INACCESSIBLE);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_SYNTHESIZED_DEFAULT_CONSTRUCTOR_ID,
                            bind_constructor("Empty", ZR_NULL, 0U,
                                             ZR_VALUE_CONSTRUCTOR_RESOLVED));
}

static void test_repeated_import_does_not_duplicate_constructor_identities(void) {
    TZrSymbolId original = bind_constructor("Point", ZR_NULL, 0U,
                                           ZR_VALUE_CONSTRUCTOR_RESOLVED);
    TZrSize count = g_compiler.typePrototypes.length;

    TEST_ASSERT_TRUE(ZrParser_TypeInference_RegisterRuntimePrototypes(
            &g_compiler, g_provider));
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)count,
                            (TZrUInt32)g_compiler.typePrototypes.length);
    TEST_ASSERT_EQUAL_UINT32(original, bind_constructor(
            "Point", ZR_NULL, 0U, ZR_VALUE_CONSTRUCTOR_RESOLVED));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_import_restores_overloads_names_and_defaults);
    RUN_TEST(test_import_preserves_private_and_synthesized_default_rules);
    RUN_TEST(test_repeated_import_does_not_duplicate_constructor_identities);
    return UNITY_END();
}
