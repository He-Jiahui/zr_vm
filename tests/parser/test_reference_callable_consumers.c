#include "unity.h"

#include <string.h>

#include "backend_aot_canonical_artifact.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/module.h"
#include "zr_vm_parser/artifact_projection.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/syntax_contract.h"

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
