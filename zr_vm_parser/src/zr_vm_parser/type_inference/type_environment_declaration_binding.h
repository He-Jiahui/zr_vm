#ifndef ZR_VM_PARSER_TYPE_ENVIRONMENT_DECLARATION_BINDING_H
#define ZR_VM_PARSER_TYPE_ENVIRONMENT_DECLARATION_BINDING_H

#include "zr_vm_parser/semantic.h"

/* A declaration is identified by its AST node in this semantic snapshot.
 * Names distinguish the explicit and implicit bindings owned by one node. */
static inline const SZrSemanticSymbolRecord *
type_environment_find_declaration_symbol(
        const SZrSemanticContext *context,
        const SZrAstNode *declaration,
        const SZrString *name,
        EZrSemanticSymbolKind kind) {
    if (context == ZR_NULL || declaration == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);
        if (symbol != ZR_NULL && symbol->astNode == declaration &&
            symbol->kind == kind && symbol->name != ZR_NULL &&
            ZrCore_String_Equal(symbol->name, (SZrString *)name)) {
            return symbol;
        }
    }
    return ZR_NULL;
}

#endif
