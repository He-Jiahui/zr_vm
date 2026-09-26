# Astra 所有权与对象访问分离补充实施计划

## 目标与计划归档

继续原所有权设计，在 `E:/Git/zr_vm/docs/plans/astra/syntax/ownership-object-member-separation.md` 建立补充计划，并关联现有 Syntax、Using 审计。旧计划作为设计依据，新增记录负责当前缺陷、实施进度和验收证据。

当前基线为 `c95e5387`。本轮检查的 15 项 Python 测试通过 14 项，迁移清单 golden 比较仍失败；历史 53/53 结果保留为历史证据。

保持既定语言合同：五个保留字 intrinsic；`.`、`?.` 和 `?.(args)` 负责目标访问；直接访问空目标抛出 `NullReferenceError`。不新增公共语法、API 或 artifact ABI。

## 实施顺序

- [ ] **修复待返回值的所有权生命周期。** 对接 Using 审计 U-F1，在现有 Shared/Weak 测试中先复现 pending return 清除后引用计数未下降的问题。修复公共 pending-control 清理路径，使正常恢复、异常替换、无值控制转移和线程重置都释放原有持有；初始化仍只初始化存储。
- [ ] **验证释放期间的重入安全。** 覆盖重复 clear、替换返回值、自别名输入，以及 Drop 回调触发嵌套调用或异常。清理必须只释放一次，待恢复的返回值和异常保持有效；解释器与 AOT 共用该合同。
- [ ] **修复跨隔离域释放丢失句柄。** 对接 U-F2，在 `ZrCore_Ownership_ReleaseValue` 重置 Shared/Weak 存储前验证控制块所属域。拒绝操作时保留值身份与引用计数，同域正常释放及最后一次 Drop 行为保持一致。保留现有函数签名。
- [ ] **补齐 abrupt cleanup 回归。** 覆盖 Shared/Weak 返回经过嵌套 `finally`、返回被异常覆盖、循环内 `break/continue`、Weak 链返回值及隐藏 Shared 清理。已有 Astra 工作者持有的同一问题沿用其实现，避免重复修改。
- [ ] **收敛迁移清单。** 在相关代码和测试提交后用正式 scanner 再生 golden，连续运行两次验证确定性；保留完整比较、负向 fixture 和零 findings 要求。

## 测试与验收

- 先运行公共运行时的引用计数、Weak 失效和 Drop 次数回归，再运行 ownership separation、Shared/Weak、Unique/Drop、receiver guard、CFG finally、异常与编译器集成测试。
- 使用同一源码清单建立 GCC、Clang、MSVC 构建证据；生命周期修复增加 sanitizer 检查。记录编译器、配置、源码版本、真实退出码及失败数。
- 执行 VM、生成 C、LLVM 和 `.zro` 回读测试，比较结果、异常类型、副作用顺序与清理次数。平台能力缺失单独记录，不能计为执行通过。
- 再生语言矩阵 artifact 两次，第二次 SHA-256 不变；三工具链消费同一产物，确认二进制执行返回预期 `64`。
- 重放当前完整注册测试图、CLI/LSP smoke、55 份 syntax 状态及两个 Python verifier。测试数量以当前注册结果为准。
- 重测原设计的五种 receiver 性能场景，检查每条受保护链只有一次 wake、无成员名分派和额外包装分配。

## 提交与完成标准

- 在当前 `main` 按缺陷分别提交代码、回归和相应文档，使用详细 commit message；共享文件与 Git index 采用单一提交负责人。
- 最终补充记录逐条关联原设计的完成条件，全部验收通过后再更新总状态。其他领域失败记录责任与复现证据，期间继续可独立完成的项目。
- 清理本任务产生的构建产物和日志，保留已有并行修改、第三方 submodule 改动及受版本管理的测试 artifact。
- 默认继续现有所有权设计；Using 的未冻结语法和全仓性能优化不扩入本任务。