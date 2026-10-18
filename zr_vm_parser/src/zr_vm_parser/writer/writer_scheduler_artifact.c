#include "zr_vm_parser/writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/metadata_token.h"

typedef struct SZrWriterSchedulerArtifactType {
    const SZrFunctionArtifactSourceTypeIdentity *provider;
    TZrMetadataToken serializedToken;
    TZrUInt32 canonicalTypeId;
} SZrWriterSchedulerArtifactType;

static EZrArtifactStatus writer_scheduler_artifact_fail(
        SZrArtifactDiagnostic *diagnostic,
        EZrArtifactStatus status) {
    if (diagnostic != ZR_NULL) {
        ZrCore_Memory_RawSet(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = status;
    }
    return status;
}

static TZrBool writer_scheduler_artifact_provider_is_complete(
        const SZrFunctionArtifactSourceTypeIdentity *provider) {
    return provider != ZR_NULL && provider->metadataToken != 0u &&
           provider->signatureToken != 0u && provider->signatureHash != 0u &&
           provider->layoutVersion != 0u && provider->layoutHash != 0u &&
           provider->moduleSignatureHash != 0u &&
           ZR_METADATA_TOKEN_TABLE(provider->metadataToken) == ZR_METADATA_TABLE_TYPE_DEF &&
           ZR_METADATA_TOKEN_TABLE(provider->signatureToken) == ZR_METADATA_TABLE_SIGNATURE;
}

static void writer_scheduler_artifact_write_u32(TZrByte *bytes, TZrUInt32 value) {
    bytes[0] = (TZrByte)(value & 0xffu);
    bytes[1] = (TZrByte)((value >> 8u) & 0xffu);
    bytes[2] = (TZrByte)((value >> 16u) & 0xffu);
    bytes[3] = (TZrByte)((value >> 24u) & 0xffu);
}

static void writer_scheduler_artifact_write_type_def_signature(
        TZrByte *bytes,
        TZrMetadataToken typeDefToken) {
    bytes[0] = (TZrByte)ZR_ARTIFACT_SIGNATURE_NODE_TYPE_DEF;
    writer_scheduler_artifact_write_u32(bytes + 1u, typeDefToken);
}

static void writer_scheduler_artifact_sort_types(
        SZrWriterSchedulerArtifactType *types,
        TZrUInt32 typeCount) {
    TZrUInt32 outer;

    for (outer = 0u; outer < typeCount; ++outer) {
        TZrUInt32 inner;
        for (inner = outer + 1u; inner < typeCount; ++inner) {
            if (types[inner].serializedToken < types[outer].serializedToken) {
                SZrWriterSchedulerArtifactType temporary = types[outer];
                types[outer] = types[inner];
                types[inner] = temporary;
            }
        }
    }
}

static TZrBool writer_scheduler_artifact_types_are_distinct(
        const SZrWriterSchedulerArtifactType *types,
        TZrUInt32 typeCount) {
    TZrUInt32 index;

    for (index = 1u; index < typeCount; ++index) {
        if (types[index - 1u].serializedToken == types[index].serializedToken) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

ZR_PARSER_API EZrArtifactStatus ZrParser_Writer_WriteSchedulerArtifactFile(
        SZrState *state,
        const SZrFunction *function,
        const TZrChar *filename,
        EZrArtifactKind kind,
        SZrArtifactDiagnostic *diagnostic) {
    const SZrFunctionSchedulerSourceFact *fact;
    SZrWriterSchedulerArtifactType types[3];
    SZrArtifactTypeDefRow typeDefs[3];
    SZrArtifactTypeIdentityRow typeRefs[3];
    SZrArtifactTypeIdentityRow typeSpec;
    SZrArtifactContractRow contract;
    SZrArtifactLayoutRow layout;
    SZrArtifactDomainTransferRow transfer;
    SZrArtifactSchedulerContractRow schedulerContract;
    SZrArtifactSectionInput sections[8];
    SZrArtifactDocument document;
    TZrByte signatureHeap[20];
    TZrByte *encoded = ZR_NULL;
    TZrSize encodedSize = 0u;
    TZrSize writtenSize = 0u;
    FILE *file = ZR_NULL;
    TZrBool writeFailed = ZR_FALSE;
    EZrArtifactStatus status;
    TZrUInt32 index;
    TZrUInt32 schedulerSignatureOffset = 0u;
    TZrUInt64 schedulerSignatureHash;

    if (state == ZR_NULL || state->global == ZR_NULL || function == ZR_NULL ||
        filename == ZR_NULL || (kind != ZR_ARTIFACT_KIND_ZRI && kind != ZR_ARTIFACT_KIND_ZRO)) {
        return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT);
    }
    fact = ZrCore_Function_FindSchedulerSourceFact(function, 0u);
    if (fact == ZR_NULL || fact->schedulerTypeId == 0u || fact->taskTypeId == 0u ||
        fact->jobTypeId == 0u || fact->scheduleMemberToken == 0u ||
        fact->scheduleSignatureToken == 0u || fact->scheduleSignatureHash == 0u ||
        fact->schedulerAbiVersion == 0u || fact->schedulerPolicyMask == 0u ||
        fact->transportContractHash == 0u || fact->schedulerContractHash == 0u ||
        !writer_scheduler_artifact_provider_is_complete(&fact->schedulerProvider) ||
        !writer_scheduler_artifact_provider_is_complete(&fact->taskProvider) ||
        !writer_scheduler_artifact_provider_is_complete(&fact->jobProvider)) {
        return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT);
    }

    ZrCore_Memory_RawSet(types, 0, sizeof(types));
    types[0].provider = &fact->schedulerProvider;
    types[0].canonicalTypeId = fact->schedulerTypeId;
    types[1].provider = &fact->taskProvider;
    types[1].canonicalTypeId = fact->taskTypeId;
    types[2].provider = &fact->jobProvider;
    types[2].canonicalTypeId = fact->jobTypeId;
    for (index = 0u; index < 3u; ++index) {
        types[index].serializedToken = ZR_METADATA_TOKEN_MAKE(
                ZR_METADATA_TABLE_TYPE_REF, index + 1u);
    }
    writer_scheduler_artifact_sort_types(types, 3u);
    if (!writer_scheduler_artifact_types_are_distinct(types, 3u)) {
        return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT);
    }

    ZrCore_Memory_RawSet(signatureHeap, 0, sizeof(signatureHeap));
    ZrCore_Memory_RawSet(typeDefs, 0, sizeof(typeDefs));
    ZrCore_Memory_RawSet(typeRefs, 0, sizeof(typeRefs));
    for (index = 0u; index < 3u; ++index) {
        TZrUInt32 offset = index * 5u;
        TZrUInt64 signatureHash;

        writer_scheduler_artifact_write_type_def_signature(
                signatureHeap + offset, types[index].provider->metadataToken);
        signatureHash = ZrCore_Artifact_HashBytes(signatureHeap + offset, 5u);
        typeDefs[index].token = types[index].provider->metadataToken;
        typeDefs[index].canonicalTypeId = types[index].canonicalTypeId;
        typeDefs[index].typeSignatureHash = signatureHash;
        typeRefs[index].token = types[index].serializedToken;
        typeRefs[index].signatureToken = types[index].provider == &fact->schedulerProvider
                                                 ? fact->scheduleSignatureToken
                                                 : types[index].provider->signatureToken;
        typeRefs[index].canonicalTypeId = types[index].canonicalTypeId;
        typeRefs[index].signatureOffset = offset;
        typeRefs[index].signatureLength = 5u;
        typeRefs[index].signatureHash = signatureHash;
        typeRefs[index].layoutVersion = types[index].provider->layoutVersion;
        typeRefs[index].layoutHash = types[index].provider->layoutHash;
        if (types[index].provider == &fact->schedulerProvider) {
            schedulerSignatureOffset = offset;
            schedulerSignatureHash = signatureHash;
        }
    }
    writer_scheduler_artifact_write_type_def_signature(
            signatureHeap + 15u, fact->schedulerProvider.metadataToken);
    ZrCore_Memory_RawSet(&typeSpec, 0, sizeof(typeSpec));
    typeSpec.token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u);
    typeSpec.signatureToken = fact->scheduleSignatureToken;
    typeSpec.canonicalTypeId = fact->schedulerTypeId;
    typeSpec.signatureOffset = 15u;
    typeSpec.signatureLength = 5u;
    typeSpec.signatureHash = ZrCore_Artifact_HashBytes(signatureHeap + 15u, 5u);
    typeSpec.layoutVersion = fact->schedulerProvider.layoutVersion;
    typeSpec.layoutHash = fact->schedulerProvider.layoutHash;

    ZrCore_Memory_RawSet(&contract, 0, sizeof(contract));
    contract.memberToken = fact->scheduleMemberToken;
    contract.signatureToken = fact->scheduleSignatureToken;
    contract.parameterCount = 1u;
    contract.receiverEffect = ZR_ARTIFACT_RECEIVER_MUTABLE;
    contract.abiLoweringKind = ZR_ARTIFACT_ABI_LOWERING_NATIVE_DIRECT;
    contract.contractHash = fact->schedulerContractHash;

    ZrCore_Memory_RawSet(&layout, 0, sizeof(layout));
    layout.typeToken = types[0].provider == &fact->schedulerProvider
                               ? types[0].serializedToken
                               : 0u;
    for (index = 0u; index < 3u; ++index) {
        if (types[index].provider == &fact->schedulerProvider) {
            layout.typeToken = types[index].serializedToken;
            break;
        }
    }
    layout.version = fact->schedulerProvider.layoutVersion;
    layout.byteAlignment = 1u;
    layout.layoutHash = fact->schedulerProvider.layoutHash;

    ZrCore_Memory_RawSet(&transfer, 0, sizeof(transfer));
    for (index = 0u; index < 3u; ++index) {
        if (types[index].provider == &fact->jobProvider) {
            transfer.typeToken = types[index].serializedToken;
            break;
        }
    }
    transfer.kind = ZR_ARTIFACT_DOMAIN_TRANSFER_RESOURCE_MOVE;
    transfer.schemaVersion = 1u;
    transfer.flags = ZR_ARTIFACT_DOMAIN_TRANSFER_FLAG_DROP_ON_FAILURE;
    transfer.providerToken = fact->scheduleMemberToken;
    transfer.schemaHash = fact->jobProvider.signatureHash;
    transfer.providerContractHash = fact->schedulerContractHash;

    ZrCore_Memory_RawSet(&schedulerContract, 0, sizeof(schedulerContract));
    for (index = 0u; index < 3u; ++index) {
        if (types[index].provider == &fact->schedulerProvider) {
            schedulerContract.schedulerTypeToken = types[index].serializedToken;
        } else if (types[index].provider == &fact->taskProvider) {
            schedulerContract.taskTypeToken = types[index].serializedToken;
        } else if (types[index].provider == &fact->jobProvider) {
            schedulerContract.jobTypeToken = types[index].serializedToken;
        }
    }
    schedulerContract.abiVersion = fact->schedulerAbiVersion;
    schedulerContract.policyMask = fact->schedulerPolicyMask;
    schedulerContract.attachedRequirementFlags = fact->attachedRequirementFlags;
    schedulerContract.isolatedRequirementFlags = fact->isolatedRequirementFlags;
    schedulerContract.transportContractHash = fact->transportContractHash;
    schedulerContract.schedulerContractHash = fact->schedulerContractHash;

    ZrCore_Memory_RawSet(&document, 0, sizeof(document));
    ZrCore_Memory_RawSet(sections, 0, sizeof(sections));
    document.kind = kind;
    document.identity.canonicalTypeId = fact->schedulerTypeId;
    document.identity.typeRefToken = layout.typeToken;
    document.identity.typeSpecToken = typeSpec.token;
    document.identity.signatureToken = fact->scheduleSignatureToken;
    document.identity.typeRefHash = schedulerSignatureHash;
    document.identity.typeSpecHash = typeSpec.signatureHash;
    document.identity.signatureHash = schedulerSignatureHash;
    document.identity.layoutVersion = fact->schedulerProvider.layoutVersion;
    document.identity.layoutHash = fact->schedulerProvider.layoutHash;
    document.identity.callableContractHash = fact->schedulerContractHash;
    document.identity.moduleHash = fact->schedulerProvider.moduleSignatureHash;
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 3u, typeDefs};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_REF_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 3u, typeRefs};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &typeSpec};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_SIGNATURE_HEAP, ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            (TZrUInt32)sizeof(signatureHeap), signatureHeap};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_CONTRACT_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &contract};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_LAYOUT_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &layout};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE, ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &transfer};
    sections[document.sectionCount++] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY, 1u, &schedulerContract};
    document.sections = sections;

    status = ZrCore_Artifact_GetEncodedSize(&document, &encodedSize, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK || encodedSize == 0u) {
        return status;
    }
    encoded = (TZrByte *)malloc(encodedSize);
    if (encoded == ZR_NULL) {
        return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_BUFFER_TOO_SMALL);
    }
    status = ZrCore_Artifact_Write(&document, encoded, encodedSize, &writtenSize, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK || writtenSize != encodedSize) {
        free(encoded);
        return status != ZR_ARTIFACT_STATUS_OK
                       ? status
                       : writer_scheduler_artifact_fail(
                               diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION);
    }
    file = fopen(filename, "wb");
    if (file == ZR_NULL) {
        free(encoded);
        return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT);
    }
    if (fwrite(encoded, 1u, writtenSize, file) != writtenSize) {
        writeFailed = ZR_TRUE;
    }
    if (fclose(file) != 0) {
        writeFailed = ZR_TRUE;
    }
    if (writeFailed) {
        remove(filename);
        free(encoded);
        return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_SECTION);
    }
    free(encoded);
    (void)schedulerSignatureOffset;
    return writer_scheduler_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_OK);
}

