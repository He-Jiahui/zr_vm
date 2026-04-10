#include "lsp_interface_internal.h"

typedef struct SZrLspDecoratorHit {
    SZrAstNode *decoratorNode;
    SZrAstNode *ownerNode;
} SZrLspDecoratorHit;

typedef struct SZrLspDecoratorTarget {
    SZrFileRange range;
    SZrString *name;
    const TZrChar *kind;
} SZrLspDecoratorTarget;

static TZrNativeString decorator_navigation_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

static TZrBool decorator_navigation_file_range_contains_position(SZrFileRange range, SZrFileRange position) {
    TZrInt32 startColumn;

    if (!ZrLanguageServer_Lsp_StringsEqual(range.source, position.source) &&
        range.source != ZR_NULL &&
        position.source != ZR_NULL) {
        return ZR_FALSE;
    }

    startColumn = range.start.column > 0 ? range.start.column - 1 : range.start.column;

    return (range.start.line < position.start.line ||
            (range.start.line == position.start.line &&
             startColumn <= position.start.column)) &&
           (position.end.line < range.end.line ||
            (position.end.line == range.end.line &&
             position.end.column <= range.end.column));
}

static TZrBool decorator_navigation_range_is_valid(SZrFileRange range) {
    return range.source != ZR_NULL ||
           range.start.offset > 0 ||
           range.end.offset > 0 ||
           range.start.line != 0 ||
           range.start.column != 0 ||
           range.end.line != 0 ||
           range.end.column != 0;
}

static void decorator_navigation_append_text(TZrChar *buffer,
                                             TZrSize bufferSize,
                                             TZrSize *used,
                                             const TZrChar *text) {
    TZrSize textLength;

    if (buffer == ZR_NULL || used == ZR_NULL || text == ZR_NULL) {
        return;
    }

    textLength = strlen(text);
    if (*used >= bufferSize || textLength == 0 || *used + textLength + 1 >= bufferSize) {
        return;
    }

    memcpy(buffer + *used, text, textLength);
    *used += textLength;
    buffer[*used] = '\0';
}

static void decorator_navigation_append_string(TZrChar *buffer,
                                               TZrSize bufferSize,
                                               TZrSize *used,
                                               SZrString *value) {
    decorator_navigation_append_text(buffer, bufferSize, used, decorator_navigation_string_text(value));
}

static void decorator_navigation_build_expr_text(SZrAstNode *node,
                                                 TZrChar *buffer,
                                                 TZrSize bufferSize,
                                                 TZrSize *used) {
    if (node == ZR_NULL || buffer == ZR_NULL || used == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            decorator_navigation_append_string(buffer, bufferSize, used, node->data.identifier.name);
            break;

        case ZR_AST_STRING_LITERAL:
            decorator_navigation_append_string(buffer, bufferSize, used, node->data.stringLiteral.value);
            break;

        case ZR_AST_PRIMARY_EXPRESSION:
            decorator_navigation_build_expr_text(node->data.primaryExpression.property, buffer, bufferSize, used);
            if (node->data.primaryExpression.members != ZR_NULL &&
                node->data.primaryExpression.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.primaryExpression.members->count; index++) {
                    SZrAstNode *memberNode = node->data.primaryExpression.members->nodes[index];
                    if (memberNode == ZR_NULL) {
                        continue;
                    }

                    if (memberNode->type == ZR_AST_MEMBER_EXPRESSION) {
                        decorator_navigation_append_text(buffer, bufferSize, used,
                                                         memberNode->data.memberExpression.computed ? "[]" : ".");
                        decorator_navigation_build_expr_text(memberNode->data.memberExpression.property,
                                                             buffer,
                                                             bufferSize,
                                                             used);
                    } else if (memberNode->type == ZR_AST_FUNCTION_CALL) {
                        if (memberNode->data.functionCall.args != ZR_NULL &&
                            memberNode->data.functionCall.args->count > 0) {
                            decorator_navigation_append_text(buffer, bufferSize, used, "(...)");
                        } else {
                            decorator_navigation_append_text(buffer, bufferSize, used, "()");
                        }
                    }
                }
            }
            break;

        default:
            break;
    }
}

