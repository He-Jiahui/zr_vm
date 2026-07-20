#include "unity.h"

#include <string.h>

#include "backend_aot_canonical_artifact.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/canonical_consumer.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_parser/artifact_projection.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_query.h"

#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

#define CONSUMER_TYPE_ID ((TZrUInt32)41u)
#define CONSUMER_TYPE_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 41u)
#define CONSUMER_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 41u)
#define CONSUMER_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 41u)
#define CONSUMER_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 41u)
#define CONSUMER_MEMBER_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 41u)

void test_reference_callable_contract_roundtrips_across_artifact_vm_and_aot(void);
void test_reference_callable_ref_export_matches_return_access(void);

typedef struct SZrConsumerArtifactFixture {
    TZrByte signature[8];
    SZrArtifactTypeDefRow typeDef;
    SZrArtifactTypeIdentityRow typeRef;
    SZrArtifactTypeIdentityRow typeSpec;
    SZrArtifactContractRow contract;
    SZrArtifactLayoutRow layout;
    SZrArtifactSectionInput sections[7];
    SZrArtifactDocument document;
} SZrConsumerArtifactFixture;

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

static void consumer_write_u32(TZrByte *bytes, TZrUInt32 value) {
    bytes[0] = (TZrByte)(value & 0xffu);
    bytes[1] = (TZrByte)((value >> 8u) & 0xffu);
    bytes[2] = (TZrByte)((value >> 16u) & 0xffu);
    bytes[3] = (TZrByte)((value >> 24u) & 0xffu);
}

static void consumer_write_u64(TZrByte *bytes, TZrUInt64 value) {
    consumer_write_u32(bytes, (TZrUInt32)(value & 0xffffffffu));
    consumer_write_u32(bytes + 4u, (TZrUInt32)(value >> 32u));
}

static SZrFileRange consumer_range(TZrSize start, TZrSize end) {
    SZrFileRange range;
    memset(&range, 0, sizeof(range));
    range.start.offset = start;
    range.start.line = 1;
    range.start.column = (TZrInt32)start + 1;
    range.end.offset = end;
    range.end.line = 1;
    range.end.column = (TZrInt32)end + 1;
    range.source = ZrCore_String_Create(g_state, "canonical_consumers.zr", 22u);
    return range;
}

static void consumer_fixture_init(SZrConsumerArtifactFixture *fixture) {
    static const TZrByte code[] = {0x01u};
    TZrUInt64 signatureHash;
    TZrUInt32 sectionCount = 0u;

    memset(fixture, 0, sizeof(*fixture));
    fixture->signature[0] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_PRIMITIVE;
    consumer_write_u32(&fixture->signature[1], (TZrUInt32)ZR_VALUE_TYPE_INT64);
    signatureHash = ZrCore_Artifact_HashBytes(fixture->signature, 5u);

    fixture->typeDef.token = CONSUMER_TYPE_DEF_TOKEN;
    fixture->typeDef.flags = ZR_ARTIFACT_TYPE_FLAG_VALUE;
    fixture->typeDef.canonicalTypeId = CONSUMER_TYPE_ID;
    fixture->typeDef.typeSignatureHash = signatureHash;

    fixture->typeRef.token = CONSUMER_TYPE_REF_TOKEN;
    fixture->typeRef.signatureToken = CONSUMER_SIGNATURE_TOKEN;
    fixture->typeRef.canonicalTypeId = CONSUMER_TYPE_ID;
    fixture->typeRef.signatureLength = 5u;
    fixture->typeRef.signatureHash = 0x1111222233334444ULL;
    fixture->typeRef.layoutVersion = 3u;
    fixture->typeRef.layoutHash = 0x3333444455556666ULL;

    fixture->typeSpec = fixture->typeRef;
    fixture->typeSpec.token = CONSUMER_TYPE_SPEC_TOKEN;
    fixture->typeSpec.signatureHash = 0x2222333344445555ULL;

    fixture->contract.memberToken = CONSUMER_MEMBER_TOKEN;
    fixture->contract.signatureToken = CONSUMER_SIGNATURE_TOKEN;
    fixture->contract.parameterCount = 1u;
    fixture->contract.contractHash = 0x4444555566667777ULL;

    fixture->layout.typeToken = CONSUMER_TYPE_DEF_TOKEN;
    fixture->layout.version = 3u;
    fixture->layout.byteSize = 8u;
    fixture->layout.byteAlignment = 8u;
    fixture->layout.gcScanKind = ZR_ARTIFACT_GC_SCAN_FREE;
    fixture->layout.layoutHash = fixture->typeRef.layoutHash;

    fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u, &fixture->typeDef};
    fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_REF_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u, &fixture->typeRef};
    fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u, &fixture->typeSpec};
    fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_SIGNATURE_HEAP, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            5u, fixture->signature};
    fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_CONTRACT_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u, &fixture->contract};
    fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_LAYOUT_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u, &fixture->layout};
    fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_CODE_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            (TZrUInt32)sizeof(code), code};

    fixture->document.kind = ZR_ARTIFACT_KIND_ZRO;
    fixture->document.identity.canonicalTypeId = CONSUMER_TYPE_ID;
    fixture->document.identity.typeRefToken = CONSUMER_TYPE_REF_TOKEN;
    fixture->document.identity.typeSpecToken = CONSUMER_TYPE_SPEC_TOKEN;
    fixture->document.identity.signatureToken = CONSUMER_SIGNATURE_TOKEN;
    fixture->document.identity.typeRefHash = fixture->typeRef.signatureHash;
    fixture->document.identity.typeSpecHash = fixture->typeSpec.signatureHash;
    fixture->document.identity.signatureHash = signatureHash;
    fixture->document.identity.layoutVersion = fixture->layout.version;
    fixture->document.identity.layoutHash = fixture->layout.layoutHash;
    fixture->document.identity.callableContractHash = fixture->contract.contractHash;
    fixture->document.identity.moduleHash = 0x5555666677778888ULL;
    fixture->document.sectionCount = sectionCount;
    fixture->document.sections = fixture->sections;
}

