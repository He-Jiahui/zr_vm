# 05-M1 Unified AST/Symbol 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md` 的
`M1 Unified AST/Symbol`，执行清单：
`docs/plans/syntax/05-property-unified-ast/m1-unified-ast-symbol-implementation-plan.md`。

## 状态与产出记录

- 完成时间：2026-07-23 07:12 +08:00
- 状态：completed
- 完成项目：
  - 已让 class、struct、resource class、interface 共用 contextual `property` grammar，并统一为
    `ZR_AST_PROPERTY_DECLARATION` + 有序 `ZR_AST_PROPERTY_ACCESSOR` children。
  - 已覆盖 bodyless、expression、block accessor，`get/set/init`、visibility、static、恢复边界；
    parser 不再生成 legacy class/interface property AST。
  - 已绑定一个可见 PropertySymbol，持有 canonical value TypeId、property identity 与 getter/
    setter/init accessor SymbolId；accessor callable 保留结构化 role 和 receiver effect。
  - 已用 exact linked SymbolId 驱动 property read/write lookup；hidden accessor name 只作为
    runtime dispatch payload和旧 artifact/native descriptor 兼容读取，不是 source semantic fact。
  - 已拒绝 duplicate accessor、`set+init`、visibility widening、concrete bodyless、interface body
    以及手工 legacy property node。
  - 已迁移仓库内 source property fixtures，并保持 ordinary identifier `value` 边界。
  - 已让 serialized visible property 与 core reflection/runtime decorator 消费结构化 member type、
    property identity、type 和 metadata，`members.name[0]` 不再先暴露伪 method。
  - 已让 source-import metadata bridge 保留 serialized `propertyIdentity/accessorRole`；直接 accessor
    SymbolId 不可用时只按同一 property identity 与结构化 role 解析，不回退到 hidden name。
  - 已关闭审查发现的三条边界：普通 `__get_*` method 不形成反射 property、缺 getter/setter
    不回落普通 member opcode、缺 property `}` 不吞后续成员或重复诊断。
  - 已在 `HEAD=498c791 + 70 exact paths` 冻结快照完成 GCC/Clang/MSVC promotion：property
    16/16、parser 75/75、receiver 28/28、canonical consumers 16/16、semantic query 27/27、
    compiler integration 127/127、debug metadata 4/4、decorator reflection 4/4，全部真实 exit 0。
  - 已在三套工具链分别运行 `classes_properties.zrp` source CLI smoke，均真实 exit 0 并输出 `40`。

## 当前实现边界

- M2 负责显式 field、`let/var`、init phase、layout/token 与 partial construction。
- M3 负责完整 typed accessor lowering、compound assignment、virtual/interface/static dispatch 和
  receiver 单次求值/副作用顺序。
- M4 负责 `ref`/`ref readonly` getter、Place/region/escape 和 managed interior ref。
- M5 负责 LSP、source/binary reflection parity 与迁移体验；LSP 必须消费 PropertySymbol/query，
  不能扫描 accessor 名称重建。

## 验收结论

- 70 个 exact paths 从工作树复制到 `.codex/syntax05-m1-acceptance` 后逐文件 SHA-256 mismatch=0；
  gitlink 依赖只作为只读构建输入补入快照，不进入 milestone commit。
- 三工具链矩阵及 CLI smoke 均通过；最终 `git diff --check` 无 whitespace error，共享 index
  在 staging 前保持为空。
- LSP 路径、三份既有 dirty Syntax 草案、build/log 与根目录生成物均不属于本里程碑写集。
