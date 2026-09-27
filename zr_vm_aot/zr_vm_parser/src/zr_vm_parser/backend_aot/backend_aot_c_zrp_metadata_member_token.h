#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_MEMBER_TOKEN_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_MEMBER_TOKEN_H

#include "backend_aot_c_zrp_metadata_prune.h"

#include "zr_vm_core/zrp_metadata.h"

/**
 * @brief 在元数据表完成裁剪后，为 emitter 的导出描述符建立原成员令牌到新令牌的映射。
 * @pre 方法和字段保留数量应与已计算的目标表长度一致；失败由调用方清理 metadata。
 */
TZrBool backend_aot_c_zrp_member_token_remap_build(
        SZrAotCEmbeddedZrpMetadata *metadata,
        const SZrZrpMetadataMethodDefRow *methodRows,
        TZrUInt32 methodCount,
        const SZrZrpMetadataFieldDefRow *fieldRows,
        TZrUInt32 fieldCount,
        const SZrZrpMetadataTypeDefRow *typeRows,
        TZrUInt32 typeCount,
        const SZrMetadataTokenRecord *tokenRecords,
        TZrUInt32 tokenRecordCount,
        const SZrZrpMetadataGenericParamRow *genericParamRows,
        TZrUInt32 genericParamCount,
        const SZrZrpMetadataGenericParamConstraintRow *genericParamConstraintRows,
        TZrUInt32 genericParamConstraintCount,
        const SZrAotFunctionTable *functionTable,
        TZrUInt32 retainedMethodDefCount,
        TZrUInt32 retainedFieldDefCount);
void backend_aot_c_zrp_member_token_remap_destroy(SZrAotCEmbeddedZrpMetadata *metadata);
/** MemberDef 有映射时仅允许保留成员；空映射路径会原样返回令牌，不能据此推断目标表仍包含该成员。 */
TZrBool backend_aot_c_embedded_zrp_metadata_remap_member_token(const SZrAotCEmbeddedZrpMetadata *metadata,
                                                               TZrMetadataToken *token);
TZrBool backend_aot_c_embedded_zrp_metadata_remap_type_def_token(const SZrAotCEmbeddedZrpMetadata *metadata,
                                                                 TZrMetadataToken *token);
/**
 * @brief 为 AOT 注册描述符构造进程内导出表，令牌绑定与嵌入元数据的裁剪结果保持一致。
 * @note target 字符串仍借用声明数组；调用方需让声明在 emitter 完成前保持有效。
 */
TZrBool backend_aot_c_zrp_manifest_export_table_build(
        SZrAotCEmbeddedZrpMetadata *metadata,
        const SZrAotManifestExportDeclaration *declarations,
        TZrUInt32 declarationCount);
void backend_aot_c_zrp_manifest_export_table_destroy(SZrAotCEmbeddedZrpMetadata *metadata);

#endif
