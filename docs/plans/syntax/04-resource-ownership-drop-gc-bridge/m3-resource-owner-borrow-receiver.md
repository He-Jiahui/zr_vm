# 04-M3 owner borrow/receiver 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md`
的 `M3 owner borrow/receiver`。

## 状态与产出记录

- 完成时间：2026-07-21 22:16 +08:00
- 状态：已完成（M3 晋级门）
- 已完成项目：
  - 已用 canonical ownership shell 与 inner TypeId 建立 `in T` owner reborrow；仅
    `Unique<T>` / `Shared<T>`、mode=`in`、parameter qualifier=`NONE` 且 inner contract
    匹配时成立，value/out/ref/generic/name 路径不放宽。
  - 已建立 `Unique<T>` readonly/writable receiver 与 `Shared<T>` readonly receiver；Weak
    direct/optional target access 由 structured receiver-guard fact 单次 wake，direct 失效时抛
    `NullReferenceError`，optional 失效时跳过完整后缀，不按 member name/source text 补偿。
  - 已让 direct owner receiver 与 `in T` 参数投影同一 canonical Place/LoanId facts；nested
    readonly 可共存，Shared writable 与 active shared loan 下的 mutable receiver 拒绝。
  - 已让 Place-to-result 指令消费 reaching Place loans，使 local ref 的 later load 延长 owner
    loan 到真实 last use；无 later use 时 NLL 允许后续 move。
  - 已把 drop、Unique move、share 统一分类为 active loan 下的 exclusive owner access；五个
    source ownership operation 由 dedicated `OwnershipIntrinsicFact` 与 canonical PlaceId 判定，
    不按 opcode message、member name 或 source text 推断。
  - 已让 direct source TypeDef 的 ref-return member result 继承 receiver Place/escape bound，
    阻止返回超过 owner borrow；external/inherited/chained unavailable 保持保守边界。
  - 已消除 source TypeDef 内同名 overload 的声明顺序依赖；value-return overload 不会遮蔽
    后续 ref-return overload，mixed overload 在 resolved identity 不可用时保守视为 receiver-tied。
  - 已冻结 runtime 只镜像已通过的 compile-time contract；Borrow/Loan runtime table 不再是
    correctness dependency。

## 最终验证结果

- GCC 11.4 与 Clang 14.0 使用独立 build 目录串行完成同一 18-target 矩阵；每套
  18/18 真实进程 `exit 0`，Unity 合计 593/593、0 failure。
- MSVC 19.44 串行完成 M3 主矩阵 17/17、Unity 534/534、0 failure；同一 native fast-path
  可执行文件另以隔离进程完成 59/59、真实 `exit 0`。
- focused 结果：resource owner borrow 6/6、pre Semantic IR 10/10；expanded evidence 同时
  覆盖 M1/M2 resource、parser/type inference/compiler、canonical consumer、reference、
  dataflow、closure、exception、AOT ownership/scope、GC 与 native fast path。
- 三工具链并行执行 compiler integration 曾因共享固定名 artifact 产生不同随机失败，整轮
  明确作废；改为工具链串行后 GCC/Clang compiler integration 均 127/127，MSVC 亦 127/127。
- MSVC 将 native fast-path 接在完整 17-target 序列之后时，曾在该独立 core 测试第 25 项
  触发 `function.c:3146` Debug 断言等待；该序列未计入通过证据。目标隔离重跑 59/59，且
  M3 未修改 core/native fast-path 路径，因此记录为后续 core 稳定性风险而非 M3 晋级声明。

## 当前实现边界

- direct receiver ref-return provenance 仅对 declared inner type 可定位的 source
  class/struct TypeDef 与同名 direct reference-return declaration 成立；同名 overload
  保守合并，external descriptor、inherited/chained receiver 仍返回 unavailable。
- M3 不包含 M4 `GcDomain`、`Gc<T>`/`GcBox<T>` bridge、cross-domain write gate 或
  no-hidden-ignore-registry 晋级门。

## 2026-08-26 ownership/object member separation 收敛

- source ownership control 现只保留 `share(owner)`、`degrade(shared)`、`wake(weak)`、
  `intoGc(owner)` 与 `drop(owner)` 五个 reserved intrinsic；`.` 与 `?.` 只执行 target
  access，同名对象成员继续走普通 member lookup/call。
- Weak guard 在整个 dominated suffix 中只 materialize 一次 hidden Shared owner；成功、null、
  throw 与 scope cleanup 均释放该 owner。Weak receiver 产生的 method/property ref-like result
  不能逃逸这个临时 owner。
- GCC 11.4、Clang 14 与 MSVC 19.44 在提交 `3a36ddf` 的相同 SHA-256 source fence 下直接通过
  ownership/member separation 32/32 与 owner/borrow receiver 7/7，所有进程 exit 0。
- production parser C/H 对已移除 percent-prefixed ownership/source forms 为零匹配；旧 ownership
  member semantic selector为零匹配。结构化迁移诊断只在真实成员查找失败后发布 fix，不选择
  ownership typing 或 lowering。
