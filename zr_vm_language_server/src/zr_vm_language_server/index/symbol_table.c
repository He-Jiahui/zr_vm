//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_language_server/symbol_table.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/global.h"
#include "zr_vm_parser/type_system.h"

#include <stdio.h>
#include <string.h>

/* 位置比较只服务名称位置和作用域优先级，不能替代 parser 的 canonical SymbolAt 身份。 */
#define ZR_LSP_SYMBOL_POSITION_COMPARE_LESS (-1)
#define ZR_LSP_SYMBOL_POSITION_COMPARE_EQUAL 0
#define ZR_LSP_SYMBOL_POSITION_COMPARE_GREATER 1

/* 声明与请求可能持有不同的字符串对象；按完整字节比较源身份，避免跨文件同名误配。 */
static TZrBool source_uri_equals(SZrString *left, SZrString *right) {
    TZrNativeString leftText;
    TZrNativeString rightText;
    TZrSize leftLength;
    TZrSize rightLength;

    if (left == right) {
        return ZR_TRUE;
    }

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
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

/* 导航按声明或作用域范围筛选候选；零 offset 时仍需兼容 parser 的行列位置。 */
static TZrBool is_position_in_range(SZrFileRange position, SZrFileRange symbolRange) {
    /* TODO: 两侧任一 source 缺失会跳过文件身份校验；需核对虚拟声明和跨文件导航
     * 是否总在入表前补齐 source，并加入同名跨文件回归。 */
    // 首先检查源文件是否相同
    if (!source_uri_equals(position.source, symbolRange.source) &&
        position.source != ZR_NULL && symbolRange.source != ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 使用offset比较（如果可用）
    if (symbolRange.start.offset > 0 && symbolRange.end.offset > 0 &&
        position.start.offset > 0 && position.end.offset > 0) {
        return (symbolRange.start.offset <= position.start.offset &&
                position.end.offset <= symbolRange.end.offset);
    }
    
    // 使用行号和列号比较
    TZrBool startMatch = (symbolRange.start.line < position.start.line) ||
                      (symbolRange.start.line == position.start.line && 
                       symbolRange.start.column <= position.start.column);
    TZrBool endMatch = (position.end.line < symbolRange.end.line) ||
                    (position.end.line == symbolRange.end.line && 
                     position.end.column <= symbolRange.end.column);
    
    return startMatch && endMatch;
}

/* 名称位置与作用域排序共用同一比较规则，混合 offset/行列输入时退回行列。 */
static int compare_file_position(SZrFilePosition left, SZrFilePosition right) {
    if (left.offset > 0 && right.offset > 0) {
        if (left.offset < right.offset) {
            return ZR_LSP_SYMBOL_POSITION_COMPARE_LESS;
        }
        if (left.offset > right.offset) {
            return ZR_LSP_SYMBOL_POSITION_COMPARE_GREATER;
        }
        return ZR_LSP_SYMBOL_POSITION_COMPARE_EQUAL;
    }

    if (left.line < right.line) {
        return ZR_LSP_SYMBOL_POSITION_COMPARE_LESS;
    }
    if (left.line > right.line) {
        return ZR_LSP_SYMBOL_POSITION_COMPARE_GREATER;
    }
    if (left.column < right.column) {
        return ZR_LSP_SYMBOL_POSITION_COMPARE_LESS;
    }
    if (left.column > right.column) {
        return ZR_LSP_SYMBOL_POSITION_COMPARE_GREATER;
    }
    return ZR_LSP_SYMBOL_POSITION_COMPARE_EQUAL;
}

/* 优先用名称选区匹配光标；无名称范围的投影才退回完整声明范围。 */
static SZrFileRange get_symbol_match_range(SZrSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return ZrParser_FileRange_Create(
            ZrParser_FilePosition_Create(0, 0, 0),
            ZrParser_FilePosition_Create(0, 0, 0),
            ZR_NULL
        );
    }

    if (symbol->selectionRange.start.line > 0 ||
        symbol->selectionRange.start.column > 0 ||
        symbol->selectionRange.start.offset > 0 ||
        symbol->selectionRange.end.line > 0 ||
        symbol->selectionRange.end.column > 0 ||
        symbol->selectionRange.end.offset > 0) {
        return symbol->selectionRange;
    }

    return symbol->location;
}

static TZrBool scope_contains_position(SZrSymbolScope *scope, SZrFileRange position);

/* 供按位置查找挑选当前作用域内、已经出现的声明，限制前向同名误配。 */
static TZrBool symbol_matches_lookup_position(SZrSymbol *symbol, SZrFileRange position) {
    SZrFileRange symbolRange;

    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    symbolRange = get_symbol_match_range(symbol);
    /* TODO: 与 is_position_in_range 相同，任一 source 缺失就跳过文件身份校验；
     * 需核对 LookupAtPosition 的跨文件同名回退，并补虚拟声明缺源用例。 */
    if (!source_uri_equals(position.source, symbolRange.source) &&
        position.source != ZR_NULL && symbolRange.source != ZR_NULL) {
        return ZR_FALSE;
    }

    if (symbol->scope != ZR_NULL && !scope_contains_position(symbol->scope, position)) {
        return ZR_FALSE;
    }

    return compare_file_position(symbolRange.start, position.start) <= 0;
}

/* 多个候选匹配同一光标时，以更靠近光标的声明位置作展示层择优。 */
static TZrBool symbol_is_better_lookup_candidate(SZrSymbol *candidate,
                                                 SZrSymbol *best) {
    SZrFileRange candidateRange;
    SZrFileRange bestRange;
    int comparison;

    if (candidate == ZR_NULL) {
        return ZR_FALSE;
    }
    if (best == ZR_NULL) {
        return ZR_TRUE;
    }

    candidateRange = get_symbol_match_range(candidate);
    bestRange = get_symbol_match_range(best);
    comparison = compare_file_position(candidateRange.start, bestRange.start);
    if (comparison != 0) {
        return comparison > 0;
    }

    comparison = compare_file_position(candidateRange.end, bestRange.end);
    if (comparison != 0) {
        return comparison >= 0;
    }

    return candidate != best;
}

/* 全局作用域始终可见；其余作用域只在源位置落入词法范围时参与查找。 */
static TZrBool scope_contains_position(SZrSymbolScope *scope, SZrFileRange position) {
    if (scope == ZR_NULL) {
        return ZR_FALSE;
    }

    if (scope->parent == ZR_NULL) {
        return ZR_TRUE;
    }

    return is_position_in_range(position, scope->range);
}

/* 按范围包围关系近似选择更内层声明，供旧的名称位置查找回退使用。 */
static TZrBool scope_is_deeper_candidate(SZrSymbolScope *candidate,
                                         SZrSymbolScope *best) {
    int comparison;

    if (candidate == ZR_NULL) {
        return ZR_FALSE;
    }
    if (best == ZR_NULL) {
        return ZR_TRUE;
    }
    if (best->parent == ZR_NULL) {
        return candidate->parent != ZR_NULL;
    }
    if (candidate->parent == ZR_NULL) {
        return ZR_FALSE;
    }

    comparison = compare_file_position(candidate->range.start, best->range.start);
    if (comparison != 0) {
        return comparison >= 0;
    }

    comparison = compare_file_position(candidate->range.end, best->range.end);
    if (comparison != 0) {
        return comparison <= 0;
    }

    return candidate != best;
}

/* FFI 装饰器投影只接受字面标识符，避免把可求值表达式误作静态元数据。 */
static const TZrChar *symbol_table_identifier_node_text(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL || node->data.identifier.name == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(node->data.identifier.name);
}

/* 仅点成员可作为 @zr.ffi 的静态路径；计算属性无法在符号创建期求值。 */
static const TZrChar *symbol_table_member_property_text(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_MEMBER_EXPRESSION || node->data.memberExpression.computed) {
        return ZR_NULL;
    }

    return symbol_table_identifier_node_text(node->data.memberExpression.property);
}

