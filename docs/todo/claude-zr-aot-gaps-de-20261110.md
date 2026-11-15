# zr_vm AOT gaps D then E: root cause and Codex-ready plan (Claude, read-only dig)

> **Historical diagnosis and plan — not implementation acceptance.**
> This report describes the original `8fbbf4bb` / `4c5eebcbb` snapshot. It was
> imported on 2026-11-16 after comparison with committed main `df5e6dcc`.
> `SETTLED`, `MUST`, old starting-tip requirements and agent instructions below
> belong to the historical report; they are not current execution instructions.
> Do not reset current work to that old tip or infer that its proposed fixes passed.

## Import-time status review (2026-11-16)

- The D validator and E function-table lookup sites still appeared to need the
  reported investigation at `df5e6dcc`, by source inspection. This import did not
  rerun the original AOT crash/link fixtures or establish present-day acceptance.
- Before implementation, reproduce each issue against the then-current committed
  tree and check its actual ABI, metadata and backend contracts. Preserve newer
  work instead of applying the old one-file plan mechanically.
- Machine paths and external evidence references below describe the original
  environment. They are not portable paths or evidence artifacts supplied by this
  import. Original line numbers and remote-main statements are historical.

---

## RESULT_CARD

- **STATUS:** SETTLED
- **TARGET:** zr_vm AOT gaps D then E
- **TIP:** `8fbbf4bb99a7c12d20faf6a88173a7b09914fa49` (detached in `/workspace/zr_vm-pr7-review`, clean at start and end; same tree as `4c5eebcbb`)
- **MODEL:** Fable 5.2 (`claude-fable-5-2`; the gateway accepted it, no model switch)
- **A–F:**
  - **A.** Both gaps are present at this tip, confirmed from code plus the 11-05 evidence. No build was run.
    - D: `status=target-kind-mismatch token=0x03000003 … expected=0x0 actual=0x2`, raised at `zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_method_binding.c:336-344`. It hits 12 fixtures in both backends (24 runs), not 14.
    - E: LLVM exit 139 on `s8`, `s9`, `s11` and fixture `r5b_03`. The fault path is `ZrLibrary_AotRuntime_ToInt` → `aot_runtime_invoke_unary_meta` → `aot_runtime_find_function_index_in_record` → `ZrCore_String_Equal`.
  - **B.** D is not the `new Array<T>()` constructor. It is the typed native member call `actors.add(…)`: the core linker resolves it to a native target, and the AOT row validator rejects any `MODULE` row that already has a target. E is a type confusion: a builtin `TO_INT` meta is a native closure stored in an `SZrFunction *` field, and the AOT record lookup reads `functionName` from it. Fix shapes are one branch each, in different libraries.
  - **C.** D changes one core file; E changes one library file. No ABI, opcode, `.zro`, interpreter or emitter change. D cannot alter any module that loads today. E replaces an invalid read with the fall-through path that already exists.
  - **D.** Two separate LOCAL_ONLY Codex units, D first, then E. Files, red-first tests, scoped gates and hard stops are in §D.
  - **E.** Out of scope: interpreter fused-add loop, class A / p1a / PR #8 / PR #9, ZE, StormBound, classes B/C/F.
  - **F.** D: high that the link failure disappears on all 24 runs; medium-high that all 12 fixtures then pass end to end. E: high on the mechanism and on removing this crash; medium-high that `r5b_03` LLVM then passes fully.
- **KEY FINDINGS:**
  - **D is mislabeled in the 11-05 brief.** Instruction 26 is `KNOWN_NATIVE_MEMBER_CALL` for `actors.add(new Actor())`. The constructor at instruction 10 has no binding row. `m10` and `r5b_03` both construct `Array<int>` and pass in AOT C.
  - **D's cause is two halves of one commit disagreeing.** `call_binding_link.c:158-167` hands `DIRECT` + `MODULE` sites to the native registry, which installs `TARGET_NATIVE`. The AOT validator accepts a `MODULE` row only when it is `VIRTUAL`/`INTERFACE` with `TARGET_NONE`. `git blame` puts both in `b54a6f27e` (2026-09-09).
  - **The runtime already supports the rejected shape.** `GetMemberSlot` calls `ZrCore_CallBinding_PrepareMember`, the same function the interpreter uses for these fixtures. An existing test branch (`test_aot_c_metadata_binding_loader.c:429-437`) already expects a native target on a `MODULE` row.
  - **Typed `Array<int>.add(int)` does not hit D.** Quickening rewrites it to `SUPER_ARRAY_ADD_INT` and clears the binding (`compiler_quickening.c:9553-9590`). So that shape is a green control, not a red test.
  - **E is verified against the prebuilt tip binary, not only by reading.** `objdump` shows `functionName` and the native closure's `nativeBindingLookupIndex` both at offset `0xe0`. The latter is `0xffffffffffffffff` for builtin metas, and `ZrCore_String_Equal` then reads address `0xc7`.
  - **E is wider than `String.toArray()`.** The crash needs only an LLVM generic `TO_INT` on a number or bool. In the saved IR, a `ToInt` helper call is present in exactly the runs that crash (3 of 5 split cases, 1 of 27 fixtures).
  - **Remote main has moved.** It is at `7686b01d`, 185 commits past `4c5eebcbb` (checked 2026-11-14, read-only). The D file is byte-identical there and the E sites are unchanged, so both gaps are still open on main by code reading.
