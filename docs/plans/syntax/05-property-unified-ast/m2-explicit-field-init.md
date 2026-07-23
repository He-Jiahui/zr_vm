# 05-M2 Explicit Field/Init 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md` 的
`M2 Explicit Field/Init`，执行清单：
`docs/plans/syntax/05-property-unified-ast/m2-explicit-field-init-implementation-plan.md`。

## 状态与产出记录

- 完成时间：2026-07-23 11:52 +08:00
- 状态：completed
- 完成项目：
  - 已追加稳定 `let` token，并统一 local、member、classic-for、foreach 的 `let`/`var`
    immutable/mutable AST fact；保留 `var const` 兼容输入。
  - 已引入 structured initialization phase，构造函数和 `init` accessor body 分别进入明确阶段，
    ordinary callable 与 nested lambda 不继承该能力。
  - 已按 visible PropertySymbol linked accessor identity 与 exact role 选择 `INIT`/`SET`，拒绝
    ordinary method、foreign receiver 与 static property 消费 init capability。
  - 已冻结 immutable explicit field 的 declaration/direct-this exactly-once 初始化、compound/
    repeat/foreign write 拒绝，以及 shallow handle 与 inline struct subfield 边界。
  - 已证明只有 explicit fields 拥有 field token、layout offset、reflection field row 和 constructor
    initialization bitmap；property/accessor 不产生 storage，也不按名称关联 field。
  - 已完成 source/runtime reflection 与 `.zro` byte-exact prototype roundtrip focused GREEN。
  - 已让 constructor-before-property 的前向访问消费预绑定 PropertySymbol，class/struct 以 fields、
    ordinary methods + properties、meta functions 三阶段绑定，并在绑定后恢复 source declaration order。
  - 已把 immutable field initialization 与 property initializer call 作为不同 member-entry flags
    序列化；generic/slot/meta runtime 只消费该 provenance，不把 PIC 热度或名称当语义。
  - 已关闭审查边界：所有 `let` 解构绑定均不可重写，field writer 不再转换 embedded identifier
    指针，binding range 从 exact keyword 开始，hidden accessor 不进入普通 call candidate。
  - 已把 heap/inline initialization capability 收窄为 exact immutable non-static single-use field；
    repeated、writable、property、static 与 dynamic target 均拒绝，constructor throw 进入 partial unwind。
  - 已在 staged member binding 后稳定恢复 compiled member source order，并保留 accessor 对前置普通
    method 的 typed 参数/receiver contract。
  - 已让 constructor definite assignment 通过 visible PropertySymbol、linked init accessor SymbolId
    与 declarationOrder 投影 accessor 的 direct immutable-field write effect；accessor-only 初始化成功，
    所有正常 return 与 fallthrough 取交集，direct + accessor 重复在编译期拒绝。
  - 已从普通 call/member-reference/type-inference lookup 完全排除 hidden accessor，直接调用、裸引用
    与 alias/dynamic call 均不能绕过 visible property contract。
  - 已在 byte-exact `739efc5 + 48 M2 paths` 快照上完成 GCC/Clang/MSVC 同一 13-target
    promotion：每套均为 focused 21/21、property 16/16、parser 75/75、literal 57/57、
    receiver 28/28、canonical 16/16、semantic query 27/27、layout 38/38、debug 4/4、
    decorator 4/4、object fast path 61/61、compiler integration 127/127、AOT contract 1/1，
    所有进程真实 exit 0。
  - 已在三套工具链分别运行 `classes_properties.zrp` source CLI smoke，均真实 exit 0 并输出
    `40`；48 个 snapshot 文件逐项 hash/cmp mismatch=0，forbidden LSP/外部 Syntax 草案/build=0。

## 当前实现边界

- `init` accessor 的 direct immutable-field 写集通过 visible property/link identity 参与 constructor
  definite assignment；不会按 property/field 名称推断 backing-field 关联。跨 direct field +
  init-property 的重复写在编译期拒绝，runtime single-use capability 继续作为纵深门禁。
- M3 负责完整 typed accessor lowering、compound assignment、virtual/interface/static dispatch、
  receiver 单次求值与副作用顺序。
- M4 负责 `ref`/`ref readonly` getter 的 Place/region/escape 和 managed interior ref。
- M5 负责 LSP 与 source/binary reflection consumer parity；consumer 必须读取 canonical facts。

## 验收结论

- M2 已完成三工具链同快照 promotion、VM/AOT/source CLI smoke、最终只读复审和 exact ownership
  audit；允许 exact-stage 并提交 `feat(syntax): establish explicit property fields`。
- `zr_vm_execution_member_access_fast_paths_test` 在纯 `739efc5` 与 M2 overlay 上均命中同一既有
  `gc_mark.c:1099` closure teardown crash，未作为 M2 通过证据或晋级目标。
