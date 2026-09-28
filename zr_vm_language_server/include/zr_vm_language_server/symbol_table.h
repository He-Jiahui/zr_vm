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

/** @brief 将 AST 声明种类映射为 LSP 展示、补全与导航共用的符号分类。 */
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

/** @brief 符号分类枚举的公开类型别名，供符号构造和投影 API 传递。 */
typedef enum EZrSymbolType EZrSymbolType;

/**
 * @brief 把声明的范围、展示类型和 parser 规范身份汇集为 LSP 符号视图。
 * @note typeInfo 与 references 由符号持有；name、ffiHoverMetadata、范围 source 由 GC 管理，
 * astNode 借用当前 AST；semanticId 无效时不能据此执行规范语义查询。
 */
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
    TZrBool hasPropertyContract;      // 是否由 canonical PropertyQuery 投影
    SZrSemanticPropertyContract propertyContract; // canonical property/accessor identity
} SZrSymbol;

/**
 * @brief 保存符号收集时建立的词法层级，并在分析结束后继续服务按位置查找。
 * @note parent 指向同一表内较外层作用域；symbols 中的符号与作用域一起由表释放。
 */
typedef struct SZrSymbolScope {
    SZrArray symbols;                 // 符号数组（SZrSymbol*）
    struct SZrSymbolScope *parent;    // 父作用域
    SZrFileRange range;                // 作用域范围
    TZrBool isFunctionScope;             // 是否为函数作用域
    TZrBool isClassScope;                // 是否为类作用域
    TZrBool isStructScope;               // 是否为结构体作用域
} SZrSymbolScope;

/**
 * @brief 同时保存词法作用域树和名称索引，供分析收集与 LSP 查询复用。
 * @note scopeStack 表示构建中的当前路径，allScopes 保留退出后的作用域；
 * 名称索引只保存符号指针，符号实体仍由各作用域拥有。
 */
typedef struct SZrSymbolTable {
    SZrState *state;
    SZrSymbolScope *globalScope;      // 全局作用域
    SZrArray scopeStack;               // 作用域栈（用于构建时）
    SZrArray allScopes;                // 所有已创建作用域（SZrSymbolScope*）
    SZrObject *nameToSymbolsMap;      // 名称到符号数组的映射对象（使用 nodeMap 存储）
    SZrHashSet nameToSymbolsHashSet;  // 名称到符号的哈希表（用于快速查找）
    TZrBool useHashTable;                // 是否使用哈希表（默认使用 Object）
} SZrSymbolTable;

/**
 * @brief 为一次语义分析建立全局作用域和名称索引。
 * @return 成功时返回由调用方持有的表；失败时返回 ZR_NULL。
 * BUG: globalScope 分配失败时实现只释放表本体，先前初始化的两个数组和哈希表未析构；
 * 低内存路径会泄漏原生存储，见 symbol_table.c 的初始化与失败分支。
 */
ZR_LANGUAGE_SERVER_API SZrSymbolTable *ZrLanguageServer_SymbolTable_New(SZrState *state);

/** @brief 释放表内全部作用域与符号；先释放仍借用这些符号的分析查询结果。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SymbolTable_Free(SZrState *state, SZrSymbolTable *table);

/**
 * @brief 将一个声明投影到当前作用域及名称索引，供补全、悬停和位置查询使用。
 * @pre 非全局声明先用 EnterScope 建好所属词法作用域；name 与 astNode 在分析期保持有效。
 * @note 可选 outSymbol 返回表持有的借用指针；typeInfo 会复制到新符号。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SymbolTable_AddSymbolEx(SZrState *state, SZrSymbolTable *table,
                                                      EZrSymbolType type, SZrString *name,
                                                      SZrFileRange location,
                                                      SZrInferredType *typeInfo,
                                                      EZrAccessModifier accessModifier,
                                                      SZrAstNode *astNode,
                                                      SZrSymbol **outSymbol);

/**
 * @brief 从指定作用域向父作用域查找同名声明，供收集期解析可见符号。
 * @note scope 为空时使用当前构建作用域；同名重载只返回首个，完整候选用 LookupAll。
 */
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_SymbolTable_Lookup(SZrSymbolTable *table, SZrString *name, 
                                                       SZrSymbolScope *scope);
/**
 * @brief 结合源位置和作用域选择同名符号，供文档查询避免把局部声明误配为全局声明。
 * @note 返回表持有的借用指针；若位置携带 source，应与声明所在源文件相符。
 */
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_SymbolTable_LookupAtPosition(SZrSymbolTable *table,
                                                                                 SZrString *name,
                                                                                 SZrFileRange position);

/**
 * @brief 收集当前位置可见的同名候选，供重载与调用签名分析。
 * @pre result 是零初始化的数组，或 elementSize 为 sizeof(SZrSymbol *) 的有效数组。
 * @note 无效数组会先按符号指针类型初始化；有效数组保留原元素并追加。元素仍由符号表拥有，调用方负责释放数组容器。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_SymbolTable_LookupAll(SZrState *state, SZrSymbolTable *table, 
                                                       SZrString *name, SZrSymbolScope *scope,
                                                       SZrArray *result);

/** @brief 在已收集的声明范围内选择匹配位置的符号；返回表持有的借用指针。 */
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_SymbolTable_FindDefinition(SZrSymbolTable *table, 
                                                                SZrFileRange position);

/**
 * @brief 将 parser 的稳定 SymbolId 映射回已投影的 LSP 符号，保持导航与规范查询同一身份。
 * @note 无效 ID 或未投影的声明返回 ZR_NULL；结果仍由表拥有。
 */
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_SymbolTable_FindBySemanticId(
        SZrSymbolTable *table,
        TZrSymbolId semanticId);

/**
 * @brief 收集 AST 声明前压入词法作用域；完成该子树后须配对 ExitScope。
 * @note 作用域即使出栈仍保留在 allScopes，以支持稍后的按位置查询。
 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SymbolTable_EnterScope(SZrState *state, SZrSymbolTable *table, 
                                                      SZrFileRange range, TZrBool isFunctionScope,
                                                      TZrBool isClassScope, TZrBool isStructScope);

/** @brief 结束当前收集路径；全局作用域始终保留在栈底。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_SymbolTable_ExitScope(SZrSymbolTable *table);

/** @brief 返回收集路径的栈顶作用域；空栈时回退到全局作用域。 */
ZR_LANGUAGE_SERVER_API SZrSymbolScope *ZrLanguageServer_SymbolTable_GetCurrentScope(SZrSymbolTable *table);

/**
 * @brief 为声明创建独立符号并复制可选类型；调用方可直接持有或交给符号表。
 * @note 成功后需由 Symbol_Free 或拥有它的符号表释放；name 和 astNode 不复制。
 */
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_Symbol_New(SZrState *state, EZrSymbolType type, 
                                                SZrString *name, SZrFileRange location,
                                                SZrInferredType *typeInfo,
                                                EZrAccessModifier accessModifier,
                                                SZrAstNode *astNode);

/** @brief 释放符号拥有的类型与引用数组；不释放借用的 AST 和 GC 字符串。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Symbol_Free(SZrState *state, SZrSymbol *symbol);

/** @brief 为展示层符号保留引用位置与计数，通常经 ReferenceTracker_AddReference 调用。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Symbol_AddReference(SZrState *state, SZrSymbol *symbol, 
                                                    SZrFileRange location);

#endif //ZR_VM_LANGUAGE_SERVER_SYMBOL_TABLE_H
