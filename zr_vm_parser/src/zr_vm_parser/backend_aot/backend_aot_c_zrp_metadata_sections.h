#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_SECTIONS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_SECTIONS_H

#include "zr_vm_core/zrp_metadata.h"

/**
 * @brief 把 section kind 映射到 header 中的描述符，供裁剪和 manifest 发布共用布局顺序。
 * @return 未知 kind 或空 header 返回 NULL，调用方不可假设枚举值总能取得 section。
 */
const SZrZrpMetadataSection *backend_aot_c_zrp_metadata_section(
        const SZrZrpMetadataHeader *header,
        EZrZrpMetadataSectionKind sectionKind);
SZrZrpMetadataSection *backend_aot_c_zrp_metadata_mutable_section(
        SZrZrpMetadataHeader *header,
        EZrZrpMetadataSectionKind sectionKind);

/**
 * @brief 按协议固定顺序排布目标 section；空 section 不占空间且清零描述符。
 * @note offset 使用 32 位协议宽度；布局上限由构建目标 header 的调用链负责验证。
 */
void backend_aot_c_zrp_set_section_layout(SZrZrpMetadataSection *section,
                                          TZrUInt32 *offset,
                                          TZrUInt32 byteLength,
                                          TZrUInt32 count,
                                          TZrUInt32 elementSize);

/** 仅用于长度保持不变的 section；源、目标范围须已经由 header/view 校验。 */
void backend_aot_c_zrp_copy_section_if_needed(TZrByte *targetBlob,
                                              const TZrByte *sourceBlob,
                                              const SZrZrpMetadataHeader *sourceHeader,
                                              const SZrZrpMetadataHeader *targetHeader,
                                              EZrZrpMetadataSectionKind sectionKind);

#endif
