---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_emitter.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_cleanup_registration.c
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_emitter.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_sync.c
tests:
  - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c
  - zr_vm_aot/tests/parser/test_execbc_aot_manual_opcode_sync.c
  - zr_vm_aot/tests/parser/test_known_call_pipeline.c
  - zr_vm_aot/tests/parser/test_meta_call_pipeline.c
  - zr_vm_aot/tests/parser/test_tail_call_pipeline.c
  - tests/parser/test_aot_c_frame_setup_contracts.c
  - tests/parser/test_aot_c_metadata_binding_loader.c
  - tests/parser/test_call_binding_aot_projection.c
plan_sources:
  - user: 2026-09-10 持续扩充 ZrVm Wiki 的语言实现与 C API 说明
  - docs/plans/aot/index.md
  - docs/plans/aot/02-typed-value-and-layout.md
  - docs/plans/aot/04-semir-and-c-backend.md
  - docs/plans/aot/syntax-contract-traceability.md
doc_type: implementation-reference
---

# AOT Lowering、运行时 Helper 与注册参考

**状态：`experimental`。** ZrVm 当前具备 C/LLVM AOT 的可运行实现和测试路径，但不同语言能力的
完整 lowering 覆盖、工具链和 ABI 演进仍需以本次构建的 writer 选项、artifact 和测试矩阵为准。
本页解释生成代码与 VM 之间的实际契约，不把“能够生成 C 文本”误写为“所有程序都可 full-AOT”。

前置阅读：[AOT C/LLVM 后端](10-aot-backends.md)、[Semantic IR、CFG 与数据流事实参考](08-compiler-semantic-ir-facts-reference.md)、
[AOT ABI](05-interop/aot-abi.md)、[ABI 与兼容性参考](06-reference/abi-compatibility-reference.md)。

## 1. AOT 的正确边界

AOT 是同一份已验证语义的另一种执行投影。它不重新解析 ZR 源码，不重新定义所有权、异常、
module 或 property，也不以 C 类型名替代 canonical type identity。

```text
.zr source
  -> parser/binding/canonical Type + SemIR/CFG facts
  -> compiler function + ExecIR/ExecBC projection
  -> writer artifact and reachability set
  -> C emitter or LLVM emitter
  -> generated object/shared library + code registration tables
  -> project/AOT loader validation
  -> ZrLibrary_AotRuntime helpers + generated entry thunk
```

每个箭头都保留需要的 identity：function index、layout id、signature、native import contract、
module/artifact identity、call binding 和 debug/observation metadata。生成器不能从 AST token、
旧语法、runtime object shape 或字符串化类型重新“猜”语义。

## 2. 产物、注册表与加载时验证

### 2.1 生成物中通常包含什么

| 数据 | 目的 |
| --- | --- |
| entry thunk 表 | 将 function index 映射为 `FZrAotEntryThunk`。 |
| `SZrAotMethodInfo` | function 元数据、签名、frame/root/layout、generic/reflection 信息。 |
| `SZrAotCodeRegistration` | 模块与 code/method/native-import 注册信息的聚合。 |
| `SZrAotGcRootMap` | generated frame 中可被 GC 扫描的 slot/address。 |
| generic dictionary | type layout、prototype、method、box type 或 `sizeof` 等实例化依赖。 |
| native import contract | AOT 调用的 native module/function ABI、schema 与 capability 约束。 |
| debug/observation metadata | instruction index、source line、可观测 step 策略。 |

`FZrAotEntryThunk` 的函数类型为：

```c
typedef TZrInt64 (*FZrAotEntryThunk)(SZrState *state);
```

该返回值属于 AOT entry ABI；语言层的任意 ZR return 值仍由 generated frame、return helper 和
call-info 约定处理。不要将 thunk 的 `TZrInt64` 直接解释为所有 ZR 函数的唯一返回类型。

### 2.2 加载不能跳过 ABI/identity 检查

加载器必须同时验证 artifact schema、AOT ABI、runtime ABI、layout/signature/call-binding identity、
native import contract 和 module identity。只比较动态库文件名或导出符号名不足以证明兼容。

