//
// Created by Auto on 2025/01/XX.
//

#include "semantic/semantic_analyzer_internal.h"
#include "semantic/semantic_analyzer_query_source.h"
#include "module/lsp_module_metadata.h"
#include "semantic/lsp_stable_slot_contract.h"

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic_query.h"

#include <stdarg.h>


static void semantic_append_stable_slot_hover(
        SZrSemanticAnalyzer *analyzer,
        const SZrInferredType *typeInfo,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const TZrChar *typeName;
    const SZrTypePrototypeInfo *prototype;

    if (analyzer == ZR_NULL || typeInfo == ZR_NULL ||
        typeInfo->typeName == ZR_NULL || buffer == ZR_NULL || bufferSize == 0u) {
        return;
    }
    typeName = typeInfo->typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG
                       ? ZrCore_String_GetNativeStringShort(typeInfo->typeName)
                       : ZrCore_String_GetNativeString(typeInfo->typeName);
    prototype = ZrLanguageServer_LspModuleMetadata_FindTypePrototype(
            analyzer, typeName);
    ZrLanguageServer_LspStableSlotContract_AppendPrototypeHover(
            prototype, buffer, bufferSize);
}

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

    (void)displayTypeInfo;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || symbol == ZR_NULL ||
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

static const TZrChar *semantic_parameter_source_form_text(
        EZrParameterSourcePassingForm sourceForm) {
    switch (sourceForm) {
        case ZR_PARAMETER_SOURCE_IN: return "in";
        case ZR_PARAMETER_SOURCE_OUT: return "out";
        case ZR_PARAMETER_SOURCE_REF: return "ref";
        case ZR_PARAMETER_SOURCE_REF_READONLY: return "ref readonly";
        case ZR_PARAMETER_SOURCE_SCOPED_REF: return "scoped ref";
        case ZR_PARAMETER_SOURCE_SCOPED_REF_READONLY: return "scoped ref readonly";
        case ZR_PARAMETER_SOURCE_VALUE:
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

static TZrSize semantic_append_symbol_ffi_hover_metadata(SZrSymbol *symbol,
                                                         TZrChar *buffer,
                                                         TZrSize bufferSize,
                                                         TZrSize offset) {
    TZrNativeString metadataText;
    TZrSize metadataLength;

    if (symbol == ZR_NULL || symbol->ffiHoverMetadata == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return offset;
    }

    if (symbol->ffiHoverMetadata->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        metadataText = ZrCore_String_GetNativeStringShort(symbol->ffiHoverMetadata);
        metadataLength = symbol->ffiHoverMetadata->shortStringLength;
    } else {
        metadataText = ZrCore_String_GetNativeString(symbol->ffiHoverMetadata);
        metadataLength = symbol->ffiHoverMetadata->longStringLength;
    }

    if (metadataText == ZR_NULL || metadataLength == 0 || offset >= bufferSize - 1) {
        return offset;
    }

    if (metadataLength >= bufferSize - offset) {
        metadataLength = bufferSize - offset - 1;
    }
    memcpy(buffer + offset, metadataText, metadataLength);
    offset += metadataLength;
    buffer[offset] = '\0';
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

static TZrBool semantic_type_reference_range(const SZrType *typeNode, SZrFileRange *outRange) {
    SZrGenericType *genericType;
    TZrSize nameLength;

    if (typeNode == ZR_NULL || typeNode->name == ZR_NULL || outRange == ZR_NULL) {
        return ZR_FALSE;
    }

    *outRange = typeNode->name->location;
    if (typeNode->name->type != ZR_AST_GENERIC_TYPE) {
        return ZR_TRUE;
    }

    genericType = (SZrGenericType *)&typeNode->name->data.genericType;
    if (genericType->name == ZR_NULL || genericType->name->name == ZR_NULL) {
        return ZR_FALSE;
    }

    nameLength = ZrCore_String_GetByteLength(genericType->name->name);
    if (nameLength == 0u ||
        outRange->start.offset < nameLength + 1u ||
        outRange->start.column < (TZrInt32)(nameLength + 1u)) {
        return ZR_FALSE;
    }

    outRange->end = outRange->start;
    outRange->end.offset--;
    outRange->end.column--;
    outRange->start.offset -= nameLength + 1u;
    outRange->start.column -= (TZrInt32)(nameLength + 1u);
    return ZR_TRUE;
}

static void semantic_format_type_from_ast(SZrState *state,
                                          SZrCompilerState *compilerState,
                                          SZrSemanticAnalyzer *analyzer,
                                          SZrAstNode *ownerTypeNode,
                                          SZrAstNode *functionNode,
                                          SZrType *typeNode,
                                          TZrChar *buffer,
                                          TZrSize bufferSize) {
    SZrParserSemanticTypeQuery query = {0};
    SZrFileRange range;
    SZrInferredType inferredType;
    TZrBool hasCanonicalType = ZR_FALSE;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    if (state != ZR_NULL && compilerState != ZR_NULL && compilerState->semanticContext != ZR_NULL &&
        semantic_type_reference_range(typeNode, &range)) {
        hasCanonicalType = ZrParser_SemanticQuery_CanonicalTypeAt(
                compilerState->semanticContext, range, ZR_NULL, &query);
        if (!hasCanonicalType && analyzer != ZR_NULL) {
            ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
            (void)ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(
                    analyzer, ownerTypeNode, functionNode, typeNode, &inferredType);
            ZrParser_InferredType_Free(state, &inferredType);
            hasCanonicalType = ZrParser_SemanticQuery_CanonicalTypeAt(
                    compilerState->semanticContext, range, ZR_NULL, &query);
        }
    }

    if (hasCanonicalType && query.typeId != ZR_SEMANTIC_ID_INVALID &&
        query.reference != ZR_NULL && query.reference->kind == ZR_SEMANTIC_REFERENCE_TYPE &&
        query.reference->isResolved &&
        ZrParser_CanonicalType_Format(compilerState->semanticContext, query.typeId, buffer, bufferSize)) {
        return;
    }

    snprintf(buffer, bufferSize, "%s", semantic_exact_type_failure_text());
}

static void semantic_format_display_type_from_ast_or_inferred(SZrState *state,
                                                              SZrCompilerState *compilerState,
                                                              SZrSemanticAnalyzer *analyzer,
                                                              SZrAstNode *ownerTypeNode,
                                                              SZrAstNode *functionNode,
                                                              SZrType *typeNode,
                                                              const SZrInferredType *fallbackTypeInfo,
                                                              TZrChar *buffer,
                                                              TZrSize bufferSize) {
    const TZrChar *typeText;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    semantic_format_type_from_ast(state,
                                  compilerState,
                                  analyzer,
                                  ownerTypeNode,
                                  functionNode,
                                  typeNode,
                                  buffer,
                                  bufferSize);
    if (semantic_type_text_is_specific(buffer)) {
        return;
    }

    /* An explicit declaration must be rendered by its resolved canonical fact. */
    if (typeNode != ZR_NULL) {
        return;
    }

    typeText = semantic_precise_inferred_type_text(state, fallbackTypeInfo, buffer, bufferSize);
    if (typeText != buffer) {
        snprintf(buffer, bufferSize, "%s", typeText);
    }
}

static TZrSize semantic_append_generic_parameter_decl(SZrState *state,
                                                      SZrCompilerState *compilerState,
                                                      SZrSemanticAnalyzer *analyzer,
                                                      SZrAstNode *ownerTypeNode,
                                                      SZrAstNode *functionNode,
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
        semantic_format_type_from_ast(state,
                                      compilerState,
                                      analyzer,
                                      ownerTypeNode,
                                      functionNode,
                                      parameter->typeInfo,
                                      typeBuffer,
                                      sizeof(typeBuffer));
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
                                                   SZrSemanticAnalyzer *analyzer,
                                                   SZrAstNode *ownerTypeNode,
                                                   SZrAstNode *functionNode,
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
                                                        analyzer,
                                                        ownerTypeNode,
                                                        functionNode,
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
                                                   SZrSemanticAnalyzer *analyzer,
                                                   SZrAstNode *ownerTypeNode,
                                                   SZrAstNode *functionNode,
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
                                          analyzer,
                                          ownerTypeNode,
                                          functionNode,
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
                                             SZrSemanticAnalyzer *analyzer,
                                             SZrAstNode *ownerTypeNode,
                                             SZrAstNode *functionNode,
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
                                                     analyzer,
                                                     ownerTypeNode,
                                                     functionNode,
                                                     buffer,
                                                     bufferSize,
                                                     offset,
                                                     &paramNode->data.parameter);
    }

    return offset;
}

static TZrSize semantic_append_inheritance_clause(SZrState *state,
                                                  SZrCompilerState *compilerState,
                                                  SZrSemanticAnalyzer *analyzer,
                                                  SZrAstNode *ownerTypeNode,
                                                  SZrAstNode *functionNode,
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
                                      analyzer,
                                      ownerTypeNode,
                                      functionNode,
                                      &inheritNode->data.type,
                                      typeBuffer,
                                      sizeof(typeBuffer));
        offset = semantic_buffer_append(buffer, bufferSize, offset, "%s", typeBuffer);
    }

    return offset;
}

