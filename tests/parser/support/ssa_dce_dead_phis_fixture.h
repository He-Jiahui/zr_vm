#ifndef ZR_TEST_SSA_DCE_DEAD_PHIS_FIXTURE_H
#define ZR_TEST_SSA_DCE_DEAD_PHIS_FIXTURE_H
#include <stddef.h>
#include <stdint.h>
#include "zr_vm_parser/exec_ir_pass_manager.h"
#include "zr_vm_parser/exec_ir_state_maps.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "exec_ir/exec_ir_pass_internal.h"
#include "exec_ir/exec_ir_mutation_internal.h"
#include "ssa_exec_ir_owned_module_allocator.h"
enum { DP_INST=96, DP_BLOCK=12, DP_PHI=12, DP_INCOMING=48, DP_OWNER=128,
       DP_MAX_INSTRUCTIONS=DP_INST, DP_MAX_BLOCKS=DP_BLOCK, DP_MAX_PHIS=DP_PHI,
       DP_MAX_INCOMING=DP_INCOMING, DP_MAX_OWNERS=DP_OWNER, DP_MAX_FUNCTIONS=2 };
typedef struct SDpResult { unsigned safety,feature; } SDpResult;
typedef struct SDpOwner { uintptr_t address; size_t bytes; unsigned record; } SDpOwner;
typedef struct SDpSnapshot { SDpOwner owners[DP_OWNER]; unsigned count; TZrUInt64 hash; } SDpSnapshot;
typedef struct SDpExpected {
    unsigned instructionCount,valueCount,blockCount,incomingCount,phiCount;
    unsigned survivingCount,surviving[DP_PHI],phiBlock[DP_PHI],erase[DP_INST];
    SZrExecIrPhi phis[DP_PHI];
    SZrExecIrInstruction instructions[DP_INST];
    SZrExecIrBlock blocks[DP_BLOCK];
    SZrExecIrPhiIncoming incoming[DP_INCOMING];
    unsigned predCount,succCount,preds[DP_INCOMING],succs[DP_INCOMING],root;
} SDpExpected;
typedef struct SDpFixture {
    SZrExecIrModule module;
    SZrExecIrPassContext context;
    SZrExecIrAnalysisCache cache;
    SZrExecIrRemarkSink remarks;
    SZrExecIrOptimizeOptions options;
    SZrExecIrOptimizationResult result;
    SZrExecIrDiagnostic diagnostic;
    SDpExpected expected[2];
    unsigned profile,source,changed;
} SDpFixture;
typedef struct SDpTotals { unsigned attempts,live,errors,faultMeasured,faultCompleted; } SDpTotals;
void Dp_SetUp(void);
void Dp_TearDown(void);
SDpTotals Dp_Totals(void);
void Dp_FaultCount(unsigned measured,unsigned completed);
int Dp_Build(SDpFixture *f,unsigned id,unsigned profile,unsigned functions);
int Dp_Verify(SDpFixture *f);
int Dp_Run(SDpFixture *f,unsigned entry,unsigned writable);
int Dp_Live(const SDpFixture *f);
int Dp_Snapshot(const SDpFixture *f,SDpSnapshot *s);
int Dp_SameOwners(const SDpFixture *f,const SDpSnapshot *s);
TZrUInt64 Dp_Hash(const SDpFixture *f);
int Dp_ObserveChange(const SDpFixture *f,unsigned entry,unsigned expected,TZrUInt64 before);
int Dp_Stable(const SDpFixture *f);
int Dp_Expected(const SDpFixture *f);
int Dp_Remarks(SDpFixture *f);
int Dp_RemarkPrefix(const SDpFixture *f);
int Dp_Dispose(SDpFixture *f);
void Dp_Profile(unsigned id,const char *name,unsigned entry,unsigned writable,SDpResult r);
SDpResult Dp_Semantic(unsigned id);
SDpResult Dp_Faults(unsigned id);
#endif
