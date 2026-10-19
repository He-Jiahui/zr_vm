#ifndef ZR_VM_PARSER_TYPE_ENVIRONMENT_DECLARATION_BINDING_H
#define ZR_VM_PARSER_TYPE_ENVIRONMENT_DECLARATION_BINDING_H

#include "zr_vm_parser/semantic.h"

static inline SZrSyntaxContext type_environment_declaration_context(
        const SZrAstNode *declaration) {
    if (declaration == ZR_NULL) {
        return ZrParser_SyntaxContext_Root();
    }
    switch (declaration->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return declaration->data.functionDeclaration.name != ZR_NULL
                           ? declaration->data.functionDeclaration.name->syntaxContext
                           : ZrParser_SyntaxContext_Root();
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            return declaration->data.externFunctionDeclaration.name != ZR_NULL
                           ? declaration->data.externFunctionDeclaration.name->syntaxContext
                           : ZrParser_SyntaxContext_Root();
        case ZR_AST_PARAMETER:
            return declaration->data.parameter.name != ZR_NULL
                           ? declaration->data.parameter.name->syntaxContext
                           : ZrParser_SyntaxContext_Root();
        case ZR_AST_VARIABLE_DECLARATION:
            if (declaration->data.variableDeclaration.pattern != ZR_NULL &&
                declaration->data.variableDeclaration.pattern->type ==
                        ZR_AST_IDENTIFIER_LITERAL) {
                return declaration->data.variableDeclaration.pattern->data.identifier.syntaxContext;
            }
            return ZrParser_SyntaxContext_Root();
        default:
            return ZrParser_SyntaxContext_Root();
    }
}

/* A declaration is identified by its AST node in this semantic snapshot.
 * Names distinguish the explicit and implicit bindings owned by one node. */
static inline TZrBool type_environment_find_declaration_symbol(
        const SZrSemanticContext *context,
        const SZrAstNode *declaration,
        const SZrString *name,
        EZrSemanticSymbolKind kind,
        SZrSyntaxContext syntaxContext,
        const SZrSemanticSymbolRecord **outRecord) {
    SZrSyntaxContext normalizedContext;
    TZrBool hasNormalizedContext = ZR_FALSE;
    EZrSyntaxNamespace namespaceKind =
            (kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
             kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE)
                    ? ZR_SYNTAX_NAMESPACE_ITEM
                    : ZR_SYNTAX_NAMESPACE_LOCAL;
    if (outRecord == ZR_NULL) {
        return ZR_FALSE;
    }
    *outRecord = ZR_NULL;
    if (context == ZR_NULL || name == ZR_NULL ||
        !ZrParser_SyntaxContext_IsValid(syntaxContext)) {
        return ZR_FALSE;
    }
    if (declaration == ZR_NULL) {
        return ZR_TRUE;
    }
    /* A later raw hit must win before an earlier candidate spends scratch. */
    for (TZrSize index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);
        if (symbol != ZR_NULL && symbol->astNode == declaration &&
            symbol->kind == kind && symbol->name != ZR_NULL &&
            ZrCore_String_Equal(symbol->name, (SZrString *)name) &&
            ZrParser_SyntaxContext_IsValid(symbol->syntaxContext) &&
            ZrParser_SyntaxContext_Equal(symbol->syntaxContext, syntaxContext)) {
            *outRecord = symbol;
            return ZR_TRUE;
        }
    }
    for (TZrSize index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);
        SZrSyntaxContext normalizedCandidate;

        if (symbol == ZR_NULL || symbol->astNode != declaration ||
            symbol->kind != kind || symbol->name == ZR_NULL ||
            !ZrCore_String_Equal(symbol->name, (SZrString *)name)) {
            continue;
        }
        if (!hasNormalizedContext) {
            normalizedContext = ZrParser_SyntaxContext_Normalize(syntaxContext, namespaceKind);
            if (!ZrParser_SyntaxContext_IsValid(normalizedContext)) {
                return ZR_FALSE;
            }
            hasNormalizedContext = ZR_TRUE;
        }
        normalizedCandidate = ZrParser_SyntaxContext_Normalize(symbol->syntaxContext, namespaceKind);
        if (!ZrParser_SyntaxContext_IsValid(normalizedCandidate)) {
            return ZR_FALSE;
        }
        if (ZrParser_SyntaxContext_Equal(normalizedCandidate, normalizedContext)) {
            *outRecord = symbol;
            return ZR_TRUE;
        }
    }
    return ZR_TRUE;
}

#endif
