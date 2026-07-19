# 10 Native extern、内建库、模块与包解析设计

> 状态：设计已确认，等待按里程碑实施。
>
> 上游契约：[01 Canonical TypeRef、Place、CFG 与 artifact](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md)、[06 `%xxx` 迁移](./2026-07-18-06-percent-migration-lsp-fixtures-design.md)。
>
> 使用本设计的计划：[AOT](../aot/index.md)、[LSP](../lsp/index.md)、[debug](../debug/index.md)。

## 1. 目标与边界

本设计统一四个长期耦合但此前分散处理的问题：

1. native 声明使用语言级 `native extern`，不再以 `%extern` 或运行时字符串对象代表静态 FFI contract。
2. `zr.xxx` 是标准 native 库命名空间；源码、artifact、反射和 LSP 使用同一个包含 ModuleDomain 的 Canonical ModuleId，显示名与 provider locator 不参与类型身份。
3. import literal 先解析为结构化 ModuleSpecifier，再由项目清单、包图和 provider 解析；`.`、`/` 只是允许的输入分隔形式。
4. `.zrp` 明确工作区、别名、依赖、exports 和 `.zrm` 输出；第三方包名只允许单段 `@identifier`。

第一版不提供组织作用域包名、最长包名前缀匹配、隐式native搜索、运行时任意路径静态导入或`%extern`双轨语义。`@org/math`若出现，只能解释为包`@org`的子模块`math`；绝不能解释为“组织org下的包math”。

## 2. 规范源码表层

```zr
let math = import("zr.math");
let quaternion = import("core.math.quaternion");
let quaternionSlash = import("core/math/quaternion");
let sibling = import(".math.quaternion");
let siblingSlash = import("./math/quaternion");
let parent = import("..math.quaternion");
let parentSlash = import("../math/quaternion");
let tool = import("#lib/tool");
let packageRoot = import("@math");
let matrixDot = import("@math.matrix");
let matrixSlash = import("@math/matrix");
let assembly = import("artifacts/physics.zrm");
let workspaceRenderer = import("engine.render");
let nativeRenderer = import("native:engine.render");
let localSdk = import("file:///E:/sdk/physics.zrp");

native extern("zr_native_math") {
    #zr.ffi.entry("zr_add_i32")#
    #zr.ffi.callingConvention("c")#
    pub fn add(lhs: i32, rhs: i32): i32;

    #zr.ffi.entry("zr_fill")#
    pub fn fill(values: out Span<i32>, count: usize): bool;
}
```

锁定规则：

- `native` 说明该声明由宿主 ABI 实现；`extern("library")` 说明 native library locator。两者共同构成 `NativeExternBlock`，不是普通调用或 attribute 宏。
- block 中只允许 bodyless function/delegate/type mapping 声明；simple declaration 必须以 `;` 终止。
- 函数定义/声明仍用 `:` 引出 return TypeRef；callable TypeRef 仍写 `fn(A) -> R`。
- `#zr.ffi.*#` 是 metadata，不参与语法控制流。entry、ABI、encoding、marshalling 等必须在绑定期验证为已注册 schema。
- 静态导入仍只能写成模块级 `let alias = import("literal");`。import 返回 readonly ModuleNamespace object，同时形成 artifact dependency。

## 3. Canonical FFI contract

### 3.1 AST 与绑定

Syntax AST 只保存：

```text
NativeExternBlockSyntax(libraryLiteral, declarations, metadata, sourceRange)
NativeFunctionSyntax(name, parameters, returnTypeSyntax, metadata, semicolonRange)
```

Binder 将其一次性降低为：

```text
NativeImportContract {
  symbolId;
  declaringModuleId;
  libraryLocator;
  entryPoint;
  callable: CanonicalCallableContract;
  ffiSignature: FfiSignature;
  availability;
}

FfiSignature {
  abi;
  callingConvention;
  isVariadic;
  returnType: FfiType;
  parameters: FfiParameter[];
  charset;
  errorPolicy;
}

FfiParameter {
  type: FfiType;
  direction: in | ref | out;
  ownership;
  nullability;
  marshalling;
}
```

`CanonicalCallableContract` 保存语言语义；`FfiSignature` 保存 ABI 投影。二者必须在 semantic binding 时建立并校验，不能让 `zr.ffi` 在每次调用时重新解析 object/string 签名。

### 3.2 类型与安全边界

- scalar、pointer、string、enum、blittable struct/union、function pointer 必须由 Canonical TypeRef 与 TypeLayout 映射。
- by-value struct 必须校验 size、alignment、field ABI 和 target triple；不允许只凭类型名调用。
- `in/ref/out` 同时进入 borrow/definite-assignment facts 和 FFI direction；`out` 返回后才成为 initialized。
- GC reference、ref struct、Span、resource/owner 类型默认不可跨 ABI。只有注册 marshaller 或明确 pin/copy contract 才允许。
- callback 必须声明 lifetime、thread attach 和 exception policy；第一版不推断闭包逃逸。
- native 输入、动态库缺失和符号缺失是运行时边界；类型、direction、layout 与 metadata 冲突是编译期错误。
- 动态发现仍由 `zr.ffi` 库提供显式 API，但动态调用生成运行时 FfiSignature，与静态 `native extern` contract 分离。

