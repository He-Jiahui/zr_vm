# 调试工件类型转发合同

本批逐项审查1个单元；1个旧键不重复计数，新增0个键。定义、全部可检索调用、间接入口、现有测试、约束与每项结论记录于[逐单元台账](../coverage/zr_vm_core_debug_native_log.tsv)；选中行无未审查项。

复核一个已搬迁的实现键，通过layout映射纠正物理路径，并为转发函数补充EOF Doxygen。调用方必须提供成功Open且在查询期间保持只读存活的投影及原始buffer；查询依次消费已验证根、TypeSpec及TypeDef，输出签名字节仍借用工件。状态原样转发，失败不保证outType清空或回滚，调用方须先查状态；包装层不分配、不延长寿命、不提供锁。公开声明及下层消费者仅作调用证据，不重复授予定义信用。

canonical消费者现有21个Unity用例实际运行，其中既有成功/缺失ID调用作为本转发器证据；不外推全部工件格式、失败恢复或并发访问。

## 验证与边界

复用实际checkout与E:/cargo-targets/zr_vm/build/first-party-review-01a13463/clang-cl-products-debug；ClangCL/Debug、Ninja，配置、工具及选中依赖按真实前后记录核对。共同验证构建六目标，直接运行七步骤；故意失败探针的退出1为必要结果。以下只授予本模块对应步骤的有限证据。

| 现有步骤 | BEFORE实际退出 | AFTER实际退出 | 预期 |
| --- | ---: | ---: | ---: |
| canonical | 0 | 0 | 0 |

第一轮真实build-only尝试因外部compile_expression_types.c在构建期间变化而actual1，七测试均未启动，原失败保留，不升级为通过。后续baseline将该源加入strict106并重新运行；七个本任务文件仅有注释变化，原程序文本与位置保持，公开API签名及行为未修改。

实际pair验收：released-contracts-native-final-acceptance-r1.json，SHA256 5d5cff63a88d2315c9510c098632c845063ae9f56e713cc45c31c4dde21827ed；注释等价证明SHA256 003df9bdde1a52b86298fddd4823411089fe308e9921a9214144b36ad4c23765；独审 core-debug-artifact-forwarder-current-contract-independent-review-r1.json，SHA256 687eff673303476b3a8d42e7be491b11f66d0f3af23d8d22ad9a1750870206eb。完整收据及原始输出在E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/。全仓清单仍存在外域pending，不据本批声称全仓完成。

## 源身份与提交

- zr_vm_core/src/zr_vm_core/debug/debug_artifact.c：7a97029d20d8e7526289c58f967e772f4a7e7c29fb0036c9163cdb7185b0217b

提交前必须核对仅本批差异并通过git diff --cached --check、既有check-batch及独立暂存审查；本批不增加重复实现的测试。其他会话的暂存、源码与混合台账外行保留。构建与必要成功/失败证据保留，生命周期OPEN，manual freed0B。
