---
related_code:
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/include/zr_vm_core/memory.h
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - zr_vm_common/include/zr_vm_common/zr_runtime_limits_conf.h
implementation_files:
  - zr_vm_lib_math/include/zr_vm_lib_math/vector2.h
  - zr_vm_lib_math/include/zr_vm_lib_math/vector2_registry.h
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c
  - zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 2026-10-13 Vector2 contract review assignment
  - root: 2026-10-13 Vector2 scope and independent-review notes
tests:
  - tests/fixtures/projects/native_numeric_pipeline/src/signal.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/callbacks.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/probe_callbacks.zr
  - tests/cmake/run_projects_suite.cmake
  - tests/library/test_official_provider_convergence.c
  - tests/cmake/compiler-project-targets.cmake
doc_type: workflow-detail
---

# Vector2 契约审查批次（2026-10-13）

本批覆盖 Vector2 两个公开头、两个叶实现，以及被这些实现直接使用的 `native_numeric_pipeline` 手写 fixture。只补充中文文档级注释和叶 coverage 台账，没有改变实现、签名、预处理条件或测试文件。

本批四个源码的冻结工作树身份如下；这些 SHA-256 标识审查 checkout 中的文件内容，不代表持久源码副本：

| 文件 | SHA-256 |
| --- | --- |
| `zr_vm_lib_math/include/zr_vm_lib_math/vector2.h` | `3510631af641e5635681dba6c05949ba85f80c512a2272f65e8ed63bb2f0108e` |
| `zr_vm_lib_math/include/zr_vm_lib_math/vector2_registry.h` | `0495434f06a3a2a73af9aacf13b4016d162997e0bfee723ed804b2cf30876412` |
| `zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c` | `82af326298c0ffa52a78b2d73bdcc726d468829a0f3f370094c7ed71152399bb` |
| `zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c` | `bbc8e27f47950a3eaa92ee614705ad99be32917344bc3c0524e8d5dfc73c5fb2` |

## 单元枚举和范围

叶表 [`zr_vm_lib_math_vector2.tsv`](../coverage/zr_vm_lib_math_vector2.tsv) 严格按九列记录 46 个 `(file, unit, kind)` 唯一单元：

| 文件 | 单元拆分 | 数量 |
| --- | --- | ---: |
| `vector2.h` | 文件契约、include guard、12 个 callback 声明 | 14 |
| `vector2_registry.h` | 文件契约、include guard、3 个 registry getter 声明 | 5 |
| `vector2.c` | 文件契约、12 个 callback 实现、构造字段发布块、factory 字段发布块、toString 字符串发布块、`Construct.kFields` 配置 | 17 |
| `vector2_registry.c` | 文件契约、3 个 getter、`kFields`/`kMethods`/`kMeta`/`kType`/`GetHints.kHints` 五个静态配置 | 9 |
| `signal.zr` fixture | `mixSignalImpl` 中 Vector2 构造与归一化块 | 1 |

本叶没有自行定义 C struct 或运行时配置宏。两个 include guard 单独登记；registry 使用的 `ZR_LIB_*_DESCRIPTOR_INIT` 与 `ZR_ARRAY_COUNT` 是共享定义的初始化/计数宏，已追其展开语义但不重复登记为本叶定义单元。`zr_vm_lib_math/CMakeLists.txt:4` 经 `zr_vm_common/CommonMacros.cmake:6-14` 将 `src/*.c` 纳入模块，因此 registry 与 callback 实现处于活动构建输入；`zr_vm_lib_math/src/zr_vm_lib_math/module.c:113,125,138` 实际调用本叶三个 getter。

## Descriptor 与运行入口

`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:11-14` 将 `x`、`y` 注册为结构原型的 float 字段；`:16-28` 登记六个实例方法；`:30-37` 登记 constructor 与五个 meta 行为；`:40-59` 组成静态 type descriptor。`:69-75` 的 hints 是另一个静态表。模块聚合器经 `zr_vm_lib_math/src/zr_vm_lib_math/module.c:138` 返回该表；下游 LSP 在 `zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c:776-779` 查找 hint，在 `zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.c:264-274` 取签名。描述符、嵌套数组、callback 地址和字面量由 provider image 持有，返回值均为只读借用。

callback arity 如下；dispatcher 在选择 lane 之前核对固定数量：

| Callback | Arity | 参数与结果要点 |
| --- | ---: | --- |
| `Construct` | 2 | 两个 VM 数值；兼容的 self 被复用，否则按 construct target/owner prototype 新建 receiver；结果交给 VM |
| `Length`、`LengthSquared`、`Normalized` | 0 | 只读取 receiver 的 `x`、`y`；归一化结果为新 VM 对象 |
| `Dot`、`Distance`、`MetaAdd`、`MetaSub`、`MetaCompare` | 1 | rhs 必须是对象/数组形状，并可读取数值 `x`、`y` 字段 |
| `Lerp` | 2 | rhs 对象/数组加一个 VM 数值 factor；factor 不夹紧，当前公式允许外推 |
| `MetaNeg`、`MetaToString` | 0 | 分别返回新对象与格式化字符串 |

