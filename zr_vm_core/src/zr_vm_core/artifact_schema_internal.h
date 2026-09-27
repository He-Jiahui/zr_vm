#ifndef ZR_VM_CORE_ARTIFACT_SCHEMA_INTERNAL_H
#define ZR_VM_CORE_ARTIFACT_SCHEMA_INTERNAL_H

#include "zr_vm_core/artifact_schema.h"

/** @brief 顶层编码、解码和行接口复用的诊断起点；空指针允许，非空时连扩展字段清零。 */
void zr_artifact_diagnostic_clear(SZrArtifactDiagnostic *diagnostic);

/** @brief 将失败状态及节、行、字节位置写入可选诊断并原样返回 status。
 * @note 哈希和版本等扩展字段由具体失败分支填写；此函数不清空它们。 */
EZrArtifactStatus zr_artifact_fail(SZrArtifactDiagnostic *diagnostic,
                                   EZrArtifactStatus status,
                                   TZrUInt32 sectionKind,
                                   TZrUInt32 rowIndex,
                                   TZrUInt32 byteOffset);

/** @brief 读取已验证可访问的两个小端线格式字节；不持有 bytes。 */
TZrUInt16 zr_artifact_read_u16(const TZrByte *bytes);
/** @brief 读取已验证可访问的四个小端线格式字节；边界由调用层先检查。 */
TZrUInt32 zr_artifact_read_u32(const TZrByte *bytes);
/** @brief 从已验证的八字节片段恢复哈希或身份字段。 */
TZrUInt64 zr_artifact_read_u64(const TZrByte *bytes);
/** @brief 将短 header 字段写为两个小端字节；调用方保证容量。 */
void zr_artifact_write_u16(TZrByte *bytes, TZrUInt16 value);
/** @brief 将 token、目录及行字段写为四个小端字节；调用方保证容量。 */
void zr_artifact_write_u32(TZrByte *bytes, TZrUInt32 value);
/** @brief 将哈希或身份字段写为八个小端字节；调用方保证容量。 */
void zr_artifact_write_u64(TZrByte *bytes, TZrUInt64 value);

/** @brief 给文档 writer、reader 与文本投影共享当前 schema 接受的 ZRS/ZRI/ZRO 集合。 */
TZrBool zr_artifact_kind_is_valid(EZrArtifactKind kind);
/** @brief 区分本版本已知节与 reader 可跳过的未来可选节。 */
TZrBool zr_artifact_section_is_known(TZrUInt32 kind);
/** @brief 在种类已知后检查该节是否属于指定 ZRS/ZRI/ZRO 产物。 */
TZrBool zr_artifact_section_is_allowed(EZrArtifactKind artifactKind, TZrUInt32 sectionKind);
/** @brief 返回固定行宽或字节堆槽宽；未知种类返回零。 */
TZrUInt32 zr_artifact_section_element_size(TZrUInt32 kind);
/** @brief 写端已预检文档、行和目标容量后，分派固定行编码或原始字节堆拷贝。
 * @pre section->data 与目标片段不重叠，借用数据在整个调用期间有效。 */
void zr_artifact_write_section_payload(TZrByte *bytes, const SZrArtifactSectionInput *section);

/** @brief 从已核对 header 与目录边界的 view 解码一条目录项。
 * @note outSection->data 借用 view->buffer；此步尚不证明节自身的内容有效。 */
EZrArtifactStatus zr_artifact_decode_directory_entry(const SZrArtifactView *view,
                                                      TZrUInt32 index,
                                                      SZrArtifactSectionView *outSection,
                                                      SZrArtifactDiagnostic *diagnostic);

/** @brief 写前检查 metadata token 引用、状态和记录间的跨节关系。 */
EZrArtifactStatus zr_artifact_metadata_graph_validate_input(
        const SZrArtifactDocument *document,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 读后在有界节 view 上复核 metadata 引用、状态和记录关系。 */
EZrArtifactStatus zr_artifact_metadata_graph_validate_decoded(
        const SZrArtifactView *view,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 写前逐行检查 call binding 版本、字段和严格排序。 */
EZrArtifactStatus zr_artifact_call_binding_validate_input(
        const SZrArtifactSectionInput *section,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 读后逐行复核 call binding，并允许产物没有该可选节。 */
EZrArtifactStatus zr_artifact_call_binding_validate_decoded(
        const SZrArtifactView *view,
        SZrArtifactDiagnostic *diagnostic);

#endif // ZR_VM_CORE_ARTIFACT_SCHEMA_INTERNAL_H
