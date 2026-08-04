---
scope:
  - Syntax 14 M1-M4
status: passed
last_verified: 2026-08-05
---

# Syntax 14 typed test harness acceptance

## Accepted contract

- Tests remain ordinary `fn`/`async fn` declarations carrying only the
  `zr.testing.test`, `case`, and `skip` roles.
- TestManifest stores canonical semantic function SymbolId/TypeId,
  module-qualified names, module signature identity, source span, async/skip
  state, and bound case constants. Production compilation trims test roots.
- `AssertionFailure` carries structured source span and bounded expected/actual
  snapshots. Formatter faults cannot replace the original failure.
- Public `throws<E>(fn() -> void): E` keeps one source argument. Compiler
  lowering supplies hidden canonical `E` TypeId metadata, the official
  descriptor constrains `E` to `Error`, and the runtime requires an exact or
  allowed subtype match.
- Runner discovery and execution are deterministic. LSP lenses and Debug frame
  projection consume TestManifest instead of reconstructing roles from source
  spelling. Arguments and async logical stacks remain ordinary runtime views.
- `%test`, `test fn`, and `#zr.test.*#` remain migration/negative-fixture input
  only and have no production parse/lowering path.

## Regression corrections

- Artifact roundtrip expectations now use the current source patch level, which
  includes closure-capture metadata, rather than the former TestManifest patch
  level.
- Manifest display names are asserted as module-qualified names rather than the
  former unqualified compatibility projection.

## Validation matrix

Each toolchain built and ran the same eight executables independently:

| Executable | Tests |
|---|---:|
| `zr_vm_test_role_binding_test` | 8 |
| `zr_vm_test_manifest_roundtrip_test` | 1 |
| `zr_vm_test_runner_test` | 5 |
| `zr_vm_testing_assertions_test` | 12 |
| `zr_vm_language_server_lsp_advanced_editor_features_test` | 60 |
| `zr_vm_debug_agent_protocol_test` | 7 |
| `zr_vm_percent_test_migration_test` | 4 |
| `zr_vm_type_inference_test` | 122 |
| **Total per toolchain** | **219** |

| Toolchain | Result |
|---|---:|
| Ubuntu 22.04 GCC 11.4 Debug | 219/219 |
| Ubuntu 22.04 Clang 14.0 Debug | 219/219 |
| MSVC 19.44 Debug | 219/219 |

The MSVC Debug protocol executable completed 7/7 in 151.5 seconds; the earlier
180-second aggregate shell run was a timeout of the surrounding command, not a
test failure or deadlock.

## Review result

The final delta review found no unresolved correctness issue in the promoted
surface. Decoder failure cleanup remains valid for partial manifests, Debug
agent shutdown remains valid after partial initialization, and exception
matching reuses the canonical core exact/subtype rule. Review-added coverage
forces a full GC inside the `throws<E>` callback and proves the hidden TypeId is
reloaded from its tracked argument slot; non-`Error` generic arguments fail at
the descriptor/type-inference boundary. Expected diagnostics from deliberately
invalid LSP binding fixtures are test output, not failures.
