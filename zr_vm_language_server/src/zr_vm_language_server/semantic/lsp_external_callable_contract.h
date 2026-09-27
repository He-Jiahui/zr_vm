#ifndef ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_CALLABLE_CONTRACT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_EXTERNAL_CALLABLE_CONTRACT_H

#include "metadata/lsp_metadata_provider.h"
#include "zr_vm_parser/canonical_type.h"

/** 外部 callable 的来源；决定签名来自 descriptor 还是 canonical 函数类型。 */
typedef enum EZrLspExternalCallableKind {
    ZR_LSP_EXTERNAL_CALLABLE_NONE = 0,
    ZR_LSP_EXTERNAL_CALLABLE_FUNCTION = 1,
    ZR_LSP_EXTERNAL_CALLABLE_METHOD = 2
} EZrLspExternalCallableKind;

/**
 * 借用元数据 descriptor 与 parser canonicalContext 的短期视图。
 * 元数据提供者和语义快照释放后不得继续格式化此结构。
 */
typedef struct SZrLspExternalCallableContract {
    EZrLspExternalCallableKind kind;
    const TZrChar *name;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    const ZrLibParameterDescriptor *parameters;
    TZrSize parameterCount;
    const ZrLibGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    const SZrSemanticContext *canonicalContext;
    const SZrCanonicalTypeNode *canonicalFunctionType;
} SZrLspExternalCallableContract;

/** @brief 为外部普通函数建立签名视图，供悬停和签名帮助共享。 */
TZrBool ZrLanguageServer_LspExternalCallableContract_FromResolvedMember(
        const SZrLspResolvedMetadataMember *member,
        SZrLspExternalCallableContract *contract);
/**
 * @brief 将外部方法 descriptor 与当前 canonical 接收者及参数契约对齐。
 * @pre callableTypeId 必须属于 canonicalContext；不支持的 effect/泛型约束会拒绝格式化。
 */
TZrBool ZrLanguageServer_LspExternalCallableContract_FromResolvedMethod(
        const SZrLspResolvedMetadataMember *member,
        const SZrSemanticContext *canonicalContext,
        TZrTypeId callableTypeId,
        SZrLspExternalCallableContract *contract);
/** @brief 将已验证的借用签名视图写入调用方缓冲区，容量不足返回失败。 */
TZrBool ZrLanguageServer_LspExternalCallableContract_Format(
        const SZrLspExternalCallableContract *contract,
        TZrChar *buffer,
        TZrSize bufferSize);
/** @brief 为签名帮助生成与完整签名一致的单个参数标签。 */
TZrBool ZrLanguageServer_LspExternalCallableContract_FormatParameter(
        const SZrLspExternalCallableContract *contract,
        TZrSize index,
        TZrChar *buffer,
        TZrSize bufferSize);

#endif
