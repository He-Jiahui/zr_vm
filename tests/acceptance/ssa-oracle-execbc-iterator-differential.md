# SSA 01.05: pointer-free iterator projection differential

## Scope

The ExecBC test runner accepts explicit `FZrExecBcIterator` callbacks for
`ITER_INIT`, `ITER_MOVE_NEXT`, and `ITER_CURRENT`. The callback receives stable
instruction and operand records, returns a pointer-free value, and chooses
normal or exceptional control flow. It does not supply a production iterator
object or executable exception ABI. AOTIR remains non-runnable.

## Tests and Evidence

`test_oracle_execbc_iterator_differential` builds three independent functions
with tagged managed-heap and native/FFI memory tokens, ordered normal and
exception successors, and handler-local `EXCEPTION_PAYLOAD`. Each function
passes full ExecIR verification before projection. Independent Oracle and
ExecBC providers agree on the return block/value, instruction count, callback
counts, and ITERATOR event identity/operand snapshot on both edges. The
exceptional edge does not publish the normal result. Missing and rejected
providers, and an undefined normal result, report `UNSUPPORTED`,
`ORACLE_ITERATOR_ERROR`, and `INVALID_VALUE` respectively with the iterator
instruction/source while retaining the last published ExecBC result.

On MSVC 19.44 Debug, the initial test build failed because ExecBC had no
iterator input member. After adding the provider and execution support, the
test fixture first failed full verification due to missing iterator memory
tokens; adding those tokens gave a passing `ssa_oracle_projections` CTest
(1/1). The final MSVC static parser library and four targeted executables
built successfully; `^ssa_(builder_iterator_invokes|source_straight_line_cfg|
oracle_projections|oracle_parallel_edges)$` passed 4/4. The existing
unsupported-opcode test now expects runnable ExecBC iterator records, while
AOTIR records remain non-runnable. The ExecBC interpreter stays in one source
file at roughly 1000 lines because this addition shares the existing INVOKE
handler; the next distinct provider family should extract provider dispatch
into its own implementation unit. GCC/Clang and sanitizer results are not
claimed here: the 35-second WSL/GCC attempt stopped during mounted-directory
`build.ninja` regeneration, before any changed source compiled. Neither
source-to-production ExecBC nor AOT execution parity is claimed.
