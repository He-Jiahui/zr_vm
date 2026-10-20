#include "unity.h"

#include <string.h>

#include "backend_aot/backend_aot_canonical_artifact.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/canonical_consumer.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/reflection.h"
#include "artifact/artifact_projection.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_query.h"

#include "compiler/compiler_internal.h"

#define CONSUMER_TYPE_ID ((TZrUInt32)41u)
#define CONSUMER_TYPE_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 41u)
#define CONSUMER_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 41u)
#define CONSUMER_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 41u)
#define CONSUMER_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 41u)
#define CONSUMER_MEMBER_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 41u)
#define CONSUMER_TASK_TYPE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 42u)
#define CONSUMER_JOB_TYPE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 43u)

void test_reference_callable_contract_roundtrips_across_artifact_vm_and_aot(void);
void test_reference_callable_ref_export_matches_return_access(void);

typedef struct SZrConsumerArtifactFixture {
    TZrByte signature[8];
    SZrArtifactTypeDefRow typeDef;
    SZrArtifactTypeIdentityRow typeRef;
    SZrArtifactTypeIdentityRow typeSpec;
    SZrArtifactContractRow contract;
    SZrArtifactLayoutRow layout;
    SZrArtifactDomainTransferRow domainTransfer;
    SZrArtifactSchedulerContractRow schedulerContract;
    SZrArtifactSectionInput sections[9];
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

    fixture->domainTransfer.typeToken = CONSUMER_TYPE_DEF_TOKEN;
    fixture->domainTransfer.kind = ZR_ARTIFACT_DOMAIN_TRANSFER_VALUE_COPY;
    fixture->domainTransfer.schemaVersion = 1u;
    fixture->domainTransfer.schemaHash = 0x6666777788889999ULL;

    fixture->schedulerContract.schedulerTypeToken = CONSUMER_TYPE_REF_TOKEN;
    fixture->schedulerContract.taskTypeToken = CONSUMER_TASK_TYPE_TOKEN;
    fixture->schedulerContract.jobTypeToken = CONSUMER_JOB_TYPE_TOKEN;
    fixture->schedulerContract.abiVersion = 1u;
    fixture->schedulerContract.policyMask =
            ZR_ARTIFACT_SCHEDULER_POLICY_ATTACHED_DOMAIN |
            ZR_ARTIFACT_SCHEDULER_POLICY_ISOLATED_DOMAIN;
    fixture->schedulerContract.isolatedRequirementFlags =
            ZR_ARTIFACT_SCHEDULER_REQUIREMENT_SEND |
            ZR_ARTIFACT_SCHEDULER_REQUIREMENT_SYNC;
    fixture->schedulerContract.transportContractHash = 0x777788889999aaaaULL;
    fixture->schedulerContract.schedulerContractHash = 0xaaaabbbbccccddddULL;

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
    fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_OPTIONAL,
            1u,
            &fixture->domainTransfer};
    fixture->sections[sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_OPTIONAL,
            1u,
            &fixture->schedulerContract};

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
    TEST_ASSERT_TRUE(vmProjection.rootType.hasDomainTransfer);
    TEST_ASSERT_EQUAL_MEMORY(
            &fixture.domainTransfer,
            &vmProjection.rootType.domainTransfer,
            sizeof(fixture.domainTransfer));

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

