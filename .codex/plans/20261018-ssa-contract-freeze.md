# 00.02 完整共享契约冻结接续

状态：完整字典推进；后端准入与三TU拆分、33案GREEN、7服务场景、2项CTest及6C有限MSVC语法均已独审接受，正准备有限提交。任务ssa-contract-freeze-01a0fe2b，完整47叶goal持续active；本叶尚未签收。

## 目标与现有证据

执行docs/plans/ssa/00-measurement-contracts/02-contract-freeze.md的四编号任务及三批出口。此前16组纯checker与8种AOTIR reserved组合是已提交的有限证据，保留其精确范围，不重复冒充整叶验证。直接修改E:/Git/zr_vm实际main checkout，禁止源码副本、snapshot、tar、worktree、跨盘镜像。

所有报告/缓存/编译产物只存config.toml指定E:/cargo-targets/zr_vm的reports/cache/build/tests/tmp。新产物先登记reports/ssa-contract-freeze-01a0fe2b/artifacts.json。与Source41产物和历史失败证据分别登记归属，保留其它会话源码、索引、jobs、leases及未知缓存。

## 完整出口分母

- I：主计划19接口族、Read/Write拆分后20操作，逐项列真实声明、实现、alias或planned及下游owner。计划伪名不构成重复API需求。
- F：实际持久format/version/record/field及全部wire offset；同名字段跨格式不能去重。进程内address-free identity不自动等于磁盘格式，带指针manifest不伪造wire offset。
- H：每个hash域算法、宽度/顺序/端序、包含与排除字段、producer与recompute/admission consumer；signatureHash同名不自动同域。
- C：每个F/H项的全部writer/reader/admission反向引用；不存在的consumer明确planned而非虚构实现。Runtime handles/pointers/callbacks与磁盘generation分列。

## 已读源码后的候选政策

1. 保持generic checker的纯相等/位mask合同，等值任意logical11仍可比较；不把它当consumer支持集合。
2. backend service当前支持execution schema6/logical1；queryTarget没有logical参数，不宣称已有任意logical协商。
3. 目标ABI由已注册descriptor负责，非零且request与所选注册精确匹配；不机械钉为AOT ABI17。相同目标判重键暂不含ABI，明确现有限制。
4. descriptor/target及request/result contract reserved零准入；结果污染在READY之前CONTRACT_MISMATCH并沿原unpublished cleanup。不只比较reserved相等。
5. 保留request flags/immutable-input优先于contract，Complete generation优先于contract；新增诊断报告实际字段，不改公开结构/版本。
6. 不机械统一所有codec reserved政策：区分writer写零、reader忽略和reader真实拒绝。ZRAF旧版UNSUPPORTED_VERSION到上层source-recompile是映射，不改底层enum迎合草案。
7. metadata token是域内table/RID，runtime TypeId/manager generation/load epoch不可伪装同一稳定ID。loadEpoch草案没有现成实现时明确新API owner及下游依赖。
8. CoreIR是ExecIR旧称；非法IR/静态link拒绝、合法speculation baseline、显式dynamic、unsupported与pending分别描述。严格数值和域共享/复制/热更规则以实际owner为准。

## 执行批次

