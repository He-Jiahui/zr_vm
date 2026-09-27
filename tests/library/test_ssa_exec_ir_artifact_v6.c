#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/artifact_exec_ir.h"
#include "zr_vm_core/artifact_exec_ir_scalar.h"
#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_core/module.h"
#include "zr_vm_parser/artifact_exec_ir.h"
#include "zr_vm_common/zr_type_conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_TYPE_ID 17u
#define TEST_TYPE_DEF ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u)
#define TEST_TYPE_REF ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u)
#define TEST_TYPE_SPEC ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u)
#define TEST_SIGNATURE ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)
#define TEST_MEMBER ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u)
#define TEST_MODULE ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MODULE, 1u)
#define TEST_REF_HASH UINT64_C(0x1111222233334444)
#define TEST_SPEC_HASH UINT64_C(0x2222333344445555)
#define TEST_SIGNATURE_HASH UINT64_C(0x3333444455556666)
#define TEST_LAYOUT_HASH UINT64_C(0x4444555566667777)
#define TEST_CONTRACT_HASH UINT64_C(0x5555666677778888)
#define TEST_MODULE_HASH UINT64_C(0x6666777788889999)

#ifdef ZR_ARTIFACT_TEST_IO_FAILURE
void ZrParser_ExecIrArtifact_TestInjectWriteFailure(TZrBool enabled);
#endif

typedef struct SFixture {
    TZrByte signature[10];
    SZrArtifactTypeDefRow typeDef;
    SZrArtifactTypeIdentityRow typeRef;
    SZrArtifactTypeIdentityRow typeSpec;
    SZrArtifactContractRow contract;
    SZrArtifactLayoutRow layout;
    SZrArtifactSectionInput sections[7];
    SZrArtifactDocument metadata;
} SFixture;

static void require_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void put16(TZrByte *bytes, TZrUInt16 value) {
    bytes[0] = (TZrByte)value;
    bytes[1] = (TZrByte)(value >> 8u);
}

static void put32(TZrByte *bytes, TZrUInt32 value) {
    for (TZrUInt32 index = 0u; index < 4u; ++index)
        bytes[index] = (TZrByte)(value >> (8u * index));
}

static void put64(TZrByte *bytes, TZrUInt64 value) {
    for (TZrUInt32 index = 0u; index < 8u; ++index)
        bytes[index] = (TZrByte)(value >> (8u * index));
}

