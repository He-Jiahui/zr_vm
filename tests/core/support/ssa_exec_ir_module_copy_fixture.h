#ifndef ZR_TEST_MODULE_COPY_FIXTURE_H
#define ZR_TEST_MODULE_COPY_FIXTURE_H
#include <stdint.h>
#include "ssa_exec_ir_owned_module_fixture.h"
#include "zr_vm_core/exec_ir_module_copy.h"
enum { ZR_COPY_OUTPUTS=4,ZR_COPY_FUNCTIONS=2,ZR_COPY_RECORDS=256 };
typedef struct SZrCopyState { SZrOwnerContents contents;unsigned records[ZR_COPY_RECORDS],count;uintptr_t addresses[ZR_COPY_RECORDS];unsigned addressCount; } SZrCopyState;
extern SZrExecIrModule g_copyOutputs[ZR_COPY_OUTPUTS];
extern SZrExecIrFunction g_copyFunctions[ZR_COPY_FUNCTIONS];
void ZrTests_ModuleCopy_SetUp(void);
void ZrTests_ModuleCopy_TearDown(void);
void ZrTests_ModuleCopy_Inputs(unsigned mode);
SZrExecIrModule *ZrTests_ModuleCopy_Source(void);
SZrExecIrFunction *ZrTests_ModuleCopy_Replacement(void);
void ZrTests_ModuleCopy_State(const SZrExecIrModule *module,unsigned first,unsigned last,SZrCopyState *state);
void ZrTests_ModuleCopy_Unchanged(const SZrCopyState *state,const SZrExecIrModule *module);
void ZrTests_ModuleCopy_Released(const SZrCopyState *state);
void ZrTests_ModuleCopy_OutputState(unsigned slot,SZrCopyState *state);
void ZrTests_ModuleCopy_InputState(unsigned slot,SZrCopyState *state);
void ZrTests_ModuleCopy_SetOutputRange(unsigned slot,unsigned first,unsigned last);
void ZrTests_ModuleCopy_Require(const SZrExecIrModule *source,TZrExecIrFunctionId id,const SZrExecIrFunction *replacement,unsigned output);
void ZrTests_ModuleCopy_Expected(const SZrExecIrModule *source,TZrExecIrFunctionId id,const SZrExecIrFunction *replacement,SZrOwnerContents *contents);
void ZrTests_ModuleCopy_ZeroDiagnostic(const SZrExecIrDiagnostic *diagnostic);
void ZrTests_ModuleCopy_Published(const SZrExecIrModule *source,unsigned output,const SZrCopyState *old,const SZrOwnerContents *expected);
#endif