/* 装饰器路径名与已知 FFI 标签比较；这里只消费 parser 的静态文本。 */
static TZrBool symbol_table_text_equals(const TZrChar *value, const TZrChar *expected) {
    return value != ZR_NULL && expected != ZR_NULL && strcmp(value, expected) == 0;
}

/* 各声明 AST 的装饰器字段不同；符号创建期统一取出以生成 hover 补充信息。 */
static SZrAstNodeArray *symbol_table_get_decorator_array_for_node(SZrAstNode *node) {
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

/* 只识别 @zr.ffi.<tag>(...) 的 AST 形状，不触发装饰器求值或运行时代码。 */
static TZrBool symbol_table_extract_ffi_decorator(SZrAstNode *decoratorNode,
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
    if (!symbol_table_text_equals(symbol_table_identifier_node_text(primary->property), "zr") ||
        primary->members == ZR_NULL || primary->members->count < 2 || primary->members->count > 3) {
        return ZR_FALSE;
    }

    ffiMember = primary->members->nodes[0];
    leafMember = primary->members->nodes[1];
    if (!symbol_table_text_equals(symbol_table_member_property_text(ffiMember), "ffi")) {
        return ZR_FALSE;
    }

    if (outLeafName != ZR_NULL) {
        *outLeafName = symbol_table_member_property_text(leafMember);
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

/* pack/align/offset/value 展示仅取单个整数字面量，动态参数留给正式语义层。 */
static TZrBool symbol_table_call_read_single_integer_arg(SZrFunctionCall *call, TZrInt64 *outValue) {
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

/* enum underlying hover 只读取单个字符串字面量；返回 AST/GC 字符串的借用文本。 */
static TZrBool symbol_table_call_read_single_string_arg(SZrFunctionCall *call, const TZrChar **outValue) {
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
        *outValue = ZrCore_String_GetNativeString(arg->data.stringLiteral.value);
    }
    return ZR_TRUE;
}

/* 声明符号创建时缓存可静态识别的 FFI 布局提示，供 hover 直接展示；
 * 它是展示投影，不参与 parser 的 canonical decorator 校验或 ABI 计算。 */
static SZrString *build_symbol_ffi_hover_metadata_string(SZrState *state, SZrAstNode *astNode) {
    SZrAstNodeArray *decorators;
    TZrChar metadataBuffer[ZR_LSP_COMMENT_BUFFER_LENGTH];
    TZrSize used = 0;

    if (state == ZR_NULL || astNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (astNode->type) {
        case ZR_AST_STRUCT_DECLARATION:
        case ZR_AST_STRUCT_FIELD:
        case ZR_AST_ENUM_DECLARATION:
        case ZR_AST_ENUM_MEMBER:
            break;
        default:
            return ZR_NULL;
    }

    decorators = symbol_table_get_decorator_array_for_node(astNode);
    if (decorators == ZR_NULL || decorators->nodes == ZR_NULL) {
        return ZR_NULL;
    }

    metadataBuffer[0] = '\0';
    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        TZrInt64 integerValue = 0;
        const TZrChar *stringValue = ZR_NULL;
        TZrChar metadataLine[96];

        if (!symbol_table_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) ||
            leafName == ZR_NULL || !hasCall) {
            continue;
        }

        metadataLine[0] = '\0';
        switch (astNode->type) {
            case ZR_AST_STRUCT_DECLARATION:
                if (symbol_table_text_equals(leafName, "pack") &&
                    symbol_table_call_read_single_integer_arg(call, &integerValue)) {
                    snprintf(metadataLine, sizeof(metadataLine), "\nPack: %lld", (long long)integerValue);
                } else if (symbol_table_text_equals(leafName, "align") &&
                           symbol_table_call_read_single_integer_arg(call, &integerValue)) {
                    snprintf(metadataLine, sizeof(metadataLine), "\nAlign: %lld", (long long)integerValue);
                }
                break;

            case ZR_AST_STRUCT_FIELD:
                if (symbol_table_text_equals(leafName, "offset") &&
                    symbol_table_call_read_single_integer_arg(call, &integerValue)) {
                    snprintf(metadataLine, sizeof(metadataLine), "\nOffset: %lld", (long long)integerValue);
                }
                break;

            case ZR_AST_ENUM_DECLARATION:
                if (symbol_table_text_equals(leafName, "underlying") &&
                    symbol_table_call_read_single_string_arg(call, &stringValue) &&
                    stringValue != ZR_NULL) {
                    snprintf(metadataLine, sizeof(metadataLine), "\nUnderlying: %s", stringValue);
                }
                break;

            case ZR_AST_ENUM_MEMBER:
                if (symbol_table_text_equals(leafName, "value") &&
                    symbol_table_call_read_single_integer_arg(call, &integerValue)) {
                    snprintf(metadataLine, sizeof(metadataLine), "\nValue: %lld", (long long)integerValue);
                }
                break;

            default:
                break;
        }

        /* TODO: 单行与总缓冲区都会静默截断长 underlying 文本或多条提示；
         * 需结合 decorator 校验和 hover 用例确认是否可达，再决定拒绝还是显式标注截断。 */
        if (metadataLine[0] != '\0') {
            TZrSize metadataLength = strlen(metadataLine);
            TZrSize available = sizeof(metadataBuffer) - 1 - used;
            TZrSize writeLength = metadataLength < available ? metadataLength : available;

            if (writeLength == 0) {
                break;
            }
            memcpy(metadataBuffer + used, metadataLine, writeLength);
            used += writeLength;
            metadataBuffer[used] = '\0';
        }
    }

    return used > 0 ? ZrCore_String_Create(state, metadataBuffer, used) : ZR_NULL;
}

/* 为导航与文档符号优先提供名称范围；无专门 nameLocation 的声明保留调用方范围。 */
static SZrFileRange get_symbol_selection_range_from_ast(SZrAstNode *astNode, SZrFileRange fallback) {
    if (astNode == ZR_NULL) {
        return fallback;
    }

    switch (astNode->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            if (astNode->data.variableDeclaration.pattern != ZR_NULL &&
                astNode->data.variableDeclaration.pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
                return astNode->data.variableDeclaration.pattern->location;
            }
            break;

        case ZR_AST_FUNCTION_DECLARATION:
            return astNode->data.functionDeclaration.nameLocation;

        case ZR_AST_CLASS_DECLARATION:
            return astNode->data.classDeclaration.nameLocation;

        case ZR_AST_CLASS_FIELD:
            return astNode->data.classField.nameLocation;

        case ZR_AST_CLASS_METHOD:
            return astNode->data.classMethod.nameLocation;

        case ZR_AST_CLASS_PROPERTY:
            if (astNode->data.classProperty.modifier != ZR_NULL) {
                return get_symbol_selection_range_from_ast(astNode->data.classProperty.modifier, fallback);
            }
            break;

        case ZR_AST_PARAMETER:
            if (astNode->data.parameter.name != ZR_NULL) {
                SZrFileRange parameterRange = astNode->data.parameter.nameLocation;
                if (parameterRange.start.line != 0 || parameterRange.start.column != 0 ||
                    parameterRange.end.line != 0 || parameterRange.end.column != 0) {
                    return parameterRange;
                }
            }
            break;

        case ZR_AST_PROPERTY_GET:
            return astNode->data.propertyGet.nameLocation;

        case ZR_AST_PROPERTY_SET:
            return astNode->data.propertySet.nameLocation;

        default:
            break;
    }

    return fallback;
}

/* 分析器每次重建投影时新建表；作用域实体和双名称索引只在该表的生命周期内有效。 */
SZrSymbolTable *ZrLanguageServer_SymbolTable_New(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrSymbolTable *table = (SZrSymbolTable *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrSymbolTable));
    if (table == ZR_NULL) {
        return ZR_NULL;
    }
    
    table->state = state;
    table->globalScope = ZR_NULL;
    ZrCore_Array_Init(state, &table->scopeStack, sizeof(SZrSymbolScope *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    ZrCore_Array_Init(state, &table->allScopes, sizeof(SZrSymbolScope *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    
    // 创建映射对象（使用 SZrObject 的 nodeMap 存储）
    table->nameToSymbolsMap = ZrCore_Object_New(state, ZR_NULL);
    if (table->nameToSymbolsMap != ZR_NULL) {
        // 初始化 Object 的 nodeMap（ZrCore_Object_New 只调用了 ZrCore_HashSet_Construct，需要调用 ZrCore_HashSet_Init）
        // 直接调用 ZrCore_HashSet_Init 而不是 ZrCore_Object_Init，避免调用构造函数查找
        ZrCore_HashSet_Init(state, &table->nameToSymbolsMap->nodeMap, ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2);
    }
    table->useHashTable = ZR_FALSE; // 使用 Object 映射
    
    // 同时初始化哈希表作为备选
    ZrCore_HashSet_Construct(&table->nameToSymbolsHashSet);
    ZrCore_HashSet_Init(state, &table->nameToSymbolsHashSet, ZR_LSP_HASH_TABLE_INITIAL_SIZE_LOG2);
    
    // 创建全局作用域
    table->globalScope = (SZrSymbolScope *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrSymbolScope));
    /* BUG: 若此处分配失败，已分配的 scopeStack、allScopes 和哈希桶没有析构；
     * New 返回 NULL 后分析器无表可释放，低内存请求会泄漏这些原生资源。 */
    if (table->globalScope == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, table, sizeof(SZrSymbolTable));
        return ZR_NULL;
    }
    
    ZrCore_Array_Init(state,
                      &table->globalScope->symbols,
                      sizeof(SZrSymbol *),
                      ZR_LSP_GLOBAL_SCOPE_SYMBOL_INITIAL_CAPACITY);
    table->globalScope->parent = ZR_NULL;
    table->globalScope->isFunctionScope = ZR_FALSE;
    table->globalScope->isClassScope = ZR_FALSE;
    table->globalScope->isStructScope = ZR_FALSE;
    table->globalScope->range = ZrParser_FileRange_Create(
        ZrParser_FilePosition_Create(0, 0, 0),
        ZrParser_FilePosition_Create(0, 0, 0),
        ZR_NULL
    );
    
    // 将全局作用域压入栈
    ZrCore_Array_Push(state, &table->scopeStack, &table->globalScope);
    ZrCore_Array_Push(state, &table->allScopes, &table->globalScope);
    
    return table;
}

/* 分析器销毁或清缓存时回收全部作用域和符号，再卸载两套仅持有借用符号的索引。 */
void ZrLanguageServer_SymbolTable_Free(SZrState *state, SZrSymbolTable *table) {
    if (state == ZR_NULL || table == ZR_NULL) {
        return;
    }
    
    for (TZrSize scopeIndex = 0; scopeIndex < table->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrCore_Array_Get(&table->allScopes, scopeIndex);
        if (scopePtr != ZR_NULL && *scopePtr != ZR_NULL) {
            SZrSymbolScope *scope = *scopePtr;
            for (TZrSize symbolIndex = 0; symbolIndex < scope->symbols.length; symbolIndex++) {
                SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&scope->symbols, symbolIndex);
                if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                    ZrLanguageServer_Symbol_Free(state, *symbolPtr);
                }
            }
            ZrCore_Array_Free(state, &scope->symbols);
            ZrCore_Memory_RawFree(state->global, scope, sizeof(SZrSymbolScope));
        }
    }

    table->globalScope = ZR_NULL;
    ZrCore_Array_Free(state, &table->scopeStack);
    ZrCore_Array_Free(state, &table->allScopes);
    
    // 释放映射对象（GC 会自动处理，但我们需要清理内部的数组引用）
    if (table->nameToSymbolsMap != ZR_NULL) {
        // 清理 Object 内部的 nodeMap，释放所有存储的数组和节点
        SZrHashSet *nodeMap = &table->nameToSymbolsMap->nodeMap;
        if (nodeMap->isValid && nodeMap->buckets != ZR_NULL && nodeMap->capacity > 0) {
            // 遍历 nodeMap 中的所有键值对，释放存储的数组和节点
            for (TZrSize i = 0; i < nodeMap->capacity; i++) {
                SZrHashKeyValuePair *pair = nodeMap->buckets[i];
                while (pair != ZR_NULL) {
                    // 释放节点中存储的数组
                    if (pair->key.type != ZR_VALUE_TYPE_NULL) {
                        if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                            SZrArray *symbolArray = 
                                (SZrArray *)pair->value.value.nativeObject.nativePointer;
                            if (symbolArray != ZR_NULL && symbolArray->isValid) {
                                ZrCore_Array_Free(state, symbolArray);
                                ZrCore_Memory_RawFree(state->global, symbolArray, sizeof(SZrArray));
                            }
                        }
                    }
                    SZrHashKeyValuePair *next = pair->next;
                    /* HashSet pairs are owned by the pair pool released by Deconstruct. */
                    pair = next;
                }
            }
            // 释放 buckets 数组
            ZrCore_HashSet_Deconstruct(state, nodeMap);
        }
        // Object 会被 GC 管理，这里不需要手动释放
        // 但需要清理引用
        table->nameToSymbolsMap = ZR_NULL;
    }
    
    // 释放哈希表中的数组和节点
    if (table->nameToSymbolsHashSet.isValid && table->nameToSymbolsHashSet.buckets != ZR_NULL && 
        table->nameToSymbolsHashSet.capacity > 0) {
        for (TZrSize i = 0; i < table->nameToSymbolsHashSet.capacity; i++) {
            SZrHashKeyValuePair *pair = table->nameToSymbolsHashSet.buckets[i];
            while (pair != ZR_NULL) {
                // 释放节点中存储的数据
                if (pair->key.type != ZR_VALUE_TYPE_NULL) {
                    if (pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                        SZrArray *symbolArray = 
                            (SZrArray *)pair->value.value.nativeObject.nativePointer;
                        if (symbolArray != ZR_NULL && symbolArray->isValid) {
                            ZrCore_Array_Free(state, symbolArray);
                            ZrCore_Memory_RawFree(state->global, symbolArray, sizeof(SZrArray));
                        }
                    }
                }
                SZrHashKeyValuePair *next = pair->next;
                /* HashSet pairs are owned by the pair pool released by Deconstruct. */
                pair = next;
            }
        }
        // 释放 buckets 数组
        ZrCore_HashSet_Deconstruct(state, &table->nameToSymbolsHashSet);
    }
    
    ZrCore_Memory_RawFree(state->global, table, sizeof(SZrSymbolTable));
}

