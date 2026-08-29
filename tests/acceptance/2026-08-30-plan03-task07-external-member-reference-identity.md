# Plan 03 Task 7.26 External Member Reference Identity Acceptance

## 验收基线

- 基线 HEAD：`3bec1f2f58336bb5b65e8d1e93b77071d6aa6b82`。
- code/test overlay：
  `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c`、
  `tests/language_server/test_lsp_semantic_query_parity.c`、
  `tests/language_server/test_lsp_external_member_reference_identity_cases.h`。
- RED：exact external member query 的 declaration range 被改成另一范围后，旧 matcher 仍通过
  module/type/member spelling 返回两个 `addLast` usage，进程 exit 1。
- GREEN：任一侧声明 exact identity 后，不相等即不产生 usage location；原始 query 仍返回两个
  usage，mismatch 返回 false/zero locations。

## 验收结果

- GCC 11.4、Clang 14、MSVC 19.44 semantic-query parity 均 12/12、真实 exit 0。
- 三工具链 LSP source-contract 均输出 `PASSED`、真实 exit 0；MSVC
  `VSCMD_VER=17.14.38`。
- GCC/Clang/MSVC project feature suite 均真实 exit 0；三套都保留同一 9 条既有 marker，
  binary import references、binary highlights、native import references/highlights 均 PASS，新增
  marker 0。GCC parent/overlay A/B 的 marker 名称逐项相同。
- MSVC 固定 snapshot 先从 HEAD 补齐 Task 7.24 exact paths，再叠加本任务三路径，避免消费共享
  工作树中 benchmark、ownership 与 Syntax05 的并行修改。
- `git diff --check` 通过；测试命令均让可执行文件直接决定退出码，未使用会被 PowerShell 提前
  展开的 bash 状态变量 wrapper。

## 状态与产出记录

- 完成时间：2026-08-30 02:53 +08:00。
- 状态：通过本子里程碑验收。
- 完成项目：RED/GREEN、exact identity mismatch fail-closed、三工具链 focused 验证、project
  marker delta 审计、binary/native reference regression closure。
