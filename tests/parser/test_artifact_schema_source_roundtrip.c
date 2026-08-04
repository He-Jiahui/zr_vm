#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_common/zr_contract_conf.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/canonical_consumer.h"
#include "zr_vm_core/task_runtime.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_lib_thread/module.h"
#include "zr_vm_lib_thread/runtime.h"
#include "zr_vm_parser/artifact_projection.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/writer.h"

void test_source_without_scheduler_call_rejects_artifact_write(void);
void test_scheduler_artifact_writer_rejects_unavailable_provider(void);

#define SOURCE_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 21u)
#define SOURCE_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 21u)
#define SOURCE_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 21u)

void test_real_source_compile_and_binary_signature_import_are_identical(void);
void test_real_source_scheduler_call_publishes_canonical_source_fact(void);
void test_repeated_scheduler_calls_coalesce_canonical_source_fact(void);
void test_source_without_scheduler_call_publishes_no_scheduler_fact(void);
void test_real_source_scheduler_call_writes_and_imports_canonical_artifact(void);
void test_source_without_scheduler_provider_rejects_artifact_write(void);

void test_real_source_compile_and_binary_signature_import_are_identical(void) {
    static const TZrChar source[] = "fn identity(value: int): int { return value; }";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFunction *compiled;
    SZrFunctionDeclaration *declaration;
    SZrCompilerState projectionCompiler;
    SZrInferredType parameterType;
    SZrInferredType returnType;
    SZrArray parameterTypes;
    SZrParserArtifactPublicContract contract;
    SZrArtifactPublicIdentity sourceIdentity;
    SZrArtifactPublicIdentity importedIdentity;
    SZrArtifactDiagnostic diagnostic;
    TZrByte sourceSignature[256];
    TZrByte importedSignature[256];
    TZrSize sourceSignatureLength = 0u;
    TZrSize importedSignatureLength = 0u;
    TZrTypeId sourceTypeId;
    TZrTypeId importedTypeId = ZR_SEMANTIC_ID_INVALID;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "artifact_source_roundtrip.zr", 28u);
    ast = ZrParser_Parse(state, source, sizeof(source) - 1u, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION,
                          ast->data.script.statements->nodes[0]->type);

    compiled = ZrParser_Compiler_Compile(state, ast);
    TEST_ASSERT_NOT_NULL_MESSAGE(compiled, "real source must compile before artifact projection");

    ZrParser_CompilerState_Init(&projectionCompiler, state);
    declaration = &ast->data.script.statements->nodes[0]->data.functionDeclaration;
    TEST_ASSERT_NOT_NULL(declaration->params);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)declaration->params->count);
    ZrParser_InferredType_Init(state, &parameterType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(state, &returnType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &projectionCompiler,
            declaration->params->nodes[0]->data.parameter.typeInfo,
            &parameterType));
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &projectionCompiler,
            declaration->returnType,
            &returnType));
    ZrCore_Array_Init(state, &parameterTypes, sizeof(SZrInferredType), 1u);
    ZrCore_Array_Push(state, &parameterTypes, &parameterType);
    sourceTypeId = ZrParser_CanonicalType_FromFunctionSignature(
            projectionCompiler.semanticContext,
            &parameterTypes,
            ZR_NULL,
            &returnType,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, sourceTypeId);

    memset(&contract, 0, sizeof(contract));
    contract.typeRefToken = SOURCE_TYPE_REF_TOKEN;
    contract.typeSpecToken = SOURCE_TYPE_SPEC_TOKEN;
    contract.signatureToken = SOURCE_SIGNATURE_TOKEN;
    contract.layoutVersion = 1u;
    contract.layoutHash = 0x1020304050607080ULL;
    contract.callableContractHash = 0x2030405060708090ULL;
    contract.moduleHash = 0x30405060708090A0ULL;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_BuildPublicIdentity(
                    projectionCompiler.semanticContext,
                    sourceTypeId,
                    &contract,
                    sourceSignature,
                    sizeof(sourceSignature),
                    &sourceSignatureLength,
                    &sourceIdentity,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_InternSignature(
                    projectionCompiler.semanticContext,
                    ZR_NULL,
                    sourceSignature,
                    sourceSignatureLength,
                    &importedTypeId,
                    &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(sourceTypeId, importedTypeId);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_BuildPublicIdentity(
                    projectionCompiler.semanticContext,
                    importedTypeId,
                    &contract,
                    importedSignature,
                    sizeof(importedSignature),
                    &importedSignatureLength,
                    &importedIdentity,
                    &diagnostic));
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)sourceSignatureLength,
                             (TZrUInt64)importedSignatureLength);
    TEST_ASSERT_EQUAL_MEMORY(sourceSignature, importedSignature, sourceSignatureLength);
    TEST_ASSERT_EQUAL_MEMORY(&sourceIdentity, &importedIdentity, sizeof(sourceIdentity));

    ZrCore_Array_Free(state, &parameterTypes);
    ZrParser_InferredType_Free(state, &returnType);
    ZrParser_InferredType_Free(state, &parameterType);
    ZrParser_CompilerState_Free(&projectionCompiler);
    ZrParser_Ast_Free(state, ast);
    ZrTests_Runtime_State_Destroy(state);
}

