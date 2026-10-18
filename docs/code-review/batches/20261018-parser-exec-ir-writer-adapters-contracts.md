# Parser ExecIR 读写与规范工件文件适配合同

相关代码：

- `zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h`
- `zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c`
- `zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c`

20单元、6真body、16Dox；私有H6、writer2、filewriter12，17commented/3no-comment。0旧、20个遗漏单元已补齐。

私有流接口/Core转接/规范文件写出；先编码、再排他临时文件、关闭后替换，失败删除仅尽力；宏注入状态与串行前提保持。

实际源码应用见 parser-artifact-family-source-comment-equivalence-r1.json，SHA256 7de541c3ee1dc7cf5159167e80d346cbfbc0551f859871f2ec7d27d2506c0d8b；原程序字节、原行列及同配置的核对见 同一等价收据：原始程序字节和原位置完整保留，EOF仅追加完整注释与空白；前后配置身份相同；未执行编译器词法或AST比较。

关联schema_relocation普通main及v6 --write/--read/--validate-zraf三条direct；两目标四direct，不以Unity footer判读普通main，不授freshCTest运行。 修改前 driver 30196：schema_relocation=0, exec_ir_write=0, exec_ir_read=0, exec_ir_validate=0；修改后 driver 15916：schema_relocation=0, exec_ir_write=0, exec_ir_read=0, exec_ir_validate=0；共享输入端点/argv/原始日志/目录快照见 parser-artifact-family-native-final-acceptance-r1.json，SHA256 1726d54b8d2a0b997de308f746941764ef3909cff177b97a38655b2183c1c8c4；BEFORE/AFTER原始输出、选中依赖和每步独占目录快照均保留于E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/。只将实际结果写入本段，不据静态注册数推断PASS，不把direct当CTest验收。

无新增BUG/TODO，不修API/行为。

最终台账见 [逐单元台账](../coverage/zr_vm_parser_exec_ir_writer_adapters.tsv)；inventory仅本批既有键sha/status/note三列按实际交付投影，其余字节保持。共享运行与借用定义不重复计信用。

独立源码/台账复核和暂存收据须按各自门禁登记并通过后，方可串行提交；不授权未来parent或全仓验收。

## 物理源身份与验证限制

- zr_vm_parser/src/zr_vm_parser/artifact/artifact_exec_ir.h：d990685b78f577bbfa055d38f8657b0b75e1daa23d7bcc300529f465c01c969b
- zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir.c：7691589cd562067a9cdcb0d12cccbdd40f731b99523b9fcc4261cf1d923d6027
- zr_vm_parser/src/zr_vm_parser/writer/writer_exec_ir_artifact.c：eec2b1da966df64178ffc287609e63ca91dc37d3f0582a9d3199632aa331db71

共享验证只含现有6目标、8direct和4只读Ninja查询，未运行CTest/GDB/WSL/POSIX或全仓测试；build原始警告保留。普通失败、OOM和非局部异常仍以各行合同/既有证据限制为准。生命周期OPEN，人工释放0B；复用构建及必要成功、失败原始证据保留，无源码副本。
