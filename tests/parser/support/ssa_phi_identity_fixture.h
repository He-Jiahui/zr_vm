#ifndef ZR_TEST_SSA_PHI_IDENTITY_FIXTURE_H
#define ZR_TEST_SSA_PHI_IDENTITY_FIXTURE_H
#include <stddef.h>
#include "zr_vm_parser/exec_ir_pass_manager.h"
#include "ssa_exec_ir_owned_module_allocator.h"

enum { PI_BLOCKS=4, PI_VALUES=4, PI_INST=4, PI_EDGES=4, PI_INCOMING=2 };
typedef struct SPiResult { unsigned safety,feature; } SPiResult;
typedef struct SPiTotals { unsigned attempts,live,errors,faultMeasured,faultCompleted; } SPiTotals;
typedef struct SPiFixture {
    SZrExecIrModule module;
    SZrExecIrRemarkSink remarks;
    SZrExecIrOptimizeOptions options;
    SZrExecIrOptimizationResult result;
    SZrExecIrDiagnostic diagnostic;
    unsigned a,p;
    SZrExecIrAnalysisCache cache;
    SZrExecIrPassContext context;
    TZrBool changed;
} SPiFixture;
typedef struct SPiSnapshot {
    SZrExecIrModule module;
    SZrExecIrFunction function;
    SZrExecIrBlock blocks[PI_BLOCKS];
    SZrExecIrInstruction instructions[PI_INST];
    SZrExecIrValue values[PI_VALUES];
    SZrExecIrPhi phi;
    SZrExecIrPhiIncoming incoming[PI_INCOMING];
    unsigned operands[2],preds[PI_EDGES],succs[PI_EDGES];
} SPiSnapshot;
void Pi_SetUp(void);
void Pi_TearDown(void);
int Pi_Dispose(SPiFixture *fixture);
int Pi_DisposeFault(SPiFixture *fixture,unsigned failures);
void Pi_NoteFault(unsigned measured,unsigned completed);
SPiTotals Pi_Totals(void);
int Pi_Live(const SPiFixture *fixture);
SPiResult Pi_Semantic(unsigned id,const char *name);
SPiResult Pi_Minimal(unsigned id,const char *name);
#endif