static TZrSize consumer_fixture_write(const SZrConsumerArtifactFixture *fixture,
                                      TZrByte *buffer,
                                      TZrSize capacity) {
    SZrArtifactDiagnostic diagnostic;
    TZrSize written = 0u;
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Artifact_Write(&fixture->document,
                                                buffer,
                                                capacity,
                                                &written,
                                                &diagnostic));
    return written;
}

static void consumer_release_compiler_function(SZrCompilerState *cs) {
    if (cs->topLevelFunction != ZR_NULL && cs->topLevelFunction != cs->currentFunction) {
        ZrCore_Function_Free(g_state, cs->topLevelFunction);
        cs->topLevelFunction = ZR_NULL;
    }
    if (cs->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
    }
}

static void test_vm_and_aot_consume_the_same_canonical_contract_and_fail_identically(void) {
    SZrConsumerArtifactFixture fixture;
    SZrCanonicalConsumerProjection vmProjection;
    SZrCanonicalConsumerProjection aotProjection;
    SZrArtifactDiagnostic vmDiagnostic;
    SZrArtifactDiagnostic aotDiagnostic;
    TZrByte buffer[2048];
    TZrSize length;

    consumer_fixture_init(&fixture);
    length = consumer_fixture_write(&fixture, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Module_OpenCanonicalArtifact(buffer,
                                                              length,
                                                              &fixture.document.identity,
                                                              &vmProjection,
                                                              &vmDiagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          backend_aot_open_canonical_artifact(buffer,
                                                              length,
                                                              &fixture.document.identity,
                                                              &aotProjection,
                                                              &aotDiagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&vmProjection.rootType,
                             &aotProjection.rootType,
                             sizeof(vmProjection.rootType));

    consumer_write_u64(buffer + 64u, fixture.document.identity.signatureHash + 1u);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_SIGNATURE_HASH_MISMATCH,
                          ZrCore_Module_OpenCanonicalArtifact(buffer,
                                                              length,
                                                              ZR_NULL,
                                                              &vmProjection,
                                                              &vmDiagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_SIGNATURE_HASH_MISMATCH,
                          backend_aot_open_canonical_artifact(buffer,
                                                              length,
                                                              ZR_NULL,
                                                              &aotProjection,
                                                              &aotDiagnostic));
    TEST_ASSERT_EQUAL_UINT64(vmDiagnostic.expectedHash, aotDiagnostic.expectedHash);
    TEST_ASSERT_EQUAL_UINT64(vmDiagnostic.actualHash, aotDiagnostic.actualHash);
}

static void test_reflection_debug_and_layout_resolve_only_canonical_ids_and_tokens(void) {
    SZrConsumerArtifactFixture fixture;
    SZrCanonicalConsumerProjection projection;
    SZrCanonicalTypeProjection reflected;
    SZrCanonicalTypeProjection debugged;
    SZrArtifactLayoutRow layout;
    SZrArtifactDiagnostic diagnostic;
    TZrByte buffer[2048];
    TZrSize length;

    consumer_fixture_init(&fixture);
    length = consumer_fixture_write(&fixture, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Module_OpenCanonicalArtifact(buffer,
                                                              length,
                                                              ZR_NULL,
                                                              &projection,
                                                              &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Reflection_ResolveArtifactType(&projection,
                                                               CONSUMER_TYPE_DEF_TOKEN,
                                                               &reflected,
                                                               &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_Debug_ResolveArtifactType(&projection,
                                                          CONSUMER_TYPE_ID,
                                                          &debugged,
                                                          &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(CONSUMER_TYPE_ID, reflected.canonicalTypeId);
    TEST_ASSERT_EQUAL_UINT32(reflected.canonicalTypeId, debugged.canonicalTypeId);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_OK,
                          ZrCore_CanonicalConsumer_ResolveLayout(&projection,
                                                                CONSUMER_TYPE_DEF_TOKEN,
                                                                &layout,
                                                                &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(8u, layout.byteSize);
    TEST_ASSERT_EQUAL_UINT64(fixture.document.identity.layoutHash, layout.layoutHash);

    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_INVALID_SECTION,
                          ZrCore_Reflection_ResolveArtifactType(
                                  &projection,
                                  ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 99u),
                                  &reflected,
                                  &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_STATUS_INVALID_SECTION,
                          ZrCore_Debug_ResolveArtifactType(&projection,
                                                          99u,
                                                          &debugged,
                                                          &diagnostic));
}

static void test_semantic_query_projects_expression_and_call_types_from_canonical_facts(void) {
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrSemanticExpressionFact expression;
    SZrSemanticExpressionFact callExpression;
    SZrSemanticReferenceFact callReference;
    SZrParserSemanticTypeQuery typeQuery;
    SZrParserSemanticCallQuery callQuery;
    SZrCanonicalParameterContract parameter;
    SZrInferredType intType;
    SZrAstNode expressionNode;
    SZrAstNode callNode;
    TZrTypeId intTypeId;
    TZrTypeId functionTypeId;
    TZrChar label[128];

    TEST_ASSERT_NOT_NULL(context);
    memset(&expressionNode, 0, sizeof(expressionNode));
    expressionNode.type = ZR_AST_INTEGER_LITERAL;
    expressionNode.location = consumer_range(1u, 3u);
    ZrParser_InferredType_Init(g_state, &intType, ZR_VALUE_TYPE_INT64);
    memset(&expression, 0, sizeof(expression));
    expression.node = &expressionNode;
    expression.range = expressionNode.location;
    expression.kind = ZR_SEMANTIC_EXPRESSION_FACT_LITERAL;
    expression.exactness = ZR_SEMANTIC_FACT_EXACT;
    ZrParser_InferredType_Copy(g_state, &expression.inferredType, &intType);
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(context, &expression));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CanonicalTypeAt(context,
                                                            consumer_range(2u, 2u),
                                                            ZR_NULL,
                                                            &typeQuery));
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, typeQuery.typeId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(context,
                                                   typeQuery.typeId,
                                                   label,
                                                   sizeof(label)));
    TEST_ASSERT_EQUAL_STRING("int", label);

    intTypeId = typeQuery.typeId;
    memset(&parameter, 0, sizeof(parameter));
    parameter.typeId = intTypeId;
    parameter.escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameter.acceptsTemporary = ZR_TRUE;
    functionTypeId = ZrParser_CanonicalType_InternFunction(context,
                                                           &parameter,
                                                           1u,
                                                           intTypeId,
                                                           ZR_CANONICAL_RECEIVER_NONE,
                                                           ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, functionTypeId);

    memset(&callNode, 0, sizeof(callNode));
    callNode.type = ZR_AST_PRIMARY_EXPRESSION;
    callNode.location = consumer_range(10u, 30u);
    memset(&callExpression, 0, sizeof(callExpression));
    callExpression.node = &callNode;
    callExpression.range = callNode.location;
    callExpression.kind = ZR_SEMANTIC_EXPRESSION_FACT_CALL;
    callExpression.hasCallInfo = ZR_TRUE;
    callExpression.callTargetName = ZrCore_String_Create(g_state, "identity", 8u);
    callExpression.callTargetRange = consumer_range(10u, 17u);
    callExpression.argumentCount = 1u;
    ZrParser_InferredType_Copy(g_state, &callExpression.inferredType, &intType);
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendExpression(context, &callExpression));

    memset(&callReference, 0, sizeof(callReference));
    callReference.node = &callNode;
    callReference.range = callExpression.callTargetRange;
    callReference.kind = ZR_SEMANTIC_REFERENCE_CALL;
    callReference.typeId = functionTypeId;
    callReference.name = callExpression.callTargetName;
    callReference.isResolved = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_SemanticFacts_AppendReference(context, &callReference));

    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(context,
                                                   consumer_range(22u, 22u),
                                                   ZR_NULL,
                                                   &callQuery));
    TEST_ASSERT_EQUAL_UINT32(functionTypeId, callQuery.callableTypeId);
    TEST_ASSERT_FALSE(callQuery.hasResolvedTarget);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID, callQuery.targetSymbolId);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(context,
                                                       &callQuery,
                                                       label,
                                                       sizeof(label)));
    TEST_ASSERT_EQUAL_STRING("identity: fn(int) -> int", label);

    ZrParser_InferredType_Free(g_state, &callExpression.inferredType);
    ZrParser_InferredType_Free(g_state, &expression.inferredType);
    ZrParser_InferredType_Free(g_state, &intType);
    ZrParser_SemanticContext_Free(context);
}

