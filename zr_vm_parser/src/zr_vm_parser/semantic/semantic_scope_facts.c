#include "semantic_scope_facts.h"

#include "zr_vm_parser/semantic_facts.h"

#include <string.h>

typedef struct SZrSemanticScopeFactBuilder {
    SZrSemanticContext *context;
    TZrUInt32 nextDeclarationOrder;
} SZrSemanticScopeFactBuilder;

static const SZrSemanticReferenceFact *semantic_scope_facts_find_declaration(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);

        if (fact != ZR_NULL && fact->node == node &&
            fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION && fact->isResolved &&
            fact->symbolId != ZR_SEMANTIC_ID_INVALID) {
            return fact;
        }
    }
    return ZR_NULL;
}

static const SZrSemanticSymbolRecord *semantic_scope_facts_find_type_declaration(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL || !context->symbols.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);

        if (symbol != ZR_NULL && symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE &&
            symbol->astNode == node && symbol->typeId != ZR_SEMANTIC_ID_INVALID) {
            return symbol;
        }
    }
    return ZR_NULL;
}

static const SZrSemanticSymbolRecord *semantic_scope_facts_find_symbol_by_node(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL || !context->symbols.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);

        if (symbol != ZR_NULL && symbol->astNode == node) {
            return symbol;
        }
    }
    return ZR_NULL;
}

static const SZrSemanticSymbolRecord *semantic_scope_facts_find_generic_parameter(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL || !context->symbols.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);

        if (symbol != ZR_NULL &&
            symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_PARAMETER &&
            symbol->astNode == node) {
            return symbol;
        }
    }
    return ZR_NULL;
}

