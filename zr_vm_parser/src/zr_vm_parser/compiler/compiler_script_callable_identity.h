#ifndef ZR_VM_PARSER_COMPILER_SCRIPT_CALLABLE_IDENTITY_H
#define ZR_VM_PARSER_COMPILER_SCRIPT_CALLABLE_IDENTITY_H

#include "compiler_internal.h"

/* Optional identity for an actual pure, versionless SCRIPT with one explicit
 * I64 literal or pure I64 +,-,* expression return. The return TypeId comes from
 * validated reachable-CFG proof. Unsupported or incomplete state publishes
 * nothing and does not turn a legal source into an ordinary compile error.
 * The binary identity candidate is nonallocating and checks actual validated
 * SemIR; it does not replace per-call AST/pool/ExecIR retention certification.
 * Only scalar IDs/hash/range coordinates survive the semantic context. */
void compiler_script_callable_identity_try_publish(
        SZrCompilerState *cs, SZrFunction *function, TZrTypeId returnTypeId);

#endif
