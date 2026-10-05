#ifndef ZR_VM_PARSER_EXEC_IR_HOST_PRIMITIVE_LAYOUT_H
#define ZR_VM_PARSER_EXEC_IR_HOST_PRIMITIVE_LAYOUT_H

#include "zr_vm_parser/semantic.h"
#include "zr_vm_core/exec_ir.h"

/* Construct an independent row for the current host ABI. The caller supplies a
 * typeId from the same live semantic context and a nonzero layoutId; the output
 * must not alias context storage. This first slice accepts only actual canonical
 * PRIMITIVE INT64 nodes. It does not establish a target ABI or append to a module.
 * Failure preserves every output byte; diagnostic is optional. The stable hash
 * covers schema 1, primitive valueType, canonical structuralHash, host size and
 * alignment, CHAR_BIT and endianness, excluding layoutId and context addresses.
 */
ZR_PARSER_API TZrBool ZrParser_ExecIr_MakeHostPrimitiveLayout(
        const SZrSemanticContext *context, TZrTypeId typeId, TZrUInt32 layoutId,
        SZrExecIrLayout *output, SZrExecIrDiagnostic *diagnostic);

#endif
