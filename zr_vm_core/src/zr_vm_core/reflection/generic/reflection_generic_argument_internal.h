/**
 * @file
 * @brief 声明泛型实参树的验证和元数据签名匹配接口。
 */
#ifndef ZR_VM_REFLECTION_GENERIC_ARGUMENT_INTERNAL_H
#define ZR_VM_REFLECTION_GENERIC_ARGUMENT_INTERNAL_H

#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/reflection.h"

/**
 * @brief 在查询 MethodSpec 或 TypeSpec 前验证借用的泛型类型实参树。
 * @pre 非空 argument 的子节点须在本次递归中可读；含类型 token 的节点需要有效 runtime。
 * @return 输入为空、结构无效或超过递归上限时返回 false。
 * @note depth 由外层查询从零传入，递归上限由反射协议常量约束。
 */
TZrBool ZrCore_Reflection_ValidateGenericTypeArgument(
        SZrMetadataRuntime *runtime,
        const SZrReflectionGenericTypeArgument *argument,
        TZrUInt32 depth);

/**
 * @brief 将已验证的请求实参与候选元数据签名节点逐层比较。
 * @pre candidateBlob、candidateNode、argument 非空且有效；调用方先验证实参数组。
 * @note resolvedCandidateToken 非零时直接用已解析 token 比较类型身份；其余节点按签名树递归匹配。
 */
TZrBool ZrCore_Reflection_GenericTypeArgumentMatchesSignatureNode(
        SZrMetadataRuntime *runtime,
        const SZrZrpMetadataPoolSliceView *candidateBlob,
        const SZrMetadataRuntimeSignatureTypeNodeView *candidateNode,
        TZrMetadataToken resolvedCandidateToken,
        const SZrReflectionGenericTypeArgument *argument,
        TZrUInt32 depth);

#endif