static void test_canonical_consumer_projects_optional_domain_transfer_contract(void) {
    SZrConsumerArtifactFixture fixture;
    SZrCanonicalConsumerProjection projection;
    SZrArtifactDomainTransferRow contract;
    SZrArtifactDiagnostic diagnostic;
    TZrByte buffer[2048];
    TZrSize length;

    consumer_fixture_init(&fixture);
    length = consumer_fixture_write(&fixture, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_Open(
                    buffer, length, ZR_NULL, &projection, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_ResolveDomainTransfer(
                    &projection,
                    CONSUMER_TYPE_DEF_TOKEN,
                    &contract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_DOMAIN_TRANSFER_VALUE_COPY, contract.kind);
    TEST_ASSERT_EQUAL_UINT32(1u, contract.schemaVersion);
    TEST_ASSERT_EQUAL_UINT64(
            fixture.domainTransfer.schemaHash, contract.schemaHash);

    fixture.sections[fixture.document.sectionCount - 2u] =
            fixture.sections[fixture.document.sectionCount - 1u];
    fixture.document.sectionCount--;
    length = consumer_fixture_write(&fixture, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_Open(
                    buffer, length, ZR_NULL, &projection, &diagnostic));
    TEST_ASSERT_FALSE(projection.rootType.hasDomainTransfer);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_SECTION,
            ZrCore_CanonicalConsumer_ResolveDomainTransfer(
                    &projection,
                    CONSUMER_TYPE_DEF_TOKEN,
                    &contract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE,
            diagnostic.sectionKind);
}

static void test_canonical_consumer_validates_scheduler_contract_without_name_fallback(void) {
    SZrConsumerArtifactFixture fixture;
    SZrCanonicalConsumerProjection projection;
    SZrArtifactSchedulerContractRow contract;
    SZrCanonicalSchedulerContractExpectation expected;
    SZrArtifactDiagnostic diagnostic;
    TZrByte buffer[2048];
    TZrSize length;

    consumer_fixture_init(&fixture);
    length = consumer_fixture_write(&fixture, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_Open(
                    buffer, length, ZR_NULL, &projection, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_ResolveSchedulerContract(
                    &projection,
                    CONSUMER_TYPE_REF_TOKEN,
                    &contract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(
            &fixture.schedulerContract, &contract, sizeof(contract));

    memset(&expected, 0, sizeof(expected));
    expected.schedulerTypeToken = CONSUMER_TYPE_REF_TOKEN;
    expected.taskTypeToken = CONSUMER_TASK_TYPE_TOKEN;
    expected.jobTypeToken = CONSUMER_JOB_TYPE_TOKEN;
    expected.abiVersion = 1u;
    expected.policy = ZR_ARTIFACT_SCHEDULER_POLICY_ISOLATED_DOMAIN;
    expected.requirementFlags = ZR_ARTIFACT_SCHEDULER_REQUIREMENT_SEND |
                                ZR_ARTIFACT_SCHEDULER_REQUIREMENT_SYNC;
    expected.transportContractHash = fixture.schedulerContract.transportContractHash;
    expected.schedulerContractHash = fixture.schedulerContract.schedulerContractHash;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expected, &diagnostic));

    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
            ZrCore_CanonicalConsumer_ResolveSchedulerContract(
                    &projection,
                    CONSUMER_TYPE_SPEC_TOKEN,
                    &contract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
            ZrCore_CanonicalConsumer_ResolveSchedulerContract(
                    &projection,
                    CONSUMER_MEMBER_TOKEN,
                    &contract,
                    &diagnostic));

    expected.taskTypeToken++;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expected, &diagnostic));
    expected.taskTypeToken = CONSUMER_TASK_TYPE_TOKEN;

    expected.abiVersion++;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_SCHEDULER_ABI_MISMATCH,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expected, &diagnostic));
    expected.abiVersion = fixture.schedulerContract.abiVersion;

    expected.policy = ZR_ARTIFACT_SCHEDULER_POLICY_ATTACHED_DOMAIN;
    expected.requirementFlags = ZR_ARTIFACT_SCHEDULER_REQUIREMENT_SEND;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_SCHEDULER_REQUIREMENT_MISMATCH,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expected, &diagnostic));

    expected.policy = ZR_ARTIFACT_SCHEDULER_POLICY_ISOLATED_DOMAIN;
    expected.requirementFlags = fixture.schedulerContract.isolatedRequirementFlags;
    expected.transportContractHash++;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_TRANSPORT_CONTRACT_MISMATCH,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expected, &diagnostic));
    expected.transportContractHash = fixture.schedulerContract.transportContractHash;

    expected.schedulerContractHash++;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_SCHEDULER_CONTRACT_MISMATCH,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expected, &diagnostic));
    expected.schedulerContractHash = fixture.schedulerContract.schedulerContractHash;

    fixture.schedulerContract.policyMask =
            ZR_ARTIFACT_SCHEDULER_POLICY_ISOLATED_DOMAIN;
    length = consumer_fixture_write(&fixture, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_Open(
                    buffer, length, ZR_NULL, &projection, &diagnostic));
    expected.policy = ZR_ARTIFACT_SCHEDULER_POLICY_ATTACHED_DOMAIN;
    expected.requirementFlags = 0u;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_SCHEDULER_POLICY_MISMATCH,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expected, &diagnostic));

    fixture.document.sectionCount--;
    length = consumer_fixture_write(&fixture, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_Open(
                    buffer, length, ZR_NULL, &projection, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_SECTION,
            ZrCore_CanonicalConsumer_ResolveSchedulerContract(
                    &projection,
                    CONSUMER_TYPE_REF_TOKEN,
                    &contract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE,
            diagnostic.sectionKind);
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
    callExpression.exactness = ZR_SEMANTIC_FACT_EXACT;
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
            "fn identity<T>(value: T): T { return value; }\n"
            "fn zero(): int { return 0; }\n"
            "fn use(): int { var value = identity(42); return zero(); }\n";
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
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID, query.receiverTypeId);
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

static void test_source_constructors_publish_canonical_call_contracts(void) {
    const TZrChar *source =
            "class Hero {\n"
            "  pub @constructor(seed: float) { }\n"
            "}\n"
            "struct Point {\n"
            "  pub var x: int;\n"
            "  pub var y: int;\n"
            "  pub @constructor(x: int, y: int) { this.x = x; this.y = y; }\n"
            "}\n"
            "var hero: Hero = new Hero(42);\n"
            "var point: Point = init Point(y: 2, x: 1);\n";
    const TZrChar *classCall = strstr(source, "new Hero(42)");
    const TZrChar *structCall = strstr(source, "init Point(y: 2, x: 1)");
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *classNode;
    SZrAstNode *classConstructor;
    SZrAstNode *structNode;
    SZrAstNode *structConstructor;
    SZrFileRange position;
    SZrParserSemanticCallQuery query;
    const SZrSemanticReferenceFact *declaration;
    const SZrSemanticCallArgumentFact *mapping;
    const SZrCanonicalTypeNode *callableType;
    TZrChar typeLabel[128];
    TZrChar callLabel[160];

    TEST_ASSERT_NOT_NULL(classCall);
    TEST_ASSERT_NOT_NULL(structCall);
    sourceName = ZrCore_String_Create(
            g_state, "canonical_constructor_calls.zr", 30u);
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    classNode = ast->data.script.statements->nodes[0];
    structNode = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(classNode);
    TEST_ASSERT_NOT_NULL(structNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classNode->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, structNode->type);
    TEST_ASSERT_NOT_NULL(classNode->data.classDeclaration.members);
    TEST_ASSERT_NOT_NULL(structNode->data.structDeclaration.members);
    classConstructor = classNode->data.classDeclaration.members->nodes[0];
    structConstructor = structNode->data.structDeclaration.members->nodes[2];
    TEST_ASSERT_NOT_NULL(classConstructor);
    TEST_ASSERT_NOT_NULL(structConstructor);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_META_FUNCTION, classConstructor->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_META_FUNCTION, structConstructor->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);
    TEST_ASSERT_FALSE_MESSAGE(
            cs.hasError,
            cs.errorMessage != ZR_NULL ? cs.errorMessage
                                       : "source constructor compile failed");
    TEST_ASSERT_NOT_NULL(cs.semanticContext);

    position = consumer_range(
            (TZrSize)(classCall - source + strlen("new Hero(")),
            (TZrSize)(classCall - source + strlen("new Hero(")));
    position.source = sourceName;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, position, ZR_NULL, &query));
    TEST_ASSERT_FALSE(query.isMemberCall);
    TEST_ASSERT_FALSE(query.hasNamedArguments);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)query.argumentCount);
    TEST_ASSERT_TRUE(query.hasResolvedTarget);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.targetSymbolId);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(classCall - source + strlen("new ")),
            (TZrUInt64)query.callTargetRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)classConstructor->location.start.offset,
            (TZrUInt64)query.targetDeclarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)classConstructor->location.end.offset,
            (TZrUInt64)query.targetDeclarationRange.end.offset);
    TEST_ASSERT_NOT_NULL(query.argumentMappings);
    TEST_ASSERT_EQUAL_UINT(1U, query.argumentMappings->length);
    mapping = (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
            (SZrArray *)query.argumentMappings, 0U);
    TEST_ASSERT_NOT_NULL(mapping);
    TEST_ASSERT_EQUAL_UINT(0U, mapping->argumentIndex);
    TEST_ASSERT_EQUAL_UINT(0U, mapping->parameterIndex);
    TEST_ASSERT_FALSE(mapping->isNamed);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_CALL_CONVERSION_IMPLICIT, mapping->conversion);
    TEST_ASSERT_NOT_EQUAL(mapping->argumentTypeId, mapping->parameterTypeId);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(classCall - source + strlen("new Hero(")),
            (TZrUInt64)mapping->argumentRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(classCall - source + strlen("new Hero(42")),
            (TZrUInt64)mapping->argumentRange.end.offset);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, query.callableTypeId, typeLabel, sizeof(typeLabel)));
    TEST_ASSERT_EQUAL_STRING("fn(double) -> null", typeLabel);
    callableType = ZrParser_CanonicalType_Find(
            cs.semanticContext, query.callableTypeId);
    TEST_ASSERT_NOT_NULL(callableType);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callableType->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_NONE,
            callableType->data.function.receiverEffect);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &query, callLabel, sizeof(callLabel)));
    TEST_ASSERT_EQUAL_STRING("@constructor(seed: double): null", callLabel);
    declaration = ZrParser_SemanticQuery_DeclarationOf(
            cs.semanticContext, query.targetSymbolId, ZR_NULL);
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_PTR(classConstructor, declaration->node);

    position = consumer_range(
            (TZrSize)(structCall - source + strlen("init Point(")),
            (TZrSize)(structCall - source + strlen("init Point(")));
    position.source = sourceName;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, position, ZR_NULL, &query));
    TEST_ASSERT_FALSE(query.isMemberCall);
    TEST_ASSERT_TRUE(query.hasNamedArguments);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)query.argumentCount);
    TEST_ASSERT_TRUE(query.hasResolvedTarget);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.targetSymbolId);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(structCall - source + strlen("init ")),
            (TZrUInt64)query.callTargetRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)structConstructor->location.start.offset,
            (TZrUInt64)query.targetDeclarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)structConstructor->location.end.offset,
            (TZrUInt64)query.targetDeclarationRange.end.offset);
    TEST_ASSERT_NOT_NULL(query.argumentMappings);
    TEST_ASSERT_EQUAL_UINT(2U, query.argumentMappings->length);
    mapping = (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
            (SZrArray *)query.argumentMappings, 0U);
    TEST_ASSERT_NOT_NULL(mapping);
    TEST_ASSERT_EQUAL_UINT(0U, mapping->argumentIndex);
    TEST_ASSERT_EQUAL_UINT(1U, mapping->parameterIndex);
    TEST_ASSERT_TRUE(mapping->isNamed);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_CALL_CONVERSION_EXACT, mapping->conversion);
    TEST_ASSERT_EQUAL_UINT64(mapping->argumentTypeId, mapping->parameterTypeId);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(structCall - source + strlen("init Point(y: ")),
            (TZrUInt64)mapping->argumentRange.start.offset);
    mapping = (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
            (SZrArray *)query.argumentMappings, 1U);
    TEST_ASSERT_NOT_NULL(mapping);
    TEST_ASSERT_EQUAL_UINT(1U, mapping->argumentIndex);
    TEST_ASSERT_EQUAL_UINT(0U, mapping->parameterIndex);
    TEST_ASSERT_TRUE(mapping->isNamed);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_CALL_CONVERSION_EXACT, mapping->conversion);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, query.callableTypeId, typeLabel, sizeof(typeLabel)));
    TEST_ASSERT_EQUAL_STRING("fn(int, int) -> null", typeLabel);
    callableType = ZrParser_CanonicalType_Find(
            cs.semanticContext, query.callableTypeId);
    TEST_ASSERT_NOT_NULL(callableType);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callableType->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_NONE,
            callableType->data.function.receiverEffect);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &query, callLabel, sizeof(callLabel)));
    TEST_ASSERT_EQUAL_STRING("@constructor(x: int, y: int): null", callLabel);
    declaration = ZrParser_SemanticQuery_DeclarationOf(
            cs.semanticContext, query.targetSymbolId, ZR_NULL);
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_PTR(structConstructor, declaration->node);

    consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_source_super_constructor_publishes_canonical_call_contract(void) {
    const TZrChar *source =
            "class BaseHero {\n"
            "  pub @constructor(origin: float) { }\n"
            "}\n"
            "class BossHero: BaseHero {\n"
            "  pub @constructor(seed: int) super(seed) { }\n"
            "}\n";
    const TZrChar *superCall = strstr(source, "super(seed)");
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *baseClass;
    SZrAstNode *baseConstructor;
    SZrFileRange position;
    SZrParserSemanticCallQuery query;
    const SZrSemanticReferenceFact *declaration;
    const SZrSemanticCallArgumentFact *mapping;
    const SZrCanonicalTypeNode *callableType;
    TZrChar typeLabel[128];
    TZrChar callLabel[160];

    TEST_ASSERT_NOT_NULL(superCall);
    sourceName = ZrCore_String_Create(
            g_state, "canonical_super_constructor_call.zr", 35u);
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    baseClass = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(baseClass);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, baseClass->type);
    TEST_ASSERT_NOT_NULL(baseClass->data.classDeclaration.members);
    baseConstructor = baseClass->data.classDeclaration.members->nodes[0];
    TEST_ASSERT_NOT_NULL(baseConstructor);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_META_FUNCTION, baseConstructor->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);
    TEST_ASSERT_FALSE_MESSAGE(
            cs.hasError,
            cs.errorMessage != ZR_NULL ? cs.errorMessage
                                       : "source super constructor compile failed");
    TEST_ASSERT_NOT_NULL(cs.semanticContext);

    position = consumer_range(
            (TZrSize)(superCall - source + strlen("super(")),
            (TZrSize)(superCall - source + strlen("super(")));
    position.source = sourceName;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, position, ZR_NULL, &query));
    TEST_ASSERT_FALSE(query.isMemberCall);
    TEST_ASSERT_FALSE(query.hasNamedArguments);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)query.argumentCount);
    TEST_ASSERT_TRUE(query.hasResolvedTarget);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.targetSymbolId);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(superCall - source),
            (TZrUInt64)query.callSiteRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(superCall - source + strlen("super(seed)")),
            (TZrUInt64)query.callSiteRange.end.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(superCall - source),
            (TZrUInt64)query.callTargetRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(superCall - source + strlen("super")),
            (TZrUInt64)query.callTargetRange.end.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)baseConstructor->location.start.offset,
            (TZrUInt64)query.targetDeclarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)baseConstructor->location.end.offset,
            (TZrUInt64)query.targetDeclarationRange.end.offset);
    TEST_ASSERT_NOT_NULL(query.argumentMappings);
    TEST_ASSERT_EQUAL_UINT(1U, query.argumentMappings->length);
    mapping = (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
            (SZrArray *)query.argumentMappings, 0U);
    TEST_ASSERT_NOT_NULL(mapping);
    TEST_ASSERT_EQUAL_UINT(0U, mapping->argumentIndex);
    TEST_ASSERT_EQUAL_UINT(0U, mapping->parameterIndex);
    TEST_ASSERT_FALSE(mapping->isNamed);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_CALL_CONVERSION_IMPLICIT, mapping->conversion);
    TEST_ASSERT_NOT_EQUAL(mapping->argumentTypeId, mapping->parameterTypeId);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(superCall - source + strlen("super(")),
            (TZrUInt64)mapping->argumentRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64(
            (TZrUInt64)(superCall - source + strlen("super(seed")),
            (TZrUInt64)mapping->argumentRange.end.offset);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, query.callableTypeId, typeLabel, sizeof(typeLabel)));
    TEST_ASSERT_EQUAL_STRING("fn(double) -> null", typeLabel);
    callableType = ZrParser_CanonicalType_Find(
            cs.semanticContext, query.callableTypeId);
    TEST_ASSERT_NOT_NULL(callableType);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callableType->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_NONE,
            callableType->data.function.receiverEffect);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &query, callLabel, sizeof(callLabel)));
    TEST_ASSERT_EQUAL_STRING("@constructor(origin: double): null", callLabel);
    declaration = ZrParser_SemanticQuery_DeclarationOf(
            cs.semanticContext, query.targetSymbolId, ZR_NULL);
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_PTR(baseConstructor, declaration->node);

    consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_callable_value_call_publishes_canonical_contract(void) {
    const TZrChar *source =
            "fn runBossScenarioImpl(seed: int, prepareAmount: int, battleAmount: int) {\n"
            "    return seed + prepareAmount + battleAmount;\n"
            "}\n"
            "pub var runBossScenario = runBossScenarioImpl;\n"
            "fn useScenario(): int { return runBossScenario(30, 7, 5); }\n";
    const TZrChar *call = strstr(source, "runBossScenario(30, 7, 5)");
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange position;
    SZrParserSemanticCallQuery query;
    TZrChar typeLabel[128];
    TZrChar callLabel[256];

    TEST_ASSERT_NOT_NULL(call);
    sourceName = ZrCore_String_Create(g_state, "canonical_callable_value.zr", 27u);
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

    position = consumer_range(
            (TZrSize)(call - source + strlen("runBossScenario(")),
            (TZrSize)(call - source + strlen("runBossScenario(")));
    position.source = sourceName;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, position, ZR_NULL, &query));
    TEST_ASSERT_NOT_NULL(query.reference);
    TEST_ASSERT_TRUE(query.reference->isResolved);
    TEST_ASSERT_TRUE(query.hasResolvedTarget);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.targetSymbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.callableTypeId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, query.callableTypeId, typeLabel, sizeof(typeLabel)));
    TEST_ASSERT_EQUAL_STRING("fn(int, int, int) -> int", typeLabel);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &query, callLabel, sizeof(callLabel)));
    TEST_ASSERT_EQUAL_STRING(
            "runBossScenario(seed: int, prepareAmount: int, battleAmount: int): int",
            callLabel);

    consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_lambda_callable_value_call_publishes_canonical_contract(void) {
    const TZrChar *source =
            "pub var add = fn(left: int, right: int): int => left + right;\n"
            "fn useAdd(): int { return add(20, 22); }\n";
    const TZrChar *call = strstr(source, "add(20, 22)");
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *lambdaNode;
    const SZrSemanticReferenceFact *declaration;
    SZrFileRange position;
    SZrParserSemanticCallQuery query;
    TZrChar typeLabel[128];
    TZrChar callLabel[256];

    TEST_ASSERT_NOT_NULL(call);
    sourceName = ZrCore_String_Create(g_state, "canonical_lambda_value.zr", 25u);
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32(
            0u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION,
                          ast->data.script.statements->nodes[0]->type);
    lambdaNode = ast->data.script.statements->nodes[0]
                         ->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(lambdaNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, lambdaNode->type);

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);
    compile_script(&cs, ast);
    TEST_ASSERT_FALSE(cs.hasError);
    TEST_ASSERT_NOT_NULL(cs.semanticContext);

    position = consumer_range(
            (TZrSize)(call - source + strlen("add(")),
            (TZrSize)(call - source + strlen("add(")));
    position.source = sourceName;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CallAt(
            cs.semanticContext, position, ZR_NULL, &query));
    TEST_ASSERT_NOT_NULL(query.reference);
    TEST_ASSERT_TRUE(query.reference->isResolved);
    TEST_ASSERT_TRUE(query.hasResolvedTarget);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.targetSymbolId);
    declaration = ZrParser_SemanticQuery_DeclarationOf(
            cs.semanticContext, query.targetSymbolId, ZR_NULL);
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_PTR(lambdaNode, declaration->node);
    TEST_ASSERT_TRUE(declaration->isResolved);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)lambdaNode->location.start.offset,
                             (TZrUInt64)query.targetDeclarationRange.start.offset);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)lambdaNode->location.end.offset,
                             (TZrUInt64)query.targetDeclarationRange.end.offset);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)lambdaNode->location.start.offset,
                             (TZrUInt64)declaration->range.start.offset);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)lambdaNode->location.end.offset,
                             (TZrUInt64)declaration->range.end.offset);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, query.callableTypeId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, query.callableTypeId, typeLabel, sizeof(typeLabel)));
    TEST_ASSERT_EQUAL_STRING("fn(int, int) -> int", typeLabel);
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_FormatCall(
            cs.semanticContext, &query, callLabel, sizeof(callLabel)));
    TEST_ASSERT_EQUAL_STRING("add(left: int, right: int): int", callLabel);

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
    TZrChar receiverTypeLabel[128];
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
    TEST_ASSERT_NOT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID, query.receiverTypeId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext,
            query.receiverTypeId,
            receiverTypeLabel,
            sizeof(receiverTypeLabel)));
    TEST_ASSERT_EQUAL_STRING("Box<int>", receiverTypeLabel);
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
            "native extern(\"fixture\") {\n"
            "    fn NativeAdd(lhs: i32, rhs: i32): i32;\n"
            "}\n"
            "fn use(): i32 { return NativeAdd(1, 2); }\n";
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
    TZrChar receiverTypeLabel[128];
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
    TEST_ASSERT_NOT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID, query.receiverTypeId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext,
            query.receiverTypeId,
            receiverTypeLabel,
            sizeof(receiverTypeLabel)));
    TEST_ASSERT_EQUAL_STRING("readonly Counter", receiverTypeLabel);
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
    TEST_ASSERT_NOT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID, query.receiverTypeId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext,
            query.receiverTypeId,
            receiverTypeLabel,
            sizeof(receiverTypeLabel)));
    TEST_ASSERT_EQUAL_STRING("Counter", receiverTypeLabel);
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