/* Symbol_New 复制语义收集器传入的推断类型，避免释放表时析构调用方仍持有的原件。 */
static SZrInferredType *copy_symbol_type_info(SZrState *state, const SZrInferredType *typeInfo) {
    SZrInferredType *copy;

    if (state == ZR_NULL || typeInfo == ZR_NULL) {
        return ZR_NULL;
    }

    copy = (SZrInferredType *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrInferredType));
    if (copy == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_InferredType_Init(state, copy, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Copy(state, copy, typeInfo);
    return copy;
}

/* 为声明建立 LSP 展示投影：类型与引用列表归符号所有，AST 和名称沿分析期借用。 */
SZrSymbol *ZrLanguageServer_Symbol_New(SZrState *state, EZrSymbolType type, 
                        SZrString *name, SZrFileRange location,
                        SZrInferredType *typeInfo,
                        EZrAccessModifier accessModifier,
                        SZrAstNode *astNode) {
    if (state == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrSymbol *symbol = (SZrSymbol *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrSymbol));
    if (symbol == ZR_NULL) {
        return ZR_NULL;
    }
    
    symbol->type = type;
    symbol->name = name;
    symbol->location = location;
    symbol->selectionRange = get_symbol_selection_range_from_ast(astNode, location);
    symbol->typeInfo = copy_symbol_type_info(state, typeInfo);
    if (typeInfo != ZR_NULL && symbol->typeInfo == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, symbol, sizeof(SZrSymbol));
        return ZR_NULL;
    }
    symbol->ffiHoverMetadata = build_symbol_ffi_hover_metadata_string(state, astNode);
    symbol->isExported = ZR_FALSE;
    symbol->accessModifier = accessModifier;
    symbol->isConst = ZR_FALSE; // 默认不是 const
    symbol->astNode = astNode;
    symbol->scope = ZR_NULL;
    symbol->referenceCount = 0;
    symbol->semanticId = 0;
    symbol->semanticTypeId = 0;
    symbol->overloadSetId = 0;
    symbol->hasPropertyContract = ZR_FALSE;
    ZrCore_Memory_RawSet(&symbol->propertyContract, 0, sizeof(symbol->propertyContract));
    
    // 从 AST 节点中提取 isConst 信息
    if (astNode != ZR_NULL) {
        if (astNode->type == ZR_AST_VARIABLE_DECLARATION) {
            symbol->isConst = astNode->data.variableDeclaration.isConst;
        } else if (astNode->type == ZR_AST_PARAMETER) {
            symbol->isConst = astNode->data.parameter.isConst;
        } else if (astNode->type == ZR_AST_STRUCT_FIELD) {
            symbol->isConst = astNode->data.structField.isConst;
        } else if (astNode->type == ZR_AST_CLASS_FIELD) {
            symbol->isConst = astNode->data.classField.isConst;
        } else if (astNode->type == ZR_AST_INTERFACE_FIELD_DECLARATION) {
            symbol->isConst = astNode->data.interfaceFieldDeclaration.isConst;
        }
    }
    
    ZrCore_Array_Init(state, &symbol->references, sizeof(SZrFileRange), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    
    return symbol;
}

/* 表和独立测试都会调用；只释放本体拥有的资源，不释放借用的 AST 与 GC 字符串。 */
void ZrLanguageServer_Symbol_Free(SZrState *state, SZrSymbol *symbol) {
    if (state == ZR_NULL || symbol == ZR_NULL) {
        return;
    }
    
    ZrCore_Array_Free(state, &symbol->references);
    if (symbol->typeInfo != ZR_NULL) {
        ZrParser_InferredType_Free(state, symbol->typeInfo);
        ZrCore_Memory_RawFree(state->global, symbol->typeInfo, sizeof(SZrInferredType));
        symbol->typeInfo = ZR_NULL;
    }
    ZrCore_Memory_RawFree(state->global, symbol, sizeof(SZrSymbol));
}

/* ReferenceTracker 将声明或属性账本命中的位置写入展示层引用列表。 */
TZrBool ZrLanguageServer_Symbol_AddReference(SZrState *state, SZrSymbol *symbol, SZrFileRange location) {
    if (state == ZR_NULL || symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    ZrCore_Array_Push(state, &symbol->references, &location);
    symbol->referenceCount++;
    return ZR_TRUE;
}

/* 线性回退按名称字节找单个声明；它不能枚举同层重载。 */
static SZrSymbol *lookup_symbol_in_scope(SZrSymbolScope *scope, SZrString *name) {
    if (scope == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 获取名称的字符串
    TZrNativeString nameStr;
    TZrSize nameLen;
    if (name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        nameStr = ZrCore_String_GetNativeStringShort(name);
        nameLen = name->shortStringLength;
    } else {
        nameStr = ZrCore_String_GetNativeString(name);
        nameLen = name->longStringLength;
    }
    
    // 在当前作用域中查找
    for (TZrSize i = 0; i < scope->symbols.length; i++) {
        SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&scope->symbols, i);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
            SZrSymbol *symbol = *symbolPtr;
            if (symbol->name != ZR_NULL) {
                TZrNativeString symbolNameStr;
                TZrSize symbolNameLen;
                if (symbol->name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                    symbolNameStr = ZrCore_String_GetNativeStringShort(symbol->name);
                    symbolNameLen = symbol->name->shortStringLength;
                } else {
                    symbolNameStr = ZrCore_String_GetNativeString(symbol->name);
                    symbolNameLen = symbol->name->longStringLength;
                }
                
                if (symbolNameLen == nameLen && memcmp(symbolNameStr, nameStr, nameLen) == 0) {
                    return symbol;
                }
            }
        }
    }
    
    return ZR_NULL;
}

/* 语义收集器先按词法作用域安放声明，再为按名和按位置查询建立辅助索引；
 * 作用域拥有符号，两个名称索引仅保存借用指针。 */
TZrBool ZrLanguageServer_SymbolTable_AddSymbolEx(SZrState *state, SZrSymbolTable *table,
                               EZrSymbolType type, SZrString *name,
                               SZrFileRange location,
                               SZrInferredType *typeInfo,
                               EZrAccessModifier accessModifier,
                               SZrAstNode *astNode,
                               SZrSymbol **outSymbol) {
    if (state == ZR_NULL || table == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 获取当前作用域
    SZrSymbolScope *currentScope = ZrLanguageServer_SymbolTable_GetCurrentScope(table);
    if (currentScope == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 创建符号
    SZrSymbol *symbol = ZrLanguageServer_Symbol_New(state, type, name, location, typeInfo, accessModifier, astNode);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outSymbol != ZR_NULL) {
        *outSymbol = symbol;
    }
    
    symbol->scope = currentScope;
    
    // 添加到当前作用域
    ZrCore_Array_Push(state, &currentScope->symbols, &symbol);
    
    /* 相同名称的多个投影共享数组；索引失败仍可由 allScopes 做线性查找。 */
    if (table->nameToSymbolsMap != ZR_NULL) {
        SZrTypeValue key;
        ZrCore_Value_InitAsRawObject(state, &key, &name->super);
        
        // 查找是否已存在同名符号数组
        const SZrTypeValue *existingValue = ZrCore_Object_GetValue(state, table->nameToSymbolsMap, &key);
        SZrArray *symbolArray = ZR_NULL;
        
        if (existingValue != ZR_NULL && existingValue->type == ZR_VALUE_TYPE_NATIVE_POINTER) {
            symbolArray = (SZrArray *)existingValue->value.nativeObject.nativePointer;
        }
        
        // 如果不存在，创建新数组
        if (symbolArray == ZR_NULL || !symbolArray->isValid) {
            // 单独分配 SZrArray（不能嵌入在 SZrObject 中）
            symbolArray = (SZrArray *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrArray));
            if (symbolArray != ZR_NULL) {
                ZrCore_Array_Init(state,
                                  symbolArray,
                                  sizeof(SZrSymbol *),
                                  ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
                
                // 添加到 Object 映射（使用 NATIVE_POINTER 类型）
                SZrTypeValue value;
                ZrCore_Value_InitAsNativePointer(state, &value, (TZrPtr)symbolArray);
                ZrCore_Object_SetValue(state, table->nameToSymbolsMap, &key, &value);
            }
        }
        
        // 将符号添加到数组
        if (symbolArray != ZR_NULL && symbolArray->isValid) {
            ZrCore_Array_Push(state, symbolArray, &symbol);
        }
    }
    
    /* TODO: 此备选哈希表在查询函数中未被读取，却与 Object 映射重复分配；
     * 需核对性能与失败回退契约，再决定保留或移除。 */
    if (table->nameToSymbolsHashSet.isValid) {
        SZrTypeValue key;
        ZrCore_Value_InitAsRawObject(state, &key, &name->super);
        
        SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &table->nameToSymbolsHashSet, &key);
        SZrArray *symbolArray = ZR_NULL;
        
        if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
            symbolArray = (SZrArray *)pair->value.value.nativeObject.nativePointer;
        } else {
            // 创建新数组
            symbolArray = (SZrArray *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrArray));
            if (symbolArray != ZR_NULL) {
                ZrCore_Array_Init(state,
                                  symbolArray,
                                  sizeof(SZrSymbol *),
                                  ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
                
                // 添加到哈希表
                pair = ZrCore_HashSet_Add(state, &table->nameToSymbolsHashSet, &key);
                if (pair != ZR_NULL) {
                    SZrTypeValue value;
                    ZrCore_Value_InitAsNativePointer(state, &value, (TZrPtr)symbolArray);
                    ZrCore_Value_Copy(state, &pair->value, &value);
                }
            }
        }
        
        // 将符号添加到数组
        if (symbolArray != ZR_NULL && symbolArray->isValid) {
            ZrCore_Array_Push(state, symbolArray, &symbol);
        }
    }
    
    return ZR_TRUE;
}