/**
 * @struct SZrWriterSchedulerArtifactType
 *
 * ## Summary
 * 保存一次调度器产物组装中的提供者身份、本地产物 token 与 canonical 类型 ID 的对应关系。 从函数的首条 scheduler source fact 建立三类临时行，并在编码前排序时使用。
 *
 * ## Fields
 * provider：借用 fact 内嵌的 scheduler、task 或 job 提供者身份；不是新分配的记录。
 * serializedToken：本次产物的 TYPE_REF 表 token，当前入口按 1、2、3 分配；不同于提供者的 TYPE_DEF token。
 * canonicalTypeId：从同一类 source fact 复制的 canonical 类型 ID，随整行交换保持配对。
 *
 * ## Value model
 * 自动存储期的内部关联行；排序复制字段值，不取得提供者或函数的所有权。
 *
 * ## Invariants
 * 三个字段表示同一逻辑类型。比较与去重依据 serializedToken，不能据此证明 provider token 或 canonical ID 全局唯一。
 *
 * ## Constraints and lifecycle
 * provider 所属 function 及其 source fact 数组必须在同步编码期间保持有效且不被重分配；该行不能逃逸为持久 provider 指针。此结构无初始化、释放或线程同步服务。
 *
 * ## References
 * types（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:87）：本入口的三行自动数组。
 * schedulerProvider（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:125）：提供者借用来源。
 * writer_scheduler_artifact_sort_types（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:135）：排序整行以保留配对。
 * SZrFunctionArtifactSourceTypeIdentity（zr_vm_core/include/zr_vm_core/function.h:302）：被借用的身份字段。
 * ZrCore_Function_Free（zr_vm_core/src/zr_vm_core/function/function.c:1474）：所属 fact 数组的释放路径。
 */

