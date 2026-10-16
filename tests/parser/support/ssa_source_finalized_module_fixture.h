#ifndef ZR_TEST_SOURCE_FINALIZED_MODULE_FIXTURE_H
#define ZR_TEST_SOURCE_FINALIZED_MODULE_FIXTURE_H
#include <stddef.h>
#include <stdint.h>
#include "tests/core/support/ssa_exec_ir_owned_module_fixture.h"
#include "zr_vm_core/exec_ir_module_copy.h"
#include "zr_vm_core/state.h"
#include "exec_ir/source/exec_ir_source_finalized_module_internal.h"
#include "exec_ir/source/exec_ir_source_frame.h"
enum { FM_BYTES = 65536, FM_LIVE = 256, FM_RETURN = 113, FM_CALLABLE = 271 };
/* Declared readable lower consistency witnesses; no VM or semantic producer history. */
typedef struct SFmFixture {
    SZrState state;
    SZrSemanticContext context;
    SZrCanonicalTypeNode nodes[3];
    SZrSemanticSymbolRecord symbols[3];
    SZrSemanticIrFunction semantic;
    SZrSemanticIrInstruction instructions[4];
    SZrSemanticIrValue values[2];
    SZrSemanticIrSourceMapEntry maps[4];
    SZrSemanticIrRegion regions[2];
    SZrParserPlace places[2];
    SZrParserCfgBlock blocks[2];
    TZrValueId operands[2];
    SZrAstNode script, returned, integer;
    SZrAstNodeArray statements;
    SZrAstNode *statementNodes[2];
    SZrFunctionSourceCallableIdentity identity;
    SZrArray constants;
    SZrTypeValue *compilerPool, *functionPool;
    SZrExecIrModule original, candidate, scratch;
    SZrExecIrFunction finalized, temporary;
    SZrExecIrOwnedModule *output, *directOwner;
} SFmFixture;
typedef struct SFmObservation {
    unsigned char bytes[FM_BYTES];
    size_t length;
    unsigned error;
    SZrOwnerHeapStats heap;
    SZrOwnerContents output;
    TZrBool hasOutput;
    uintptr_t outputAddress;
    unsigned live[FM_LIVE], liveCount;
} SFmObservation;
typedef struct SFmResult {
    SFmObservation before, after;
    SZrExecIrDiagnostic diagnostic;
    TZrBool success;
} SFmResult;
extern SFmFixture g_fm;
extern SZrExecIrModule g_fmModuleOwner;
extern SZrExecIrFunction g_fmFunctionOwner;
extern SZrExecIrFunction g_fmOriginalFunctionOwner;
extern SZrExecIrFrameLayout g_fmFrameOwner;
void Fm_SetUp(void);
void Fm_TearDown(void);
void Fm_ReleaseInputs(void);
void Fm_ClearDeclared(void);
unsigned Fm_DeferredErrors(void);
void Fm_Refresh(void);
void Fm_RegisterOutput(unsigned first, unsigned last);
TZrBool Fm_InputLive(void);
TZrBool Fm_OutputLive(void);
TZrBool Fm_OutputReleased(void);
void Fm_Spare(SZrExecIrFunction *function);
void Fm_Bits(TZrInt64 value);
void Fm_EmptyMaps(void);
SZrArray Fm_Array(void *head, size_t width, size_t length, size_t capacity);
SZrExecIrDiagnostic Fm_Error(EZrExecutionDiagnosticCode code);
void Fm_Diagnostic(const SZrExecIrDiagnostic *expected, const SZrExecIrDiagnostic *actual);
void Fm_Observe(SFmObservation *observation);
void Fm_Unchanged(const SFmObservation *before, const SFmObservation *after, TZrBool output);
SFmResult Fm_Call(void);
SFmResult Fm_CallWithFault(unsigned site);
void Fm_Refused(const SFmResult *result, const SZrExecIrDiagnostic *expected);
void Fm_Feature(void);
void Fm_Published(void);
void Fm_OutputCapture(SZrOwnerContents *contents);
void Fm_Faults(void);
TZrBool Fm_Direct(unsigned *phase, SZrExecIrDiagnostic *diagnostic);
#endif
