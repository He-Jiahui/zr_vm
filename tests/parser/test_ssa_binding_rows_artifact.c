#include "zr_vm_core/artifact_exec_ir_scalar.h"
#include "zr_vm_core/exec_ir.h"
#include "artifact_exec_ir_scalar_eis3.h"
#include "artifact_exec_ir_scalar_eis4.h"
#include "artifact_exec_ir_scalar_eis5.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_TEST_EIS6_FAULT_INJECTION)
#include "ssa_eis6_fault_allocator.h"
#endif

static int require_true(TZrBool condition, const char *message) {
    if (condition) return 1;
    fprintf(stderr, "FAIL: %s\n", message);
    return 0;
}

static void set_execution_contract(SZrExecutionContract *contract,
                                   TZrUInt32 targetToken,
                                   TZrUInt64 signatureHash,
                                   TZrUInt64 layoutHash,
                                   TZrUInt64 moduleHash) {
    memset(contract, 0, sizeof(*contract));
    contract->schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    contract->abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    contract->logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    contract->generation = 1u;
    contract->targetToken = targetToken;
    contract->signatureHash = signatureHash;
    contract->layoutHash = layoutHash;
    contract->moduleHash = moduleHash;
}

static int build_legacy_eis1_module(SZrExecIrModule *module) {
    const TZrMetadataToken moduleToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MODULE, 1u);
    const TZrMetadataToken functionToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 91u);
    const TZrUInt64 signatureHash = UINT64_C(0x33889911);
    const TZrUInt64 layoutHash = UINT64_C(0x4499aa22);
    const TZrUInt64 moduleHash = UINT64_C(0x55aabb33);
    const SZrExecIrConstant constant = {ZR_VALUE_TYPE_INT64, 0u, 42u};
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrFunction *function;
    SZrExecIrInstruction instruction;
    SZrExecIrRange constantRange, valueRange, returnRange;
    TZrExecIrFunctionId functionId;
    TZrExecIrValueId valueId;
    TZrExecIrInstructionId instructionId;

    ZrCore_ExecIr_ModuleInit(module);
    module->id = 1u;
    module->moduleToken = moduleToken;
    module->moduleHash = moduleHash;
    set_execution_contract(&module->contract, moduleToken, signatureHash,
                          layoutHash, moduleHash);
    if (!ZrCore_ExecIr_ModuleAppendConstant(module, &constant, 1u,
                                           &constantRange) ||
        !ZrCore_ExecIr_ModuleAddFunction(module, functionToken, signatureHash,
                                         &functionId)) {
        ZrCore_ExecIr_FreeModule(module);
        return 0;
    }
    function = ZrCore_ExecIr_ModuleFunctionAt(module, functionId);
    if (function == ZR_NULL) {
        ZrCore_ExecIr_FreeModule(module);
        return 0;
    }
    function->contract.layoutHash = layoutHash;
    function->contract.moduleHash = moduleHash;
    if (ZrCore_ExecIr_FunctionAddBlock(function,
                                      ZR_EXEC_IR_BLOCK_FLAG_ENTRY) != 1u) {
        ZrCore_ExecIr_FreeModule(module);
        return 0;
    }
    valueId = ZrCore_ExecIr_FunctionAddValue(
            function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    if (valueId != 1u ||
        !ZrCore_ExecIr_FunctionAppendResults(function, &valueId, 1u,
                                             &valueRange) ||
        !ZrCore_ExecIr_FunctionAppendOperands(function, &valueId, 1u,
                                              &returnRange)) {
        ZrCore_ExecIr_FreeModule(module);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = valueRange;
    instruction.layoutId = constantRange.start;
    instruction.sourceId = 101u;
    if (!ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                 &instructionId) ||
        instructionId != 1u) {
        ZrCore_ExecIr_FreeModule(module);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnRange;
    instruction.sourceId = 102u;
    if (!ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                 &instructionId) ||
        instructionId != 2u) {
        ZrCore_ExecIr_FreeModule(module);
        return 0;
    }
    function->blocks[0].instructions.start = 0u;
    function->blocks[0].instructions.count = 2u;
    function->blocks[0].terminatorInstructionId = 2u;
    if (!ZrCore_ExecIr_VerifyModule(module, &diagnostic)) {
        ZrCore_ExecIr_FreeModule(module);
        return 0;
    }
    return 1;
}

typedef EZrArtifactExecIrStatus (*FScalarGetSize)(
        const SZrExecIrModule *, TZrUInt32 *, SZrArtifactExecIrDiagnostic *);
typedef EZrArtifactExecIrStatus (*FScalarWrite)(
        const SZrExecIrModule *, TZrByte *, TZrUInt32,
        SZrArtifactExecIrDiagnostic *);

