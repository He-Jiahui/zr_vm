---
doc_type: code-review-batch
related_code:
  - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h
  - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c
implementation_files:
  - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c
plan_sources:
  - docs/code-review/comment-standard.md
  - docs/plans/ssa/10-jit-platforms/03-platform-matrix.md
tests:
  - tests/core/test_ssa_platform_matrix.c
  - tests/cmake/ssa-tests.cmake
---

# Common SSA 平台契约复审

本批逐项核对107个单元：C的36函数、2对象宏和4语句块；H的31个普通宏物理定义、16类型、17公开接口及1 guard。旧105个exact key在原两份台账中更新，另追加policy函数与guard两键；107是本批复审规模，仅2键此前未登记，105不重复累计。8条static assert属于已有宏协议，不另造单元；测试24函数及32依赖文件仅作证据，不授其定义信用。

正式决策为commented51、no-comment53、TODO3。新增53段中文Doxygen（H36/C17）、55个@param、21个@return；54单元不新增Dox，其中C DetectHostAbi为TODO并复用H公开契约，其余53项无需新增注释。提案的no_comment_units=54指“不新增Dox”指标；本批将其明确为no_new_documentation_units=54，不能当作正式no-comment决策数量。没有未审项、新问题、测试、API、程序token、断言、配置或行为修改。

原H20771B/442CRLF、C52839B/1144LF字节与原程序坐标精确保留，仅EOF追加已独审的中文文档。公开接口说明调用时机、输入可空边界、输出、不变量、首失败、资源及线程限制；内部只解释不能从实现名称直接判断的编码、门禁和调用协作。简单枚举范围判断及已被H完整覆盖的实现不重复加文档，其理由逐键记录。

## 当前契约与旧问题复核

Check绑定declared与observed，按身份、枚举、目标、ABI及哈希、需求/来源策略、后端/分派、outcome/阶段和可选语义见证的现有顺序返回首失败。共享policy在C198、815、988连接两个observation入口，拒绝Android/iOS/WASM的三类JIT触发及CROSS_COMPILE声称executed/semanticPassed/PASSED。旧叶Check、IsRuntimeAcceptance的两条BUG理由已不适用于这些具体触发，故仅更正过时台账，未修代码、未恢复历史临时探针或声称所有JIT/运行风险已消失；当前源文件本来没有这两条BUG标签。

IsRuntimeAcceptance仅是单份记录的轻量自洽谓词，无declared参数，也不比较三种外层契约哈希或完整ValidateCapability目标规则。Artifact先验证声明/身份/ABI/哈希和需求子集；需求JIT位未被声明支持时先返回REQUIRED_FEATURE_UNSUPPORTED，不能保证到后面的FORBIDDEN分支。任何返回OK/真都不认证记录来源、设备执行或产物装载。

Check与ValidateArtifact先清诊断再读取输入，diagnostic须不重叠输入及内嵌数组。较低层校验只更新相关诊断字段，成功不保证抹掉旧详情。值对象由调用方持有，本模块不保留指针、不分配或执行；输入读取期间稳定，输出独占写入，未提供锁或原子发布。名称查询返回静态字符串借用。

保留两个既有TODO issue共三键镜像：triple文本尚未绑定target/architecture语义，以及通用native callback ABI哈希尚无目标专用真实调用见证。旧C的两个TODO标签原样保留，EOF两处新增说明对应这些相同issue及具体核查入口；新增BUG/TODO issue为0。

## 调用、构建与测试边界

全量可检索180个标识符、1083处程序出现及191真实直接调用已按定义/调用/引用分列；guard台账补足matrix.c:8、umbrella头:32和本C:1三个已知直接包含入口，保留无需注释判断，不使用“未见使用”模板。17个公开ABI通过ZR_API导出，当前普通消费者集中于本C和matrix测试，未因缺少其它调用而判为死代码。umbrella头的下游与Rust conf引入是可用性入口，手写Rust extern及build.rs未生成这些接口的绑定；未发现当前文本中的平台API跨语言调用不证明外部动态使用不存在。

32个完整依赖pin与2179个含EOL行pin支持契约、入口和现有测试；Common CMake导出GLOB源列表，由消费目标编入。tests/cmake/ssa-tests.cmake:970–978直接注册现有matrix目标与CTest。20个main case是普通assert值夹具，测试包括策略、ABI/哈希漂移与诊断，但不构成真实Android/iOS/WASM/线程/PMU/JIT/设备或native callback执行见证。

实际复用E盘Windows Clang19.1.5 Debug构建，原代码BEFORE构建exit0、CTest1/1、直接main exit0且完整流为空，34个锁定输入/3601个有限观察输入首尾稳定。历史Opcode AFTER之后两项外来测试配置变化已在本次fresh BEFORE重新采集，不继承旧运行信用。构建保留unused c11atomics及配置长路径告警，不声称W0、完整依赖闭包、SDK/配置原子隔离、Linux或全库验证。首次汇总筛选把Common对象链接行算为编译命令，断言在汇总文件创建前退出1；实际native未失败，保留失败reader并改为真实-c动作筛选后独立汇总。

严格词法比较通过，H/C原程序token、flags和原坐标相同，四次stdin raw lexer完整消费stderr并退出0。原代码BEFORE与文档AFTER构建均exit0、CTest均1/1、直接main均exit0；完整stdout/stderr逐字相同且为空，三个argv、20项cache及两个实际源编译命令相同且无NDEBUG。34个锁定输入、3601个有限观察输入在各次运行首尾稳定，前后只有本H/C的53段EOF注释变化。实际source/leaf与stage独审、串行提交待完成。