### 3.3 artifact

`.zri/.zro` 保存 canonical native import table：library locator、entry、ABI、FfiSignature、layout hashes、source mapping 和 required capabilities。不得保存 `%extern` 源码拼写或需要运行时二次解析的临时对象图。

AOT backend 将该表降为 platform symbol/import thunk；VM 将同一表交给 libffi/provider。两条路径必须共享 ABI classification golden。

## 4. `zr.xxx` 标准模块与 native library

`zr`是官方核心模块保留根。workspace source module、`.zrp` alias、第三方package和`.zrm`不得声明或覆盖`zr`及其任意子模块；resolver在ModuleSpecifier root阶段拒绝，而不是按最长匹配或加载顺序决定所有者。

所有正式`zr.*`核心库都是native module：它们的公开类型、函数、常量、module link和capability由宿主注册的`ZrLibModuleDescriptor`或compiler/test host的等价descriptor提供，不以ZR源码实现作为规范真源。这里的native不等于“每个模块一个DLL”；builtin静态链接、descriptor plugin、compiler host和test host只是provider/deployment差异，Canonical ModuleId、TypeId和CallableContract不变。

生成的`.zrs` interface projection、type hint JSON、LSP virtual document和reflection table都只是同一descriptor的只读投影，不能反向成为第二套签名真源。用户仍只写普通`let alias = import("zr.xxx");`，不增加`native import`、`core import`或level关键字。

### 4.1 Occam准入门

新增`zr.*`模块或export必须同时满足：

1. 现有模块职责无法容纳，且拆分能消除真实phase、capability或依赖边界；仅为类型名建立子模块不成立。
2. ModuleId、公开type/constructor/function/metadata role、Canonical TypeId/CallableContract、provider phase和required capabilities全部登记。
3. 每个公开类型和函数分别进入其owner计划的reference ledger；不能用“相关协程/容器实现”代替该类型自身来源。
4. native descriptor、artifact、LSP、reflection和VM/AOT必须由同一schema验证；禁止手写五份可漂移清单。
5. shared compiler/runtime只消费registered role/capability id，不比较`Task`、`Iterator`等具体type-name字符串。

第一版明确不增加`zr.core`、`zr.async`、`zr.coroutine`、`zr.job`、`zr.scheduler`、`zr.iterator`、`zr.generator`、`zr.concurrent`、`zr.test.runtime`等重叠模块，也不按每个类型建立leaf module。

### 4.2 Native核心库分级

等级只描述build/link/load/phase策略，不进入源码语法、TypeId、overload resolution或运行时类型分派，也不要求在`ZrLibModuleDescriptor`增加可被用户观察的tier enum。

| 等级 | 含义 | ModuleId | 装载/裁剪规则 |
|---|---|---|---|
| N0 ABI内核 | VM和compiler不可缺少的最小类型/协议 | `zr.builtin` | 随runtime存在、不可覆盖；通常无需用户import |
| N1无权限基础库 | 普通程序可安全使用且无OS权限要求的核心数据/执行能力 | `zr.container`、`zr.iteration`、`zr.math`、`zr.task` | native descriptor可预注册；module object按import物化，未引用export可裁剪 |
| N2按需能力库 | 需要平台、项目capability、metadata preservation或显式资源策略 | `zr.thread`、`zr.pooling`、`zr.reflection`、`zr.ffi`、`zr.network.*`、`zr.system.*`、`zr.debug` | 只在import graph和capability policy允许时链接/加载；root namespace不隐式加载leaf |
| N3阶段宿主库 | 只在compiler或test phase存在 | `zr.compile`、`zr.compile.declaration`、`zr.testing` | 由compiler/test host注册；普通runtime resolver拒绝，production artifact不携带host executable |

N0-N3不是质量、稳定性或访问修饰符等级。N2也可以是静态链接builtin；N3也属于native，只是provider phase不是Runtime。

### 4.3 ModuleId与公开职责清单

