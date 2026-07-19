# 10 Native extern、内建库、模块与包解析设计

> 状态：设计已确认，等待按里程碑实施。
>
> 上游契约：[01 Canonical TypeRef、Place、CFG 与 artifact](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md)、[06 `%xxx` 迁移](./2026-07-18-06-percent-migration-lsp-fixtures-design.md)。
>
> 使用本设计的计划：[AOT](../aot/index.md)、[LSP](../lsp/index.md)、[debug](../debug/index.md)。

## 1. 目标与边界

本设计统一四个长期耦合但此前分散处理的问题：

1. native 声明使用语言级 `native extern`，不再以 `%extern` 或运行时字符串对象代表静态 FFI contract。
2. `zr.xxx` 是标准 native 库命名空间；源码、artifact、反射和 LSP 使用同一个 Canonical ModuleId。
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

## 4. `zr.xxx` 标准 native 库

统一规则：标准库公开模块都位于 `zr` 根命名空间；implementation library 名和源码目录名不是公共 ModuleId。

| ModuleId | 当前/目标职责 | 设计处置 |
|---|---|---|
| `zr.builtin` | 基础类型、核心 interface 与编译器固有契约 | 保持内部基础模块；公开 surface 需显式清单 |
| `zr.container` | Array/Map/Set/LinkedList 等容器 | 以 Canonical TypeRef/Layout 描述 element contract |
| `zr.math` | 数学函数与数值工具 | 保持稳定 ModuleId |
| `zr.ffi` | 动态 FFI、marshaller 与 ABI capability | 静态 native 声明不依赖运行时字符串重解析 |
| `zr.network` | 网络根 namespace | leaves 为 `zr.network.tcp`、`zr.network.udp` |
| `zr.system` | 系统能力根 namespace | leaves 为 console/fs/assembly/env/process/gc/exception/vm |
| `zr.task` | Task、await 与调度 contract | 与 `zr.thread` 通过公开 contract 连接 |
| `zr.thread` | 多线程 scheduler、同步与 Send/Sync | 仍受项目 multithread capability 控制 |
| `zr.debug` | 脚本调试、快照、coverage/profile facade | 把当前裸 `debug` ModuleId 迁移为 `zr.debug` |
| `zr.reflection` | Type、member query、type resolution | 独立库；类型声明位于 `zr.reflection.declaration` |
| `zr.pooling` | Pool、PoolHandle、PoolRef 与 slab policy | 独立库；不得内嵌进 ownership 核心语法 |

新增模块必须登记 ModuleId、exports、capabilities、native descriptor schema、reflection roots 和 artifact compatibility；禁止只增加一个可加载 DLL 名而没有公共模块 contract。

### 4.1 分层与依赖方向

```text
foundation:   zr.builtin <- zr.container
services:     zr.math / zr.ffi / zr.network.* / zr.system.*
concurrency:  zr.task <- zr.thread
data/runtime: zr.reflection / zr.pooling
tooling:      zr.debug
```

- `zr.builtin`只保存语言/runtime不可避免的最小protocol；普通容器、pool、reflection helper不能继续堆入builtin。
- `zr.container`可以依赖builtin interface，不能反向让builtin依赖具体Array/Map实现。
- `zr.thread`可以消费`zr.task`的Task/IScheduler公开contract；`zr.task`核心不能要求thread模块存在，单线程scheduler仍可工作。
- `zr.debug`只消费公开core debug/profile/reflection query，不成为业务模块运行的隐式依赖。
- `zr.reflection`查询metadata；`zr.pooling`消费TypeLayout/GC capability。两者都通过注册service接入，不修改parser来识别具体类型名。
- coroutine/async是语言与task runtime contract；在存在独立native descriptor和公开API前，不虚构`zr.coroutine`模块。

### 4.2 Native descriptor 一致性

每个native module必须由单一descriptor源生成或验证以下投影：

```text
Canonical ModuleId
public exports and signatures
type/member metadata hints
required runtime capabilities
leaf module descriptors
reflection visibility/preserve roots
native library entry descriptor
```

当前`zr.network`、`zr.system`已经使用root + leaves；目标保留这种组织，但root import不会隐式把所有高权限leaf注入调用方。`zr.system.fs/process/gc`等capability必须分别在项目与runtime policy中授权。当前`zr.debug`实现中的descriptor、hint JSON和lookup都使用裸`debug`，迁移时必须一次性改为`zr.debug`并加入artifact/module-loader兼容诊断。

### 4.3 导入与部署

用户代码通过`let math = import("zr.math");`使用已注册标准模块，不需要重复写`native extern`。`native extern`用于声明项目自己的native ABI或生成binding；标准模块自身的C descriptor仍需导出与Canonical CallableContract一致的machine-readable metadata。静态链接、动态库和`.zrm` side-by-side部署只是provider差异，不改变公开ModuleId。

## 5. ModuleSpecifier

### 5.1 输入类别

```text
BuiltinOrAbsolute:  zr.math | core.math.quaternion | core/math/quaternion
Relative:           .math.quaternion | ./math/quaternion
ParentRelative:     ..math.quaternion | ../math/quaternion
Alias:              #lib | #lib.tool | #lib/tool
Package:            @math | @math.matrix | @math/matrix
AssemblyLocator:    artifacts/physics.zrm
```

Parser 不直接拼接文件路径，而是生成：

```text
ModuleSpecifier {
  kind;
  root;
  segments[];
  artifactExtension?;
  spelling;
}
```

`.` 与 `/` 在 segment 区域等价。相对前缀是独立 token 规则：单个 `.`/`./` 表示当前模块目录，连续两个 `..`/`../` 表示上级；它们不能被误拆成空 segment。

