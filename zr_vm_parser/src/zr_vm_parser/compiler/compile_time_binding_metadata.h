#ifndef ZR_VM_PARSER_COMPILE_TIME_BINDING_METADATA_H
#define ZR_VM_PARSER_COMPILE_TIME_BINDING_METADATA_H

#include "compiler_internal.h"

/**
 * @brief 将来源变量 initializer 与其可序列化路径输出配对，供 resolver 延迟递归求值。
 * @note name/value 借用当前 AST/编译器快照；info 由调用方分配，解析器只写入其中的 pathBindings。
 * @note isResolving 是递归环检测状态，isResolved 才表示本次解析结果已完成；两者不可混作失败标志。
 */
typedef struct SZrCompileTimeBindingSourceVariable {
    SZrString *name;
    SZrAstNode *value;
    SZrFunctionCompileTimeVariableInfo *info;
    TZrBool isResolving;
    TZrBool isResolved;
} SZrCompileTimeBindingSourceVariable;

/**
 * @brief 为绑定图收集器提供本次扫描的分配状态和按名查询适配器。
 * @pre 若提供查找 callbacks，它们的 userData 与 resolver 须活到 ResolveAll 返回；回调返回的 AST/metadata 由外部拥有。
 * @note function 查询先于 variable 查询，确保同名位置按 compile-time function identity 优先分类。
 */
typedef struct SZrCompileTimeBindingResolver {
    SZrState *state;
    TZrPtr userData;
    SZrCompileTimeBindingSourceVariable *(*findVariable)(TZrPtr userData, SZrString *name);
    SZrCompileTimeFunction *(*findFunction)(TZrPtr userData, SZrString *name);
} SZrCompileTimeBindingResolver;

/**
 * @brief 解析连续来源变量数组，并把路径到 compile-time function 的结果发布到每个 info。
 * @pre resolver、variables 与 state 在整个同步调用中有效；非空 callbacks 同期有效，结果字符串由 state 管理。
 * @return 任一收集或分配失败返回 false；不改变 AST、导入模块或函数注册表。
 */
TZrBool ZrParser_CompileTimeBinding_ResolveAll(SZrCompileTimeBindingResolver *resolver,
                                               SZrCompileTimeBindingSourceVariable *variables,
                                               TZrSize variableCount);

/**
 * @brief 把 AST member 子区间转换为 ResolveAll/FindPath 共用的静态点路径键。
 * @pre 被选 member 必须是非 computed identifier property；endIndex 可超过数组长度并会截断。
 * @return 成功时 outPath 接收 state 管理的新字符串；无 member 区间返回空键。
 */
TZrBool ZrParser_CompileTimeBinding_BuildStaticMemberPath(SZrState *state,
                                                          const SZrAstNodeArray *members,
                                                          TZrSize startIndex,
                                                          TZrSize endIndex,
                                                          SZrString **outPath);

/**
 * @brief 按路径键查询 compile-time variable info 中的函数绑定。
 * @return 返回 info 所有的只读记录或 null；返回指针随 info 生命周期失效。
 */
const SZrFunctionCompileTimePathBinding *ZrParser_CompileTimeBinding_FindPath(
        const SZrFunctionCompileTimeVariableInfo *info,
        SZrString *path);

#endif