| ModuleId | 等级/phase | canonical公开职责 | 依赖与处置 |
|---|---|---|---|
| `zr.builtin` | N0 / Runtime | 基础TypeRef、核心capability与编译器固有contract | 只保存不可下沉的最小ABI；普通helper不得继续堆入 |
| `zr.task` | N1 / Runtime | `Task<T>`、`Job<T>`、`Scheduler`、`currentScheduler`、`yieldNow`、`delay` | 只依赖builtin；hot/cold/schedule contract唯一owner |
| `zr.iteration` | N1 / Runtime | `Iterable<T>`、`Enumerator<T>`、`Iterator<T>`、`AsyncIterator<T>` | 依赖builtin和`zr.task.Task<T>`；同步provider可不触发Task runtime work |
| `zr.container` | N1 / Runtime | Array/Map/Set/LinkedList等容器 | 依赖builtin/iteration；Array/Span/Map提供concrete Enumerator |
| `zr.math` | N1 / Runtime | 数学函数与数值工具 | 无权限、可裁剪；不拆`zr.math.scalar/vector`第一版子层 |
| `zr.thread` | N2 / Runtime | `ThreadScheduler`、`Send`与独立isolate transport | 依赖task；第一版无Channel/SharedCell/Sync，不共享裸GC pointer |
| `zr.pooling` | N2 / Runtime | Pool、PoolHandle、PoolRef与slab policy | 消费TypeLayout/GC capability，不内嵌ownership核心语法 |
| `zr.reflection` | N2 / Runtime | Type/TypeOf、member query、type resolution与动态构造 | 声明descriptor位于`zr.reflection.declaration`；受preservation policy约束 |
| `zr.ffi` | N2 / Runtime | 动态FFI、marshaller与ABI capability | 静态native声明不在调用时重解析string signature |
| `zr.network` / leaves | N2 / Runtime | network namespace；`zr.network.tcp/udp`等leaf | root只提供module links，不隐式获取网络权限或加载所有leaf |
| `zr.system` / leaves | N2 / Runtime | system namespace；console/fs/assembly/env/process/gc/exception/vm | fs/process/gc等capability分别授权；root不聚合高权限exports |
| `zr.debug` | N2 / Runtime/Tool | debug、snapshot、coverage/profile facade | 当前裸`debug`一次性迁移；release可按policy裁剪 |
| `zr.compile` | N3 / CompileTool | BuildFacts、comptime diagnostic与compiler-only service | runtime不可导入；无runtime module-init |
| `zr.compile.declaration` | N3 / CompileTool | immutable declaration view与typed DeclarationPatch data | 依赖compile；只供带role的普通comptime fn |
| `zr.testing` | N3 / Test | test/case/skip metadata、assert/equal/throws与AssertionFailure | 依赖task；runner属于CLI host，production graph不链接 |

新增模块必须登记ModuleId、exports、capabilities、native descriptor schema、reflection roots和artifact compatibility；禁止只增加可加载library名而没有公共module contract。

### 4.4 核心TypeRef唯一归属

| Canonical TypeRef | 唯一owner ModuleId | native/compiler形态 | 禁止的重复归属 |
|---|---|---|---|
| `zr.task.Task<T>` | `zr.task` | native completion header + compiler async frame ABI | `zr.async.Task`、builtin同名type |
| `zr.task.Job<T>` | `zr.task` | native-described non-Copy value + `init` constructor contract | `zr.thread.Job`、`zr.scheduler.Job` |
| `zr.task.Scheduler` | `zr.task` | native-described interface/capability | `IScheduler`或thread私有同义interface |
| `zr.thread.ThreadScheduler` | `zr.thread` | native class/provider proxy | 第二套Task/Scheduler hierarchy |
| `zr.thread.Send` | `zr.thread` | descriptor注册的canonical type capability | 用户自定义同名marker或第一版`Sync` |
| `zr.iteration.Iterable<T>` | `zr.iteration` | native-described repeatable source capability | container私有同义protocol |
| `zr.iteration.Enumerator<T>` | `zr.iteration` | native-described sync cursor capability | Array/Span type-name special protocol |
| `zr.iteration.Iterator<T>` | `zr.iteration` | compiler-backed opaque value；public TypeId由native descriptor拥有 | `zr.generator.Iterator`、GC generator class |
| `zr.iteration.AsyncIterator<T>` | `zr.iteration` | compiler-backed async cursor；推进/close使用`zr.task.Task<T>` ABI | `zr.task.AsyncIterator`、额外AsyncEnumerator层 |

compiler-generated Iterator/AsyncIterator frame是函数私有artifact type，不获得新的public ModuleId。descriptor为公开opaque carrier分配稳定TypeId和role；`.zri/.zro`另存专用frame layout/hash。compiler、VM、AOT和LSP按registered role/capability id识别，不按完整限定名或短名写分支。

### 4.5 依赖方向

下图中`A -> B`表示A依赖B：

```text
zr.task                -> zr.builtin
zr.iteration           -> zr.builtin, zr.task
zr.container           -> zr.builtin, zr.iteration
zr.thread              -> zr.task
zr.pooling             -> zr.builtin
zr.reflection          -> zr.builtin
zr.compile.declaration -> zr.compile
zr.testing             -> zr.builtin, zr.task
```

- dependency只要求descriptor/type contract可解析，不等于立即materialize module object或启动scheduler。
- `zr.task`不能依赖thread/iteration/testing；单isolate scheduler独立成立。
- `zr.iteration`对task的依赖只承载AsyncIterator签名和ABI；同步Array/Span循环不得因此创建Task或初始化scheduler。
- root namespace module只保存module links；import `zr.system`不能隐式获得`zr.system.fs/process/gc`能力。
- capability或phase不匹配在binding/load前失败，不能fallback到同名source/package module。

### 4.6 Native descriptor唯一真源

每个native module必须由单一descriptor schema生成或验证以下投影：

```text
Canonical ModuleId and provider phase
public TypeIds, type roles and layout contracts
public constructors/functions/properties and CallableContracts
constants and module links
required runtime/project capabilities
reflection visibility and preserve roots
artifact ABI/schema version
native library entry descriptor
LSP/documentation projection
```

