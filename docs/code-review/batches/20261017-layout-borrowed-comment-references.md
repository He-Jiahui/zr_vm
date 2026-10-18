# Layout 辅助模块既有注释引用更正

本批承接 `9306cc82`，将四份已审源码中 22 处 `References` 行号指向同一条当前程序语句。涉及 11 个既有单元，新增审查信用为 0；定义、调用关系、职责、所有权、失败与生命周期契约沿用下面的既有台账。

| 文件 | 既有单元数 | 引用更正数 | 原字节数 → 当前字节数 |
| --- | ---: | ---: | ---: |
| `scripts/layout/generated_includes.py` | 1 | 2 | 17756 → 17758 |
| `scripts/layout/dormant_contexts.py` | 4 | 8 | 47876 → 47876 |
| `tests/tooling/test_layout_dormant_contexts.py` | 3 | 6 | 42744 → 42750 |
| `tests/tooling/test_layout_generated_includes.py` | 3 | 6 | 27288 → 27294 |

每项仅替换 `# - scripts/layout/apply_moves.py:<行号>` 的数字，不增减源码行。共增加 14 字节，保留各原始 EOL。程序 AST（包含位置）、程序 token（包含位置）、docstring 和同解释器完整编译结果均逐文件相同；注释文本与部分注释后的换行 token 列位置发生变化，完整 token 流并不相同。

既有单元及台账位置如下。台账的定义、caller 和证据引用已在上批迁移到当前程序行，本批无需改写台账行。

| 既有单元 | 台账 |
| --- | --- |
| `scripts/layout/dormant_contexts.py` / `DormantContextError` / `class` | [layout_dormant_contexts.tsv:2](../coverage/layout_dormant_contexts.tsv) |
| `scripts/layout/dormant_contexts.py` / `SourceContext` / `class` | [layout_dormant_contexts.tsv:3](../coverage/layout_dormant_contexts.tsv) |
| `scripts/layout/dormant_contexts.py` / `propagate_dormant_headers` / `function` | [layout_dormant_contexts.tsv:29](../coverage/layout_dormant_contexts.tsv) |
| `scripts/layout/dormant_contexts.py` / `recover_contexts` / `function` | [layout_dormant_contexts.tsv:26](../coverage/layout_dormant_contexts.tsv) |
| `scripts/layout/generated_includes.py` / `classify_include` / `function` | [layout_generated_includes.tsv:11](../coverage/layout_generated_includes.tsv) |
| `tests/tooling/test_layout_dormant_contexts.py` / `APPLY_PATH` / `state` | [layout_dormant_contexts.tsv:32](../coverage/layout_dormant_contexts.tsv) |
| `tests/tooling/test_layout_dormant_contexts.py` / `FIXTURE_ROOT` / `state` | [layout_dormant_contexts.tsv:36](../coverage/layout_dormant_contexts.tsv) |
| `tests/tooling/test_layout_dormant_contexts.py` / `module::apply_moves_import` / `block` | [layout_dormant_contexts.tsv:33](../coverage/layout_dormant_contexts.tsv) |
| `tests/tooling/test_layout_generated_includes.py` / `FIXTURE_ROOT` / `state` | [layout_generated_includes.tsv:16](../coverage/layout_generated_includes.tsv) |
| `tests/tooling/test_layout_generated_includes.py` / `GeneratedIncludeTests.test_missing_header_is_unresolved_and_variants_are_not_collapsed` / `function` | [layout_generated_includes.tsv:30](../coverage/layout_generated_includes.tsv) |
| `tests/tooling/test_layout_generated_includes.py` / `classifier` / `state` | [layout_generated_includes.tsv:15](../coverage/layout_generated_includes.tsv) |

这些引用供维护者回查上层调度、动态装载和迁移入口，不参与执行，不赋予路径、构建图或文件系统对象额外有效期。Dormant 的分离图和有序变体、Generated 的精确生成头白名单及 reparse 拒绝契约保持既有结论。未新增 BUG 或 TODO，未修改公开签名、测试断言或程序行为。

有效修改前基线 `before-r2` 与修改后 `after-r1` 使用同 argv/env 和配置：Dormant 现有 9 项均通过；Generated 现有 10 项为 9 通过、1 因 Windows `WinError 1314` 缺少创建符号链接权限而跳过。两阶段四个测试子进程都实际退出 0，测试身份及结果相同。stderr 仅在 `Ran ... tests in ...s` 和该跳过原因的独占随机 `case-*` 路径分量不同；逐项记录替换后相同，原始日志不相同。未取得符号链接支持时 reparse 分支、native 或跨平台验收信用。

最初 `before-r1` 的测试已输出结果，但外围守门将测试合法创建的空夹具父目录视作变化，实际退出 1，且未序列化子进程退出码，因此不作有效基线。保留失败收据后，以同命令重新执行的 `before-r2` 完整终态作比较。Generated 测试固定使用 `E:/cargo-targets/zr_vm/tests/layout-01a13033/generated-includes-fixtures`；测试只清理本次独占 case，保留空父目录及未知同级内容。

证据位于 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/`：

- 提案 `layout-borrowed-comment-reference-relocation-proposal-r1.json`，SHA256 `a0b139f3bbc10b85aa3fdd43bca1750be23b7305b1897e919b6e55806447c773`；独审 `layout-borrowed-comment-references-proposal-independent-review-r1.json`，SHA256 `2645077b9b724159c93220f56fd1f8b960bd23ddf855a3b9ad6a5a95ff876f21`，ACCEPT。
- 应用 `layout-borrowed-comment-references-source-application-r1.json`，SHA256 `f79565a91345bbb42642e26e01063eee3d9965a014c5ff0276bea7409d9042bd`。
- 前后比较 `layout-borrowed-comment-references-validation-pair-r1.json`，SHA256 `e8aa8ef1eb4f914c9df0b78a25db33f9ff6e7949bcd6526fc9be454f3442c6b1`；两份 plan/results 与原始日志绑定完整 argv/env、源码摘要及进程实际终态。

只提交本批四份源码的纯注释、此记录、导航与清单四行摘要/说明。首方全仓审查尚未完成；保留并发差异和外围暂存内容。必要证据、复用构建/缓存、私有 Git index 及空夹具目录保留，本批手工释放 0 B，产物生命周期 OPEN。
