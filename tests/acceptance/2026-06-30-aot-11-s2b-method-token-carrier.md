# AOT 11-S2B / 10-S2 Method Token Code Registration Carrier

## 状态

- 完成时间：2026-06-30 01:05:20 +08:00
- 状态：11-S2 code-registration method token carrier 子切片完成；完整 10-S2/10-S3/11-S2 仍未关闭。

## 完成项目

- 公共 AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 11u`。
- `SZrAotCodeRegistration` 与 `ZrAotCompiledModule` 新增 `methodTokens` / `methodTokenCount`。
- AOT C 生成物新增 `zr_aot_method_tokens[]`，按 `functionIndex` 对齐 `methodInfos[]`。
- method token 表只为 root module 的 typed exported function 填充可靠 `MEMBER_DEF` token；无法可靠绑定的槽位写 `0u`。
- `SZrMetadataRuntime` mirror `methodTokenCount`，模块 attach 时从 code registration 复制。
- AOT runtime descriptor validation 同步检查 descriptor 与 code registration 的 method token 指针/计数一致性，并拒绝空/非空形态或计数与 `methodInfoCount` 不一致。

## RED/GREEN

- RED：`tests/parser/test_aot_c_frame_setup_contracts.c` 先要求 ABI、emitter 和 runtime validation 暴露 method token carrier，WSL gcc 运行失败，缺少 `descriptor->codeRegistration->methodTokens != descriptor->methodTokens` 等源契约文本。
- GREEN：补齐 ABI、生成器、runtime validation、metadata runtime mirror 和 shared-library runtime 断言后，focused 组通过。
- 备注：shared-library smoke 的实际导出 token 是当前 token 模型的 `MEMBER_DEF` 表编码 `0x03000001u`，不是 MethodDef 专用 `0x06000001u`。

## 验证

- WSL gcc：metadata runtime query 24/0，AOT C source contracts 22/0，frame setup contracts 1/0，shared-library smoke 13/0，descriptor diagnostics 2/0。
- WSL clang：同五项分别 24/0、22/0、1/0、13/0、2/0。
- Windows MSVC Debug：metadata runtime query 24/0，AOT C source contracts 22/0，frame setup contracts 1/0；shared-library smoke 13 项 ignored，descriptor diagnostics 2 项 ignored。
- 备注：clang/gcc 与 MSVC 仍有既有 warning，本切片未扩大处理范围。

## 未完成

- 不声明 token 到 `MethodInfo`、function pointer 或 invoker 的 public resolver 已完成。
- 不声明 `Method.Invoke` 参数/返回 marshaling、public method reflection object、cross-module token rewrite、trim annotation diagnostics 或完整 10-S2/10-S3/11-S2 关闭。