static void test_unbound_method_references_preserve_receiver_effect_roundtrip(void) {
    const TZrChar *source =
            "readonly struct Snapshot {\n"
            "  pub var value: int;\n"
            "  pub fn read(): int { return this.value; }\n"
            "}\n"
            "struct Buffer {\n"
            "  pub var value: int;\n"
            "  pub fn write(next: int): int { this.value = next; return this.value; }\n"
            "}\n"
            "var read = Snapshot.read;\n"
            "var write = Buffer.write;\n";
    const TZrChar *readonlyReference = strstr(source, "Snapshot.read");
    const TZrChar *mutableReference = strstr(source, "Buffer.write");
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange position;
    SZrParserSemanticTypeQuery query;
    const SZrCanonicalTypeNode *callableType;
    TZrChar typeLabel[128];
    TZrByte signature[256];
    TZrSize signatureLength = 0U;
    TZrTypeId importedTypeId = ZR_SEMANTIC_ID_INVALID;
    SZrArtifactDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(readonlyReference);
    TEST_ASSERT_NOT_NULL(mutableReference);
    sourceName = ZrCore_String_Create(
            g_state, "canonical_method_reference.zr", 29u);
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

    position = consumer_range(
            (TZrSize)(readonlyReference - source + strlen("Snapshot.")),
            (TZrSize)(readonlyReference - source + strlen("Snapshot.")));
    position.source = sourceName;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CanonicalTypeAt(
            cs.semanticContext, position, ZR_NULL, &query));
    callableType = ZrParser_CanonicalType_Find(cs.semanticContext, query.typeId);
    TEST_ASSERT_NOT_NULL(callableType);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callableType->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_READONLY,
            callableType->data.function.receiverEffect);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, query.typeId, typeLabel, sizeof(typeLabel)));
    TEST_ASSERT_EQUAL_STRING("const fn() -> int", typeLabel);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_WriteSignature(
                    cs.semanticContext,
                    query.typeId,
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
    TEST_ASSERT_EQUAL_UINT32(query.typeId, importedTypeId);

    position = consumer_range(
            (TZrSize)(mutableReference - source + strlen("Buffer.")),
            (TZrSize)(mutableReference - source + strlen("Buffer.")));
    position.source = sourceName;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CanonicalTypeAt(
            cs.semanticContext, position, ZR_NULL, &query));
    callableType = ZrParser_CanonicalType_Find(cs.semanticContext, query.typeId);
    TEST_ASSERT_NOT_NULL(callableType);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callableType->kind);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_RECEIVER_MUTABLE,
            callableType->data.function.receiverEffect);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, query.typeId, typeLabel, sizeof(typeLabel)));
    TEST_ASSERT_EQUAL_STRING("fn(int) -> int", typeLabel);
    signatureLength = 0U;
    importedTypeId = ZR_SEMANTIC_ID_INVALID;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_WriteSignature(
                    cs.semanticContext,
                    query.typeId,
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
    TEST_ASSERT_EQUAL_UINT32(query.typeId, importedTypeId);

    consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unbound_open_generic_owner_does_not_publish_callable_fact(void) {
    const TZrChar *source =
            "struct Box<T> {\n"
            "  pub var value: T;\n"
            "  pub fn read(): T { return this.value; }\n"
            "}\n"
            "var openRead = Box.read;\n";
    const TZrChar *openReference = strstr(source, "Box.read");
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange position;
    SZrParserSemanticTypeQuery query;

    TEST_ASSERT_NOT_NULL(openReference);
    sourceName = ZrCore_String_Create(
            g_state, "canonical_generic_method_reference.zr", 37u);
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

    position = consumer_range(
            (TZrSize)(openReference - source + strlen("Box.")),
            (TZrSize)(openReference - source + strlen("Box.")));
    position.source = sourceName;
    (void)ZrParser_SemanticQuery_CanonicalTypeAt(
            cs.semanticContext, position, ZR_NULL, &query);
    if (query.reference != ZR_NULL) {
        const SZrCanonicalTypeNode *referenceType =
                ZrParser_CanonicalType_Find(
                        cs.semanticContext, query.reference->typeId);
        TEST_ASSERT_FALSE(
                query.reference->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS &&
                query.reference->isResolved && referenceType != ZR_NULL &&
                referenceType->kind == ZR_CANONICAL_TYPE_FUNCTION);
    }

    consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unbound_method_with_untyped_parameter_does_not_publish_callable_fact(void) {
    const TZrChar *source =
            "struct Buffer {\n"
            "  pub fn write(next): int { return 1; }\n"
            "}\n"
            "var write = Buffer.write;\n";
    const TZrChar *reference = strstr(source, "Buffer.write");
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange position;
    SZrParserSemanticTypeQuery query;

    TEST_ASSERT_NOT_NULL(reference);
    sourceName = ZrCore_String_Create(
            g_state, "canonical_untyped_method_reference.zr", 37u);
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

    position = consumer_range(
            (TZrSize)(reference - source + strlen("Buffer.")),
            (TZrSize)(reference - source + strlen("Buffer.")));
    position.source = sourceName;
    (void)ZrParser_SemanticQuery_CanonicalTypeAt(
            cs.semanticContext, position, ZR_NULL, &query);
    if (query.reference != ZR_NULL) {
        const SZrCanonicalTypeNode *referenceType =
                ZrParser_CanonicalType_Find(
                        cs.semanticContext, query.reference->typeId);
        TEST_ASSERT_FALSE(
                query.reference->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS &&
                query.reference->isResolved && referenceType != ZR_NULL &&
                referenceType->kind == ZR_CANONICAL_TYPE_FUNCTION);
    }

    consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unbound_method_reference_preserves_structured_generic_return(void) {
    const TZrChar *source =
            "class Matrix<T, const N: int> { }\n"
            "readonly struct Factory {\n"
            "  pub fn make(): Matrix<int, 4> { return new Matrix<int, 4>(); }\n"
            "}\n"
            "var make = Factory.make;\n";
    const TZrChar *reference = strstr(source, "Factory.make");
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange position;
    SZrParserSemanticTypeQuery query;
    TZrChar typeLabel[160];

    TEST_ASSERT_NOT_NULL(reference);
    sourceName = ZrCore_String_Create(
            g_state, "canonical_structured_return_method_reference.zr", 47u);
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

    position = consumer_range(
            (TZrSize)(reference - source + strlen("Factory.")),
            (TZrSize)(reference - source + strlen("Factory.")));
    position.source = sourceName;
    TEST_ASSERT_TRUE(ZrParser_SemanticQuery_CanonicalTypeAt(
            cs.semanticContext, position, ZR_NULL, &query));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            cs.semanticContext, query.typeId, typeLabel, sizeof(typeLabel)));
    TEST_ASSERT_EQUAL_STRING("const fn() -> Matrix<int, 4>", typeLabel);

    consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_unbound_generic_method_does_not_publish_callable_fact(void) {
    const TZrChar *source =
            "struct Factory {\n"
            "  pub fn echo<T>(value: T): T { return value; }\n"
            "}\n"
            "var echo = Factory.echo;\n";
    const TZrChar *reference = strstr(source, "Factory.echo");
    SZrCompilerState cs;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFileRange position;
    SZrParserSemanticTypeQuery query;

    TEST_ASSERT_NOT_NULL(reference);
    sourceName = ZrCore_String_Create(
            g_state, "canonical_generic_method_reference.zr", 37u);
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

    position = consumer_range(
            (TZrSize)(reference - source + strlen("Factory.")),
            (TZrSize)(reference - source + strlen("Factory.")));
    position.source = sourceName;
    (void)ZrParser_SemanticQuery_CanonicalTypeAt(
            cs.semanticContext, position, ZR_NULL, &query);
    if (query.reference != ZR_NULL) {
        const SZrCanonicalTypeNode *referenceType =
                ZrParser_CanonicalType_Find(
                        cs.semanticContext, query.reference->typeId);
        TEST_ASSERT_FALSE(
                query.reference->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS &&
                query.reference->isResolved && referenceType != ZR_NULL &&
                referenceType->kind == ZR_CANONICAL_TYPE_FUNCTION);
    }

    consumer_release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, ast);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_vm_and_aot_consume_the_same_canonical_contract_and_fail_identically);
    RUN_TEST(test_canonical_consumer_projects_optional_domain_transfer_contract);
    RUN_TEST(test_canonical_consumer_validates_scheduler_contract_without_name_fallback);
    RUN_TEST(test_reflection_debug_and_layout_resolve_only_canonical_ids_and_tokens);
    RUN_TEST(test_semantic_query_projects_expression_and_call_types_from_canonical_facts);
    RUN_TEST(test_resolved_generic_call_publishes_closed_canonical_signature);
    RUN_TEST(test_source_constructors_publish_canonical_call_contracts);
    RUN_TEST(test_source_super_constructor_publishes_canonical_call_contract);
    RUN_TEST(test_callable_value_call_publishes_canonical_contract);
    RUN_TEST(test_lambda_callable_value_call_publishes_canonical_contract);
    RUN_TEST(test_resolved_generic_member_call_preserves_declaration_generic_clause);
    RUN_TEST(test_resolved_extern_call_preserves_parameter_names_in_canonical_signature);
    RUN_TEST(test_source_scoped_call_preserves_contract_and_target_identity);
    RUN_TEST(test_receiver_call_publishes_resolved_target_identity);
    RUN_TEST(test_unbound_method_references_preserve_receiver_effect_roundtrip);
    RUN_TEST(test_unbound_open_generic_owner_does_not_publish_callable_fact);
    RUN_TEST(test_unbound_method_with_untyped_parameter_does_not_publish_callable_fact);
    RUN_TEST(test_unbound_method_reference_preserves_structured_generic_return);
    RUN_TEST(test_unbound_generic_method_does_not_publish_callable_fact);
    RUN_TEST(test_reference_callable_contract_roundtrips_across_artifact_vm_and_aot);
    RUN_TEST(test_reference_callable_ref_export_matches_return_access);
    return UNITY_END();
}

