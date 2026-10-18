# 程序集资源公开合同复核

本批逐项审查8个单元；8个旧键不重复计数，新增0个键。定义、全部可检索调用、间接入口、现有测试、约束与每项结论记录于[逐单元台账](../coverage/zr_vm_lib_system.tsv)；选中行无未审查项。

复核两个公开头中的8个既有单元，并更正1个借用实现行。注册getter借出静态描述符，资源callback由registry及通用native派发层同步调用；VM持有成功交付的字符串或数组，普通失败与非局部异常的清理边界分别记录。当前SetString检查NULL并抛出MEMORY_ERROR，旧短串OOM静默成功文字已失效，本批仅更正文档。内嵌NUL截断、miniz提取NULL/0可误作空结果的两项旧问题及非局部清理TODO继续按完整路径留证，无新增BUG/TODO或故障注入信用。

仅现有Assembly两个main用例的普通路径；不宣称OOM、miniz故障注入或全部异常资源回收通过。

## 验证与边界

复用实际checkout与E:/cargo-targets/zr_vm/build/first-party-review-01a13463/clang-cl-products-debug；ClangCL/Debug、Ninja，配置、工具及选中依赖按真实前后记录核对。共同验证构建六目标，直接运行七步骤；故意失败探针的退出1为必要结果。以下只授予本模块对应步骤的有限证据。

| 现有步骤 | BEFORE实际退出 | AFTER实际退出 | 预期 |
| --- | ---: | ---: | ---: |
| assembly | 0 | 0 | 0 |

第一轮真实build-only尝试因外部compile_expression_types.c在构建期间变化而actual1，七测试均未启动，原失败保留，不升级为通过。后续baseline将该源加入strict106并重新运行；七个本任务文件仅有注释变化，原程序文本与位置保持，公开API签名及行为未修改。

实际pair验收：released-contracts-native-final-acceptance-r1.json，SHA256 5d5cff63a88d2315c9510c098632c845063ae9f56e713cc45c31c4dde21827ed；注释等价证明SHA256 003df9bdde1a52b86298fddd4823411089fe308e9921a9214144b36ad4c23765；独审 system-assembly-public-current-contract-independent-review-r2.json，SHA256 a1b0e3e1386fc17c9482aa151b33410f199c941177e3d704abdab00c499ead93。完整收据及原始输出在E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/。全仓清单仍存在外域pending，不据本批声称全仓完成。

## 源身份与提交

- zr_vm_lib_system/include/zr_vm_lib_system/assembly.h：c6b3e1ee4f91bb221dafd1f15d2103d9ccc3ce2c1b5b522736775200477adc5c
- zr_vm_lib_system/include/zr_vm_lib_system/assembly_registry.h：eab51ec89937edce0511d37713be1e99671442b68440b037b98f4bcb32d27a28

提交前必须核对仅本批差异并通过git diff --cached --check、既有check-batch及独立暂存审查；本批不增加重复实现的测试。其他会话的暂存、源码与混合台账外行保留。构建与必要成功/失败证据保留，生命周期OPEN，manual freed0B。
