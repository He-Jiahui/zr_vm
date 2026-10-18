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
#include "artifact/artifact_projection.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/writer.h"

void test_source_without_scheduler_call_rejects_artifact_write(void);
void test_scheduler_artifact_writer_rejects_unavailable_provider(void);

// 源码签名导出与导入使用同一组三类 token，避免由测试夹具制造身份漂移。
#define SOURCE_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 21u)
#define SOURCE_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 21u)
#define SOURCE_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 21u)

void test_real_source_compile_and_binary_signature_import_are_identical(void);
void test_real_source_scheduler_call_publishes_canonical_source_fact(void);
void test_repeated_scheduler_calls_coalesce_canonical_source_fact(void);
void test_source_without_scheduler_call_publishes_no_scheduler_fact(void);
void test_real_source_scheduler_call_writes_and_imports_canonical_artifact(void);

// 从真实函数声明建立 canonical 签名，再导入同一上下文校验类型 ID 和公开身份。
// BUG: 建立 state、AST、推断类型和数组后若断言失败，Unity 跳过尾部 Free；共享空 tearDown 无法回收。
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

// 为源码调度器用例注册 task/thread 提供者；注册失败时销毁已建 state。
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

// 解析并编译调度器源码，AST 在返回前释放；成功函数归调用者管理。
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

// 一次 schedule 调用须产出带提供者、协议与签名信息的 canonical source fact。
// BUG: state/function 建立后若断言失败，函数尾部释放会被 Unity 跳过，空 tearDown 不持有这些局部资源。
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

// 两次 schedule 调用共享同一调度器契约事实，不产生重复记录。
// BUG: 编译后断言失败会越过局部 function/state 清理，空 tearDown 无法回收。
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

// 仅创建 scheduler 不调用 schedule 时，不应凭类型出现就发布 source fact。
// BUG: 断言失败跳过局部 function/state 销毁，空 tearDown 没有可回收的句柄。
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

// 真实源码事实写入 ZRO 后，由 canonical consumer 解析并逐字段校验调度与传输契约。
// BUG: 写入后若读取/契约断言失败，尾部 state/function 清理被跳过；ferror 断言失败还跳过 fclose/remove。
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
    // BUG: ferror 非零时断言 longjmp，下面的 fclose/remove 不执行，留下句柄和生成文件。
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

// 缺少 schedule 事实时写入器应拒绝生成调度器产物。
// BUG: 编译后的拒绝路径断言失败会跳过局部 function/state 释放，空 tearDown 无法补偿。
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

// 篡改 job 提供者身份后，写入器必须拒绝不完整的调度器契约。
// BUG: 篡改后的写入断言失败会跳过 function/state 释放，空 tearDown 无法补偿。
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

/**
 * @fn void test_real_source_compile_and_binary_signature_import_are_identical(void)
 * ## Summary
 * 解析并编译单int参数/int返回的identity函数，从AST声明构造canonical函数签名，再导入同一context比较类型ID、签名字节和公开身份。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_real_source_compile_and_binary_signature_import_are_identical
 *
 * ## Returns
 * 无返回值；由Unity断言记录该场景结果，失败可中止当前用例的剩余语句。
 *
 * ## Constraints and lifecycle
 * 不执行编译得到的identity函数；AST/推断类型/数组/projectionCompiler均只由当前测试管理，正常路径按尾部顺序释放。BUG: 建立资源后的Unity断言失败可跳过这些释放与state销毁。
 *
 * ## References
 * - tests/parser/test_artifact_schema.c:1251 — Unity callback registration; not direct C call (test_real_source_compile_and_binary_signature_import_are_identical)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:36 — physical definition or protocol span (test_real_source_compile_and_binary_signature_import_are_identical)
 */