/**
 * @unit CCONS-009 declaration::test_reference_callable_contract_roundtrips_across_artifact_vm_and_aot
 * @brief 固定跨TU Unity 用例原型：验证七种引用参数合同在 parser、artifact、VM 与 AOT 间一致
 *
 * ## Summary
 * main 文件的声明供 RUN_TEST 链接到 reference 定义。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；预期条件不符时 Unity 中断当前用例。
 *
 * ## Constraints and lifecycle
 * Parse/Compile 唯一 contract 语言体并单独 intern callable；检查七参数签名、readonly ref 导出、SCOPED合同和非法导出/flags拒绝，不执行语言体或artifact code。
 * 声明本身不分配或转移资源；定义的普通清理和失败边界见对应函数。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:31
 * tests/parser/test_reference_callable_consumers.c:142
 */

/**
 * @unit CCONS-010 declaration::test_reference_callable_ref_export_matches_return_access
 * @brief 固定跨TU Unity 用例原型：验证引用返回访问模式与签名导出字节一致
 *
 * ## Summary
 * main 文件的声明供 RUN_TEST 链接到 reference 定义。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败由 Unity 中断。
 *
 * ## Constraints and lifecycle
 * 分别 intern writable/readonly ref 返回 callable，核 signature[2]；将 readonly 签名该字节改为 writable 后期望 INVALID_SIGNATURE 且 out id 重设为 INVALID。
 * 声明本身不分配或转移资源；定义的普通清理和失败边界见对应函数。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:32
 * tests/parser/test_reference_callable_consumers.c:354
 */

/**
 * @unit CCONS-011 type::SZrConsumerArtifactFixture
 * @brief 聚合 canonical consumer 的合成行、节视图与文档
 *
 * ## Summary
 * 基础consumer测试在栈上构造并传给 consumer_fixture_init/write。
 *
 * ## Fields
 * signature[8]容纳5字节primitive签名；typeDef/typeRef/typeSpec、contract/layout、domainTransfer、schedulerContract为独立行；sections[9]及document是借用这些成员的视图。
 *
 * ## Value model
 * 初始化后9节 ZRO；typed token、canonical id、各独立哈希保持对应关系。
 *
 * ## Invariants and lifecycle
 * sections.data/document.sections 含指向本对象的内指针，不可裸搬移后继续使用旧地址；改动节数须维持数组容量和对应行生命期。
 * 由调用者栈持有，无堆析构；静态code是借用常量，序列化同步完成。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:34
 */

/**
 * @unit CCONS-012 state::file::g_state
 * @brief 保存每个 Unity 用例的共享 VM 夹具槽
 *
 * ## Summary
 * setUp 创建，基础和source-contract用例借用，tearDown销毁并置NULL。
 *
 * ## Fields
 * 单个 SZrState* 槽；静态零初始化。
 *
 * ## Value model
 * 它指向一个独立global的main state，不与reference用例的local state混同。
 *
 * ## Invariants and lifecycle
 * 本binary顺序运行，不提供并行访问同步。
 * 唯一归属由hooks管理；不会自动收集reference函数内另建VM。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:47
 */

/**
 * @unit CCONS-013 function::setUp
 * @brief 为每个 Unity 用例创建共享 VM 夹具
 *
 * ## Summary
 * UnityDefaultTestRun 在调用任何已注册测试函数前调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；VM 创建失败时由 Unity 失败断言中断本次用例。
 *
 * ## Constraints and lifecycle
 * g_state 仅供此二源目标按 Unity 顺序使用；不为并行用例提供互斥。
 * 成功 state 由 tearDown 销毁；reference 文件另建的局部 state 不属于此槽。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:49
 * tests/third_party/zr_unity/Unity/src/unity.c:2298
 */

/**
 * @unit CCONS-014 function::tearDown
 * @brief 销毁当前共享 VM 并清空全局夹具槽
 *
 * ## Summary
 * 默认 Unity runner 在受保护的用例结束后进入该 hook。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；g_state 为空时不操作。
 *
 * ## Constraints and lifecycle
 * 只管理 g_state，不枚举或销毁其他独立 VM；不可把它当成所有局部编译资源的统一清理器。
 * Destroy 返回后将槽置 NULL；致命崩溃保护另有跳过 tearDown 的路径，不作全异常清理保证。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:54
 * tests/third_party/zr_unity/Unity/src/unity.c:2303
 */

/**
 * @unit CCONS-015 function::consumer_write_u32
 * @brief 将测试整数写成四字节小端序字段
 *
 * ## Summary
 * 构造签名或定点破坏 artifact 字段时调用。
 *
 * ## Parameters
 * @param[out] bytes 至少4字节的可写连续区域；不验证 NULL 或容量。
 * @param[in] value 待按低字节优先编码的32位无符号值。
 *
 * ## Returns
 * 无返回值；覆盖 bytes[0..3]。
 *
 * ## Constraints and lifecycle
 * 调用者负责边界与不和所需输入产生破坏性重叠；无分配/状态码。
 * 只在本次调用写入调用者缓冲，不持有指针。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:61
 */

/**
 * @unit CCONS-016 function::consumer_write_u64
 * @brief 以两个32位写入构成八字节小端序字段
 *
 * ## Summary
 * 需要更改 artifact 的64位哈希字节时调用。
 *
 * ## Parameters
 * @param[out] bytes 至少8字节可写区域；本函数不检查容量。
 * @param[in] value 低32位先写、高32位后写的64位值。
 *
 * ## Returns
 * 无返回值；覆盖 bytes[0..7]。
 *
 * ## Constraints and lifecycle
 * 复用 consumer_write_u32，两个子区域均须有效；不是原子并发更新。
 * 不分配、不持有调用者缓冲。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:68
 */

/**
 * @unit CCONS-017 function::consumer_range
 * @brief 由字节偏移构造单行语义查询范围
 *
 * ## Summary
 * 用人工 source 的字节位置查询 canonical facts 前调用。
 *
 * ## Parameters
 * @param[in] start 起始字节偏移；调用者保证转为 TZrInt32 后加1仍可表示。
 * @param[in] end 结束字节偏移；调用者负责范围次序和单行查询约定。
 *
 * ## Returns
 * @return 按值返回行号均为1、列号为偏移+1的范围；source 为新建的 canonical_consumers.zr VM 字符串，未验证其非空。
 *
 * ## Constraints and lifecycle
 * 不计算真实多行位置；部分调用者随后改用当前 sourceName。文件名是诊断标识，不进行磁盘读取。
 * source 字符串归 g_state 所在 VM 管理，返回结构不转移字符串所有权。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:73
 */

/**
 * @unit CCONS-018 function::consumer_fixture_init
 * @brief 初始化可在原地址序列化的 canonical consumer 内存夹具
 *
 * ## Summary
 * 各基础 consumer 用例先调用本函数，再按本例需求改动字段或节视图。
 *
 * ## Parameters
 * @param[out] fixture 非NULL且可写的完整聚合；被 memset 覆盖，内部 sections/document 建立指向此聚合成员的借用指针。
 *
 * ## Returns
 * 无返回值；构造9节 ZRO 视图、5字节 int64 签名及一致 identity。
 *
 * ## Constraints and lifecycle
 * typeRef/typeSpec 外部哈希、layout/contract/domain/scheduler 哈希有意各自独立；不能将这些测试常量当全局ABI。
 * 不分配堆资源；document/sections 借用 fixture 自身和静态 code，序列化前须保持原地址有效，不能裸拷贝后假定内部指针已重定位。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:86
 */

/**
 * @unit CCONS-020 function::consumer_fixture_write
 * @brief 将测试夹具同步写入调用者内存并要求 writer 成功
 *
 * ## Summary
 * 基础测试在构造或改动 fixture 后取得 artifact 字节。
 *
 * ## Parameters
 * @param[in] fixture 非NULL且内部借用指针仍有效的夹具。
 * @param[out] buffer writer 可写区域，至少有 capacity 字节。
 * @param[in] capacity buffer 可写容量；不在 helper 内补充分配。
 *
 * ## Returns
 * @return 只有 OK 断言未中断时返回 writer 的 written 字节数；失败走 Unity 中断，不以0作为普通错误返回协议。
 *
 * ## Constraints and lifecycle
 * 调用 ZrCore_Artifact_Write，不创建文件，不把 helper 返回当独立错误恢复接口。
 * fixture 仅借用至调用结束，buffer 内容归调用者；local diagnostic 不逃逸。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:191
 */

