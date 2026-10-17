# Benchmark Viewer 测试契约审查

审查窗口：2026-10-17，main 基线 a2a324cc7c0f2b6d1baffc564c7283cd888677dd。范围仅 tests/benchmarks/test_benchmark_compare_viewer.py；逐单元记录见 [benchmark_viewer_test.tsv](../coverage/benchmark_viewer_test.tsv)。实际 HTML、Node harness、生产聚合器及其依赖、CMake 登记与既有说明作为调用和测试证据，不增加本批定义信用。

## 清单与结论

完整覆盖 24 个单元：15 个函数、1 个测试类、6 个模块状态、2 个入口/装载语句块。19 个单元新增中文纯 # 文档，覆盖 Summary、适用的 Parameters/Returns、Constraints and lifecycle 与 References，共19个参数项和4个非空返回项。ROOT、BENCHMARK_SCRIPTS、VIEWER、HARNESS 及标准 __main__ 共5个单元记录无需新增注释的理由；没有未审查单元，无新增 BUG: 或 TODO:。

F1 与独立 reviewer 核对24个完整定义范围、15个函数体及类 suite、10份完整依赖和344条含原 EOL 的锚点。495份既有 coverage leaf 按精确键、物理文件、旧 basename 和符号入口交叉核对无旧定义交集；24键为新增，借用页面既有24键不重复计数。

## 调用意图与生命周期

benchmark/comparison/summary 构造内存展示模型，PASS、耗时与比率是夹具输入，未运行 benchmark 或计算真实测量。comparison 返回含 suite/cases 的外层字典，另创建 cases 列表及 relative_to 字典；summary 才把传入报告放在 reports 中。嵌套输入值可以继续共享引用，调用方在 page 序列化前保持模型稳定；这些构造器不校验完整测量身份或 schema。

模块装载先修改本进程 sys.path，再按 viewer_actual_aggregate 规格创建聚合器并执行正文，未把该私有名登记到 sys.modules，也不恢复搜索路径或常规依赖缓存。应在可信、稳定 checkout 的受控新进程执行；导入失败直接中断装载，不能把私有名当完整模块隔离。仅导入不调用聚合 CLI 或写 bundle 文件。

setUpClass 优先读取非空 BENCHMARK_VIEWER_NODE，后备 which；缺 Node 是类级错误，非 skip。它只保存共享命令字符串，不验证非空路径可执行；实际启动失败在 page 传播。测试无文件夹具或 setUp/tearDown 资源钩子，执行期间共享类属性不应并发改写。

page 只读真实 HTML，按需调用生产 _inject_bundle 在内存注入 UTF-8/base64，再把 HTML 和文件文本序列经 UTF-8 stdin 交给独立 Node harness。非字符串模型先 JSON 编码；读取、序列化、注入、启动、非零退出、超时或输出解析异常均直接传播。返回快照由父 Python 持有，不借用已经退出的 DOM 对象。

harness 使用简化元素对象保存字符串和标志，FileReader 的 onload 同步执行；它不解析 HTML、不触发布局或真实攻击事件。vm.runInContext 的2秒时限只包住每段初始 inline script，后续宿主 change/FileReader 回调不在此范围。Python subprocess.run 的10秒限制该子进程等待/通信，不是整个 case 的硬时限；管道和子进程由原 subprocess.run 管理，不能据此把 VM 当任意页面的安全沙箱。

## 实际测试及边界

原有9个 unittest 方法在注释前后各实际运行一次，均9通过、0跳过、0错误；全部测试 ID 与完整解析结果一致。覆盖有限 comparison-only 区域清理、bundle 自动加载及中文/非 BMP、C#/.NET 分组、时间戳转义、legacy 包装、模板提示和损坏 JSON 输出。每例只按实际断言授信：23处直接 unittest 断言，加辅助函数中1处断言定义及7处辅助调用，共31个静态 assert 前缀调用点；31不是运行断言次数。

