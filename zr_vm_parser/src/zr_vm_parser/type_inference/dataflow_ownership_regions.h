#ifndef ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_REGIONS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_DATAFLOW_OWNERSHIP_REGIONS_H

#include "zr_vm_parser/semantic_facts.h"

/**
 * @brief 保存一个借用别名与其 owner 的区域绑定事实。
 *
 * @details aliasReference 指向声明或重绑定写入，ownerReference 指向构造目标的 READ；
 * qualifier 记录绑定后的 borrowed、loaned 或 weak 限定，isDeclaration 区分声明与赋值。
 * constructNode 和两个事实指针均借用语义 context 与 AST 的存储，不转移所有权。
 */
typedef struct SZrDataflowOwnershipRegionBinding {
    SZrAstNode *constructNode;
    const SZrSemanticReferenceFact *aliasReference;
    const SZrSemanticReferenceFact *ownerReference;
    EZrOwnershipQualifier qualifier;
    TZrBool isDeclaration;
} SZrDataflowOwnershipRegionBinding;

/**
 * @brief 从变量声明或简单赋值提取别名与 owner 的区域绑定。
 *
 * @note context、statement 或 outBinding 为空时返回 false；outBinding 非空时先清零。
 * @note 支持 borrow、loan construct 以及 degrade intrinsic；只接受简单等号赋值。
 *       若语句形状不支持或两侧引用事实未解析，返回 false 且输出保持清零。
 *       返回的 AST 节点和引用事实指针由输入 AST/context 持有，调用方不得释放。
 * @return 两侧均解析成功时写入 qualifier 与引用事实并返回 true。
 */
TZrBool ZrParser_DataflowOwnership_StatementRegionBinding(
        const SZrSemanticContext *context,
        SZrAstNode *statement,
        SZrDataflowOwnershipRegionBinding *outBinding);
/**
 * @brief 查找 borrow/loan/degrade construct target 对应的已解析 READ 事实。
 *
 * @note context 或 constructNode 为空、constructNode 不是 construct expression，或引用事实无效时返回 NULL。
 * @note 结果指针借用自 context 的 referenceFacts；函数只读事实，不更改生命周期状态。
 * @return 找到同节点或同源范围内的有效 READ 事实时返回其指针，否则返回 NULL。
 */
const SZrSemanticReferenceFact *ZrParser_DataflowOwnership_ConstructTargetRead(
        const SZrSemanticContext *context,
        SZrAstNode *constructNode);
/**
 * @brief 判断语句是否通过 DROP 或 using 资源范围释放给定 READ。
 *
 * @note statement/fact 为空或 READ 不落入识别范围时返回 false；调用方按已解析 READ 事实调用。
 * @note 普通表达式识别根部 DROP 与简单等号赋值右侧；using 单独匹配 resource 子树。
 *       该查询只返回分类结果，不修改 ownership state；复合表达式中的嵌套 DROP 尚未遍历。
 * @return 目标 READ 属于当前可识别的释放表达式或 using resource 时返回 true。
 */
TZrBool ZrParser_DataflowOwnership_StatementReleasesRead(
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact);

#endif
