---
related_code:
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/include/zr_vm_core/canonical_consumer.h
  - zr_vm_core/src/zr_vm_core/artifact_call_binding.c
  - zr_vm_core/src/zr_vm_core/artifact_encoding.c
  - zr_vm_core/src/zr_vm_core/artifact_rows.c
  - zr_vm_core/src/zr_vm_core/artifact_schema.c
  - zr_vm_core/src/zr_vm_core/canonical_consumer.c
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
  - zr_vm_parser/include/zr_vm_parser/artifact_projection.h
  - zr_vm_parser/src/zr_vm_parser/artifact_call_binding_projection.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_bindings.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_call_bindings.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/artifact_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/artifact_call_binding_projection.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_bindings.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_call_bindings.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
plan_sources:
  - user: 2026-09-06 W2 / Call Binding M1 static call binding and relocatable target table
tests:
  - tests/parser/test_call_binding_artifact.c
  - tests/parser/test_call_binding_aot_projection.c
  - tests/parser/test_aot_c_metadata_binding_loader.c
  - tests/module/test_metadata_runtime_method_binding.c
  - tests/acceptance/2026-09-06-call-binding-aot-runtime.md
  - tests/cmake/zr_vm_call_binding_artifact_tests.cmake
doc_type: module-detail
---

# Call Binding Artifact And AOT

## Persistent Record

Canonical artifact schema 5 adds `CALL_BINDING_TABLE` (section 21) to executable
`.zro` documents. This section is forbidden in `.zrs` and `.zri`. It uses the
same `SZrCallBindingContract` as the legacy `.zro` call-binding section, while
adding the enclosing function and callsite coordinates needed by a flat AOT
function table. Native C structure size and padding do not define the wire
format.

Each record is exactly 96 bytes, with little-endian integer fields:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | Call-binding schema version, currently 1 |
| 4 | 4 | Function index in the producer's function graph |
| 8 | 4 | Callsite cache index in that function |
| 12 | 4 | Executable instruction index |
| 16 | 64 | Encoded call-binding contract |
| 80 | 16 | Relocation kind, target index, owner depth, flags |

The 64-byte contract retains binding kind, member/signature/owner tokens,
signature hash, module hash, layout version/hash, dispatch slot, and operation.
Reserved fields must remain zero. Generation, cached receiver state, VM object
pointers, callbacks, AOT entry pointers, GC references, and hit counters are
absent from the wire row.

## Validation

`ZrCore_Artifact_WriteCallBindingRow` validates the contract and location before
writing. `ZrCore_Artifact_ReadCallBindingRow` checks section kind, element size,
row bounds, available bytes, schema version, token shape, contract completeness,
reserved fields, and relocation kind. A failed read clears the destination row.

The whole-section validators require rows in ascending `(functionIndex,
cacheIndex)` order. Duplicate or reordered callsites are errors in both source
documents and decoded bytes. This permits linear validation for untrusted large
tables. The generic artifact does not interpret its opaque code payload;
function-local instruction and constant bounds are checked by the parser
projection and the runtime linker.

`ZrParser_ArtifactCallBinding_BuildRows` supports a count-only query followed by
a caller-owned buffer. It validates every bound cache before copying any row.
The projection reads only the persistent contract and location. An invalid
instruction index or local constant index reports the cache index in the
artifact diagnostic. `SZrCanonicalConsumerProjection.callBindings` exposes the
validated optional section to AOT and other canonical consumers.

## AOT Registration

AOT ABI 16 adds the following fields to the code registration and compiled
module descriptors:

- `callBindingRows`: contiguous 96-byte persistent records;
- `callBindingRowCount` and `callBindingRowSize`;
- `callBindingTargetFunctionIndices`: one generated function index per record.

The C and LLVM emitters share the same projection. They preserve the source
tokens, signatures, hashes, and relocation location byte for byte. A separate
target-index table connects a local constant target to the retained AOT
function table. A module relocation has no local target index and uses the
existing invalid-function-index sentinel. The runtime loader validates its
contract and leaves interface/virtual selection deferred until receiver
dispatch.

Projection fails when a local static target no longer exists in the retained
function table. The generated table contains indices, and the generated thunk
table contains ordinary linker-resolved symbols. No compiler-process pointer
is formatted into generated C, LLVM IR, or canonical artifact bytes.

The ABI version increment prevents an old descriptor layout from being read as
the expanded layout. LLVM's explicit structure types are updated with the same
field order and widths as the C ABI declarations.

When call-binding rows exist, LLVM also publishes method-info entries and a
matching token array. These entries advertise runtime mapping only and use the
ordinary runtime value frame; reflective invocation keeps LLVM's existing
unsupported boundary. C and LLVM generic/meta call preparation consumes the
binding before entering the callee, preserving the receiver and captured
module context.

At runtime, `ZrCore_MetadataRuntime_ReadCallBindingView` decodes each 96-byte
row through the artifact schema and selects only the process-local thunk,
method-info, and invoker referenced by the target-function index table. Before
installing AOT targets, `ZrCore_MetadataRuntime_LinkCallBindings` rebuilds the
interpreter binding from the loaded function graph and compares its contract,
instruction coordinate, and relocation location with every registered row.
Only a complete independent match is promoted to an AOT target. The existing
VM callable object remains available to the interpreter and garbage
collector; a malformed, duplicated, reordered, or mismatched row invalidates
the linking pass with a structured call-binding diagnostic.

## Coverage

`call_binding_artifact` exercises fixed-width encoding, malformed tokens,
reserved fields, unsupported versions, truncated views, source cache projection,
copied buffers, duplicate callsites, forbidden artifact kinds, and the canonical
consumer section view. `call_binding_aot_projection` exercises a compiled member
call, preserved contracts and locations, missing retained targets, generated
registration data, runtime pointer selection, interpreter callable preservation,
and signature tampering rejection. `aot_c_metadata_binding_loader` validates
metadata compatibility failures and executes static calls, C/LLVM property
accessors and meta-calls. Its provider cases execute imported C functions,
static methods, and functions retaining module captures. Interface MODULE rows
start with `TARGET_NONE` and resolve during receiver dispatch. Runtime
generations, GC movement, and receiver guards have separate M1 runtime tests.
