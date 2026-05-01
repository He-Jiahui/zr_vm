#include "interface/lsp_interface_internal.h"

#include <ctype.h>
#include <string.h>

static TZrBool super_navigation_file_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (!ZrLanguageServer_Lsp_StringsEqual(range.source, position.source) &&
        range.source != ZR_NULL &&
        position.source != ZR_NULL) {
        return ZR_FALSE;
    }

    if (range.start.offset > 0 && range.end.offset > 0 &&
        position.start.offset > 0 && position.end.offset > 0) {
        return range.start.offset <= position.start.offset &&
               position.end.offset <= range.end.offset;
    }

    return (range.start.line < position.start.line ||
            (range.start.line == position.start.line &&
             range.start.column <= position.start.column)) &&
           (position.end.line < range.end.line ||
            (position.end.line == range.end.line &&
             position.end.column <= range.end.column));
}

static TZrBool super_navigation_meta_function_is_constructor(SZrAstNode *metaFunctionNode) {
    SZrString *metaName = ZR_NULL;

    if (metaFunctionNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (metaFunctionNode->type == ZR_AST_CLASS_META_FUNCTION &&
        metaFunctionNode->data.classMetaFunction.meta != ZR_NULL) {
        metaName = metaFunctionNode->data.classMetaFunction.meta->name;
    } else if (metaFunctionNode->type == ZR_AST_STRUCT_META_FUNCTION &&
               metaFunctionNode->data.structMetaFunction.meta != ZR_NULL) {
        metaName = metaFunctionNode->data.structMetaFunction.meta->name;
    }

    return metaName != ZR_NULL &&
           metaName->shortStringLength == strlen("constructor") &&
           strcmp(ZrCore_String_GetNativeStringShort(metaName), "constructor") == 0;
}

static TZrBool super_navigation_is_identifier_char(TZrChar value) {
    return isalnum((unsigned char)value) || value == '_';
}

static SZrFilePosition super_navigation_file_position_from_offset(const TZrChar *content,
                                                                  TZrSize contentLength,
                                                                  TZrSize targetOffset) {
    SZrFilePosition position;
    TZrSize offset = 0;

    position.line = 1;
    position.column = 1;
    position.offset = 0;

    while (offset < contentLength && offset < targetOffset) {
        if (content[offset] == '\n') {
            position.line++;
            position.column = 1;
        } else if (content[offset] != '\r') {
            position.column++;
        }
        offset++;
    }

    position.offset = targetOffset;
    return position;
}

static TZrBool super_navigation_find_super_token_range(const TZrChar *content,
                                                       TZrSize contentLength,
                                                       SZrFileRange scope,
                                                       SZrString *uri,
                                                       SZrFileRange *outRange) {
    TZrSize startOffset;
    TZrSize endOffset;

    if (content == ZR_NULL || contentLength == 0 || outRange == ZR_NULL) {
        return ZR_FALSE;
    }

    startOffset = scope.start.offset < contentLength ? scope.start.offset : 0;
    endOffset = scope.end.offset > startOffset && scope.end.offset <= contentLength ? scope.end.offset : contentLength;
    for (TZrSize index = startOffset; index + 5 <= endOffset; index++) {
        if (memcmp(content + index, "super", 5) != 0) {
            continue;
        }
        if (index > 0 && super_navigation_is_identifier_char(content[index - 1])) {
            continue;
        }
        if (index + 5 < contentLength && super_navigation_is_identifier_char(content[index + 5])) {
            continue;
        }

        *outRange = ZrParser_FileRange_Create(super_navigation_file_position_from_offset(content, contentLength, index),
                                              super_navigation_file_position_from_offset(content,
                                                                                         contentLength,
                                                                                         index + 5),
                                              uri);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool super_navigation_position_is_super_token(const TZrChar *content,
                                                        TZrSize contentLength,
                                                        TZrSize offset) {
    TZrSize start;
    TZrSize end;

    if (content == ZR_NULL || contentLength == 0) {
        return ZR_FALSE;
    }

    if (offset >= contentLength) {
        offset = contentLength - 1;
    }
    if (!super_navigation_is_identifier_char(content[offset]) &&
        offset > 0 &&
        super_navigation_is_identifier_char(content[offset - 1])) {
        offset--;
    }
    if (!super_navigation_is_identifier_char(content[offset])) {
        return ZR_FALSE;
    }

    start = offset;
    while (start > 0 && super_navigation_is_identifier_char(content[start - 1])) {
        start--;
    }
    end = offset + 1;
    while (end < contentLength && super_navigation_is_identifier_char(content[end])) {
        end++;
    }

    return end - start == 5 && memcmp(content + start, "super", 5) == 0;
}

static SZrFileRange super_navigation_super_call_context_range(SZrAstNode *metaFunctionNode) {
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    if (metaFunctionNode != ZR_NULL) {
        range = metaFunctionNode->location;
    }
    if (metaFunctionNode == ZR_NULL || metaFunctionNode->type != ZR_AST_CLASS_META_FUNCTION) {
        return range;
    }

    if (metaFunctionNode->data.classMetaFunction.superArgs != ZR_NULL &&
        metaFunctionNode->data.classMetaFunction.superArgs->count > 0 &&
        metaFunctionNode->data.classMetaFunction.superArgs->nodes != ZR_NULL &&
        metaFunctionNode->data.classMetaFunction.superArgs->nodes[0] != ZR_NULL) {
        SZrAstNode *lastArgNode =
            metaFunctionNode->data.classMetaFunction.superArgs
                ->nodes[metaFunctionNode->data.classMetaFunction.superArgs->count - 1];
        range.start = metaFunctionNode->data.classMetaFunction.superArgs->nodes[0]->location.start;
        if (range.start.offset >= 6) {
            range.start.offset -= 6;
        }
        if (range.start.column >= 6) {
            range.start.column -= 6;
        }
        range.end = lastArgNode != ZR_NULL ? lastArgNode->location.end : range.end;
    } else if (metaFunctionNode->data.classMetaFunction.body != ZR_NULL) {
        range.end = metaFunctionNode->data.classMetaFunction.body->location.start;
    }

    return range;
}

static SZrFileRange super_navigation_super_call_token_range(SZrAstNode *metaFunctionNode,
                                                            const TZrChar *content,
                                                            TZrSize contentLength,
                                                            SZrString *uri) {
    SZrFileRange scope;
    SZrFileRange tokenRange;

    scope = super_navigation_super_call_context_range(metaFunctionNode);
    if (metaFunctionNode != ZR_NULL && metaFunctionNode->type == ZR_AST_CLASS_META_FUNCTION) {
        scope = metaFunctionNode->location;
        if (metaFunctionNode->data.classMetaFunction.body != ZR_NULL) {
            scope.end = metaFunctionNode->data.classMetaFunction.body->location.start;
        }
    }

    if (super_navigation_find_super_token_range(content, contentLength, scope, uri, &tokenRange)) {
        return tokenRange;
    }

    return super_navigation_super_call_context_range(metaFunctionNode);
}

static TZrBool super_navigation_super_call_matches_position(SZrAstNode *metaFunctionNode,
                                                            SZrFileRange position,
                                                            const TZrChar *content,
                                                            TZrSize contentLength,
                                                            SZrString *uri) {
    SZrClassMetaFunction *metaFunction;
    SZrFileRange superRange;

    if (metaFunctionNode == ZR_NULL || metaFunctionNode->type != ZR_AST_CLASS_META_FUNCTION) {
        return ZR_FALSE;
    }

    metaFunction = &metaFunctionNode->data.classMetaFunction;
    if (!metaFunction->hasSuperCall || !super_navigation_meta_function_is_constructor(metaFunctionNode)) {
        return ZR_FALSE;
    }

    superRange = super_navigation_super_call_token_range(metaFunctionNode, content, contentLength, uri);
    if (super_navigation_file_range_contains_position(superRange, position)) {
        return ZR_TRUE;
    }

    if (metaFunction->superArgs != ZR_NULL) {
        for (TZrSize index = 0; index < metaFunction->superArgs->count; index++) {
            SZrAstNode *argNode = metaFunction->superArgs->nodes[index];
            if (argNode != ZR_NULL && super_navigation_file_range_contains_position(argNode->location, position)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static TZrBool super_navigation_find_super_constructor_context(SZrAstNode *node,
                                                               SZrFileRange position,
                                                               const TZrChar *content,
                                                               TZrSize contentLength,
                                                               SZrString *uri,
                                                               SZrAstNode **ownerTypeNode,
                                                               SZrAstNode **metaFunctionNode) {
    if (ownerTypeNode != ZR_NULL) {
        *ownerTypeNode = ZR_NULL;
    }
    if (metaFunctionNode != ZR_NULL) {
        *metaFunctionNode = ZR_NULL;
    }
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    if (super_navigation_find_super_constructor_context(node->data.script.statements->nodes[index],
                                                                        position,
                                                                        content,
                                                                        contentLength,
                                                                        uri,
                                                                        ownerTypeNode,
                                                                        metaFunctionNode)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    if (super_navigation_find_super_constructor_context(node->data.block.body->nodes[index],
                                                                        position,
                                                                        content,
                                                                        contentLength,
                                                                        uri,
                                                                        ownerTypeNode,
                                                                        metaFunctionNode)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        case ZR_AST_CLASS_DECLARATION:
            if (node->data.classDeclaration.members != ZR_NULL &&
                node->data.classDeclaration.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.classDeclaration.members->count; index++) {
                    SZrAstNode *memberNode = node->data.classDeclaration.members->nodes[index];
                    if (memberNode == ZR_NULL) {
                        continue;
                    }

                    if (memberNode->type == ZR_AST_CLASS_META_FUNCTION &&
                        super_navigation_super_call_matches_position(memberNode,
                                                                     position,
                                                                     content,
                                                                     contentLength,
                                                                     uri)) {
                        if (ownerTypeNode != ZR_NULL) {
                            *ownerTypeNode = node;
                        }
                        if (metaFunctionNode != ZR_NULL) {
                            *metaFunctionNode = memberNode;
                        }
                        return ZR_TRUE;
                    }

                    if (super_navigation_find_super_constructor_context(memberNode,
                                                                        position,
                                                                        content,
                                                                        contentLength,
                                                                        uri,
                                                                        ownerTypeNode,
                                                                        metaFunctionNode)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        default:
            break;
    }

    return ZR_FALSE;
}

static TZrBool super_navigation_find_constructor_declaration_context(SZrAstNode *node,
                                                                     SZrFileRange position,
                                                                     SZrAstNode **ownerTypeNode,
                                                                     SZrAstNode **metaFunctionNode) {
    if (ownerTypeNode != ZR_NULL) {
        *ownerTypeNode = ZR_NULL;
    }
    if (metaFunctionNode != ZR_NULL) {
        *metaFunctionNode = ZR_NULL;
    }
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    if (super_navigation_find_constructor_declaration_context(node->data.script.statements->nodes[index],
                                                                              position,
                                                                              ownerTypeNode,
                                                                              metaFunctionNode)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    if (super_navigation_find_constructor_declaration_context(node->data.block.body->nodes[index],
                                                                              position,
                                                                              ownerTypeNode,
                                                                              metaFunctionNode)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        case ZR_AST_CLASS_DECLARATION:
            if (node->data.classDeclaration.members != ZR_NULL &&
                node->data.classDeclaration.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.classDeclaration.members->count; index++) {
                    SZrAstNode *memberNode = node->data.classDeclaration.members->nodes[index];
                    if (memberNode == ZR_NULL) {
                        continue;
                    }

                    if (memberNode->type == ZR_AST_CLASS_META_FUNCTION &&
                        super_navigation_meta_function_is_constructor(memberNode) &&
                        super_navigation_file_range_contains_position(memberNode->location, position)) {
                        if (ownerTypeNode != ZR_NULL) {
                            *ownerTypeNode = node;
                        }
                        if (metaFunctionNode != ZR_NULL) {
                            *metaFunctionNode = memberNode;
                        }
                        return ZR_TRUE;
                    }

                    if (super_navigation_find_constructor_declaration_context(memberNode,
                                                                              position,
                                                                              ownerTypeNode,
                                                                              metaFunctionNode)) {
                        return ZR_TRUE;
                    }
                }
            }
            break;

        case ZR_AST_STRUCT_DECLARATION:
            if (node->data.structDeclaration.members != ZR_NULL &&
                node->data.structDeclaration.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.structDeclaration.members->count; index++) {
                    SZrAstNode *memberNode = node->data.structDeclaration.members->nodes[index];
                    if (memberNode == ZR_NULL) {
                        continue;
                    }

                    if (memberNode->type == ZR_AST_STRUCT_META_FUNCTION &&
                        super_navigation_meta_function_is_constructor(memberNode) &&
                        super_navigation_file_range_contains_position(memberNode->location, position)) {
                        if (ownerTypeNode != ZR_NULL) {
                            *ownerTypeNode = node;
                        }
                        if (metaFunctionNode != ZR_NULL) {
                            *metaFunctionNode = memberNode;
                        }
                        return ZR_TRUE;
                    }
                }
            }
            break;

        default:
            break;
    }

    return ZR_FALSE;
}

static SZrString *super_navigation_get_direct_base_declaration_name(SZrAstNode *ownerTypeNode) {
    SZrAstNode *inheritNode;

    if (ownerTypeNode == ZR_NULL ||
        ownerTypeNode->type != ZR_AST_CLASS_DECLARATION ||
        ownerTypeNode->data.classDeclaration.inherits == ZR_NULL ||
        ownerTypeNode->data.classDeclaration.inherits->count == 0) {
        return ZR_NULL;
    }

    inheritNode = ownerTypeNode->data.classDeclaration.inherits->nodes[0];
    if (inheritNode == ZR_NULL || inheritNode->type != ZR_AST_TYPE || inheritNode->data.type.name == ZR_NULL) {
        return ZR_NULL;
    }

    if (inheritNode->data.type.name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return inheritNode->data.type.name->data.identifier.name;
    }

    if (inheritNode->data.type.name->type == ZR_AST_GENERIC_TYPE &&
        inheritNode->data.type.name->data.genericType.name != ZR_NULL) {
        return inheritNode->data.type.name->data.genericType.name->name;
    }

    return ZR_NULL;
}

static SZrString *super_navigation_get_declared_type_name(SZrAstNode *typeDeclarationNode) {
    if (typeDeclarationNode == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeDeclarationNode->type == ZR_AST_CLASS_DECLARATION &&
        typeDeclarationNode->data.classDeclaration.name != ZR_NULL) {
        return typeDeclarationNode->data.classDeclaration.name->name;
    }

    if (typeDeclarationNode->type == ZR_AST_STRUCT_DECLARATION &&
        typeDeclarationNode->data.structDeclaration.name != ZR_NULL) {
        return typeDeclarationNode->data.structDeclaration.name->name;
    }

    return ZR_NULL;
}

static SZrAstNode *super_navigation_find_type_declaration_recursive(SZrAstNode *node, SZrString *typeName) {
    if (node == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    SZrAstNode *resolved =
                        super_navigation_find_type_declaration_recursive(node->data.script.statements->nodes[index],
                                                                         typeName);
                    if (resolved != ZR_NULL) {
                        return resolved;
                    }
                }
            }
            break;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    SZrAstNode *resolved =
                        super_navigation_find_type_declaration_recursive(node->data.block.body->nodes[index],
                                                                         typeName);
                    if (resolved != ZR_NULL) {
                        return resolved;
                    }
                }
            }
            break;

        case ZR_AST_CLASS_DECLARATION:
            if (node->data.classDeclaration.name != ZR_NULL &&
                node->data.classDeclaration.name->name != ZR_NULL &&
                ZrCore_String_Equal(node->data.classDeclaration.name->name, typeName)) {
                return node;
            }
            break;

        case ZR_AST_STRUCT_DECLARATION:
            if (node->data.structDeclaration.name != ZR_NULL &&
                node->data.structDeclaration.name->name != ZR_NULL &&
                ZrCore_String_Equal(node->data.structDeclaration.name->name, typeName)) {
                return node;
            }
            break;

        default:
            break;
    }

    return ZR_NULL;
}

static SZrAstNode *super_navigation_find_constructor_declaration_in_type(SZrAstNode *typeDeclarationNode) {
    SZrAstNodeArray *members = ZR_NULL;

    if (typeDeclarationNode == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeDeclarationNode->type == ZR_AST_CLASS_DECLARATION) {
        members = typeDeclarationNode->data.classDeclaration.members;
    } else if (typeDeclarationNode->type == ZR_AST_STRUCT_DECLARATION) {
        members = typeDeclarationNode->data.structDeclaration.members;
    }

    if (members == ZR_NULL || members->nodes == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < members->count; index++) {
        SZrAstNode *memberNode = members->nodes[index];
        if (memberNode != ZR_NULL && super_navigation_meta_function_is_constructor(memberNode)) {
            return memberNode;
        }
    }

    return ZR_NULL;
}

static TZrBool super_navigation_resolve_target(SZrSemanticAnalyzer *analyzer,
                                               SZrFileRange position,
                                               const TZrChar *content,
                                               TZrSize contentLength,
                                               SZrString *uri,
                                               SZrString **targetBaseTypeName,
                                               SZrAstNode **targetConstructorDeclaration) {
    SZrAstNode *ownerTypeNode = ZR_NULL;
    SZrAstNode *metaFunctionNode = ZR_NULL;
    SZrAstNode *baseTypeDeclaration;
    SZrString *baseTypeName;

    if (targetBaseTypeName != ZR_NULL) {
        *targetBaseTypeName = ZR_NULL;
    }
    if (targetConstructorDeclaration != ZR_NULL) {
        *targetConstructorDeclaration = ZR_NULL;
    }
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL || targetBaseTypeName == ZR_NULL ||
        targetConstructorDeclaration == ZR_NULL) {
        return ZR_FALSE;
    }

    if (super_navigation_find_super_constructor_context(analyzer->ast,
                                                        position,
                                                        content,
                                                        contentLength,
                                                        uri,
                                                        &ownerTypeNode,
                                                        &metaFunctionNode) &&
        ownerTypeNode != ZR_NULL && metaFunctionNode != ZR_NULL) {
        baseTypeName = super_navigation_get_direct_base_declaration_name(ownerTypeNode);
        if (baseTypeName == ZR_NULL) {
            return ZR_FALSE;
        }

        baseTypeDeclaration = super_navigation_find_type_declaration_recursive(analyzer->ast, baseTypeName);
        *targetConstructorDeclaration = super_navigation_find_constructor_declaration_in_type(baseTypeDeclaration);
        *targetBaseTypeName = baseTypeName;
        return *targetConstructorDeclaration != ZR_NULL;
    }

    if (super_navigation_find_constructor_declaration_context(analyzer->ast,
                                                              position,
                                                              &ownerTypeNode,
                                                              &metaFunctionNode) &&
        ownerTypeNode != ZR_NULL && metaFunctionNode != ZR_NULL) {
        if (metaFunctionNode->type == ZR_AST_CLASS_META_FUNCTION &&
            metaFunctionNode->data.classMetaFunction.hasSuperCall &&
            super_navigation_position_is_super_token(content, contentLength, position.start.offset)) {
            baseTypeName = super_navigation_get_direct_base_declaration_name(ownerTypeNode);
            if (baseTypeName == ZR_NULL) {
                return ZR_FALSE;
            }

            baseTypeDeclaration = super_navigation_find_type_declaration_recursive(analyzer->ast, baseTypeName);
            *targetConstructorDeclaration = super_navigation_find_constructor_declaration_in_type(baseTypeDeclaration);
            *targetBaseTypeName = baseTypeName;
            return *targetConstructorDeclaration != ZR_NULL;
        }

        *targetBaseTypeName = super_navigation_get_declared_type_name(ownerTypeNode);
        *targetConstructorDeclaration = metaFunctionNode;
        return *targetBaseTypeName != ZR_NULL;
    }

    return ZR_FALSE;
}

static void super_navigation_collect_reference_ranges_recursive(SZrState *state,
                                                                SZrAstNode *node,
                                                                SZrString *targetBaseTypeName,
                                                                const TZrChar *content,
                                                                TZrSize contentLength,
                                                                SZrString *uri,
                                                                SZrArray *ranges) {
    SZrString *directBaseTypeName = ZR_NULL;

    if (node == ZR_NULL || targetBaseTypeName == ZR_NULL || ranges == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    super_navigation_collect_reference_ranges_recursive(state,
                                                                       node->data.script.statements->nodes[index],
                                                                       targetBaseTypeName,
                                                                       content,
                                                                       contentLength,
                                                                       uri,
                                                                       ranges);
                }
            }
            break;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    super_navigation_collect_reference_ranges_recursive(state,
                                                                       node->data.block.body->nodes[index],
                                                                       targetBaseTypeName,
                                                                       content,
                                                                       contentLength,
                                                                       uri,
                                                                       ranges);
                }
            }
            break;

        case ZR_AST_CLASS_DECLARATION:
            directBaseTypeName = super_navigation_get_direct_base_declaration_name(node);
            if (directBaseTypeName != ZR_NULL &&
                ZrCore_String_Equal(directBaseTypeName, targetBaseTypeName) &&
                node->data.classDeclaration.members != ZR_NULL &&
                node->data.classDeclaration.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.classDeclaration.members->count; index++) {
                    SZrAstNode *memberNode = node->data.classDeclaration.members->nodes[index];
                    if (memberNode != ZR_NULL &&
                        memberNode->type == ZR_AST_CLASS_META_FUNCTION &&
                        super_navigation_meta_function_is_constructor(memberNode) &&
                        memberNode->data.classMetaFunction.hasSuperCall) {
                        SZrFileRange range =
                            super_navigation_super_call_token_range(memberNode, content, contentLength, uri);
                        ZrCore_Array_Push(state, ranges, &range);
                    }
                }
            }
            break;

        default:
            break;
    }
}