static void init_fixture(SFixture *fixture) {
    static const TZrByte strings[] = {'a', 'p', 'p', 0u};
    SZrArtifactPublicIdentity *identity;
    memset(fixture, 0, sizeof(*fixture));
    fixture->signature[0] = ZR_ARTIFACT_SIGNATURE_NODE_OWNER;
    fixture->signature[1] = ZR_ARTIFACT_OWNER_SHARED;
    fixture->signature[2] = ZR_ARTIFACT_SIGNATURE_NODE_READONLY_VIEW;
    fixture->signature[3] = ZR_ARTIFACT_SIGNATURE_NODE_REF;
    fixture->signature[4] = ZR_ARTIFACT_REF_READONLY;
    fixture->signature[5] = ZR_ARTIFACT_SIGNATURE_NODE_PRIMITIVE;
    put32(fixture->signature + 6u, 4u);

    fixture->typeDef.token = TEST_TYPE_DEF;
    fixture->typeDef.flags = ZR_ARTIFACT_TYPE_FLAG_VALUE |
                             ZR_ARTIFACT_TYPE_FLAG_VALUE_CONSTRUCTIBLE |
                             ZR_ARTIFACT_TYPE_FLAG_READONLY;
    fixture->typeDef.canonicalTypeId = TEST_TYPE_ID;
    fixture->typeDef.constructorToken = TEST_MEMBER;
    fixture->typeDef.constructorSignatureToken = TEST_SIGNATURE;
    fixture->typeDef.typeSignatureHash = TEST_SIGNATURE_HASH;
    fixture->typeDef.constructorContractHash = TEST_CONTRACT_HASH;

    fixture->typeRef.token = TEST_TYPE_REF;
    fixture->typeRef.signatureToken = TEST_SIGNATURE;
    fixture->typeRef.canonicalTypeId = TEST_TYPE_ID;
    fixture->typeRef.signatureLength = sizeof(fixture->signature);
    fixture->typeRef.signatureHash = TEST_REF_HASH;
    fixture->typeRef.layoutVersion = 7u;
    fixture->typeRef.layoutHash = TEST_LAYOUT_HASH;
    fixture->typeSpec = fixture->typeRef;
    fixture->typeSpec.token = TEST_TYPE_SPEC;
    fixture->typeSpec.signatureHash = TEST_SPEC_HASH;

    fixture->contract.memberToken = TEST_MEMBER;
    fixture->contract.signatureToken = TEST_SIGNATURE;
    fixture->contract.parameterCount = 0u;
    fixture->contract.receiverEffect = ZR_ARTIFACT_RECEIVER_NONE;
    fixture->contract.refExportEffect = ZR_ARTIFACT_REF_EXPORT_NONE;
    fixture->contract.contractHash = TEST_CONTRACT_HASH;
    fixture->layout.typeToken = TEST_TYPE_DEF;
    fixture->layout.version = 7u;
    fixture->layout.byteSize = 16u;
    fixture->layout.byteAlignment = 8u;
    fixture->layout.gcScanKind = ZR_ARTIFACT_GC_SCAN_MAPPED;
    fixture->layout.capabilityFlags = ZR_ARTIFACT_LAYOUT_CAPABILITY_STABLE_SLOT_SOURCE;
    fixture->layout.layoutHash = TEST_LAYOUT_HASH;
    fixture->layout.stableSlotContractHash = 0xabcdefu;

    fixture->sections[0] = (SZrArtifactSectionInput){
        ZR_ARTIFACT_SECTION_STRING_HEAP, 0u, sizeof(strings), strings};
    fixture->sections[1] = (SZrArtifactSectionInput){
        ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE, 0u, 1u, &fixture->typeDef};
    fixture->sections[2] = (SZrArtifactSectionInput){
        ZR_ARTIFACT_SECTION_TYPE_REF_TABLE, 0u, 1u, &fixture->typeRef};
    fixture->sections[3] = (SZrArtifactSectionInput){
        ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE, 0u, 1u, &fixture->typeSpec};
    fixture->sections[4] = (SZrArtifactSectionInput){
        ZR_ARTIFACT_SECTION_SIGNATURE_HEAP, 0u,
        sizeof(fixture->signature), fixture->signature};
    fixture->sections[5] = (SZrArtifactSectionInput){
        ZR_ARTIFACT_SECTION_CONTRACT_TABLE, 0u, 1u, &fixture->contract};
    fixture->sections[6] = (SZrArtifactSectionInput){
        ZR_ARTIFACT_SECTION_LAYOUT_TABLE, 0u, 1u, &fixture->layout};
    fixture->metadata.kind = ZR_ARTIFACT_KIND_ZRO;
    fixture->metadata.sectionCount = 7u;
    fixture->metadata.sections = fixture->sections;
    identity = &fixture->metadata.identity;
    identity->canonicalTypeId = TEST_TYPE_ID;
    identity->typeRefToken = TEST_TYPE_REF;
    identity->typeSpecToken = TEST_TYPE_SPEC;
    identity->signatureToken = TEST_SIGNATURE;
    identity->typeRefHash = TEST_REF_HASH;
    identity->typeSpecHash = TEST_SPEC_HASH;
    identity->signatureHash = TEST_SIGNATURE_HASH;
    identity->layoutVersion = 7u;
    identity->layoutHash = TEST_LAYOUT_HASH;
    identity->callableContractHash = TEST_CONTRACT_HASH;
    identity->moduleHash = TEST_MODULE_HASH;
}