/**
 * @unit CCONS-021 function::consumer_release_compiler_function
 * @brief 按两个编译函数槽的别名关系释放其函数载荷
 *
 * ## Summary
 * source-contract 用例在普通尾路径、CompilerState_Free 前调用一次。
 *
 * ## Parameters
 * @param[in,out] cs 非NULL的当前用例编译状态，函数必须属于 g_state；非别名 topLevel 先释放并清空，再释放 current 并清空。
 *
 * ## Returns
 * 无返回值；空函数槽跳过。
 *
 * ## Constraints and lifecycle
 * topLevel==current 时只走 current 分支，topLevel 别名槽不会被此分支清空；不能把它当作可任意重入的完整 cs reset。
 * 随后仍须调用 CompilerState_Free 和 Ast_Free；此顺序只存在于普通返回路径，不覆盖 Unity longjmp。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:205
 */

/**
 * @unit CCONS-022 function::test_vm_and_aot_consume_the_same_canonical_contract_and_fail_identically
 * @brief 在同一内存 ZRO 上比较 VM 与 AOT 的 canonical 根投影和诊断
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 验证两个消费入口对同一 identity、签名/布局/合同给出一致投影；随后破坏序列化签名哈希，分别断言 SIGNATURE_HASH_MISMATCH 及 expected/actual 字段一致。
 * 全部 artifact/投影/诊断为本函数栈对象；未加载模块或执行 artifact code。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:216
 * tests/parser/test_canonical_consumers.c:1713
 */

/**
 * @unit CCONS-023 function::test_canonical_consumer_projects_optional_domain_transfer_contract
 * @brief 验证可选 domain-transfer 节的解析与缺节拒绝
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 先断言 VALUE_COPY/schemaVersion/schemaHash，再将该可选节从 document 视图移除；打开仍可成功，显式 ResolveDomainTransfer 才期望 INVALID_SECTION。
 * 去节是本地 sections 视图重排，不修改磁盘或 provider。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:265
 * tests/parser/test_canonical_consumers.c:1714
 */

/**
 * @unit CCONS-024 function::test_canonical_consumer_validates_scheduler_contract_without_name_fallback
 * @brief 验证 scheduler 令牌身份、ABI、策略及缺节边界
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 比较 scheduler/task/job tokens、ABI1、ATTACHED/ISOLATED policy、SEND/SYNC、transport/scheduler hash；构造非法 token-kind 和缺节输入并检查拒绝；不以名字补缺。
 * 这里只调用 canonical scheduler projection，不创建 scheduler、task 或线程。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:313
 * tests/parser/test_canonical_consumers.c:1715
 */

/**
 * @unit CCONS-025 function::test_reflection_debug_and_layout_resolve_only_canonical_ids_and_tokens
 * @brief 验证 reflection、debug 和布局仅按 canonical 身份解析
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 从同一内存 consumer projection 按 token/canonical id 查询、比较 layout identity，并以不存在的 id99 检查拒绝；不依赖字符串名称 fallback。
 * Debug 查询只是 metadata forwarder，非 debug-agent 调试会话。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:437
 * tests/parser/test_canonical_consumers.c:1716
 */

/**
 * @unit CCONS-026 function::test_semantic_query_projects_expression_and_call_types_from_canonical_facts
 * @brief 由手工 semantic facts 验证类型查询与调用格式化
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 建立 int expression、call expression 与 reference facts，检查 CanonicalTypeAt/CallAt 的 canonical id 与 identity: fn(int) -> int 标签；hasResolvedTarget 为 FALSE。
 * context 和 inferred types 只在普通尾路径显式释放；Unity 断言非局部退出不经过这些尾语句。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:488
 * tests/parser/test_canonical_consumers.c:1717
 */

/**
 * @unit CCONS-027 function::test_resolved_generic_call_publishes_closed_canonical_signature
 * @brief 验证泛型调用关闭后的签名与零参调用标签
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * Parse/compile identity<T>(42) 与 zero()；查询 canonical fn(int) -> int 以及 zero 的命名调用标签，不执行函数。
 * source、AST 和 cs 的作用域限定本用例；查询结果借用 cs.semanticContext。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:583
 * tests/parser/test_canonical_consumers.c:1718
 */

/**
 * @unit CCONS-028 state::test_resolved_generic_call_publishes_closed_canonical_signature::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证泛型调用关闭后的签名与零参调用标签时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * Parse/compile identity<T>(42) 与 zero()；查询 canonical fn(int) -> int 以及 zero 的命名调用标签，不执行函数。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:584
 */

/**
 * @unit CCONS-029 function::test_source_constructors_publish_canonical_call_contracts
 * @brief 验证 new/init 构造调用的目标、参数映射与隐式转换
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * Parse/compile Hero(float) 与 Point(x,y)；断言 new Hero(42) 的 int→double 实参转换、constructor 目标范围及 init Point(y:2,x:1) 的 named 参数重排。
 * 构造器源码仅编译，未创建运行时 Hero/Point 实例。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:646
 * tests/parser/test_canonical_consumers.c:1719
 */

/**
 * @unit CCONS-030 state::test_source_constructors_publish_canonical_call_contracts::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证 new/init 构造调用的目标、参数映射与隐式转换时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * Parse/compile Hero(float) 与 Point(x,y)；断言 new Hero(42) 的 int→double 实参转换、constructor 目标范围及 init Point(y:2,x:1) 的 named 参数重排。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:647
 */

/**
 * @unit CCONS-031 function::test_source_super_constructor_publishes_canonical_call_contract
 * @brief 验证 super 构造调用的 canonical 目标及范围
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * Parse/compile BossHero(int) super(seed) 到 BaseHero(float)，核 receiver NONE、位置参数映射、int→double 转换及调用/声明范围。
 * 此处是编译期 super-call metadata，未运行继承构造过程。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:828
 * tests/parser/test_canonical_consumers.c:1720
 */

/**
 * @unit CCONS-032 state::test_source_super_constructor_publishes_canonical_call_contract::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证 super 构造调用的 canonical 目标及范围时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * Parse/compile BossHero(int) super(seed) 到 BaseHero(float)，核 receiver NONE、位置参数映射、int→double 转换及调用/声明范围。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:829
 */

/**
 * @unit CCONS-033 function::test_callable_value_call_publishes_canonical_contract
 * @brief 验证命名函数别名调用的 canonical 合同
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 编译 runBossScenarioImpl 及其公开变量别名，核三个 int 参数与 int 返回的 canonical type 和命名调用标签。
 * 不得把别名 fixture 的计算结果当作运行断言。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:946
 * tests/parser/test_canonical_consumers.c:1721
 */

/**
 * @unit CCONS-034 state::test_callable_value_call_publishes_canonical_contract::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证命名函数别名调用的 canonical 合同时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * 编译 runBossScenarioImpl 及其公开变量别名，核三个 int 参数与 int 返回的 canonical type 和命名调用标签。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:947
 */

/**
 * @unit CCONS-035 function::test_lambda_callable_value_call_publishes_canonical_contract
 * @brief 验证 lambda 值调用的目标身份与命名签名
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 从 AST 首个变量取得 lambdaNode；编译后比较 CallAt/DeclarationOf 返回节点、声明范围、两个 int 参数和 add(left,right) 标签。
 * AST 节点和 semantic reference 是借用对象，必须在 AST/cs 释放前使用。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1002
 * tests/parser/test_canonical_consumers.c:1722
 */

/**
 * @unit CCONS-036 state::test_lambda_callable_value_call_publishes_canonical_contract::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证 lambda 值调用的目标身份与命名签名时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * 从 AST 首个变量取得 lambdaNode；编译后比较 CallAt/DeclarationOf 返回节点、声明范围、两个 int 参数和 add(left,right) 标签。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1003
 */

/**
 * @unit CCONS-037 function::test_resolved_generic_member_call_preserves_declaration_generic_clause
 * @brief 验证泛型成员调用保留声明泛型子句
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * Box<int>.shape 接受 Matrix<int,4>，核 receiver Box<int> 和含 const N 泛型子句的显示签名。
 * 只检查编译后的泛型/显示合同，未验证 Matrix 实例布局或执行。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1078
 * tests/parser/test_canonical_consumers.c:1723
 */

/**
 * @unit CCONS-038 state::test_resolved_generic_member_call_preserves_declaration_generic_clause::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证泛型成员调用保留声明泛型子句时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * Box<int>.shape 接受 Matrix<int,4>，核 receiver Box<int> 和含 const N 泛型子句的显示签名。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1079
 */

/**
 * @unit CCONS-039 function::test_resolved_extern_call_preserves_parameter_names_in_canonical_signature
 * @brief 验证 extern 声明的命名参数保留
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 编译 native extern("fixture") NativeAdd(lhs:i32,rhs:i32) 及 use 调用，期望显示标签保留 lhs/rhs。
 * 不加载名为 fixture 的原生库，不调用 NativeAdd 的实现。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1141
 * tests/parser/test_canonical_consumers.c:1724
 */

/**
 * @unit CCONS-040 state::test_resolved_extern_call_preserves_parameter_names_in_canonical_signature::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证 extern 声明的命名参数保留时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * 编译 native extern("fixture") NativeAdd(lhs:i32,rhs:i32) 及 use 调用，期望显示标签保留 lhs/rhs。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1142
 */

/**
 * @unit CCONS-041 function::test_source_scoped_call_preserves_contract_and_target_identity
 * @brief 验证 scoped ref readonly 合同及目标身份可往返
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 编译 inspect/use，查询命名范围和 resolved target；写 signature、再 intern，并断言 canonical id 相同。
 * signature 是本地字节缓冲；不把签名往返当作被引用值的运行时生命周期证明。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1183
 * tests/parser/test_canonical_consumers.c:1725
 */

/**
 * @unit CCONS-042 state::test_source_scoped_call_preserves_contract_and_target_identity::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证 scoped ref readonly 合同及目标身份可往返时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * 编译 inspect/use，查询命名范围和 resolved target；写 signature、再 intern，并断言 canonical id 相同。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1184
 */

/**
 * @unit CCONS-043 function::test_receiver_call_publishes_resolved_target_identity
 * @brief 验证只读和可变 receiver 调用的目标元数据
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 编译 Counter.read/write 与 use/change；核 const fn()/fn(int)、readonly Counter/Counter receiver、method AST 声明范围和目标 id。
 * 仅读取编译期 facts；未实际修改 Counter.value。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1267
 * tests/parser/test_canonical_consumers.c:1726
 */

/**
 * @unit CCONS-044 state::test_receiver_call_publishes_resolved_target_identity::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证只读和可变 receiver 调用的目标元数据时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * 编译 Counter.read/write 与 use/change；核 const fn()/fn(int)、readonly Counter/Counter receiver、method AST 声明范围和目标 id。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1268
 */

