#ifndef ZR_TEST_SSA_SCCP_POOLED_BOOL_FIXTURE_H
#define ZR_TEST_SSA_SCCP_POOLED_BOOL_FIXTURE_H
#include <stdint.h>
#include "zr_vm_parser/exec_ir_pass_manager.h"
#include "exec_ir/exec_ir_mutation_internal.h"
#include "exec_ir/exec_ir_pass_internal.h"
#include "ssa_exec_ir_owned_module_allocator.h"

typedef struct SBpResult { unsigned safety, feature; } SBpResult;
typedef struct SBpTotals {
    unsigned attempts, live, errors, faultMeasured, faultCompleted;
} SBpTotals;
typedef struct SBpFixture {
    SZrExecIrModule module;
    SZrExecIrAnalysisCache cache;
    SZrExecIrPassContext context;
    SZrExecIrRemarkSink remarks;
    SZrExecIrOptimizeOptions options;
    SZrExecIrOptimizationResult result;
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrConstant prefix[128];
    SZrExecIrInstruction original;
    SZrExecIrOptimizationRemark existingRemark;
    unsigned initialCount, expected, resultId, appended, expectedIndex, boundary;
    TZrBool changed;
} SBpFixture;

void Bp_SetUp(void);
void Bp_TearDown(void);
SBpTotals Bp_Totals(void);
void Bp_NoteFault(unsigned measured, unsigned completed);
int Bp_Build(SBpFixture *f, unsigned predicate, TZrUInt64 left,
             TZrUInt64 right, unsigned expected);
int Bp_Live(const SBpFixture *f);
int Bp_Verify(SBpFixture *f);
int Bp_Run(SBpFixture *f, unsigned entry, unsigned writable);
int Bp_Check(const SBpFixture *f, unsigned folded);
int Bp_Partial(const SBpFixture *f);
int Bp_Dispose(SBpFixture *f);
SBpResult Bp_Semantic(unsigned id);
int Bp_Seal(SBpFixture *f);
int Bp_AddFunction(SBpFixture *f);
int Bp_NoOrphans(const SBpFixture *f);
SBpResult Bp_Advanced(unsigned id);
SBpResult Bp_Budget(void);
SBpResult Bp_Faults(void);
SBpResult Bp_PublicBudgets(void);
#endif
