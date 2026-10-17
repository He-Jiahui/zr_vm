---
related_code:
  - tests/fixtures/projects/syntax_reference_v1/artifacts/fixturedep.zrm
  - tests/parser/test_syntax_reference_v1.c
implementation_files:
  - tests/fixtures/projects/syntax_reference_v1/artifacts/fixturedep.zrm
plan_sources:
  - user: 首方人工 fixture 调用链审查与注释任务
tests:
  - tests/parser/test_syntax_reference_v1.c
doc_type: review-batch
---
# fixturedep JSON 占位元数据合同

范围为1个 metadata-interface，原169B/6LF不变，JSON不插入注释。手工补充域只登记此路径。

## Summary
fixturedep.zrm 是 syntax_reference_v1 人工维护的 JSON 占位对象，配合 modules.zr 的路径式 import 展示归档定位语法。它提供稳定路径与示例标记，不提供可装载的归档内容。

## Fields
schema 固定为 syntax-reference-v1-artifact-placeholder，区别于生产 ZRM 清单格式。
moduleIdentity 的 @fixturedep 与项目所列示例包名对应，但不证明 loader 已解析出该 identity。
status 的 current 是样例标记，不代表运行、归档装载或测试成功。
publicContractHash 的 fixturedep:v1:index-tool 是人工示例文本，不是本文件 SHA-256，现有 C 测试没有按合同摘要验证它。

## Constraints and lifecycle
文件由仓库人工维护，消费者借用其路径。现有 C 测试通过 fopen 只读打开并关闭以检查存在性；解析循环只处理 .zr，因而不解析此 JSON。modules.zr 的导入声明被语法解析，checksum 未使用 fixtureArtifact。项目 smoke 的 main 只调用 host，不证明归档导入运行。既有 projects runner 可把整个项目 fixture 树复制到其生成目录；复制不是重新生成或容器装载。此对象没有可调用体、函数指针或线程状态；读取 helper 的文件句柄和缓冲区按各自普通成功/失败路径管理，不声称并发隔离或 Unity 断言非局部退出时仍执行普通 cleanup。

## Failure boundary
生产 ZrLibrary_Zrm_Open 先要求 miniz ZIP reader 成功，之后读取归档内 manifest。当前169B不是ZIP，也没有归档内清单/模块条目，不能把扩展名或字段拼写当成可加载性。此结论是静态格式与调用链证据，未执行生产入口。当前 C 测试未断言这四字段的具体值或篡改后的诊断；本批不新增测试、BUG 或 TODO。

## References
tests/parser/test_syntax_reference_v1.c:119
tests/parser/test_syntax_reference_v1.c:427
tests/parser/test_syntax_reference_v1.c:536
tests/parser/test_syntax_reference_v1.c:579
tests/fixtures/projects/syntax_reference_v1/src/modules.zr:7
tests/fixtures/projects/syntax_reference_v1/src/modules.zr:9
tests/fixtures/projects/syntax_reference_v1/src/main.zr:1
tests/cmake/run_projects_suite.cmake:43
zr_vm_library/src/zr_vm_library/zrm.c:1175

## 台账与有限验证
手工清单六列和独立台账一行九列。旧 coverage/tests_fixtures_projects_late.tsv:94 的 fixtureArtifact 只更正 constraints 中二进制样例的物理类型；旧键保留，不迁移或重复计数。
focused CMake target zr_vm_syntax_reference_v1_test，CTest syntax_reference_v1，main静态注册13个Unity用例。相关断言只证明清单成员、路径存在与 .zr 语法；没有此JSON字段或归档装载断言。本记录不能以13注册代称PASS。实际运行、独立提案审查、源/台账应用和暂存收据须按各自门禁记录并通过后，方可串行交付。

## 本批实际文档验收

本批仅写六份审查文档，JSON原169B/6LF及产品、测试、扫描器代码保持不变。主清单与手工清单互斥；复核四字段、重复键、Git三态原文、唯一九列单元和当前非空证据锚点，并运行现有 code_review_inventory.py check-batch 检查手工台账。具体进程、退出码与前后有限摘要见本任务 E 根 reports 的 fixturedep-json-placeholder-validation-r1.json；实际暂存和提交另有收据。此纯文档批次无需重建未变化的 C 目标，不以13个注册用例代称本轮运行通过；全仓主域仍有待审文件。

F1为不可变历史提案（32fac627…）；应用仅将有调用语义的 C:580 锚纠正为 C:579、两段说明改为EOF追加保留原行号，并在独立复核及静态验收后将手工文件记为reviewed。旧coverage第94行只更正constraints，不迁移或重复计入fixtureArtifact。本批不新增问题标签。manual cleanup 0B，必要证据保留，lifecycle OPEN。
