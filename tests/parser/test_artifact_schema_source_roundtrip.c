#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_parser/artifact_projection.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"

#define SOURCE_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 21u)
#define SOURCE_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 21u)
#define SOURCE_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 21u)

void test_real_source_compile_and_binary_signature_import_are_identical(void) {
    static const TZrChar source[] = "identity(value: int): int { return value; }";
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