`ZrLibrary_AotRuntime_ConfigureGlobal` 选择项目执行模式（解释器、binary、AOT C 或 AOT LLVM），
`ZrLibrary_AotRuntime_ExecuteEntry` 从当前 project/global 上执行已选入口。`requireAotPath` 或
writer 的 full-AOT 要求开启时，缺失 lowerings/registration/contract 必须成为失败，而不是静默
回到解释器后仍报告为 AOT 成功。

## 3. Generated frame：AOT 与 VM 共用的执行容器

生成函数通过 `ZrAotGeneratedFrame` 与 runtime 协作。它包含：

- `recordHandle`、`function`、`callInfo`、`slotBase`：把生成代码接回当前 VM 调用帧；
- `functionIndex`、`currentInstructionIndex`、`lastObservedInstructionIndex/Line`：定位执行与调试；
- `generatedFrameSlotCount`：frame slot 数量，不能由 C 局部变量数量猜测；
- `module`、`moduleExecuted`、function table/count：模块初始化与函数解析；
- `codeRegistration`、thunk table/count：已验证的生成代码表；
- observation mask / `publishAllInstructions`：调试和 profile 的观测策略。

`ZrAotGeneratedModuleContext` 是模块级解析视图，带 metadata function、method info、module、
function table、registration 和解析后的 function index。它的地址仅适合当前 helper/entry 执行期，
不得缓存到后续项目 reload、state exit 或另一个 global。

### 3.1 函数入口协议

生成入口应由 runtime 建立 frame，而不是在 C 代码中手写 `SZrCallInfo` 或直接偏移 state stack：

```c
ZrAotGeneratedFrame frame;

if (!ZrLibrary_AotRuntime_BeginGeneratedFunction(state, functionIndex, &frame)) {
    /* state 已携带失败/异常状态；generated code 立即走失败出口。 */
    return 0;
}

/* 对每个可观察 instruction 使用 BeginInstruction，并调用对应 helper。 */
return ZrLibrary_AotRuntime_Return(state, &frame, resultSlot, ZR_FALSE);
```

这只是协议形状，不是独立可编译模板。真实 emitter 会同时生成 metadata、root map、label/resume
结构和适当的 return/publish 参数。不要在手写 native 插件中伪造 generated frame；插件应使用
[Native Call Context 与回调 C API](05-interop/native-call-context-reference.md)。

### 3.2 可观测指令

`ZrLibrary_AotRuntime_DefaultObservationMask()` 默认包含：

- `ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW`；
- `ZR_AOT_GENERATED_STEP_FLAG_CONTROL_FLOW`；
- `ZR_AOT_GENERATED_STEP_FLAG_CALL`；
- `ZR_AOT_GENERATED_STEP_FLAG_RETURN`。

`SetObservationPolicy` / `ResetObservationPolicy` / `GetObservationPolicy` 控制当前 state 的观测。
`BeginInstruction` 在生成代码进入一条可能需要观测的指令时记录 index/flag。优化不能绕过这些
边界，否则 debugger、coverage/profile 或 exception resume 会和解释器产生不同可见行为。

## 4. 从 SemIR/ExecIR 到 helper 的 lowering 矩阵

以下表不是“每条 opcode 必定调用一条 helper”的承诺，而是生成代码应保留的语义分工。