static int require_direct_rejection(
        const SZrExecIrModule *module,
        FScalarGetSize getSize,
        FScalarWrite write,
        const char *label) {
    TZrUInt32 encodedSize = 0x12345678u;
    TZrByte bytes[ZR_ARTIFACT_EXEC_IR_CFG_ENCODED_SIZE];
    SZrArtifactExecIrDiagnostic diagnostic;
    int ok = 1;
    memset(bytes, 0xa5, sizeof(bytes));
    ok &= require_true(getSize(module, &encodedSize, &diagnostic) ==
                               ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                       label);
    ok &= require_true(encodedSize == 0x12345678u &&
                               diagnostic.status ==
                                       ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                               diagnostic.sectionKind ==
                                       ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR &&
                               diagnostic.byteOffset == 0u,
                       "direct scalar size failure published output or imprecise diagnostic");
    ok &= require_true(write(module, bytes, (TZrUInt32)sizeof(bytes),
                             &diagnostic) ==
                               ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
                       "direct scalar writer accepted typed-empty rows");
    ok &= require_true(diagnostic.status == ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                               diagnostic.sectionKind ==
                                       ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR &&
                               diagnostic.byteOffset == 0u,
                       "direct scalar write returned an imprecise diagnostic");
    for (TZrUInt32 index = 0u; index < (TZrUInt32)sizeof(bytes); ++index) {
        ok &= require_true(bytes[index] == 0xa5u,
                            "direct scalar write changed bytes on rejection");
    }
    return ok;
}

#define TEST_EIS6_FIXED_PREFIX_SIZE ((TZrUInt32)224u)
#define TEST_EIS6_CONSTANT_SIZE ((TZrUInt32)16u)
#define TEST_EIS6_VALUE_SIZE ((TZrUInt32)24u)
#define TEST_EIS6_BLOCK_SIZE ((TZrUInt32)40u)
#define TEST_EIS6_INSTRUCTION_SIZE ((TZrUInt32)84u)
#define TEST_EIS6_BINDING_ROW_SIZE ((TZrUInt32)96u)
#define TEST_EIS6_INSTRUCTION_BINDING_ROW_OFFSET ((TZrUInt32)80u)

static TZrUInt32 read_u32_le(const TZrByte *bytes) {
    return (TZrUInt32)bytes[0] | ((TZrUInt32)bytes[1] << 8u) |
           ((TZrUInt32)bytes[2] << 16u) | ((TZrUInt32)bytes[3] << 24u);
}

static TZrUInt64 read_u64_le(const TZrByte *bytes) {
    return (TZrUInt64)read_u32_le(bytes) |
           ((TZrUInt64)read_u32_le(bytes + 4u) << 32u);
}

static void write_u32_le(TZrByte *bytes, TZrUInt32 value) {
    bytes[0] = (TZrByte)(value & 0xffu);
    bytes[1] = (TZrByte)((value >> 8u) & 0xffu);
    bytes[2] = (TZrByte)((value >> 16u) & 0xffu);
    bytes[3] = (TZrByte)((value >> 24u) & 0xffu);
}

static TZrUInt32 eis6_instruction_offset(const TZrByte *bytes) {
    TZrUInt32 offset = TEST_EIS6_FIXED_PREFIX_SIZE;
    offset += read_u32_le(bytes + 12u) * TEST_EIS6_CONSTANT_SIZE;
    offset += read_u32_le(bytes + 16u) * TEST_EIS6_VALUE_SIZE;
    offset += read_u32_le(bytes + 20u) * TEST_EIS6_BLOCK_SIZE;
    return offset;
}

static int binding_rows_equal(const SZrExecIrBindingRow *left,
                              const SZrExecIrBindingRow *right) {
    return left->rowIndex == right->rowIndex &&
           left->instructionId == right->instructionId &&
           left->segmentIndex == right->segmentIndex &&
           left->contract.bindingKind == right->contract.bindingKind &&
           left->contract.targetMetadataToken ==
                   right->contract.targetMetadataToken &&
           left->contract.signatureToken == right->contract.signatureToken &&
           left->contract.ownerTypeToken == right->contract.ownerTypeToken &&
           left->contract.signatureHash == right->contract.signatureHash &&
           left->contract.moduleSignatureHash ==
                   right->contract.moduleSignatureHash &&
           left->contract.layoutVersion == right->contract.layoutVersion &&
           left->contract.dispatchSlot == right->contract.dispatchSlot &&
           left->contract.layoutHash == right->contract.layoutHash &&
           left->contract.operation == right->contract.operation &&
           left->contract.reserved0 == right->contract.reserved0 &&
           left->contract.reserved1 == right->contract.reserved1 &&
           left->location.kind == right->location.kind &&
           left->location.targetIndex == right->location.targetIndex &&
           left->location.ownerDepth == right->location.ownerDepth &&
           left->location.flags == right->location.flags &&
           left->sourceId == right->sourceId;
}

static int module_is_empty(const SZrExecIrModule *module) {
    return module->id == 0u && module->moduleToken == 0u &&
           module->moduleHash == 0u &&
           module->contract.schemaVersion == 0u &&
           module->constantCount == 0u && module->constantCapacity == 0u &&
           module->constants == ZR_NULL && module->functionCount == 0u &&
           module->functionCapacity == 0u && module->functions == ZR_NULL &&
           module->layoutCount == 0u && module->layoutCapacity == 0u &&
           module->layouts == ZR_NULL && module->sourceMapCount == 0u &&
           module->sourceMapCapacity == 0u && module->sourceMaps == ZR_NULL;
}

static int expect_eis6_read_rejection(const TZrByte *bytes,
                                      TZrUInt32 length,
                                      EZrArtifactExecIrStatus expectedStatus,
                                      const char *message) {
    SZrExecIrModule decoded, before;
    SZrArtifactExecIrDiagnostic diagnostic;
    EZrArtifactExecIrStatus status;
    ZrCore_ExecIr_ModuleInit(&decoded);
    before = decoded;
    status = ZrCore_ArtifactExecIrScalar_Read(bytes, length, &decoded,
                                               &diagnostic);
    int ok = require_true(status == expectedStatus &&
                                  module_is_empty(&decoded) &&
                                  memcmp(&decoded, &before, sizeof(decoded)) == 0,
                          message);
    ZrCore_ExecIr_FreeModule(&decoded);
    return ok;
}

