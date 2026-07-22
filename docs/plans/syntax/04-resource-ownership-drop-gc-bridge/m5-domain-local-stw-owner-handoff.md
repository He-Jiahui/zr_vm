# 04-M5 domain-local STW、多 mutator 与同域 owner handoff 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md`
的 `M5 domain-local STW、多 mutator 与同域 owner handoff`。

## 状态与产出记录

- 完成时间：2026-07-22 13:22 +08:00
- 状态：已完成
- 已完成项目：
  - 已建立每domain mutator registry、stable mutator id、safepoint epoch、pause depth与
    `AttachedInactive/Running/Parked/BlockingDetached/NoSafepointCritical`状态机；attach/
    detach、nested VM/native execution depth与pause-boundary entry均按exact state identity协调。
  - 已让 collector只等待当前domain registered mutators；其他domain progress不被该handshake
    暂停。timeout structured diagnostic包含blocking mutator id、native mode、exact state与
    native call-frame identity，不能按线程名/native name/message重建。
  - 已把full、generational minor、major remark与compact work包在domain-local pause中；
    parked mutator发布的VM stack/state、AOT root frame、call-info function、exception与pending
    control在mark/relocation时统一扫描和重写。
  - 已让解释器`ZrCore_Execute`使用nested mutator execution depth，按固定fetch预算poll；park前
    保存下一条instruction与frame top，恢复后重载movable function/callable/frame cache。
    AOT继续使用同一`ZrCore_Gc_SafePoint`和root-map合同。
  - 已在native descriptor `dispatchFlags`上冻结`GcAware`默认、`BlockingDetached`和
    `NoSafepointCritical`三种mode；generic/cached/inline/pinned/known-native direct/readonly
    index fast callback均统一执行NativeEnter/Leave，cached path不丢descriptor flags。
  - 已关闭native recover-point unwind scope缺口：只在native longjmp/throw边界按exact state
    identity清零被放弃的VM execution/native depth并恢复inactive mutator状态；普通脚本异常路径
    不触发该reset，也不按异常code/message/native function name判断。
  - 已实现opaque same-domain `TransferEnvelope`的Prepared/Queued/Claimed/Committed/Aborted
    状态机；prepare把direct resource Unique从source Place move进exact ownership root，publish/
    claim使用release/acquire，worker id+claim epoch拒绝stale completion。
  - 已固定same-domain Unique O(1) object/owner identity move，不复制payload、不clone、不创建
    Shared/refcount；queue close、worker exit、throw/abort/commit race均只存在一个terminal owner，
    source保持Moved且Drop count恰好为1。
  - 已保留架构边界：本阶段未接入TaskScheduler/`zr.thread`，不发布Send/Sync，不实现cross-
    domain ValueCopy/StructuredClone/ImmutableHandle/ResourceMove；这些继续由M6及Syntax12消费。

## 最终验证结果

- 固定source snapshot为`HEAD 3db282e12a60d831ed1de1fc58286779e0e5b3e0 + 24个M5
  code/test/CMake exact overlays`；Unity及4个第三方source目录只读复制，不包含LSP路径、三份
  既有dirty Syntax草案、根目录生成物或build输出；24/24 overlays与共享工作树逐字节一致。
- GCC 11.4与Clang 14.0在WSL-native fresh目录分别完成原始657-step构建，MSVC 19.44.35228
  在独立Windows Ninja目录完成原始666-step fresh构建；加入unwind closure后，三套均在刷新
  snapshot/工作树上重建受影响目标并完整重放同一20-target矩阵，分别20/20真实进程
  `exit 0`、Unity合计530/530、0 failure。
- focused M5 suites在三工具链分别为：domain bridge 5/5、multi-mutator 11/11、resource
  Unique 19/19、Shared/Weak 11/11、owner borrow 6/6、same-domain handoff 7/7、native
  descriptor 5/5、known-native direct fast path 61/61。
- expanded regression还覆盖GC 66/66、pre Semantic IR 11/11、type inference 119/119、
  compiler integration 127/127、canonical consumer 15/15、dataflow 9/9、closure 1/1、
  exception 8/8、AOT ownership 1/1、AOT root frame 6/6、instruction execution 31/31和
  lexer/parser/compiler execution 11/11。
- TDD证据包含nested execution depth RED（9项中仅nested enter失败）、active-pause nested
  entry RED（10项中仅pause boundary失败）及随后10/10 GREEN；新增native recover-point
  unwind RED为11项中仅scope reset失败，`exception.c`最窄recover-point支持后11/11 GREEN；
  exception 8/8与instruction execution 31/31 focused回归同步保持GREEN。

## 当前实现边界

- 当前domain topology仍以一个`SZrGlobalState`拥有一个GcDomain为host基线；M5允许同domain
  attach多个state/mutator，但不提供public topology builder或跨global共享heap API。
- `BlockingDetached`由provider负责在enter前发布/固定roots；`NoSafepointCritical`要求短时且
  禁止未pin interior pointer逃逸。runtime执行mode/timeout合同，不分析native callback body。
- TransferEnvelope当前仅接受same-domain direct resource Unique并由runtime manager分配；
  scheduler未来可共分配/池化，但不得复制payload、改变transfer id或自动retry committed Job。
- M6继续实现cross-domain transport kind、provider prepare/commit/abort、quota、stale
  generation与domain shutdown；Syntax12只能在本M5 gate之后消费same-domain scheduler底座。