/**
 * ## Summary
 * 给test_real_source_compile_and_binary_signature_import_are_identical提供固定内存源码，限定编译器应发布或不应发布的canonical身份/调用事实。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_real_source_compile_and_binary_signature_import_are_identical::source
 *
 * ## Value model
 * static const字符数组，含尾NUL；Parse长度按sizeof(source)-1。包含1段函数或lambda文本，仅解析/编译，当前C用例没有执行该源码函数、schedule或result。
 *
 * ## Parameters
 * 内嵌identity的输入形参value为int。
 *
 * ## Returns
 * 内嵌identity的函数体return value交回同一int值；本C路径只编译其声明/签名。
 *
 * ## Constraints and lifecycle
 * 数组为进程期只读存储，调用中借用给Parse；诊断sourceName不代表磁盘输入。VM、AST及compiled function各有独立生命期，字符串常量不负责其清理。
 *
 * ## References
 * - tests/parser/test_artifact_schema_source_roundtrip.c:36 — enclosing scene/helper (test_real_source_compile_and_binary_signature_import_are_identical)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:60 — scoped initializer consumer (source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:37 — physical definition or protocol span (test_real_source_compile_and_binary_signature_import_are_identical::source)
 */

/**
 * @fn static SZrState *create_scheduler_artifact_test_state(void)
 * ## Summary
 * 为源码scheduler场景创建独立测试VM，登记Parser与task/thread provider以便编译器解析真实调用合同。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::create_scheduler_artifact_test_state
 *
 * ## Returns
 * @return 正常成功返回调用者持有的state，须用ZrTests_Runtime_State_Destroy回收；state/global缺失或注册失败分支按上述规则返回NULL或原state。
 *
 * ## Constraints and lifecycle
 * state或global缺失时按当前代码直接返回state；只有具global的普通路径才继续注册。task或thread注册返回FALSE时销毁state并返回NULL；不承诺非局部异常路径自动清理，不创建工作线程来运行源码。
 *
 * ## References
 * - tests/parser/test_artifact_schema_source_roundtrip.c:203 — direct call expression (create_scheduler_artifact_test_state)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:243 — direct call expression (create_scheduler_artifact_test_state)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:267 — direct call expression (create_scheduler_artifact_test_state)
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:108 — writer fact rejection precedes file I/O (ZrParser_Writer_WriteSchedulerArtifactFile)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:150 — physical definition or protocol span (create_scheduler_artifact_test_state)
 */

/**
 * @fn static SZrFunction *compile_scheduler_artifact_source(SZrState *state, const TZrChar *source, TZrSize sourceLength, const TZrChar *sourceNameText)
 * ## Summary
 * 为当前内存源码建立诊断sourceName，Parse后Compile，并在编译普通返回后先释放AST，再把function交回测试调用者。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::compile_scheduler_artifact_source
 *
 * ## Parameters
 * @param[in,out] state 供字符串、AST及编译结果所属的当前VM；不得在调用中销毁。
 * @param[in] source 借用的内存源码。
 * @param[in] sourceLength source中送入Parse的字节数，不含调用者尾NUL。
 * @param[in] sourceNameText 诊断源名NUL字符串，调用期借用。
 *
 * ## Returns
 * @return NULL表示当前显式参数/字符串/Parse拒绝或Compiler返回NULL；成功function归当前VM，调用者在使用fact等借用后按现有Free/State_Destroy顺序回收。
 *
 * ## Constraints and lifecycle
 * sourceNameText必须NUL结尾，source至少sourceLength可读字节，state须为已初始化VM。sourceName只是诊断身份，不按该名称打开源文件；编译可能改变VM元数据，AST释放只描述普通返回，不保证Throw/fatal/Unity跳转自动清理。
 *
 * ## References
 * - tests/parser/test_artifact_schema_source_roundtrip.c:208 — direct call expression (compile_scheduler_artifact_source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:247 — direct call expression (compile_scheduler_artifact_source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:271 — direct call expression (compile_scheduler_artifact_source)
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:108 — writer fact rejection precedes file I/O (ZrParser_Writer_WriteSchedulerArtifactFile)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:166 — physical definition or protocol span (compile_scheduler_artifact_source)
 */