运行使用真实 E 盘 Python3.12.10 与物理 Node v22.22.3，固定 BENCHMARK_VIEWER_NODE、NODE_OPTIONS 空值、TMP/TEMP 及 UTF-8 环境，并用 -B 和保持不存在的隔离 cache prefix。原方法、hook、断言和 Node 调用未改写或插桩；driver/helper/Python 测试进程均已等待真实 exit0。Node 由原 page 的 subprocess.run 同步等待，没有另造每个 Node PID 的追踪证据。两次控制器、argv 和环境相同；10份仓内文件及2个工具在各次稳定，跨次仅本测试文件的批准 EOF 注释变化，不声称钉住整机或全部共享运行库。

本批未创建夹具目录、生成 HTML 或执行聚合 CLI、CMake、C 构建、额外 CTest、Linux 或真实浏览器。DOM 字符串正例不是完整安全审计；不外推全部 schema/base64/Unicode 错误、异步 FileReader、任意模板布局或未直接断言的区域。stdout 完整结果相同；stderr 含不同时长，不声称原始日志逐字一致。

## 注释等价性、证据与保留

原6042B/133行完整保留，仅 EOF 追加19块纯 # 文档，最终37348B/564行。原 AST 含位置属性和 docstrings、全部 token 种类/拼写/顺序及实体坐标相同；1254个实体 token 包含 ENCODING 和内部 DEDENT，仅虚拟 DEDENT/ENDMARKER 坐标从134行移到565行。相同文件名、编译选项及保留 code 对象方式下，compile-only marshal 前后字节相同；此门禁不执行模块、不生成 pyc。

物理证据根 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/，前缀 benchmark-viewer-test-。原 F1 1d23ca182345195b1b924d31ba9dcd7b718014610edb37d7f6367072867c9400；I1 6dc8b1edd7fd5b1d02e090ea6e05ef62368626d6973c898aaf2e4331f6ff7e67 为 REVISE，指出 comparison 误写 reports 容器。F2 c76f0cb63ead420677b0cf93022ca31c7adda92a2b8416740fd2824234d99c6b 精确13处文字/派生字段更正，I2 da663216f201b0c6bf91d567f10b15782df2c3aad8d4053e9fc2b6d0cc1e4a32 独立接受；其余18块文档与原始证据保持。旧报告冻结，继承 producer/registration/source_observation 等字段明确仅为历史。

Root 等价报告 56a3f3bc91ddaeaf587c162b3e32176caff7b3a1daf619d3cec7bb999fcce7fa；原始 BEFORE 汇总 d2588ce24e49213e8dcf93b2e1632f631ce9d8430a5157ffd65b24035ada8c73；AFTER 汇总 85d09553894a3cfaa639ce5a350f47c835bd2c372ca7a036a4814b70dbb54cf9。源码/台账独审、实际暂存 mode/blob/父提交/差异复核、独立暂存审查及 git diff --cached --check 仍是独立门禁；receipt 和 delivery 记录最终提交者与路径，未预先授权未来父提交。

inventory 仅更新本测试既有一行的 SHA、reviewed 状态和注记，其他行列与495旧 leaf 原字节保留。本批实际释放0B；必要证据、复用构建/缓存/索引、Git 元数据与未知/外来产物继续保留，生命周期 OPEN，此前清理自动审批拒绝未重试。本批不代表全仓审查完成。

最终台账状态更正：F2 formal_rows 的 reason 保留作者初审时点的候选说明；最终叶表仅对19个 commented 行的 reason（第9列）作有记录的投影，改为已落实注释与实际前后9项通过的有限结论。5个无需注释的理由及全部定义、调用、意图、约束、证据和决策不变；原 F2、源租约、注释与运行报告保持冻结。投影记录为同证据根下 benchmark-viewer-test-final-ledger-projection-r1.json；它不预授源码独审、暂存、未来父提交或全仓完成信用。
