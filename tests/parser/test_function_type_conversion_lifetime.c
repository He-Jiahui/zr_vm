#include "unity.h"

#include <string.h>

#include "compiler/compiler_internal.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"

typedef struct SArrayAllocationProbe {
    SZrGlobalState *global;
    FZrAllocator allocator;
    TZrPtr userData;
    TZrSize allocated;
    TZrSize freed;
    TZrSize allocatedBytes;
    TZrSize freedBytes;
} SArrayAllocationProbe;

static SZrState *g_state;
static SZrCompilerState g_compiler;
static TZrBool g_compilerInitialized;
static SZrAstNode *g_ast;
static SZrSyntaxContextTable *g_table;
static SZrSyntaxContext g_definition;
static SZrInferredType g_result;
static SZrInferredType g_child;
static TZrBool g_typesInitialized;
static SArrayAllocationProbe g_probe;

static TZrPtr counting_allocator(TZrPtr userData, TZrPtr pointer,
        TZrSize oldSize, TZrSize newSize, TZrInt64 type) {
    SArrayAllocationProbe *probe = (SArrayAllocationProbe *)userData;
    TZrBool hadAllocation = pointer != ZR_NULL;
    TZrPtr result = probe->allocator(
            probe->userData, pointer, oldSize, newSize, type);

    if (type == ZR_MEMORY_NATIVE_TYPE_ARRAY) {
        if (hadAllocation && (newSize == 0U || result != ZR_NULL)) {
            probe->freed++;
            probe->freedBytes += oldSize;
        }
        if (result != ZR_NULL && newSize != 0U) {
            probe->allocated++;
            probe->allocatedBytes += newSize;
        }
    }
    return result;
}

static void end_probe(void) {
    if (g_probe.global != ZR_NULL) {
        g_probe.global->allocator = g_probe.allocator;
        g_probe.global->userAllocationArguments = g_probe.userData;
        g_probe.global = ZR_NULL;
    }
}

static void begin_probe(void) {
    memset(&g_probe, 0, sizeof(g_probe));
    g_probe.global = g_state->global;
    g_probe.allocator = g_state->global->allocator;
    g_probe.userData = g_state->global->userAllocationArguments;
    g_state->global->allocator = counting_allocator;
    g_state->global->userAllocationArguments = &g_probe;
}

static void clear_error_flags(void) {
    g_compiler.hasError = ZR_FALSE;
    g_compiler.hadRecoverableError = ZR_FALSE;
    g_compiler.errorMessage = ZR_NULL;
}

void setUp(void) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;
    SZrScope scope = {0};

    g_state = ZR_NULL;
    g_ast = ZR_NULL;
    g_table = ZR_NULL;
    g_compilerInitialized = ZR_FALSE;
    g_typesInitialized = ZR_FALSE;
    memset(&g_probe, 0, sizeof(g_probe));
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
    ZrParser_InferredType_Init(g_state, &g_result, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(g_state, &g_child, ZR_VALUE_TYPE_OBJECT);
    g_typesInitialized = ZR_TRUE;
    g_table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_table);
    expansion.stableIdentity = 0xB701U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    TEST_ASSERT_TRUE(ZrParser_SyntaxContextTable_RegisterExpansion(
            g_table, &expansion, &expansionId));
    g_definition = ZrParser_SyntaxContext_DefSite(g_table, expansionId);
    ZrParser_CompilerState_Init(&g_compiler, g_state);
    g_compilerInitialized = ZR_TRUE;
    g_compiler.suppressErrorOutput = ZR_TRUE;
    g_compiler.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(g_compiler.currentFunction);
    g_compiler.isScriptLevel = ZR_TRUE;
    scope.parentCompiler = &g_compiler;
    ZrCore_Array_Push(g_state, &g_compiler.scopeStack, &scope);
}

