---
related_code:
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/src/zr_vm_core/exception/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/exception/exception.c
  - tests/core/test_exception_gc_scopes.c
  - tests/core/test_gc_domain_multimutator.c
  - tests/cmake/exception-gc-scopes-tests.cmake
  - tests/CMakeLists.txt
  - CMakeLists.txt
  - zr_vm_core/CMakeLists.txt
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
  - docs/code-review/coverage/tests_cmake_runtime_control_registration.tsv
implementation_files:
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c
  - tests/core/test_exception_gc_scopes.c
tests:
  - tests/core/test_exception_gc_scopes.c
  - tests/core/test_gc_domain_multimutator.c
  - tests/cmake/exception-gc-scopes-tests.cmake
  - tests/CMakeLists.txt
plan_sources:
  - docs/code-review/comment-standard.md
  - E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/core-gc-scopes-source-leases.json
doc_type: code-review-batch
---

# GC-domain scope 异常恢复与测试夹具契约

## 审查范围

本批覆盖 gc_domain_scopes.c 的文件职责、三个实现函数和一个锁发布顺序块，以及 test_exception_gc_scopes.c 的文件职责、4 个宏、2 个 typedef、3 个结构、一个全局状态指针、29 个函数符号（worker entry 在 Windows/POSIX 条件下有互斥实现）。共 45 个唯一的 file/unit/kind 键：43 项为 commented，2 项为 TODO；无 BUG 或 no-comment 单元。测试源的两个 TODO 分别记录嵌套用例没有断言内层 execution/native Enter 结果，以及 worker 锁观察缺少“已经进入等待”的阶段握手。它们描述现有断言边界，不是 GC runtime 缺陷结论。

本批只编辑上述两个 C 文件中的中文 Doxygen 注释和专用 leaf/batch 文档；不改函数签名、程序语句、CMake、共享测试或既有覆盖表。C 实现原有 77 行程序文本、测试实现原有 401 行程序文本保持在原位置；新增说明追加在各文件末尾。producer 冻结交接值为 gc_domain_scopes.c SHA-256 9930ED0F32178FD6851B02CC416EB47D9D1D0DACA3B8C14810F6039E2CF71476、test_exception_gc_scopes.c SHA-256 290B1518C74B7E062D9B589C891385FF44C0D3924709838F05991505BDFD2804。Root 随后补齐线程前提、独立复核的真实 caller 方向与互斥平台入口文档；当前身份和严格证明见下方验证段。修改后结果见末尾 Root 实际验证段。

## 生产调用链与状态边界

ZrCore_Exception_TryRun 在 zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:45 捕获入口快照，然后才安装恢复点并调用回调。发生本地异常时，zr_vm_core/src/zr_vm_core/exception/exception.c:566 转到本地恢复 helper；zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:26 先恢复被丢弃 C 栈上的 AOT root frame，再调用 RestoreScopes。快照保存 domain 指针、identity、mutatorId、execution/native/mutation 深度、nativeMode 和 nativeEnteredFromInactive 的值，不保留 mutator 表中记录指针，也不延长 domain 生命周期。GC-domain 记录布局与快照定义见 zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h:42、57。

CaptureScopes 对非空且有关联 domain 的 state 加协调锁查找已登记记录；空 state、无 domain 或未找到记录时留下清零快照。RestoreScopes 在锁内重新查找记录，并要求 domain 地址/identity、mutatorId 相符，且当前 mutationDepth 不低于入口深度。身份校验失败或深度已低于快照时它静默不操作，不重建已释放的调用者 scope。

有效恢复路径仅解开 callback 比入口多获得的递归 mutation-lock 层，随后在协调锁仍持有时恢复记录深度和 native 状态、重算 status、更新 observedEpoch 并广播。该顺序是一个需与快照校验合读的发布边界，专用 leaf 将它单列为 block。TryRun 正常返回不调用此异常恢复；正常 callback 必须自己配对 MutatorLeave、NativeLeave、MutationEnd。

nativeEnteredFromInactive 的生产写入在 zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c:679-681，离开 native 状态时在 :710-714 清除；本实现仅把它纳入快照。测试比较此字段的前后值，但不验证恢复后 collector 的调度行为。

## 夹具实际运行与断言范围

tests/CMakeLists.txt:739 包含专用片段。tests/cmake/exception-gc-scopes-tests.cmake:4-5 仅在 BUILD_STATIC_LIB、非 BUILD_SHARED_LIB 且该 target 尚不存在时创建目标并执行本片段的 add_test 注册；:6 添加本测试源，:8-10 增加私有 Core 内部头路径，:12 经 zr_vm_link_core 选链接目标，:14 链接 Threads::Threads，:16 注册名为 exception_gc_scopes 的 CTest，:18 设置 gc;exception 标签和 15 秒进程超时。顶层 CMakeLists.txt:106 先添加 zr_vm_core，:185 才添加 tests；Core 的 find_package(Threads REQUIRED) 位于 zr_vm_core/CMakeLists.txt:4；tests/CMakeLists.txt 中没有该调用，片段的现有注释 :13 因而不准确。tests/CMakeLists.txt:65-75 的 Unity helper 追加公共 harness/Unity 源，:182-188 的 zr_vm_link_core 在非 shared 分支选择 zr_vm_core_static。

