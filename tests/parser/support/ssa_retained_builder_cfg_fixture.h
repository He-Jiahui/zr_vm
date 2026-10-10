#ifndef ZR_TEST_SSA_RETAINED_BUILDER_CFG_FIXTURE_H
#define ZR_TEST_SSA_RETAINED_BUILDER_CFG_FIXTURE_H

#include "zr_vm_core/exec_ir_owned_module.h"

/* These are private canonical identities, not runtime type aliases. */
enum {
    ZR_RETAINED_CFG_BOOL_TYPE = 10,
    ZR_RETAINED_CFG_I64_TYPE = 11,
    ZR_RETAINED_CFG_GRAPH_COUNT = 4,
    ZR_RETAINED_CFG_CASE_COUNT = 8
};

typedef enum EZrRetainedCfgGraph {
    ZR_RETAINED_CFG_PROMOTED_DIAMOND = 0,
    ZR_RETAINED_CFG_PROMOTED_LOOP,
    ZR_RETAINED_CFG_PARAMETER_DIAMOND,
    ZR_RETAINED_CFG_PARAMETER_LOOP
} EZrRetainedCfgGraph;

typedef struct SZrRetainedCfgSpec {
    EZrRetainedCfgGraph graph;
    TZrMetadataToken token;
    TZrUInt64 signature;
} SZrRetainedCfgSpec;

/* Numerical observations survive cleanup; never retain producer addresses. */
typedef struct SZrRetainedCfgProduction {
    TZrUInt32 stage;
    TZrUInt32 producerAllocated;
    TZrUInt32 producerReleased;
    TZrUInt32 functionsBuilt;
    TZrUInt32 transientReleased;
    TZrUInt32 ownerPublished;
    SZrExecIrDiagnostic diagnostic;
} SZrRetainedCfgProduction;

/* observation and owner are exclusive non-overlapping live slots; observation
 * is non-NULL and owner initially contains NULL. Supported counts are 1 and 2.
 * On every return all SemIR producers and the transient module are released.
 * Success returns only an actual opaque Core owner. Failure leaves its slot
 * empty. The caller owns the slot and must release it before reporting. */
TZrBool ZrTest_RetainedCfgProduce(const SZrRetainedCfgSpec *specs,
                                TZrUInt32 count,
                                SZrExecIrOwnedModule **owner,
                                SZrRetainedCfgProduction *observation);

#endif
