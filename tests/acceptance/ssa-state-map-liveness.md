# CFG liveness for logical state maps

## Scope and design evidence

2026-09-24, plan 01.04 task 2: replace the producer's global linear use scan
with backward CFG dataflow. Results kill earlier liveness, operands generate
liveness, successor sets are merged to a fixed point, block phi inputs belong
to individual predecessor edges, and deopt values are checkpoint uses.

Repository reference evidence:

- `lua/rust/compiler/rustc_mir_dataflow/src/impls/liveness.rs`: backward gen/kill,
  empty initial sets and edge-specific call-result definitions.
- `lua/mono/mono/mini/liveness.c`: successor live-in unions and gen/kill
  propagation until predecessor work stabilizes.
- `lua/jdk/src/hotspot/share/compiler/methodLiveness.cpp`: CFG liveness and
  normal/exceptional path contributions.
- `lua/rust/tests/ui/mir-dataflow/liveness-ptr.rs` and
  `lua/rust/tests/ui/loops/loop-proper-liveness.rs`: liveness is not a replacement
  for alias or definite-initialization analysis; loop control flow matters.

ExecIR already provides verified CFG adjacency, ordinary result/operand ranges,
block phi pools and deopt records. No new public IR form or language syntax is
needed. The private `exec_ir_state_map_liveness` module computes the live sets
without modifying the function. Builder publication and ownership policy remain
separate. Bitsets require `ceil(valueCount / 8)` bytes per instruction phase;
block inputs and one scratch row are temporary. All products are checked before
allocation. No performance improvement is claimed.

## Baseline and test inventory

Before implementation, the first eight tests reported **8 tests, 7 failures**.
The empty-function case passed. The old implementation missed loop roots and
loop-carried borrows, rejected a borrow used only in a sibling branch, retained
three roots where a phi edge needed one, missed a dominating definition stored
after its use, kept expired deopt values, and failed the reverse-layout case.

`tests/parser/test_ssa_state_map_liveness.c` now covers:

- Loop reference retention before/after a GC boundary.
- A loop-carried borrowed value rejecting suspension with function, block,
  instruction and source diagnostics; failure retains the published map.
- A sibling-branch borrowed value not crossing the suspending branch.
- Two phi incoming edges retaining their own value and the destination retaining
  only the phi result.
- A dominating definition whose stored instruction ID follows the use.
- Separate normal and exception successor roots after an invoke.
- Deopt liveness before and at its checkpoint, ending at the next checkpoint.
- A 64-block reverse-layout chain, value ID 65, and repeated analysis.
- An empty function producing an empty map.

Root assertions also materialize the checkpoint through the core consumer and
compare its root IDs. These are IR/consumer integration tests, not source-language
execution or physical GC/frame restoration tests.

## Tooling evidence

GCC 11.4.0 and Clang 14.0.0, CMake 3.22.1; run from the WSL repository root
for each of `build/ssa-gcc-debug` and `build/ssa-clang-debug`:

```bash
cmake --build <build-dir> --target zr_vm_ssa_state_map_liveness_test zr_vm_ssa_state_maps_test zr_vm_ssa_deopt_validation_test -j 4
ctest --test-dir <build-dir> -R '^ssa_(state_map_liveness|state_maps|deopt_validation)$' --output-on-failure --no-tests=error
```

Both rebuilt successfully and passed **3/3 suites**. The liveness executable
contains **9 passing cases**, plus 35 existing state-map and 7 deopt cases.

MSVC 19.44.35228.0 used the same targets and CTest expression in
`build/ssa-msvc-debug`, adding `--config Debug` to the build and `-C Debug` to
CTest after importing `Import-VsDevCmdEnvironment.ps1`. **3/3 suites passed**.
The new test helper's unreachable-return warning was removed; existing `/W3`
to `/W4` overrides and unrelated CMake numeric-loop path-length warnings remain.

GCC ASan/UBSan, including leak detection and immediate failure on findings:

```bash
cmake --build build/ssa-gcc-asan-phase80 --target zr_vm_ssa_state_map_liveness_test zr_vm_ssa_state_maps_test zr_vm_ssa_deopt_validation_test -j 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/ssa-gcc-asan-phase80 -R '^ssa_(state_map_liveness|state_maps|deopt_validation)$' --output-on-failure --no-tests=error
```

**3/3 suites passed**, without sanitizer findings.

The new invoke fixture initially failed with `EFFECT_TOKEN` at source 101.
GDB 12.1 at `UnityFail` confirmed both tokens were zero. The fixture was fixed
to carry the required `1 -> 2` effect transition; no verifier rule was relaxed.
The reusable diagnostic script is `tests/parser/gdb_ssa_state_map_liveness.gdb`:

```bash
gdb -q -batch -x tests/parser/gdb_ssa_state_map_liveness.gdb --args build/ssa-gcc-debug/bin/zr_vm_ssa_state_map_liveness_test
```

## Acceptance decision

Accepted for CFG live-value generation and logical consumer integration. The
linear use scan and numeric definition filter were removed, with no alternate
legacy liveness path. Full 01.04 completion still requires CFG ownership
states, scalarized aggregates and inline frames, and executable prepare/commit
frame recovery. Deopt value dominance and allocator fault injection into the
new analysis allocations are not covered by this stage.
