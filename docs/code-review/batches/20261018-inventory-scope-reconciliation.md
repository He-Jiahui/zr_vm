---
related_code:
  - scripts/code_review_inventory.py
implementation_files:
  - docs/code-review/inventory.tsv
plan_sources:
  - user: 全仓库首方代码审查与注释方案
  - docs/code-review/comment-standard.md
tests:
  - scripts/code_review_inventory.py
doc_type: review-batch
---

# 首方范围与陈旧状态维护

本批在 main f03219a5e8d97bd049d126158bff7065e3a734d1 上维护文件清单，采集时间为 2026-10-18T06:05:20.425777+00:00。现行 scanner 从 Git 当前已跟踪及未忽略的未跟踪文件中辨认首方范围，得到 4248 个文件；原清单 4194 行。本批补录 54 个文件为 pending，并把 49 个源码哈希已变化的 reviewed 行退回 pending。新增的 parser/SSA 支持模块、测试、脚本与配置随后仍须逐单元审查。

## 判定和生命周期

新增行记录采集时的路径、分类和 SHA256。活动会话可能继续修改这些源码，因此该哈希仅定位本次观察；pending 保持待核查状态。本批不接管源码，不推断源码租约已释放，不写产品、测试或脚本代码。陈旧行保留原 SHA256 和原 note，并在 note 尾部记明重审原因。原有 203 条陈旧 pending 行的哈希保持原值，便于后续按实际模块更新。

清单中的 49 条外部 worktree 改动共 86 个字段，和本次撤回的 49 个键没有交集；它们的原始字节全部保持。暂存以当前 HEAD 为基底，只投影本批既有键的 status/note 和明确的 54 个新增键；外部 worktree 差异继续留在工作区。其他既有行、表头和单元级 coverage 台账均保持原字节。

## 证据和适用边界

详细键、旧行、新行、分类器哈希、Git 枚举进程、外围保持性及采集时状态位于 inventory-scope-reconciliation-delivery-r1-ledger.json；完整 SHA256 与实际命令结果位于同前缀的 checks/serial-plan/staged/commit 收据，产物根为 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/。此前范围观察保留于 parser-artifact-family-delivery-checkpoint-r1.json，SHA256 e39de2689b3bc4423536231a0eed4147337a2ca471fae37f7d84adbebf23b2db；本批按当前实际范围重算新增54，历史42观察仍按其时刻解释。

本批新增函数、结构、接口及语句块审查信用均为 0；不新增 BUG/TODO，不将本批状态修正算作语义复核。代码审查仍需给出定义、调用方、入口、测试证据、契约和逐单元结论。相关 C 构建和 native 测试由 Schema 批次等待依赖源码释放后另行取得真实基线与后测，本批无 C 改动，不引用它们作为通过证据。

运行现有 inventory check 并保留修改前后实际状态、输出和已知全局限制，再检查具体暂存差异。全仓库完成门槛仍未达到，不能由清单行数或既有 coverage 行数推断全部模块已经覆盖。动态新路径与后续源码变化应在下一次范围复扫中继续纳入。