/**
 * @unit CCONS-045 function::test_unbound_method_references_preserve_receiver_effect_roundtrip
 * @brief 验证未绑定方法引用保留 receiver effect 并往返
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 编译 Snapshot.read 与 Buffer.write 别名，分别核 READONLY/MUTABLE callable receiver、显示签名和 artifact signature intern 后 id。
 * 未绑定方法值仍带 receiver 合同，不能当无 receiver 的普通函数解释。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1394
 * tests/parser/test_canonical_consumers.c:1727
 */

/**
 * @unit CCONS-046 state::test_unbound_method_references_preserve_receiver_effect_roundtrip::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证未绑定方法引用保留 receiver effect 并往返时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * 编译 Snapshot.read 与 Buffer.write 别名，分别核 READONLY/MUTABLE callable receiver、显示签名和 artifact signature intern 后 id。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1395
 */

/**
 * @unit CCONS-047 function::test_unbound_open_generic_owner_does_not_publish_callable_fact
 * @brief 验证开放泛型 owner 不发布已解析 callable fact
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 编译 Box<T> 与 Box.read；允许查询无结果，但若返回 reference，不得同时是已解析 MEMBER_ACCESS 和 canonical FUNCTION。
 * 忽略查询 bool 后读取 query 依赖 CanonicalTypeAt 入口先清零 outQuery；不是未初始化读取。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1514
 * tests/parser/test_canonical_consumers.c:1728
 */

/**
 * @unit CCONS-048 state::test_unbound_open_generic_owner_does_not_publish_callable_fact::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证开放泛型 owner 不发布已解析 callable fact时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * 编译 Box<T> 与 Box.read；允许查询无结果，但若返回 reference，不得同时是已解析 MEMBER_ACCESS 和 canonical FUNCTION。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1515
 */

/**
 * @unit CCONS-049 function::test_unbound_method_with_untyped_parameter_does_not_publish_callable_fact
 * @brief 验证未定类型参数的方法引用不发布完整 callable fact
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 编译 Buffer.write(next)；以同一负向谓词排除已解析 MEMBER_ACCESS/FUNCTION 的错误发布。
 * next 故意无类型；不把该 fixture 解释为运行时调用失败用例。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1565
 * tests/parser/test_canonical_consumers.c:1729
 */

/**
 * @unit CCONS-050 state::test_unbound_method_with_untyped_parameter_does_not_publish_callable_fact::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证未定类型参数的方法引用不发布完整 callable fact时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * 编译 Buffer.write(next)；以同一负向谓词排除已解析 MEMBER_ACCESS/FUNCTION 的错误发布。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1566
 */

/**
 * @unit CCONS-051 function::test_unbound_method_reference_preserves_structured_generic_return
 * @brief 验证未绑定方法的结构化泛型返回类型
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 编译 readonly Factory.make 返回 Matrix<int,4>，期望 canonical 显示 const fn() -> Matrix<int, 4>。
 * 该用例只证明类型显示；未执行 new Matrix<int,4>()。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1615
 * tests/parser/test_canonical_consumers.c:1730
 */

/**
 * @unit CCONS-052 state::test_unbound_method_reference_preserves_structured_generic_return::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证未绑定方法的结构化泛型返回类型时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * 编译 readonly Factory.make 返回 Matrix<int,4>，期望 canonical 显示 const fn() -> Matrix<int, 4>。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1616
 */

/**
 * @unit CCONS-053 function::test_unbound_generic_method_does_not_publish_callable_fact
 * @brief 验证开放方法泛型不发布已解析 callable fact
 *
 * ## Summary
 * 此目标 main 的 RUN_TEST 通过 UnityTestFunction 顺序调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败进入 Unity 的受保护非局部退出，不提供 bool 错误返回。
 *
 * ## Constraints and lifecycle
 * 编译 Factory.echo<T> 和 Factory.echo 别名；若有 reference，排除错误的已解析 MEMBER_ACCESS/FUNCTION 组合。
 * 该负向断言不要求查询一定返回 FALSE，也不证明所有无关 facts 缺失。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1661
 * tests/parser/test_canonical_consumers.c:1731
 */

/**
 * @unit CCONS-054 state::test_unbound_generic_method_does_not_publish_callable_fact::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证开放方法泛型不发布已解析 callable fact时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * 编译 Factory.echo<T> 和 Factory.echo 别名；若有 reference，排除错误的已解析 MEMBER_ACCESS/FUNCTION 组合。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1662
 */

/**
 * @unit CCONS-055 function::main
 * @brief 顺序注册并运行此二源目标的21个 Unity 用例
 *
 * ## Summary
 * zr_vm_canonical_consumers_test 可执行程序的进程入口。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 返回 UNITY_END()/UnityEnd 汇总的失败数量；0表示此轮注册用例无 Unity failure，不是全仓测试状态。
 *
 * ## Constraints and lifecycle
 * 19个本文件静态测试加2个 reference 导出测试；RUN_TEST 经 UnityTestFunction 回调调用，共享同一 setUp/tearDown。
 * 不执行 fixture 内的 Zr 函数体；没有额外 DestroyAll/atexit 回收局部VM的入口。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1711
 */

/**
 * @unit CCONS-071 fixture::test_resolved_generic_call_publishes_closed_canonical_signature::module::identity<T>
 * @brief 透传泛型实参以提供关闭调用的声明
 *
 * ## Summary
 * 所属C用例 test_resolved_generic_call_publishes_closed_canonical_signature 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] value 传入的泛型 T 值，body 只返回该值。
 *
 * ## Returns
 * @return value 参数原样返回。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:585
 * tests/parser/test_canonical_consumers.c:583
 */

/**
 * @unit CCONS-072 fixture::test_resolved_generic_call_publishes_closed_canonical_signature::module::zero
 * @brief 提供独立零参调用目标
 *
 * ## Summary
 * 所属C用例 test_resolved_generic_call_publishes_closed_canonical_signature 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 返回 int 字面量 0。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:586
 * tests/parser/test_canonical_consumers.c:583
 */

/**
 * @unit CCONS-073 fixture::test_resolved_generic_call_publishes_closed_canonical_signature::module::use
 * @brief 形成 identity<int> 与 zero 的两个静态调用点
 *
 * ## Summary
 * 所属C用例 test_resolved_generic_call_publishes_closed_canonical_signature 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 声明局部 value = identity(42)，最终返回 zero() 的 int。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:587
 * tests/parser/test_canonical_consumers.c:583
 */

/**
 * @unit CCONS-074 fixture::test_source_constructors_publish_canonical_call_contracts::type::Hero
 * @brief 定义 Hero 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * class，无显式实例字段；只有 seed:float 构造器。
 *
 * ## Value model
 * Hero；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * 空构造体不建立持久字段值；new Hero(42) 仅用于编译期转换与目标查询。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:648
 * tests/parser/test_canonical_consumers.c:646
 */

/**
 * @unit CCONS-075 fixture::test_source_constructors_publish_canonical_call_contracts::type::Point
 * @brief 定义 Point 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * x:int、y:int 两个公开字段。
 *
 * ## Value model
 * Point；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * 构造体分别从同名形参写 this.x/this.y；init 的命名实参与形参映射单独断言，未运行初始化。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:651
 * tests/parser/test_canonical_consumers.c:646
 */

/**
 * @unit CCONS-076 fixture::test_source_constructors_publish_canonical_call_contracts::Hero::@constructor
 * @brief 提供空 Hero 构造器的 float 形参接口
 *
 * ## Summary
 * 所属C用例 test_source_constructors_publish_canonical_call_contracts 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] seed float 输入；空 body 不保存该参数。
 *
 * ## Returns
 * 无显式返回语句；外围 new 表达式的类型与转换由 C 断言检查。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:649
 * tests/parser/test_canonical_consumers.c:646
 */

/**
 * @unit CCONS-077 fixture::test_source_constructors_publish_canonical_call_contracts::Point::@constructor
 * @brief 为命名参数重排提供 Point 构造器
 *
 * ## Summary
 * 所属C用例 test_source_constructors_publish_canonical_call_contracts 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] x 待写入 this.x 的 int。
 * @param[in] y 待写入 this.y 的 int。
 *
 * ## Returns
 * 无显式返回值；语言体将 x/y 分别赋给 this.x/this.y。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:654
 * tests/parser/test_canonical_consumers.c:646
 */

/**
 * @unit CCONS-078 fixture::test_source_constructors_publish_canonical_call_contracts::state::hero
 * @brief 建立语言夹具的 hero 值或别名
 *
 * ## Summary
 * 所属source中的表达式形成canonical查询入口。
 *
 * ## Fields
 * 一个语言变量/绑定，未另增函数体。
 *
 * ## Value model
 * 声明类型 Hero，初始化文本 new Hero(42)。
 *
 * ## Invariants and lifecycle
 * int 字面量到 float 构造形参的转换与目标范围是检查重点。
 * 仅Parse/Compile，未通过Function_Execute建立运行时对象；其语言body若有则由独立function unit追溯。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:656
 * tests/parser/test_canonical_consumers.c:646
 */

/**
 * @unit CCONS-079 fixture::test_source_constructors_publish_canonical_call_contracts::state::point
 * @brief 建立语言夹具的 point 值或别名
 *
 * ## Summary
 * 所属source中的表达式形成canonical查询入口。
 *
 * ## Fields
 * 一个语言变量/绑定，未另增函数体。
 *
 * ## Value model
 * 声明类型 Point，初始化文本 init Point(y:2,x:1)。
 *
 * ## Invariants and lifecycle
 * 命名实参顺序 y,x 与声明 x,y 故意不同；不可将源码次序当作参数序号。
 * 仅Parse/Compile，未通过Function_Execute建立运行时对象；其语言body若有则由独立function unit追溯。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:657
 * tests/parser/test_canonical_consumers.c:646
 */

/**
 * @unit CCONS-080 fixture::test_source_super_constructor_publishes_canonical_call_contract::type::BaseHero
 * @brief 定义 BaseHero 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * class，无显式字段，声明 origin:float 构造器。
 *
 * ## Value model
 * BaseHero；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * 作为 BossHero 的基类构造目标，空 body 不存储 origin。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:830
 * tests/parser/test_canonical_consumers.c:828
 */

/**
 * @unit CCONS-081 fixture::test_source_super_constructor_publishes_canonical_call_contract::type::BossHero
 * @brief 定义 BossHero 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * 继承 BaseHero，无新增显式字段，声明 seed:int 构造器。
 *
 * ## Value model
 * BossHero；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * super(seed) 提供 int→float 的编译期基类参数映射。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:833
 * tests/parser/test_canonical_consumers.c:828
 */

/**
 * @unit CCONS-082 fixture::test_source_super_constructor_publishes_canonical_call_contract::BaseHero::@constructor
 * @brief 提供 BaseHero 的 float 构造接口
 *
 * ## Summary
 * 所属C用例 test_source_super_constructor_publishes_canonical_call_contract 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] origin float 输入；空 body 不保存该参数。
 *
 * ## Returns
 * 无显式返回语句；body 为空。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:831
 * tests/parser/test_canonical_consumers.c:828
 */