/**
 * @fn void test_real_source_scheduler_call_publishes_canonical_source_fact(void)
 * ## Summary
 * 编译含一次ThreadScheduler.schedule<int>的固定源字符串，检查唯一canonical source fact的类型、token、签名、role和protocol位。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_real_source_scheduler_call_publishes_canonical_source_fact
 *
 * ## Returns
 * 无返回值；由Unity断言记录该场景结果，失败可中止当前用例的剩余语句。
 *
 * ## Constraints and lifecycle
 * 只编译并读取fact，不执行Job lambda、schedule或result；fact借用function，所有检查均在Function_Free之前。BUG: 断言失败可跳过function/state清理。
 *
 * ## References
 * - tests/parser/test_artifact_schema.c:1252 — Unity callback registration; not direct C call (test_real_source_scheduler_call_publishes_canonical_source_fact)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:150 — paired helper definition (create_scheduler_artifact_test_state)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:166 — paired helper definition (compile_scheduler_artifact_source)
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:108 — writer fact rejection precedes file I/O (ZrParser_Writer_WriteSchedulerArtifactFile)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:195 — physical definition or protocol span (test_real_source_scheduler_call_publishes_canonical_source_fact)
 */

/**
 * ## Summary
 * 给test_real_source_scheduler_call_publishes_canonical_source_fact提供固定内存源码，限定编译器应发布或不应发布的canonical身份/调用事实。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_real_source_scheduler_call_publishes_canonical_source_fact::source
 *
 * ## Value model
 * static const字符数组，含尾NUL；Parse长度按sizeof(source)-1。包含1段函数或lambda文本，仅解析/编译，当前C用例没有执行该源码函数、schedule或result。
 *
 * ## Returns
 * 内嵌Job lambda无显式形参，若执行则只return 7；构造/调用相关语法由编译器读取，当前lambda体本身未运行。
 *
 * ## Constraints and lifecycle
 * 数组为进程期只读存储，调用中借用给Parse；诊断sourceName不代表磁盘输入。VM、AST及compiled function各有独立生命期，字符串常量不负责其清理。
 *
 * ## References
 * - tests/parser/test_artifact_schema_source_roundtrip.c:195 — enclosing scene/helper (test_real_source_scheduler_call_publishes_canonical_source_fact)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:210 — scoped initializer consumer (source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:211 — scoped initializer consumer (source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:196 — physical definition or protocol span (test_real_source_scheduler_call_publishes_canonical_source_fact::source)
 */

/**
 * @fn void test_repeated_scheduler_calls_coalesce_canonical_source_fact(void)
 * ## Summary
 * 编译同一scheduler上的两个int Job调用，要求共享一个canonical source fact。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_repeated_scheduler_calls_coalesce_canonical_source_fact
 *
 * ## Returns
 * 无返回值；由Unity断言记录该场景结果，失败可中止当前用例的剩余语句。
 *
 * ## Constraints and lifecycle
 * 只约束该固定源与相同scheduler合同的去重，不代表任意provider/泛型实例均应合并。BUG: 编译后断言失败越过局部function/state释放。
 *
 * ## References
 * - tests/parser/test_artifact_schema.c:1253 — Unity callback registration; not direct C call (test_repeated_scheduler_calls_coalesce_canonical_source_fact)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:150 — paired helper definition (create_scheduler_artifact_test_state)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:166 — paired helper definition (compile_scheduler_artifact_source)
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:108 — writer fact rejection precedes file I/O (ZrParser_Writer_WriteSchedulerArtifactFile)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:233 — physical definition or protocol span (test_repeated_scheduler_calls_coalesce_canonical_source_fact)
 */

/**
 * ## Summary
 * 给test_repeated_scheduler_calls_coalesce_canonical_source_fact提供固定内存源码，限定编译器应发布或不应发布的canonical身份/调用事实。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_repeated_scheduler_calls_coalesce_canonical_source_fact::source
 *
 * ## Value model
 * static const字符数组，含尾NUL；Parse长度按sizeof(source)-1。包含2段函数或lambda文本，仅解析/编译，当前C用例没有执行该源码函数、schedule或result。
 *
 * ## Returns
 * 内嵌第一、第二个Job lambda均无显式形参，若执行则分别return 7与return 8；构造/调用相关语法由编译器读取，当前lambda体本身未运行。
 *
 * ## Constraints and lifecycle
 * 数组为进程期只读存储，调用中借用给Parse；诊断sourceName不代表磁盘输入。VM、AST及compiled function各有独立生命期，字符串常量不负责其清理。
 *
 * ## References
 * - tests/parser/test_artifact_schema_source_roundtrip.c:233 — enclosing scene/helper (test_repeated_scheduler_calls_coalesce_canonical_source_fact)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:249 — scoped initializer consumer (source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:250 — scoped initializer consumer (source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:234 — physical definition or protocol span (test_repeated_scheduler_calls_coalesce_canonical_source_fact::source)
 */

