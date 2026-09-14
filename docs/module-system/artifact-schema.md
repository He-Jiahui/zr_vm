# ExecIR artifact schema and relocation boundary

The versioned ExecIR artifact codec is a disk contract, not a serialized
runtime object. artifact_exec_ir.h/.c encodes every field in fixed-width
little-endian form and represents references as tokens, indices, offsets, or
hashes. It never writes process pointers.

An artifact begins with a bounded header and a section directory. Each section
has a kind, flags, element count/width, offset, and byte length. The reader
checks the magic and schema version, directory bounds, maximum section/byte
limits, duplicate kinds, alignment-independent element sizes, and pairwise
non-overlap before exposing a view. ExecIR and ExecBC hashes are checked before
consumers inspect rows. A failed read leaves no partially callable module.

Relocations are validated only after the complete immutable view is accepted.
The resolver receives a token, target kind, expected content hash, and
expected contract hash and returns a process-local index. Resolution uses a
temporary result array and publishes it only when every row succeeds, so a
stale or tampered target cannot leave a half-relocated graph.

ExecBC consumers apply an additional fixed-width verifier (execbc_verify.c).
It checks section shape, row width, opcode/flag policy, state-map and binding
requirements, and the section hash. Unknown or invalid rows are rejected with
an offset/index diagnostic; no operand word is interpreted as a host address.

The schema is intentionally additive to the existing ZR artifact formats.
Unsupported versions return an explicit recompile-required status. CMake's
ssa_schema_relocation test covers round-trip encoding, relocation success and
failure, truncation/overlap rejection, hash mismatch, and ExecBC opcode
validation.
