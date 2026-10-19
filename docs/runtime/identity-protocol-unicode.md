---
related_code:
  - zr_vm_core/include/zr_vm_core/raw_object.h
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/utf8.c
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/object.h
implementation_files:
  - zr_vm_core/include/zr_vm_core/raw_object.h
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/utf8.c
plan_sources:
  - user: 2026-10-12 audit request to correct object identity hashing, module protocol reconstruction, and Unicode scalar encoding
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/object.h
tests:
  - tests/core/test_identity_protocol_unicode.c
doc_type: module-detail
---

# Core Identity, Protocol, and UTF-8 Scalar Contracts

## Purpose

Three core boundaries share small but important representation contracts. Raw objects need a stable per-object identity hash for identity-keyed maps. Compiled prototype metadata stores protocol membership as a bit mask that module materialization must reproduce completely. UTF-8 encoding accepts Unicode scalar values, which exclude surrogate code points and values above U+10FFFF.

## Related Files

`SZrRawObject.hash` is initialized by `ZrCore_RawObject_Construct`; string construction may replace that identity hash with a content hash. Compiled prototype records use `SZrCompiledPrototypeInfo.protocolMask`, and runtime protocol queries use `ZrCore_ObjectPrototype_ImplementsProtocol`. `ZrCore_Utf8_EncodeCodePoint` is the shared encoder for string formatting and character append paths.

## Behavior Model

A newly constructed raw object receives `((TZrUInt64)objectAddress) / ZR_ALIGN_SIZE`. The hash identifies the object allocation and remains independent of the constructor's local parameter slot. Types with value-based hashes, such as strings, may explicitly replace it through `ZrCore_RawObject_InitHash`.

Prototype materialization iterates protocol identifiers from the first valid ID through `ZR_PROTOCOL_ID_MAX`. Each set bit is installed with `ZrCore_ObjectPrototype_AddProtocol`, so module-loaded prototypes answer the same runtime protocol query as the metadata mask supplied by the compiler or artifact.

The UTF-8 encoder rejects U+D800 through U+DFFF and all values greater than U+10FFFF before calling utf8proc. Rejection leaves `outLength` unchanged. Valid scalar values are encoded by utf8proc and report the number of payload bytes without the optional terminating NUL.

## Design and Rationale

The object pointer itself is the identity input because the hash is attached to that object and used as its key hash. Hashing the address of the local pointer parameter instead aliases unrelated objects created through the same constructor call path.

The maximum protocol ID is the enum's maintained range boundary. Referring to `ZR_PROTOCOL_ID_MAX` keeps reconstruction aligned with protocol IDs added after array-like behavior without hard-coding a mid-range endpoint.

utf8proc's encoder accepts integer code points outside Unicode scalar-value space. The core API validates its own documented input contract instead of publishing byte sequences that later fail strict UTF-8 validation.

## Control Flow

Module loading reads each serialized `SZrCompiledPrototypeInfo`, creates or reuses its runtime prototype, then applies the record's protocol mask after setting up the prototype. UTF-8 validation occurs before the library encoder, and output length is assigned only after a positive encoded length is returned.

## Edge Cases and Constraints

Surrogate endpoints U+D800 and U+DFFF are invalid even though they fit in the BMP. U+10FFFF is valid and encodes to four bytes; U+110000 is invalid. `buffer` and `outLength` must both be non-null. The caller must still provide space for `ZR_STRING_UTF8_SIZE` bytes, as required by the API contract.

Protocol ID zero is the no-protocol sentinel and is not replayed. Bits above the declared `ZR_PROTOCOL_ID_MAX` are outside the runtime enum contract.

## Test Coverage

`tests/core/test_identity_protocol_unicode.c` checks the raw-object address hash, creates a minimal module metadata record with a high protocol bit and confirms the loaded prototype answers the runtime query, and checks surrogate/out-of-range rejection plus U+10FFFF acceptance. The test target must link the core library and include the core public headers; see the registration/link requirement supplied with this change.

## Plan Sources

This document records the three defects and regression scope from the 2026-10-12 audit request. The serialized prototype record layout and protocol enum are defined by the related core headers.

## Open Issues or Follow-up

The repository test target still needs registration in `tests/CMakeLists.txt` by the coordinating agent. WSL validation was unavailable in this environment; native target results are owned by the coordinating agent.