当前`ZrLibModuleDescriptor`已有moduleName、functions、types、typeHints、moduleLinks、moduleVersion、minRuntimeAbi、requiredCapabilities和materialize callback基础；目标是在该schema上收敛，不建立第二个core registry。Runtime/Test/CompileTool phase记录在descriptor与resolver/dependency result，不进入ModuleIdentity或TypeId；N0-N3由官方inventory/build policy决定，不参与动态dispatch。

当前`zr.network`、`zr.system`已经使用root + leaves；目标保留这种组织，但root import不会隐式把所有高权限leaf注入调用方。当前`zr.debug`实现中的descriptor、hint JSON和lookup都使用裸`debug`，迁移时必须一次性改为`zr.debug`并加入artifact/module-loader兼容诊断。

### 4.7 导入与部署

用户代码通过`let task = import("zr.task");`或`let iteration = import("zr.iteration");`使用已注册标准模块，不需要重复写`native extern`。`native extern`只用于项目自己的native ABI或generated binding；官方`zr.*`自身由registry descriptor提供。

静态链接、descriptor plugin和`.zrm` side-by-side部署只是provider差异，不改变公开ModuleId。官方native `.zrm`如果存在，也必须声明受保留`zr.*` identity并通过签名/发行者/ABI policy验证，不能被普通dependency伪装。

compiler/test phase仍复用同一ImportExpression和ModuleSpecifier，不增加`compile import`/`test import`语法。resolver把provider phase写入resolution/dependency record与binding，不写入ModuleIdentity：runtime/test ModuleNamespace只在对应phase的artifact graph实例化；`zr.compile`或`buildDependencies`产生CompileToolNamespace，只能被comptime、静态metadata schema或declaration transform引用，使用于runtime expression时报错且不会写入runtime dependency table。

### 4.8 Reference依据与刻意差异

- 当前ZR native registry：`zr_vm_library/include/zr_vm_library/native_registry.h`、`zr_vm_library/include/zr_vm_library/native_binding.h`、`zr_vm_lib_task/src/zr_vm_lib_task/module.c`、`zr_vm_lib_thread/src/zr_vm_lib_thread/module.c`。目标复用`ZrLibModuleDescriptor`/NativeRegistry，不创建新loader。
- Lua builtin/preload：`lua/src/linit.c`、`lua/src/loadlib.c`与`lua/testes/api.lua`证明builtin table、preload和普通require可以共享模块身份。ZR保留普通import，但使用typed descriptor而非global table。
- CPython builtin/extension：`lua/cpython/Python/import.c`、`lua/cpython/Python/importdl.c`、`lua/cpython/Lib/test/test_import/__init__.py`证明builtin与extension provider可统一进入module cache，同时保留loader类别。ZR把provider phase/ABI写入resolution record，与domain-aware ModuleIdentity分离。
- Mono internal call registry：`lua/mono/mono/metadata/icall.c`、`lua/mono/mono/metadata/icall-table.c`、`lua/mono/mono/tests/loader.cs`证明managed signature与native实现可由registration table连接。ZR进一步要求type/function/artifact/LSP共享descriptor schema。

刻意差异：ZR不复制Lua全局注入、CPython任意覆盖`sys.modules`或Mono字符串method lookup作为公共语义；`zr.*`根不可覆盖，所有编译器特例通过descriptor role/capability id表达。

## 5. ModuleSpecifier

### 5.0 Module Domain、provider与locator分离

前一版“custom native与workspace共享普通ModuleId，只在精确名字重复时报错”的方案被本节替代。它会让`zr.*`与其他native模块遵守两套identity规则，也无法自然表达同segments的source/native并存。

第一版固定三层：

```text
ModuleSpecifier  -> ModuleIdentity(domain + logical segments + package?)
                 -> selected ProviderKind + ProviderLocator
```

| 层 | 示例 | 进入TypeId | 作用 |
|---|---|---:|---|
| ModuleDomain + logical segments | `OfficialNative:zr.task`、`RegisteredNative:engine.render`、`Workspace:engine.render` | 是 | 形成Canonical ModuleId，用于name binding、类型所有者和artifact依赖 |
| ProviderKind | BuiltinDescriptor、DescriptorPlugin、Source、Zrm、CompileHost、TestHost | 否 | 决定实现/装载方式和phase |
| ProviderLocator | DLL路径、source root、`.zrm`路径、host catalog entry | 否 | 找到provider；只进lock/debug sidecar |

ModuleDomain至少包含：

```text
OfficialNative
RegisteredNative
Workspace
Package
```

第一版将ModuleDomain编码为artifact中的closed enum/tag，而不是可伪造的显示字符串；unknown mandatory domain必须拒绝加载。该tag是resolver/artifact内部identity字段，不是用户可声明的新关键字或泛型类型参数。

Relative和Alias是specifier kind，解析后继承目标domain；`file:`是provider locator scheme，读取目标后使用其声明、manifest或artifact内嵌的domain/ModuleId，不独立制造File TypeId。N0-N3仍只是official inventory的load policy，不是ModuleDomain。

### 5.0.1 统一前缀规则

