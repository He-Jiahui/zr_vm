#ifndef ZR_VM_PARSER_TYPE_DECLARATION_CONTEXT_H
#define ZR_VM_PARSER_TYPE_DECLARATION_CONTEXT_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/syntax_context.h"

static inline const SZrIdentifier *type_declaration_identifier(const SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    switch (node->type) {
        case ZR_AST_STRUCT_DECLARATION:
            return node->data.structDeclaration.name;
        case ZR_AST_CLASS_DECLARATION:
            return node->data.classDeclaration.name;
        case ZR_AST_INTERFACE_DECLARATION:
            return node->data.interfaceDeclaration.name;
        case ZR_AST_ENUM_DECLARATION:
            return node->data.enumDeclaration.name;
        case ZR_AST_UNION_DECLARATION:
            return node->data.unionDeclaration.name;
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            return node->data.externDelegateDeclaration.name;
        default:
            return ZR_NULL;
    }
}

static inline TZrBool type_declaration_raw_context(
        const SZrAstNode *node, SZrSyntaxContext *outContext) {
    const SZrIdentifier *identifier;

    if (outContext == ZR_NULL) {
        return ZR_FALSE;
    }
    if (node == ZR_NULL) {
        *outContext = ZrParser_SyntaxContext_Root();
        return ZR_TRUE;
    }
    identifier = type_declaration_identifier(node);
    if (identifier == ZR_NULL ||
        !ZrParser_SyntaxContext_IsValid(identifier->syntaxContext)) {
        return ZR_FALSE;
    }
    *outContext = identifier->syntaxContext;
    return ZR_TRUE;
}

static inline TZrBool type_declaration_normalize_context(
        SZrSyntaxContext context, SZrSyntaxContext *outContext) {
    if (outContext == ZR_NULL || !ZrParser_SyntaxContext_IsValid(context)) {
        return ZR_FALSE;
    }
    *outContext = ZrParser_SyntaxContext_Normalize(context, ZR_SYNTAX_NAMESPACE_ITEM);
    return ZrParser_SyntaxContext_IsValid(*outContext);
}

static inline TZrBool type_declaration_item_context(
        const SZrAstNode *node, SZrSyntaxContext *outContext) {
    SZrSyntaxContext context;

    return node != ZR_NULL && type_declaration_raw_context(node, &context) &&
           type_declaration_normalize_context(context, outContext);
}

#endif
