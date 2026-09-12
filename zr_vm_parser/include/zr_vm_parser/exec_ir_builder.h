#ifndef ZR_VM_PARSER_EXEC_IR_BUILDER_H
#define ZR_VM_PARSER_EXEC_IR_BUILDER_H

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"

struct SZrSemanticIrFunction;

typedef struct SZrExecIrBuildOptions {
    TZrBool enableOptimization;
    TZrBool preserveSourceMaps;
    TZrBool preserveDeoptStates;
    TZrBool rejectMissingFacts;
} SZrExecIrBuildOptions;

typedef struct SZrExecIrBuildInput {
    const struct SZrSemanticIrFunction *semanticFunction;
    TZrMetadataToken functionToken;
    TZrUInt64 signatureHash;
    SZrExecIrBuildOptions options;
} SZrExecIrBuildInput;

ZR_PARSER_API TZrBool ZrParser_ExecIr_Build(
        const struct SZrSemanticIrFunction *semanticFunction,
        const SZrExecIrBuildOptions *options,
        SZrExecIrFunction *output,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildModule(
        const SZrExecIrBuildInput *input,
        SZrExecIrModule *output,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildSsa(
        SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_VerifyFunction(
        const SZrExecIrFunction *function,
        EZrExecIrVerifyLevel level,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_Verify(
        const SZrExecIrModule *module,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ComputeDominators(
        SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic);

#endif