具体注册行是 indirect binding entry，而不是 C 直接调用点。运行时 dispatcher 的 callback 指针解析在 `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:202,205`；真正的间接表达式是该头文件 `:277` 的 `callback(context, result)`，由 `:283-293` wrapper 接收 lane 调用。`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:383` 选择 fast lane 并进入 `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:157`；`:395` 是 inline-pinned lane 选择入口，其他 lane 调用点见 `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c:225,309,405`；通用路径通过 `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:615`。`:349-354` 的快路参数门只接受非 GC/无 ownership 参数，故 rhs 为 Vector2 对象/数组的 `Dot`、`Distance`、`Lerp` 与二元 meta 不属于 scalar fast lane。`Construct` 的两个数字实参能满足数值准入；fast lane `:143-150` 仅浅复制 receiver 和 argument value slot，不 pin 对象。

数量错误由 `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:34,364` 在进入 lane/callback 前报 `ArityError`。合法 arity 进入回调后，`ReadFloat` / `ReadObject` 的错误值标签走 VM `RaiseTypeError`（同文件 `:1152,1182,1221-1229`）；缺字段或错误字段形状是 Vector2 callback 的普通 false 路径。result 在 dispatch 入口先初始化为 null（`:362`）；callback false 只有当线程状态仍为 `FINE` 才转为 null（`:375-378,618-624`），若状态已错误则按 `:664-674` 清理并传播错误。上述契约假设 callback 收到有效 context/state 和非空可写 result 槽，不能外推成对任意无效指针的保护。

## 构造字段和生命周期

`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c:9` 的 `Construct.kFields` 是静态字符串指针表，`values[2]` 则在 `:19-20` 赋值、经 `:21` 同步传入 `ZrMath_ConstructFloatObject`，只活到 helper 返回；两者不是同一存储期。它们按相同顺序对应 registry 的 `zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:12-13`。通用构造 helper 在 `zr_vm_lib_math/src/zr_vm_lib_math/common.c:200-219` 复用与 owner prototype 兼容的 receiver，否则尝试 construct target/owner prototype 创建对象；`:248-252` 循环写字段后完成结果发布。

真实脚本构造链是 `zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c:994-1003` 发出 `CREATE_OBJECT`/`TO_STRUCT`，`:914-955` 查找 `__constructor` 并发出 `FUNCTION_CALL`；测试 fixture `tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:8` 有该语言入口。对 arity=2 的 Vector2，两数值实参满足 fast-lane eligibility，不能假定 receiver 已由 lane pin。合法 ignored-root `count=capacity-1` 且 seed self/key 尚未登记时，setter 先 pin receiver 占末槽，再为字段字符串 key 扩表；`SetFieldCString` 的 key pin 分配失败分支位于 `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1400-1403`，ignored registry 扩容失败从 `zr_vm_core/src/zr_vm_core/gc/gc.c:696-704` 传播，数组重配在 `zr_vm_core/src/zr_vm_core/gc/gc_object.c:746`。该 setter 返回前会软退出，`ZrMath_ConstructFloatObject` 不检查 void setter 结果并继续 `FinishConstructObject`（`zr_vm_lib_math/src/zr_vm_lib_math/common.c:248-252`），所以可发布缺字段 receiver。此结论只针对闭合的 key-pin 分配路径，不把 self pin 本身描述为失败，也不说数值转换失败是普通 false。

## 可达 BUG 与待核项

