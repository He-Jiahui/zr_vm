# Dormant 上下文恢复与测试契约审查

审查窗口：2026-10-17；代码基线 main 7889191053f9106ff6d6f6c5cf3403a79e40bcc7。逐单元台账为 [layout_dormant_contexts.tsv](../coverage/layout_dormant_contexts.tsv)。本批仅覆盖 scripts/layout/dormant_contexts.py 与 tests/tooling/test_layout_dormant_contexts.py；apply_moves.py、layout README 和审查校验器仅作依赖证据。

## 审查清单与结论

完整覆盖 55 个单元：实现 28、测试 27；31 个具名函数、3 个 lambda 回调、4 个类、14 个模块状态和 3 个语句块。55 个定义范围、34 个 callable body 与 4 个 class suite 均核对到原字节和 AST；九字段台账记录全部调用方、入口、意图、契约、测试证据和决定。50 个单元补充中文纯 # 文档，含适用的 Summary、Parameters、Returns、Constraints and lifecycle、References，共 59 个参数项、19 个非空返回项。_COMMENT_RE、三个简单 lambda 和标准 __main__ 共 5 个单元记录无需注释的理由；未找到读取引用不据此断言死代码。没有未审查单元，无新增 BUG: 或 TODO:。

F1、独立 reviewer 与 Root 核对 5 个完整依赖、908 条含原 EOL 的证据行；全文文档与九字段合计 1024 处引用、284 个唯一锚点。494 份既有 coverage leaf 中未发现这两个物理模块的旧定义或可证实旧名别名，55 个键均为新增候选；借用调用方和静态测试登记不增加定义信用。

## 已核实的调用与数据契约

apply_moves.py 在 --check 与 --cmake-build 路径中，经缓存动态加载器调用 recover_contexts，再把结果用于独立 dormant header 图。dataclass 所在模块须先发布到 sys.modules 后执行；测试以自身固定名装载，不执行迁移 main，执行失败不保证恢复旧绑定。SourceContext 与 _Registration 是 frozen 值记录，不验证所有直接构造的 status/字段，不冻结文件系统，也不持有构建租约。

CMake 扫描只解释有限普通引号、括号、行注释和字面源声明，不执行条件、函数、变量或生成器表达式。common-wrapper fallback 需要字面 helper 调用证据；目录名、声明出现及恢复 status 不证明目标已启用或构建成功。目录迭代器在 with 中关闭，部分读取错误会跳过文件或目录，结果不代表完整扫描。排除回调的异常可向上传播。

File API 选用最新索引和唯一匹配配置，仅提取 C compile groups；每组 include roots 保持独立有序，不能求并。实际源 variants 优先，不追加恢复证据；任一 owner 缺完整目标组且无受证 fallback 时返回 unknown。raw_context 仍需读取目标组，转换可新建容器，不承诺输入身份。特定读取、JSON 或源根身份错误包装为 DormantContextError；其余类型、解码、路径或回调异常可能直接传播。

header 传播创建独立字典，以 (相对路径, roots tuple) 去重；读取失败保留已收集前缀，不回滚局部图。include 正则不执行预处理，也可能命中 C 注释样本。_safe_repo_file 检查现存组件、reparse 与解析范围，本身不保证最终路径是普通文件；_resolve_quoted 另作 is_file 检查。这些是时点观测，调用方负责目录树、路径和构建证据在查询期间稳定，没有防并发替换锁。

## 实际验证及边界

原有 9 个 unittest 方法在原始源码和最终 F2 注释后各运行一次，均为 9 通过、0 跳过、0 错误；历史 r1 注释后另一次运行也全部通过。driver、helper、测试子进程均已实际等待退出且 exit 0。测试调用本仓库 Python 实现，并在每例独有临时目录创建人工 CMake、File API JSON 和 C/H 文本；不执行 CMake、编译器或夹具中的 C/H 源码。实际图优先、多 owner 缺证据、target_sources、wrapper fallback、错误源根、排除子树和独立 header 传播均按实际断言记录；不声称覆盖所有畸形 JSON、raw_context 分支或竞态。