/**
 * @fn static EZrArtifactStatus writer_scheduler_artifact_fail(SZrArtifactDiagnostic *diagnostic, EZrArtifactStatus status)
 *
 * ## Summary
 * 把本层状态返回值与可选诊断统一为只含 status 的结果。 参数、临时分配、文件 I/O 的本层退出以及最终成功返回时调用；下层已有诊断的失败直接传播。
 *
 * ## Parameters
 * @param[out] diagnostic 可空诊断存储；非空时整结构清零后写入 status，不能依赖先前 detail。
 * @param[in] status 本次要报告的 EZrArtifactStatus 值，原值传回。
 *
 * ## Returns
 * @return 原样返回 status；OK 也会清除诊断中原有附加字段。
 *
 * ## Constraints and lifecycle
 * diagnostic 非空时须是独立、完整且可写的对象，不与仍需使用的输入或资源状态重叠。该函数不回收文件或编码缓冲区，调用方先完成必要清理；没有分配、异常转换或内部同步。
 *
 * ## References
 * ZrParser_Writer_WriteSchedulerArtifactFile（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:110）：参数门禁调用。
 * ZrParser_Writer_WriteSchedulerArtifactFile（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:286）：打开失败后先释放编码区。
 * ZrParser_Writer_WriteSchedulerArtifactFile（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:297）：写失败清理后报告。
 * ZrParser_Writer_WriteSchedulerArtifactFile（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:301）：成功清空诊断并返回。
 */

