---
related_code:
  - tests/task/task_scheduler_queue_reuse_cases.inc
  - tests/task/test_task_job_scheduler.c
  - tests/cmake/task-scheduler-queue-tests.cmake
  - tests/CMakeLists.txt
  - tests/harness/runtime_support.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_library/src/zr_vm_library/task_runtime_scheduler_queue.inc
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/include/zr_vm_core/value.h
implementation_files:
  - tests/task/task_scheduler_queue_reuse_cases.inc
plan_sources:
  - docs/code-review/comment-standard.md
tests:
  - tests/task/test_task_job_scheduler.c
  - tests/task/task_scheduler_queue_reuse_cases.inc
doc_type: test-contract
---

# 本地 Scheduler 队列复用测试的调用契约

## 编译和执行入口

`tests/task/test_task_job_scheduler.c:459` 将此片段编入同一翻译单元；父文件提供状态创建、源码编译和descriptor查找支持。片段先定义两项私有字段名，再在 `tests/task/task_scheduler_queue_reuse_cases.inc:35` include原生产private helper，开放真实checked-write/pin/OOM路径的测试调用，产品ABI不增加入口，也没有替换生产queue实现。

`tests/CMakeLists.txt:4752-4754` 创建父测试目标；`tests/cmake/task-scheduler-queue-tests.cmake:20-25` 只在目标存在时登记一个 `task_job_scheduler` CTest项及ssa/task标签、120秒整程序timeout。父main的`tests/task/test_task_job_scheduler.c:478-482` 顺序执行这里的五个Unity用例，另外五个父文件用例不在本片段覆盖范围。构建目标后可用 `ctest -R '^task_job_scheduler$' --output-on-failure` 选择整个程序。

## 场景和观察范围

| 场景 | 真实调用意图 | 当前断言范围 |
| --- | --- | --- |
| completed结果保留 | 三次源执行，首Task建根、第二Job触发后续调度、最后取回旧Task | 首结果7/新结果9/旧completed及结果7；没有queue数组身份断言 |
| 重入提交 | outer Job内通过宿主回调向同Scheduler提交inner Job | 提交时QUEUED快照，outer结果13、随后inner结果19；不同于递归pump或OS worker |
| cooperative推进 | 已有completed Task之后源级yieldNow再原生delay(3)/Task.result | 意图观察7、23、null、旧completed；直接Task.result有未初始化目标槽BUG，不能据断言源码宣称运行通过；turns不是墙钟时长 |
| 字段拒写与replacement | 四个新状态调用真实核心setter和private helper | head拒写、detach拒写、attach拒写、成功attach的字段/版本/pin观察 |
| 限定OOM与重试 | 建fixture后临时替换该Global的上游allocator | 两次匹配拒绝、MEMORY_ERROR、pin/ignored/queue-null和恢复allocator后的非空重试 |

只读字段场景中，mode0要求head仍5、queue为null、memberVersion/pinFlags/ignored标志及计数不变；mode1仅检查detach失败、原array/head5/版本不变；mode2检查head归零和版本+1而attach不发布；mode3检查发布的pair确指向返回queue、head0和版本+2。mode2/3还比较pin/ignored及计数。上述观察不自动覆盖所有barrier、回收或异常状态。

## 描述符、源函数和根责任

静态 `task.queue.test` module发布四个回调。captureTask取得旧Task根；savedTask解析并借出该根对象；captureJob先取得Job根再尝试Scheduler根，后者失败时前者仍需释放；enqueueInner使用两根提交Job，再取得Task根。enqueueInner的bool只表示提交/Task根成功，整数13是outer结果，QUEUED另由状态快照保存，不能互换。

三GC句柄和四flags是父main串行用例的静态状态，不支持并发测试共享。Resolve的裸指针只在紧接的同步操作中使用，返回对象值不取代保存根。所有实际取得的根在State_Destroy之前独立释放，Unity断言放在清理之后；部分创建失败不能根据整个源执行的false省略cleanup。私有status扫描直接读 `__zr_task_status`，避免新key分配，缺根/字段或整数类型时返回-1；它不是公开状态API。

