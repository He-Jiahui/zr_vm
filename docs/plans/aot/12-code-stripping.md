---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 参照 hybridclr/mono/roslyn(runtime illink) 完善代码裁剪
  - decision: 2026-06-20 裁剪默认最小 + 注解保留（对标 NativeAOT/illink mark-and-sweep）
references:
  - lua/runtime/src/tools/illink/src/linker/Linker.Steps/MarkStep.cs        # mark-and-sweep 主循环
  - lua/runtime/src/tools/illink/src/linker/Linker/Annotations.cs           # marked_pending/processed 状态机
  - lua/runtime/src/tools/illink/src/linker/Linker.Steps/DescriptorMarker.cs # link.xml descriptor
  - lua/runtime/src/tools/illink/src/linker/Linker.Dataflow/ReflectionMarker.cs
  - lua/runtime/src/coreclr/tools/aot/ILCompiler.Compiler/Compiler/AnalysisBasedMetadataManager.cs
  - lua/hybridclr/libil2cpp/                                                # il2cpp + Unity linker 配合
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.h   # 现状：全量收集
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_callable_provenance.h
  - zr_vm_parser/src/zr_vm_parser/compiler/   # 编译流程入口
---

# 12 · 代码裁剪（mark-and-sweep 可达性 + 注解保留 + 泛型实例可达性）

> 承接缺口：**几乎完全缺失**。当前 AOT 全量生成（`backend_aot_function_table` 全收集，
> 仅有 `executable_subset`/`unsupported_instruction` 检查，无可达性分析、无 DCE、无体积统计、
> 无符号剥离）。本文按既定决策（**默认最小 + 注解保留**，对标 illink/NativeAOT）补齐裁剪管线，
> 它是 `08`(泛型实例)/`10`(反射级别)/`11`(元数据策略) 的**共同上游驱动**。

## 0. 定位：裁剪是统一上游

```
入口/导出/manifest 保留 + 注解
        │  mark-and-sweep 可达性（§1）
        ▼
   可达实体集合（函数/类型/字段/泛型实例/元数据）
   ┌──────────┬──────────┬──────────┬──────────┐
   ▼          ▼          ▼          ▼
 只生成     只生成      反射级别    元数据策略
 可达函数   可达 layout  (10§0)     (11§7)
 (AOT C)   + GC desc(09)
```

- 一次可达性分析，四处消费：决定 AOT 生成哪些函数、哪些类型 layout/descriptor、每实体反射级别、
  每实体元数据生成量。对标 illink `MarkStep` 一遍标记驱动后续 sweep/emit。

## 1. 可达性分析（mark-and-sweep，对标 illink MarkStep）

- **状态机**（对标 `Annotations`）：每实体三态 `unmarked / marked_pending / processed`，BFS 队列驱动。
- **根（roots）**：程序入口 `main`、模块导出（`SZrFunctionTopLevelCallableBinding` 导出位）、
  manifest 显式保留（§3）、注解保留目标（`10`§5）。
- **标记传播**（扫描函数体 SemIR / 字节码，对标 illink 扫 IL）：
  - 直接调用 → 标记被调函数；
  - 字段/类型使用 → 标记类型及其 layout；
  - 虚/接口调用 → 标记该接口所有**可达类型**上的覆盖实现（保守，对标 illink 虚方法处理）；
  - 泛型使用 → 收集并标记具体实例（§2）；
  - 反射点 → 经数据流分析标记（§4）。
- **依赖原因记录**（对标 `DependencyKind`）：每条标记记原因（DirectCall/FieldAccess/Virtual/
  XmlDescriptor/Reflection/Generic），供诊断与 trim 报告（§6）。
- 队列空 → 未标记实体即死代码，sweep 阶段不进入 AOT 产物与元数据。

## 2. 泛型实例可达性（衔接 08）

- 裁剪与 `08`§3 实例化收集是同一遍：标记到泛型使用点时，按实参类型收集具体实例，加入可达集。
- 传递闭包：实例内部用到的其它泛型实例递归标记（对标 mono full-AOT transitive closure、
  NativeAOT `ExactMethodInstantiationsNode`）。
- 引用类型实例共享代码（`08`§1）→ 只需保留一份共享函数 + 各实例的泛型字典；值类型实例逐份保留。
- 运行期动态实例（反射 `MakeGenericType`）：默认不静态保留 → deopt 解释器（`08`§6）；
  若 manifest/注解声明 → 强制收集保留（对标 link.xml 预声明动态泛型实例）。

