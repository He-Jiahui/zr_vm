//
// Created by Auto on 2025/01/XX.
//

#include "semantic_analyzer_internal.h"
#include "lsp_module_metadata.h"

#include <stdarg.h>

#define ZR_LSP_NATIVE_MODULE_COMPLETION_MAX_DEPTH ((TZrSize)4)

static TZrBool semantic_source_uri_equals(SZrString *left, SZrString *right) {
    TZrNativeString leftText;
    TZrNativeString rightText;
    TZrSize leftLength;
    TZrSize rightLength;

    if (left == right) {
        return ZR_TRUE;
    }

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_TRUE;
    }

    if (left->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        leftText = ZrCore_String_GetNativeStringShort(left);
        leftLength = left->shortStringLength;
    } else {
        leftText = ZrCore_String_GetNativeString(left);
        leftLength = left->longStringLength;
    }

    if (right->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        rightText = ZrCore_String_GetNativeStringShort(right);
        rightLength = right->shortStringLength;
    } else {
        rightText = ZrCore_String_GetNativeString(right);
        rightLength = right->longStringLength;
    }

    return leftText != ZR_NULL && rightText != ZR_NULL &&
           leftLength == rightLength &&
           memcmp(leftText, rightText, leftLength) == 0;
}

static void semantic_get_string_view(SZrString *value, TZrNativeString *text, TZrSize *length) {
    if (text == ZR_NULL || length == ZR_NULL) {
        return;
    }

    *text = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *text = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
    } else {
        *text = ZrCore_String_GetNativeString(value);
        *length = value->longStringLength;
    }
}

static TZrBool semantic_file_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (!semantic_source_uri_equals(range.source, position.source)) {
        return ZR_FALSE;
    }

    if (range.start.offset > 0 && range.end.offset > 0 &&
        position.start.offset > 0 && position.end.offset > 0) {
        return range.start.offset <= position.start.offset && position.end.offset <= range.end.offset;
    }

    return (range.start.line < position.start.line ||
            (range.start.line == position.start.line && range.start.column <= position.start.column)) &&
           (position.end.line < range.end.line ||
            (position.end.line == range.end.line && position.end.column <= range.end.column));
}

