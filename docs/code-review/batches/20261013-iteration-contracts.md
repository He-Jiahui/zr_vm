# zr.iteration provider 与测试契约审查 — 2026-10-13

## 审查范围与源码身份

本批次审查四个 Iteration 模块文件，并只读逐单元审阅六个 iterator 测试源。四个模块文件只新增中文契约注释，公开签名、宏值、descriptor 初始化器、注册调用、控制流和 CMake 表达式均未改动。本代理没有编辑测试源、CMake 迁移文件或 inventory。Root 另行更新了 docs/library-and-builtins/zr-iteration-protocol.md，该文档不在本代理的写入清单内。

注释前 checkout 为 901f9898d2326d7b2694a89a77c14255003632d9。Root 在 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/iteration-before-pins.json 固定了源码和测试 SHA。

| 模块文件 | 注释前 SHA-256 | 审查单元 | 注释后 SHA-256 |
| --- | --- | --- | --- |
| `zr_vm_lib_iteration/CMakeLists.txt` | `27FF3848E4B1D614CD60F7FB06894EDD7F15958806F429CA4B19AF7C4CB0701F` | Provider 的构建与安装意图；保留第 6 行 | `89EA658CD12145C8DF957DC5AF0C41C842CF40CBEADE4F80935BC986A52DDAB4` |
| `zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h` | `646DC8D8778BAAA32C154EE9F85945D7D548ACC61D0B2EAE7F59C019A1436FD1` | 导出宏与三个公开声明 | `E071770082EAF37B714E06BDA43D581C790228215039C57B2B3366DC42F7D4D0` |
| `zr_vm_lib_iteration/src/zr_vm_lib_iteration/module.c` | `EE1B7BE05522B27038368BE16C693603952041CE12305A4B5E7BA073B73DB7EB` | 运行时桥接、getter、注册与共享库入口 | `82059724C831F09099CEBA5537A3F40DD990770E2FDDEDDA9F55EE728A14E90B` |
| `zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c` | `C1B63AAD43895C7A10A9AFEEA31383086F55EB13C606E018AA98ACD28078EB77` | 全部描述符表及私有 getter | `011F3163E471C875887EEF388D14FC7C4D52C3C0A623AA7F83ED73950D9BE37E` |

模块 coverage ledger 从 iteration-before-pins.json 中的 SHA E3D5550BF3B3C6C686AAB9D3919844C55CEC6F567EFFE2B2AA7549DA8D7E926E 更新为九列当前路径证据；最终 SHA-256 为 403609DAF19AB52E0D192CBDCF4EFD0255848E0BFBF193E0A662D8F6A280B7E0。执行 scripts/code_review_inventory.py check-batch docs/code-review/coverage/zr_vm_lib_iteration.tsv 通过。

## 模块契约与已证疑点

module.h 现按 Summary、Parameters、Returns、Constraints and lifecycle、References 记录公开声明（无参数项合理省略 Parameters）。描述符 getter 只返回模块静态只读地址，不执行注册。ZrVmLibIteration_Register 要求非空 global 且 mainThreadState 已建立；Attach 成功后，即使描述符校验或注册失败也不回滚附着状态。初始化流程应串行调用，因为接口未承诺并发注册安全；同名项仅在注册表兼容性校验接受时才可复用。

共享库专用 ZrVm_GetNativeModule_v1 返回相同静态描述符，不执行注册。插件 loader 按符号名查找并调用该函数，再校验 ABI、module name 与兼容性。注册记录借用描述符，插件句柄必须覆盖所有描述符及字符串的使用期。基线配置只构建静态 provider；共享插件路径以真实 loader 调用链作只读审查，没有由该配置动态加载。

descriptor.c 将协议身份与可执行实现分开：它静态持有 Iterable、Enumerator、Iterator、AsyncIterator 元数据和共享泛型 T。Iterator<T> 是不透明、不可构造且实现 Enumerator<T> 的编译器载体；Iterator 与 Enumerator 共用能力位，AsyncIterator 使用独立位。该文件的方法 callback 都为 NULL，metadata builder 会跳过；实际同步生产与推进由 zr.container 以相同角色提供。异步表项只描述协议，当前同步绑定器只查 ITERATOR/ITERABLE。