## 3. 保留规则 manifest（对标 link.xml descriptor，落在 zrp manifest 11§8）

```
preserve {
  type   "Foo"            all       # 类型 + 全部成员 + 完整元数据（10§0 DESCRIPTION）
  type   "Bar"            methods   # 仅方法
  method "Baz.run"                  # 单个成员
  generic "List" <"Foo">            # 预声明动态泛型实例
  feature "EnableX" = true { ... }  # feature switch 条件保留
}
```

- 由 `DescriptorMarker` 等价物解析，把声明项加入根集（对标 illink XML 驱动标记）。
- `feature/featurevalue` 支持条件裁剪（对标 illink feature switch）：按构建配置选择性保留。

## 4. 反射数据流分析（对标 illink FlowAnnotations / ReflectionMarker）

- `@dynamically_accessed(MemberTypes)`（`10`§5）标注参数/返回 → 追踪「哪些类型流向该反射点」，
  保留其相应成员（对标 `DynamicallyAccessedMembers` 流分析）。
- `@dynamic_dependency` → 直接把目标加入根（对标 `[DynamicDependency]`）。
- 无法静态决议的反射（`ResolveToken(动态值)`、按运行期 string 查类型）→ 该点标记为「unanalyzable」，
  产 trim 警告（§6），目标默认不保留（除非注解/ manifest）。

## 5. sweep 与产出收窄

- **AOT 函数**：只对可达函数发 C（取代 `backend_aot_function_table` 全量收集 → 可达过滤）。
- **类型 layout / GC descriptor（09）**：只为可达类型生成。
- **元数据（11）/ 反射（10）**：按可达性 + 级别生成，名表/签名 blob 中不可达项不写入池。
- **符号剥离选项**：release 模式可把生成 C 的函数/类型符号名替换为短稳定 ID（`zr_fn_<hash>`），
  仅保留 manifest 导出与反射 `DESCRIPTION` 级所需名字（对标 release 名称剥离）。

## 6. 裁剪诊断（trim warnings）与体积统计

- **trim 警告**（对标 illink/NativeAOT trim analyzer）：unanalyzable 反射、`@requires_unreferenced_code`
  调用点、被裁剪但运行期可能需要的目标 → 编译期警告，列出依赖原因（§1）。
- **体积统计**：报告每函数/类型/元数据在产物中的字节占用、裁剪前后对比（补现状「无体积统计」缺口）。

## 7. hybrid 安全网

- 裁剪是「typed/AOT 产物」的瘦身；**解释器 + 完整数据元数据（`11`）始终是兜底**：被裁剪的 typed
  目标若运行期被需要 → deopt 到解释器动态执行（`04`§6 / `08`§6）。
- 「full-AOT 模式」（`08`§6）关闭 deopt 兜底 → 裁剪必须证明闭合，否则编译期报错（对标 mono full-AOT）。

## 8. 落地切片

| 切片 | 内容 | 验收 |
|------|------|------|
| 12-S1 | 可达性分析引擎（状态机 + BFS + 依赖原因）（§1） | 标记集合正确；死函数不进产物 |
| 12-S2 | AOT 生成接入可达过滤（取代全量收集）（§5） | 仅可达函数发 C；体积下降可测 |
| 12-S3 | 泛型实例可达性（与 08-S1 合一）（§2） | 泛型实例集合 = 静态可达闭包；动态实例 deopt |
| 12-S4 | manifest 保留规则 + feature switch（§3） | 声明项保留；条件裁剪按配置生效 |
| 12-S5 | 反射数据流分析 + 注解标记（§4，衔接 10-S5） | 注解反射目标保留；未注解给警告 |
| 12-S6 | 元数据/反射级别按可达性收窄（衔接 10/11）（§5） | 默认产物最小；token 通道仍可用 |
| 12-S7 | trim 警告 + 体积统计；符号剥离选项（§6/§5） | 诊断准确；release 符号可剥离 |
| 12-S8 | full-AOT 闭合校验（§7） | 开启后裁剪不闭合即报错 |

## 9. 不变量校验

- **C 单一真相**：裁剪决策基于唯一 token/layout 图；可达性结果是 `08`/`10`/`11` 的共同输入，不各自重算。
- **D 环境隔离**：裁剪不改变 typed 函数体形态（`07`），只决定「生成哪些」，不影响「怎么生成」。
- hybrid 安全：默认保留 deopt 兜底，裁剪激进但不致运行期不可恢复的缺失（除显式 full-AOT）。
