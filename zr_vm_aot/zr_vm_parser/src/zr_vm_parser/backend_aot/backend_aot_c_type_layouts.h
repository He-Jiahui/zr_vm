#ifndef ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUTS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_TYPE_LAYOUTS_H

#include <stdio.h>

#include "backend_aot_function_table.h"
#include "zr_vm_core/type_layout.h"

/** @brief 为函数帧引用和动态元数据根生成 C 布局、字段及运行时描述符。 */
void backend_aot_write_c_type_layout_declarations(FILE *file,
                                                  SZrState *state,
                                                  const SZrAotFunctionTable *table,
                                                  const TZrUInt32 *typeLayoutRoots,
                                                  TZrUInt32 typeLayoutRootCount);
/** @brief 从可达函数的动态类型及字段令牌补入布局根；容量不足时返回 false。 */
TZrBool backend_aot_c_type_layout_collect_dynamic_dependency_roots(SZrState *state,
                                                                   const SZrAotFunctionTable *table,
                                                                   const TZrByte *metadataBlob,
                                                                   TZrSize metadataBlobLength,
                                                                   TZrUInt32 *typeLayoutRoots,
                                                                   TZrUInt32 rootCapacity,
                                                                   TZrUInt32 *outRootCount);
/** @brief 统计去重后的帧布局引用与显式根，供裁剪统计使用。 */
TZrUInt32 backend_aot_c_type_layout_count_referenced(SZrState *state,
                                                     const SZrAotFunctionTable *table,
                                                     const TZrUInt32 *typeLayoutRoots,
                                                     TZrUInt32 typeLayoutRootCount);
/** @brief 统计保留布局的内联载荷字节数。 */
unsigned long long backend_aot_c_type_layout_payload_bytes_referenced(SZrState *state,
                                                                      const SZrAotFunctionTable *table,
                                                                      const TZrUInt32 *typeLayoutRoots,
                                                                      TZrUInt32 typeLayoutRootCount);
/** @brief 用实际声明生成路径测量布局生成源码字节数。 */
unsigned long long backend_aot_c_type_layout_generated_bytes_referenced(SZrState *state,
                                                                        const SZrAotFunctionTable *table,
                                                                        const TZrUInt32 *typeLayoutRoots,
                                                                        TZrUInt32 typeLayoutRootCount);
/** @brief 在函数表中解析布局 ID 对应的运行时描述符。 */
const SZrTypeLayout *backend_aot_c_type_layout_resolve_from_table(SZrState *state,
                                                                  const SZrAotFunctionTable *table,
                                                                  TZrUInt32 typeLayoutId);
/** @brief 计算以布局 ID 为索引的注册表长度，包含显式元数据根。 */
TZrUInt32 backend_aot_c_type_layout_index_space(SZrState *state,
                                                const SZrAotFunctionTable *table,
                                                const TZrUInt32 *typeLayoutRoots,
                                                TZrUInt32 typeLayoutRootCount);
/** @brief 计算 GC 描述符索引空间，供生成表保持布局 ID 对齐。 */
TZrUInt32 backend_aot_c_type_layout_gc_descriptor_index_space(SZrState *state,
                                                              const SZrAotFunctionTable *table,
                                                              const TZrUInt32 *typeLayoutRoots,
                                                              TZrUInt32 typeLayoutRootCount);
/** @brief 按布局 ID 输出 GC 描述符指针表。 */
void backend_aot_write_c_type_layout_gc_descriptor_table(FILE *file,
                                                         SZrState *state,
                                                         const SZrAotFunctionTable *table,
                                                         const TZrUInt32 *typeLayoutRoots,
                                                         TZrUInt32 typeLayoutRootCount,
                                                         TZrUInt32 descriptorIndexSpace);
/** @brief 输出生成模块运行时布局描述符的注册表。 */
void backend_aot_write_c_type_layout_registration_table(FILE *file,
                                                        SZrState *state,
                                                        const SZrAotFunctionTable *table,
                                                        const TZrUInt32 *typeLayoutRoots,
                                                        TZrUInt32 typeLayoutRootCount,
                                                        TZrUInt32 typeLayoutIndexSpace);
/** @brief 将布局 ID 映射到唯一的 TypeDef 或 TypeSpec 元数据令牌。 */
void backend_aot_write_c_type_layout_token_table(FILE *file,
                                                 SZrState *state,
                                                 const SZrAotFunctionTable *table,
                                                 const TZrUInt32 *typeLayoutRoots,
                                                 TZrUInt32 typeLayoutRootCount,
                                                 const TZrByte *metadataBlob,
                                                 TZrSize metadataBlobLength,
                                                 TZrUInt32 typeLayoutIndexSpace);

#endif
