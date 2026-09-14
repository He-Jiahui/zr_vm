---
related_code:
  - zr_vm_core/include/zr_vm_core/optimization_remark.h
  - zr_vm_core/src/zr_vm_core/optimization_remark.c
  - zr_vm_parser/include/zr_vm_parser/optimization_remarks.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/optimization_remarks.c
  - zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.h
  - zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c
  - tests/parser/test_ssa_optimization_remarks.c
  - tests/parser/test_ssa_optimization_remarks_projection.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/optimization_remark.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/optimization_remarks.c
  - zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c
plan_sources:
  - docs/plans/ssa/11-tooling-acceptance/02-optimization-remarks.md
tests:
  - tests/parser/test_ssa_optimization_remarks.c
  - tests/parser/test_ssa_optimization_remarks_projection.c
doc_type: testing-guide
status: focused-passed
---

# SSA optimization remarks focused acceptance

## Scope and gate

This acceptance slice verifies the complete in-memory remark path:

```text
ExecIR sink -> parser/source-map adapter -> pointer-free core store
           -> deterministic query/page -> CLI text/JSON or LSP projection
```

The top-level project command still owns artifact loading and CMake target
registration.  This test intentionally passes an already-produced store to
the formal CLI formatter; it does not fabricate a loader or claim a full
repository build.

## Covered assertions

`test_ssa_optimization_remarks.c` checks:

- schema/version, range, status, reason, backend, and counter diagnostics;
- proven/estimated/measured/unavailable evidence separation;
- software inline-cache versus hardware-cache counter identity;
- bounded-store truncation and page/store dropped count;
- source-version invalidation and deterministic query paging;
- address-free `siteKey` preservation through parser, JSON, text, and LSP;
- ExecIR reason mapping and canonical source-map identity;
- unknown reason, missing pass, malformed sink, non-measured counter rejection,
  invalid masks, and transactional rollback.

`test_ssa_optimization_remarks_projection.c` checks:

- `explain optimize` option parsing (module, reason/backend/range filters,
  pagination, `--json`, and malformed-filter rejection);
- CLI text/JSON schema/version, module, reason, site key, and source identity;
- LSP UTF-8-byte to UTF-16 range projection;
- explicit document version zero, stale-version rejection for matching identity,
  unrelated-candidate isolation, invalid query rejection, and cancellation;
- cache generation/version publication and stale generation rejection.

## Reproducible commands

Run from the WSL mount `/mnt/e/Git/zr_vm`:

```bash
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic \
  -Wstrict-prototypes -Wmissing-prototypes -Werror \
  -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include \
  zr_vm_core/src/zr_vm_core/optimization_remark.c \
  zr_vm_parser/src/zr_vm_parser/diagnostics/optimization_remarks.c \
  tests/parser/test_ssa_optimization_remarks.c \
  -o /tmp/ssa_opt_remarks_gcc && /tmp/ssa_opt_remarks_gcc

clang -std=c11 -O2 -Wall -Wextra -Wpedantic \
  -Wstrict-prototypes -Wmissing-prototypes -Werror \
  -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include \
  zr_vm_core/src/zr_vm_core/optimization_remark.c \
  zr_vm_parser/src/zr_vm_parser/diagnostics/optimization_remarks.c \
  tests/parser/test_ssa_optimization_remarks.c \
  -o /tmp/ssa_opt_remarks_clang && /tmp/ssa_opt_remarks_clang
```

Projection test (the CLI and LSP sources require their private source include
roots and the library include root pulled by `lsp_interface.h`):

```bash
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic \
  -Wstrict-prototypes -Wmissing-prototypes -Werror \
  -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include \
  -Izr_vm_library/include -Izr_vm_cli/include -Izr_vm_cli/src/zr_vm_cli \
  -Izr_vm_language_server/include \
  -Izr_vm_language_server/src/zr_vm_language_server \
  zr_vm_core/src/zr_vm_core/optimization_remark.c \
  zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c \
  zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c \
  tests/parser/test_ssa_optimization_remarks_projection.c \
  -o /tmp/ssa_opt_projection_gcc && /tmp/ssa_opt_projection_gcc

clang -std=c11 -O2 -Wall -Wextra -Wpedantic \
  -Wstrict-prototypes -Wmissing-prototypes -Werror \
  -Izr_vm_common/include -Izr_vm_core/include -Izr_vm_parser/include \
  -Izr_vm_library/include -Izr_vm_cli/include -Izr_vm_cli/src/zr_vm_cli \
  -Izr_vm_language_server/include \
  -Izr_vm_language_server/src/zr_vm_language_server \
  zr_vm_core/src/zr_vm_core/optimization_remark.c \
  zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c \
  zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c \
  tests/parser/test_ssa_optimization_remarks_projection.c \
  -o /tmp/ssa_opt_projection_clang && /tmp/ssa_opt_projection_clang
```

Observed on 2026-09-14: WSL GCC 11.4 and Clang 14 strict builds and both
focused executables exited 0.  Windows-host GCC 4.8.3 also built and ran the
core/parser executable and syntax-checked the CLI source.  Clang/GCC ASan +
UBSan focused runs exited 0 (the Clang sanitizer executable uses `-fno-pie
-no-pie` for this WSL runtime).  Earlier RED evidence was a missing
`zr_vm_core/optimization_remark.h`; the focused tests then exposed and
guarded transactional import, malformed sink bounds, invalid masks/counter
combinations, and unknown reason handling.

## Sanitizer and ownership checks

The core/parser focused executable was also run with GCC and Clang
`-fsanitize=address,undefined -fno-omit-frame-pointer` and
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`; both exited 0 without sanitizer
diagnostics.  Projection pages are released by
`ZrLanguageServer_LspOptimizationRemarkPage_Free`; core pages by
`ZrCore_OptimizationRemarks_PageFree`; the store by
`ZrCore_OptimizationRemarks_StoreFree`.  No remark record stores a pointer.

## Integration hand-off

The parent integration phase should add the new core/parser/CLI/LSP sources to
their owning CMake targets and register `ssa_optimization_remarks` using the
two focused test sources.  It should route the existing command parser's
`explain optimize` token pair to `ZrCli_ExplainOptimizeOptions_Parse`, obtain
the compiler-owned remark store, and call `ZrCli_ExplainOptimize_RunStore`.
The LSP handler should pass the current document version and cache generation
to the versioned projection/query APIs.  No umbrella header, shared registry,
or top-level index was changed in this slice.

## Limits

No full CTest/SSA matrix, artifact persistence, PMU availability, or dynamic
hardware-cause inference is claimed here.  Unknown measurements remain
`null`/`unavailable`; they are never converted into measured estimates.