/* 收集期和展示层解析沿词法父链找首个同名投影；重载候选走 LookupAll。 */
SZrSymbol *ZrLanguageServer_SymbolTable_Lookup(SZrSymbolTable *table, SZrString *name, SZrSymbolScope *scope) {
    if (table == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 从指定作用域开始向上查找
    SZrSymbolScope *currentScope = scope;
    if (currentScope == ZR_NULL) {
        currentScope = ZrLanguageServer_SymbolTable_GetCurrentScope(table);
    }
    
    while (currentScope != ZR_NULL) {
        SZrSymbol *symbol = lookup_symbol_in_scope(currentScope, name);
        if (symbol != ZR_NULL) {
            return symbol;
        }
        currentScope = currentScope->parent;
    }
    
    return ZR_NULL;
}

/* 旧位置查询服务类型提示和兼容展示；canonical SymbolAt 仍由 parser 的 SymbolId 决定。 */
SZrSymbol *ZrLanguageServer_SymbolTable_LookupAtPosition(SZrSymbolTable *table,
                                                         SZrString *name,
                                                         SZrFileRange position) {
    SZrSymbol *bestSymbol = ZR_NULL;

    if (table == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    if (table->nameToSymbolsMap != ZR_NULL) {
        SZrState *state = table->state;
        SZrTypeValue key;
        const SZrTypeValue *existingValue;
        SZrArray *symbolArray;

        if (state != ZR_NULL) {
            ZrCore_Value_InitAsRawObject(state, &key, &name->super);
            existingValue = ZrCore_Object_GetValue(state, table->nameToSymbolsMap, &key);
            if (existingValue != ZR_NULL && existingValue->type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                symbolArray = (SZrArray *)existingValue->value.nativeObject.nativePointer;
                if (symbolArray != ZR_NULL && symbolArray->isValid) {
                    for (TZrSize i = 0; i < symbolArray->length; i++) {
                        SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(symbolArray, i);
                        SZrSymbol *symbol;

                        if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL) {
                            continue;
                        }

                        symbol = *symbolPtr;
                        if (is_position_in_range(position, get_symbol_match_range(symbol))) {
                            return symbol;
                        }

                        if (symbol_matches_lookup_position(symbol, position) &&
                            symbol_is_better_lookup_candidate(symbol, bestSymbol)) {
                            bestSymbol = symbol;
                        }
                    }
                }
            }
        }
    }

    if (bestSymbol != ZR_NULL) {
        return bestSymbol;
    }

    /* 索引未命中时仍用完整名称字节比较；AST 名称不保证与声明共用索引键对象。 */
    for (TZrSize scopeIndex = 0; scopeIndex < table->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr =
            (SZrSymbolScope **)ZrCore_Array_Get(&table->allScopes, scopeIndex);
        SZrSymbolScope *scope = scopePtr != ZR_NULL ? *scopePtr : ZR_NULL;
        SZrSymbol *candidate;

        if (scope == ZR_NULL || !scope_contains_position(scope, position)) {
            continue;
        }

        candidate = lookup_symbol_in_scope(scope, name);
        if (candidate != ZR_NULL && symbol_matches_lookup_position(candidate, position) &&
            (bestSymbol == ZR_NULL ||
             scope_is_deeper_candidate(scope, bestSymbol->scope) ||
             (scope == bestSymbol->scope &&
              symbol_is_better_lookup_candidate(candidate, bestSymbol)))) {
            bestSymbol = candidate;
        }
    }

    if (bestSymbol != ZR_NULL) {
        return bestSymbol;
    }

    return ZrLanguageServer_SymbolTable_Lookup(table, name, ZR_NULL);
}

/* 属性访问器合并需要同名完整候选；调用方持有 result 容器，元素只在表存活时有效。 */
TZrBool ZrLanguageServer_SymbolTable_LookupAll(SZrState *state, SZrSymbolTable *table, 
                              SZrString *name, SZrSymbolScope *scope,
                              SZrArray *result) {
    if (state == ZR_NULL || table == ZR_NULL || name == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrSymbol *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }
    
    // 使用哈希表和 Object 映射快速查找
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, &name->super);
    
    // 首先从 Object 映射中查找（支持重载）
    if (table->nameToSymbolsMap != ZR_NULL) {
        const SZrTypeValue *existingValue = ZrCore_Object_GetValue(state, table->nameToSymbolsMap, &key);
        if (existingValue != ZR_NULL && existingValue->type == ZR_VALUE_TYPE_NATIVE_POINTER) {
            SZrArray *symbolArray = (SZrArray *)existingValue->value.nativeObject.nativePointer;
            if (symbolArray != ZR_NULL && symbolArray->isValid) {
                // 检查作用域可见性
                SZrSymbolScope *currentScope = scope;
                if (currentScope == ZR_NULL) {
                    currentScope = ZrLanguageServer_SymbolTable_GetCurrentScope(table);
                }
                
                for (TZrSize i = 0; i < symbolArray->length; i++) {
                    SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(symbolArray, i);
                    if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                        SZrSymbol *symbol = *symbolPtr;
                        
                        // 检查作用域可见性
                        TZrBool isVisible = ZR_FALSE;
                        SZrSymbolScope *symbolScope = symbol->scope;
                        
                        // 检查是否在可见的作用域内
                        SZrSymbolScope *checkScope = currentScope;
                        while (checkScope != ZR_NULL) {
                            if (checkScope == symbolScope) {
                                isVisible = ZR_TRUE;
                                break;
                            }
                            checkScope = checkScope->parent;
                        }
                        
                        // 如果是全局作用域，也可见
                        if (symbolScope == table->globalScope) {
                            isVisible = ZR_TRUE;
                        }
                        
                        if (isVisible) {
                            ZrCore_Array_Push(state, result, &symbol);
                        }
                    }
                }
                return ZR_TRUE;
            }
        }
    }
    
    /* BUG: 若 New 的 Object 映射创建失败或键未命中，回退每层只取首个同名符号；
     * 同层 getter/setter 或重载被截断，semantic_analyzer_symbols 的属性合并会漏候选。 */
    SZrSymbolScope *currentScope = scope;
    if (currentScope == ZR_NULL) {
        currentScope = ZrLanguageServer_SymbolTable_GetCurrentScope(table);
    }
    
    while (currentScope != ZR_NULL) {
        SZrSymbol *symbol = lookup_symbol_in_scope(currentScope, name);
        if (symbol != ZR_NULL) {
            ZrCore_Array_Push(state, result, &symbol);
        }
        currentScope = currentScope->parent;
    }
    
    return ZR_TRUE;
    
    // TODO: 原哈希表实现（暂时注释）
    /*
    SZrTypeValue key;
    ZrCore_Value_InitAsRawObject(state, &key, &name->super);
    
    SZrHashKeyValuePair *pair = ZrCore_HashSet_Find(state, &table->nameToSymbolsMap, &key);
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_OBJECT) {
        SZrArray *symbolArray = (SZrArray *)pair->value.value.object;
        if (symbolArray != ZR_NULL) {
            // 检查作用域可见性
            SZrSymbolScope *currentScope = scope;
            if (currentScope == ZR_NULL) {
                currentScope = ZrLanguageServer_SymbolTable_GetCurrentScope(table);
            }
            
            for (TZrSize i = 0; i < symbolArray->length; i++) {
                SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(symbolArray, i);
                if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                    SZrSymbol *symbol = *symbolPtr;
                    
                    // 检查作用域可见性
                    SZrSymbolScope *symbolScope = symbol->scope;
                    TZrBool isVisible = ZR_FALSE;
                    
                    // 检查是否在可见的作用域内
                    SZrSymbolScope *checkScope = currentScope;
                    while (checkScope != ZR_NULL) {
                        if (checkScope == symbolScope) {
                            isVisible = ZR_TRUE;
                            break;
                        }
                        checkScope = checkScope->parent;
                    }
                    
                    // 如果是全局作用域，也可见
                    if (symbolScope == table->globalScope) {
                        isVisible = ZR_TRUE;
                    }
                    
                    if (isVisible) {
                        ZrCore_Array_Push(state, result, &symbol);
                    }
                }
            }
        }
    }
    
    return ZR_TRUE;
    */
}