| source spelling | 解析结果 | 说明 |
|---|---|---|
| `zr.task` | `OfficialNative:zr.task` | official native简写；canonical display保持`zr.task` |
| `native:engine.render` / `native:engine/render` | `RegisteredNative:engine.render` | host/project直接注册的custom native module |
| `engine.render` / `engine/render` | `Workspace:engine.render` | workspace逻辑绝对模块 |
| `.mesh` / `../mesh` | 继承当前workspace/package domain | 相对模块 |
| `@render/mesh` | `Package(@render):mesh` | package provider可以是source/native/`.zrm`，调用方不暴露provider |
| `#render/mesh` | alias target解析后的domain | alias不创建或抹除domain |
| `file:///E:/repo/render.zr` | locator；读取后取得declared identity | Windows绝对文件 |
| `file:///opt/repo/render.zrp` | locator；读取project default entry | POSIX绝对project |
| `file://server/share/render.zrm` | locator；读取artifact identity | UNC/network locator，受capability policy约束 |

`native:`和`file:`只存在于ImportExpression string literal的ModuleSpecifier grammar，不是语言关键字。禁止`!native`、`$native`、`native import`等第二套表层。

### 5.0.2 `zr`与RegisteredNative

- `zr.*`整体保留给OfficialNative domain。workspace/package/RegisteredNative和未通过官方签名/发行者/ABI policy的file target不能声明或嵌入OfficialNative identity；diagnostic为`module.reserved_root`。经过验证的官方`.zrm`只是OfficialNative的另一provider部署，不产生新domain。
- `native:zr.task`非法；official module只有`zr.task`一种source spelling，避免双轨。
- `native`没有冒号时只是普通workspace segment，因此`native.engine`与`native:engine`是两个明确identity。
- RegisteredNative的logical segments可以与Workspace相同：`native:engine.render`和`engine.render`合法并存，TypeId因domain不同而不同。
- native module不占用segments前缀：`native:engine.render`、`native:engine.render.backend`、`engine.render`和`engine.render.mesh`均可独立存在。

### 5.0.3 自定义native注册与host注入

`native extern("library")`中的literal只是ABI library locator；其中声明的函数仍属于当前Workspace/Package source module，不创建RegisteredNative identity。需要`import("native:...")`的模块必须提供native descriptor：

```json
{
  "nativeProviders": {
    "engine.render": {
      "library": "native/engine_render",
      "entry": "ZrVm_GetNativeModule_v1",
      "abiVersion": 1
    }
  }
}
```

- `nativeProviders`区段已隐含RegisteredNative domain，map key只写logical segments；对应import写`native:engine.render`。
- descriptor registration envelope保存`moduleDomain = RegisteredNative`；`ZrLibModuleDescriptor.moduleName`必须规范化为map key，否则报`native.module_name_mismatch`。
- `library`是provider locator；文件/目录名、entry symbol和绝对路径都不参与ModuleIdentity。
- 同一shared library可以通过不同entry提供多个descriptor；每个`RegisteredNative + segments + package identity`只能选择一个provider。
- 宿主静态注入也必须在compile前提供同一descriptor catalog/contract hash。只有runtime callback而没有compile descriptor时，不能满足静态import/TypeRef，只能由dynamic `loadModule/loadPlugin`返回erased ModuleNamespace/object。
- 第三方发布优先通过`@package` dependency/exports暴露native实现；对外identity为Package，不要求用户写`native:`。

### 5.0.4 `file:`真实绝对路径

- 第一版只接受URI形式：Windows drive使用`file:///E:/path`，POSIX使用`file:///opt/path`，UNC使用`file://server/share/path`。裸`C:/...`、`/opt/...`和`\\server\share`仍报`module.physical_path_requires_file_scheme`。
- target可以是`.zr`、`.zrp`、`.zrm`或以`/`结尾的directory URI。directory root必须恰有一个`.zrp`；零个或多个都报错，再由该manifest的entry/exports解析，不猜测`index.zr`、目录同名文件或注册顺序。调用方可直接写到具体`.zrp`以消除directory ambiguity。
- 直接指向`.zr`时必须有显式`module`声明，并固定形成Workspace identity；resolver不向上搜索manifest。需要Package identity必须指向`.zrp`/directory或带embedded package identity的`.zrm`。`.zrp`提供workspace/package identity；`.zrm`提供embedded identity。locator spelling和normalized absolute path不进入public TypeId。
- lockfile记录target identity、content hash和必要provider facts；绝对路径只保存在local lock/debug sidecar。发布package时未映射为dependency/package的`file:`import报`module.nonportable_file_import`。
- filesystem case、drive、separator、symlink和UNC规范化只用于locator cache/sandbox，不能改变语言identifier大小写或segment规则。

### 5.0.5 冲突与去重

resolver先形成domain-aware ModuleIdentity，再选择provider；不按source > native > binary或注册顺序设置隐式优先级：