- **数值范围 BUG**：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c:29` 与 `:47-49` 都以平方和求长度。有限 `(1e200,0)` 的平方和成 Inf，因此 `Length` 返回 Inf，`Normalized` 将非零分量除以 Inf 得零向量。`Distance` 的差分后平方同样使 `(1e200,0)` 到零的有限距离返回 Inf（`:72-75`）。`Lerp` 按 `lhs + (rhs-lhs)*factor` 运算，有限端点 `1e308` 与 `-1e308`、factor `0.5` 的中间差溢出为 `-Inf`，数学中点为零（`:89`）。这些结论来自表达式及合法接口参数的完整静态路径，未声称运行了这些数值用例。`LengthSquared` 直接返回不可表示的平方和，属于平方值本身溢出，不与有限长度丢失混记。
- **构造器 key-pin BUG**：见上一节；根因是 ignored-root 扩容分配失败被 void field setter 静默吞掉，构造 helper 继续发布。另一个兼容 self 复用路径与 fallback target 实例路径都记录在叶表；漏洞证据不依赖把 self pin 说成失败。
- **factory 新对象缺字段 BUG**：`zr_vm_lib_math/src/zr_vm_lib_math/common.c:22-35,255-259` 创建 Vector2 后逐字段调用 void setter，非空 object 即返回；Normalized/Lerp/MetaAdd/MetaSub/MetaNeg 的调用处见 `zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c:49,89,101,113,124`。新实例初始化 `zr_vm_core/src/zr_vm_core/object/object.c:1509,1519` 建立普通两字段 nodeMap；此处两字段未达到 `zr_vm_core/include/zr_vm_core/hash_set.h:36-37` 与 `zr_vm_common/include/zr_vm_common/zr_runtime_limits_conf.h:17` 的扩桶条件。独立可达故障仍是 ignored-root pin 扩容失败造成 x/y 缺失，不移植 Quaternion 的第四字段 nodeMap 扩桶 BUG。
- **toString 结果传播 BUG**：仅当格式结果为未驻留短串，且短串表插入达到 `resizeThreshold` 后触发 `HASH_BUCKET` rehash 时，原生分配器失败会使 `ZrCore_HashSet_Rehash` 返回 false，字符串创建链得到 null；直接 allocator 接口原样返回 null（`zr_vm_core/include/zr_vm_core/memory.h:19-22`），rehash 对失败的实际分支见 `zr_vm_core/src/zr_vm_core/base/hash_set.c:170-181`。`ZrLib_Value_SetStringObject` 遇到 null 不写结果（`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1311-1316`），`ZrMath_MakeStringResult` 仍返回 true（`zr_vm_lib_math/src/zr_vm_lib_math/common.c:512-519`）。dispatch 在 `:362` 初始化 result 为 null，成功回调后由 `:671-674` 将这个 null 拷贝至调用结果；写回闭包槽的拷贝点在 `:249-267`。其余链路见 `zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:320-324`、`zr_vm_core/src/zr_vm_core/string/string.c:802,914,958,973`、`zr_vm_core/include/zr_vm_core/hash_set.h:525`。这是特定未驻留短串 rehash 分配失败路径，不是所有格式结果都会遇到，也没有做 allocator 故障注入。
- **共享布局 TODO**：`Dot` 以及 Length、LengthSquared、Normalized、MetaCompare 将 `ZrMathVector2` 地址 cast 成连续 `TZrFloat64` 输入；结构声明与既有共享 TODO 在 `zr_vm_lib_math/include/zr_vm_lib_math/math_common.h:20,54`，故需要核实成员布局、对象模型和 strict-aliasing 前提。
- **MetaCompare 契约 TODO**：实现的严格 `>`/`<` 比较在 NaN 或双方平方和溢出为 Inf 时都落入 0；wiki 的 Vector2 比较正文锚点为 `docs/wiki/03-modules/math-api.md:308`，但未定义这些 unordered/overflow 输入的排序语义。后续需核对 VM 消费 compare 结果的排序约定，并为本分支补边界断言；struct cast 布局仍是独立 TODO。

## 现有测试证据边界

手写项目 fixture `tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:8` 构造并归一化 Vector2，`:12-13` 返回 `vx` 与 `vy`。`tests/fixtures/projects/native_numeric_pipeline/src/callbacks.zr:15,31` 与 `tests/fixtures/projects/native_numeric_pipeline/src/probe_callbacks.zr:16,28` 调用该函数，但 checksum 只累计 `signalInfo.magnitude`，没有读取或断言 `vx`、`vy`。`tests/cmake/run_projects_suite.cmake:506-508` 分别登记 banner、checksum 与 VM probe 项；suite 成功不能证明 Vector2 分量值或上述分配故障。没有修改这些测试/fixture，也没有新增数值断言或故障注入。

本批只为 Vector2 建立 canonical 叶 coverage。旧 `zr_vm_lib_math.tsv` 的 Vector2 历史行由 Root 迁移，避免同一 `(file, unit, kind)` 重复记录；本文件没有更改全局 index/inventory、旧 Math 总表或共享测试代码。

## 校验状态

- 叶表通过 `scripts/code_review_inventory.py check-batch docs/code-review/coverage/zr_vm_lib_math_vector2.tsv` 的九列与路径锚点检查；终版再次校验通过。对批次文件的 UTF-8/行尾/尾随空格检查及 `git diff --check` 通过。
- Root 在实际 checkout 复用 MSVC/Ninja Debug/static build tree，强制重建两个 Vector2 对象后构建 Math static、CLI executable 与 official-provider convergence 目标，exit 0（4.719 秒），无本轮编译 warning。结果记录在 `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/vector2-final-r3-results.json`，对象重建回执在同目录 `vector2-final-r3-object-rebuild-receipt.json`。随后 existing official-provider convergence 9/9 PASS（0.115 秒），九个 case 与修改前相同。
- `vector2-final-r3-comment-equivalence.json`（SHA-256 `8ce03297798acfc5691dc326bff304d8b67f3628481a81933b685ba1cab9197f`）记录终版四个 Vector2 源的非注释 token 与行首预处理标志一致；较早的 r1 proof 曾覆盖 8 个 Vector2/Console 源，但此处以后续 r3 四源证明为准。输入 pins 均稳定，测试与 fixture 未改。较早 r1 集成 CLI 构建曾有 7 条 C4090/C4267 warning；本次两对象强制重建无 warning，不据此声称所有配置均无 warning。
- 此验证只重建 Vector2 目标并运行 provider 收敛测试；没有执行 Vector2 数值、compare、布局边界或 allocator 故障注入用例。9/9 provider 结果及手写 fixture 不构成这些行为的直接断言。
