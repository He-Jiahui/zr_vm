# AOT 10：反射、动态构造与调用

## 公开语义

反射由`zr.reflection`提供，声明分类位于`zr.reflection.declaration`。`typeid(TypeRef)`返回轻量稳定TypeId；`typeof(expr)`返回运行时最精确Type descriptor。AOT不得把二者合并。

动态构造仅通过：

```zr
let type = reflection.requireConstructible(runtimeType);
let value: object = type.createInstance(...constructionArgs);
```

`init/new/own/@call`不能接受runtime Type绕过该边界。

## AOT contract

- metadata token/TypeId定位Type、field、property、method、constructor和meta。
- reflection root决定哪些member保留；未root的member被裁剪后必须稳定返回not found。
- `createInstance`按public constructor和runtime argument TypeId vector绑定；class返回GC object，struct返回boxed object。
- ref struct、resource class、interface、abstract、open generic拒绝createInstance。
- property reflection使用统一Property AST/metadata，区分get/set/ref-get与accessibility。
- method invoke thunk由Canonical CallableContract生成，支持generic context与异常清理。
- cache key包含TypeId、argument TypeIds/null marker、module generation和binder policy。

## AOT模式

full metadata、trimmed metadata与explicit preserve三种模式共享同一查询API。AOT可以预生成constructor/invoke thunk，但不能让“恰好生成的symbol”替代metadata可见性规则。

## 实施阶段与API闭环

| 阶段 | 交付 | 必须拒绝/保留 |
|---|---|---|
| R1 Type hierarchy | `Type`、TypeOf与declaration分类、`typeof/typeid/resolve` | open/invalid token、runtime generation drift |
| R2 member query | field/property/method/meta、visibility与inheritance filter | duplicate/ambiguous、trimmed不可见member |
| R3 construction | ConstructibleType binder、`createInstance(...object)`与cache | resource/ref struct/interface/abstract/open generic；ctor throw cleanup |
| R4 invocation | method/property ref-get invoker、generic context、marshal | ref/out/owner/exception不兼容 |
| R5 module service | `zr.reflection`/`.declaration`标准import、per-runtime generation/cache | forged runtime pointer、stale/replacement service |
| R6 AOT/trim | invoker/constructor closure、preserve roots、diagnostics | symbol存在但metadata未root时仍不可见 |

现有token/generic记录只覆盖R1/R5的一部分。验收入口包括`tests/module/test_reflection_token_resolve.c`、`tests/module/test_reflection_dynamic_generic_instance.c`、`tests/module/test_reflection_dynamic_generic_method_context.h`及AOT 10 acceptance系列。

最终退出：`typeof`返回最精确descriptor、`typeid`只提供identity；member/property query统一；createInstance class/boxed struct成功且非法类别稳定失败；source/binary/AOT/trim模式返回一致可见集合；cache按ModuleIdentity/generation正确失效。

## Syntax 上游追踪

| Syntax 节点 | 本计划消费的稳定输入 | 本计划退出责任 |
|---|---|---|
| [01/M4-M5](../syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md) | canonical artifact/query projection | reflection consumer 只读 versioned schema，不从 runtime shape 补语义 |
| [03/M1-M2](../syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md) | TypeLayout 与 receiver effect | descriptor layout/effect 与 backend hash 一致 |
| [04/M7](../syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md) | ownership/domain artifact projection | resource/GC bridge category、visibility 与 lifecycle 边界 |
| [05/M5](../syntax/05-property-unified-ast/m5-property-consumers-reflection-migration.md) | unified PropertyDef/accessor identity | get/set/ref-get query/invoker 与 trimming roots |
| [08/M1-M5](../syntax/2026-07-19-08-reflection-library-type-system-design.md) | Type/TypeOf/member/construct/invoke/preserve contracts | full/trim/preserve AOT reflection service 闭合 |
| [09/M4](../syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md) | pool descriptor/provider contract | pool type/member query 与 native service identity |
| [10R/M2、10C/M4-M5](../syntax/2026-07-19-10-native-ffi-module-package-design.md) | ModuleIdentity/provider phase 与 convergence | runtime-bound service/cache、provider-owned TypeIds、stale rejection |
| [11/M3-M5](../syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | static metadata roles 与 generated declaration identity | generated member visibility、attribute rows 与 preserve roots |
| [12/M6](../syntax/2026-07-20-12-async-task-job-scheduler-design.md) | task artifact/debug projection | Task/Job/frame descriptor visibility、generation 与 trim behavior |
| [13/M4](../syntax/2026-07-20-13-iterator-enumerator-yield-design.md) | iterator artifact/debug projection | iterator carrier/frame descriptor、resume roots 与 trim behavior |
| [14/M1、M4](../syntax/2026-07-20-14-test-function-harness-design.md) | TestManifest/test metadata role | test build 可见、production trim、debug/LSP consumer parity |

逐节点阻塞和证据边界见[完整追踪矩阵](./syntax-contract-traceability.md)。

## 完成记录

- [Token/generic reflection baseline](./10-reflection/2026-07-19-token-and-generic-reflection-baseline.md)
- [Constructed generic method object](./10-reflection/2026-07-19-constructed-generic-method-object.md)
- [MakeGenericMethod object](./10-reflection/2026-07-19-make-generic-method-object.md)
- [Argument object decoding](./10-reflection/2026-07-19-generic-method-argument-object-decoding.md)
- [Native entry](./10-reflection/2026-07-19-generic-method-native-entry.md)
- [Runtime-bound `zr.reflection` module](./10-reflection/2026-07-19-runtime-bound-reflection-module.md)
- [Target-owned service cache](./10-reflection/2026-07-19-target-owned-reflection-module-cache.md)

Type/TypeOf层级、`typeof/typeid`、member/property query、`createInstance`、普通import bridge、Invoke与trim diagnostics仍按本文晋级门实施。
