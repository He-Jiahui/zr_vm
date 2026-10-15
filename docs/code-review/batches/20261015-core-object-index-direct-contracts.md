---
related_code:
  - zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c
  - zr_vm_core/src/zr_vm_core/object/object_call_internal.h
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_core/src/zr_vm_core/exception/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - tests/core/test_object_call_known_native_fast_path.c
  - tests/CMakeLists.txt
  - tests/cmake/manifest-suite-registration.cmake
implementation_files:
  - zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c
tests:
  - tests/core/test_object_call_known_native_fast_path.c
  - tests/CMakeLists.txt
  - tests/cmake/manifest-suite-registration.cmake
plan_sources:
  - docs/code-review/comment-standard.md
  - E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/core-object-index-direct-source-leases.json
doc_type: code-review-batch
---

# Core 对象索引直派遣契约

## 范围与审查单元

本批只覆盖 `zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c`，不修改内部声明头、生产调用方、测试或共享台账。源码包含 7 个函数、一个文件职责单元和一组匿名枚举组合位，共 9 个唯一单元。所有函数均在文件末尾集中记录了文档级 Doxygen；文件头原有三行说明改为中文。源码原有 252 行程序 token 的行位置保持不变，未改签名或执行逻辑。原始源码 SHA-256 为 `1c8545cf49ec6e3dbe473a47581ab00e0edc223cdd1e4c46ce6c4ff3f5f78702`，producer 注释交接版 SHA-256 为 `7210360906342f6db0f201704972b31a7271231406fff82283e7fb5fce0be9bf`。

调用链从已解析的 `SZrObjectKnownNativeDirectDispatch` 读取 descriptor 和回调。入口先检查 state、callback、receiver/参数、debug hook、原始参数数目、接收者布局、`reserved0` 必需位及 VM 栈地址；然后依据 descriptor 的 safepoint mode 调用 `ZrCore_GcDomain_NativeEnter`。专用 GET/SET fast callback 优先；没有专用回调时构造同步借用的 `ZrLibCallContext`，两参数不连续时改用局部地址数组。正常 callback 返回后配对 `NativeLeave`。若调用位于 VM TryRun 边界内，非局部异常由该边界恢复 VM/GC scope；本文件不捕获异常，也不经过普通 callback false 的归一化分支。

descriptor 中函数、方法、元方法字段的提取顺序为函数优先、方法其次、元方法最后。GET 组合位要求结果必写；callback 返回 false 且 VM thread status 仍为 FINE 时，将结果重置为 null 并把该索引未命中作为成功交付。无结果 SET 组合位允许省略结果；callback 返回 false 且状态仍为 FINE 时同样归一化为成功。参数与 descriptor/context 都是同步借用，不得跨 callback 保存。

生产对象层在 `object.c:1172` 与 `:1215` 直接执行缓存 GET/SET callback，索引操作分别通过 `object.c:3677`、`:3710`、`:3911`、`:3944` 到达这些包装函数；这些不是本文件两个导出 helper 的调用点。对两个导出 helper 的仓内检索只找到一个直接调用：GET helper 在专用测试 `tests/core/test_object_call_known_native_fast_path.c:6371`；SET helper 未找到直接调用。历史验收材料 `tests/acceptance/2026-04-16-w2-member-call-and-member-get-hotpath.md:3748` 曾列出两个 helper，但当前对象层直接执行 callback 的实现不支持把该记录当作现行 caller 或运行证据。

## 候选疑点

`object_index_contract_value_resides_on_vm_stack` 以关系比较验证输入地址。当前已检索到的直接测试传入 VM 栈地址；若未来内部调用方传入不同对象的任意指针，需核查关系比较的 C 可移植性及拒绝策略。该边界尚未证明由当前合法调用触发，登记为窄 `TODO`，不升级为缺陷。

GET helper 只校验输出槽 `result` 非空，不检查它与 receiver/key 的别名、VM 栈归属，也不建立临时根。唯一直接测试使用独立 C 局部结果槽并只写整数 72。既有 GET TODO 因而同时待核别名策略，以及 callback 写入 GC 引用后由谁在 callback 余下操作和 NativeLeave 条件 Poll 前后保持结果可达；不把整数用例当作引用 rooting 证明，不声称已出现错误结果。

## 测试、构建与覆盖边界

目标 `zr_vm_object_call_known_native_fast_path_test` 在 `tests/CMakeLists.txt:441` 声明，通过该文件 `:67` 的 `zr_vm_add_unity_test_target` 使用 `add_executable` 创建；Core 链接见 `:609`，按配置选择 library variant 的链接见 `:612` 或 `:615`。helper 不会自行注册 CTest；当前 `core_runtime` suite 的显式列表在 `tests/cmake/manifest-suite-registration.cmake:4`，未包含此目标。producer 未重新运行构建或测试；Root 修改后实际结果见末段。

