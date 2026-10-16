#ifndef ZR_VM_PARSER_COMPILER_SOURCE_RETENTION_INTERNAL_H
#define ZR_VM_PARSER_COMPILER_SOURCE_RETENTION_INTERNAL_H

#include "zr_vm_parser/compiler.h"
#include "compiler/semantic/compiler_source_retention.h"

/* Called only for REQUIRE after finalized call bindings and before publication,
 * while CompilerState, selected Function, AST/SemIR/context and source identities
 * are live. The active caller owns Function/CompilerState cleanup after refusal;
 * the ordinary source wrapper owns AST identity cleanup and AST destruction.
 * Diagnostic is optional live writable storage independent of input owners.
 * The helper validates actual source metadata, owns every transient producer,
 * and publishes AOT only after its producer validation. ExecIR publication follows
 * genuine frame attachment and verification, then finalizer witness validation
 * and deep ownership cloning of whole module storage with the selected Function
 * replaced by its finalized output. Numeric canonical IDs do not own or extend
 * registry lifetime or authorize execution. Only exact AOT/ExecIR REQUIRE modes
 * and unattached owners are accepted.
 * Failure preserves the actual lower diagnostic and any corresponding live
 * numeric source range; transient cleanup precedes the caller's destruction.
 */
TZrBool compiler_retain_source_scalar_metadata(SZrCompilerState *cs,
        SZrFunction *selected, EZrSourceRetention mode,
        SZrSourceCompileDiagnostic *diagnostic);

#endif