| 情况 | 结果 |
|---|---|
| `native:engine.render`与`engine.render` | 合法并存；domain不同，TypeId不同 |
| 两个native provider声明`RegisteredNative:engine.render` | `module.duplicate_provider`；不按先注册者获胜 |
| 两个source/artifact provider声明同一Workspace identity | build target/lock必须选定；否则duplicate/ambiguous |
| file locator读取的declared/embedded identity与expected不符 | `module.locator_identity_mismatch` |
| 两个locator指向相同domain identity、artifact generation和contract hash | 去重为同一provider instance |
| alias指向native/file/package/workspace | 保留目标domain；alias不能把两个冲突provider变成不同identity |
| 同package不同version | package identity不同；按dependency graph共存或在unqualified export处报版本冲突 |
| custom provider尝试OfficialNative identity | `module.reserved_root`，不进入普通duplicate resolution |

Canonical ModuleId/ModuleIdentity只保存domain、logical segments和package instance identity/version（若有）。selected provider kind/phase、locator、artifact identity/generation和public contract hash属于resolver结果与dependency record，不反向进入ModuleIdentity。public TypeId使用ModuleIdentity + symbol/generic shape；DLL/source绝对路径、provider部署方式和同contract artifact重建不改变它。

### 5.1 输入类别

```text
OfficialNative:     zr.math
RegisteredNative:   native:engine.render | native:engine/render
WorkspaceAbsolute:  core.math.quaternion | core/math/quaternion
Relative:           .math.quaternion | ./math/quaternion
ParentRelative:     ..math.quaternion | ../math/quaternion
Alias:              #lib | #lib.tool | #lib/tool
Package:            @math | @math.matrix | @math/matrix
AssemblyLocator:    artifacts/physics.zrm
FileLocator:        file:///E:/sdk/physics.zrp | file:///opt/sdk/physics.zrm
```

Parser 不直接拼接文件路径，而是生成：

```text
ModuleSpecifier {
  kind;
  domainHint?;
  root;
  segments[];
  locator?;
  artifactExtension?;
  spelling;
}
```

`.` 与 `/` 在 segment 区域等价。相对前缀是独立 token 规则：单个 `.`/`./` 表示当前模块目录，连续两个 `..`/`../` 表示上级；它们不能被误拆成空 segment。

### 5.2 等价与规范化

```text
core.math.quaternion  == core/math/quaternion
native:engine.render  == native:engine/render
.math.quaternion      == ./math/quaternion
..math.quaternion     == ../math/quaternion
#lib.tool              == #lib/tool
@math.matrix           == @math/matrix
```

Canonical logical name使用`.`连接segments，但ModuleIdentity还包含domain：`Workspace:engine.render`不等于`RegisteredNative:engine.render`。文件provider可以映射到`core/math/quaternion.zr`；native scheme冒号前不参与segments，file URI path不应用`.`/`/`module分隔符等价。

`import("physics.zrm")`或带目录的`.zrm`literal是project-relative assembly locator；真实绝对artifact使用`file:`。resolver读取`META-INF/zrm.json`取得domain-aware ModuleIdentity/default entry。`.zrm`内部依赖仍使用Canonical ModuleIdentity，不把ZIP member或physical path暴露为TypeId语义。

### 5.3 `#alias`

`.zrp` alias key必须为`#identifier`；value是一个非alias ModuleSpecifier，可指向workspace、`native:`、`file:`或package root。`#lib/tool`先替换完整root，再按目标kind追加segments并保留domain。指向workspace/package/RegisteredNative root或`file:`目录/`.zrp`的alias可追加segments；指向terminal `.zr`或无exports的`.zrm`时追加segment报`module.alias_target_not_expandable`。禁止递归alias、最长字符串匹配，以及用alias覆盖`zr.*`trust policy。

### 5.4 单段 `@package`

已确认的第一版规则：

- 包名语法严格为 `@identifier`，不能包含 `.` 或 `/`。
- `@math` 解析 package root/default export。
- `@math.matrix` 与 `@math/matrix` 都解析包 `@math` 的 `./matrix` export。
- package root 只读取完整的首个 `@identifier`，因此不需要最长匹配，也不存在包名/子模块边界歧义。
- root之后可以有多个合法module segment，例如`@math.linear.matrix`等价于`@math/linear/matrix`；这些segment属于module export path，不属于package name。
- `@@math`、`@1math`、`@math//matrix`、trailing separator和非法identifier segment在specifier parse阶段失败。
- 未在 `.zrp` dependencies 中声明、未通过 lock/integrity policy 的 package 一律失败。
- 组织命名以后采用独立、可见的语法扩展；第一版不保留推测性兼容。

## 6. `.zrp` v2

```json
{
  "manifestVersion": 2,
  "name": "physics-workspace",
  "version": "1.0.0",
  "description": "Deterministic physics primitives for ZR applications.",
  "license": "MIT",
  "authors": ["ZR Physics Team"],
  "repository": "https://github.com/He-Jiahui/zr_vm.git",
  "keywords": ["physics", "math"],
  "kind": "library",
  "source": "src",
  "binary": "bin",
  "entry": "index",
  "engines": {
    "zr": ">=0.1.0",
    "zrmSchema": "2"
  },
  "capabilities": ["native.math"],
  "aliases": {
    "#lib": "core/lib"
  },
  "package": {
    "name": "@physics",
    "exports": {
      ".": "index",
      "./matrix": "math/matrix"
    }
  },
  "dependencies": {
    "@math": {
      "version": "^2.0.0",
      "path": "../math/math.zrp"
    }
  },
  "buildDependencies": {
    "@derive": {
      "version": "^1.0.0",
      "path": "../derive/derive.zrp"
    }
  },
  "nativeProviders": {
    "acme.simd": {
      "library": "native/acme_simd",
      "entry": "ZrVm_GetNativeModule_v1",
      "abiVersion": 1
    }
  },
  "assembly": {
    "name": "physics",
    "output": "dist/physics.zrm"
  }
}
```

