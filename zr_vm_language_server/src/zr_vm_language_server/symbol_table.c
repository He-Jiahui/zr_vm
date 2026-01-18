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

// 辅助函数：检查位置是否在范围内
static TBool is_position_in_range(SZrFileRange position, SZrFileRange symbolRange) {
    // 首先检查源文件是否相同
    if (position.source != symbolRange.source && 
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
    TBool startMatch = (symbolRange.start.line < position.start.line) ||
                      (symbolRange.start.line == position.start.line && 
                       symbolRange.start.column <= position.start.column);
    TBool endMatch = (position.end.line < symbolRange.end.line) ||
                    (position.end.line == symbolRange.end.line && 
                     position.end.column <= symbolRange.end.column);
    
    return startMatch && endMatch;
}

// 辅助函数：检查符号是否与范围重叠
static TBool is_symbol_in_range(SZrFileRange symbolRange, SZrFileRange queryRange) {
    // 首先检查源文件是否相同
    if (queryRange.source != symbolRange.source && 
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
    TBool startOverlap = (symbolRange.start.line < queryRange.end.line) ||
                        (symbolRange.start.line == queryRange.end.line && 
                         symbolRange.start.column <= queryRange.end.column);
    TBool endOverlap = (queryRange.start.line < symbolRange.end.line) ||
                      (queryRange.start.line == symbolRange.end.line && 
                       queryRange.start.column <= symbolRange.end.column);
    
    return startOverlap && endOverlap;
}

// 创建符号表
SZrSymbolTable *ZrSymbolTableNew(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrSymbolTable *table = (SZrSymbolTable *)ZrMemoryRawMalloc(state->global, sizeof(SZrSymbolTable));
    if (table == ZR_NULL) {
        return ZR_NULL;
    }
    
    table->state = state;
    table->globalScope = ZR_NULL;
    ZrArrayInit(state, &table->scopeStack, sizeof(SZrSymbolScope *), 8);
    
    // 创建映射对象（使用 SZrObject 的 nodeMap 存储）
    table->nameToSymbolsMap = ZrObjectNew(state, ZR_NULL);
    if (table->nameToSymbolsMap != ZR_NULL) {
        // 初始化 Object 的 nodeMap（ZrObjectNew 只调用了 ZrHashSetConstruct，需要调用 ZrHashSetInit）
        // 直接调用 ZrHashSetInit 而不是 ZrObjectInit，避免调用构造函数查找
        ZrHashSetInit(state, &table->nameToSymbolsMap->nodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);
    }
    table->useHashTable = ZR_FALSE; // 使用 Object 映射
    
    // 同时初始化哈希表作为备选
    table->nameToSymbolsHashSet.buckets = ZR_NULL;
    table->nameToSymbolsHashSet.bucketSize = 0;
    table->nameToSymbolsHashSet.elementCount = 0;
    table->nameToSymbolsHashSet.capacity = 0;
    table->nameToSymbolsHashSet.resizeThreshold = 0;
    table->nameToSymbolsHashSet.isValid = ZR_FALSE;
    ZrHashSetInit(state, &table->nameToSymbolsHashSet, 4); // 4 = 16 buckets
    
    // 创建全局作用域
    table->globalScope = (SZrSymbolScope *)ZrMemoryRawMalloc(state->global, sizeof(SZrSymbolScope));
    if (table->globalScope == ZR_NULL) {
        ZrMemoryRawFree(state->global, table, sizeof(SZrSymbolTable));
        return ZR_NULL;
    }
    
    ZrArrayInit(state, &table->globalScope->symbols, sizeof(SZrSymbol *), 16);
    table->globalScope->parent = ZR_NULL;
    table->globalScope->isFunctionScope = ZR_FALSE;
    table->globalScope->isClassScope = ZR_FALSE;
    table->globalScope->isStructScope = ZR_FALSE;
    table->globalScope->range = ZrFileRangeCreate(
        ZrFilePositionCreate(0, 0, 0),
        ZrFilePositionCreate(0, 0, 0),
        ZR_NULL
    );
    
    // 将全局作用域压入栈
    ZrArrayPush(state, &table->scopeStack, &table->globalScope);
    
    return table;
}

// 释放符号表
void ZrSymbolTableFree(SZrState *state, SZrSymbolTable *table) {
    if (state == ZR_NULL || table == ZR_NULL) {
        return;
    }
    
    // 释放所有作用域和符号
    // 递归释放所有作用域（从栈顶开始，但不包括全局作用域）
    while (table->scopeStack.length > 0) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrArrayPop(&table->scopeStack);
        if (scopePtr != ZR_NULL && *scopePtr != ZR_NULL) {
            SZrSymbolScope *scope = *scopePtr;
            // 跳过全局作用域，它会在后面单独释放
            if (scope == table->globalScope) {
                continue;
            }
            // 释放作用域内的所有符号
            for (TZrSize i = 0; i < scope->symbols.length; i++) {
                SZrSymbol **symbolPtr = (SZrSymbol **)ZrArrayGet(&scope->symbols, i);
                if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                    ZrSymbolFree(state, *symbolPtr);
                }
            }
            ZrArrayFree(state, &scope->symbols);
            ZrMemoryRawFree(state->global, scope, sizeof(SZrSymbolScope));
        }
    }
    
    // 释放全局作用域
    if (table->globalScope != ZR_NULL) {
        // 释放作用域内的所有符号
        for (TZrSize i = 0; i < table->globalScope->symbols.length; i++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrArrayGet(&table->globalScope->symbols, i);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                ZrSymbolFree(state, *symbolPtr);
            }
        }
        ZrArrayFree(state, &table->globalScope->symbols);
        ZrMemoryRawFree(state->global, table->globalScope, sizeof(SZrSymbolScope));
    }
    
    ZrArrayFree(state, &table->scopeStack);
    
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
                                ZrArrayFree(state, symbolArray);
                                ZrMemoryRawFree(state->global, symbolArray, sizeof(SZrArray));
                            }
                        }
                    }
                    // 释放节点本身
                    SZrHashKeyValuePair *next = pair->next;
                    ZrMemoryRawFreeWithType(state->global, pair, sizeof(SZrHashKeyValuePair), 
                                           ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
                    pair = next;
                }
                nodeMap->buckets[i] = ZR_NULL;
            }
            nodeMap->elementCount = 0;
            // 释放 buckets 数组
            ZrHashSetDeconstruct(state, nodeMap);
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
                            ZrArrayFree(state, symbolArray);
                            ZrMemoryRawFree(state->global, symbolArray, sizeof(SZrArray));
                        }
                    }
                }
                // 释放节点本身
                SZrHashKeyValuePair *next = pair->next;
                ZrMemoryRawFreeWithType(state->global, pair, sizeof(SZrHashKeyValuePair), 
                                       ZR_MEMORY_NATIVE_TYPE_HASH_PAIR);
                pair = next;
            }
            table->nameToSymbolsHashSet.buckets[i] = ZR_NULL;
        }
        table->nameToSymbolsHashSet.elementCount = 0;
        // 释放 buckets 数组
        ZrHashSetDeconstruct(state, &table->nameToSymbolsHashSet);
    }
    
    ZrMemoryRawFree(state->global, table, sizeof(SZrSymbolTable));
}

