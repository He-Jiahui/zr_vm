# 05-M3 Access Lowering/Receiver Effect 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md` 的
`M3 Access lowering/receiver effect`，执行清单：
`docs/plans/syntax/05-property-unified-ast/m3-access-lowering-receiver-effect-implementation-plan.md`。

## 状态与产出记录

- 完成时间：2026-07-23 16:37 +08:00
- 状态：completed
- 完成项目：
  - 已从总设计提取 typed get/set/init、single-evaluation compound assignment、receiver effect、
    virtual/interface/static/inherited dispatch 和 VM/AOT promotion gate，并按 Task 1 到 Task 6 顺序完成。
  - 编译器现在先解析 visible PropertySymbol，再按 linked accessor SymbolId/role 选择 getter/setter/init；
    hidden accessor 仍不可由源码直接调用，static/readonly/init capability 保持 M2 合同。
  - compound property assignment 形成单一 lowering plan：receiver、getter、RHS、operator、setter 各执行
    一次且顺序固定；missing getter/setter 在写入前拒绝，assignment expression 返回计算值。
  - inline struct receiver 保留 source frame base/slot Place provenance；runtime 的 generic/cached meta get/set
    和 VM precall 可借用原 frame slot，mutable setter 精确写回，class/owner/static 路径保持既有行为。
  - runtime property descriptor、call-info/stack anchor 和 exception boundary 已收口：getter throw 阻止 RHS，
    RHS throw 阻止 setter，setter throw 只发生在前序成功之后，receiver cleanup 恰好一次。
  - virtual、interface、inherited 与 static access 均由 structured descriptor/accessor identity 驱动；
    inherited 测试显式 `super()` 初始化 base state，不改变既有构造器继承语义。
  - executable IO source patch 已推进到 36，writer/reader roundtrip `vmEntryClearStackSizePlusOne`；
    source/binary function 的 stack boundary、member entries、instruction bytes 与执行结果一致。
  - `classes_properties.zrp` 已切换到 instance/static compound property，三工具链 CLI 均 exit 0 并输出 40。
  - 已新增 core runtime property accessor dispatch、type inference、artifact schema、category index 与 acceptance
    文档；`ref`/`ref readonly` getter、Place/region/interior-ref 明确保留给 M4，LSP parity 保留给 M5。

## 当前实现边界

- M2 已提供 visible PropertySymbol、linked accessor SymbolId、structured role、receiver effect、init phase
  和严格 single-use field initialization capability；M3 必须消费这些事实，不能恢复 hidden-name lookup。
- M3 只完成普通 value property access；`ref`/`ref readonly` getter、Place/region/managed interior ref 属于 M4。
- LSP consumer parity 属于 M5，M3 不修改 language-server 路径或增加文本 fallback。

## 2026-08-26 ownership/object member separation 收敛

- `.` 与 `?.` 现只表示 target member/property/call access；ownership control 只由五个
  reserved intrinsic 的 dedicated AST/fact 路径承载。成员名为 `share`、`degrade`、`wake`、
  `intoGc` 或 `drop` 时仍执行普通 visible-member lookup 和 dispatch。
- optional access mode 由 postfix segment 与 chain-level `ReceiverGuardFact` 表示。receiver
  expression只求值一次；guard 失败跳过 property getter、computed-index expression、callable
  suffix 与 arguments，且不改变 existing single-evaluation property lowering contract。
- 当前 GCC 11.4 focused runner 直接通过 35/35，其中新增 live/expired Weak 的 receiver
  single-evaluation、computed-index skip 与 throwing property-getter skip 回归。Clang/MSVC 和
  stable-HEAD full graph 仍属于 ownership separation 最终验收，不改写本记录原 M3 晋级结论。

## 验收结论

- focused `zr_vm_property_access_lowering_test` 在 GCC/Clang/MSVC 均为 22/22，最终同字节二次复跑均
  真实 exit 0。22 个 code/test/fixture 路径 SHA-256 mismatch=0。
- GCC/Clang 父矩阵为 property M1 16/16、M2 21/21、receiver 28/28、canonical 16/16、semantic query
  27/27、parser 75/75、literal 57/57；VM/AOT-facing 为 compiler 127/127、known-native 61/61、debug
  4/4、decorator 4/4、layout 9/9、artifact 14/14、AOT call 8/8，全部真实 exit 0。
- MSVC 同样通过 focused、父矩阵、compiler/debug/decorator/layout/artifact/AOT；独立 known-native 目标仍在
  既有 stack-root set-by-index Debug assert 以 `0x80000003` 停止，本里程碑不宣称该无关基线 GREEN，
  也未越界修改 `function.c` 的既有断言合同。GCC/Clang 已覆盖完整 61 项。
- `git diff --check` 通过，shared index 在 staging 前为空，M3 exact ownership 为 30 paths，LSP、3 份
  外部 dirty Syntax 草案、build/log/generated artifacts 均明确排除。
