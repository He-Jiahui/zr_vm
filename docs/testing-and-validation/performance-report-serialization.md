---
related_code:
  - tests/performance/perf_report.c
  - tests/performance/perf_report.h
  - tests/performance/perf_runner.c
  - tests/performance/perf_process.c
  - tests/performance/persistent_protocol.c
  - tests/performance/persistent_protocol.h
  - tests/performance/perf_statistics.c
  - tests/performance/perf_statistics.h
  - tests/benchmarks/aot_runner/aot_coverage.c
  - tests/benchmarks/test_ssa_aot_runner_coverage.c
  - tests/performance/test_perf_report.c
  - tests/cmake/benchmark_task3_suite.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/CMakeLists.txt
  - tests/cmake/ssa-tests.cmake
implementation_files:
  - tests/performance/perf_report.c
  - tests/performance/perf_report.h
plan_sources:
  - user: 2026-09-26 全仓库首方代码调用链审查与注释任务
tests:
  - tests/performance/test_perf_report.c
  - tests/performance/test_perf_statistics.c
  - tests/benchmarks/test_ssa_aot_runner_coverage.c
  - tests/cmake/run_perf_runner_persistent_protocol_test.cmake
  - tests/cmake/run_benchmark_task3_suite_contract_test.cmake
doc_type: module-detail
---

# 性能报告汇总与序列化合同

`perf_report.c` 将已经完成的测量转换为可消费的报告，并在发布 AOT 快照前检查字段的一致性。普通报告承接 runner 的逻辑测量批次，AOT 报告承接独立的阶段成本和覆盖率快照。两者共享字符串转义，但输入对象、schema 位置、内存作用域和验证职责不同。

本文的代码说明范围是 `tests/performance/perf_report.c` 全文及 `perf_report.h` 的必要 11 个合同单元：5 个类型、5 个公开声明、1 个文本容量宏。调用方、统计层、协议层和测试只作为这些合同的上下文，不构成对应文件、整个头文件或测试模块的完成声明。

suite 的配置、准备与执行流程继续由 [CTest 性能报告指南](ctest-performance-reporting.md) 说明；backend 注册和覆盖率采集由 [AOT runner 与覆盖率合同](aot-runner-coverage.md) 说明。这里集中说明两个报告接口的输入、输出、所有权与失败边界，避免在两份入口指南重复这些细节。

## 从调用方到消费端

普通报告的真实入口是 `perf_runner.c`。runner 先采集正式样本和必要的追加样本，持久模式还会完成 `STOP` 并取得会话快照，然后调用 `ZrPerfReport_ComputeSummary`，最后调用 `ZrPerfReport_WriteJson`。ComputeSummary 或写入失败时，runner 返回失败，不把该次报告发布作为成功处理。

`run_performance_suite.cmake` 在 runner 成功返回后读取报告文件，将完整 JSON 交给 `benchmark_task3_suite.cmake` 的解析器。writer 对协议允许的未测量字段写显式 `null`；解析器接受这些 `null` 并转为空值，缺字段仍拒绝。suite 消费这一 JSON，不从控制台摘要重建时间或内存统计。

AOT 报告接口在当前仓内的实际外部调用来自 `test_ssa_aot_runner_coverage.c`。测试构造快照后调用 `ZrPerfReport_ValidateAotPhase` 或 `ZrPerfReport_WriteAotJson`；其中比例 fixture 的转换 helper 只复制必要覆盖率字段，不是完整 runner 结果到性能报告的通用适配器。当前调用证据不能证明 release automation 或进程 AOT runner 已调用该 writer，也不能排除仓外消费者。

构建分别把 `perf_report.c` 链入 `zr_vm_perf_runner`、`zr_vm_perf_report_test` 和 `zr_vm_ssa_aot_runner_coverage_test`。这三个入口说明同一实现同时提供批次汇总、普通报告发布和 AOT 快照合同验证。

## 两种 JSON 的版本和用途

