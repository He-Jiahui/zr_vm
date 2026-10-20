#include "type_inference/dataflow/semantic_da_ast.h"

#include "zr_vm_core/memory.h"

#include <stdint.h>
#include <stdlib.h>

struct SZrSemanticDaAstEntry {
    SZrAstNode *node;
    SZrAstNode *executionOwner;
    TZrSize begin;
    TZrSize end;
};

typedef struct SZrSemanticDaAstBuilder {
    SZrSemanticDaAstEntry *entries;
    TZrSize capacity;
    TZrSize count;
} SZrSemanticDaAstBuilder;

static TZrBool semantic_da_ast_walk(SZrSemanticDaAstBuilder *builder,
                                    SZrAstNode *node,
                                    SZrAstNode *owner);

static TZrBool semantic_da_ast_walk_array(SZrSemanticDaAstBuilder *builder,
                                          SZrAstNodeArray *array,
                                          SZrAstNode *owner) {
    TZrSize index;

    if (array == ZR_NULL) {
        return ZR_TRUE;
    }
    if (array->count != 0 && array->nodes == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < array->count; index++) {
        if (!semantic_da_ast_walk(builder, array->nodes[index], owner)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool semantic_da_ast_walk_type(SZrSemanticDaAstBuilder *builder,
                                         SZrType *type,
                                         SZrAstNode *owner) {
    return type == ZR_NULL ||
           (semantic_da_ast_walk(builder, type->name, owner) &&
            semantic_da_ast_walk_type(builder, type->subType, owner) &&
            semantic_da_ast_walk(builder, type->arraySizeExpression, owner));
}

static TZrBool semantic_da_ast_walk_identifier(SZrSemanticDaAstBuilder *builder,
                                               SZrIdentifier *identifier,
                                               SZrAstNode *owner) {
    SZrAstNode *node;

    if (identifier == ZR_NULL) {
        return ZR_TRUE;
    }
    node = (SZrAstNode *)((char *)identifier - offsetof(SZrAstNode, data.identifier));
    return node->type != ZR_AST_IDENTIFIER_LITERAL ||
           semantic_da_ast_walk(builder, node, owner);
}

static TZrBool semantic_da_ast_walk_parameter(SZrSemanticDaAstBuilder *builder,
                                              SZrParameter *parameter,
                                              SZrAstNode *owner) {
    SZrAstNode *node;

    if (parameter == ZR_NULL) {
        return ZR_TRUE;
    }
    node = (SZrAstNode *)((char *)parameter - offsetof(SZrAstNode, data.parameter));
    return node->type != ZR_AST_PARAMETER || semantic_da_ast_walk(builder, node, owner);
}

static TZrBool semantic_da_ast_walk_generic(SZrSemanticDaAstBuilder *builder,
                                            SZrGenericDeclaration *generic,
                                            SZrAstNode *owner) {
    return generic == ZR_NULL || semantic_da_ast_walk_array(builder, generic->params, owner);
}

static TZrBool semantic_da_ast_is_callable(SZrAstNode *node) {
    switch (node->type) {
        case ZR_AST_FUNCTION_DECLARATION:
        case ZR_AST_LAMBDA_EXPRESSION:
        case ZR_AST_STRUCT_METHOD:
        case ZR_AST_STRUCT_META_FUNCTION:
        case ZR_AST_CLASS_METHOD:
        case ZR_AST_CLASS_META_FUNCTION:
        case ZR_AST_PROPERTY_ACCESSOR:
        case ZR_AST_PROPERTY_GET:
        case ZR_AST_PROPERTY_SET:
        case ZR_AST_GENERATOR_EXPRESSION:
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
        case ZR_AST_INTERFACE_META_SIGNATURE:
        case ZR_AST_FUNCTION_TYPE:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

/* Follow only the owning child edges used by parser_ast_free.c. */
static TZrBool semantic_da_ast_walk_children(SZrSemanticDaAstBuilder *builder,
                                             SZrAstNode *node,
                                             SZrAstNode *owner) {
#define CHILD(FIELD) do { if (!semantic_da_ast_walk(builder, node->data.FIELD, owner)) return ZR_FALSE; } while (0)
#define ARRAY(FIELD) do { if (!semantic_da_ast_walk_array(builder, node->data.FIELD, owner)) return ZR_FALSE; } while (0)
#define TYPE(FIELD) do { if (!semantic_da_ast_walk_type(builder, node->data.FIELD, owner)) return ZR_FALSE; } while (0)
#define IDENTIFIER(FIELD) do { if (!semantic_da_ast_walk_identifier(builder, node->data.FIELD, owner)) return ZR_FALSE; } while (0)
#define PARAMETER(FIELD) do { if (!semantic_da_ast_walk_parameter(builder, node->data.FIELD, owner)) return ZR_FALSE; } while (0)
#define GENERIC(FIELD) do { if (!semantic_da_ast_walk_generic(builder, node->data.FIELD, owner)) return ZR_FALSE; } while (0)
    switch (node->type) {
        case ZR_AST_SCRIPT:
            CHILD(script.moduleName);
            ARRAY(script.statements);
            break;
        case ZR_AST_MODULE_DECLARATION:
            CHILD(moduleDeclaration.name);
            break;
        case ZR_AST_FUNCTION_DECLARATION:
            IDENTIFIER(functionDeclaration.name);
            GENERIC(functionDeclaration.generic);
            ARRAY(functionDeclaration.params);
            PARAMETER(functionDeclaration.args);
            TYPE(functionDeclaration.returnType);
            CHILD(functionDeclaration.body);
            ARRAY(functionDeclaration.decorators);
            break;
        case ZR_AST_VARIABLE_DECLARATION:
            CHILD(variableDeclaration.pattern);
            CHILD(variableDeclaration.value);
            TYPE(variableDeclaration.typeInfo);
            break;
        case ZR_AST_COMPILE_TIME_DECLARATION:
            CHILD(compileTimeDeclaration.declaration);
            break;
        case ZR_AST_EXTERN_BLOCK:
            CHILD(externBlock.libraryName);
            ARRAY(externBlock.declarations);
            break;
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            IDENTIFIER(externFunctionDeclaration.name);
            ARRAY(externFunctionDeclaration.params);
            PARAMETER(externFunctionDeclaration.args);
            TYPE(externFunctionDeclaration.returnType);
            ARRAY(externFunctionDeclaration.decorators);
            break;
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            IDENTIFIER(externDelegateDeclaration.name);
            ARRAY(externDelegateDeclaration.params);
            PARAMETER(externDelegateDeclaration.args);
            TYPE(externDelegateDeclaration.returnType);
            ARRAY(externDelegateDeclaration.decorators);
            break;
        case ZR_AST_STRUCT_DECLARATION:
            IDENTIFIER(structDeclaration.name);
            GENERIC(structDeclaration.generic);
            ARRAY(structDeclaration.inherits);
            ARRAY(structDeclaration.members);
            ARRAY(structDeclaration.decorators);
            break;
        case ZR_AST_CLASS_DECLARATION:
            IDENTIFIER(classDeclaration.name);
            GENERIC(classDeclaration.generic);
            ARRAY(classDeclaration.inherits);
            ARRAY(classDeclaration.members);
            ARRAY(classDeclaration.decorators);
            break;
        case ZR_AST_ENUM_DECLARATION:
            IDENTIFIER(enumDeclaration.name);
            TYPE(enumDeclaration.baseType);
            ARRAY(enumDeclaration.members);
            ARRAY(enumDeclaration.decorators);
            break;
        case ZR_AST_ENUM_MEMBER:
            IDENTIFIER(enumMember.name);
            CHILD(enumMember.value);
            ARRAY(enumMember.decorators);
            break;
        case ZR_AST_INTERFACE_DECLARATION:
            IDENTIFIER(interfaceDeclaration.name);
            GENERIC(interfaceDeclaration.generic);
            ARRAY(interfaceDeclaration.inherits);
            ARRAY(interfaceDeclaration.members);
            break;
        case ZR_AST_INTERFACE_FIELD_DECLARATION:
            IDENTIFIER(interfaceFieldDeclaration.name);
            TYPE(interfaceFieldDeclaration.typeInfo);
            break;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            IDENTIFIER(interfaceMethodSignature.name);
            GENERIC(interfaceMethodSignature.generic);
            ARRAY(interfaceMethodSignature.params);
            PARAMETER(interfaceMethodSignature.args);
            TYPE(interfaceMethodSignature.returnType);
            break;
        case ZR_AST_INTERFACE_PROPERTY_SIGNATURE:
            IDENTIFIER(interfacePropertySignature.name);
            TYPE(interfacePropertySignature.typeInfo);
            break;
        case ZR_AST_INTERFACE_META_SIGNATURE:
            IDENTIFIER(interfaceMetaSignature.meta);
            ARRAY(interfaceMetaSignature.params);
            PARAMETER(interfaceMetaSignature.args);
            TYPE(interfaceMetaSignature.returnType);
            break;
        case ZR_AST_UNION_DECLARATION:
            IDENTIFIER(unionDeclaration.name);
            GENERIC(unionDeclaration.generic);
            ARRAY(unionDeclaration.variants);
            ARRAY(unionDeclaration.decorators);
            break;
        case ZR_AST_UNION_VARIANT:
            IDENTIFIER(unionVariant.name);
            ARRAY(unionVariant.fields);
            ARRAY(unionVariant.decorators);
            break;
        case ZR_AST_STRUCT_FIELD:
            ARRAY(structField.decorators);
            IDENTIFIER(structField.name);
            TYPE(structField.typeInfo);
            CHILD(structField.init);
            break;
        case ZR_AST_STRUCT_METHOD:
            ARRAY(structMethod.decorators);
            IDENTIFIER(structMethod.name);
            GENERIC(structMethod.generic);
            ARRAY(structMethod.params);
            PARAMETER(structMethod.args);
            TYPE(structMethod.returnType);
            CHILD(structMethod.body);
            break;
        case ZR_AST_STRUCT_META_FUNCTION:
            IDENTIFIER(structMetaFunction.meta);
            ARRAY(structMetaFunction.params);
            PARAMETER(structMetaFunction.args);
            TYPE(structMetaFunction.returnType);
            CHILD(structMetaFunction.body);
            break;
        case ZR_AST_CLASS_FIELD:
            ARRAY(classField.decorators);
            IDENTIFIER(classField.name);
            TYPE(classField.typeInfo);
            CHILD(classField.init);
            break;
        case ZR_AST_CLASS_METHOD:
            ARRAY(classMethod.decorators);
            IDENTIFIER(classMethod.name);
            GENERIC(classMethod.generic);
            ARRAY(classMethod.params);
            PARAMETER(classMethod.args);
            TYPE(classMethod.returnType);
            CHILD(classMethod.body);
            break;
        case ZR_AST_CLASS_PROPERTY:
            ARRAY(classProperty.decorators);
            CHILD(classProperty.modifier);
            break;
        case ZR_AST_CLASS_META_FUNCTION:
            IDENTIFIER(classMetaFunction.meta);
            ARRAY(classMetaFunction.params);
            PARAMETER(classMetaFunction.args);
            ARRAY(classMetaFunction.superArgs);
            TYPE(classMetaFunction.returnType);
            CHILD(classMetaFunction.body);
            break;
        case ZR_AST_PARAMETER:
            IDENTIFIER(parameter.name);
            TYPE(parameter.typeInfo);
            CHILD(parameter.defaultValue);
            ARRAY(parameter.genericTypeConstraints);
            ARRAY(parameter.decorators);
            break;
        case ZR_AST_PROPERTY_DECLARATION:
            ARRAY(propertyDeclaration.decorators);
            IDENTIFIER(propertyDeclaration.name);
            TYPE(propertyDeclaration.typeInfo);
            ARRAY(propertyDeclaration.accessors);
            break;
        case ZR_AST_PROPERTY_ACCESSOR:
            CHILD(propertyAccessor.body);
            break;
        case ZR_AST_DECORATOR_EXPRESSION:
            CHILD(decoratorExpression.expr);
            break;
        case ZR_AST_PROPERTY_GET:
            IDENTIFIER(propertyGet.name);
            TYPE(propertyGet.targetType);
            CHILD(propertyGet.body);
            break;
        case ZR_AST_PROPERTY_SET:
            IDENTIFIER(propertySet.name);
            IDENTIFIER(propertySet.param);
            TYPE(propertySet.targetType);
            CHILD(propertySet.body);
            break;
        case ZR_AST_BINARY_EXPRESSION:
            CHILD(binaryExpression.left);
            CHILD(binaryExpression.right);
            break;
        case ZR_AST_UNARY_EXPRESSION:
            CHILD(unaryExpression.argument);
            break;
        case ZR_AST_LOGICAL_EXPRESSION:
            CHILD(logicalExpression.left);
            CHILD(logicalExpression.right);
            break;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            TYPE(typeCastExpression.targetType);
            CHILD(typeCastExpression.expression);
            break;
        case ZR_AST_AWAIT_EXPRESSION:
            CHILD(awaitExpression.operand);
            break;
        case ZR_AST_SPREAD_ARGUMENT:
            CHILD(spreadArgument.expression);
            break;
        case ZR_AST_FUNCTION_CALL:
            ARRAY(functionCall.args);
            ARRAY(functionCall.genericArguments);
            break;
        case ZR_AST_ARRAY_LITERAL:
            ARRAY(arrayLiteral.elements);
            break;
        case ZR_AST_OBJECT_LITERAL:
            ARRAY(objectLiteral.properties);
            break;
        case ZR_AST_BLOCK:
            ARRAY(block.body);
            break;
        case ZR_AST_EXPRESSION_STATEMENT:
            CHILD(expressionStatement.expr);
            break;
        case ZR_AST_USING_STATEMENT:
            CHILD(usingStatement.resource);
            CHILD(usingStatement.body);
            CHILD(usingStatement.pattern);
            TYPE(usingStatement.guardTypeInfo);
            CHILD(usingStatement.elseBody);
            break;
        case ZR_AST_IF_EXPRESSION:
            CHILD(ifExpression.condition);
            CHILD(ifExpression.thenExpr);
            CHILD(ifExpression.elseExpr);
            break;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            CHILD(conditionalExpression.test);
            CHILD(conditionalExpression.consequent);
            CHILD(conditionalExpression.alternate);
            break;
        case ZR_AST_SWITCH_EXPRESSION:
            CHILD(switchExpression.expr);
            ARRAY(switchExpression.cases);
            CHILD(switchExpression.defaultCase);
            break;
        case ZR_AST_LAMBDA_EXPRESSION:
            ARRAY(lambdaExpression.params);
            PARAMETER(lambdaExpression.args);
            TYPE(lambdaExpression.returnType);
            CHILD(lambdaExpression.block);
            break;
        case ZR_AST_PRIMARY_EXPRESSION:
            CHILD(primaryExpression.property);
            ARRAY(primaryExpression.members);
            break;
        case ZR_AST_IMPORT_EXPRESSION:
            CHILD(importExpression.modulePath);
            break;
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            CHILD(typeQueryExpression.operand);
            TYPE(typeQueryExpression.typeOperand);
            break;
        case ZR_AST_TYPE_LITERAL_EXPRESSION:
            TYPE(typeLiteralExpression.typeInfo);
            break;
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            CHILD(prototypeReferenceExpression.target);
            break;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            CHILD(constructExpression.target);
            ARRAY(constructExpression.args);
            break;
        case ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION:
            CHILD(ownershipIntrinsicExpression.argument);
            break;
        case ZR_AST_MEMBER_EXPRESSION:
            CHILD(memberExpression.property);
            break;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            CHILD(assignmentExpression.left);
            CHILD(assignmentExpression.right);
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            CHILD(keyValuePair.key);
            CHILD(keyValuePair.value);
            break;
        case ZR_AST_WHILE_LOOP:
            CHILD(whileLoop.cond);
            CHILD(whileLoop.block);
            break;
        case ZR_AST_FOR_LOOP:
            CHILD(forLoop.init);
            CHILD(forLoop.cond);
            CHILD(forLoop.step);
            CHILD(forLoop.block);
            break;
        case ZR_AST_FOREACH_LOOP:
            CHILD(foreachLoop.pattern);
            TYPE(foreachLoop.typeInfo);
            CHILD(foreachLoop.expr);
            CHILD(foreachLoop.block);
            break;
        case ZR_AST_RETURN_STATEMENT:
            CHILD(returnStatement.expr);
            break;
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            CHILD(breakContinueStatement.expr);
            break;
        case ZR_AST_THROW_STATEMENT:
            CHILD(throwStatement.expr);
            break;
        case ZR_AST_OUT_STATEMENT:
            CHILD(outStatement.expr);
            break;
        case ZR_AST_DESTRUCTURING_OBJECT:
            ARRAY(destructuringObject.keys);
            break;
        case ZR_AST_DESTRUCTURING_ARRAY:
            ARRAY(destructuringArray.keys);
            break;
        case ZR_AST_YIELD_STATEMENT:
            CHILD(yieldStatement.expr);
            break;
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            CHILD(tryCatchFinallyStatement.block);
            ARRAY(tryCatchFinallyStatement.catchClauses);
            CHILD(tryCatchFinallyStatement.finallyBlock);
            break;
        case ZR_AST_CATCH_CLAUSE:
            ARRAY(catchClause.pattern);
            CHILD(catchClause.block);
            break;
        case ZR_AST_SWITCH_CASE:
            CHILD(switchCase.value);
            CHILD(switchCase.block);
            break;
        case ZR_AST_SWITCH_DEFAULT:
            CHILD(switchDefault.block);
            break;
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            ARRAY(templateStringLiteral.segments);
            break;
        case ZR_AST_INTERPOLATED_SEGMENT:
            CHILD(interpolatedSegment.expression);
            break;
        case ZR_AST_TYPE:
            if (!semantic_da_ast_walk_type(builder, &node->data.type, owner)) return ZR_FALSE;
            break;
        case ZR_AST_STRUCT_INIT_EXPRESSION:
            TYPE(structInitExpression.typeInfo);
            ARRAY(structInitExpression.args);
            break;
        case ZR_AST_GENERATOR_EXPRESSION:
            CHILD(generatorExpression.block);
            break;
        case ZR_AST_FUNCTION_TYPE:
            GENERIC(functionType.generic);
            ARRAY(functionType.params);
            PARAMETER(functionType.args);
            TYPE(functionType.returnType);
            break;
        case ZR_AST_GENERIC_TYPE:
            ARRAY(genericType.params);
            IDENTIFIER(genericType.name);
            break;
        case ZR_AST_TUPLE_TYPE:
            ARRAY(tupleType.elements);
            break;
        default:
            break;
    }
#undef CHILD
#undef ARRAY
#undef TYPE
#undef IDENTIFIER
#undef PARAMETER
#undef GENERIC
    return ZR_TRUE;
}

static TZrBool semantic_da_ast_walk(SZrSemanticDaAstBuilder *builder,
                                    SZrAstNode *node,
                                    SZrAstNode *owner) {
    TZrSize begin;

    if (node == ZR_NULL) {
        return ZR_TRUE;
    }
    if (builder->count == ZR_MAX_SIZE ||
        (builder->entries != ZR_NULL && builder->count >= builder->capacity)) {
        return ZR_FALSE;
    }
    begin = builder->count++;
    if (builder->entries != ZR_NULL) {
        builder->entries[begin].node = node;
        builder->entries[begin].executionOwner = owner;
        builder->entries[begin].begin = begin;
    }
    if (!semantic_da_ast_walk_children(builder, node,
                                       semantic_da_ast_is_callable(node) ? node : owner)) {
        return ZR_FALSE;
    }
    if (builder->entries != ZR_NULL) {
        builder->entries[begin].end = builder->count;
    }
    return ZR_TRUE;
}

static int semantic_da_ast_compare_address(const void *left, const void *right) {
    const SZrSemanticDaAstEntry *leftEntry = *(SZrSemanticDaAstEntry *const *)left;
    const SZrSemanticDaAstEntry *rightEntry = *(SZrSemanticDaAstEntry *const *)right;
    uintptr_t leftAddress = (uintptr_t)leftEntry->node;
    uintptr_t rightAddress = (uintptr_t)rightEntry->node;

    return leftAddress < rightAddress ? -1 : leftAddress > rightAddress ? 1 : 0;
}

void ZrParser_SemanticDaAst_Init(SZrSemanticDaAstIndex *index) {
    if (index != ZR_NULL) {
        index->state = ZR_NULL;
        index->entries = ZR_NULL;
        index->byAddress = ZR_NULL;
        index->count = 0;
    }
}

void ZrParser_SemanticDaAst_Free(SZrSemanticDaAstIndex *index) {
    if (index == ZR_NULL) {
        return;
    }
    if (index->entries != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(index->state->global, index->entries,
                                    index->count * sizeof(*index->entries),
                                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (index->byAddress != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(index->state->global, index->byAddress,
                                    index->count * sizeof(*index->byAddress),
                                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    ZrParser_SemanticDaAst_Init(index);
}

TZrBool ZrParser_SemanticDaAst_Build(SZrState *state,
                                   SZrAstNode *root,
                                   SZrSemanticDaAstIndex *index) {
    SZrSemanticDaAstBuilder builder = { ZR_NULL, 0, 0 };
    TZrSize position;

    if (state == ZR_NULL || state->global == ZR_NULL || root == ZR_NULL || index == ZR_NULL ||
        index->entries != ZR_NULL || index->byAddress != ZR_NULL || index->count != 0) {
        return ZR_FALSE;
    }
    if (!semantic_da_ast_walk(&builder, root, ZR_NULL) || builder.count == 0 ||
        builder.count > ZR_MAX_SIZE / sizeof(*index->entries) ||
        builder.count > ZR_MAX_SIZE / sizeof(*index->byAddress)) {
        return ZR_FALSE;
    }
    index->state = state;
    index->count = builder.count;
    index->entries = (SZrSemanticDaAstEntry *)ZrCore_Memory_RawMallocWithType(
            state->global, index->count * sizeof(*index->entries), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    index->byAddress = (SZrSemanticDaAstEntry **)ZrCore_Memory_RawMallocWithType(
            state->global, index->count * sizeof(*index->byAddress), ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (index->entries == ZR_NULL || index->byAddress == ZR_NULL) {
        ZrParser_SemanticDaAst_Free(index);
        return ZR_FALSE;
    }
    builder.entries = index->entries;
    builder.capacity = index->count;
    builder.count = 0;
    if (!semantic_da_ast_walk(&builder, root, ZR_NULL) || builder.count != index->count) {
        ZrParser_SemanticDaAst_Free(index);
        return ZR_FALSE;
    }
    for (position = 0; position < index->count; position++) {
        index->byAddress[position] = &index->entries[position];
    }
    qsort(index->byAddress, index->count, sizeof(*index->byAddress), semantic_da_ast_compare_address);
    for (position = 1; position < index->count; position++) {
        if (index->byAddress[position - 1]->node == index->byAddress[position]->node) {
            ZrParser_SemanticDaAst_Free(index);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static const SZrSemanticDaAstEntry *semantic_da_ast_find(
        const SZrSemanticDaAstIndex *index,
        const SZrAstNode *node) {
    TZrSize low = 0;
    TZrSize high;
    uintptr_t address = (uintptr_t)node;

    if (index == ZR_NULL || index->byAddress == ZR_NULL || node == ZR_NULL) {
        return ZR_NULL;
    }
    high = index->count;
    while (low < high) {
        TZrSize middle = low + (high - low) / 2;
        const SZrSemanticDaAstEntry *entry = index->byAddress[middle];
        uintptr_t candidate = (uintptr_t)entry->node;
        if (candidate < address) {
            low = middle + 1;
        } else if (candidate > address) {
            high = middle;
        } else {
            return entry;
        }
    }
    return ZR_NULL;
}

EZrSemanticDaAstMatch ZrParser_SemanticDaAst_Match(
        const SZrSemanticDaAstIndex *index,
        const SZrAstNode *subtree,
        const SZrAstNode *factNode) {
    const SZrSemanticDaAstEntry *factEntry;
    const SZrSemanticDaAstEntry *subtreeEntry;

    if (subtree == ZR_NULL) {
        return ZR_SEMANTIC_DA_AST_EXCLUDED;
    }
    factEntry = semantic_da_ast_find(index, factNode);
    if (factEntry == ZR_NULL) {
        return factNode == ZR_NULL || factNode->ownership == ZR_NULL
                       ? ZR_SEMANTIC_DA_AST_LEGACY
                       : ZR_SEMANTIC_DA_AST_EXCLUDED;
    }
    subtreeEntry = semantic_da_ast_find(index, subtree);
    return subtreeEntry != ZR_NULL &&
                   factEntry->begin >= subtreeEntry->begin &&
                   factEntry->begin < subtreeEntry->end &&
                   factEntry->executionOwner == subtreeEntry->executionOwner
                   ? ZR_SEMANTIC_DA_AST_INCLUDED
                   : ZR_SEMANTIC_DA_AST_EXCLUDED;
}

TZrBool ZrParser_SemanticDaAst_VisitCfgRoots(
        const SZrSemanticDaAstIndex *index,
        TZrSemanticDaAstRootVisitor visitor,
        void *userData) {
    TZrSize position;

    if (index == ZR_NULL || index->entries == ZR_NULL || visitor == ZR_NULL) {
        return ZR_FALSE;
    }
    for (position = 0; position < index->count; position++) {
        SZrAstNode *node = index->entries[position].node;
        SZrAstNode *root = ZR_NULL;
        if (node->type == ZR_AST_SCRIPT || node->type == ZR_AST_FUNCTION_DECLARATION ||
            (position == 0 && node->type == ZR_AST_BLOCK)) {
            root = node;
        } else if (node->type == ZR_AST_LAMBDA_EXPRESSION) {
            root = node->data.lambdaExpression.block;
        }
        if (root != ZR_NULL && !visitor(root, userData)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
