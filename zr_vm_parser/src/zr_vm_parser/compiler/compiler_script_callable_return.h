#ifndef ZR_VM_PARSER_COMPILER_SCRIPT_CALLABLE_RETURN_H
#define ZR_VM_PARSER_COMPILER_SCRIPT_CALLABLE_RETURN_H

#include "compiler_internal.h"

/* After source-module finalization, certify only the actual pure, noargs script
 * entry whose validated, finite reachable CFG returns canonical INT64 on every
 * path. Unsupported sources succeed without publication. Malformed admitted
 * state or scratch allocation failure returns false for the normal error path.
 * No graph, frame, instruction or callable identity is retained or rewritten.
 * The optional output is cleared first and receives the actual canonical
 * return TypeId only when the reachable CFG proof and publication succeed. */
TZrBool compiler_script_callable_return_publish(
        SZrCompilerState *cs, SZrFunction *function, const SZrAstNode *script,
        TZrTypeId *outProvenReturnTypeId);

#endif