/**
 * @fn static TZrBool writer_scheduler_artifact_provider_is_complete(const SZrFunctionArtifactSourceTypeIdentity *provider)
 *
 * ## Summary
 * 为 source fact 的提供者身份执行本层所需的非零字段与 token 表号门禁。 在建立任何行和编码缓冲区之前，分别核 scheduler、task 与 job provider。
 *
 * ## Parameters
 * @param[in] provider 可空的提供者身份借用；非空时必须指向可读完整对象。
 *
 * ## Returns
 * @return 指针非空、六个被查身份/布局字段均非零且两个 token 表号分别为 TYPE_DEF 与 SIGNATURE 时为 TRUE，否则 FALSE。
 *
 * ## Constraints and lifecycle
 * 只检查字段存在与两个表号；不核 token 的非零 RID、记录是否实际存在、provider 域一致性或所有协议语义。非空指针的可读性由调用方保证。函数只读，无分配、所有权转移或同步。
 *
 * ## References
 * ZrParser_Writer_WriteSchedulerArtifactFile（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:118）：scheduler provider 调用。
 * ZrParser_Writer_WriteSchedulerArtifactFile（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:119）：task provider 调用。
 * ZrParser_Writer_WriteSchedulerArtifactFile（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:120）：job provider 调用。
 * SZrFunctionArtifactSourceTypeIdentity（zr_vm_core/include/zr_vm_core/function.h:302）：六个被查字段所在类型。
 * ZR_METADATA_TOKEN_TABLE（zr_vm_core/include/zr_vm_core/metadata_token.h:175）：表号解码，不提供 RID 验证。
 * ZrCore_Artifact_GetEncodedSize（zr_vm_core/src/zr_vm_core/artifact/artifact_schema.c:621）：后续完整文档校验入口。
 */

