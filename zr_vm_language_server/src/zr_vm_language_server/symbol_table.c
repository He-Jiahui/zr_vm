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

#include <string.h>

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

// 辅助函数：检查位置是否在范围内
static TZrBool is_position_in_range(SZrFileRange position, SZrFileRange symbolRange) {
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

// 辅助函数：检查符号是否与范围重叠
static TZrBool is_symbol_in_range(SZrFileRange symbolRange, SZrFileRange queryRange) {
    // 首先检查源文件是否相同
    if (!source_uri_equals(queryRange.source, symbolRange.source) &&
        queryRange.source != ZR_NULL && symbolRange.source != ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 使用offset比较（如果可用）
    if (symbolRange.start.offset > 0 && symbolRange.end.offset > 0 &&
        queryRange.start.offset > 0 && queryRange.end.offset > 0) {
        // 符号范围与查询范围有重叠
        return (symbolRange.start.offset <= queryRange.end.offset &&
                queryRange.start.offset <= symbolRange.end.offset);
    }
    
    // 使用行号和列号比较
    TZrBool startOverlap = (symbolRange.start.line < queryRange.end.line) ||
                        (symbolRange.start.line == queryRange.end.line && 
                         symbolRange.start.column <= queryRange.end.column);
    TZrBool endOverlap = (queryRange.start.line < symbolRange.end.line) ||
                      (queryRange.start.line == symbolRange.end.line && 
                       queryRange.start.column <= symbolRange.end.column);
    
    return startOverlap && endOverlap;
}

static int compare_file_position(SZrFilePosition left, SZrFilePosition right) {
    if (left.offset > 0 && right.offset > 0) {
        if (left.offset < right.offset) {
            return -1;
        }
        if (left.offset > right.offset) {
            return 1;
        }
        return 0;
    }

    if (left.line < right.line) {
        return -1;
    }
    if (left.line > right.line) {
        return 1;
    }
    if (left.column < right.column) {
        return -1;
    }
    if (left.column > right.column) {
        return 1;
    }
    return 0;
}

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

static TZrBool symbol_matches_lookup_position(SZrSymbol *symbol, SZrFileRange position) {
    SZrFileRange symbolRange;

    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    symbolRange = get_symbol_match_range(symbol);
    if (!source_uri_equals(position.source, symbolRange.source) &&
        position.source != ZR_NULL && symbolRange.source != ZR_NULL) {
        return ZR_FALSE;
    }

    return compare_file_position(symbolRange.start, position.start) <= 0;
}

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

static TZrBool scope_contains_position(SZrSymbolScope *scope, SZrFileRange position) {
    if (scope == ZR_NULL) {
        return ZR_FALSE;
    }

    if (scope->parent == ZR_NULL) {
        return ZR_TRUE;
    }

    return is_position_in_range(position, scope->range);
}

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
                return fallback;
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

// 创建符号表
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
    ZrCore_Array_Init(state, &table->scopeStack, sizeof(SZrSymbolScope *), 8);
    ZrCore_Array_Init(state, &table->allScopes, sizeof(SZrSymbolScope *), 8);
    
    // 创建映射对象（使用 SZrObject 的 nodeMap 存储）
    table->nameToSymbolsMap = ZrCore_Object_New(state, ZR_NULL);
    if (table->nameToSymbolsMap != ZR_NULL) {
        // 初始化 Object 的 nodeMap（ZrCore_Object_New 只调用了 ZrCore_HashSet_Construct，需要调用 ZrCore_HashSet_Init）
        // 直接调用 ZrCore_HashSet_Init 而不是 ZrCore_Object_Init，避免调用构造函数查找
        ZrCore_HashSet_Init(state, &table->nameToSymbolsMap->nodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);
    }
    table->useHashTable = ZR_FALSE; // 使用 Object 映射
    
    // 同时初始化哈希表作为备选
    table->nameToSymbolsHashSet.buckets = ZR_NULL;
    table->nameToSymbolsHashSet.bucketSize = 0;
    table->nameToSymbolsHashSet.elementCount = 0;
    table->nameToSymbolsHashSet.capacity = 0;
    table->nameToSymbolsHashSet.resizeThreshold = 0;
    table->nameToSymbolsHashSet.isValid = ZR_FALSE;
    ZrCore_HashSet_Init(state, &table->nameToSymbolsHashSet, 4); // 4 = 16 buckets
    
    // 创建全局作用域
    table->globalScope = (SZrSymbolScope *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrSymbolScope));
    if (table->globalScope == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, table, sizeof(SZrSymbolTable));
        return ZR_NULL;
    }
    
    ZrCore_Array_Init(state, &table->globalScope->symbols, sizeof(SZrSymbol *), 16);
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

