#ifndef ZR_TEST_SSA_PHI_IDENTITY_SUPPORT_H
#define ZR_TEST_SSA_PHI_IDENTITY_SUPPORT_H
#include <stdint.h>
#include "ssa_phi_identity_fixture.h"
#include "exec_ir/exec_ir_mutation_internal.h"
#include "exec_ir/exec_ir_pass_internal.h"
#include "zr_vm_parser/exec_ir_state_maps.h"
#include "zr_vm_core/exec_ir_state_map.h"

enum { PIX_FUNCTIONS=2, PIX_BLOCKS=24, PIX_VALUES=64, PIX_INST=128,
       PIX_POOL=192, PIX_PHIS=24, PIX_META=16384, PIX_PATCHES=32,
       PIX_TRACE=8192, PIX_WORK=65536 };
typedef struct SPixPatch { unsigned slot,value; } SPixPatch;
typedef struct SPixPlan {
    unsigned patches;SPixPatch operand[PIX_PATCHES];
    unsigned incomingPatches;SPixPatch incoming[PIX_PATCHES];
    unsigned erased,erase[PIX_INST],removed,remove[PIX_PHIS];
    unsigned dom[PIX_BLOCKS];
} SPixPlan;
typedef struct SPixFixture {
    SPiFixture base;
    SPixPlan plan[PIX_FUNCTIONS];
    unsigned id,profile,functions,rejection,optional;
} SPixFixture;
typedef struct SPixFunctionSnapshot {
    SZrExecIrFunction header;
    SZrExecIrValue values[PIX_VALUES];
    SZrExecIrInstruction instructions[PIX_INST];
    SZrExecIrBlock blocks[PIX_BLOCKS];
    SZrExecIrPhi phis[PIX_PHIS];
    SZrExecIrPhiIncoming incoming[PIX_POOL];
    unsigned operands[PIX_POOL],results[PIX_POOL],memory[PIX_POOL],pred[PIX_POOL],succ[PIX_POOL];
    SZrExecIrSourceMap sources[PIX_POOL];
    unsigned metaSize;unsigned char metadata[PIX_META];
} SPixFunctionSnapshot;
typedef struct SPixSnapshot {
    SZrExecIrModule header;
    unsigned metaSize;unsigned char metadata[PIX_META];
    SPixFunctionSnapshot fn[PIX_FUNCTIONS];
    TZrUInt64 hash;
} SPixSnapshot;
typedef struct SPixChecks { unsigned core,parser; } SPixChecks;

void Pix_Init(SPixFixture *f,unsigned id,unsigned profile);
int Pix_Build(SPixFixture *f,unsigned id,unsigned profile,unsigned functions);
SZrExecIrFunction *Pix_AddFunction(SPixFixture *f,unsigned blocks);
unsigned Pix_Value(SZrExecIrFunction *fn,unsigned external,unsigned type);
unsigned Pix_Emit(SZrExecIrFunction *fn,unsigned opcode,unsigned a,unsigned b,unsigned result);
int Pix_Finish(SZrExecIrFunction *fn,unsigned block,unsigned start);
int Pix_Edges(SZrExecIrFunction *fn,unsigned block,const unsigned *targets,unsigned count);
int Pix_Phi(SZrExecIrFunction *fn,unsigned block,unsigned result,const unsigned *values,unsigned count);
int Pix_Diamond(SPixFixture *f,SZrExecIrFunction *fn,unsigned mode,unsigned reverse);
int Pix_AddOperandPatch(SPixPlan *p,unsigned slot,unsigned value);
int Pix_AddIncomingPatch(SPixPlan *p,unsigned slot,unsigned value);
int Pix_RemovePhi(SPixPlan *p,unsigned result);
int Pix_Erase(SPixPlan *p,unsigned instruction);
int Pix_EdgeShape(SPixFixture *f,SZrExecIrFunction *fn);
int Pix_Metadata(SPixFixture *f,SZrExecIrFunction *fn,unsigned profile);
int Pix_Live(const SPixFixture *f);
SPixChecks Pix_Verify(SPixFixture *f,unsigned writable);
TZrUInt64 Pix_Hash(const SPixFixture *f);
int Pix_Capture(const SPixFixture *f,SPixSnapshot *s);
int Pix_Equal(const SPixFixture *f,const SPixSnapshot *s);
int Pix_Expected(const SPixFixture *f,const SPixSnapshot *s,unsigned entry);
int Pix_Partial(const SPixFixture *f,const SPixSnapshot *s,unsigned entry);
int Pix_Run(SPixFixture *f,unsigned entry,unsigned writable);
int Pix_Observe(const SPixFixture *f,unsigned entry,TZrUInt64 before,unsigned firstRemark,unsigned expected);
unsigned Pix_Profiles(unsigned id);
unsigned Pix_Repeats(unsigned id);
void Pix_Row(unsigned id,unsigned profile,unsigned entry,unsigned writable,unsigned repeat,
             SPixChecks pre,unsigned run,SPixChecks post,unsigned exact,SPiResult r);
SPiResult Pix_Semantic(unsigned id,const char *name);
SPiResult Pix_Budget(unsigned id);
SPiResult Pix_Transactions(unsigned id);
#endif
