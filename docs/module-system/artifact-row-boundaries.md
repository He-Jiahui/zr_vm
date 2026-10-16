---
related_code:
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/artifact/artifact_rows.c
  - zr_vm_core/src/zr_vm_core/artifact/artifact_identity.c
plan_sources:
  - user: 2026-10-16 active BUG repair, artifact fixed-row boundaries
tests:
  - tests/core/test_artifact_row_boundaries.c
  - tests/cmake/artifact-row-boundary-tests.cmake
---

# Artifact 固定行读取边界

本模块补充 [artifact schema](artifact-schema.md) 的公开单行读取契约。调用方可以直接构造
`SZrArtifactSectionView`，因此读取器不能仅依赖完整文档 reader 已验证目录。

## 输入与输出

`artifact_rows.c` 的十一种读取器在形成行指针前，检查节视图、非空 data、目标节 kind、
固定 elementSize、rowIndex 和整个声明表的字节范围。TypeIdentity 接受 TYPE_REF_TABLE
与 TYPE_SPEC_TABLE；其他读取器各自只接受对应种类，即使其他行恰好具有相同宽度。

完整表检查采用 UInt64 的 `elementCount * expectedSize <= byteLength`，避免宿主 32 位乘法
回绕；通过此检查后，目标行偏移也在 UInt32 字节范围内。允许视图含尾部额外字节，
但不允许只包含目标行而缺少后续声明行。data 实际指向足够可读存储仍是调用方责任，
接口无法验证任意原生指针的真实分配大小。

所有参数和范围错误返回 INVALID_ARGUMENT，失败不修改 outRow。每个公开读取入口先清空
整个可选 diagnostic，再进行包括空 outRow 在内的检查；成功时诊断全零，失败时只含本次错误。
读取器投影小端字段，不验证 token 图、哈希正确性或跨表引用，这些仍由完整文档/元数据图处理。

## 状态名称

StatusName 包含五个 scheduler/transport 失配名称。枚举尾部 COUNT 是无效状态哨兵，
名称表长度通过静态断言与 COUNT 一致；后续在哨兵前增加状态但遗漏名称会导致编译失败。
负值、COUNT 及其他未识别数值返回 unknown；现有有效状态编号保持不变。

## 验收证据与边界

根代理以 support-batch-2-red 已实际编译的 Core 静态归档链接新增测试，没有复制生产源码。
`reports/review-bugfix-20261016/artifact-row-red-receipt.json` 记录编译退出 0、输入前后
哈希一致及 changed_inputs=[]。valid_rows、invalid_row 两项控制通过；wrong_kind、null_data、
truncated、inconsistent_count、huge_count、diagnostic_reuse、status_names 七项行为 RED。
其中空 data 真实访问违规，五个合法新增状态原先均返回 unknown。

独立测试进程覆盖十一种 reader 和 TypeIdentity 的 REF/SPEC 两条路径。成功控制核对第二行
首字段和高位 UInt64 字段；失败核对输出字节保留。截断与超大计数测试保留足够真实存储，
后者只请求 row 0，不靠测试自身越界制造失败。Windows 禁用故障对话框，每项超时 20 秒。

修复后根代理统一重新构建实际 Core，integrated-support-batch-3-red 中九项全部通过，输入哈希稳定。
ExecIR、CallBinding 的独立契约与完整 token 图校验不属于这组测试。