// 释放符号表
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
                    // 释放节点本身
                    SZrHashKeyValuePair *next = pair->next;
                    ZrCore_Memory_RawFreeWithType(state->global, pair, sizeof(SZrHashKeyValuePair), 
                                           ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
                    pair = next;
                }
                nodeMap->buckets[i] = ZR_NULL;
            }
            nodeMap->elementCount = 0;
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
                // 释放节点本身
                SZrHashKeyValuePair *next = pair->next;
                ZrCore_Memory_RawFreeWithType(state->global, pair, sizeof(SZrHashKeyValuePair), 
                                       ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
                pair = next;
            }
            table->nameToSymbolsHashSet.buckets[i] = ZR_NULL;
        }
        table->nameToSymbolsHashSet.elementCount = 0;
        // 释放 buckets 数组
        ZrCore_HashSet_Deconstruct(state, &table->nameToSymbolsHashSet);
    }
    
    ZrCore_Memory_RawFree(state->global, table, sizeof(SZrSymbolTable));
}

// 创建符号
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
    symbol->typeInfo = typeInfo; // 注意：不复制，只是引用
    symbol->isExported = ZR_FALSE;
    symbol->accessModifier = accessModifier;
    symbol->isConst = ZR_FALSE; // 默认不是 const
    symbol->astNode = astNode;
    symbol->scope = ZR_NULL;
    symbol->referenceCount = 0;
    symbol->semanticId = 0;
    symbol->semanticTypeId = 0;
    symbol->overloadSetId = 0;
    
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
    
    ZrCore_Array_Init(state, &symbol->references, sizeof(SZrFileRange), 4);
    
    return symbol;
}

// 释放符号
void ZrLanguageServer_Symbol_Free(SZrState *state, SZrSymbol *symbol) {
    if (state == ZR_NULL || symbol == ZR_NULL) {
        return;
    }
    
    ZrCore_Array_Free(state, &symbol->references);
    // 注意：不释放 typeInfo，因为它可能被其他地方引用
    ZrCore_Memory_RawFree(state->global, symbol, sizeof(SZrSymbol));
}

// 添加引用到符号
TZrBool ZrLanguageServer_Symbol_AddReference(SZrState *state, SZrSymbol *symbol, SZrFileRange location) {
    if (state == ZR_NULL || symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    ZrCore_Array_Push(state, &symbol->references, &location);
    symbol->referenceCount++;
    return ZR_TRUE;
}

// 获取符号的引用计数
TZrSize ZrLanguageServer_Symbol_GetReferenceCount(SZrSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return 0;
    }
    return symbol->referenceCount;
}

// 辅助函数：在作用域中查找符号
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

// 添加符号定义
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
    
    // 实现 Object 映射用于快速查找
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
                ZrCore_Array_Init(state, symbolArray, sizeof(SZrSymbol *), 4);
                
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
    
    // 同时添加到哈希表（作为备选）
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
                ZrCore_Array_Init(state, symbolArray, sizeof(SZrSymbol *), 4);
                
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

TZrBool ZrLanguageServer_SymbolTable_AddSymbol(SZrState *state, SZrSymbolTable *table,
                             EZrSymbolType type, SZrString *name,
                             SZrFileRange location,
                             SZrInferredType *typeInfo,
                             EZrAccessModifier accessModifier,
                             SZrAstNode *astNode) {
    return ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                    table,
                                    type,
                                    name,
                                    location,
                                    typeInfo,
                                    accessModifier,
                                    astNode,
                                    ZR_NULL);
}

// 查找符号（返回第一个匹配的符号）
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

    return ZrLanguageServer_SymbolTable_Lookup(table, name, ZR_NULL);
}

// 查找所有匹配的符号（用于函数重载）
TZrBool ZrLanguageServer_SymbolTable_LookupAll(SZrState *state, SZrSymbolTable *table, 
                              SZrString *name, SZrSymbolScope *scope,
                              SZrArray *result) {
    if (state == ZR_NULL || table == ZR_NULL || name == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrSymbol *), 4);
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
    
    // 回退到线性查找
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

