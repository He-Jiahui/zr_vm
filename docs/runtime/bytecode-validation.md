---
related_code:
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io_source_free.c
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/include/zr_vm_core/io.h
plan_sources:
  - user: 2026-10-12 Audit and repair malformed .zro loader handling
tests:
  - tests/core/test_io_validation.c
doc_type: module-detail
---

# Binary source validation

The parser writer and core reader share the `.zro` header defined in
`zr_vm_common/zr_io_conf.h`. The four-byte signature, three version fields,
packed major/minor format value, platform width declarations, byte order, and
module count must be checked before module records are consumed. The encoding
still stores native-width values, so the reader accepts files only when the
declared integer, `TZrSize`, and instruction widths and byte order match the
running build. This is a host-layout format, not a portable interchange
encoding.

`ZrCore_Io_Read` reports short reads through both its return value and the
sticky `hasReadError` flag. It clears the unread part of the requested output,
including when a previous read already failed. Scalar readers initialize
their local values and string decoding stops before allocation when the length
prefix failed. A string length whose terminator addition would overflow is
rejected, as are failed string allocations and truncated string payloads.

`ZrCore_Io_ReadSourceNew` rejects bad signatures, inconsistent version format
values, incompatible host layout, unsupported version patches, oversized
serialized lengths/counts, overflowing top-level and instruction arrays, and
failed allocations. It caps serialized `TZrSize` values and generic parameter
counts at one million and aggregate native allocations per source at 512 MiB.
There is no explicit recursion-depth limit yet. New allocations are zeroed so
cleanup can safely walk partially decoded trees; truncated reads and invalid
value tags set a sticky error and unwind through
`ZrCore_Io_ReadSourceFree`. Strings and runtime values embedded in the graph
remain GC-owned, as documented by the source-free API.

These checks protect framing, allocation arithmetic/budgets, and partial
cleanup. They do not establish that a well-framed hostile bytecode tree is safe
to execute. The loader currently does not cross-check reference indexes against
module declarations, local instruction spans and stack slots against their
function, closure indexes against their capture source, exception target
offsets and catch slices against instruction/catch tables, callable child
indexes against child functions, or metadata token/string/signature offsets
against their owning tables and heaps. Raw instruction words and their opcode
operands also are not verified here. Treat those as unresolved validation work
before executing untrusted `.zro` files; successful loading alone is not an
execution-safety guarantee.

The regression test writes and reloads a genuine `.zro` with the parser writer,
then checks signature and packed-version mismatches, impossible top-level and
nested import counts, an overflowing module-name length, and truncation at
representative header, count, and payload boundaries.
