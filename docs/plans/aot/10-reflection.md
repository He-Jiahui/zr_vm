---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 参照 hybridclr/mono/roslyn 完善反射
  - decision: 2026-06-20 反射默认最小 + 注解保留（对标 NativeAOT/illink）；分三级 None/RuntimeMapping/Description
references:
  - lua/hybridclr/libil2cpp/vm/Reflection.h          # invoker_method / 反射对象缓存
  - lua/hybridclr/libil2cpp/il2cpp-class-internals.h   # MethodInfo.invoker / Il2CppMethodPointer
  - lua/mono/mono/metadata/reflection.c               # 完整动态反射（备选，未采用为默认）
  - lua/runtime/src/coreclr/tools/aot/ILCompiler.Compiler/Compiler/AnalysisBasedMetadataManager.cs   # MetadataCategory 三态
  - lua/runtime/src/tools/illink/src/linker/Linker.Dataflow/FlowAnnotations.cs   # DynamicallyAccessedMembers
related_code:
  - zr_vm_core/include/zr_vm_core/reflection.h         # TypeOfValue / BuildTypeLiteralObject / 成员反射
  - zr_vm_core/src/zr_vm_core/reflection.c             # 缓存 + PIN
  - zr_vm_core/include/zr_vm_core/object.h             # SZrMemberDescriptor / prototype
  - zr_vm_core/include/zr_vm_core/metadata_token.h
---

# 10 · 反射（分级元数据 + invoker thunk + 注解保留）

> 承接缺口：zr_vm 已有 `ZrCore_Reflection_TypeOfValue`、`BuildTypeLiteralObject`（按名查类型）、
> 成员查询（`SZrMemberDescriptor` + `FindMemberDescriptor`，含继承）、协议检查、反射对象缓存与 PIN。
> **缺**：按 token 反射、泛型参数反射、字段 offset 反射、统一动态调用入口（invoker）、
> 反射与裁剪的隔离。本文按既定决策（**默认最小 + 注解保留**）补齐，并与 `11`/`12` 联动。

## 0. 核心决策：反射三级（对标 NativeAOT MetadataCategory）

每个类型/方法/字段的反射能力分三级，由裁剪（`12`）可达性 + 注解决定，**默认按可达性取最小**：

| 级别 | 含义 | 产出 | 默认 |
|------|------|------|------|
| `NONE` | 无反射元数据 | 不可被反射发现 | 未被反射可达的实体 |
| `RUNTIME_MAPPING` | 可动态调用 / 查表，但不可完整枚举 | invoker thunk + token↔实体映射 | 被间接调用/泛型字典引用的实体 |
| `DESCRIPTION` | 完整签名，可枚举成员 | 完整元数据（参数名/类型/特性/offset） | 被注解或 manifest 显式要求 |

> 默认产物最小：未被注解、未被反射可达的类型只生成执行所需的 layout/函数，**不**带反射元数据。
> 这是「极致性能 + 小体积」与「按需反射」的平衡（对标 NativeAOT，反 il2cpp/mono 的默认全保留）。

## 1. 统一动态调用入口（invoker thunk，对标 il2cpp invoker_method）

反射调用（`Method.Invoke` 等价）需要把「`SZrValue[]` 参数 → 调具体 typed C 函数 → 打包返回」。
**这正是 `07`§6 的边界 marshaling**，复用而非新建：

```c
/* 每个 RUNTIME_MAPPING+ 的 typed 函数生成一个 invoker，签名统一 */
typedef void (*TZrAotInvoker)(TZrAotFunctionPointer target,
                              const SZrAotMethodInfo *method,
                              SZrTypeValue *self,        /* 实例方法的接收者，静态则 NULL */
                              SZrTypeValue *args,        /* 入参数组（dynamic 表示） */
                              SZrTypeValue *outReturn);  /* 返回打包目标 */
```

- invoker 内部：按 `method->signature` 把 `args[i]` unbox 成 typed 寄存器（`07`§6 入参解包）→
  调 `target` 的真实 C 函数 → 把返回寄存器 box 回 `outReturn`（返回打包）。
- invoker 按**签名分桶生成**（相同 C 签名共享一个 invoker，对标 il2cpp 按签名生成 invoker），
  避免每方法一份导致膨胀。
- 入口表登记到代码注册表（`11`§2），token/反射对象据此找到 invoker（对标 il2cpp `invokerPointers[]`）。