static const SZrType *semantic_find_type_info_at_position(const SZrType *typeInfo, SZrFileRange position) {
    TZrNativeString genericNameText = ZR_NULL;
    TZrSize genericNameLength = 0;

    if (typeInfo == ZR_NULL || typeInfo->name == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeInfo->subType != ZR_NULL) {
        const SZrType *nestedType = semantic_find_type_info_at_position(typeInfo->subType, position);
        if (nestedType != ZR_NULL) {
            return nestedType;
        }
    }

    if (typeInfo->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = (SZrGenericType *)&typeInfo->name->data.genericType;
        if (genericType->params != ZR_NULL && genericType->params->nodes != ZR_NULL) {
            for (TZrSize index = 0; index < genericType->params->count; index++) {
                SZrAstNode *paramNode = genericType->params->nodes[index];
                if (paramNode != ZR_NULL && paramNode->type == ZR_AST_TYPE) {
                    const SZrType *nestedType = semantic_find_type_info_at_position(&paramNode->data.type, position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
        }

        if (genericType->name != ZR_NULL && genericType->name->name != ZR_NULL) {
            semantic_get_string_view(genericType->name->name, &genericNameText, &genericNameLength);
            if (genericNameText != ZR_NULL && genericNameLength > 0) {
                if (typeInfo->name->location.start.offset > 0 && position.start.offset > 0) {
                    TZrSize baseStart =
                        typeInfo->name->location.start.offset > genericNameLength + 1
                            ? typeInfo->name->location.start.offset - genericNameLength - 1
                            : 0;
                    if (baseStart <= position.start.offset &&
                        position.end.offset <= typeInfo->name->location.start.offset) {
                        return typeInfo;
                    }
                } else if (typeInfo->name->location.start.line == position.start.line) {
                    TZrInt32 baseColumn = typeInfo->name->location.start.column - (TZrInt32)genericNameLength - 1;
                    if (baseColumn <= position.start.column &&
                        position.end.column <= typeInfo->name->location.start.column) {
                        return typeInfo;
                    }
                }
            }
        }
    } else if (typeInfo->name->type == ZR_AST_TUPLE_TYPE) {
        SZrTupleType *tupleType = (SZrTupleType *)&typeInfo->name->data.tupleType;
        if (tupleType->elements != ZR_NULL && tupleType->elements->nodes != ZR_NULL) {
            for (TZrSize index = 0; index < tupleType->elements->count; index++) {
                SZrAstNode *elementNode = tupleType->elements->nodes[index];
                if (elementNode != ZR_NULL && elementNode->type == ZR_AST_TYPE) {
                    const SZrType *nestedType = semantic_find_type_info_at_position(&elementNode->data.type, position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
        }
    }

    return semantic_file_range_contains_position(typeInfo->name->location, position) ? typeInfo : ZR_NULL;
}

static TZrBool semantic_symbol_is_type_position_candidate(SZrSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (symbol->type) {
        case ZR_SYMBOL_CLASS:
        case ZR_SYMBOL_STRUCT:
        case ZR_SYMBOL_INTERFACE:
        case ZR_SYMBOL_ENUM:
        case ZR_SYMBOL_PARAMETER:
            return ZR_TRUE;
        case ZR_SYMBOL_FUNCTION:
            return symbol->astNode != ZR_NULL &&
                   symbol->astNode->type == ZR_AST_EXTERN_DELEGATE_DECLARATION;
        default:
            return ZR_FALSE;
    }
}

static SZrString *semantic_extract_type_symbol_name(const SZrType *typeInfo) {
    if (typeInfo == ZR_NULL || typeInfo->name == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeInfo->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return typeInfo->name->data.identifier.name;
    }

    if (typeInfo->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = (SZrGenericType *)&typeInfo->name->data.genericType;
        if (genericType->name != ZR_NULL) {
            return genericType->name->name;
        }
    }

    return ZR_NULL;
}

static SZrSymbol *semantic_lookup_type_symbol_at_position(SZrState *state,
                                                          SZrSemanticAnalyzer *analyzer,
                                                          const SZrType *typeInfo,
                                                          SZrFileRange position) {
    SZrString *typeName;
    SZrSymbol *symbol;
    SZrArray candidates;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL) {
        return ZR_NULL;
    }

    typeName = semantic_extract_type_symbol_name(typeInfo);
    if (typeName == ZR_NULL) {
        return ZR_NULL;
    }

    symbol = ZrLanguageServer_SymbolTable_LookupAtPosition(analyzer->symbolTable, typeName, position);
    if (symbol != ZR_NULL && semantic_symbol_is_type_position_candidate(symbol)) {
        return symbol;
    }

    ZrCore_Array_Construct(&candidates);
    if (state != ZR_NULL &&
        ZrLanguageServer_SymbolTable_LookupAll(state, analyzer->symbolTable, typeName, ZR_NULL, &candidates)) {
        for (TZrSize index = 0; index < candidates.length; index++) {
            SZrSymbol **candidatePtr = (SZrSymbol **)ZrCore_Array_Get(&candidates, index);
            if (candidatePtr != ZR_NULL &&
                *candidatePtr != ZR_NULL &&
                semantic_symbol_is_type_position_candidate(*candidatePtr)) {
                symbol = *candidatePtr;
                break;
            }
        }
    }
    if (candidates.isValid) {
        ZrCore_Array_Free(state, &candidates);
    }

    return symbol;
}

static const SZrType *semantic_find_type_node_at_position(SZrAstNode *node, SZrFileRange position) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    const SZrType *nestedType =
                            semantic_find_type_node_at_position(node->data.script.statements->nodes[index], position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
            return ZR_NULL;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    const SZrType *nestedType =
                            semantic_find_type_node_at_position(node->data.block.body->nodes[index], position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
            return ZR_NULL;

        case ZR_AST_VARIABLE_DECLARATION:
            if (node->data.variableDeclaration.typeInfo != ZR_NULL) {
                const SZrType *typeInfo =
                        semantic_find_type_info_at_position(node->data.variableDeclaration.typeInfo, position);
                if (typeInfo != ZR_NULL) {
                    return typeInfo;
                }
            }
            return node->data.variableDeclaration.value != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.variableDeclaration.value, position)
                           : ZR_NULL;

        case ZR_AST_FUNCTION_DECLARATION:
            if (node->data.functionDeclaration.params != ZR_NULL && node->data.functionDeclaration.params->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.functionDeclaration.params->count; index++) {
                    const SZrType *nestedType =
                            semantic_find_type_node_at_position(node->data.functionDeclaration.params->nodes[index],
                                                                position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
            if (node->data.functionDeclaration.returnType != ZR_NULL) {
                const SZrType *typeInfo =
                        semantic_find_type_info_at_position(node->data.functionDeclaration.returnType, position);
                if (typeInfo != ZR_NULL) {
                    return typeInfo;
                }
            }
            return node->data.functionDeclaration.body != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.functionDeclaration.body, position)
                           : ZR_NULL;

        case ZR_AST_TEST_DECLARATION:
            return node->data.testDeclaration.body != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.testDeclaration.body, position)
                           : ZR_NULL;

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return node->data.compileTimeDeclaration.declaration != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.compileTimeDeclaration.declaration, position)
                           : ZR_NULL;

        case ZR_AST_CLASS_DECLARATION:
            if (node->data.classDeclaration.inherits != ZR_NULL && node->data.classDeclaration.inherits->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.classDeclaration.inherits->count; index++) {
                    const SZrType *nestedType =
                            semantic_find_type_node_at_position(node->data.classDeclaration.inherits->nodes[index],
                                                                position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
            if (node->data.classDeclaration.members != ZR_NULL && node->data.classDeclaration.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.classDeclaration.members->count; index++) {
                    const SZrType *nestedType =
                            semantic_find_type_node_at_position(node->data.classDeclaration.members->nodes[index],
                                                                position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
            return ZR_NULL;

        case ZR_AST_CLASS_FIELD:
            if (node->data.classField.typeInfo != ZR_NULL) {
                const SZrType *typeInfo = semantic_find_type_info_at_position(node->data.classField.typeInfo, position);
                if (typeInfo != ZR_NULL) {
                    return typeInfo;
                }
            }
            return node->data.classField.init != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.classField.init, position)
                           : ZR_NULL;

        case ZR_AST_CLASS_METHOD:
            if (node->data.classMethod.params != ZR_NULL && node->data.classMethod.params->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.classMethod.params->count; index++) {
                    const SZrType *nestedType =
                            semantic_find_type_node_at_position(node->data.classMethod.params->nodes[index], position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
            if (node->data.classMethod.returnType != ZR_NULL) {
                const SZrType *typeInfo = semantic_find_type_info_at_position(node->data.classMethod.returnType, position);
                if (typeInfo != ZR_NULL) {
                    return typeInfo;
                }
            }
            return node->data.classMethod.body != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.classMethod.body, position)
                           : ZR_NULL;

        case ZR_AST_CLASS_META_FUNCTION:
            if (node->data.classMetaFunction.params != ZR_NULL &&
                node->data.classMetaFunction.params->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.classMetaFunction.params->count; index++) {
                    const SZrType *nestedType =
                            semantic_find_type_node_at_position(node->data.classMetaFunction.params->nodes[index],
                                                                position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
            if (node->data.classMetaFunction.returnType != ZR_NULL) {
                const SZrType *typeInfo =
                        semantic_find_type_info_at_position(node->data.classMetaFunction.returnType, position);
                if (typeInfo != ZR_NULL) {
                    return typeInfo;
                }
            }
            return node->data.classMetaFunction.body != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.classMetaFunction.body, position)
                           : ZR_NULL;

        case ZR_AST_CLASS_PROPERTY:
            return node->data.classProperty.modifier != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.classProperty.modifier, position)
                           : ZR_NULL;

        case ZR_AST_PROPERTY_GET:
            if (node->data.propertyGet.targetType != ZR_NULL) {
                const SZrType *typeInfo =
                        semantic_find_type_info_at_position(node->data.propertyGet.targetType, position);
                if (typeInfo != ZR_NULL) {
                    return typeInfo;
                }
            }
            return node->data.propertyGet.body != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.propertyGet.body, position)
                           : ZR_NULL;

        case ZR_AST_PROPERTY_SET:
            if (node->data.propertySet.targetType != ZR_NULL) {
                const SZrType *typeInfo =
                        semantic_find_type_info_at_position(node->data.propertySet.targetType, position);
                if (typeInfo != ZR_NULL) {
                    return typeInfo;
                }
            }
            return node->data.propertySet.body != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.propertySet.body, position)
                           : ZR_NULL;

        case ZR_AST_INTERFACE_DECLARATION:
            if (node->data.interfaceDeclaration.inherits != ZR_NULL &&
                node->data.interfaceDeclaration.inherits->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.interfaceDeclaration.inherits->count; index++) {
                    const SZrType *nestedType =
                            semantic_find_type_node_at_position(node->data.interfaceDeclaration.inherits->nodes[index],
                                                                position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
            if (node->data.interfaceDeclaration.members != ZR_NULL &&
                node->data.interfaceDeclaration.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.interfaceDeclaration.members->count; index++) {
                    const SZrType *nestedType =
                            semantic_find_type_node_at_position(node->data.interfaceDeclaration.members->nodes[index],
                                                                position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
            return ZR_NULL;

        case ZR_AST_INTERFACE_FIELD_DECLARATION:
            return node->data.interfaceFieldDeclaration.typeInfo != ZR_NULL
                           ? semantic_find_type_info_at_position(node->data.interfaceFieldDeclaration.typeInfo, position)
                           : ZR_NULL;

        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            if (node->data.interfaceMethodSignature.params != ZR_NULL &&
                node->data.interfaceMethodSignature.params->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.interfaceMethodSignature.params->count; index++) {
                    const SZrType *nestedType =
                            semantic_find_type_node_at_position(node->data.interfaceMethodSignature.params->nodes[index],
                                                                position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
            return node->data.interfaceMethodSignature.returnType != ZR_NULL
                           ? semantic_find_type_info_at_position(node->data.interfaceMethodSignature.returnType, position)
                           : ZR_NULL;

        case ZR_AST_PARAMETER:
            return node->data.parameter.typeInfo != ZR_NULL
                           ? semantic_find_type_info_at_position(node->data.parameter.typeInfo, position)
                           : ZR_NULL;

        case ZR_AST_LAMBDA_EXPRESSION:
            if (node->data.lambdaExpression.params != ZR_NULL && node->data.lambdaExpression.params->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.lambdaExpression.params->count; index++) {
                    const SZrType *nestedType =
                            semantic_find_type_node_at_position(node->data.lambdaExpression.params->nodes[index],
                                                                position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
            return node->data.lambdaExpression.block != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.lambdaExpression.block, position)
                           : ZR_NULL;

        case ZR_AST_EXPRESSION_STATEMENT:
            return node->data.expressionStatement.expr != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.expressionStatement.expr, position)
                           : ZR_NULL;

        case ZR_AST_RETURN_STATEMENT:
            return node->data.returnStatement.expr != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.returnStatement.expr, position)
                           : ZR_NULL;

        case ZR_AST_PRIMARY_EXPRESSION:
            if (node->data.primaryExpression.property != ZR_NULL) {
                const SZrType *nestedType =
                        semantic_find_type_node_at_position(node->data.primaryExpression.property, position);
                if (nestedType != ZR_NULL) {
                    return nestedType;
                }
            }
            if (node->data.primaryExpression.members != ZR_NULL && node->data.primaryExpression.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.primaryExpression.members->count; index++) {
                    const SZrType *nestedType =
                            semantic_find_type_node_at_position(node->data.primaryExpression.members->nodes[index],
                                                                position);
                    if (nestedType != ZR_NULL) {
                        return nestedType;
                    }
                }
            }
            return ZR_NULL;

        case ZR_AST_MEMBER_EXPRESSION:
            return node->data.memberExpression.property != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.memberExpression.property, position)
                           : ZR_NULL;

        case ZR_AST_CONSTRUCT_EXPRESSION:
            if (node->data.constructExpression.target != ZR_NULL) {
                const SZrType *nestedType =
                        semantic_find_type_node_at_position(node->data.constructExpression.target, position);
                if (nestedType != ZR_NULL) {
                    return nestedType;
                }
            }
            return ZR_NULL;

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            if (node->data.assignmentExpression.left != ZR_NULL) {
                const SZrType *nestedType =
                        semantic_find_type_node_at_position(node->data.assignmentExpression.left, position);
                if (nestedType != ZR_NULL) {
                    return nestedType;
                }
            }
            return node->data.assignmentExpression.right != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.assignmentExpression.right, position)
                           : ZR_NULL;

        case ZR_AST_BINARY_EXPRESSION:
            if (node->data.binaryExpression.left != ZR_NULL) {
                const SZrType *nestedType =
                        semantic_find_type_node_at_position(node->data.binaryExpression.left, position);
                if (nestedType != ZR_NULL) {
                    return nestedType;
                }
            }
            return node->data.binaryExpression.right != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.binaryExpression.right, position)
                           : ZR_NULL;

        case ZR_AST_UNARY_EXPRESSION:
            return node->data.unaryExpression.argument != ZR_NULL
                           ? semantic_find_type_node_at_position(node->data.unaryExpression.argument, position)
                           : ZR_NULL;

        case ZR_AST_TYPE:
            return semantic_find_type_info_at_position(&node->data.type, position);

        default:
            return ZR_NULL;
    }
}

static TZrBool semantic_node_contains_position(SZrAstNode *node, SZrFileRange position) {
    return node != ZR_NULL && semantic_file_range_contains_position(node->location, position);
}

static SZrAstNode *semantic_find_expression_node_in_array(SZrAstNodeArray *nodes, SZrFileRange position);

static SZrAstNode *semantic_find_expression_node_at_position(SZrAstNode *node, SZrFileRange position) {
    SZrAstNode *nested = ZR_NULL;

    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return semantic_find_expression_node_in_array(node->data.script.statements, position);

        case ZR_AST_BLOCK:
            return semantic_find_expression_node_in_array(node->data.block.body, position);

        case ZR_AST_VARIABLE_DECLARATION:
            return semantic_find_expression_node_at_position(node->data.variableDeclaration.value, position);

        case ZR_AST_FUNCTION_DECLARATION:
            return semantic_find_expression_node_at_position(node->data.functionDeclaration.body, position);

        case ZR_AST_TEST_DECLARATION:
            return semantic_find_expression_node_at_position(node->data.testDeclaration.body, position);

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return semantic_find_expression_node_at_position(node->data.compileTimeDeclaration.declaration, position);

        case ZR_AST_CLASS_DECLARATION:
            return semantic_find_expression_node_in_array(node->data.classDeclaration.members, position);

        case ZR_AST_STRUCT_DECLARATION:
            return semantic_find_expression_node_in_array(node->data.structDeclaration.members, position);

        case ZR_AST_INTERFACE_DECLARATION:
            return semantic_find_expression_node_in_array(node->data.interfaceDeclaration.members, position);

        case ZR_AST_CLASS_FIELD:
            return semantic_find_expression_node_at_position(node->data.classField.init, position);

        case ZR_AST_STRUCT_FIELD:
            return semantic_find_expression_node_at_position(node->data.structField.init, position);

        case ZR_AST_ENUM_MEMBER:
            return semantic_find_expression_node_at_position(node->data.enumMember.value, position);

        case ZR_AST_CLASS_METHOD:
            return semantic_find_expression_node_at_position(node->data.classMethod.body, position);

        case ZR_AST_STRUCT_METHOD:
            return semantic_find_expression_node_at_position(node->data.structMethod.body, position);

        case ZR_AST_CLASS_META_FUNCTION:
            return semantic_find_expression_node_at_position(node->data.classMetaFunction.body, position);

        case ZR_AST_STRUCT_META_FUNCTION:
            return semantic_find_expression_node_at_position(node->data.structMetaFunction.body, position);

        case ZR_AST_CLASS_PROPERTY:
            return semantic_find_expression_node_at_position(node->data.classProperty.modifier, position);

        case ZR_AST_PROPERTY_GET:
            return semantic_find_expression_node_at_position(node->data.propertyGet.body, position);

        case ZR_AST_PROPERTY_SET:
            return semantic_find_expression_node_at_position(node->data.propertySet.body, position);

        case ZR_AST_EXPRESSION_STATEMENT:
            return semantic_find_expression_node_at_position(node->data.expressionStatement.expr, position);

        case ZR_AST_RETURN_STATEMENT:
            return semantic_find_expression_node_at_position(node->data.returnStatement.expr, position);

        case ZR_AST_PRIMARY_EXPRESSION:
            nested = semantic_find_expression_node_at_position(node->data.primaryExpression.property, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = semantic_find_expression_node_in_array(node->data.primaryExpression.members, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return semantic_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_MEMBER_EXPRESSION:
            nested = semantic_find_expression_node_at_position(node->data.memberExpression.property, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return semantic_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_CONSTRUCT_EXPRESSION:
            nested = semantic_find_expression_node_at_position(node->data.constructExpression.target, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = semantic_find_expression_node_in_array(node->data.constructExpression.args, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return semantic_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            nested = semantic_find_expression_node_at_position(node->data.assignmentExpression.left, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = semantic_find_expression_node_at_position(node->data.assignmentExpression.right, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return semantic_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_BINARY_EXPRESSION:
            nested = semantic_find_expression_node_at_position(node->data.binaryExpression.left, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = semantic_find_expression_node_at_position(node->data.binaryExpression.right, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return semantic_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_LOGICAL_EXPRESSION:
            nested = semantic_find_expression_node_at_position(node->data.logicalExpression.left, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = semantic_find_expression_node_at_position(node->data.logicalExpression.right, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return semantic_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_UNARY_EXPRESSION:
            nested = semantic_find_expression_node_at_position(node->data.unaryExpression.argument, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return semantic_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_FUNCTION_CALL:
            nested = semantic_find_expression_node_in_array(node->data.functionCall.args, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = semantic_find_expression_node_in_array(node->data.functionCall.genericArguments, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return semantic_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_ARRAY_LITERAL:
            nested = semantic_find_expression_node_in_array(node->data.arrayLiteral.elements, position);
            return nested != ZR_NULL ? nested : (semantic_node_contains_position(node, position) ? node : ZR_NULL);

        case ZR_AST_OBJECT_LITERAL:
            nested = semantic_find_expression_node_in_array(node->data.objectLiteral.properties, position);
            return nested != ZR_NULL ? nested : (semantic_node_contains_position(node, position) ? node : ZR_NULL);

        case ZR_AST_KEY_VALUE_PAIR:
            nested = semantic_find_expression_node_at_position(node->data.keyValuePair.key, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = semantic_find_expression_node_at_position(node->data.keyValuePair.value, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return semantic_node_contains_position(node, position) ? node : ZR_NULL;

        case ZR_AST_IF_EXPRESSION:
            nested = semantic_find_expression_node_at_position(node->data.ifExpression.condition, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = semantic_find_expression_node_at_position(node->data.ifExpression.thenExpr, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = semantic_find_expression_node_at_position(node->data.ifExpression.elseExpr, position);
            return nested != ZR_NULL ? nested : (semantic_node_contains_position(node, position) ? node : ZR_NULL);

        case ZR_AST_SWITCH_EXPRESSION:
            nested = semantic_find_expression_node_at_position(node->data.switchExpression.expr, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = semantic_find_expression_node_in_array(node->data.switchExpression.cases, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = semantic_find_expression_node_at_position(node->data.switchExpression.defaultCase, position);
            return nested != ZR_NULL ? nested : (semantic_node_contains_position(node, position) ? node : ZR_NULL);

        case ZR_AST_SWITCH_CASE:
            nested = semantic_find_expression_node_at_position(node->data.switchCase.value, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return semantic_find_expression_node_at_position(node->data.switchCase.block, position);

        case ZR_AST_SWITCH_DEFAULT:
            return semantic_find_expression_node_at_position(node->data.switchDefault.block, position);

        case ZR_AST_WHILE_LOOP:
            nested = semantic_find_expression_node_at_position(node->data.whileLoop.cond, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return semantic_find_expression_node_at_position(node->data.whileLoop.block, position);

        case ZR_AST_FOR_LOOP:
            nested = semantic_find_expression_node_at_position(node->data.forLoop.init, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = semantic_find_expression_node_at_position(node->data.forLoop.cond, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            nested = semantic_find_expression_node_at_position(node->data.forLoop.step, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return semantic_find_expression_node_at_position(node->data.forLoop.block, position);

        case ZR_AST_FOREACH_LOOP:
            nested = semantic_find_expression_node_at_position(node->data.foreachLoop.expr, position);
            if (nested != ZR_NULL) {
                return nested;
            }
            return semantic_find_expression_node_at_position(node->data.foreachLoop.block, position);

        case ZR_AST_LAMBDA_EXPRESSION:
            nested = semantic_find_expression_node_at_position(node->data.lambdaExpression.block, position);
            return nested != ZR_NULL ? nested : (semantic_node_contains_position(node, position) ? node : ZR_NULL);

        case ZR_AST_IMPORT_EXPRESSION:
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_TEMPLATE_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
        case ZR_AST_IDENTIFIER_LITERAL:
            return semantic_node_contains_position(node, position) ? node : ZR_NULL;

        default:
            return ZR_NULL;
    }
}

static SZrAstNode *semantic_find_expression_node_in_array(SZrAstNodeArray *nodes, SZrFileRange position) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        SZrAstNode *nested = semantic_find_expression_node_at_position(nodes->nodes[index], position);
        if (nested != ZR_NULL) {
            return nested;
        }
    }

    return ZR_NULL;
}

static const SZrSemanticTypeRecord *semantic_find_type_record_by_id(const SZrSemanticContext *context,
                                                                    TZrTypeId typeId) {
    if (context == ZR_NULL || typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < context->types.length; index++) {
        const SZrSemanticTypeRecord *record =
            (const SZrSemanticTypeRecord *)ZrCore_Array_Get((SZrArray *)&context->types, index);
        if (record != ZR_NULL && record->id == typeId) {
            return record;
        }
    }

    return ZR_NULL;
}

static const SZrInferredType *semantic_symbol_display_type_info(SZrSemanticAnalyzer *analyzer, SZrSymbol *symbol) {
    const SZrSymbol *canonicalSymbol = ZR_NULL;

    if (symbol == ZR_NULL) {
        return ZR_NULL;
    }

    if (ZrLanguageServer_SemanticAnalyzer_IsPreciseInferredType(symbol->typeInfo)) {
        return symbol->typeInfo;
    }

    if (analyzer != ZR_NULL && analyzer->symbolTable != ZR_NULL && symbol->name != ZR_NULL) {
        canonicalSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, symbol->name, ZR_NULL);
        if (canonicalSymbol != ZR_NULL &&
            ZrLanguageServer_SemanticAnalyzer_IsPreciseInferredType(canonicalSymbol->typeInfo)) {
            return canonicalSymbol->typeInfo;
        }
    }

    if (analyzer != ZR_NULL &&
        analyzer->semanticContext != ZR_NULL &&
        (symbol->semanticId != ZR_SEMANTIC_ID_INVALID ||
         (canonicalSymbol != ZR_NULL && canonicalSymbol->semanticId != ZR_SEMANTIC_ID_INVALID))) {
        TZrSymbolId semanticId = symbol->semanticId != ZR_SEMANTIC_ID_INVALID
                                     ? symbol->semanticId
                                     : canonicalSymbol->semanticId;
        for (TZrSize index = 0; index < analyzer->semanticContext->symbols.length; index++) {
            const SZrSemanticSymbolRecord *symbolRecord = (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                (SZrArray *)&analyzer->semanticContext->symbols,
                index);
            const SZrSemanticTypeRecord *typeRecord;

            if (symbolRecord == ZR_NULL || symbolRecord->id != semanticId) {
                continue;
            }

            typeRecord = semantic_find_type_record_by_id(analyzer->semanticContext, symbolRecord->typeId);
            if (typeRecord != ZR_NULL &&
                ZrLanguageServer_SemanticAnalyzer_IsPreciseInferredType(&typeRecord->inferredType)) {
                return &typeRecord->inferredType;
            }
            break;
        }
    }

    return symbol->typeInfo;
}

static SZrString *semantic_extract_direct_identifier_name(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return node->data.identifier.name;
    }

    if (node->type == ZR_AST_PRIMARY_EXPRESSION &&
        node->data.primaryExpression.property != ZR_NULL &&
        node->data.primaryExpression.property->type == ZR_AST_IDENTIFIER_LITERAL &&
        (node->data.primaryExpression.members == ZR_NULL ||
         node->data.primaryExpression.members->count == 0)) {
        return node->data.primaryExpression.property->data.identifier.name;
    }

    return ZR_NULL;
}

static TZrBool semantic_ast_can_supply_signature(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_FUNCTION_DECLARATION:
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
        case ZR_AST_CLASS_METHOD:
        case ZR_AST_STRUCT_METHOD:
        case ZR_AST_CLASS_META_FUNCTION:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static SZrSymbol *semantic_resolve_callable_signature_symbol(SZrSemanticAnalyzer *analyzer,
                                                             SZrSymbol *symbol,
                                                             const SZrInferredType *displayTypeInfo) {
    SZrString *sourceName;
    SZrFileRange lookupPosition;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || symbol == ZR_NULL ||
        displayTypeInfo == ZR_NULL || displayTypeInfo->baseType != ZR_VALUE_TYPE_CLOSURE ||
        symbol->astNode == ZR_NULL || symbol->astNode->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_NULL;
    }

    sourceName = semantic_extract_direct_identifier_name(symbol->astNode->data.variableDeclaration.value);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    lookupPosition = symbol->astNode->data.variableDeclaration.value != ZR_NULL
                         ? symbol->astNode->data.variableDeclaration.value->location
                         : symbol->location;
    symbol = ZrLanguageServer_SymbolTable_LookupAtPosition(analyzer->symbolTable, sourceName, lookupPosition);
    if (symbol != ZR_NULL && semantic_ast_can_supply_signature(symbol->astNode)) {
        return symbol;
    }

    return ZR_NULL;
}

static const TZrChar *semantic_symbol_kind_text(EZrSymbolType type) {
    switch (type) {
        case ZR_SYMBOL_FUNCTION: return "function";
        case ZR_SYMBOL_CLASS: return "class";
        case ZR_SYMBOL_STRUCT: return "struct";
        case ZR_SYMBOL_METHOD: return "method";
        case ZR_SYMBOL_PROPERTY: return "property";
        case ZR_SYMBOL_FIELD: return "field";
        case ZR_SYMBOL_PARAMETER: return "parameter";
        case ZR_SYMBOL_ENUM: return "enum";
        case ZR_SYMBOL_ENUM_MEMBER: return "enum member";
        case ZR_SYMBOL_INTERFACE: return "interface";
        case ZR_SYMBOL_MODULE: return "module";
        default: return "variable";
    }
}

static TZrBool semantic_file_range_contains_range(SZrFileRange outer, SZrFileRange inner) {
    if (!semantic_source_uri_equals(outer.source, inner.source)) {
        return ZR_FALSE;
    }

    if (outer.start.offset > 0 && outer.end.offset > 0 &&
        inner.start.offset > 0 && inner.end.offset > 0) {
        return outer.start.offset <= inner.start.offset && inner.end.offset <= outer.end.offset;
    }

    return (outer.start.line < inner.start.line ||
            (outer.start.line == inner.start.line && outer.start.column <= inner.start.column)) &&
           (inner.end.line < outer.end.line ||
            (inner.end.line == outer.end.line && inner.end.column <= outer.end.column));
}

static TZrBool semantic_range_is_declared_in_extern_block(SZrAstNode *node, SZrFileRange range) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    if (semantic_range_is_declared_in_extern_block(node->data.script.statements->nodes[index], range)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return node->data.compileTimeDeclaration.declaration != ZR_NULL &&
                   semantic_range_is_declared_in_extern_block(node->data.compileTimeDeclaration.declaration, range);

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    if (semantic_range_is_declared_in_extern_block(node->data.block.body->nodes[index], range)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        case ZR_AST_EXTERN_BLOCK:
            if (node->data.externBlock.declarations != ZR_NULL &&
                node->data.externBlock.declarations->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.externBlock.declarations->count; index++) {
                    SZrAstNode *declaration = node->data.externBlock.declarations->nodes[index];
                    if (declaration != ZR_NULL &&
                        semantic_file_range_contains_range(declaration->location, range)) {
                        return ZR_TRUE;
                    }
                }
            }
            return ZR_FALSE;

        default:
            return ZR_FALSE;
    }
}

static TZrBool semantic_symbol_is_ffi_extern(SZrSemanticAnalyzer *analyzer, SZrSymbol *symbol) {
    if (analyzer == ZR_NULL || symbol == ZR_NULL || symbol->astNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (symbol->astNode->type == ZR_AST_EXTERN_FUNCTION_DECLARATION ||
        symbol->astNode->type == ZR_AST_EXTERN_DELEGATE_DECLARATION) {
        return ZR_TRUE;
    }

    return analyzer->ast != ZR_NULL &&
           semantic_range_is_declared_in_extern_block(analyzer->ast, symbol->astNode->location);
}

static const TZrChar *semantic_symbol_kind_text_for_symbol(SZrSemanticAnalyzer *analyzer, SZrSymbol *symbol) {
    if (semantic_symbol_is_ffi_extern(analyzer, symbol)) {
        if (symbol->astNode != ZR_NULL && symbol->astNode->type == ZR_AST_EXTERN_FUNCTION_DECLARATION) {
            return "extern function";
        }
        if (symbol->astNode != ZR_NULL && symbol->astNode->type == ZR_AST_EXTERN_DELEGATE_DECLARATION) {
            return "extern delegate";
        }
    }

    return semantic_symbol_kind_text(symbol != ZR_NULL ? symbol->type : ZR_SYMBOL_VARIABLE);
}

static const TZrChar *semantic_access_modifier_text(EZrAccessModifier accessModifier) {
    switch (accessModifier) {
        case ZR_ACCESS_PUBLIC: return "public";
        case ZR_ACCESS_PROTECTED: return "protected";
        case ZR_ACCESS_PRIVATE:
        default:
            return "private";
    }
}

static const TZrChar *semantic_parameter_passing_mode_text(EZrParameterPassingMode passingMode) {
    switch (passingMode) {
        case ZR_PARAMETER_PASSING_MODE_IN: return "%in";
        case ZR_PARAMETER_PASSING_MODE_OUT: return "%out";
        case ZR_PARAMETER_PASSING_MODE_REF: return "%ref";
        case ZR_PARAMETER_PASSING_MODE_VALUE:
        default:
            return ZR_NULL;
    }
}

static TZrSize semantic_buffer_append(TZrChar *buffer,
                                      TZrSize bufferSize,
                                      TZrSize offset,
                                      const TZrChar *format,
                                      ...) {
    va_list args;
    TZrInt32 written;

    if (buffer == ZR_NULL || bufferSize == 0 || format == ZR_NULL) {
        return offset;
    }

    if (offset >= bufferSize) {
        return bufferSize - 1;
    }

    va_start(args, format);
    written = vsnprintf(buffer + offset, bufferSize - offset, format, args);
    va_end(args);
    if (written < 0) {
        return offset;
    }

    if ((TZrSize)written >= bufferSize - offset) {
        return bufferSize - 1;
    }

    return offset + (TZrSize)written;
}

static TZrBool semantic_append_ast_type_decl(SZrType *typeInfo,
                                             TZrChar *buffer,
                                             TZrSize bufferSize,
                                             TZrSize *offset);

static const TZrChar *semantic_ast_ownership_prefix(EZrOwnershipQualifier ownershipQualifier) {
    switch (ownershipQualifier) {
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
            return "%unique ";
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
            return "%shared ";
        case ZR_OWNERSHIP_QUALIFIER_WEAK:
            return "%weak ";
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
            return "%borrowed ";
        case ZR_OWNERSHIP_QUALIFIER_LOANED:
            return "%loaned ";
        default:
            return "";
    }
}

static TZrBool semantic_append_ast_generic_argument_decl(SZrAstNode *node,
                                                         TZrChar *buffer,
                                                         TZrSize bufferSize,
                                                         TZrSize *offset) {
    TZrSize nextOffset;

    if (node == ZR_NULL || buffer == ZR_NULL || offset == ZR_NULL) {
        return ZR_FALSE;
    }

    nextOffset = *offset;
    switch (node->type) {
        case ZR_AST_TYPE:
            if (!semantic_append_ast_type_decl(&node->data.type, buffer, bufferSize, &nextOffset)) {
                return ZR_FALSE;
            }
            break;

        case ZR_AST_IDENTIFIER_LITERAL:
            nextOffset = semantic_buffer_append(buffer,
                                                bufferSize,
                                                nextOffset,
                                                "%s",
                                                semantic_string_native(node->data.identifier.name));
            break;

        case ZR_AST_INTEGER_LITERAL:
            nextOffset = semantic_buffer_append(buffer,
                                                bufferSize,
                                                nextOffset,
                                                "%lld",
                                                (long long)node->data.integerLiteral.value);
            break;

        default:
            nextOffset = semantic_buffer_append(buffer, bufferSize, nextOffset, "?");
            break;
    }

    *offset = nextOffset;
    return ZR_TRUE;
}

static TZrBool semantic_append_ast_type_decl(SZrType *typeInfo,
                                             TZrChar *buffer,
                                             TZrSize bufferSize,
                                             TZrSize *offset) {
    TZrSize nextOffset;
    const TZrChar *ownershipPrefix;

    if (typeInfo == ZR_NULL || typeInfo->name == ZR_NULL || buffer == ZR_NULL || offset == ZR_NULL) {
        return ZR_FALSE;
    }

    nextOffset = *offset;
    ownershipPrefix = semantic_ast_ownership_prefix(typeInfo->ownershipQualifier);
    if (ownershipPrefix[0] != '\0') {
        nextOffset = semantic_buffer_append(buffer, bufferSize, nextOffset, "%s", ownershipPrefix);
    }

    switch (typeInfo->name->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            nextOffset = semantic_buffer_append(buffer,
                                                bufferSize,
                                                nextOffset,
                                                "%s",
                                                semantic_string_native(typeInfo->name->data.identifier.name));
            break;

        case ZR_AST_GENERIC_TYPE: {
            SZrGenericType *genericType = &typeInfo->name->data.genericType;

            nextOffset = semantic_buffer_append(buffer,
                                                bufferSize,
                                                nextOffset,
                                                "%s<",
                                                semantic_string_native(genericType->name != ZR_NULL
                                                                           ? genericType->name->name
                                                                           : ZR_NULL));
            if (genericType->params != ZR_NULL) {
                for (TZrSize index = 0; index < genericType->params->count; index++) {
                    if (index > 0) {
                        nextOffset = semantic_buffer_append(buffer, bufferSize, nextOffset, ", ");
                    }
                    if (!semantic_append_ast_generic_argument_decl(genericType->params->nodes[index],
                                                                   buffer,
                                                                   bufferSize,
                                                                   &nextOffset)) {
                        return ZR_FALSE;
                    }
                }
            }
            nextOffset = semantic_buffer_append(buffer, bufferSize, nextOffset, ">");
            break;
        }

        case ZR_AST_TUPLE_TYPE: {
            SZrTupleType *tupleType = &typeInfo->name->data.tupleType;

            nextOffset = semantic_buffer_append(buffer, bufferSize, nextOffset, "(");
            if (tupleType->elements != ZR_NULL) {
                for (TZrSize index = 0; index < tupleType->elements->count; index++) {
                    if (index > 0) {
                        nextOffset = semantic_buffer_append(buffer, bufferSize, nextOffset, ", ");
                    }
                    if (!semantic_append_ast_generic_argument_decl(tupleType->elements->nodes[index],
                                                                   buffer,
                                                                   bufferSize,
                                                                   &nextOffset)) {
                        return ZR_FALSE;
                    }
                }
            }
            nextOffset = semantic_buffer_append(buffer, bufferSize, nextOffset, ")");
            break;
        }

        default:
            return ZR_FALSE;
    }

    if (typeInfo->subType != ZR_NULL) {
        nextOffset = semantic_buffer_append(buffer, bufferSize, nextOffset, ".");
        if (!semantic_append_ast_type_decl(typeInfo->subType, buffer, bufferSize, &nextOffset)) {
            return ZR_FALSE;
        }
    }

    for (TZrInt32 dimension = 0; dimension < typeInfo->dimensions; dimension++) {
        nextOffset = semantic_buffer_append(buffer, bufferSize, nextOffset, "[]");
    }

    *offset = nextOffset;
    return ZR_TRUE;
}

static const TZrChar *semantic_identifier_node_text(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL || node->data.identifier.name == ZR_NULL) {
        return ZR_NULL;
    }

    return semantic_string_native(node->data.identifier.name);
}

static const TZrChar *semantic_member_property_text(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_MEMBER_EXPRESSION || node->data.memberExpression.computed) {
        return ZR_NULL;
    }

    return semantic_identifier_node_text(node->data.memberExpression.property);
}

static TZrBool semantic_text_equals(const TZrChar *value, const TZrChar *expected) {
    return value != ZR_NULL && expected != ZR_NULL && strcmp(value, expected) == 0;
}

static SZrAstNodeArray *semantic_get_decorator_array_for_node(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_STRUCT_DECLARATION:
            return node->data.structDeclaration.decorators;
        case ZR_AST_STRUCT_FIELD:
            return node->data.structField.decorators;
        case ZR_AST_ENUM_DECLARATION:
            return node->data.enumDeclaration.decorators;
        case ZR_AST_ENUM_MEMBER:
            return node->data.enumMember.decorators;
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            return node->data.externFunctionDeclaration.decorators;
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            return node->data.externDelegateDeclaration.decorators;
        default:
            return ZR_NULL;
    }
}

static TZrBool semantic_extract_ffi_decorator(SZrAstNode *decoratorNode,
                                              const TZrChar **outLeafName,
                                              TZrBool *outHasCall,
                                              SZrFunctionCall **outCall) {
    SZrAstNode *expr;
    SZrPrimaryExpression *primary;
    SZrAstNode *ffiMember;
    SZrAstNode *leafMember;

    if (outLeafName != ZR_NULL) {
        *outLeafName = ZR_NULL;
    }
    if (outHasCall != ZR_NULL) {
        *outHasCall = ZR_FALSE;
    }
    if (outCall != ZR_NULL) {
        *outCall = ZR_NULL;
    }

    if (decoratorNode == ZR_NULL || decoratorNode->type != ZR_AST_DECORATOR_EXPRESSION) {
        return ZR_FALSE;
    }

    expr = decoratorNode->data.decoratorExpression.expr;
    if (expr == ZR_NULL || expr->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primary = &expr->data.primaryExpression;
    if (!semantic_text_equals(semantic_identifier_node_text(primary->property), "zr") ||
        primary->members == ZR_NULL || primary->members->count < 2 || primary->members->count > 3) {
        return ZR_FALSE;
    }

    ffiMember = primary->members->nodes[0];
    leafMember = primary->members->nodes[1];
    if (!semantic_text_equals(semantic_member_property_text(ffiMember), "ffi")) {
        return ZR_FALSE;
    }

    if (outLeafName != ZR_NULL) {
        *outLeafName = semantic_member_property_text(leafMember);
        if (*outLeafName == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    if (primary->members->count == 3) {
        SZrAstNode *callNode = primary->members->nodes[2];
        if (callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
            return ZR_FALSE;
        }
        if (outHasCall != ZR_NULL) {
            *outHasCall = ZR_TRUE;
        }
        if (outCall != ZR_NULL) {
            *outCall = &callNode->data.functionCall;
        }
    }

    return ZR_TRUE;
}

static TZrBool semantic_call_read_single_integer_arg(SZrFunctionCall *call, TZrInt64 *outValue) {
    SZrAstNode *arg;

    if (outValue != ZR_NULL) {
        *outValue = 0;
    }
    if (call == ZR_NULL || call->args == ZR_NULL || call->args->count != 1) {
        return ZR_FALSE;
    }

    arg = call->args->nodes[0];
    if (arg == ZR_NULL || arg->type != ZR_AST_INTEGER_LITERAL) {
        return ZR_FALSE;
    }

    if (outValue != ZR_NULL) {
        *outValue = arg->data.integerLiteral.value;
    }
    return ZR_TRUE;
}

static TZrBool semantic_call_read_single_string_arg(SZrFunctionCall *call, const TZrChar **outValue) {
    SZrAstNode *arg;

    if (outValue != ZR_NULL) {
        *outValue = ZR_NULL;
    }
    if (call == ZR_NULL || call->args == ZR_NULL || call->args->count != 1) {
        return ZR_FALSE;
    }

    arg = call->args->nodes[0];
    if (arg == ZR_NULL || arg->type != ZR_AST_STRING_LITERAL || arg->data.stringLiteral.value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outValue != ZR_NULL) {
        *outValue = semantic_string_native(arg->data.stringLiteral.value);
    }
    return ZR_TRUE;
}

static TZrSize semantic_append_ffi_hover_metadata(SZrAstNode *node,
                                                  TZrChar *buffer,
                                                  TZrSize bufferSize,
                                                  TZrSize offset) {
    SZrAstNodeArray *decorators;

    if (node == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return offset;
    }

    decorators = semantic_get_decorator_array_for_node(node);
    if (decorators == ZR_NULL || decorators->nodes == ZR_NULL) {
        return offset;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        TZrInt64 integerValue = 0;
        const TZrChar *stringValue = ZR_NULL;

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) ||
            leafName == ZR_NULL || !hasCall) {
            continue;
        }

        switch (node->type) {
            case ZR_AST_STRUCT_DECLARATION:
                if (semantic_text_equals(leafName, "pack") &&
                    semantic_call_read_single_integer_arg(call, &integerValue)) {
                    offset = semantic_buffer_append(buffer, bufferSize, offset, "\nPack: %lld", (long long)integerValue);
                } else if (semantic_text_equals(leafName, "align") &&
                           semantic_call_read_single_integer_arg(call, &integerValue)) {
                    offset =
                        semantic_buffer_append(buffer, bufferSize, offset, "\nAlign: %lld", (long long)integerValue);
                }
                break;

            case ZR_AST_STRUCT_FIELD:
                if (semantic_text_equals(leafName, "offset") &&
                    semantic_call_read_single_integer_arg(call, &integerValue)) {
                    offset =
                        semantic_buffer_append(buffer, bufferSize, offset, "\nOffset: %lld", (long long)integerValue);
                }
                break;

            case ZR_AST_ENUM_DECLARATION:
                if (semantic_text_equals(leafName, "underlying") &&
                    semantic_call_read_single_string_arg(call, &stringValue) &&
                    stringValue != ZR_NULL) {
                    offset = semantic_buffer_append(buffer, bufferSize, offset, "\nUnderlying: %s", stringValue);
                }
                break;

            case ZR_AST_ENUM_MEMBER:
                if (semantic_text_equals(leafName, "value") &&
                    semantic_call_read_single_integer_arg(call, &integerValue)) {
                    offset = semantic_buffer_append(buffer, bufferSize, offset, "\nValue: %lld", (long long)integerValue);
                }
                break;

            default:
                break;
        }
    }

    return offset;
}

static const TZrChar *semantic_exact_type_failure_text(void) {
    return "cannot infer exact type";
}

static TZrBool semantic_type_text_is_specific(const TZrChar *typeText) {
    return typeText != ZR_NULL && typeText[0] != '\0' &&
           strcmp(typeText, semantic_exact_type_failure_text()) != 0 &&
           strcmp(typeText, "object") != 0 &&
           strcmp(typeText, "unknown") != 0;
}

static const TZrChar *semantic_type_text_or_failure(const TZrChar *typeText) {
    return semantic_type_text_is_specific(typeText)
               ? typeText
               : semantic_exact_type_failure_text();
}

static const TZrChar *semantic_precise_inferred_type_text(SZrState *state,
                                                          const SZrInferredType *typeInfo,
                                                          TZrChar *buffer,
                                                          TZrSize bufferSize) {
    const TZrChar *typeText = ZR_NULL;

    if (state != ZR_NULL &&
        buffer != ZR_NULL &&
        bufferSize > 0 &&
        ZrLanguageServer_SemanticAnalyzer_IsPreciseInferredType(typeInfo)) {
        typeText =
            ZrParser_TypeNameString_Get(state, (SZrInferredType *)typeInfo, buffer, bufferSize);
        if (semantic_type_text_is_specific(typeText)) {
            return typeText;
        }
    }

    return semantic_exact_type_failure_text();
}

static void semantic_format_type_from_ast(SZrState *state,
                                          SZrCompilerState *compilerState,
                                          SZrType *typeNode,
                                          TZrChar *buffer,
                                          TZrSize bufferSize) {
    TZrSize offset = 0;

    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(compilerState);

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (typeNode != ZR_NULL &&
        semantic_append_ast_type_decl(typeNode, buffer, bufferSize, &offset) &&
        buffer[0] != '\0') {
        return;
    }

    if (typeNode != ZR_NULL &&
        typeNode->name != ZR_NULL &&
        typeNode->name->type == ZR_AST_IDENTIFIER_LITERAL &&
        typeNode->name->data.identifier.name != ZR_NULL) {
        TZrNativeString nameText;
        TZrSize nameLength;

        semantic_get_string_view(typeNode->name->data.identifier.name, &nameText, &nameLength);
        if (nameText != ZR_NULL && nameLength < bufferSize) {
            memcpy(buffer, nameText, nameLength);
            buffer[nameLength] = '\0';
            return;
        }
    }

    snprintf(buffer, bufferSize, "%s", semantic_exact_type_failure_text());
}

static void semantic_format_display_type_from_ast_or_inferred(SZrState *state,
                                                              SZrCompilerState *compilerState,
                                                              SZrType *typeNode,
                                                              const SZrInferredType *fallbackTypeInfo,
                                                              TZrChar *buffer,
                                                              TZrSize bufferSize) {
    const TZrChar *typeText;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    semantic_format_type_from_ast(state, compilerState, typeNode, buffer, bufferSize);
    if (semantic_type_text_is_specific(buffer)) {
        return;
    }

    typeText = semantic_precise_inferred_type_text(state, fallbackTypeInfo, buffer, bufferSize);
    if (typeText != buffer) {
        snprintf(buffer, bufferSize, "%s", typeText);
    }
}

static TZrSize semantic_append_generic_parameter_decl(SZrState *state,
                                                      SZrCompilerState *compilerState,
                                                      TZrChar *buffer,
                                                      TZrSize bufferSize,
                                                      TZrSize offset,
                                                      SZrAstNode *paramNode) {
    SZrParameter *parameter;
    TZrNativeString nameText;
    TZrSize nameLength;
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER || paramNode->data.parameter.name == ZR_NULL) {
        return offset;
    }

    parameter = &paramNode->data.parameter;
    semantic_get_string_view(parameter->name->name, &nameText, &nameLength);
    if (parameter->genericKind == ZR_GENERIC_PARAMETER_CONST_INT) {
        semantic_format_type_from_ast(state, compilerState, parameter->typeInfo, typeBuffer, sizeof(typeBuffer));
        return semantic_buffer_append(buffer,
                                      bufferSize,
                                      offset,
                                      "const %.*s: %s",
                                      (int)nameLength,
                                      nameText != ZR_NULL ? nameText : "",
                                      typeBuffer);
    }

    if (parameter->variance == ZR_GENERIC_VARIANCE_IN) {
        offset = semantic_buffer_append(buffer, bufferSize, offset, "in ");
    } else if (parameter->variance == ZR_GENERIC_VARIANCE_OUT) {
        offset = semantic_buffer_append(buffer, bufferSize, offset, "out ");
    }

    return semantic_buffer_append(buffer,
                                  bufferSize,
                                  offset,
                                  "%.*s",
                                  (int)nameLength,
                                  nameText != ZR_NULL ? nameText : "");
}

static TZrSize semantic_append_generic_declaration(SZrState *state,
                                                   SZrCompilerState *compilerState,
                                                   TZrChar *buffer,
                                                   TZrSize bufferSize,
                                                   TZrSize offset,
                                                   SZrGenericDeclaration *generic) {
    if (generic == ZR_NULL || generic->params == ZR_NULL || generic->params->count == 0) {
        return offset;
    }

    offset = semantic_buffer_append(buffer, bufferSize, offset, "<");
    for (TZrSize index = 0; index < generic->params->count; index++) {
        if (index > 0) {
            offset = semantic_buffer_append(buffer, bufferSize, offset, ", ");
        }
        offset = semantic_append_generic_parameter_decl(state,
                                                        compilerState,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        generic->params->nodes[index]);
    }
    return semantic_buffer_append(buffer, bufferSize, offset, ">");
}

static TZrBool semantic_parameter_has_constraints(SZrParameter *parameter) {
    return parameter != ZR_NULL &&
           (parameter->genericRequiresClass ||
            parameter->genericRequiresStruct ||
            parameter->genericRequiresNew ||
            (parameter->genericTypeConstraints != ZR_NULL && parameter->genericTypeConstraints->count > 0));
}

static TZrSize semantic_append_generic_constraints(SZrState *state,
                                                   SZrCompilerState *compilerState,
                                                   TZrChar *buffer,
                                                   TZrSize bufferSize,
                                                   TZrSize offset,
                                                   SZrParameter *parameter) {
    TZrBool firstConstraint = ZR_TRUE;
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    if (!semantic_parameter_has_constraints(parameter) || parameter->name == ZR_NULL) {
        return offset;
    }

    offset = semantic_buffer_append(buffer,
                                    bufferSize,
                                    offset,
                                    " where %s: ",
                                    semantic_string_native(parameter->name->name));
    if (parameter->genericRequiresClass) {
        offset = semantic_buffer_append(buffer, bufferSize, offset, "class");
        firstConstraint = ZR_FALSE;
    }
    if (parameter->genericRequiresStruct) {
        offset = semantic_buffer_append(buffer,
                                        bufferSize,
                                        offset,
                                        "%sstruct",
                                        firstConstraint ? "" : ", ");
        firstConstraint = ZR_FALSE;
    }
    if (parameter->genericTypeConstraints != ZR_NULL) {
        for (TZrSize index = 0; index < parameter->genericTypeConstraints->count; index++) {
            SZrAstNode *constraintNode = parameter->genericTypeConstraints->nodes[index];
            if (constraintNode == ZR_NULL || constraintNode->type != ZR_AST_TYPE) {
                continue;
            }
            semantic_format_type_from_ast(state,
                                          compilerState,
                                          &constraintNode->data.type,
                                          typeBuffer,
                                          sizeof(typeBuffer));
            offset = semantic_buffer_append(buffer,
                                            bufferSize,
                                            offset,
                                            "%s%s",
                                            firstConstraint ? "" : ", ",
                                            typeBuffer);
            firstConstraint = ZR_FALSE;
        }
    }
    if (parameter->genericRequiresNew) {
        offset = semantic_buffer_append(buffer,
                                        bufferSize,
                                        offset,
                                        "%snew()",
                                        firstConstraint ? "" : ", ");
    }

    return offset;
}

static TZrSize semantic_append_where_clauses(SZrState *state,
                                             SZrCompilerState *compilerState,
                                             TZrChar *buffer,
                                             TZrSize bufferSize,
                                             TZrSize offset,
                                             SZrGenericDeclaration *generic) {
    if (generic == ZR_NULL || generic->params == ZR_NULL) {
        return offset;
    }

    for (TZrSize index = 0; index < generic->params->count; index++) {
        SZrAstNode *paramNode = generic->params->nodes[index];
        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }
        offset = semantic_append_generic_constraints(state,
                                                     compilerState,
                                                     buffer,
                                                     bufferSize,
                                                     offset,
                                                     &paramNode->data.parameter);
    }

    return offset;
}

static TZrSize semantic_append_inheritance_clause(SZrState *state,
                                                  SZrCompilerState *compilerState,
                                                  TZrChar *buffer,
                                                  TZrSize bufferSize,
                                                  TZrSize offset,
                                                  SZrAstNodeArray *inherits) {
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrBool appendedAny = ZR_FALSE;

    if (inherits == ZR_NULL || inherits->count == 0) {
        return offset;
    }

    for (TZrSize index = 0; index < inherits->count; index++) {
        SZrAstNode *inheritNode = inherits->nodes[index];

        if (inheritNode == ZR_NULL || inheritNode->type != ZR_AST_TYPE) {
            continue;
        }

        offset = semantic_buffer_append(buffer,
                                        bufferSize,
                                        offset,
                                        "%s",
                                        appendedAny ? ", " : " : ");
        appendedAny = ZR_TRUE;

        semantic_format_type_from_ast(state,
                                      compilerState,
                                      &inheritNode->data.type,
                                      typeBuffer,
                                      sizeof(typeBuffer));
        offset = semantic_buffer_append(buffer, bufferSize, offset, "%s", typeBuffer);
    }

    return offset;
}

static TZrSize semantic_append_parameter_list(SZrState *state,
                                              SZrCompilerState *compilerState,
                                              TZrChar *buffer,
                                              TZrSize bufferSize,
                                              TZrSize offset,
                                              SZrAstNodeArray *params) {
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    offset = semantic_buffer_append(buffer, bufferSize, offset, "(");
    if (params != ZR_NULL) {
        for (TZrSize index = 0; index < params->count; index++) {
            SZrAstNode *paramNode = params->nodes[index];
            SZrParameter *parameter;
            const TZrChar *passingModeText;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            parameter = &paramNode->data.parameter;
            if (index > 0) {
                offset = semantic_buffer_append(buffer, bufferSize, offset, ", ");
            }

            passingModeText = semantic_parameter_passing_mode_text(parameter->passingMode);
            if (passingModeText != ZR_NULL) {
                offset = semantic_buffer_append(buffer, bufferSize, offset, "%s ", passingModeText);
            }

            semantic_format_type_from_ast(state, compilerState, parameter->typeInfo, typeBuffer, sizeof(typeBuffer));
            offset = semantic_buffer_append(buffer,
                                            bufferSize,
                                            offset,
                                            "%s: %s",
                                            semantic_string_native(parameter->name != ZR_NULL ? parameter->name->name : ZR_NULL),
                                            typeBuffer);
        }
    }

    return semantic_buffer_append(buffer, bufferSize, offset, ")");
}

static TZrBool semantic_build_ast_signature(SZrState *state,
                                            SZrCompilerState *compilerState,
                                            const SZrInferredType *fallbackTypeInfo,
                                            SZrAstNode *node,
                                            TZrChar *buffer,
                                            TZrSize bufferSize) {
    TZrSize offset = 0;
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    if (buffer == ZR_NULL || bufferSize == 0 || node == ZR_NULL) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    switch (node->type) {
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            if (funcDecl->name == ZR_NULL || funcDecl->name->name == ZR_NULL) {
                return ZR_FALSE;
            }

            offset = semantic_buffer_append(buffer,
                                            bufferSize,
                                            offset,
                                            "%s",
                                            semantic_string_native(funcDecl->name->name));
            offset = semantic_append_generic_declaration(state,
                                                        compilerState,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        funcDecl->generic);
            offset = semantic_append_parameter_list(state,
                                                   compilerState,
                                                   buffer,
                                                   bufferSize,
                                                   offset,
                                                   funcDecl->params);
            semantic_format_display_type_from_ast_or_inferred(state,
                                                              compilerState,
                                                              funcDecl->returnType,
                                                              fallbackTypeInfo,
                                                              typeBuffer,
                                                              sizeof(typeBuffer));
            offset = semantic_buffer_append(buffer, bufferSize, offset, ": %s", typeBuffer);
            offset = semantic_append_where_clauses(state,
                                                   compilerState,
                                                   buffer,
                                                   bufferSize,
                                                   offset,
                                                   funcDecl->generic);
            return ZR_TRUE;
        }

        case ZR_AST_EXTERN_FUNCTION_DECLARATION: {
            SZrExternFunctionDeclaration *funcDecl = &node->data.externFunctionDeclaration;
            if (funcDecl->name == ZR_NULL || funcDecl->name->name == ZR_NULL) {
                return ZR_FALSE;
            }

            offset = semantic_buffer_append(buffer,
                                            bufferSize,
                                            offset,
                                            "%s",
                                            semantic_string_native(funcDecl->name->name));
            offset = semantic_append_parameter_list(state,
                                                    compilerState,
                                                    buffer,
                                                    bufferSize,
                                                    offset,
                                                    funcDecl->params);
            semantic_format_display_type_from_ast_or_inferred(state,
                                                              compilerState,
                                                              funcDecl->returnType,
                                                              fallbackTypeInfo,
                                                              typeBuffer,
                                                              sizeof(typeBuffer));
            offset = semantic_buffer_append(buffer, bufferSize, offset, ": %s", typeBuffer);
            return ZR_TRUE;
        }

        case ZR_AST_EXTERN_DELEGATE_DECLARATION: {
            SZrExternDelegateDeclaration *delegateDecl = &node->data.externDelegateDeclaration;
            if (delegateDecl->name == ZR_NULL || delegateDecl->name->name == ZR_NULL) {
                return ZR_FALSE;
            }

            offset = semantic_buffer_append(buffer,
                                            bufferSize,
                                            offset,
                                            "delegate %s",
                                            semantic_string_native(delegateDecl->name->name));
            offset = semantic_append_parameter_list(state,
                                                    compilerState,
                                                    buffer,
                                                    bufferSize,
                                                    offset,
                                                    delegateDecl->params);
            semantic_format_display_type_from_ast_or_inferred(state,
                                                              compilerState,
                                                              delegateDecl->returnType,
                                                              fallbackTypeInfo,
                                                              typeBuffer,
                                                              sizeof(typeBuffer));
            offset = semantic_buffer_append(buffer, bufferSize, offset, ": %s", typeBuffer);
            return ZR_TRUE;
        }

        case ZR_AST_CLASS_METHOD: {
            SZrClassMethod *method = &node->data.classMethod;
            if (method->name == ZR_NULL || method->name->name == ZR_NULL) {
                return ZR_FALSE;
            }

            offset = semantic_buffer_append(buffer,
                                            bufferSize,
                                            offset,
                                            "%s",
                                            semantic_string_native(method->name->name));
            offset = semantic_append_generic_declaration(state,
                                                        compilerState,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        method->generic);
            offset = semantic_append_parameter_list(state,
                                                   compilerState,
                                                   buffer,
                                                   bufferSize,
                                                   offset,
                                                   method->params);
            semantic_format_display_type_from_ast_or_inferred(state,
                                                              compilerState,
                                                              method->returnType,
                                                              fallbackTypeInfo,
                                                              typeBuffer,
                                                              sizeof(typeBuffer));
            offset = semantic_buffer_append(buffer, bufferSize, offset, ": %s", typeBuffer);
            offset = semantic_append_where_clauses(state,
                                                   compilerState,
                                                   buffer,
                                                   bufferSize,
                                                   offset,
                                                   method->generic);
            return ZR_TRUE;
        }

        case ZR_AST_INTERFACE_METHOD_SIGNATURE: {
            SZrInterfaceMethodSignature *method = &node->data.interfaceMethodSignature;
            if (method->name == ZR_NULL || method->name->name == ZR_NULL) {
                return ZR_FALSE;
            }

            offset = semantic_buffer_append(buffer,
                                            bufferSize,
                                            offset,
                                            "%s",
                                            semantic_string_native(method->name->name));
            offset = semantic_append_generic_declaration(state,
                                                        compilerState,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        method->generic);
            offset = semantic_append_parameter_list(state,
                                                   compilerState,
                                                   buffer,
                                                   bufferSize,
                                                   offset,
                                                   method->params);
            semantic_format_display_type_from_ast_or_inferred(state,
                                                              compilerState,
                                                              method->returnType,
                                                              fallbackTypeInfo,
                                                              typeBuffer,
                                                              sizeof(typeBuffer));
            offset = semantic_buffer_append(buffer, bufferSize, offset, ": %s", typeBuffer);
            offset = semantic_append_where_clauses(state,
                                                   compilerState,
                                                   buffer,
                                                   bufferSize,
                                                   offset,
                                                   method->generic);
            return ZR_TRUE;
        }

        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            if (classDecl->name == ZR_NULL || classDecl->name->name == ZR_NULL) {
                return ZR_FALSE;
            }

            offset = semantic_buffer_append(buffer,
                                            bufferSize,
                                            offset,
                                            "class %s",
                                            semantic_string_native(classDecl->name->name));
            offset = semantic_append_generic_declaration(state,
                                                        compilerState,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        classDecl->generic);
            offset = semantic_append_inheritance_clause(state,
                                                        compilerState,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        classDecl->inherits);
            offset = semantic_append_where_clauses(state,
                                                   compilerState,
                                                   buffer,
                                                   bufferSize,
                                                   offset,
                                                   classDecl->generic);
            return ZR_TRUE;
        }

        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            if (structDecl->name == ZR_NULL || structDecl->name->name == ZR_NULL) {
                return ZR_FALSE;
            }

            offset = semantic_buffer_append(buffer,
                                            bufferSize,
                                            offset,
                                            "struct %s",
                                            semantic_string_native(structDecl->name->name));
            offset = semantic_append_generic_declaration(state,
                                                        compilerState,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        structDecl->generic);
            offset = semantic_append_inheritance_clause(state,
                                                        compilerState,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        structDecl->inherits);
            offset = semantic_append_where_clauses(state,
                                                   compilerState,
                                                   buffer,
                                                   bufferSize,
                                                   offset,
                                                   structDecl->generic);
            return ZR_TRUE;
        }

        case ZR_AST_INTERFACE_DECLARATION: {
            SZrInterfaceDeclaration *interfaceDecl = &node->data.interfaceDeclaration;
            if (interfaceDecl->name == ZR_NULL || interfaceDecl->name->name == ZR_NULL) {
                return ZR_FALSE;
            }

            offset = semantic_buffer_append(buffer,
                                            bufferSize,
                                            offset,
                                            "interface %s",
                                            semantic_string_native(interfaceDecl->name->name));
            offset = semantic_append_generic_declaration(state,
                                                        compilerState,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        interfaceDecl->generic);
            offset = semantic_append_inheritance_clause(state,
                                                        compilerState,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        interfaceDecl->inherits);
            offset = semantic_append_where_clauses(state,
                                                   compilerState,
                                                   buffer,
                                                   bufferSize,
                                                   offset,
                                                   interfaceDecl->generic);
            return ZR_TRUE;
        }

        default:
            return ZR_FALSE;
    }
}

static void semantic_build_symbol_detail(SZrState *state,
                                         SZrSemanticAnalyzer *analyzer,
                                         SZrCompilerState *compilerState,
                                         SZrSymbol *symbol,
                                         TZrChar *buffer,
                                         TZrSize bufferSize) {
    TZrChar signatureBuffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    const TZrChar *typeText = semantic_symbol_kind_text(symbol != ZR_NULL ? symbol->type : ZR_SYMBOL_VARIABLE);
    const TZrChar *accessText = semantic_access_modifier_text(symbol != ZR_NULL
                                                              ? symbol->accessModifier
                                                              : ZR_ACCESS_PRIVATE);
    const SZrInferredType *displayTypeInfo = semantic_symbol_display_type_info(analyzer, symbol);
    SZrSymbol *signatureSymbol = semantic_resolve_callable_signature_symbol(analyzer, symbol, displayTypeInfo);
    const SZrInferredType *signatureTypeInfo =
        signatureSymbol != ZR_NULL ? semantic_symbol_display_type_info(analyzer, signatureSymbol) : displayTypeInfo;
    SZrAstNode *signatureNode = signatureSymbol != ZR_NULL ? signatureSymbol->astNode : (symbol != ZR_NULL ? symbol->astNode : ZR_NULL);

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (state != ZR_NULL &&
        symbol != ZR_NULL &&
        signatureNode != ZR_NULL &&
        semantic_build_ast_signature(state,
                                     compilerState,
                                     signatureTypeInfo,
                                     signatureNode,
                                     signatureBuffer,
                                     sizeof(signatureBuffer))) {
        snprintf(buffer, bufferSize, "%s %s", accessText, signatureBuffer);
        return;
    }

    if (state != ZR_NULL && symbol != ZR_NULL) {
        typeText = semantic_precise_inferred_type_text(state, displayTypeInfo, typeBuffer, sizeof(typeBuffer));
    }

    snprintf(buffer, bufferSize, "%s %s", accessText, typeText != ZR_NULL ? typeText : semantic_exact_type_failure_text());
}

static TZrBool semantic_completion_has_label(SZrArray *items, const TZrChar *label) {
    if (items == ZR_NULL || label == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < items->length; index++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(items, index);
        TZrNativeString itemLabel;
        TZrSize itemLabelLength;
        TZrSize labelLength;

        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL || (*itemPtr)->label == ZR_NULL) {
            continue;
        }

        semantic_get_string_view((*itemPtr)->label, &itemLabel, &itemLabelLength);
        labelLength = strlen(label);
        if (itemLabel != ZR_NULL &&
            itemLabelLength == labelLength &&
            memcmp(itemLabel, label, labelLength) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void semantic_append_completion_item(SZrState *state,
                                            SZrArray *result,
                                            const TZrChar *label,
                                            const TZrChar *kind,
                                            const TZrChar *detail,
                                            SZrInferredType *typeInfo) {
    SZrCompletionItem *item;

    if (state == ZR_NULL || result == ZR_NULL || label == ZR_NULL ||
        semantic_completion_has_label(result, label)) {
        return;
    }

    item = ZrLanguageServer_CompletionItem_New(state, label, kind, detail, ZR_NULL, typeInfo);
    if (item != ZR_NULL) {
        ZrCore_Array_Push(state, result, &item);
    }
}

static void semantic_append_symbol_completion(SZrState *state,
                                              SZrSemanticAnalyzer *analyzer,
                                              SZrCompilerState *compilerState,
                                              SZrArray *result,
                                              SZrSymbol *symbol) {
    TZrNativeString nameText;
    TZrSize nameLength;
    TZrChar label[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrChar detail[ZR_LSP_COMPLETION_DETAIL_BUFFER_LENGTH];

    if (state == ZR_NULL || result == ZR_NULL || symbol == ZR_NULL || symbol->name == ZR_NULL) {
        return;
    }

    semantic_get_string_view(symbol->name, &nameText, &nameLength);
    if (nameText == ZR_NULL || nameLength == 0 || nameLength >= sizeof(label)) {
        return;
    }

    memcpy(label, nameText, nameLength);
    label[nameLength] = '\0';
    semantic_build_symbol_detail(state, analyzer, compilerState, symbol, detail, sizeof(detail));
    semantic_append_completion_item(state,
                                    result,
                                    label,
                                    semantic_symbol_kind_text(symbol->type),
                                    detail,
                                    symbol->typeInfo);
}

static const TZrChar *semantic_module_exported_prototype_kind(EZrObjectPrototypeType type) {
    switch (type) {
        case ZR_OBJECT_PROTOTYPE_TYPE_CLASS:
            return "class";
        case ZR_OBJECT_PROTOTYPE_TYPE_STRUCT:
            return "struct";
        case ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE:
            return "interface";
        case ZR_OBJECT_PROTOTYPE_TYPE_MODULE:
            return "module";
        default:
            return ZR_NULL;
    }
}

static void semantic_append_module_prototype_completions(SZrState *state,
                                                         SZrSemanticAnalyzer *analyzer,
                                                         const SZrTypePrototypeInfo *modulePrototype,
                                                         SZrArray *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || modulePrototype == ZR_NULL || result == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < modulePrototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
            (const SZrTypeMemberInfo *)ZrCore_Array_Get((SZrArray *)&modulePrototype->members, index);
        const TZrChar *kind = ZR_NULL;
        const TZrChar *detailText = semantic_exact_type_failure_text();
        TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];

        if (member == ZR_NULL || member->name == ZR_NULL || member->isMetaMethod) {
            continue;
        }

        if (member->memberType == ZR_AST_CLASS_METHOD || member->memberType == ZR_AST_STRUCT_METHOD) {
            kind = "function";
            detailText = semantic_string_native(member->returnTypeName);
            snprintf(detail,
                     sizeof(detail),
                     "%s(...): %s",
                     semantic_string_native(member->name),
                     semantic_type_text_or_failure(detailText));
        } else {
            const SZrTypePrototypeInfo *exportedType =
                member->fieldTypeName != ZR_NULL
                    ? ZrLanguageServer_LspModuleMetadata_FindTypePrototype(analyzer,
                                                                          semantic_string_native(member->fieldTypeName))
                    : ZR_NULL;

            if (exportedType != ZR_NULL) {
                kind = semantic_module_exported_prototype_kind(exportedType->type);
            }
            if (kind == ZR_NULL && member->memberType == ZR_AST_CLASS_PROPERTY) {
                kind = "property";
            }
            if (kind == ZR_NULL) {
                kind = "field";
            }

            detailText = member->fieldTypeName != ZR_NULL
                             ? semantic_string_native(member->fieldTypeName)
                             : semantic_exact_type_failure_text();
            snprintf(detail,
                     sizeof(detail),
                     "%s %s",
                     kind,
                     semantic_type_text_or_failure(detailText));
        }

        semantic_append_completion_item(state,
                                        result,
                                        semantic_string_native(member->name),
                                        kind != ZR_NULL ? kind : "symbol",
                                        detail,
                                        ZR_NULL);
    }
}

static const ZrLibModuleDescriptor *semantic_resolve_native_module_descriptor(SZrState *state,
                                                                              const TZrChar *moduleName) {
    return ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(state, moduleName, ZR_NULL);
}

static void semantic_append_native_module_descriptor_completions(SZrState *state,
                                                                 const ZrLibModuleDescriptor *descriptor,
                                                                 SZrArray *result,
                                                                 TZrSize depth) {
    if (state == ZR_NULL || descriptor == ZR_NULL || result == ZR_NULL ||
        depth > ZR_LSP_NATIVE_MODULE_COMPLETION_MAX_DEPTH) {
        return;
    }

    for (TZrSize index = 0; index < descriptor->functionCount; index++) {
        const ZrLibFunctionDescriptor *functionDescriptor = &descriptor->functions[index];
        TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];

        if (functionDescriptor->name == ZR_NULL) {
            continue;
        }

        snprintf(detail,
                 sizeof(detail),
                 "%s(...): %s",
                 functionDescriptor->name,
                 semantic_type_text_or_failure(functionDescriptor->returnTypeName));
        semantic_append_completion_item(state, result, functionDescriptor->name, "function", detail, ZR_NULL);
    }

    for (TZrSize index = 0; index < descriptor->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[index];
        const TZrChar *kind;
        TZrChar detail[ZR_LSP_DETAIL_BUFFER_LENGTH];

        if (typeDescriptor->name == ZR_NULL) {
            continue;
        }

        kind = (typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) ? "class" : "struct";
        snprintf(detail, sizeof(detail), "%s %s", kind, typeDescriptor->name);
        semantic_append_completion_item(state, result, typeDescriptor->name, kind, detail, ZR_NULL);

        for (TZrSize methodIndex = 0; methodIndex < typeDescriptor->methodCount; methodIndex++) {
            const ZrLibMethodDescriptor *methodDescriptor = &typeDescriptor->methods[methodIndex];
            TZrChar methodDetail[ZR_LSP_DETAIL_BUFFER_LENGTH];

            if (methodDescriptor->name == ZR_NULL) {
                continue;
            }

            snprintf(methodDetail,
                     sizeof(methodDetail),
                     "%s(...): %s",
                     methodDescriptor->name,
                     semantic_type_text_or_failure(methodDescriptor->returnTypeName));
            semantic_append_completion_item(state, result, methodDescriptor->name, "method", methodDetail, ZR_NULL);
        }
    }

    for (TZrSize index = 0; index < descriptor->moduleLinkCount; index++) {
        const ZrLibModuleLinkDescriptor *linkDescriptor = &descriptor->moduleLinks[index];
        const ZrLibModuleDescriptor *linkedDescriptor;

        if (linkDescriptor->moduleName == ZR_NULL) {
            continue;
        }

        linkedDescriptor = semantic_resolve_native_module_descriptor(state, linkDescriptor->moduleName);
        semantic_append_native_module_descriptor_completions(state, linkedDescriptor, result, depth + 1);
    }
}

static void semantic_append_imported_module_completions(SZrState *state,
                                                        SZrSemanticAnalyzer *analyzer,
                                                        SZrAstNode *node,
                                                        SZrArray *result) {
    const ZrLibModuleDescriptor *descriptor;
    const SZrTypePrototypeInfo *modulePrototype;
    TZrNativeString moduleText;
    TZrSize moduleLength;
    TZrChar moduleName[ZR_LSP_TEXT_BUFFER_LENGTH];

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    semantic_append_imported_module_completions(state,
                                                                analyzer,
                                                                node->data.script.statements->nodes[index],
                                                                result);
                }
            }
            return;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    semantic_append_imported_module_completions(state,
                                                                analyzer,
                                                                node->data.block.body->nodes[index],
                                                                result);
                }
            }
            return;

        case ZR_AST_FUNCTION_DECLARATION:
            semantic_append_imported_module_completions(state, analyzer, node->data.functionDeclaration.body, result);
            return;

        case ZR_AST_TEST_DECLARATION:
            semantic_append_imported_module_completions(state, analyzer, node->data.testDeclaration.body, result);
            return;

        case ZR_AST_VARIABLE_DECLARATION:
            if (node->data.variableDeclaration.value == ZR_NULL ||
                node->data.variableDeclaration.value->type != ZR_AST_IMPORT_EXPRESSION ||
                node->data.variableDeclaration.value->data.importExpression.modulePath == ZR_NULL ||
                node->data.variableDeclaration.value->data.importExpression.modulePath->type != ZR_AST_STRING_LITERAL ||
                node->data.variableDeclaration.value->data.importExpression.modulePath->data.stringLiteral.value == ZR_NULL) {
                return;
            }

            semantic_get_string_view(node->data.variableDeclaration.value->data.importExpression.modulePath->data.stringLiteral.value,
                                     &moduleText,
                                     &moduleLength);
            if (moduleText == ZR_NULL || moduleLength == 0 || moduleLength >= sizeof(moduleName)) {
                return;
            }

            memcpy(moduleName, moduleText, moduleLength);
            moduleName[moduleLength] = '\0';
            modulePrototype = ZrLanguageServer_LspModuleMetadata_FindTypePrototype(analyzer, moduleName);
            if (modulePrototype != ZR_NULL) {
                semantic_append_module_prototype_completions(state, analyzer, modulePrototype, result);
            }

            descriptor = semantic_resolve_native_module_descriptor(state, moduleName);
            semantic_append_native_module_descriptor_completions(state, descriptor, result, 0);
            return;

        default:
            return;
    }
}

SZrSemanticAnalyzer *ZrLanguageServer_SemanticAnalyzer_New(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrSemanticAnalyzer));
    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }
    
    analyzer->state = state;
    analyzer->symbolTable = ZrLanguageServer_SymbolTable_New(state);
    analyzer->referenceTracker = ZR_NULL;
    analyzer->ast = ZR_NULL;
    analyzer->cache = ZR_NULL;
    analyzer->enableCache = ZR_TRUE; // 默认启用缓存
    analyzer->compilerState = ZR_NULL; // 延迟创建
    analyzer->semanticContext = ZR_NULL;
    analyzer->hirModule = ZR_NULL;
    
    if (analyzer->symbolTable == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
        return ZR_NULL;
    }
    
    analyzer->referenceTracker = ZrLanguageServer_ReferenceTracker_New(state, analyzer->symbolTable);
    if (analyzer->referenceTracker == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
        ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
        return ZR_NULL;
    }
    
    ZrCore_Array_Init(state, &analyzer->diagnostics, sizeof(SZrDiagnostic *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    
    // 创建缓存
    analyzer->cache = (SZrAnalysisCache *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrAnalysisCache));
    if (analyzer->cache != ZR_NULL) {
        analyzer->cache->isValid = ZR_FALSE;
        analyzer->cache->astHash = 0;
        ZrCore_Array_Init(state,
                          &analyzer->cache->cachedDiagnostics,
                          sizeof(SZrDiagnostic *),
                          ZR_LSP_ARRAY_INITIAL_CAPACITY);
        ZrCore_Array_Init(state,
                          &analyzer->cache->cachedSymbols,
                          sizeof(SZrSymbol *),
                          ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }
    
    return analyzer;
}

static void semantic_analyzer_clear_cached_diagnostic_refs(SZrSemanticAnalyzer *analyzer) {
    if (analyzer == ZR_NULL || analyzer->cache == ZR_NULL || !analyzer->cache->cachedDiagnostics.isValid) {
        return;
    }

    analyzer->cache->cachedDiagnostics.length = 0;
}

static void semantic_analyzer_release_diagnostics(SZrState *state,
                                                  SZrSemanticAnalyzer *analyzer,
                                                  TZrBool resetStorage) {
    TZrSize capacity;

    if (state == ZR_NULL || analyzer == ZR_NULL || !analyzer->diagnostics.isValid) {
        return;
    }

    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        SZrDiagnostic *diagnostic = diagPtr != ZR_NULL ? *diagPtr : ZR_NULL;
        TZrBool alreadyReleased = ZR_FALSE;

        if (diagnostic == ZR_NULL) {
            continue;
        }

        for (TZrSize previousIndex = 0; previousIndex < i; previousIndex++) {
            SZrDiagnostic **previousPtr =
                (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, previousIndex);
            if (previousPtr != ZR_NULL && *previousPtr == diagnostic) {
                alreadyReleased = ZR_TRUE;
                break;
            }
        }

        if (!alreadyReleased) {
            ZrLanguageServer_Diagnostic_Free(state, diagnostic);
        }

        if (diagPtr != ZR_NULL) {
            *diagPtr = ZR_NULL;
        }
    }

    semantic_analyzer_clear_cached_diagnostic_refs(analyzer);

    if (resetStorage) {
        capacity = analyzer->diagnostics.capacity > 0 ? analyzer->diagnostics.capacity : ZR_LSP_ARRAY_INITIAL_CAPACITY;
        ZrCore_Array_Free(state, &analyzer->diagnostics);
        ZrCore_Array_Init(state, &analyzer->diagnostics, sizeof(SZrDiagnostic *), capacity);
        return;
    }

    analyzer->diagnostics.length = 0;
}

// 释放语义分析器
void ZrLanguageServer_SemanticAnalyzer_Free(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return;
    }

    semantic_analyzer_release_diagnostics(state, analyzer, ZR_FALSE);
    ZrCore_Array_Free(state, &analyzer->diagnostics);
    
    // 释放缓存
    if (analyzer->cache != ZR_NULL) {
        // 释放缓存的诊断信息
        for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
            SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->cache->cachedDiagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                // 注意：诊断可能在 analyzer->diagnostics 中，避免重复释放
            }
        }
        ZrCore_Array_Free(state, &analyzer->cache->cachedDiagnostics);
        ZrCore_Array_Free(state, &analyzer->cache->cachedSymbols);
        ZrCore_Memory_RawFree(state->global, analyzer->cache, sizeof(SZrAnalysisCache));
    }

    if (analyzer->referenceTracker != ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, analyzer->referenceTracker);
    }

    if (analyzer->symbolTable != ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
    }

    // 释放编译器状态
    if (analyzer->compilerState != ZR_NULL) {
        ZrParser_CompilerState_Free(analyzer->compilerState);
        ZrCore_Memory_RawFree(state->global, analyzer->compilerState, sizeof(SZrCompilerState));
    }

    analyzer->semanticContext = ZR_NULL;
    analyzer->hirModule = ZR_NULL;

    ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
}

// 辅助函数：从 AST 节点提取标识符名称

TZrBool ZrLanguageServer_SemanticAnalyzer_Analyze(SZrState *state, 
                                 SZrSemanticAnalyzer *analyzer,
                                 SZrAstNode *ast) {
    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer->ast = ast;
    
    TZrSize astHash = 0;
    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        astHash = ZrLanguageServer_SemanticAnalyzer_ComputeAstHash(ast);
        if (analyzer->cache->isValid && analyzer->cache->astHash == astHash) {
            return ZR_TRUE;
        }
    }

    // 分析器独占 diagnostics 的所有权；重新分析前释放并重建数组存储，避免保留悬空条目。
    semantic_analyzer_release_diagnostics(state, analyzer, ZR_TRUE);
    
    if (!ZrLanguageServer_SemanticAnalyzer_PrepareState(state, analyzer, ast)) {
        return ZR_FALSE;
    }
    
    // 第一阶段：收集符号定义
    ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, ast);
    
    // 第二阶段：收集引用
    ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, ast);
    
    // 第三阶段：类型检查（集成类型推断系统）
    // 遍历 AST 进行类型检查
    if (analyzer->compilerState != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ast);
    }
    
    // 更新缓存
    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        analyzer->cache->astHash = astHash;
        analyzer->cache->isValid = ZR_TRUE;
        
        /*
         * cachedDiagnostics 当前没有恢复路径，保留借用指针只会扩大诊断对象的别名范围。
         * 让 analyzer->diagnostics 继续作为唯一所有者，缓存只记录 AST 哈希即可。
         */
        analyzer->cache->cachedDiagnostics.length = 0;
    }
    
    return ZR_TRUE;
}

// 获取诊断信息
TZrBool ZrLanguageServer_SemanticAnalyzer_GetDiagnostics(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrArray *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrDiagnostic *), analyzer->diagnostics.length);
    }
    
    // 复制所有诊断
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrCore_Array_Push(state, result, diagPtr);
        }
    }
    
    return ZR_TRUE;
}