static TZrBool super_navigation_append_location(SZrState *state,
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

static TZrBool super_navigation_append_highlight(SZrState *state,
                                                 SZrArray *result,
                                                 SZrFileRange range,
                                                 TZrInt32 kind) {
    SZrLspDocumentHighlight *highlight;

    if (state == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    highlight = (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentHighlight));
    if (highlight == ZR_NULL) {
        return ZR_FALSE;
    }

    highlight->range = ZrLanguageServer_LspRange_FromFileRange(range);
    highlight->kind = kind;
    ZrCore_Array_Push(state, result, &highlight);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_TryGetSuperConstructorDefinition(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri,
                                                              SZrLspPosition position,
                                                              SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrFilePosition filePos;
    SZrFileRange fileRange;
    SZrString *baseTypeName = ZR_NULL;
    SZrAstNode *baseConstructorDeclaration = ZR_NULL;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL || fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }

    filePos = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    if (!super_navigation_resolve_target(analyzer,
                                         fileRange,
                                         fileVersion->content,
                                         fileVersion->contentLength,
                                         uri,
                                         &baseTypeName,
                                         &baseConstructorDeclaration) ||
        baseTypeName == ZR_NULL || baseConstructorDeclaration == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), 1);
    }

    return super_navigation_append_location(state, result, uri, baseConstructorDeclaration->location);
}

