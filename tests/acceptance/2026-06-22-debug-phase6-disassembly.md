---
doc_type: acceptance-record
phase: 6.3
title: bytecode disassembly and CLI dump-bytecode switch
status: accepted
updated_at: 2026-06-22 06:36:30 +08:00
---

# Debug Phase 6.3 Disassembly

## Scope

- Expose a core bytecode disassembly view for compiled functions.
- Print each instruction with offset, opcode name, generic operand view, and source line comment.
- Add CLI `--dump-bytecode <out>` for project run paths.
- Keep dump generation deterministic and non-invasive: write the file after entry load and before execution, then run normally.

Heap summary is accepted separately in `tests/acceptance/2026-06-22-debug-phase6-heap-summary.md`.

## Baseline RED

| Check | Initial result |
|------|----------------|
| `tests/debug/test_disassemble.c` | Manual WSL/GCC link failed with `undefined reference to ZrCore_Debug_DisassembleFunction` |
| CLI args dump-bytecode parse test | Failed before `SZrCliCommand` exposed `dumpBytecodeEnabled` / `dumpBytecodeOutputPath` |
| CLI import fixture dump report | Failed before runtime wrote a disassembly report file |

## Implemented

- Added `ZrCore_Debug_DisassembleFunction(state, function, FILE*)`.
- Disassembly output starts with `ZR_DISASSEMBLY function ...` and `offset opcode operands`.
- Each instruction row includes offset, opcode name, raw `u8`/`u16`/`i32` operands, raw 64-bit instruction value, and `; line N`.
- Added `tests/debug/test_disassemble.c`.
- Added CLI `--dump-bytecode <out>` parse support.
- Runtime dumps the loaded entry function for interp, binary, and debug project run paths before executing user code.
- CLI rejects missing output path and compile-only/non-run modes.

## Validation

| Build | Command / range | Result |
|------|------------------|--------|
| WSL/GCC Debug | `debug_disassemble|cli_args|cli_import_basic_fixture` | 3/3 PASS |
| WSL/Clang Debug | `debug_disassemble|cli_args|cli_import_basic_fixture` | 3/3 PASS |
| Windows/MSVC Debug | `debug_disassemble|cli_args|cli_import_basic_fixture` | 3/3 PASS |

## Decision

Accepted for Phase 6.3 disassembly. Phase 6 planned tooling items through 6.4 are covered by individual acceptance records.
