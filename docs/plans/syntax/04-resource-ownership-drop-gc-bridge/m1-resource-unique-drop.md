# 04-M1 resource/Unique + Drop 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md`
的 `M1 resource/Unique + Drop`。

## 状态与产出记录

- 完成时间：2026-07-21 16:48 +08:00
- 状态：已完成
- 完成项目：
  - 已接入 contextual `resource class`、`own T(...)`、`drop(owner)`，AST/prototype 使用共享
    resource modifier 表达生命周期 world；resource 只能由 `own` 构造，ordinary class 不能
    使用 `own`；普通 `resource(value)` / `own(value)` identifier call 保持有效。
  - 已冻结 `Unique<T>` 非 nullable 赋值规则；canonical ownership dataflow 的 move violation
    通过结构化 `kind/qualifier/isViolation/range` fact 门禁 compiler，不按 code/message/name/source
    推断。
  - direct resource Unique runtime value 只携带 resource pointer/handle，`ownershipControl=null`；
    move 清空源，普通构造/move/drop 路径不创建 hidden refcount/control block。
  - 已统一正常退出、early return、throw、break、continue 与 explicit drop；构造 seed 在 call 前
    armed，成功后 move 到最终 slot，异常 unwind 只逆序 Drop 已初始化字段且跳过 custom Drop。
  - 已实现 resource lifecycle state 与幂等/reentrant Drop；full Drop 顺序为 custom Drop、derived
    fields reverse order、base fields reverse order。
  - 已用 CFG may-enter-catch fact 拒绝可能抛错的 resource custom Drop，普通 GC class destructor
    不受该门禁影响。
  - 已让 VM、Semantic IR、`.zri`、AOT C 与 AOT LLVM 保留同一 `OWN_UNIQUE`、`OWN_RELEASE`、
    `MARK_TO_BE_CLOSED`、`CLOSE_SCOPE` contract，并执行 Drop log `21`。
  - 已将 direct-resource lifecycle 从超过约 1000 行的 `ownership.c` 拆分到
    `ownership_resource.c` 与窄 internal API，control-block orchestration 保留在原模块。

## 当前验证结果

- GCC 11.4、Clang 14.0、MSVC 19.44 使用独立 build 目录，各 11/11 target 真实进程
  exit 0；每套合计 388 Tests、0 Failures、0 Ignored。
- 每套 target 集：resource Unique Drop 13/13、parser 75/75、type inference 119/119、compiler
  integration 127/127、semantic query diagnostics 33/33、dataflow 9/9、closure capture 1/1、
  exceptions 8/8、AOT ownership 1/1、AOT scope 1/1、resource ExecBC/AOT pipeline 1/1。
- resource focused 覆盖 contextual identifier fallback、type-directed world、无 control block、move/source invalidation、
  explicit/scope Drop、nested field reverse order、partial construction、custom Drop non-throw、
  return/throw/break/continue cleanup、parameter/return move 与 non-nullable invariant。
- AOT focused 同时检查 function tree、Semantic IR、intermediate ownership op、generated C/LLVM
  helper，并通过真实 VM execution 验证 Drop order。
- compiler integration 初次 broad regression 暴露 legacy using marker 污染 8 项；修复为只对
  canonical resource prototype 注册 close marker 后，三工具链均恢复 127/127。

## 当前实现边界

- M1 不宣称 Shared/Weak resource control lifetime、owner borrow/receiver 或 Gc/GcBox bridge；
  它们分别属于 M2、M3、M4。
- direct resource storage 当前暂时通过 existing GC ignore registry 保活，Drop 后归还 GC；
  M4 必须用显式 domain/bridge identity 替换，并满足 no-hidden-ignore-registry gate。
- M1 晋级声明严格限定为：没有 borrow/shared/GC bridge 参与时，direct Unique 无 hidden
  refcount/control block，且 VM/AOT Drop 顺序一致。