## 2. token 驱动的反射解析（衔接 11）

现状只能按 string 名查类型；补 token 通道（对标 mono token→entity、il2cpp index→entity）：

- 反射 API 增 `ZrCore_Reflection_ResolveToken(metadataToken) -> 运行期实体`，经 `11` 的
  `SZrMetadataRuntime`（MetadataCache 等价）lazy 解析 token → 原型/方法/字段。
- string 名查找改为「名→token→实体」两段，token 是单一真相（不变量 C），名表可被裁剪（`12`）。

## 3. 字段 offset / 布局反射

- `DESCRIPTION` 级类型暴露字段偏移：直接读唯一 `SZrTypeLayout.fields[i].byteOffset`（`02`/`11`），
  **不**在反射层另存偏移（不变量 C）。
- 反射读写字段值 = 在边界处按 offset 构造/解构 `SZrValue`（`07`§6），typed 内部仍是 `.`/偏移。

## 4. 泛型反射（衔接 08）

- 暴露泛型实例的类型实参：反射对象记 `baseToken + argTokens[]`（来自 `08`§3 实例化表）。
- `MakeGenericType`/运行期构造泛型 → 若实例已静态收集（`08`§3）返回其原型；否则解释器动态实例化
  （`08`§6 deopt），动态实例反射级别为 `RUNTIME_MAPPING`。
- 泛型参数约束（`08`§4）在 `DESCRIPTION` 级暴露。

## 5. 反射与裁剪的注解（对标 DynamicallyAccessedMembers / DynamicDependency / RequiresUnreferencedCode）

在 zr 语言层引入保留注解，驱动 `12` 的标记，使「静态裁剪 + 动态反射」共存：

- `@reflectable(members: methods|fields|all)`：把某类型/成员提升到 `DESCRIPTION` 级（对标 `preserve`）。
- `@dynamically_accessed(MemberTypes)` 标注参数/返回：数据流分析（`12`）据此保留被反射访问的成员
  （对标 `DynamicallyAccessedMembers` + `FlowAnnotations`）。
- `@dynamic_dependency("Member", Type)`：手工保留特定成员（对标 `[DynamicDependency]`）。
- `@requires_unreferenced_code("reason")`：标记「内部用反射、裁剪下不安全」的 API，编译器在调用点
  给裁剪警告（对标 `RequiresUnreferencedCode` + analyzer，衔接 `12` trim warnings）。
- 未注解的动态反射点（`ResolveToken`/按名查未知类型）→ 裁剪后该目标可能 `NONE` → 返回空/抛错
  并产出 trim 警告（对标 illink「未注解反射」诊断）。

## 6. 反射对象缓存与 GC（沿用现状 + 对接 09）

- 沿用现有反射对象缓存（`__zr_reflection_cache`）+ PIN（`reflection_pin_raw_object`）。
- PIN 与 `09` 移动 GC 协同：反射对象进 `pinned` region 或登记为不可移动根，避免 compact 失效。

## 7. 落地切片

| 切片 | 内容 | 验收 |
|------|------|------|
| 10-S1 | 反射三级模型 + 实体级别标注（默认按可达性最小）（§0） | 未反射可达类型不带反射元数据；体积下降可测 |
| 10-S2 | 按签名分桶 invoker thunk（复用 07§6）+ 注册表登记（§1） | `Invoke` 等价调用 AOT 与解释器结果一致 |
| 10-S3 | token 驱动反射解析（衔接 11）（§2） | 按 token 查类型/方法/字段成功；名表可裁剪后仍按 token 可用 |
| 10-S4 | 字段 offset / 泛型参数反射（§3/§4） | `DESCRIPTION` 级暴露正确偏移与类型实参 |
| 10-S5 | 保留注解（@reflectable/@dynamically_accessed/@dynamic_dependency/@requires_unreferenced_code）驱动 12（§5） | 注解的反射目标裁剪后仍可用；未注解给警告 |

## 8. 不变量校验

- **B 纯降级**：反射是边界能力，invoker = 边界 marshaling；typed 函数体本身不含反射代码。
- **C 单一真相**：偏移/签名/类型实参全部读唯一 layout + token 记录，反射层不另存。
- **D 环境隔离**：invoker 是独立边界函数，与 typed 函数体物理分离（可被 `07`§9 grep 排除）。
- 与 `12` 协同：反射级别即裁剪可达性的产物，注解是二者唯一接口。