static TZrSemanticScopeId semantic_scope_facts_publish_scope(
        SZrSemanticScopeFactBuilder *builder,
        TZrSemanticScopeId parentScopeId,
        EZrSemanticScopeKind kind,
        SZrFileRange range,
        TZrSymbolId ownerSymbolId) {
    SZrSemanticScopeFact fact;

    if (builder == ZR_NULL || builder->context == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    memset(&fact, 0, sizeof(fact));
    fact.parentScopeId = parentScopeId;
    fact.kind = kind;
    fact.range = range;
    fact.ownerSymbolId = ownerSymbolId;
    return ZrParser_Semantic_PublishScopeFact(builder->context, &fact);
}

static TZrBool semantic_scope_facts_publish_declaration(
        SZrSemanticScopeFactBuilder *builder,
        TZrSemanticScopeId scopeId,
        const SZrAstNode *node,
        TZrSymbolId ownerSymbolId,
        TZrBool isHoisted) {
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticSymbolRecord *symbol;
    SZrSemanticVisibleSymbolFact fact;

    if (builder == ZR_NULL || builder->context == ZR_NULL ||
        scopeId == ZR_SEMANTIC_ID_INVALID || node == ZR_NULL) {
        return ZR_FALSE;
    }
    reference = semantic_scope_facts_find_declaration(builder->context, node);
    if (reference == ZR_NULL) {
        return ZR_TRUE;
    }
    symbol = ZrParser_Semantic_FindSymbolById(builder->context, reference->symbolId);
    if (symbol == ZR_NULL) {
        return ZR_TRUE;
    }

    memset(&fact, 0, sizeof(fact));
    fact.scopeId = scopeId;
    fact.symbolId = reference->symbolId;
    fact.ownerSymbolId = ownerSymbolId;
    fact.access = ZR_ACCESS_PUBLIC;
    fact.declarationOrder = ++builder->nextDeclarationOrder;
    fact.declarationRange = reference->declarationRange;
    fact.definitionRange = reference->definitionRange;
    fact.hasDefinitionRange = reference->hasDefinitionRange;
    fact.signatureDisplay = reference->signatureDisplay;
    fact.isHoisted = isHoisted;
    fact.isAccessible = ZR_TRUE;
    return ZrParser_Semantic_PublishVisibleSymbolFact(builder->context, &fact);
}

static TZrBool semantic_scope_facts_publish_type_declaration(
        SZrSemanticScopeFactBuilder *builder,
        TZrSemanticScopeId scopeId,
        const SZrAstNode *node) {
    const SZrSemanticSymbolRecord *symbol;
    SZrSemanticVisibleSymbolFact fact;

    if (builder == ZR_NULL || builder->context == ZR_NULL ||
        scopeId == ZR_SEMANTIC_ID_INVALID || node == ZR_NULL) {
        return ZR_FALSE;
    }
    symbol = semantic_scope_facts_find_type_declaration(builder->context, node);
    if (symbol == ZR_NULL) {
        return ZR_TRUE;
    }

    memset(&fact, 0, sizeof(fact));
    fact.scopeId = scopeId;
    fact.symbolId = symbol->id;
    fact.access = ZR_ACCESS_PUBLIC;
    fact.declarationOrder = ++builder->nextDeclarationOrder;
    fact.declarationRange = symbol->location;
    fact.definitionRange = symbol->location;
    fact.hasDefinitionRange = ZR_TRUE;
    fact.isHoisted = ZR_TRUE;
    fact.isAccessible = ZR_TRUE;
    return ZrParser_Semantic_PublishVisibleSymbolFact(builder->context, &fact);
}

static TZrBool semantic_scope_facts_publish_generic_parameter(
        SZrSemanticScopeFactBuilder *builder,
        TZrSemanticScopeId scopeId,
        TZrSymbolId ownerSymbolId,
        SZrAstNode *node,
        TZrUInt32 ordinal) {
    const SZrSemanticSymbolRecord *symbol;
    SZrParameter *parameter;
    SZrSemanticVisibleSymbolFact fact;
    TZrTypeId typeId;
    TZrSymbolId symbolId;

    if (builder == ZR_NULL || builder->context == ZR_NULL ||
        scopeId == ZR_SEMANTIC_ID_INVALID ||
        ownerSymbolId == ZR_SEMANTIC_ID_INVALID ||
        node == ZR_NULL || node->type != ZR_AST_PARAMETER) {
        return ZR_FALSE;
    }
    parameter = &node->data.parameter;
    if (parameter->genericKind != ZR_GENERIC_PARAMETER_TYPE ||
        parameter->name == ZR_NULL || parameter->name->name == ZR_NULL) {
        return ZR_TRUE;
    }

    typeId = ZrParser_CanonicalType_InternGenericParameter(
            builder->context, ownerSymbolId, ordinal);
    if (typeId == ZR_SEMANTIC_ID_INVALID ||
        !ZrParser_Semantic_RegisterCanonicalType(
                builder->context,
                typeId,
                ZR_SEMANTIC_TYPE_KIND_GENERIC_PARAMETER,
                parameter->name->name,
                node)) {
        return ZR_FALSE;
    }
    symbol = semantic_scope_facts_find_generic_parameter(builder->context, node);
    if (symbol != ZR_NULL) {
        if (symbol->typeId != typeId) {
            return ZR_FALSE;
        }
        symbolId = symbol->id;
    } else {
        symbolId = ZrParser_Semantic_RegisterSymbol(
                builder->context,
                parameter->name->name,
                ZR_SEMANTIC_SYMBOL_KIND_PARAMETER,
                typeId,
                ZR_SEMANTIC_ID_INVALID,
                node,
                parameter->nameLocation);
        if (symbolId == ZR_SEMANTIC_ID_INVALID) {
            return ZR_FALSE;
        }
    }

    memset(&fact, 0, sizeof(fact));
    fact.scopeId = scopeId;
    fact.symbolId = symbolId;
    fact.ownerSymbolId = ownerSymbolId;
    fact.access = ZR_ACCESS_PUBLIC;
    fact.declarationOrder = ++builder->nextDeclarationOrder;
    fact.declarationRange = parameter->nameLocation;
    fact.definitionRange = parameter->nameLocation;
    fact.hasDefinitionRange = ZR_TRUE;
    fact.isAccessible = ZR_TRUE;
    fact.isGenericParameter = ZR_TRUE;
    return ZrParser_Semantic_PublishVisibleSymbolFact(builder->context, &fact);
}

static TZrBool semantic_scope_facts_publish_generic_parameters(
        SZrSemanticScopeFactBuilder *builder,
        TZrSemanticScopeId scopeId,
        TZrSymbolId ownerSymbolId,
        SZrGenericDeclaration *generic) {
    TZrSize index;

    if (generic == ZR_NULL || generic->params == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < generic->params->count; index++) {
        if (!semantic_scope_facts_publish_generic_parameter(
                    builder,
                    scopeId,
                    ownerSymbolId,
                    generic->params->nodes[index],
                    (TZrUInt32)index)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool semantic_scope_facts_visit_node(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId ownerSymbolId);

static TZrBool semantic_scope_facts_visit_nodes(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNodeArray *nodes,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId ownerSymbolId) {
    TZrSize index;

    if (nodes == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < nodes->count; index++) {
        if (!semantic_scope_facts_visit_node(
                    builder, nodes->nodes[index], parentScopeId, ownerSymbolId)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool semantic_scope_facts_visit_block(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId ownerSymbolId) {
    TZrSemanticScopeId scopeId;

    scopeId = semantic_scope_facts_publish_scope(
            builder, parentScopeId, ZR_SEMANTIC_SCOPE_KIND_BLOCK, node->location, ownerSymbolId);
    return scopeId != ZR_SEMANTIC_ID_INVALID &&
           semantic_scope_facts_visit_nodes(
                   builder, node->data.block.body, scopeId, ownerSymbolId);
}

static TZrBool semantic_scope_facts_visit_function(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId enclosingOwnerSymbolId) {
    const SZrSemanticReferenceFact *reference;
    TZrSymbolId ownerSymbolId = ZR_SEMANTIC_ID_INVALID;
    TZrSemanticScopeId scopeId;
    SZrFunctionDeclaration *declaration = &node->data.functionDeclaration;
    TZrSize index;

    if (!semantic_scope_facts_publish_declaration(
                builder, parentScopeId, node, enclosingOwnerSymbolId, ZR_TRUE)) {
        return ZR_FALSE;
    }
    reference = semantic_scope_facts_find_declaration(builder->context, node);
    if (reference != ZR_NULL) {
        ownerSymbolId = reference->symbolId;
    }
    scopeId = semantic_scope_facts_publish_scope(
            builder, parentScopeId, ZR_SEMANTIC_SCOPE_KIND_FUNCTION, node->location, ownerSymbolId);
    if (scopeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    if (ownerSymbolId != ZR_SEMANTIC_ID_INVALID &&
        !semantic_scope_facts_publish_generic_parameters(
                builder, scopeId, ownerSymbolId, declaration->generic)) {
        return ZR_FALSE;
    }
    if (declaration->params != ZR_NULL) {
        for (index = 0U; index < declaration->params->count; index++) {
            if (!semantic_scope_facts_publish_declaration(
                        builder,
                        scopeId,
                        declaration->params->nodes[index],
                        ownerSymbolId,
                        ZR_FALSE)) {
                return ZR_FALSE;
            }
        }
    }
    return declaration->body == ZR_NULL ||
           semantic_scope_facts_visit_node(builder, declaration->body, scopeId, ownerSymbolId);
}

static TZrBool semantic_scope_facts_visit_struct_method(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId) {
    const SZrSemanticSymbolRecord *symbol;
    SZrStructMethod *method;
    TZrSemanticScopeId scopeId;
    TZrSize index;

    if (node == ZR_NULL || node->type != ZR_AST_STRUCT_METHOD) {
        return ZR_FALSE;
    }
    symbol = semantic_scope_facts_find_symbol_by_node(builder->context, node);
    if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        symbol->id == ZR_SEMANTIC_ID_INVALID) {
        return ZR_TRUE;
    }
    method = &node->data.structMethod;
    scopeId = semantic_scope_facts_publish_scope(
            builder,
            parentScopeId,
            ZR_SEMANTIC_SCOPE_KIND_FUNCTION,
            node->location,
            symbol->id);
    if (scopeId == ZR_SEMANTIC_ID_INVALID ||
        !semantic_scope_facts_publish_generic_parameters(
                builder, scopeId, symbol->id, method->generic)) {
        return ZR_FALSE;
    }
    if (method->params != ZR_NULL) {
        for (index = 0U; index < method->params->count; index++) {
            if (!semantic_scope_facts_publish_declaration(
                        builder,
                        scopeId,
                        method->params->nodes[index],
                        symbol->id,
                        ZR_FALSE)) {
                return ZR_FALSE;
            }
        }
    }
    return method->body == ZR_NULL ||
           semantic_scope_facts_visit_node(builder, method->body, scopeId, symbol->id);
}

static TZrBool semantic_scope_facts_visit_type(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId) {
    const SZrSemanticSymbolRecord *symbol;
    SZrGenericDeclaration *generic = ZR_NULL;
    SZrAstNodeArray *members = ZR_NULL;
    TZrSemanticScopeId scopeId;

    if (!semantic_scope_facts_publish_type_declaration(builder, parentScopeId, node)) {
        return ZR_FALSE;
    }
    symbol = semantic_scope_facts_find_type_declaration(builder->context, node);
    if (symbol == ZR_NULL) {
        return ZR_TRUE;
    }
    scopeId = semantic_scope_facts_publish_scope(
            builder,
            parentScopeId,
            ZR_SEMANTIC_SCOPE_KIND_TYPE,
            node->location,
            symbol->id);
    switch (node->type) {
        case ZR_AST_STRUCT_DECLARATION:
            generic = node->data.structDeclaration.generic;
            members = node->data.structDeclaration.members;
            break;
        case ZR_AST_CLASS_DECLARATION:
            generic = node->data.classDeclaration.generic;
            members = node->data.classDeclaration.members;
            break;
        case ZR_AST_INTERFACE_DECLARATION:
            generic = node->data.interfaceDeclaration.generic;
            members = node->data.interfaceDeclaration.members;
            break;
        default:
            return ZR_FALSE;
    }
    return scopeId != ZR_SEMANTIC_ID_INVALID &&
           semantic_scope_facts_publish_generic_parameters(
                   builder, scopeId, symbol->id, generic) &&
           semantic_scope_facts_visit_nodes(builder, members, scopeId, symbol->id);
}

static SZrFileRange semantic_scope_facts_loop_range(
        const SZrAstNode *node,
        const SZrAstNode *body) {
    SZrFileRange range = node->location;

    if (body != ZR_NULL) {
        range.end = body->location.end;
    }
    return range;
}

static TZrBool semantic_scope_facts_visit_for_loop(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId ownerSymbolId) {
    TZrSemanticScopeId scopeId = semantic_scope_facts_publish_scope(
            builder,
            parentScopeId,
            ZR_SEMANTIC_SCOPE_KIND_BLOCK,
            semantic_scope_facts_loop_range(node, node->data.forLoop.block),
            ownerSymbolId);

    return scopeId != ZR_SEMANTIC_ID_INVALID &&
           semantic_scope_facts_visit_node(
                   builder, node->data.forLoop.init, scopeId, ownerSymbolId) &&
           semantic_scope_facts_visit_node(
                   builder, node->data.forLoop.block, scopeId, ownerSymbolId);
}

static TZrBool semantic_scope_facts_visit_foreach_loop(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId ownerSymbolId) {
    TZrSemanticScopeId scopeId = semantic_scope_facts_publish_scope(
            builder,
            parentScopeId,
            ZR_SEMANTIC_SCOPE_KIND_BLOCK,
            semantic_scope_facts_loop_range(node, node->data.foreachLoop.block),
            ownerSymbolId);

    return scopeId != ZR_SEMANTIC_ID_INVALID &&
           semantic_scope_facts_publish_declaration(
                   builder, scopeId, node->data.foreachLoop.pattern, ownerSymbolId, ZR_FALSE) &&
           semantic_scope_facts_visit_node(
                   builder, node->data.foreachLoop.block, scopeId, ownerSymbolId);
}

static TZrBool semantic_scope_facts_visit_node(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId ownerSymbolId) {
    if (builder == ZR_NULL || builder->context == ZR_NULL || node == ZR_NULL) {
        return builder != ZR_NULL && builder->context != ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return semantic_scope_facts_visit_function(
                    builder, node, parentScopeId, ownerSymbolId);
        case ZR_AST_STRUCT_DECLARATION:
        case ZR_AST_CLASS_DECLARATION:
        case ZR_AST_INTERFACE_DECLARATION:
            return semantic_scope_facts_visit_type(builder, node, parentScopeId);
        case ZR_AST_STRUCT_METHOD:
            return semantic_scope_facts_visit_struct_method(builder, node, parentScopeId);
        case ZR_AST_VARIABLE_DECLARATION:
            return semantic_scope_facts_publish_declaration(
                    builder, parentScopeId, node, ownerSymbolId, ZR_FALSE);
        case ZR_AST_BLOCK:
            return semantic_scope_facts_visit_block(builder, node, parentScopeId, ownerSymbolId);
        case ZR_AST_IF_EXPRESSION:
            return semantic_scope_facts_visit_node(
                           builder,
                           node->data.ifExpression.thenExpr,
                           parentScopeId,
                           ownerSymbolId) &&
                   semantic_scope_facts_visit_node(
                           builder,
                           node->data.ifExpression.elseExpr,
                           parentScopeId,
                           ownerSymbolId);
        case ZR_AST_WHILE_LOOP:
            return semantic_scope_facts_visit_node(
                    builder, node->data.whileLoop.block, parentScopeId, ownerSymbolId);
        case ZR_AST_FOR_LOOP:
            return semantic_scope_facts_visit_for_loop(
                    builder, node, parentScopeId, ownerSymbolId);
        case ZR_AST_FOREACH_LOOP:
            return semantic_scope_facts_visit_foreach_loop(
                    builder, node, parentScopeId, ownerSymbolId);
        case ZR_AST_SWITCH_EXPRESSION: {
            TZrSize index;
            SZrSwitchExpression *switchExpression = &node->data.switchExpression;

            if (switchExpression->cases != ZR_NULL) {
                for (index = 0U; index < switchExpression->cases->count; index++) {
                    if (!semantic_scope_facts_visit_node(
                                builder,
                                switchExpression->cases->nodes[index],
                                parentScopeId,
                                ownerSymbolId)) {
                        return ZR_FALSE;
                    }
                }
            }
            return semantic_scope_facts_visit_node(
                    builder, switchExpression->defaultCase, parentScopeId, ownerSymbolId);
        }
        case ZR_AST_SWITCH_CASE:
            return semantic_scope_facts_visit_node(
                    builder, node->data.switchCase.block, parentScopeId, ownerSymbolId);
        case ZR_AST_SWITCH_DEFAULT:
            return semantic_scope_facts_visit_node(
                    builder, node->data.switchDefault.block, parentScopeId, ownerSymbolId);
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT: {
            SZrTryCatchFinallyStatement *statement = &node->data.tryCatchFinallyStatement;

            return semantic_scope_facts_visit_node(
                           builder, statement->block, parentScopeId, ownerSymbolId) &&
                   semantic_scope_facts_visit_nodes(
                           builder, statement->catchClauses, parentScopeId, ownerSymbolId) &&
                   semantic_scope_facts_visit_node(
                           builder, statement->finallyBlock, parentScopeId, ownerSymbolId);
        }
        case ZR_AST_CATCH_CLAUSE:
            return semantic_scope_facts_visit_node(
                    builder, node->data.catchClause.block, parentScopeId, ownerSymbolId);
        case ZR_AST_USING_STATEMENT:
            return semantic_scope_facts_visit_node(
                           builder, node->data.usingStatement.body, parentScopeId, ownerSymbolId) &&
                   semantic_scope_facts_visit_node(
                           builder, node->data.usingStatement.elseBody, parentScopeId, ownerSymbolId);
        default:
            return ZR_TRUE;
    }
}

TZrBool ZrParser_Semantic_BuildSourceScopeFacts(
        SZrSemanticContext *context,
        SZrAstNode *root) {
    SZrSemanticScopeFactBuilder builder;
    TZrSemanticScopeId moduleScopeId;

    if (context == ZR_NULL || root == ZR_NULL || root->type != ZR_AST_SCRIPT) {
        return ZR_FALSE;
    }
    memset(&builder, 0, sizeof(builder));
    builder.context = context;
    moduleScopeId = semantic_scope_facts_publish_scope(
            &builder,
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_SCOPE_KIND_MODULE,
            root->location,
            ZR_SEMANTIC_ID_INVALID);
    return moduleScopeId != ZR_SEMANTIC_ID_INVALID &&
           semantic_scope_facts_visit_nodes(
                   &builder,
                   root->data.script.statements,
                   moduleScopeId,
                   ZR_SEMANTIC_ID_INVALID);
}
