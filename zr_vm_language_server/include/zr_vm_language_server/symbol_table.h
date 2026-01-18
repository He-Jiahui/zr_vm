//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_LANGUAGE_SERVER_SYMBOL_TABLE_H
#define ZR_VM_LANGUAGE_SERVER_SYMBOL_TABLE_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/type_system.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/object.h"

// 符号类型枚举
enum EZrSymbolType {
    ZR_SYMBOL_VARIABLE,
    ZR_SYMBOL_FUNCTION,
    ZR_SYMBOL_CLASS,
    ZR_SYMBOL_STRUCT,
    ZR_SYMBOL_INTERFACE,
    ZR_SYMBOL_ENUM,
    ZR_SYMBOL_MODULE,
    ZR_SYMBOL_PARAMETER,
    ZR_SYMBOL_FIELD,
    ZR_SYMBOL_METHOD,
    ZR_SYMBOL_PROPERTY,
    ZR_SYMBOL_ENUM_MEMBER,
};

typedef enum EZrSymbolType EZrSymbolType;

// 符号定义
typedef struct SZrSymbol {
    EZrSymbolType type;
    SZrString *name;
    SZrFileRange location;           // 定义位置
    SZrInferredType *typeInfo;       // 类型信息（可选，可能为ZR_NULL）
    SZrArray references;              // 引用位置数组（SZrFileRange）
    TBool isExported;                 // 是否导出
    EZrAccessModifier accessModifier; // 访问修饰符
    TBool isConst;                    // 是否为 const 符号
    SZrAstNode *astNode;              // 关联的 AST 节点（可选）
    struct SZrSymbolScope *scope;     // 所属作用域
    TZrSize referenceCount;           // 引用计数
} SZrSymbol;

// 作用域节点
typedef struct SZrSymbolScope {
    SZrArray symbols;                 // 符号数组（SZrSymbol*）
    struct SZrSymbolScope *parent;    // 父作用域
    SZrFileRange range;                // 作用域范围
    TBool isFunctionScope;             // 是否为函数作用域
    TBool isClassScope;                // 是否为类作用域
    TBool isStructScope;               // 是否为结构体作用域
} SZrSymbolScope;

// 符号表
typedef struct SZrSymbolTable {
    SZrState *state;
    SZrSymbolScope *globalScope;      // 全局作用域
    SZrArray scopeStack;               // 作用域栈（用于构建时）
    SZrObject *nameToSymbolsMap;      // 名称到符号数组的映射对象（使用 nodeMap 存储）
    SZrHashSet nameToSymbolsHashSet;  // 名称到符号的哈希表（用于快速查找）
    TBool useHashTable;                // 是否使用哈希表（默认使用 Object）
} SZrSymbolTable;

// 符号表管理函数

// 创建符号表
ZR_LANGUAGE_SERVER_API SZrSymbolTable *ZrSymbolTableNew(SZrState *state);

// 释放符号表
ZR_LANGUAGE_SERVER_API void ZrSymbolTableFree(SZrState *state, SZrSymbolTable *table);

// 添加符号定义
ZR_LANGUAGE_SERVER_API TBool ZrSymbolTableAddSymbol(SZrState *state, SZrSymbolTable *table, 
                                                      EZrSymbolType type, SZrString *name,
                                                      SZrFileRange location, 
                                                      SZrInferredType *typeInfo,
                                                      EZrAccessModifier accessModifier,
                                                      SZrAstNode *astNode);

// 查找符号（返回第一个匹配的符号，如果有重载则返回第一个）
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrSymbolTableLookup(SZrSymbolTable *table, SZrString *name, 
                                                       SZrSymbolScope *scope);

// 查找所有匹配的符号（用于函数重载）
ZR_LANGUAGE_SERVER_API TBool ZrSymbolTableLookupAll(SZrState *state, SZrSymbolTable *table, 
                                                      SZrString *name, SZrSymbolScope *scope,
                                                      SZrArray *result);

// 查找定义位置
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrSymbolTableFindDefinition(SZrSymbolTable *table, 
                                                                SZrFileRange position);

// 获取范围内的符号
ZR_LANGUAGE_SERVER_API TBool ZrSymbolTableGetSymbolsInRange(SZrState *state, SZrSymbolTable *table,
                                                            SZrFileRange range, SZrArray *result);

// 作用域管理函数

// 进入作用域
ZR_LANGUAGE_SERVER_API void ZrSymbolTableEnterScope(SZrState *state, SZrSymbolTable *table, 
                                                      SZrFileRange range, TBool isFunctionScope,
                                                      TBool isClassScope, TBool isStructScope);

// 退出作用域
ZR_LANGUAGE_SERVER_API void ZrSymbolTableExitScope(SZrSymbolTable *table);

// 获取当前作用域
ZR_LANGUAGE_SERVER_API SZrSymbolScope *ZrSymbolTableGetCurrentScope(SZrSymbolTable *table);

// 符号管理函数

// 创建符号
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrSymbolNew(SZrState *state, EZrSymbolType type, 
                                                SZrString *name, SZrFileRange location,
                                                SZrInferredType *typeInfo,
                                                EZrAccessModifier accessModifier,
                                                SZrAstNode *astNode);

// 释放符号
ZR_LANGUAGE_SERVER_API void ZrSymbolFree(SZrState *state, SZrSymbol *symbol);

// 添加引用到符号
ZR_LANGUAGE_SERVER_API TBool ZrSymbolAddReference(SZrState *state, SZrSymbol *symbol, 
                                                    SZrFileRange location);

// 获取符号的引用计数
ZR_LANGUAGE_SERVER_API TZrSize ZrSymbolGetReferenceCount(SZrSymbol *symbol);

#endif //ZR_VM_LANGUAGE_SERVER_SYMBOL_TABLE_H
