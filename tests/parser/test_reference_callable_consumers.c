#include "unity.h"

#include <string.h>

#include "backend_aot/backend_aot_canonical_artifact.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/module.h"
#include "artifact/artifact_projection.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "contract/syntax_contract.h"

#define REFERENCE_CALLABLE_TYPE_DEF_TOKEN \
    ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 52u)
#define REFERENCE_CALLABLE_TYPE_REF_TOKEN \
    ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 52u)
#define REFERENCE_CALLABLE_TYPE_SPEC_TOKEN \
    ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 52u)
#define REFERENCE_CALLABLE_SIGNATURE_TOKEN \
    ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 52u)
#define REFERENCE_CALLABLE_MEMBER_TOKEN \
    ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 52u)

#define REFERENCE_CALLABLE_LAYOUT_HASH ((TZrUInt64)0x3141592653589793ULL)
#define REFERENCE_CALLABLE_MODULE_HASH ((TZrUInt64)0x2718281828459045ULL)

void test_reference_callable_contract_roundtrips_across_artifact_vm_and_aot(void);
void test_reference_callable_ref_export_matches_return_access(void);

static TZrSize write_reference_callable_artifact(
        TZrTypeId typeId,
        const TZrByte *signature,
        TZrSize signatureLength,
        const SZrArtifactPublicIdentity *identity,
        TZrUInt32 contractFlags,
        EZrArtifactRefExportEffect refExportEffect,
        TZrByte *buffer,
        TZrSize bufferCapacity) {
    static const TZrByte code[] = {0x01u};
    SZrArtifactTypeDefRow typeDef;
    SZrArtifactTypeIdentityRow typeRef;
    SZrArtifactTypeIdentityRow typeSpec;
    SZrArtifactContractRow contract;
    SZrArtifactLayoutRow layout;
    SZrArtifactSectionInput sections[7];
    SZrArtifactDocument document;
    SZrArtifactDiagnostic diagnostic;
    TZrSize writtenSize = 0U;

    memset(&typeDef, 0, sizeof(typeDef));
    typeDef.token = REFERENCE_CALLABLE_TYPE_DEF_TOKEN;
    typeDef.flags = ZR_ARTIFACT_TYPE_FLAG_VALUE;
    typeDef.canonicalTypeId = typeId;
    typeDef.typeSignatureHash = identity->signatureHash;

    memset(&typeRef, 0, sizeof(typeRef));
    typeRef.token = REFERENCE_CALLABLE_TYPE_REF_TOKEN;
    typeRef.signatureToken = REFERENCE_CALLABLE_SIGNATURE_TOKEN;
    typeRef.canonicalTypeId = typeId;
    typeRef.signatureLength = (TZrUInt32)signatureLength;
    typeRef.signatureHash = identity->typeRefHash;
    typeRef.layoutVersion = identity->layoutVersion;
    typeRef.layoutHash = identity->layoutHash;
    typeSpec = typeRef;
    typeSpec.token = REFERENCE_CALLABLE_TYPE_SPEC_TOKEN;
    typeSpec.signatureHash = identity->typeSpecHash;

    memset(&contract, 0, sizeof(contract));
    contract.memberToken = REFERENCE_CALLABLE_MEMBER_TOKEN;
    contract.signatureToken = REFERENCE_CALLABLE_SIGNATURE_TOKEN;
    contract.parameterCount = 7U;
    contract.flags = contractFlags;
    contract.receiverEffect = ZR_ARTIFACT_RECEIVER_NONE;
    contract.refExportEffect = refExportEffect;
    contract.contractHash = identity->callableContractHash;

    memset(&layout, 0, sizeof(layout));
    layout.typeToken = REFERENCE_CALLABLE_TYPE_DEF_TOKEN;
    layout.version = identity->layoutVersion;
    layout.byteSize = (TZrUInt32)sizeof(void *);
    layout.byteAlignment = (TZrUInt32)sizeof(void *);
    layout.gcScanKind = ZR_ARTIFACT_GC_SCAN_FREE;
    layout.layoutHash = identity->layoutHash;

    sections[0] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1U,
            &typeDef};
    sections[1] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_REF_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1U,
            &typeRef};
    sections[2] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1U,
            &typeSpec};
    sections[3] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            (TZrUInt32)signatureLength,
            signature};
    sections[4] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1U,
            &contract};
    sections[5] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1U,
            &layout};
    sections[6] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_CODE_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            (TZrUInt32)sizeof(code),
            code};

    memset(&document, 0, sizeof(document));
    document.kind = ZR_ARTIFACT_KIND_ZRO;
    document.identity = *identity;
    document.sectionCount = 7U;
    document.sections = sections;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_Write(
                    &document,
                    buffer,
                    bufferCapacity,
                    &writtenSize,
                    &diagnostic));
    return writtenSize;
}

