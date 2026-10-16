#ifndef ZR_TEST_SSA_SOURCE_RETURNED_RETENTION_FIXTURE_H
#define ZR_TEST_SSA_SOURCE_RETURNED_RETENTION_FIXTURE_H
#include <stddef.h>
#include "ssa_source_returned_crt_ledger.h"
#include "zr_vm_parser/compiler.h"
#include "compiler/semantic/compiler_source_retention.h"
#include "zr_vm_core/function_aot_ir.h"

void ZrTests_Returned_Init(void);
void ZrTests_Returned_Open(void);
void ZrTests_Returned_Close(void);
void ZrTests_Returned_Shutdown(void);
void ZrTests_Returned_AssertBalanced(void);
SZrState *ZrTests_Returned_State(void);
SZrString *ZrTests_Returned_Name(void);
SZrFunction *ZrTests_Returned_Compile(unsigned slot, const char *source,
        TZrBool ordinary, const SZrSourceCompileOptions *options,
        SZrSourceCompileDiagnostic *diagnostic);
SZrFunction *ZrTests_Returned_Function(unsigned slot);
void ZrTests_Returned_FreeFunction(unsigned slot);
unsigned ZrTests_Returned_OwnerRecord(unsigned slot);
unsigned ZrTests_Returned_RecordFrees(unsigned kind, unsigned record);
unsigned ZrTests_Returned_VmAttempts(void);
typedef struct SZrReturnedVmStats {
    unsigned attempts, frees, live, errors, diagnosticCount, dropped;
} SZrReturnedVmStats;
/* Serialized read of the actual VM allocator counters, including after Shutdown
 * until Init. Output must be genuine independent writable storage. Null refuses
 * with no mutation; acceptance copies six counters, without allocation or Unity.
 * Attempts include realloc/free requests and do not count successful allocations.
 */
int ZrTests_Returned_TryVmStats(SZrReturnedVmStats *output);
SZrReturnedHeapStats ZrTests_Returned_Stats(unsigned kind);
SZrReturnedHeapEvent ZrTests_Returned_Event(unsigned kind, unsigned index);
void ZrTests_Returned_ArmDescriptor(unsigned site);
void ZrTests_Returned_ArmOwner(void);
void ZrTests_Returned_Disarm(void);
#endif
