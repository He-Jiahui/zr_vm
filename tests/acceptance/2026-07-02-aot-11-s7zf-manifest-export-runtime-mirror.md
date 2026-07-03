# AOT 11-S7ZF / 12-S7 Manifest Export Runtime Mirror

时间：2026-07-02 03:57:34 +08:00

状态：完成支撑子切片。该记录只关闭 attached metadata runtime 对 generated-C manifest export table 的 pointer/count
mirror，不声明跨模块 provider/version 绑定、standalone provider import-path 消费、完整 metadata sweep/pruning 或 full
trim analyzer 完成。

## 完成项目

- `SZrMetadataRuntime` 尾部追加 `manifestExports` 与 `manifestExportCount`，避免移动已有 runtime 字段顺序。
- `ZrCore_Module_AttachMetadataRuntime()` 从 `SZrAotCodeRegistration.manifestExports/manifestExportCount` 镜像只读 table view。
- 新增 `tests/module/test_metadata_runtime_manifest_exports.c`，验证 type export `TYPE_DEF` token 和 method export
  `MEMBER_DEF` token 可经 attached metadata runtime 读取。
- `tests/CMakeLists.txt` 注册 `zr_vm_metadata_runtime_manifest_exports_test` 和 CTest `metadata_runtime_manifest_exports`。

## RED/GREEN

- RED：WSL GCC focused build 在新测试引用 `runtime->manifestExportCount` / `runtime->manifestExports` 时编译失败，因为
  `SZrMetadataRuntime` 尚无对应字段。
- 调试修正：初始实现把字段插入到已有计数字段中间，相邻 metadata runtime query 暴露旧字段布局错位风险；最终改为尾部追加字段。
- GREEN：三套工具链 direct test 和 focused CTest 均通过。

## 验证

- WSL GCC：`zr_vm_metadata_runtime_manifest_exports_test` 1/0、`zr_vm_metadata_runtime_query_test` 25/0、
  `zr_vm_metadata_runtime_binding_compatibility_test` 15/0；对应 CTest 3/3。
- WSL clang：同一组 direct tests 为 1/0、25/0、15/0；对应 CTest 3/3。
- Windows MSVC Debug：同一组 direct tests 为 1/0、25/0、15/0；对应 CTest 3/3。

备注：GCC/clang/MSVC 构建仍输出既有 `execution_dispatch.c` computed-goto/label、`reflection.c` unused 变量等警告；
本切片未引入新的失败。
