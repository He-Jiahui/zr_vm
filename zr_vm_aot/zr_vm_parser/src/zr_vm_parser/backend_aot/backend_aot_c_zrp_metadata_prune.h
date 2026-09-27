#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_PRUNE_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_PRUNE_H

#include "backend_aot_function_table.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_parser/writer.h"

/** 裁剪前后同一成员的令牌映射，供生成的 AOT 注册信息继续引用保留成员。 */
typedef struct SZrAotCZrpMemberTokenRemapEntry {
    TZrMetadataToken sourceToken;
    TZrMetadataToken targetToken;
} SZrAotCZrpMemberTokenRemapEntry;

/** 类型定义压缩后的令牌映射；与成员映射分开，避免混淆令牌表。 */
typedef struct SZrAotCZrpTypeDefTokenRemapEntry {
    TZrMetadataToken sourceToken;
    TZrMetadataToken targetToken;
} SZrAotCZrpTypeDefTokenRemapEntry;

/**
 * @brief emitter 在准备、代码生成和发布期间持有的元数据视图。
 * @note blob 可能借用 options->embeddedModuleBlob；仅 owned* 字段由 release 释放。
 *       令牌映射和导出表只在此视图存活期间有效。
 */
typedef struct SZrAotCEmbeddedZrpMetadata {
    const TZrByte *blob;
    TZrSize length;
    TZrByte *ownedBlob;
    TZrBool hasTypeDefTokenRemap;
    const SZrAotCZrpTypeDefTokenRemapEntry *typeDefTokenRemapEntries;
    TZrUInt32 typeDefTokenRemapCount;
    SZrAotCZrpTypeDefTokenRemapEntry *ownedTypeDefTokenRemapEntries;
    const SZrAotCZrpMemberTokenRemapEntry *memberTokenRemapEntries;
    TZrUInt32 memberTokenRemapCount;
    SZrAotCZrpMemberTokenRemapEntry *ownedMemberTokenRemapEntries;
    const SZrAotManifestExportEntry *manifestExportEntries;
    TZrUInt32 manifestExportCount;
    SZrAotManifestExportEntry *ownedManifestExportEntries;
} SZrAotCEmbeddedZrpMetadata;

/**
 * @brief 为 AOT emitter 准备与最终函数表一致的嵌入元数据，并合入可发布的导出声明。
 * @pre functionTable 应与稍后写入的 AOT 函数表相同；outMetadata 由调用方提供。
 * @return 成功后即使未裁剪也需调用 release；失败时本函数已清理其分配的资源。
 */
TZrBool backend_aot_c_prepare_embedded_zrp_metadata(const SZrAotWriterOptions *options,
                                                    TZrBool enableCodeStripping,
                                                    const SZrAotFunctionTable *functionTable,
                                                    SZrAotCEmbeddedZrpMetadata *outMetadata);
/** 结束 emitter 对准备结果的使用；不会释放借用的原始模块 blob。 */
void backend_aot_c_release_embedded_zrp_metadata(SZrAotCEmbeddedZrpMetadata *metadata);

#endif
