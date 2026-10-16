#include "interface/lsp_interface_internal.h"

#include <ctype.h>
#include <string.h>

/* AST 来源和当前请求 URI 可能分别缺少 source；位置比较优先采用 parser 字节偏移。 */
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

/* 只把 class/struct 的 @constructor 当作导航目标，普通元函数不参与 super 关系。 */
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

/* 原文扫描中的标识符边界门控；是否处于可执行代码由 code-span helper 再判断。 */
static TZrBool super_navigation_is_identifier_char(TZrChar value) {
    return isalnum((unsigned char)value) || value == '_';
}

/* raw token 的字节位置需还原为 parser 的一基行列，随后才能走统一的 UTF-16 输出转换。 */
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

/* 光标来自文档位置转换；只有无有效 offset 时才按当前快照文本重建扫描起点。 */
static TZrSize super_navigation_offset_from_file_position(const TZrChar *content,
                                                          TZrSize contentLength,
                                                          SZrFilePosition targetPosition) {
    TZrSize offset = 0;
    TZrInt32 line = 1;
    TZrInt32 column = 1;

    if (content == ZR_NULL || contentLength == 0) {
        return 0;
    }
    if (targetPosition.offset > 0 && targetPosition.offset < contentLength) {
        return targetPosition.offset;
    }

    while (offset < contentLength) {
        if (line == targetPosition.line && column == targetPosition.column) {
            return offset;
        }
        if (content[offset] == '\n') {
            line++;
            column = 1;
        } else if (content[offset] != '\r') {
            column++;
        }
        offset++;
    }

    return contentLength - 1;
}

/* 给定义、引用和高亮共用的 super 命中范围；仅把代码 span 中的完整 token 发布为位置。 */
/* TODO: parser 已保存 classMetaFunction.superCallRange；核对能否直接采用该事实，避免在构造器头部重新猜测 token。 */
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
        if (!ZrLanguageServer_Lsp_IsOffsetInCodeSpan(content, contentLength, index)) {
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

/* 声明上下文覆盖整个构造器，故只有光标确在代码 token super 上才应改为基类目标。 */
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

    if (!ZrLanguageServer_Lsp_IsOffsetInCodeSpan(content, contentLength, start)) {
        return ZR_FALSE;
    }

    return end - start == 5 && memcmp(content + start, "super", 5) == 0;
}

/* token 扫描失败时仍给调用者一个构造器头部回退范围，供 super 引用展示路径使用。 */
/* TODO: 回退范围从首个实参向前推六字节；空格或换行会改变其含义，需与 parser.superCallRange 对照。 */
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

/* 在 constructor body 之前定位显式 super 调用，避免正文内的同名文本充当引用锚点。 */
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

/* 仅对声明过 super(...) 的构造器尝试特殊导航，其他位置应让通用语义查询处理。 */
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

    /* BUG: 光标位于 super(seed) 的 seed 实参时，此分支也拦截定义、引用和高亮请求，
     * 并把 seed 误投影到基类构造器；GetDefinition 的优先分派在 interface/lsp_interface.c 中可达。 */
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

/* 从当前 AST 找到显式 super 调用所属类型；只沿脚本、块和类成员的导航作用域下降。 */
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

/* 从构造器声明自身发起 references/highlights 时，用所属类型反查显式 super 调用。 */
/* BUG: memberNode->location 覆盖整个方法体；正文任意代码 token 也会被当作构造器声明上下文。 */
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

/* 显式 super 只指向第一个直接继承项；结果借用 AST 名称，由当前 analyzer 持有。 */
/* TODO: 这里使用原始类型名；核查别名、同名作用域与跨文件基类是否需要 canonical type identity。 */
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

/* 声明端的 references/highlights 需要同一 AST 内可比较的类型名；不创建新字符串。 */
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

/* 当前导航限定于同一语法树中的命名类型；返回的节点仍属于 analyzer，不由调用方释放。 */
/* TODO: 同名类型取遍历遇到的首项，需以局部作用域和导入类型用例核查是否会选错声明。 */
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

