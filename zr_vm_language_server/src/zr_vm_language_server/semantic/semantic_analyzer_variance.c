#include "semantic/semantic_analyzer_internal.h"

typedef enum EZrSemanticVariancePosition {
    ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT = 0,
    ZR_SEMANTIC_VARIANCE_POSITION_OUT = 1,
    ZR_SEMANTIC_VARIANCE_POSITION_IN = -1,
} EZrSemanticVariancePosition;

static EZrGenericVariance semantic_interface_generic_variance_for_name(SZrAstNodeArray *params, SZrString *name) {
    if (params == ZR_NULL || name == ZR_NULL) {
        return ZR_GENERIC_VARIANCE_NONE;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        if (paramNode == ZR_NULL ||
            paramNode->type != ZR_AST_PARAMETER ||
            paramNode->data.parameter.name == ZR_NULL ||
            paramNode->data.parameter.name->name == ZR_NULL) {
            continue;
        }

        if (ZrCore_String_Equal(paramNode->data.parameter.name->name, name)) {
            return paramNode->data.parameter.variance;
        }
    }

    return ZR_GENERIC_VARIANCE_NONE;
}

static SZrInterfaceDeclaration *semantic_lookup_interface_declaration(SZrSemanticAnalyzer *analyzer,
                                                                      SZrString *typeName) {
    SZrSymbol *symbol;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, typeName, ZR_NULL);
    if (symbol == ZR_NULL ||
        symbol->type != ZR_SYMBOL_INTERFACE ||
        symbol->astNode == ZR_NULL ||
        symbol->astNode->type != ZR_AST_INTERFACE_DECLARATION) {
        return ZR_NULL;
    }

    return &symbol->astNode->data.interfaceDeclaration;
}

static EZrGenericVariance semantic_interface_decl_variance_at(SZrSemanticAnalyzer *analyzer,
                                                              SZrString *typeName,
                                                              TZrSize index) {
    SZrInterfaceDeclaration *interfaceDecl = semantic_lookup_interface_declaration(analyzer, typeName);
    SZrAstNode *paramNode;

    if (interfaceDecl == ZR_NULL ||
        interfaceDecl->generic == ZR_NULL ||
        interfaceDecl->generic->params == ZR_NULL ||
        index >= interfaceDecl->generic->params->count) {
        return ZR_GENERIC_VARIANCE_NONE;
    }

    paramNode = interfaceDecl->generic->params->nodes[index];
    if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
        return ZR_GENERIC_VARIANCE_NONE;
    }

    return paramNode->data.parameter.variance;
}

static EZrSemanticVariancePosition semantic_combine_variance_position(EZrSemanticVariancePosition outerPosition,
                                                                      EZrGenericVariance declaredVariance) {
    if (outerPosition == ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT ||
        declaredVariance == ZR_GENERIC_VARIANCE_NONE) {
        return ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT;
    }

    if (declaredVariance == ZR_GENERIC_VARIANCE_OUT) {
        return outerPosition;
    }

    return outerPosition == ZR_SEMANTIC_VARIANCE_POSITION_OUT
           ? ZR_SEMANTIC_VARIANCE_POSITION_IN
           : ZR_SEMANTIC_VARIANCE_POSITION_OUT;
}

static void semantic_add_invalid_variance_diagnostic(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     SZrString *parameterName,
                                                     EZrGenericVariance declaredVariance,
                                                     const TZrChar *context,
                                                     TZrBool nestedUsage,
                                                     SZrFileRange location) {
    TZrChar buffer[ZR_LSP_TEXT_BUFFER_LENGTH];
    const TZrChar *nameText = semantic_string_native(parameterName);

    if (state == ZR_NULL || analyzer == ZR_NULL || parameterName == ZR_NULL || context == ZR_NULL) {
        return;
    }

    snprintf(buffer,
             sizeof(buffer),
             "%s generic parameter '%s' cannot be used in %s%s position",
             declaredVariance == ZR_GENERIC_VARIANCE_OUT ? "covariant" : "contravariant",
             nameText != ZR_NULL ? nameText : "<unknown>",
             nestedUsage ? "nested " : "",
             context);
    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                    analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    location,
                                                    buffer,
                                                    "invalid_variance");
}