static TZrSize semantic_append_parameter_list(SZrState *state,
                                              SZrCompilerState *compilerState,
                                              SZrSemanticAnalyzer *analyzer,
                                              SZrAstNode *ownerTypeNode,
                                              SZrAstNode *functionNode,
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
            const TZrChar *sourceFormText;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            parameter = &paramNode->data.parameter;
            if (index > 0) {
                offset = semantic_buffer_append(buffer, bufferSize, offset, ", ");
            }

            semantic_format_type_from_ast(state,
                                          compilerState,
                                          analyzer,
                                          ownerTypeNode,
                                          functionNode,
                                          parameter->typeInfo,
                                          typeBuffer,
                                          sizeof(typeBuffer));
            sourceFormText = semantic_parameter_source_form_text(parameter->sourcePassingForm);
            offset = semantic_buffer_append(buffer,
                                            bufferSize,
                                            offset,
                                            "%s: %s%s",
                                            semantic_string_native(parameter->name != ZR_NULL ? parameter->name->name : ZR_NULL),
                                            sourceFormText != ZR_NULL ? sourceFormText : "",
                                            sourceFormText != ZR_NULL ? " " : "");
            offset = semantic_buffer_append(buffer, bufferSize, offset, "%s", typeBuffer);
        }
    }

    return semantic_buffer_append(buffer, bufferSize, offset, ")");
}

