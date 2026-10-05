#ifndef ZR_VM_PARSER_COMPILER_SOURCE_CALLABLE_IDENTITY_H
#define ZR_VM_PARSER_COMPILER_SOURCE_CALLABLE_IDENTITY_H

#include "compiler_internal.h"

/* Optional proof for the actual single-literal return declaration. Called
 * inside the existing return-expression isolation, after expression lowering.
 * Unsupported or failed proof leaves no witness and adds no diagnostic. */
void compiler_source_callable_identity_try_publish_return(
        SZrCompilerState *cs, const SZrAstNode *returnNode, TZrUInt32 resultSlot);

/* Check the ordinary compiled return contract before retaining the value
 * witness. Context-local IDs are provenance, never persistent lookup handles. */
void compiler_source_callable_identity_finish(SZrCompilerState *cs);

#endif
