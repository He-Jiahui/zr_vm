#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_display.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_binding.h"

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

static void test_semantic_type_display_alias_is_use_site_scoped(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrString *source = ZrCore_String_CreateFromNative(g_state, "alias_scope.zr");
    SZrString *equivalentSource = ZrCore_String_CreateFromNative(g_state, "alias_scope.zr");
    SZrString *otherSource = ZrCore_String_CreateFromNative(g_state, "other_scope.zr");
    SZrString *alias = ZrCore_String_CreateFromNative(g_state, "Index");
    SZrString *conflictingAlias = ZrCore_String_CreateFromNative(g_state, "Count");
    SZrString *emptyAlias = ZrCore_String_Create(g_state, "", 0U);
    SZrFileRange range = display_range(30U);
    SZrFileRange equivalentRange = display_range(30U);
    SZrFileRange otherRange = display_range(30U);
    SZrFileRange shiftedRange = display_range(31U);
    TZrTypeId intType;
    TZrTypeId boolType;
    SZrString *queriedAlias;
    TZrChar buffer[32];

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(equivalentSource);
    TEST_ASSERT_NOT_NULL(otherSource);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_NOT_NULL(conflictingAlias);
    TEST_ASSERT_NOT_NULL(emptyAlias);
    range.source = source;
    equivalentRange.source = equivalentSource;
    otherRange.source = otherSource;
    shiftedRange.source = source;
    intType = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    boolType = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_BOOL);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, intType);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, boolType);

    TEST_ASSERT_TRUE(ZrParser_SemanticTypeDisplayAlias_Publish(
            context, intType, &range, alias));
    queriedAlias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            context, intType, &equivalentRange);
    TEST_ASSERT_NOT_NULL(queriedAlias);
    TEST_ASSERT_EQUAL_STRING("Index", ZrCore_String_GetNativeString(queriedAlias));
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
            context, intType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("int", buffer);

    TEST_ASSERT_TRUE(ZrParser_SemanticTypeDisplayAlias_Publish(
            context, intType, &equivalentRange, alias));
    TEST_ASSERT_FALSE(ZrParser_SemanticTypeDisplayAlias_Publish(
            context, intType, &range, conflictingAlias));
    TEST_ASSERT_FALSE(ZrParser_SemanticTypeDisplayAlias_Publish(
            context, intType, &range, emptyAlias));
    TEST_ASSERT_NULL(ZrParser_SemanticQuery_TypeDisplayAliasAt(
            context, boolType, &range));
    TEST_ASSERT_NULL(ZrParser_SemanticQuery_TypeDisplayAliasAt(
            context, intType, &otherRange));
    TEST_ASSERT_NULL(ZrParser_SemanticQuery_TypeDisplayAliasAt(
            context, intType, &shiftedRange));

    ZrParser_SemanticContext_Reset(context);
    TEST_ASSERT_NULL(ZrParser_SemanticQuery_TypeDisplayAliasAt(
            context, intType, &range));
    ZrParser_SemanticContext_Free(context);
}

