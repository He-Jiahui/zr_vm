---
doc_type: code-review-batch
related_code:
  - scripts/layout/apply_moves.py
  - scripts/layout/dormant_contexts.py
  - scripts/layout/generated_includes.py
implementation_files:
  - scripts/layout/apply_moves.py
plan_sources:
  - docs/code-review/comment-standard.md
  - .codex/plans/20261012-layout-01a13033.md
tests:
  - tests/tooling/test_layout_moves.py
  - tests/CMakeLists.txt
---

# Layout 迁移工具与测试的调用及生命周期契约

本批完整审查迁移工具和相关测试的 118 个单元：93 个函数、3 个 lambda 回调、4 个类、14 个状态和 4 个语句块。逐项定义、调用方、入口、测试证据、契约和结论见[单元台账](../coverage/layout_move_script.tsv)，结论为 106 commented、11 no-comment、1 TODO，无未审单元。新增 107 份相邻中文注释，含 TODO 所属函数；没有把“无需注释”的单元从审查中省略。借用的 dormant_contexts、generated_includes 和其他测试定义不计入本批。

## 职责与上下层协作

main 解析迁移表、预览、计划与应用选项，再将实际根目录交给下层；读取、校验、包含上下文解析、改写计划和文件应用分别保留自己的前提与失败边界。目录名称不证明模块停用：AOT 的目录别名保留父路径，实际构建上下文以 CMake File API、当前注册及调用证据交叉核实。迁移工具本身及保护的历史、依赖和文档范围不能仅靠别名绕过。

PathRewriter 预先建立替换与目标保护结构，供多处字节改写复用；内部可变映射由调用方借用并按只读约定使用。包含解析按实际编译上下文的顺序和变体求解，不能把多个配置的 include 根做无条件并集。找不到安全、唯一的解释时保留拒绝或待核状态；跨 checkout 的 File API 不作为当前根的授权。生成 include 保持生成路径身份，不把尚不存在的生成文件当普通源码读入。

计划 JSON 的版本、输入指纹、目标碰撞及已应用状态各有独立校验。已存在的目标由实际前置检查拒绝，测试断言为 destination already exists；不能据此声称执行过覆盖或回滚分支。应用涉及文件移动与改写；原模块 docstring 中“不直接更新 Git index”的边界不代表没有文件系统副作用。错误返回、部分进度与恢复限制以逐单元合同为准，不推导全仓事务或崩溃恢复保证。

## 资源、顺序与失败边界

包含上下文、文件字节、重写缓存和计划对象的借用与有效期随所属调用链说明。动态测试模块在执行前登记到 sys.modules，供 dataclass 等加载期间查询模块身份；不能调换该登记与 exec_module 顺序。脚本的 docstring 继续作为 argparse 描述，原文、位置及 __future__ 导入语义保留。

原子改写的临时目录、文件模式与替换顺序有各自限制；“单文件替换”不授予多文件迁移的原子事务保证。测试在专用人工根运行，并以断言验证预览不修改、碰撞拒绝、计划一致性、旧/新路径别名、生成 include 和编译上下文等行为。subTest 内失败后继续仅适用于 unittest 默认非 failfast 模式；failfast 或 subTest 外普通断言可提前终止方法。

## 待核项

TODO: apply_moves.py:1098 使用 ascii/errors=ignore 编码别名，而 :334 的路径校验没有限制相同字符域。需从迁移表支持的非 ASCII 路径、path_rewriter 的匹配和已有调用约定核实预期；现有测试没有该向量。本轮未复现完整触发路径，因此仅记为 TODO，保留实现，不升级为 BUG。

## 注释与台账一致性

新增内容均为相邻纯 # 注释，包含摘要、参数、返回、约束及引用等适用信息；不更改 docstring、接口、语句或现有测试。两份原文件共 2168 条原始行按明确映射逐字节保留；逆向删除 107 个已核验插入即可恢复原文件。AST 去掉位置后及非注释 token 拼写相同。相邻插入会改变后续行号和代码行表，本批没有声称原坐标或整段 raw 前缀不变。

F1 经 I1 的三项语义修订和 R2 的机械引用迁移，再由非作者 I2 独审通过。107 份注释内的 1050 处两份源码引用（1048 References 和 2 TODO）均指向注释后的原代码行；正式台账从已审历史行映射到当前行。三个 lambda 的既有物理键标识保持原样，未因引用行号移动重命名。原始锚点与全文件哈希作为修改前证据保留；借用文件的行号未改写。

I1 中否定命名的 inventory 布尔字段有一处记录错误，R2/I2 已明确外置勘误：当时全清单哈希不同，只核实了两条指定清单行，原不可变报告未被覆盖。本次清单只更新这两行的哈希、状态和说明；两条选中清单行原有的未暂存哈希已过期，本次按已核验的当前内容更新；其余 49 条其他会话的工作树差异保留，不暂存。导航仅追加本批链接。

## 现有验证与交付边界

修改前后使用同一物理 Python 3.12.10、同 argv/env、同 E 盘专用夹具根运行 tests/tooling/test_layout_moves.py -v。两次子进程均自然退出 0，36 个相同的 class.method ID 均为 ok，Ran 36 与 OK 完整出现。stdout 均为空，stderr 各 5616 字节，仅 unittest 汇总的耗时不同。源文件、借用依赖及正常 index 的四项外围变更均按两端哈希守门，没有新增重复实现的测试。

测试仅在人工 fixture 根调用迁移入口；没有对实际 checkout 执行生产迁移，也没有运行 native 构建。用例 teardown 清理其自有夹具，保留的专用根内四个预建空目录、原流和证据逐项登记；不据此声称整个产物根已清理。

为保持调用证据有效，dormant_contexts 与 generated_includes 两份既有台账的 12 行、18 个 caller/evidence 单元格、31 处源码引用仅迁移行号；逐条核对原代码 EOL 在新行完全相同，其他列与键保留，不增加审查信用。

本批提交两份源码注释、新台账、本批文档、导航、清单及两份既有台账的引用，共八个路径。118 个此前不存在的精确键在提交后计入历史 Root 交付计数，2873→2991；该数不代表全仓去重覆盖率。其他会话工作树与 index 均保留。证据、复用私有 index 和专用目录位于 E:/cargo-targets/zr_vm；本批手动释放 0B，产物生命周期为 OPEN。
