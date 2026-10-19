---
related_code:
  - zr_vm_lib_math/include/zr_vm_lib_math/complex.h
  - zr_vm_lib_math/include/zr_vm_lib_math/complex_registry.h
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c
implementation_files:
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c
  - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c
test_files:
  - tests/cmake/run_projects_suite.cmake
  - tests/fixtures/projects/native_numeric_pipeline/src/signal.zr
  - tests/fixtures/projects/native_complex_capture_probe/src/main.zr
plan_sources:
  - "user: 全量审查、按需中文注释；公开 API 与程序行为保持不变；按功能模块提交。"
  - docs/code-review/comment-standard.md
tests:
  - tests/cmake/run_projects_suite.cmake
doc_type: review-batch
---

# Complex 当前契约复审

## 范围与结论

以 main 的 aa91ef7fb51d524877626bba5fee8d70da34f948 和实际工作树为基线，逐项复审 Complex 的四个首方文件、61 个既有台账键。包括 14 个函数体、11 个 native callback 声明和 3 个 registry 声明，语句块和静态 descriptor 条目归入同一模块。61 条结论为 32 条已注释、16 条无需注释、12 条 BUG 和 1 条 TODO；没有未审查项。BUG 行是 3 个既有缺陷家族在声明、实现和语句块中的对应记录；没有新增缺陷家族、接口或行为修复。本批不增加全仓覆盖计数，Root 累计已交付的不同单元仍为 3308。

18 段现有 Doxygen 已复核，其中 12 段按当前契约精修、6 段保留原文，未新增 Doxygen 段。源文件只替换 55 处既有物理行的注释：complex.h 48 处、complex.c 7 处；两个 registry 源文件保持原始字节。公开签名、程序语句、字符串和执行行为保持。四文件的职责、入口、必要前提、所有权、有效期、失败状态及“不再补注释”的逐项理由见 [九列台账](../coverage/zr_vm_lib_math_complex.tsv)。

## 当前调用契约

可检索入口按当前构建、源码和现有项目用例核对，不能只根据目录名称判断是否启用。对当前 3795 个可检索候选文件扫描 14 个回调/getter 名称，42 处代码标识符引用的文件、物理行及 UTF-8 字节列与原审查一致，没有文本引用。候选文件比旧扫描的 3782 个多 13 个，恰对应会话释放后补入清单的 Scope13 文件；移除这 13 个文件后，旧扫描成员集合的数量和摘要精确恢复。这只证明当前有限名称与成员集合，不等于动态名称或不可检索入口均不存在。

间接链由 complex_registry.c 的 4 个实例方法和 7 个元方法 descriptor，经 module.c:117、129、142 聚合函数、类型和形状元数据，再由 native_binding_metadata.c 构建绑定信息，进入 native_binding_dispatch_lanes.h:202、205、274、277 的同步派发。context、self、args 和 result 由此次调用借用，不跨调用保存。返回的对象和字符串归 VM 管理；宿主保存裸对象时仍须维持有效根。

Construct 可能复用与 owner prototype 兼容的 self，否则经 construct target 或 owner prototype 建立目标；不承诺每次分配新对象。其余对象运算创建新的 Complex，不改写输入。缺参和标签错误走 VM 异常入口；字段读取或对象 helper 的普通失败与非局部 Throw 分开处理，只在成功时消费结果槽。有效 state 下字段 setter 失败或字符串创建失败会 Throw；后段异常可能保留已经写入的字段，不提供复合操作整体回滚，也不保证普通 C 尾部清理一定执行。无恢复点时的终止边界由上层 VM 处理。

registry getter 只借出模块映像内的 const 描述符、数组和字符串，不创建 VM 实例、不转移所有权。借用期间 provider 映像必须保持加载，调用方不释放、不修改嵌套数据。空模块函数表不代表没有实例方法。数学模块上层聚合器的首次可写表初始化仍依赖宿主同步；getter 的 const 数据不能作为整个模块线程安全的证明。

旧审查中“字段 setter 忽略失败后仍成功”和“toString 将 NULL 短串结果当成功”的前提，已按当前 object_write、SetString 和 Throw 链撤回。构造字段失败传播的旧待核前提也已核实；这来自当前调用链，未在本批修改程序行为。历史 [Complex 批次](20261014-complex-contracts.md) 的旧前提以本批当前证据和台账为准。

## 保留的问题