/* 公开的旧范围查询目前仓内无直接调用；保留给展示层，返回表持有的符号。 */
SZrSymbol *ZrLanguageServer_SymbolTable_FindDefinition(SZrSymbolTable *table, SZrFileRange position) {
    SZrSymbol *bestSymbol = ZR_NULL;

    if (table == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize scopeIndex = 0; scopeIndex < table->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrCore_Array_Get(&table->allScopes, scopeIndex);
        if (scopePtr == ZR_NULL || *scopePtr == ZR_NULL) {
            continue;
        }

        for (TZrSize symbolIndex = 0; symbolIndex < (*scopePtr)->symbols.length; symbolIndex++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&(*scopePtr)->symbols, symbolIndex);
            if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL) {
                continue;
            }

            if (is_position_in_range(position, get_symbol_match_range(*symbolPtr)) &&
                symbol_is_better_lookup_candidate(*symbolPtr, bestSymbol)) {
                bestSymbol = *symbolPtr;
            }
        }
    }

    return bestSymbol;
}

/* 按 parser 规范 SymbolId 查找 LSP 符号投影；导航不得用同名范围猜测身份。 */
SZrSymbol *ZrLanguageServer_SymbolTable_FindBySemanticId(SZrSymbolTable *table,
                                                         TZrSymbolId semanticId) {
    if (table == ZR_NULL || semanticId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }

    for (TZrSize scopeIndex = 0; scopeIndex < table->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr =
                (SZrSymbolScope **)ZrCore_Array_Get(&table->allScopes, scopeIndex);
        SZrSymbolScope *scope = scopePtr != ZR_NULL ? *scopePtr : ZR_NULL;

        if (scope == ZR_NULL) {
            continue;
        }

        for (TZrSize symbolIndex = 0; symbolIndex < scope->symbols.length; symbolIndex++) {
            SZrSymbol **symbolPtr =
                    (SZrSymbol **)ZrCore_Array_Get(&scope->symbols, symbolIndex);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
                (*symbolPtr)->semanticId == semanticId) {
                return *symbolPtr;
            }
        }
    }

    return ZR_NULL;
}