Root 使用真实 E 盘 Python 3.12.10、-X utf8 -B 和保持不存在的隔离 cache prefix，通过既有 ZR_VM_LAYOUT_TMP_ROOT/ZR_VM_LAYOUT_FIXTURE_ROOT 配置自有 E 根；未替换测试模块全局量、函数、hook 或断言。两次 argv、控制器、环境及全部测试 ID/解析结果相同；5 个有限仓内输入在各次运行稳定，跨次仅两个批准的 EOF 注释变动。stderr 运行时长不同，不声称原始日志逐字相同。没有运行额外 CTest、C 构建或 Linux 验证。

成功 setUp 后由 unittest 安排 tearDown 释放本例 TemporaryDirectory；setUp 中途失败没有此保证。write/file_api 依赖受控相对参数，不提供事务回滚或任意路径认证。原测试清理随机 case 子目录后，自有测试父根为空；Root 不递归清理父根或共享目录。

两源保留全部原字节，仅在 EOF 追加 50 块纯 # 文档。原 AST 属性、docstrings、全部非注释 token 的种类、拼写、顺序和实体坐标均相同；compile-only marshal 在相同文件名、选项及对象保留方式下前后字节相同。6420 个比较 token 包含 ENCODING 和内部 DEDENT；仅两文件末尾 DEDENT/ENDMARKER 共 4 个虚拟 EOF 坐标后移。未执行模块来建立编译等价性，也未生成 pyc。

## 证据、暂存与保留边界

物理证据根 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/，前缀 layout-dormant-contexts-。历史 F1 SHA256 5bf359ba9c8f7f3462cb0fed3f3248787090d05e6c0880bd383a568d92c88cf1；F1 独审 756b117bd4919699bf3f8d6bd9578e0165e29e42c8157df322e354d2ece07b54。最终 F2 SHA256 d171acf7b0c240bca9d4a9ebee37b2d5b077b4afd108d68ed7d498b510c2226f；Root 等价报告 5343667c7d4feba548d1f2a0bf59ce488b93161fc9d4bcf2230374b3e6fe250d；原始 BEFORE 汇总 015c85cf6715abf13ab3b58550315cba1f4101955089ef2a373745deff17a1b8；最终 AFTER-r2 汇总 0890f11baedbd2545893fd6156d2a2d119e8e24ab467f361d12eb36e524126d6。

独立源码复核发现 DOC35 原句错误排除了本仓库 Python 实现与真实文件操作。F2 仅收窄该类的一行纯 # 注释和台账第 40 行 constraints 格，明确实际执行 Python、生成文本/JSON 夹具，但不执行 CMake、编译器或夹具 C/H。原始程序、其余 49 块文档、全部定义/调用/测试锚点不变。F1 数据树精确 13 处文本或派生摘要/字节数变化均记于 F2 的 derived_root_refinement；producer、registration、source_observation 等继承字段明确为历史。F1、I1、sourceI1、r1 lease/strict 和运行证据均原字节保留，原 ACCEPT 不授 F2 通过信用；最终 sourceI2 与暂存独审仍须单独核对。

F1 producer 在 xb/fsync 写完后因 tuple 坐标与 JSON list 直接比较而实际 exit 1，冻结报告未重写。独审从原源码及 suffix 重建全部 16 个 tuple 字段，重新 JSON 编码与原报告逐字相同；没有虚构首次序列化前独立 payload hash。Root 首次写入前也因 inline 与保留引用 code 对象的 marshal 表示不同而停止；统一先保留两个 code 对象后精确复现 F1 摘要，全部等价守门保留。这是证据表示及校验方法更正，不是行为修复。

inventory 两条既有 WT-only pending 行的原哈希保存于 source lease，不推断来源或旧程序状态。本批依据当前 clean 源码及完整审查更新这两个键的哈希、状态和注记，只显式授权将两个 HEAD 缺失键插入提交；其他 WT 行、旧 leaf、暂存项均保留。实际源码/台账独审、逐项暂存差异、独立暂存审查和 git diff --cached --check 是后续独立门禁，确切父提交、路径、mode/blob 和提交者记录于同前缀 receipt、stage audit 与 commit delivery。

Root 本批实际释放 0 B；证据、自有空测试父根、复用构建/缓存、Git 元数据和未知归属项继续登记保留，生命周期仍 OPEN。此前清理自动审批拒绝保持原记录，未重试或绕过。本批不代表全仓覆盖完成。
