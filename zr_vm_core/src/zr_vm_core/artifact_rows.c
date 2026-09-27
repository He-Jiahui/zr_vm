#include "zr_vm_core/artifact_schema.h"

#include <string.h>

#include "artifact_schema_internal.h"

/* 将 TypeDef 的类型与构造身份写成固定行；文档校验已核 token 和类型能力。 */
static void artifact_write_type_def_row(TZrByte *bytes, const SZrArtifactTypeDefRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->token);
    zr_artifact_write_u32(bytes + 4u, row->flags);
    zr_artifact_write_u32(bytes + 8u, row->canonicalTypeId);
    zr_artifact_write_u32(bytes + 12u, row->constructorToken);
    zr_artifact_write_u32(bytes + 16u, row->constructorSignatureToken);
    zr_artifact_write_u32(bytes + 20u, 0u);
    zr_artifact_write_u64(bytes + 24u, row->typeSignatureHash);
    zr_artifact_write_u64(bytes + 32u, row->constructorContractHash);
    zr_artifact_write_u64(bytes + 40u, 0u);
}

/* TypeRef 与 TypeSpec 共用身份行格式；签名窗口须由上层与 signature heap 对照。 */
static void artifact_write_type_identity_row(TZrByte *bytes, const SZrArtifactTypeIdentityRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->token);
    zr_artifact_write_u32(bytes + 4u, row->signatureToken);
    zr_artifact_write_u32(bytes + 8u, row->canonicalTypeId);
    zr_artifact_write_u32(bytes + 12u, row->flags);
    zr_artifact_write_u32(bytes + 16u, row->signatureOffset);
    zr_artifact_write_u32(bytes + 20u, row->signatureLength);
    zr_artifact_write_u64(bytes + 24u, row->signatureHash);
    zr_artifact_write_u32(bytes + 32u, row->layoutVersion);
    zr_artifact_write_u32(bytes + 36u, 0u);
    zr_artifact_write_u64(bytes + 40u, row->layoutHash);
}

/* 成员的 owner、签名及契约随行持久化；跨表引用由文档/元数据图校验负责。 */
static void artifact_write_member_row(TZrByte *bytes, const SZrArtifactMemberDefRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->token);
    zr_artifact_write_u32(bytes + 4u, row->ownerTypeToken);
    zr_artifact_write_u32(bytes + 8u, row->signatureToken);
    zr_artifact_write_u32(bytes + 12u, row->flags);
    zr_artifact_write_u32(bytes + 16u, row->nameStringOffset);
    zr_artifact_write_u32(bytes + 20u, 0u);
    zr_artifact_write_u64(bytes + 24u, row->signatureHash);
    zr_artifact_write_u64(bytes + 32u, row->contractHash);
}

/* 属性把访问器和初始化器身份绑定到 owner；写出前由上层检查成员归属。 */
static void artifact_write_property_row(TZrByte *bytes, const SZrArtifactPropertyDefRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->token);
    zr_artifact_write_u32(bytes + 4u, row->ownerTypeToken);
    zr_artifact_write_u32(bytes + 8u, row->getterToken);
    zr_artifact_write_u32(bytes + 12u, row->setterToken);
    zr_artifact_write_u32(bytes + 16u, row->signatureToken);
    zr_artifact_write_u32(bytes + 20u, row->flags);
    zr_artifact_write_u64(bytes + 24u, row->signatureHash);
    zr_artifact_write_u64(bytes + 32u, row->contractHash);
    zr_artifact_write_u32(bytes + 40u, row->initializerToken);
    zr_artifact_write_u32(bytes + 44u, row->nameStringOffset);
}

/* 保留 callable 的 receiver、逃逸与 ABI 契约，供后续 canonical consumer 比对。 */
static void artifact_write_contract_row(TZrByte *bytes, const SZrArtifactContractRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->memberToken);
    zr_artifact_write_u32(bytes + 4u, row->signatureToken);
    zr_artifact_write_u32(bytes + 8u, row->parameterCount);
    zr_artifact_write_u32(bytes + 12u, row->flags);
    zr_artifact_write_u32(bytes + 16u, row->receiverEffect);
    zr_artifact_write_u32(bytes + 20u, row->refExportEffect);
    zr_artifact_write_u32(bytes + 24u, row->escapeFlags);
    zr_artifact_write_u32(bytes + 28u, (TZrUInt32)row->abiLoweringKind);
    zr_artifact_write_u64(bytes + 32u, row->contractHash);
}

