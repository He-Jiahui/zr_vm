---
related_code:
  - scripts/benchmark/benchmark_execution_plan.py
  - tests/benchmarks/test_benchmark_execution_plan.py
implementation_files:
  - scripts/benchmark/benchmark_execution_plan.py
plan_sources:
  - user: first-party caller-first contract review
  - docs/code-review/comment-standard.md
tests:
  - tests/benchmarks/test_benchmark_execution_plan.py
doc_type: review-batch
---

# Benchmark 执行计划契约

本批逐项审查21单元：16个真实函数、3项配置、1项callable alias赋值及标准入口。原20键在scripts_remaining.tsv原位置复核，补1个标准入口键；20项补中文文档，入口在台账中说明无需重复注释。职责、调用时机、参数、返回、必要约束、失败及生命周期、引用均按适用情况记录，未新增BUG/TODO。两个JSON解码callback是已列真实函数，不重复计算；借用统计类和原测试定义不在本批增加信用。

原10261B/322LF逐字保留，在EOF追加25526B/411LF纯#注释，现35787B/733LF。AST含位置属性、原docstrings、1927个非COMMENT/NL token的种类拼写及1925个原坐标保持；仅两个虚拟EOF token由323移至734。两个code对象同时保留后完整marshal均为12047B，SHA256 eb3b795485790bcf3ad0536895af91f7ac3908c674144aa01ab1ce4d784cb06a。静态比较使用物理Python3.12.10，实际测试使用正常CPython3.14.4，不作跨解释器marshal等价声称。

作业检查接受非空list及精确case/implementation字段，复制记录并以复合身份去重。过滤条件None与空序列不同；先校验作业和过滤、形成非空候选，再验证seed并洗牌。局部SplitMix64与逆序Fisher–Yates产生版本化顺序，不消耗全局random状态；相同输入顺序、过滤、seed和实现版本才有相同字节的前提，不保证不同seed必有不同排列。plan是可变Python值，不执行基准或认证registry成员；build_execution_plan只绑定同一函数对象，保留旧state键身份。

严格JSON解析在对象hook拒绝重复键，在constant callback拒绝非标准常量，再检查根与filters字段；返回的jobs、seed仍由构造层做语义检查。序列化先形成完整bytes，输入/输出“-”借用stdio且不关闭。文件输出mkdir后直接覆盖，写出失败可能留下目录、截断或部分结果；没有回滚或原子替换。main正常返回0，try内OSError/ValueError返回2，argparse和其他异常另有传播路径。输入输出同路径先读完再覆盖；这些契约未作行为修复。

修改前BEFOREr2与修改后AFTERr1分别运行原有9个unittest方法，两轮均9通过、0失败、0错误、0跳过；结果JSON和测试stdout逐字相同，stderr只一条耗时行不同。原方法包含9次CLI调用，负例预期非零，第二次正向stdin调用未单独断言returncode，不把全部子进程概括为exit0。原文件用例的TemporaryDirectory位于已登记E临时根，退出后为空；专有字节码缓存前缀始终不存在。未修改方法、helper、断言、钩子或夹具，未增加测试。

两轮控制器、argv、13项显式环境与空PATH前缀相同，各冻结14份仓库输入及16项物理工具资源共30项有限输入。使用已存在的正常Python3.14.4，清空PYTHONPATH并禁用户site及字节码写入，所有本任务产物均在登记的E根。原CLI子进程没有单独-B/cwd/env/timeout覆写，继承上述环境；外层180s限制不能冒称每个原child的独立timeout。静态F2的793条原EOL锚全部核对，F2/I2仅修订三处借用subTest元数据，未改源注释提案。

首次BEFOREr1控制器把测试原有两个全局名写错，在进入原方法前exit1；原失败证据保留。r2只将控制器两处字段改为REPOSITORY_ROOT/EXECUTION_PLAN_SCRIPT，再取得上述9项基线，不据此标产品BUG。源租约ba1至f679的Status20四路径提交已逐项衔接；共享脚本台账在本次BEFORE开始前就是Status20投影后的8ed67版本，跨运行阶段仅本模块纯注释原始字节变化。

当前498份台账在源租约登记。本批只更新20旧行并末尾追加1行，391条其他数据行及header共392条物理行逐字保持，其余497份台账保持。selected21格式检查无错误；整份共享叶仍保留389项历史无关格式问题。inventory只改该文件已有reviewed行的rawSHA及说明两格，4091键和51条其他工作树差异保留。原测试14单元仍待独立批次，不以借用的9项运行授予测试定义新信用。

CMake集成入口是benchmark-registration.cmake、benchmark_task3_suite.cmake与run_performance_suite.cmake，C性能入口是现有perf_runner.c。旧B缓存中的Python别名及生成CTest图未运行或刷新，本轮无需C构建。30项输入与显式环境端点比较不证明完整stdlib、sitecustomize、Windows加载器/系统库、完整PATH、其余继承环境、原子快照或无ABA；未测错误分支只按调用链审查登记。

F2/I2、源租约、严格比较、两轮原输出及最终投影凭据见E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/benchmark-execution-plan-*。交付前继续完成源码独审、实际暂存复核及git diff --cached --check。历史Root分批数2798加1入口为2799，不是全仓去重覆盖率；必要E产物保留，手工释放0B，生命周期OPEN，全仓目标继续。
