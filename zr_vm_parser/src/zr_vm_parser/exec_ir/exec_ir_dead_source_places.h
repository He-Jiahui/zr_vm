#ifndef ZR_VM_PARSER_EXEC_IR_DEAD_SOURCE_PLACES_H
#define ZR_VM_PARSER_EXEC_IR_DEAD_SOURCE_PLACES_H

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_ir.h"

/* Eliminate only proved, unused source temporary PLACE_BASE addresses and
 * their exclusive builder provenance. Instruction/source identities and empty
 * state-map identity headers survive; surviving values and pools become dense.
 * Borrowed inputs remain unchanged. Output must be initialized, independently
 * owned storage: aliasing input is invalid. Failure preserves existing output.
 * This slice admits only a verified pure integer literal SCRIPT, or its exact
 * previously eliminated NOP form (idempotent). Richer places, uses and metadata
 * are explicitly unsupported; sealed inputs report SEALED. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_EliminateDeadSourcePlaces(
        const SZrSemanticIrFunction *semantic,
        const SZrSemanticContext *context,
        const SZrExecIrFunction *input,
        SZrExecIrFunction *output,
        SZrExecIrDiagnostic *diagnostic);

#endif
