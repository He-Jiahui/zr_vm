---
related_code:
  - zr_vm_core/src/zr_vm_core/exception/exception.c
  - zr_vm_core/src/zr_vm_core/exception/exception_errors.c
  - zr_vm_core/src/zr_vm_core/exception/exception_roots_internal.h
  - zr_vm_core/src/zr_vm_core/exception/exception_error_internal.h
  - zr_vm_core/src/zr_vm_core/exception/exception_try_run.c
  - tests/core/test_exception_construction_roots.c
  - tests/cmake/exception-construction-roots-tests.cmake
implementation_files:
  - zr_vm_core/src/zr_vm_core/exception/exception_errors.c
  - zr_vm_core/src/zr_vm_core/exception/exception_roots_internal.h
  - zr_vm_core/src/zr_vm_core/exception/exception_error_internal.h
plan_sources:
  - .codex/plans/20261016-review-bugfix.md
tests:
  - tests/core/test_exception_construction_roots.c
  - tests/cmake/exception-construction-roots-tests.cmake
doc_type: module-detail
---

# Error construction during collection

An allocation failure can run a full collection before Error construction has
published `currentException`. A C pointer to the new Error, frame array, or frame
object does not keep it alive. Previously, failure while allocating the frame
array collected its Error; failure while creating the `stacks` key collected
both the Error and its unpublished array.

Construction now registers local roots before the next allocating operation.
The root frame uses four `LOCAL_ADDRESS` entries and allocates no memory, so it
also works while the allocator is rejecting requests. Tagged inputs are copied
before borrowing their original storage can become invalid. Their collectable
object pointers have separate root slots; each subsequent use reloads the
collector's updated pointer. Scalars never enter object-root slots.

Each helper protects its own recipient and borrowed inputs. Stack capture also
protects the array, metadata function, and current frame until the array owns
the frame. The outer constructor retains the Error until its completed fields
are installed and it becomes the state's current exception. Ordinary returns
pop the local frame. A Throw uses the existing TryRun snapshot restoration
before abandoning its C locals.

Error construction and normalization live in `exception_errors.c`; exception
transfer and host text formatting remain in `exception.c`. The two private
inspection helpers are shared through `exception_error_internal.h`. This
separation reduces the original 1054-line file without changing public names.

## Validation and limits

The allocator-driven regression constructs a real MemoryError through Throw.
It persistently rejects either the next frame array or the first `stacks` key,
allows the production full collection to finish, and watches actual allocator
free callbacks for the live construction objects. It also checks recovery and
root-chain restoration, successful retry, ordinary construction, and complete
Core allocator teardown.

The stable `exception-thread-red-r2` run observed one full collection in each
failure case and premature frees of one and two construction objects. The stable support-batch-6-green run passes both failure cases and normal
control; it also passes 15 real exception contracts, including frame order.
All 168 CTest entries, pool14, CLI hello and exceptions15 pass, with 1944
input hashes unchanged. These tests do not establish that
all borrowed values or native buffers in unhandled text formatting are safe,
nor do they prove moving collection under concurrent mutators.