static void test_resolved_generic_call_publishes_closed_canonical_signature(void) {
    const TZrChar *source =
            "func identity<T>(value: T): T { return value; }\n"
            "func zero(): int { return 0; }\n"
            "func use(): int { var value = identity(42); return zero(); }\n";
    const TZrChar *call = strstr(source, "identity(42)");
    const TZrChar *emptyCallDeclaration = strstr(source, "zero()");
    const TZrChar *emptyCall = emptyCallDeclaration != ZR_NULL
                                      ? strstr(emptyCallDeclaration + strlen("zero()"), "zero()")
                                      : ZR_NULL;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange position;
    SZrParserSemanticCallQuery query;
    TZrChar typeLabel[128];
    TZrChar callLabel[128];

    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_NOT_NULL(emptyCall);
    sourceName = ZrCore_String_Create(g_state, "canonical_consumers.zr", 22u);
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);
    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);

    position = consumer_range((TZrSize)(call - source + strlen("identity(")),
                              (TZrSize)(call - source + strlen("identity(")));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, position, ZR_NULL, &query));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, query.callableTypeId, typeLabel, sizeof(typeLabel)));
    TEST_ASSERT_EQUAL_STRING("fn(int) -> int", typeLabel);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &query, callLabel, sizeof(callLabel)));
    TEST_ASSERT_EQUAL_STRING("identity<T>(value: int): int", callLabel);

    position = consumer_range((TZrSize)(emptyCall - source + strlen("zero(")),
                              (TZrSize)(emptyCall - source + strlen("zero(")));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, position, ZR_NULL, &query));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, query.callableTypeId, typeLabel, sizeof(typeLabel)));
    TEST_ASSERT_EQUAL_STRING("fn() -> int", typeLabel);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &query, callLabel, sizeof(callLabel)));
    TEST_ASSERT_EQUAL_STRING("zero(): int", callLabel);

    consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_resolved_generic_member_call_preserves_declaration_generic_clause(void) {
    const TZrChar *source =
            "class Matrix<T, const N: int> { }\n"
            "class Box<T> {\n"
            "    pub fn shape<const N: int>(value: Matrix<T, N>): Matrix<T, N> { return value; }\n"
            "}\n"
            "var box = new Box<int>();\n"
            "var value = new Matrix<int, 4>();\n"
            "box.shape(value);\n";
    const TZrChar *callExpression = strstr(source, "box.shape(value)");
    const TZrChar *call = callExpression != ZR_NULL
                                  ? callExpression + strlen("box.")
                                  : ZR_NULL;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange position;
    SZrParserSemanticCallQuery query;
    TZrChar callLabel[256];

    TEST_ASSERT_NOT_NULL(call);
    sourceName = ZrCore_String_Create(
            g_state, "canonical_generic_member_call.zr", 32u);
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);
    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);

    position = consumer_range((TZrSize)(call - source + strlen("shape(")),
                              (TZrSize)(call - source + strlen("shape(")));
    position.source = sourceName;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, position, ZR_NULL, &query));
    TEST_ASSERT_TRUE(query.hasResolvedTarget);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.targetSymbolId);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &query, callLabel, sizeof(callLabel)));
    TEST_ASSERT_EQUAL_STRING(
            "fn shape<const N: int>(value: Matrix<int, 4>): Matrix<int, 4>",
            callLabel);

    consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_resolved_extern_call_preserves_parameter_names_in_canonical_signature(void) {
    const TZrChar *source =
            "%extern(\"fixture\") {\n"
            "    NativeAdd(lhs: i32, rhs: i32): i32;\n"
            "}\n"
            "func use(): i32 { return NativeAdd(1, 2); }\n";
    const TZrChar *call = strstr(source, "NativeAdd(1, 2)");
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange position;
    SZrParserSemanticCallQuery query;
    TZrChar callLabel[128];

    TEST_ASSERT_NOT_NULL(call);
    sourceName = ZrCore_String_Create(g_state, "canonical_consumers.zr", 22u);
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);
    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);

    position = consumer_range((TZrSize)(call - source + strlen("NativeAdd(")),
                              (TZrSize)(call - source + strlen("NativeAdd(")));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, position, ZR_NULL, &query));
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &query, callLabel, sizeof(callLabel)));
    TEST_ASSERT_EQUAL_STRING("NativeAdd(lhs: i32, rhs: i32): i32", callLabel);

    consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_scoped_call_preserves_contract_and_target_identity(void) {
    const TZrChar *source =
            "fn inspect(value: scoped ref readonly int): int { return 1; }\n"
            "fn use(value: ref readonly int): int { return inspect(ref value); }\n";
    const TZrChar *declaration = strstr(source, "inspect(");
    const TZrChar *call = declaration != ZR_NULL
                                  ? strstr(declaration + strlen("inspect("), "inspect(")
                                  : ZR_NULL;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange position;
    SZrParserSemanticCallQuery query;
    TZrChar typeLabel[160];
    TZrChar callLabel[160];
    TZrByte signature[256];
    TZrSize signatureLength = 0U;
    TZrTypeId importedTypeId = ZR_SEMANTIC_ID_INVALID;
    SZrArtifactDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_NOT_NULL(call);
    sourceName = ZrCore_String_Create(g_state, "canonical_scoped_call.zr", 24u);
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);
    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);

    position = consumer_range((TZrSize)(call - source + strlen("inspect(")),
                              (TZrSize)(call - source + strlen("inspect(")));
    position.source = sourceName;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, position, ZR_NULL, &query));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, query.callableTypeId, typeLabel, sizeof(typeLabel)));
    TEST_ASSERT_EQUAL_STRING("fn(scoped ref readonly int) -> int", typeLabel);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &query, callLabel, sizeof(callLabel)));
    TEST_ASSERT_EQUAL_STRING(
            "inspect(value: scoped ref readonly int): int", callLabel);
    TEST_ASSERT_NOT_NULL(query.reference);
    TEST_ASSERT_TRUE(query.reference->isResolved);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.reference->symbolId);
    TEST_ASSERT_TRUE(query.hasResolvedTarget);
    TEST_ASSERT_EQUAL_UINT32(query.reference->symbolId, query.targetSymbolId);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(declaration - source),
            (TZrUInt64)query.reference->declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(declaration - source),
            (TZrUInt64)query.targetDeclarationRange.start.offset);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_WriteSignature(
                    cs.semanticContext,
                    query.callableTypeId,
                    signature,
                    sizeof(signature),
                    &signatureLength,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_InternSignature(
                    cs.semanticContext,
                    ZR_NULL,
                    signature,
                    signatureLength,
                    &importedTypeId,
                    &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(query.callableTypeId, importedTypeId);

    consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_receiver_call_publishes_resolved_target_identity(void) {
    const TZrChar *source =
            "class Counter {\n"
            "  pub var value: int;\n"
            "  pub const fn read(): int { return this.value; }\n"
            "  pub fn write(next: int): int { this.value = next; return this.value; }\n"
            "}\n"
            "fn use(counter: readonly Counter): int { return counter.read(); }\n"
            "fn change(counter: Counter): int { return counter.write(1); }\n";
    const TZrChar *callExpression = strstr(source, "counter.read()");
    const TZrChar *call = callExpression != ZR_NULL
                                  ? callExpression + strlen("counter.")
                                  : ZR_NULL;
    const TZrChar *mutableCallExpression = strstr(source, "counter.write(1)");
    const TZrChar *mutableCall = mutableCallExpression != ZR_NULL
                                         ? mutableCallExpression + strlen("counter.")
                                         : ZR_NULL;
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *classNode;
    SZrAstNode *methodNode;
    SZrAstNode *mutableMethodNode;
    SZrFileRange position;
    SZrParserSemanticCallQuery query;
    TZrChar typeLabel[128];
    TZrChar callLabel[128];

    TEST_ASSERT_NOT_NULL(call);
    TEST_ASSERT_NOT_NULL(mutableCall);
    sourceName = ZrCore_String_Create(g_state, "canonical_receiver_call.zr", 26u);
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    classNode = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(classNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classNode->type);
    TEST_ASSERT_NOT_NULL(classNode->data.classDeclaration.members);
    methodNode = classNode->data.classDeclaration.members->nodes[1];
    TEST_ASSERT_NOT_NULL(methodNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_METHOD, methodNode->type);
    mutableMethodNode = classNode->data.classDeclaration.members->nodes[2];
    TEST_ASSERT_NOT_NULL(mutableMethodNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_METHOD, mutableMethodNode->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);
    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);

    position = consumer_range((TZrSize)(call - source + strlen("read(")),
                              (TZrSize)(call - source + strlen("read(")));
    position.source = sourceName;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, position, ZR_NULL, &query));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, query.callableTypeId, typeLabel, sizeof(typeLabel)));
    TEST_ASSERT_EQUAL_STRING("const fn() -> int", typeLabel);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &query, callLabel, sizeof(callLabel)));
    TEST_ASSERT_EQUAL_STRING("const fn read(): int", callLabel);
    TEST_ASSERT_NOT_NULL(query.reference);
    TEST_ASSERT_TRUE(query.reference->isResolved);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.reference->symbolId);
    TEST_ASSERT_TRUE(query.hasResolvedTarget);
    TEST_ASSERT_EQUAL_UINT32(query.reference->symbolId, query.targetSymbolId);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)methodNode->location.start.offset,
            (TZrUInt64)query.reference->declarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)methodNode->location.end.offset,
            (TZrUInt64)query.reference->declarationRange.end.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)methodNode->location.start.offset,
            (TZrUInt64)query.targetDeclarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)methodNode->location.end.offset,
            (TZrUInt64)query.targetDeclarationRange.end.offset);

    position = consumer_range(
            (TZrSize)(mutableCall - source + strlen("write(")),
            (TZrSize)(mutableCall - source + strlen("write(")));
    position.source = sourceName;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, position, ZR_NULL, &query));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, query.callableTypeId, typeLabel, sizeof(typeLabel)));
    TEST_ASSERT_EQUAL_STRING("fn(int) -> int", typeLabel);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &query, callLabel, sizeof(callLabel)));
    TEST_ASSERT_EQUAL_STRING("fn write(next: int): int", callLabel);
    TEST_ASSERT_TRUE(query.hasResolvedTarget);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.targetSymbolId);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)mutableMethodNode->location.start.offset,
            (TZrUInt64)query.targetDeclarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)mutableMethodNode->location.end.offset,
            (TZrUInt64)query.targetDeclarationRange.end.offset);

    consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_vm_and_aot_consume_the_same_canonical_contract_and_fail_identically);
    RUN_TEST(test_reflection_debug_and_layout_resolve_only_canonical_ids_and_tokens);
    RUN_TEST(test_semantic_query_projects_expression_and_call_types_from_canonical_facts);
    RUN_TEST(test_resolved_generic_call_publishes_closed_canonical_signature);
    RUN_TEST(test_resolved_generic_member_call_preserves_declaration_generic_clause);
    RUN_TEST(test_resolved_extern_call_preserves_parameter_names_in_canonical_signature);
    RUN_TEST(test_source_scoped_call_preserves_contract_and_target_identity);
    RUN_TEST(test_receiver_call_publishes_resolved_target_identity);
    RUN_TEST(test_reference_callable_contract_roundtrips_across_artifact_vm_and_aot);
    RUN_TEST(test_reference_callable_ref_export_matches_return_access);
    return UNITY_END();
}