static void build_graph(SZrExecIrModule *module) {
    SZrExecIrFunction *function;
    SZrExecIrBlock *block;
    SZrExecIrConstant constant = {ZR_VALUE_TYPE_INT64, 0u, 42u};
    SZrExecIrInstruction instruction;
    SZrExecIrRange constantRange, resultRange, returnRange;
    TZrExecIrFunctionId functionId;
    TZrExecIrValueId valueId;
    TZrExecIrInstructionId instructionId;
    SZrExecIrDiagnostic diagnostic;

    ZrCore_ExecIr_ModuleInit(module);
    module->id = 1u;
    module->moduleToken = TEST_MODULE;
    module->moduleHash = TEST_MODULE_HASH;
    module->contract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    module->contract.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    module->contract.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    module->contract.generation = 1u;
    module->contract.targetToken = TEST_MODULE;
    module->contract.signatureHash = TEST_SIGNATURE_HASH;
    module->contract.layoutHash = TEST_LAYOUT_HASH;
    module->contract.moduleHash = TEST_MODULE_HASH;
    require_true(ZrCore_ExecIr_ModuleAppendConstant(module, &constant, 1u,
                                                    &constantRange), "append constant");
    require_true(constantRange.start == 0u && constantRange.count == 1u,
                 "constant range");
    require_true(ZrCore_ExecIr_ModuleAddFunction(module, TEST_MEMBER,
                                                TEST_SIGNATURE_HASH, &functionId),
                 "add function");
    function = ZrCore_ExecIr_ModuleFunctionAt(module, functionId);
    require_true(function != ZR_NULL, "function lookup");
    function->contract.layoutHash = TEST_LAYOUT_HASH;
    function->contract.moduleHash = TEST_MODULE_HASH;
    require_true(ZrCore_ExecIr_FunctionAddBlock(function,
                                               ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
                 "add entry block");
    valueId = ZrCore_ExecIr_FunctionAddValue(function, ZR_VALUE_TYPE_INT64,
                                            ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                                            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    require_true(valueId == 1u, "add result value");
    require_true(ZrCore_ExecIr_FunctionAppendResults(function, &valueId, 1u,
                                                    &resultRange), "append result");
    require_true(ZrCore_ExecIr_FunctionAppendOperands(function, &valueId, 1u,
                                                     &returnRange), "append operand");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = resultRange;
    instruction.layoutId = constantRange.start;
    instruction.sourceId = 101u;
    require_true(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                         &instructionId) && instructionId == 1u,
                 "append CONSTANT");
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnRange;
    instruction.sourceId = 102u;
    require_true(ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                         &instructionId) && instructionId == 2u,
                 "append RETURN");
    block = ZrCore_ExecIr_FunctionBlockAt(function, 1u);
    block->instructions.start = 0u;
    block->instructions.count = 2u;
    block->terminatorInstructionId = 2u;
    require_true(ZrCore_ExecIr_VerifyModule(module, &diagnostic), "verify source graph");
}

static TZrByte *read_file(const char *path, TZrSize *outLength) {
    FILE *file = fopen(path, "rb");
    long length;
    TZrByte *bytes;
    require_true(file != NULL, "open generated artifact");
    require_true(fseek(file, 0, SEEK_END) == 0, "seek end");
    length = ftell(file);
    require_true(length > 0 && length < 65536, "bounded artifact length");
    require_true(fseek(file, 0, SEEK_SET) == 0, "rewind artifact");
    bytes = (TZrByte *)malloc((size_t)length);
    require_true(bytes != NULL, "allocate artifact buffer");
    require_true(fread(bytes, 1u, (size_t)length, file) == (size_t)length,
                 "read artifact bytes");
    require_true(fclose(file) == 0, "close artifact");
    *outLength = (TZrSize)length;
    return bytes;
}

static SZrArtifactDiagnostic require_rejected(const TZrByte *bytes,
                                              TZrSize length,
                                              EZrArtifactStatus expected) {
    SFixture fixture;
    SZrExecIrModule output;
    SZrArtifactDiagnostic diagnostic;
    EZrArtifactStatus actual;
    init_fixture(&fixture);
    ZrCore_ExecIr_ModuleInit(&output);
    actual = ZrCore_Module_OpenExecIrArtifact(bytes, length,
                                             &fixture.metadata.identity,
                                             &output, &diagnostic);
    require_true(actual == expected, "malformed artifact status");
    require_true(output.functionCount == 0u && output.functions == ZR_NULL &&
                 output.constantCount == 0u && output.constants == ZR_NULL,
                 "failed load published graph");
    return diagnostic;
}

static void run_oracle(const SZrExecIrModule *module) {
    SZrExecIrOracleValue constant;
    SZrExecIrOracleInput input;
    SZrExecIrOracleExecutionResult execution;
    SZrExecIrDiagnostic diagnostic;
    memset(&constant, 0, sizeof(constant));
    constant.kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    constant.as.signedInteger = (TZrInt64)module->constants[0].bits;
    memset(&input, 0, sizeof(input));
    input.function = &module->functions[0];
    input.constants = &constant;
    input.constantCount = module->constantCount;
    ZrCore_ExecIr_OracleResultInit(&execution);
    require_true(ZrCore_ExecIr_RunOracleEx(&input, &execution, &diagnostic),
                 "Oracle execution");
    require_true(execution.returned &&
                 execution.returnValue.kind == ZR_EXEC_IR_ORACLE_VALUE_SIGNED &&
                 execution.returnValue.as.signedInteger == 42,
                 "Oracle return 42");
    ZrCore_ExecIr_OracleResultFree(&execution);
}