/**
 * @fn static void writer_scheduler_artifact_write_u32(TZrByte *bytes, TZrUInt32 value)
 *
 * ## Summary
 * 按产物签名的固定小端字节序写出一个 32 位值，避免依赖宿主整数内存布局。 TYPE_DEF 签名节点写入 token 的四个负载字节时使用。
 *
 * ## Parameters
 * @param[out] bytes 非空且至少四字节可写的目标范围；由上层预留容量。
 * @param[in] value 完整 32 位无符号值，按低字节到高字节编码。
 *
 * ## Returns
 * 无返回值；完成后 bytes[0..4) 是 value 的四字节小端表示。
 *
 * ## Constraints and lifecycle
 * 没有指针或容量检查，也不产生可返回的失败状态。调用者保证目标范围可写且没有并发写者；函数不分配、不保留地址。
 *
 * ## References
 * writer_scheduler_artifact_write_type_def_signature（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:47）：唯一程序调用，将节点后的四字节交给此函数。
 * signatureHeap（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:97）：由外层提供固定二十字节存储。
 */

/**
 * @fn static void writer_scheduler_artifact_write_type_def_signature(TZrByte *bytes, TZrMetadataToken typeDefToken)
 *
 * ## Summary
 * 将提供者 TYPE_DEF token 封装为一个五字节 TYPE_DEF 签名节点。 为三个类型行及额外的 scheduler TypeSpec 构建签名堆时使用。
 *
 * ## Parameters
 * @param[out] bytes 非空且至少五字节可写；首字节为节点种类，后四字节为小端 token。
 * @param[in] typeDefToken 调用方已筛选的提供者 TYPE_DEF 身份；本函数只编码，不自行验证表号或 RID。
 *
 * ## Returns
 * 无返回值；五字节结果就地写入 bytes。
 *
 * ## Constraints and lifecycle
 * 依赖调用者保证容量与 token 来源；不查表、不分配、不保留缓冲区。写入范围不得与正在并行访问的存储发生数据竞争。
 *
 * ## References
 * ZrParser_Writer_WriteSchedulerArtifactFile（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:147）：三个 TypeRef 对应签名调用。
 * ZrParser_Writer_WriteSchedulerArtifactFile（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:168）：scheduler TypeSpec 对应签名调用。
 * writer_scheduler_artifact_write_u32（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:36）：编码负载字节的配套函数。
 */

