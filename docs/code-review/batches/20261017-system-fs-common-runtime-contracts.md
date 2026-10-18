---
related_code:
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c
  - zr_vm_core/src/zr_vm_core/object/object_write.c
  - zr_vm_core/src/zr_vm_core/exception/exception_try_run.c
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 全仓首方逐单元调用链审查与中文注释
tests:
  - zr_vm_system_fs_test
  - zr_vm_system_fs_failures_test
  - zr_vm_system_fs_resource_cleanup_test
doc_type: review-batch
---

# FsCommon 共享运行时助手契约复审

本批覆盖 fs_common.c 的29个已有物理单元：文件职责与28个函数（5个static、23个共享助手）。逐项定义、调用者、native入口、测试证据、契约和判断见 [逐单元台账](../coverage/zr_vm_lib_system_fs_common.tsv)。结论为25 commented、3 TODO、1 no-comment、0 BUG；无未审查项、无新增单元信用，历史Root分批数仍2844。私有头的另外44个旧键及 entry、stream、file_io、directory_listing、Core、Library 与原测试只作调用证据；不把它们算成本批复审完成。

审查由全可检索引用进入实际 callback、字段助手、VM异常与资源路径；966条证据行不等于966次调用或执行轨迹。GetBoolField 当前无仓内实调用，但仍保留仓外宿主/旧closed字段设计的核查入口，不能据搜索判死代码。相同 native helper 从不同 callback 进入时，receiver/参数借用窗口仍须依各入口约束解释。

## 职责、约束与生命周期

临时 ZrLibTempValueRoot 保活的是注册值；发生VM分配后须从根重新取得对象。End的正常C收尾不能当作Throw路径的finally。对象字段写入经 void setter，底层 TrySet 失败可走 Throw；本批校正旧“软失败仍正常返回”的注释，不承诺布尔成功状态、事务回滚或全部局部资源的异常释放。IOException构造与字段物化也服从同一异常边界。

WriteIntField按值封装有符号64位整数。WriteBoolField保留TZrBool原值，不能写成自动规范化0/1；实际可选bool参数读取与默认值复制各有自己的规则。WriteStringField同步读取首NUL前的字符串，NULL按空串处理，调用窗口内输入必须有效。getter返回的对象/字符串为所属VM借用，字段读取默认值与严格类型规则逐项列在Doxygen，不能概括成统一转换或统一IOException。

PopulateEntryObject先根receiver，再查宿主信息、构造info与递归parent；parent的普通NULL结果可继续写parent=NULL。字段已写出的部分不回滚，正常cleanup按临时根逆序结束，Throw可能绕过该尾部。receiver根不自动证明originalPath/fullPathOverride的字符串来源安全，跨VM分配后仍读取原路径的lane/参数根核查保留为TODO。RefreshEntryObject先清输出再验证输入，只在正常成功尾部发布结果；中途setter异常不能解释成普通false。

RaiseErrnoIOException在进入助手时读取errno，而path getter等实参先求值；是否覆盖原宿主错误须沿fs_stream的实际 getter/分配路径核实，保持TODO，不把尚未复现的errno覆盖升级为BUG。本批三项TODO沿用具体核查入口；未新增问题标签，旧批次的历史缺陷文字不直接充当当前调用链证据。

## 注释应用与验证

本轮仅校正11行原注释并追加23段EOF中文Doxygen，精修原有5段文档，共28段函数文档、89项参数与22项返回说明。原区29610B变为30088B，追加42741B，最终72829B/1347LF；完整原字节前缀并非保持，11行的长度变化会移动后续byte offset。实际Clang raw lexer证明3459个程序token的种类、拼写、StartOfLine、LeadingSpace及全部原始行列一致，全部输出均解析且子进程实际0。28个完整定义/函数体也独立核为原字节相同；不改签名、API、行为、断言、配置或测试。

源码超过1000行由文档增加造成，原28函数及职责边界没有扩展；本批不为行数移动实现。后续若另行整理，可沿根管理、字段/实参访问、异常、entry快照四个现有边界评估拆分，不能把本次注释验证当作重构授权。

修改前实际构建三目标exit0（62.812s），修改后同配置构建exit0（14.781s）。两阶段各运行23个原测试进程、30个runner级case：Unity原8例均为7PASS/1FAIL，另8个failure与14个resource进程均exit0，无timeout，全部自有子进程已等待退出。直接执行原EXE、cwd为同一B；没有调用缓存中指向其他会话夹具的failure/resource CTest入口，也未改缓存。

既有失败为 test_system_fs_source_runtime_supports_stream_modes_and_using，tests/system/test_system_fs_module.c:1035，Expected TRUE Was FALSE；诊断为 KNOWN_NATIVE_MEMBER_CALL_RECV_U8: invalid cached native member callable，takeReader的脚本第7行、外层第31行。失败发生在注释前，两阶段保持；其后未到达的流操作及using路径没有执行信用。普通调用拒绝FileStream到i32的类型诊断属于另一通过的负例，不记为新异常；本轮不修复行为，也不推断该失败的完整根因。

前后46份runtime输出流中45份逐字节一致；Unity stdout仅7条耗时字段及4个非NULL运行地址发生变化，指针相等关系与两类指针相异关系保持，其余文本一致。Unity全部case/失败行及另22进程的471条PASS消息一致。两阶段各22个payload文件/4138B与4个Unity新文件/13B相同，仅Unity根名中的time/clock归一并保留counter。原未知40文件/130B与本轮两阶段新增文件均保留，现有Unity共48文件/156B。每阶段构建后封存12项runtime pin，实执行脚本在各测试前比较；逐次hash未持久化，不称各步骤独立receipt。两个原生外层driver均实际0，单个Unity测试exit1仍明确保留。

验证只涵盖有限28项契约输入、实际缓存/图/工具及执行窗口；AFTER相对BEFORE的该集合只改变本C注释字节。不授当前整checkout全绿、完整宿主依赖闭包、原子快照、无ABA、全GC/OOM点、并发、Sanitizer或跨平台证明。resource observer额外编译fs_stream/file相关TU，fs_common仍来自普通静态库对象，不能扩大成FsCommon所有分配点注入验证。

冻结提案/独审为system-fs-common-runtime-contracts-current-review-r3.json（65edd8d8…）和independent-review-r3.json（77568aee…）；actual sourceI为8c31dfd1…，lexer为1d948c9b…。原生原始日志、结果和配对及逐项暂存复核保留在E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/。所有sourceI/native/staged报告的文件内outer null保持原样，外层实际终态另行绑定；不补造历史退出码。

本批仅更新叶台账的29个旧C键、inventory的本C一行及本批/旧批次适用范围说明；其余44个头文件键与其他行原字节保留。必要E证据、复用构建/缓存与夹具保留，手工释放0B、生命周期OPEN；此前自动清理拒绝不绕过，全仓目标继续。