- **CODEX FEED (ordered, LOCAL_ONLY, no push/PR, commit only after acceptance):**
  1. **Unit D:** accept `DIRECT` + `MODULE` + `TARGET_NATIVE` rows in `metadata_runtime_validate_call_binding_rows`. One product file. Red-first test on the `r5_01` shape in C and LLVM.
  2. **Unit E:** make the AOT function-table lookup refuse non-VM function objects (`aot_runtime.c`). One product file. Pin the crash first, then red-first tests on `s9` and a dynamic `<int>x`.
  - `tip_at_start` for both: `4c5eebcbb39340be0771a0bc96a89cc5533b1047` (tree == `8fbbf4bb`), fresh worktree. See §G for the main-drift decision.
- **NEXT:** await New Bot / Codex implement. This report starts no ZE or interpreter-loop work.

---

## 0. Method and limits

- **Read-only.** I ran no cmake, ninja, ctest, cargo or rustc, and no compiler at all. Nothing was staged, committed, stashed, fetched or killed. The worktree is clean at `8fbbf4bb`.
- **Sources:** the tip's source; the 11-05 evidence under `evidence/gd-h2-e5-aot-20261105/` (logs, generated C and LLVM IR, `.zro` files); and `objdump` on the prebuilt Release libraries in `evidence/baseline-lsp2-20261105/build/lib` (built from `git archive 8fbbf4bb`).
- **Scratch:** `evidence/aot-gaps-de-20261110/` holds the excerpts cited below.
  - `d-diagnostics-tally.txt`, `d-row-decode.txt`: D diagnostics per log and the decoded binding rows.
  - `e-objdump-layout.txt`, `e-llvm-static-scan.txt`: the struct-offset disassembly and the per-fixture IR scan.
  - `main-7686b01d-*.c`: four files read from remote main through `gh api` for the drift check in §G.
- **Not observed:** neither fix was applied or run. E has no debugger backtrace. Statements about behaviour after a fix are labelled as inference.
- **Related report:** `claude-zr-remaining-fable-20261112.md` §1–§2 reached the same two root causes independently. This report adds the binary check for E, the quickening control for D, the static residual scan, the drift check and the unit plans. §G lists the two points where I differ.

## A. D and E are still present at this tip

### D

**The diagnostic.** All 24 failing logs in `fx-logs/tip-*.log` carry the same message, formatted at `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c:3297-3325`:

```
failed to link AOT call bindings for module 'main': status=target-kind-mismatch
token=0x03000003 instruction=26 candidate=0 expected=0x0000000000000000 actual=0x0000000000000002
```

| Fact | Value |
| --- | --- |
| Fixtures | 12: `r5_01/02/04/05/06/07/08/11`, `q_x2`, `m7`, `w1`, `w2` |
| Runs | 24 (C and LLVM); 22 at instruction 26, 2 at instruction 22 (`r5_04`) |
| `expected` / `actual` | `ZR_CALL_BINDING_TARGET_NONE` (0) / `ZR_CALL_BINDING_TARGET_NATIVE` (2) |
| `candidate` | row index 0 |

**The emitting site.** Only one `TARGET_KIND_MISMATCH` site writes `expected=TARGET_NONE` and the entry's target kind as `actual`: `metadata_runtime_method_binding.c:341-342`, inside `metadata_runtime_validate_call_binding_rows`.

**The rejected row** (decoded from `artifacts-fx/tip/fx_r5_01_index_method_mutate_c/…/main.c:1670-1688`):

| Field | Row 0 | Row 1 |
| --- | --- | --- |
| function / cache / instruction | 0 / 1 / 26 | 0 / 2 / 34 |
| binding kind | `DIRECT` | `DIRECT` |
| target token | `0x03000003` (provider `MEMBER_DEF`) | `0x03000001` |
| owner token | `0x02000001` | `0x02000001` |
| operation | `CALL` | `CALL` |
| location | `MODULE` (2), target index 2 | `CONSTANT` (1), index 3 |
| target function index | `4294967295` (none) | 1 |

**Which call it is.** In the same generated C:

| Instruction | Emission | Source |
| --- | --- | --- |
| 10 | `zr_aot_known_native_member_call`, cache 5 | `new Array<Actor>()`; no row |
| 21 | `zr_aot_direct_static_function_call` | `new Actor()` |
| 26 | `zr_aot_known_native_member_call`, cache 1 | `actors.add(new Actor())`; **row 0** |
| 34 | VM member call, cache 2 | `actors[0].bump()`; row 1 |

**Controls.** 14 fixtures contain `Array<`, but only 12 fail.
- `m10` (`new Array<int>()` plus an untyped `arr.add(5)`) emits zero rows and passes in C and LLVM.
- `r5b_03` (`new container.Array<int>()` plus untyped adds) passes in C.

So the constructor is not the failing site.

### E

**Evidence** (`results-r5b03-split2.txt`, `e-llvm-static-scan.txt`):

| Case | Shape | `ToInt` helper calls in IR | C | LLVM |
| --- | --- | --- | --- | --- |
| `s7` | `toArray()` then `.length` | 0 | PASS | PASS |
| `s8` | untyped `bytesOf(r)`, then `<int>bytes[0]` | 1 | PASS | **139** |
| `s9` | `"A".toArray()`, then `<int>bytes[0]` | 1 | PASS | **139** |
| `s10` | `[65]` literal, then `<int>bytes[0]` | 0 (cast elided) | PASS | PASS |
| `s11` / fixture `r5b_03` | full | 1 | PASS | **139** |

Across the 27 fixtures with saved IR, `r5b_03` is the only one with a conversion-helper call, and the only one that crashes.

**Crash path at this tip** (all in `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c` unless noted):

1. LLVM lowers generic `TO_INT` to the helper (`backend_aot_llvm_lowering_arithmetic.c:735-736`). In the `s9` IR this is instruction 15: `call i1 @ZrLibrary_AotRuntime_ToInt(ptr %state, ptr %frame, i32 3, i32 2)`.
2. `ZrLibrary_AotRuntime_ToInt` (`:9867`) looks up the `TO_INT` meta before it handles any primitive (`:9892-9894`).
3. `aot_runtime_invoke_unary_meta` passes `metaValue->function` to the record lookup (`:6424`).
4. `aot_runtime_find_function_index_in_record` (`:1697-1711`) compares it with every function in every loaded AOT module through `aot_runtime_functions_equivalent` (`:1289-1304`), which reads `functionName`.
5. `ZrCore_String_Equal` (`zr_vm_core/src/zr_vm_core/string/string.c:939-943`) dereferences that name.

**Why C passes.** The C backend inlines the cast (`backend_aot_c_scalar_conversion.c:194`, `zr_aot_scalar_exec_to_i64`) and never calls the helper. A contract test pins that absence (`tests/parser/test_aot_c_shared_library_smoke.c:2486`).

## B. Root cause and smallest safe fix

**D.** The compiler gives a statically typed call to a native class member a provider contract: `DIRECT`, provider `MEMBER_DEF` and `SIGNATURE` tokens, and a `MODULE` relocation (`compile_expression_support.c:226-251`, `compiler_native_call_binding.c:49-64`). `link_function` treats that as a supported shape: it calls the module resolver and continues (`call_binding_link.c:158-167`). The native registry then installs a `TARGET_NATIVE` witness with the closure and owner guard (`native_binding_call_binding.c:486`, `:504-565`). Both AOT backends project every bound cache into a row (`backend_aot_c_call_bindings.c:117,140`), so the row exists. At load, `metadata_runtime_validate_call_binding_rows` then requires every `MODULE` row to be `VIRTUAL` or `INTERFACE` and still have `TARGET_NONE` (`metadata_runtime_method_binding.c:336-344`). A resolved native site satisfies neither, so the whole module fails to load, in both backends. Nothing binds to the wrong target; a correctly linked target is refused. The smallest safe fix is to make that one branch accept a second shape: `DIRECT` with `TARGET_NATIVE`, a non-null native function and callable object, and a matching generation. The install loop already skips `MODULE` rows (`:399-401`), so the native witness stays in place.

**E.** `String.toArray()` returns an array of unsigned ints (`string.c:895-930`), so `<int>bytes[0]` compiles to a generic `TO_INT`. Core registers a builtin `TO_INT` meta on every bool, signed, unsigned and float type (`zr_vm_core/src/zr_vm_core/value/meta.c:1016,1032,1048,1062`). That meta's `function` field holds an `SZrClosureNative *` cast to `SZrFunction *` (`meta.c:995`). The interpreter never reaches the meta for integers (`execution_dispatch.c:6428-6434`), and the C backend inlines the cast. The LLVM helper is meta-first, so it passes the native closure to a lookup that treats it as a VM function. In the tip binary, `SZrFunction.functionName` and `SZrClosureNative.nativeBindingLookupIndex` are both at offset `0xe0`. `ZrCore_ClosureNative_New` sets that index to `ZR_MAX_SIZE` (`closure.c:249`), and builtin metas never rebind it. The AOT module's entry function is named `__entry` by the binary writer (`writer_binary.c:87`), so the name comparison runs and `ZrCore_String_Equal` loads from `0xc8(%rsi)` with `rsi = 0xffffffffffffffff`, which is address `0xc7`. The smallest safe fix is to stop the type confusion at the lookup boundary: `aot_runtime_find_function_index_in_record` returns "not found" for any object that is not a VM function. The invoker then takes its existing fall-through, `aot_runtime_call_temp_base_without_yield`, which calls the native meta generically.