static TZrBool semantic_build_ast_signature(SZrState *state,
                                            SZrCompilerState *compilerState,
                                            SZrSemanticAnalyzer *analyzer,
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
                                                        analyzer,
                                                        ZR_NULL,
                                                        node,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        funcDecl->generic);
            offset = semantic_append_parameter_list(state,
                                                   compilerState,
                                                   analyzer,
                                                   ZR_NULL,
                                                   node,
                                                   buffer,
                                                   bufferSize,
                                                   offset,
                                                   funcDecl->params);
            semantic_format_display_type_from_ast_or_inferred(state,
                                                              compilerState,
                                                              analyzer,
                                                              ZR_NULL,
                                                              node,
                                                              funcDecl->returnType,
                                                              fallbackTypeInfo,
                                                              typeBuffer,
                                                              sizeof(typeBuffer));
            offset = semantic_buffer_append(buffer, bufferSize, offset, ": %s", typeBuffer);
            offset = semantic_append_where_clauses(state,
                                                   compilerState,
                                                   analyzer,
                                                   ZR_NULL,
                                                   node,
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
                                                    analyzer,
                                                    ZR_NULL,
                                                    node,
                                                    buffer,
                                                    bufferSize,
                                                    offset,
                                                    funcDecl->params);
            semantic_format_display_type_from_ast_or_inferred(state,
                                                              compilerState,
                                                              analyzer,
                                                              ZR_NULL,
                                                              node,
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
                                                    analyzer,
                                                    ZR_NULL,
                                                    node,
                                                    buffer,
                                                    bufferSize,
                                                    offset,
                                                    delegateDecl->params);
            semantic_format_display_type_from_ast_or_inferred(state,
                                                              compilerState,
                                                              analyzer,
                                                              ZR_NULL,
                                                              node,
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
                                                        analyzer,
                                                        ZR_NULL,
                                                        node,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        method->generic);
            offset = semantic_append_parameter_list(state,
                                                   compilerState,
                                                   analyzer,
                                                   ZR_NULL,
                                                   node,
                                                   buffer,
                                                   bufferSize,
                                                   offset,
                                                   method->params);
            semantic_format_display_type_from_ast_or_inferred(state,
                                                              compilerState,
                                                              analyzer,
                                                              ZR_NULL,
                                                              node,
                                                              method->returnType,
                                                              fallbackTypeInfo,
                                                              typeBuffer,
                                                              sizeof(typeBuffer));
            offset = semantic_buffer_append(buffer, bufferSize, offset, ": %s", typeBuffer);
            offset = semantic_append_where_clauses(state,
                                                   compilerState,
                                                   analyzer,
                                                   ZR_NULL,
                                                   node,
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
                                                        analyzer,
                                                        ZR_NULL,
                                                        node,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        method->generic);
            offset = semantic_append_parameter_list(state,
                                                   compilerState,
                                                   analyzer,
                                                   ZR_NULL,
                                                   node,
                                                   buffer,
                                                   bufferSize,
                                                   offset,
                                                   method->params);
            semantic_format_display_type_from_ast_or_inferred(state,
                                                              compilerState,
                                                              analyzer,
                                                              ZR_NULL,
                                                              node,
                                                              method->returnType,
                                                              fallbackTypeInfo,
                                                              typeBuffer,
                                                              sizeof(typeBuffer));
            offset = semantic_buffer_append(buffer, bufferSize, offset, ": %s", typeBuffer);
            offset = semantic_append_where_clauses(state,
                                                   compilerState,
                                                   analyzer,
                                                   ZR_NULL,
                                                   node,
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
                                                        analyzer,
                                                        node,
                                                        ZR_NULL,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        classDecl->generic);
            offset = semantic_append_inheritance_clause(state,
                                                        compilerState,
                                                        analyzer,
                                                        node,
                                                        ZR_NULL,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        classDecl->inherits);
            offset = semantic_append_where_clauses(state,
                                                   compilerState,
                                                   analyzer,
                                                   node,
                                                   ZR_NULL,
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
                                                        analyzer,
                                                        node,
                                                        ZR_NULL,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        structDecl->generic);
            offset = semantic_append_inheritance_clause(state,
                                                        compilerState,
                                                        analyzer,
                                                        node,
                                                        ZR_NULL,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        structDecl->inherits);
            offset = semantic_append_where_clauses(state,
                                                   compilerState,
                                                   analyzer,
                                                   node,
                                                   ZR_NULL,
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
                                                        analyzer,
                                                        node,
                                                        ZR_NULL,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        interfaceDecl->generic);
            offset = semantic_append_inheritance_clause(state,
                                                        compilerState,
                                                        analyzer,
                                                        node,
                                                        ZR_NULL,
                                                        buffer,
                                                        bufferSize,
                                                        offset,
                                                        interfaceDecl->inherits);
            offset = semantic_append_where_clauses(state,
                                                   compilerState,
                                                   analyzer,
                                                   node,
                                                   ZR_NULL,
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

TZrBool ZrLanguageServer_SemanticAnalyzer_EnsureCacheStorage(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || state->global == ZR_NULL || analyzer == ZR_NULL) {
        return ZR_FALSE;
    }
    if (analyzer->cache != ZR_NULL) {
        return ZR_TRUE;
    }

    analyzer->cache = (SZrAnalysisCache *)ZrCore_Memory_RawMalloc(
            state->global,
            sizeof(SZrAnalysisCache));
    if (analyzer->cache == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer->cache->isValid = ZR_FALSE;
    analyzer->cache->astHash = 0;
    analyzer->cache->scopeAstHash = 0;
    analyzer->cache->cacheRange = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(0, 1, 1),
            ZrParser_FilePosition_Create(0, 1, 1),
            ZR_NULL);
    ZrCore_Array_Init(state,
                      &analyzer->cache->cachedDiagnostics,
                      sizeof(SZrDiagnostic *),
                      ZR_LSP_ARRAY_INITIAL_CAPACITY);
    ZrCore_Array_Init(state,
                      &analyzer->cache->cachedSymbols,
                      sizeof(SZrSymbol *),
                      ZR_LSP_ARRAY_INITIAL_CAPACITY);
    return ZR_TRUE;
}

