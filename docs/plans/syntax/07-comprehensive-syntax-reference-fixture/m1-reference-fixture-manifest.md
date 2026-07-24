# Syntax 07A M1 Reference Fixture And Coverage Manifest Record

Design source: `docs/plans/syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md`,
Task 1 (07A).

Implementation plan:
`docs/plans/syntax/07-comprehensive-syntax-reference-fixture/m1-reference-fixture-manifest-implementation-plan.md`.

## 状态与产出记录

- 状态：completed
- 开始时间：2026-07-24 19:00 +08:00
- 完成时间：2026-07-24 19:47 +08:00
- 已完成项目：
  - 建立 `zr_vm_syntax_reference_v1_test` 的 TDD 红测：fixture 缺失时两个 discovery contract 均明确失败。
  - 新增 29 个稳定 feature id、`current`/`negative`/`design-pending` 三类 collection、pending owner
    plan/gate/post-promotion expectation，以及 platform-neutral provider/locator golden。
  - 建立 Workspace `engine.render` 与 RegisteredNative `native:engine.render` 的并列 skeleton，保留逻辑
    identity，不让 fixture 路径进入 public golden。
  - 新增 formatted/minified host source 的 parser/writer proof。Red input `return 8` 触发 fingerprint
    mismatch；恢复 `return 7` 后 `.zrs` 和排除 source-map line range 的 `.zri` 指纹一致。
  - focused test 现在在运行时将真实 fixture path 投影为 canonical local `file:` URI，并覆盖 POSIX、盘符
    与 UNC authority (`file://host/share/...`) 形式；检查入库的 locator golden 仍只保留
    `${SYNTAX_REFERENCE_FILE_URI}` placeholder，因而不泄露 host path。
  - 已通过 GCC 11.4、Clang 14 和 MSVC 19.44 的 `zr_vm_syntax_reference_v1_test` 定向验证，三套均为
    7 tests / 0 failures，且每套 focused `syntax_reference_v1` CTest 均为 1/1 passed。
  - 已完成 coverage JSON 静态审计：29 feature、13 current、1 negative、15 design-pending，pending
    owner plan/gate/post-promotion expectation 缺失数和 status/collection mapping mismatch 均为 0。
  - 已补齐 module/testing documentation 与 acceptance evidence：
    `tests/acceptance/2026-07-24-syntax-07a-m1-reference-fixture-manifest.md`。
  - 已完成独立只读代码审查、exact-path audit 与本里程碑 commit。

## Boundary

07A 只建立可发现、可审计的 fixture skeleton。`08-14` 和 `10R/10F/10C` 的 feature slot 保持
`design-pending`，不作为 compiler/project/VM/AOT/LSP current-pass 证据。07B 才能按各 owner promotion
gate 将它们改为 `current`。