**Separate units.** D and E share no fix: different libraries, different files, different mechanisms. Their fixture sets are also disjoint. The 12 D fixtures contain no conversion-helper call, and `r5b_03` has no native `MODULE` row.

### Supporting detail for D

- **The runtime consumer exists.** `ZrLibrary_AotRuntime_GetMemberSlot` (`aot_runtime.c:7854-7882`) calls `ZrCore_CallBinding_PrepareMember` for a bound cache. The interpreter calls the same function for the same cache (`execution_member_access.c:2465-2473`), and these fixtures pass in interpreter, compile and binary modes.
- **The call emission matches a passing fixture.** Instruction 26 uses `GetMemberSlot` + `PrepareDirectCall` + `CallPreparedOrGenericWithResume`. `m10` uses the same three helpers for its native constructor, in both backends, and passes. The only difference is the bound branch inside `GetMemberSlot`.
- **The design doc never covered this shape.** `docs/parser-and-semantics/call-binding-artifact-and-aot.md` says only that "Interface MODULE rows start with `TARGET_NONE`". The accessor path already has the comment "A native provider has no generated thunk; invoke its callable object directly" (`aot_runtime.c:4570-4571`).
- **Why it stayed hidden.** No registered AOT test has a native `DIRECT` + `MODULE` row. The common typed case, `Array<int>.add(int)`, is quickened to `SUPER_ARRAY_ADD_INT` and its binding is cleared.
- **Breadth (inference).** Any typed native class member call that keeps its provider contract should fail the same way: other element types, other native methods, native static methods and typed native accessors. Only `Array<Actor>.add` is observed.

### Supporting detail for E

- **Binary evidence** (`e-objdump-layout.txt`):
  - `ZrCore_ClosureNative_New` stores the 16 bytes `ff ff ff ff ff ff ff ff 01 00 …` at `0xe0(%rax)`. That is the lookup index followed by `callBindingGeneration`.
  - `aot_runtime_find_function_index_in_record` loads `0xe0(%rbp)` and `0xe0(%r12)`, then calls `ZrCore_String_Equal` when both are non-null and different.
  - `ZrCore_String_Equal` starts with `mov 0xc8(%rsi),%rcx`.
- **Deterministic.** The crash needs one loaded AOT function with a non-null name. Every module loaded from a `.zro` has `__entry`.
- **Same family, unobserved:**
  - `ToBool`, `ToUInt` and `ToFloat` are meta-first in the same way (`:9840`, `:9951`, `:10008`), and LLVM lowers to them (`backend_aot_llvm_lowering_type_conversions.c:62-73`).
  - `aot_runtime_invoke_binary_meta` does the same lookup (`:6499`). Its callers handle same-type primitives first, so only mixed primitive operands reach it.
- **Precedent for the guard.** `aot_runtime.c:9047-9048` already skips the lookup for native metas: "Native meta methods are callable raw objects, not bytecode functions."
- **No coverage.** No LLVM-executed test in the repo contains an `<int>` cast. All such tests are AOT C.

## C. Side effects and risk

| | Unit D | Unit E |
| --- | --- | --- |
| Product file | `zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_method_binding.c` | `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c` |
| Size | about 10 lines, one branch | about 4 lines, one function |
| ABI / row format / `.zro` | none; rows are already emitted byte for byte | none |
| Opcodes / compiler / quickening | none | none |
| Interpreter | none; `LinkCallBindings` returns early without a code registration (`:367`) | none; the file is AOT runtime only |
| C backend | no emitter change; loader behaviour shared with LLVM | no effect on generated C; the lookup is also used by C-run meta paths |
| LLVM backend | no emitter change | no emitter change |

**Can D regress a passing fixture?** Not through the validator. Today a module with any `DIRECT` + `MODULE` row always fails to load, so no module that loads now contains such a row. The risk is in newly reachable code: a D fixture may get past load and fail later for another reason. That would be a new visible failure in a fixture that already fails, not a regression.

**Can E regress a passing fixture?** Only if some caller relies on a native object matching a table entry, which would itself be an invalid read. The function table holds VM functions flattened from the module graph (`:1323-1345`), so a native closure can never be a member. After the guard, native metas take the generic path that non-AOT metas already use.