static void test_scalar_codec(SZrExecIrModule *module) {
    TZrByte encoded[ZR_ARTIFACT_EXEC_IR_SCALAR_ENCODED_SIZE];
    TZrByte unchanged[ZR_ARTIFACT_EXEC_IR_SCALAR_ENCODED_SIZE];
    SZrExecIrModule decoded;
    SZrArtifactExecIrDiagnostic diagnostic;
    memset(encoded, 0xa5, sizeof(encoded));
    require_true(ZrCore_ArtifactExecIrScalar_Write(module, encoded,
                 sizeof(encoded), &diagnostic) == ZR_ARTIFACT_EXEC_IR_OK,
                 "encode canonical graph fields");
    require_true(encoded[0] == 'E' && encoded[1] == 'I' &&
                 encoded[2] == 'S' && encoded[3] == '1', "EIS1 payload magic");
    ZrCore_ExecIr_ModuleInit(&decoded);
    require_true(ZrCore_ArtifactExecIrScalar_Read(encoded, sizeof(encoded),
                 &decoded, &diagnostic) == ZR_ARTIFACT_EXEC_IR_OK,
                 "decode canonical graph fields");
    require_true(decoded.moduleHash == module->moduleHash &&
                 decoded.contract.abiVersion == ZR_EXECUTION_CONTRACT_ABI_VERSION &&
                 decoded.functions[0].functionToken == TEST_MEMBER &&
                 decoded.functions[0].instructions[0].sourceId == 101u &&
                 decoded.functions[0].instructions[1].sourceId == 102u,
                 "roundtrip preserves graph identity and source IDs");
    run_oracle(&decoded);
    ZrCore_ExecIr_FreeModule(&decoded);

    memcpy(unchanged, encoded, sizeof(encoded));
    module->functions[0].instructions[0].bindingRow = 1u;
    require_true(ZrCore_ArtifactExecIrScalar_Write(module, encoded,
                 sizeof(encoded), &diagnostic) ==
                 ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                 "codec rejects unresolved binding");
    require_true(memcmp(encoded, unchanged, sizeof(encoded)) == 0,
                 "rejected writer changed destination bytes");
    module->functions[0].instructions[0].bindingRow = 0u;
    put16(encoded + ZR_ARTIFACT_EXEC_IR_SCALAR_CONSTANT_OPCODE_OFFSET,
          ZR_EXEC_IR_OPCODE_ADD);
    ZrCore_ExecIr_ModuleInit(&decoded);
    require_true(ZrCore_ArtifactExecIrScalar_Read(encoded, sizeof(encoded),
                 &decoded, &diagnostic) ==
                 ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                 "codec rejects malformed opcode");
    require_true(decoded.functions == ZR_NULL && decoded.functionCount == 0u,
                 "rejected codec published graph");
}

