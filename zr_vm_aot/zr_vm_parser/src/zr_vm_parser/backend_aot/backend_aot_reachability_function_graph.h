#ifndef ZR_VM_PARSER_BACKEND_AOT_REACHABILITY_FUNCTION_GRAPH_H
#define ZR_VM_PARSER_BACKEND_AOT_REACHABILITY_FUNCTION_GRAPH_H

#include "backend_aot_function_table.h"
#include "backend_aot_reachability.h"
#include "zr_vm_parser/writer.h"

/** @brief 合并入口、导出、注解、manifest 与 required-member 根，再扫描函数调用边。
 *  @pre roots/rootReasons/marks/edges/queue 均由调用方分配到相应 capacity。
 *  @note 返回的 marks 与原函数表 flatIndex 对应，供 code stripping 使用。 */
TZrBool backend_aot_compute_static_callable_reachability_with_preserve_roots(
        SZrState *state,
        const SZrAotFunctionTable *table,
        const TZrUInt32 *annotationRoots,
        TZrUInt32 annotationRootCount,
        const TZrUInt32 *manifestRoots,
        TZrUInt32 manifestRootCount,
        const SZrAotManifestGenericRoot *genericRoots,
        TZrUInt32 genericRootCount,
        const SZrAotManifestExportDeclaration *manifestExports,
        TZrUInt32 manifestExportCount,
        TZrUInt32 *roots,
        EZrAotReachabilityReason *rootReasons,
        TZrUInt32 rootCapacity,
        SZrAotReachabilityMark *marks,
        TZrUInt32 markCount,
        SZrAotReachabilityEdge *edges,
        TZrUInt32 edgeCapacity,
        TZrUInt32 *queue,
        TZrUInt32 queueCapacity,
        TZrUInt32 *outMarkedCount,
        TZrUInt32 *outEdgeCount);

/** @brief 在普通根之外加入 manifest 泛型实例根的兼容入口。 */
TZrBool backend_aot_compute_static_callable_reachability_with_generic_roots(
        SZrState *state,
        const SZrAotFunctionTable *table,
        const TZrUInt32 *annotationRoots,
        TZrUInt32 annotationRootCount,
        const TZrUInt32 *manifestRoots,
        TZrUInt32 manifestRootCount,
        const SZrAotManifestGenericRoot *genericRoots,
        TZrUInt32 genericRootCount,
        TZrUInt32 *roots,
        EZrAotReachabilityReason *rootReasons,
        TZrUInt32 rootCapacity,
        SZrAotReachabilityMark *marks,
        TZrUInt32 markCount,
        SZrAotReachabilityEdge *edges,
        TZrUInt32 edgeCapacity,
        TZrUInt32 *queue,
        TZrUInt32 queueCapacity,
        TZrUInt32 *outMarkedCount,
        TZrUInt32 *outEdgeCount);

/** @brief 用入口、导出与显式 manifest 根计算静态函数保留集。 */
TZrBool backend_aot_compute_static_callable_reachability(SZrState *state,
                                                         const SZrAotFunctionTable *table,
                                                         const TZrUInt32 *annotationRoots,
                                                         TZrUInt32 annotationRootCount,
                                                         const TZrUInt32 *manifestRoots,
                                                         TZrUInt32 manifestRootCount,
                                                         TZrUInt32 *roots,
                                                         EZrAotReachabilityReason *rootReasons,
                                                         TZrUInt32 rootCapacity,
                                                         SZrAotReachabilityMark *marks,
                                                         TZrUInt32 markCount,
                                                         SZrAotReachabilityEdge *edges,
                                                         TZrUInt32 edgeCapacity,
                                                         TZrUInt32 *queue,
                                                         TZrUInt32 queueCapacity,
                                                         TZrUInt32 *outMarkedCount,
                                                         TZrUInt32 *outEdgeCount);

/** @brief 扫描函数元数据中的动态依赖注解并转换为扁平函数根。
 *  @note 无法解析的注解不会凭空保留函数；返回失败时计数输出清零。 */
TZrBool backend_aot_collect_reflection_annotation_roots(SZrState *state,
                                                        const SZrAotFunctionTable *table,
                                                        TZrUInt32 *annotationRoots,
                                                        TZrUInt32 annotationRootCapacity,
                                                        TZrUInt32 *outAnnotationRootCount);

#endif