/* 固定布局行承载 GC 扫描和稳定 slot 合同；ownership map 内容另存于 heap。 */
static void artifact_write_layout_row(TZrByte *bytes, const SZrArtifactLayoutRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->typeToken);
    zr_artifact_write_u32(bytes + 4u, row->version);
    zr_artifact_write_u32(bytes + 8u, row->byteSize);
    zr_artifact_write_u32(bytes + 12u, row->byteAlignment);
    zr_artifact_write_u32(bytes + 16u, row->gcScanKind);
    zr_artifact_write_u32(bytes + 20u, row->capabilityFlags);
    zr_artifact_write_u32(bytes + 24u, row->ownershipMapOffset);
    zr_artifact_write_u32(bytes + 28u, row->ownershipMapLength);
    zr_artifact_write_u64(bytes + 32u, row->layoutHash);
    zr_artifact_write_u64(bytes + 40u, row->stableSlotContractHash);
}

/* 反射保留级别、代数与摘要同列写出，以便加载时由元数据图复算核验。 */
static void artifact_write_metadata_state_row(
        TZrByte *bytes,
        const SZrArtifactMetadataStateRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->typeToken);
    zr_artifact_write_u32(bytes + 4u, (TZrUInt32)row->preservationState);
    zr_artifact_write_u32(bytes + 8u, (TZrUInt32)row->category);
    zr_artifact_write_u32(bytes + 12u, row->metadataGeneration);
    zr_artifact_write_u32(bytes + 16u, row->retainedMemberCount);
    zr_artifact_write_u32(bytes + 20u, row->retainedPropertyCount);
    zr_artifact_write_u32(bytes + 24u, row->retainedMetaRecordCount);
    zr_artifact_write_u32(bytes + 28u, row->flags);
    zr_artifact_write_u64(bytes + 32u, row->typeSignatureHash);
    zr_artifact_write_u64(bytes + 40u, row->layoutHash);
    zr_artifact_write_u64(bytes + 48u, row->callableContractHash);
    zr_artifact_write_u64(bytes + 56u, row->metadataHash);
}

/* 元数据记录引用独立 blob 载荷；此行只保存其坐标、有效期代数与摘要。 */
static void artifact_write_metadata_record_row(
        TZrByte *bytes,
        const SZrArtifactMetadataRecordRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->ownerToken);
    zr_artifact_write_u32(bytes + 4u, (TZrUInt32)row->kind);
    zr_artifact_write_u32(bytes + 8u, (TZrUInt32)row->retention);
    zr_artifact_write_u32(bytes + 12u, row->flags);
    zr_artifact_write_u32(bytes + 16u, row->payloadOffset);
    zr_artifact_write_u32(bytes + 20u, row->payloadLength);
    zr_artifact_write_u32(bytes + 24u, row->metadataGeneration);
    zr_artifact_write_u32(bytes + 28u, 0u);
    zr_artifact_write_u64(bytes + 32u, row->recordHash);
}

/* 传输声明绑定 provider token/contract hash 与 schema 窗口；TODO: 需确认谁核实体与哈希对应。 */
static void artifact_write_domain_transfer_row(
        TZrByte *bytes,
        const SZrArtifactDomainTransferRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->typeToken);
    zr_artifact_write_u32(bytes + 4u, (TZrUInt32)row->kind);
    zr_artifact_write_u32(bytes + 8u, row->schemaVersion);
    zr_artifact_write_u32(bytes + 12u, row->flags);
    zr_artifact_write_u32(bytes + 16u, row->schemaOffset);
    zr_artifact_write_u32(bytes + 20u, row->schemaLength);
    zr_artifact_write_u32(bytes + 24u, row->providerToken);
    zr_artifact_write_u32(bytes + 28u, 0u);
    zr_artifact_write_u64(bytes + 32u, row->schemaHash);
    zr_artifact_write_u64(bytes + 40u, row->providerContractHash);
}

