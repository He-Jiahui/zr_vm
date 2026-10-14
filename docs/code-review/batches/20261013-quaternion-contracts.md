---
related_code:
  - docs/wiki/03-modules/math-api.md
  - zr_vm_lib_math/src/zr_vm_lib_math/module.c
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
implementation_files:
  - zr_vm_lib_math/src/zr_vm_lib_math/quaternion/quaternion.c
  - zr_vm_lib_math/src/zr_vm_lib_math/quaternion/quaternion_registry.c
  - zr_vm_lib_math/include/zr_vm_lib_math/quaternion.h
  - zr_vm_lib_math/include/zr_vm_lib_math/quaternion_registry.h
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 2026-10-13 Quaternion contract review assignment
tests:
  - tests/library/test_official_provider_convergence.c
  - tests/module/test_module_system.c
  - tests/cmake/compiler-project-targets.cmake
doc_type: workflow-detail
---

# Quaternion 契约审查

## 范围与清点

本批审查 Quaternion 两实现、两公开头文件。[逐单元台账](../coverage/zr_vm_lib_math_quaternion.tsv)
包含 48 项：实现 17 函数与构造字段配置共 18 项；registry 的 3 getter 与
fields/methods/meta/type/hints 五配置共 8 项；callback header 的 15 声明、文件契约与
include guard 共 17 项；registry header 的 3 声明、文件契约与 include guard 共 5 项。
结论为 26 BUG、2 TODO、18 已注释、2 无需注释；无未审查项。BUG 的行数是受影响单元数，
不是独立缺陷数；Slerp 单元同时保留下述次要契约 TODO。

descriptor 把 8 个实例方法和 7 个元操作映射到 callbacks；Quaternion 没有额外顶层
模块函数。聚合器 `module.c:116/:128/:141` 分别取函数、类型与 hints。`GetType` 返回
静态 descriptor，聚合器浅拷贝后仍借用嵌套表；注册方必须使 provider image 驻留至
全部借用结束。对象调用从方法/meta descriptor 取得 callback，再经
`native_binding_dispatch_lanes.h:277` 调用函数指针。所有直接调用和注册入口逐项列入台账。

## 参数、失败状态和生命周期

构造器读取四个可转 double 的数值并按 x/y/z/w 写入；`common.c:200-220` 可复用
兼容 self，也可创建实例，不能一概说构造总创建新对象。数值读取不要求有限值或单位范数。
字段读取按名称和数值类型，不验证 Quaternion prototype。结果对象交给 VM 管理；
context、self/参数视图和 result 只借用于本次 native callback，不得保存或在返回后使用。
构造字段名表是 provider image 中的 static const，values 是当前 callback 的栈快照。

`ReadFloat`/`ReadObject` 缺参或标签不匹配进入 `ZR_NO_RETURN` VM 错误；它们不以普通
false 返回。字段缺失或字段非数值则可由 `ReadQuaternionObject` 返回 false；factory
返回 NULL 也是普通失败入口。factory 返回非 NULL 不保证四字段完整，见下述已证实缺陷。

## BUG：完整静态调用路径

构造器允许有限输入 `(1e200,0,0,0)`。未缩放平方的中间值溢出，尽管数学结果可表示：
`length` 返回无穷；`normalized` 返回零四元数；`inverse` 将可表示的 `-1e-200` 分量算成零。
`lengthSquared` 的数学平方结果本身不可表示，不把它的无穷返回重复标为 BUG。

合法 `toString` 经 descriptor → native dispatch → `ZrMath_MakeStringResult`
→ void `ZrLib_Value_SetString` → `native_create_string` → `String_CreateTryHitCache`
→ 未驻留短串创建 → `HashSet_AddRawObject` → `HashSet_Rehash` → 原生分配器。
当短串表增长所需 HASH_BUCKET 分配失败、没有 VM 异常，setter 不写结果；dispatch
预置的 null 保留，而 helper/callback 仍报告成功。证据包含 `common.c:512-519`、
`native_binding_dispatch.c:362/:1307/:1312/:664/:675`、support `:320/:324`、
string `:802/:916/:958/:974`、hash_set header `:535/:536` 和 Rehash `:170/:176`。
结论限定到合法调用与可返回的失败路径，不将其他 core 缺陷转记到本叶模块。