| 语义类别 | 生成代码的典型工作 | AOT runtime 协作 |
| --- | --- | --- |
| 常量/slot | 把 constant 或 slot id 投影为 frame 操作。 | `CopyConstant`、`SetConstant`、`CopyStack`、`GetStack`、`ResetStackNull*`。 |
| 数值与转换 | 对已知标量可生成直接 C/LLVM 算术；保持语言语义检查。 | `ConvertGenericTo*`、`GenericNumericAdd/Sub/Mul/Div/Mod/Neg/Power`、`ToBool/ToInt/ToUInt/ToFloat`。 |
| 值/集合构造 | 根据 canonical layout 构造 object、array 或 inline array。 | `CreateObject`、`CreateArray`、`CreateInlineArray`、`BindInlineArrayElementPlace`。 |
| member/property | 使用已知 cache/member id 或调用动态/meta contract。 | `MetaGet*`、`MetaSet*`、`GetMember*`、`SetMember*` 等 helper。 |
| ownership | 把 unique/share/degrade/wake/GC boxing 映射为规定的 transition。 | `OwnUnique` 等 ownership helper；不能用普通 C copy 偷换 move/drop。 |
| 控制流 | C label/LLVM basic block 反映已验证 CFG。 | branch/switch 和 instruction observation/resume contract。 |
| iterator | 保留 protocol 和 suspension 完成语义。 | `IterInit`、`IterMoveNext`、`IterCurrent` 等 runtime lane。 |
| module/export | 只在模块执行边界发布已构造 exports。 | `PublishModuleExports`、module loader/context helper。 |

对 inline aggregate，emitter 必须携带 destination type layout id、byte offset、byte size；不能把
它退化为无类型 `void *` 并丢弃 copy/drop/GC scan contract。

## 5. 调用：直接路径、通用路径与 deopt

调用是 AOT 最容易出现“快路径看似正确、边界即错误”的区域。runtime 将它分为三类。

### 5.1 已准备的直接调用

`ZrAotGeneratedDirectCall` 保存 native thunk、caller/callee call-info、caller/callee function index、
call/resume instruction index、观测快照和 `prepared` 标记。生成器可用：

```c
ZrLibrary_AotRuntime_PrepareDirectCall(state, &frame,
                                       destinationSlot, functionSlot,
                                       argumentCount, &directCall);
/* 若已准备且契约仍有效，可运行 thunk；随后一定完成/恢复。 */
ZrLibrary_AotRuntime_FinishDirectCall(state, &frame, &directCall, resultCount);
```

对于静态已知 callee，可使用 `PrepareStaticDirectCall` 或 `CallStaticDirect`；meta 调用使用
`PrepareMetaCall`。这些 API 的存在不代表 emitter 可以跳过 receiver、argument count、return
destination、exception 或 resume index 的维护。

### 5.2 prepared-or-generic

`CallPreparedOrGeneric` 和 `CallPreparedOrGenericWithResume` 允许 runtime 在准备的 fast path 无法
维持时回到通用 call 语义。`CompletePreparedDirectCallWithResume` 将 invocation 成功/失败和继续
instruction 统一收束。生成的 C/LLVM 代码应把返回的 `TZrBool` 作为控制流条件，而不是在失败后
继续读取 destination slot。

### 5.3 deopt 不是异常吞没

`CanUseTypedDirectCall` 只回答当前能否使用 typed direct call。若 guard、contract、动态 callable
或 layout 不再适用，调用：

- `DeoptTypedDirectCall`：从 typed direct call 回到正确的通用/动态语义；
- `CallDynamicDeoptBridge`：为带 `deoptId` 的动态调用进入 bridge；
- `ValidateDynamicDeoptBridge`：验证该 bridge 能否安全使用；
- `CallInlineStructDynamicDeoptBridge`：保留 inline destination layout 的回退。

deopt 保留程序语义，不表示成功执行，也不等价于忽略错误。`UnsupportedMetaCall`、
`UnsupportedMetaValueAccess`、`UnsupportedDynamicValueAccess` 是明确的 unsupported/fallback
边界；是否允许 fallback 由 writer/执行模式决定。

## 6. 异常、pending control 和 cleanup

生成代码必须和解释器共用控制转移模型：

| 语言事件 | AOT runtime API | 需要保留的东西 |
| --- | --- | --- |
| try 进入/结束 | `Try`、`EndTry` | handler index、frame 和 cleanup boundary。 |
| throw/non-null check | `Throw`、`RequireNonNull` | source slot 与 `outResumeInstructionIndex`。 |
| catch | `Catch` | exception 到 destination slot 的物化。 |
| finally | `EndFinally` | pending control 后的 resume target。 |
| return/break/continue | `SetPendingReturn`、`SetPendingBreak`、`SetPendingContinue` | 目标 instruction，不能跳过 finally/drop。 |
| resource close | `MarkToBeClosed`、`CloseScope` | 所有已登记 cleanup 的逆序关闭。 |