/* 调度器政策、需求与传输摘要进入固定行，供二进制 consumer 精确比对。 */
static void artifact_write_scheduler_contract_row(
        TZrByte *bytes,
        const SZrArtifactSchedulerContractRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->schedulerTypeToken);
    zr_artifact_write_u32(bytes + 4u, row->taskTypeToken);
    zr_artifact_write_u32(bytes + 8u, row->jobTypeToken);
    zr_artifact_write_u32(bytes + 12u, row->abiVersion);
    zr_artifact_write_u32(bytes + 16u, row->policyMask);
    zr_artifact_write_u32(bytes + 20u, row->attachedRequirementFlags);
    zr_artifact_write_u32(bytes + 24u, row->isolatedRequirementFlags);
    zr_artifact_write_u32(bytes + 28u, 0u);
    zr_artifact_write_u64(bytes + 32u, row->transportContractHash);
    zr_artifact_write_u64(bytes + 40u, row->schedulerContractHash);
}

/* 重定位记录将代码坐标与预期目标身份绑定；写端先校验代码节和 token。 */
static void artifact_write_relocation_row(TZrByte *bytes, const SZrArtifactRelocationRow *row) {
    zr_artifact_write_u32(bytes + 0u, row->codeOffset);
    zr_artifact_write_u32(bytes + 4u, row->kind);
    zr_artifact_write_u32(bytes + 8u, row->targetToken);
    zr_artifact_write_u32(bytes + 12u, row->targetSignatureToken);
    zr_artifact_write_u64(bytes + 16u, row->expectedSignatureHash);
    zr_artifact_write_u64(bytes + 24u, row->expectedContractHash);
    zr_artifact_write_u64(bytes + 32u, row->expectedModuleHash);
}

/* 整文档 writer 已校验节种类、行数据和目标容量；此处分派固定行或原始 byte heap。
 * 输入 section->data 不得与 writer 的输出 buffer 重叠，写端先清空输出区。 */
