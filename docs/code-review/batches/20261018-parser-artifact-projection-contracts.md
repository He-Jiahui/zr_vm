# Parser artifact 投影合同

相关代码：

- `zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c`
- `zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c`
- `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c`
- `zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.h`

53单元、32真body、47Dox；15旧键迁移原位替换，38个遗漏单元补齐。42commented/4BUG/3TODO/4no-comment。

调用绑定两遍校验/发布，metadata状态与摘要，类型签名和公开身份；失败后的buffer前缀、canonical context增加及输出字段按原合同限定，不能称事务回滚。

实际源码应用见 parser-artifact-family-source-comment-equivalence-r1.json，SHA256 7de541c3ee1dc7cf5159167e80d346cbfbc0551f859871f2ec7d27d2506c0d8b；原程序字节、原行列及同配置的核对见 同一等价收据：原始程序字节和原位置完整保留，EOF仅追加完整注释与空白；前后配置身份相同；未执行编译器词法或AST比较。

关联artifact_schema/canonical_consumers/call_binding/generational_pool四条direct；25+21+8+3是57个静态Unity注册，真实逐ID/状态来自运行日志。 修改前 driver 30196：artifact_schema=0, canonical_consumers=0, call_binding=0, generational_pool=0；修改后 driver 15916：artifact_schema=0, canonical_consumers=0, call_binding=0, generational_pool=0；共享输入端点/argv/原始日志/目录快照见 parser-artifact-family-native-final-acceptance-r1.json，SHA256 1726d54b8d2a0b997de308f746941764ef3909cff177b97a38655b2183c1c8c4；BEFORE/AFTER原始输出、选中依赖和每步独占目录快照均保留于E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/。只将实际结果写入本段，不据静态注册数推断PASS，不把direct当CTest验收。

保留BUG-PROPERTY-PUBLISH-COUNT及BUG-PROPERTY-DIAGNOSTIC两BUG意图、四TODO问题；4BUG/3TODO行及全部issue尾句不因native结果清除。

最终台账见 [逐单元台账](../coverage/zr_vm_parser_artifact_projections.tsv)；inventory仅本批既有键sha/status/note三列按实际交付投影，其余字节保持。共享运行与借用定义不重复计信用。

独立源码/台账复核和暂存收据须按各自门禁登记并通过后，方可串行提交；不授权未来parent或全仓验收。

## 物理源身份与验证限制

- zr_vm_parser/src/zr_vm_parser/artifact/artifact_call_binding_projection.c：f8e291d128e8aceb2f465d8d83de8aacd6113d0596427407c34b951a76a7bf35
- zr_vm_parser/src/zr_vm_parser/artifact/artifact_metadata_projection.c：5e781faae82f2fd8dd509280985f891575fc49d1ae61b35d822754ee8014a2db
- zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.c：2598d202c1e1ca336b185fb70de29c2f67f4a3bea22eafac9a47ca8ceb53fbd9
- zr_vm_parser/src/zr_vm_parser/artifact/artifact_projection.h：055ed1ac0399f25f7168a8a9a3fc212f85cadd9cd92d20b3d55e9161663a7824

共享验证只含现有6目标、8direct和4只读Ninja查询，未运行CTest/GDB/WSL/POSIX或全仓测试；build原始警告保留。普通失败、OOM和非局部异常仍以各行合同/既有证据限制为准。生命周期OPEN，人工释放0B；复用构建及必要成功、失败原始证据保留，无源码副本。