static void test_primitive_type_use_publishes_source_display_alias(void) {
    const TZrChar *source = "var index: i64 = 0;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "primitive_alias_display.zr");
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrCompilerState cs;
    SZrInferredType inferred;
    TZrTypeId typeId;
    SZrString *alias;
    TZrChar buffer[32];

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(1U, ast->data.script.statements->count);
    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.typeInfo);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.typeInfo->name);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    ZrParser_InferredType_Init(g_state, &inferred, ZR_VALUE_TYPE_UNKNOWN);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &cs, declaration->data.variableDeclaration.typeInfo, &inferred));
    typeId = ZrParser_CanonicalType_FromInferred(cs.semanticContext, &inferred);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, typeId);
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
            cs.semanticContext, typeId, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("int", buffer);

    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            cs.semanticContext,
            typeId,
            &declaration->data.variableDeclaration.typeInfo->name->location);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_EQUAL_STRING("i64", ZrCore_String_GetNativeString(alias));

    ZrParser_InferredType_Free(g_state, &inferred);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_owner_inner_type_use_publishes_source_display_alias(void) {
    const TZrChar *source = "var handle: Unique<i64> = null;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "owner_inner_alias_display.zr");
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrGenericType *genericType;
    SZrAstNode *argumentNode;
    SZrCompilerState cs;
    SZrInferredType inferred;
    TZrTypeId intTypeId;
    TZrTypeId ownerTypeId;
    SZrString *alias;
    TZrChar buffer[64];

    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(1U, ast->data.script.statements->count);
    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.typeInfo);
    TEST_ASSERT_NOT_NULL(declaration->data.variableDeclaration.typeInfo->name);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_GENERIC_TYPE,
            declaration->data.variableDeclaration.typeInfo->name->type);
    genericType = &declaration->data.variableDeclaration.typeInfo->name->data.genericType;
    TEST_ASSERT_NOT_NULL(genericType->params);
    TEST_ASSERT_EQUAL_UINT(1U, genericType->params->count);
    argumentNode = genericType->params->nodes[0];
    TEST_ASSERT_NOT_NULL(argumentNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, argumentNode->type);
    TEST_ASSERT_NOT_NULL(argumentNode->data.type.name);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);
    ZrParser_InferredType_Init(g_state, &inferred, ZR_VALUE_TYPE_UNKNOWN);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &cs, declaration->data.variableDeclaration.typeInfo, &inferred));
    ownerTypeId = ZrParser_CanonicalType_FromInferred(cs.semanticContext, &inferred);
    intTypeId = ZrParser_CanonicalType_InternPrimitive(
            cs.semanticContext, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, ownerTypeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, intTypeId);
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
            cs.semanticContext, ownerTypeId, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("Unique<int>", buffer);

    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            cs.semanticContext,
            intTypeId,
            &argumentNode->data.type.name->location);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_EQUAL_STRING("i64", ZrCore_String_GetNativeString(alias));

    ZrParser_InferredType_Free(g_state, &inferred);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

#include "test_semantic_display_generic_alias_cases.h"
#include "test_semantic_display_nominal_alias_cases.h"