void tearDown(void) {
    end_probe();
    if (g_typesInitialized) {
        ZrParser_InferredType_Free(g_state, &g_result);
        ZrParser_InferredType_Free(g_state, &g_child);
        g_typesInitialized = ZR_FALSE;
    }
    if (g_compilerInitialized) {
        if (g_compiler.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(g_state, g_compiler.currentFunction);
            g_compiler.currentFunction = ZR_NULL;
        }
        ZrParser_CompilerState_Free(&g_compiler);
        g_compilerInitialized = ZR_FALSE;
    }
    if (g_ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, g_ast);
        g_ast = ZR_NULL;
    }
    if (g_table != ZR_NULL) {
        ZrParser_SyntaxContextTable_Release(g_table);
        g_table = ZR_NULL;
    }
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void check_child_conversion(SZrType *type, TZrBool success,
        EZrValueType expectedType) {
    TEST_ASSERT_NOT_NULL(type);
    clear_error_flags();
    TEST_ASSERT_EQUAL_INT(success, ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, type, &g_child));
    TEST_ASSERT_EQUAL_INT(!success, g_compiler.hasError);
    if (success) {
        TEST_ASSERT_EQUAL_INT(expectedType, g_child.baseType);
        if (expectedType == ZR_VALUE_TYPE_ARRAY) {
            const SZrInferredType *element;

            TEST_ASSERT_EQUAL_INT(1, type->dimensions);
            TEST_ASSERT_EQUAL_UINT(1U, g_child.elementTypes.length);
            element = ZrCore_Array_Get(&g_child.elementTypes, 0U);
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, element->baseType);
        }
    } else {
        TEST_ASSERT_NOT_NULL(g_compiler.errorMessage);
        TEST_ASSERT_NOT_NULL(strstr(g_compiler.errorMessage,
                "requires an explicit module qualifier"));
    }
    ZrParser_InferredType_Free(g_state, &g_child);
    clear_error_flags();
}

