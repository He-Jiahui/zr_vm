#ifndef ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_TRANSACTION_H
#define ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_TRANSACTION_H

#include "compiler_internal.h"
#include "compile_time_declaration_patch_attributes.h"
#include "compile_time_declaration_patch_interfaces.h"
#include "zr_vm_parser/declaration_transform_contract.h"

/**
 * @brief 生成字段兼容入口的单阶段 observer。
 * @return 返回 true 继续；返回 false 拒绝当前事务。
 * @note 仅在 generated 阶段、目标状态发布前同步调用；参数是已准备数量，不表示目标已可见；
 *       userData 只借用到回调返回，回调自身副作用由调用方负责。
 */
typedef TZrBool (*FZrParserDeclarationPatchCommitObserver)(
        TZrSize committedAdditionCount,
        TZrPtr userData);

/**
 * @brief 统一声明补丁事务的准备阶段标签。
 * @note 顺序为生成成员、接口列表、属性 metadata/decorator；非空阶段在目标发布前通知。
 */
typedef enum EZrParserDeclarationPatchCommitStage {
    ZR_PARSER_DECLARATION_PATCH_COMMIT_GENERATED = 1,
    ZR_PARSER_DECLARATION_PATCH_COMMIT_INTERFACES = 2,
    ZR_PARSER_DECLARATION_PATCH_COMMIT_ATTRIBUTES = 3
} EZrParserDeclarationPatchCommitStage;

/**
 * @brief 接收一个非空事务阶段及其已准备项数的 observer。
 * @return 返回 true 继续后续准备；返回 false 中止发布并清理 detached 状态。
 * @note callback 与 userData 仅在同步调用期间借用；count 表示待发布的已准备项。
 */
typedef TZrBool (*FZrParserDeclarationPatchTransactionObserver)(
        EZrParserDeclarationPatchCommitStage stage,
        TZrSize committedCount,
        TZrPtr userData);

/**
 * @brief 通过统一事务提交一批生成字段，保留历史单阶段 observer 接口。
 * @pre cs 与 targetInfo 非空，受影响的 targetInfo 数组已初始化；additionCount 非零时
 *      additions 与 canonicalTypeNames 按索引一一对应；其引用及 observer/userData 在同步
 *      调用返回前保持有效。observer 只用于检查/否决，不得改写目标或 compiler state、重入事务。
 * @return 空批次或成功发布返回 true；准备/分配失败或 observer 拒绝返回 false。
 * @note 此入口不添加接口、属性或 transform decorator；单批追加数不得超过
 *       ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS。observer 仅在非空 generated 阶段、
 *       目标状态发布之前调用。
 */
ZR_PARSER_API TZrBool ZrParser_CompileTime_CommitGeneratedFieldsAtomic(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *targetInfo,
        const SZrParserGeneratedDeclaration *additions,
        SZrString *const *canonicalTypeNames,
        TZrSize additionCount,
        TZrSymbolId originTargetSymbolId,
        SZrFileRange location,
        FZrParserDeclarationPatchCommitObserver observer,
        TZrPtr observerUserData);

/**
 * @brief 原子准备并发布 generated members/symbols、接口关系、属性 metadata 与 decorator。
 * @pre cs、targetInfo、interfaceAdds、attributeAdds 非空；受影响的 targetInfo arrays 已初始化；
 *      非零 additionCount 要求 additions 与 canonicalTypeNames 有效且逐项配对。生产调用方
 *      应传入经对应 Prepare helper 规范化的 aggregates；所有借用数组、schema/name 字符串及
 *      target/compiler state 保持有效到返回。observer 只用于检查/否决，不得改写目标或 compiler
 *      state、重入事务。
 * @return 空补丁或全部准备并发布成功返回 true；参数/预算/分配失败或 observer veto 返回 false。
 * @note generated、interface 和 attribute 输入批次各自最多追加
 *       ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS 项。阶段 observer 在发布前同步收到已准备
 *       的数量；任何准备失败或 veto 都不执行本函数的发布尾段。observer 自身的外部副作用
 *       不由事务回滚。
 */
ZR_PARSER_API TZrBool ZrParser_CompileTime_CommitDeclarationPatchAtomic(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *targetInfo,
        const SZrParserGeneratedDeclaration *additions,
        SZrString *const *canonicalTypeNames,
        TZrSize additionCount,
        const SZrParserCompileTimePatchInterfaceAdds *interfaceAdds,
        const SZrParserCompileTimePatchAttributeAdds *attributeAdds,
        SZrString *transformDecoratorName,
        TZrSymbolId originTargetSymbolId,
        SZrFileRange location,
        FZrParserDeclarationPatchTransactionObserver observer,
        TZrPtr observerUserData);

#endif // ZR_VM_PARSER_COMPILE_TIME_DECLARATION_PATCH_TRANSACTION_H
