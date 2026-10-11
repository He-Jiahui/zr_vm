#ifndef ZR_TEST_EXEC_IR_OWNER_ALLOCATOR_H
#define ZR_TEST_EXEC_IR_OWNER_ALLOCATOR_H
#include <stddef.h>
typedef struct SZrOwnerHeapStats { unsigned attempts, records, live, frees, errors, failures; } SZrOwnerHeapStats;
typedef struct SZrOwnerHeapEvent { unsigned kind, record, failed; size_t count, width, bytes; } SZrOwnerHeapEvent;
void *ZrTests_ExecOwner_Malloc(size_t bytes);
void *ZrTests_ExecOwner_Calloc(size_t count, size_t width);
void *ZrTests_ExecOwner_Realloc(void *pointer, size_t bytes);
void ZrTests_ExecOwner_Free(void *pointer);
void ZrTests_ExecOwner_Reset(void);
void ZrTests_ExecOwner_Arm(unsigned relativeAttempt);
void ZrTests_ExecOwner_Disarm(void);
SZrOwnerHeapStats ZrTests_ExecOwner_Stats(void);
SZrOwnerHeapEvent ZrTests_ExecOwner_Event(unsigned index);
/* Serialized coordination over the actual owner ledger; no Unity assertions.
 * TryArm requires no armed fault, a positive relative attempt and remaining
 * private event capacity. Acceptance explicitly schedules that fault, changes
 * no statistics or event rows, and allocates nothing. Refusal returns 0 with
 * the complete ledger and fault schedule unchanged.
 * TryEvent requires genuine independent writable output and an existing event
 * index within the private bound. Success returns 1 with the actual event copy;
 * refusal returns 0 preserving the complete output representation. This pure
 * query allocates nothing and changes no ledger bytes or fault schedule.
 */
int ZrTests_ExecOwner_TryArm(unsigned relativeAttempt);
int ZrTests_ExecOwner_TryEvent(unsigned index, SZrOwnerHeapEvent *output);
unsigned ZrTests_ExecOwner_RecordFrees(unsigned record);
/* Nonasserting read; invalid record/output leaves the output unchanged. */
int ZrTests_ExecOwner_TryRecordFrees(unsigned record, unsigned *frees);
typedef struct SZrOwnerLiveSpan {
    unsigned record;
    const void *allocation;
    size_t bytes;
} SZrOwnerLiveSpan;
/* Serialized test-ledger query; output is genuine writable independent storage.
 * Finds unique current live containment globally, then requires its record in
 * the half-open [first,last) window. Numeric containment proves neither pointer
 * readability nor typed owner authenticity. Failure preserves output; this
 * nonasserting query allocates nothing and changes no ledger state.
 */
int ZrTests_ExecOwner_TryLiveSpan(unsigned first, unsigned last, const void *pointer,
                               size_t count, size_t width, SZrOwnerLiveSpan *output);
void ZrTests_ExecOwner_AssertReleased(unsigned first, unsigned last);
void ZrTests_ExecOwner_AssertLive(unsigned first, unsigned last);
void ZrTests_ExecOwner_Recover(void);
void ZrTests_ExecOwner_AssertBalanced(void);
#endif