`ZR_AOT_RUNTIME_RESUME_FALLTHROUGH` 表示“按普通顺序继续”的特殊 resume 值；它不是任意
label 编号。emitter 必须区分它和真实 instruction index，并把 helper 失败/异常路径导向生成的
resume/cleanup code。

## 7. GC root、layout 与同步

AOT frame 的 object/value slot 必须由 `SZrAotGcRootMap` 描述，进入函数时 push root frame，正常
返回、异常、deopt、tail-call 或取消路径都要 pop。根位置可通过 frame byte offset 或局部地址
描述，但都必须和已验证 layout id、field offset 一致。

生成代码不能假设：

- C 局部变量天然被 VM GC 看见；
- 一次 helper 调用不会分配或 safepoint；
- inline struct 可当作普通 object pointer；
- stack slot 的地址跨 helper 永远稳定。

`SyncSignedIntLocal`、`SyncUnsignedIntLocal`、`SyncFloatLocal`、`SyncBoolLocal` 等 helper 用于
把生成的标量局部和 runtime slot 语义同步。用直接 C 变量优化时，也必须在 call、throw、debug、
GC 和 resume 可观察边界按 emitter contract 同步。

## 8. Native import 与模块初始化

`ZrLibrary_AotRuntime_ResolveNativeImportContract(codeRegistration, functionIndex, localContractIndex)`
从生成 registration 获取 native import contract；
`FindNativeImportContract(state, function, localContractIndex)` 从当前运行时 function 侧查询。
它们是 AOT/native 边界的身份依据，不能靠 module/name 字符串拼接替代。

模块执行时，`ZrLibrary_AotRuntime_ModuleLoader` 作为 loader bridge；生成 frame 只在 module 已准备
且其 registration/metadata 已通过校验时使用 function table。`PublishModuleExports` 在 exports
构造完成后发布；`Return(..., publishExports)` 把 module return 与发布边界统一起来。重复执行、
循环 import、失败 module 或 reload 都应遵守 project/module runtime 的状态机，而不是在生成
function 中直接改 global cache。

## 9. Reachability、反射和 stripping

AOT 的可达集至少需要包含：

- entry function 和显式 export；
- native import/callback、module initializer；
- 反射可见的 type、constructor、field/property/method；
- generic dictionary、TypeLayout、box/GC scan helper；
- resource Drop/cleanup、test manifest roots、AOT registration tables。

只从静态 call graph 裁剪会错误删除反射、dynamic/meta、native callback 和 module export 入口。
如果生成器发现一个能力必须依赖解释器 fallback，应在 artifact/writer diagnostic 中明确记录；
full-AOT 模式将其作为拒绝条件。

## 10. 给 generated-code 作者的检查表

1. 输入来自已验证 canonical type、SemIR/CFG、layout 和 call binding，而非 token/名称猜测。
2. 每个 generated function 通过 `BeginGeneratedFunction` 建立 frame，并有对称的 return/cleanup 路径。
3. 每个可见 instruction 根据 observation policy 调用 `BeginInstruction`。
4. managed slot 都在 GC root map 中，且任何退出路径都会移除 root frame。
5. typed direct call 有 `CanUse`/prepare/finish 或正确的 deopt bridge，不把 guard miss 当 success。
6. try/finally/return/break/continue 通过 AOT runtime pending-control helper，不手写无 cleanup 的 C `goto`。
7. 模块 export 在正确的 publish 边界发布，native import 通过 registration contract 解析。
8. 生成产物、AOT ABI、runtime ABI、artifact schema 和 native ABI 一起验证。

## 11. 验证策略

变更 emitter 或 helper 时，至少运行本页 Front Matter 中的 ExecBC/AOT pipeline、known/meta/tail
call、frame setup、metadata loader 和 call-binding projection 测试。新增语言语义还应有一组
解释器与 AOT 输出一致的测试，以及一个故意触发 unsupported/deopt/异常 cleanup 的负向用例。
不要用“生成的 C 能编译”代替运行时语义验证。