/**
 * @fn static void writer_scheduler_artifact_sort_types(SZrWriterSchedulerArtifactType *types, TZrUInt32 typeCount)
 *
 * ## Summary
 * 按本地产物 serializedToken 升序排列临时类型行，保持行内提供者与 canonical ID 配对。 生成 TypeDef、TypeRef、布局和 scheduler 行之前，对已赋 token 的内部数组排序。
 *
 * ## Parameters
 * @param[in,out] types 至少 typeCount 个已初始化行的可写范围；每次交换整个结构体。typeCount 为 0 或 1 时不访问数组元素。
 * @param[in] typeCount 可访问行数；当前唯一调用固定为 3。
 *
 * ## Returns
 * 无返回值；数组中的 token 按升序排列，字段集合保留；不承诺稳定排序或消除重复。
 *
 * ## Constraints and lifecycle
 * 双重循环为二次复杂度。调用期间不得并发修改数组，provider 仍是原借用地址；排序不验证 provider 身份，也不分配或释放资源。
 *
 * ## References
 * ZrParser_Writer_WriteSchedulerArtifactFile（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:135）：唯一调用，在 token 分配后排序。
 * temporary（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:59）：整行交换保持关联。
 * writer_scheduler_artifact_types_are_distinct（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:136）：排序后的相邻重复检查。
 */

/**
 * @fn static TZrBool writer_scheduler_artifact_types_are_distinct(const SZrWriterSchedulerArtifactType *types, TZrUInt32 typeCount)
 *
 * ## Summary
 * 检查已经按 serializedToken 排序的临时类型行是否存在相邻重复 token。 排序完成后、任何编码或文件操作之前作为本地身份表的重复门禁。
 *
 * ## Parameters
 * @param[in] types 至少 typeCount 行的可读数组；需要已按 serializedToken 排序，才能由相邻比较判断全体是否重复。
 * @param[in] typeCount 可读行数；0 或 1 直接返回 TRUE 而不访问元素。
 *
 * ## Returns
 * @return 发现任意相邻 token 相等时为 FALSE，否则 TRUE；不比较 provider token 或 canonicalTypeId。
 *
 * ## Constraints and lifecycle
 * 这是排序后前置条件下的本地 token 检查，不是任意未排序输入的全局去重器。只读且不保留地址，不分配、释放或同步。
 *
 * ## References
 * writer_scheduler_artifact_sort_types（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:135）：直接前置排序。
 * ZrParser_Writer_WriteSchedulerArtifactFile（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:136）：唯一程序调用。
 * ZR_METADATA_TOKEN_MAKE（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:132）：当前入口给三行分配不同本地 RID。
 */