目前仅保留一项有证据支撑的 TODO，位于 `g_enumerator_fields`：`ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT` 以 `isReadonly=false` 发布 `current`，因此 native metadata 会将其映射为可写成员（`native_binding_metadata.c:1240-1244`）。该映射本身不能证明脚本运行时允许赋值。源码注释给出了核查入口：在真实 Enumerator 成功执行 `moveNext` 后运行 `current = value`，并将赋值结果与反射得到的原型 metadata 比较。现有协议测试只查询方法角色和返回类型，未查询 current 字段，也没有实际赋值（`tests/iterator/test_enumerator_protocol.c:117-127`）。

检查完整调用路径后，移除了 `RegisterModule` 前显式调用 `Attach` 的旧 TODO：对已附着 registry 再调用 `Attach` 会返回 true（`native_binding.c:94-96`），且 `native_registry_register_module_record` 自身也会调用 `Attach`（`native_binding_support.c:739-741`）。显式调用及其失败返回保持原样。现有证据只说明没有单独证实重复附着会造成额外生命周期影响，并不表示所有重复模块注册都会成功。

## 构建与直接测试入口

六个可执行程序由当前工作树中尚未提交的 CMake 迁移注册，入口如下：

| 可执行目标 | 当前目标定义与注册路径 |
| --- | --- |
| `zr_vm_enumerator_protocol_test` | `tests/cmake/container-task-ffi-targets.cmake:112-128` |
| `zr_vm_iterator_runtime_test` | `tests/cmake/parser-foundation-targets.cmake:146-153` |
| `zr_vm_iterator_gc_drop_test` | `tests/cmake/parser-foundation-targets.cmake:155-162` |
| `zr_vm_yield_syntax_test` | `tests/cmake/parser-foundation-targets.cmake:164-172` |
| `zr_vm_iterator_semantic_ir_test` | `tests/cmake/parser-foundation-targets.cmake:174-182` |
| `zr_vm_yield_semantic_test` | `tests/cmake/parser-foundation-targets.cmake:184-200` |

zr_vm_add_unity_test_target 定义于 tests/CMakeLists.txt:65-75，负责创建 Unity 测试可执行目标并应用通用设置。六个目标没有直接 add_test 条目；Root 从 checkout 根目录直接串行运行现有可执行文件。test_enumerator_protocol 和 test_yield_semantic 链接 Iteration 模块；runtime/GC/syntax/semantic-IR 目标测试 core/parser/core，不链接此 provider。

本次基线使用当前工作树的 CMake 迁移，而非仅用 HEAD 的干净构建图。迁移修改了 tests/CMakeLists.txt 及 enumerator/runtime/GC 测试源，并新增未跟踪的 parser/container target 片段；本代理没有编辑这些文件。Root 的 products-configure-before-results.json 记录选项为 static ON/shared OFF、Debug、Thread ON、Network/Debug/LSP/Rust/CLI/JIT OFF、Tests ON，工具为 MSVC 19.44.35228、SDK 10.0.26100.0、Ninja。configure 和八目标基线 build 均 exit 0，构建目录为 E:/cargo-targets/zr_vm/build/first-party-review-01a13463/msvc-products-debug。

