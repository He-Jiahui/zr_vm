#ifndef ZR_VM_PARSER_EXEC_IR_INTERNAL_H
#define ZR_VM_PARSER_EXEC_IR_INTERNAL_H

#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/semantic_ir.h"

typedef struct SZrParserExecIrNormalizedCfg {
    SZrSemanticIrFunction semantic;
    SZrParserCfgBlock *blocks;
    SZrParserCfgEdge *edges;
    TZrBool changed;
} SZrParserExecIrNormalizedCfg;

TZrBool zr_parser_exec_ir_has_typed_invoke_edges(
        const SZrSemanticIrFunction *semantic,
        const SZrParserCfgBlock *block);

TZrBool zr_parser_exec_ir_validate_cfg_edges(
        const SZrSemanticIrFunction *semantic,
        SZrExecIrFunction *output,
        SZrExecIrDiagnostic *diagnostic);

TZrBool zr_parser_exec_ir_normalize_exception_cfg(
        const SZrSemanticIrFunction *semantic,
        SZrParserExecIrNormalizedCfg *normalized,
        SZrExecIrDiagnostic *diagnostic);

void zr_parser_exec_ir_free_normalized_cfg(
        SZrParserExecIrNormalizedCfg *normalized);

TZrBool zr_parser_exec_ir_mark_place_values(
        const SZrSemanticIrFunction *semantic,
        SZrExecIrFunction *output,
        TZrExecIrValueId firstPlaceValue,
        SZrExecIrDiagnostic *diagnostic);

TZrBool zr_parser_exec_ir_promote_places(
        SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_EXEC_IR_INTERNAL_H */
