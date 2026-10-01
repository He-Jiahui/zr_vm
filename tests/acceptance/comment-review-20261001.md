---
related_code:
  - zr_vm_common/CommonMacros.cmake
  - zr_vm_common/CMakeLists.txt
  - zr_vm_common/ThirdPartyMacros.cmake
  - zr_vm_cli/CMakeLists.txt
  - zr_vm_lib_ffi/CMakeLists.txt
  - zr_vm_lib_thread/CMakeLists.txt
  - zr_vm_rust_binding/CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli/project/project.c
implementation_files:
  - zr_vm_common/CommonMacros.cmake
plan_sources:
  - user: 全仓库首方代码调用链审查与意图注释任务
  - docs/code-review/comment-standard.md
tests:
  - tests/fixtures/projects/hello_world/hello_world.zrp
doc_type: testing-guide
---

# 2026-10-01 注释审查批次验收

## CommonMacros 的15个审查单元

独立 reviewer 阅读全部17个 include 方及六函数的调用上下文。当前首方构建范围是88文件；旧87文件快照之后，另一会话增加了12行的静态库异常/GC测试注册，且更新了 `tests/CMakeLists.txt`。两处变化已复采；CommonMacros 的 include、六函数调用位置及语句仍与草稿一致。调用数依次为 module15、executable1、module link48、internal link3、executable link12、install15。

专用台账登记六函数、一次 include 和八个独立契约块，共15单元：7 `commented`、5 `no-comment`、2 `BUG`、1 `TODO`。源码保留161行，六处注释位置改变，非注释 CMake 命令逐行相同。审查清楚区分逻辑目标名与目录名、PRIVATE 依赖和公开头使用要求、配置结束前解析的前向依赖，以及 CLI 自身链接与生成外部产物的宿主库目录。

八条旧 CommonMacros 记录从 common 总表迁至专用台账；其他旧行字节相同。FFI 旧表仅物理第4行的 evidence 字段改变，以16个精确位置说明 libffi attachment 的目标创建前提和两种变体调用；该修订不代表整张旧 FFI 表已通过当前规范。inventory 只更新 CommonMacros 一行的实际 SHA 和审查说明。

实际应用门禁确认15单元、340个非空 caller/evidence 锚点、161/161行、相同非注释命令、旧表精确移除8行、FFI精确单字段和inventory精确单行。schema 与 `git diff --check` 均退出0。原反向预检记录263个有效 CommonMacros 位置、19个表；源码行数和配置语句未移动。旧 FFI 的空行起始范围单独修正，不能用有效位置计数掩盖它。

## 已确认问题的边界

- `BUG:` MSVC19.44.35228.0 + Ninja 同开静态/共享库时，静态归档与共享导入库都输出 `lib/zr_vm_core.lib`，配置实际退出1。失败缓存和 receipt 保留；此批只在共同 basename 处标记，不改命名和库选择。
- `BUG:` 对已创建 executable 组合首方 plain 与已注册第三方 PRIVATE 链接入口，会触发 CMake 签名混用错误；当前 CLI 的相关调用全是 plain，不把该 API 组合缺陷误称默认 CLI 配置失败。
- `BUG:` thread 的逻辑目标名为 `zr_vm_thread_*`，CLI/Rust 却查询 `zr_vm_lib_thread_*`，因此跳过能力宏与模块注册。已登记完整调用链；`TODO:` 后续对应调用方模块在调用位置补充标签，行为修复另行处理。
- common 新源的增量发现、裸模块名宏的外部消费和 Windows 共享依赖导出声明，保留各自具体核查入口。

## 修改前后实际验证

复用本任务专属缓存 `D:/tmp/zr_vm/comment-review-20260930/msvc-shared`，采用项目默认 shared-only 配置，使用 VSdev 包装器分别运行显式配置、构建现有 `zr_vm_cli_executable` 和现有 hello_world 项目。未增加模拟实现的测试。

| 阶段 | 时间（UTC） | 配置 | CLI构建 | hello_world | 原生句柄终态 |
| --- | --- | --- | --- | --- | --- |
| 修改前 | 02:36:15—02:37:33 | 0 | 0 | 0，输出 hello world | session33967，退出0 |
| 修改后 | 03:10:48—03:11:38 | 0 | 0 | 0，输出 hello world | session51723，退出0 |

前后 CommonMacros SHA 分别为 `cdd3c30deb745111c26311a9283696c389c86da2adc1a78017551a9b0204fe4e` 和 `8ba6dcf309f57d4180868967faaa72c249fe20d903a7ed0a1507ed765bc8ac6b`，各自运行期间保持不变，各次构建配置文件哈希也未漂移。两次之间其他会话的源码和测试配置有更新；这两次构建不是整个工作区的受控旧版本对照。注释等价性另由精确源码门禁证明。

实际命令和日志在该缓存的 `common-macros-review-20261001/before.json`、`after.json` 及对应 configure/build/smoke 日志。修改后的18个 Ninja 动作包含其他会话已更新的 core 源编译与共享库重链接；没有宣称只重新配置了注释或完成全平台矩阵。

验证范围是 MSVC shared-only CLI 与一个现有项目。上批 GCC/Clang 四目标仍记录101例中88通过、13个相同 LSP 失败；此批没有重复或覆盖那个结果。没有全仓绿色或跨仓采集成功结论。

## Git 与跨仓协调

根代理只对本批精确文件使用正常串行提交，不清理或提交外部 dirty/untracked 文件，不删除共享锁，不使用私有 index。提交成员与实际 SHA 另由终态 receipt 核实。

唯一已接受的 readiness 队列为 `01a0e3a1-2bbe-7f23-9026-9e5aa6296c8f`。本批未收到 capture START/terminal result/release，也未执行 inventory/archive、重复 validation 或网络调整。源码哈希、构建终态和模块提交不构成采集封印。
