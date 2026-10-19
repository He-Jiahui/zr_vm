---
related_code:
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - zr_vm_library/src/zr_vm_library/file.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_directory_listing.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_resource_roots_internal.h
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_directory_listing.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_resource_roots_internal.h
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h
  - tests/system/test_system_fs_resource_cleanup.c
  - tests/system/support/fs_resource_crt_observer.c
  - tests/system/support/fs_resource_crt_observer.h
plan_sources:
  - .codex/plans/20261016-review-bugfix.md
tests:
  - tests/system/test_system_fs_resource_cleanup.c
  - tests/cmake/system-fs-resource-cleanup-tests.cmake
doc_type: module-detail
---

# FS resource cleanup during allocation failures

This fixture exercises actual public module/method calls against actual Core,
Library, and System implementations. It adds no production hooks. The dedicated
`zr_vm_fs_resource_observed` object target compiles the checkout's `fs_stream.c`
and Library `file.c` directly. A forced header redirects their CRT
`malloc/realloc/free` calls to a fixed-storage observer that delegates to the
real CRT. The fixture and observer itself are not compiled with this redirection.
The executable links these actual objects before the remaining real static
libraries; their original archive members must not be pulled in a second time.
No production source is copied or replaced by a mock implementation.

The observer records the exact returned native pointer and counts its release.
It selects text buffers by `fs_stream.c` malloc size 258, read buffers by size
4096, raw-write buffers by size 257, and initial directory list reallocations in
`file.c` by `8 * sizeof(SZrLibrary_File_ListEntry)`. Reallocating a tracked block
updates the row; allocation failure preserves it. Freed rows remain present,
including across retry, so a successful retry cannot hide an earlier leak.
The observer never allocates. It does not use Core allocator totals as evidence
of native buffer release.

| Case | Public entry | Rejected Core allocation after the native resource exists |
| --- | --- | --- |
| `module_text` | `zr.system.fs.readText(path)` | STRING, 258 bytes |
| `file_text` | `File.readText()` | STRING, 258 bytes |
| `bytes_new` | `File.readBytes()` | OBJECT, sizeof(SZrObject) for the output array |
| `bytes_push` | `File.readBytes()` | HASH_PAIR, the first single-slot pair-pool block |
| `raw_append` | `File.appendBytes(array)` | HASH_PAIR, the 257-slot materialization pool block |
| `entries` | `Folder.entries()` | OBJECT, sizeof(SZrObject) for the output array |
| `glob` | `Folder.glob("*", false)` | OBJECT, sizeof(SZrObject) for the output array |

Pair-pool sizes use the production ABI's `SZrHashPairPoolBlock`,
`ZR_HASH_PAIR_POOL_INLINE_COUNT`, and `SZrHashKeyValuePair`. The raw argument is
an actual Array with an initialized empty node map. Its backing storage is
allocated by the public Core raw-capacity API, then populated with contiguous
byte integers and valid RAW_CANONICAL length/dirty metadata, following the
existing checkpoint fixture's representation preparation. Get/materialization
remain the actual production implementations. This is native public FS-call
coverage, not script constructor/bytecode lowering coverage.

Each case has `oom` and `normal` CTest registrations with prefix
`system_fs_resource_`, target `zr_vm_system_fs_resource_cleanup_test`. All
parameters, method resolution and receiver roots are prepared before arming.
The first rejection requires a matching live observed native resource. For file
operations on Windows it additionally requires sharing violation when opening
the exact fixture exclusively. Once hit, rejection stays persistent for that
category and size until the protected public call returns. Failure must expose
MemoryError and release its selected resources. A normal call after removing
the fault must succeed with the original input. The public dispatcher may
initialize the result slot; the test does not impose old-result preservation.

