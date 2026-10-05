---
related_code:
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_library/src/zr_vm_library/project/project.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
implementation_files:
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
plan_sources:
  - user: 2026-10-04 全仓 caller-first 中文意图注释与公共调用契约静态审查
tests:
  - tests/library/test_close_proxy_aot_runtime.c
  - tests/parser/test_aot_llvm_symbol_stripping.c
  - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c
doc_type: module-detail
---

# AOT 运行时适配层

## 调用目的与分层

适配层让生成函数以 VM 的模块、帧、值槽、所有权和异常状态执行，供宿主装载 AOT 模块及生成代码调用。公开 ABI 声明不是每个入口都已由当前生成器消费的证明。当前 CLI 的普通项目执行路径不能当作 ConfigureGlobal/ExecuteEntry 的直接调用；宿主嵌入接口、生成器输出、人工帧测试及归档生成文件分别承担不同的消费证据。

| 分区 | 调用目的 | 主要契约 |
| --- | --- | --- |
| 装载、ABI、模块记录 | 把 provider 产物校验、注册并关联项目状态 | 描述符与生成表借用；记录拥有 native functionTable 指针数组，数组元素由 VM pin 维持 |
| 帧、调用、异常和 ownership | 在生成代码与 VM 调用帧之间转换，并保持清理登记 | frame/slot 借用受当前 VM 栈与 invocation scope 限制；回调可能扩栈 |
| 数值与转换 | 为选定标签实现快速操作或元方法分派 | 每个 helper 的输入标签分别核对；C writer 的 Generic* 路径不能当作旧数值 helper 的直接调用 |
| 成员、数组与迭代 | 桥接对象运行限制、属性回调、容器能力和迭代协议 | 编译访问上下文须合法；core 的实际限制不等于完整 private/protected 授权检查 |

## 配置与模块寿命

ConfigureGlobal 设置运行时入口所需配置，ExecuteEntry 驱动对应模块入口。调用方须提供仍有效的项目和模块描述存储；借用的 registration 表、字符串、thunk 和元数据在消费期间不能失效。装载状态还记录初始化是否完成，moduleExecuted 是记录布尔值；它与 context/frame 中指向记录状态的借用指针角色不同。见 `zr_vm_library/include/zr_vm_library/aot_runtime.h:148`、`zr_vm_library/include/zr_vm_library/aot_runtime.h:206`。

FreeProjectState 在项目释放路径中直接被调用，测试还核对裁剪后的表；它不是仓内无消费者的保留声明。释放时应遵守当前项目/运行时顺序和该入口的清理契约（`zr_vm_library/include/zr_vm_library/aot_runtime.h:155`）。已有 records/context 搬迁 TODO 仍须补合法递归导入、记录扩容及后续访问的完整触发链；本次不授运行缺陷信用。

## 帧、根与调用返回

BeginGeneratedFunction 建立生成函数适配状态；FinishDirectCall 结束直接调用时要求仍处于对应 callee，并匹配 caller/frame 凭据。VM frame、slotBase 和 Value 地址是受栈存储约束的借用，不因有 C 指针就稳定。可能扩栈或执行回调的入口需要遵循其 refresh/重新取槽契约；GC roots、pin 和 cleanup registration 各解决不同的寿命问题，不能互相替代。见 `zr_vm_library/include/zr_vm_library/aot_runtime.h:226`、`zr_vm_library/include/zr_vm_library/aot_runtime.h:2183`。

invocation scope 约束当前调用的模块、闭包、帧与失败展开状态。future callback/任务辅助入口须沿真实注册和恢复链解释其 scope；回调结束后不能任意复用旧 invocation/frame。直接调用、prepared 路径和 generic 回退分别遵循公开返回状态，resume 哨兵不能被解释为所有回退路径已发布续执行位置。

## 值所有权、失败与清理

OwnLoan 等操作把生成槽上的所有权动作桥接给 core。非 null 源必须符合该操作对应 kind；core 的 null 成功路径仍有效。部分适配包装把底层 operation false 转换为目标 null，再返回桥接完成 true；true 不能概括为所有权转换成功。参数无效、前置状态无效、refresh 或地址解析失败的 false 也不应缩成单一地址失效。见 `zr_vm_library/include/zr_vm_library/aot_runtime.h:628`。

异常退出、ToBeClosed 登记与 unwind cleanup 必须维持当前函数的清理边界；清理回调可能再入或扩栈。借用 Value 的地址有效期与其中 payload 的所有权不同，复制一个槽地址不会转移或延长值的寿命。数组批量 prepare/commit 可逐步产生副作用，后段失败不回滚已有写入。

## 分派与优化提示

cached property accessor 要求 binding 与对应 thunk 存在；缺失时返回失败，不按成员名退回通用查找。kind/static 模式也须与注册契约一致。new-owner 提示仍有 core 的 young guard/fallback；现有 TODO 核生成器 CFG、alias 和 inline 路径的证明，不能仅凭局部证明不完整宣称已绕过旧 owner 写屏障。

数值 helper 的 bool、signed/unsigned 与 float 输入集合不同。ToInt 的整数类早分支、ToUInt 的元转换标签检查和 signed 比较的 bool 差异按各入口实际代码保留。移位 TODO 须分别核合法 count、左移符号/结果边界与真实生成选路；右移不继承左移的极值前提。ToBool/LogicalNot 的 truthy 求值仍需核合法同槽输入与编译槽分配，不能宣称任意 in-place 安全（`zr_vm_library/include/zr_vm_library/aot_runtime.h:2295`）。

复合 IterMoveNextJumpIfFalse 在同一指令中调用 MoveNext 后读 bool；其 TODO 核 callback 扩栈后立即读取所用 frame 是否已刷新。普通 nextinstruction 刷新事实不能自动证明这个复合路径安全（`zr_vm_library/include/zr_vm_library/aot_runtime.h:1943`）。

## 观察策略、ABI 入口与测试限制

Set/Reset/GetObservationPolicy 有 execbc pipeline 测试中的真实 C 调用，即使它们嵌在 TEST_ASSERT_TRUE 等宏内仍是调用。该测试核状态顺序，不能授线程隔离保证或本轮执行信用（`zr_vm_library/include/zr_vm_library/aot_runtime.h:250`）。UnsupportedMetaCall 有归档生成 fixture 的实际 guard 调用；这不证明当前 writer 会发射该入口（`zr_vm_library/include/zr_vm_library/aot_runtime.h:2082`）。仅声明、名称白名单、strstr 或 forbidden needles 是 ABI/文本契约检查，不能当作 helper 执行链。

本轮完成两份源码 798 单元的静态 caller/字段/块契约审查；不改变 token、字面值、指令、布局或运行分支。本轮没有运行 native、生成器、装载、moving GC 或 runtime 测试。人工帧 fixture 和 compile-only 历史收据只能支持各自既有场景；未补齐的合法触发、alias、records 搬迁及回调刷新核查仍以具体 TODO 保留。

相关 ABI 总览见 [AOT ABI](../wiki/05-interop/aot-abi.md)，生成端说明见 [AOT Lowering、运行时 Helper 与注册参考](../wiki/10-aot-lowering-registration-reference.md)。
