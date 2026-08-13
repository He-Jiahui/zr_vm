# LSP L8 Canonical Callable-Value Signature Fact Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
| --- | --- | --- |
| 2026-08-13 11:38 +08:00 | 已完成 | source callable-value call publishes a canonical call fact and signature help fails closed when that fact is unavailable. |

## Scope

- Parser/compiler callable binding for source identifier initializers.
- Callable-value-scoped inferred return-type refinement of the original function SymbolId and canonical TypeId.
- LSP symbol bootstrap projection and signature-help consumption through `CallAt/FormatCall`.

## Baseline

- An unannotated source function was registered with an `object` return before body inference completed, so its callable-value alias could not publish the exact call contract.
- LSP symbol collection registered the variable type but did not register its callable initializer.

## Test Inventory

- Parser positive case: resolved reference, target SymbolId, `fn(int, int, int) -> int`, parameter names and exact formatted call label.
- LSP positive case: signature label exactly equals `FormatCall` output.
- LSP negative case: clearing the same expression fact's `hasCallInfo` makes signature help unavailable, proving no local variable/name/AST fallback.
- Compiler regression case: unrelated unannotated nested functions with destructured shadow bindings retain their prior scope behavior.
- Regression matrix: semantic facts, local query, expression/local hover, interface, project, compiler integration and stdio/CLI.

## Tooling Evidence

- Fixed snapshot: `5922bcb + 9-path overlay`, byte-exact 9/9.
- GCC 11.4 and Clang 14.0 static Debug builds used isolated WSL ext4 source/build directories.
- MSVC 17.14.38 static Debug build used an isolated Ninja directory.
- Direct test executables were run separately; stdio/CLI used `ctest -R "^(language_server_stdio_smoke|cli_integration)$" --output-on-failure`.

## Results

All three toolchains passed canonical 18/18, facts 13/13, local query 32/32, expression hover 9/9, local hover 12/12, interface 111/111, project 58/58, compiler integration 127/127 and stdio/CLI 2/2. Every process returned exit 0.

The first broad refinement implementation caused the existing nested-function destructuring-shadow compiler case to fail by projecting a local value as a function type. Refinement was restricted to the callable-value registration boundary; the final three-toolchain compiler integration runs then passed 127/127.

Two setup attempts were excluded from acceptance evidence: the first WSL archive omitted vendor submodules, so the frozen source was rebuilt with explicit submodule archives; the first MSVC build requested the nonexistent `zr_vm_cli` target, so it was rerun with `zr_vm_cli_executable` before any test result was counted.

## Acceptance Decision

Accepted for the source callable-value identifier-initializer contract. Closure/lambda values and binary/native/provider callable values remain separate fail-closed milestones; L8 remains in progress.
