---
related_code:
  - scripts/code_review_inventory.py
implementation_files:
  - docs/code-review/inventory.tsv
plan_sources:
  - "user: 全仓库首方代码审查与注释方案；已释放会话文件可核查接管。"
  - docs/code-review/comment-standard.md
tests:
  - scripts/code_review_inventory.py
doc_type: review-batch
---

# 会话交接后的首方文件范围补录

本批冻结于 main `7e3d69aa6d7714f96b4caebb8f707ee8965448b9` 之后。现行分类器识别 4261 个首方路径，已有清单 4248 行；差集精确为以下 13 个新路径（4 个配置、6 个测试、3 个实现），没有清单多余路径。采集时间为 2026-10-18T20:47:04.199467+00:00，逐路径内容哈希见 inventory.tsv 与外部审查台账。

本批仅补录 13 个新路径为 pending。文件级清点不授予函数、结构、接口或语句块的审查信用；这些文件仍需检查定义、调用方、间接入口、失败和释放路径及现有测试。此次单元信用为 0，不增加 BUG/TODO 标记。

## 会话归属与后续写入边界

用户已允许核查并接管原 SSA、LSP 与目录整理会话释放的文件。后续新增的占用仍按当前会话记录判断。文件是否参与当前构建由配置和调用链核实，路径名称不构成停用依据。

| 组别 | 路径 | 观察到的归属及限制 |
| --- | --- | --- |
| SSA signed-remainder | tests/cmake/integer-remainder-tests.cmake；tests/cmake/integer-remainder/CMakeLists.txt；tests/core/test_execution_integer_remainder.c | 当前 signed-remainder 会话仍记录有限第44批活动；3个路径具关联证据，两份 CMake 并未逐文件列为租约。此次只登记，后续源码写入继续核查。 |
| SSA admission | tests/cmake/ssa-backend-admission/CMakeLists.txt；tests/cmake/ssa-backend-tests.cmake；tests/core/test_ssa_backend_admission.c；zr_vm_core/src/zr_vm_core/execution/execution_backend_compile.c；zr_vm_core/src/zr_vm_core/execution/execution_backend_facade.c | 5个路径均在既有 7c006421 提交中，有限第42批旧会话已记录关闭；该历史证据不保证之后没有新的使用者。 |
| Macro frame | tests/compileTime/test_comptime_frame_budget_cases.h；tests/compileTime/test_comptime_frame_transaction_cases.h；zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_frame.c | 会话无存活 native job 的观察与 M1 OPEN 同时存在；budget header 和 frame 实现明确关联占用，transaction header 的最新头部未逐文件列租约。源码修改前仍须核实。 |
| Macro 关联测试 | tests/language_server/test_semantic_analyzer_prototype_identity_cases.h；tests/parser/test_type_inference_extern_visibility_cases.h | 关联当前 prototype/extern 可见性工作，最新会话头部没有这2个路径的明确租约或释放证明。此次不推断释放。 |

## 清单保持性和验证

新增行使用采集时真实源码 SHA-256 和现行分类器的 module/kind；pending 表示内容和调用契约仍待逐单元复核。其他现有行、表头、coverage 台账、源码和现有外部暂存项保持原字节。暂存从当前 HEAD 投影且仅允许这13个明确缺失的键，以及本批说明和导航链接。

现有 `scripts/code_review_inventory.py check` 在写入前后均记录实际退出码与完整输出；原有 stale 诊断继续保留，不运行 sync 覆盖其他会话状态。本批没有 C/CMake/TypeScript/Rust 程序改动，采用现有清单检查、13 键投影与暂存 diff 检查验证；不运行与此元数据修改无关的 native 测试。独立 reviewer 在实际暂存后再次核对三路径及外部改动保持性，Root 只在该结论和完整暂存差异复核后串行提交。

新增路径使原有全局清单诊断可能减少，仍不能据此宣称全库审查完成。后续清点还要核查人工维护 fixture 与名称排除规则，并迁移旧物理路径的 coverage 记录；历史行数不直接代表当前定义覆盖。产物生命周期保持 OPEN，手工释放 0 B。