static SZrExecIrBindingRow make_test_binding_row(TZrUInt64 moduleHash) {
    SZrExecIrBindingRow row;
    memset(&row, 0, sizeof(row));
    row.rowIndex = 0u;
    row.instructionId = 2u;
    row.segmentIndex = ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE;
    row.contract.bindingKind = ZR_CALL_BINDING_DIRECT;
    row.contract.targetMetadataToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 77u);
    row.contract.signatureToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 91u);
    row.contract.signatureHash = UINT64_C(0x0102030405060708);
    row.contract.moduleSignatureHash = moduleHash;
    row.contract.dispatchSlot = ZR_CALL_BINDING_SLOT_NONE;
    row.contract.operation = ZR_CALL_BINDING_OPERATION_CALL;
    row.location.kind = ZR_CALL_BINDING_RELOCATION_MODULE;
    row.location.targetIndex = UINT32_C(0x11223344);
    row.sourceId = UINT32_C(0xa1b2c3d4);
    return row;
}

static int build_typed_call_module(SZrExecIrModule *module) {
    const TZrMetadataToken moduleToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MODULE, 1u);
    const TZrMetadataToken functionToken =
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 91u);
    const TZrUInt64 signatureHash = UINT64_C(0x445566778899aabb);
    const TZrUInt64 layoutHash = UINT64_C(0x1029384756abcdef);
    const TZrUInt64 moduleHash = UINT64_C(0x8877665544332211);
    const TZrExecIrMemoryTokenId memoryInTokens[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 1u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 1u)
    };
    const TZrExecIrMemoryTokenId memoryOutTokens[2] = {
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_MANAGED_HEAP, 2u),
        ZR_EXEC_IR_MEMORY_TOKEN_MAKE(ZR_EXEC_IR_MEMORY_NATIVE_FFI, 2u)
    };
    const SZrExecIrConstant constant = {ZR_VALUE_TYPE_INT64, 0u, 42u};
    SZrExecIrRange constantRange, constantResult, callOperand, callResult;
    SZrExecIrRange returnOperand, memoryIn, memoryOut;
    SZrExecIrBindingRow row;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrInstruction instruction;
    SZrExecIrFunction *function;
    TZrExecIrFunctionId functionId;
    TZrExecIrValueId inputValue, callValue;
    TZrExecIrInstructionId instructionId;

    ZrCore_ExecIr_ModuleInit(module);
    module->id = 1u;
    module->moduleToken = moduleToken;
    module->moduleHash = moduleHash;
    set_execution_contract(&module->contract, moduleToken, signatureHash,
                          layoutHash, moduleHash);
    if (!ZrCore_ExecIr_ModuleAppendConstant(module, &constant, 1u,
                                           &constantRange) ||
        !ZrCore_ExecIr_ModuleAddFunction(module, functionToken, signatureHash,
                                         &functionId))
        goto fail;
    function = ZrCore_ExecIr_ModuleFunctionAt(module, functionId);
    if (function == ZR_NULL) goto fail;
    set_execution_contract(&function->contract, functionToken, signatureHash,
                           layoutHash, moduleHash);
    if (ZrCore_ExecIr_FunctionAddBlock(
                function, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) !=
        ZR_EXEC_IR_BLOCK_ID_ENTRY)
        goto fail;
    function->entryBlockId = ZR_EXEC_IR_BLOCK_ID_ENTRY;

    inputValue = ZrCore_ExecIr_FunctionAddValue(
            function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    if (inputValue != 1u ||
        !ZrCore_ExecIr_FunctionAppendResults(function, &inputValue, 1u,
                                             &constantResult))
        goto fail;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
    instruction.results = constantResult;
    instruction.layoutId = constantRange.start;
    instruction.sourceId = 501u;
    if (!ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                 &instructionId) ||
        instructionId != 1u)
        goto fail;

    callValue = ZrCore_ExecIr_FunctionAddValue(
            function, ZR_VALUE_TYPE_INT64, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
            ZR_EXEC_IR_NULLABILITY_UNKNOWN);
    if (callValue != 2u ||
        !ZrCore_ExecIr_FunctionAppendResults(function, &callValue, 1u,
                                             &callResult) ||
        !ZrCore_ExecIr_FunctionAppendOperands(function, &inputValue, 1u,
                                              &callOperand) ||
        !ZrCore_ExecIr_FunctionAppendMemoryTokens(function, memoryInTokens, 2u,
                                                  &memoryIn) ||
        !ZrCore_ExecIr_FunctionAppendMemoryTokens(function, memoryOutTokens, 2u,
                                                  &memoryOut))
        goto fail;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_CALL;
    instruction.flags = (TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_THROW |
                                    ZR_EXEC_IR_FLAG_MAY_ALLOCATE);
    instruction.operands = callOperand;
    instruction.results = callResult;
    instruction.memoryIn = memoryIn;
    instruction.memoryOut = memoryOut;
    instruction.effectIn = 1u;
    instruction.effectOut = 2u;
    instruction.sourceId = 502u;
    if (!ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                 &instructionId) ||
        instructionId != 2u)
        goto fail;

    if (!ZrCore_ExecIr_FunctionAppendOperands(function, &callValue, 1u,
                                              &returnOperand))
        goto fail;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    instruction.operands = returnOperand;
    instruction.sourceId = 503u;
    if (!ZrCore_ExecIr_FunctionAppendInstruction(function, &instruction,
                                                 &instructionId) ||
        instructionId != 3u)
        goto fail;
    function->blocks[0].instructions.start = 0u;
    function->blocks[0].instructions.count = 3u;
    function->blocks[0].terminatorInstructionId = 3u;

    row = make_test_binding_row(moduleHash);
    if (!ZrCore_ExecIr_FunctionSetBindingRows(function, &row, 1u,
                                               &diagnostic) ||
        !ZrCore_ExecIr_VerifyModule(module, &diagnostic))
        goto fail;
    return 1;