/* 基类必须有显式 @constructor 才能返回具体定义；类型名命中并不保证存在目标。 */
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

/* 将光标命中的 super 调用或构造器声明转为同文件目标，供三种导航入口复用。 */
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
    TZrSize positionOffset;

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

    positionOffset = super_navigation_offset_from_file_position(content, contentLength, position.start);

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
            super_navigation_position_is_super_token(content, contentLength, positionOffset)) {
            baseTypeName = super_navigation_get_direct_base_declaration_name(ownerTypeNode);
            if (baseTypeName == ZR_NULL) {
                return ZR_FALSE;
            }

            baseTypeDeclaration = super_navigation_find_type_declaration_recursive(analyzer->ast, baseTypeName);
            *targetConstructorDeclaration = super_navigation_find_constructor_declaration_in_type(baseTypeDeclaration);
            *targetBaseTypeName = baseTypeName;
            return *targetConstructorDeclaration != ZR_NULL;
        }

        if (!ZrLanguageServer_Lsp_IsOffsetInCodeSpan(content, contentLength, positionOffset)) {
            return ZR_FALSE;
        }

        /* BUG: 声明上下文已把整个方法体纳入，正文变量等代码 token 会使导航提前返回
         * 当前构造器，覆盖 interface/lsp_interface.c 后续的普通符号定义、引用和高亮。 */
        *targetBaseTypeName = super_navigation_get_declared_type_name(ownerTypeNode);
        *targetConstructorDeclaration = metaFunctionNode;
        return *targetBaseTypeName != ZR_NULL;
    }

    return ZR_FALSE;
}

/* 以基类名称收集当前语法树中派生构造器的显式 super 调用，结果是暂存的 FileRange 值。 */
/* TODO: 原始名称相等未验证继承边的语义身份；同名基类或别名可能合并不相关引用。 */
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

/* 把借用的 AST 范围投影为调用方负责释放的 Location；位置统一按文档内容转换。 */
static TZrBool super_navigation_append_location(SZrState *state,
                                                SZrLspContext *context,
                                                SZrArray *result,
                                                SZrString *uri,
                                                SZrFileRange range) {
    SZrLspLocation *location;

    if (state == ZR_NULL || context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }

    location->uri = range.source != ZR_NULL ? range.source : uri;
    location->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(context, uri, range);
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

/* 调用方先筛选当前 URI 的范围；此处只负责位置投影，kind 由声明与读取引用区分。 */
static TZrBool super_navigation_append_highlight(SZrState *state,
                                                 SZrLspContext *context,
                                                 SZrArray *result,
                                                 SZrString *uri,
                                                 SZrFileRange range,
                                                 TZrInt32 kind) {
    SZrLspDocumentHighlight *highlight;

    if (state == ZR_NULL || context == ZR_NULL || result == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    highlight = (SZrLspDocumentHighlight *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspDocumentHighlight));
    if (highlight == ZR_NULL) {
        return ZR_FALSE;
    }

    highlight->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(context, uri, range);
    highlight->kind = kind;
    ZrCore_Array_Push(state, result, &highlight);
    return ZR_TRUE;
}

/* GetDefinition 在通用语义查询前调用本入口，仅显式 super 与构造器目标应被此处截获。 */
TZrBool ZrLanguageServer_Lsp_TryGetSuperConstructorDefinition(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri,
                                                              SZrLspPosition position,
                                                              SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
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
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    if (!super_navigation_resolve_target(analyzer,
                                         fileRange,
                                         snapshot.content,
                                         snapshot.contentLength,
                                         uri,
                                         &baseTypeName,
                                         &baseConstructorDeclaration) ||
        baseTypeName == ZR_NULL || baseConstructorDeclaration == ZR_NULL) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), 1);
    }

    if (!super_navigation_append_location(state, context, result, uri, baseConstructorDeclaration->location)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return ZR_TRUE;
}