- 范数平方中间值：普通 binary64 IEEE 求值下，有限输入 (1e200, 0) 的平方先溢出，magnitude 得到 Inf，normalized 可缩放为零复数；入口为 complex.c:26、45–47。
- 乘法有限抵消：lhs = rhs = (1e200, 1e200) 的实部数学值为 0，分离乘积先溢出，complex.c:74 的 Inf - Inf 写入 NaN。
- 平方模比较：不同有限输入 (1e200, 0) 与 (2e200, 0) 的平方模均变为 Inf，complex.c:94–95 返回 0，折叠本应严格不同的模长。
- NaN/unordered 的 tri-state 用户约定仍为 TODO；沿 complex.c:94–95、Core value/meta.c:29–31、682–704 及实际比较指令消费者继续核实。当前默认返回 0 本身不足以证明该策略是缺陷。

上述数值反例依据当前完整调用链及普通 IEEE 求值推导，没有在本批注入运行反例；现有项目通过不能称为缺陷动态复现，也不能把 TODO 标为已解决。

## 源码与支持文件证据

实际应用记录为 `math-complex-current-source-comment-application-r1.json`（a636c26e6a218babe29fb82b11747a58089367ea0a5847608b3bb0eb9045278e，24319 B；Root writer53036/317233 与 different-full-fresh39220/b3abd0 均实际 0）。四文件的 before/after 原始摘要、字节数、EOL 序列、55 处 assert-old 替换和逐行逆向证据均在该记录中。注释移除后的有限手写 C atom/string 序列、原物理行和 UTF-8 字节列逐项相同；逆替换恢复四份原始字节。注释长度变化允许后续绝对字节偏移变化，不声称完整旧字节前缀或全部绝对偏移不变，也不将此证明冒称 compiler lexer/AST 验证。

修改前，将 44 个支持文件与旧审查逐一比较，其中 42 个原始摘要一致。Common 公共头和 Math common.c 的差异仅来自此前已提交的注释：去除已批准后缀并逆替换对应注释行后，两份旧原始摘要精确恢复。2747 个旧证据行均在逆向基线上恢复；当前分类为 2640 个原始字节与偏移一致、104 个字节相同但偏移移动、3 个 Math common.c 既有注释行改写。当前扫描和两个已提交注释变更的桥接记录是 `math-complex-current-native-baseline-bridge-r1.json`（df932fe95c6cfc9c326b82bbd4d8ecb8127339d3af27ac10b613d3197cbacdbd，1848852 B） 与 `math-complex-current-native-baseline-caller-supplement-r1.json`（8ffb0b4e89e56aa9a77fbaa89930292ce401632a1fc07fcfab470ba17b3d7629，266058 B）；不把旧证据行一概称为原始摘要不变。

## 修改前后验证

实际 BEFORE 由 Root31344/driver29208 执行（942939/session5962 → f72cda，原外层退出 1）；AFTER 为 Root24932/driver39272（59b43e/session79896 → 85b198，原外层退出 1）。构建 PID6884 和 44556 均退出 0；suite PID11440 和 42028 均退出 1。两次各 2 个原生步骤与 4 个查询全部已等待，无超时或 launch error。AFTER 唯一 outer 为 `math-complex-current-native-after-r1-outer-terminal.json`（e0f6ec86912507c8884a4084b483d2cdb0c6033151077bc53e44388bc1e5b4cf，2072 B）。

唯一配对记录 `math-complex-current-native-final-acceptance-r1.json`（1fadc41836d73d24045bf0147487c00ac2439e25b9760ceda7de8ad0238658b8，63809 B），Root writer45640（9bd0ab/session61861 → b38996）和不同 PID7204 完整重构（9b3253/session10897 → a1d1ca）均实际退出 0；该 0 表示有限证据比较完成，原 suite 的 1 保持。唯一独立复核为 `math-complex-current-native-final-acceptance-independent-review-r1.json`（57026c98909fbef88791956f9bc46c78868e55dbbe0e7be5df171ab8ef2ac54b，26381 B；writer30008：0c15f7/session97528 → d03fa4，完整 fresh28400：8d4a50/session69314 → 3e263c，均实际退出 0）。精确输入池 2522 条、1013 C 对象和 1 个 ASM 对象、1003 个不同 C 源路径、17 个 FileAPI 目标、16 个静态库、2013 个选中编译输入及 1032 个当前运行构建文件均来自本目标；只有两处批准注释源码 pin 改变，重建后的 deps 仅允许 mtime 变化，其余字段和成员集合精确一致。

