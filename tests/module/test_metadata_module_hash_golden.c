#include "unity.h"

#include "runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/compiler.h"

#include <string.h>

TZrBool compiler_build_function_metadata_tokens(SZrCompilerState *cs, SZrFunction *function);

void setUp(void) {}

void tearDown(void) {}

static void init_base_type_ref(SZrFunctionTypedTypeRef *typeRef, EZrValueType baseType) {
    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = baseType;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
}

static void init_named_object_type_ref(SZrState *state, SZrFunctionTypedTypeRef *typeRef, const char *typeName) {
    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->typeName = ZrCore_String_CreateFromNative(state, (TZrNativeString)typeName);
    TEST_ASSERT_NOT_NULL(typeRef->typeName);
}

static SZrFunctionTypedExportSymbol *init_single_public_function_export(
        SZrState *state,
        SZrFunction *function,
        const char *exportName,
        TZrUInt32 parameterCount) {
    SZrFunctionTypedExportSymbol *symbol;

    function->typedExportedSymbols = (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedExportSymbol),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->typedExportedSymbols);
    ZrCore_Memory_RawSet(function->typedExportedSymbols, 0, sizeof(SZrFunctionTypedExportSymbol));
    function->typedExportedSymbolLength = 1u;

    symbol = &function->typedExportedSymbols[0];
    symbol->name = ZrCore_String_CreateFromNative(state, (TZrNativeString)exportName);
    TEST_ASSERT_NOT_NULL(symbol->name);
    symbol->accessModifier = ZR_ACCESS_PUBLIC;
    symbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    symbol->exportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
    symbol->readiness = ZR_MODULE_EXPORT_READY_DECLARATION;
    symbol->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
    symbol->parameterCount = parameterCount;
    if (parameterCount > 0u) {
        symbol->parameterTypes = (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(
                state->global,
                sizeof(SZrFunctionTypedTypeRef) * parameterCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        TEST_ASSERT_NOT_NULL(symbol->parameterTypes);
        ZrCore_Memory_RawSet(symbol->parameterTypes, 0, sizeof(SZrFunctionTypedTypeRef) * parameterCount);
    }
    return symbol;
}

static SZrFunction *create_sum_module_fixture(SZrState *state) {
    SZrFunction *function;
    SZrFunctionTypedExportSymbol *symbol;
    SZrCompilerState compilerState;

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->functionName = ZrCore_String_CreateFromNative(state, "__entry");
    function->moduleVersion = ZrCore_String_CreateFromNative(state, "1.0.0");
    TEST_ASSERT_NOT_NULL(function->functionName);
    TEST_ASSERT_NOT_NULL(function->moduleVersion);

    symbol = init_single_public_function_export(state, function, "sum", 2u);
    init_base_type_ref(&symbol->valueType, ZR_VALUE_TYPE_INT64);
    init_base_type_ref(&symbol->parameterTypes[0], ZR_VALUE_TYPE_INT64);
    init_base_type_ref(&symbol->parameterTypes[1], ZR_VALUE_TYPE_INT64);

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = function;
    TEST_ASSERT_TRUE(compiler_build_function_metadata_tokens(&compilerState, function));
    return function;
}

static SZrFunction *create_union_module_fixture(SZrState *state, SZrAstNode *scriptAst) {
    SZrFunction *function;
    SZrFunctionTypedExportSymbol *symbol;
    SZrCompilerState compilerState;

    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->functionName = ZrCore_String_CreateFromNative(state, "__entry");
    function->moduleVersion = ZrCore_String_CreateFromNative(state, "1.0.0");
    TEST_ASSERT_NOT_NULL(function->functionName);
    TEST_ASSERT_NOT_NULL(function->moduleVersion);

    symbol = init_single_public_function_export(state, function, "choose", 0u);
    init_named_object_type_ref(state, &symbol->valueType, "Option<int>");

    ZrCore_Memory_RawSet(&compilerState, 0, sizeof(compilerState));
    compilerState.state = state;
    compilerState.currentFunction = function;
    compilerState.scriptAst = scriptAst;
    TEST_ASSERT_TRUE(compiler_build_function_metadata_tokens(&compilerState, function));
    return function;
}

static void test_sum_module_signature_hash_matches_golden(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = create_sum_module_fixture(state);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_HEX64((TZrUInt64)0xE701BC33ECB6BF89ULL, function->moduleSignatureHash);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_union_module_signature_hash_matches_golden(void) {
    const char *source =
            "union Option<T> {\n"
            "    None;\n"
            "    Some(value: T);\n"
            "}\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *scriptAst;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_CreateFromNative(state, "metadata_module_hash_golden.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    scriptAst = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(scriptAst);

    function = create_union_module_fixture(state, scriptAst);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_HEX64((TZrUInt64)0x485AE44EE06010E4ULL, function->moduleSignatureHash);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, scriptAst);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sum_module_signature_hash_matches_golden);
    RUN_TEST(test_union_module_signature_hash_matches_golden);
    return UNITY_END();
}