void zr_artifact_write_section_payload(TZrByte *bytes, const SZrArtifactSectionInput *section) {
    TZrUInt32 index;
    TZrUInt32 elementSize = zr_artifact_section_element_size(section->kind);

    switch (section->kind) {
        case ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_type_def_row(bytes + (TZrSize)index * elementSize,
                                            &((const SZrArtifactTypeDefRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_TYPE_REF_TABLE:
        case ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_type_identity_row(bytes + (TZrSize)index * elementSize,
                                                 &((const SZrArtifactTypeIdentityRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_member_row(bytes + (TZrSize)index * elementSize,
                                          &((const SZrArtifactMemberDefRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_property_row(bytes + (TZrSize)index * elementSize,
                                            &((const SZrArtifactPropertyDefRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_CONTRACT_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_contract_row(bytes + (TZrSize)index * elementSize,
                                            &((const SZrArtifactContractRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_LAYOUT_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_layout_row(bytes + (TZrSize)index * elementSize,
                                          &((const SZrArtifactLayoutRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_METADATA_STATE_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_metadata_state_row(
                        bytes + (TZrSize)index * elementSize,
                        &((const SZrArtifactMetadataStateRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_METADATA_RECORD_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_metadata_record_row(
                        bytes + (TZrSize)index * elementSize,
                        &((const SZrArtifactMetadataRecordRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_domain_transfer_row(
                        bytes + (TZrSize)index * elementSize,
                        &((const SZrArtifactDomainTransferRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_scheduler_contract_row(
                        bytes + (TZrSize)index * elementSize,
                        &((const SZrArtifactSchedulerContractRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE:
            for (index = 0u; index < section->elementCount; ++index)
                artifact_write_relocation_row(bytes + (TZrSize)index * elementSize,
                                              &((const SZrArtifactRelocationRow *)section->data)[index]);
            break;
        case ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE:
            /* TODO: 当前依赖上层以同一约束预检每行；若写入路径分离，需传播行编码失败。 */
            for (index = 0u; index < section->elementCount; ++index)
                ZrCore_Artifact_WriteCallBindingRow(
                        &((const SZrArtifactCallBindingRow *)section->data)[index],
                        bytes + (TZrSize)index * elementSize, elementSize, ZR_NULL);
            break;
        default:
            if (section->elementCount > 0u) memcpy(bytes, section->data, section->elementCount);
            break;
    }
}

/* 单行 API 的共同入口；正常解码路径从 Read/FindSection 取得已验证的借用节视图。 */
/* BUG: 公开调用可用错误 kind、空 data 或不足 byteLength 且匹配的步长通过检查；
 * 随后的读取可能误解码或越界，需按目标行种类和完整字节范围拒绝该输入。 */
static EZrArtifactStatus artifact_get_row_bytes(const SZrArtifactSectionView *section,
                                                TZrUInt32 rowIndex,
                                                TZrUInt32 expectedSize,
                                                const TZrByte **outBytes,
                                                SZrArtifactDiagnostic *diagnostic) {
    if (section == ZR_NULL || outBytes == ZR_NULL ||
        section->elementSize != expectedSize || rowIndex >= section->elementCount) {
        return zr_artifact_fail(diagnostic,
                                ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
                                section != ZR_NULL ? section->kind : 0u,
                                rowIndex,
                                0u);
    }
    /* BUG: 复用 diagnostic 时成功不清旧状态，可返回 OK 而 diagnostic.status 仍为上次错误。 */
    *outBytes = section->data + (TZrSize)rowIndex * expectedSize;
    return ZR_ARTIFACT_STATUS_OK;
}

/* 以下单行读取仅在 OK 时发布可用输出；outRow 不得与 section 的编码字节重叠。 */
/* 从已验证 TypeDef 节投影类型身份；字段合法性仍由整文档和元数据图校验。 */
EZrArtifactStatus ZrCore_Artifact_ReadTypeDefRow(const SZrArtifactSectionView *section,
                                                 TZrUInt32 rowIndex,
                                                 SZrArtifactTypeDefRow *outRow,
                                                 SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_TYPE_DEF_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->token = zr_artifact_read_u32(bytes + 0u);
    outRow->flags = zr_artifact_read_u32(bytes + 4u);
    outRow->canonicalTypeId = zr_artifact_read_u32(bytes + 8u);
    outRow->constructorToken = zr_artifact_read_u32(bytes + 12u);
    outRow->constructorSignatureToken = zr_artifact_read_u32(bytes + 16u);
    outRow->typeSignatureHash = zr_artifact_read_u64(bytes + 24u);
    outRow->constructorContractHash = zr_artifact_read_u64(bytes + 32u);
    return ZR_ARTIFACT_STATUS_OK;
}

/* 为 TypeRef/TypeSpec consumer 还原签名窗口与布局身份，不复制 signature heap 字节。 */
EZrArtifactStatus ZrCore_Artifact_ReadTypeIdentityRow(const SZrArtifactSectionView *section,
                                                      TZrUInt32 rowIndex,
                                                      SZrArtifactTypeIdentityRow *outRow,
                                                      SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_TYPE_IDENTITY_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->token = zr_artifact_read_u32(bytes + 0u);
    outRow->signatureToken = zr_artifact_read_u32(bytes + 4u);
    outRow->canonicalTypeId = zr_artifact_read_u32(bytes + 8u);
    outRow->flags = zr_artifact_read_u32(bytes + 12u);
    outRow->signatureOffset = zr_artifact_read_u32(bytes + 16u);
    outRow->signatureLength = zr_artifact_read_u32(bytes + 20u);
    outRow->signatureHash = zr_artifact_read_u64(bytes + 24u);
    outRow->layoutVersion = zr_artifact_read_u32(bytes + 32u);
    outRow->layoutHash = zr_artifact_read_u64(bytes + 40u);
    return ZR_ARTIFACT_STATUS_OK;
}

/* 供成员引用与反射图读取 owner、名称及 callable 身份；不在单行阶段解析引用。 */
EZrArtifactStatus ZrCore_Artifact_ReadMemberDefRow(const SZrArtifactSectionView *section,
                                                   TZrUInt32 rowIndex,
                                                   SZrArtifactMemberDefRow *outRow,
                                                   SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_MEMBER_DEF_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->token = zr_artifact_read_u32(bytes + 0u);
    outRow->ownerTypeToken = zr_artifact_read_u32(bytes + 4u);
    outRow->signatureToken = zr_artifact_read_u32(bytes + 8u);
    outRow->flags = zr_artifact_read_u32(bytes + 12u);
    outRow->nameStringOffset = zr_artifact_read_u32(bytes + 16u);
    outRow->signatureHash = zr_artifact_read_u64(bytes + 24u);
    outRow->contractHash = zr_artifact_read_u64(bytes + 32u);
    return ZR_ARTIFACT_STATUS_OK;
}

/* 将属性访问器和名称坐标交给元数据图；owner/访问器关联在上层复核。 */
EZrArtifactStatus ZrCore_Artifact_ReadPropertyDefRow(const SZrArtifactSectionView *section,
                                                     TZrUInt32 rowIndex,
                                                     SZrArtifactPropertyDefRow *outRow,
                                                     SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_PROPERTY_DEF_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->token = zr_artifact_read_u32(bytes + 0u);
    outRow->ownerTypeToken = zr_artifact_read_u32(bytes + 4u);
    outRow->getterToken = zr_artifact_read_u32(bytes + 8u);
    outRow->setterToken = zr_artifact_read_u32(bytes + 12u);
    outRow->signatureToken = zr_artifact_read_u32(bytes + 16u);
    outRow->flags = zr_artifact_read_u32(bytes + 20u);
    outRow->signatureHash = zr_artifact_read_u64(bytes + 24u);
    outRow->contractHash = zr_artifact_read_u64(bytes + 32u);
    outRow->initializerToken = zr_artifact_read_u32(bytes + 40u);
    outRow->nameStringOffset = zr_artifact_read_u32(bytes + 44u);
    return ZR_ARTIFACT_STATUS_OK;
}

/* 恢复 callable 的 receiver/ref 逃逸与 ABI 字段，供解码校验及 canonical consumer 复核。 */
EZrArtifactStatus ZrCore_Artifact_ReadContractRow(const SZrArtifactSectionView *section,
                                                  TZrUInt32 rowIndex,
                                                  SZrArtifactContractRow *outRow,
                                                  SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_CONTRACT_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->memberToken = zr_artifact_read_u32(bytes + 0u);
    outRow->signatureToken = zr_artifact_read_u32(bytes + 4u);
    outRow->parameterCount = zr_artifact_read_u32(bytes + 8u);
    outRow->flags = zr_artifact_read_u32(bytes + 12u);
    outRow->receiverEffect = zr_artifact_read_u32(bytes + 16u);
    outRow->refExportEffect = zr_artifact_read_u32(bytes + 20u);
    outRow->escapeFlags = zr_artifact_read_u32(bytes + 24u);
    outRow->abiLoweringKind = (EZrArtifactAbiLoweringKind)zr_artifact_read_u32(bytes + 28u);
    outRow->contractHash = zr_artifact_read_u64(bytes + 32u);
    return ZR_ARTIFACT_STATUS_OK;
}

/* 恢复 GC/ownership 布局标识；独立 map 载荷和 type 关联仍需上层验证。 */
EZrArtifactStatus ZrCore_Artifact_ReadLayoutRow(const SZrArtifactSectionView *section,
                                                TZrUInt32 rowIndex,
                                                SZrArtifactLayoutRow *outRow,
                                                SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_LAYOUT_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->typeToken = zr_artifact_read_u32(bytes + 0u);
    outRow->version = zr_artifact_read_u32(bytes + 4u);
    outRow->byteSize = zr_artifact_read_u32(bytes + 8u);
    outRow->byteAlignment = zr_artifact_read_u32(bytes + 12u);
    outRow->gcScanKind = zr_artifact_read_u32(bytes + 16u);
    outRow->capabilityFlags = zr_artifact_read_u32(bytes + 20u);
    outRow->ownershipMapOffset = zr_artifact_read_u32(bytes + 24u);
    outRow->ownershipMapLength = zr_artifact_read_u32(bytes + 28u);
    outRow->layoutHash = zr_artifact_read_u64(bytes + 32u);
    outRow->stableSlotContractHash = zr_artifact_read_u64(bytes + 40u);
    return ZR_ARTIFACT_STATUS_OK;
}

/* 反射加载路径取得保留级别、generation 和摘要；图校验再核关联与哈希。 */
EZrArtifactStatus ZrCore_Artifact_ReadMetadataStateRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactMetadataStateRow *outRow,
        SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;

    if (outRow == ZR_NULL) {
        return zr_artifact_fail(
                diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    }
    status = artifact_get_row_bytes(
            section,
            rowIndex,
            ZR_ARTIFACT_METADATA_STATE_ROW_ENCODED_SIZE,
            &bytes,
            diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->typeToken = zr_artifact_read_u32(bytes + 0u);
    outRow->preservationState =
            (EZrArtifactMetadataPreservationState)zr_artifact_read_u32(bytes + 4u);
    outRow->category =
            (EZrArtifactReflectionCategory)zr_artifact_read_u32(bytes + 8u);
    outRow->metadataGeneration = zr_artifact_read_u32(bytes + 12u);
    outRow->retainedMemberCount = zr_artifact_read_u32(bytes + 16u);
    outRow->retainedPropertyCount = zr_artifact_read_u32(bytes + 20u);
    outRow->retainedMetaRecordCount = zr_artifact_read_u32(bytes + 24u);
    outRow->flags = zr_artifact_read_u32(bytes + 28u);
    outRow->typeSignatureHash = zr_artifact_read_u64(bytes + 32u);
    outRow->layoutHash = zr_artifact_read_u64(bytes + 40u);
    outRow->callableContractHash = zr_artifact_read_u64(bytes + 48u);
    outRow->metadataHash = zr_artifact_read_u64(bytes + 56u);
    return ZR_ARTIFACT_STATUS_OK;
}

/* 读取指向 metadata blob 的记录坐标；不复制或拥有 blob 载荷。 */
EZrArtifactStatus ZrCore_Artifact_ReadMetadataRecordRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactMetadataRecordRow *outRow,
        SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;

    if (outRow == ZR_NULL) {
        return zr_artifact_fail(
                diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    }
    status = artifact_get_row_bytes(
            section,
            rowIndex,
            ZR_ARTIFACT_METADATA_RECORD_ROW_ENCODED_SIZE,
            &bytes,
            diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->ownerToken = zr_artifact_read_u32(bytes + 0u);
    outRow->kind =
            (EZrArtifactMetadataRecordKind)zr_artifact_read_u32(bytes + 4u);
    outRow->retention =
            (EZrArtifactMetadataRetention)zr_artifact_read_u32(bytes + 8u);
    outRow->flags = zr_artifact_read_u32(bytes + 12u);
    outRow->payloadOffset = zr_artifact_read_u32(bytes + 16u);
    outRow->payloadLength = zr_artifact_read_u32(bytes + 20u);
    outRow->metadataGeneration = zr_artifact_read_u32(bytes + 24u);
    outRow->reserved0 = zr_artifact_read_u32(bytes + 28u);
    outRow->recordHash = zr_artifact_read_u64(bytes + 32u);
    return ZR_ARTIFACT_STATUS_OK;
}

/* 供二进制消费方恢复 provider 与 schema 合同；签名窗口由整文档先检查。 */
EZrArtifactStatus ZrCore_Artifact_ReadDomainTransferRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactDomainTransferRow *outRow,
        SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;

    if (outRow == ZR_NULL) {
        return zr_artifact_fail(
                diagnostic,
                ZR_ARTIFACT_STATUS_INVALID_ARGUMENT,
                0u,
                rowIndex,
                0u);
    }
    status = artifact_get_row_bytes(
            section,
            rowIndex,
            ZR_ARTIFACT_DOMAIN_TRANSFER_ROW_ENCODED_SIZE,
            &bytes,
            diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }
    memset(outRow, 0, sizeof(*outRow));
    outRow->typeToken = zr_artifact_read_u32(bytes + 0u);
    outRow->kind = (EZrArtifactDomainTransferKind)zr_artifact_read_u32(bytes + 4u);
    outRow->schemaVersion = zr_artifact_read_u32(bytes + 8u);
    outRow->flags = zr_artifact_read_u32(bytes + 12u);
    outRow->schemaOffset = zr_artifact_read_u32(bytes + 16u);
    outRow->schemaLength = zr_artifact_read_u32(bytes + 20u);
    outRow->providerToken = zr_artifact_read_u32(bytes + 24u);
    outRow->schemaHash = zr_artifact_read_u64(bytes + 32u);
    outRow->providerContractHash = zr_artifact_read_u64(bytes + 40u);
    return ZR_ARTIFACT_STATUS_OK;
}

/* 调度器 consumer 依据此行核政策、ABI 和哈希；单行 API 仅投影字段。 */
EZrArtifactStatus ZrCore_Artifact_ReadSchedulerContractRow(
        const SZrArtifactSectionView *section,
        TZrUInt32 rowIndex,
        SZrArtifactSchedulerContractRow *outRow,
        SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;

    if (outRow == ZR_NULL) {
        return zr_artifact_fail(
                diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    }
    status = artifact_get_row_bytes(
            section,
            rowIndex,
            ZR_ARTIFACT_SCHEDULER_CONTRACT_ROW_ENCODED_SIZE,
            &bytes,
            diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) {
        return status;
    }
    memset(outRow, 0, sizeof(*outRow));
    outRow->schedulerTypeToken = zr_artifact_read_u32(bytes + 0u);
    outRow->taskTypeToken = zr_artifact_read_u32(bytes + 4u);
    outRow->jobTypeToken = zr_artifact_read_u32(bytes + 8u);
    outRow->abiVersion = zr_artifact_read_u32(bytes + 12u);
    outRow->policyMask = zr_artifact_read_u32(bytes + 16u);
    outRow->attachedRequirementFlags = zr_artifact_read_u32(bytes + 20u);
    outRow->isolatedRequirementFlags = zr_artifact_read_u32(bytes + 24u);
    outRow->reserved0 = zr_artifact_read_u32(bytes + 28u);
    outRow->transportContractHash = zr_artifact_read_u64(bytes + 32u);
    outRow->schedulerContractHash = zr_artifact_read_u64(bytes + 40u);
    return ZR_ARTIFACT_STATUS_OK;
}

/* 将 code offset 与预期目标身份交给重定位校验；代码偏移范围由整文档先核。 */
EZrArtifactStatus ZrCore_Artifact_ReadRelocationRow(const SZrArtifactSectionView *section,
                                                    TZrUInt32 rowIndex,
                                                    SZrArtifactRelocationRow *outRow,
                                                    SZrArtifactDiagnostic *diagnostic) {
    const TZrByte *bytes;
    EZrArtifactStatus status;
    if (outRow == ZR_NULL)
        return zr_artifact_fail(diagnostic, ZR_ARTIFACT_STATUS_INVALID_ARGUMENT, 0u, rowIndex, 0u);
    status = artifact_get_row_bytes(section, rowIndex, ZR_ARTIFACT_RELOCATION_ROW_ENCODED_SIZE,
                                    &bytes, diagnostic);
    if (status != ZR_ARTIFACT_STATUS_OK) return status;
    memset(outRow, 0, sizeof(*outRow));
    outRow->codeOffset = zr_artifact_read_u32(bytes + 0u);
    outRow->kind = zr_artifact_read_u32(bytes + 4u);
    outRow->targetToken = zr_artifact_read_u32(bytes + 8u);
    outRow->targetSignatureToken = zr_artifact_read_u32(bytes + 12u);
    outRow->expectedSignatureHash = zr_artifact_read_u64(bytes + 16u);
    outRow->expectedContractHash = zr_artifact_read_u64(bytes + 24u);
    outRow->expectedModuleHash = zr_artifact_read_u64(bytes + 32u);
    return ZR_ARTIFACT_STATUS_OK;
}