| 发布接口 | schema 的实际位置 | 保存内容 | 消费约束 |
|---|---|---|---|
| `ZrPerfReport_WriteJson` | 当前原始报告顶层没有 `schema_version`；每个 `runs[]` 对象带 `schema_version: 3` | 作用域、复用声明、采样预算、校准信息、资格、命令、样本、summary 和可选持久会话 | suite 按固定字段读取；suite 组装后的 `benchmark_report.json` 另有顶层 schema 3 |
| `ZrPerfReport_WriteAotJson` | 顶层 `schema_version: 1` | 状态、退出码、请求/实际 backend、entry token、可选产物信息、阶段、内存和覆盖率 | 独立快照；不混入普通报告的逻辑样本或 gate 资格 |

普通报告中 `iterations` 表示初始采样数，`sample_count` 表示实际总数，`extra_sample_count` 表示追加数。writer 要求初始数为正、追加数非负，且总数等于两者之和、合计不超过 20。检查先限制两项再核对相加结果，保证格式不会同时宣称矛盾的预算和实际样本数。

`aggregate_wall_ms` 保存一个逻辑样本的 repetitions 总耗时，`wall_ms` 保存除以 repetitions 后的耗时。summary 使用归一化的 `wallMs`，不使用 aggregate 值重新扩大样本。校准与预热已由 runner 排除，不进入 `runs` 或 summary；校准字段只解释为何选择本次 repetitions。未启用校准时相关成本写 `null`，保留“未测量”的含义。

耗时以三位小数显示，CV 和 AOT 原生比率以九位小数显示；Bootstrap seed 用十进制字符串保存完整 uint64_t 值。显示精度不改变 C 输入对象的精度合同。

## 汇总结果与 runner 资格

`ZrPerfReport_ComputeSummary` 接受 1 至 20 项样本，要求时间为非负有限值、Bootstrap resample 数非零。它借助统计层计算耗时分布和确定性的 95% 中位数区间，再补充进程峰值分布。本文只规定报告如何使用这些结果，不展开统计层的排序或重采样实现。

`stddevWallMs` 是样本标准差，单项样本为 0；MAD 是相对于中位数的绝对偏差中位数。CV 在均值非零时为标准差除以均值；合法全零 wallMs 的均值和标准差均为 0，此时 CV 保持 0，不能把这类输入表述为未定义或无穷。

summary 不授予可比性或 gate 资格。当前 runner 以 CV 不超过 0.05 判为稳定，并要求至少 10 个稳定样本才能授予 gate；profile 单样本报告设为 `NOT_COMPARABLE`。这些决定通过 `SZrPerfMeasurementMetadata` 保存，writer 不重新判定。metadata 的 seed/resample 数和 summary 的实际计算参数也须由调用方保持同批一致，writer 不交叉验证它们。

## 输入借用、内存作用域与生命周期

两个 writer 都是同步接口。它们借用输入对象和文本，调用返回后不持有这些地址，也不接管调用方的样本、metadata、summary 或快照。调用期间输入应保持有效、稳定，不能依靠另一个线程改写字段来补全报告。普通 writer 的 `command` 必须是 NULL 终止的参数数组，`samples` 必须至少包含 metadata 声明的实际总数；`stability` 来自当前 runner 的固定状态文本。

ComputeSummary 复制时间和峰值到临时工作数组，排序不会改变调用方样本的原次序，临时数组在返回前释放。summary 本身不拥有动态数组。其失败输出按阶段区分：参数前置检查拒绝时尚未写 summary，原值保留；工作数组分配或时间统计/Bootstrap 被检测到的失败会将 summary 清零。调用方应依赖返回值，不从输出是否仍有值推断成功。

process 模式下，一个逻辑样本可包含多个新进程。采样方累加它们的总耗时，按 repetitions 归一化时间，并取这些进程峰值的最大值作为该样本峰值。summary 的内存均值与中位数跨逻辑样本计算，峰值不再除以 repetitions。

