#ifndef ZR_TEST_EXEC_IR_OWNER_CAPTURE_H
#define ZR_TEST_EXEC_IR_OWNER_CAPTURE_H
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"
enum { ZR_OWNER_WORDS = 4096 };
typedef struct SZrOwnerContents { TZrUInt64 words[ZR_OWNER_WORDS]; unsigned count; } SZrOwnerContents;
/* Serialized logical observation of a genuine live, readable typed module graph.
 * Every selected pool must have a real readable span for its count; capacities
 * do not prove pointer validity, ownership, schema validity or Source identity.
 * Present gcMap/stateMap/frameLayout objects must themselves be readable.
 * Contents must be genuine independent writable storage disjoint from the graph.
 * Success returns 1 with the logical fields in legacy order; refusal returns 0
 * preserving the complete output representation and every graph/ledger byte.
 * No allocation, Unity assertion, reset, fault scheduling or Core call occurs. */
int ZrTests_ExecOwner_TryCapture(const SZrExecIrModule *module, SZrOwnerContents *contents);
/* Internal coordination: callers use only literal checkedGeometry 0 or 1 and
 * a trusted emitter/context. The emitter returns 1 after consuming one word,
 * or 0 to stop; successful words already emitted are not rolled back.
 * Checked 1 validates pool shapes and the 4096-word budget before payload reads.
 * Checked 0 requires the legacy readable graph preconditions and bypasses every
 * added shape/budget guard, leaving bounds/assertions to the legacy emitter. */
int ZrTests_ExecOwner_VisitContents(const SZrExecIrModule *module, int checkedGeometry,
                                  int (*emit)(void *, TZrUInt64), void *context);
#endif