/**
 * @fn ZR_PARSER_API EZrArtifactStatus ZrParser_Writer_WriteSchedulerArtifactFile(SZrState *state, const SZrFunction *function, const TZrChar *filename, EZrArtifactKind kind, SZrArtifactDiagnostic *diagnostic)
 *
 * ## Summary
 * 把已编译函数的首条 scheduler source fact 投影为 canonical 调度器产物并写入指定文件。 源码编译已发布 scheduler、task、job 的提供者身份后，为 artifact consumer 生成 ZRI 或 ZRO；本入口不编译源码、不执行调度任务，也不是旧流式 writer。
 *
 * ## Parameters
 * @param[in] state 非空且 global 非空的状态；本入口用作状态有效门禁，不取得所有权。
 * @param[in] function 非空函数及其 source fact 数组，调用期间保持存活、稳定且不被并发改写。只选 FindSchedulerSourceFact(function, 0u) 返回的首条。
 * @param[in] filename 非空、NUL 结尾的宿主路径借用；父目录须已准备，调用者独占目标。wb 打开会创建或截断文件。
 * @param[in] kind 仅接受 ZR_ARTIFACT_KIND_ZRI 或 ZR_ARTIFACT_KIND_ZRO。
 * @param[out] diagnostic 可空诊断对象；本层状态会清零后写 status，下层校验失败保留其诊断内容。
 *
 * ## Returns
 * @return OK 表示文档已编码且 fwrite/fclose 均报告成功。参数、缺少/不完整事实或打开失败返回 INVALID_ARGUMENT；malloc 失败返回 BUFFER_TOO_SMALL；短写、关闭失败或成功编码长度不一致返回 INVALID_SECTION；Core 格式校验失败直接传播其状态。成功不表示任务已执行或文件已持久同步。
 *
 * ## Constraints and lifecycle
 * 类型、提供者、签名及协议信息来自被借用的 fact；入口只做明确列出的字段门禁，不在此核 contractRole 或 schedulerProtocolMask，也不遍历导出全部 facts。三类本地 token 与八个 mandatory section 在栈上组装，经 Core 校验和编码完成后才 fopen。编码区使用宿主 malloc/free，不走 VM 分配器；普通返回路径释放自身资源，文件打开后的路径调用 fclose。目标不是原子替换：wb 已可破坏旧内容，写/关闭失败会尝试 remove，未检查 remove 结果；既可能删除目标，也可能留下残片。无目录创建、备份回滚、文件锁或 fsync；相同路径并发调用及输入/诊断别名由调用者避免。函数只同步借用 function、fact 和路径，不取得其所有权。
 *
 * ## References
 * ZrParser_Writer_WriteSchedulerArtifactFile（zr_vm_parser/include/zr_vm_parser/writer.h:104）：公开声明，仅作配套证据。
 * ZrCore_Function_FindSchedulerSourceFact（zr_vm_core/src/zr_vm_core/function/function.c:1322）：0u 选择首条事实。
 * compiler_scheduler_artifact_record_resolved_call（zr_vm_parser/src/zr_vm_parser/compiler/compiler_scheduler_artifact.c:276）：事实生产入口。
 * ZrCore_Artifact_GetEncodedSize（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:267）：编码前校验。
 * ZrCore_Artifact_Write（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:275）：内存编码。
 * fopen（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:283）：破坏性 wb 打开边界。
 * remove（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:295）：写失败后尽力删除。
 * test_real_source_scheduler_call_writes_and_imports_canonical_artifact（tests/parser/test_artifact_schema_source_roundtrip.c:337）：真实源码正例调用。
 * test_source_without_scheduler_call_rejects_artifact_write（tests/parser/test_artifact_schema_source_roundtrip.c:458）：无事实拒绝调用。
 * test_scheduler_artifact_writer_rejects_unavailable_provider（tests/parser/test_artifact_schema_source_roundtrip.c:494）：提供者缺失字段拒绝调用。
 * test_lsp_scheduler_contract_resolves_source_call_from_canonical_artifact（tests/language_server/test_lsp_interface.c:7358）：LSP consumer 调用。
 */

