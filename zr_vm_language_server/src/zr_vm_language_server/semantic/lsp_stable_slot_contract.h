#ifndef ZR_VM_LANGUAGE_SERVER_LSP_STABLE_SLOT_CONTRACT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_STABLE_SLOT_CONTRACT_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_parser/compiler.h"

/** 源、弱句柄、带守卫引用对应不同的可显示所有权约束。 */
typedef enum EZrLspStableSlotContractKind {
    ZR_LSP_STABLE_SLOT_CONTRACT_NONE = 0,
    ZR_LSP_STABLE_SLOT_CONTRACT_HANDLE,
    ZR_LSP_STABLE_SLOT_CONTRACT_SOURCE,
    ZR_LSP_STABLE_SLOT_CONTRACT_WRITABLE_REF,
    ZR_LSP_STABLE_SLOT_CONTRACT_READONLY_REF,
} EZrLspStableSlotContractKind;

/** 成员指针借用 prototype；只有 prototype 存活时才可用于 hover 格式化。 */
typedef struct SZrLspStableSlotContract {
    EZrLspStableSlotContractKind kind;
    const SZrTypeMemberInfo *acquireRead;
    const SZrTypeMemberInfo *acquireWrite;
    const SZrTypeMemberInfo *projection;
} SZrLspStableSlotContract;

/** @brief 根据显式协议位和成员角色分类稳定槽契约，避免仅靠名称推断。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspStableSlotContract_Classify(
        const SZrTypePrototypeInfo *prototype,
        SZrLspStableSlotContract *outContract);
/** @brief 向既有类型 hover 追加弱身份或守卫引用限制；要求缓冲区已有终止符。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspStableSlotContract_AppendPrototypeHover(
        const SZrTypePrototypeInfo *prototype,
        TZrChar *buffer,
        TZrSize bufferSize);

#endif /* ZR_VM_LANGUAGE_SERVER_LSP_STABLE_SLOT_CONTRACT_H */