persistent 模式下，多个 RUN 共用同一服务器。runner 完成 STOP 后取得会话 PID、退出码和整个会话的峰值；writer 要求每个样本 PID 与会话快照相同，才写 `same_pid: true`。该检查只建立身份一致性，不重新验证 checksum 或退出状态。会话峰值可能包含启动、校准、预热和所有测量的生命周期成本，不能用它伪造每个 RUN 的独立峰值。

因此 persistent 的 `runs[].peak_working_set_bytes` 和 summary 内存统计全部写 `null`，会话峰值只放在 `persistent_session.peak_working_set_bytes`。内部样本或 summary 的零内存值不表示独立测量到了零字节；suite 读取会话峰值作为 max peak，并将跨样本 mean 标为无数据。

## AOT 快照验证所能证明的事实

AOT 文本存储在容量为 128 字节的内嵌数组中，容量包含 NUL。validator 要求在数组内找到终止符，并只接受可打印 ASCII。requestedBackend 和 entryToken 必填；actualBackend 只有 UNAVAILABLE 状态允许为空；artifactHash、toolchain、failureReason 可以为空。

这类检查只能证明字段有界、可序列化以及字段之间满足现有关系。validator 不查询 backend 注册表，不验证 entry token 存在，不计算或鉴定 artifact hash，也不验证 toolchain 的真实性。AOT JSON 没有 checksum 字段，不能将快照状态或结构验证称为 checksum 身份验证。

| 状态 | 当前 validator 的附加约束 |
|---|---|
| RAN | processExitCode 为 0，requestedBackend 与 actualBackend 文本相同 |
| FALLBACK | processExitCode 为 0，actualBackend 非空；若 backend 文本仍相同，coverage 必须可用且 interpreterSites 大于 0 |
| UNAVAILABLE | processExitCode 非零，actualBackend 可为空 |
| FAILED | 仍须满足通用文本、阶段和覆盖率约束；没有额外的退出码规则 |
| INVALID、COUNT 或未知状态 | 拒绝写入 |

RAN 的规则没有禁止 interpreterSites，因此不能从 RAN 名称推导“所有工作均为原生”。同名 backend 的 FALLBACK 用可用解释器计数表示内部回退；backend 文本不同时仍要满足其他快照约束。

compile、link、load、startup、run 阶段分别保留成本。每项必须是非负有限毫秒数，或唯一的不可用哨兵 -1。writer 将 -1 转为 `null`，零仍是合法测量。RSS 和 code size 是否输出 `null` 由各自 has* 标志决定，合法零字节不会被推断为缺失。可选文本为空时写 `null`。

## 覆盖率的分母与精度合同

覆盖率不可用时，快照要求 `nativeCoverage=-1`，并要求 semanticSites、executedSemanticSites、nativeSites、nativeHelperSites、interpreterSites 这五个语义计数为 0。writer 将这些计数和比例写 `null`；fallbackCount 和 deoptCount 是独立事件数，仍写整数，validator 不要求它们随不可用状态归零。

覆盖率可用时，executedSemanticSites 和 semanticSites 均须为正，且 executedSemanticSites 不超过 semanticSites。semanticSites 是声明计数，可与执行计数相等，也可更大；它不是本报告原生比例的分母。native、native helper、interpreter 三类须在受检、无溢出的相加后恰好等于 executedSemanticSites。fallback/deopt 事件不加入这次语义分项。

`nativeCoverage` 必须精确等于：

```c
(double)nativeSites / (double)executedSemanticSites
```

该表达式与当前 coverage producer 一致。native helper 不进入分子；既不能用 semanticSites 代替实际执行分母，也不能将 native 和 helper 合并后称为此处的 nativeCoverage。没有执行观察时不可用，与“观察到了执行，但 nativeSites 为 0”的合法 0.0 比例不同。

