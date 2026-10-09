#ifndef ZR_VM_PARSER_EXEC_IR_SOURCE_CALLABLE_QUERY_INTERNAL_H
#define ZR_VM_PARSER_EXEC_IR_SOURCE_CALLABLE_QUERY_INTERNAL_H

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/canonical_type.h"

struct SZrSemanticContext;

/* Numeric current-snapshot facts only; no registry or node ownership. */
typedef struct SSourceCallableFacts {
    TZrTypeId callableTypeId;
    TZrTypeId returnTypeId;
    TZrUInt64 signatureHash;
    TZrUInt64 returnStructuralHash;
    EZrValueType returnPrimitive;
} SSourceCallableFacts;

/* Query current noargs/no-receiver/no-effects primitive INT64 field predicates.
 * Context, typed canonical nodes and selected parameter storage must be readable,
 * aligned, initialized, live, stable and serialized throughout the call. Output
 * is genuine writable storage. Detected overlap with a traversed input span must
 * be refused before publication; output independence from untraversed allocations
 * remains a caller precondition. Optional diagnostic is independent writable
 * storage disjoint from output and all context-owned reachable storage.
 * Numeric span checks establish
 * no arbitrary-pointer readability, source history or hash authenticity.
 * functionToken is only a supplied diagnostic scalar; zero is allowed.
 * Intended success publishes a zeroed numeric candidate and clears all nine
 * diagnostic fields; failure preserves output and context. No allocation/free,
 * lookup-lifetime retention, interning, hashing or semantic lifecycle occurs.
 * NULL context/output reports INVALID_ARGUMENT with token/payload zero.
 * Current RED staging: other calls report UNSUPPORTED with the supplied token
 * and all other diagnostic fields zero, without lookup or traversal.
 * Exact intended geometry/profile/diagnostic order is frozen in the callable
 * query design v1 and Root execution addendum; implementation awaits GREEN GO.
 */
TZrBool source_callable_query(const struct SZrSemanticContext *context, TZrTypeId callableTypeId,
                              TZrMetadataToken functionToken, SSourceCallableFacts *output,
                              SZrExecIrDiagnostic *diagnostic);

#endif