static void semantic_validate_interface_type_variance(SZrState *state,
                                                      SZrSemanticAnalyzer *analyzer,
                                                      SZrAstNodeArray *interfaceGenericParams,
                                                      SZrType *typeNode,
                                                      EZrSemanticVariancePosition position,
                                                      const TZrChar *context,
                                                      TZrBool nestedUsage,
                                                      SZrFileRange location) {
    SZrString *typeName = ZR_NULL;
    EZrGenericVariance declaredVariance;

    if (state == ZR_NULL || analyzer == ZR_NULL || interfaceGenericParams == ZR_NULL || typeNode == ZR_NULL) {
        return;
    }

    if (typeNode->subType != ZR_NULL) {
        semantic_validate_interface_type_variance(state,
                                                 analyzer,
                                                 interfaceGenericParams,
                                                 typeNode->subType,
                                                 ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT,
                                                 context,
                                                 nestedUsage,
                                                 location);
    }

    if (typeNode->name == ZR_NULL) {
        return;
    }

    if (typeNode->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        typeName = typeNode->name->data.identifier.name;
        declaredVariance = semantic_interface_generic_variance_for_name(interfaceGenericParams, typeName);
        if (declaredVariance == ZR_GENERIC_VARIANCE_NONE) {
            return;
        }

        if (position == ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT ||
            (declaredVariance == ZR_GENERIC_VARIANCE_OUT && position == ZR_SEMANTIC_VARIANCE_POSITION_IN) ||
            (declaredVariance == ZR_GENERIC_VARIANCE_IN && position == ZR_SEMANTIC_VARIANCE_POSITION_OUT)) {
            semantic_add_invalid_variance_diagnostic(state,
                                                     analyzer,
                                                     typeName,
                                                     declaredVariance,
                                                     context,
                                                     nestedUsage,
                                                     location);
        }
        return;
    }

    if (typeNode->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &typeNode->name->data.genericType;
        SZrString *outerTypeName = genericType->name != ZR_NULL ? genericType->name->name : ZR_NULL;

        if (genericType->params == ZR_NULL) {
            return;
        }

        for (TZrSize index = 0; index < genericType->params->count; index++) {
            SZrAstNode *argNode = genericType->params->nodes[index];
            EZrGenericVariance declaredOuterVariance =
                    semantic_interface_decl_variance_at(analyzer, outerTypeName, index);
            EZrSemanticVariancePosition childPosition =
                    semantic_combine_variance_position(position, declaredOuterVariance);

            if (argNode != ZR_NULL && argNode->type == ZR_AST_TYPE) {
                semantic_validate_interface_type_variance(state,
                                                         analyzer,
                                                         interfaceGenericParams,
                                                         &argNode->data.type,
                                                         childPosition,
                                                         context,
                                                         ZR_TRUE,
                                                         location);
            }
        }
    }
}

