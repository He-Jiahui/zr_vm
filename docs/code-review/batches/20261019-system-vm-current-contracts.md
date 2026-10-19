---
related_code:
  - zr_vm_lib_system/include/zr_vm_lib_system/vm.h
  - zr_vm_lib_system/include/zr_vm_lib_system/vm_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/vm/vm.c
  - zr_vm_lib_system/src/zr_vm_lib_system/vm/vm_registry.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c
  - zr_vm_core/src/zr_vm_core/object/object_write.c
implementation_files:
  - zr_vm_lib_system/include/zr_vm_lib_system/vm.h
  - zr_vm_lib_system/src/zr_vm_lib_system/vm/vm.c
  - zr_vm_lib_system/src/zr_vm_lib_system/vm/vm_registry.c
plan_sources:
  - "user: 全量调用链审查、按需中文注释、问题只标记，按功能模块提交。"
  - docs/code-review/comment-standard.md
  - docs/library-and-builtins/zr-system-submodules.md
tests:
  - tests/module/test_module_system.c
  - tests/library/test_official_provider_convergence.c
doc_type: review-batch
---

# System VM 调用与注册契约复核

本批重新核对29个旧完整三列键，原叶原位更新；不迁移定义、不增加信用。结果为14 commented、11 TODO、1 BUG、3 no-comment。范围含公开回调、注册接口、实现与所属语句块；57份有限完整输入、223处证据及可检索调用点由冻结审查记录针定。vm_registry.h两项只复核，源码不改。

11份Doxygen中8份修订、3份保留。三个源码共27项同物理行注释替换，说明调用职责、借用资源、异常边界和上下层配合；原程序与字符串拼写、行号、UTF-8列及每行EOL保持。工作树原有CRLF及混合EOL逐字节保留，Git暂存按既有属性归一化为LF。inventory记录工作树原EOL哈希，应用及暂存报告分别记录原始和LF哈希，不将两者混称相等。

当前写入层已由TrySet失败接Throw传播；SetString在字符串创建返回NULL时会Throw，不会以未初始化局部值继续写入，void setter也不等于吞掉错误。GetExport、延迟导入及调用均在现有TryRun边界内，撤回旧“导入先于handler”的推论。旧键CallModuleExport pre-handler import window保留身份，意图和限制已改成当前实际边界。

保留一项已有BUG：Array_PushValue普通返回FALSE的完整调用路径中，调用方忽略失败，仍继续遍历并发布可能缺项的模块信息数组，影响与触发证据见29行台账；本批不修行为。11项TODO分别保留根保护、可变快照、插件导出借用、多mutator、Array_Get早于helper清理等待核查边界，不把尚未执行的GC、故障或并发场景当作已证实缺陷。

历史批次文档仅更正5处已失实的当前契约表述，原执行记录及哈希段落保留。模块说明zr-system-submodules.md的VM小节已与当前事实相符，本批复核后无需改写。静态独立复核结论接受，并在产物清单单独保留4处证据路径元数据勘误：实际路径是native_binding_dispatch.c、native_binding_object_write.c及core/object/object_write.c；冻结原报告不重写，有限完整输入及行锚不受该转录勘误影响。

修改前后使用同一当前FileAPI构建及VS/Clang配置，构建各退出0；直接运行module78项及provider9项既有Unity入口，每阶段87项全部PASS、无FAIL或IGNORE，逐项原始Unity结果和footer一致。provider stdout及两组stderr逐字节相同；module stdout只在Pass - Cost Time数值变化，完整字节经精确计时字段替换后相同：77个计时字段、76行数值改变。本批不作性能结论。

应用报告system-vm-current-source-application-r1.json，SHA256 32e2d62f44c377376fbcd4d8ef35d6624db4052fa874941fe8f76e5aa4ce4a0b；最终配对system-vm-current-native-pair-r1.json，SHA256 5dfcf11aa9b7050895b4bd1c0f858ad283d35f7bb5dffb77fb8fbc9035f84bb8。前后原始结果4fc724e6c5f197f50b13735374c0fa26787ba2c7ce342ea9707bc2e992962e6f、f46e5c72765ac31c38c26c55fa9536ce6b16dc489fe489074b842b2ddd9853f5。当前87PASS与旧批次70/78及8项当时失败分开记录。

直接EXE验证不称为CTest，120秒只是任务runner上限。实际checkout作为SOURCE_DIR，生成输出限定物理E盘登记测试目录；两阶段结束均无生成夹具残留。未执行故障注入、GC保活、多mutator或线程组合，不宣称29项均有运行覆盖或全仓通过。

inventory仅4个受审源码的sha、status、note三列更新；外来49行86格差异和所有其它工作树原始行保留。全局check修改前后仍为同一既有stale；完整输出和退出码记录于checks。check-batch检查本叶全部29行的九列、完整键与非空物理行锚；调用真实性另由冻结证据、Root及独立复核承担。