**Static residual scan of the 12 D fixtures** (saved generated C and IR at the tip):

- No `CallOptionalReceiver` call in their C, so class A cannot appear in them.
- No statically unsupported LLVM opcode, so class C cannot appear in them.
- No conversion-helper call, so E is not hiding behind D.
- `r5_05`, `r5_08` and `w2` contain one or two scalar stack copies in C. That is possible class B exposure, not a prediction.

**Must stay green** (11-05 matrix, `results-fixtures.txt`):

| Backend | Fixtures passing at the tip |
| --- | --- |
| AOT C | `m1`, `m8`, `m10`, `m14`, `r5_03`, `r5_09`, `r5_10`, `r5_12`, `r5_14_typed_callable_controls`, `r5_14_untyped_tail_receiver`, `r5b_01`, `r5b_03`, `r5b_04` |
| AOT LLVM | `m1`, `m8`, `m10`, `m14`, `r5_03`, `r5_09`, `r5_10`, `r5_12`, `r5_13`, `r5_14_untyped_tail_receiver`, `r5b_01`, `r5b_02`, `r5b_04` |

Registered suites that must keep their results:
- `aot_c_metadata_binding_loader` (12 tests), the receiver-guard smoke (12 tests) and `aot_llvm_symbol_stripping` (3 tests);
- `call_binding_aot_projection`, `call_binding_native_registry`, `call_binding_runtime`, `call_binding_module`, `call_binding_artifact`, `typed_call_binding`, `metadata_runtime_method_binding`;
- the gd_h2_e5 + inline group (`tests/cmake/inline-struct-regressions.cmake`), 336/336 in three modes.

Known reds at the tip that these units must neither fix nor widen: C `r5_13` (F/B), C `r5b_02` (B), LLVM `r5_14_typed_callable_controls` (C), and `m11`/`m13` (the evidence harness did not register their modules).

## D. Codex-ready plan

Common rules for both units:

- **Base:** `tip_at_start` MUST be `4c5eebcbb39340be0771a0bc96a89cc5533b1047` (tree == `8fbbf4bb99a7c12d20faf6a88173a7b09914fa49`).
- **Worktree:** fresh and LOCAL_ONLY, one per unit. Not `/workspace/zr_vm-interp-fused-add-loop-20261110`, not the dirty `/workspace/zr_vm-gd-h2-e5`, not `/workspace/zr_vm-pr7-review`.
- **No push, PR or merge.** One focused commit per unit, only after acceptance. Report HARD_STOP with evidence otherwise.
- **Builds:** isolated build dir, `-j1`, `TMPDIR` under `/workspace`, never `/tmp`. Do not kill other processes.
- **LLVM tests** need `clang` on `PATH`. The box has only `clang-19`, so use a shim directory containing a `clang` symlink.
- **Test placement:** add new test functions and `RUN_TEST`s; existing test bodies stay byte-identical. Avoid `tests/parser/test_aot_receiver_guard_shared_library.c` and `tests/cmake/aot-generated-targets.cmake`: both changed on main after this base.
- **No full-repo Release gate is required.** The scoped gates below cover every consumer of the touched function.

### Unit D — admit native-provider rows in the AOT call-binding validator

**Allowed surface**
- Product: `zr_vm_core/src/zr_vm_core/metadata/metadata_runtime_method_binding.c`, the `MODULE` branch at `:336-345` only.
- Tests: `tests/parser/test_aot_c_metadata_binding_loader.c`. Its target already links `zr_vm_lib_container` (`tests/CMakeLists.txt:233-243`), and its row loop already has the native branch (`:429-437`).
- Docs: one sentence in `docs/parser-and-semantics/call-binding-artifact-and-aot.md` on native `DIRECT` `MODULE` rows.

**Must not touch:** `call_binding_link.c`, the native registry, the AOT backends, the AOT runtime, the compiler or quickening, row or ABI layouts, any fixture.

**Change**

```c
if (view.location.kind == ZR_CALL_BINDING_RELOCATION_MODULE) {
    const SZrCallBindingTarget *linked = &entry->binding.target;
    TZrBool deferred =
            (view.contract.bindingKind == ZR_CALL_BINDING_VIRTUAL ||
             view.contract.bindingKind == ZR_CALL_BINDING_INTERFACE) &&
            linked->targetKind == ZR_CALL_BINDING_TARGET_NONE;
    /* link_function resolves DIRECT + MODULE through the native registry. */
    TZrBool nativeProvider =
            view.contract.bindingKind == ZR_CALL_BINDING_DIRECT &&
            linked->targetKind == ZR_CALL_BINDING_TARGET_NATIVE &&
            linked->native.function != ZR_NULL && linked->callableObject != ZR_NULL;
    if ((!deferred && !nativeProvider) ||
        entry->binding.generation != function->callBindingGeneration) {
        return metadata_runtime_call_binding_fail(/* unchanged arguments */);
    }
    continue;
}
```

