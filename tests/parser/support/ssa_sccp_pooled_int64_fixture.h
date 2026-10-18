#ifndef ZR_TEST_SSA_SCCP_POOLED_INT64_FIXTURE_H
#define ZR_TEST_SSA_SCCP_POOLED_INT64_FIXTURE_H
#include <stdint.h>
#include <stddef.h>
#include "zr_vm_parser/exec_ir_pass_manager.h"
#include "exec_ir/exec_ir_mutation_internal.h"
#include "exec_ir/exec_ir_pass_internal.h"
#include "ssa_exec_ir_owned_module_allocator.h"
/* Pure metadata only; no evaluator, runtime, Oracle or execution provider. */
enum { PI_MAX_TARGETS = 80, PI_MAX_OWNERS = 100, PI_MAX_CONSTANTS = 48,
       PI_PHI_PROFILES = 4, PI_GREEN_PROFILES = 113 };
typedef struct SPiTarget {
    unsigned function, source, fold, type;
    TZrUInt64 bits;
    SZrExecIrInstruction original;
} SPiTarget;
typedef struct SPiOwner {
    uintptr_t address;
    unsigned count, capacity, record;
    size_t width;
} SPiOwner;
typedef struct SPiSnapshot {
    SPiOwner owners[PI_MAX_OWNERS];
    unsigned ownerCount, constantCount, functionCount;
    TZrUInt64 hash;
} SPiSnapshot;
typedef struct SPiFixture {
    SZrExecIrModule module;
    SZrExecIrAnalysisCache cache;
    SZrExecIrPassContext context;
    SZrExecIrRemarkSink remarks;
    SZrExecIrPassFailure failure;
    SZrExecIrOptimizeOptions options;
    SZrExecIrOptimizationResult result;
    SZrExecIrDiagnostic diagnostic;
    SPiTarget targets[PI_MAX_TARGETS];
    unsigned targetCount, source, initialCount;
    SZrExecIrConstant initial[PI_MAX_CONSTANTS];
} SPiFixture;
typedef struct SPiResult {
    unsigned safety, feature;
} SPiResult;
typedef struct SPiTotals {
    unsigned attempts, live, errors, faultMeasured, faultCompleted;
} SPiTotals;
void Pi_SetUp(void);
void Pi_TearDown(void);
SPiTotals Pi_Totals(void);
void Pi_NoteFault(unsigned measured, unsigned completed);
void Pi_Init(SPiFixture *f);
int Pi_Build(SPiFixture *f, unsigned id, unsigned profile);
int Pi_PrepareRemarks(SPiFixture *f);
int Pi_Verify(SPiFixture *f);
int Pi_Optimize(SPiFixture *f, unsigned writable);
int Pi_CheckTargets(const SPiFixture *f);
int Pi_CheckPartialTargets(const SPiFixture *f);
int Pi_CheckPrefix(const SPiFixture *f);
int Pi_NoOrphans(const SPiFixture *f);
int Pi_Snapshot(const SPiFixture *f, SPiSnapshot *s);
int Pi_CurrentLive(const SPiFixture *f);
int Pi_OwnersEqual(const SPiFixture *f, const SPiSnapshot *s);
TZrUInt64 Pi_Hash(const SPiFixture *f);
int Pi_Dispose(SPiFixture *f);
void Pi_Profile(unsigned id, const char *name, SPiResult r);
SPiResult Pi_Matrix(unsigned id);
SPiResult Pi_Faults(unsigned id);
#endif
