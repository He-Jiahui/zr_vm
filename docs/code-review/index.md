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
| 历史 `zr_vm_lib_task` 实现 | [225 项](coverage/zr_vm_lib_task.tsv) | 10 个文件经独立复核；顶层当前不构建此目录，GCC 单文件语法检查发现旧代码引用已删除字段（原始版本同样失败），不能视为当前 CLI task provider 的测试结果。 |
| `zr.thread` 调度器与共享封装 | [309 项](coverage/zr_vm_lib_thread.tsv) | 11 个文件独立复核；GCC/Clang 语法检查通过；运行时目标因 VerifyGlobs 卡顿未执行，BUG/TODO 为静态证据。 |
| Wiki 渲染与 ZR 词法着色 | [33 项](coverage/wiki_rendering.tsv) | 8 个配置、插件和测试文件独立复核；注释之外的 Python AST 与配置保持一致，定向测试 7/7 通过。 |
| Wiki 路由清单与主题样式 | [13 项](coverage/wiki_manifest_theme.tsv) | 2 个文件独立复核；JSON 路由由校验器检查，CSS 仅改注释；Wiki 校验和主题测试 4/4 通过。 |
| `zr.system` 宿主能力 provider | [386 项](coverage/zr_vm_lib_system.tsv) | 39 个文件独立复核，非注释 token 未变；GCC 构建、Clang 语法检查通过。assembly 2/2、GC 67/67、provider convergence 9/9 通过；FS 8 项中 2 项、module system 78 项中 6 项、exceptions 8 项中 2 项失败。 |
| `zr.network` TCP/UDP provider | [197 项](coverage/zr_vm_lib_network.tsv) | 13 个文件独立复核，79 个 C 定义均登记；GCC/Clang 语法检查和完整 TCP 帧烟测通过，半帧 EOF 与零字节 UDP 的既有缺陷在烟测中复现。 |
| 根 CMake 与跨平台 CI | [30 项](coverage/root_build_ci.tsv) | 2 个入口文件独立复核，去注释后内容未变；YAML 矩阵解析与 `diff --check` 通过，记录扩展 WASM 构建目标缺失和未消费的依赖开关。 |
| `zr.ffi` 动态调用与 ABI provider | [287 项](coverage/zr_vm_lib_ffi.tsv) | 15 个文件独立复核，100 个 C 定义均登记；GCC shared 构建和 Clang 8 个 C 文件语法检查通过。四个既有测试目标仍有失败（8/30、3/30、1/2、另一个断言中止），未取得同基准对照结果。 |
| 工作区维护脚本（剩余批次） | [392 项](coverage/scripts_remaining.tsv) | 21 个文件完成调用链审查；Python AST、Shell/PowerShell 语法与注释纯度复核通过；魔术常量审计现存 199 项中 7 项失败，已保留风险记录。 |
| 维护脚本测试（剩余批次） | [31 项](coverage/tests_scripts_remaining.tsv) | 3 个文件完成审查；两个 Python 测试 AST 与 PowerShell 非注释文本不变，状态测试 4/4、PowerShell 语法通过；迁移测试 11 项中 1 项因五处 Wiki 旧语法片段失败。 |
| 公共基础类型、宏与 SSA 平台契约 | [632 项](coverage/zr_vm_common.tsv) | 33 个文件完成调用链审查；GCC/Clang 严格 C11 语法与 SSA matrix 测试通过；CMake 签名组合缺陷经隔离复现，另保留 6 个 BUG 与 17 个 TODO。 |
| CLI 回归测试与 REPL smoke | [443 项](coverage/tests_cli.tsv) | 30 个文件完成调用链审查；19 个 JS 语法检查通过，18 个 REPL smoke 为 17 通过、1 个 assignment 失败，语法迁移 golden 另有漂移；两处 BUG 已记录，未取得同基准旧版运行结果。 |
| zr.debug 调试、coverage 与 profile provider | [771 项](coverage/zr_vm_lib_debug.tsv) | 32 个文件完成调用链审查，465 个 C 定义、68 个内部声明与 35 个宏均登记；作者分组 GCC/Clang 语法检查通过，独立整批重跑受共享 WSL I/O 阻塞未完成；Wiki API 契约与 metadata 已校正。 |
| CLI 构建与程序入口 | [27 项](coverage/zr_vm_cli_entry.tsv) | 6 个文件的 11 个宏、4 个 C 定义、2 个接口及分发块完成审查；差异仅新增注释，现有 MSVC Debug CLI 的版本和空优化记录查询均正常退出，未重新构建。 |
| 项目清单、导入与依赖锁 | [275 项](coverage/zr_vm_library_project.tsv) | 16 个文件完成调用链审查，193 个 C 定义及公开声明均登记；GCC/Clang 语法检查通过，定向测试目标构建停在 Ninja glob 复查，运行测试未执行。 |
| 可选 JIT 后端与状态映射 | [122 项](coverage/zr_vm_jit.tsv) | 4 个文件完成调用链审查；GCC/Clang 严格语法和直接链接的可选 JIT 测试通过，JIT ON 配置通过；完整目标构建停在 glob 复查，CTest 未执行。 |
| coverage 与 profile 回归用例 | [15 项](coverage/tests_profile.tsv) | 2 个测试文件完成调用链审查；Clang C11 语法和非注释 token 核对通过，GCC 检查受共享 WSL I/O 阻塞而中断，测试目标未构建；固定行数组容量疑问已标 TODO。 |
| C/Rust binding 与安全封装 | [826 项](coverage/zr_vm_rust_binding.tsv) | 28 文件独立复核，23 个改动文件仅增注释；Rust fmt 与 sys crate 离线检查通过；workspace 检查仍有两处已标 BUG 的 E0283。C 独立目标构建未完成；Wiki 的 session、buffer、错误对象及整数读取边界已校正。 |
| 测试 manifest 二进制往返与 IoSource 生命周期 | [17 项](coverage/tests_artifact.tsv) | 2 文件独立复核、纯注释及 Clang C11 语法检查通过；已标读取失败原生泄漏和断言跳过分配器恢复两处 BUG。Unity 目标构建停在 CMake 重新配置，运行测试未执行。 |
| `zr.testing` 断言、并行 runner 与角色绑定回归 | [53 项](coverage/tests_testing.tsv) | 3 文件独立复核，源码只增注释、Clang C11 语法检查通过；并行 probe 共享计数器数据竞争已标 BUG，失败清理与快照覆盖疑问已标 TODO。四个 Unity 目标构建停在 CMake 重新配置，运行测试未执行。 |
| AOT runtime、typed call 与生命周期边界 | [624 项](coverage/zr_vm_library_aot.tsv) | 11 文件独立复核，非注释词法流不变，Clang C11 语法和 MSVC Debug 共享库构建通过；生成器未引用的 shim 风险记为 TODO，已证实的调用/清理问题记为 BUG。WSL GCC 检查因 I/O 阻塞中断，未计通过。 |
| library 基础状态、批处理、文件与任务运行时 | [312 项](coverage/zr_vm_library.tsv) | 14 文件独立复核，9 个改动源码文件的非注释内容不变；补准 Map 原位布局、global 初始化、ReadAll 释放后访问及整数比较精度问题的契约与 BUG。作者此前 GCC/Clang 语法自检通过；独立编译受本机头文件和 WSL I/O 限制，未计通过。 |
| 异常处理编译与运行时回归 | [31 项](coverage/tests_exceptions.tsv) | 2 文件独立复核，仅改注释；Clang C11 语法检查通过。Unity 断言中断后的 VM 状态清理缺口已标 BUG，两个覆盖疑问及孤立的子目录 CMake 配方已标 TODO；测试目标未完成构建或运行。 |
| 迁移、reference、decorator 与项目调试小边界 | [24 项](coverage/tests_small_contracts.tsv) | 5 文件独立复核，3 个 C 文件仅加注释且 Clang C11 语法通过；迁移缓冲区、reference 文本及 decorator 测试失败后的资源泄漏和过期 GDB 断点已标 BUG。生成 suite 清单只登记来源，未改动；运行测试未执行。 |

表中只列已独立复核并完成文件级状态登记的功能边界；其余文件仍以 `inventory.tsv` 中的 `pending` 为准。共享 `main` 工作树中的其它会话曾把部分尚在审查的注释收入广域 checkpoint，因此验收状态以台账、内容哈希和对应功能验证为准，不以单个 checkpoint 提交名推断完成。
