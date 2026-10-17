# 生成头文件分类与配套测试契约审查

审查窗口：2026-10-17；原始代码基线 main `0f889998976cca908404c0cd3e20ecc8250a7362`。逐单元台账为 [layout_generated_includes.tsv](../coverage/layout_generated_includes.tsv)。范围仅 `scripts/layout/generated_includes.py` 与 `tests/tooling/test_layout_generated_includes.py`，`apply_moves.py`、迁移测试与 README 仅作为调用证据，不获得本批定义信用。

## 审查清单与结论

本批完整覆盖 32 个定义：实现 10、测试 22；按种类为 20 个函数、2 个类、8 个模块状态、2 个语句块。32 个定义范围、20 个函数体及 2 个类 suite 均单独核对；每行九字段记录调用方、意图、约束、证据和决定。31 个单元补充纯 `#` 中文文档，保留 Summary、Parameters、Returns、Constraints and lifecycle、References；测试 `__main__` 入口保留一行“无需注释”判断。没有未审查单元，无新增 `BUG:` 或 `TODO:`。

F1 与独立 reviewer 对 6 个完整依赖、689 条包含原 EOL 的证据行、动态加载入口及 unittest 发现机制逐项复核。全量 493 份既有 coverage leaf 中未发现这两路径的现存定义，32 个键均为新增候选；本批不会把借用调用方或测试集成说明算作额外定义。结构化引用计数为 400；全文与九字段合并检索为 517；两种口径的唯一锚均为 133。它们不是互相替代的统计量。

## 已核实的协作契约

`apply_moves.py` 缓存动态加载器通过 `sys.modules` 注册后执行分类模块；这是 dataclass 解析类归属所需的加载顺序。测试用另一固定模块名加载，模块身份与生产缓存不同，加载失败不提供回滚或恢复已有 `sys.modules` 项的保证。分类对象中的 `Path` 只是值，不延长文件或目录生命周期，`frozen`/`slots` 不构成 Literal 值的运行时校验。

分类保留输入 compile-group 变体的原始顺序与重复项，每个变体先检查源目录，再检查有序 include roots。先存在的候选即使不安全，也会阻断后续根；不是“跳过不安全候选后继续选择”。仓内普通文件先按 repository 规则判断；仓外仅选择构建根内精确 `zr_vm_ffi_fixture_path.h` 名称的生成头进入白名单。生成头分支只查询文件系统元数据，不读入头内容、不编译或写入目标。父相对路径可在最终仍落入仓内时接受；绝对路径、drive 与 NUL 请求有各自拒绝路径。

词法 `abspath`/`relative_to` 及逐组件 reparse 检查是当前时点的有限观察，不是文件身份认证或并发替换锁。缺失组件可继续，其他部分 `OSError` 会判不安全；严格 `resolve` 的竞态异常仍可传播。调用方需保持路径输入、仓根和构建根的类型及一代有效性；including 路径允许等于 root，不能据此承诺其 parent 总在仓内。函数没有打开的文件句柄需要交给上层释放。

配套测试的 setUp 为每个 case 创建独有临时目录；成功 setUp 后由 unittest 调用 tearDown，包括断言失败或 skip。setUp 中途失败没有 `addCleanup` 兜底，不能承诺 teardown 必定执行。测试只在其独有合成树创建/删除人工头，不清理共享真实 build 或父根。symlink case 仅在创建权限可用时证明对应候选拒绝，不证明所有 junction、祖先 reparse 或竞态路径。

## 实际验证及边界

注释前后均以真实 Python 3.12.10、`-X utf8 -B` 和相同隔离 cache prefix 执行原有 10 个测试方法；两次均为 **9 通过、1 跳过**，所有 driver、helper 与测试子进程均已确认实际 exit 0。唯一跳过为 `test_reparse_candidate_is_rejected_when_symlink_creation_is_supported`，Windows `WinError 1314` 表示创建 symlink 权限不足，该分支不计作已运行。原有真实 build 生成头探针正常通过；借用头只比较 stat 元数据，未读取、哈希内容、修改或重建。

Root 编排器只在实际测试模块导入后、unittest 发现之前，将模块 `FIXTURE_ROOT` 改为本任务预登记的 E 盘测试根；没有改测试函数、hook、`REAL_BUILD`、断言或默认脚本入口。两次使用完全相同的 argv、控制器和环境；6 个有限仓内输入在各次运行中稳定，跨批仅这两个批准的 EOF 注释变动。测试 ID 与每项结局一致；运行时长和 skip 文本中的随机目录允许变化，不声称日志逐字一致。没有运行额外集成迁移用例、CMake、CTest、C 构建或 Linux 测试。

两源均保留全部原始字节作为前缀，仅追加 31 块纯 `#` 文档。全部 token 的种类/文字/顺序、原有 AST 属性和 docstring、相同文件名下 compile-only marshal 字节均相同；未执行模块来建立编译等价性，没有生成 pyc。F1 的 2911 个原程序 token 包含 60 个未移动的内部 DEDENT；Root 的 2851 计数排除全部 DEDENT，二者口径已区分。只有各文件末尾的 DEDENT/ENDMARKER 共 4 个虚拟 EOF 坐标随注释后移，原程序实体坐标均不变。

## 可追溯证据与提交门槛

物理证据根：`E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/`，前缀 `layout-generated-includes-`。F1 SHA256 `54bbd87f244c679083e88ba6bae435fe8504b6017c80e36e583984c328360704`；独审 F1 `77687b0f8ba63f8392f061115f11be50fe43f0bec67e4a3726c3815535510b12`；Root 严格等价报告 `5ed4b0c99a1d59e50aa4905c5a12ca59f28459ab18488e3383b073d7a97bec65`；实际 BEFORE 汇总 `a73c677385a2400d2cfd883f30a48fcdca140f512cb4c8ac5768e149ba8ba2a3`；实际 AFTER 汇总 `9326f1b7b6494c53b8c3b3bb105c48a22c78f8fea50f5a4ada6e8fe5f622f575`。

本批提交需再通过实际源码与台账独审、Root 逐项暂存差异审查、独立暂存审查和 `git diff --cached --check`；这些门禁与确切父提交/路径/blob 记录于同前缀 serial receipt、stage audit 与 commit delivery，不以此文档预授将来提交权。inventory 仅更新两个归属行的哈希、状态和注记，其他会话的未提交行与暂存项原样保留。

自有 case 子目录由原测试 TemporaryDirectory 清理，测试父根现为空，隔离 cache prefix 未创建；它们不计 Root 释放空间。证据、复用构建/缓存与未知归属项继续登记保留，生命周期仍 OPEN，Root 本批实际释放 0 B。此前清理自动审批拒绝保持原记录，未重试或绕过。当前批次不是全仓覆盖完成结论。