/**
 * @unit CCONS-083 fixture::test_source_super_constructor_publishes_canonical_call_contract::BossHero::@constructor
 * @brief 形成 int 实参到基类 float 构造器的 super 调用
 *
 * ## Summary
 * 所属C用例 test_source_super_constructor_publishes_canonical_call_contract 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] seed 传给 BaseHero 构造器的 int，编译期检查隐式转换。
 *
 * ## Returns
 * 无显式返回值；super(seed) 是构造头的一部分。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:834
 * tests/parser/test_canonical_consumers.c:828
 */

/**
 * @unit CCONS-084 fixture::test_callable_value_call_publishes_canonical_contract::module::runBossScenarioImpl
 * @brief 提供三参数函数值别名的被指向函数
 *
 * ## Summary
 * 所属C用例 test_callable_value_call_publishes_canonical_contract 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] seed 加法中的首个 int。
 * @param[in] prepareAmount 加法中的第二个 int。
 * @param[in] battleAmount 加法中的第三个 int。
 *
 * ## Returns
 * @return 语言体返回 seed + prepareAmount + battleAmount，编译后的返回类型被断言为 int。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:948
 * tests/parser/test_canonical_consumers.c:946
 */

/**
 * @unit CCONS-085 fixture::test_callable_value_call_publishes_canonical_contract::module::useScenario
 * @brief 经变量别名形成三参数 callable-value 调用
 *
 * ## Summary
 * 所属C用例 test_callable_value_call_publishes_canonical_contract 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 返回 runBossScenario(30,7,5) 的 int 结果。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:952
 * tests/parser/test_canonical_consumers.c:946
 */

/**
 * @unit CCONS-086 fixture::test_callable_value_call_publishes_canonical_contract::state::runBossScenario
 * @brief 建立语言夹具的 runBossScenario 值或别名
 *
 * ## Summary
 * 所属source中的表达式形成canonical查询入口。
 *
 * ## Fields
 * 一个语言变量/绑定，未另增函数体。
 *
 * ## Value model
 * 公开变量初始化为 runBossScenarioImpl 的函数值。
 *
 * ## Invariants and lifecycle
 * 别名供 useScenario 静态调用；保留三参数函数的 canonical 合同。
 * 仅Parse/Compile，未通过Function_Execute建立运行时对象；其语言body若有则由独立function unit追溯。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:951
 * tests/parser/test_canonical_consumers.c:946
 */

/**
 * @unit CCONS-087 fixture::test_lambda_callable_value_call_publishes_canonical_contract::module::lambda
 * @brief 形成可命名 lambda 值的两参数函数体
 *
 * ## Summary
 * 所属C用例 test_lambda_callable_value_call_publishes_canonical_contract 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] left 加法左侧 int。
 * @param[in] right 加法右侧 int。
 *
 * ## Returns
 * @return 返回 left + right 的 int。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1004
 * tests/parser/test_canonical_consumers.c:1002
 */

/**
 * @unit CCONS-088 fixture::test_lambda_callable_value_call_publishes_canonical_contract::module::useAdd
 * @brief 经 add 变量形成 lambda 调用点
 *
 * ## Summary
 * 所属C用例 test_lambda_callable_value_call_publishes_canonical_contract 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 返回 add(20,22) 的 int。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1005
 * tests/parser/test_canonical_consumers.c:1002
 */

/**
 * @unit CCONS-089 fixture::test_lambda_callable_value_call_publishes_canonical_contract::state::add
 * @brief 建立语言夹具的 add 值或别名
 *
 * ## Summary
 * 所属source中的表达式形成canonical查询入口。
 *
 * ## Fields
 * 一个语言变量/绑定，未另增函数体。
 *
 * ## Value model
 * 公开变量初始化为二元 int lambda。
 *
 * ## Invariants and lifecycle
 * 变量、lambda AST 与声明目标身份需关联，不能把变量本身计为第二个函数体。
 * 仅Parse/Compile，未通过Function_Execute建立运行时对象；其语言body若有则由独立function unit追溯。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1004
 * tests/parser/test_canonical_consumers.c:1002
 */

/**
 * @unit CCONS-090 fixture::test_resolved_generic_member_call_preserves_declaration_generic_clause::type::Matrix
 * @brief 定义 Matrix<T,const N:int> 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * 无实例字段；T 为类型参数，N 为 int 常量参数。
 *
 * ## Value model
 * Matrix<T,const N:int>；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * 空类仅承载结构化类型身份，N 不是运行时字段。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1080
 * tests/parser/test_canonical_consumers.c:1078
 */

/**
 * @unit CCONS-091 fixture::test_resolved_generic_member_call_preserves_declaration_generic_clause::type::Box
 * @brief 定义 Box<T> 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * 无实例字段；shape<const N:int> 是公开方法。
 *
 * ## Value model
 * Box<T>；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * 接收者在本夹具关闭为 Box<int>，方法 Matrix<T,N> 仍保留声明泛型子句。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1081
 * tests/parser/test_canonical_consumers.c:1078
 */

/**
 * @unit CCONS-092 fixture::test_resolved_generic_member_call_preserves_declaration_generic_clause::Box::shape<const N: int>
 * @brief 提供带 owner T 和方法 const N 的泛型成员声明
 *
 * ## Summary
 * 所属C用例 test_resolved_generic_member_call_preserves_declaration_generic_clause 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] value Matrix<T,N> 输入，body 原样返回。
 *
 * ## Returns
 * @return 返回传入的 Matrix<T,N> 值，不另分配 Matrix。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1082
 * tests/parser/test_canonical_consumers.c:1078
 */

/**
 * @unit CCONS-093 fixture::test_resolved_generic_member_call_preserves_declaration_generic_clause::state::box
 * @brief 建立语言夹具的 box 值或别名
 *
 * ## Summary
 * 所属source中的表达式形成canonical查询入口。
 *
 * ## Fields
 * 一个语言变量/绑定，未另增函数体。
 *
 * ## Value model
 * 初始化为 new Box<int>()。
 *
 * ## Invariants and lifecycle
 * 作为关闭泛型 owner 的成员调用接收者。
 * 仅Parse/Compile，未通过Function_Execute建立运行时对象；其语言body若有则由独立function unit追溯。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1084
 * tests/parser/test_canonical_consumers.c:1078
 */

/**
 * @unit CCONS-094 fixture::test_resolved_generic_member_call_preserves_declaration_generic_clause::state::value
 * @brief 建立语言夹具的 value 值或别名
 *
 * ## Summary
 * 所属source中的表达式形成canonical查询入口。
 *
 * ## Fields
 * 一个语言变量/绑定，未另增函数体。
 *
 * ## Value model
 * 初始化为 new Matrix<int,4>()。
 *
 * ## Invariants and lifecycle
 * 为 shape 的类型/const 泛型推导提供实参。
 * 仅Parse/Compile，未通过Function_Execute建立运行时对象；其语言body若有则由独立function unit追溯。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1085
 * tests/parser/test_canonical_consumers.c:1078
 */

/**
 * @unit CCONS-095 fixture::test_resolved_extern_call_preserves_parameter_names_in_canonical_signature::module::NativeAdd
 * @brief 提供原生 extern 的无函数体接口
 *
 * ## Summary
 * 所属C用例 test_resolved_extern_call_preserves_parameter_names_in_canonical_signature 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] lhs i32 左操作数形参，仅验证名字。
 * @param[in] rhs i32 右操作数形参，仅验证名字。
 *
 * ## Returns
 * @return 声明的返回类型为 i32；此声明没有可执行实现。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。NativeAdd仅声明接口，没有body或动态库执行证据。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1144
 * tests/parser/test_canonical_consumers.c:1141
 */

/**
 * @unit CCONS-096 fixture::test_resolved_extern_call_preserves_parameter_names_in_canonical_signature::module::use
 * @brief 形成 extern NativeAdd 的静态调用点
 *
 * ## Summary
 * 所属C用例 test_resolved_extern_call_preserves_parameter_names_in_canonical_signature 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 返回 NativeAdd(1,2) 的声明类型 i32；本用例不调用原生实现。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1146
 * tests/parser/test_canonical_consumers.c:1141
 */

/**
 * @unit CCONS-097 fixture::test_source_scoped_call_preserves_contract_and_target_identity::module::inspect
 * @brief 提供 scoped ref readonly 参数的被调声明
 *
 * ## Summary
 * 所属C用例 test_source_scoped_call_preserves_contract_and_target_identity 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] value 函数作用域的只读 int 引用，本体未使用；不把它保存到外部。
 *
 * ## Returns
 * @return 返回 int 字面量 1，不读取或修改被引用值。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1185
 * tests/parser/test_canonical_consumers.c:1183
 */

/**
 * @unit CCONS-098 fixture::test_source_scoped_call_preserves_contract_and_target_identity::module::use
 * @brief 形成将只读引用传给 scoped 形参的调用点
 *
 * ## Summary
 * 所属C用例 test_source_scoped_call_preserves_contract_and_target_identity 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] value 只读 int 引用，以 ref 实参转交 inspect。
 *
 * ## Returns
 * @return 返回 inspect(ref value) 的 int。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1186
 * tests/parser/test_canonical_consumers.c:1183
 */

/**
 * @unit CCONS-099 fixture::test_receiver_call_publishes_resolved_target_identity::type::Counter
 * @brief 定义 Counter 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * 公开字段 value:int。
 *
 * ## Value model
 * Counter；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * read 为 const 方法只读字段，write 可更新字段；readonly/mutable receiver 分开查询。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1269
 * tests/parser/test_canonical_consumers.c:1267
 */

/**
 * @unit CCONS-100 fixture::test_receiver_call_publishes_resolved_target_identity::Counter::read
 * @brief 读取 Counter 接收者字段的 const 方法
 *
 * ## Summary
 * 所属C用例 test_receiver_call_publishes_resolved_target_identity 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 返回 this.value 的 int。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1271
 * tests/parser/test_canonical_consumers.c:1267
 */

/**
 * @unit CCONS-101 fixture::test_receiver_call_publishes_resolved_target_identity::Counter::write
 * @brief 提供可变 Counter receiver 的写方法
 *
 * ## Summary
 * 所属C用例 test_receiver_call_publishes_resolved_target_identity 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] next 写入接收者 value 的 int。
 *
 * ## Returns
 * @return 先赋 this.value = next，再返回该字段的 int。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1272
 * tests/parser/test_canonical_consumers.c:1267
 */

/**
 * @unit CCONS-102 fixture::test_receiver_call_publishes_resolved_target_identity::module::use
 * @brief 通过 readonly Counter 形成 const 方法调用
 *
 * ## Summary
 * 所属C用例 test_receiver_call_publishes_resolved_target_identity 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] counter 只读 Counter 接收者输入。
 *
 * ## Returns
 * @return 返回 counter.read() 的 int。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1274
 * tests/parser/test_canonical_consumers.c:1267
 */

/**
 * @unit CCONS-103 fixture::test_receiver_call_publishes_resolved_target_identity::module::change
 * @brief 通过可变 Counter 形成写方法调用
 *
 * ## Summary
 * 所属C用例 test_receiver_call_publishes_resolved_target_identity 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in,out] counter 可变 Counter 接收者，语言体可能经 write 更新其 value。
 *
 * ## Returns
 * @return 返回 counter.write(1) 的 int；若执行会更新 receiver.value。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1275
 * tests/parser/test_canonical_consumers.c:1267
 */

