#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SCHEDULER_CONTRACT_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SCHEDULER_CONTRACT_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/function.h"

/** 已核验的调度器 artifact 投影；所有值为拷贝，独立于输入字节存活。 */
typedef struct SZrLspSchedulerContract {
    TZrUInt32 receiverTypeId;
    TZrMetadataToken schedulerTypeToken;
    TZrMetadataToken taskTypeToken;
    TZrMetadataToken jobTypeToken;
    TZrMetadataToken scheduleSignatureToken;
    TZrUInt32 abiVersion;
    TZrUInt32 policyMask;
    TZrUInt32 attachedRequirementFlags;
    TZrUInt32 isolatedRequirementFlags;
    TZrUInt32 ownerLayoutVersion;
    TZrUInt64 ownerLayoutHash;
    TZrUInt64 ownerModuleHash;
    TZrUInt64 transportContractHash;
    TZrUInt64 schedulerContractHash;
} SZrLspSchedulerContract;

/**
 * @brief 对照源码事实和 canonical artifact 校验调度器 ABI，再输出编辑器可用的契约。
 * @pre artifactBytes 在本次调用期间有效；失败时 outContract 被清零。
 * @note 当前仓内直接消费者主要是契约测试，接入用户可见流程时需复查快照来源。
 * TODO: 接入真实 LSP 请求前，确认 sourceFact 与 artifactBytes 来自同一已发布构建代；当前测试只传入配对样本。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspSchedulerContract_ResolveArtifact(
        const SZrFunctionSchedulerSourceFact *sourceFact,
        const TZrByte *artifactBytes,
        TZrSize artifactLength,
        SZrLspSchedulerContract *outContract,
        SZrArtifactDiagnostic *outDiagnostic);

#endif /* ZR_VM_LANGUAGE_SERVER_LSP_SCHEDULER_CONTRACT_H */
