# 构造器prototype增长手写回归合同

本批逐项审查3个单元；0个旧键不重复计数，新增3个键。定义、全部可检索调用、间接入口、现有测试、约束与每项结论记录于[逐单元台账](../coverage/constructor_prototype_growth.tsv)；选中行无未审查项。

审查完整手写场景、Job实参中的完整零参数lambda及CMake runner，共3个新键。Job构造保存callable，schedule消费Job后才由任务运行期以零参数派发；sys.vm读取保留原生元数据解析交互，局部v不交付。编译器在参数编译前按值保存构造决策，避免借用可能随prototype表增长失效的指针。runner先读全文，再用独立-e实参启动CLI；普通退出0且STRIP后的stdout精确为7才通过。内部30秒与外层40秒区分，源码、CLI结果及VM值的生命周期分别记录。

仅这个现有场景的外部结果；没有直接测量prototype容量/地址变化，也未证明所有构造器、scheduler、OOM、线程交错或CLI原生模块无宿主副作用。

## 验证与边界

复用实际checkout与E:/cargo-targets/zr_vm/build/first-party-review-01a13463/clang-cl-products-debug；ClangCL/Debug、Ninja，配置、工具及选中依赖按真实前后记录核对。共同验证构建六目标，直接运行七步骤；故意失败探针的退出1为必要结果。以下只授予本模块对应步骤的有限证据。

| 现有步骤 | BEFORE实际退出 | AFTER实际退出 | 预期 |
| --- | ---: | ---: | ---: |
| constructor | 0 | 0 | 0 |

第一轮真实build-only尝试因外部compile_expression_types.c在构建期间变化而actual1，七测试均未启动，原失败保留，不升级为通过。后续baseline将该源加入strict106并重新运行；七个本任务文件仅有注释变化，原程序文本与位置保持，公开API签名及行为未修改。

实际pair验收：released-contracts-native-final-acceptance-r1.json，SHA256 5d5cff63a88d2315c9510c098632c845063ae9f56e713cc45c31c4dde21827ed；注释等价证明SHA256 003df9bdde1a52b86298fddd4823411089fe308e9921a9214144b36ad4c23765；独审 constructor-prototype-growth-fixture-current-contract-independent-review-r1.json，SHA256 7abe75de720e95ae099328a3c4a6c08b71af98da32a1bd7a6e2ad979e86e37bc。完整收据及原始输出在E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/。全仓清单仍存在外域pending，不据本批声称全仓完成。

## 源身份与提交

- tests/fixtures/parser/constructor_prototype_growth.zr：3edc19258450cd20119b24f46f362371df95544e9360328a37df614f6d413c40
- tests/cmake/run_constructor_prototype_growth.cmake：e38a496513f33afd427b8c184e00a602ec5f6d24682ebad4c14e7a7738dae5da

提交前必须核对仅本批差异并通过git diff --cached --check、既有check-batch及独立暂存审查；本批不增加重复实现的测试。其他会话的暂存、源码与混合台账外行保留。构建与必要成功/失败证据保留，生命周期OPEN，manual freed0B。