约束：

- `aliases`、`dependencies`、`buildDependencies`、`nativeProviders`、`package.exports`是不同用途的manifest区段；alias/package key和native ModuleId不得借前缀转换互相覆盖。
- `name/version/description/license/authors/repository/keywords/engines`构成发布元数据；`name`是项目/发布显示名，不承担import package root，后者只来自`package.name`。
- dependency key 必须满足单段 package 语法；每项至少给出 version requirement，并由 registry/path/git 等单一 provider 提供详细来源。发布/锁定阶段记录 resolved version、content integrity 和 transitive graph。
- `buildDependencies`只供compiler sandbox加载exported metadata schema、declaration transform和comptime tool；其runtime exports不自动进入业务ModuleNamespace，compile-tool hash进入incremental key和lock graph。
- `nativeProviders`只绑定RegisteredNative logical segments到physical provider；key不能使用`zr.*`或`native:`前缀，descriptor.moduleName必须匹配规范化key，library locator不参与ModuleIdentity。
- registry dependency记录registry URL/package id；path dependency记录规范化项目路径；git dependency记录repository与commit/tag。三种source互斥，不能在不同机器上按“先找到哪个”选择。
- exports key 只允许 `.` 或 `./segments`；未 export 的内部模块不能被 package import 绕过。
- `entry` 是项目或 `.zrm` 默认入口，不改变 `@package/submodule` 的显式 exports 解析。
- `capabilities`声明构建/运行所需权限；native、filesystem、network、process、thread等高权限能力由loader/policy校验，不能因import某个root module隐式获得。
- resolved version、content hash、transitive identity和provider结果写入生成的lock graph；`.zrp`保留人类声明，writer不把机器本地绝对缓存路径写入可发布manifest。
- 旧 `pathAliases` 的 `@alias`、依赖 `$name`、源码 `&name` 只进入 migration reader；v2 writer 永远只输出 `#alias` 与 `@package`。

## 7. Resolver pipeline

解析必须按固定阶段执行：

```text
literal
  -> parse ModuleSpecifier
  -> classify zr/native/file/workspace/relative/#alias/@package form
  -> normalize separators and relative base
  -> expand exactly one #alias or resolve one @package root
  -> for file:, read and validate the target's declared/manifest/embedded identity
  -> otherwise form domain-aware Canonical ModuleId from the logical specifier
  -> enforce package exports / workspace root
  -> choose and validate source, .zro, .zrm or descriptor provider
  -> record dependency and compatibility hashes
```

Canonical ModuleId是结构化、domain-aware identity，不是裸显示字符串：至少包含ModuleDomain、logical segments和package identity/version（若有）。resolver结果另存provider kind、provider phase、locator、artifact identity/generation和public contract hash。循环检测、TypeId和module object identity以Canonical ModuleId工作；artifact cache、LSP definition与debugger source lookup使用Canonical ModuleId加resolver结果，不能以用户输入字符串或物理路径为key。

provider descriptor还必须声明`Runtime | Test | CompileTool` phase。phase不产生第二个ModuleIdentity；resolver必须校验同一ModuleId的所有候选provider具有相同公开phase contract，并在目标environment中建立独立instance/cache entry。compile-tool executable section绝不能被runtime loader当普通`.zro/.zrm`代码映射。

解析错误分层为invalid scheme/specifier、reserved domain/root、physical path requires `file:`、unknown alias/package、package export denied、module not found、same-domain duplicate/ambiguous provider、locator identity mismatch、artifact incompatible、native capability unavailable和provider phase mismatch。每类错误必须携带原literal range、已解析domain/root/segments，以及可用时的manifest/descriptor related range。

## 8. LSP、反射与 debug

- hover import alias显示原spelling、ModuleDomain、Canonical ModuleId、package/version、provider resolution和readonly namespace type；physical locator单独显示。
- completion在`#`后仅列alias，在`@`后仅列dependencies，选中package后只列exports；`native:`后只列compile descriptor catalog中的RegisteredNative模块，`file:`只提供显式URI locator completion。
- definition 优先落到 source declaration；只有 binary/native 可用时落到 `.zri`/metadata virtual document。
- rename 不改 dependency/package identity，只改本地 binding；module move 使用独立 refactor并更新 `.zrp` exports/aliases。
- reflection 的 assembly/module API 返回 Canonical ModuleId 与 artifact identity，不暴露 resolver 临时路径。
- DebugMap 记录 ModuleIdentity + source document checksum；`.zrm`/package 中调试符号可稳定回到原 source。

## 9. 迁移

迁移工具按 AST/manifest schema 执行：