// 获取位置的符号
SZrSymbol *ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(SZrSemanticAnalyzer *analyzer,
                                         SZrFileRange position) {
    SZrReference *reference;
    const SZrType *hoverTypeInfo;
    SZrSymbol *symbol;

    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }

    hoverTypeInfo = semantic_find_type_node_at_position(analyzer->ast, position);
    if (hoverTypeInfo != ZR_NULL) {
        symbol = semantic_lookup_type_symbol_at_position(analyzer->state, analyzer, hoverTypeInfo, position);
        if (symbol != ZR_NULL) {
            return symbol;
        }
    }

    if (analyzer->referenceTracker != ZR_NULL) {
        reference = ZrLanguageServer_ReferenceTracker_FindReferenceAt(analyzer->referenceTracker, position);
        if (reference != ZR_NULL) {
            return reference->symbol;
        }
    }

    symbol = ZrLanguageServer_SymbolTable_FindDefinition(analyzer->symbolTable, position);
    if (symbol != ZR_NULL) {
        return symbol;
    }

    return ZR_NULL;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition(SZrState *state,
                                                                SZrSemanticAnalyzer *analyzer,
                                                                SZrFileRange position,
                                                                SZrInferredType *outType) {
    const SZrType *typeInfo;
    SZrAstNode *expressionNode;
    SZrSymbol *symbol;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->ast == ZR_NULL ||
        analyzer->compilerState == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    typeInfo = semantic_find_type_node_at_position(analyzer->ast, position);
    if (typeInfo == ZR_NULL) {
        expressionNode = semantic_find_expression_node_at_position(analyzer->ast, position);
        if (expressionNode != ZR_NULL &&
            ZrLanguageServer_SemanticAnalyzer_InferExactExpressionType(state, analyzer, expressionNode, outType)) {
            return ZR_TRUE;
        }

        symbol = ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, position);
        if (symbol != ZR_NULL &&
            symbol->typeInfo != ZR_NULL &&
            ZrLanguageServer_SemanticAnalyzer_IsPreciseInferredType(symbol->typeInfo)) {
            ZrParser_InferredType_Copy(state, outType, symbol->typeInfo);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    return ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(analyzer,
                                                                           ZR_NULL,
                                                                           ZR_NULL,
                                                                           typeInfo,
                                                                           outType);
}

// 获取悬停信息
TZrBool ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer,
                                     SZrFileRange position,
                                     SZrHoverInfo **result) {
    TZrNativeString nameStr;
    TZrSize nameLen;
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrChar buffer[ZR_LSP_HOVER_BUFFER_LENGTH];
    const TZrChar *kindText;
    const TZrChar *typeText;
    const TZrChar *accessText;
    const TZrChar *sourceText = ZR_NULL;
    TZrChar signatureBuffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrChar resolvedTypeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrChar expressionTypeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    const SZrType *hoverTypeInfo = ZR_NULL;
    SZrInferredType resolvedType;
    const TZrChar *resolvedTypeText = ZR_NULL;
    TZrBool hasResolvedType = ZR_FALSE;
    const SZrInferredType *displayTypeInfo = ZR_NULL;
    SZrSymbol *signatureSymbol = ZR_NULL;
    const SZrInferredType *signatureTypeInfo = ZR_NULL;
    SZrAstNode *signatureNode = ZR_NULL;

    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    SZrSymbol *symbol = ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, position);
    if (symbol == ZR_NULL) {
        ZrParser_InferredType_Init(state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
        if (ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition(state, analyzer, position, &resolvedType)) {
            resolvedTypeText = semantic_precise_inferred_type_text(state,
                                                                   &resolvedType,
                                                                   expressionTypeBuffer,
                                                                   sizeof(expressionTypeBuffer));
            snprintf(buffer, sizeof(buffer), "**expression**\n\nType: %s", resolvedTypeText);
            *result = ZrLanguageServer_HoverInfo_New(state,
                                                     buffer,
                                                     position,
                                                     ZrLanguageServer_SemanticAnalyzer_IsPreciseInferredType(&resolvedType)
                                                         ? &resolvedType
                                                         : ZR_NULL);
            ZrParser_InferredType_Free(state, &resolvedType);
            return *result != ZR_NULL;
        }
        snprintf(buffer, sizeof(buffer), "**expression**\n\nType: %s", semantic_exact_type_failure_text());
        ZrParser_InferredType_Free(state, &resolvedType);
        *result = ZrLanguageServer_HoverInfo_New(state, buffer, position, ZR_NULL);
        return *result != ZR_NULL;
    }

    semantic_get_string_view(symbol->name, &nameStr, &nameLen);
    if (nameStr == ZR_NULL || nameLen == 0) {
        return ZR_FALSE;
    }

    kindText = semantic_symbol_kind_text_for_symbol(analyzer, symbol);
    accessText = semantic_access_modifier_text(symbol->accessModifier);
    displayTypeInfo = semantic_symbol_display_type_info(analyzer, symbol);
    signatureSymbol = semantic_resolve_callable_signature_symbol(analyzer, symbol, displayTypeInfo);
    signatureTypeInfo = signatureSymbol != ZR_NULL ? semantic_symbol_display_type_info(analyzer, signatureSymbol)
                                                   : displayTypeInfo;
    signatureNode = signatureSymbol != ZR_NULL ? signatureSymbol->astNode : symbol->astNode;
    if (semantic_symbol_is_ffi_extern(analyzer, symbol)) {
        sourceText = "ffi extern";
    }
    hoverTypeInfo = semantic_find_type_node_at_position(analyzer->ast, position);
    if (hoverTypeInfo != ZR_NULL &&
        analyzer->compilerState != ZR_NULL &&
        (symbol->type == ZR_SYMBOL_CLASS || symbol->type == ZR_SYMBOL_STRUCT || symbol->type == ZR_SYMBOL_INTERFACE)) {
        ZrParser_InferredType_Init(state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
        if (ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(analyzer,
                                                                            ZR_NULL,
                                                                            ZR_NULL,
                                                                            hoverTypeInfo,
                                                                            &resolvedType)) {
            resolvedTypeText = semantic_precise_inferred_type_text(state,
                                                                   &resolvedType,
                                                                   resolvedTypeBuffer,
                                                                   sizeof(resolvedTypeBuffer));
            hasResolvedType = semantic_type_text_is_specific(resolvedTypeText);
        }
        ZrParser_InferredType_Free(state, &resolvedType);
    }
    if (semantic_build_ast_signature(state,
                                     analyzer->compilerState,
                                     signatureTypeInfo,
                                     signatureNode,
                                     signatureBuffer,
                                     sizeof(signatureBuffer))) {
        if (hasResolvedType) {
            snprintf(buffer,
                     sizeof(buffer),
                     "**%s**: %.*s\n\nSignature: %s\nResolved Type: %s\nAccess: %s",
                     kindText,
                     (int)nameLen,
                     nameStr,
                     signatureBuffer,
                     resolvedTypeText,
                     accessText);
        } else {
            snprintf(buffer,
                     sizeof(buffer),
                     "**%s**: %.*s\n\nSignature: %s\nAccess: %s",
                     kindText,
                     (int)nameLen,
                     nameStr,
                     signatureBuffer,
                     accessText);
        }
        if (sourceText != ZR_NULL) {
            semantic_buffer_append(buffer, sizeof(buffer), strlen(buffer), "\nSource: %s", sourceText);
        }
        if (symbol->astNode != ZR_NULL) {
            semantic_append_ffi_hover_metadata(symbol->astNode, buffer, sizeof(buffer), strlen(buffer));
        }
        *result = ZrLanguageServer_HoverInfo_New(state, buffer, symbol->selectionRange, symbol->typeInfo);
        return *result != ZR_NULL;
    }

    typeText = semantic_precise_inferred_type_text(state, displayTypeInfo, typeBuffer, sizeof(typeBuffer));
    if (ZrLanguageServer_SemanticAnalyzer_IsPreciseInferredType(displayTypeInfo)) {
        snprintf(buffer,
                 sizeof(buffer),
                 "**%s**: %.*s\n\nResolved Type: %s\nAccess: %s",
                 kindText,
                 (int)nameLen,
                 nameStr,
                 typeText != ZR_NULL ? typeText : semantic_exact_type_failure_text(),
                 accessText);
    } else {
        snprintf(buffer,
                 sizeof(buffer),
                 "**%s**: %.*s\n\nType: %s\nAccess: %s",
                 kindText,
                 (int)nameLen,
                 nameStr,
                 typeText != ZR_NULL ? typeText : semantic_exact_type_failure_text(),
                 accessText);
    }

    if (sourceText != ZR_NULL) {
        semantic_buffer_append(buffer, sizeof(buffer), strlen(buffer), "\nSource: %s", sourceText);
    }
    if (symbol->astNode != ZR_NULL) {
        semantic_append_ffi_hover_metadata(symbol->astNode, buffer, sizeof(buffer), strlen(buffer));
    }

    *result = ZrLanguageServer_HoverInfo_New(state, buffer, symbol->selectionRange, symbol->typeInfo);
    return *result != ZR_NULL;
}

// 获取代码补全
TZrBool ZrLanguageServer_SemanticAnalyzer_GetCompletions(SZrState *state,
                                       SZrSemanticAnalyzer *analyzer,
                                       SZrFileRange position,
                                       SZrArray *result) {
    SZrArray visibleSymbols;

    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrCompletionItem *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    ZrCore_Array_Construct(&visibleSymbols);
    if (ZrLanguageServer_SymbolTable_GetVisibleSymbolsAtPosition(state,
                                                                 analyzer->symbolTable,
                                                                 position,
                                                                 &visibleSymbols)) {
        for (TZrSize index = 0; index < visibleSymbols.length; index++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&visibleSymbols, index);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                semantic_append_symbol_completion(state, analyzer, analyzer->compilerState, result, *symbolPtr);
            }
        }
    }
    ZrCore_Array_Free(state, &visibleSymbols);

    if (analyzer->ast != ZR_NULL) {
        semantic_append_imported_module_completions(state, analyzer, analyzer->ast, result);
    }

    return ZR_TRUE;
}