static void assert_conversion_releases_arrays(const TZrChar *source,
        TZrSize parameterCount, TZrBool missingParameter,
        TZrBool missingReturn, TZrBool markedReturn,
        const TZrChar *expectedDisplay,
        const EZrParameterPassingMode *expectedModes) {
    SZrString *name = ZrCore_String_CreateFromNative(
            g_state, "function_type_lifetime.zr");
    SZrAstNodeArray *statements;
    SZrAstNode *declaration;
    SZrType *annotation;
    SZrFunctionType *signature;
    TZrBool success = !missingParameter && !missingReturn && !markedReturn;
    TZrBool converted;
    TZrBool compositeParameter = ZR_FALSE;
    TZrSize capacity = parameterCount == 0U ? 1U : parameterCount;

    TEST_ASSERT_NOT_NULL(name);
    g_ast = ZrParser_Parse(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL(g_ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, g_ast->type);
    statements = g_ast->data.script.statements;
    TEST_ASSERT_NOT_NULL(statements);
    TEST_ASSERT_EQUAL_UINT(markedReturn ? 2U : 1U, statements->count);
    g_compiler.scriptAst = g_ast;
    if (markedReturn) {
        SZrAstNode *widget = statements->nodes[0];
        const SZrNamedTypeBinding *binding;
        const SZrCanonicalTypeNode *nominal;

        TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, widget->type);
        extern_compiler_register_struct_prototype(&g_compiler, widget);
        TEST_ASSERT_FALSE(g_compiler.hasError);
        binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
                g_compiler.typeEnv, widget->data.structDeclaration.name->name,
                ZrParser_SyntaxContext_Root());
        TEST_ASSERT_NOT_NULL(binding);
        TEST_ASSERT_EQUAL_PTR(widget, binding->declarationNode);
        nominal = ZrParser_CanonicalType_Find(
                g_compiler.semanticContext, binding->typeId);
        TEST_ASSERT_NOT_NULL(nominal);
        TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_NOMINAL, nominal->kind);
        TEST_ASSERT_EQUAL_UINT32(0U, nominal->data.nominal.definitionToken);
    }
    declaration = statements->nodes[markedReturn ? 1U : 0U];
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    annotation = declaration->data.variableDeclaration.typeInfo;
    TEST_ASSERT_NOT_NULL(annotation);
    TEST_ASSERT_NOT_NULL(annotation->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_TYPE, annotation->name->type);
    signature = &annotation->name->data.functionType;
    TEST_ASSERT_NOT_NULL(signature->params);
    TEST_ASSERT_EQUAL_UINT(parameterCount, signature->params->count);
    TEST_ASSERT_NOT_NULL(signature->returnType);
    if (markedReturn) {
        SZrAstNode *identifier = signature->returnType->name;

        TEST_ASSERT_NOT_NULL(identifier);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, identifier->type);
        TEST_ASSERT_EQUAL_STRING("Widget",
                ZrCore_String_GetNativeString(identifier->data.identifier.name));
        check_child_conversion(signature->returnType, ZR_TRUE, ZR_VALUE_TYPE_OBJECT);
        identifier->data.identifier.syntaxContext = g_definition;
        TEST_ASSERT_NULL(ZrParser_TypeEnvironment_FindTypeBindingWithContext(
                g_compiler.typeEnv, identifier->data.identifier.name, g_definition));
    }
    /* Warm children and diagnostic storage without invoking the leaking signature. */
    for (TZrSize index = 0U; index < parameterCount; index++) {
        SZrAstNode *parameter = signature->params->nodes[index];
        SZrType *parameterType;
        EZrValueType expectedType;

        TEST_ASSERT_NOT_NULL(parameter);
        TEST_ASSERT_EQUAL_INT(ZR_AST_PARAMETER, parameter->type);
        TEST_ASSERT_EQUAL_INT(expectedModes != ZR_NULL ? expectedModes[index] :
                ZR_PARAMETER_PASSING_MODE_VALUE,
                parameter->data.parameter.passingMode);
        parameterType = parameter->data.parameter.typeInfo;
        TEST_ASSERT_NOT_NULL(parameterType);
        TEST_ASSERT_NOT_NULL(parameterType->name);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, parameterType->name->type);
        expectedType = strcmp(ZrCore_String_GetNativeString(
                parameterType->name->data.identifier.name), "string") == 0 ?
                ZR_VALUE_TYPE_STRING : ZR_VALUE_TYPE_INT64;
        if (parameterType->dimensions > 0) {
            compositeParameter = ZR_TRUE;
            expectedType = ZR_VALUE_TYPE_ARRAY;
        }
        check_child_conversion(parameterType,
                !(missingParameter && index == 0U), expectedType);
    }
    check_child_conversion(signature->returnType,
            !missingReturn && !markedReturn, ZR_VALUE_TYPE_INT64);
    if (success) {
        TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
                &g_compiler, annotation, &g_result));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_CLOSURE, g_result.baseType);
        TEST_ASSERT_EQUAL_STRING(expectedDisplay,
                ZrCore_String_GetNativeString(g_result.typeName));
        ZrParser_InferredType_Free(g_state, &g_result);
    }
    clear_error_flags();
    begin_probe();
    converted = ZrParser_AstTypeToInferredType_Convert(
            &g_compiler, annotation, &g_result);
    ZrParser_InferredType_Free(g_state, &g_result);
    end_probe();
    TEST_ASSERT_EQUAL_INT(success, converted);
    TEST_ASSERT_EQUAL_INT(!success, g_compiler.hasError);
    if (markedReturn || compositeParameter) {
        /* Queries and composite parameters add owned arrays beyond the signature pair. */
        TEST_ASSERT_TRUE(g_probe.allocated >= 2U);
        TEST_ASSERT_TRUE(g_probe.allocatedBytes >= capacity *
                (sizeof(SZrInferredType) + sizeof(EZrParameterPassingMode)));
    } else {
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(2U, g_probe.allocated,
                "Root signature conversion must allocate only its two parameter arrays");
        TEST_ASSERT_EQUAL_UINT64(capacity * (sizeof(SZrInferredType) +
                sizeof(EZrParameterPassingMode)), g_probe.allocatedBytes);
    }
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(g_probe.allocated, g_probe.freed,
            "signature conversion must release both parameter arrays on every exit");
    TEST_ASSERT_EQUAL_UINT64(g_probe.allocatedBytes, g_probe.freedBytes);
}

