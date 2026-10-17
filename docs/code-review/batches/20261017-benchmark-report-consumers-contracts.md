---
doc_type: code-review-batch
related_code:
  - scripts/benchmark/aggregate_benchmark_summary.py
  - scripts/benchmark/benchmark_reports_to_csv.py
  - tests/benchmarks/test_benchmark_consumer_scope_types.py
  - tests/cmake/benchmark-scope-types-tests.cmake
implementation_files:
  - scripts/benchmark/aggregate_benchmark_summary.py
  - scripts/benchmark/benchmark_reports_to_csv.py
plan_sources:
  - docs/code-review/comment-standard.md
  - docs/testing-and-validation/benchmark-consumer-scope-validation.md
tests:
  - tests/benchmarks/test_benchmark_consumer_scope_types.py
  - tests/cmake/benchmark-scope-types-tests.cmake
---

# Benchmark 汇总与 CSV 消费契约

本批逐项审查4份首方文件的50个单元：32个函数、1个回调、1个类、9个状态及7个语句块。当前493份台账按精确三元键去重，重新审查37个既有键，另有13个新键；不将旧键重复计作新增交付。结论为45项commented、4项no-comment及1项BUG，无未审单元和新TODO。四项无需追加注释的判断仍保留定义、调用方、入口、契约及证据。

在4份源码EOF追加38段纯#文档，包含48个参数和28个返回值标签，另5个无返回值入口以正文说明副作用与异常。原始程序前缀、混合EOL、既有docstrings、人工JSON及断言全部保留，不改签名、配置、行为或测试。声明之外的间接入口包括模块导入、CLI主入口、unittest发现、mock回调及CMake条件注册；未找到字面直调的单元不因此判为死代码。

## 职责、使用时机及边界

聚合器读取可选报告、建立副本并筛除不可信比值，向汇总JSON及查看器提供诊断。CSV消费者按既定列序转录统计字段与已有比例，不重算比值；身份、PASS、scope和可选采样门槛共同决定比例能否保留。返回原ratio对象的helper允许Any值，不能把比例门控说成数值或范围验证。

两族测量契约都先确认scope为str，再作集合成员判断；非字符串JSON类型被拒绝。模式兼容列仅对str执行strip/lower，非str使用空串而返回字符串false，不调用str(mode)或自定义__str__。五字段、名称唯一性、环境可比性及比例数值真实性是不同约束，注释没有将它们合并为单一验证保证。

Mapping索引创建新dict但借用原记录；深拷贝、只读查询和就地清除分别说明，不将新容器视为完整所有权转移。两个CLI按顺序直接写输出，可能留下已经写出的文件或部分内容，没有原子替换、回滚或全面输入/输出别名保护。文件上下文负责关闭真实句柄；JSON、结构、转换或IO失败按实际捕获边界传播。

相关测试由unittest发现。Path.open/mkdir及可选JSON读取在局部mock作用域替换，替换对同进程其他线程可见，离开作用域包括异常退出均恢复；StringIO与真实文件关闭行为不同。既有CMake仅条件注册Python脚本，配置成功不等于解释器可执行或测试通过。

## 旧问题复核与新增静态缺陷

既有4行“非字符串scope进入集合而抛TypeError”的BUG不再描述当前代码。当前两族消费器已有字符串类型检查，原有3个测试方法覆盖非字符串JSON值及无效比例；本批更新这些旧审查行，不把此前修复计作本任务行为修改。

新增1个BUG只涉及移除路径记账。触发条件是benchmark_report.environment为dict且status不是COMPARABLE，同时其他有效记录使前一scope门控保留比值。在_build_summary的343行先得到空removed列表，352行进入环境拒绝分支，356行调用Task4递归清除比值，但357行只收集先前列表。最终measurement_contract.ratios_removed漏记仅由环境分支移除的字段，违反tests/benchmarks/README.md:268的完整路径记录约定。比值本身已清除。

该结论由完整静态调用路径及独立审核的反例值证明；未执行该反例，没有将上述3个回归方法宣称为环境记账缺陷的复现，也不将单独environment_report.status混作触发字段。本轮只增加标记和台账记录，未修复行为。

## 修改前后验证

修改前后均使用真实E盘Python3.12.10，以-B执行同一现有测试脚本，3个方法全部ok，两个child及两个driver均实际exit0。每方法16个subTest上下文、合计48个，是原静态循环清点；verbose日志只有3个方法结果，不声称取得48条逐例运行轨迹。两次完整stdout均为空，stderr除Ran3耗时字段外逐字一致。

六个有限源码输入在各次运行首尾稳定，跨pair只有本批4份EOF注释；原AST、全部docstrings和递归代码签名相同。严格等价门进一步比较6951个原程序token的kind、spelling及实体坐标，并比较AST属性和同文件名compile-only marshal。6个虚拟EOF DEDENT/ENDMARKER坐标后移，单列披露；不将其称为原实体位置变化。CMake原程序前缀与EOL逐字保留。

15个完整依赖、935个保留EOL的行pin、50个定义范围及34个函数/回调或类体范围和128个已解析AST调用支持有限当前契约。直接运行测试没有调用两生产CLI main、CMake或CTest，没有写实体CSV/报告fixture；WindowsApps解释器别名未执行。不新增仅复述实现的测试，也不作全语法、所有失败分支、Linux或原子checkout保证。

50行落入原scripts_remaining.tsv：37行原位更新、13行追加，其他行及既有Embedded批次保持原字节。inventory仅允许所审4行的sha256/status/note改变。实际源码/台账、暂存差异和串行提交分别以对应门禁报告为准。必要E盘证据及复用产物保留，释放0B，产物生命周期仍开放。