static void write_phase(const char *path) {
    SFixture fixture;
    SZrExecIrModule module;
    SZrArtifactDiagnostic diagnostic;
    SZrArtifactSectionInput sectionsWithRelocation[8];
    SZrExecIrSourceMap unsupportedMap;
    TZrByte *originalBytes;
    TZrByte *retainedBytes;
    TZrSize originalLength;
    TZrSize retainedLength;
    init_fixture(&fixture);
    build_graph(&module);
    test_scalar_codec(&module);
    module.functions[0].instructions[0].bindingRow = 1u;
    require_true(ZrParser_ExecIr_WriteCanonicalZroFile(&fixture.metadata, &module,
                                                       path, &diagnostic) ==
                 ZR_ARTIFACT_STATUS_INVALID_SECTION,
                 "writer rejects unresolved binding");
    module.functions[0].instructions[0].bindingRow = 0u;
    memset(&unsupportedMap, 0, sizeof(unsupportedMap));
    module.functions[0].sourceMaps = &unsupportedMap;
    module.functions[0].sourceMapCount = 1u;
    module.functions[0].sourceMapCapacity = 1u;
    require_true(ZrParser_ExecIr_WriteCanonicalZroFile(&fixture.metadata, &module,
                                                       path, &diagnostic) ==
                 ZR_ARTIFACT_STATUS_INVALID_SECTION,
                 "writer rejects unsupported source map");
    module.functions[0].sourceMaps = NULL;
    module.functions[0].sourceMapCount = 0u;
    module.functions[0].sourceMapCapacity = 0u;
    memcpy(sectionsWithRelocation, fixture.sections, sizeof(fixture.sections));
    sectionsWithRelocation[7] = (SZrArtifactSectionInput){
        ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE, 0u, 0u, NULL};
    fixture.metadata.sections = sectionsWithRelocation;
    fixture.metadata.sectionCount = 8u;
    require_true(ZrParser_ExecIr_WriteCanonicalZroFile(&fixture.metadata, &module,
                                                       path, &diagnostic) ==
                 ZR_ARTIFACT_STATUS_INVALID_SECTION,
                 "writer rejects unsupported relocation section");
    fixture.metadata.sections = fixture.sections;
    fixture.metadata.sectionCount = 7u;
    require_true(ZrParser_ExecIr_WriteCanonicalZroFile(&fixture.metadata, &module,
                                                       path, &diagnostic) ==
                 ZR_ARTIFACT_STATUS_OK, "write canonical ZRAF file");
    originalBytes = read_file(path, &originalLength);
#ifdef ZR_ARTIFACT_TEST_IO_FAILURE
    ZrParser_ExecIrArtifact_TestInjectWriteFailure(ZR_TRUE);
    require_true(ZrParser_ExecIr_WriteCanonicalZroFile(&fixture.metadata, &module,
                                                       path, &diagnostic) ==
                 ZR_ARTIFACT_STATUS_INVALID_SECTION,
                 "injected file write failure");
    ZrParser_ExecIrArtifact_TestInjectWriteFailure(ZR_FALSE);
#endif
    module.functions[0].instructions[0].bindingRow = 1u;
    require_true(ZrParser_ExecIr_WriteCanonicalZroFile(&fixture.metadata, &module,
                                                       path, &diagnostic) ==
                 ZR_ARTIFACT_STATUS_INVALID_SECTION,
                 "existing artifact survives invalid graph");
    module.functions[0].instructions[0].bindingRow = 0u;
    retainedBytes = read_file(path, &retainedLength);
    require_true(retainedLength == originalLength &&
                 memcmp(retainedBytes, originalBytes, originalLength) == 0,
                 "failed writes preserved existing artifact bytes");
    free(retainedBytes);
    free(originalBytes);
    ZrCore_ExecIr_FreeModule(&module);
}