fail:
    ZrCore_ExecIr_FreeModule(module);
    return 0;
}

static int roundtrip_typed_empty(const SZrExecIrModule *source) {
    SZrExecIrModule decoded;
    EZrArtifactExecIrStatus readStatus;
    SZrArtifactExecIrDiagnostic diagnostic;
    TZrUInt32 encodedSize = 0u;
    TZrByte *bytes = ZR_NULL;
    TZrByte *repeat = ZR_NULL;
    TZrUInt32 index;
    int ok = 1;

    if (!require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                              source, &encodedSize, &diagnostic) ==
                              ZR_ARTIFACT_EXEC_IR_OK,
                      "typed-empty graph did not select a scalar wire format"))
        return 0;
    bytes = (TZrByte *)malloc(encodedSize);
    repeat = (TZrByte *)malloc(encodedSize);
    if (!require_true(bytes != ZR_NULL && repeat != ZR_NULL,
                      "typed-empty roundtrip buffer allocation failed"))
        goto cleanup;
    memset(bytes, 0xa5, encodedSize);
    ok &= require_true(ZrCore_ArtifactExecIrScalar_Write(
                               source, bytes, encodedSize, &diagnostic) ==
                               ZR_ARTIFACT_EXEC_IR_OK,
                       "typed-empty EIS6 write failed");
    ok &= require_true(encodedSize > TEST_EIS6_FIXED_PREFIX_SIZE &&
                               bytes[0] == 'E' && bytes[1] == 'I' &&
                               bytes[2] == 'S' && bytes[3] == '6' &&
                               bytes[4] == 6u && bytes[5] == 0u &&
                               bytes[6] == 0u && bytes[7] == 0u &&
                               read_u32_le(bytes + 8u) == encodedSize &&
                               read_u32_le(bytes + 48u) ==
                                       ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED &&
                               read_u32_le(bytes + 52u) == 0u,
                       "typed-empty graph did not encode the EIS6 schema header");
    memset(repeat, 0, encodedSize);
    ok &= require_true(ZrCore_ArtifactExecIrScalar_Write(
                               source, repeat, encodedSize, &diagnostic) ==
                                       ZR_ARTIFACT_EXEC_IR_OK &&
                               memcmp(bytes, repeat, encodedSize) == 0,
                       "typed-empty EIS6 bytes were not deterministic");
    memset(repeat, 0xa5, encodedSize);
    ok &= require_true(ZrCore_ArtifactExecIrScalar_Write(
                               source, repeat, encodedSize - 1u,
                               &diagnostic) == ZR_ARTIFACT_EXEC_IR_TRUNCATED,
                       "short typed-empty EIS6 write did not fail as truncated");
    for (index = 0u; index < encodedSize; ++index) {
        ok &= require_true(repeat[index] == 0xa5u,
                           "short typed-empty EIS6 write modified destination bytes");
    }

    ZrCore_ExecIr_ModuleInit(&decoded);
    readStatus = ZrCore_ArtifactExecIrScalar_Read(
            bytes, encodedSize, &decoded, &diagnostic);
    ok &= require_true(readStatus == ZR_ARTIFACT_EXEC_IR_OK,
                       "typed-empty EIS6 read failed");
    if (readStatus == ZR_ARTIFACT_EXEC_IR_OK &&
        decoded.functionCount == 1u && decoded.functions != ZR_NULL) {
        const SZrExecIrFunction *function = &decoded.functions[0];
        ok &= require_true(ZrCore_ExecIr_VerifyModule(&decoded, ZR_NULL) &&
                                   function->bindingRowsSchemaVersion ==
                                           ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED &&
                                   function->bindingRowCount == 0u &&
                                   function->bindingRows == ZR_NULL &&
                                   ZrCore_ExecIr_FunctionBindingRowsHash(function) !=
                                           0u,
                           "typed-empty EIS6 read lost the explicit empty row schema");
    } else {
        ok &= require_true(readStatus != ZR_ARTIFACT_EXEC_IR_OK &&
                                   module_is_empty(&decoded),
                           "typed-empty EIS6 read failed or published an incomplete module");
    }
    ZrCore_ExecIr_FreeModule(&decoded);

cleanup:
    free(repeat);
    free(bytes);
    return ok;
}