/**
 * @name ZrParser_Writer_WriteSchedulerArtifactFile::source_fact_identity_sections
 *
 * ## Summary
 * 先固定三类身份对应关系，再用同一批存储构建八个 mandatory section，避免排序后把 provider、canonical ID 与本地 token 拆散。 首条 fact 门禁通过后，编码器借用各节数据之前。
 *
 * ## Constraints and lifecycle
 * 三条 TypeRef 的本地 RID 为 1..3，不冒充 provider TypeDef 身份；三个五字节签名及额外 TypeSpec 共用二十字节签名堆。schedulerTypeToken、taskTypeToken、jobTypeToken 按 provider 地址配对回填；布局采用 alignment 1。RESOURCE_MOVE、DROP_ON_FAILURE、ABI/policy 等字段是序列化契约描述，不在这里执行移动、Drop、Send 检查或 scheduler。sections、行和签名均为本次栈上对象，只能在其有效期内同步交给 Core；不能把 document.sections 保存到调用之外。
 *
 * ## References
 * types（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:125）：先建立 provider 与 canonical ID 配对。
 * serializedToken（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:132）：本地 TypeRef token 分配。
 * writer_scheduler_artifact_write_type_def_signature（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:147）：固定五字节节点。
 * transfer（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:209）：记录资源移动描述。
 * schedulerContract（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:217）：按原 provider 关联恢复三类 token。
 * sections（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:247）：八个 mandatory 节起点。
 * document.sections（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:265）：借用栈上节数组。
 */

/**
 * @name ZrParser_Writer_WriteSchedulerArtifactFile::encode_before_open
 *
 * ## Summary
 * 在目标文件可能被截断之前完成格式校验、容量求值和完整内存编码。 所有节与公开身份已形成、尚未进行文件 I/O 时。
 *
 * ## Constraints and lifecycle
 * Core 拒绝文档或编码失败时直接传播状态；宿主 malloc 失败映射为 BUFFER_TOO_SMALL。编码成功还须 writtenSize 等于 encodedSize，否则释放编码区并报告 INVALID_SECTION。此阶段未打开 filename，因此这些普通失败不会由本入口创建或截断目标；它不保证其它并发进程没有操作目标。文档及栈上节在两次 Core 调用间保持一致。
 *
 * ## References
 * ZrCore_Artifact_GetEncodedSize（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:267）：先求值和验证。
 * malloc（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:271）：独立编码区。
 * ZrCore_Artifact_Write（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:275）：打开前完整编码。
 * writtenSize（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:276）：成功长度一致性门禁。
 * fopen（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:283）：严格位于本块之后。
 */

/**
 * @name ZrParser_Writer_WriteSchedulerArtifactFile::destructive_file_write_cleanup
 *
 * ## Summary
 * 以宿主流写入已编码产物，并把短写与关闭错误合并到同一普通清理路径。 内存编码及长度校验成功之后，调用者已为目标提供独占路径时。
 *
 * ## Constraints and lifecycle
 * wb 打开既有目标会先截断，随后 fwrite 或 fclose 失败不能恢复旧内容。即使 fwrite 短写仍执行 fclose；二者任一失败时尽力 remove 后释放 encoded，再报告 INVALID_SECTION。remove 的失败未被检验，因此不能承诺失败后目标必定不存在。打开失败只释放编码区并报告 INVALID_ARGUMENT。成功也会释放编码区；没有文件句柄或编码区交付给调用者，没有目录创建、原子替换或持久同步保证。
 *
 * ## References
 * fopen（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:283）：创建或截断边界。
 * fwrite（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:288）：短写观察。
 * fclose（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:291）：即使短写也执行关闭。
 * remove（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:295）：尽力删除且返回值未检查。
 * free（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:299）：成功释放编码区。
 * writer_scheduler_artifact_fail（zr_vm_parser/src/zr_vm_parser/writer/writer_scheduler_artifact.c:301）：最终状态输出。
 */
