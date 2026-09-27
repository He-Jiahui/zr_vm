#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_PUBLICATION_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_PUBLICATION_H

#include "backend_aot_c_zrp_metadata_prune.h"

/**
 * @brief 在 emitter 成功写出 AOT 内容后，可选地发布与嵌入版本相同的压缩元数据。
 * @return 未配置输出路径视为成功；验证失败或打开后写入失败会清理目标路径，打开失败则保留旧文件。
 */
TZrBool backend_aot_c_publish_compacted_zrp_metadata(const SZrAotWriterOptions *options,
                                                     const SZrAotCEmbeddedZrpMetadata *metadata);

#endif