static int roundtrip_typed_call(const SZrExecIrModule *source) {
    const SZrExecIrFunction *sourceFunction = &source->functions[0];
    const SZrExecIrBindingRow *sourceRow =
            ZrCore_ExecIr_FunctionBindingRowAt(sourceFunction, 1u);
    SZrExecIrModule decoded;
    EZrArtifactExecIrStatus readStatus;
    SZrArtifactExecIrDiagnostic diagnostic;
    TZrUInt32 encodedSize = 0u;
    TZrUInt32 instructionOffset, rowOffset, associationOffset;
    TZrByte *bytes = ZR_NULL;
    TZrByte *repeat = ZR_NULL;
    TZrByte *malformed = ZR_NULL;
    int ok = 1;

    if (!require_true(sourceRow != ZR_NULL &&
                              ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                                      source, &encodedSize, &diagnostic) ==
                                      ZR_ARTIFACT_EXEC_IR_OK,
                      "typed CALL graph did not select an EIS6 payload"))
        return 0;
    bytes = (TZrByte *)malloc(encodedSize);
    repeat = (TZrByte *)malloc(encodedSize);
    malformed = (TZrByte *)malloc(encodedSize);
    if (!require_true(bytes != ZR_NULL && repeat != ZR_NULL &&
                              malformed != ZR_NULL,
                      "typed CALL roundtrip buffer allocation failed"))
        goto cleanup;

    memset(bytes, 0xa5, encodedSize);
    ok &= require_true(ZrCore_ArtifactExecIrScalar_Write(
                               source, bytes, encodedSize, &diagnostic) ==
                               ZR_ARTIFACT_EXEC_IR_OK,
                       "typed CALL EIS6 write failed");
    ok &= require_true(bytes[0] == 'E' && bytes[1] == 'I' &&
                               bytes[2] == 'S' && bytes[3] == '6' &&
                               read_u32_le(bytes + 8u) == encodedSize &&
                               read_u32_le(bytes + 44u) ==
                                       sourceFunction->memoryTokenCount &&
                               read_u32_le(bytes + 48u) ==
                                       ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED &&
                               read_u32_le(bytes + 52u) == 1u,
                       "typed CALL EIS6 header lost counts or schema identity");
    memset(repeat, 0, encodedSize);
    ok &= require_true(ZrCore_ArtifactExecIrScalar_Write(
                               source, repeat, encodedSize, &diagnostic) ==
                                       ZR_ARTIFACT_EXEC_IR_OK &&
                               memcmp(bytes, repeat, encodedSize) == 0,
                       "typed CALL EIS6 bytes were not deterministic");

    rowOffset = encodedSize - TEST_EIS6_BINDING_ROW_SIZE;
    ok &= require_true(read_u32_le(bytes + rowOffset) == sourceRow->rowIndex &&
                               read_u32_le(bytes + rowOffset + 4u) ==
                                       sourceRow->instructionId &&
                               read_u32_le(bytes + rowOffset + 8u) ==
                                       sourceRow->segmentIndex &&
                               read_u32_le(bytes + rowOffset + 12u) ==
                                       sourceRow->contract.bindingKind &&
                               read_u32_le(bytes + rowOffset + 16u) ==
                                       sourceRow->contract.targetMetadataToken &&
                               read_u32_le(bytes + rowOffset + 20u) ==
                                       sourceRow->contract.signatureToken &&
                               read_u32_le(bytes + rowOffset + 24u) ==
                                       sourceRow->contract.ownerTypeToken &&
                               read_u64_le(bytes + rowOffset + 28u) ==
                                       sourceRow->contract.signatureHash &&
                               read_u64_le(bytes + rowOffset + 36u) ==
                                       sourceRow->contract.moduleSignatureHash &&
                               read_u32_le(bytes + rowOffset + 44u) ==
                                       sourceRow->contract.layoutVersion &&
                               read_u32_le(bytes + rowOffset + 48u) ==
                                       sourceRow->contract.dispatchSlot &&
                               read_u64_le(bytes + rowOffset + 52u) ==
                                       sourceRow->contract.layoutHash &&
                               read_u32_le(bytes + rowOffset + 60u) ==
                                       sourceRow->contract.operation &&
                               read_u32_le(bytes + rowOffset + 64u) ==
                                       sourceRow->contract.reserved0 &&
                               read_u64_le(bytes + rowOffset + 68u) ==
                                       sourceRow->contract.reserved1 &&
                               read_u32_le(bytes + rowOffset + 76u) ==
                                       sourceRow->location.kind &&
                               read_u32_le(bytes + rowOffset + 80u) ==
                                       sourceRow->location.targetIndex &&
                               read_u32_le(bytes + rowOffset + 84u) ==
                                       sourceRow->location.ownerDepth &&
                               read_u32_le(bytes + rowOffset + 88u) ==
                                       sourceRow->location.flags &&
                               read_u32_le(bytes + rowOffset + 92u) ==
                                       sourceRow->sourceId &&
                               read_u32_le(bytes + rowOffset + 8u) == UINT32_MAX &&
                               bytes[rowOffset + 28u] == 0x08u &&
                               bytes[rowOffset + 29u] == 0x07u &&
                               bytes[rowOffset + 30u] == 0x06u &&
                               bytes[rowOffset + 31u] == 0x05u &&
                               bytes[rowOffset + 32u] == 0x04u &&
                               bytes[rowOffset + 33u] == 0x03u &&
                               bytes[rowOffset + 34u] == 0x02u &&
                               bytes[rowOffset + 35u] == 0x01u &&
                               bytes[rowOffset + 36u] == 0x11u &&
                               bytes[rowOffset + 37u] == 0x22u &&
                               bytes[rowOffset + 38u] == 0x33u &&
                               bytes[rowOffset + 39u] == 0x44u &&
                               bytes[rowOffset + 40u] == 0x55u &&
                               bytes[rowOffset + 41u] == 0x66u &&
                               bytes[rowOffset + 42u] == 0x77u &&
                               bytes[rowOffset + 43u] == 0x88u &&
                               bytes[rowOffset + 80u] == 0x44u &&
                               bytes[rowOffset + 81u] == 0x33u &&
                               bytes[rowOffset + 82u] == 0x22u &&
                               bytes[rowOffset + 83u] == 0x11u &&
                               bytes[rowOffset + 92u] == 0xd4u &&
                               bytes[rowOffset + 93u] == 0xc3u &&
                               bytes[rowOffset + 94u] == 0xb2u &&
                               bytes[rowOffset + 95u] == 0xa1u,
                       "typed CALL EIS6 row fields were not fixed little-endian bytes");

    ZrCore_ExecIr_ModuleInit(&decoded);
    readStatus = ZrCore_ArtifactExecIrScalar_Read(
            bytes, encodedSize, &decoded, &diagnostic);
    ok &= require_true(readStatus == ZR_ARTIFACT_EXEC_IR_OK,
                       "typed CALL EIS6 read failed");
    if (readStatus == ZR_ARTIFACT_EXEC_IR_OK &&
        decoded.functionCount == 1u && decoded.functions != ZR_NULL) {
        const SZrExecIrFunction *decodedFunction = &decoded.functions[0];
        const SZrExecIrBindingRow *decodedRow =
                ZrCore_ExecIr_FunctionBindingRowAt(decodedFunction, 1u);
        ok &= require_true(decoded.id == source->id &&
                                   decoded.moduleToken == source->moduleToken &&
                                   decoded.moduleHash == source->moduleHash &&
                                   decodedFunction->id == sourceFunction->id &&
                                   decodedFunction->functionToken ==
                                           sourceFunction->functionToken &&
                                   decodedFunction->signatureHash ==
                                           sourceFunction->signatureHash &&
                                   decodedFunction->bindingRowsSchemaVersion ==
                                           ZR_EXEC_IR_BINDING_ROWS_SCHEMA_TYPED &&
                                   decodedFunction->bindingRowCount == 1u &&
                                   decodedFunction->instructions[1].bindingRow ==
                                           sourceFunction->instructions[1].bindingRow &&
                                   decodedFunction->memoryTokenCount ==
                                           sourceFunction->memoryTokenCount &&
                                   decodedFunction->memoryTokenPool != ZR_NULL &&
                                   memcmp(decodedFunction->memoryTokenPool,
                                          sourceFunction->memoryTokenPool,
                                          sourceFunction->memoryTokenCount *
                                                  sizeof(TZrExecIrMemoryTokenId)) ==
                                           0 &&
                                   decodedRow != ZR_NULL &&
                                   binding_rows_equal(sourceRow, decodedRow) &&
                                   sourceRow->sourceId !=
                                           sourceFunction->instructions[1].sourceId &&
                                   decodedRow->sourceId !=
                                           decodedFunction->instructions[1].sourceId &&
                                   ZrCore_ExecIr_FunctionBindingRowsHash(
                                           decodedFunction) ==
                                           ZrCore_ExecIr_FunctionBindingRowsHash(
                                                   sourceFunction) &&
                                   ZrCore_ExecIr_VerifyModule(&decoded, ZR_NULL),
                           "typed CALL EIS6 roundtrip changed row, association, memory pool, or module identity");
    } else {
        ok &= require_true(readStatus != ZR_ARTIFACT_EXEC_IR_OK &&
                                   module_is_empty(&decoded),
                           "typed CALL EIS6 read failed or published an incomplete module");
    }
    ZrCore_ExecIr_FreeModule(&decoded);

    instructionOffset = eis6_instruction_offset(bytes);
    associationOffset = instructionOffset + TEST_EIS6_INSTRUCTION_SIZE +
                        TEST_EIS6_INSTRUCTION_BINDING_ROW_OFFSET;
    memset(malformed, 0, encodedSize);
    memcpy(malformed, bytes, encodedSize);
    write_u32_le(malformed + associationOffset, 0u);
    ZrCore_ExecIr_ModuleInit(&decoded);
    ok &= require_true(ZrCore_ArtifactExecIrScalar_Read(
                               malformed, encodedSize, &decoded, &diagnostic) ==
                               ZR_ARTIFACT_EXEC_IR_INVALID_SECTION &&
                               module_is_empty(&decoded),
                       "EIS6 reader repaired a missing raw instruction-to-row reference");
    ZrCore_ExecIr_FreeModule(&decoded);

    memset(malformed, 0, encodedSize);
    memcpy(malformed, bytes, encodedSize);
    write_u32_le(malformed + rowOffset + 4u, 1u);
    ok &= expect_eis6_read_rejection(
            malformed, encodedSize, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
            "EIS6 reader accepted a row that names the non-CALL instruction");

    memset(malformed, 0, encodedSize);
    memcpy(malformed, bytes, encodedSize);
    malformed[6u] = 1u;
    ok &= expect_eis6_read_rejection(
            malformed, encodedSize,
            ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION,
            "EIS6 reader accepted nonzero reserved header bits");

    memset(malformed, 0, encodedSize);
    memcpy(malformed, bytes, encodedSize);
    write_u32_le(malformed + 8u, encodedSize - 1u);
    ok &= expect_eis6_read_rejection(
            malformed, encodedSize, ZR_ARTIFACT_EXEC_IR_TRUNCATED,
            "EIS6 reader accepted a mismatched declared byte length");

    memset(malformed, 0, encodedSize);
    memcpy(malformed, bytes, encodedSize);
    write_u32_le(malformed + 44u, 16384u);
    ok &= expect_eis6_read_rejection(
            malformed, encodedSize, ZR_ARTIFACT_EXEC_IR_LIMIT,
            "EIS6 reader exceeded the shared memory-token pool limit");

    memset(malformed, 0, encodedSize);
    memcpy(malformed, bytes, encodedSize);
    write_u32_le(malformed + 24u, 4097u);
    ok &= expect_eis6_read_rejection(
            malformed, encodedSize, ZR_ARTIFACT_EXEC_IR_LIMIT,
            "EIS6 reader exceeded the EIS5 scalar-node work bound");

    memset(malformed, 0, encodedSize);
    memcpy(malformed, bytes, encodedSize);
    write_u32_le(malformed + 52u, UINT32_MAX);
    ok &= expect_eis6_read_rejection(
            malformed, encodedSize, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
            "EIS6 reader accepted rows beyond the bounded instruction count");

    memset(malformed, 0, encodedSize);
    memcpy(malformed, bytes, encodedSize);
    write_u32_le(malformed + instructionOffset +
                         TEST_EIS6_INSTRUCTION_SIZE + 48u,
                 UINT32_MAX);
    ok &= expect_eis6_read_rejection(
            malformed, encodedSize, ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
            "EIS6 reader accepted an overflowing CALL memory-token range");

