---
scope: Syntax 08 M5 LSP migration and stress acceptance
status: proven
date: 2026-08-04
toolchains:
  - WSL GCC 11.4
  - WSL Clang 14.0
  - MSVC 19.44 x64 Debug
---

# Syntax 08 M5 LSP migration and stress acceptance

## Accepted consumers

- LSP hover reports the precise canonical reflection descriptor category and
  completion follows the registered descriptor hierarchy.
- The migration planner classifies `%type` and legacy dynamic construction
  edits as machine-applicable, requires-review, or blocked. Production parsing
  is not reopened to support migration input.
- Reflection query stress covers 100,000 members, 512 inheritance levels,
  repeated cache access, compacting GC, constructor exceptions, and 10,000
  dynamically constructed objects.

## Three-toolchain replay

On GCC, Clang, and MSVC, the following all completed with zero failures:

- `zr_vm_language_server_expression_fact_hover_test`: 8 focused assertions,
  including precise reflection hover and descriptor-hierarchy completion.
- `legacy_migration`: canonical edit classification and idempotence.
- `reflection_type_stress`: 4 stress assertions.
- `reflection_type_surface`, the M4 AOT boundary, and official provider
  convergence were replayed in the same command to protect the parent gate.

The GCC and Clang AOT path compiled and loaded a generated shared library; MSVC
validated the guarded platform contract. No test observed a stale descriptor,
partially constructed result, or reflection cache activity from an ordinary
call/construction path.

## Decision

M5 is promoted. With M1-M4 already proven, Syntax 08 is promoted as a complete
dependency for Syntax 11, 10C, 06B, and 07B.