每阶段运行同一份现有 run_projects_suite.cmake，以实际 FileAPI 识别的 CLI EXE 执行，显式指定 -DTIER=core，保留原 42 项意图、执行顺序、expected_exit=0 和构建配置。两阶段实际只到第 36 个 marker：此前 35 项通过原 CMake 检查，第 36 项 network_loopback 因 BUILD_NETWORK_LIB=OFF 未注册 zr.network 而失败，后 6 项未启动。原 suite、driver 与外层实际退出码 1 均保留；本批比较成功前缀、相同网络失败和两份既有报告，不称 42 项全通过。稳定的 numeric 报告逐字节相同；GC 报告保留两次原始值，分别满足原报告模式：checksum 为 253640659 和 374502164，均与各自输出和返回值一致，minorSamples=30、fullSamples=7。GC 压测在 main.zr:229、258、299 将 lastStepWork 遥测混入 checksum，原 run_projects_suite.cmake:510–520 只要求数字格式。我先前自行推定 checksum 应固定，并因此产生一次实际退出 1 的配对检查；该无原测试依据的推定已撤回并留证。当前未定位此次 GC 工作量差异的具体来源，不声称 GC 前后完全等价或将它定为新缺陷。Network 失败区段仅移除阶段路径后比较字节摘要。

6 个 Complex 相关 case 是这 42 个中的子集：import_capture_native、native_chain_calls、native_complex_capture_probe，以及 native_numeric_pipeline 的 banner、result 和 probe。构造/模长正例见 case 23、25、29、30、31；归一化和相角在 case 30、31 对应的 pipeline 中执行，phase 写入返回记录，但 checksum 只消费 signalInfo.magnitude，没有针对归一化或相角结果的数值断言。case 32 的 probe:vmState 不授予 Complex 数值信用。这些场景不构成全部 11 个回调、每个浮点边界、GC/OOM 失败或异常恢复路径的覆盖。其余回调没有新增定向数值运行证据；构建与模块装载也不能替代这些断言。

每阶段使用独占物理 E 盘夹具目录。现有 runner 复制 297 个文件和 171 个子目录，限定 37 个 core bin 根、8 个临时 provider 根、两处复制文件中的路径 literal 改写及两份报告；原始测试样例不改。非 core 输入、.manual_prepare 和 .gitkeep 保持。修改前原审计额外报告两处复制文件的 raw 差异：Windows 的 FILE READ/REPLACE/WRITE 将预测中的混合或纯 LF 转为 CRLF。经原始样例、唯一 literal 改写及逐行字节复核，只在 RAM 视图中更正这两个精确 forecast pin，再执行完整 no-follow 夹具审计；没有修改旧 P、helper、runner、原始样例或失败夹具，也没有放宽未知输出检查。修改后的 P 使用同一已核实转换重新预测其阶段路径。成功路径嵌套 CLI 调用数为静态推导的 52 次，未取得每个嵌套 CLI 的 PID/exit 单独回执；直接受跟踪的是 2 个步骤及 4 个 Ninja 查询。读取保护在 Windows DirEntry.stat 与 Path.lstat 的 inode 表示差异上曾误报，失败发生于任何查询、P、夹具或 native 启动之前；R1 保留，R2 仅统一为同一 no-follow 状态接口并通过独立复核，没有放宽未知输出或重解析点检查。

每阶段输入池、真实 C/ASM 编译命令、VALID/#deps 数量、libffi 九项手工依赖、工具身份、生成文件、cache 和运行产物均由当前 CLI 目标重新派生，未继承其他目标的对象计数。原始 stdout/stderr 分别保留，不拼接虚构跨流顺序。未知残留或读取错误保留为 HOLD，不做自动清理重试。验证仅为当前 Windows clang-cl Debug 有限运行，不授予 CTest、WSL、其他平台、所有 ABI、线程安全或全部 AOT 执行信用。

## 台账和提交边界

模块门禁为当前 61 行 `check-batch`、本批源码和文档差异检查及实际暂存的 `git diff --cached --check`；执行结果与精确索引、提交回执一并留在同批 reports 中。全仓 inventory 检查另行保留修改前后的真实状态：现存其他模块的过期记录尚待后续批次处理，不能把本模块通过写成全仓通过；本批只刷新四个已核源码条目，保持外围差异。

只更新旧 Complex 叶的 61 个既有键、四个清单条目的 sha256/status/note 三列，以及本批说明和导航。16 条无需注释的逐项理由原样保留；其余完成理由与实际应用、配对验证及已知限制对应。全仓 508 个覆盖叶的 43960 个不同键保持不变，不增加单元信用。其他会话的 49 行 86 个清单单元格和四条既有暂存记录保持。提交仅含实际改变的两个源文件、Complex 覆盖叶、本批说明、清单和导航六条路径。

保留 F/I、支持文件桥接、失败诊断、实际应用、两阶段原始输出、唯一独立审查及精确提交记录。产物生命周期 OPEN，实际手工释放 0 B；复用 E 盘构建和共享 cache，保留必要失败证据及其他会话资产。