cleanup:
    free(malformed);
    free(repeat);
    free(bytes);
    return ok;
}

#if defined(ZR_TEST_EIS6_FAULT_INJECTION)
static int allocation_fault_sweep(const SZrExecIrModule *source) {
    const SZrExecIrFunction *sourceFunction = &source->functions[0];
    TZrUInt32 encodedSize = 0u;
    TZrByte *bytes = ZR_NULL;
    size_t baselineOutstanding = ZrEis6Test_OutstandingAllocations();
    size_t allocationCount;
    SZrArtifactExecIrDiagnostic diagnostic;
    int ok = 1;

    ok &= require_true(!ZrEis6Test_TrackingOverflowed(),
                       "EIS6 fault tracker overflowed before sweep");
    ok &= require_true(ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                               source, &encodedSize, &diagnostic) ==
                               ZR_ARTIFACT_EXEC_IR_OK,
                       "could not size allocation-sweep EIS6 fixture");
    if (!ok) return 0;
    bytes = (TZrByte *)malloc(encodedSize);
    if (!require_true(bytes != ZR_NULL,
                      "allocation-sweep wire buffer allocation failed"))
        return 0;
    ok &= require_true(ZrCore_ArtifactExecIrScalar_Write(
                               source, bytes, encodedSize, &diagnostic) ==
                               ZR_ARTIFACT_EXEC_IR_OK,
                       "could not encode allocation-sweep fixture");

    {
        SZrExecIrModule decoded;
        ZrCore_ExecIr_ModuleInit(&decoded);
        ZrEis6Test_ArmFailure(SIZE_MAX);
        EZrArtifactExecIrStatus status = ZrCore_ArtifactExecIrScalar_Read(
                bytes, encodedSize, &decoded, &diagnostic);
        allocationCount = ZrEis6Test_AllocationAttempts();
        ok &= require_true(status == ZR_ARTIFACT_EXEC_IR_OK &&
                                   allocationCount != 0u &&
                                   !ZrEis6Test_TrackingOverflowed() &&
                                   decoded.functionCount == 1u &&
                                   ZrCore_ExecIr_VerifyModule(&decoded,
                                                              ZR_NULL) &&
                                   ZrCore_ExecIr_FunctionBindingRowsHash(
                                           &decoded.functions[0]) ==
                                           ZrCore_ExecIr_FunctionBindingRowsHash(
                                                   sourceFunction),
                           "no-fault EIS6 allocation baseline failed");
        ZrCore_ExecIr_FreeModule(&decoded);
        ok &= require_true(ZrEis6Test_OutstandingAllocations() ==
                                   baselineOutstanding,
                           "successful EIS6 baseline leaked a Core allocation");
    }

    for (size_t ordinal = 0u; ordinal < allocationCount; ++ordinal) {
        SZrExecIrModule decoded, before;
        EZrArtifactExecIrStatus status;
        ZrCore_ExecIr_ModuleInit(&decoded);
        before = decoded;
        ZrEis6Test_ArmFailure(ordinal);
        status = ZrCore_ArtifactExecIrScalar_Read(
                bytes, encodedSize, &decoded, &diagnostic);
        ok &= require_true(status == ZR_ARTIFACT_EXEC_IR_LIMIT &&
                                   module_is_empty(&decoded) &&
                                   memcmp(&decoded, &before, sizeof(decoded)) ==
                                           0 &&
                                   ZrEis6Test_AllocationAttempts() ==
                                           ordinal + 1u &&
                                   !ZrEis6Test_TrackingOverflowed() &&
                                   ZrEis6Test_OutstandingAllocations() ==
                                           baselineOutstanding,
                           "EIS6 allocation failure changed output or leaked tracked memory");

        ZrEis6Test_DisableFailure();
        status = ZrCore_ArtifactExecIrScalar_Read(
                bytes, encodedSize, &decoded, &diagnostic);
        ok &= require_true(status == ZR_ARTIFACT_EXEC_IR_OK &&
                                   ZrCore_ExecIr_VerifyModule(&decoded,
                                                              ZR_NULL) &&
                                   ZrCore_ExecIr_FunctionBindingRowsHash(
                                           &decoded.functions[0]) ==
                                           ZrCore_ExecIr_FunctionBindingRowsHash(
                                                   sourceFunction),
                           "EIS6 retry after an allocation failure did not decode the original module");
        ZrCore_ExecIr_FreeModule(&decoded);
        ok &= require_true(ZrEis6Test_OutstandingAllocations() ==
                                   baselineOutstanding,
                           "EIS6 retry/free did not restore the allocation baseline");
    }
    ZrEis6Test_DisableFailure();
    free(bytes);
    if (ok) printf("EIS6 allocation-fault sweep PASS: %zu allocation sites\n",
                   allocationCount);
    return ok;
}
#endif

