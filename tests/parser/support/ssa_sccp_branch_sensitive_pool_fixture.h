#ifndef ZR_TEST_SCCP_BRANCH_SENSITIVE_POOL_H
#define ZR_TEST_SCCP_BRANCH_SENSITIVE_POOL_H
#include <stdint.h>
#include <stddef.h>
#include "zr_vm_parser/exec_ir_pass_manager.h"
#include "exec_ir/exec_ir_mutation_internal.h"
#include "exec_ir/exec_ir_pass_internal.h"
#include "ssa_exec_ir_owned_module_allocator.h"
enum { BE_OWNER_LIMIT=64, BE_TARGET_LIMIT=2, BE_TRACE_LIMIT=2048 };
typedef struct SBeResult { unsigned safety,feature; } SBeResult;
typedef struct SBeOwner { uintptr_t address; unsigned count,capacity,record; } SBeOwner;
typedef struct SBeSnapshot { SBeOwner owners[BE_OWNER_LIMIT]; unsigned count; TZrUInt64 hash; } SBeSnapshot;
typedef struct SBeTarget { unsigned function,source,fold; TZrUInt64 bits; SZrExecIrInstruction original; } SBeTarget;
typedef struct SBeCompare { unsigned function,instruction,expected; SZrExecIrInstruction original; } SBeCompare;
typedef struct SBeFixture {
    SZrExecIrModule module;
    SZrExecIrAnalysisCache cache;
    SZrExecIrPassContext context;
    SZrExecIrRemarkSink remarks;
    SZrExecIrPassFailure failure;
    SZrExecIrOptimizeOptions options;
    SZrExecIrOptimizationResult result;
    SZrExecIrDiagnostic diagnostic;
    SBeTarget targets[BE_TARGET_LIMIT]; unsigned targetCount,source,initialCount;
    SBeCompare compares[BE_TARGET_LIMIT]; unsigned compareCount;
    SZrExecIrConstant initial[16];
    TZrUInt64 cfgHash;
} SBeFixture;
typedef struct SBeTotals { unsigned attempts,live,errors,faultMeasured,faultCompleted; } SBeTotals;
void Be_SetUp(void);
void Be_TearDown(void);
SBeTotals Be_Totals(void);
void Be_NoteFault(unsigned measured,unsigned completed);
int Be_Build(SBeFixture *f,unsigned id,unsigned profile);
int Be_Verify(SBeFixture *f);
int Be_Optimize(SBeFixture *f,unsigned writable);
int Be_Private(SBeFixture *f,unsigned entry,unsigned writable);
int Be_Dispose(SBeFixture *f);
int Be_Check(SBeFixture *f,unsigned requireFold);
int Be_Snapshot(const SBeFixture *f,SBeSnapshot *s);
int Be_OwnersEqual(const SBeFixture *f,const SBeSnapshot *s);
TZrUInt64 Be_Hash(const SBeFixture *f);
void Be_Profile(unsigned id,const char *name,SBeResult r);
SBeResult Be_Matrix(unsigned id);
SBeResult Be_Budget(unsigned id);
SBeResult Be_Faults(unsigned id);
#endif
