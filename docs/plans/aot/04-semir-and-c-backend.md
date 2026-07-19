# AOT 04：Semantic IR 与 Native Backend

## Backend interface

backend接收已验证ExecIR、target TypeLayout和runtime ABI descriptor，输出object/module artifact；不得接触parser AST。

### AOT C

- C只是portable lowering target，不是语言类型系统。
- typed scalar可使用C local；address-taken、aggregate、ref-like和cleanup-sensitive value必须有稳定slot/address策略。
- generated helper/thunk按CallableContractHash与LayoutHash去重。
- exception/cleanup使用统一runtime ABI，不允许某些路径漏Drop或barrier。
- 生成C的source map可追到DebugMap location。

### AOT LLVM

- 与C backend共享ExecIR op和ABI classification golden。
- LLVM-specific attributes只来自已证明facts，例如nonnull/noalias/readonly/nounwind；不得根据surface keyword直接猜。
- GC safepoint、stack map、pin和write barrier必须通过runtime ABI显式建模。

## 调用类别

direct/static、virtual/interface、callable value、reflection invoke、native import分别具有明确callsite kind。函数声明返回用`:`、callable TypeRef用`->`只影响parser；backend看到的是同一个Canonical CallableContract。

## 验收矩阵

- scalar/aggregate/ref/owner参数和返回值。
- nested callable return、generic dictionary、virtual/interface dispatch。
- constructor四分：ValueConstruct、GcNew、OwnConstruct、reflection createInstance。
- native ABI success/failure与callback。
- debug/release、x64/arm64至少各一套layout golden。
- interp、binary-first、AOT C、AOT LLVM checksum和异常分类一致。

## 实施包与退出门

| 包 | 产物 | 验证 |
|---|---|---|
| A4.1 call/return ABI | direct/virtual/interface/callable/native callsite与return slot | scalar、aggregate、ref/out、nested callable、throw |
| A4.2 C lowering | typed locals、stable slots、helpers、cleanup labels、source map | generated C compile + project execution |
| A4.3 LLVM lowering |相同ExecIR op、attributes、GC/exception ABI | IR verifier + target execution |
| A4.4 runtime boundary | safepoint、barrier、invoke/native thunk、error normalization | GC pressure、callback、missing symbol、unwind |
| A4.5 parity/perf | backend checksum、allocation/boxing/thunk counters | reference fixture和benchmark guardrail |

现有证据入口包括`tests/acceptance/2026-06-06-aot-generic-call-direct-core.md`、`tests/acceptance/2026-06-06-aot-value-semir-field-module.md`及`tests/parser/test_aot_c_value_construction_guardrail.c`。新目标必须增加syntax reference v1的interp/binary/AOT C/AOT LLVM四路case，并对每个unsupported op输出IR kind、TypeId、target与source range。

退出条件：C与LLVM不读取AST；同一CallableContract得到一致ABI classification；native call使用artifact FfiSignature；关闭优化后仍语义一致，开启优化只凭facts添加nonnull/noalias/readonly等属性。
