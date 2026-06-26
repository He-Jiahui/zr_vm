#ifndef ZR_VM_PARSER_BACKEND_AOT_REACHABILITY_H
#define ZR_VM_PARSER_BACKEND_AOT_REACHABILITY_H

#include "zr_vm_parser/conf.h"

#define ZR_AOT_REACHABILITY_NO_NODE ((TZrUInt32)0xFFFFFFFFu)

typedef enum EZrAotReachabilityState {
    ZR_AOT_REACHABILITY_STATE_UNMARKED = 0,
    ZR_AOT_REACHABILITY_STATE_MARKED_PENDING = 1,
    ZR_AOT_REACHABILITY_STATE_PROCESSED = 2
} EZrAotReachabilityState;

typedef enum EZrAotReachabilityReason {
    ZR_AOT_REACHABILITY_REASON_NONE = 0,
    ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY = 1,
    ZR_AOT_REACHABILITY_REASON_ROOT_EXPORT = 2,
    ZR_AOT_REACHABILITY_REASON_MANIFEST = 3,
    ZR_AOT_REACHABILITY_REASON_DIRECT_CALL = 4,
    ZR_AOT_REACHABILITY_REASON_FIELD_ACCESS = 5,
    ZR_AOT_REACHABILITY_REASON_VIRTUAL_CALL = 6,
    ZR_AOT_REACHABILITY_REASON_REFLECTION = 7,
    ZR_AOT_REACHABILITY_REASON_GENERIC_INSTANCE = 8
} EZrAotReachabilityReason;

typedef struct SZrAotReachabilityEdge {
    TZrUInt32 source;
    TZrUInt32 target;
    EZrAotReachabilityReason reason;
} SZrAotReachabilityEdge;

typedef struct SZrAotReachabilityMark {
    EZrAotReachabilityState state;
    EZrAotReachabilityReason reason;
    TZrUInt32 predecessor;
} SZrAotReachabilityMark;

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

#endif