Every other shape keeps today's status and `expected`/`actual` values.

**Tests (red first on the base)**

| ID | Source | At base | After |
| --- | --- | --- | --- |
| T-D1 | `r5_01` shape ending in `return 42;`, AOT C | fails with `target-kind-mismatch token=0x03000003` | 42; the `MODULE` row has `TARGET_NATIVE` and `runtimeHitCount > 0` |
| T-D2 | same source, AOT LLVM | same failure | 42 |
| T-D3 (control) | `var a = new Array<int>(); a.add(41); return 42;`, AOT C | expected to pass (quickened, no row) | pass |

- The new cases need `zr.container` registered before compile and before execution. The 11-05 harness called `ZrVmLibIteration_Register` and `ZrVmLibContainer_Register` (`scripts/make_fixture_harness.py`). Add a helper variant; do not edit the shared helper used by existing tests.
- If T-D3 is red at base with the same diagnostic, treat it as a second red-to-green case and record that. It is not a hard stop.

**Scoped gates**
1. T-D1 and T-D2 red at base with the exact diagnostic, green after. Keep both logs.
2. `ctest -R 'call_binding|aot_c_metadata_binding_loader|metadata_runtime_method_binding|aot_llvm_symbol_stripping'` plus the receiver-guard smoke: results identical to base.
3. Fixture matrix for the gd_h2_e5 single-file fixtures in AOT C and LLVM, before and after (the 11-05 harness method is enough):
   - the string `target-kind-mismatch` is gone from all 24 runs;
   - every run in the must-stay-green table still passes;
   - any D fixture that now fails later is recorded with its message and class, and left alone.
4. gd_h2_e5 + inline group 336/336 in three modes.
5. ASan on the new tests and `call_binding_runtime` (the fix admits a GC-tracked native witness into AOT execution).

**HARD_STOP**
- T-D1 is not red at base with `token=0x03000003 … expected=0 actual=2`.
- The fix needs any product file other than the validator.
- After the fix, a D fixture fails with `GET_MEMBER_SLOT: bound target failed`, inside `ZrCore_CallBinding_PrepareMember`, or in `aot_runtime_validate_metadata_bindings`. The row is then admitted but not consumable; report it and do not patch the runtime.
- Any test or fixture that passed at base changes result, or any existing assertion needs a new expectation (for example the deferred-row block in `test_call_binding_aot_projection.c:296-345`).
- A gate can only be met by working on class A, B, C or F.

### Unit E — keep native callables out of the AOT function-table lookup

**Allowed surface**
- Product: `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c` only.
- Tests: new test functions in `tests/parser/test_aot_c_metadata_binding_loader.c`, or one new test file registered like it. Each new source must run in LLVM and C and match the interpreter.

**Must not touch:** `zr_vm_core` (`meta.c`, `value.c`, `string.c`), either AOT emitter, the interpreter, opcodes.

**Step 0 — pin the crash (no product change).** Run the `s9` source under AOT LLVM with ASan or a debugger on the base. Expected:
- fault address `0xc7`;
- frames `ZrCore_String_Equal` ← `aot_runtime_find_function_index_in_record` ← `aot_runtime_invoke_unary_meta` ← `ZrLibrary_AotRuntime_ToInt`.

Save the trace.

**Change (required).** At the top of `aot_runtime_find_function_index_in_record` (`:1697-1711`), after the null checks:

```c
/* Native metas and closures are stored in SZrFunction* fields; they are never
 * members of a generated function table and must not be read as VM functions. */
if (function->super.type != ZR_RAW_OBJECT_TYPE_FUNCTION || function->super.isNative) {
    return UINT32_MAX;
}
```

This covers the unary and binary invokers (`:6424`, `:6499`) and every other caller of the lookup. An extra early-out in the two invokers, in the style of `:9047-9048`, is allowed but not needed.

**Tests (red first on the base)**

| ID | Source | LLVM at base | After |
| --- | --- | --- | --- |
| T-E1 | `var s = "A"; var bytes = s.toArray(); if (<int>bytes[0] != 65) { throw 3; } return 42;` | process exits 139 | 42 |
| T-E2 | `fn f(x) { return <int>x; } return f(40) + f(2.9);` | predicted 139 (unobserved) | 42 |
| T-E3 (optional) | dynamic `<bool>x`, `<uint>x`, `<float>x` on numbers | predicted 139 (unobserved) | interpreter value |

- A crash kills the whole Unity executable, so the red proof at base is the executable's exit status 139. Record it.
- If T-E2 or T-E3 is green at base, keep it as a pin and record that the prediction was wrong. It is not a hard stop.
- The same sources must pass in AOT C at base and after.

