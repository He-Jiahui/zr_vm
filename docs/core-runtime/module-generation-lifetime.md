---
doc_type: core-runtime-contract
status: implemented
---

# Module generation lifetime

`hotpatch_generation.h` provides a fixed-capacity process-local generation
registry. A validated patch is first copied into a `PREPARED` record and only
then published with an acquire/release active pointer swap. Preparation failure
does not affect the active generation.

Entering from a host call uses `AcquireActive`; the registry lock protects the
active-pointer read and lease increment as one operation. Existing frames may
retain an older handle and continue resolving that immutable version while new
entries observe the new active generation. Handles include the generation
number, so reclaimed records cannot satisfy an old (ABA) link.

Retired records are reclaimed only when their lease count reaches zero and
they are no longer active. `CollectRetired` clears such records for reuse;
stale or non-leased handles return a structured `STALE_LINK`/state error.
Rollback and publication callers must allocate a fresh generation rather than
mutating an existing record in place.
