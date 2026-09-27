#include "zr_vm_core/artifact_schema.h"

#include <string.h>

#include "artifact_schema_internal.h"

/* 顶层读写与身份校验共用的诊断起点；先清除上一次操作的扩展失配字段。 */
void zr_artifact_diagnostic_clear(SZrArtifactDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_ARTIFACT_STATUS_OK;
    }
}

/* 将失败状态与节、行、字节位置一起返回；哈希和版本等扩展字段由具体失败分支补充。 */
EZrArtifactStatus zr_artifact_fail(SZrArtifactDiagnostic *diagnostic,
                                   EZrArtifactStatus status,
                                   TZrUInt32 sectionKind,
                                   TZrUInt32 rowIndex,
                                   TZrUInt32 byteOffset) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
        diagnostic->sectionKind = sectionKind;
        diagnostic->rowIndex = rowIndex;
        diagnostic->byteOffset = byteOffset;
    }
    return status;
}

/* 解码器验证完整 header 后读取版本等 16 位线格式字段；调用方保证两个可读字节。 */
TZrUInt16 zr_artifact_read_u16(const TZrByte *bytes) {
    return (TZrUInt16)((TZrUInt16)bytes[0] | ((TZrUInt16)bytes[1] << 8u));
}

/* header、目录、行与布局图共用的小端 32 位字段入口；边界由各层先行校验。 */
TZrUInt32 zr_artifact_read_u32(const TZrByte *bytes) {
    return (TZrUInt32)bytes[0] | ((TZrUInt32)bytes[1] << 8u) |
           ((TZrUInt32)bytes[2] << 16u) | ((TZrUInt32)bytes[3] << 24u);
}

/* 从已验证的身份或行字节中恢复 64 位哈希，不对底层 buffer 建立所有权。 */
TZrUInt64 zr_artifact_read_u64(const TZrByte *bytes) {
    return (TZrUInt64)zr_artifact_read_u32(bytes) |
           ((TZrUInt64)zr_artifact_read_u32(bytes + 4u) << 32u);
}

/* 写入线格式 header 的短字段；上层先核输出容量，再交给此处编码。 */
void zr_artifact_write_u16(TZrByte *bytes, TZrUInt16 value) {
    bytes[0] = (TZrByte)(value & 0xffu);
    bytes[1] = (TZrByte)((value >> 8u) & 0xffu);
}

/* 为 header、目录和固定行提供共同的小端字段表示，避免依赖主机整数布局。 */
void zr_artifact_write_u32(TZrByte *bytes, TZrUInt32 value) {
    bytes[0] = (TZrByte)(value & 0xffu);
    bytes[1] = (TZrByte)((value >> 8u) & 0xffu);
    bytes[2] = (TZrByte)((value >> 16u) & 0xffu);
    bytes[3] = (TZrByte)((value >> 24u) & 0xffu);
}

/* 身份和行哈希沿用与读端一致的线格式；调用方保证八字节可写空间。 */
void zr_artifact_write_u64(TZrByte *bytes, TZrUInt64 value) {
    zr_artifact_write_u32(bytes, (TZrUInt32)(value & 0xffffffffULL));
    zr_artifact_write_u32(bytes + 4u, (TZrUInt32)(value >> 32u));
}

/* 写前校验与读端拒绝未知产物种类共用同一 schema 版本的 kind 集合。 */
TZrBool zr_artifact_kind_is_valid(EZrArtifactKind kind) {
    return (TZrBool)(kind == ZR_ARTIFACT_KIND_ZRS ||
                     kind == ZR_ARTIFACT_KIND_ZRI ||
                     kind == ZR_ARTIFACT_KIND_ZRO);
}

/* 区分本版本已知节与可跳过的未来可选节；写端只接受当前已知节。 */
TZrBool zr_artifact_section_is_known(TZrUInt32 kind) {
    return (TZrBool)(kind >= ZR_ARTIFACT_SECTION_STRING_HEAP &&
                     kind <= ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE);
}

/* 已知节还须符合 ZRS/ZRI/ZRO 的职责边界；调用方先检查 kind 是否已知。 */
TZrBool zr_artifact_section_is_allowed(EZrArtifactKind artifactKind, TZrUInt32 sectionKind) {
    if (artifactKind == ZR_ARTIFACT_KIND_ZRS)
        return (TZrBool)(sectionKind == ZR_ARTIFACT_SECTION_STRING_HEAP ||
                         sectionKind == ZR_ARTIFACT_SECTION_SYNTAX_TREE ||
                         sectionKind == ZR_ARTIFACT_SECTION_DEBUG_MAP);
    if (artifactKind == ZR_ARTIFACT_KIND_ZRI)
        return (TZrBool)(sectionKind != ZR_ARTIFACT_SECTION_SYNTAX_TREE &&
                         sectionKind != ZR_ARTIFACT_SECTION_CODE_TABLE &&
                         sectionKind != ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE &&
                         sectionKind != ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE);
    if (artifactKind == ZR_ARTIFACT_KIND_ZRO)
        return (TZrBool)(sectionKind != ZR_ARTIFACT_SECTION_SYNTAX_TREE &&
                         sectionKind != ZR_ARTIFACT_SECTION_SEMANTIC_IR);
    return ZR_FALSE;
}

/* 统一写端大小计算和读端目录步长检查；堆类节以单字节为元素，未知节无大小。 */
TZrUInt32 zr_artifact_section_element_size(TZrUInt32 kind) {
    switch (kind) {
        case ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE: return ZR_ARTIFACT_TYPE_DEF_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_TYPE_REF_TABLE:
        case ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE: return ZR_ARTIFACT_TYPE_IDENTITY_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE: return ZR_ARTIFACT_MEMBER_DEF_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE: return ZR_ARTIFACT_PROPERTY_DEF_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_CONTRACT_TABLE: return ZR_ARTIFACT_CONTRACT_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_LAYOUT_TABLE: return ZR_ARTIFACT_LAYOUT_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE:
            return ZR_ARTIFACT_DOMAIN_TRANSFER_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE:
            return ZR_ARTIFACT_SCHEDULER_CONTRACT_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_METADATA_STATE_TABLE:
            return ZR_ARTIFACT_METADATA_STATE_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_METADATA_RECORD_TABLE:
            return ZR_ARTIFACT_METADATA_RECORD_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE: return ZR_ARTIFACT_RELOCATION_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE: return ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE;
        case ZR_ARTIFACT_SECTION_STRING_HEAP:
        case ZR_ARTIFACT_SECTION_SIGNATURE_HEAP:
        case ZR_ARTIFACT_SECTION_CODE_TABLE:
        case ZR_ARTIFACT_SECTION_DEBUG_MAP:
        case ZR_ARTIFACT_SECTION_SYNTAX_TREE:
        case ZR_ARTIFACT_SECTION_SEMANTIC_IR:
        case ZR_ARTIFACT_SECTION_METADATA_BLOB_HEAP:
        case ZR_ARTIFACT_SECTION_LAYOUT_MAP_HEAP: return 1u;
        default: return 0u;
    }
}