**Scoped gates**
1. Step 0 trace matches. T-E1 is red at base and green after.
2. Fixture `r5b_03_native_and_static`: LLVM goes from exit 139 to PASS; C stays PASS.
3. Receiver-guard smoke, `aot_c_metadata_binding_loader`, `aot_llvm_symbol_stripping`, and every registered AOT executable: zero result delta against base. (Reference from 11-05: 73 executables, 541 tests.)
4. ASan run of the new tests and of the receiver-guard smoke: no report.
5. The fixture matrix from unit D's gate 3 is unchanged except `r5b_03` LLVM.

**HARD_STOP**
- The Step 0 trace does not show these frames. The mechanism in this report would then be wrong; stop and send it back.
- T-E1 still crashes or fails after the guard. That is a second defect; report it.
- The fix needs a file outside `aot_runtime.c`.
- Any existing AOT test changes result, or any generated-C or IR text contract needs a new expectation.
- Making `ToInt` primitive-first becomes necessary. That changes the result tag for small integer types and needs John's approval (see §G).

**Order.** D first, then E, as requested. They do not depend on each other and touch different files, so they could also run in parallel in separate worktrees.

## E. Out of scope

- Interpreter fused-add loop (Codex unit INTERP-FUSED-ADD-LOOP and its worktree).
- AOT class A, the p1a/p1 prototypes, PR #8, PR #9, and any merge.
- Classes B, C and F. Neither fix touches their sites. If a D fixture fails later for one of these, the unit records it and stops.
- ZE, StormBound, and any new Cap. This report proposes units and authorizes nothing.
- Emitting fewer binding rows, changing row or ABI layouts, or making `ToInt`, `ToUInt`, `ToFloat` and `ToBool` primitive-first.

## F. Confidence

| Claim | Confidence | Basis |
| --- | --- | --- |
| D: the validator change removes `target-kind-mismatch` on all 24 runs | **High** | One unconditional gate; decoded rows and 24 identical diagnostics |
| D: all 12 fixtures then pass in both backends | **Medium-high** | Same helpers as passing `m10`; `PrepareMember` shared with the passing interpreter path; static scan finds no class A, C or E exposure. Never executed past load in AOT; three fixtures have scalar copies in C |
| D: no new semantic change | **High** | The loader admits what the core linker already resolved; nothing else changes |
| E: the crash mechanism | **High** | Offsets read from the tip binary; `ToInt` presence matches the crash set exactly. No debugger trace (Step 0 closes this) |
| E: the lookup guard removes this crash | **High** | The faulting read sits behind the guarded condition |
| E: `r5b_03` LLVM then passes fully | **Medium-high** | The fall-through generic meta call exists but is unobserved for builtin native metas under LLVM |
| E: no new semantic change | **High** | An invalid read becomes "not found", which is already the result for any function outside the table |

## G. Corrections, drift and notes

**Corrections to the 11-05 brief**

| Brief | This dig |
| --- | --- |
| "`new Array<T>()` constructor binding" | The typed member call `actors.add(…)`. The constructor has no row. |
| "14 of 29 fixtures" | 12 fixtures. 14 contain `Array<`; `m10` and `r5b_03` pass. |
| "separate provider/metadata item" | A one-branch AOT loader validation gap. Provider metadata is correct. |
| E: "`String.toArray()` index + cast" | Any LLVM generic `TO_INT` on a number or bool. `toArray()` is only how the fixture gets an untyped unsigned value. |

**Remote main has moved** (checked 2026-11-14 with `git ls-remote` and `gh api`; nothing was fetched into the box repo).

- `refs/heads/main` is `7686b01d83b7e0b2962728df783321100d3c99e0`, 185 commits ahead of `4c5eebcbb`.
- D on main: `metadata_runtime_method_binding.c` is byte-identical to this tip. No call-binding, native-registry or compiler binding file changed.
- E on main: `ZrLibrary_AotRuntime_ToInt` is still meta-first (now `:9937-9939`), the lookup at `:6424` is unchanged, and generic `TO_INT` still lowers to the helper (`backend_aot_llvm_lowering_arithmetic.c:785-786`). PR #44 added inline lowering for the typed casts `TO_INT_FLOAT` and `TO_INT_UNSIGNED`, which does not cover a dynamically typed source.
- No open PR touches either product file.
- So both gaps are still open on main by code reading; I did not run main.
- The base `4c5eebcbb` is what I was told to require, and the plan keeps it. Both patches should port unchanged: the D hunk is identical, and the E hunk sits above the region main changed.
- **Decision for New Bot / John:** whether Codex should start from current main instead, to avoid integrating a branch that is 185 commits behind.

**Differences from the 11-12 report**

- It dates the resolver branch in `link_function` to `d3e4c1e74` (2026-10-21). `git blame -L 150,172 call_binding_link.c` attributes lines 158-168 to `b54a6f27e` (2026-09-09), the same commit as the validator.
- It proposes a typed `Array<int>` case to test D's breadth. That shape is quickened and its binding cleared, so it should be green at base (T-D3 here).