验证使用 C double 的精确比较。producer 的 `1.0/3.0` 是合法输入，JSON 显示值 `0.333333333` 不是同一个 double，因此不能将显示小数回填后期待验证通过。端点、分数和 uint64_t 最大计数 fixture 检查的是这个 producer 表达式合同，不授予任意外部 JSON 比率容差。

## 文件发布与失败后的目标内容

writer 先做结构检查，再以 `fopen(path, "wb")` 打开目标。普通报告先拒绝矛盾采样计数和 persistent PID，AOT 报告先调用 validator。前置拒绝发生在打开之前；AOT 的 sentinel 测试进一步逐字节验证了错误比率不会覆盖已有目标。

打开成功后，旧目标已被截断。任一输出、flush 或 close 失败都会使 writer 返回 0，但当前实现不恢复旧文件，也不删除已写的部分文件。调用方必须依赖本次返回值或 runner 退出状态，不能因为目标存在就将它作为成功报告。普通字符串 helper 将 NULL 文本写成空字符串；是否用 JSON `null` 表达可选数据由外层判断，helper 不验证普通输入的 UTF-8 编码。

## 既有 BUG：进程峰值中位数分配失败

现有 median helper 的 BUG 标签指出：用于内存中位数排序的临时 malloc 失败时，helper 返回 0，ComputeSummary 随后仍返回 1。这里确认的是既有静态错误路径，未执行 OOM 故障注入。

可观察的普通报告误报须同时满足：process 模式正式样本合法；先前工作数组、时间统计和 Bootstrap 已成功；本次中位数临时分配失败；样本峰值的真实中位数大于 0。仅存在一个正峰值不足以保证真实中位数为正，混入零峰值后仍须按整组中位数判断。

完整可达链为：suite 调 runner；runner 取得成功退出的 process 样本并调用 ComputeSummary；ComputeSummary 在时间统计与区间成功后调用峰值 median helper；该次分配失败返回 0；summary 保留这个 0 且函数返回 1；runner 继续 WriteJson；普通 JSON 将 `median_peak_working_set_bytes` 写为 0；suite 成功路径读取并把它作为内存统计消费。

persistent JSON 的 sample 与 summary 内存字段均为 `null`，不能把同样的 JSON 误报影响扩大到持久模式。后续修复需要给中位数 helper 独立失败通道，并在对应调用链核查失败传播；当前注释和本文不改变程序行为，也不声称已修复。

## 当前测试证明范围

| 现有测试调用或断言 | 从当前正文可确认的场景 |
|---|---|
| test_perf_report 的超上限输入 | ComputeSummary 拒绝 21 项 count；不覆盖成功统计内容 |
| test_perf_report 的矛盾采样计数 | WriteJson 返回失败；该负例在打开前结束，不能当成 runs 或输出收尾块的实际消费者 |
| test_perf_report 的 /dev/full 路径 | 非 Windows 检查普通 writer 的缓冲输出失败传播；Windows 明确跳过；不能扩大成 AOT writer 的运行证据 |
| persistent protocol CMake 断言 | 同 PID、会话峰值、逐样本内存 NULL，以及稳定 gate、追加耗尽、profile 不可比较的报告场景 |
| Task 3 suite contract 的手写 JSON | 解析器按字段消费报告；不直接执行 C writer |
| AOT 阶段成功写入 fixture | 验证 API 和 writer 返回成功，随后删除文件；没有重新解析成功 JSON 的逐字段断言 |
| AOT 动态比例 fixture | 声明100、执行10、原生8时比例为0.8；检查0、1、1/3和最大计数，并拒绝不一致及舍入回填比例 |
| AOT sentinel fixture | validator 拒绝错误比率后，已有文件长度、EOF 和全部字节保持原样 |

上述覆盖范围来自当前测试正文和静态调用链阅读。测试场景、实际执行结果与故障注入分别记录；这些合同说明本身不授予新的构建、平台运行或 OOM 复现信用。