/**
 * @unit CCONS-104 fixture::test_unbound_method_references_preserve_receiver_effect_roundtrip::type::Snapshot
 * @brief 定义 Snapshot 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * readonly struct，公开字段 value:int。
 *
 * ## Value model
 * Snapshot；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * 方法 read 的未绑定值应保持 READONLY receiver effect，未运行读取。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1396
 * tests/parser/test_canonical_consumers.c:1394
 */

/**
 * @unit CCONS-105 fixture::test_unbound_method_references_preserve_receiver_effect_roundtrip::type::Buffer
 * @brief 定义 Buffer 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * 公开字段 value:int。
 *
 * ## Value model
 * Buffer；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * write 接受 int 并更新字段；未绑定值应保留可变 receiver 合同。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1400
 * tests/parser/test_canonical_consumers.c:1394
 */

/**
 * @unit CCONS-106 fixture::test_unbound_method_references_preserve_receiver_effect_roundtrip::Snapshot::read
 * @brief 提供 readonly struct 的未绑定方法目标
 *
 * ## Summary
 * 所属C用例 test_unbound_method_references_preserve_receiver_effect_roundtrip 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 返回 this.value 的 int。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1398
 * tests/parser/test_canonical_consumers.c:1394
 */

/**
 * @unit CCONS-107 fixture::test_unbound_method_references_preserve_receiver_effect_roundtrip::Buffer::write
 * @brief 提供可变 Buffer 的未绑定写方法目标
 *
 * ## Summary
 * 所属C用例 test_unbound_method_references_preserve_receiver_effect_roundtrip 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] next 写入 receiver.value 的 int。
 *
 * ## Returns
 * @return 先写 this.value = next，再返回该字段的 int。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1402
 * tests/parser/test_canonical_consumers.c:1394
 */

/**
 * @unit CCONS-108 fixture::test_unbound_method_references_preserve_receiver_effect_roundtrip::state::read
 * @brief 建立语言夹具的 read 值或别名
 *
 * ## Summary
 * 所属source中的表达式形成canonical查询入口。
 *
 * ## Fields
 * 一个语言变量/绑定，未另增函数体。
 *
 * ## Value model
 * 未绑定方法值 Snapshot.read。
 *
 * ## Invariants and lifecycle
 * 保留 readonly receiver effect；没有绑定某个 Snapshot 实例。
 * 仅Parse/Compile，未通过Function_Execute建立运行时对象；其语言body若有则由独立function unit追溯。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1404
 * tests/parser/test_canonical_consumers.c:1394
 */

/**
 * @unit CCONS-109 fixture::test_unbound_method_references_preserve_receiver_effect_roundtrip::state::write
 * @brief 建立语言夹具的 write 值或别名
 *
 * ## Summary
 * 所属source中的表达式形成canonical查询入口。
 *
 * ## Fields
 * 一个语言变量/绑定，未另增函数体。
 *
 * ## Value model
 * 未绑定方法值 Buffer.write。
 *
 * ## Invariants and lifecycle
 * 保留可变 receiver 合同；没有绑定某个 Buffer 实例。
 * 仅Parse/Compile，未通过Function_Execute建立运行时对象；其语言body若有则由独立function unit追溯。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1405
 * tests/parser/test_canonical_consumers.c:1394
 */

/**
 * @unit CCONS-110 fixture::test_unbound_open_generic_owner_does_not_publish_callable_fact::type::Box
 * @brief 定义 Box<T> 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * 公开字段 value:T。
 *
 * ## Value model
 * Box<T>；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * owner 的 T 故意未关闭；Box.read 不得被当成完整已解析 canonical function。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1516
 * tests/parser/test_canonical_consumers.c:1514
 */

/**
 * @unit CCONS-111 fixture::test_unbound_open_generic_owner_does_not_publish_callable_fact::Box::read
 * @brief 保留开放 owner T 的方法定义
 *
 * ## Summary
 * 所属C用例 test_unbound_open_generic_owner_does_not_publish_callable_fact 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 返回 this.value，静态声明返回类型为开放 T。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1518
 * tests/parser/test_canonical_consumers.c:1514
 */

/**
 * @unit CCONS-112 fixture::test_unbound_open_generic_owner_does_not_publish_callable_fact::state::openRead
 * @brief 建立语言夹具的 openRead 值或别名
 *
 * ## Summary
 * 所属source中的表达式形成canonical查询入口。
 *
 * ## Fields
 * 一个语言变量/绑定，未另增函数体。
 *
 * ## Value model
 * 未绑定方法值 Box.read，owner 泛型未关闭。
 *
 * ## Invariants and lifecycle
 * 负向用例只排除错误发布，不要求完全没有 reference fact。
 * 仅Parse/Compile，未通过Function_Execute建立运行时对象；其语言body若有则由独立function unit追溯。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1520
 * tests/parser/test_canonical_consumers.c:1514
 */

/**
 * @unit CCONS-113 fixture::test_unbound_method_with_untyped_parameter_does_not_publish_callable_fact::type::Buffer
 * @brief 定义 Buffer 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * 无显式字段；仅有 write(next):int 方法。
 *
 * ## Value model
 * Buffer；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * next 故意无类型，用于阻止发布完整 callable fact。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1567
 * tests/parser/test_canonical_consumers.c:1565
 */

/**
 * @unit CCONS-114 fixture::test_unbound_method_with_untyped_parameter_does_not_publish_callable_fact::Buffer::write
 * @brief 保留故意缺失参数类型的方法定义
 *
 * ## Summary
 * 所属C用例 test_unbound_method_with_untyped_parameter_does_not_publish_callable_fact 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] next 故意无显式类型的形参，用于不发布完整 callable fact 的负向检查。
 *
 * ## Returns
 * @return 返回 int 字面量 1，next 未读取。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1568
 * tests/parser/test_canonical_consumers.c:1565
 */

/**
 * @unit CCONS-115 fixture::test_unbound_method_with_untyped_parameter_does_not_publish_callable_fact::state::write
 * @brief 建立语言夹具的 write 值或别名
 *
 * ## Summary
 * 所属source中的表达式形成canonical查询入口。
 *
 * ## Fields
 * 一个语言变量/绑定，未另增函数体。
 *
 * ## Value model
 * 未绑定 Buffer.write，形参 next 没有类型。
 *
 * ## Invariants and lifecycle
 * 用于排除错误完整 callable fact，不执行调用。
 * 仅Parse/Compile，未通过Function_Execute建立运行时对象；其语言body若有则由独立function unit追溯。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1570
 * tests/parser/test_canonical_consumers.c:1565
 */

/**
 * @unit CCONS-116 fixture::test_unbound_method_reference_preserves_structured_generic_return::type::Matrix
 * @brief 定义 Matrix<T,const N:int> 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * 无实例字段；T 和常量 N 构成泛型类型参数。
 *
 * ## Value model
 * Matrix<T,const N:int>；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * 仅作为 Factory.make 的结构化返回类型。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1617
 * tests/parser/test_canonical_consumers.c:1615
 */

/**
 * @unit CCONS-117 fixture::test_unbound_method_reference_preserves_structured_generic_return::type::Factory
 * @brief 定义 Factory 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * readonly struct，无显式字段。
 *
 * ## Value model
 * Factory；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * make 的返回类型为 Matrix<int,4>，未绑定方法应保留 const receiver effect。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1618
 * tests/parser/test_canonical_consumers.c:1615
 */

/**
 * @unit CCONS-118 fixture::test_unbound_method_reference_preserves_structured_generic_return::Factory::make
 * @brief 提供结构化泛型返回类型的只读方法
 *
 * ## Summary
 * 所属C用例 test_unbound_method_reference_preserves_structured_generic_return 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * @return 语言体返回 new Matrix<int,4>()，C 用例只核其 canonical 返回类型。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1619
 * tests/parser/test_canonical_consumers.c:1615
 */

/**
 * @unit CCONS-119 fixture::test_unbound_method_reference_preserves_structured_generic_return::state::make
 * @brief 建立语言夹具的 make 值或别名
 *
 * ## Summary
 * 所属source中的表达式形成canonical查询入口。
 *
 * ## Fields
 * 一个语言变量/绑定，未另增函数体。
 *
 * ## Value model
 * 未绑定 readonly Factory.make。
 *
 * ## Invariants and lifecycle
 * 返回类型仍应包含 Matrix<int,4> 的结构化参数。
 * 仅Parse/Compile，未通过Function_Execute建立运行时对象；其语言body若有则由独立function unit追溯。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1621
 * tests/parser/test_canonical_consumers.c:1615
 */

/**
 * @unit CCONS-120 fixture::test_unbound_generic_method_does_not_publish_callable_fact::type::Factory
 * @brief 定义 Factory 的人工类型夹具
 *
 * ## Summary
 * 所属C用例解析/编译并查询构造器、成员或未绑定方法的类型事实。
 *
 * ## Fields
 * 无显式字段；拥有 echo<T> 泛型方法。
 *
 * ## Value model
 * Factory；泛型/readonly/继承信息由原语言源码表达。
 *
 * ## Invariants and lifecycle
 * 开放方法 T 不应因取得 Factory.echo 就发布已解析 callable fact。
 * 当前只Parse/Compile，未执行实例构造或检查对象内存布局；字段不是C struct的额外物理定义。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1663
 * tests/parser/test_canonical_consumers.c:1661
 */

/**
 * @unit CCONS-121 fixture::test_unbound_generic_method_does_not_publish_callable_fact::Factory::echo<T>
 * @brief 保留开放方法泛型的透传定义
 *
 * ## Summary
 * 所属C用例 test_unbound_generic_method_does_not_publish_callable_fact 的人工source输入定义；仅由Parse/Compile和semantic查询消费。
 *
 * ## Parameters
 * @param[in] value 方法泛型 T 输入，body 原样返回。
 *
 * ## Returns
 * @return 返回 value，声明类型为方法参数 T。
 *
 * ## Constraints and lifecycle
 * 同一fixture内的声明/变量是唯一语言调用上下文；不同source字符串中的同名符号不共享模块。本体的语言含义不等于当前测试已运行该计算或副作用。
 * 原C字符串保持逐字节不变；当前只验证编译期合同，不证明任意调用方的运行时资源/线程安全。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1664
 * tests/parser/test_canonical_consumers.c:1661
 */

/**
 * @unit CCONS-122 fixture::test_unbound_generic_method_does_not_publish_callable_fact::state::echo
 * @brief 建立语言夹具的 echo 值或别名
 *
 * ## Summary
 * 所属source中的表达式形成canonical查询入口。
 *
 * ## Fields
 * 一个语言变量/绑定，未另增函数体。
 *
 * ## Value model
 * 未绑定 Factory.echo<T>。
 *
 * ## Invariants and lifecycle
 * 开放方法泛型应保持未关闭边界，不执行 echo。
 * 仅Parse/Compile，未通过Function_Execute建立运行时对象；其语言body若有则由独立function unit追溯。
 *
 * ## References
 * tests/parser/test_canonical_consumers.c:1666
 * tests/parser/test_canonical_consumers.c:1661
 */
