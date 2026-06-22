---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 参照 hybridclr/mono/roslyn 完善元数据
  - decision: 2026-06-20 元数据默认最小 + 注解保留；zrp 两段式（数据元数据 + 代码注册表）
references:
  - lua/hybridclr/libil2cpp/vm/GlobalMetadataFileInternals.h   # global-metadata.dat 头/表
  - lua/hybridclr/libil2cpp/il2cpp-metadata.h                  # CodeRegistration / MetadataRegistration 分离
  - lua/mono/mono/metadata/metadata-internals.h               # MonoImage / 表 / token 缓存
  - lua/mono/mono/mini/aot-runtime.h                          # MonoAotFileInfo
  - lua/runtime/src/coreclr/tools/aot/ILCompiler.MetadataTransform/ILCompiler/Metadata/MetadataTransform.cs
  - lua/roslyn/src/Compilers/Core/Portable/PEWriter/MetadataWriter.cs   # TypeSpec/MethodSpec
related_code:
  - zr_vm_core/include/zr_vm_core/metadata_token.h     # 8 表 token 体系 + 签名节点 + TokenBinding
  - zr_vm_core/src/zr_vm_core/function_metadata_query.c
  - zr_vm_core/include/zr_vm_core/function.h           # SZrFunctionMetadata / ModuleEffect
  - zr_vm_core/include/zr_vm_core/type_layout.h        # cTypeId / SZrTypeLayoutMetadata
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_callable_provenance.h
  - docs/parser-and-semantics/（zrp assembly manifest 设计）
---

# 11 · 元数据（zrp 两段式 + 运行期解析 + token↔C 三向表）

> 承接缺口：zr_vm 已有完整 token 体系（`metadata_token.h`：8 表 MODULE/TYPE_DEF/MEMBER_DEF/
> ASSEMBLY_REF/TYPE_REF/MEMBER_REF/TYPE_SPEC/SIGNATURE，签名节点含 `GENERIC_INST/OWNERSHIP/
> UNION/NULLABLE/...`，`SZrMetadataTokenRecord` + `SZrMetadataTokenBinding` 含版本/layoutHash）、
> function 元数据、AOT function table、callable provenance。**缺**：编译期元数据消除、
> token↔C type 显式映射、版本检查运行实现、泛型参数标准化编码、zrp manifest 实现、导出/导入 API。
> 本文确立 zrp 元数据格式与运行期解析，default 最小化（`12`）。

## 0. 两段式元数据（对标 il2cpp CodeRegistration / MetadataRegistration 分离）

zrp 装配产物的元数据分两段，职责清晰、可独立裁剪：

```
zrp assembly
├── 数据元数据（data metadata，只读、可 mmap、版本化）           ← 对标 global-metadata.dat / MonoImage 表
│     类型/方法/字段/泛型定义表 + 字符串池 + 签名 blob 池 + token 表
└── 代码注册表（code registration，AOT 编译产出、随 .so/.c 链接）  ← 对标 Il2CppCodeRegistration
      函数指针表 · invoker 表(10) · 泛型实例/字典表(08) · type layout 表 · GC descriptor 表(09)
```

- 解释器/dynamic 路径主要读**数据元数据**；AOT/typed 路径主要用**代码注册表**；两者经 token 关联。
- 这与 il2cpp「静态数据 + 生成代码表」一致，也与 mono「MonoImage 表 + MonoAotFileInfo」对应。

## 1. 数据元数据格式（对标 ECMA 表 / global-metadata.dat）

- **头** `SZrZrpMetadataHeader`：magic、version、各表偏移+计数、池偏移（对标 `Il2CppGlobalMetadataHeader`）。
- **定义表**（沿用 `metadata_token.h` 的 8 表语义，落为紧凑只读数组）：
  TypeDef / MethodDef / FieldDef / GenericParam / GenericParamConstraint / TypeSpec / MethodSpec / ModuleRef。
- **池**：字符串池（名表，可被 `12` 裁剪）、签名 blob 池（`SZrMetadataTokenRecord.signatureBlob` 指入）、
  默认值/常量池。
- **token 编码**：沿用现有「高 8 位表 ID + 低 24 位 RID」（`metadata_token.h`），全程序唯一。

## 2. 代码注册表（对标 Il2CppCodeRegistration，编译期生成 C）

AOT 编译为每个 zrp 模块发射一份只读注册表（C 静态数组），是 typed 路径的入口：

```c
typedef struct SZrAotCodeRegistration {
    TZrUInt32                   functionCount;
    const TZrAotFunctionPointer *functionPointers;   /* token→具体 C 函数（已含 08 单态实例） */
    const SZrAotMethodInfo     *methodInfos;          /* 07§4，每函数描述符 */
    const TZrAotInvoker        *invokers;             /* 10§1，按签名分桶 */
    const SZrAotGenericInstanceEntry *genericInstances; /* 08§3 收集表 → cInstanceId */
    const SZrTypeLayout * const *typeLayouts;         /* 02，token→layout（亦驱动 09 descriptor） */
    const SZrAotGcDescriptor   *gcDescriptors;        /* 09§1 */
} SZrAotCodeRegistration;
```

- 对标 il2cpp `genericMethodPointers/invokerPointers/codeGenModules/...`，但裁剪后只含可达项（`12`）。
- 注册表在模块加载时登记到运行期（`ZrLibrary_AotRuntime_*` 已有雏形，见 `backend_aot` 运行时）。

