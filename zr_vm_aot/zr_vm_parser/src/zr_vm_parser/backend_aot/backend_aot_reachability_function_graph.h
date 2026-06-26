#ifndef ZR_VM_PARSER_BACKEND_AOT_REACHABILITY_FUNCTION_GRAPH_H
#define ZR_VM_PARSER_BACKEND_AOT_REACHABILITY_FUNCTION_GRAPH_H

#include "backend_aot_function_table.h"
#include "backend_aot_reachability.h"

TZrBool backend_aot_compute_static_callable_reachability(SZrState *state,
                                                         const SZrAotFunctionTable *table,
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

#endif
