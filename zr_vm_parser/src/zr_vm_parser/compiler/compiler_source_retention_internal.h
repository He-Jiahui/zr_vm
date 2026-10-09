#ifndef ZR_VM_PARSER_COMPILER_SOURCE_RETENTION_INTERNAL_H
#define ZR_VM_PARSER_COMPILER_SOURCE_RETENTION_INTERNAL_H

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/compiler_source_retention.h"

/* Called only for REQUIRE after finalized call bindings and before publication,
 * while CompilerState, selected Function, AST/SemIR/context and source identities
 * are live. The active caller owns Function/CompilerState cleanup after refusal;
 * the ordinary source wrapper owns AST identity cleanup and AST destruction.
 * Diagnostic is optional live writable storage independent of input owners.
 * RED stub resets it completely, reports COMPILE/INVALID_ARGUMENT for NULL
 * compiler/selected and COMPILE/UNSUPPORTED otherwise, and returns false without
 * reading or modifying input owners, allocating, freeing or attaching an owner.
 * All lower/range flags and payloads remain zero; no producer stage is fabricated.
 */
TZrBool compiler_retain_source_scalar_aot(SZrCompilerState *cs, SZrFunction *selected,
        SZrSourceCompileDiagnostic *diagnostic);

#endif
