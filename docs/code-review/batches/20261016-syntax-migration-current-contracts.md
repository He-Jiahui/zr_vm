---
related_code:
  - scripts/syntax_migration_inventory.py
  - tests/scripts/test_syntax_migration_inventory.py
  - scripts/syntax_migration_embedded.py
implementation_files:
  - scripts/syntax_migration_inventory.py
plan_sources:
  - user: first-party caller-first contract review
  - docs/code-review/comment-standard.md
tests:
  - tests/scripts/test_syntax_migration_inventory.py
  - tests/scripts/test_syntax_migration_embedded_positions.py
doc_type: workflow-detail
---

# Syntax migration 扫描器逐单位契约审查

本批覆盖 scripts/syntax_migration_inventory.py 的53个单元：11个数据类型、26个具名函数或方法、8个配置/缓存状态、7个排序lambda回调和1个主入口块。复审46个已有键，补入7个回调键；相关测试和嵌入字符串解析器是借用证据，各自台账不在本批重复计数。所选53项结论为44已注释、8无需注释、1保留原TODO。现有首方文件清单中的此源码从pending转为reviewed。

45份纯#文档追加在文件末尾，说明参数、返回值、职责、适用时机、必要约束、失败状态、引用及生命周期。保留原1291行/45286B前缀，原docstrings和有效token坐标不变；只有两个虚拟EOF位置移动。同上下文AST全属性及完整marshal字节相等。InventoryReport构造器仅存字段，排序、分类及分区由两个build入口负责，fixture投影另排序新数组；此处已按独立复核修正。

## 调用与失败边界

当前调用从fixture和真实Git仓库两个build入口到分类/扫描器，再投影到JSON、文本或fixture golden。仓库缓存每次重新枚举Git候选，以候选元数据组成签名；不是内容哈希一致性或并发原子快照。被排除的候选仍影响签名；缺失tracked路径不扫描。嵌入宿主仅在context准入后交给字符串解析器，adjacent字面量、原文坐标及callers逐项记入九列表。初始化状态、反射dataclass/枚举、排序回调及__main__间接入口均已单独登记，不因无直接调用判为死代码。

原文档扫描TODO保留同一疑点：旧Wiki示例是负向反例还是过时文档，及仓库空findings/golden应由哪层承担。本轮真实仓库测试在第342行确实看到5条Wiki finding，未运行到433–436的repository golden比较；当次数量不能替代意图核查。原896–899行及追加文档镜像同一问题，未新增缺陷、修改Wiki、断言或程序行为。

## 原有验证及比较限制

BEFORE和AFTER各执行原11方法：均9通过、2失败、0错误、0跳过，测试进程均exit1。完整测试JSON/stdout相同；stderr仅归一化一条“Ran 11 tests in …s”耗时行后相同。两次独立原CLI入口均exit1，stdout为空、stderr完全相同：物理嵌入式Python脚本入口在原第15行因搜索路径找不到syntax_migration_embedded，尚未进入main；其中一个原测试也在CLI返回码断言失败。上述结果不能称全绿、CLI导出成功或repository golden通过。嵌入解析器的其他4方法在此只核查静态合同，没有新增运行。

每个运行窗口均核真实Git NUL成员、2790候选元数据、1903扫描文件rawSHA、887排除项、7个fixture文件/11个含目录成员、3个程序、2个物理工具及1917项保守输入pins，窗口两端一致。它们是有限顺序端点证据，不证明原子、无ABA、完整宿主或动态库冻结。

跨阶段另有两处外来会话变化，首次AFTER守门因此在测试启动前拒绝并保留exit1证据。解析器测试文件新增include、tearDown清理、Unity注册三行，分别55/44/72B；在内存逆删精确恢复BEFORE rawSHA，原内联Zr文本未变，但host行号可移动。SSA索引文档按historicalPlan排除，仍参与候选metadata。之后AFTER采用当前输入运行，不回退外来文件；恢复驱动对PATH额外前置相同工具目录，未声称完整PATH/继承环境等同。观察结果保持，严格自有代码等价由AST/token/marshal证据支撑；不声称运行只改变了自有源码。

## 台账和提交门禁

复用scripts_remaining.tsv，46旧行原位替换、7回调行追加；其余356条数据行及表头共357物理行原字节保护。所选53项现有check_batch为0问题；整个混合台账仍有464项未选旧格式问题，与本批预测基线相同，不能称全叶通过。inventory仅本源码行的rawSHA、reviewed状态及说明三格更新，保留其他工作树差异和原有顺序。

F2/I2、Root落源、等价、原始运行流及首次拒绝/恢复证据保存于E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/的syntax-migration-*。独立源复核和暂存收据须按各自门禁登记并通过后，方可串行提交。冻结旧reconciliation中把Wiki9身份误写为403e8d的字段已在新证据勘误：Wiki9实际为122fdb3，403e8d为其SSA直接后继；旧记录不改写。提交前执行现有台账checker、逐项暂存复核及git diff --cached --check，只纳入本批四个路径。未改公开签名、算法、生成配置、断言或测试逻辑，未新增测试。

本批只新增7个台账键；Root历史分批计数由2788到2795，不是全仓去重覆盖率。完整仓库审查仍继续；构建、缓存及必要成功/失败证据保留，手工释放0B，产物清单保持OPEN。