static void test_valid_single_parameter_releases_arrays(void) {
    assert_conversion_releases_arrays("var callback: fn(int) -> int;",
            1U, ZR_FALSE, ZR_FALSE, ZR_FALSE, "fn(int) -> int", ZR_NULL);
}

static void test_valid_two_parameters_release_arrays(void) {
    assert_conversion_releases_arrays("var callback: fn(int, string) -> int;",
            2U, ZR_FALSE, ZR_FALSE, ZR_FALSE, "fn(int, string) -> int", ZR_NULL);
}

static void test_missing_parameter_releases_arrays(void) {
    assert_conversion_releases_arrays("var callback: fn(Missing) -> int;",
            1U, ZR_TRUE, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL);
}

static void test_missing_return_releases_arrays(void) {
    assert_conversion_releases_arrays("var callback: fn(int) -> Missing;",
            1U, ZR_FALSE, ZR_TRUE, ZR_FALSE, ZR_NULL, ZR_NULL);
}

static void test_missing_return_without_parameters_releases_arrays(void) {
    assert_conversion_releases_arrays("var callback: fn() -> Missing;",
            0U, ZR_FALSE, ZR_TRUE, ZR_FALSE, ZR_NULL, ZR_NULL);
}

static void test_missing_return_with_two_parameters_releases_arrays(void) {
    assert_conversion_releases_arrays("var callback: fn(int, string) -> Missing;",
            2U, ZR_FALSE, ZR_TRUE, ZR_FALSE, ZR_NULL, ZR_NULL);
}

static void test_unavailable_marked_return_releases_arrays(void) {
    assert_conversion_releases_arrays(
            "struct Widget {} var callback: fn(int) -> Widget;",
            1U, ZR_FALSE, ZR_FALSE, ZR_TRUE, ZR_NULL, ZR_NULL);
}

static void test_array_parameter_released_after_missing_return(void) {
    assert_conversion_releases_arrays("var callback: fn(int[]) -> Missing;",
            1U, ZR_FALSE, ZR_TRUE, ZR_FALSE, ZR_NULL, ZR_NULL);
}

static void test_passing_modes_preserve_display_and_release_arrays(void) {
    const EZrParameterPassingMode modes[] = {
        ZR_PARAMETER_PASSING_MODE_IN,
        ZR_PARAMETER_PASSING_MODE_REF,
        ZR_PARAMETER_PASSING_MODE_OUT
    };

    assert_conversion_releases_arrays(
            "var callback: fn(in int, ref int, out string) -> int;",
            3U, ZR_FALSE, ZR_FALSE, ZR_FALSE,
            "fn(in int, ref int, out string) -> int", modes);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_single_parameter_releases_arrays);
    RUN_TEST(test_valid_two_parameters_release_arrays);
    RUN_TEST(test_missing_parameter_releases_arrays);
    RUN_TEST(test_missing_return_releases_arrays);
    RUN_TEST(test_missing_return_without_parameters_releases_arrays);
    RUN_TEST(test_missing_return_with_two_parameters_releases_arrays);
    RUN_TEST(test_unavailable_marked_return_releases_arrays);
    RUN_TEST(test_array_parameter_released_after_missing_return);
    RUN_TEST(test_passing_modes_preserve_display_and_release_arrays);
    return UNITY_END();
}