/**
 * @fn void test_source_without_scheduler_call_publishes_no_scheduler_fact(void)
 * ## Summary
 * 编译只构造ThreadScheduler而未调用schedule的源字符串，要求fact长度为0且Find返回NULL。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_source_without_scheduler_call_publishes_no_scheduler_fact
 *
 * ## Returns
 * 无返回值；由Unity断言记录该场景结果，失败可中止当前用例的剩余语句。
 *
 * ## Constraints and lifecycle
 * 源文本的return scheduler未被执行；类型出现不应凭空产生调用事实。BUG: 断言失败可跳过function/state销毁。
 *
 * ## References
 * - tests/parser/test_artifact_schema.c:1254 — Unity callback registration; not direct C call (test_source_without_scheduler_call_publishes_no_scheduler_fact)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:150 — paired helper definition (create_scheduler_artifact_test_state)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:166 — paired helper definition (compile_scheduler_artifact_source)
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:108 — writer fact rejection precedes file I/O (ZrParser_Writer_WriteSchedulerArtifactFile)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:262 — physical definition or protocol span (test_source_without_scheduler_call_publishes_no_scheduler_fact)
 */

/**
 * ## Summary
 * 给test_source_without_scheduler_call_publishes_no_scheduler_fact提供固定内存源码，限定编译器应发布或不应发布的canonical身份/调用事实。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_source_without_scheduler_call_publishes_no_scheduler_fact::source
 *
 * ## Value model
 * static const字符数组，含尾NUL；Parse长度按sizeof(source)-1。包含0段函数或lambda文本，仅解析/编译，当前C用例没有执行该源码函数、schedule或result。
 *
 * ## Constraints and lifecycle
 * 数组为进程期只读存储，调用中借用给Parse；诊断sourceName不代表磁盘输入。VM、AST及compiled function各有独立生命期，字符串常量不负责其清理。
 *
 * ## References
 * - tests/parser/test_artifact_schema_source_roundtrip.c:262 — enclosing scene/helper (test_source_without_scheduler_call_publishes_no_scheduler_fact)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:273 — scoped initializer consumer (source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:274 — scoped initializer consumer (source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:263 — physical definition or protocol span (test_source_without_scheduler_call_publishes_no_scheduler_fact::source)
 */

/**
 * @fn void test_real_source_scheduler_call_writes_and_imports_canonical_artifact(void)
 * ## Summary
 * 编译实际scheduler源事实，写固定相对ZRO文件再读取到栈缓冲，以canonical consumer核查scheduler/domain-transfer合同及若干精确失配。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_real_source_scheduler_call_writes_and_imports_canonical_artifact
 *
 * ## Returns
 * 无返回值；由Unity断言记录该场景结果，失败可中止当前用例的剩余语句。
 *
 * ## Constraints and lifecycle
 * scheduler_source_contract.zro属于进程cwd；调用方须提供独占可写目录，禁止与其他执行并发复用该文件。普通路径读后fclose/remove再读内存；BUG: ferror断言失败可跳过关文件/删除，其他断言失败也可能跳过function/state清理。fact只在Function_Free前借用。
 *
 * ## References
 * - tests/parser/test_artifact_schema.c:1255 — Unity callback registration; not direct C call (test_real_source_scheduler_call_writes_and_imports_canonical_artifact)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:150 — paired helper definition (create_scheduler_artifact_test_state)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:166 — paired helper definition (compile_scheduler_artifact_source)
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:108 — writer fact rejection precedes file I/O (ZrParser_Writer_WriteSchedulerArtifactFile)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:286 — physical definition or protocol span (test_real_source_scheduler_call_writes_and_imports_canonical_artifact)
 */