void test_reference_callable_contract_roundtrips_across_artifact_vm_and_aot(void) {
    static const TZrChar source[] =
            "fn contract(value: int, input: in int, writable: ref int, "
            "observed: ref readonly int, local: scoped ref int, "
            "localView: scoped ref readonly int, result: out int): "
            "ref readonly int { result = value; return observed; }";
    static const TZrChar expectedCanonical[] =
            "fn(int, in int, ref int, ref readonly int, scoped ref int, "
            "scoped ref readonly int, out int) -> ref readonly int";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *script;
    SZrFunction *compiled;
    SZrFunctionDeclaration *declaration;
    SZrSemanticContext *context;
    TZrTypeId intTypeId;
    TZrTypeId returnTypeId;
    TZrTypeId sourceTypeId;
    TZrTypeId importedTypeId = ZR_SEMANTIC_ID_INVALID;
    TZrTypeId parameterTypeIds[7];
    TZrChar sourceCanonical[256];
    TZrChar importedCanonical[256];
    TZrByte signature[512];
    TZrSize signatureLength = 0U;
    SZrParserArtifactPublicContract publicContract;
    SZrArtifactPublicIdentity identity;
    SZrArtifactDiagnostic diagnostic;
    TZrByte artifact[4096];
    TZrSize artifactLength;
    SZrCanonicalConsumerProjection vmProjection;
    SZrCanonicalConsumerProjection aotProjection;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state, "reference_callable_consumers.zr", 31U);
    TEST_ASSERT_NOT_NULL(sourceName);
    script = ZrParser_Parse(state, source, sizeof(source) - 1U, sourceName);
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(
            1U, (TZrUInt32)script->data.script.statements->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_FUNCTION_DECLARATION,
            script->data.script.statements->nodes[0]->type);
    compiled = ZrParser_Compiler_Compile(state, script);
    TEST_ASSERT_NOT_NULL_MESSAGE(compiled, "target reference source must compile");

    declaration =
            &script->data.script.statements->nodes[0]->data.functionDeclaration;
    TEST_ASSERT_NOT_NULL(declaration->params);
    TEST_ASSERT_EQUAL_UINT32(7U, (TZrUInt32)declaration->params->count);
    context = ZrParser_SemanticContext_New(state);
    TEST_ASSERT_NOT_NULL(context);
    intTypeId = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    returnTypeId = ZrParser_CanonicalType_InternRef(
            context, intTypeId, ZR_CANONICAL_REF_READONLY);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, intTypeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, returnTypeId);
    for (index = 0U; index < 7U; index++) {
        parameterTypeIds[index] = intTypeId;
    }
    sourceTypeId = ZrParser_SyntaxCallable_Intern(
            context,
            declaration->params,
            parameterTypeIds,
            returnTypeId,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, sourceTypeId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            context, sourceTypeId, sourceCanonical, sizeof(sourceCanonical)));
    TEST_ASSERT_EQUAL_STRING(expectedCanonical, sourceCanonical);

    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_WriteSignature(
                    context,
                    sourceTypeId,
                    signature,
                    sizeof(signature),
                    &signatureLength,
                    &diagnostic));
    TEST_ASSERT_GREATER_THAN_UINT32(2U, (TZrUInt32)signatureLength);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_SIGNATURE_NODE_FUNCTION, signature[0]);
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_REF_EXPORT_READONLY, signature[2]);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_InternSignature(
                    context,
                    ZR_NULL,
                    signature,
                    signatureLength,
                    &importedTypeId,
                    &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(sourceTypeId, importedTypeId);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            context, importedTypeId, importedCanonical, sizeof(importedCanonical)));
    TEST_ASSERT_EQUAL_STRING(sourceCanonical, importedCanonical);

    memset(&publicContract, 0, sizeof(publicContract));
    publicContract.typeRefToken = REFERENCE_CALLABLE_TYPE_REF_TOKEN;
    publicContract.typeSpecToken = REFERENCE_CALLABLE_TYPE_SPEC_TOKEN;
    publicContract.signatureToken = REFERENCE_CALLABLE_SIGNATURE_TOKEN;
    publicContract.layoutVersion = 1U;
    publicContract.layoutHash = REFERENCE_CALLABLE_LAYOUT_HASH;
    publicContract.callableContractHash =
            ZrCore_Artifact_HashBytes(signature, signatureLength);
    publicContract.moduleHash = REFERENCE_CALLABLE_MODULE_HASH;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_BuildPublicIdentity(
                    context,
                    sourceTypeId,
                    &publicContract,
                    signature,
                    sizeof(signature),
                    &signatureLength,
                    &identity,
                    &diagnostic));
    artifactLength = write_reference_callable_artifact(
            sourceTypeId,
            signature,
            signatureLength,
            &identity,
            ZR_ARTIFACT_CONTRACT_FLAG_SCOPED,
            ZR_ARTIFACT_REF_EXPORT_READONLY,
            artifact,
            sizeof(artifact));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Module_OpenCanonicalArtifact(
                    artifact,
                    artifactLength,
                    &identity,
                    &vmProjection,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            backend_aot_open_canonical_artifact(
                    artifact,
                    artifactLength,
                    &identity,
                    &aotProjection,
                    &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(
            (TZrUInt32)signatureLength, vmProjection.rootType.signatureLength);
    TEST_ASSERT_EQUAL_MEMORY(
            signature, vmProjection.rootType.signatureData, signatureLength);
    TEST_ASSERT_EQUAL_MEMORY(
            &vmProjection.rootType,
            &aotProjection.rootType,
            sizeof(vmProjection.rootType));
    TEST_ASSERT_TRUE(vmProjection.rootType.hasContract);
    TEST_ASSERT_EQUAL_UINT32(7U, vmProjection.rootType.contract.parameterCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_REF_EXPORT_READONLY,
            vmProjection.rootType.contract.refExportEffect);
    TEST_ASSERT_BITS_HIGH(
            ZR_ARTIFACT_CONTRACT_FLAG_SCOPED,
            vmProjection.rootType.contract.flags);

    artifactLength = write_reference_callable_artifact(
            sourceTypeId,
            signature,
            signatureLength,
            &identity,
            ZR_ARTIFACT_CONTRACT_FLAG_SCOPED,
            ZR_ARTIFACT_REF_EXPORT_WRITABLE,
            artifact,
            sizeof(artifact));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
            ZrCore_Module_OpenCanonicalArtifact(
                    artifact,
                    artifactLength,
                    &identity,
                    &vmProjection,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
            backend_aot_open_canonical_artifact(
                    artifact,
                    artifactLength,
                    &identity,
                    &aotProjection,
                    &diagnostic));
    artifactLength = write_reference_callable_artifact(
            sourceTypeId,
            signature,
            signatureLength,
            &identity,
            0U,
            ZR_ARTIFACT_REF_EXPORT_READONLY,
            artifact,
            sizeof(artifact));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
            ZrCore_Module_OpenCanonicalArtifact(
                    artifact,
                    artifactLength,
                    &identity,
                    &vmProjection,
                    &diagnostic));

    ZrParser_SemanticContext_Free(context);
    ZrCore_Function_Free(state, compiled);
    ZrParser_Ast_Free(state, script);
    ZrTests_Runtime_State_Destroy(state);
}

