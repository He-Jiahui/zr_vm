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

## 已验收批次

| 功能边界 | 逐单元台账 | 验证 |
| --- | --- | --- |
| ZRM 容器公开接口与实现 | [53 项](coverage/zr_vm_library_zrm.tsv) | GCC、Clang 的容器测试各 9/9；独立审查发现 2 类已证实缺陷和 3 类待核实边界。 |
| `zr.iteration` 协议 provider | [20 项](coverage/zr_vm_lib_iteration.tsv) | GCC、Clang、MSVC 语法检查通过；GCC 两个模块源文件编译通过，完整测试目标链接因共享构建并发未完成。 |
| CLI 二进制 ZRP 元数据检查 | [24 项](coverage/zr_vm_cli_metadata_dump.tsv) | GCC 定向 CTest 2/2；元数据测试在 GCC、Clang、MSVC 均通过。 |
| `zr.testing` 测试阶段 provider | [68 项](coverage/zr_vm_lib_testing.tsv) | GCC、Clang 语法检查通过；独立 GCC Debug 构建成功，断言测试 12/12；独立审查保留 UTF-8 截断 BUG 与异常快照 TODO。 |
| Wiki 源文件校验器及定向测试 | [31 项](coverage/wiki_source_validation.tsv) | 独立调用链复核通过；Python 单测 5/5，真实 wiki 扫描通过；引用式链接尚未纳入预检，已标 TODO。 |
| 审查清单扫描器 | [8 项](coverage/review_tooling.tsv) | 独立复核临时副本过滤；4 项分类检查通过，两个已跟踪工作副本移出审查范围，正式脚本仍保留。 |
| `zr.math` 数值 provider | [262 项](coverage/zr_vm_lib_math.tsv) | 42 个文件经独立复核，GCC 共享库构建、Clang 20 个 C 文件语法检查与动态插件 smoke 通过；CLI 集成测试未运行。 |
| `zr.container` 集合、视图和池 | [池 128](coverage/zr_vm_lib_container.tsv)、[视图 80](coverage/zr_vm_lib_container_views.tsv)、[模块 249](coverage/zr_vm_lib_container_module.tsv)、[构建 1](coverage/zr_vm_lib_container_build.tsv) | 合计 458 项覆盖 14 文件；GCC Debug 构建与定向测试 55 项通过；另 7 项失败在注释前基线复现，视图测试因共享文件系统配置阻塞未运行。 |

表中只列已独立复核并完成文件级状态登记的功能边界；其余文件仍以 `inventory.tsv` 中的 `pending` 为准。共享 `main` 工作树中的其它会话曾把部分尚在审查的注释收入广域 checkpoint，因此验收状态以台账、内容哈希和对应功能验证为准，不以单个 checkpoint 提交名推断完成。
