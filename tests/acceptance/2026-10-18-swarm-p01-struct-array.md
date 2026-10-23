# Swarm Arena P01: struct array writeback and nested construction

Scope: compiler and VM fixes on `perf/interpreter-20261017`. The demo worktree
is unchanged. No opcode, bytecode version, or public ABI change is required.

## Reproduction

The baseline at `3fe24909` reproduces both failures with copies of the original
`p01_struct_array` probe under this worktree's ignored `build/p01-before/`:

| Variant | Interpreter | Fresh `.zro`, source hidden |
| --- | --- | --- |
| control | Pass | Pass |
| main | `GET_MEMBER` after explicit element writeback | Same failure |
| nested | `KNOWN_VM_CALL` in `points.add(init Vec2(...))` | Same failure |

The original main probe subsequently prints
`PROBE_OK P01 writeThrough=true iterations=10000` in both modes. The nested and
control variants also pass. The runner checks `executed_via=binary` after hiding
the copied source directory and requiring a newly compiled entry `.zro`.

## Causes and changes

* `SET_BY_INDEX` read the ordinary value mirror of an inline struct source.
  That mirror can be null while the complete value lives in frame bytes.
  The store now materializes those bytes, retains the value through an
  exception-safe temporary GC root, and calls the index contract without
  claiming that the materialized C local is a stack operand.
* Assignment prefixes did not retain struct elements as parents requiring
  writeback. They now infer the indexed prefix type, retain its evaluated key,
  and write modified value parents back from the innermost field to the array.
  The frame finalizer also preserves indexed-result inline type hints; the VM's
  `GET_BY_INDEX` path already supports copying into inline destinations.
* Explicit struct constructors use a callable immediately before their
  receiver. Lowering directly into an enclosing call's argument slot overwrote
  the outer receiver or an earlier argument. Nested expressions now construct
  in a fresh callable/receiver window and copy the result to their destination.
  General expression construction also avoids recycled slots carrying earlier
  inline layout hints. Local initializers use their already reserved constructor
  window, preserving construction into their final semantic destination place.

## Element semantics

`var copy = points[i]` reads an independent value. Mutating `copy` does not
change the element. `points[i] = copy` copies the complete modified struct back;
later changes to `copy` remain independent.

`points[i].x = v` and `points[i].x += v` write through. The index expression is
evaluated once. Nested struct field assignments propagate their updates back
to the containing element. The language reference documents these semantics in
`docs/wiki/02-language/expression-construction-reference.md`, section 7.

## Regression coverage

Six CTests cover three source fixtures in interpreter and binary modes:

* `struct_array_writeback_{interp,binary}`: read isolation, whole-value store
  isolation, two elements, repeated writeback, and a dynamically built string
  field surviving the same storage boundary.
* `struct_array_field_write_{interp,binary}`: direct and compound field writes,
  independent local copies, one index evaluation, and nested field writeback.
* `struct_array_nested_init_{interp,binary}`: constructors inside `add`, indexed
  assignment, multiple function arguments with surrounding scalar arguments,
  and two constructor arguments that are themselves struct constructions.

The binary tests require an entry artifact, hide the source directory, and
check the execution-mode marker, so interpreter fallback cannot satisfy them.

## Validation

All three existing trees were rebuilt. The Release core suite passes **431/431**
CTests (63.12 s), and its performance-report gate passes **1/1** for
`c,zr_interp,zr_binary` (193.21 s). The full ordinary ASan/UBSan core suite passes
**431/431** CTests (605.72 s), with no sanitizer findings. The forced-full-GC tree
passes the prior round's **94** selected CTests plus the six new regressions:
**100/100** (13.86 s).

Additional forced-GC direct runs pass **70/70** GC, **16/16** value-type runtime,
**40/40** inline-copy layout, **4/4** tail-reuse, and **6/6** native/AOT value
checks. The original three demo probe variants pass in both modes in all three
trees (**18/18** checks), including the 10,000-iteration main probe. The six new
CTests also pass separately in ordinary ASan/UBSan and forced-GC builds.

The unregistered `zr_vm_struct_value_init_test` has one existing failure in
`test_vm_executes_struct_init_against_inline_field_alias_place`. Both retained
pre-task `build/perf-release` and `build/gcstress` binaries reproduce that same
failure (14 passed, 1 failed). Final Release, ASan, and forced-GC runs retain the
same result; the local destination-place assertion passes. This separate
field-alias-construction issue is outside the P01 change.

The sanitizer gates use `ASAN_OPTIONS=detect_leaks=0` and
`UBSAN_OPTIONS=halt_on_error=1`, matching the previous perf rounds. The forced-GC
tree additionally has `-DZR_DEBUG_GARBAGE_COLLECT_MEM_TEST` in its C flags.
These results do not claim a complete forced-GC language-suite run or a new
performance comparison.

From the worktree root:

```bash
export PATH="$PWD/build/local-tools/usr/bin:$PWD/build/tools/bin:$PATH"
export LD_LIBRARY_PATH="$PWD/build/local-tools/usr/lib/x86_64-linux-gnu"
cmake --build build/r2-merged-release -j4
cmake --build build/r2-asan -j4
cmake --build build/r2-merged-gcstress -j4
ZR_VM_TEST_TIER=core ctest --test-dir build/r2-merged-release -j6 \
  -E '^performance_report$' --output-on-failure
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ZR_VM_TEST_TIER=core \
  ctest --test-dir build/r2-asan -j6 -E '^performance_report$' --output-on-failure
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ZR_VM_TEST_TIER=core \
  ctest --test-dir build/r2-merged-gcstress -j4 \
  --tests-from-file "$PWD/build/p01-gcstress-tests.txt" --no-tests=error \
  --output-on-failure
ZR_VM_TEST_TIER=core ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary \
  ctest --test-dir build/r2-merged-release -R '^performance_report$' \
  --output-on-failure
```

The ignored evidence is under `build/`: `p01-before*/`,
`p01-demo-{r2-merged-release,r2-asan,r2-merged-gcstress}/results.json`,
`p01-release-ctest-verified.log`, `p01-asan-ctest-verified.log`,
`p01-gcstress-ctest-final.log`, `p01-gcstress-tests.txt`,
`p01-release-performance.log`, and `p01-unit-results-verified.json`.
The 100-test selection contains every test name from
`r2-m3-complete-gcstress-ctest.log` plus `struct_array_*_{interp,binary}`.