## 3. 运行期元数据解析（SZrMetadataRuntime，对标 il2cpp MetadataCache / mono MonoImage 缓存）

- 新增 `SZrMetadataRuntime`：持有 mmap 的数据元数据 + 代码注册表，提供 token → 运行期实体的 **lazy 解析**：
  `ResolveType/ResolveMethod/ResolveField(token)`，结果缓存（对标 mono `class_cache`/`method_cache`、
  il2cpp `MetadataCache::GetTypeInfoFromTypeIndex` 延迟初始化）。
- 这是 `10`（反射按 token）、`08`（泛型字典 slot 解析）、`12`（裁剪后实体查找）的共同底座。

## 4. token ↔ cTypeId ↔ ZrLayout 三向映射（单一真相落地）

现状 `cTypeId` 存在但无公开「token → C type」表。补一张三向表（不变量 C 的物化）：

```
metadataToken  ⇄  cTypeId  ⇄  struct ZrLayout_<cTypeId>
        ↑ 数据元数据 TypeDef        ↑ 生成 C 类型 / layout / GC descriptor
```

- 反射（`10`）、泛型字典（`08` TYPE_LAYOUT slot）、GC descriptor（`09`）全部经此表取同一 layout，
  禁止各自硬编码偏移（`01`§不变量 C）。
- 编译期发射该表为代码注册表的一部分；运行期经 `SZrMetadataRuntime` 索引。

## 5. 泛型参数标准化编码（衔接 08，对标 roslyn TypeSpec/MethodSpec）

- `GENERIC_INST` 签名节点标准化为：`baseToken + argCount + argSignatures[]`（对标 roslyn TypeSpec 签名）。
- 泛型方法实例 → MethodSpec 记录 `methodToken + instantiationSignature`（对标 MethodSpec）。
- 泛型参数定义/约束 → GenericParam / GenericParamConstraint 表（对标 ECMA、roslyn `ITypeParameterSymbol`）。
- 与 `08`§3 实例化表共用同一签名编码，去重键一致。

## 6. 版本检查运行实现（落地 SZrMetadataTokenBinding 既有字段）

现有 `SZrMetadataTokenBinding` 已有 `requestedModuleVersion/min/maxModuleVersion/layoutVersion/
layoutHash/signatureHash`，但无运行期校验流程。补：

- 模块加载/跨模块 token 解析时校验：版本落在 `[min, max)`、`layoutHash`/`signatureHash` 匹配。
- 不匹配处置（对标 mono AOT out_of_date）：dynamic 模块 → 拒绝加载/报错；typed 调用边界 → deopt 到
  解释器（`04`§6），用数据元数据动态解释。保证 ABI 漂移不致崩溃。

## 7. 元数据策略（对标 NativeAOT IMetadataPolicy，default 最小）

- 每实体「生成 def 还是仅 ref / 生成哪一级反射元数据」由策略决定：可达性（`12`）+ 反射级别（`10`§0）+ 注解。
- 默认：仅可达实体生成 def；被外部引用但内部不反射 → ref；未可达 → 不生成（对标
  `AnalysisBasedMetadataManager` + `MetadataCategory`）。

## 8. 导出/导入与 zrp manifest

- 形式化 `docs/parser-and-semantics/` 的 zrp assembly manifest：声明模块标识、版本、依赖、导出 token、
  保留规则（`12` link.xml 等价物）、AOT 模式（hybrid/full-AOT，`08`§6）。
- 提供 `zrp` 工具读写数据元数据（dump/diff/版本检查），便于跨模块 ABI 演进。

## 9. 落地切片

| 切片 | 内容 | 验收 |
|------|------|------|
| 11-S1 | zrp 数据元数据格式（头/表/池）+ 读写器（§1/§8） | round-trip 一致；可 mmap 只读加载 |
| 11-S2 | 代码注册表发射 + 模块加载登记（§2） | AOT 模块加载后 token→函数/layout 可解析 |
| 11-S3 | `SZrMetadataRuntime` token lazy 解析 + 缓存（§3） | 按 token 解析类型/方法/字段正确，二次命中缓存 |
| 11-S4 | token↔cTypeId↔ZrLayout 三向表（§4） | 反射/泛型/GC 三方读同一 layout；无重复偏移源 |
| 11-S5 | 泛型参数标准化编码（GENERIC_INST/MethodSpec/约束）（§5） | 与 08 实例化去重键一致；解释器/AOT 同解 |
| 11-S6 | 版本检查运行实现 + 不匹配 deopt/拒绝（§6） | 注入 ABI 漂移：dynamic 拒绝、typed deopt，无崩溃 |
| 11-S7 | 元数据策略（默认最小）+ zrp manifest + 工具（§7/§8） | 默认产物仅含可达元数据；manifest 保留规则生效 |

## 10. 不变量校验

- **C 单一真相**：token↔layout 三向表是偏移/大小/签名的唯一来源；反射/泛型/GC/序列化全部经它。
- **B 纯降级**：元数据是数据 + 边界能力，typed 函数体不读元数据（纯标量函数 `methodInfo` 都不读，`07`§4.1）。
- 与 `12` 协同：元数据生成量由可达性 + 注解决定，二者是同一裁剪管线的输出。