TZrBool ZrLanguageServer_SymbolTable_GetVisibleSymbolsAtPosition(SZrState *state,
                                                                 SZrSymbolTable *table,
                                                                 SZrFileRange position,
                                                                 SZrArray *result) {
    SZrSymbolScope *bestScope = ZR_NULL;
    SZrSymbolScope *scope;

    if (state == ZR_NULL || table == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrSymbol *), 8);
    }

    for (TZrSize scopeIndex = 0; scopeIndex < table->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrCore_Array_Get(&table->allScopes, scopeIndex);
        if (scopePtr == ZR_NULL || *scopePtr == ZR_NULL) {
            continue;
        }

        if (scope_contains_position(*scopePtr, position) &&
            scope_is_deeper_candidate(*scopePtr, bestScope)) {
            bestScope = *scopePtr;
        }
    }

    if (bestScope == ZR_NULL) {
        bestScope = table->globalScope;
    }

    scope = bestScope;
    while (scope != ZR_NULL) {
        for (TZrSize symbolIndex = 0; symbolIndex < scope->symbols.length; symbolIndex++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&scope->symbols, symbolIndex);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
                symbol_matches_lookup_position(*symbolPtr, position)) {
                ZrCore_Array_Push(state, result, symbolPtr);
            }
        }
        scope = scope->parent;
    }

    return ZR_TRUE;
}

// 查找定义位置
SZrSymbol *ZrLanguageServer_SymbolTable_FindDefinition(SZrSymbolTable *table, SZrFileRange position) {
    if (table == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 遍历所有作用域查找包含该位置的符号
    // 实现位置比较：检查位置是否在符号的定义范围内
    
    // 从当前作用域栈开始查找（从内到外）
    for (TZrSize stackIdx = table->scopeStack.length; stackIdx > 0; stackIdx--) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrCore_Array_Get(&table->scopeStack, stackIdx - 1);
        if (scopePtr != ZR_NULL && *scopePtr != ZR_NULL) {
            SZrSymbolScope *scope = *scopePtr;
            for (TZrSize i = 0; i < scope->symbols.length; i++) {
                SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&scope->symbols, i);
                if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                    SZrSymbol *symbol = *symbolPtr;
                    if (is_position_in_range(position, symbol->selectionRange)) {
                        return symbol;
                    }
                }
            }
        }
    }
    
    // 查找全局作用域
    if (table->globalScope != ZR_NULL) {
        for (TZrSize i = 0; i < table->globalScope->symbols.length; i++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&table->globalScope->symbols, i);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                SZrSymbol *symbol = *symbolPtr;
                if (is_position_in_range(position, symbol->selectionRange)) {
                    return symbol;
                }
            }
        }
    }
    
    return ZR_NULL;
}

// 获取范围内的符号
TZrBool ZrLanguageServer_SymbolTable_GetSymbolsInRange(SZrState *state, SZrSymbolTable *table,
                                      SZrFileRange range, SZrArray *result) {
    if (state == ZR_NULL || table == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrSymbol *), 4);
    }
    
    // 实现范围查询：查找所有在指定范围内的符号
    
    // 从当前作用域栈开始查找
    for (TZrSize stackIdx = table->scopeStack.length; stackIdx > 0; stackIdx--) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrCore_Array_Get(&table->scopeStack, stackIdx - 1);
        if (scopePtr != ZR_NULL && *scopePtr != ZR_NULL) {
            SZrSymbolScope *scope = *scopePtr;
            for (TZrSize i = 0; i < scope->symbols.length; i++) {
                SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&scope->symbols, i);
                if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                    SZrSymbol *symbol = *symbolPtr;
                    if (is_symbol_in_range(symbol->location, range)) {
                        ZrCore_Array_Push(state, result, &symbol);
                    }
                }
            }
        }
    }
    
    // 查找全局作用域
    if (table->globalScope != ZR_NULL) {
        for (TZrSize i = 0; i < table->globalScope->symbols.length; i++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&table->globalScope->symbols, i);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                SZrSymbol *symbol = *symbolPtr;
                if (is_symbol_in_range(symbol->location, range)) {
                    ZrCore_Array_Push(state, result, &symbol);
                }
            }
        }
    }
    
    return ZR_TRUE;
}

// 进入作用域
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
    
    ZrCore_Array_Init(state, &newScope->symbols, sizeof(SZrSymbol *), 8);
    newScope->parent = ZrLanguageServer_SymbolTable_GetCurrentScope(table);
    newScope->range = range;
    newScope->isFunctionScope = isFunctionScope;
    newScope->isClassScope = isClassScope;
    newScope->isStructScope = isStructScope;
    
    ZrCore_Array_Push(state, &table->scopeStack, &newScope);
    ZrCore_Array_Push(state, &table->allScopes, &newScope);
}

// 退出作用域
void ZrLanguageServer_SymbolTable_ExitScope(SZrSymbolTable *table) {
    if (table == ZR_NULL || table->scopeStack.length <= 1) {
        return;
    }
    
    ZR_UNUSED_PARAMETER(ZrCore_Array_Pop(&table->scopeStack));
}

// 获取当前作用域
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
