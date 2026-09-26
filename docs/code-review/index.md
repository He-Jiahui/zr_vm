---
related_code:
  - scripts/code_review_inventory.py
implementation_files:
  - scripts/code_review_inventory.py
plan_sources:
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
tests: []
doc_type: category-index
---

# 代码审查与注释

本目录记录首方代码的调用意图审查。审查先于注释：每个函数、数据结构、接口及其有意义的控制流都要核对实际调用方、资源契约和失败路径；源码只保留有长期价值的解释。

- [注释与审查规范](comment-standard.md)：范围、证据要求、注释格式、问题标签和提交门槛。
- `inventory.tsv`：当前首方代码候选文件及文件级审查状态；由 `scripts/code_review_inventory.py` 对照工作树更新和核验。
- `coverage/`：按功能模块保存 TSV 审查台账，包含已审查但无需注释的单元。

台账中的调用方和证据是审查时点的事实，不代替源码与构建配置。后续代码变更应重新核对受影响的单元。