另一个已闭合路径发生在空 nodeMap 的新 Quaternion 实例。`Object_Init:1519` 用
`zr_runtime_limits_conf.h:17` 的 log2=2 初始化容量 4，hash_set 的 3/4 负载阈值为 3。
`x/y/z` 插入及所有 pin/键字符串创建成功后，第四个 `w` 插入先触发
`HashSet_Add:498-500` 的扩桶；宿主 realloc 对 HASH_BUCKET 返回 NULL 且保留旧块。
`Object_SetValue:1840-1847` 结束 mutation、解除 pin 后只记录日志，不设置 VM 错误。
日志 callback 正常返回时，void setter 无法反馈失败，`common.c:22-36/:231-253/:273-277`
的逐字段 factory 与外层仅检查对象非 NULL 的 callbacks 可以交付缺 `w` 的对象并报告成功。
原生分配器入口、对象初始化、hash 阈值、setter/日志、factory、dispatch 清理均列入台账。
构造复用兼容 self 的路径仍存在；该触发条件限定从空 nodeMap 开始的填充路径。

以上五组缺陷为完整静态路径证据；没有运行数值边界复现或分配故障注入，本轮不修复行为。

## TODO 与已核文档

`math-api.md:145-148` 要求 Slerp 单位端点；native 不检查、归一化或夹紧 dot。文档没有
规定 t 区间或承诺单位输出。TODO 入口是单位近邻端点的消费者和线性分支，核实它们是否
依赖单位输出以及 t 契约；不拿非单位端点扩张 BUG 结论。

MetaCompare 按平方模长返回 -1/0/1。两侧有限大分量的平方和可同时成为无穷而返回相等，
但公开 compare 是否承诺保存这些输入的真实大小次序仍待核。TODO 指向 VM compare
消费协议与 Quaternion 排序用途，不直接断言实现违反尚未确认的承诺。

[math API](../../wiki/03-modules/math-api.md) 仅更正本 Quaternion 契约及问题；既有 Slerp
单位端点说明保留。旧 Math 总台账只迁移本四叶源的 23 项为本次 48 项，其他行字节和
相对顺序保持；其既有失效锚点单独登记，整张 Math 表尚未认证。

## 验证及测试边界

仓内未检索到 Quaternion 数值运算测试。module 测试的 sqrt/Vector3 并非这些 callbacks
的算法覆盖；official-provider 的 `test_official_provider_convergence.c:37` 只是 zr.math
预期清单项，九 cases 不调用 Math/System getter 或 Quaternion callback。

Root 在同一实际 checkout、同一 MSVC Debug/static 构建目录做修改前后对照。前构建退出0、
provider 9/9 PASS；最后源码修订后实际构建 24.984 秒退出0、provider 9/9 PASS（1.469秒），
各 case 与前基线完全相同。17 个源码/测试 pins 和 15 个共享证据依赖 pins 在该轮前后稳定。
WSL 无可执行权限，未获得 Linux/Emscripten/故障注入覆盖。本批没有新增测试、源码副本或修复。

Git 原始 blob 与当前源码在内存中经 Clang19 raw lexer 比较，kind/spelling/StartOfLine
在四叶源及同轮 Process 四源全部相同；不保存原始源码或 token dump。证据目录：
`E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/`，文件为
`leaf-libraries-before-pins.json`、`leaf-libraries-before-results.json`、
`leaf-libraries-final-r2-comment-equivalence.json`、`leaf-libraries-final-r2-results.json`。
check-batch 只校验 schema 与非空行锚点，独立 reviewer 另核调用方向、注释和遗漏。

## 冻结内容身份

最终 raw-lexer 证据 SHA-256：`f3b9c8b267da4e09d3455799976af307c9c43dd7353783353d455fc5655d1dae`。

| 文件 | SHA-256 |
| --- | --- |
| `zr_vm_lib_math/src/zr_vm_lib_math/quaternion/quaternion.c` | `6c1bb3dbcd7e782c1bd27a463758288173ad791294cbfba939a51392a0c4be39` |
| `zr_vm_lib_math/src/zr_vm_lib_math/quaternion/quaternion_registry.c` | `0251a3114a6b71ea8957f88acf2ae9fcdf117adfa7ba0a90d4e6eebff55ca743` |
| `zr_vm_lib_math/include/zr_vm_lib_math/quaternion.h` | `33d96d805e12de5985240ef9bab4b62b1cfcfba4cb3c5de57f402ea07aed1a90` |
| `zr_vm_lib_math/include/zr_vm_lib_math/quaternion_registry.h` | `0965f7057b903f78f1f76a84abeecf1f991b7fbb7e0ef33e64ee34c1c21a383b` |

台账 SHA-256：`3f4f6c2f6a29d891327fffaa5dfdd2e76913ac04dcda4bb2deabca66ebeca9ff`。