Every child fixture root must be absent; an existing root is rejected before
any write. The registered parent is
`E:/cargo-targets/zr_vm/tests/review-bugfix-20261016/fs-resource-cleanup`.
Each `<case>_<mode>/payload.bin` starts with exactly 257 ASCII `Q` bytes.
The raw append normal/retry success ends with 514 `Q` bytes. The test never
deletes files: the root runner records actual hashes after process exit before
cleaning its owned fixture paths. Before and after calls Windows probes use
the same volume serial and file-index identity, with share mode zero. This
detects a leaked handle for this particular fixture, not global process handle
counts. Directory enumeration releases its OS enumeration handle before VM
materialization; directory-list cleanup is therefore checked only by native
allocation observation, not file-handle probes.

Root's stable `integrated-gc-fs-support-red` run configured and built
successfully, with 1952 unchanged inputs. All seven OOM cases failed and all
seven normal controls passed. The two text cases left the exact fixture handle
open; the byte/list cases retained the actual observed CRT allocations. The
subsequent GC-only RED run completed before production edits began. Production
is now statically frozen for independent review and root GREEN validation; this
subagent has not run a native build or test. This group does not
claim forced moving/concurrent GC coverage, allocation-failure recovery of
other Library manual pins, rollback of file positions, close-failure behavior,
or atomic filesystem changes. Core allocator metadata and teardown are checked
separately and do not prove CRT/handle cleanup. Constructor-internal roots and raw node-map moving-pointer TODOs remain
outside this resource group.

## Resource ownership and implementation

`fs_file_io.c` owns the complete one-shot module/File text and byte I/O
responsibility, extracted together with its callbacks and direct function
contracts from `fs_entry.c`. The actual stream operation runs in a protected
callback with a context in its caller. After TryRun returns, the owner closes
its temporary handle exactly once. A captured status is rethrown unchanged
after closing. On ordinary false, the operation's errno wins over close errno;
on ordinary success, a failed close still returns false with its own errno.
The same adapter handles the existing text-write helper without changing its
strlen/partial-write behavior. No handle is transferred to a wrapper in these
one-shot operations. File.open's existing hidden-owner transaction is separate.

`fs_stream.c` keeps the real buffer allocations at their original translation
unit, so the frozen observer continues to see precisely the same native
resource classes. ReadBytes owns its read buffer until the protected output
array construction finishes or throws. WriteBytes owns its staging buffer
through raw materialization, byte validation, and write-all. Both free their
buffer before rethrowing the original status, preserve ordinary errno, and
publish the byte count only on success. Handles remain borrowed: a stream
operation does not close its caller's FileStream. One-shot helpers provide the
outer handle boundary. Already-read file positions, host opening side effects,
and partially written prefixes are not rolled back.

`fs_directory_listing.c` owns direct enumeration/filtering and glob callbacks,
also extracted as a complete responsibility. A successful Library list is
consumed by the protected array conversion; File_List_Free runs exactly once
on success, ordinary false, and caught Throw. Enumeration's ordinary failure
still follows its existing IOException path and Library partial-list cleanup.

The private root helper uses two allocation-free LOCAL_ADDRESS slots in a
caller-owned AOT frame for the in-progress array and current entry, or for a
borrowed input byte array. The frame is pushed before entering TryRun; its
snapshot therefore retains that still-live caller frame after a Throw.
Cleanup frees the native resource, pops the frame once, then propagates status.
Callbacks reload object pointers from those slots after allocating operations.
This avoids adding a VM stack allocation or explicit ignored root during
resource cleanup. It does not repair construction internals in Array_New,
NewEntryObject, raw materialization, or Library PushValue's own manual pins.
Those independent support-layer lifetimes retain their existing coverage limits.

The extraction reduces `fs_entry.c` to about 820 lines. `fs_stream.c` remains
about 1050 lines including its existing direct function documentation. It still
has its single persistent-stream and shared buffer-marshalling responsibility;
the narrow callbacks add exception cleanup to that responsibility. Keeping the
actual malloc/free operations here preserves the already-RED observer's source
identity and avoids an unnecessary test-oracle change. A future independent
buffer-I/O extraction should move that complete responsibility and explicitly
preserve equivalent actual-source observation.