/* FindReferences 的优先路径：可选声明项，加上同一 AST 内的显式 super 调用位置。 */
/* TODO: 目前返回 true 代表目标已解析，范围收集仅查当前文档；核查跨文件派生类的引用需求。 */
TZrBool ZrLanguageServer_Lsp_TryFindSuperConstructorReferences(SZrState *state,
                                                               SZrLspContext *context,
                                                               SZrString *uri,
                                                               SZrLspPosition position,
                                                               TZrBool includeDeclaration,
                                                               SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
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
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    if (!super_navigation_resolve_target(analyzer,
                                         fileRange,
                                         snapshot.content,
                                         snapshot.contentLength,
                                         uri,
                                         &baseTypeName,
                                         &baseConstructorDeclaration) ||
        baseTypeName == ZR_NULL || baseConstructorDeclaration == ZR_NULL) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    /* BUG: append_location 分配失败会返回 false，但声明项和下面循环均忽略该结果；
     * 此入口最后仍返回 true，客户端可能收到不完整的引用集。 */
    if (includeDeclaration) {
        super_navigation_append_location(state, context, result, uri, baseConstructorDeclaration->location);
    }

    ZrCore_Array_Init(state, &ranges, sizeof(SZrFileRange), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    super_navigation_collect_reference_ranges_recursive(state,
                                                       analyzer->ast,
                                                       baseTypeName,
                                                       snapshot.content,
                                                       snapshot.contentLength,
                                                       uri,
                                                       &ranges);
    for (TZrSize index = 0; index < ranges.length; index++) {
        SZrFileRange *range = (SZrFileRange *)ZrCore_Array_Get(&ranges, index);
        if (range != ZR_NULL) {
            super_navigation_append_location(state, context, result, uri, *range);
        }
    }
    ZrCore_Array_Free(state, &ranges);
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return ZR_TRUE;
}

/* GetDocumentHighlights 复用同一目标解析，但只发布当前文档中的声明与调用范围。 */
TZrBool ZrLanguageServer_Lsp_TryGetSuperConstructorDocumentHighlights(SZrState *state,
                                                                      SZrLspContext *context,
                                                                      SZrString *uri,
                                                                      SZrLspPosition position,
                                                                      SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
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
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot)) {
        return ZR_FALSE;
    }
    fileRange = ZrParser_FileRange_Create(filePos, filePos, uri);
    if (!super_navigation_resolve_target(analyzer,
                                         fileRange,
                                         snapshot.content,
                                         snapshot.contentLength,
                                         uri,
                                         &baseTypeName,
                                         &baseConstructorDeclaration) ||
        baseTypeName == ZR_NULL || baseConstructorDeclaration == ZR_NULL) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspDocumentHighlight *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    /* BUG: append_highlight 分配失败会返回 false，声明项与引用项都忽略失败；
     * 此入口仍返回 true，客户端可能收到不完整的高亮集。 */
    if (baseConstructorDeclaration->location.source == ZR_NULL ||
        ZrLanguageServer_Lsp_StringsEqual(baseConstructorDeclaration->location.source, uri)) {
        super_navigation_append_highlight(state, context, result, uri, baseConstructorDeclaration->location, 3);
    }

    ZrCore_Array_Init(state, &ranges, sizeof(SZrFileRange), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    super_navigation_collect_reference_ranges_recursive(state,
                                                       analyzer->ast,
                                                       baseTypeName,
                                                       snapshot.content,
                                                       snapshot.contentLength,
                                                       uri,
                                                       &ranges);
    for (TZrSize index = 0; index < ranges.length; index++) {
        SZrFileRange *range = (SZrFileRange *)ZrCore_Array_Get(&ranges, index);
        if (range != ZR_NULL &&
            (range->source == ZR_NULL || ZrLanguageServer_Lsp_StringsEqual(range->source, uri))) {
            super_navigation_append_highlight(state, context, result, uri, *range, 2);
        }
    }
    ZrCore_Array_Free(state, &ranges);
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
    return ZR_TRUE;
}