static void test_qualified_type_use_publishes_source_display_alias(void) {
    const TZrChar *source = "var patch: declaration.Patch = null;\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "qualified_alias_display.zr");
    SZrString *bindingName = ZrCore_String_CreateFromNative(
            g_state, "declaration");
    const SZrParserCompileToolModuleDescriptor *provider =
            ZrParser_CompileTool_FindModule("zr.compile.declaration");
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrType *typeUse;
    SZrType *terminalType;
    SZrFileRange useRange;
    SZrCompilerState cs;
    SZrInferredType inferred;
    TZrTypeId typeId;
    SZrString *alias;
    TZrChar buffer[96];

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_NOT_NULL(bindingName);
    TEST_ASSERT_NOT_NULL(provider);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT(1U, ast->data.script.statements->count);
    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);
    typeUse = declaration->data.variableDeclaration.typeInfo;
    TEST_ASSERT_NOT_NULL(typeUse);
    TEST_ASSERT_NOT_NULL(typeUse->name);
    TEST_ASSERT_NOT_NULL(typeUse->subType);
    TEST_ASSERT_NOT_NULL(typeUse->subType->name);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    TEST_ASSERT_TRUE(ZrParser_CompileToolBinding_DeclareProvider(
            &cs, bindingName, provider));
    ZrParser_InferredType_Init(g_state, &inferred, ZR_VALUE_TYPE_UNKNOWN);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &cs, typeUse, &inferred));
    typeId = ZrParser_CanonicalType_FromInferred(cs.semanticContext, &inferred);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, typeId);
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
            cs.semanticContext, typeId, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("zr.compile.declaration.Patch", buffer);

    terminalType = typeUse;
    while (terminalType->subType != ZR_NULL) {
        terminalType = terminalType->subType;
    }
    useRange = typeUse->name->location;
    useRange.end = terminalType->name->location.end;
    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            cs.semanticContext, typeId, &useRange);
    TEST_ASSERT_NOT_NULL(alias);
    TEST_ASSERT_EQUAL_STRING(
            "declaration.Patch", ZrCore_String_GetNativeString(alias));

    ZrParser_InferredType_Free(g_state, &inferred);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_semantic_display_separates_const_parameter_alias_from_identity(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrCanonicalGenericArgument arguments[3];
    TZrTypeId intType;
    TZrTypeId matrixDefinition;
    TZrTypeId firstType;
    TZrTypeId sameType;
    TZrChar buffer[128];

    TEST_ASSERT_NOT_NULL(context);
    intType = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    matrixDefinition = ZrParser_CanonicalType_InternNominal(
            context,
            ZrCore_String_CreateFromNative(g_state, "app.types"),
            ZrCore_String_CreateFromNative(g_state, "Matrix"),
            0x02000051U);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, matrixDefinition);

    memset(arguments, 0, sizeof(arguments));
    arguments[0].kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
    arguments[0].data.typeId = intType;
    arguments[1].kind = ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT;
    arguments[1].data.constIntValue = 4;
    arguments[2].kind = ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER;
    arguments[2].data.constParameter.ownerSymbolId = 77U;
    arguments[2].data.constParameter.ordinal = 2U;
    arguments[2].data.constParameter.displayName =
            ZrCore_String_CreateFromNative(g_state, "N");
    firstType = ZrParser_CanonicalType_InternGenericInstanceEx(
            context, matrixDefinition, arguments, 3U);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, firstType);

    arguments[2].data.constParameter.displayName =
            ZrCore_String_CreateFromNative(g_state, "OtherAlias");
    sameType = ZrParser_CanonicalType_InternGenericInstanceEx(
            context, matrixDefinition, arguments, 3U);
    TEST_ASSERT_EQUAL_UINT32(firstType, sameType);
    TEST_ASSERT_TRUE(ZrParser_SemanticDisplay_FormatType(
            context, firstType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING(
            "app.types.Matrix<int, 4, $const(77,2)>", buffer);

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_display_rejects_malformed_composite_shapes(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrCanonicalTypeNode *genericParameterNode;
    SZrCanonicalTypeNode *genericInstanceNode;
    SZrCanonicalTypeNode *arrayNode;
    SZrCanonicalTypeNode *unionNode;
    TZrTypeId intType;
    TZrTypeId genericDefinition;
    TZrTypeId genericParameterType;
    TZrTypeId genericArguments[1];
    TZrTypeId genericInstanceType;
    TZrTypeId arrayType;
    TZrTypeId unionType;
    TZrSize storedLength;
    TZrChar buffer[128];

    TEST_ASSERT_NOT_NULL(context);
    intType = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    genericDefinition = ZrParser_CanonicalType_InternNominal(
            context,
            ZrCore_String_CreateFromNative(g_state, "app.types"),
            ZrCore_String_CreateFromNative(g_state, "Box"),
            0x02000052U);
    genericParameterType = ZrParser_CanonicalType_InternGenericParameter(
            context, 81U, 0U);
    genericArguments[0] = intType;
    genericInstanceType = ZrParser_CanonicalType_InternGenericInstance(
            context, genericDefinition, genericArguments, 1U);
    arrayType = ZrParser_CanonicalType_InternArray(
            context, intType, 1U, ZR_CANONICAL_ARRAY_STORAGE_MANAGED);
    unionType = ZrParser_CanonicalType_InternUnion(
            context, genericDefinition, genericArguments, 1U);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, genericDefinition);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, genericParameterType);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, genericInstanceType);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, arrayType);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, unionType);

    genericParameterNode = (SZrCanonicalTypeNode *)ZrParser_CanonicalType_Find(
            context, genericParameterType);
    genericInstanceNode = (SZrCanonicalTypeNode *)ZrParser_CanonicalType_Find(
            context, genericInstanceType);
    arrayNode = (SZrCanonicalTypeNode *)ZrParser_CanonicalType_Find(context, arrayType);
    unionNode = (SZrCanonicalTypeNode *)ZrParser_CanonicalType_Find(context, unionType);
    TEST_ASSERT_NOT_NULL(genericParameterNode);
    TEST_ASSERT_NOT_NULL(genericInstanceNode);
    TEST_ASSERT_NOT_NULL(arrayNode);
    TEST_ASSERT_NOT_NULL(unionNode);

    genericParameterNode->data.genericParameter.ownerSymbolId = ZR_SEMANTIC_ID_INVALID;
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatType(
            context, genericParameterType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    genericParameterNode->data.genericParameter.ownerSymbolId = 81U;

    storedLength = genericInstanceNode->data.genericInstance.arguments.length;
    genericInstanceNode->data.genericInstance.arguments.length = 0U;
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatType(
            context, genericInstanceType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    genericInstanceNode->data.genericInstance.arguments.length = storedLength;

    arrayNode->data.array.rank = 0U;
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatType(
            context, arrayType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    arrayNode->data.array.rank = 1U;
    arrayNode->data.array.storageKind = (EZrCanonicalArrayStorageKind)99;
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatType(
            context, arrayType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    arrayNode->data.array.storageKind = ZR_CANONICAL_ARRAY_STORAGE_MANAGED;

    storedLength = unionNode->data.unionType.variantTypeIds.length;
    unionNode->data.unionType.variantTypeIds.length = 0U;
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatType(
            context, unionType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);
    unionNode->data.unionType.variantTypeIds.length = storedLength;

    ZrParser_SemanticContext_Free(context);
}

static void test_semantic_display_rejects_empty_nominal_identity(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrString *moduleIdentity = ZrCore_String_CreateFromNative(g_state, "app.model");
    SZrString *emptyName = ZrCore_String_Create(g_state, "", 0U);
    TZrTypeId emptyType;
    TZrTypeId validType;
    SZrCanonicalTypeNode *validNode;
    SZrString *validName;
    TZrChar buffer[64];

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(moduleIdentity);
    TEST_ASSERT_NOT_NULL(emptyName);
    emptyType = ZrParser_CanonicalType_InternNominal(
            context, moduleIdentity, emptyName, 0x02000041U);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, emptyType);

    validName = ZrCore_String_CreateFromNative(g_state, "Document");
    TEST_ASSERT_NOT_NULL(validName);
    validType = ZrParser_CanonicalType_InternNominal(
            context, moduleIdentity, validName, 0x02000042U);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, validType);
    validNode = (SZrCanonicalTypeNode *)ZrParser_CanonicalType_Find(context, validType);
    TEST_ASSERT_NOT_NULL(validNode);
    validNode->data.nominal.name = emptyName;
    strcpy(buffer, "sentinel");
    TEST_ASSERT_FALSE(ZrParser_SemanticDisplay_FormatType(
            context, validType, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING("", buffer);

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

static void test_callable_signature_formats_canonical_effects_and_passing_modes(void) {
    const TZrChar *source =
            "fn execute(value: int, input: in int, mutable: ref int, "
            "readonlyValue: ref readonly int, output: out int): int { return value; }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, "semantic_display_callable_effects.zr");
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrSemanticContext *context;
    SZrCanonicalParameterContract parameters[5];
    SZrString *signature;
    TZrTypeId intType;
    TZrTypeId readonlyRefType;
    TZrTypeId writableRefType;
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
    TEST_ASSERT_EQUAL_UINT(5U, declaration->data.functionDeclaration.params->count);

    context = ZrParser_SemanticContext_New(g_state);
    TEST_ASSERT_NOT_NULL(context);
    intType = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    readonlyRefType = ZrParser_CanonicalType_InternRef(
            context, intType, ZR_CANONICAL_REF_READONLY);
    writableRefType = ZrParser_CanonicalType_InternRef(
            context, intType, ZR_CANONICAL_REF_WRITABLE);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, intType);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, readonlyRefType);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, writableRefType);

    memset(parameters, 0, sizeof(parameters));
    parameters[0].typeId = intType;
    parameters[0].passingForm = ZR_CANONICAL_PASSING_VALUE;
    parameters[0].escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameters[0].entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    parameters[0].exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    parameters[0].acceptsTemporary = ZR_TRUE;

    parameters[1].typeId = readonlyRefType;
    parameters[1].passingForm = ZR_CANONICAL_PASSING_IN;
    parameters[1].escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameters[1].entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    parameters[1].exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    parameters[1].acceptsTemporary = ZR_TRUE;

    parameters[2].typeId = writableRefType;
    parameters[2].passingForm = ZR_CANONICAL_PASSING_REF;
    parameters[2].escapeUpperBound = ZR_CANONICAL_ESCAPE_CALLER;
    parameters[2].entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    parameters[2].exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    parameters[2].callSiteMarker = ZR_CANONICAL_CALL_SITE_REF;

    parameters[3].typeId = readonlyRefType;
    parameters[3].passingForm = ZR_CANONICAL_PASSING_REF_READONLY;
    parameters[3].escapeUpperBound = ZR_CANONICAL_ESCAPE_CALLER;
    parameters[3].entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    parameters[3].exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    parameters[3].callSiteMarker = ZR_CANONICAL_CALL_SITE_REF;

    parameters[4].typeId = writableRefType;
    parameters[4].passingForm = ZR_CANONICAL_PASSING_OUT;
    parameters[4].escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameters[4].entryInitialization = ZR_CANONICAL_ENTRY_UNINITIALIZED;
    parameters[4].exitInitialization = ZR_CANONICAL_EXIT_DEFINITELY_INITIALIZED;
    parameters[4].callSiteMarker = ZR_CANONICAL_CALL_SITE_OUT;

    callableType = ZrParser_CanonicalType_InternFunction(
            context,
            parameters,
            sizeof(parameters) / sizeof(parameters[0]),
            intType,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_ASYNC |
                    ZR_CANONICAL_CALLABLE_EFFECT_GENERATOR |
                    ZR_CANONICAL_CALLABLE_EFFECT_THROWS);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, callableType);
    symbolId = ZrParser_Semantic_RegisterSymbol(
            context,
            ZrCore_String_CreateFromNative(g_state, "execute"),
            ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            callableType,
            ZR_SEMANTIC_ID_INVALID,
            declaration,
            declaration->location);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, symbolId);

    signature = ZrParser_SemanticDisplay_CreateCallableSignature(context, symbolId);
    TEST_ASSERT_NOT_NULL(signature);
    TEST_ASSERT_EQUAL_STRING(
            "async generator execute(value: int, input: in int, mutable: ref int, "
            "readonlyValue: ref readonly int, output: out int): int throws",
            ZrCore_String_GetNativeString(signature));

    ZrParser_SemanticContext_Free(context);
    ZrParser_Ast_Free(g_state, ast);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_semantic_display_formats_canonical_type_symbol_and_property);
    RUN_TEST(test_semantic_display_fails_closed_for_missing_identity);
    RUN_TEST(test_semantic_display_uses_matching_declaration_signature_fact);
    RUN_TEST(test_semantic_documentation_projects_exact_symbol_fact);
    RUN_TEST(test_semantic_type_display_alias_is_use_site_scoped);
    RUN_TEST(test_primitive_type_use_publishes_source_display_alias);
    RUN_TEST(test_owner_inner_type_use_publishes_source_display_alias);
    RUN_TEST(test_generic_type_use_publishes_exact_whole_display_alias);
    RUN_TEST(test_nested_generic_type_uses_preserve_split_angle_ranges);
    RUN_TEST(test_const_generic_type_use_preserves_source_expression_alias);
    RUN_TEST(test_type_value_alias_use_preserves_nominal_source_alias);
    RUN_TEST(test_owner_type_value_alias_preserves_inner_source_alias);
    RUN_TEST(test_qualified_type_use_publishes_source_display_alias);
    RUN_TEST(test_semantic_display_separates_const_parameter_alias_from_identity);
    RUN_TEST(test_semantic_display_rejects_malformed_composite_shapes);
    RUN_TEST(test_semantic_display_rejects_empty_nominal_identity);
    RUN_TEST(test_semantic_display_rejects_malformed_canonical_contracts);
    RUN_TEST(test_callable_signature_rejects_malformed_canonical_contracts);
    RUN_TEST(test_callable_signature_formats_canonical_effects_and_passing_modes);
    return UNITY_END();
}