static void semantic_analyzer_free_cache_storage(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || state->global == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->cache == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(state, &analyzer->cache->cachedDiagnostics);
    ZrCore_Array_Free(state, &analyzer->cache->cachedSymbols);
    ZrCore_Memory_RawFree(state->global,
                          analyzer->cache,
                          sizeof(SZrAnalysisCache));
    analyzer->cache = ZR_NULL;
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
    analyzer->scopedQueryAnalyzer = ZR_NULL;
    analyzer->ownedAst = ZR_NULL;
    analyzer->borrowedAst = ZR_NULL;
    analyzer->preserveScopedQueryAnalyzerOnNextAstChange = ZR_FALSE;
    analyzer->cacheStorageAccessOrder = 0U;
    memset(&analyzer->metrics, 0, sizeof(analyzer->metrics));
    
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
    
    (void)ZrLanguageServer_SemanticAnalyzer_EnsureCacheStorage(state, analyzer);
    
    return analyzer;
}

// 释放语义分析器
void ZrLanguageServer_SemanticAnalyzer_Free(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(state, analyzer);
    ZrLanguageServer_SemanticAnalyzer_ReleaseDiagnostics(state, analyzer, ZR_FALSE);
    ZrCore_Array_Free(state, &analyzer->diagnostics);
    
    semantic_analyzer_free_cache_storage(state, analyzer);

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

    if (analyzer->ownedAst != ZR_NULL) {
        ZrParser_Ast_Free(state, analyzer->ownedAst);
        analyzer->ownedAst = ZR_NULL;
    }
    analyzer->borrowedAst = ZR_NULL;

    analyzer->semanticContext = ZR_NULL;
    analyzer->hirModule = ZR_NULL;

    ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
}

