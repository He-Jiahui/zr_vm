---
related_code:
  - tests/benchmarks/test_benchmark_execution_plan.py
  - scripts/benchmark/benchmark_execution_plan.py
implementation_files:
  - tests/benchmarks/test_benchmark_execution_plan.py
plan_sources:
  - user: first-party caller-first contract review
  - docs/code-review/comment-standard.md
tests:
  - tests/benchmarks/test_benchmark_execution_plan.py
doc_type: review-batch
---

# Benchmark 执行计划测试契约

本批逐项复核14个既有单元：11函数（9个测试方法、2个helper）、1类、1路径状态及包括既有guard的模块入口。14项均补中文文档，覆盖职责、使用时机、适用的参数/返回、输入约束、失败和生命周期、引用；10项参数与2项返回条目，不新增BUG/TODO或审查键。借用的生产模块、统计类和CMake入口均只作调用证据，不重复计入本批。

原8027B/220LF逐字保留，EOF追加24950B/343LF纯#注释，现32977B/563LF。原docstrings、含位置属性的完整AST、1504个非COMMENT/NL token种类拼写及1502个原坐标保持，仅两个虚拟EOF token由221移至564。两个code对象同时持有后完整marshal均为11713B、SHA256 b112e14156fc984d80047bcdce6151adb217cd72fdfea18cbf1ca2b131438bf6。静态检查用物理Python3.12.10，运行用正常CPython3.14.4，不作跨解释器marshal等价声称。

_jobs每次构造独立作业记录供合法输入用例复用；_identities保留输入顺序与复合身份，供排列和唯一性断言。测试类由unittest loader发现并同步运行，不是性能执行器。路径状态按__file__定位仓库，导入时的sys.path调整影响进程；文档不承诺可卸载恢复或并行隔离。9个方法分别检查固定Fisher–Yates向量、同seed字节稳定与输入不变、不同seed的指定向量、先过滤再洗牌、非法seed/filters、非法或重复作业，以及CLI文件/stdin/stdout与负例结构。

原文件有91处AST调用、29处断言调用、4处subTest站点共27次静态迭代；这些是静态证据，不冒称运行断言计数。原9方法若完整到达共含9次CLI调用（3次正向、6次预期exit2负向），第二次正向stdin调用未单独断言returncode。CLI负例确认错误通道和失败结果，不能概括为全部子进程exit0。TemporaryDirectory上下文管理路径生命周期，测试不执行实际benchmark作业。

修改前与修改后分别重新运行原有9项，两轮均9通过、0失败、0错误、0跳过，完整结果JSON及stdout逐字相同，stderr只一条耗时行不同。控制器3697B、argv、13项显式环境与空PATH前缀相同；各轮14份仓库输入及16项物理工具资源共30项有限输入端点稳定，跨轮仅本测试源码纯注释字节变化。原方法、helper、断言、subTest、钩子、夹具与CLI均未改动，不新增测试。

运行使用既有正常Python3.14.4，清空PYTHONPATH并禁用户site及字节码写入，本任务产物和临时夹具都在已登记E根。已登记专有临时父目录两轮退出后为空，专有字节码缓存前缀保持不存在；原child继承环境，未单独设置-B/cwd/env/timeout。外层180s限制不等于每个原child都有独立timeout。未授予完整stdlib、sitecustomize、Windows加载器/系统库、PATH、其余继承环境、原子快照、无ABA、CTest或C构建信用。

F1的360条原EOL锚逐一核对。F1借用生产源码7ebabfcf至当前413c96d3的差异是已交付EOF纯#文档，10261B/322LF原前缀保持，其余12项依赖保持；旧Benchmark21 AFTER运行只作控制器设计参考，本批重新取得BEFORE/AFTER。旧运行台账8ed67到684b的变化已在本批BEFORE前明确桥接，不以旧测试结果代替本轮验证。

本批在共享tests_benchmarks.tsv原位置替换14旧行，不追加；其余958条数据行与header共959物理行逐字保持，其余497份台账保持。selected14格式检查无错；整份共享叶保留11项历史无关锚错误（原12项中本批一项修正）。inventory只改该文件既有reviewed行的rawSHA与说明两格，4091键、51条其他工作树差异保留。逐行定义、调用方、入口、证据、契约及结论见对应14条台账。

F1/I1、源租约、严格比较、原测试两轮原始输出和最终投影凭据见E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/benchmark-execution-plan-tests-*。交付还须源码独审、真实暂存复核及git diff --cached --check。历史Root分批数2799不变，不是全仓去重覆盖率；必要E证据与可复用产物保留，手工释放0B、生命周期OPEN，全仓目标继续。