/* 收集器进入块、函数或类型成员前压栈；范围和 parent 供后续按位置查询。 */
void ZrLanguageServer_SymbolTable_EnterScope(SZrState *state, SZrSymbolTable *table, 
                              SZrFileRange range, TZrBool isFunctionScope,
                              TZrBool isClassScope, TZrBool isStructScope) {
    if (state == ZR_NULL || table == ZR_NULL) {
        return;
    }
    
    SZrSymbolScope *newScope = (SZrSymbolScope *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrSymbolScope));
    if (newScope == ZR_NULL) {
        return;
    }
    
    ZrCore_Array_Init(state, &newScope->symbols, sizeof(SZrSymbol *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    newScope->parent = ZrLanguageServer_SymbolTable_GetCurrentScope(table);
    newScope->range = range;
    newScope->isFunctionScope = isFunctionScope;
    newScope->isClassScope = isClassScope;
    newScope->isStructScope = isStructScope;
    
    ZrCore_Array_Push(state, &table->scopeStack, &newScope);
    ZrCore_Array_Push(state, &table->allScopes, &newScope);
}

/* 收集器与 EnterScope 配对，只改当前路径；历史作用域仍归 allScopes 持有。 */
void ZrLanguageServer_SymbolTable_ExitScope(SZrSymbolTable *table) {
    if (table == ZR_NULL || table->scopeStack.length <= 1) {
        return;
    }
    
    ZR_UNUSED_PARAMETER(ZrCore_Array_Pop(&table->scopeStack));
}

/* 声明收集与名称回退共用当前词法路径；无有效栈顶时回到全局作用域。 */
SZrSymbolScope *ZrLanguageServer_SymbolTable_GetCurrentScope(SZrSymbolTable *table) {
    if (table == ZR_NULL || table->scopeStack.length == 0) {
        return table != ZR_NULL ? table->globalScope : ZR_NULL;
    }
    
    SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrCore_Array_Get(&table->scopeStack, 
                                                                 table->scopeStack.length - 1);
    if (scopePtr != ZR_NULL) {
        return *scopePtr;
    }
    
    return table->globalScope;
}