/**
 * ## Summary
 * 给test_real_source_scheduler_call_writes_and_imports_canonical_artifact提供固定内存源码，限定编译器应发布或不应发布的canonical身份/调用事实。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_real_source_scheduler_call_writes_and_imports_canonical_artifact::source
 *
 * ## Value model
 * static const字符数组，含尾NUL；Parse长度按sizeof(source)-1。包含1段函数或lambda文本，仅解析/编译，当前C用例没有执行该源码函数、schedule或result。
 *
 * ## Returns
 * 内嵌Job lambda无显式形参，若执行则只return 7；构造/调用相关语法由编译器读取，当前lambda体本身未运行。
 *
 * ## Constraints and lifecycle
 * 数组为进程期只读存储，调用中借用给Parse；诊断sourceName不代表磁盘输入。VM、AST及compiled function各有独立生命期，字符串常量不负责其清理。
 *
 * ## References
 * - tests/parser/test_artifact_schema_source_roundtrip.c:286 — enclosing scene/helper (test_real_source_scheduler_call_writes_and_imports_canonical_artifact)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:312 — scoped initializer consumer (source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:313 — scoped initializer consumer (source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:287 — physical definition or protocol span (test_real_source_scheduler_call_writes_and_imports_canonical_artifact::source)
 */

/**
 * ## Summary
 * 为scheduler产物场景固定cwd内的输出文件名，使写入、读取和普通删除访问同一目标。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_real_source_scheduler_call_writes_and_imports_canonical_artifact::artifactPath
 *
 * ## Value model
 * static const NUL字符串 scheduler_source_contract.zro；相对进程cwd，不是全局绝对夹具目录。
 *
 * ## Constraints and lifecycle
 * 运行者须给本进程独占可写cwd；不与其它该测试并发复用文件。普通读取后fclose/remove，Unity跳转可跳过清理；数组不拥有FILE句柄。
 *
 * ## References
 * - tests/parser/test_artifact_schema_source_roundtrip.c:286 — enclosing scene/helper (test_real_source_scheduler_call_writes_and_imports_canonical_artifact)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:340 — scoped initializer consumer (artifactPath)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:344 — scoped initializer consumer (artifactPath)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:294 — physical definition or protocol span (test_real_source_scheduler_call_writes_and_imports_canonical_artifact::artifactPath)
 */

/**
 * ## Summary
 * 把磁盘产物读入栈缓冲后，在普通路径关闭并删除文件，再进行内存合同消费。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_real_source_scheduler_call_writes_and_imports_canonical_artifact::file_read_cleanup
 *
 * ## Constraints and lifecycle
 * BUG: 文件打开成功后ferror断言位于fclose/remove前；非零触发Unity跳转可遗留FILE与产物。fread长度是后续内存读取边界，空tearDown无法回收本地句柄。
 *
 * ## References
 * - tests/parser/test_artifact_schema_source_roundtrip.c:286 — executed within enclosing function (test_real_source_scheduler_call_writes_and_imports_canonical_artifact)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:344 — physical definition or protocol span (test_real_source_scheduler_call_writes_and_imports_canonical_artifact::file_read_cleanup)
 */

/**
 * @fn void test_source_without_scheduler_call_rejects_artifact_write(void)
 * ## Summary
 * 编译无schedule事实的源字符串，要求writer在缺失事实时返回INVALID_ARGUMENT。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_source_without_scheduler_call_rejects_artifact_write
 *
 * ## Returns
 * 无返回值；由Unity断言记录该场景结果，失败可中止当前用例的剩余语句。
 *
 * ## Constraints and lifecycle
 * 当前writer在事实检查失败处尚未到fopen；测试只断言状态，没有额外文件不存在断言。BUG: 断言失败可跳过局部function/state释放。
 *
 * ## References
 * - tests/parser/test_artifact_schema.c:1256 — Unity callback registration; not direct C call (test_source_without_scheduler_call_rejects_artifact_write)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:150 — paired helper definition (create_scheduler_artifact_test_state)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:166 — paired helper definition (compile_scheduler_artifact_source)
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:108 — writer fact rejection precedes file I/O (ZrParser_Writer_WriteSchedulerArtifactFile)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:439 — physical definition or protocol span (test_source_without_scheduler_call_rejects_artifact_write)
 */