static TZrBool decorator_navigation_match_decorator_array(SZrAstNodeArray *decorators,
                                                          SZrFileRange position,
                                                          SZrAstNode *ownerNode,
                                                          SZrLspDecoratorHit *outHit) {
    if (decorators == ZR_NULL || decorators->nodes == ZR_NULL || ownerNode == ZR_NULL || outHit == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        TZrBool contains = decoratorNode != ZR_NULL
                               ? decorator_navigation_file_range_contains_position(decoratorNode->location, position)
                               : ZR_FALSE;
        if (decoratorNode != ZR_NULL &&
            decoratorNode->type == ZR_AST_DECORATOR_EXPRESSION &&
            contains) {
            outHit->decoratorNode = decoratorNode;
            outHit->ownerNode = ownerNode;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool decorator_navigation_find_hit_recursive(SZrAstNode *node,
                                                       SZrFileRange position,
                                                       SZrLspDecoratorHit *outHit) {
    if (node == ZR_NULL || outHit == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    if (decorator_navigation_find_hit_recursive(node->data.script.statements->nodes[index],
                                                                position,
                                                                outHit)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    if (decorator_navigation_find_hit_recursive(node->data.block.body->nodes[index],
                                                                position,
                                                                outHit)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return decorator_navigation_find_hit_recursive(node->data.compileTimeDeclaration.declaration,
                                                           position,
                                                           outHit);

        case ZR_AST_FUNCTION_DECLARATION:
            return decorator_navigation_match_decorator_array(node->data.functionDeclaration.decorators,
                                                              position,
                                                              node,
                                                              outHit);

        case ZR_AST_CLASS_DECLARATION:
            if (decorator_navigation_match_decorator_array(node->data.classDeclaration.decorators,
                                                           position,
                                                           node,
                                                           outHit)) {
                return ZR_TRUE;
            }
            if (node->data.classDeclaration.members != ZR_NULL &&
                node->data.classDeclaration.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.classDeclaration.members->count; index++) {
                    if (decorator_navigation_find_hit_recursive(node->data.classDeclaration.members->nodes[index],
                                                                position,
                                                                outHit)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        case ZR_AST_CLASS_FIELD:
            return decorator_navigation_match_decorator_array(node->data.classField.decorators, position, node, outHit);

        case ZR_AST_CLASS_METHOD:
            return decorator_navigation_match_decorator_array(node->data.classMethod.decorators, position, node, outHit);

        case ZR_AST_CLASS_PROPERTY:
            return decorator_navigation_match_decorator_array(node->data.classProperty.decorators,
                                                              position,
                                                              node,
                                                              outHit);

        case ZR_AST_STRUCT_DECLARATION:
            if (decorator_navigation_match_decorator_array(node->data.structDeclaration.decorators,
                                                           position,
                                                           node,
                                                           outHit)) {
                return ZR_TRUE;
            }
            if (node->data.structDeclaration.members != ZR_NULL &&
                node->data.structDeclaration.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.structDeclaration.members->count; index++) {
                    if (decorator_navigation_find_hit_recursive(node->data.structDeclaration.members->nodes[index],
                                                                position,
                                                                outHit)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        case ZR_AST_STRUCT_FIELD:
            return decorator_navigation_match_decorator_array(node->data.structField.decorators,
                                                              position,
                                                              node,
                                                              outHit);

        case ZR_AST_STRUCT_METHOD:
            return decorator_navigation_match_decorator_array(node->data.structMethod.decorators,
                                                              position,
                                                              node,
                                                              outHit);

        case ZR_AST_ENUM_DECLARATION:
            if (decorator_navigation_match_decorator_array(node->data.enumDeclaration.decorators,
                                                           position,
                                                           node,
                                                           outHit)) {
                return ZR_TRUE;
            }
            if (node->data.enumDeclaration.members != ZR_NULL &&
                node->data.enumDeclaration.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.enumDeclaration.members->count; index++) {
                    if (decorator_navigation_find_hit_recursive(node->data.enumDeclaration.members->nodes[index],
                                                                position,
                                                                outHit)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        case ZR_AST_ENUM_MEMBER:
            return decorator_navigation_match_decorator_array(node->data.enumMember.decorators,
                                                              position,
                                                              node,
                                                              outHit);

        case ZR_AST_EXTERN_BLOCK:
            if (node->data.externBlock.declarations != ZR_NULL && node->data.externBlock.declarations->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.externBlock.declarations->count; index++) {
                    if (decorator_navigation_find_hit_recursive(node->data.externBlock.declarations->nodes[index],
                                                                position,
                                                                outHit)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            return decorator_navigation_match_decorator_array(node->data.externFunctionDeclaration.decorators,
                                                              position,
                                                              node,
                                                              outHit);

        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            return decorator_navigation_match_decorator_array(node->data.externDelegateDeclaration.decorators,
                                                              position,
                                                              node,
                                                              outHit);

        default:
            break;
    }

    return ZR_FALSE;
}

static SZrFileRange decorator_navigation_resolve_property_range(SZrAstNode *propertyNode, SZrString **outName) {
    SZrFileRange emptyRange;

    if (outName != ZR_NULL) {
        *outName = ZR_NULL;
    }
    if (propertyNode == ZR_NULL) {
        memset(&emptyRange, 0, sizeof(emptyRange));
        return emptyRange;
    }

    if (propertyNode->type == ZR_AST_PROPERTY_GET) {
        if (outName != ZR_NULL && propertyNode->data.propertyGet.name != ZR_NULL) {
            *outName = propertyNode->data.propertyGet.name->name;
        }
        if (decorator_navigation_range_is_valid(propertyNode->data.propertyGet.nameLocation)) {
            return propertyNode->data.propertyGet.nameLocation;
        }
    } else if (propertyNode->type == ZR_AST_PROPERTY_SET) {
        if (outName != ZR_NULL && propertyNode->data.propertySet.name != ZR_NULL) {
            *outName = propertyNode->data.propertySet.name->name;
        }
        if (decorator_navigation_range_is_valid(propertyNode->data.propertySet.nameLocation)) {
            return propertyNode->data.propertySet.nameLocation;
        }
    }

    return propertyNode->location;
}

static TZrBool decorator_navigation_resolve_target(SZrAstNode *ownerNode, SZrLspDecoratorTarget *outTarget) {
    if (ownerNode == ZR_NULL || outTarget == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outTarget, 0, sizeof(*outTarget));
    outTarget->range = ownerNode->location;
    outTarget->kind = "declaration";

    switch (ownerNode->type) {
        case ZR_AST_CLASS_DECLARATION:
            outTarget->name = ownerNode->data.classDeclaration.name != ZR_NULL
                                  ? ownerNode->data.classDeclaration.name->name
                                  : ZR_NULL;
            outTarget->kind = "class";
            if (decorator_navigation_range_is_valid(ownerNode->data.classDeclaration.nameLocation)) {
                outTarget->range = ownerNode->data.classDeclaration.nameLocation;
            }
            return ZR_TRUE;

        case ZR_AST_CLASS_METHOD:
            outTarget->name = ownerNode->data.classMethod.name != ZR_NULL ? ownerNode->data.classMethod.name->name : ZR_NULL;
            outTarget->kind = "method";
            if (decorator_navigation_range_is_valid(ownerNode->data.classMethod.nameLocation)) {
                outTarget->range = ownerNode->data.classMethod.nameLocation;
            }
            return ZR_TRUE;

        case ZR_AST_CLASS_FIELD:
            outTarget->name = ownerNode->data.classField.name != ZR_NULL ? ownerNode->data.classField.name->name : ZR_NULL;
            outTarget->kind = "field";
            if (decorator_navigation_range_is_valid(ownerNode->data.classField.nameLocation)) {
                outTarget->range = ownerNode->data.classField.nameLocation;
            }
            return ZR_TRUE;

        case ZR_AST_CLASS_PROPERTY:
            outTarget->kind = "property";
            outTarget->range = decorator_navigation_resolve_property_range(ownerNode->data.classProperty.modifier,
                                                                           &outTarget->name);
            return ZR_TRUE;

        case ZR_AST_FUNCTION_DECLARATION:
            outTarget->name = ownerNode->data.functionDeclaration.name != ZR_NULL
                                  ? ownerNode->data.functionDeclaration.name->name
                                  : ZR_NULL;
            outTarget->kind = "function";
            if (decorator_navigation_range_is_valid(ownerNode->data.functionDeclaration.nameLocation)) {
                outTarget->range = ownerNode->data.functionDeclaration.nameLocation;
            }
            return ZR_TRUE;

        case ZR_AST_STRUCT_DECLARATION:
            outTarget->name = ownerNode->data.structDeclaration.name != ZR_NULL
                                  ? ownerNode->data.structDeclaration.name->name
                                  : ZR_NULL;
            outTarget->kind = "struct";
            return ZR_TRUE;

        case ZR_AST_STRUCT_FIELD:
            outTarget->name = ownerNode->data.structField.name != ZR_NULL ? ownerNode->data.structField.name->name : ZR_NULL;
            outTarget->kind = "field";
            return ZR_TRUE;

        case ZR_AST_STRUCT_METHOD:
            outTarget->name = ownerNode->data.structMethod.name != ZR_NULL ? ownerNode->data.structMethod.name->name : ZR_NULL;
            outTarget->kind = "method";
            return ZR_TRUE;

        case ZR_AST_ENUM_DECLARATION:
            outTarget->name = ownerNode->data.enumDeclaration.name != ZR_NULL
                                  ? ownerNode->data.enumDeclaration.name->name
                                  : ZR_NULL;
            outTarget->kind = "enum";
            return ZR_TRUE;

        case ZR_AST_ENUM_MEMBER:
            outTarget->name = ownerNode->data.enumMember.name != ZR_NULL ? ownerNode->data.enumMember.name->name : ZR_NULL;
            outTarget->kind = "enum member";
            return ZR_TRUE;

        case ZR_AST_INTERFACE_DECLARATION:
            outTarget->name = ownerNode->data.interfaceDeclaration.name != ZR_NULL
                                  ? ownerNode->data.interfaceDeclaration.name->name
                                  : ZR_NULL;
            outTarget->kind = "interface";
            return ZR_TRUE;

        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            outTarget->name = ownerNode->data.externFunctionDeclaration.name != ZR_NULL
                                  ? ownerNode->data.externFunctionDeclaration.name->name
                                  : ZR_NULL;
            outTarget->kind = "extern function";
            return ZR_TRUE;

        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            outTarget->name = ownerNode->data.externDelegateDeclaration.name != ZR_NULL
                                  ? ownerNode->data.externDelegateDeclaration.name->name
                                  : ZR_NULL;
            outTarget->kind = "extern delegate";
            return ZR_TRUE;

        default:
            return ZR_FALSE;
    }
}

static TZrBool decorator_navigation_find_hit(SZrSemanticAnalyzer *analyzer,
                                             SZrFileRange position,
                                             SZrLspDecoratorHit *outHit) {
    if (outHit != ZR_NULL) {
        outHit->decoratorNode = ZR_NULL;
        outHit->ownerNode = ZR_NULL;
    }
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL || outHit == ZR_NULL) {
        return ZR_FALSE;
    }

    return decorator_navigation_find_hit_recursive(analyzer->ast, position, outHit);
}

static TZrBool decorator_navigation_append_definition(SZrState *state,
                                                      SZrArray *result,
                                                      SZrString *uri,
                                                      SZrFileRange range) {
    SZrLspLocation *location;

    if (state == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }

    location->uri = range.source != ZR_NULL ? range.source : uri;
    location->range = ZrLanguageServer_LspRange_FromFileRange(range);
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

static TZrBool decorator_navigation_create_hover(SZrState *state,
                                                 SZrAstNode *decoratorNode,
                                                 const TZrChar *decoratorText,
                                                 const TZrChar *category,
                                                 SZrLspDecoratorTarget target,
                                                 SZrLspHover **result) {
    SZrLspHover *hover;
    SZrString *content;
    TZrChar buffer[ZR_LSP_MARKDOWN_BUFFER_SIZE];
    TZrSize used = 0;

    if (state == ZR_NULL || decoratorNode == ZR_NULL || decoratorText == ZR_NULL || category == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    decorator_navigation_append_text(buffer, sizeof(buffer), &used, "Decorator: ");
    decorator_navigation_append_text(buffer, sizeof(buffer), &used, decoratorText);
    decorator_navigation_append_text(buffer, sizeof(buffer), &used, "\n\nCategory: ");
    decorator_navigation_append_text(buffer, sizeof(buffer), &used, category);
    decorator_navigation_append_text(buffer, sizeof(buffer), &used, "\n\nTarget: ");
    decorator_navigation_append_text(buffer, sizeof(buffer), &used, target.kind);
    if (target.name != ZR_NULL) {
        decorator_navigation_append_text(buffer, sizeof(buffer), &used, " ");
        decorator_navigation_append_string(buffer, sizeof(buffer), &used, target.name);
    }

    content = ZrCore_String_Create(state, buffer, used);
    if (content == ZR_NULL) {
        return ZR_FALSE;
    }

    hover = (SZrLspHover *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHover));
    if (hover == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1);
    ZrCore_Array_Push(state, &hover->contents, &content);
    hover->range = ZrLanguageServer_LspRange_FromFileRange(decoratorNode->location);
    *result = hover;
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_TryGetDecoratorDefinition(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrString *uri,
                                                       SZrLspPosition position,
                                                       SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrLspDecoratorHit hit;
    SZrLspDecoratorTarget target;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    if (!decorator_navigation_find_hit(analyzer, fileRange, &hit) ||
        !decorator_navigation_resolve_target(hit.ownerNode, &target)) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), 1);
    }

    return decorator_navigation_append_definition(state, result, uri, target.range);
}

TZrBool ZrLanguageServer_Lsp_TryGetDecoratorHover(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrLspPosition position,
                                                  SZrLspHover **result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrLspDecoratorHit hit;
    SZrLspDecoratorTarget target;
    TZrChar decoratorBuffer[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrSize used = 0;
    const TZrChar *category;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    if (!decorator_navigation_find_hit(analyzer, fileRange, &hit) ||
        hit.decoratorNode == ZR_NULL ||
        hit.decoratorNode->type != ZR_AST_DECORATOR_EXPRESSION ||
        !decorator_navigation_resolve_target(hit.ownerNode, &target)) {
        return ZR_FALSE;
    }

    decoratorBuffer[0] = '\0';
    decorator_navigation_build_expr_text(hit.decoratorNode->data.decoratorExpression.expr,
                                         decoratorBuffer,
                                         sizeof(decoratorBuffer),
                                         &used);
    if (used == 0) {
        decorator_navigation_append_text(decoratorBuffer, sizeof(decoratorBuffer), &used, "decorator");
    }

    category = strncmp(decoratorBuffer, "zr.ffi.", strlen("zr.ffi.")) == 0 ? "ffi decorator" : "decorator";
    return decorator_navigation_create_hover(state, hit.decoratorNode, decoratorBuffer, category, target, result);
}
