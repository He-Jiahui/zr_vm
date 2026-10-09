#ifndef ZR_TEST_SSA_SOURCE_RETURNED_RETENTION_FIXTURE_H
#define ZR_TEST_SSA_SOURCE_RETURNED_RETENTION_FIXTURE_H
#include <stddef.h>
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/compiler_source_retention.h"
#include "zr_vm_core/function_aot_ir.h"

enum { ZR_RETURNED_DESCRIPTOR = 0, ZR_RETURNED_OWNER = 1 };
typedef struct SZrReturnedHeapStats {
    unsigned attempts, successes, frees, live, errors, failures;
} SZrReturnedHeapStats;
typedef struct SZrReturnedHeapEvent {
    size_t count, width, bytes;
    unsigned site, record;
    TZrBool injected;
} SZrReturnedHeapEvent;
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
SZrReturnedHeapStats ZrTests_Returned_Stats(unsigned kind);
SZrReturnedHeapEvent ZrTests_Returned_Event(unsigned kind, unsigned index);
void ZrTests_Returned_ArmDescriptor(unsigned site);
void ZrTests_Returned_ArmOwner(void);
void ZrTests_Returned_Disarm(void);
/* Root forces its private shims into only the real descriptor/owner objects. */
void *ZrTests_ReturnedDescriptor_Calloc(size_t count, size_t width);
void ZrTests_ReturnedDescriptor_Free(void *pointer);
void *ZrTests_ReturnedOwner_Malloc(size_t bytes);
void ZrTests_ReturnedOwner_Free(void *pointer);
#endif
