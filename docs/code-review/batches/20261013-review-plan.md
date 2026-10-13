---
related_code:
  - scripts/code_review_inventory.py
  - zr_vm_jit/include/zr_vm_jit/backend.h
  - zr_vm_jit/src/orc_backend.cpp
  - zr_vm_jit/src/jit_state_maps.cpp
implementation_files:
  - scripts/code_review_inventory.py
  - zr_vm_jit/CMakeLists.txt
  - zr_vm_jit/include/zr_vm_jit/backend.h
  - zr_vm_jit/src/orc_backend.cpp
  - zr_vm_jit/src/jit_state_maps.cpp
plan_sources:
  - user: 2026-10-13 全仓库首方代码审查与注释方案
tests:
  - tests/core/test_ssa_host_jit_optional.c
  - tests/cmake/ssa-tests.cmake
doc_type: review-plan
---

# 首方契约审查续行计划

在实际 `main` checkout 继续全量审查、按需注释。每个函数、类型、接口、可调用宏和有独立约束的语句块均须有具体定义、实际调用或间接入口、测试证据、契约和结论。文档级注释按 [现有规范](../comment-standard.md)覆盖职责、使用阶段、参数返回、所有权、失败状态及线程或生命周期边界；不得改变 API 与程序行为。

## 当前清单与归属

初始 HEAD/main：`901f9898d2326d7b2694a89a77c14255003632d9`。现有清单 3963 行（1805 reviewed、2158 pending），这是文件级历史记录，不等于当前全仓审查完成。对当前 tracked 与 untracked 文件复算得到 3965 个候选，新增的 `scripts/layout/generated_includes.py` 和 `tests/tooling/test_layout_generated_includes.py` 属于活跃迁移工作，先登记遗漏，不修改其代码。当前 reviewed 文件的 SHA 复核未发现失效；该机械结果不授予逐单元注释格式或调用链复核信用。

3965 是现有扫描器规则产生的候选数。`classify()` 按路径分段跳过 `expected`、`golden`、`snapshots` 等目录，尚须追溯生产者与消费者，区分人工编写样例和生成数据；候选数不能充当全部首方文件的最终总数。新增注释会改变原始 SHA，当前批次须重记摘要与逐单元证据。

初始普通索引的三份旧 plan 删除和 Rust Cargo.lock 属于其他任务。当前 core、parser、AOT、LSP、扩展及其耦合文件保留其他会话归属；CLI、math、system、testing、Thread runtime 实现也已有差异，按实际路径避开。

报告根为 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/`，先登记 `artifacts.json` 再创建报告或构建。初始状态、差异、索引条目和文件清单已保存在该报告根。它们是 Git 文本记录及各自 SHA，不是源码镜像，也不是全树持续静默证明。

## 功能批次

1. **JIT 可选后端与状态映射。** 四个文件和对应台账当前无既有差异。producer 先独立清点公开声明、内部函数、结构、回调及关键块，核对 core manager、descriptor vtable、可选 CMake 和既有测试；Root 在修改前运行定向验证，再允许直接写入中文注释。台账迁移为当前具体 `path:line`，独立 reviewer 检查每个单元的证据和遗漏。
2. **Thread 公开 runtime 契约。** runtime.h 的 11 项先只读复审，检查已有文档是否足够、引用是否随迁移漂移。相邻八 helper 仅追踪真实控制流；实现和旧台账有并发差异，未取得文件释放前不写入，也不将只读候选记作已应用。
3. **审查清单工具。** `scripts/code_review_inventory.py` 的九个顶层函数及一个模块策略单元共十项（CLI 守卫随 main 核查），连同 `coverage/review_tooling.tsv` 按当前调用方向复核。保留 docstring 与 AST，补充路径校验、摘要失效、写入顺序和失败状态；验证既有 CLI 正常与拒绝路径。
4. **下个独立模块。** 范围审计提供具体 clean 文件和既有单元清单；iteration 先只读清点当前实际路径与测试。其他任务的脏文件和未释放路径继续延后。
5. **最终验收。** 重扫首方候选、所有台账和 BUG/TODO 标签，核对新增或变更文件，确认无未审单元。未达到此门槛时保持全仓 goal active，不把有限批次扩大为完整验收。

## 每批验证与提交

- 新标签须对应可复现或完整可达 BUG 证据；未知约束写 TODO 和具体核查入口，只记录问题。
- 核对所有新增注释及台账与当前真实调用方向。独立 reviewer 实际阅读定义和相关上游、下游失败路径。
- 运行本批 `check-batch`、相关构建和现有测试；与修改前同一配置的结果逐项对照。WSL 实际不可用时记录限制，不借用历史 GCC/Clang PASS。
- 用 Git diff 和文件 SHA 确认源码身份；验证非注释内容、预处理边界和公开签名不变。保护字符串和 raw literal，不能用简单正则删除注释来替代词法核对。
- 重新核对 HEAD、锁和普通索引，检查暂存范围与完整差异，执行 `git diff --cached --check`。以 He-Jiahui 身份仅提交本任务的注释、台账和必要文档更正，保留其他任务暂存内容。每个完整功能批次验收后立即提交。
- 等待自有原生命令退出；清理明确归属的临时 helper/生成夹具，登记保留构建、诊断与实际释放字节数。未知历史产物保留，不能宣称全部关闭。
