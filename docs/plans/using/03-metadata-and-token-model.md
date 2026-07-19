# Using 03：Ownership/Close Metadata 与 Token

## Artifact contract

| 信息 | 保存位置 |
|---|---|
| resource category | Canonical type declaration flags |
| owner constructor/argument | Canonical TypeRef graph |
| move/copy/drop kind | TypeLayout + callable/type contract |
| Close/Dispose protocol | interface/capability token + callable contract |
| cleanup region | `.zri` Semantic IR/CFG；公开行为需要时进入`.zro` |
| runtime refcount/weak state | 不进入artifact |

token必须稳定定位resource type、Drop glue、Close member和generic owner instantiation。metadata不能只保存`"Unique<Foo>"`字符串，也不能从旧`%owned`标志恢复目标语义。

## Reflection

`typeof/typeid`能识别resource category与owner TypeRef，但`ConstructibleType.createInstance`拒绝resource class。reflection可查询Drop/Close metadata，却不能直接绕过owner构造与borrow checker取得裸resource实例。

## Loader/AOT

跨模块使用owner/resource时校验public contract hash、TypeLayout/drop kind和Close signature。AOT生成cleanup/Drop thunk；VM读取同一contract。schema mismatch必须在load/link阶段失败。

## 完成记录

[2026-06-21 ownership metadata baseline](./03-metadata/2026-06-21-ownership-metadata-baseline.md) 记录已有token/metadata证据；目标Canonical TypeRef和Close capability尚未完成。

## Schema 工作包

- **M1 type/owner contract**：resource category、owner TypeNode、copy/move/drop kind、constraints和public hash。
- **M2 Close contract**：protocol/interface token、resolved callable、receiver/throw/async effect与visibility。
- **M3 cleanup projection**：局部cleanup region保存在`.zri`；跨模块required Drop/Close contract进入`.zro`，不保存runtime strong count或loan stack。
- **M4 reflection/debug view**：可查询resource/owner/Close信息，但不暴露能伪造owner/refcount/runtime pointer的字段。
- **M5 compatibility**：source/binary/AOT writer-reader roundtrip，旧ownership flags只能由migration reader解释。

测试从metadata runtime、`tests/parser/test_aot_c_ownership_contracts.c`及`tests/acceptance/2026-06-26-aot-11-s4r-generated-ownership-offset-table.md`延续。新增negative覆盖unknown owner constructor、Drop/Close signature drift、stale layout/token、trimmed protocol和malformed cleanup section。

退出条件：VM/AOT/reflection/LSP只读取schema API；不从`Unique<T>`字符串恢复语义；跨模块hash不一致在执行前失败；旧writer不再产生目标artifact。
