# ZR AOT 重设计计划索引

> 状态：按目标语法 contract 重写；已有实现只作为 baseline，不代表新 contract 已完成。
>
> 语言权威：[syntax 计划](../syntax/README.md)。AOT 不定义语法、TypeRef、ownership 或 module 语义。

## 目标

AOT 的职责是把同一份 Canonical TypeRef、TypeLayout、Place/Value IR、CFG facts 与 Semantic IR 确定性降低为 native code，并与 VM 保持可观察语义一致。禁止从旧 AST 拼写、`%xxx`、类型名字符串或 runtime object shape 反推语义。

## 依赖顺序

| 阶段 | 文档 | 核心交付 |
|---:|---|---|
| 0 | [当前状态](./00-current-state.md) | baseline、缺口与重写边界 |
| 1 | [设计原则](./01-design-principles.md) | backend-neutral contract 与 parity gate |
| 2 | [类型与布局](./02-typed-value-and-layout.md) | Canonical TypeRef、ABI/Layout、GC maps |
| 3 | [Semantic IR](./03-instruction-set-refactor.md) | Place/Value/CFG/cleanup 到 ExecIR |
| 4 | [C/LLVM backend](./04-semir-and-c-backend.md) | target lowering、thunk 与异常路径 |
| 5 | [ownership/GC bridge](./05-ownership-gc-and-bridge.md) | Drop、barrier、pin、owner/GC bridge |
| 6 | [总实施路线](./06-implementation-blueprint.md) | 分层里程碑与验收顺序 |
| 7 | [寄存器与调用 ABI](./07-codegen-register-model-and-environment-isolation.md) | typed register、call frame、environment |
| 8 | [泛型共享](./08-generic-sharing.md) | canonical instantiation 与 dictionary |
| 9 | [内存管理](./09-memory-management.md) | allocation/stack map/pool/no-scan |
| 10 | [反射](./10-reflection.md) | token roots、dynamic construction、invoke |
| 11 | [metadata/module/FFI](./11-metadata.md) | artifact schema、ModuleIdentity、FfiSignature |
| 12 | [裁剪](./12-code-stripping.md) | 可证明可达性与 metadata preserve |

## Syntax Contract 投影

| Syntax设计 | AOT责任 | 主计划 | 必须产物 |
|---|---|---|---|
| 01 Canonical TypeRef/Place/CFG/artifact | 只消费规范类型、Place/Value、flow与schema | 02、03、11 | Type/Layout table、ExecIR、versioned artifact |
| 02 ref/in/out/scoped/readonly | 将已验证passing/region/receiver contract降为ABI | 03、05、07 | call frame、ref provenance、cleanup edge |
| 03 struct/ref struct/Span/layout | 计算target layout、aggregate ABI、stack/ref-like限制 | 02、04、09 | LayoutHash、GC map、bounds/provenance ops |
| 04 resource/owner/Drop/GC bridge | 生成copy/move/drop glue、root/barrier/bridge | 05、09 | ownership map、Drop thunk、GC stack map |
| 05 property | 降低get/set/ref-get及显式field Place | 03、04、10 | PropertyContract、call/Place lowering |
| 06 migration | 拒绝旧artifact writer与旧IR fallback | 06 | schema migration、parity与删除清单 |
| 07 reference fixture | 四backend纵向验收 | 06 | checksum、异常、artifact golden |
| 08 reflection | 保留token/roots并生成construct/invoke thunk | 10、11、12 | reflection table、binder/invoker、preserve roots |
| 09 pooling | 按TypeLayout支持slab与guarded direct ref | 05、09 | GcScanKind、StableSlot/guard contract |
| 10 native/module/package | 降低ModuleIdentity、`.zrm`和FfiSignature | 04、11、12 | dependency/native import table、AOT thunk |

任何上游contract仍为`surfacePending`时，AOT计划只能冻结IR/ABI语义，不能自行发明源码拼写。

## 每个实现里程碑的必填证据

1. 明确上游schema版本、输入TypeId/PlaceId/SymbolId及不接受的旧输入。
2. 列出新增或修改的IR/artifact/runtime ABI，不以“backend支持”笼统代替。
3. 覆盖成功、边界、失败、异常cleanup和跨模块/target不兼容。
4. 先通过leaf/core测试，再通过source、binary-first、AOT C、AOT LLVM项目矩阵。
5. 记录性能计数与允许的runtime check；未经CFG/layout证明不得声称消检。
6. 完成记录必须写明验证命令/结果和未完成边界，并链接回本目录稳定plan-id。

## 共同晋级门

- interp、binary-first、AOT C、AOT LLVM 对同一 fixture 输出一致。
- backend 只消费 versioned artifact/IR；没有 AST token 或 `%xxx` 分支。
- LayoutHash、CallableContractHash、ModuleIdentity、FfiSignature 在 writer/reader/backend roundtrip 后一致。
- borrow、move、readonly、out definite assignment、ref struct escape 在 AOT 前已静态完成；AOT 只实现已验证的操作与必要 runtime guard。
- 所有完成结论写入对应 `plan-id/detail.md`，计划正文只链接，不追加滚动执行日志。

## 完成记录

可复用 baseline 已迁移到 `02-type-layout/`、`07-codegen/`、`08-generics/`、`09-memory/`、`10-reflection/`、`11-metadata/`、`12-stripping/`。记录中的“完成”只对其 evidence scope 成立。
