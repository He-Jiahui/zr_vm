---
related_code:
  - scripts/code_review_inventory.py
implementation_files:
  - scripts/code_review_inventory.py
plan_sources:
  - docs/code-review/comment-standard.md
tests: []
doc_type: workflow-detail
---

# 审查清单工具的范围与执行契约

本批逐行复核 scripts/code_review_inventory.py 当前实现，共 9 个函数和 1 个模块级 inventory policy 单元，记录在 coverage/review_tooling.tsv。原有模块与函数 docstring 保留；新增 Python # 文档块记录职责、输入/输出、约束、失败路径和真实的内部调用方向。

调用从文件末尾的 __main__ 守卫进入 main。普通动作由 main 依次调用 load_existing、expected_rows 和 render；expected_rows 委托 worktree_paths 收集候选路径并调用 classify 判定角色，render 按调用方提供的行序序列化。check --require-complete 在清单新鲜时调用 check_coverage；清单过期时 main 先返回，不进入该覆盖检查。check-batch 调用 check_batch，后者通过 review_source_path 校验台账文件名和证据路径。仓库内代码检索未发现其他生产调用点；规范文档提供 CLI 用法，不能据此排除仓外使用者。

check 不写 inventory，但会读取 Git 路径列表、候选源码以计算哈希和当前 inventory。sync --require-complete 在覆盖检查之前先写 inventory，因此即使命令最终因 pending 或 coverage 问题返回 1，写入已经发生且不会自动回滚。旧五列 inventory 没有 SHA，读取兼容表头不代表可保留 reviewed 状态，expected_rows 会将其重置为 pending。check-batch 校验九列、重复键、仓内现存路径和非空行号，但不认证调用方向、锚点与结论的语义关联、源码编码或单元清点完整性。

配置单元保留一个明确的范围 TODO。当前路径快照里有 6 个位于 expected/golden 路径段的 JSON 被规则跳过，而测试源码直接消费或要求其存在：

- tests/fixtures/projects/syntax_reference_v1/golden/coverage.json
- tests/fixtures/projects/syntax_reference_v1/golden/diagnostics.json
- tests/fixtures/projects/syntax_reference_v1/golden/provider-locator.json
- tests/fixtures/syntax_migration_frontend/expected/machine_forms.json
- tests/fixtures/syntax_migration_inventory/expected/inventory.json
- tests/fixtures/syntax_migration_inventory/expected/repository-inventory.json

tests/scripts/test_syntax_migration_inventory.py 读取两个 expected 清单；tests/cli/syntax_migration_smoke.js 比较 CLI 输出与 expected 报告；tests/parser/test_syntax_reference_v1.c 要求 coverage、diagnostics、provider-locator 三份 golden 存在，并读取 coverage 与 provider-locator 内容。消费证据不能证明这些文件的生成和维护归属；需要逐文件沿生成器与消费者确认哪些是人工维护的 fixture，再讨论精确路径规则。worktree_paths 的 --exclude-standard 会过滤被忽略的未跟踪路径；--cached 仍返回已跟踪且命中忽略规则的路径。因此忽略来源可能隐藏未跟踪的首方文件，不能据此排除已跟踪文件。该批只记录当前行为，不改变分类规则。

语义核查覆盖完整函数体、CLI 守卫及当前测试消费位置；九个 AST 函数定义与台账逐项一致，模块策略另有一条 TODO，合计十项。源码只增加 # 注释，原有模块与函数 docstring、公开签名和 AST 均不变；未新增测试。

Root 使用 Python 3.12.10 验证：修改前后 AST 摘要均为 `5f24dd64b1249f203f46e65431abf2c01b1ceb776464e0f9872acdfec585aad7`；当前源码 SHA-256 为 `efef2c51c209d89129e7bd1b34ec2e103c0214643a33e7f0ab21ae1c5830dda2`。现有 `check-batch` 退出 0；不兼容的 `--require-complete` 参数退出 2；缺失批次退出 1。全仓 `check` 在修改前后均退出 1，原因是共享 inventory 与并发工作树尚未同步，该结果不计作全仓验收通过。最终锚点验证记录于 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/tooling-final-results.json`。

本批只刷新共享 inventory 中本工具的摘要与结论，保留其他行；inventory 已有其他会话差异，文件级提交延后。模块提交包含本工具注释、十项台账、当前批次文档与续行计划；全仓目标仍须完成其余模块和最终重扫。
