---
related_code:
  - scripts/syntax_status_records.py
  - tests/scripts/test_syntax_status_records.py
implementation_files:
  - scripts/syntax_status_records.py
plan_sources:
  - user: first-party caller-first contract review
  - docs/code-review/comment-standard.md
tests:
  - tests/scripts/test_syntax_status_records.py
doc_type: review-batch
---

# SyntaxStatus 状态记录契约

本批逐项审查20单元：9个函数、2个数据类型、6项模块状态、2个同步排序callback和标准脚本入口。18个已有键在scripts_remaining.tsv原位置复核，另补2个此前未列的lambda键；17项补中文文档，三个简单入口/callback在台账中说明无需重复注释。职责、调用时机、参数或字段、返回值、失败与生命周期、引用均按适用情况记录，未新增BUG/TODO。

原8748B/231LF程序逐字保留，在EOF追加19253B/305LF纯#注释，现28001B/536LF。AST含全部位置属性、原docstrings、1433个非COMMENT/NL token的种类拼写和1431个原实体坐标相同；仅两个虚拟EOF token由232移至537。两个code对象同时保留后完整marshal均为11975B，SHA256 a0bc188ace5b7e27ab60ec558d4530fb7dabac675f56fbe69b44b68a678ba53f。冻结I1中1403是排除全部DEDENT/ENDMARKER的另一计数口径；本批Root记录明确纠正，不改写原I1。

记录与报告的frozen dataclass限制属性重新赋值，报告内directory_counts仍为可变dict。采集器生成的记录、分组与缺失分区彼此一致；公开构造器不强制此不变量，调用方应保持一致并在验证/输出期间避免并发修改。报告持有文本值，不持有已读取文件的句柄，也不阻止后续文件改变。

选择器立即构造列表，按直属目录首字符isdigit筛选，再递归枚举Markdown；它不限定两位ASCII编号，也不在末端以is_file作额外校验。实现计划后缀及精确support路径排除，最终按相对路径排序；两个lambda仅供同步排序，不外逃。首个匹配字段可在strip后成为空串，None只表达未匹配；完成时间保留文本，不校验日期。validate检查现有计数、目录分布及预先分类问题，不重新解析身份或日期。

collect读取当前UTF-8文件，目录枚举与逐文件读取不是原子快照；错误会向上传播，不发布部分报告。to_json与文本格式化依据调用时的对象值，main负责stdout及退出码；JSON有问题时仍输出报告并以1退出，读写/解码异常、参数解析及正常问题退出具有不同路径。本批未改输出协议或CLI参数，也未新增--output。

修改前与修改后分别直接运行原有4个unittest方法：两轮均4通过、0失败、0错误、0跳过。原JSON CLI两轮均exit0，55份记录全部满足现有校验。结果JSON、测试stdout及CLI两条原始流逐字相同；测试stderr只一条耗时行不同。三个TemporaryDirectory均归属登记的E临时根，退出后为空；专有字节码缓存前缀始终不存在。没有改测试方法、helper、断言、钩子或夹具以取得证据。

两轮使用相同控制器、argv、8项显式环境设置与空PATH前缀，冻结87份仓库输入和物理Python/zip/pth共90项有限输入。55份选中文件原始字节与F1一致；其余20份排除候选的原始字节是本轮新登记的保守输入，未倒称历史F1已冻结。根32直属成员、75份编号目录候选及55份选中成员在两轮端点相同。唯一跨轮运行输入变化是本脚本纯注释；源租约fc4至Viewer提交ba1的4条路径明确不在90项输入内。

F1至本轮的借用测试台账变化仅为Root Test15已提交的15行记录，其余包括StatusTests9原行保持。当前498份台账在源租约登记；本批只更新原18行并末尾追加2行，392条非本批物理行含header逐字保留，其余497份台账保持。selected20格式检查无错误；整份共享叶还保留429项历史无关格式问题，不为其授予审查信用。inventory仅改本文件已有reviewed行的rawSHA和说明两格，4091键及51条其他工作树差异保留。

当前B无专用SyntaxStatus CTest登记；本轮无需C构建，未运行或刷新旧CTest图。有限文件、工具及显式环境比较不证明完整PATH、其余继承环境、Windows加载器/系统共享库、原子快照或无ABA。文本CLI分支及异常路径以调用链审查为依据，不冒充新增动态覆盖。

F1/I1、Root源租约、严格比较、两轮原始输出与投影凭据保存在E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/syntax-status-*。20单元完成后的提交仍须实际源独审、暂存复核和git diff --cached --check；历史Root分批数2796加2新callback为2798，不是全仓去重覆盖率。必要E产物保留，手工释放0B，生命周期OPEN，全仓目标继续。
