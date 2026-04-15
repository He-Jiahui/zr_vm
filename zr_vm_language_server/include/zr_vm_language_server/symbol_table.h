//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_LANGUAGE_SERVER_SYMBOL_TABLE_H
#define ZR_VM_LANGUAGE_SERVER_SYMBOL_TABLE_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/semantic.h"
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
    SZrFileRange selectionRange;     // 名称位置
    SZrInferredType *typeInfo;       // 类型信息（可选，可能为ZR_NULL）
    SZrString *ffiHoverMetadata;     // 稳定缓存的 FFI/decorator hover 文本
    SZrArray references;              // 引用位置数组（SZrFileRange）
    TZrBool isExported;                 // 是否导出
    EZrAccessModifier accessModifier; // 访问修饰符
    TZrBool isConst;                    // 是否为 const 符号
    SZrAstNode *astNode;              // 关联的 AST 节点（可选）
    struct SZrSymbolScope *scope;     // 所属作用域
    TZrSize referenceCount;           // 引用计数
    TZrSymbolId semanticId;           // 语义层稳定符号 ID
    TZrTypeId semanticTypeId;         // 语义层类型 ID
    TZrOverloadSetId overloadSetId;   // 语义层重载集 ID
} SZrSymbol;

// 作用域节点
typedef struct SZrSymbolScope {
    SZrArray symbols;                 // 符号数组（SZrSymbol*）
    struct SZrSymbolScope *parent;    // 父作用域
    SZrFileRange range;                // 作用域范围
    TZrBool isFunctionScope;             // 是否为函数作用域
    TZrBool isClassScope;                // 是否为类作用域
    TZrBool isStructScope;               // 是否为结构体作用域
} SZrSymbolScope;

// 符号表
typedef struct SZrSymbolTable {
    SZrState *state;
    SZrSymbolScope *globalScope;      // 全局作用域
    SZrArray scopeStack;               // 作用域栈（用于构建时）
    SZrArray allScopes;                // 所有已创建作用域（SZrSymbolScope*）
    SZrObject *nameToSymbolsMap;      // 名称到符号数组的映射对象（使用 nodeMap 存储）
    SZrHashSet nameToSymbolsHashSet;  // 名称到符号的哈希表（用于快速查找）
    TZrBool useHashTable;                // 是否使用哈希表（默认使用 Object）
} SZrSymbolTable;

// 符号表管理函数

// 创建符号表
ZR_LANGUAGE_SERVER_API SZrSymbolTable *ZrLanguageServer_SymbolTable_New(SZrState *state);

// 释放符号表
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SymbolTable_Free(SZrState *state, SZrSymbolTable *table);

// 添加符号定义
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SymbolTable_AddSymbol(SZrState *state, SZrSymbolTable *table, 
                                                      EZrSymbolType type, SZrString *name,
                                                      SZrFileRange location, 
                                                      SZrInferredType *typeInfo,
                                                      EZrAccessModifier accessModifier,
                                                      SZrAstNode *astNode);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SymbolTable_AddSymbolEx(SZrState *state, SZrSymbolTable *table,
                                                      EZrSymbolType type, SZrString *name,
                                                      SZrFileRange location,
                                                      SZrInferredType *typeInfo,
                                                      EZrAccessModifier accessModifier,
                                                      SZrAstNode *astNode,
                                                      SZrSymbol **outSymbol);

// 查找符号（返回第一个匹配的符号，如果有重载则返回第一个）
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_SymbolTable_Lookup(SZrSymbolTable *table, SZrString *name, 
                                                       SZrSymbolScope *scope);
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_SymbolTable_LookupAtPosition(SZrSymbolTable *table,
                                                                                 SZrString *name,
                                                                                 SZrFileRange position);

// 查找所有匹配的符号（用于函数重载）
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SymbolTable_LookupAll(SZrState *state, SZrSymbolTable *table, 
                                                       SZrString *name, SZrSymbolScope *scope,
                                                       SZrArray *result);
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SymbolTable_GetVisibleSymbolsAtPosition(SZrState *state,
                                                                                         SZrSymbolTable *table,
                                                                                         SZrFileRange position,
                                                                                         SZrArray *result);

// 查找定义位置
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_SymbolTable_FindDefinition(SZrSymbolTable *table, 
                                                                SZrFileRange position);

// 获取范围内的符号
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SymbolTable_GetSymbolsInRange(SZrState *state, SZrSymbolTable *table,
                                                            SZrFileRange range, SZrArray *result);

// 作用域管理函数

// 进入作用域
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SymbolTable_EnterScope(SZrState *state, SZrSymbolTable *table, 
                                                      SZrFileRange range, TZrBool isFunctionScope,
                                                      TZrBool isClassScope, TZrBool isStructScope);

// 退出作用域
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SymbolTable_ExitScope(SZrSymbolTable *table);

// 获取当前作用域
ZR_LANGUAGE_SERVER_API SZrSymbolScope *ZrLanguageServer_SymbolTable_GetCurrentScope(SZrSymbolTable *table);

// 符号管理函数

// 创建符号
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_Symbol_New(SZrState *state, EZrSymbolType type, 
                                                SZrString *name, SZrFileRange location,
                                                SZrInferredType *typeInfo,
                                                EZrAccessModifier accessModifier,
                                                SZrAstNode *astNode);

// 释放符号
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Symbol_Free(SZrState *state, SZrSymbol *symbol);

// 添加引用到符号
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Symbol_AddReference(SZrState *state, SZrSymbol *symbol, 
                                                    SZrFileRange location);

// 获取符号的引用计数
ZR_LANGUAGE_SERVER_API TZrSize ZrLanguageServer_Symbol_GetReferenceCount(SZrSymbol *symbol);

#endif //ZR_VM_LANGUAGE_SERVER_SYMBOL_TABLE_H