SZrSemanticAnalyzer *
ZrLanguageServer_SemanticAnalyzer_DetachCurrentStateForSnapshot(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *retainedAst,
        TZrBool preserveScopedQueryAnalyzer) {
    SZrSemanticAnalyzer *replacement;
    SZrSemanticAnalyzer *snapshot;
    SZrSemanticAnalyzer *scopedAnalyzer;
    SZrSemanticAnalysisMetrics metrics;
    TZrBool enableCache;
    TZrBool preserveOnNextAstChange;

    if (state == ZR_NULL || state->global == ZR_NULL || analyzer == ZR_NULL ||
        retainedAst == ZR_NULL || analyzer->ast != retainedAst ||
        analyzer->ownedAst != ZR_NULL || analyzer->borrowedAst != ZR_NULL) {
        return ZR_NULL;
    }

    replacement = ZrLanguageServer_SemanticAnalyzer_New(state);
    if (replacement == ZR_NULL) {
        return ZR_NULL;
    }
    snapshot = (SZrSemanticAnalyzer *)ZrCore_Memory_RawMalloc(
            state->global,
            sizeof(SZrSemanticAnalyzer));
    if (snapshot == ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_Free(state, replacement);
        return ZR_NULL;
    }

    metrics = analyzer->metrics;
    enableCache = analyzer->enableCache;
    preserveOnNextAstChange = analyzer->preserveScopedQueryAnalyzerOnNextAstChange;
    *snapshot = *analyzer;
    scopedAnalyzer = snapshot->scopedQueryAnalyzer;
    snapshot->ownedAst = retainedAst;
    snapshot->borrowedAst = ZR_NULL;

    *analyzer = *replacement;
    ZrCore_Memory_RawFree(
            state->global,
            replacement,
            sizeof(SZrSemanticAnalyzer));
    analyzer->metrics = metrics;
    analyzer->enableCache = enableCache;
    analyzer->preserveScopedQueryAnalyzerOnNextAstChange =
            preserveOnNextAstChange;
    analyzer->scopedQueryAnalyzer = preserveScopedQueryAnalyzer
                                            ? scopedAnalyzer
                                            : ZR_NULL;

    if (preserveScopedQueryAnalyzer && scopedAnalyzer != ZR_NULL) {
        /* The scoped analyzer stays with the live analyzer, even when it
         * already borrows an older historical AST. */
        snapshot->scopedQueryAnalyzer = ZR_NULL;
        if (scopedAnalyzer->ast == retainedAst &&
            scopedAnalyzer->ownedAst == ZR_NULL &&
            scopedAnalyzer->borrowedAst == ZR_NULL) {
            scopedAnalyzer->borrowedAst = retainedAst;
        }
    }
    return snapshot;
}

void ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzerBorrowingAst(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const SZrAstNode *borrowedAst) {
    if (state == ZR_NULL || analyzer == ZR_NULL || borrowedAst == ZR_NULL ||
        analyzer->scopedQueryAnalyzer == ZR_NULL ||
        analyzer->scopedQueryAnalyzer->borrowedAst != borrowedAst) {
        return;
    }

    analyzer->metrics.scopedCacheInvalidationCount++;
    ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(
            state,
            analyzer);
}

// 辅助函数：从 AST 节点提取标识符名称

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
    SZrParserSemanticSymbolQuery canonicalSymbol;

    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }

    position = ZrLanguageServer_SemanticAnalyzer_BindQuerySource(
            analyzer, position);

    memset(&canonicalSymbol, 0, sizeof(canonicalSymbol));
    if (analyzer->semanticContext == ZR_NULL ||
        !ZrParser_SemanticQuery_SymbolAt(
                analyzer->semanticContext,
                position,
                ZR_NULL,
                &canonicalSymbol) ||
        canonicalSymbol.symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }

    return ZrLanguageServer_SymbolTable_FindBySemanticId(
            analyzer->symbolTable,
            canonicalSymbol.symbolId);
}

TZrBool ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition(SZrState *state,
                                                                SZrSemanticAnalyzer *analyzer,
                                                                SZrFileRange position,
                                                                SZrInferredType *outType) {
    const SZrSemanticContext *semanticContext;
    const SZrSemanticTypeRecord *typeRecord;
    SZrParserSemanticTypeQuery typeQuery;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        outType == ZR_NULL) {
        return ZR_FALSE;
    }

    semanticContext = analyzer->semanticContext;
    if (!ZrParser_SemanticQuery_CanonicalTypeAt(
                semanticContext, position, ZR_NULL, &typeQuery) ||
        typeQuery.typeId == ZR_SEMANTIC_ID_INVALID ||
        ZrParser_CanonicalType_Find(semanticContext, typeQuery.typeId) == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeQuery.expression != ZR_NULL &&
        typeQuery.expression->typeId == typeQuery.typeId &&
        typeQuery.expression->exactness == ZR_SEMANTIC_FACT_EXACT &&
        ZrLanguageServer_SemanticAnalyzer_IsPreciseInferredType(
                &typeQuery.expression->inferredType)) {
        ZrParser_InferredType_Copy(state, outType, &typeQuery.expression->inferredType);
        return ZR_TRUE;
    }

    if (typeQuery.reference == ZR_NULL ||
        typeQuery.reference->kind != ZR_SEMANTIC_REFERENCE_TYPE ||
        !typeQuery.reference->isResolved) {
        return ZR_FALSE;
    }

    typeRecord = semantic_find_type_record_by_id(semanticContext, typeQuery.typeId);
    if (typeRecord == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_IsPreciseInferredType(
                &typeRecord->inferredType)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Copy(state, outType, &typeRecord->inferredType);
    return ZR_TRUE;
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
            semantic_append_stable_slot_hover(
                    analyzer, &resolvedType, buffer, sizeof(buffer));
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
                                     analyzer,
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
        semantic_append_symbol_ffi_hover_metadata(symbol, buffer, sizeof(buffer), strlen(buffer));
        semantic_append_stable_slot_hover(
                analyzer, displayTypeInfo, buffer, sizeof(buffer));
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
    semantic_append_symbol_ffi_hover_metadata(symbol, buffer, sizeof(buffer), strlen(buffer));
    semantic_append_stable_slot_hover(
            analyzer, displayTypeInfo, buffer, sizeof(buffer));

    *result = ZrLanguageServer_HoverInfo_New(state, buffer, symbol->selectionRange, symbol->typeInfo);
    return *result != ZR_NULL;
}

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
    diagnostic->cause = ZR_NULL;
    diagnostic->suggestion = ZR_NULL;
    ZrCore_Array_Construct(&diagnostic->relatedInformation);
    ZrCore_Array_Construct(&diagnostic->fixes);
    diagnostic->descriptorId = 0;
    diagnostic->codeDescriptionHref = ZR_NULL;
    diagnostic->noFixReason = ZR_DIAGNOSTIC_NO_FIX_REASON_UNSPECIFIED;
    
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
    if (diagnostic->cause != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (diagnostic->suggestion != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (diagnostic->relatedInformation.isValid) {
        ZrCore_Array_Free(state, &diagnostic->relatedInformation);
    }
    if (diagnostic->fixes.isValid) {
        ZrCore_Array_Free(state, &diagnostic->fixes);
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
    if (analyzer->scopedQueryAnalyzer != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(
                analyzer->scopedQueryAnalyzer,
                enabled);
    }
}

// 清除缓存
void ZrLanguageServer_SemanticAnalyzer_ClearCache(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_InvalidateScopedQueryAnalyzer(state, analyzer);
    if (analyzer->cache == ZR_NULL) {
        return;
    }
    
    analyzer->cache->isValid = ZR_FALSE;
    analyzer->cache->astHash = 0;
    analyzer->cache->scopeAstHash = 0;
    analyzer->cache->cacheRange = ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(0, 1, 1),
            ZrParser_FilePosition_Create(0, 1, 1),
            ZR_NULL);
    
    ZrLanguageServer_SemanticAnalyzer_ClearCachedDiagnosticRefs(analyzer);
    analyzer->cache->cachedSymbols.length = 0;
}

TZrSize ZrLanguageServer_SemanticAnalyzer_GetCacheStorageBytes(
        const SZrSemanticAnalyzer *analyzer) {
    TZrSize bytes;
    TZrSize diagnosticBytes;
    TZrSize symbolBytes;

    if (analyzer == ZR_NULL) {
        return 0;
    }

    bytes = 0;
    if (analyzer->cache != ZR_NULL) {
        if (analyzer->cache->cachedDiagnostics.capacity >
            (ZR_MAX_SIZE - sizeof(SZrAnalysisCache)) /
                    sizeof(SZrDiagnostic *)) {
            return ZR_MAX_SIZE;
        }
        diagnosticBytes = analyzer->cache->cachedDiagnostics.capacity *
                          sizeof(SZrDiagnostic *);
        if (analyzer->cache->cachedSymbols.capacity >
            (ZR_MAX_SIZE - sizeof(SZrAnalysisCache) - diagnosticBytes) /
                    sizeof(SZrSymbol *)) {
            return ZR_MAX_SIZE;
        }
        symbolBytes = analyzer->cache->cachedSymbols.capacity *
                      sizeof(SZrSymbol *);
        bytes = sizeof(SZrAnalysisCache) + diagnosticBytes + symbolBytes;
    }
    if (analyzer->scopedQueryAnalyzer != ZR_NULL &&
        analyzer->scopedQueryAnalyzer != analyzer) {
        TZrSize scopedBytes =
                ZrLanguageServer_SemanticAnalyzer_GetCacheStorageBytes(
                        analyzer->scopedQueryAnalyzer);
        if (scopedBytes > ZR_MAX_SIZE - bytes) {
            return ZR_MAX_SIZE;
        }
        bytes += scopedBytes;
    }
    return bytes;
}

void ZrLanguageServer_SemanticAnalyzer_ReleaseCacheStorage(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return;
    }

    if (analyzer->scopedQueryAnalyzer != ZR_NULL &&
        analyzer->scopedQueryAnalyzer != analyzer) {
        ZrLanguageServer_SemanticAnalyzer_ReleaseCacheStorage(
                state,
                analyzer->scopedQueryAnalyzer);
    }
    semantic_analyzer_free_cache_storage(state, analyzer);
}
