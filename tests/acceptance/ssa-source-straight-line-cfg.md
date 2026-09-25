# Source straight-line CFG finalization

## Scope

This stage advances 01.02 source-owned CFG production. A supported entry body
with no explicit control flow must have real SemanticIR terminators and lower
through the ordinary ExecIR builder without inserting a dummy source branch.
It does not complete production effects, automatic cleanup or the entire SSA
plan. Source capabilities that lack canonical producers must remain rejected
as executable SSA, even when the legacy compiler can compile them.

## Baseline

Base commit: `3b97c6e3`. The inactive graph built by
`ZrParser_Compiler_ValidatePreSemanticIr` has an entry-to-exit RETURN edge but
no corresponding return instruction. Removing the three dummy `if (true) {}`
continuations in `test_ssa_source_value_facts.c` yielded three failures out of
four cases: builder diagnostic 28, block 1, expected edge 4, actual edge 7.
The plain-line facts-only case still passed. The defect is CFG production,
not the new canonical ownership facts and not an overly strict verifier.

Code inspection also found that binary-expression compilation emitted ExecBC
without a canonical binary-expression producer. Activating every inactive
body without checking completeness could silently omit an expression. The
regression scope therefore includes unsupported source forms as well as
successful fallthrough. This stage adds a deliberately narrow producer subset
for typed signed/unsigned/float `+`, `-`, and `*`; unsupported forms continue
to exercise the analysis-only boundary.

The initial focused suite reproduced four missing-activation failures out of
14 cases: empty source, scalar prefix, repeated validation, and isolated child
declaration. Existing explicit return/throw and unsupported-form negatives
passed before the production fix. Full-source compilation was also included.
Subsequent independent review showed the initial child-declaration expectation
was unsafe: the child body is isolated but CREATE_CLOSURE/SET_STACK in its parent
are still legacy-only. The final regression expects analysis-only rejection.

While adding execution coverage, inspection found a second directly coupled
defect: the builder did not project `constantPoolIndex` into the ExecIR
CONSTANT operand field (`layoutId`). The oracle consumes that field as the
constant-table index. This stage therefore also requires actual multi-constant
source execution to preserve assignment values, not merely a successful build.
The added source oracle test observed the expected behavioral RED: expected 9,
actual 7. Projecting the canonical pool index at the common builder boundary
then produced 9. No source instruction or value was manually patched.

The existing `pre_semantic_ir` golden sequence remains 24 instructions:
its mixed-type initializer does not satisfy the later cross-TypeId completeness
gate, so its CFG stays analysis-only. The golden now asserts that boundary;
the new source suite checks the BRANCH and RETURN on supported source instead.
A candidate nested-brace fixture was invalid at the top-level parser boundary:
top-level braces are parsed as expressions rather than statement blocks. It
was replaced with a valid block-expression rejection case; no parser grammar
change is part of this stage. Initial oracle-provider setup issues and a run
attempted while its executable was still linking are not behavioral evidence;
only completed rebuilt executable runs are counted.

Specification review found a second overpromotion: `zr;` read used legacy
GET_GLOBAL without a SemIR producer. A new 22-case run failed exactly that
negative test while a shadowing local `zr` still passed. Quality review then
found global `missing = 7;` silently omitted its legacy SET_MEMBER effect,
named child callable declarations omitted parent CREATE_CLOSURE, and
`own Value()` could omit instance creation/close/constructor effects. The
25-case expanded run failed both global assignment and child read negatives;
the finalizer now requires source-local LOAD plus prior INITIALIZE/STORE and
source-local STORE for assignment, and rejects child declarations and new
instance construction. Resource-construction facts still validate as analysis
facts but cannot be advertised as executable ExecIR. Empty classes without
runtime-bearing members remain metadata-only.

The review follow-up found another value-completeness case. Numeric
int-to-float initialization emits a generic SemanticIR CONVERT but its current
oracle execution copies the input; numeric int-to-float assignment emits a
legacy TO_FLOAT and a SemanticIR STORE of the original RHS. Both valid source
regressions failed against the earlier preflight (29 cases / 2 failures).
Cross-type CONVERT/STORE now retains the analysis-only graph. Typed arithmetic
producer tests cover a direct sum, nested subtraction/multiplication, strict
builder lowering, and oracle execution; mixed-type arithmetic remains
analysis-only. The local read
preflight also uses a checked scratch bitmap indexed by PlaceId and a single
instruction cursor, rather than rescanning all preceding instructions for
each of 256 repeated reads. Its allocation participates in fault injection.