#include "test_ssa_binding_rows_artifact_capacity.inc"

int main(void) {
    SZrExecIrModule legacyModule;
    SZrExecIrModule callModule;
    SZrExecIrFunction *function;
    SZrExecIrDiagnostic execDiagnostic;
    SZrArtifactExecIrDiagnostic artifactDiagnostic;
    TZrUInt32 encodedSize = 0x12345678u;
    TZrByte bytes[ZR_ARTIFACT_EXEC_IR_EIS5_HEADER_SIZE];
    TZrUInt32 index;
    int ok = 1;

    if (!require_true(build_legacy_eis1_module(&legacyModule),
                      "legacy EIS1 fixture failed VerifyModule"))
        return EXIT_FAILURE;
    ok &= require_true(legacyModule.functions[0].bindingRowsSchemaVersion ==
                               ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY &&
                               ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                                       &legacyModule, &encodedSize,
                                       &artifactDiagnostic) ==
                                       ZR_ARTIFACT_EXEC_IR_OK &&
                               encodedSize ==
                                       ZR_ARTIFACT_EXEC_IR_SCALAR_ENCODED_SIZE,
                       "legacy schema-zero graph no longer selects EIS1");
    memset(bytes, 0xa5, sizeof(bytes));
    ok &= require_true(ZrCore_ArtifactExecIrScalar_Write(
                               &legacyModule, bytes, (TZrUInt32)sizeof(bytes),
                               &artifactDiagnostic) == ZR_ARTIFACT_EXEC_IR_TRUNCATED,
                       "short legacy EIS1 buffer did not fail safely");
    {
        TZrByte legacyBytes[ZR_ARTIFACT_EXEC_IR_SCALAR_ENCODED_SIZE];
        ok &= require_true(ZrCore_ArtifactExecIrScalar_Write(
                                   &legacyModule, legacyBytes,
                                   (TZrUInt32)sizeof(legacyBytes),
                                   &artifactDiagnostic) ==
                                   ZR_ARTIFACT_EXEC_IR_OK &&
                                   legacyBytes[0] == 'E' &&
                                   legacyBytes[1] == 'I' &&
                                   legacyBytes[2] == 'S' &&
                                   legacyBytes[3] == '1',
                           "legacy schema-zero graph could not write EIS1");
    }
    function = &legacyModule.functions[0];
    if (!require_true(ZrCore_ExecIr_FunctionSetBindingRows(
                              function, ZR_NULL, 0u, &execDiagnostic),
                      "could not set typed-empty row schema")) {
        ZrCore_ExecIr_FreeModule(&legacyModule);
        return EXIT_FAILURE;
    }
    ok &= require_direct_rejection(
            &legacyModule, ZrCore_ArtifactExecIrScalarEis3_GetEncodedSize,
            ZrCore_ArtifactExecIrScalarEis3_Write,
            "direct EIS3 accepted typed-empty rows");
    ok &= require_direct_rejection(
            &legacyModule, ZrCore_ArtifactExecIrScalarEis4_GetEncodedSize,
            ZrCore_ArtifactExecIrScalarEis4_Write,
            "direct EIS4 accepted typed-empty rows");
    ok &= require_direct_rejection(
            &legacyModule, ZrCore_ArtifactExecIrScalarEis5_GetEncodedSize,
            ZrCore_ArtifactExecIrScalarEis5_Write,
            "direct EIS5 accepted typed-empty rows");
    ok &= require_true(roundtrip_typed_empty(&legacyModule),
                       "typed-empty scalar EIS6 roundtrip failed");
    ok &= require_true(roundtrip_reserved_row_capacity(&legacyModule),
                       "typed-empty EIS6 rejected reserved binding storage");

    if (!require_true(build_typed_call_module(&callModule),
                      "typed CALL fixture failed Core VerifyModule")) {
        ZrCore_ExecIr_FreeModule(&legacyModule);
        return EXIT_FAILURE;
    }
    ok &= require_true(roundtrip_typed_call(&callModule),
                       "typed CALL scalar EIS6 roundtrip failed");
    ok &= require_true(roundtrip_reserved_row_capacity(&callModule),
                       "typed CALL EIS6 rejected reserved row capacity");
    ok &= require_true(roundtrip_call_without_binding_facts(&callModule),
                       "typed rowless CALL scalar EIS6 roundtrip failed");
#if defined(ZR_TEST_EIS6_FAULT_INJECTION)
    ok &= require_true(allocation_fault_sweep(&callModule),
                       "typed CALL EIS6 allocation-fault sweep failed");
#endif

    ZrCore_ExecIr_FreeModule(&callModule);
    ZrCore_ExecIr_FreeModule(&legacyModule);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
