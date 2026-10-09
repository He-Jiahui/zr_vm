#ifndef ZR_TEST_FUNCTION_EXEC_IR_FIXTURE_H
#define ZR_TEST_FUNCTION_EXEC_IR_FIXTURE_H
#include "ssa_exec_ir_owned_module_fixture.h"
#include "ssa_function_aot_owner_fixture.h"
#include "ssa_function_exec_ir_owner_ledger.h"
#include "zr_vm_core/function_exec_ir.h"
#include "zr_vm_core/function_aot_ir.h"
#include "zr_vm_core/memory.h"
enum { ZR_FN_EXEC_FUNCTIONS=16,ZR_FN_EXEC_PRODUCERS=4 };
extern SZrFunction *g_fnExecFunctions[ZR_FN_EXEC_FUNCTIONS];
extern SZrTestsFunctionAotProducer *g_fnAotProducers[ZR_FN_EXEC_PRODUCERS];
void ZrTests_FnExec_SetUp(void);
void ZrTests_FnExec_TearDown(void);
SZrState *ZrTests_FnExec_State(void);
SZrGlobalState *ZrTests_FnExec_Global(void);
SZrFunction *ZrTests_FnExec_New(unsigned slot);
void ZrTests_FnExec_Free(unsigned slot);
void ZrTests_FnExec_Shutdown(void);
SZrTestsFunctionAotProducer *ZrTests_FnExec_AotProducer(unsigned slot,TZrInt64 literal);
unsigned ZrTests_FnExec_AttachAot(SZrFunction *function,const SZrAotIrModule *source);
const SZrExecIrModule *ZrTests_FnExec_Attach(SZrFunction *function,const SZrExecIrModule *source);
SZrFunction *ZrTests_FnExec_Move(SZrFunction *parent,SZrFunction *donor);
#endif