- [ ] 1. 读齐所有当前声明、codec/hash和producer/consumer；Sol封定I/F/H/C完整盘点与缺口，独立审查确认分母，不先猜字段总数。
- [ ] 2. 按模块职责维护execution ownership、persistent fields、interface matrix、compatibility四文档并链接原guide/叶计划。冻结独立版本域、历史AOT ABI2全部已知include路线与隔离政策、runtime-only生命周期映射；未知仓外消费者如实限定。
- [x] 3. 独立审查backend准入政策与原错误优先级，刷新实际源码基线后实现。原巨型backend按服务生命周期、编译协议、默认facade拆为三个TU，40原函数均保留，36函数signature/body逐字不变；私有头与公开ABI、generic checker、code_handle边界已核。共享CMake统一四个生产TU，Core递归收集新文件。独审F3546F3D接受拆分与gate，generation诊断缺口另列，未称完整正确。
- [x] 4. 隔离真实service最小RED已实际运行：原10案2PASS/8个预期语义FAIL，旧实现暴露unsupported logical/reserved缺口；4阶段0/0/0/8、CASE状态与计数全匹配，未出现cleanup检查失败。独审51B3A6B9接受。定长栈夹具无真实机器码、VM、IR或FFI对象。
- [x] 5. 有限backend GREEN已实际通过并独审：33/33准入案、7服务场景、2/2 CTest；非17 ABI23正例、失败恢复、NULL诊断、输入不变、U→R清理、无READY/PUBLISHED及优先级均保留。额外6C MSVC语法通过。此固定栈mock不分配堆，OOM不适用于新增fixture；原服务取消协议与真实并发/partial-init仍按下游接口分别验收，不据此签收全部接口。
- [ ] 6. 按具体安全入口审核source/artifact/profile/patch兼容矩阵：current/old/unknown、source重编译/旧artifact拒绝/profile忽略/patch拒绝分别映射实际消费者。涉及真实加载、capability授权、网络或信息安全功能先限缩为静态/纯local metadata，不借收口越界测试。
- [ ] 7. 核全部主接口族及持久字段唯一owner、语义与跨域生命周期决定，I/F/H/C无未决项才签收本叶。新增API若属于下游实现则明确owner和依赖；不把预期future integration写成既有能力。
- [ ] 8. 实际构建与必要回归独审、文档和精确owned双He-Jiahui提交、自有临时产物hash清理及释放/保留/未知登记。逐有限交付commit，完整00.02出口另签收，full47继续推进。

## 当前授权

Source41已提交4f53caa8并完成7项已知自有临时文件清理；完整47目标继续。本计划最小RED已真实运行且独审接受，随后授权并完成后端准入及三TU拆分。当前生产冻结，Root共享CMake已冻结为admission/service两个CTest。已独审接受的旧service fixture仅执行metadata mock：入口哨兵从不调用、map为标量、resume只转发诊断。扩充33案已实际32PASS/1FAIL复现独立contract generation诊断；最小修正后GREEN33/33与service7通过，独审221A4D88接受。MSVC6C语法独审394EB90F接受；原50实时guard及8项先前封定头离线补充分别登记。尚未完成本批Git提交。两项任务分别登记产物，不持有跨会话source/index/build hold，不停止其它会话job。

## 2026-10-18 字典独审接续

Sol原盘点FCD3A6A7与design B189D823保持原样；Luna独审05B25A3E仅接受算术计数，不接受完整字典。六个EIS metadata及两个counts记录的208字段须修正坐标基准并补定位公式；FUNCTION structuralHash producer、signature嵌套hash helper/consumer闭包以及BackendDescriptor/CallBindingTarget/CallBinding运行期字段owner/lifetime须增补。原824 wire字段模板未包括旧native IO完整版本/分支字典，Sol已确认reader精确拒绝非16..45patch、平台宽度/端序/格式等不匹配，仍不能据此宣称全部native padding ABI一致。只追加精确delta和旧IO独立字典，禁止复制另一份全量清单或源码。未改源码、未启动native，完整00.02尚未签收。

## 2026-10-18 历史10案RED及当前33案接续

原353行10案已实际证明2PASS/8预期FAIL，原日志、计划及收据保持；原辅助器88B1拒绝的未执行版本不授成功信用，修正后的A04E7D9A静审与0CF2F7B3实际收据对应。测试现扩充为574行33案，保留原10案顺序和生命周期断言，13个直调入口不增加；新增准确诊断、优先级、NULL diagnostic与两失败槽后健康复用。静态预期仅result_contract_generation一案FAIL：Complete应报contract generation的1/2而当前报generationKey的1/1，必须实际复现后最小修复，不修改正确oracle。service7场景入口闭包02AD7F55已独审接受并加入当前共享入口，实际编译须验证UNDEBUG生效。

旧IO坐标/site/packed分项由5B6BDC接受，计数35→实际18独立pins及527字段精确行号delta保留，原输入不改。Prototype14+31=45字段pack1固定60/124B已核；端序/版本语义和其它旧格式逐项登记。contract-inventory补表FC771882及独审BD740436接受15运行期记录/115字段路径、H有限union31/C有限union315；这些是有限overlay，不能作为全项目分母。I19接口族/20操作归属和移动后视图继续补齐，完整00.02仍open。