| 旧形式 | v2 目标 | 策略 |
|---|---|---|
| `%extern("x") { ... }` | `native extern("x") { ... }` | 解析声明与 metadata，重建 FfiSignature |
| `%import("x")` | `let alias = import("x");` | 需要上下文生成 alias，不能纯文本替换 |
| `pathAliases: { "@lib": ... }` | `aliases: { "#lib": ... }` | 检测与 package dependency 冲突 |
| `$dep` / `&dep` | `@dep` | 必须有 dependency 记录和 exports 校验 |
| bare `debug` native module | `zr.debug` | 提供明确过渡诊断，不永久双注册 |
| 裸 `C:/...`、`/opt/...`、UNC import | `file:///...` / `file://server/...` | 仅在目标identity可静态读取时提供requires-review URI改写；发布库优先迁到dependency/alias |
| custom native裸名与workspace同名 | `native:engine.render` | 为RegisteredNative补显式scheme；workspace `engine.render`保持不变，两者允许并存 |

读取旧 `.zrp` 可以有版本化兼容窗口；目标源码、目标 artifact 与 formatter 不输出旧形式。迁移后二次运行必须幂等。

## 10. 实施里程碑与验收

1. **M1 specifier foundation**：加入domain-aware ModuleSpecifier/ModuleIdentity、`zr`/`native:`/`file:`分类、分隔符等价、relative、`#alias`、单段`@package` parser和resolver单元测试。
2. **M2 manifest/artifact**：实现 `.zrp` v2 reader/writer、exports、dependency lock identity、`.zrm` entry 与 `.zro` dependency roundtrip。
3. **M3 native contract**：实现 NativeExtern AST、CanonicalCallableContract 到 FfiSignature 的静态校验和 VM/AOT 共享 golden。
4. **M4 native library convergence**：保留`zr.*`根，冻结N0-N3 inventory；把`debug`迁移到`zr.debug`，为task/iteration/container/thread/reflection/pooling/compile/testing登记唯一native/host descriptor和canonical type role。
5. **M5 consumers/migration**：LSP、reflection、debug、formatter、migration tool 只消费 canonical identity/schema。

晋级要求：

- 本文全部同domain等价import对产生相同Canonical ModuleId；`RegisteredNative:engine.render`与`Workspace:engine.render`必须产生不同ModuleIdentity并能同时导入；不同package version不产生相同ModuleIdentity。
- 非法package root/空module segment、未导出子模块、越出workspace root、alias cycle、同domain duplicate provider和provider ambiguity都有稳定负例。
- `.zr`、`.zro`、`.zrm` 与 native provider 的 import 结果共享 public contract hash 检查。
- VM/libffi 与 AOT 对 scalar、struct、in/ref/out、callback、错误路径使用相同 FfiSignature 测试向量。
- workspace source、alias、package、RegisteredNative或未授权`.zrm`声明`zr.*`时在resolver入口失败；`native:zr.task`也失败；builtin/descriptor plugin部署同一官方模块时产生相同ModuleId、TypeId和public contract hash。
- `zr.task.Task<T>`、`zr.task.Job<T>`、`zr.iteration.Iterator<T>`和`zr.iteration.AsyncIterator<T>`分别只有一个owner TypeDef；source/binary/LSP/reflection不出现短名复制或第二套type identity。
- N0-N3只影响build/link/load/phase policy；parser、TypeRef identity和runtime dispatch中不存在tier分支。
- Runtime/Test/CompileTool phase错误、missing required capability、duplicate official descriptor和ABI/schema mismatch都有稳定诊断。
- 裸OS绝对路径失败，三种`file:` URI形式成功读取声明/manifest/artifact identity；directory无`.zrp`、locator identity mismatch和发布时nonportable file import都有稳定诊断。
- alias展开后保留目标domain；它不能把同domain重复provider拆成两个identity，也不能把`native:`、package或file target降格为Workspace。
- resolver、LSP、debug、reflection 内不存在独立字符串切分副本。
- 仓库目标 fixture 不再出现 `%extern`、旧 `@alias`、`$dep` 或 `&dep`。

## 11. 参考依据

- CPython `Python/import.c::resolve_name`：相对层级先结构化解析，再交给 module loader；ZR 采用这一分层，不复制 Python 的 package object 语义。
- QuickJS module normalize/load callback：specifier normalization 与 provider 加载分离；ZR 在两者之间加入 `.zrp` exports 与 ModuleIdentity。
- JDK `ModuleFinder`/module descriptor：稳定 module name、descriptor 与 container 分离；ZR 的 `.zrm` 同样不把物理 member path 当公共 identity。
- .NET `DllImportAttribute`、`LibraryImportAttribute`、`CallingConvention`、`UnmanagedCallConvAttribute` 与 `MarshalAsAttribute`：静态 native contract显式保存 entry、ABI 与 marshalling。
- Rust `ExternAbi` 与 target call-convention lowering：源码 ABI 必须在 target lowering 前成为结构化 contract。

这些参考实现位于仓库 `lua/` 镜像中。ZR 的关键差异是：第三方包 root 使用单段 `@identifier`，workspace alias 使用 `#identifier`，二者在词法阶段即可区分。
