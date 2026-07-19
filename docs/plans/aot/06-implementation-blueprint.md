# AOT 06：分层实施路线

## 顺序

1. **Foundation**：冻结Canonical TypeRef、Place/Value IR、CFG facts和artifact schema；writer/reader先roundtrip。
2. **Typed execution**：让VM与AOT共同消费Semantic/ExecIR，先覆盖scalar，再aggregate/ref/owner。
3. **Control/cleanup**：统一exception、finally、using、Drop和partial construction edge。
4. **ABI/backend**：完成call frame、C/LLVM ABI、generic dictionary与native FfiSignature。
5. **Runtime bridges**：GC maps/barrier、ownership、reflection、pooling与debug map。
6. **Packaging/stripping**：ModuleIdentity、`.zrm`、metadata roots、reachability与裁剪。
7. **Convergence**：删除AST/旧指令/字符串type-name fallback，运行全消费者parity。

## 每阶段工作法

- 先添加artifact/IR golden和一个预期失败的focused test。
- 实现最低共享能力，不在backend增加具体fixture名字特判。
- 先跑leaf unit，再跑parser/core/module，再跑project/CLI。
- 完成后将可复用证据写入对应`plan-id/<detail>.md`；正文只更新状态和链接。

## Promotion gate

每个阶段必须同时满足：schema version明确、旧reader迁移策略明确、VM/AOT parity、diagnostic可定位、debug verifier可检查、性能指标有baseline。仅通过AOT C单一路径不能晋级共享contract。

## 与活动实现的关系

已有typed register/loop/reflection/metadata工作归入阶段2、4、5；它们是输入baseline。只有接入Canonical TypeRef/Place/ModuleIdentity/FfiSignature并通过本计划矩阵后，才算目标AOT完成。

## 里程碑账本

| 里程碑 | 硬依赖 | 退出artifact/行为 | 不得提前宣称 |
|---|---|---|---|
| M1 foundation | syntax 01 schema冻结 | canonical type/place/CFG writer-reader golden | 新语法可执行 |
| M2 typed execution | M1 + 02/03 | VM/AOT scalar+aggregate+ref parity | owner/reflection完整 |
| M3 cleanup | M2 + syntax 04/using semantic contract | normal/abrupt exact cleanup | using surface冻结 |
| M4 ABI/backend | M2/M3 + target layouts | C/LLVM call/return/native ABI parity | 跨target layout兼容 |
| M5 runtime bridges | M3/M4 | GC/owner/reflection/pooling/debug contracts | full trimming/package closure |
| M6 package/metadata | M1-M5 + syntax 10 | ModuleIdentity/FfiSignature/DebugMap `.zro/.zrm` | source/binary全等价 |
| M7 convergence | M1-M6 + syntax fixture | 四backend+artifact+LSP/debug纵向矩阵 | 全仓完成（除非全矩阵证实） |

每次晋级记录必须包含：变更schema/hash、leaf target及结果、四backend差异、未运行矩阵及原因、性能计数、仍open边界。一个工具链或一个fixture通过只能记录为sub-milestone。

测试顺序固定为canonical type/CFG leaf、core/runtime、artifact reader-writer、backend target、project/CLI和consumer parity；任一上游失败时不得用上层smoke覆盖结论。
