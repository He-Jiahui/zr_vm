#include "zr_vm_core/artifact_exec_ir_scalar.h"
#include "zr_vm_core/exec_ir.h"
#include "artifact_exec_ir_scalar_eis3.h"
#include "artifact_exec_ir_scalar_eis4.h"
#include "artifact_exec_ir_scalar_eis5.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(void) {
    SZrExecIrModule module;
    SZrExecIrModule legacyModule;
    SZrExecIrFunction *function;
    SZrExecIrDiagnostic execDiagnostic;
    SZrArtifactExecIrDiagnostic artifactDiagnostic;
    TZrExecIrFunctionId functionId;
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

    ZrCore_ExecIr_ModuleInit(&module);
    if (!require_true(ZrCore_ExecIr_ModuleAddFunction(
                              &module,
                              ZR_METADATA_TOKEN_MAKE(
                                      ZR_METADATA_TABLE_MEMBER_DEF, 91u),
                              UINT64_C(0x7711), &functionId),
                      "typed artifact fixture function append failed")) {
        ZrCore_ExecIr_FreeModule(&module);
        return EXIT_FAILURE;
    }
    function = ZrCore_ExecIr_ModuleFunctionAt(&module, functionId);
    if (!require_true(function != ZR_NULL &&
                              ZrCore_ExecIr_FunctionSetBindingRows(
                                      function, ZR_NULL, 0u, &execDiagnostic),
                      "typed empty artifact fixture setup failed")) {
        ZrCore_ExecIr_FreeModule(&module);
        return EXIT_FAILURE;
    }

    encodedSize = 0x12345678u;
    memset(bytes, 0xa5, sizeof(bytes));
    ok &= require_true(
            ZrCore_ArtifactExecIrScalar_GetEncodedSize(
                    &module, &encodedSize, &artifactDiagnostic) ==
                    ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
            "scalar writer selected a wire version that drops typed mode");
    ok &= require_true(encodedSize == 0x12345678u &&
                               artifactDiagnostic.sectionKind ==
                                       ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR &&
                               artifactDiagnostic.byteOffset == 0u,
                       "rejected scalar size query published an encoded size");
    ok &= require_true(
            ZrCore_ArtifactExecIrScalar_Write(
                    &module, bytes, (TZrUInt32)sizeof(bytes), &artifactDiagnostic) ==
                    ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,
            "scalar writer accepted a typed empty table");
    for (index = 0u; index < (TZrUInt32)sizeof(bytes); ++index) {
        ok &= require_true(bytes[index] == 0xa5u,
                           "rejected scalar write modified destination bytes");
    }

    ZrCore_ExecIr_FreeModule(&module);
    ZrCore_ExecIr_FreeModule(&legacyModule);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