### 5.2 等价与规范化

```text
core.math.quaternion  == core/math/quaternion
.math.quaternion      == ./math/quaternion
..math.quaternion     == ../math/quaternion
#lib.tool              == #lib/tool
@math.matrix           == @math/matrix
```

Canonical ModuleId 使用 `.` 连接逻辑 segments，例如 `core.math.quaternion`。文件 provider 可以映射到 `core/math/quaternion.zr`；分隔符等价不代表源语言接受 OS 绝对路径。

`import("physics.zrm")` 或带目录的 `.zrm` literal 是显式 assembly locator：resolver 读取 `META-INF/zrm.json`，解析 default entry 或清单指定 module。无相对前缀的locator从`.zrp`项目根/artifact roots解析；`./`、`../`从importing source目录解析但必须受workspace/dependency root约束；OS绝对路径和越界路径被拒绝。`.zrm`内部模块依赖仍使用Canonical ModuleId，不把ZIP member path暴露为源码语义。

### 5.3 `#alias`

`.zrp` alias key 必须为 `#identifier`；value 是逻辑 module prefix。`#lib/tool` 在 segment 边界替换为 `core/lib/tool`。禁止递归 alias、最长字符串匹配和 alias 指向 package/version表达式。

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
  "assembly": {
    "name": "physics",
    "output": "dist/physics.zrm"
  }
}
```

约束：

- `aliases`、`dependencies`、`package.exports` 是不同 namespace，不允许互相抢前缀。
- `name/version/description/license/authors/repository/keywords/engines`构成发布元数据；`name`是项目/发布显示名，不承担import package root，后者只来自`package.name`。
- dependency key 必须满足单段 package 语法；每项至少给出 version requirement，并由 registry/path/git 等单一 provider 提供详细来源。发布/锁定阶段记录 resolved version、content integrity 和 transitive graph。
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
  -> normalize separators and relative base
  -> expand exactly one #alias or @package root
  -> enforce package exports / workspace root
  -> choose source, .zro, .zrm or native provider
  -> produce Canonical ModuleId + ModuleIdentity
  -> record dependency and compatibility hashes
```

`ModuleIdentity` 至少包含 Canonical ModuleId、package identity/version（若有）、provider kind、artifact identity/generation 和 public contract hash。缓存、循环检测、反射、LSP definition 与 debugger source lookup 都以该 identity 工作，不能以用户输入字符串为 key。

解析错误分层为 invalid specifier、unknown alias/package、package export denied、module not found、ambiguous provider、artifact incompatible、native capability unavailable。每类错误必须携带原 literal range 和已解析的 root/segments。

## 8. LSP、反射与 debug

- hover import alias显示原 spelling、Canonical ModuleId、package/version、provider 和 readonly namespace type。
- completion 在 `#` 后仅列 alias，在 `@` 后仅列 dependencies；选中 package 后只列 exports。
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

读取旧 `.zrp` 可以有版本化兼容窗口；目标源码、目标 artifact 与 formatter 不输出旧形式。迁移后二次运行必须幂等。

## 10. 实施里程碑与验收

1. **M1 specifier foundation**：加入 ModuleSpecifier/ModuleIdentity、分隔符等价、relative、`#alias`、单段 `@package` parser 和 resolver 单元测试。
2. **M2 manifest/artifact**：实现 `.zrp` v2 reader/writer、exports、dependency lock identity、`.zrm` entry 与 `.zro` dependency roundtrip。
3. **M3 native contract**：实现 NativeExtern AST、CanonicalCallableContract 到 FfiSignature 的静态校验和 VM/AOT 共享 golden。
4. **M4 native library convergence**：登记 `zr.xxx` inventory，把 `debug` 迁移到 `zr.debug`，接入 reflection/pooling。
5. **M5 consumers/migration**：LSP、reflection、debug、formatter、migration tool 只消费 canonical identity/schema。

晋级要求：

- 本文全部等价 import 对产生相同 Canonical ModuleId；不同 package version 不产生相同 ModuleIdentity。
- 非法package root/空module segment、未导出子模块、越出workspace root、alias cycle和provider ambiguity都有稳定负例。
- `.zr`、`.zro`、`.zrm` 与 native provider 的 import 结果共享 public contract hash 检查。
- VM/libffi 与 AOT 对 scalar、struct、in/ref/out、callback、错误路径使用相同 FfiSignature 测试向量。
- resolver、LSP、debug、reflection 内不存在独立字符串切分副本。
- 仓库目标 fixture 不再出现 `%extern`、旧 `@alias`、`$dep` 或 `&dep`。

## 11. 参考依据

- CPython `Python/import.c::resolve_name`：相对层级先结构化解析，再交给 module loader；ZR 采用这一分层，不复制 Python 的 package object 语义。
- QuickJS module normalize/load callback：specifier normalization 与 provider 加载分离；ZR 在两者之间加入 `.zrp` exports 与 ModuleIdentity。
- JDK `ModuleFinder`/module descriptor：稳定 module name、descriptor 与 container 分离；ZR 的 `.zrm` 同样不把物理 member path 当公共 identity。
- .NET `DllImportAttribute`、`LibraryImportAttribute`、`CallingConvention`、`UnmanagedCallConvAttribute` 与 `MarshalAsAttribute`：静态 native contract显式保存 entry、ABI 与 marshalling。
- Rust `ExternAbi` 与 target call-convention lowering：源码 ABI 必须在 target lowering 前成为结构化 contract。

这些参考实现位于仓库 `lua/` 镜像中。ZR 的关键差异是：第三方包 root 使用单段 `@identifier`，workspace alias 使用 `#identifier`，二者在词法阶段即可区分。