void ZrLanguageServer_SemanticAnalyzer_ValidateInterfaceVarianceRules(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *interfaceNode) {
    SZrInterfaceDeclaration *interfaceDecl;

    if (state == ZR_NULL ||
        analyzer == ZR_NULL ||
        interfaceNode == ZR_NULL ||
        interfaceNode->type != ZR_AST_INTERFACE_DECLARATION) {
        return;
    }

    interfaceDecl = &interfaceNode->data.interfaceDeclaration;
    if (interfaceDecl->generic == ZR_NULL ||
        interfaceDecl->generic->params == ZR_NULL ||
        interfaceDecl->members == ZR_NULL) {
        return;
    }

    for (TZrSize memberIndex = 0; memberIndex < interfaceDecl->members->count; memberIndex++) {
        SZrAstNode *member = interfaceDecl->members->nodes[memberIndex];

        if (member == ZR_NULL) {
            continue;
        }

        switch (member->type) {
            case ZR_AST_INTERFACE_FIELD_DECLARATION:
                semantic_validate_interface_type_variance(state,
                                                         analyzer,
                                                         interfaceDecl->generic->params,
                                                         member->data.interfaceFieldDeclaration.typeInfo,
                                                         ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT,
                                                         "field",
                                                         ZR_FALSE,
                                                         member->location);
                break;

            case ZR_AST_INTERFACE_METHOD_SIGNATURE:
                if (member->data.interfaceMethodSignature.params != ZR_NULL) {
                    for (TZrSize paramIndex = 0;
                         paramIndex < member->data.interfaceMethodSignature.params->count;
                         paramIndex++) {
                        SZrAstNode *paramNode = member->data.interfaceMethodSignature.params->nodes[paramIndex];
                        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                            semantic_validate_interface_type_variance(state,
                                                                     analyzer,
                                                                     interfaceDecl->generic->params,
                                                                     paramNode->data.parameter.typeInfo,
                                                                     ZR_SEMANTIC_VARIANCE_POSITION_IN,
                                                                     "contravariant parameter",
                                                                     ZR_FALSE,
                                                                     paramNode->location);
                        }
                    }
                }
                semantic_validate_interface_type_variance(state,
                                                         analyzer,
                                                         interfaceDecl->generic->params,
                                                         member->data.interfaceMethodSignature.returnType,
                                                         ZR_SEMANTIC_VARIANCE_POSITION_OUT,
                                                         "covariant return",
                                                         ZR_FALSE,
                                                         member->location);
                break;

            case ZR_AST_INTERFACE_PROPERTY_SIGNATURE: {
                SZrInterfacePropertySignature *property = &member->data.interfacePropertySignature;
                EZrSemanticVariancePosition propertyPosition = ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT;
                const TZrChar *context = "property";

                if (property->hasGet && !property->hasSet) {
                    propertyPosition = ZR_SEMANTIC_VARIANCE_POSITION_OUT;
                    context = "getter";
                } else if (property->hasSet && !property->hasGet) {
                    propertyPosition = ZR_SEMANTIC_VARIANCE_POSITION_IN;
                    context = "setter";
                }

                semantic_validate_interface_type_variance(state,
                                                         analyzer,
                                                         interfaceDecl->generic->params,
                                                         property->typeInfo,
                                                         propertyPosition,
                                                         context,
                                                         ZR_FALSE,
                                                         member->location);
                break;
            }

            case ZR_AST_PROPERTY_DECLARATION: {
                SZrPropertyDeclaration *property = &member->data.propertyDeclaration;
                TZrBool hasGet = ZR_FALSE;
                TZrBool hasWrite = ZR_FALSE;
                EZrSemanticVariancePosition propertyPosition = ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT;
                const TZrChar *context = "property";

                for (TZrSize accessorIndex = 0U;
                     property->accessors != ZR_NULL && accessorIndex < property->accessors->count;
                     accessorIndex++) {
                    SZrAstNode *accessorNode = property->accessors->nodes[accessorIndex];

                    if (accessorNode == ZR_NULL || accessorNode->type != ZR_AST_PROPERTY_ACCESSOR) {
                        continue;
                    }
                    if (accessorNode->data.propertyAccessor.kind == ZR_PROPERTY_ACCESSOR_GET) {
                        hasGet = ZR_TRUE;
                    } else {
                        hasWrite = ZR_TRUE;
                    }
                }

                if (hasGet && !hasWrite) {
                    propertyPosition = ZR_SEMANTIC_VARIANCE_POSITION_OUT;
                    context = "getter";
                } else if (hasWrite && !hasGet) {
                    propertyPosition = ZR_SEMANTIC_VARIANCE_POSITION_IN;
                    context = "setter";
                }
                semantic_validate_interface_type_variance(state,
                                                         analyzer,
                                                         interfaceDecl->generic->params,
                                                         property->typeInfo,
                                                         propertyPosition,
                                                         context,
                                                         ZR_FALSE,
                                                         member->location);
                break;
            }

            case ZR_AST_INTERFACE_META_SIGNATURE:
                if (member->data.interfaceMetaSignature.params != ZR_NULL) {
                    for (TZrSize paramIndex = 0;
                         paramIndex < member->data.interfaceMetaSignature.params->count;
                         paramIndex++) {
                        SZrAstNode *paramNode = member->data.interfaceMetaSignature.params->nodes[paramIndex];
                        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                            semantic_validate_interface_type_variance(state,
                                                                     analyzer,
                                                                     interfaceDecl->generic->params,
                                                                     paramNode->data.parameter.typeInfo,
                                                                     ZR_SEMANTIC_VARIANCE_POSITION_IN,
                                                                     "contravariant parameter",
                                                                     ZR_FALSE,
                                                                     paramNode->location);
                        }
                    }
                }
                semantic_validate_interface_type_variance(state,
                                                         analyzer,
                                                         interfaceDecl->generic->params,
                                                         member->data.interfaceMetaSignature.returnType,
                                                         ZR_SEMANTIC_VARIANCE_POSITION_OUT,
                                                         "covariant return",
                                                         ZR_FALSE,
                                                         member->location);
                break;

            default:
                break;
        }
    }
}
