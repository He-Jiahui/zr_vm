---
related_code:
  - tests/scripts/test_syntax_migration_inventory.py
  - scripts/syntax_migration_inventory.py
  - scripts/syntax_migration_embedded.py
implementation_files:
  - tests/scripts/test_syntax_migration_inventory.py
plan_sources:
  - user: first-party caller-first contract review
  - docs/code-review/comment-standard.md
tests:
  - tests/scripts/test_syntax_migration_inventory.py
doc_type: review-batch
---

# Syntax migration 测试逐单位契约审查

本批复审15个既有单元：模块配置、参数helper、unittest类型、11个原方法及主入口。14份中文纯#文档追加在EOF；13已注释、1无需注释、1保留原TODO。原588LF/22486B前缀、docstrings和所有断言不变；新增307LF/25604B后共895LF/48090B。AST全属性、2412个非COMMENT/NL token的种类/拼写、前2410个实体坐标及同上下文完整held marshal均相同，仅两个虚拟EOF从589移至896。

## 调用、失败和生命周期

模块先处理首个--repository路径对并移除相应argv，再设置sys.path、导入生产模块，unittest反射选择11方法。三个参数说明已精确区分全局待测模块与用例自有TemporaryDirectory根；类本身没有统一setUp/tearDown清理。四个方法各持有临时目录，mock上下文恢复替换对象；缓存可能继续持有已删除临时根的值。测试中的C/Zr字符串只作为扫描输入，不执行对应语言。

fixture与真实仓库承担不同合同：词法形式集合有子集比较、单个坐标范围和固定协议；缓存案例只证明本次不同长度重写使stat签名失效。CLI案例先要求子进程返回0，再读输出并与父进程API字节比较，不比较冻结repository golden。子进程无显式-B、env或timeout，外层继承环境提供禁缓存/UTF-8约束。真实仓库方法先检查JSON稳定和空findings，只有通过后才能到达后续分区、golden和闭包。原“五处Wiki”注记保留同一意图TODO，不能代替当次结果；没有新增BUG/TODO问题或行为修复。

## 原有运行与输入边界

明确复用Syntax53实际AFTER11作为本批修改前基线，不称新增BEFORE。该历史运行与本轮fresh AFTER原11均9通过、2失败、0错误、0跳过，测试exit1。完整结果JSON和stdout相同；stderr只归一化一条耗时行后相同。原独立CLI均exit1，stdout为空、stderr相同；物理嵌入式Python在生产源码15行找不到syntax_migration_embedded，测试578行返回码断言失败。仓库方法342行实际看到5条Wiki finding，433–436的golden及后续闭包未达；不能称全绿、CLI导出或golden通过。

首次AFTER守门发现外来输入变化，在plan/helper/测试/CLI启动前拒绝；恢复预检又发现文档继续漂移，同样未启动运行。两次exit1记录均保留。之后r2重新捕获当前真实输入并立即运行，Root32880/helper44496均exit0；原测试32600与CLI51080均exit1，全部自有进程已等待终态。

历史到本轮跨阶段有5个新tracked路径、4个新候选、25个新增或改变的扫描raw条目；旧输入语义等同不在证明范围。Root上一批文档按legacyOrMigrationFixture排除，仍参与metadata签名。SSA提交193f464的12路径与本批22个语义依赖无交集，parent桥显式登记；活跃Macro/SSA文件没有回退或纳入本批提交。原源码本身等价由上述严格检查支撑，运行结果只作已观察比较，不声称仅自有注释变化。

本轮两个端点的7345 tracked、2794候选、1906扫描、888排除、7个fixture文件/11成员、3程序、2物理工具及1920项保守pins均一致。固定E临时父目录运行后为空，禁写缓存路径仍不存在。端点为有限顺序观察，不证明原子、无ABA、完整宿主环境或系统库冻结；完整PATH/继承环境跨阶段等同也未主张。

## 台账与提交门禁

tests_scripts_remaining.tsv仅9–23行的15旧键原位更新；其余16条数据行及表头共17物理行原字节保留，其他496叶保持本批lease原SHA。所选15项及全31行check_batch均无格式问题。inventory仅本测试已有行的rawSHA和说明两格更新，reviewed状态不变；其他工作树差异和顺序保留。0新规范键、0重复信用；Root历史分批计数仍2795，不是全仓去重覆盖率。

F2/I2、Rootproposal/lease/严格等价/透明基线adoption、两次拒绝及r2原始流保存在E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/syntax-migration-tests-*。独立实际源审查、逐项暂存复核及git diff --cached --check须通过后方可串行提交。未改公开签名、算法、断言、fixture、golden或配置，未新增测试。完整仓库目标仍继续；必要E证据、复用构建与缓存保留，手工释放0B，产物清单保持OPEN。
