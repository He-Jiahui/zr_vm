---
related_code:
  - tests/task/test_task_job_scheduler.c
  - tests/task/task_scheduler_queue_reuse_cases.inc
  - tests/cmake/task-scheduler-queue-tests.cmake
  - tests/CMakeLists.txt
  - tests/harness/runtime_support.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
implementation_files:
  - tests/task/test_task_job_scheduler.c
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/task/test_task_job_scheduler.c
doc_type: test-contract
---

# Job/Scheduler 父测试的调用契约

## 构建与运行入口

`tests/CMakeLists.txt:4753` 将父文件作为真实目标的源；`tests/cmake/task-scheduler-queue-tests.cmake:20-25` 在目标存在时注册一个 `task_job_scheduler` CTest 项，标签为 ssa/task，120秒约束一次整程序执行。构建该目标后可运行 `ctest -R '^task_job_scheduler$' --output-on-failure`。这选择整个Unity程序，不能逐个选择内部用例；配置注册和C编译都不代表测试已运行。

`tests/task/test_task_job_scheduler.c:471` 的main经`tests/task/test_task_job_scheduler.c:473-482`顺序执行十个RUN_TEST，`tests/task/test_task_job_scheduler.c:486`传播Unity汇总退出码。前五个检查描述符、源调度roundtrip、直接void结果ABI、Job消费和Task must-use；后五个在`tests/task/test_task_job_scheduler.c:459` include的queue片段中，见[队列复用调用契约](task-scheduler-queue-reuse-caller-contract.md)。此文件提供它们共同的状态、源码编译和descriptor查找支持，不创建另一套生产provider。

## 支持函数与借用边界

create_state先建立VM/Global，再注册task builtins和公共Source_Compile入口；失败清理并返回NULL。Parser注册只安装编译回调，不运行ZR源。成功状态由单个用例负责销毁，main没有自动清理失败用例遗留状态的能力。

三个find helper只以精确名称线性查找当前原表。调用者必须提供有效的表指针和计数；helper检查容器/名称NULL，不验证完整表一致性或ABI。当前builtins descriptor来自静态表，返回地址是借用，不应释放、修改或当作VM对象根。旧短注释要求在当前状态销毁前使用，是本测试的局部使用限制，不能反推静态表由VM拥有。

compile helper同步借入NUL结尾source/name，strlen不包含嵌入NUL后的文本；SourceName分配与解析/编译失败都可返回NULL。当前成功调用方立即消费Function。若以后跨分配保存返回地址，需核对GC保护；裸地址本身不是根。NULL不区分失败阶段，更不单独证明预期的语言诊断。

## 五个父用例的实际观察

| 用例 | 调用意图 | 当前观察限制 |
| --- | --- | --- |
| descriptor形状 | 元数据连接Job构造、schedule消费role、Task<void>及Duration名称 | 检查版本3.0.0和指定表字段，不验证完整ABI、不执行Job或物化currentScheduler |
| roundtrip | 实际源导入→Job<int>→currentScheduler.schedule→Task.result→返回7 | 没有调用计数；纯函数多次返回7仍可满足断言 |
| 直接void ABI | 直接yieldNow/delay(0) callback，再以真实Task作self调用result | 本地cooperative轮次，不是墙钟Duration或worker；完成值目标有已证BUG |
| Job单次消费负测 | 同一个Job绑定第二次schedule意图被编译器拒绝 | 只断言Compile为NULL，未验证诊断原因，也未先证明单次编译成功 |
| must-use负测 | 独立表达式丢弃yieldNow Task意图被拒绝 | 只断言Compile为NULL，没有执行或结果资源清理证据 |

三组静态源模板各是一项数据定义；其中两段fn()注入函数均无参数、无捕获、返回整数7。负测源不会执行fn；正常源经整数执行支持观察返回值。C注释在字符串外，全部原271LF行和ZR字符串字节保留。

## 可达的结果槽 BUG

`tests/task/test_task_job_scheduler.c:183` 的局部result未初始化，`tests/task/test_task_job_scheduler.c:191`直接传入Task.result回调，`tests/task/test_task_job_scheduler.c:192`的类型断言已经晚于该回调。正常yieldNow或delay(0)由 `zr_vm_library/src/zr_vm_library/task_runtime.c:795` 建立typed-null结果，pump完成时 `zr_vm_library/src/zr_vm_library/task_runtime.c:710` 与 `zr_vm_library/src/zr_vm_library/task_runtime.c:713` 再次发布typed-null/COMPLETED；`zr_vm_library/src/zr_vm_library/task_runtime.c:1162` → `zr_vm_library/src/zr_vm_library/task_runtime.c:835-837` 仍取到存在的null值，经 `zr_vm_library/src/zr_vm_library/task_runtime.c:216-217` 调用Value_Copy。`zr_vm_core/include/zr_vm_core/value.h:229` 进入PrepareDestination，在 `zr_vm_core/include/zr_vm_core/value.h:151` 与 `zr_vm_core/include/zr_vm_core/value.h:161` 读取未初始化ownershipKind/control/weakRef，可能误释放或崩溃。callback输出的yieldTask/delayTask由SetObject初始化，与这个完成值Copy目标是两个不同的槽。直接两条调用路径均到达同一个helper，故helper、目标槽和两种直接调用上下文均标BUG；这些标签不是五个不同的根因。

本轮仅按完整静态可达路径记录，没有运行时崩溃复现或行为修复。源码roundtrip的ZrTests_Function_ExecuteExpectInt64使用 `tests/harness/runtime_support.c:429` 初始化自己的返回值，不能把这个BUG扩展给它。

## TODO 与失败责任

TODO: 两个NULL负测须读取诊断类别及具体源位置，Job消费负测还需明确单次调用的成功基线；否则导入、语法或其他失败也可能满足NULL断言。具体入口是消费源的第二次schedule和must-use表达式的yieldNow。

TODO: 若要求名字中的callable_once作为验收结论，需可观察调用次数或副作用；目前7的值断言不能证明次数。

Unity失败断言会中断当前用例；各函数尾部Destroy只描述正常路径，不保证失败之后继续执行。main只汇总结果，不接管VM或GC根。queue片段按自己的部分root flags独立清理，与父用例的正常Destroy责任各自对应；文件边界不能被当成资源所有权转移。

## 完整覆盖与验证边界

父文件台账在 `docs/code-review/coverage/tests_iterator_task.tsv`，替换旧12项局部结论为27项完整census：12函数、3源模板、2注入函数、1queue include接口和9重要块。没有本地类型、字段、宏定义或独立global数据。27完整注释块增加216行，形成487LF行；所有原始代码、注释、空行和字符串原样保留。支持include及RUN_TEST宏调用随对应fixture/main职责审查，不制造重复定义单元。

本批还携带queue片段一条References、CTest片段两条References，以及对应59项queue/4项CTest台账的数值调用锚点迁移；这90项是新父27项加既有59/4项台账的当前完整复核，不代表新增90项实现。其他core/parser台账只改变真实同语句调用或证据锚，保留其已有语义结论和全部非关联字节。实际编译需以该父文件为TU，对照原/新parent及queue两个拥有的源文件；不得为头文件变化放宽非本批依赖比较。编译不是Unity/CTest运行或生产runtime验收。