// 创建符号
SZrSymbol *ZrSymbolNew(SZrState *state, EZrSymbolType type, 
                        SZrString *name, SZrFileRange location,
                        SZrInferredType *typeInfo,
                        EZrAccessModifier accessModifier,
                        SZrAstNode *astNode) {
    if (state == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrSymbol *symbol = (SZrSymbol *)ZrMemoryRawMalloc(state->global, sizeof(SZrSymbol));
    if (symbol == ZR_NULL) {
        return ZR_NULL;
    }
    
    symbol->type = type;
    symbol->name = name;
    symbol->location = location;
    symbol->typeInfo = typeInfo; // 注意：不复制，只是引用
    symbol->isExported = ZR_FALSE;
    symbol->accessModifier = accessModifier;
    symbol->isConst = ZR_FALSE; // 默认不是 const
    symbol->astNode = astNode;
    symbol->scope = ZR_NULL;
    symbol->referenceCount = 0;
    
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
    
    ZrArrayInit(state, &symbol->references, sizeof(SZrFileRange), 4);
    
    return symbol;
}

// 释放符号
void ZrSymbolFree(SZrState *state, SZrSymbol *symbol) {
    if (state == ZR_NULL || symbol == ZR_NULL) {
        return;
    }
    
    ZrArrayFree(state, &symbol->references);
    // 注意：不释放 typeInfo，因为它可能被其他地方引用
    ZrMemoryRawFree(state->global, symbol, sizeof(SZrSymbol));
}

// 添加引用到符号
TBool ZrSymbolAddReference(SZrState *state, SZrSymbol *symbol, SZrFileRange location) {
    if (state == ZR_NULL || symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    ZrArrayPush(state, &symbol->references, &location);
    symbol->referenceCount++;
    return ZR_TRUE;
}

// 获取符号的引用计数
TZrSize ZrSymbolGetReferenceCount(SZrSymbol *symbol) {
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
    TNativeString nameStr;
    TZrSize nameLen;
    if (name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        nameStr = ZrStringGetNativeStringShort(name);
        nameLen = name->shortStringLength;
    } else {
        nameStr = ZrStringGetNativeString(name);
        nameLen = name->longStringLength;
    }
    
    // 在当前作用域中查找
    for (TZrSize i = 0; i < scope->symbols.length; i++) {
        SZrSymbol **symbolPtr = (SZrSymbol **)ZrArrayGet(&scope->symbols, i);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
            SZrSymbol *symbol = *symbolPtr;
            if (symbol->name != ZR_NULL) {
                TNativeString symbolNameStr;
                TZrSize symbolNameLen;
                if (symbol->name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                    symbolNameStr = ZrStringGetNativeStringShort(symbol->name);
                    symbolNameLen = symbol->name->shortStringLength;
                } else {
                    symbolNameStr = ZrStringGetNativeString(symbol->name);
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
TBool ZrSymbolTableAddSymbol(SZrState *state, SZrSymbolTable *table, 
                              EZrSymbolType type, SZrString *name,
                              SZrFileRange location, 
                              SZrInferredType *typeInfo,
                              EZrAccessModifier accessModifier,
                              SZrAstNode *astNode) {
    if (state == ZR_NULL || table == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 获取当前作用域
    SZrSymbolScope *currentScope = ZrSymbolTableGetCurrentScope(table);
    if (currentScope == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 创建符号
    SZrSymbol *symbol = ZrSymbolNew(state, type, name, location, typeInfo, accessModifier, astNode);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    
    symbol->scope = currentScope;
    
    // 添加到当前作用域
    ZrArrayPush(state, &currentScope->symbols, &symbol);
    
    // 实现 Object 映射用于快速查找
    if (table->nameToSymbolsMap != ZR_NULL) {
        SZrTypeValue key;
        ZrValueInitAsRawObject(state, &key, &name->super);
        
        // 查找是否已存在同名符号数组
        const SZrTypeValue *existingValue = ZrObjectGetValue(state, table->nameToSymbolsMap, &key);
        SZrArray *symbolArray = ZR_NULL;
        
        if (existingValue != ZR_NULL && existingValue->type == ZR_VALUE_TYPE_NATIVE_POINTER) {
            symbolArray = (SZrArray *)existingValue->value.nativeObject.nativePointer;
        }
        
        // 如果不存在，创建新数组
        if (symbolArray == ZR_NULL || !symbolArray->isValid) {
            // 单独分配 SZrArray（不能嵌入在 SZrObject 中）
            symbolArray = (SZrArray *)ZrMemoryRawMalloc(state->global, sizeof(SZrArray));
            if (symbolArray != ZR_NULL) {
                ZrArrayInit(state, symbolArray, sizeof(SZrSymbol *), 4);
                
                // 添加到 Object 映射（使用 NATIVE_POINTER 类型）
                SZrTypeValue value;
                ZrValueInitAsNativePointer(state, &value, (TZrPtr)symbolArray);
                ZrObjectSetValue(state, table->nameToSymbolsMap, &key, &value);
            }
        }
        
        // 将符号添加到数组
        if (symbolArray != ZR_NULL && symbolArray->isValid) {
            ZrArrayPush(state, symbolArray, &symbol);
        }
    }
    
    // 同时添加到哈希表（作为备选）
    if (table->nameToSymbolsHashSet.isValid) {
        SZrTypeValue key;
        ZrValueInitAsRawObject(state, &key, &name->super);
        
        SZrHashKeyValuePair *pair = ZrHashSetFind(state, &table->nameToSymbolsHashSet, &key);
        SZrArray *symbolArray = ZR_NULL;
        
        if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
            symbolArray = (SZrArray *)pair->value.value.nativeObject.nativePointer;
        } else {
            // 创建新数组
            symbolArray = (SZrArray *)ZrMemoryRawMalloc(state->global, sizeof(SZrArray));
            if (symbolArray != ZR_NULL) {
                ZrArrayInit(state, symbolArray, sizeof(SZrSymbol *), 4);
                
                // 添加到哈希表
                pair = ZrHashSetAdd(state, &table->nameToSymbolsHashSet, &key);
                if (pair != ZR_NULL) {
                    SZrTypeValue value;
                    ZrValueInitAsNativePointer(state, &value, (TZrPtr)symbolArray);
                    ZrValueCopy(state, &pair->value, &value);
                }
            }
        }
        
        // 将符号添加到数组
        if (symbolArray != ZR_NULL && symbolArray->isValid) {
            ZrArrayPush(state, symbolArray, &symbol);
        }
    }
    
    return ZR_TRUE;
}

// 查找符号（返回第一个匹配的符号）
SZrSymbol *ZrSymbolTableLookup(SZrSymbolTable *table, SZrString *name, SZrSymbolScope *scope) {
    if (table == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 从指定作用域开始向上查找
    SZrSymbolScope *currentScope = scope;
    if (currentScope == ZR_NULL) {
        currentScope = ZrSymbolTableGetCurrentScope(table);
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

// 查找所有匹配的符号（用于函数重载）
TBool ZrSymbolTableLookupAll(SZrState *state, SZrSymbolTable *table, 
                              SZrString *name, SZrSymbolScope *scope,
                              SZrArray *result) {
    if (state == ZR_NULL || table == ZR_NULL || name == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrArrayInit(state, result, sizeof(SZrSymbol *), 4);
    }
    
    // 使用哈希表和 Object 映射快速查找
    SZrTypeValue key;
    ZrValueInitAsRawObject(state, &key, &name->super);
    
    // 首先从 Object 映射中查找（支持重载）
    if (table->nameToSymbolsMap != ZR_NULL) {
        const SZrTypeValue *existingValue = ZrObjectGetValue(state, table->nameToSymbolsMap, &key);
        if (existingValue != ZR_NULL && existingValue->type == ZR_VALUE_TYPE_NATIVE_POINTER) {
            SZrArray *symbolArray = (SZrArray *)existingValue->value.nativeObject.nativePointer;
            if (symbolArray != ZR_NULL && symbolArray->isValid) {
                // 检查作用域可见性
                SZrSymbolScope *currentScope = scope;
                if (currentScope == ZR_NULL) {
                    currentScope = ZrSymbolTableGetCurrentScope(table);
                }
                
                for (TZrSize i = 0; i < symbolArray->length; i++) {
                    SZrSymbol **symbolPtr = (SZrSymbol **)ZrArrayGet(symbolArray, i);
                    if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                        SZrSymbol *symbol = *symbolPtr;
                        
                        // 检查作用域可见性
                        TBool isVisible = ZR_FALSE;
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
                            ZrArrayPush(state, result, &symbol);
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
        currentScope = ZrSymbolTableGetCurrentScope(table);
    }
    
    while (currentScope != ZR_NULL) {
        SZrSymbol *symbol = lookup_symbol_in_scope(currentScope, name);
        if (symbol != ZR_NULL) {
            ZrArrayPush(state, result, &symbol);
        }
        currentScope = currentScope->parent;
    }
    
    return ZR_TRUE;
    
    // TODO: 原哈希表实现（暂时注释）
    /*
    SZrTypeValue key;
    ZrValueInitAsRawObject(state, &key, &name->super);
    
    SZrHashKeyValuePair *pair = ZrHashSetFind(state, &table->nameToSymbolsMap, &key);
    if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_OBJECT) {
        SZrArray *symbolArray = (SZrArray *)pair->value.value.object;
        if (symbolArray != ZR_NULL) {
            // 检查作用域可见性
            SZrSymbolScope *currentScope = scope;
            if (currentScope == ZR_NULL) {
                currentScope = ZrSymbolTableGetCurrentScope(table);
            }
            
            for (TZrSize i = 0; i < symbolArray->length; i++) {
                SZrSymbol **symbolPtr = (SZrSymbol **)ZrArrayGet(symbolArray, i);
                if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                    SZrSymbol *symbol = *symbolPtr;
                    
                    // 检查作用域可见性
                    SZrSymbolScope *symbolScope = symbol->scope;
                    TBool isVisible = ZR_FALSE;
                    
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
                        ZrArrayPush(state, result, &symbol);
                    }
                }
            }
        }
    }
    
    return ZR_TRUE;
    */
}

// 查找定义位置
SZrSymbol *ZrSymbolTableFindDefinition(SZrSymbolTable *table, SZrFileRange position) {
    if (table == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 遍历所有作用域查找包含该位置的符号
    // 实现位置比较：检查位置是否在符号的定义范围内
    
    // 从当前作用域栈开始查找（从内到外）
    for (TZrSize stackIdx = table->scopeStack.length; stackIdx > 0; stackIdx--) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrArrayGet(&table->scopeStack, stackIdx - 1);
        if (scopePtr != ZR_NULL && *scopePtr != ZR_NULL) {
            SZrSymbolScope *scope = *scopePtr;
            for (TZrSize i = 0; i < scope->symbols.length; i++) {
                SZrSymbol **symbolPtr = (SZrSymbol **)ZrArrayGet(&scope->symbols, i);
                if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                    SZrSymbol *symbol = *symbolPtr;
                    if (is_position_in_range(position, symbol->location)) {
                        return symbol;
                    }
                }
            }
        }
    }
    
    // 查找全局作用域
    if (table->globalScope != ZR_NULL) {
        for (TZrSize i = 0; i < table->globalScope->symbols.length; i++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrArrayGet(&table->globalScope->symbols, i);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                SZrSymbol *symbol = *symbolPtr;
                if (is_position_in_range(position, symbol->location)) {
                    return symbol;
                }
            }
        }
    }
    
    return ZR_NULL;
}

// 获取范围内的符号
TBool ZrSymbolTableGetSymbolsInRange(SZrState *state, SZrSymbolTable *table,
                                      SZrFileRange range, SZrArray *result) {
    if (state == ZR_NULL || table == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrArrayInit(state, result, sizeof(SZrSymbol *), 4);
    }
    
    // 实现范围查询：查找所有在指定范围内的符号
    
    // 从当前作用域栈开始查找
    for (TZrSize stackIdx = table->scopeStack.length; stackIdx > 0; stackIdx--) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrArrayGet(&table->scopeStack, stackIdx - 1);
        if (scopePtr != ZR_NULL && *scopePtr != ZR_NULL) {
            SZrSymbolScope *scope = *scopePtr;
            for (TZrSize i = 0; i < scope->symbols.length; i++) {
                SZrSymbol **symbolPtr = (SZrSymbol **)ZrArrayGet(&scope->symbols, i);
                if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                    SZrSymbol *symbol = *symbolPtr;
                    if (is_symbol_in_range(symbol->location, range)) {
                        ZrArrayPush(state, result, &symbol);
                    }
                }
            }
        }
    }
    
    // 查找全局作用域
    if (table->globalScope != ZR_NULL) {
        for (TZrSize i = 0; i < table->globalScope->symbols.length; i++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrArrayGet(&table->globalScope->symbols, i);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                SZrSymbol *symbol = *symbolPtr;
                if (is_symbol_in_range(symbol->location, range)) {
                    ZrArrayPush(state, result, &symbol);
                }
            }
        }
    }
    
    return ZR_TRUE;
}

// 进入作用域
void ZrSymbolTableEnterScope(SZrState *state, SZrSymbolTable *table, 
                              SZrFileRange range, TBool isFunctionScope,
                              TBool isClassScope, TBool isStructScope) {
    if (state == ZR_NULL || table == ZR_NULL) {
        return;
    }
    
    SZrSymbolScope *newScope = (SZrSymbolScope *)ZrMemoryRawMalloc(state->global, sizeof(SZrSymbolScope));
    if (newScope == ZR_NULL) {
        return;
    }
    
    ZrArrayInit(state, &newScope->symbols, sizeof(SZrSymbol *), 8);
    newScope->parent = ZrSymbolTableGetCurrentScope(table);
    newScope->range = range;
    newScope->isFunctionScope = isFunctionScope;
    newScope->isClassScope = isClassScope;
    newScope->isStructScope = isStructScope;
    
    ZrArrayPush(state, &table->scopeStack, &newScope);
}

// 退出作用域
void ZrSymbolTableExitScope(SZrSymbolTable *table) {
    if (table == ZR_NULL || table->scopeStack.length == 0) {
        return;
    }
    
    SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrArrayPop(&table->scopeStack);
    if (scopePtr != ZR_NULL && *scopePtr != ZR_NULL) {
        SZrSymbolScope *scope = *scopePtr;
        // 注意：不释放符号，因为它们可能被其他地方引用
        ZrArrayFree(table->state, &scope->symbols);
        ZrMemoryRawFree(table->state->global, scope, sizeof(SZrSymbolScope));
    }
}

// 获取当前作用域
SZrSymbolScope *ZrSymbolTableGetCurrentScope(SZrSymbolTable *table) {
    if (table == ZR_NULL || table->scopeStack.length == 0) {
        return table != ZR_NULL ? table->globalScope : ZR_NULL;
    }
    
    SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrArrayGet(&table->scopeStack, 
                                                                 table->scopeStack.length - 1);
    if (scopePtr != ZR_NULL) {
        return *scopePtr;
    }
    
    return table->globalScope;
}
