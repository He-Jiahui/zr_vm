#ifndef ZR_VM_PARSER_COMPILER_SCRIPT_CALLABLE_IDENTITY_H
#define ZR_VM_PARSER_COMPILER_SCRIPT_CALLABLE_IDENTITY_H

#include "compiler_internal.h"

/* Optional identity for an actual pure, versionless SCRIPT with one explicit
 * integer literal return. The return TypeId must come from the existing
 * validated reachable-CFG proof. Unsupported or incomplete state publishes
 * nothing and does not turn a legal source into an ordinary compile error.
 * Only scalar IDs/hash/range coordinates survive the semantic context. */
void compiler_script_callable_identity_try_publish(
        SZrCompilerState *cs, SZrFunction *function, TZrTypeId returnTypeId);

#endif