片段没有给 CTest COMMAND 参数，所以默认调用 main 并执行十个 RUN_TEST；仅明确传入 --scope-depths-only 时才略过最后两个 worker 用例。Unity RUN_TEST 经过 UnityDefaultTestRun 派发 setUp/tearDown（tests/third_party/zr_unity/Unity/src/unity_internals.h:909-910；tests/third_party/zr_unity/Unity/src/unity.c:2283、2298、2303）。setUp 为每例创建 state；tearDown 先确认 scope 深度和 concurrent major 已归零，再销毁。worker 使用独立栈 state：主测试线程先 attach，线程完成并 join/关闭 Windows handle 后再 detach。若主要步骤失败，scope_fail_process 使用 _Exit fail-stop，故不会经过 Unity teardown；这样避免销毁仍持锁或仍有 active major 的 state。

十个 case 都经真实 concurrent-major fixture，范围如下：

| Case | 配置与可见断言 |
| --- | --- |
| test_throw_restores_empty_entry | 空入口 + GC-aware callback 抛 INVALID；比较 TryRun 前后记录，并断言返回状态。 |
| test_throw_preserves_outer_execution | 保留一个外层 execution scope 后抛错；验证入口深度与 callback Enter/Begin 结果。 |
| test_throw_preserves_outer_gc_aware_native | 外层 execution/native 同时存在；比较深度、mode、status 和 nativeEnteredFromInactive 字段。 |
| test_throw_preserves_outer_detached_native | detached native 入口无 outer execution；只证明选定字段回到入口值，不验证后续调度。 |
| test_throw_preserves_outer_critical_native | critical native 入口无 outer execution；只比较状态记录，不演示暂停阻塞行为。 |
| test_throw_preserves_outer_mutation | 同时保留 outer execution/native/mutation；观测快照发生在清理 caller 自有层之前。 |
| test_normal_return_preserves_scopes | callback 显式配对退出后返回 FINE；它不证明 TryRun 自动修复正常返回时漏配的 scope。 |
| test_nested_tryrun_restores_each_entry_scope | 比较内层和外层 TryRun 的入口/返回记录；当前没有断言 inner.executionEntered 或 inner.nativeEntered。 |
| test_throw_keeps_outer_lock_until_caller_releases_it | caller 保留 mutation lock，启动第二个 mutator 并在 caller 清理后等待其完成。 |
| test_throw_releases_lock_for_another_mutator | 没有 outer mutation 时，worker 最终取得并释放锁，异常后记录深度为零。 |

scope_run_case 在读取 after 记录之后才清理 caller scopes，并比较七项字段：mutatorId、executionDepth、nativeDepth、mutationDepth、nativeMode、status、nativeEnteredFromInactive。它不是结构体逐字节比较，且不比较 epoch 或其他记录成员。嵌套测试检查 outerEntered、内层两次 MutationBegin、两层 INVALID status 与两组字段快照；尚未检查 inner execution/native Enter 的 bool 返回值，因此该单元保留 TODO。

锁测试的 worker 在 MutationBegin 前就发布 started。retainOuterMutation 分支等待 started 后固定睡眠 100 ms，再读取 acquired；没有握手证明 worker 已经进入阻塞等待，所以延迟调度可能将“尚未尝试”误作“被锁阻塞”。另一分支的 outerBlockedWorker 初始化为 TRUE 且不会修改，末尾对此变量的断言是恒真；实际释放证据是有界等待内 acquired/exited、worker 自行 MutationEnd，以及 after 深度为零。两种限制只在 scope_run_lock_case 记为一项 TODO，未升级为 runtime BUG。

Root 修改前实际 baseline 已运行：构建 exit 0（24.922 秒），exception-gc-scopes 10/10、GC-domain multimutator 11/11、knownnative 61/61 均 exit 0。results SHA-256 为 1b1ead5c816bc8970fa3e2dc31c95801692ead713c06299098436963f8f8f047，summary SHA-256 为 fd2493cc4828aee44de7c9fa57785b5607de6dc07b80ca01f75964e8b6539802，位于 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463。producer 未运行修改后测试；Root 实际严格证明与 after 结果见后续验收段。

## 相关覆盖与 CMake 记录边界

tests/core/test_gc_domain_multimutator.c:542-560 的 test_native_unwind_resets_abandoned_vm_and_native_scopes 是相关但不同的既有用例：它验证异常后 runtime error、execution/native depth 为零以及可以重新进入；它不比较本批七项 scope snapshot，也不重复计为本 leaf 单元。本批只将它作为邻接行为证据。