TZrBool ZrLanguageServer_Lsp_TryFindSuperConstructorReferences(SZrState *state,
                                                               SZrLspContext *context,
                                                               SZrString *uri,
                                                               SZrLspPosition position,
                                                               TZrBool includeDeclaration,
                                                               SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrFilePosition filePos;
    SZrFileRange fileRange;
    SZrString *baseTypeName = ZR_NULL;
    SZrAstNode *baseConstructorDeclaration = ZR_NULL;
    SZrArray ranges;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL || fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }

    filePos = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    if (!super_navigation_resolve_target(analyzer,
                                         fileRange,
                                         fileVersion->content,
                                         fileVersion->contentLength,
                                         uri,
                                         &baseTypeName,
                                         &baseConstructorDeclaration) ||
        baseTypeName == ZR_NULL || baseConstructorDeclaration == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    if (includeDeclaration) {
        super_navigation_append_location(state, result, uri, baseConstructorDeclaration->location);
    }

    ZrCore_Array_Init(state, &ranges, sizeof(SZrFileRange), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    super_navigation_collect_reference_ranges_recursive(state,
                                                       analyzer->ast,
                                                       baseTypeName,
                                                       fileVersion->content,
                                                       fileVersion->contentLength,
                                                       uri,
                                                       &ranges);
    for (TZrSize index = 0; index < ranges.length; index++) {
        SZrFileRange *range = (SZrFileRange *)ZrCore_Array_Get(&ranges, index);
        if (range != ZR_NULL) {
            super_navigation_append_location(state, result, uri, *range);
        }
    }
    ZrCore_Array_Free(state, &ranges);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_TryGetSuperConstructorDocumentHighlights(SZrState *state,
                                                                      SZrLspContext *context,
                                                                      SZrString *uri,
                                                                      SZrLspPosition position,
                                                                      SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrFilePosition filePos;
    SZrFileRange fileRange;
    SZrString *baseTypeName = ZR_NULL;
    SZrAstNode *baseConstructorDeclaration = ZR_NULL;
    SZrArray ranges;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL || fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }

    filePos = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    if (!super_navigation_resolve_target(analyzer,
                                         fileRange,
                                         fileVersion->content,
                                         fileVersion->contentLength,
                                         uri,
                                         &baseTypeName,
                                         &baseConstructorDeclaration) ||
        baseTypeName == ZR_NULL || baseConstructorDeclaration == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentHighlight *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    if (baseConstructorDeclaration->location.source == ZR_NULL ||
        ZrLanguageServer_Lsp_StringsEqual(baseConstructorDeclaration->location.source, uri)) {
        super_navigation_append_highlight(state, result, baseConstructorDeclaration->location, 3);
    }

    ZrCore_Array_Init(state, &ranges, sizeof(SZrFileRange), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    super_navigation_collect_reference_ranges_recursive(state,
                                                       analyzer->ast,
                                                       baseTypeName,
                                                       fileVersion->content,
                                                       fileVersion->contentLength,
                                                       uri,
                                                       &ranges);
    for (TZrSize index = 0; index < ranges.length; index++) {
        SZrFileRange *range = (SZrFileRange *)ZrCore_Array_Get(&ranges, index);
        if (range != ZR_NULL &&
            (range->source == ZR_NULL || ZrLanguageServer_Lsp_StringsEqual(range->source, uri))) {
            super_navigation_append_highlight(state, result, *range, 2);
        }
    }
    ZrCore_Array_Free(state, &ranges);
    return ZR_TRUE;
}
