---
plan_id: aot-07-codegen
record_id: 2026-07-30-execir-frame-abi-verifier
status: completed
completed_at: 2026-07-30 14:00:28 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# 07-A7.2A ExecIR Frame ABI Verifier And Retained Manifest

## 状态与产出记录

- 完成时间：2026-07-30 14:00:28 +08:00
- 状态：ExecIR frame ABI verifier/reporting 前置子里程碑完成；AOT 07 A7.2 仍为部分完成，AOT 07~12
  总目标继续进行。
- 完成项目：ExecIR build 在复制 frame sidecar 前校验完整 function tree；不可达 owner 的损坏布局不能再被
  code stripping 隐藏。
- 完成项目：空布局、parameter/stack count、table/count/allocation 上界、frame/slot power-of-two alignment、
  kind/TypeLayout identity、已知 flag 组合、parameter marker、stack slot 唯一性和物理 storage span 均 fail
  closed。
- 完成项目：direct alias 允许合法 span 重叠；indirect/borrowed alias 按 binding 的 size/alignment 校验物理
  frame storage，payload 的更高 alignment 不会被误拒。
- 完成项目：裁剪后按 stable flat owner 与 source slot-layout index 发布版本 1 `frameLayoutManifest`，并报告
  `frameLayoutSlotsBefore/After/Removed`；每行保留完整 slot ABI 和 owner predecessor。
- 计划映射：完成 A7.2 frame ABI 的现有 sidecar verifier/reporting 前置切片，并补充 AOT 12 retained-node
  auditability；不将 CallableContract frame derivation 或完整 A7.2 标为完成。

## 代码与文档产出

- `backend_aot_exec_ir.c` 新增 frame sidecar verifier，并在任一 slot allocation/copy 前执行。
- `backend_aot_c_frame_layout_manifest.h/.c` 新增 retained frame-row count、稳定排序与版本化 manifest writer。
- `backend_aot_c_emitter.c` 接入裁剪前后 row count、removed delta、manifest publication 与失败清理。
- `test_aot_c_code_stripping.c` 覆盖 malformed unreachable alignment/span、value/inline multi-owner order、同 offset
  direct alias、indirect/borrowed binding、4→3/3→2 trimming 和 zero-row manifest。
- 模块文档同步 `csharp-value-type-semir-aot.md` 与 `aot-function-reachability-manifest.md`；验收入口为
  `tests/acceptance/2026-07-30-aot-07-execir-frame-abi-verifier.md`。

## 验证结果

- 初始 RED：冻结 WSL GCC 原基线 28/28；扩展到 29 项后，缺失 manifest/count 与不可达坏布局错误接受两项
  失败，其余 27 项通过。
- 独立审查发现 payload alignment 在 alias binding 物理 storage 重写前被过严比较；新增第 30 项后稳定复现
  indirect alias 误拒。修复为校验最终 `storageAlign`，并补齐 overlapping/indirect/borrowed/multi-owner/empty
  coverage；最终复审无发现。
- WSL GCC 11.4、WSL Clang 14.0、Windows MSVC 19.44 均完成 focused build 与 code stripping 30/0；GCC
  相邻 generic reference sharing 为 9/0。
- 五份生产代码/测试文件在主工作树、WSL 冻结树与 Windows 冻结树 SHA-256 完全匹配。
- 生成 C 精确报告主 fixture 3→2、alias fixture 4→3；owner 0 value 行先于 owner 1 inline 行，同 owner
  direct/alias 行保持 slot-layout 顺序；indirect `0x0003` 与 borrowed `0x0013` 行保留高对齐 payload；两个
  malformed 输出不存在。
- 冻结 GCC `source_contracts` 为 21/24，`frame_setup_contracts` 为 0/1；四项均是既有 source-text contract
  漂移，在文本扫描断言处失败且不执行本 verifier。本提交不修改对应 source contract、frame setup、parser、
  core runtime 或 CMake。MSVC 仅保留冻结目录位于 `%TEMP%` 的既有 MSB8029 warning。

## 未完成边界

- 本切片不从 Canonical CallableContract 生成 receiver、`in/ref/out`、return、aggregate destination、spill 或
  address-taken slot，不完成 A7.2。
- register allocation、GC/debug/provenance/cleanup map 重写、exception cleanup、closure/module/async environment
  隔离与 generation key 仍开放。
- C/LLVM frame layout golden 等价、dynamic boundary thunk 收敛、四 backend、性能 guardrail 和 AOT 07~12
  总验收仍开放。
