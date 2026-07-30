---
plan_id: aot-12-stripping
record_id: 2026-07-30-debug-sidecar-reachability
status: completed
completed_at: 2026-07-30 14:44:39 +08:00
source_plans:
  - docs/plans/aot/12-code-stripping.md
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
evidence_scope: sub-milestone
---

# AOT 12 Debug Sidecar Reachability

## 状态与产出记录

- 完成时间：2026-07-30 14:44:39 +08:00
- 状态：canonical execution-location sidecar 的全树 verifier、owner-linked reachability manifest 与裁剪统计
  子里程碑完成；AOT 12 S1/S3/S6 仍为部分完成，AOT 07~12 总目标继续进行。
- 完成项目：ExecIR 在构建任何函数前校验完整 source function tree；不可达 owner 的 nonempty/null 表、负数或
  越界 instruction offset、offset 乱序、倒置行区间与倒置同一行列区间均 fail closed。
- 完成项目：允许 quickening 将不同 source range 合并到同一 instruction offset；合法的重复 offset 与
  `rowCount > instructionCount` 不会被误拒。
- 完成项目：code stripping 独立报告 `debugLocationsBefore/After/Removed`，并按 flat owner、source location
  index 发布版本 1 `debugSidecarManifest`；每行以 retained owner 作为 predecessor。
- 完成项目：主 fixture 精确报告 4→3→1，owner 0 行先于 owner 1 的两个 coalesced source row；zero-row
  fixture 报告 0→0→0 且无 node。
- 计划映射：完成 AOT 12 S1 debug sidecar node schema、S3 owner-function trim 与 S6 count/reason reporting
  子切片；不将 S1/S3/S6 或 AOT 12 标为完成。

## 代码与文档产出

- `backend_aot_exec_ir_source_location.h/.c` 新增 canonical execution-location verifier。
- `backend_aot_exec_ir.c` 在 frame/basic-block 或 source-location 消费前执行完整树校验。
- `backend_aot_c_debug_sidecar_manifest.h/.c` 新增 retained row count、ExecIR owner 一致性检查、稳定排序与
  version 1 manifest writer。
- `backend_aot_c_emitter.c` 接入裁剪前后 row count、removed delta、manifest publication 与失败清理。
- `test_aot_c_code_stripping.c` 覆盖 4→3、0→0、multi-owner/source-row order、quickening coalescing 与六类
  malformed unreachable sidecar。
- 模块文档同步 `aot-function-reachability-manifest.md` 与 `csharp-value-type-semir-aot.md`；验收入口为
  `tests/acceptance/2026-07-30-aot-12-debug-sidecar-reachability.md`。

## 验证结果

- 初始 RED：冻结 WSL GCC 基线 30/30；扩展为 31 项后，缺失 debug stats/manifest、malformed unreachable
  sidecar 被接受、zero-row 输出缺失共三项失败，其余 28 项通过。
- 独立审查发现首版 `rowCount <= instructionCount` 会误拒 quickening coalescing。正例改为两个不同 source
  range 共用一条指令后，修复前 30/31，移除该上限并保留逐行 offset gate 后恢复 31/31。
- 独立复审确认 quickening P1 与缺失状态链接 P3 均闭合，最终返回 `No findings`。
- WSL GCC 11.4、WSL Clang 14.0、Windows MSVC 19.44 均完成 focused build 与 code stripping 31/0；GCC
  相邻 generic reference sharing 为 9/0。
- 生成 C 精确报告 `debugLocationsBefore=4`、`After=3`、`Removed=1`；manifest 按 owner 0/1 与 source row
  0/1 排序，owner 1 两行都映射 offset 0；malformed 输出不存在。
- MSVC 仅保留冻结目录位于 `%TEMP%` 的既有 MSB8029 warning；未修改 `tests/CMakeLists.txt`、parser/core
  producer、runtime debug 模块或活跃 syntax/LSP/CI 路径。
- 七份生产代码/测试文件在主工作树、WSL 冻结树与 Windows 冻结树 SHA-256 完全匹配。

## 未完成边界

- 本切片只发布 generated-C diagnostic manifest，不定义 AOT 11 versioned DebugMap artifact section、source
  checksum、document table、local scope 或 artifact loader contract。
- 不生成 A7.4 safepoint variable-location map，不重写 register/spill/provenance/cleanup/deopt location，也不
  完成 A7.4。
- debug/release/aggressive policy、reflection/debug token remap、删除 token 的 debug visibility、四 backend
  parity、binary size/behavior 总验收与 AOT 07~12 总目标仍开放。
