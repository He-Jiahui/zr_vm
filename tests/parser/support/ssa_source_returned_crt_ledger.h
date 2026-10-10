#ifndef ZR_TEST_SSA_SOURCE_RETURNED_CRT_LEDGER_H
#define ZR_TEST_SSA_SOURCE_RETURNED_CRT_LEDGER_H
#include <stddef.h>
#include "zr_vm_common/zr_common_conf.h"

enum { ZR_RETURNED_DESCRIPTOR = 0, ZR_RETURNED_OWNER = 1, ZR_RETURNED_CRT_ROWS = 256 };
typedef struct SZrReturnedHeapStats {
    unsigned attempts, successes, frees, live, errors, failures;
} SZrReturnedHeapStats;
typedef struct SZrReturnedHeapEvent {
    size_t count, width, bytes;
    unsigned site, record;
    TZrBool injected;
} SZrReturnedHeapEvent;

/* Serialized nonasserting ledger reads. Output is genuine independent writable
 * storage; refusal preserves it. Reads allocate nothing, mutate no ledger state
 * and consume no fault. Counters survive recovery until an explicit Reset.
 */
int ZrTests_Returned_TryStats(unsigned kind, SZrReturnedHeapStats *output);
int ZrTests_Returned_TryRecordFrees(unsigned kind, unsigned record, unsigned *output);
int ZrTests_ReturnedCrt_TryEvent(unsigned kind, unsigned index, SZrReturnedHeapEvent *output);
/* Exact current-live allocation address lookup only; no pointer dereference,
 * numeric span admission, readability or typed ownership certification.
 */
int ZrTests_ReturnedCrt_TryFindLiveRecord(unsigned kind, const void *pointer, unsigned *output);

/* Internal fixture coordination. Stats requires a literal or validated kind<2;
 * it preserves the old direct counter read and gives no TryStats feature credit.
 * Reset requires both ledgers already have live==0. BeginCall requires a site
 * in [0,6], rebases both local site inventories and schedules only requested
 * faults; an unarmed argument does not clear a previously scheduled fault.
 * Disarm and Recover remain explicit. Recover records one error per affected
 * live ledger before freeing its current rows, without resetting observations.
 */
SZrReturnedHeapStats ZrTests_ReturnedCrt_Stats(unsigned kind);
void ZrTests_ReturnedCrt_Reset(void);
void ZrTests_ReturnedCrt_BeginCall(unsigned descriptorSite, TZrBool ownerFault);
void ZrTests_ReturnedCrt_Disarm(void);
void ZrTests_ReturnedCrt_Recover(void);

/* Only the real descriptor/owner translation units redirect these callbacks. */
void *ZrTests_ReturnedDescriptor_Calloc(size_t count, size_t width);
void ZrTests_ReturnedDescriptor_Free(void *pointer);
void *ZrTests_ReturnedOwner_Malloc(size_t bytes);
void ZrTests_ReturnedOwner_Free(void *pointer);
#endif
