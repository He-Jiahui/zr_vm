#ifndef ZR_VM_PARSER_EXEC_IR_HOST_PRIMITIVE_LAYOUT_H
#define ZR_VM_PARSER_EXEC_IR_HOST_PRIMITIVE_LAYOUT_H

#include "zr_vm_parser/semantic.h"
#include "zr_vm_core/exec_ir.h"

/* Construct an independent row for current host INT64 primitive storage.
 * The caller supplies a typeId from the same live semantic context and a nonzero
 * layoutId representing its destination table identity. The context must remain
 * a current interner snapshot with sorted node IDs and actual readable canonical
 * node allocation; shape and address-span checks do not prove memory readability.
 * Context storage, writable output storage, and optional writable diagnostic
 * storage must be independently valid, nonoverlapping, and live for this call.
 * Only actual canonical PRIMITIVE INT64 nodes are accepted. This does not establish
 * a complete ABI or provenance, append to a module, or bind a frame layout hash.
 * Failure preserves every output byte; context is never changed. Diagnostic may
 * be cleared at entry. Success publishes a completely initialized row.
 * Stable64 hashes the 31 ASCII bytes "zr.execir.host.primitive-layout" without
 * NUL, followed by explicit little-endian schema 1 (u32), primitive valueType
 * (u32), canonical structuralHash (u64), host size (u32), alignment (u32), CHAR_BIT
 * (u32), and byte order (u32: little=0, big=1). Mixed byte order is unsupported.
 * The actual primitive structural hash excludes context-local typeId, addresses,
 * salt and time; layoutId and typeId are not additional inputs to this hash.
 */
ZR_PARSER_API TZrBool ZrParser_ExecIr_MakeHostPrimitiveLayout(
        const SZrSemanticContext *context, TZrTypeId typeId, TZrUInt32 layoutId,
        SZrExecIrLayout *output, SZrExecIrDiagnostic *diagnostic);

#endif
