#ifndef ZR_VM_PARSER_BACKEND_AOT_REACHABILITY_H
#define ZR_VM_PARSER_BACKEND_AOT_REACHABILITY_H

#include <stdio.h>

#include "zr_vm_parser/conf.h"

#define ZR_AOT_REACHABILITY_NO_NODE ((TZrUInt32)0xFFFFFFFFu)

/** @brief BFS 节点状态；完成清单只接受 PROCESSED。 */
typedef enum EZrAotReachabilityState {
    ZR_AOT_REACHABILITY_STATE_UNMARKED = 0,
    ZR_AOT_REACHABILITY_STATE_MARKED_PENDING = 1,
    ZR_AOT_REACHABILITY_STATE_PROCESSED = 2
} EZrAotReachabilityState;

/** @brief 根保留或函数引用边的来源，写入清单供裁剪结果追溯。 */
typedef enum EZrAotReachabilityReason {
    ZR_AOT_REACHABILITY_REASON_NONE = 0,
    ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY = 1,
    ZR_AOT_REACHABILITY_REASON_ROOT_EXPORT = 2,
    ZR_AOT_REACHABILITY_REASON_MANIFEST = 3,
    ZR_AOT_REACHABILITY_REASON_DIRECT_CALL = 4,
    ZR_AOT_REACHABILITY_REASON_FIELD_ACCESS = 5,
    ZR_AOT_REACHABILITY_REASON_VIRTUAL_CALL = 6,
    ZR_AOT_REACHABILITY_REASON_REFLECTION = 7,
    ZR_AOT_REACHABILITY_REASON_GENERIC_INSTANCE = 8,
    ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION = 9,
    ZR_AOT_REACHABILITY_REASON_PROPERTY_ACCESSOR = 10,
    ZR_AOT_REACHABILITY_REASON_RESOURCE_DROP = 11,
    ZR_AOT_REACHABILITY_REASON_GENERIC_METHODSPEC = 12,
    ZR_AOT_REACHABILITY_REASON_REFLECTION_CONSTRUCTOR = 13,
    ZR_AOT_REACHABILITY_REASON_PACKAGE_EXPORT = 14,
    ZR_AOT_REACHABILITY_REASON_NATIVE_CALLBACK = 15,
    ZR_AOT_REACHABILITY_REASON_NATIVE_IMPORT = 16
} EZrAotReachabilityReason;

/** @brief 静态函数图中从 source 指向 target 的引用边。 */
typedef struct SZrAotReachabilityEdge {
    TZrUInt32 source;
    TZrUInt32 target;
    EZrAotReachabilityReason reason;
} SZrAotReachabilityEdge;

/** @brief 每个函数的保留证据；根节点 predecessor 为 NO_NODE，其他节点指向已保留前驱。 */
typedef struct SZrAotReachabilityMark {
    EZrAotReachabilityState state;
    EZrAotReachabilityReason reason;
    TZrUInt32 predecessor;
} SZrAotReachabilityMark;

/** @brief 返回清单使用的稳定原因文本；未知原因返回空指针。 */
const TZrChar *backend_aot_reachability_reason_name(
        EZrAotReachabilityReason reason);

/** @brief 从根沿静态边计算函数保留集，并保存第一条发现路径。
 *  @pre marks 可容纳 markCount 项，queue 可容纳 queueCapacity 项且至少为 markCount。
 *  @note 校验失败时不会重置 marks；成功后所有节点状态均已确定。 */
TZrBool backend_aot_reachability_compute(SZrAotReachabilityMark *marks,
                                          TZrUInt32 markCount,
                                          const TZrUInt32 *roots,
                                          const EZrAotReachabilityReason *rootReasons,
                                          TZrUInt32 rootCount,
                                          const SZrAotReachabilityEdge *edges,
                                          TZrUInt32 edgeCount,
                                          TZrUInt32 *queue,
                                          TZrUInt32 queueCapacity,
                                          TZrUInt32 *outMarkedCount);

/** @brief 验证根到节点的前驱链并输出注释形式的函数保留清单。
 *  @note 写入失败时目标 FILE 可能已有部分输出。 */
TZrBool backend_aot_reachability_write_function_manifest(FILE *file,
                                                          const SZrAotReachabilityMark *marks,
                                                          TZrUInt32 markCount);

#endif