/**
 * ## Summary
 * 给test_source_without_scheduler_call_rejects_artifact_write提供固定内存源码，限定编译器应发布或不应发布的canonical身份/调用事实。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_source_without_scheduler_call_rejects_artifact_write::source
 *
 * ## Value model
 * static const字符数组，含尾NUL；Parse长度按sizeof(source)-1。包含0段函数或lambda文本，仅解析/编译，当前C用例没有执行该源码函数、schedule或result。
 *
 * ## Constraints and lifecycle
 * 数组为进程期只读存储，调用中借用给Parse；诊断sourceName不代表磁盘输入。VM、AST及compiled function各有独立生命期，字符串常量不负责其清理。
 *
 * ## References
 * - tests/parser/test_artifact_schema_source_roundtrip.c:439 — enclosing scene/helper (test_source_without_scheduler_call_rejects_artifact_write)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:451 — scoped initializer consumer (source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:452 — scoped initializer consumer (source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:440 — physical definition or protocol span (test_source_without_scheduler_call_rejects_artifact_write::source)
 */

/**
 * @fn void test_scheduler_artifact_writer_rejects_unavailable_provider(void)
 * ## Summary
 * 编译合法scheduler调用后仅将jobProvider.moduleSignatureHash清零，要求writer拒绝不完整provider身份。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_scheduler_artifact_writer_rejects_unavailable_provider
 *
 * ## Returns
 * 无返回值；由Unity断言记录该场景结果，失败可中止当前用例的剩余语句。
 *
 * ## Constraints and lifecycle
 * 这是修改后的数据夹具，不能推广为任意provider错误或文件失败；当前拒绝先于fopen。BUG: 断言失败可跳过function/state清理。
 *
 * ## References
 * - tests/parser/test_artifact_schema.c:1257 — Unity callback registration; not direct C call (test_scheduler_artifact_writer_rejects_unavailable_provider)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:150 — paired helper definition (create_scheduler_artifact_test_state)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:166 — paired helper definition (compile_scheduler_artifact_source)
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:108 — writer fact rejection precedes file I/O (ZrParser_Writer_WriteSchedulerArtifactFile)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:471 — physical definition or protocol span (test_scheduler_artifact_writer_rejects_unavailable_provider)
 */

/**
 * ## Summary
 * 给test_scheduler_artifact_writer_rejects_unavailable_provider提供固定内存源码，限定编译器应发布或不应发布的canonical身份/调用事实。
 *
 * ## Unit
 * test_artifact_schema_source_roundtrip.c::test_scheduler_artifact_writer_rejects_unavailable_provider::source
 *
 * ## Value model
 * static const字符数组，含尾NUL；Parse长度按sizeof(source)-1。包含1段函数或lambda文本，仅解析/编译，当前C用例没有执行该源码函数、schedule或result。
 *
 * ## Returns
 * 内嵌Job lambda无显式形参，若执行则只return 7；构造/调用相关语法由编译器读取，当前lambda体本身未运行。
 *
 * ## Constraints and lifecycle
 * 数组为进程期只读存储，调用中借用给Parse；诊断sourceName不代表磁盘输入。VM、AST及compiled function各有独立生命期，字符串常量不负责其清理。
 *
 * ## References
 * - tests/parser/test_artifact_schema_source_roundtrip.c:471 — enclosing scene/helper (test_scheduler_artifact_writer_rejects_unavailable_provider)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:486 — scoped initializer consumer (source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:487 — scoped initializer consumer (source)
 * - tests/parser/test_artifact_schema_source_roundtrip.c:472 — physical definition or protocol span (test_scheduler_artifact_writer_rejects_unavailable_provider::source)
 */