基线 configure/build/test 证据保存在 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/：products-configure-before-results.json、products-build-before-results.json、products-tests-before-results.json 以及六组 *-before.stdout.log/*-before.stderr.log。六个测试源 SHA 与 iteration-before-pins.json 固定值一致。

## 六个迭代测试源的只读单元台账

本次先检查了历史台账 `docs/code-review/coverage/tests_iterator_task.tsv`，没有把它视为当前覆盖证明。其 inventory 检查因多处证据锚点指向已迁移或不存在的 core/parser/CMake 路径而以 exit 1 结束，因此旧行不能证明当前模块覆盖。下表是对这六个测试源的逐单元只读审阅；每个测试用例都记录定义位置、`main`/`RUN_TEST` 入口以及注释前实际执行结果。本代理没有改动测试源：其中 `test_enumerator_protocol.c`、`test_iterator_gc_drop.c`、`test_iterator_runtime.c` 有其他工作树改动，其余三个测试源干净。采集的 SHA-256 与构建前 pin 一致。

### `tests/iterator/test_enumerator_protocol.c`

| 单元 | 定义与调用方 | 契约、运行结果与结论 |
| --- | --- | --- |
| `find_type` | `:17`; 描述符用例 `:102-105`, `:140-143`, `:169-170` | 按名称查找模块类型；空参数或未命中时返回 NULL，结果是模块静态描述符的借用指针。三个协议/容器/内建描述符用例通过，现有注释覆盖借用边界。**已注释** |
| `find_method` | `:35`; 协议/容器用例 `:117-119`, `:155` | 按方法名查找描述符；空参数或未命中时返回 NULL，返回项为借用元数据且不会调用 回调。协议与容器用例通过，已有注释说明。**已注释** |
| `type_implements` | `:53`; 容器用例 `:149-153` | 扫描 implements 名称表；空类型或未命中返回 false。容器协议用例通过，已有注释说明协议归属。**已注释** |
| `function_contains_opcode` | `:71`; lowering 用例 `:292-297` | 检查已编译函数是否含指定 opcode，用于区分静态和动态 iterator lowering；对应 显式类型的 foreach 用例通过。已有用途注释。**已注释** |
| `test_iteration_descriptor_owns_public_enumerator_contract` | 定义 `:88`; `main` 调用 `:309` | 验证模块名、四个协议类型、能力位、不透明 Iterator 的构造限制、成员返回类型和角色。5 个用例中本例通过。**已注释** |
| `test_container_descriptor_uses_iteration_contract_owner` | `:131`; `main` 调用 `:310` | 验证 Array/Map/Set/LinkedList 使用 iteration 所有的 Iterable 类型、getIterator 的 ITERABLE_INIT 角色，并排除旧 builtin 协议名。本例通过。**已注释** |
| `test_builtin_descriptor_does_not_own_iteration_protocols` | `:162`; `main` 调用 `:311` | 创建并销毁测试状态，确认 zr.builtin 不再导出 IEnumerable/IEnumerator。本例通过。**已注释** |
| `test_enumerator_binding_uses_protocol_facts_only` | `:176`; `main` 调用 `:312` | 注册并导入 zr.iteration；验证 Enumerator/Iterable/Iterator 与 readonly ref-like iterator 的元素类型推导，拒绝原生数组和无能力对象，再释放类型、compiler、AST、状态。本例通过。**已注释** |
| `test_typed_foreach_keeps_static_iterator_lowering_through_break_cleanup` | `:274`; `main` 调用 `:313` | 编译带 break 的 显式类型的 foreach；确认生成静态 ITER_INIT/current/move-next 指令且没有动态迭代指令，随后释放 AST、function 和 状态。本例通过。**已注释** |
| `setUp` | `:303`; Unity 可执行目标 `container-task-ffi-targets.cmake:112-114` | 空 Unity 钩子；各用例自行创建状态，无额外生命周期注释必要。**无需独立注释** |
| `tearDown` | `:305`; Unity 可执行目标 `container-task-ffi-targets.cmake:112-114` | 空 Unity 钩子；各用例自行释放状态，无额外生命周期注释必要。**无需独立注释** |
| `main` | `:307`; 可执行目标 `container-task-ffi-targets.cmake:112-128` | 按顺序注册并运行五个 Unity 用例；基线 5/5 通过。入口列表即本测试程序的执行契约。**无需独立注释** |

### `tests/iterator/test_iterator_runtime.c`

| 单元 | 定义与调用方 | 契约、运行结果与结论 |
| --- | --- | --- |
| `SZrIteratorRuntimeTestProducer` | 类型 `:12`; 回调 `iterator_runtime_test_produce :47`; 用例 `:184`, `:295-303` | 保存最多三个整数及 count/next 游标；producer 回调 每次发布一项，耗尽后完成。现有注释说明游标归属。**已注释** |
| `SZrIteratorRuntimeTestCleanup` | 类型 `:19`; cleanup 回调 `:84`; completion/fault/close 用例 | 记录终结清理次数，用于断言 cleanup 幂等；现有注释说明观测用途。**已注释** |
| `SZrIteratorRuntimeTestContext` | 类型 `:24`; forwarding 回调 `:97`, `:108`; pool/close 用例 | 把 producer 与 cleanup 状态放在同一 userData 下，供两个回调按约定解释；现有注释已说明。**已注释** |
| `SZrIteratorRuntimeTestReentrantProducer` | 类型 `:30`; 回调 `:119`; 用例 `:321-331` | 记录是否发起/接受嵌套 MoveNext 和外层发布数；用于检查同一 frame 重入拒绝。现有注释已说明。**已注释** |
| `SZrIteratorRuntimeTestObjectProducer` | 类型 `:37`; 回调 `:139`; GC 用例 `:335`, `:374` | 保存作为输入的裸对象指针；跨压缩 GC 保活当前值的是 frame root，而非这些字段。现有注释已限定其作用。**已注释** |
| `g_state` | 状态 `:44`; `setUp :161`, `tearDown :166`; 回调与测试 | 每个用例各自创建的 runtime 状态须覆盖该用例回调和 frame 的使用期；现有 fixture 注释说明这一生命周期。**已注释** |
| `iterator_runtime_test_produce` | `:47`; passed to `IteratorFrame_Init :184`, `:298`; calls `MoveNext` in 用例 | 每次推进一个 producer 值并发布初始化的整数 current，耗尽后完成；userData 必须指向 producer 状态。用例会检查发布是否被拒绝。已有回调契约注释。**已注释** |
| `iterator_runtime_test_complete` | `:66`; installed by 用例 `:206-211`, `:263-268`, `:421-427`, `:462-467` | 调用 Complete 驱动正常终态转换；多个终态用例安装此回调，现有注释说明用途。**已注释** |
| `iterator_runtime_test_fault` | `:75`; installed by fault 用例 `:225-230` | 调用 Fault 驱动故障终态；fault 用例安装此回调，现有注释说明用途。**已注释** |
| `iterator_runtime_test_cleanup` | `:84`; 由终态用例或 context 包装回调安装 | 从 userData 读取清理状态并递增计数，不改 frame 状态；现有注释说明观测契约。**已注释** |
| `iterator_runtime_test_context_produce` | `:97`; installed by close/pool 用例 `:298-303`, `:422-427` | 把 userData 解释为 context，再转发其 producer 子对象；现有注释说明转发关系。**已注释** |
| `iterator_runtime_test_context_cleanup` | `:108`; installed by same context 用例 `:298-303`, `:437-442` | 把 userData 解释为 context，再转发其 cleanup 计数器；现有注释说明转发关系。**已注释** |
| `iterator_runtime_test_reentrant_produce` | `:119`; installed by reentrancy 用例 `:321-326` | 只尝试一次嵌套 MoveNext；若外层仍 READY，则发布值 9 并记录恰好一次外层发布。现有注释说明重入检查。**已注释** |
| `iterator_runtime_test_object_produce` | `:139`; installed by compact-GC/root replacement 用例 `:346-351`, `:385-390` | 从 producer 状态发布对象 current 或完成；调用方依赖 frame 管理 current root，不依赖裸指针长期有效。现有注释说明边界。**已注释** |
| `setUp` | `:161`; Unity 测试目标 `parser-foundation-targets.cmake:146-148` | 每个用例创建新的 runtime 状态 并断言成功；标准 Unity 初始化钩子语义明确。**无需独立注释** |
| `tearDown` | `:166`; Unity 测试目标 `parser-foundation-targets.cmake:146-148` | 销毁非空 状态 并清空全局指针；标准 Unity 清理钩子语义明确。**无需独立注释** |
| `test_iterator_frame_yields_multiple_values_then_completes` | `:174`; `main` 调用 `:479` | 验证按序得到 1、2、3，耗尽后返回 false、current 无效且状态为 COMPLETED。本例通过。**已注释** |
| `test_iterator_frame_completion_runs_cleanup_once` | `:201`; `main` 调用 `:480` | Complete 后重复 MoveNext 返回 false，cleanup 不会重复调用。本例通过。**已注释** |
| `test_iterator_frame_fault_runs_cleanup_once` | `:220`; `main` 调用 `:481` | Fault 将状态置为 FAULTED，重复 MoveNext 不会再次清理。本例通过。**已注释** |
| `test_iterator_frame_missing_producer_faults_and_cleans_up` | `:240`; `main` 调用 `:482` | producer 为空时 MoveNext 进入故障并执行一次 cleanup。本例通过。**已注释** |
| `test_iterator_frame_preserves_the_first_terminal_state` | `:258`; `main` 调用 `:483` | Fault 后的 Complete/Close 保持 FAULTED；另一个先 Complete 的 frame 在后续 Fault/Close 后保持 COMPLETED。本例通过。**已注释** |
| `test_iterator_frame_early_close_runs_cleanup_once` | `:290`; `main` 调用 `:484` | 产出一项后 Close 会使 current 失效；重复 Close 仅清理一次。本例通过。**已注释** |
| `test_iterator_frame_rejects_same_frame_reentrancy` | `:316`; `main` 调用 `:485` | 外层 producer 重入同一 frame 时，嵌套 MoveNext 返回 false，外层只发布一次。本例通过。**已注释** |
| `test_iterator_frame_roots_current_object_across_compact_gc` | `:335`; `main` 调用 `:486` | 发布对象增加一个 frame root；完整压缩 GC 后 root 解析仍指向 Current；Close 移除 root。本例通过。**已注释** |
| `test_iterator_frame_replaces_the_previous_object_root` | `:374`; `main` 调用 `:487` | 连续两次产出只保留一个 current root；Close 恢复此前 root 数。本例通过。**已注释** |
| `test_iterator_frame_pool_reuses_storage_without_state_leaks` | `:408`; `main` 调用 `:488` | 终结并释放后再次 acquire 复用同一 frame 存储，同时重置 READY/current/cleanup 状态并更新分配复用计数；最后释放 pool。本例通过。**已注释** |
| `test_iterator_frame_pool_rejects_a_nonterminal_lease` | `:457`; `main` 调用 `:489` | pool 拒绝释放仍为 READY 的 frame；Close 后允许释放并清空 pool。本例通过。**已注释** |
| `main` | `:477`; 可执行目标 `parser-foundation-targets.cmake:146-153` | 按顺序运行 11 个 Unity 用例；基线 11/11 通过。**无需独立注释** |

### `tests/iterator/test_iterator_gc_drop.c`

| 单元 | 定义与调用方 | 契约、运行结果与结论 |
| --- | --- | --- |
| `EZrIteratorGcDropTerminal` | 枚举 `:14`; selected by `iterator_gc_drop_init_context :111` | 选择对象发布后的 Complete 或 Fault 分支；两条终态共用 cleanup 断言。现有注释说明分支用途。**已注释** |
| `SZrIteratorGcDropContext` | 类型 `:20`; produce/cleanup 回调 `:72`, `:97`; 四个用例 | 分离保存 yield 对象、独立 unique cleanup owner、终态选择、root/cleanup 观察值和 emitted 标记；frame root 与 owner drop 分开统计。现有注释说明所有权划分。**已注释** |
| `g_state` | 状态 `:30`; setup/teardown `:121`, `:127`; 回调/用例 | 每个用例的 runtime 状态 承载 GC、对象、closure 和所有权操作；fixture 注释覆盖生命周期。**已注释** |
| `g_resourceDropCount` | 状态 `:31`; destructor `:34-38`; assertions `:163`, `:188`, `:216`, `:245` | 统计 destructor 调用次数以发现重复 owner drop；每个用例重置。fixture 注释覆盖其目的。**已注释** |
| `iterator_gc_drop_resource_destructor` | `:34`; assigned to native closure `:53`; invoked through ownership release `:107` | 递增全局计数并返回 0；仅在测试 状态 有效期间观察 回调 状态。现有注释说明间接调用。**已注释** |
| `iterator_gc_drop_init_resource_unique` | `:41`; called by context initializer `:118` | 构造 resource proto类型、永久 native destructor closure、object 和 unique owner；断言防止必要分配/所有权初始化失败。现有注释说明测试搭建。**已注释** |
| `iterator_gc_drop_produce` | `:72`; passed to frame init in each 用例 | 首次 MoveNext 发布 yield 对象；下一次按 context 选择 Complete/Fault。context 经 userData 传入。现有注释说明两步契约。**已注释** |
| `iterator_gc_drop_cleanup` | `:97`; passed to frame init in each 用例 | cleanup 中先观察 root 数、递增回调数，再释放独立 owner；frame 必须先移除 root。现有注释说明顺序。**已注释** |
| `iterator_gc_drop_init_context` | `:111`; 用例 call at `:142`, `:172`, `:200`, `:228` | 清零 context，创建 yield 对象，记录终态并初始化独立 unique owner；栈上 context 由调用方保持到 frame 生命周期结束。现有注释说明隔离关系。**已注释** |
| `setUp` | `:121`; Unity 测试目标 `parser-foundation-targets.cmake:155-157` | 创建每用例 runtime 状态 并重置 destructor 计数；fixture 行为明确。**无需独立注释** |
| `tearDown` | `:127`; Unity 测试目标 `parser-foundation-targets.cmake:155-157` | 销毁非空 状态 并清空指针；标准 Unity 清理职责明确。**无需独立注释** |
| `test_iterator_gc_root_resolves_current_value_after_compact_collection` | `:135`; `main` 调用 `:253` | 断言 yield 后多一个 root、压缩 GC 后对象身份仍可解析、Current 一致，并在 Close 后只清理/销毁一次。本例通过。**已注释** |
| `test_iterator_completion_releases_root_before_direct_owner_cleanup` | `:167`; `main` 调用 `:254` | 正常耗尽时 cleanup 观察到基线 root 数后再释放 unique owner；root 只移除一次。本例通过。**已注释** |
| `test_iterator_fault_releases_root_before_direct_owner_cleanup` | `:195`; `main` 调用 `:255` | 故障路径同样先移除 root 再释放 owner，destructor 只调用一次。本例通过。**已注释** |
| `test_iterator_close_releases_root_before_direct_owner_cleanup_once` | `:223`; `main` 调用 `:256` | 提前及重复 Close 均在 回调 前移除 root；cleanup 和 destructor 各只发生一次。本例通过。**已注释** |
| `main` | `:251`; 可执行目标 `parser-foundation-targets.cmake:155-162` | 运行四个 Unity 用例；基线 4/4 通过。**无需独立注释** |

### `tests/iterator/test_yield_syntax.c`

| 单元 | 定义与调用方 | 契约、运行结果与结论 |
| --- | --- | --- |
| `g_state` | 状态 `:12`; `setUp :35`, `tearDown :40`; parser 辅助函数及用例 | 新建 parser/runtime 状态；源文本与 AST 的释放顺序受该状态管理。现有注释明确 AST 先于状态销毁。**已注释** |
| `parse_source_has_error` | `:15`; rejection 用例 `:105-106`, `:114` | 创建 parser 状态、屏蔽预期诊断、读取 hasError，释放 AST 和 状态；source-name 分配失败按错误处理。现有注释限定断言用途。**已注释** |
| `setUp` | `:35`; Unity 测试目标 `parser-foundation-targets.cmake:164-166` | 每个用例创建新的 runtime 状态 并断言成功。**无需独立注释** |
| `tearDown` | `:40`; Unity 测试目标 `parser-foundation-targets.cmake:164-166` | 销毁非空 runtime 状态。**无需独立注释** |
| `test_yield_is_a_reserved_statement_keyword` | `:48`; `main` 调用 `:119` | 在 yield 1; 上初始化 lexer，检查独立的 ZR_TK_YIELD token 并释放 lexer。本例通过。**已注释** |
| `test_yield_parses_as_a_dedicated_statement` | `:62`; `main` 调用 `:120` | 解析函数体，验证 ZR_AST_YIELD_STATEMENT 与表达式节点，再释放 AST。本例通过。**已注释** |
| `test_yield_requires_expression_and_terminating_semicolon` | `:99`; `main` 调用 `:121` | 分别通过 辅助函数 拒绝缺少表达式和缺少分号的语句。本例通过。**已注释** |
| `test_iterator_function_modifier_remains_rejected` | `:110`; `main` 调用 `:122` | 拒绝 iterator fn；迭代身份从返回载体与 yield 推导，而不是修饰符。本例通过。**已注释** |
| `main` | `:117`; 可执行目标 `parser-foundation-targets.cmake:164-172` | 运行四个 Unity 用例；基线 4/4 通过。**无需独立注释** |

### `tests/iterator/test_iterator_semantic_ir.c`

| 单元 | 定义与调用方 | 契约、运行结果与结论 |
| --- | --- | --- |
| `g_state` | 状态 `:10`; setup/teardown `:12`, `:17`; IR 辅助函数及测试 | 管理 SemanticIrFunction 和 flow 分析分配，直至显式释放；现有注释说明 fixture 生命周期边界。**已注释** |
| `setUp` | `:12`; Unity 测试目标 `parser-foundation-targets.cmake:174-176` | 为每个 IR 用例创建独立 runtime 状态。**无需独立注释** |
| `tearDown` | `:17`; Unity 测试目标 `parser-foundation-targets.cmake:174-176` | 在 function/flow 清理完成后销毁非空 状态。**无需独立注释** |
| `source_range` | `:25`; 发射辅助函数及测试 | 清零 source range 并写入起止偏移，使发出的语义事实保留来源范围。现有注释说明用途。**已注释** |
| `emit_iterator_instruction` | `:35`; 三个 IR 测试 | 清零 指令描述，设置 opcode/类型/value/range，并明确以 无效 CFG 目标 ID 作为默认值；返回 semantic emit 结果。现有注释解释安全默认。**已注释** |
| `test_iterator_yield_sequence_has_canonical_suspension_facts` | `:53`; `main` 调用 `:267` | 构造 canonical value/suspend/resume/complete 事实并核对顺序与稳定文本。本例通过。**已注释** |
| `test_yield_value_requires_a_canonical_value` | `:118`; `main` 调用 `:268` | 以 invalid value ID 发射 yield 并确认校验失败，随后释放 function。本例通过。**已注释** |
| `test_yield_suspension_keeps_borrowed_value_live_until_resume` | `:135`; `main` 调用 `:269` | 创建局部值、共享借用与 yield pair，在嵌套块绑定 CFG 入口/出口后执行分析，检查借用在两侧的活跃状态，再释放结果/function。本例通过。**已注释** |
| `test_yield_suspension_keeps_borrowed_value_live_until_resume::bind_cfg` | 代码块 `:227-250`; 所属用例 `:135` | 手动将指令范围绑定到相连的入口/出口块，因为活跃性分析依赖 CFG 所有权；绑定后验证并分析再查询。代码块注释说明原因。**已注释** |
| `main` | `:265`; 可执行目标 `parser-foundation-targets.cmake:174-182` | 运行三个 Unity 用例；基线 3/3 通过。**无需独立注释** |

### `tests/iterator/test_yield_semantic.c`

| 单元 | 定义与调用方 | 契约、运行结果与结论 |
| --- | --- | --- |
| `semantic_ir_has_opcode` | 辅助函数 `:13`; 首个语义用例 `:153` | 空值安全地在传入的 semantic IR 中查找 opcode；本用例传入编译结束后已恢复的父级 `compiler->preSemanticIr`，其中没有已释放的子函数 yield 事实，因此首个断言失败。辅助函数的用途已有注释。**已注释** |
| `semantic_ir_opcode_count` | 辅助函数 `:31`; return/nesting 用例 `:236`, `:301` | 统计传入 IR 中指定 opcode 的数量；第 3、6 用例读取恢复后的父级 `preSemanticIr`，按旧假设期待子函数 completion 事实，实际计数为 0。已有注释。**已注释** |
| `semantic_ir_first_opcode` | 辅助函数 `:50`; 语义事实断言 | 查找首个匹配指令并返回借用指针；指针仅在 compiler 所有的 IR 仍存活时有效。已有注释说明借用寿命。**已注释** |
| `compiled_child_function_at` | 辅助函数 `:69`; 语义用例读取已编译子函数产物 | 检查 child index 边界并返回 compiler child-function 存储中的借用指针。已有注释说明声明顺序映射。**已注释** |
| `compile_source` | 辅助函数 `:81`; 六个语义测试 | 执行真实 parse 和顶层编译；先将 outAst 置空，失败时释放部分 AST/compiler，成功时将 AST 所有权转给调用方，由调用方稍后销毁 compiler 与 AST。已有注释覆盖流程和所有权。**已注释** |
| `destroy_compilation` | 辅助函数 `:120`; 各语义用例的清理路径 | 依次释放 current function、compiler 状态、AST；随后由调用方销毁 状态。已有注释说明顺序。**已注释** |
| `test_yield_requires_canonical_iterator_carrier_and_projects_facts` | `:134`; `main` 调用 `:312` | 成功编译显式 Iterator<int> yield 后，从已恢复的父级 `compiler->preSemanticIr` 查找子函数 yield 事实。基线第 154 行断言 Expected TRUE Was FALSE；之后的 suspend/resume/completion、子函数产物及 source range 断言尚未执行。该观察假设早于 child SemIR isolation。**BUG：过期测试观察契约，不是 Iteration provider 缺陷** |
| `test_yield_rejects_iterable_carrier_and_incompatible_payload` | `:189`; `main` 调用 `:313` | 验证 Iterable 不是 Iterator carrier，并在 parse 成功后拒绝不兼容 payload。本例基线通过。**已注释** |
| `test_yield_function_allows_only_empty_return_completion` | `:218`; `main` 调用 `:314` | 成功编译空 return 后，从恢复的父级 `preSemanticIr` 期待一个子函数 iterator completion 事实。基线第 237 行断言 Expected 1 Was 0；子函数 IR 已由 callable isolation 释放，后续带值 return 的拒绝检查未执行。**BUG：过期测试观察契约，不是 Iteration provider 缺陷** |
| `test_yield_rejects_top_level_statement` | `:251`; `main` 调用 `:315` | 确认顶层 yield 没有 iterator frame，因此语义编译拒绝。本例基线通过。**已注释** |
| `test_yield_rejects_property_accessor_context` | `:266`; `main` 调用 `:316` | 确认 property accessor 没有 Iterator 返回 carrier，因此拒绝 yield。本例基线通过。**已注释** |
| `test_nested_iterator_yield_does_not_reclassify_the_outer_function` | `:285`; `main` 调用 `:317` | 成功编译普通 outer 函数与嵌套 Iterator<int> inner 后，在恢复的父级 `preSemanticIr` 中期待一个 inner completion 事实。基线第 302 行断言 Expected 1 Was 0；源码分类器确实跳过嵌套函数，但该断言观察不到已释放的 child IR。**BUG：过期测试观察契约，不是 Iteration provider 缺陷** |
| `setUp` | `:307`; Unity 测试目标 `parser-foundation-targets.cmake:184-186` | 空钩子；语义用例自行创建并销毁各自 状态。**无需独立注释** |
| `tearDown` | `:308`; Unity 测试目标 `parser-foundation-targets.cmake:184-186` | 空钩子；每个用例在返回前完成自身清理。**无需独立注释** |
| `main` | `:310`; 可执行目标 `parser-foundation-targets.cmake:184-200` | 运行六个 Unity 用例；基线三项通过、三项断言失败，执行顺序明确。**无需独立注释** |

## yield 语义断言失败及其范围

三处失败都是正向语义探针：测试成功编译源码后，继续通过已恢复的父级 `compiler->preSemanticIr` 读取子函数事实，假设子函数编译结束后仍会把 yield/completion 事实留在父图中。当前隔离路径从 zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c:305-313 开始；`compiler_semantic_ir.c:1411-1466` 保存父级图、初始化临时 child 图，再释放 child 图并恢复父图；`compiler_function.c:425-430` 在 child 图内发射 completion，`:522-525` 结束隔离并恢复其他编译状态。现行设计记录于 docs/parser-and-semantics/pre-semantic-ir-flow.md:442-448、:590-595，并由 tests/parser/test_pre_semantic_ir_callable_isolation.inc:17-65 直接测试。

yield_semantic-before.stdout.log 记录三处失败，断言分别位于 154、237、302 行；其余三个用例通过。将其登记为三条过期的可观察测试契约 BUG，而不是 Iteration 描述符或 yield lowering 缺陷。本批次不改变行为或测试。

相邻的 parser binder 仍有独立源码 TODO：同时存在 Iterator<T> 和 Iterable<U> 事实时优先选择 Iterator，但未验证两种类型实参冲突兼容性（zr_vm_parser/src/zr_vm_parser/compiler/enumerator_binding.c:15-16）。通过的协议测试分别检查两种协议，没有覆盖冲突输入；这只是邻接疑点，不归因于 Iteration provider，也未在本批次改动。

## 历史台账边界

本批次没有改动 `docs/code-review/coverage/tests_iterator_task.tsv`：该 TSV 文件本身当前干净，但它引用的源码有他人迁移改动；当前 check-batch 因旧 zr_vm_core/.../iterator 及已迁移 parser/CMake 路径等证据失效而 exit 1。历史台账只作线索，不作当前覆盖证明。下面六个表逐项记录 helper/state/type/callback/setup/teardown/main/测试用例，共 90 个单元；测试源仍按只读范围处理。

Root 使用同一 MSVC 静态 Debug 构建目录完成最终复核。`iteration-final-comment-equivalence.json` 证明四个模块源文件的注释外词法内容、行首以及 CMake 非注释命令与基线相同；`iteration-final-after-results.json` 记录最终注释后的增量构建 exit 0。六个现有测试可执行文件的 33 项结果与注释前完全一致：30 项通过，`yield_semantic` 中位于 154、237、302 行的三项断言仍失败。失败来自旧测试观察契约：它从 callable isolation 后恢复的父级 `preSemanticIr` 查找已释放的子函数事实；根因记录见 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/yield-semantic-before-diagnosis.md`。这些失败登记为过期测试观察契约 BUG，本批次不修改行为。最终源码 SHA-256 见上表；最终 ledger SHA-256 为 `403609DAF19AB52E0D192CBDCF4EFD0255848E0BFBF193E0A662D8F6A280B7E0`。