// 添加诊断
TZrBool ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer,
                                     EZrDiagnosticSeverity severity,
                                     SZrFileRange location,
                                     const TZrChar *message,
                                     const TZrChar *code) {
    if (state == ZR_NULL || analyzer == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }
    
    SZrDiagnostic *diagnostic = ZrLanguageServer_Diagnostic_New(state, severity, location, message, code);
    if (diagnostic == ZR_NULL) {
        return ZR_FALSE;
    }
    
    ZrCore_Array_Push(state, &analyzer->diagnostics, &diagnostic);
    
    return ZR_TRUE;
}

// 创建诊断
SZrDiagnostic *ZrLanguageServer_Diagnostic_New(SZrState *state,
                                EZrDiagnosticSeverity severity,
                                SZrFileRange location,
                                const TZrChar *message,
                                const TZrChar *code) {
    if (state == ZR_NULL || message == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrDiagnostic *diagnostic = (SZrDiagnostic *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrDiagnostic));
    if (diagnostic == ZR_NULL) {
        return ZR_NULL;
    }
    
    diagnostic->severity = severity;
    diagnostic->location = location;
    diagnostic->message = ZrCore_String_Create(state, (TZrNativeString)message, strlen(message));
    diagnostic->code = code != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)code, strlen(code)) : ZR_NULL;
    ZrCore_Array_Construct(&diagnostic->relatedInformation);
    
    if (diagnostic->message == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, diagnostic, sizeof(SZrDiagnostic));
        return ZR_NULL;
    }
    
    return diagnostic;
}