## Reference evidence

- Lua `lua/src/lparser.c:close_func` emits `luaK_ret` at function completion.
  `lua/testes/calls.lua` covers null-like and missing return values.
- QuickJS `lua/QuickJS-master/quickjs.c:emit_class_init_end` emits an explicit
  `OP_return_undef` at the end of an initializer.
- Rust `lua/rust/compiler/rustc_mir_build/src/builder/mod.rs` terminates the
  final body block with MIR `Return`; its
  `tests/mir-opt/building/custom/terminators.rs` covers call continuations,
  return and drop terminators.
- CPython `lua/cpython/Lib/test/test_compile.py` includes
  `test_lineno_after_implicit_return`, reinforcing that an implicit terminal
  operation is distinct from a missing operation and has source-position
  consequences.

These references inform explicit finalization, not a change to Zr return
value semantics. No bytecode-to-SemIR reconstruction is permitted.

## Validation status

The supported source suite passed 29/29 cases on GCC, Clang, MSVC static and
GCC ASan/UBSan. Its companion fault suite passed 3/3 on those configurations
and on the MSVC shared-DLL build. The selected 17 SSA CTests passed 17/17
on GCC, Clang, MSVC static and GCC ASan/UBSan after the conversion gate.
The pre-semantic-IR golden was subsequently corrected to retain its
analysis-only shape; its executable passed 101/101 on GCC, Clang, MSVC static
and GCC ASan/UBSan with leak detection. The extra canonical-type-graph and
reference-loan/NLL executables passed 19/19 and 15/15 respectively on all four
configurations after the final production changes. GDB's
`gdb_ssa_source_straight_line_cfg.gdb` hit the actual CFG finish producer and
its inferior passed 29/29; it was rerun after the golden correction.

Builds used the existing `build/ssa-gcc-debug`, `build/ssa-clang-debug`,
`build/ssa-msvc-debug`, `build/ssa-gcc-asan-phase80` and
`build/ssa-msvc-shared-debug` configurations. Selected CTests were invoked via
`ctest --test-dir build/<configuration> --output-on-failure -R <17-case SSA
selection>`. The sanitizer run set `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`
and `UBSAN_OPTIONS=halt_on_error=1`. The Windows DLL fault test passed 3/3.
All these configurations have `BUILD_CLI=OFF`, so CLI integration was not run;
the source compile API and supported oracle execution were tested directly.

The separate, user-modified `test_place_cfg_graph.c` currently fails two
alias-overlap assertions (2/8) on GCC, Clang and MSVC static: lines 335 and
381 report expected 3, actual 1. The sanitizer variant also reports 1792
bytes in 12 allocations after Unity aborts at those failures. That suite and
its changes are outside this stage, were not reverted, and are not counted as
passing acceptance evidence here.

The additional `ssa_source_cfg_faults` suite uses a renamed test copy of the
production finalizer with only libc scratch realloc/free intercepted. It
compiles 256 real read statements and enumerates initial/growth allocation
failures. Each failure must preserve the unpublished or prior analysis graph,
free all scratch memory, and permit a later successful retry. A sealed graph
must avoid the worklist altogether. Null/uninitialized compiler inputs are
also checked. Windows shared builds place the fault copy inside the parser
DLL so private CFG helpers need no new exported production API.

Only this new preflight scratch allocation boundary is fault-injected; this
does not prove transactionality for all pre-existing CFG/semantic allocators.
The graph is freed before `ensure_active`/`finish`; failures there may leave a
partial graph. That rollback boundary remains unfinished, not validated here.

## Environment

WSL x86-64 GCC 11.4.0, Clang 14.0.0 and GDB 12.1 are available. Windows
x64 MSVC 19.44.35228 was confirmed through the Visual Studio environment
wrapper; `cl /Bv` printed its version then exited 1 because no source file was
provided. That version probe is not a build failure or successful test.
The existing sanitizer configuration is `-fsanitize=address,undefined
-fno-omit-frame-pointer`. Unrelated dirty files remain outside this stage.