static void read_phase(const char *path) {
    SFixture fixture;
    TZrSize length;
    TZrByte *bytes = read_file(path, &length);
    TZrByte *mutated = (TZrByte *)malloc(length);
    TZrByte *withTrailing = (TZrByte *)malloc(length + 1u);
    SZrExecIrModule module;
    SZrArtifactDiagnostic diagnostic;
    SZrArtifactView outer;
    SZrArtifactSectionView bundle;
    SZrArtifactSectionView contractSection;
    SZrArtifactExecIrView nested;
    const SZrArtifactExecIrSectionView *payload;
    TZrSize bundleOffset;
    require_true(mutated != NULL && withTrailing != NULL,
                 "allocate mutation buffers");
    init_fixture(&fixture);
    require_true(bytes[0] == 'Z' && bytes[1] == 'R' &&
                 bytes[2] == 'A' && bytes[3] == 'F', "ZRAF magic");
    require_true(bytes[4] == 6u, "ZRAF v6");
    require_true(strcmp(ZrCore_Artifact_SectionName(
                 ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE), "exec-ir-bundle") == 0,
                 "canonical section name");
    ZrCore_ExecIr_ModuleInit(&module);
    require_true(ZrCore_Module_OpenExecIrArtifact(bytes, length,
                                                 &fixture.metadata.identity,
                                                 &module, &diagnostic) ==
                 ZR_ARTIFACT_STATUS_OK,
                 "open canonical ExecIR artifact");
    require_true(module.functionCount == 1u && module.constantCount == 1u &&
                 module.constants[0].bits == 42u, "decoded canonical graph");
    run_oracle(&module);
    ZrCore_ExecIr_FreeModule(&module);

    memcpy(mutated, bytes, length);
    mutated[0] = 1u; mutated[1] = 'Z'; mutated[2] = 'R';
    require_rejected(mutated, length, ZR_ARTIFACT_STATUS_BAD_MAGIC);
    memcpy(mutated, bytes, length);
    put16(mutated + 4u, 5u);
    diagnostic = require_rejected(mutated, length,
                                  ZR_ARTIFACT_STATUS_UNSUPPORTED_VERSION);
    require_true(diagnostic.expectedVersion == 6u &&
                 diagnostic.actualVersion == 5u,
                 "old ZRAF schema diagnostic");
    require_rejected(bytes, length - 1u, ZR_ARTIFACT_STATUS_TRUNCATED);
    memcpy(withTrailing, bytes, length);
    withTrailing[length] = 0xa5u;
    require_rejected(withTrailing, length + 1u, ZR_ARTIFACT_STATUS_INVALID_SECTION);

    require_true(ZrCore_Artifact_Read(bytes, length, &outer, &diagnostic) ==
                 ZR_ARTIFACT_STATUS_OK, "outer fixture validates");
    require_true(ZrCore_Artifact_FindSection(&outer,
                 ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE, &bundle, &diagnostic) ==
                 ZR_ARTIFACT_STATUS_OK, "find ExecIR bundle");
    require_true(ZrCore_Artifact_FindSection(&outer,
                 ZR_ARTIFACT_SECTION_CONTRACT_TABLE, &contractSection,
                 &diagnostic) == ZR_ARTIFACT_STATUS_OK,
                 "find outer callable contract");
    bundleOffset = (TZrSize)(bundle.data - bytes);
    memcpy(mutated, bytes, length);
    put32(mutated + (TZrSize)(contractSection.data - bytes),
          ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 4u));
    require_rejected(mutated, length, ZR_ARTIFACT_STATUS_INVALID_SECTION);
    memcpy(mutated, bytes, length);
    put32(mutated + ZR_ARTIFACT_HEADER_ENCODED_SIZE +
          7u * ZR_ARTIFACT_SECTION_DIRECTORY_ENTRY_ENCODED_SIZE,
          ZR_ARTIFACT_SECTION_CODE_TABLE);
    require_rejected(mutated, length, ZR_ARTIFACT_STATUS_INVALID_SECTION);
    require_true(ZrCore_ArtifactExecIr_Read(bundle.data, bundle.byteLength,
                                           &nested, NULL) == ZR_ARTIFACT_EXEC_IR_OK,
                 "nested document validates");
    require_true(nested.abiVersion == 17u && nested.sectionCount == 1u,
                 "nested ABI and section count");
    require_true(ZrCore_ArtifactExecIr_FindSection(&nested,
                 ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR, &payload, NULL) ==
                 ZR_ARTIFACT_EXEC_IR_OK, "find canonical graph payload");

    memcpy(mutated, bytes, length);
    put16(mutated + bundleOffset + 8u, 16u);
    diagnostic = require_rejected(mutated, length,
                                  ZR_ARTIFACT_STATUS_UNSUPPORTED_VERSION);
    require_true(diagnostic.expectedVersion == 17u &&
                 diagnostic.actualVersion == 16u,
                 "old nested ABI diagnostic");
    memcpy(mutated, bytes, length);
    put16(mutated + bundleOffset + 10u, 1u);
    require_rejected(mutated, length, ZR_ARTIFACT_STATUS_INVALID_SECTION);
    memcpy(mutated, bytes, length);
    put64(mutated + bundleOffset + 12u, TEST_MODULE_HASH + 1u);
    require_rejected(mutated, length, ZR_ARTIFACT_STATUS_MODULE_HASH_MISMATCH);
    for (TZrUInt32 kind = ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_BC;
         kind <= ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS; ++kind) {
        memcpy(mutated, bytes, length);
        put32(mutated + bundleOffset + ZR_ARTIFACT_EXEC_IR_HEADER_SIZE, kind);
        require_rejected(mutated, length, ZR_ARTIFACT_STATUS_INVALID_SECTION);
    }
    memcpy(mutated, bytes, length);
    {
        TZrSize opcodeOffset = (TZrSize)(payload->data - bytes) +
                               ZR_ARTIFACT_EXEC_IR_SCALAR_CONSTANT_OPCODE_OFFSET;
        TZrSize payloadOffset = (TZrSize)(payload->data - bytes);
        put16(mutated + opcodeOffset, ZR_EXEC_IR_OPCODE_ADD);
        put64(mutated + bundleOffset + 20u,
              ZrCore_ArtifactExecIr_HashBytes(mutated + payloadOffset,
                                              payload->byteLength));
        require_rejected(mutated, length, ZR_ARTIFACT_STATUS_INVALID_SECTION);
    }
    free(mutated);
    free(withTrailing);
    free(bytes);
}

int main(int argc, char **argv) {
    require_true(argc == 3, "expected --write or --read and D artifact path");
    if (strcmp(argv[1], "--write") == 0) write_phase(argv[2]);
    else if (strcmp(argv[1], "--read") == 0) read_phase(argv[2]);
    else require_true(0, "invalid test mode");
    return EXIT_SUCCESS;
}