TZrBool ZrLanguageServer_Diagnostic_AddRelatedInformation(SZrState *state,
                                                          SZrDiagnostic *diagnostic,
                                                          SZrFileRange location,
                                                          const TZrChar *message) {
    SZrDiagnosticRelatedInformation related;

    if (state == ZR_NULL || diagnostic == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!diagnostic->relatedInformation.isValid) {
        ZrCore_Array_Init(state,
                          &diagnostic->relatedInformation,
                          sizeof(SZrDiagnosticRelatedInformation),
                          ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    memset(&related, 0, sizeof(related));
    related.location = location;
    related.message = ZrCore_String_Create(state, (TZrNativeString)message, strlen(message));
    if (related.message == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, &diagnostic->relatedInformation, &related);
    return ZR_TRUE;
}

// 释放诊断
void ZrLanguageServer_Diagnostic_Free(SZrState *state, SZrDiagnostic *diagnostic) {
    if (state == ZR_NULL || diagnostic == ZR_NULL) {
        return;
    }
    
    if (diagnostic->message != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (diagnostic->code != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (diagnostic->relatedInformation.isValid) {
        ZrCore_Array_Free(state, &diagnostic->relatedInformation);
    }
    ZrCore_Memory_RawFree(state->global, diagnostic, sizeof(SZrDiagnostic));
}

// 创建补全项
SZrCompletionItem *ZrLanguageServer_CompletionItem_New(SZrState *state,
                                       const TZrChar *label,
                                       const TZrChar *kind,
                                       const TZrChar *detail,
                                       const TZrChar *documentation,
                                       SZrInferredType *typeInfo) {
    if (state == ZR_NULL || label == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrCompletionItem *item = (SZrCompletionItem *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrCompletionItem));
    if (item == ZR_NULL) {
        return ZR_NULL;
    }
    
    item->label = ZrCore_String_Create(state, (TZrNativeString)label, strlen(label));
    item->kind = kind != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)kind, strlen(kind)) : ZR_NULL;
    item->detail = detail != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)detail, strlen(detail)) : ZR_NULL;
    item->documentation = documentation != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)documentation, strlen(documentation)) : ZR_NULL;
    item->typeInfo = typeInfo; // 不复制，只是引用
    
    if (item->label == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, item, sizeof(SZrCompletionItem));
        return ZR_NULL;
    }
    
    return item;
}

