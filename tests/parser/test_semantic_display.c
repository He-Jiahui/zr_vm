#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_display.h"
#include "zr_vm_parser/semantic_facts.h"

static SZrState *g_state;

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

static SZrFileRange display_range(TZrSize offset) {
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    range.start.offset = offset;
    range.end.offset = offset + 1U;
    range.start.line = 1;
    range.end.line = 1;
    range.start.column = (TZrInt32)offset + 1;
    range.end.column = range.start.column + 1;
    return range;
}

static void test_semantic_display_formats_canonical_type_symbol_and_property(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrCanonicalParameterContract parameter;
    SZrSemanticPropertyContract property;
    TZrTypeId intType;
    TZrTypeId readonlyResultType;
    TZrTypeId callableType;
    TZrSymbolId callableSymbol;
    TZrSymbolId invalidCallableSymbol;
    TZrSymbolId propertySymbol;
    TZrChar buffer[256];

    TEST_ASSERT_NOT_NULL(context);
    intType = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    readonlyResultType = ZrParser_CanonicalType_InternRef(
            context, intType, ZR_CANONICAL_REF_READONLY);
    memset(&parameter, 0, sizeof(parameter));
    parameter.typeId = readonlyResultType;
    parameter.passingForm = ZR_CANONICAL_PASSING_IN;
    parameter.escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameter.entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    parameter.exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    parameter.acceptsTemporary = ZR_TRUE;
    callableType = ZrParser_CanonicalType_InternFunction(
            context,
            &parameter,
            1U,
            readonlyResultType,
            ZR_CANONICAL_RECEIVER_READONLY,
            ZR_CANONICAL_CALLABLE_EFFECT_THROWS);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, callableType);

    callableSymbol = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "inspect"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callableType,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            display_range(10U));
    propertySymbol = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "value"),
            ZR_SEMANTIC_SYMBOL_KIND_PROPERTY,
            intType,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            display_range(20U));
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, callableSymbol);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, propertySymbol);
    invalidCallableSymbol = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "notCallable"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            intType,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            display_range(15U));
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, invalidCallableSymbol);

    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
            context, callableType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("const fn(in int) -> ref readonly int throws", buffer);
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatSymbol(
            context, callableSymbol, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING(
            "inspect: const fn(in int) -> ref readonly int throws", buffer);

    memset(&property, 0, sizeof(property));
    property.propertySymbolId = propertySymbol;
    property.propertyTypeId = intType;
    property.getterSymbolId = callableSymbol;
    property.receiverEffect = ZR_CANONICAL_RECEIVER_READONLY;
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatProperty(
            context, &property, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("const property value: int { get; }", buffer);
    property.receiverEffect = (EZrCanonicalReceiverEffect)99;
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatProperty(
            context, &property, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    property.receiverEffect = ZR_CANONICAL_RECEIVER_READONLY;
    property.referenceAccess = (EZrReferenceAccess)99;
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatProperty(
            context, &property, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    property.referenceAccess = ZR_REFERENCE_ACCESS_NONE;
    property.getterSymbolId = invalidCallableSymbol;
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatProperty(
            context, &property, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_display_fails_closed_for_missing_identity(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticPropertyContract property;
    TZrChar buffer[32];

    TEST_ASSERT_NOT_NULL(context);
    memset(&property, 0, sizeof(property));
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatType(
            context, ZR_SEMANTIC_ID_INVALID, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatSymbol(
            context, ZR_SEMANTIC_ID_INVALID, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatProperty(
            context, &property, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_display_uses_matching_declaration_signature_fact(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticReferenceFact declaration;
    TZrTypeId intType;
    TZrTypeId callableType;
    TZrSymbolId symbolId;
    TZrChar buffer[128];

    TEST_ASSERT_NOT_NULL(context);
    intType = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    callableType = ZrParser_CanonicalType_InternFunction(
            context,
            ZR_NULL,
            0U,
            intType,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    symbolId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "identity"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callableType,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            display_range(30U));
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, symbolId);

    memset(&declaration, 0, sizeof(declaration));
    declaration.kind = ZR_SEMANTIC_REFERENCE_DECLARATION;
    declaration.symbolId = symbolId;
    declaration.typeId = callableType;
    declaration.range = display_range(30U);
    declaration.declarationRange = declaration.range;
    declaration.name = ZrCore_String_CreateFromNative(g_state, "identity");
    declaration.signatureDisplay = ZrCore_String_CreateFromNative(
            g_state, "fn identity<T>(value: int): int");
    declaration.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &declaration));

    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatSymbol(
            context, symbolId, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("fn identity<T>(value: int): int", buffer);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_documentation_projects_exact_symbol_fact(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrString *documentation;
    TZrTypeId intType;
    TZrSymbolId documentedSymbol;
    TZrSymbolId sameNameSymbol;

    TEST_ASSERT_NOT_NULL(context);
    intType = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    documentedSymbol = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "measure"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            intType,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            display_range(40U));
    sameNameSymbol = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "measure"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            intType,
            ZR_SEMANTIC_ID_INVALID,
            ZR_NULL,
            display_range(50U));
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, documentedSymbol);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, sameNameSymbol);
    TEST_ASSERT_NOT_EQUAL_UINT32(documentedSymbol, sameNameSymbol);

    TEST_ASSERT_TRUE(ZrParser_SemanticDocumentation_Publish(
            context,
            documentedSymbol,
            ZrCore_String_CreateFromNative(g_state, "Returns the exact measurement.")));
    documentation = ZrParser_SemanticQuery_DocumentationOfSymbol(
            context, documentedSymbol);
    TEST_ASSERT_NOT_NULL(documentation);
    TEST_ASSERT_EQUAL_STRING(
            "Returns the exact measurement.",
            ZrCore_String_GetNativeString(documentation));
    TEST_ASSERT_NULL(ZrParser_SemanticQuery_DocumentationOfSymbol(
            context, sameNameSymbol));

    TEST_ASSERT_TRUE(ZrParser_SemanticDocumentation_Publish(
            context,
            documentedSymbol,
            ZrCore_String_CreateFromNative(g_state, "Returns the exact measurement.")));
    TEST_ASSERT_FALSE(ZrParser_SemanticDocumentation_Publish(
            context,
            documentedSymbol,
            ZrCore_String_CreateFromNative(g_state, "Conflicting documentation.")));
    TEST_ASSERT_FALSE(ZrParser_SemanticDocumentation_Publish(
            context,
            ZR_SEMANTIC_ID_INVALID,
            ZrCore_String_CreateFromNative(g_state, "Invalid identity.")));
    TEST_ASSERT_NULL(ZrParser_SemanticQuery_DocumentationOfSymbol(
            context, ZR_SEMANTIC_ID_INVALID));

    ZrParser_SemanticContext_Reset(context);
    TEST_ASSERT_NULL(ZrParser_SemanticQuery_DocumentationOfSymbol(
            context, documentedSymbol));
    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_display_rejects_malformed_canonical_contracts(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    TZrTypeId intType;
    TZrTypeId refType;
    TZrTypeId callableType;
    SZrCanonicalParameterContract parameter;
    SZrCanonicalTypeNode *refNode;
    SZrCanonicalTypeNode *callableNode;
    SZrCanonicalParameterContract *storedParameter;
    TZrChar buffer[128];

    TEST_ASSERT_NOT_NULL(context);
    intType = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    refType = ZrParser_CanonicalType_InternRef(
            context, intType, ZR_CANONICAL_REF_WRITABLE);
    memset(&parameter, 0, sizeof(parameter));
    parameter.typeId = intType;
    parameter.passingForm = ZR_CANONICAL_PASSING_VALUE;
    parameter.escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameter.entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    parameter.exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    parameter.acceptsTemporary = ZR_TRUE;
    callableType = ZrParser_CanonicalType_InternFunction(
            context,
            &parameter,
            1U,
            intType,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, refType);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, callableType);
    refNode = (SZrCanonicalTypeNode *)ZrParser_CanonicalType_Find(context, refType);
    callableNode = (SZrCanonicalTypeNode *)ZrParser_CanonicalType_Find(
            context, callableType);
    TEST_ASSERT_NOT_NULL(refNode);
    TEST_ASSERT_NOT_NULL(callableNode);
    storedParameter = (SZrCanonicalParameterContract *)ZrCore_Array_Get(
            &callableNode->data.function.parameterContracts, 0U);
    TEST_ASSERT_NOT_NULL(storedParameter);

    callableNode->data.function.receiverEffect = (EZrCanonicalReceiverEffect)99;
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatType(
            context, callableType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    callableNode->data.function.receiverEffect = ZR_CANONICAL_RECEIVER_NONE;

    callableNode->data.function.effectFlags = ((TZrUInt32)1U << 31U);
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatType(
            context, callableType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    callableNode->data.function.effectFlags = ZR_CANONICAL_CALLABLE_EFFECT_NONE;

    storedParameter->escapeUpperBound = ZR_CANONICAL_ESCAPE_BLOCK;
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatType(
            context, callableType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    storedParameter->escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;

    refNode->data.refType.access = (EZrCanonicalRefAccess)99;
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatType(
            context, refType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    refNode->data.refType.access = ZR_CANONICAL_REF_WRITABLE;

    ZrParser_SemanticContext_Free(context);
}

static void test_callable_signature_rejects_malformed_canonical_contracts(void) {
    const TZrChar *source = "fn inspect(value: int): int { return value; }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_display_callable_integrity.zr");
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrSemanticContext *context;
    SZrCanonicalParameterContract parameter;
    SZrCanonicalTypeNode *callableNode;
    SZrCanonicalParameterContract *storedParameter;
    const SZrSemanticSymbolRecord *symbol;
    SZrString *signature;
    TZrTypeId intType;
    TZrTypeId callableType;
    TZrSymbolId symbolId;

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(1U, ast->data.script.statements->count);
    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.functionDeclaration.params);
    TEST_ASSERT_EQUAL_UINT(1U, declaration->data.functionDeclaration.params->count);
    TEST_ASSERT_NOT_NULL(declaration->data.functionDeclaration.params->nodes[0]);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_PARAMETER,
            declaration->data.functionDeclaration.params->nodes[0]->type);
    TEST_ASSERT_NOT_NULL(
            declaration->data.functionDeclaration.params->nodes[0]->data.parameter.name);
    TEST_ASSERT_NOT_NULL(
            declaration->data.functionDeclaration.params->nodes[0]->data.parameter.name->name);

    context = ZrParser_SemanticContext_New(g_state);
    TEST_ASSERT_NOT_NULL(context);
    intType = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    memset(&parameter, 0, sizeof(parameter));
    parameter.typeId = intType;
    parameter.passingForm = ZR_CANONICAL_PASSING_VALUE;
    parameter.escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameter.entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    parameter.exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    parameter.acceptsTemporary = ZR_TRUE;
    callableType = ZrParser_CanonicalType_InternFunction(
            context,
            &parameter,
            1U,
            intType,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    symbolId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "inspect"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callableType,
            ZR_SEMANTIC_ID_INVALID,
            declaration,
            declaration->location);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, symbolId);
    symbol = ZrParser_Semantic_FindSymbolById(context, symbolId);
    TEST_ASSERT_NOT_NULL(symbol);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, symbol->kind);
    TEST_ASSERT_EQUAL_UINT32(callableType, symbol->typeId);
    TEST_ASSERT_EQUAL_PTR(declaration, symbol->astNode);
    TEST_ASSERT_NOT_NULL(symbol->name);
    callableNode = (SZrCanonicalTypeNode *)ZrParser_CanonicalType_Find(
            context, callableType);
    TEST_ASSERT_NOT_NULL(callableNode);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callableNode->kind);
    TEST_ASSERT_EQUAL_UINT(1U, callableNode->data.function.parameterContracts.length);
    storedParameter = (SZrCanonicalParameterContract *)ZrCore_Array_Get(
            &callableNode->data.function.parameterContracts, 0U);
    TEST_ASSERT_NOT_NULL(storedParameter);

    signature = ZrParser_SemanticDisplay_CreateCallableSignature(context, symbolId);
    TEST_ASSERT_NOT_NULL(signature);
    TEST_ASSERT_EQUAL_STRING(
            "inspect(value: int): int", ZrCore_String_GetNativeString(signature));

    callableNode->data.function.receiverEffect = (EZrCanonicalReceiverEffect)99;
    TEST_ASSERT_NULL(ZrParser_SemanticDisplay_CreateCallableSignature(
            context, symbolId));
    callableNode->data.function.receiverEffect = ZR_CANONICAL_RECEIVER_NONE;

    callableNode->data.function.effectFlags = ((TZrUInt32)1U << 31U);
    TEST_ASSERT_NULL(ZrParser_SemanticDisplay_CreateCallableSignature(
            context, symbolId));
    callableNode->data.function.effectFlags = ZR_CANONICAL_CALLABLE_EFFECT_NONE;

    storedParameter->acceptsTemporary = ZR_FALSE;
    TEST_ASSERT_NULL(ZrParser_SemanticDisplay_CreateCallableSignature(
            context, symbolId));
    storedParameter->acceptsTemporary = ZR_TRUE;

    storedParameter->passingForm = (EZrCanonicalPassingForm)99;
    TEST_ASSERT_NULL(ZrParser_SemanticDisplay_CreateCallableSignature(
            context, symbolId));
    storedParameter->passingForm = ZR_CANONICAL_PASSING_VALUE;

    ZrParser_SemanticContext_Free(context);
    ZrParser_Ast_Free(g_state, ast);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_semantic_display_formats_canonical_type_symbol_and_property);
    RUN_TEST(test_semantic_display_fails_closed_for_missing_identity);
    RUN_TEST(test_semantic_display_uses_matching_declaration_signature_fact);
    RUN_TEST(test_semantic_documentation_projects_exact_symbol_fact);
    RUN_TEST(test_semantic_display_rejects_malformed_canonical_contracts);
    RUN_TEST(test_callable_signature_rejects_malformed_canonical_contracts);
    return UNITY_END();
}
