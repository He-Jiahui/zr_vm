---
related_code:
  - zr_vm_cli/src/zr_vm_cli/command/command_numbers_internal.h
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/command/command.h
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/command/command_numbers_internal.h
  - tests/cli/test_cli_command_boundaries.c
  - tests/cmake/cli-command-boundary-tests.cmake
plan_sources:
  - .codex/plans/20261016-review-bugfix.md
tests:
  - tests/cli/test_cli_command_boundaries.c
  - tests/cmake/cli-command-boundary-tests.cmake
doc_type: regression-contract
---

# CLI 数值参数及执行模式边界

本组通过实际 ZrCli_Command_Parse 公共入口调用当前 command.c；不解析输出文本来代替
结构化结果，也不启动项目、任务或 worker。八个进程覆盖超出 UINT64 的 timeout、
极长十进制串、超出 uint32 的 jobs，以及 compile --run 的显式 interp 选项。
普通单位换算、精确上界、乘法溢出拒绝、默认 binary 和旧 errno 为正常控制。

Windows 的 unsigned long 为 32 位，strtoul 饱和后仍可能等于 UINT32_MAX，故 jobs
还需要检查 ERANGE；较宽 unsigned long 平台可能已拒绝同一输入。合法上界只获得
解析信用，不代表实际建立这么多 worker 或等待这么久。显式 interp 在选项前后
两种顺序都应保留；只有未显式指定模式的 compile --run 默认选择 binary。

根会话已运行真实解析入口：超范围 timeout 两例、Windows jobs 溢出和显式 interp
四例失败，其余四个正常控制通过。修复已独立审查，尚待最终统一 GREEN。

数值解析整体提取到私有 `command_numbers_internal.h`；转换前清 errno，拒绝
ERANGE 饱和值，再保留原有尾字符、零值和范围/单位乘法检查。解析成功才写
输出参数。`command.c` 记录本轮是否收到合法 execution-mode，compile+run
仅在未显式选择模式时采用 binary 默认值。签名与普通输入行为保持一致。
该提取为超过千行的命令解析器移出一个完整职责，不新增全局接口。