// 释放补全项
void ZrLanguageServer_CompletionItem_Free(SZrState *state, SZrCompletionItem *item) {
    if (state == ZR_NULL || item == ZR_NULL) {
        return;
    }
    
    if (item->label != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->kind != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->detail != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->documentation != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrCore_Memory_RawFree(state->global, item, sizeof(SZrCompletionItem));
}

// 创建悬停信息
SZrHoverInfo *ZrLanguageServer_HoverInfo_New(SZrState *state,
                              const TZrChar *contents,
                              SZrFileRange range,
                              SZrInferredType *typeInfo) {
    if (state == ZR_NULL || contents == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrHoverInfo *info = (SZrHoverInfo *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrHoverInfo));
    if (info == ZR_NULL) {
        return ZR_NULL;
    }
    
    info->contents = ZrCore_String_Create(state, (TZrNativeString)contents, strlen(contents));
    info->range = range;
    info->typeInfo = typeInfo; // 不复制，只是引用
    
    if (info->contents == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, info, sizeof(SZrHoverInfo));
        return ZR_NULL;
    }
    
    return info;
}

// 释放悬停信息
void ZrLanguageServer_HoverInfo_Free(SZrState *state, SZrHoverInfo *info) {
    if (state == ZR_NULL || info == ZR_NULL) {
        return;
    }
    
    if (info->contents != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrCore_Memory_RawFree(state->global, info, sizeof(SZrHoverInfo));
}

// 启用/禁用缓存
void ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(SZrSemanticAnalyzer *analyzer, TZrBool enabled) {
    if (analyzer == ZR_NULL) {
        return;
    }
    analyzer->enableCache = enabled;
}

// 清除缓存
void ZrLanguageServer_SemanticAnalyzer_ClearCache(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->cache == ZR_NULL) {
        return;
    }
    
    analyzer->cache->isValid = ZR_FALSE;
    analyzer->cache->astHash = 0;
    
    semantic_analyzer_clear_cached_diagnostic_refs(analyzer);
    analyzer->cache->cachedSymbols.length = 0;
}
