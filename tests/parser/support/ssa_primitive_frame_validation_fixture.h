#ifndef ZR_TEST_PRIMITIVE_FRAME_VALIDATION_FIXTURE_H
#define ZR_TEST_PRIMITIVE_FRAME_VALIDATION_FIXTURE_H
#include "tests/core/support/ssa_exec_ir_owned_module_allocator.h"
#include "zr_vm_parser/canonical_type.h"
#include "exec_ir/exec_ir_frame_layout.h"
#include "exec_ir/source/exec_ir_source_frame.h"
enum { PF_BYTES = 8192 };
typedef struct SPfObservation {
    unsigned char bytes[PF_BYTES];
    unsigned length, error;
    SZrOwnerHeapStats heap;
} SPfObservation;
typedef struct SPfResult {
    SPfObservation before, after;
    SZrExecIrDiagnostic diagnostic;
    TZrBool success;
} SPfResult;
extern SZrExecIrFunction g_pfFunctions[2];
extern SZrSemanticContext g_pfContext;
extern SZrCanonicalTypeNode g_pfNodes[2];
extern SZrExecIrLayout g_pfRows[2];
extern SZrExecIrPackedFrameLayout g_pfPacked;
void Pf_SetUp(void);
void Pf_TearDown(void);
void Pf_Diagnostic(const SZrExecIrDiagnostic *expected, const SZrExecIrDiagnostic *actual);
SZrExecIrDiagnostic Pf_Error(EZrExecutionDiagnosticCode code);
void Pf_Observe(SPfObservation *observation);
void Pf_Healthy(const SPfObservation *before, const SPfObservation *after);
SPfResult Pf_Call(TZrUInt32 limit);
void Pf_Refused(const SPfResult *result, const SZrExecIrDiagnostic *expected);
void Pf_Feature(void);
void Pf_AttachFaults(void);
void Pf_Request(SZrExecIrPackedFrameRequest *request, SZrExecIrPackedValue values[2], TZrUInt32 limit);
SZrOwnerHeapEvent Pf_FailedEvent(unsigned first, unsigned last);
#endif
