---
related_code:
  - zr_vm_library/src/zr_vm_library/file_copy.c
  - zr_vm_library/include/zr_vm_library/file.h
  - zr_vm_lib_system/include/zr_vm_lib_system/fs.h
  - tests/library/test_file_path_operations.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/file_copy.c
  - tests/library/test_file_path_operations.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 全仓首方逐单元调用链审查与中文注释
tests:
  - zr_vm_file_path_operations_test
  - tests/cmake/file-path-operations-tests.cmake
doc_type: review-batch
---

# 文件复制、移动与路径回归契约

本批审查22个单元：复制实现8项、原路径测试8项、公开C声明2项及File/Folder回调接口4项。20段中文Doxygen覆盖职责、使用时机、参数/返回、输入与时序约束、所有权及失败后的状态；两个宿主宏保留“无需注释”判断。逐项定义、调用方、入口、测试证据、契约及结论在zr_vm_library.tsv的18条记录与zr_vm_lib_system_fs_public.tsv的4条记录中，无未审查单元。四个接口沿用既有GC TODO，未增加问题家族，未标新BUG。

Copy/Move的可检索调用点、注册表回调入口、上下层参数获取与结果发布均已追踪。路径先规范化、检查源存在，再判同路径空操作和严格组件祖先关系；危险重叠在可能删除目标之前拒绝。词法检查不认证大小写别名、符号/硬链接、任意非NULL地址或检查后被其他进程修改的路径。源为实际目录时走递归，其余存在类型走文件内容复制；上层File/Folder名称不能替代底层类型检查。

overwrite、父目录创建、文件打开与递归复制都可能在后续失败前修改目标，失败不回滚。close结果被忽略，成功不等于持久化或关闭成功；errno并非每个失败分支都重新赋值，成功也不清空旧值。Move可先尝试rename，再回退复制及移除源；同步调用可能阻塞，不提供原子提交或跨进程锁。私有helper的输入、句柄/目录列表所有权与调用前提分别列在文档中。

File回调在底层操作后执行RefreshEntryObject，但未以其普通false阻断后续路径；Folder Copy没有RefreshEntryObject步骤，保留原源路径/快照，只为目标新建包装对象。Folder Move后原源路径可能已不存在。实际文件操作早于VM结果对象分配和发布，后者失败不能撤销文件系统变化，也不能概括为失败时结果总已清空。SetString(NULL)可走Throw路径；临时根和裸借用文本的边界仍按实际异常/GC调用链记录，四项既有targetPath风险保留为待核，不冒称已复现缺陷。

旧四条函数记录在原位置由file.c迁移到实际file_copy.c定义；file.c仍存在的同名宏记录保留。新增12个精确键，其余10个是旧键重审，其中6个是公开声明。先前四条BUG前提已被上游1b2db388c65f36c3aca8f4b9ce10bbddd590bc4e的存在性/路径门禁修改，本次按当前完整调用路径更正文档，不修复行为。library叶其余228条物理行（含header）、Fs公开叶其余190条物理行逐字保持。file.h只重审两个声明，其余仍为pending；fs.h原前缀不变，保留既有整文件历史状态，不把局部复核冒充新一轮193项运行验证。

原六份源码的完整字节前缀保留，仅在EOF追加48段闭合文档；本提交涉及其中四份、20段。实际Clang raw-token比较六份合计6792个程序token，种类、拼写、原坐标、StartOfLine和LeadingSpace均相同；不改变API、行为、断言或测试。fs.h由827行增加到1116行只因文档，不新增职责；本轮按用户注释范围保留布局，未来拆分可按File、Folder、FileStream边界处理。

同一已登记E盘Debug静态配置完成修改前/修改后实际构建，两个build进程均exit0。AFTER有94条进度（含glob），93项编号动作中84次C编译；不把979个图节点全称为新编译。原13个FilePath场景两轮全部exit0、各55条PASS。跨三个目标的22进程/24 runner用例两轮通过，44份输出流逐字节相同，13个FilePath相对夹具状态和文件内容hash相同。Numeric与Integer的用例只作为共享构建证据，不计为本模块场景。运行直接执行原EXE，CTest登记另作入口证据，没有新测试。

构建窗口4637项有限输入保持稳定。构建后其他会话又修改10份源码（1份AOT测试不在selected图，另9份Parser源/头可能在图内），因此不授当前整工作树构建通过信用。原EXE及PDB、6份图/缓存/Ninja元数据、15项工具、6份本任务源码组成33项运行guard，两轮运行和13个夹具另有端点校验；33不是完整宿主依赖闭包。两个零依赖VALID对象按实际C/ASM输入说明；原依赖采集因重复hash过慢被终止exit1，独立补采5次只读Ninja查询及原11次输出核对均成功，原失败记录保留。原AFTER外层driver退出码不可恢复，build/helper已await0，未伪造外层0；316项SDK头仅在FIRST构建后冻结。

冻结证据目录为E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/。FileCopy F2及独审I2、公开声明R2及独审I2约束注释真实性；library-review-comments-current-r1-*记录实际应用/strict lexer/SourceI；library-review-native-build-{before,after}-r2-*与library-review-native-tests-{before,after}-r2-*记录实际构建、原输出、夹具、补充与独立复核。最终逐行投影及真实暂存另取独立机械审查，提交前执行三项cached whitespace检查。两位角色的源文档真实性和暂存机械范围分别留证，不以作者自审取代独审。

本批仅在inventory既有四行更新本任务字段，其他工作树行保留。22单元中12新键、10旧键，历史Root分批数2799+12=2811不是全仓去重覆盖率。文件清单仍有待审内容，全仓目标继续；必要证据、可复用构建/缓存及两轮夹具保留，手工释放0B，生命周期OPEN。未作Linux、Sanitizer、OOM注入、完整GC、原子快照或无ABA保证。
