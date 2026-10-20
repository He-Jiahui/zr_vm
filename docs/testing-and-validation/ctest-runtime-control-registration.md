---
related_code:
  - CMakeLists.txt
  - tests/CMakeLists.txt
  - tests/cmake/close-proxy-tests.cmake
  - tests/cmake/exception-gc-scopes-tests.cmake
  - tests/core/test_close_proxy.c
  - tests/core/test_close_meta_exception.c
  - tests/core/test_close_proxy_instruction.c
  - tests/library/test_close_proxy_aot_runtime.c
  - tests/module/test_close_proxy_legacy_patch.c
  - tests/core/test_exception_gc_scopes.c
implementation_files:
  - tests/CMakeLists.txt
  - tests/cmake/close-proxy-tests.cmake
  - tests/cmake/exception-gc-scopes-tests.cmake
plan_sources:
  - user: 2026-09-26 全仓首方实际调用链审查与中文职责注释
  - docs/code-review/comment-standard.md
tests:
  - tests/core/test_close_proxy.c
  - tests/core/test_close_meta_exception.c
  - tests/core/test_close_proxy_instruction.c
  - tests/library/test_close_proxy_aot_runtime.c
  - tests/module/test_close_proxy_legacy_patch.c
  - tests/core/test_exception_gc_scopes.c
doc_type: testing-guide
---

# Runtime Control CTest Registration

## 职责与配置入口

这两个 CMake 片段为 close proxy 和异常退出后的 GC scope 恢复建立独立的 Unity 可执行测试。顶层 `CMakeLists.txt:9` 启用 CTest；`BUILD_TESTS` 为真时，`:187` 进入 tests 子目录。父 `tests/CMakeLists.txt:8794` include close-proxy 片段，`:8792` include exception-gc-scopes 片段。它们使用父目录先定义的 Unity 和链接 helper，不提供独立配置项目。

`zr_vm_add_unity_test_target` 在 `tests/CMakeLists.txt:63` 定义，通过 `add_executable` 加入测试源和公共 runtime/path、Unity crash guard、reference harness，再应用公共头路径和 Unity 配置。`target_name` 是配置期目标名，source 参数从 `CMAKE_SOURCE_DIR` 指向仓根下的测试源；这一步建立构建图，不执行 C 测试函数。

## 六个注册的真实覆盖

| CTest 名 | 目标和真实 main | 用途与限制 |
|---|---|---|
| `close_proxy_core` | `zr_vm_close_proxy_core_test`；`tests/core/test_close_proxy.c:998` | 19 个 core API 夹具观察关闭链、GC、异常替换和 ownership 镜像清理；没有 parser 源码编译。 |
| `close_meta_exception` | `zr_vm_close_meta_exception_test`；`tests/core/test_close_meta_exception.c:169` | 四个源码脚本检查 original/replacement message 与复用 call-info 链可达性；不证明堆泄漏或主动 GC 路径。 |
| `close_proxy_instruction` | `zr_vm_close_proxy_instruction_test`；`tests/core/test_close_proxy_instruction.c:165` | 手写 ExecBC 实际进入解释器，另检查高代理槽导致的帧槽计数；source 槽较低，计数断言不能独立证明 source operand 扫描。 |
| `close_proxy_aot_runtime` | `zr_vm_close_proxy_aot_runtime_test`；`tests/library/test_close_proxy_aot_runtime.c:132` | 一个手工双表示帧夹具直接调用 AOT helper，观察两处 source 在回调前清空且只关闭一次；没有生成函数入口、强制 GC 或回调重入。 |
| `close_proxy_legacy_patch` | `zr_vm_close_proxy_legacy_patch_test`；`tests/module/test_close_proxy_legacy_patch.c:138` | opcode 序号与 patch 44 写出、43 读取、45 拒绝门禁；payload 仅 NOP，不能证明旧格式能执行新 opcode。 |
| `exception_gc_scopes` | `zr_vm_exception_gc_scopes_test`；`tests/core/test_exception_gc_scopes.c:385` | TryRun 正常/异常退出、嵌套入口的 execution/native/mutation 状态恢复；未传 `--scope-depths-only`，默认还包含两个跨线程 mutation 锁用例。 |

