---
related_code:
  - scripts/syntax_migration_inventory.py
  - scripts/syntax_migration_embedded.py
implementation_files:
  - scripts/syntax_migration_inventory.py
  - scripts/syntax_migration_embedded.py
plan_sources:
  - .codex/plans/20261016-review-bugfix.md
  - docs/code-review/coverage/scripts_remaining.tsv
tests:
  - tests/scripts/test_syntax_migration_embedded_positions.py
  - tests/scripts/test_syntax_migration_inventory.py
doc_type: module-detail
---

# 嵌入式迁移清单的宿主坐标

清单扫描器对宿主文件中的字符串进行有限解码，再识别其中的 Zr 旧语法。解码后的换行影响语言扫描和注释边界，却不能成为宿主文件里不存在的新行。独立的 syntax_migration_embedded 模块为每个解码字符保存宿主 offset，并保存结束位置的哨兵；所有 finding 的起点与尾后终点各自映射回宿主文本的一基行列。

相邻字面量各自保存真实范围，原有 source 上下文筛选与扫描规则保持不变。这里仍只使用既有的有限转义规则：反斜杠 n 变为换行，其他转义取后一个字符，不声称等价于宿主语言求值器。提取逻辑从原大文件移出，报告协议类型和扫描编排仍由 inventory 模块负责。

四项实际回归分别覆盖同物理行的转义换行、解码后的注释边界、相邻字面量、真实换行与缩短转义的混合。修改前4项均FAIL，修复后4项均PASS；边界文件读取使用内存夹具，位置断言仍以完整宿主字符串为准。既有11项脚本测试10项通过，唯一失败为仓库门禁发现5处wiki当前文档的旧语法示例，最早在error-handling.md:148；该失败保留，未放宽门禁或刷新golden遮盖。日志和精确输入哈希位于 E:/cargo-targets/zr_vm/reports/review-bugfix-20261016/syntax-positions/。