static SZrState *create_scheduler_artifact_test_state(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return state;
    }
    ZrParser_ToGlobalState_Register(state);
    if (!ZrCore_TaskRuntime_RegisterBuiltins(state->global) ||
        !ZrVmThread_Register(state->global)) {
        ZrTests_Runtime_State_Destroy(state);
        return ZR_NULL;
    }
    return state;
}

static SZrFunction *compile_scheduler_artifact_source(SZrState *state,
                                                       const TZrChar *source,
                                                       TZrSize sourceLength,
                                                       const TZrChar *sourceNameText) {
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFunction *function;

    if (state == ZR_NULL || source == ZR_NULL || sourceNameText == ZR_NULL) {
        return ZR_NULL;
    }
    sourceName = ZrCore_String_Create(
            state,
            (TZrNativeString) sourceNameText,
            strlen(sourceNameText));
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }
    ast = ZrParser_Parse(state, source, sourceLength, sourceName);
    if (ast == ZR_NULL) {
        return ZR_NULL;
    }
    function = ZrParser_Compiler_Compile(state, ast);
    ZrParser_Ast_Free(state, ast);
    return function;
}

void test_real_source_scheduler_call_publishes_canonical_source_fact(void) {
    static const TZrChar source[] =
            "var task = import(\"zr.task\");\n"
            "var thread = import(\"zr.thread\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<int>(fn() => { return 7; });\n"
            "var completion = scheduler.schedule<int>(job);\n"
            "return completion.result();\n";
    SZrState *state = create_scheduler_artifact_test_state();
    SZrFunction *function;
    const SZrFunctionSchedulerSourceFact *fact;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_scheduler_artifact_source(
            state,
            source,
            sizeof(source) - 1u,
            "scheduler_artifact_source_fact.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "scheduler source must compile");
    TEST_ASSERT_EQUAL_UINT32(1u, function->schedulerSourceFactLength);

    fact = ZrCore_Function_FindSchedulerSourceFact(function, 0u);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, fact->schedulerTypeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, fact->scheduleMemberToken);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, fact->scheduleSignatureToken);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, fact->scheduleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(ZR_MEMBER_CONTRACT_ROLE_TASK_SCHEDULER_SCHEDULE,
                             fact->contractRole);
    TEST_ASSERT_TRUE((fact->schedulerProtocolMask &
                      ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_TASK_SCHEDULER)) != 0u);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void test_repeated_scheduler_calls_coalesce_canonical_source_fact(void) {
    static const TZrChar source[] =
            "var task = import(\"zr.task\");\n"
            "var thread = import(\"zr.thread\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var firstJob = init task.Job<int>(fn() => { return 7; });\n"
            "var secondJob = init task.Job<int>(fn() => { return 8; });\n"
            "var first = scheduler.schedule<int>(firstJob);\n"
            "var second = scheduler.schedule<int>(secondJob);\n"
            "return first.result() + second.result();\n";
    SZrState *state = create_scheduler_artifact_test_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_scheduler_artifact_source(
            state,
            source,
            sizeof(source) - 1u,
            "scheduler_artifact_source_fact_dedup.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "repeated scheduler source must compile");
    TEST_ASSERT_EQUAL_UINT32(1u, function->schedulerSourceFactLength);
    TEST_ASSERT_NOT_NULL(ZrCore_Function_FindSchedulerSourceFact(function, 0u));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void test_source_without_scheduler_call_publishes_no_scheduler_fact(void) {
    static const TZrChar source[] =
            "var thread = import(\"zr.thread\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "return scheduler;\n";
    SZrState *state = create_scheduler_artifact_test_state();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_scheduler_artifact_source(
            state,
            source,
            sizeof(source) - 1u,
            "scheduler_artifact_unavailable.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "source without schedule call must compile");
    TEST_ASSERT_EQUAL_UINT32(0u, function->schedulerSourceFactLength);
    TEST_ASSERT_NULL(ZrCore_Function_FindSchedulerSourceFact(function, 0u));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void test_real_source_scheduler_call_writes_and_imports_canonical_artifact(void) {
    static const TZrChar source[] =
            "var task = import(\"zr.task\");\n"
            "var thread = import(\"zr.thread\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<int>(fn() => { return 7; });\n"
            "var completion = scheduler.schedule<int>(job);\n"
            "return completion.result();\n";
    static const TZrChar artifactPath[] = "scheduler_source_contract.zro";
    SZrState *state = create_scheduler_artifact_test_state();
    SZrFunction *function;
    const SZrFunctionSchedulerSourceFact *fact;
    SZrArtifactDiagnostic diagnostic;
    SZrArtifactView view;
    SZrCanonicalConsumerProjection projection;
    SZrArtifactSchedulerContractRow writtenContract;
    SZrArtifactSchedulerContractRow importedContract;
    SZrArtifactDomainTransferRow transfer;
    SZrCanonicalSchedulerContractExpectation expectation;
    TZrByte bytes[16384];
    FILE *artifact;
    size_t byteLength;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_scheduler_artifact_source(
            state,
            source,
            sizeof(source) - 1u,
            "scheduler_artifact_source_writer.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(function, "scheduler source must compile before artifact write");
    fact = ZrCore_Function_FindSchedulerSourceFact(function, 0u);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, fact->schedulerTypeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, fact->taskTypeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, fact->jobTypeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, fact->schedulerProvider.metadataToken);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, fact->taskProvider.metadataToken);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, fact->jobProvider.metadataToken);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, fact->schedulerProvider.moduleSignatureHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, fact->taskProvider.moduleSignatureHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, fact->jobProvider.moduleSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(1u, fact->schedulerAbiVersion);
    TEST_ASSERT_EQUAL_UINT32(ZR_ARTIFACT_SCHEDULER_POLICY_ISOLATED_DOMAIN,
                             fact->schedulerPolicyMask);
    TEST_ASSERT_EQUAL_UINT32(ZR_ARTIFACT_SCHEDULER_REQUIREMENT_SEND,
                             fact->isolatedRequirementFlags);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, fact->transportContractHash);
    TEST_ASSERT_EQUAL_UINT64(fact->scheduleSignatureHash, fact->schedulerContractHash);

    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_Writer_WriteSchedulerArtifactFile(
                    state,
                    function,
                    artifactPath,
                    ZR_ARTIFACT_KIND_ZRO,
                    &diagnostic));

    artifact = fopen(artifactPath, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(artifact, "artifact writer must create a real .zro file");
    byteLength = fread(bytes, 1u, sizeof(bytes), artifact);
    TEST_ASSERT_TRUE(ferror(artifact) == 0);
    fclose(artifact);
    remove(artifactPath);

    TEST_ASSERT_TRUE(byteLength > 0u);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_Read(bytes, byteLength, &view, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_Open(bytes, byteLength, &view.identity, &projection, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_ResolveSchedulerContract(
                    &projection,
                    view.identity.typeRefToken,
                    &writtenContract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_ResolveSchedulerContract(
                    &projection,
                    writtenContract.schedulerTypeToken,
                    &importedContract,
                    &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&writtenContract, &importedContract, sizeof(importedContract));
    TEST_ASSERT_EQUAL_UINT64(fact->scheduleSignatureHash, importedContract.schedulerContractHash);
    memset(&expectation, 0, sizeof(expectation));
    expectation.schedulerTypeToken = importedContract.schedulerTypeToken;
    expectation.taskTypeToken = importedContract.taskTypeToken;
    expectation.jobTypeToken = importedContract.jobTypeToken;
    expectation.abiVersion = importedContract.abiVersion;
    expectation.policy = ZR_ARTIFACT_SCHEDULER_POLICY_ISOLATED_DOMAIN;
    expectation.requirementFlags = importedContract.isolatedRequirementFlags;
    expectation.transportContractHash = importedContract.transportContractHash;
    expectation.schedulerContractHash = importedContract.schedulerContractHash;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expectation, &diagnostic));
    expectation.abiVersion++;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_SCHEDULER_ABI_MISMATCH,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expectation, &diagnostic));
    expectation.abiVersion--;
    expectation.policy = ZR_ARTIFACT_SCHEDULER_POLICY_ATTACHED_DOMAIN;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_SCHEDULER_POLICY_MISMATCH,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expectation, &diagnostic));
    expectation.policy = ZR_ARTIFACT_SCHEDULER_POLICY_ISOLATED_DOMAIN;
    expectation.requirementFlags |= ZR_ARTIFACT_SCHEDULER_REQUIREMENT_SYNC;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_SCHEDULER_REQUIREMENT_MISMATCH,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expectation, &diagnostic));
    expectation.requirementFlags = importedContract.isolatedRequirementFlags;
    expectation.transportContractHash ^= 1u;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_TRANSPORT_CONTRACT_MISMATCH,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expectation, &diagnostic));
    expectation.transportContractHash = importedContract.transportContractHash;
    expectation.schedulerContractHash ^= 1u;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_SCHEDULER_CONTRACT_MISMATCH,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expectation, &diagnostic));
    expectation.schedulerContractHash = importedContract.schedulerContractHash;
    expectation.jobTypeToken = importedContract.taskTypeToken;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
            ZrCore_CanonicalConsumer_ValidateSchedulerContract(
                    &projection, &expectation, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_ResolveDomainTransfer(
                    &projection,
                    importedContract.jobTypeToken,
                    &transfer,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_DOMAIN_TRANSFER_RESOURCE_MOVE, transfer.kind);
    TEST_ASSERT_EQUAL_UINT64(fact->scheduleSignatureHash, transfer.providerContractHash);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void test_source_without_scheduler_call_rejects_artifact_write(void) {
    static const TZrChar source[] =
            "var thread = import(\"zr.thread\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "return scheduler;\n";
    SZrState *state = create_scheduler_artifact_test_state();
    SZrFunction *function;
    SZrArtifactDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_scheduler_artifact_source(
            state,
            source,
            sizeof(source) - 1u,
            "scheduler_artifact_writer_unavailable.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(0u, function->schedulerSourceFactLength);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
            ZrParser_Writer_WriteSchedulerArtifactFile(
                    state,
                    function,
                    "scheduler_artifact_writer_unavailable.zro",
                    ZR_ARTIFACT_KIND_ZRO,
                    &diagnostic));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

void test_scheduler_artifact_writer_rejects_unavailable_provider(void) {
    static const TZrChar source[] =
            "var task = import(\"zr.task\");\n"
            "var thread = import(\"zr.thread\");\n"
            "var scheduler = new thread.ThreadScheduler(1);\n"
            "var job = init task.Job<int>(fn() => { return 7; });\n"
            "var completion = scheduler.schedule<int>(job);\n"
            "return completion.result();\n";
    SZrState *state = create_scheduler_artifact_test_state();
    SZrFunction *function;
    SZrArtifactDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_scheduler_artifact_source(
            state,
            source,
            sizeof(source) - 1u,
            "scheduler_artifact_writer_missing_provider.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(1u, function->schedulerSourceFactLength);
    function->schedulerSourceFacts[0].jobProvider.moduleSignatureHash = 0u;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
            ZrParser_Writer_WriteSchedulerArtifactFile(
                    state,
                    function,
                    "scheduler_artifact_writer_missing_provider.zro",
                    ZR_ARTIFACT_KIND_ZRO,
                    &diagnostic));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}