close 片段的实际 `add_test` 分别在 `tests/cmake/close-proxy-tests.cmake:13`、`tests/cmake/close-proxy-tests.cmake:28`、`tests/cmake/close-proxy-tests.cmake:40`、`tests/cmake/close-proxy-tests.cmake:53`、`tests/cmake/close-proxy-tests.cmake:66`；GC scope 注册在 `tests/cmake/exception-gc-scopes-tests.cmake:16`。这些 COMMAND 最终启动表中的 main，再由 `RUN_TEST` 派发具体夹具；登记 CMake target、生成 CTest 配置和运行通过是不同证据。

## 依赖和构建约束

close proxy 的 core 与 instruction 夹具链接 core；legacy patch 夹具链接 parser/core。script exception 和 AOT helper 夹具使用现有 parser/core/library 链接组合。父目录的 `zr_vm_link_core`、`zr_vm_link_parser_core`、`zr_vm_link_parser_core_plus_library` 分别在 `tests/CMakeLists.txt:180`、`:188`、`:196` 定义，按 `BUILD_SHARED_LIB` 优先选择 shared，否则选择 static。PRIVATE include 路径只供对应测试编译，公共 tests/harness/common/Unity 路径由 Unity helper 提供。

GC scope 片段只在 `BUILD_STATIC_LIB AND NOT BUILD_SHARED_LIB` 且目标尚不存在时执行整组创建、链接和注册。夹具通过 PRIVATE `zr_vm_core/src/zr_vm_core` 读取真实内部 mutator 记录，静态配置避免把这一测试接口当作 shared 公共导出契约。`NOT TARGET` 检查目标名；已有目标会跳过整组操作，不能据此推断同名 CTest 已经存在。

`Threads::Threads` 来自父 `tests/CMakeLists.txt:6439` 的 `find_package(Threads REQUIRED)`。POSIX 测试源用 pthread worker；Windows 源分支用 CreateThread。包查找与链接只是线程设施的前置条件，不证明 worker 已启动或锁恢复已经通过。

## CTest 参数和失败边界

五个 close 注册通过 `$<TARGET_FILE:...>` 得到配置对应的可执行路径；GC scope COMMAND 使用目标名，由 CMake 解析路径。GC scope 没有 depths-only 参数，main 的完整默认入口会执行线程锁检查。close 用例的标签为 `ssa;core`、`ssa;aot` 或 `ssa;artifact`，GC scope 为 `gc;exception`；标签用于筛选，不表示链接范围或运行成功。

close 注册的 `TIMEOUT 30` 与 GC scope 的 `TIMEOUT 15` 都限制整个测试进程，而不是每个 Unity 用例。Unity 断言、GC scope 的显式进程失败和超时都能使运行失败；超时本身不能定位具体 GC/锁根因。构建未完成或可执行文件缺失也不能由成功配置掩盖。

## 复用片段的待核实边界

- close proxy 仓内只发现父 tests 末尾的 include。外部若单独或重复 include，需核对 Unity/link helper 与模块 target 的创建顺序，以及同名 target/CTest 是否重复。
- GC scope 仓内只发现本片段创建目标。外部若提前创建该 target，需要核查是否另行 add_test 注册 `exception_gc_scopes`，因为本片段会跳过整组创建与注册。

这些 TODO 指向未知外部配置，当前没有完整合法触发链证明故障，不能记为 BUG。

## 审查与验证范围

本轮新增职责注释只改变两份 CMake 源码的 comment 行；参数、控制条件、目标名、原物理非注释字节和换行保持。逐单元结论见 [运行控制注册台账](../code-review/coverage/tests_cmake_runtime_control_registration.tsv)。六个 C 文件只作为真实 main/夹具的消费证据，不据此推进其整体审查信用。

本轮证据是静态调用/注册链审查、补丁可应用性和候选字节校验；没有 configure、编译、链接、CTest 或 runtime 执行信用。需要运行时验收时，由构建调用者在匹配的 shared/static 配置构建相应目标，再运行选择出的 CTest。

进一步了解清理协议，参阅 [异常作用域资源清理](../core-runtime/exception-scope-resource-cleanup.md) 与 [异常 GC scope 恢复](../core-runtime/exception-gc-scope-recovery.md)。
