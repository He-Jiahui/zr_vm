#include "compiler_type_declaration.h"

#include "semantic/type_declaration_context.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/syntax_context.h"

TZrBool compiler_type_declaration_item_context(const SZrAstNode *declaration,
        SZrSyntaxContext *outContext) {
    return type_declaration_item_context(declaration, outContext);
}

static SZrAstNode *find_in_array(SZrAstNodeArray *declarations,
        SZrString *typeName, SZrSyntaxContext normalizedContext) {
    if (declarations == ZR_NULL || declarations->nodes == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < declarations->count; index++) {
        SZrAstNode *declaration = declarations->nodes[index];
        const SZrIdentifier *name = type_declaration_identifier(declaration);
        SZrSyntaxContext declarationContext;

        if (name != ZR_NULL && name->name != ZR_NULL &&
            ZrCore_String_Equal(name->name, typeName) &&
            compiler_type_declaration_item_context(declaration, &declarationContext) &&
            ZrParser_SyntaxContext_Equal(normalizedContext, declarationContext)) {
            return declaration;
        }
    }
    return ZR_NULL;
}

SZrAstNode *find_type_declaration_with_context(SZrCompilerState *cs,
        SZrString *typeName, SZrSyntaxContext syntaxContext) {
    SZrAstNodeArray *statements;
    SZrAstNode *match;
    SZrSyntaxContext normalized;

    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL || typeName == ZR_NULL ||
        cs->scriptAst->type != ZR_AST_SCRIPT ||
        !ZrParser_SyntaxContext_IsValid(syntaxContext)) {
        return ZR_NULL;
    }
    statements = cs->scriptAst->data.script.statements;
    if (statements == ZR_NULL || statements->nodes == ZR_NULL) {
        return ZR_NULL;
    }
    normalized = ZrParser_SyntaxContext_Normalize(syntaxContext, ZR_SYNTAX_NAMESPACE_ITEM);
    match = find_in_array(statements, typeName, normalized);
    if (match != ZR_NULL) {
        return match;
    }
    for (TZrSize index = 0U; index < statements->count; index++) {
        SZrAstNode *statement = statements->nodes[index];

        if (statement != ZR_NULL && statement->type == ZR_AST_EXTERN_BLOCK) {
            match = find_in_array(statement->data.externBlock.declarations,
                    typeName, normalized);
            if (match != ZR_NULL) {
                return match;
            }
        }
    }
    return ZR_NULL;
}

SZrAstNode *find_type_declaration(SZrCompilerState *cs, SZrString *typeName) {
    return find_type_declaration_with_context(cs, typeName, ZrParser_SyntaxContext_Root());
}