void test_reference_callable_ref_export_matches_return_access(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrSemanticContext *context;
    TZrTypeId intTypeId;
    TZrTypeId writableRefTypeId;
    TZrTypeId readonlyRefTypeId;
    TZrTypeId writableCallableTypeId;
    TZrTypeId readonlyCallableTypeId;
    TZrTypeId importedTypeId = ZR_SEMANTIC_ID_INVALID;
    TZrByte signature[64];
    TZrSize signatureLength = 0U;
    SZrArtifactDiagnostic diagnostic;

    TEST_ASSERT_NOT_NULL(state);
    context = ZrParser_SemanticContext_New(state);
    TEST_ASSERT_NOT_NULL(context);
    intTypeId = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    writableRefTypeId = ZrParser_CanonicalType_InternRef(
            context, intTypeId, ZR_CANONICAL_REF_WRITABLE);
    readonlyRefTypeId = ZrParser_CanonicalType_InternRef(
            context, intTypeId, ZR_CANONICAL_REF_READONLY);
    writableCallableTypeId = ZrParser_CanonicalType_InternFunction(
            context,
            ZR_NULL,
            0U,
            writableRefTypeId,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    readonlyCallableTypeId = ZrParser_CanonicalType_InternFunction(
            context,
            ZR_NULL,
            0U,
            readonlyRefTypeId,
            ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_NONE);
    TEST_ASSERT_NOT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID, writableCallableTypeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID, readonlyCallableTypeId);

    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_WriteSignature(
                    context,
                    writableCallableTypeId,
                    signature,
                    sizeof(signature),
                    &signatureLength,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_REF_EXPORT_WRITABLE, signature[2]);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_InternSignature(
                    context,
                    ZR_NULL,
                    signature,
                    signatureLength,
                    &importedTypeId,
                    &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(writableCallableTypeId, importedTypeId);

    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrParser_ArtifactType_WriteSignature(
                    context,
                    readonlyCallableTypeId,
                    signature,
                    sizeof(signature),
                    &signatureLength,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_ARTIFACT_REF_EXPORT_READONLY, signature[2]);
    signature[2] = (TZrByte)ZR_ARTIFACT_REF_EXPORT_WRITABLE;
    importedTypeId = 99U;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
            ZrParser_ArtifactType_InternSignature(
                    context,
                    ZR_NULL,
                    signature,
                    signatureLength,
                    &importedTypeId,
                    &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, importedTypeId);

    ZrParser_SemanticContext_Free(context);
    ZrTests_Runtime_State_Destroy(state);
}

/**
 * @unit CCONS-063 declaration::test_reference_callable_contract_roundtrips_across_artifact_vm_and_aot
 * @brief 固定跨TU Unity 用例原型：验证七种引用参数合同在 parser、artifact、VM 与 AOT 间一致
 *
 * ## Summary
 * 在 reference TU 中固定随后定义的外部函数原型；main TU 另有相同原型。
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
 * tests/parser/test_reference_callable_consumers.c:32
 * tests/parser/test_reference_callable_consumers.c:142
 */

/**
 * @unit CCONS-064 declaration::test_reference_callable_ref_export_matches_return_access
 * @brief 固定跨TU Unity 用例原型：验证引用返回访问模式与签名导出字节一致
 *
 * ## Summary
 * 在 reference TU 中固定随后定义的外部函数原型；main TU 另有相同原型。
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
 * tests/parser/test_reference_callable_consumers.c:33
 * tests/parser/test_reference_callable_consumers.c:354
 */

/**
 * @unit CCONS-065 function::write_reference_callable_artifact
 * @brief 同步构建七节引用 callable artifact 的内存编码
 *
 * ## Summary
 * reference roundtrip 用例用同一 signature/identity 改变 contract flags/refExportEffect 比较两个消费者。
 *
 * ## Parameters
 * @param[in] typeId 已 intern 的 canonical callable 类型ID。
 * @param[in] signature 非NULL签名字节，借用至少 signatureLength 字节。
 * @param[in] signatureLength 签名长度；写入节长度和32位行字段前由调用者保证可表示。
 * @param[in] identity 非NULL public identity，提供签名、layout及callable contract哈希。
 * @param[in] contractFlags 写入 contract.flags 的测试输入，可用于构造负例。
 * @param[in] refExportEffect 写入 contract.refExportEffect 的读写导出效果。
 * @param[out] buffer 至少 bufferCapacity 字节的调用者写缓冲。
 * @param[in] bufferCapacity buffer 容量，不足时 writer 的非OK状态触发 Unity 中断。
 *
 * ## Returns
 * @return 成功断言后返回 writtenSize；失败不返回普通错误值。
 *
 * ## Constraints and lifecycle
 * 固定 parameterCount=7、receiver NONE、layout size/alignment=sizeof(void*)、GC_FREE；这些是本 helper 的合成fixture合同，不是任意签名的通用序列化器。
 * 七节和行是栈对象、code 为静态数组，ZrCore_Artifact_Write 同步消费；仅字节写入 caller buffer，不保存这些栈指针，不进行文件I/O。
 *
 * ## References
 * tests/parser/test_reference_callable_consumers.c:35
 */

/**
 * @unit CCONS-067 function::test_reference_callable_contract_roundtrips_across_artifact_vm_and_aot
 * @brief 验证七种引用参数合同在 parser、artifact、VM 与 AOT 间一致
 *
 * ## Summary
 * 由 main 的 RUN_TEST 经跨TU声明调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；预期条件不符时 Unity 中断当前用例。
 *
 * ## Constraints and lifecycle
 * Parse/Compile 唯一 contract 语言体并单独 intern callable；检查七参数签名、readonly ref 导出、SCOPED合同和非法导出/flags拒绝，不执行语言体或artifact code。
 * 独立 local state/context/compiled/AST 只在普通尾路径依次释放，与 main g_state 不同。BUG: local state 已建立后 context 首次 RawMalloc 普通NULL会在失败断言处 longjmp，跳过本地 Destroy；默认 tearDown 只销毁 g_state，局部VM可留到进程结束。
 *
 * ## References
 * tests/parser/test_reference_callable_consumers.c:142
 * tests/parser/test_canonical_consumers.c:1732
 * zr_vm_parser/src/zr_vm_parser/semantic/semantic.c:195
 * tests/third_party/zr_unity/Unity/src/unity.c:2247
 * tests/parser/test_canonical_consumers.c:54
 */

/**
 * @unit CCONS-068 state::test_reference_callable_contract_roundtrips_across_artifact_vm_and_aot::source
 * @brief 保存该用例的人工Zr源码字符串
 *
 * ## Summary
 * 验证七种引用参数合同在 parser、artifact、VM 与 AOT 间一致时作为Parse/Compile输入。
 *
 * ## Fields
 * source自动指针或静态const字符数组；字面量具有静态存储期。
 *
 * ## Value model
 * Parse/Compile 唯一 contract 语言体并单独 intern callable；检查七参数签名、readonly ref 导出、SCOPED合同和非法导出/flags拒绝，不执行语言体或artifact code。
 *
 * ## Invariants and lifecycle
 * 由所属C用例 Parse/Compile；语言函数、构造器与lambda没有经过 Function_Execute。各独立语言定义另列unit，字符串字节不能被文档改动。
 * 源码名是VM诊断字符串，不是磁盘文件路径；AST/context只在各自有效期内查询，普通尾路径释放，不承诺Unity中断后的局部资源恢复。
 *
 * ## References
 * tests/parser/test_reference_callable_consumers.c:143
 */

/**
 * @unit CCONS-070 function::test_reference_callable_ref_export_matches_return_access
 * @brief 验证引用返回访问模式与签名导出字节一致
 *
 * ## Summary
 * 由 main 的第二个 reference RUN_TEST 调用。
 *
 * ## Parameters
 * 无参数。
 *
 * ## Returns
 * 无返回值；断言失败由 Unity 中断。
 *
 * ## Constraints and lifecycle
 * 分别 intern writable/readonly ref 返回 callable，核 signature[2]；将 readonly 签名该字节改为 writable 后期望 INVALID_SIGNATURE 且 out id 重设为 INVALID。
 * local context/state 只在普通尾路径释放。BUG: local state 已建立而 context 首次分配普通NULL时，Unity longjmp 跳过本地 Destroy；默认 hook 不回收这个独立VM，影响限于当前测试进程余下生命周期。
 *
 * ## References
 * tests/parser/test_reference_callable_consumers.c:354
 * tests/parser/test_canonical_consumers.c:1733
 * zr_vm_parser/src/zr_vm_parser/semantic/semantic.c:195
 * tests/third_party/zr_unity/Unity/src/unity.c:2247
 * tests/parser/test_canonical_consumers.c:54
 */

/**
 * @unit CCONS-123 fixture::test_reference_callable_contract_roundtrips_across_artifact_vm_and_aot::module::contract
 * @brief 提供七种参数模式与readonly引用返回的语言函数体
 *
 * ## Summary
 * reference roundtrip C用例仅Parse/Compile并从参数AST合成canonical合同。
 *
 * ## Parameters
 * @param[in] value 普通 int 值，写入 result。
 * @param[in] input in int 接口形参，本体未使用。
 * @param[in] writable 可写 ref int 接口形参，本体未读写；仅保留签名访问模式。
 * @param[in] observed ref readonly int 借用，作为 readonly ref 返回值。
 * @param[in] local scoped ref int 接口形参，本体未使用，不保存到外部。
 * @param[in] localView scoped ref readonly int 接口形参，本体未使用。
 * @param[out] result out int 引用，语言体赋值为 value。
 *
 * ## Returns
 * @return 语言体先把 value 写到 result，再以只读引用返回 observed；其他引用形参不读写。
 *
 * ## Constraints and lifecycle
 * 参数模式是接口合同；返回借用的有效期必须覆盖调用方使用，函数体不拥有被引用值，也不提供脱离该值的快照。
 * 当前C测试没有执行本体或验证运行时引用存活；它只断言解析、编译及序列化合同。
 *
 * ## References
 * tests/parser/test_reference_callable_consumers.c:144
 * tests/parser/test_reference_callable_consumers.c:142
 */
