---
related_code:
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - zr_vm_common/include/zr_vm_common/zr_runtime_limits_conf.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - zr_vm_parser/src/zr_vm_parser/syntax/lexer.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_workers.c
  - zr_vm_lib_system/CMakeLists.txt
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
implementation_files:
  - zr_vm_lib_system/include/zr_vm_lib_system/gc.h
  - zr_vm_lib_system/include/zr_vm_lib_system/gc_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/gc/gc.c
  - zr_vm_lib_system/src/zr_vm_lib_system/gc/gc_registry.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 2026-10-14 System GC callback contracts and failure-path audit
tests:
  - tests/gc/gc_tests.c
  - tests/module/test_module_system.c
doc_type: workflow-detail
---

# System GC 契约审查

## 范围和单元清点

本批覆盖四个 GC 头文件与实现文件，并新增逐单元 leaf 台账 zr_vm_lib_system_gc.tsv。台账共 42 条数据行：33 个函数、接口、宏、描述符或独立表，另有 9 个独立 BUG/TODO 语义块；决策分布为 31 commented、2 no-comment、3 BUG、6 TODO。

四个源文件进入 zr_vm_lib_system target 的构建路径是 CMakeLists.txt:1 导入 CommonMacros.cmake，CommonMacros.cmake:9-11 对模块 src/*.c 建立 target 源列表；module.c:80-100 将 GC leaf descriptor 交给 system 根注册器。这里记录的是当前构建与注册入口，不扩大本批源码注释范围。

- gc.h：include guard 与六个公开 callback，共 7 项；六个声明都具备 Summary、Parameters、Returns、Constraints and lifecycle、References。文件级约束要求宿主协调同一 global collector 上的控制调用、显式 collection 和其他更新。
- gc_registry.h：guard 与静态 descriptor getter，共 2 项。
- gc.c：13 个函数和 9 个需单列的语义块，共 22 项。
- gc_registry.c：条件数组计数 fallback、三个参数表、42 字段表、getter、类型/函数/hints/schema 表及模块 descriptor，共 11 项。

既有 descriptor 字段名和英文提示未改。enabled 原提示涉及 background/incremental GC，实际投影为 !stopGcFlag，不代表 gcMode 或 worker 是否运行；差异列在声明及台账中。

## 调用链与契约结论

六个 callback 由静态 descriptor 注册；dispatcher 在调用 callback 前检查元数，再由 dispatch lane 调用函数指针。类型提示不是运行时校验器；参数类型错可由 ReadInt/ReadString 抛 VM 异常，callback 对无效 context/result、负转换结果或未知 kind 返回 false，不能混淆两类状态。操作作用于调用 State 所属 global 的共享 collector；宿主需串行协调同一 global 上的 GC 控制调用、显式 collection 及其他 collector 更新。

Disable 仅置 stop flag，不还原 generational mode，也不撤销已经进入的 step。Core 的 generational step 会直接推进，CheckGc 可按 debt 调用 GcStep；run-until-state、stop-aware loop 和 generational path 检查 flag 的位置不同。因此不能承诺所有自动 GC 都停止，现有测试只确认 mode 保留与 flag 设置，跨路径语义仍是 TODO。

SetBudget 用一个非负值同时设置 pause 与 remark 配置。Core 在 non-generational 增量 GcStep loop 中把零折算为至少一个工作单位；generational 模式不保证所有阶段都受此值约束。初始 major prepare 是否受 pause budget 限制待核，remark 同步尾段不受 remark budget 限制，接口也不承诺硬墙钟上限。

Collect 注册范围为 0..1 参数，零参默认同步 full。minor/major 会排程再推进一次 GcStep，不表示完整周期结束。full 使用 void GcFull；StopTheWorldBegin 超时失败时 Core 静默返回，而 callback 仍写 null 并返回 true。kind 的契约要求全字节精确匹配；合法脚本字符串 full NUL tail 经 lexer 保留显式长度并创建 VM string，native view 含 NUL，但 strcmp 只观察前缀 full 并接受它。这是本叶由完整合法路径证明的窄 BUG。

GetStats 返回新建并由 VM 管理的统计对象。Core snapshot 在创建对象和字段键前读取；之后的 VM/native 分配可能触发 GC，并改变后续可观察状态。enabled 后续单独读取 stop flag，所以它和其他字段不是同一原子时点；Core 未保证 snapshot 通用线程安全，仓内 task worker 可创建同 global 多个 State，宿主同步策略仍待核。

Stats 投影有三条范围明确的软失败漏字段路径：

1. 初始 nodeMap bucket 初始化失败时 native type registry 仍可能返回新对象。后续字段写入遇到未就绪 map 会重试 void Object_Init；retry 仍失败时 Core 只记录日志并返回，Stats 循环继续，GetStats 最终仍可能成功交付缺字段对象。
2. ZrLib_Object_SetFieldCString 固定 object 或 key 时，如 ignored-root registry 需要扩容而 RawMalloc 失败，会静默返回；void writer 无法传播失败。
3. 新对象动态字段 map 初始容量为 4、resize threshold 为 3。字段按 enabled、heapLimitBytes、managedMemoryBytes、gcDebtBytes 顺序写入；前三项后插入第 4 个总字段 gcDebtBytes 首次跨阈值。若其后续 bucket allocate 软失败，Object_SetValue 只记日志并返回，writer 和 callback 继续；42 个字段的投影还会跨越更多阈值。

首个 enabled bool 本身不触发字段 map Rehash；它仍受初始 map 重试和 object/key pin 两类路径影响。以上只记录已闭合的失败条件，不泛化为任意 OOM。

另有一项尚未定性的表示范围 TODO：GetStats 将若干 `TZrUInt64` 快照字段转为 `TZrInt64`（`gc.c:266-295`）；Core 在 `gc.c:191-194` 累加 collection count 与 duration 的语句没有显式 `INT64_MAX` 饱和检查。需继续核对这些计数的生命周期上限、溢出策略和 VM signed integer 展示契约，才能判断长周期运行是否会产生可观察的越界投影。现有字段布局（runner :8133）与 safe controls（runner :8136）不覆盖长期累计边界；本批未做长周期试验，因此仅记 TODO，不标为 BUG。

SetHeapLimit 与 SetBudget 共用 ReadInt。该共享入口接受 FLOAT/DOUBLE，越过 int64 范围时直接 cast。合法数值 1e20 可经 lexer 指数解析和 strtod 形成有限但超范围 double，再经 arity 检查后的 native callback 到达 cast，因此共享 UB 在这两个 callback 可达；本批没有运行边界探针，也没有修改 shared 行为。-0.5 向零转换为 0，不会被 helper 的负值检查拒绝，不能把所有小数/负数写成拒绝契约。另保留两个表示范围 TODO：ReadInt 对 UINT64 大于 INT64_MAX 转 signed 的结果由实现定义；SetHeapLimit 转 TZrMemoryOffset 后的 32 位超界策略未验证。Budget 阶段消费和 TZrSize cast 也保留具体核查入口。

## 测试和证据边界

已有测试只记其实际断言范围：

- test_system_gc_enable_callback_switches_generational_mode（tests/gc/gc_tests.c:1442，runner :4718）直接调用 Enable/Disable，断言 mode、stop flag 和 null 结果；该独立 GC suite 未在 final-r2 运行，因此这里只记录已读的测试契约，不作为执行结果，也不证明所有 Core 路径停止。
- test_system_gc_safe_controls_report_runtime_stats（tests/module/test_module_system.c:3692，runner :8136）从脚本调用 enable、set_heap_limit、set_budget、get_stats、disable 和三种 collect，断言若干字段；它在 final-r2 的 78 项 moduleSystem suite 中 PASS。未覆盖浮点边界、NUL、并发 snapshot 或分配失败。
- test_system_native_types_register_complete_struct_fields（:3357，runner :8133）检查布局/offset，不验证 OOM 下字段完整性。共享 exports/hints 测试函数由 Process canonical 台账登记，本表不重复相同测试单元。
- 仓内没有覆盖上述 Stats 三种软失败或 Disable 全路径停机语义的专用故障注入测试；本批没有新增测试。

Root 的 final-r2 验收对 8 个 GC/Vector3 源文件做严格非注释 token 等价证明（proof SHA-256 `3189e5479a6902de00b95e6cb8c7b49171663ffd155ad05b4f60034d67da0534`），并强制重编 4 个相关对象后构建 Debug/static `zr_vm_module_system_test`，exit 0，8.438 秒。构建有一条 foreign Parser C4100（`type_inference_import_metadata.c:690` 的未使用 `memberInfo`），因此不记为无 warning。其后现有 moduleSystem 测试 exit 2、15.750 秒，78 项中 76 PASS、2 FAIL；逐 case 与修改前完全一致：`test_module_init_summary_cache_growth_keeps_current_source_summary_stable`（`tests/module/test_module_system.c:2440`，Expected 8 / Was 0）和 `test_builtin_wrapper_box_helpers_preserve_runtime_semantics`（`:4797`，Expected Non-NULL）。GC controls、字段布局和 Vec3 构造相关用例通过。构建/测试前后当前 1652 输入 pins 稳定，未留下临时文件；但与旧 before pin 对照有 7 个 foreign Parser 文件变化，所以这不是完全隔离的 before/after。未运行 OOM 注入、并发、非法 numeric 边界测试，本批没有新增测试或行为修复。完整记录在 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/gc-vector3-final-r2-results.json` 与其 token proof 中。

加入七个 private helper 文档级注释后，Root final-r3 对最终 8 个 GC/Vector3 源文件再次完成严格 token 等价证明（proof SHA-256 `d0e2d1c3cd1e02bb6645b5fc35da7a77a3e36780521dfded24d257ffa80ec69b`）。强制重编本批 2 个 GC 对象后，Debug/static `zr_vm_module_system_test` 构建 exit 0、32.860 秒、无 warning；当前 1652 输入 pins 在此次构建/测试前后稳定且未发生变化。moduleSystem 测试 exit 8、26.172 秒，78 项中 70 PASS、8 FAIL；先前 2 个失败仍在，另有 6 个 binary/reflection/roundtrip 用例变为失败，逐项为 :2702、:5912、:6255、:7082、:7815、:7931（完整用例名和断言见 final-r3 results）。GC safe controls（runner :8136）、Stats 字段布局（:8133）、Vector3 构造（:8121）和 System VM nested（:8118）均 PASS。相对旧 before pin 有 9 个 foreign Parser 文件变化；因此新增失败不能归因于本批注释或外部改动，比较并非隔离。无 fault injection、并发或非法数值边界执行；无新增测试/行为修复、无临时文件。完整记录在 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/gc-vector3-final-r3-results.json` 与 `gc-vector3-final-r3-comment-equivalence.json`。