八个字符串模板是八个独立定义。五个真实注入fn分别属于两个completed Job、reentrant inner/outer及cooperative首Job；yieldSource中没有注入fn。它们通过真实源编译和Job调度运行，C注释位于字符串外，所有原414个物理行、源字符串及LF字节保持不变。

## 已证明的结果槽问题

BUG: cooperative用例的 `tests/task/task_scheduler_queue_reuse_cases.inc:602` 声明result却不初始化，`tests/task/task_scheduler_queue_reuse_cases.inc:619` 的Task.result回调直接使用其地址。成功delay(3)由 `zr_vm_library/src/zr_vm_library/task_runtime.c:795` 建立typed-null结果字段，`:813` 的同步pump经`:710`/`:713`进入COMPLETED；Task.result的`:1162`→`:835`/`:836`/`:837`取到的是存在的null值，不是缺失字段的NULL指针，于是`:216`/`:217`仍调用Value_Copy。`zr_vm_core/include/zr_vm_core/value.h:229`先进入PrepareDestination，`:151`/`:161`读取未初始化ownership元数据，可能触发误释放或崩溃。回调返回后的bool/type比较不能保护回调内读取。初始化入口可在调用前使用ZrLib_Value_SetNull，但本注释任务保留程序行为；完整静态可达链不代表运行时复现。父文件assert helper的同类问题由[Job/Scheduler父测试调用契约](task-job-scheduler-caller-contract.md)单独登记；它不扩大此片段59项台账的范围。

## 构造和失败边界

private_scheduler建立真实Scheduler根后才继续写queue/head、创建可选array和readonly prototype。后续返回NULL时rootCreated仍可能为true，两个调用者均按rootCreated释放；没有从返回NULL推断根泄漏。非空Scheduler却不能证明两个void字段setter成功，当前测试还假定查找pair可解引用。

TODO: 核查void字段写入后的threadStatus及head/queue pair存在/normalized状态，不能以非空构造返回替代发布确认。

TODO: checked-fields测试在可能分配的queue调用后仍缓存pair/head，应遵守private helper的借用边界并重查。当前HashSet pair节点在pool扩张/rehash时地址稳定，删除/析构会使其失效；Scheduler有测试根且建队前pin，没有本次可证实stale-pointer BUG。调用仍须遵守private helper仅借用至下一次分配的边界；当前节点实现不能扩大这项借用契约，也不是长期字段句柄承诺。

OOM wrapper只拒绝前两次 `reject && pointer==NULL && newSize==sizeof(SZrObject) && flag==OBJECT` 的请求；originalSize不参与过滤，其他请求继续交给原上游。它可覆盖初次请求和GC retry的设计意图，但仅谓词/次数不能唯一证明请求源自queue array。

TODO: 如需queue专属分配失败信用，应核对真实分配来源；当前仅限全局对象请求谓词。

恢复原allocator函数和原argument先于异常清理、状态重置、重试及VM销毁，临时failure记录不逃逸同步调用。当前cleaned检查pinFlags、ignored BOOL、queue=null，没有head/memberVersion/ignoredObjectCount比较。

TODO: 明确并观察OOM后的head归零和memberVersion变化；若要求ignored登记计数恢复，应保存并比较计数，不能用重试非空代替失败后不变量。

## 完整清单与验证范围

台账 `docs/code-review/coverage/tests_task_scheduler_queue_reuse.tsv` 共59个单元：13函数/声明、2字段名常量与1private include、7独立状态、8参数/descriptor/模块数据接口、1failure类型及4字段、8源模板、5注入函数、10重要块。原414 LF行增加59完整注释块共429行，形成843行；code、原注释、空行与字符串不变。既有runtime ledger中的ScheduleJob调用锚以及CTest契约的private include锚随同一语句迁移，不改其原语义结论。

已完成实际父测试TU的修改前编译，335个compiler依赖、owned Job自然退出0并关闭所有句柄。修改后须保持相同TU/argv/environment及334个非本批依赖，只有include注释源SHA改变；该编译不链接产品、不执行Unity/CTest，不授queue runtime、父文件/production helper/full-module验收信用。
