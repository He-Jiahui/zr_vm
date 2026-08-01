# 2026-08-01 AOT 07 Value-SemIR Parameter Layout Consumption

## Scope

This A7.2H sub-milestone makes generic/shared inline-struct `CALL_TYPED` selection consume the retained callee ExecIR
parameter sidecar instead of legacy function parameter metadata. It adds no public function, artifact, manifest, or
reachability schema.

The accepted narrow mapping requires exact runtime arity and no receiver role. Direction, default origin, receiver
mapping, return/destination, spill, address-taken ABI, and complete A7.2 remain open. Producer-materialized defaults are
compatible when already included in `argumentCount`.

## RED And Review Evidence

- The first authority test corrupted legacy metadata to I64 while preserving the typed-local projected reference type.
  The unchanged backend reported 8 passes and 1 failure because it did not emit the shared method-slot callsite.
- The initial design proposed `parameterLayoutCount == argumentCount + 1` for receivers. Independent review established
  that `CALL_TYPED.argumentCount` already includes the receiver and that exact count cannot claim to reject every default.
- The implementation was narrowed to exact-count/no-receiver mapping and direct ExecIR flat-index lookup. A negative test
  then cleared the projected reference type while leaving legacy metadata as reference; the shared marker stayed absent.
- Final review found two P3 coverage gaps rather than a production defect: receiver-role rejection and lookup after a
  lower flat index is stripped. A closed generic instance-method case now proves the shared method slot exists while the
  receiver-bearing callsite stays ordinary; a second case removes an unreferenced nested function before the retained
  callee and still resolves the shared callsite. Final generic typed-call coverage is 11/0 on all three compilers, and
  independent re-review of the completed diff returned `No findings.`

## Coverage Inventory

- Resolves the retained callee through `backend_aot_exec_ir_find_function(module, calleeFunctionIndex)`.
- Requires a non-null parameter sidecar whose row count equals both frame parameter count and call argument count.
- Rejects receiver-bearing rows and requires at least one projected OBJECT/ARRAY argument.
- Requires each corresponding caller source to remain a VALUE slot large enough for `SZrTypeValue`.
- Proves typed-local projection authority over conflicting legacy metadata.
- Proves unknown projected TypeRef cannot select the shared specialization even when legacy metadata says reference.
- Proves a receiver-bearing generic owner can publish a shared method slot without selecting the unsupported shared
  inline-struct callsite route.
- Proves a retained callee sidecar is found by sparse flat identity after code stripping removes a lower function index.
- Removes the legacy metadata table and subtraction-offset dependency from value-SemIR typed-call selection.

## Tooling Evidence

Frozen effective source is committed HEAD `1c50bad8b1675e1b4700f985139e942887964022` plus the exact A7.2H eight-file
production/test overlay:

- WSL source: `/home/hejiahui/codex-validation/zr_vm-aot12-20260801-a72d-3d67352`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260801-a72d-3d67352`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228.0 x64 Debug

All eight implementation/test files match across main, WSL, and Windows:

- `ab7d8267cc3d20836071c9c72520c85299dcbf249c68c3f8089a2a6526c48669`
- `b238754417b7c8d3b1a4088f17c252677bc130e37888d68fbf42dcaa9fe63a7b`
- `c3eb67482f3de0a9819f35af24f8e110ead013732d05d0f9563f9dad1ed360d2`
- `13ac3f0034ec52f8ba546d7171e32595ba55289f77eced3b4fa7baf78f77ba36`
- `7d14a090b210b9d4452a49d84731de89602639ca4ec04808db6236c08d0e9207`
- `e8fd65e4c750a14866659b5e6cf7328a7c8f78254c4931113de00a05e15e8ef5`
- `129e9f14c2f17b49811d60b9c4daf5356e881bfa174acfe630a42f4de14c97ad`
- `84c6f841859398ab6af4d2913ff2f83db892ce1baefe869accd85042662f5d8b`

## Results

- WSL GCC: generic typed-call 11/0; value-SemIR 8/0; MethodInfo 3/0; code stripping 37/0; SemIR 10/0;
  generic sharing 9/0; debug metadata 6/0; typed-call 4/0.
- WSL Clang: generic typed-call 11/0; value-SemIR 8/0; MethodInfo 3/0; code stripping 37/0; SemIR 10/0;
  generic sharing 9/0; debug metadata 6/0; typed-call 4/0.
- Windows MSVC x64 Debug: all eight targets rebuild with exit code 0 and pass the same counts; generic typed-call also
  reports three expected Unix-only ignores.
- MSVC retains only the existing temporary-directory MSB8029 and third-party warnings.

## Baseline Deviations

- GCC source contracts remain 20/24. The four unrelated static-text failures cover direct stack-copy scalar sync, typed
  arithmetic literal/written-before, `NEG_SIGNED` bool equality, and method-token table emission. The new value-SemIR
  source contract passes.
- GCC call shared-library smoke remains 4/5 on the previously recorded binary-input quickened dynamic-call writer case;
  the value typed-call case passes.

## Acceptance Decision

Accepted at `2026-08-01 08:42:38 +08:00` as AOT 07 A7.2H's exact-count/no-receiver value-SemIR consumption of the
retained ExecIR parameter layout. A7.2, AOT 07, AOT 12, and the broader AOT 07-12 goal remain active.