修改前 Root 的原生验证结果记录在 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/core-gc-scopes-object-binding-preedit-native-r1-results.json`（SHA-256 `1b1ead5c816bc8970fa3e2dc31c95801692ead713c06299098436963f8f8f047`），build exit 0；known-native executable 61/61、GC 10/10、multi-mutator 11/11。61 个 Unity 用例中，`test_readonly_direct_fast_callback_uses_descriptor_blocking_mode` 的 `tests/core/test_object_call_known_native_fast_path.c:6371` 直接调用 GET helper，并检查专用 fast callback、blocking-detached 模式、结果 72 及 mutator 计数恢复。它没有覆盖 GET generic-context 分支、别名、拒绝门控或 SET helper；61/61 不能解读为两个导出 helper 各有 61 个专属用例。注释后是否仍保持词法/构建结果由 Root 后续验证，本批不提前声称。

## 结果

9 个单元中 7 个为 `commented`，2 个为 `TODO`，没有通过推测登记的 BUG。未新增测试；修改前与修改后实际证据分列记录，见末段。

Root细化三条既有EOF说明：栈地址关系比较仅在满足同数组比较前提时提供范围结果，不承诺任意非栈地址能安全拒绝；该输入策略仍属原TODO。SET helper自身没有给callback额外建立互斥区，NativeEnter/Leave内部域协调不能据此推断state可跨线程并发执行。源码程序行与9键分类均未改；上方72103609为producer交接身份，当前 Root 源码与严格证明如下；修改后原生验证见末段。


## Root 当前身份与严格证明

R2 源交接 SHA-256 `9d12e7c8c9fd7a6e4290fc3a6de80278bd5fa3f4c44f89a68036a2817e11bad2`。非局部恢复的实际调用链是 exception_try_run.c:19 的 local helper，经 :26 调用 RestoreScopes，gc_domain_scopes.c:60 恢复 nativeDepth；这是与正常 callback 返回 NativeLeave 分立的 TryRun 异常路径。

严格证明 `core-object-index-direct-comment-equivalence-r2.json`（SHA-256 `5538348cb04ae7ff546acbaca619c83c8237d96ab1ed5c71915c571c7a02ba93`）通过：1353 个非注释 token、StartOfLine/LeadingSpace 与原始物理程序行位置一致，两个 Clang lexer 输出均完整消费并已等待退出。R2 时 9 键仍为 7 commented / 2 TODO；原生 after 构建/现有测试结果见末段。


## Root 输出生命周期与所选 native 模式收口

descriptor 选择 BLOCKING_DETACHED 后，gc_domain_mutator.c:685–687 发布 detached 状态，暂停 blocker 扫描 :181 跳过该 mutator；:170 明确要求 detached native 不持有可移动引用。回调及 descriptor 必须遵守既有模式协议，helper 只据 flags 选模式，不验证 state/栈地址所含引用的访问协议。NativeLeave 在 :717–725 可因 pauseRequested 在恢复 running 后 Poll；因此同步借用不意味着局部引用输出已被自动 root。此处只记录前提与原 TODO 的核查范围，没有断言普通对象一定移动或当前整数调用已触发缺陷。

当前最终源 SHA-256 `852597d0af2323a6032d1634ad9f1ad606124ae62168d3bb4a280f5a90cfecb7`；严格证明 `core-object-index-direct-comment-equivalence-r3.json` SHA-256 `b0add45b33f36953a3ebc21305a3b42a79ba8a67a5d360def6c48b9da1df3960` 通过，仍为1353 token/2个完全等待退出的lexer，flags与物理程序行一致。完整 caller 审计 `core-object-index-direct-caller-audit-r2.json` SHA-256 `c6af0012616d2c517670da4f8658c0008f1651d18509a93bec0dc5c4be997c27`，R3 身份/不变程序桥接 `core-object-index-direct-caller-bridge-r3.json` SHA-256 `2a946970ff480c423cdca400b5ac9d398ac0ab5b7f005b90f8b797a3d65051d3`；两处漏列的栈范围门控消费 :162/:212 和组合位第二次消费 :160/:209 已补入原键，未增加单元。9 键仍为7 commented/2 TODO；after native 结果见末段。


## Root 修改后实际验证

在实际 checkout 复用 clang-cl Debug 构建，未运行 configure、未复制源码或新增测试。统一构建 exit 0（100.640 秒）；GC scopes 默认进程 10/10、multimutator 11/11、known-native 61/61，三个进程均 exit 0；Fs 进程仍 exit 1、7/8，失败仍是 stream_modes 的既有 takeReader/KNOWN_NATIVE_MEMBER_CALL_RECV_U8 路径。与各自修改前实际运行逐条比较，所有 Unity case 行及退出码一致。Fs 的普通 Copy/Move 新目标用例通过，不增加对同路径、祖先/后代、覆盖失败、rename 回退、输出别名/GC rooting 或未执行分支的运行信用。

实际报告位于 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463：core-gc-object-host-final-native-r1-results.json SHA-256 679f0f5cfd4173c330b0d700f6f64d93674cd522b46bc454428b94a44a0d8655；comparison.json（同前缀）SHA-256 3ca44f91f23d306973d8d34f26307963ac96ec75e4d5956f2f10c664279a1e8e；summary.json（同前缀）SHA-256 c9c3384d2449ac8fd357d7929526dfaefb3600ce8c41e6da7c7b65e838120a6c。3216 个保守输入及 45 个冻结文件在构建/测试前后保持哈希一致，全部子进程均已等待退出。历史实际输入与当前源码之间的六个 raw 差异由四份完整严格证明桥接，共 18298 非注释 token / 12 个已等待 lexer，flags 与原程序物理坐标相同；这是原程序等价与现有执行结果证据，不称为原始源码字节完全相同的隔离实验。

本轮释放 0 B。复用构建、缓存、必要日志和 40 个 Fs 运行夹具文件（130 B，本次新增4个/13 B）保留并登记；先前自动审批已在创建清理进程前以 blocked by policy 拒绝清理，因此没有再次尝试，清理状态仍未关闭。