Root 已确认 docs/code-review/coverage/tests_cmake_runtime_control_registration.tsv 是既有 untracked 文件且 owner 未确认；本批不改它或 CMake。该表第 28-34 行的 CMake 注册键保留原 owner。新批次另行记录本次只读核实的配置事实：当前 include 为 tests/CMakeLists.txt:739（不是旧锚 :8792），顶层 tests 子目录入口为 CMakeLists.txt:185（不是 :187），线程包查找在 zr_vm_core/CMakeLists.txt:4。旧表第 32 行把该查找指到 tests/CMakeLists.txt:6439；当前 tests/CMakeLists.txt 只有 751 行且没有 find_package(Threads)，测试片段 :13 的“父 tests CMakeLists 查找”注释也不实。片段通过 :14 使用 Threads::Threads；线程目标的解析/可见性须由 Root 的既有 CMake 配置证据验收，不将链接声明说成 worker 已运行。七个旧 CMake 键不属于本批 45 个 C 单元，因既有 untracked owner 未确认而保留待其 owner 处理。

## 本批验证状态

本批没有新增/修改测试断言，也没有运行 CMake configure。producer check-batch 和 Root 严格词法证明已完成；独立 reviewer 额外逐项查验 caller 方向，修正了 checker 无法识别的定义行/空行误列。修改后原生结果见末尾 Root 实际验证段；上方 baseline 仅为比较基准。

Root补齐调用线程前提：Capture/Restore在同一宿主线程的同步TryRun栈内使用，snapshot不保活domain/注册；coordination lock只保护表，不使state可以并发执行。Restore可调用原生递归锁解锁，锁必须由获得它的线程释放（gc_domain_mutator.c:293–313/:354–396）。源只扩写2条既有EOF合同，测试源未再改；上方producer源哈希是其冻结交接身份，当前Root细化后的身份以最终证明段为准。


## Root 当前身份与严格证明

当前生产源 SHA-256 `5ed3d643cd8c2df12805badbdd45c3ab5cc3559dd3bb0bb09021778acb4fc019`，测试源 SHA-256 `d4fc4c44588ab32fd866c72e625489aeae3876fc41f64357e9aa798058eff366`。测试文档分别列 Windows/POSIX 互斥签名，仍按一个逻辑 worker-entry 键计数；45 键分类仍为 43 commented / 2 TODO。

严格证明 `core-gc-scopes-comment-equivalence-r2.json`（SHA-256 `6eacb786b6a9f6eefbc1ede3810036f5e3100bf69f5f1f8fa9e8022c2954d558`）通过：两源 2825 个非注释 token、StartOfLine/LeadingSpace 与原始物理程序行位置一致，四个 Clang lexer 均完整消费输出并已等待退出。仅证明本批注释未改程序，不替代修改后的原生测试；实际 after 结果见末段。


Root 完整调用审计 core-gc-scopes-caller-audit-r2.json SHA-256 f2f6e4885c57c9fd66ca9a9990c1b2bfef6852316fc87affa7b3bf7416f6bfaf 覆盖全部45键；独立复核更正了“仓内唯一 Threads 查找”的过宽表述，只记录本目标所依赖的 Core 查找及 include 顺序。其它模块也有 Threads 查找，不在本批改动范围。


## Root 修改后实际验证

在实际 checkout 复用 clang-cl Debug 构建，未运行 configure、未复制源码或新增测试。统一构建 exit 0（100.640 秒）；GC scopes 默认进程 10/10、multimutator 11/11、known-native 61/61，三个进程均 exit 0；Fs 进程仍 exit 1、7/8，失败仍是 stream_modes 的既有 takeReader/KNOWN_NATIVE_MEMBER_CALL_RECV_U8 路径。与各自修改前实际运行逐条比较，所有 Unity case 行及退出码一致。Fs 的普通 Copy/Move 新目标用例通过，不增加对同路径、祖先/后代、覆盖失败、rename 回退、输出别名/GC rooting 或未执行分支的运行信用。

实际报告位于 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463：core-gc-object-host-final-native-r1-results.json SHA-256 679f0f5cfd4173c330b0d700f6f64d93674cd522b46bc454428b94a44a0d8655；comparison.json（同前缀）SHA-256 3ca44f91f23d306973d8d34f26307963ac96ec75e4d5956f2f10c664279a1e8e；summary.json（同前缀）SHA-256 c9c3384d2449ac8fd357d7929526dfaefb3600ce8c41e6da7c7b65e838120a6c。3216 个保守输入及 45 个冻结文件在构建/测试前后保持哈希一致，全部子进程均已等待退出。历史实际输入与当前源码之间的六个 raw 差异由四份完整严格证明桥接，共 18298 非注释 token / 12 个已等待 lexer，flags 与原程序物理坐标相同；这是原程序等价与现有执行结果证据，不称为原始源码字节完全相同的隔离实验。

本轮释放 0 B。复用构建、缓存、必要日志和 40 个 Fs 运行夹具文件（130 B，本次新增4个/13 B）保留并登记；先前自动审批已在创建清理进程前以 blocked by policy 拒绝清理，因此没有再次尝试，清理状态仍未关闭。