**Side notes (no action in these units)**

- `aot_runtime_functions_equivalent` matches functions by name, parameter count, instruction count and line range. The code's own TODO at `:1286-1287` notes that two functions could collide.
- Under LLVM a small signed source (for example an `int8`) converts to `int64` through the meta path. The interpreter keeps the source tag (`execution_dispatch.c:6430-6431`). This is an existing divergence, and the reason primitive-first `ToInt` is kept out of unit E.

---

## 中文摘要（给 John / New Bot）

- **状态：SETTLED。** 只读挖掘，没有运行任何构建或编译器；工作树 `8fbbf4bb` 始终干净。
- **D 的真实原因（与 11-05 简报不同）：**
  - 失败的不是 `new Array<T>()` 构造函数，而是有类型的原生成员调用 `actors.add(new Actor())`（指令 26，token `0x03000003`）。
  - 受影响的是 12 个夹具（C 与 LLVM 共 24 次运行），不是 14 个。`m10` 和 `r5b_03` 同样构造了 `Array<int>`，在 AOT C 下通过。
  - core 链接器把 `DIRECT`+`MODULE` 调用点交给原生注册表，得到 `TARGET_NATIVE`（`call_binding_link.c:158-167`）。
  - AOT 加载时的行校验只接受 `VIRTUAL`/`INTERFACE` 且 `TARGET_NONE` 的 `MODULE` 行（`metadata_runtime_method_binding.c:336-344`），于是整个模块加载失败。
  - 运行时本身已支持这种形态：`GetMemberSlot` 调用的 `PrepareMember` 与解释器是同一函数，解释器下这些夹具通过。
  - `Array<int>.add(int)` 会被 quickening 改写成 `SUPER_ARRAY_ADD_INT` 并清掉绑定，所以它不触发 D，只能作为绿色对照。
- **D 的修复：** 只改校验函数的一个分支，额外接受 `DIRECT` + `TARGET_NATIVE`（原生函数与可调用对象非空、代际一致）。不动 ABI、行格式、opcode、编译器和解释器。
- **E 的真实原因：**
  - LLVM 把通用 `TO_INT` 降级为运行时函数 `ZrLibrary_AotRuntime_ToInt`，该函数先查元方法再处理基本类型。
  - 基本类型的内置 `TO_INT` 元方法是原生闭包，却存放在 `SZrFunction *` 字段里（`meta.c:995`）。
  - AOT 函数表查找把它当成 VM 函数读取 `functionName`。在 tip 的二进制里，这个偏移（`0xe0`）正好是原生闭包的 `nativeBindingLookupIndex`，值为全 1。
  - 模块入口函数名为 `__entry`，于是 `ZrCore_String_Equal` 读取地址 `0xc7`，进程段错误。
  - C 后端把转换内联了，从不调用该函数，所以 C 通过。
  - 影响面比 `String.toArray()` 大：LLVM 下任何对数字或布尔值的动态 `<int>x` 都会走到这里（预测，尚未实测）。
- **E 的修复：** 在 `aot_runtime_find_function_index_in_record` 开头拒绝非 VM 函数对象，只改 `aot_runtime.c` 几行；之后走现有的通用元方法调用路径。
- **两个独立的 Codex 单元（均 LOCAL_ONLY，不 push、不开 PR，验收后才提交）：**
  1. 单元 D：改校验分支；用 `r5_01` 形态在 C 与 LLVM 下做先红后绿测试。
  2. 单元 E：先用 ASan 或调试器钉住崩溃栈，再加守卫；用 `s9` 和动态 `<int>x` 做先红后绿测试。
  - 两者的起点都必须是 `4c5eebcbb`（树等同 `8fbbf4bb`），使用全新工作树。
  - 范围门禁和 HARD_STOP 条件见 §D，不需要全仓 Release。
- **把握程度：**
  - D：链接失败消失为高；12 个夹具随后全部通过为中高（它们在 AOT 下从未执行到加载之后）。
  - E：机制与消除崩溃为高（已对二进制核实，但没有调试器栈）；`r5b_03` LLVM 完全通过为中高。
- **需要 New Bot / John 决定：** 远端 main 已到 `7686b01d`，比 `4c5eebcbb` 多 185 个提交。
  - D 的文件在 main 上逐字节相同，E 的相关代码也未变，所以两个缺口在 main 上仍然存在（按代码阅读，未运行）。
  - 请决定 Codex 是否改从当前 main 起步。本报告按指示仍以 `4c5eebcbb` 为起点。
- **不在范围内：** 解释器 fused-add 循环、A 类 / p1a / PR #8 / PR #9、B/C/F 类、ZE、StormBound、新 Cap。
