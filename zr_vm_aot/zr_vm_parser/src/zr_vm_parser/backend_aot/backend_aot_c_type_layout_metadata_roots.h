#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUT_METADATA_ROOTS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUT_METADATA_ROOTS_H

#include "zr_vm_core/metadata_token.h"

/** @brief 一个字段令牌至多引入拥有者布局和字段布局两个根。 */
#define ZR_AOT_C_TYPE_LAYOUT_METADATA_ROOT_CAPACITY 2u

/** @brief 保存单次元数据令牌解析所得的去重布局根；count 不超过固定容量。 */
typedef struct SZrAotCTypeLayoutMetadataRoots {
    TZrUInt32 typeLayoutIds[ZR_AOT_C_TYPE_LAYOUT_METADATA_ROOT_CAPACITY];
    TZrUInt32 count;
} SZrAotCTypeLayoutMetadataRoots;

/** @brief 从 TypeDef、TypeRef 或 TypeSpec 令牌解析布局根；失败时输出计数清零。 */
TZrBool backend_aot_c_type_layout_metadata_type_token_roots(
        const TZrByte *metadataBlob,
        TZrSize metadataBlobLength,
        TZrMetadataToken typeToken,
        SZrAotCTypeLayoutMetadataRoots *outRoots);

/** @brief 从字段令牌收集拥有者及字段布局，供动态元数据访问保留描述符。 */
TZrBool backend_aot_c_type_layout_metadata_field_token_roots(
        const TZrByte *metadataBlob,
        TZrSize metadataBlobLength,
        TZrMetadataToken fieldToken,
        SZrAotCTypeLayoutMetadataRoots *outRoots);

#endif
