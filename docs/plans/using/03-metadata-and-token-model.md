---
doc_type: plan-detail
related_code:
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_core/include/zr_vm_core/hash.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature.h
  - zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.h
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/include/zr_vm_library/zrm.h
  - zr_vm_library/src/zr_vm_library/project/project.c
  - zr_vm_library/src/zr_vm_library/project/project_import_resolver.c
  - zr_vm_library/src/zr_vm_library/zrm.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_lib_system/include/zr_vm_lib_system/assembly.h
  - zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_record.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_record.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_hash.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_hash.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_ref.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_ref.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_def.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_def.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_def_layout.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_def_layout.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_ref.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_ref.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_union.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
tests:
  - tests/module/test_metadata_token_model.c
  - tests/module/test_metadata_type_ref_binding.c
  - tests/module/test_metadata_runtime_query.c
  - tests/module/test_metadata_module_hash_golden.c
  - tests/library/test_zrm_container.c
  - tests/library/test_project_import_resolver.c
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_project_incremental.c
  - tests/system/test_system_assembly_module.c
---

# 03 · 模仿 C# 程序集的 metadata / token 模型

目标：给 `.zro` 模块（以及外部 DLL 插件）一套 **C# (ECMA-335) 风格的 metadata token 体系**，让 DLL 之间**通过签名访问**彼此的类型与成员，而不依赖编译顺序、物理 slot 布局或裸名字符串匹配。这是 [02 篇插件守卫](02-using-scopes-and-plugin-guards.md) 的"签名比对"和 [01 篇所有权泛型](01-ownership-as-generics.md) 的"种类参与类型身份"的共同地基。

> 进度更新（2026-06-19 23:52:59 +08:00）
>
> 完成状态：完成。P0 `.zro` / `.zrm` 程序集边界、`.zrm` 容器基础和 runtime fixture 测试已落地。`.zro` 现在明确只保留单脚本/单模块编译中间文件职责，继续承载 typed metadata、token/signature、module ABI hash 和 import verification sidecar；程序集级语义链新增独立 `.zrm` 容器，作为类似 CLR DLL/JAR 的单文件 assembly/package，可用于第三方模块库分发。
>
> 实现：新增 `zr_vm_library/zrm` ZIP 容器 API，写入 `META-INF/zrm.json` manifest、`modules/<module>.zro` 模块 payload 和 `resources/<logicalName>` 资源 entry，并支持资源压缩与安全逻辑名校验；`.zrp` manifest 新增 `assembly.output`、`resources` 和 `.zrm references` 解析，project resolver/source loader 可从 referenced `.zrm` 中解析 `$alias@version/module` 并读取模块 `.zro`；CLI 新增 `--emit-zrm`，可把当前 project 可达模块与声明资源打包为 assembly；runtime 新增 `zr.system.assembly`，提供 `resourceExists`、`readResourceText`、`readResourceBytes` 读取当前 project assembly 资源；`zrp.schema.json` 与 `docs/module-system/zrm-assembly-container.md` 已同步。
>
> 说明备注：本切片不把程序集 manifest、资源压缩或第三方库分发语义继续塞入 `.zro` patch；`.zro` 仍是 `.zrm` 内部 module payload 与单文件编译缓存。`.zrm` open 现在拒绝损坏 ZIP、缺 manifest、manifest entry path traversal 和非 canonical module/resource entry。当前 `.zrm` 运行期资源 API 绑定当前 project assembly output，暂不声明直接执行 loose `.zrm` 或自动合成 loose resources。
>
> 验证：WSL GCC 与 WSL clang focused 均重新构建并通过 `zr_vm_zrm_container_test` 4/0、`zr_vm_project_import_resolver_test` 9/0、`zr_vm_system_assembly_test` 2/0、`zr_vm_cli_zrm_fixture_test` 1/0；MSVC focused 同四个目标构建通过并运行 4/0、9/0、2/0、1/0。`zr_vm_cli_zrm_fixture_test` 实际编译 provider `.zrm`、consumer `.zrm`，运行 consumer 从 referenced provider `.zrm` 加载模块并读取 provider 导出的 `answer` 返回 `42`，再运行 `resource_probe` 通过 `zr.system.assembly.readResourceText("config/runtime.txt")` 读取当前 consumer `.zrm` 资源。WSL GCC/clang 的 `zr_vm_cli_args_test` 均输出 `cli args tests passed`。验证期间，一个更强的临时 fixture 版本在 MSVC 上调用 referenced `.zrm` provider 导出函数时触发 `ownership_try_free_control` 访问冲突；最终 fixture 收敛为本阶段目标所需的 `.zrm` 模块加载 + 导出值读取，跨程序集导出函数调用的 Windows 所有权路径另行跟进。当前完整 `zr_vm_cli_project_incremental_test` 仍有 3 个既有 binary-run signature mismatch 失败，未作为本切片通过项。

> 进度更新（2026-06-19 04:19:38 +08:00）
>
> 状态：P0 metadata/token 的 `.zrp` assembly identity 与 alias-based references 切片已完成。`.zrp` 新增 `manifestVersion`、`assembly` 与 `references`；旧 `name/version/dependencies` 仍兼容。`references.<alias>` 会把源码导入规范化为 `$alias@version/path`，同时把真实程序集名写入 import effect 的 `assemblyName`，让 `ASSEMBLY_REF` signature 使用真实 assembly identity（例如 `zr.math`）而不是源码 alias key。
>
> 说明：`.zro` schema 升级到 patch 33，module effect 写入/读取/runtime copy 新增 `assemblyName` 字段；旧 patch 读取时该字段置 null，并回退到 canonical module key。project source loader 已覆盖 binary-only referenced assembly：目标源码移除后，引用方仍可从 referenced package 的 `.zro` summary 做语义分析并生成带真实 AssemblyRef 身份和版本范围的 token。
>
> 验证：TDD RED/GREEN 覆盖 `.zrp` references 规范化、AssemblyRef 真实身份、module effect assembly identity binary roundtrip、以及 referenced assembly 仅 `.zro` 存在时的语义导入。最终验证（2026-06-19 04:38:35 +08:00）：WSL clang direct `zr_vm_project_import_resolver_test` 7/0、`zr_vm_project_import_canonicalization_test` 34/0、`zr_vm_metadata_token_model_test` 21/0、`zr_vm_gc_test` 66/0；`zrp.schema.json` 可被 `ConvertFrom-Json` 解析；touched-file `git diff --check` 退出 0，仅有 LF/CRLF 策略提示。

> 进度更新（2026-06-19 00:56:14 +08:00）
>
> 状态：P0 metadata/token 结构实现复核完成。当前代码已覆盖 token 编码、函数级 records、entry-function 级 `moduleMetadataTokenRecords`、`moduleMetadataBindings` binding sidecar、metadata string heap、module ABI hash、provider `MODULE` record、TypeSpec/TypeDef definition/layout binding、stable provider TypeRef producer/binder/status/diagnostic，以及 runtime record/query surface；未发现新的 metadata/token 生产代码缺口。
>
> 说明：本次只同步计划/验收/模块文档状态，不改生产代码。早期记录里把显式 TypeRef surface 作为 future work 的备注，已被后续 `provider.Option<int>` module-qualified typed local annotation 和 `Option<int>` destructured/unqualified alias annotation 两个切片覆盖；保留的未完成项属于更完整 cross-function/global async 插件流、broader load+verify polish、union/using 后续矩阵或 AOT/typed IR 消费，不属于当前 P0 token 结构缺口。
>
> 验证：fresh focused metadata matrix 通过：WSL GCC `metadata_token_model|metadata_type_ref_binding|metadata_runtime_query|metadata_module_hash_golden` CTest 4/4；WSL clang 同组 4/4；MSVC Debug 同组 4/4。

> 进度更新（2026-06-19 00:41:15 +08:00）
>
> 状态：P0/P2 source-loader diagnostic path portability 已完成。`ZrLibrary_Project_SourceLoadImplementation()` 继续用平台本地路径访问 `.zr` / `.zro`，但写入 `moduleLoadDiagnostic` 时会把 `source=` / `binary=` 尝试路径规范化为 `/` 分隔显示文本，确保 required import 的 `import_load_unavailable` 诊断在 Windows/MSVC 与 WSL 上都包含稳定的 canonical module-relative substring。
>
> 说明：不改变 resolver、文件访问路径、metadata token schema 或 `.zro` patch；这是 2026-06-18 23:45 记录的 MSVC direct project-import source-loader diagnostic 文本差异修复。
>
> 验证：RED 为 MSVC Debug direct `zr_vm_project_import_canonicalization_test` 31/1，失败于 `test_required_import_runtime_reports_source_loader_attempts` 的 `feature/app/helper/math.zr` substring 断言；GREEN 后 MSVC Debug direct project-import 31/0，WSL GCC direct project-import 31/0，WSL clang direct project-import 31/0。WSL GCC/clang 与 MSVC Debug metadata CTest `metadata_token_model|metadata_type_ref_binding|metadata_runtime_query|metadata_module_hash_golden` 均保持 4/4。

> 进度更新（2026-06-19 00:12:05 +08:00）
>
> 状态：P0 runtime metadata record query surface 已完成。`function.h` / `function_metadata_query.c` 新增 `ZrCore_Function_FindMetadataTokenRecord()`、`ZrCore_Function_FindMetadataSignatureRecord()`、`ZrCore_Function_FindModuleMetadataTokenRecord()` 和 `ZrCore_Function_FindModuleMetadataSignatureRecord()`，分别只读查询函数级 metadata token records 与 entry-function 级 `moduleMetadataTokenRecords`。
>
> 说明：signature 查询要求实体 record 的 `relatedToken` 指向 `SIGNATURE`，且 signature record 的 `relatedToken` / `ownerToken` 都回指实体 token，避免松散 signature record 被误配。不新增 `.zro` patch，不改变 binding sidecar schema；这是 core 层 token/signature record 查询 surface，不等同于 05 篇非目标里的完整动态类型反射 API。
>
> 验证：新增 `tests/module/test_metadata_runtime_query.c` 和 CTest suite `metadata_runtime_query`。TDD RED 先在 WSL GCC 链接失败于四个 API 缺失；GREEN 后 WSL GCC/clang direct 新测试 3/0，WSL GCC/clang 与 MSVC Debug metadata CTest `metadata_token_model|metadata_type_ref_binding|metadata_runtime_query|metadata_module_hash_golden` 均为 4/4。

> 进度更新（2026-06-18 23:45:06 +08:00）
>
> 状态：P0 module ABI hash golden 与跨工具链 layout fingerprint 稳定化已完成。新增 `tests/module/test_metadata_module_hash_golden.c` 和 CTest suite `metadata_module_hash_golden`，固定两个模块级 ABI fingerprint：`sum(i64,i64)->i64` 为 `0xE701BC33ECB6BF89`，本地泛型 union `Option<T>` + `choose(): Option<int>` 为 `0x485AE44EE06010E4`。
>
> 实现：`compiler_metadata_type_def_layout.c` 的 metadata layout identity 不再把宿主 `ZR_ALIGN_SIZE` 作为未知/泛型 payload 或标量对齐的 hash 输入；metadata fingerprint 使用固定 64-bit reference size 和最多 8 字节的 canonical scalar alignment。运行时 union layout 生成路径未改，`layoutHash` 仍保持 record-level ABI check，不写入 `TYPE_DEF` logical signature blob，也不新增 `.zro` patch。
>
> 验证：WSL GCC 与 WSL clang 均通过 `metadata_token_model` 21/0、`metadata_type_ref_binding` 8/0、`metadata_module_hash_golden` 2/0；`zr_vm_project_import_canonicalization_test` 在 WSL GCC/clang 均为 31/0。MSVC Debug 通过 `metadata_token_model`、`metadata_type_ref_binding`、`metadata_module_hash_golden` 三个 CTest 3/3。备注：当时 MSVC direct `zr_vm_project_import_canonicalization_test` 的 source-loader diagnostic 文本断言为 31/1，已在 2026-06-19 00:41:15 +08:00 的 diagnostic path portability 切片修复为 31/0。

> 进度更新（2026-06-17 14:09 +08:00）
>
> 状态：P0 第一阶段已完成。核心 `TZrMetadataToken` 结构、`MEMBER_DEF` / `SIGNATURE` token 分配、签名 blob heap、`.zro` patch 20 写入/读取、runtime loader 复制链路已经落地。
>
> 完成：新增 [metadata_token.h](../../../zr_vm_core/include/zr_vm_core/metadata_token.h)；在 `SZrFunctionTypedExportSymbol` / `SZrIoFunctionTypedExportSymbol` 挂载 metadata token 与签名 blob 范围；新增 [compiler_metadata_token.c](../../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c) 从 typed export 生成签名 blob；[writer.c](../../../zr_vm_parser/src/zr_vm_parser/writer.c)、[io.c](../../../zr_vm_core/src/zr_vm_core/io.c)、[io_runtime.c](../../../zr_vm_core/src/zr_vm_core/io_runtime.c) 完成 roundtrip。
>
> 验证：`ctest -R metadata_token_model --output-on-failure` 通过，覆盖函数 typed export 的 token table、签名堆、`.zro` 读回和 runtime copy。
>
> 备注：本阶段先实现本模块定义侧 token 和签名结构；跨模块 `AssemblyRef/TypeRef/MemberRef` ref→def 签名匹配、签名哈希稳定性与 native/source/binary import 灰度路径仍是 P0 后续工作。

> 进度更新（2026-06-17 16:22:29 +08:00）
>
> 状态：union 闭型签名基础切片已完成。`compiler_metadata_token.c` 在当前脚本 AST 中识别已知 `union` 类型，`Option<int>` 这类 typed export 返回值/参数类型会编码为 `UNION <base-name> <argCount> <TypeSig>*` 形态；当前仍沿用函数级 signature heap 的紧凑字节编码。后续本地 union `TYPE_DEF` baseline 已在 2026-06-18 14:09:55 +08:00 完成；跨模块 ref→def 绑定继续按后续 P0 记录推进。
>
> 验证：`cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_metadata_token_model_test -j1 && ./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_metadata_token_model_test`（2 tests OK）；`ctest -R metadata_token_model --output-on-failure` 通过。

> 进度更新（2026-06-17 16:49:51 +08:00）
>
> 状态：union prototypeData 元数据基础切片已完成。编译器现在会把脚本内 `union` 声明序列化为 `ZR_OBJECT_PROTOTYPE_TYPE_UNION` prototype，并把 variant 写为 `ZR_AST_UNION_VARIANT` member；该信息服务运行时 prototype/materialization 路径，与本页 signature blob 的 `UNION` 节点互补。
>
> 验证：`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_union_test`（13 tests OK）；`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_parser_test`（72 tests OK）；metadata token ctest 仍通过。

> 进度更新（2026-06-17 17:10:59 +08:00）
>
> 状态：union variant payload 字段 metadata 已补齐基础序列化。variant member 的成员级 metadata 常量现在记录 `kind=unionVariant`、owner/variant 名称、tag、variant kind、payload 字段数和 `payloadFields[]`，每个字段包含声明名、类型名、运行时 storage name、位置和 passing mode；同时修复 union variant `returnTypeNameStringIndex` 被非方法分支清零的问题。
>
> 验证：`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_union_test`（14 tests OK）；`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_parser_test`（73 tests OK）；`ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R 'metadata_token_model|union' --output-on-failure` 中 metadata token 1/1 OK。

> 进度更新（2026-06-17 17:23:22 +08:00）
>
> 状态：union tag/payload byte layout metadata 已补齐基础序列化。union prototype 现在写入整体 `layoutByteSize/layoutByteAlign`，variant metadata 记录 tag size、payload offset、variant payload size/align，并为每个 payload field 写入绝对 byte offset、byte size 和 byte align。
>
> 验证：`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_union_test`（15 tests OK）；`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_parser_test`（73 tests OK）；metadata token CTest 1/1 OK。

> 进度更新（2026-06-17 18:20:57 +08:00）
>
> 状态：typed union local constructor materialization 已完成基础切片。union prototype/variant metadata 现在不仅落盘，还被 runtime inline-frame 路径消费：typed union 局部 slot 会使用 union prototype 的 inline layout，构造器 carrier 可在自 `SET_STACK` 中 materialize 为 inline tag/payload bytes。
>
> 验证：`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_union_test`（17 tests OK）；`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_parser_test`（73 tests OK）；`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_value_type_runtime_test`（13 tests OK）；`git diff --check` 退出码 0（仅既有 CRLF/LF 提示）。

> 进度更新（2026-06-17 18:37:08 +08:00）
>
> 状态：P2 插件 guard 所需的函数级 `MemberRef` token 前置记录已完成基础切片。`compiler_metadata_token.c` 现在会把 module init / exported callable summary 中可签名的 import member effects 过滤为 `IMPORT_REF` / `IMPORT_READ` / `IMPORT_CALL`，生成 `MEMBER_REF` token 与配对 `SIGNATURE` token；signature blob 使用新增 `MEMBER_REF <moduleName> <symbolName> <effectKind>` 节点。`module_init_analysis.c` 在 finalize 当前 source module 后刷新 token block，保证 guarded body 的 `p.*` effects 已附着后再写 token。
>
> 验证：`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_project_import_canonicalization_test`（5 tests OK，含 `MEMBER_REF` 数量与 member-ref signature blob 断言）；`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_metadata_token_model_test`（2 tests OK）；`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_parser_test`（73 tests OK）；`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_union_test`（17 tests OK）；`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_language_server_statement_parser_diagnostics_test` 通过。

> 设计答复（2026-06-17 18:37:42 +08:00）
>
> 状态：metadata plan 的开放问题已收敛为 P0 后续实现建议。建议采用 `XXH3_64bits` 作为非安全边界的 ABI fingerprint，哈希输入限定为规范化签名流；semver 范围负责人类可读兼容策略，签名哈希负责机器判定；物理 byte layout 不进入类型身份，只在需要 inline ABI 共享时作为单独 layout check；当前函数签名 v0 编码不直接作为长期 ABI hash 输入，后续先落 `zr.md.sig.v1` 规范化编码再启用稳定哈希 golden。

> 进度更新（2026-06-17 19:07:30 +08:00）
>
> 状态：P0 签名哈希基础切片已完成。`zr_vm_core/hash` 新增不使用进程随机 seed 的稳定 `XXH3_64bits` API；`compiler_metadata_token.c` 内部新增 `metadata_signature_hash_v1()`，对已生成的 signature blob 加固定前缀 `zr.md.sig.v1\0` 后计算 `signatureHash`。typed export symbol、`MEMBER_DEF`/`MEMBER_REF` record 及其配对 `SIGNATURE` record 均保存同一个非零签名哈希。
>
> 落盘：`.zro` patch 升到 21；writer 写出 `signatureHash`，reader 在 patch 21+ 读取，在 patch 20 metadata token 产物上保守置 0；runtime loader 复制 typed export symbol hash 与 metadata token record hash。
>
> 验证：`zr_vm_metadata_token_model_test` 3/0，覆盖同签名重建 hash 稳定、ABI signature 改变 hash 改变、`.zro` 读回和 runtime copy 保留 hash；`zr_vm_project_import_canonicalization_test` 5/0，覆盖 using import guard 的 `MEMBER_REF` 与配对 `SIGNATURE` hash 非零且一致；`zr_vm_parser_test` 74/0、LSP statement diagnostic OK、`zr_vm_language_server_shared` 构建通过。模块级 `AssemblyRef/TypeRef`、ref→def 匹配、loader 版本/hash 诊断和模块 hash golden 仍待后续 P0。

> 进度更新（2026-06-17 19:44:56 +08:00）
>
> 状态：P0 import ref owner-chain 基础切片已完成。函数级 import member effect 现在会生成去重后的 `ASSEMBLY_REF`、`TYPE_REF`、`MEMBER_REF` record，并用 `ownerToken` 串成 `AssemblyRef <- TypeRef <- MemberRef`；每个实体 record 的配对 `SIGNATURE` record 也通过 `ownerToken` 指回被签名实体。
>
> 落盘：`.zro` patch 升到 22；writer 写出 `SZrMetadataTokenRecord.ownerToken`，reader 在 patch 22+ 读取，在 patch 20/21 metadata token 产物上保守置 0。`signatureHash` 仍沿用 patch 21 的 `zr.md.sig.v1\0` 稳定 hash。
>
> 验证：TDD RED 先确认 `SZrMetadataTokenRecord` 缺少 `ownerToken` 时 `zr_vm_metadata_token_model_test` 编译失败；实现后 `zr_vm_metadata_token_model_test` 4/0，覆盖 import ref 的 `ASSEMBLY_REF`/`TYPE_REF`/`MEMBER_REF` 记录、owner-chain 和配对 `SIGNATURE` owner 指回。完整 native/source/binary ref→def 签名匹配、loader 版本/hash 诊断、模块签名 hash golden 仍待后续 P0。

> 进度更新（2026-06-17 20:28:55 +08:00）
>
> 状态：P0 import ref 目标签名身份基础切片已完成。`SZrFunctionModuleEffect` / `SZrIoFunctionModuleEffect` 现在携带 `targetMetadataToken`、`targetSignatureToken`、`targetSignatureHash`，module init analysis 会从 source/binary summary 尽量把 provider export 的 token/hash 传到 guarded import effect；`SZrMetadataTokenRecord` 新增 `targetSignatureHash`，`MEMBER_REF` 及其配对 `SIGNATURE` record 会保存该 ref 期望绑定到的目标签名 hash。
>
> 落盘：`.zro` patch 升到 23；writer/reader 持久化 metadata record 的 `targetSignatureHash` 以及 module effect 的目标 token/hash 字段，patch 20/21/22 读取时保守置 0。runtime loader copy 已保留 module effect target identity。
>
> 签名：函数级 `MEMBER_REF` signature blob 仍以 `MEMBER_REF <moduleName> <symbolName> <effectKind>` 为前缀，现在会在可解析 provider export 时追加目标 `METHOD_SIG` 或 `FIELD_SIG` 子签名。source summary 尚无 provider def hash 时，`targetSignatureHash` 会以追加的目标子签名计算稳定 hash；binary summary 已有 provider export hash 时优先沿用 provider hash。后续仍需把这些函数级事实提升为完整模块级 ref→def 绑定。
>
> 验证：`zr_vm_project_import_canonicalization_test` 6/0，覆盖 plugin-guard import 的 canonical module effect、`ASSEMBLY_REF`/`TYPE_REF`/`MEMBER_REF` 数量、`targetSignatureHash` 非零且同步到配对 `SIGNATURE`、以及 `helper.add(i32,i32)` 的 `METHOD_SIG` 参数数；`zr_vm_metadata_token_model_test` 5/0 覆盖 patch 23 target identity 写入/读取/runtime copy，`ctest -R metadata_token_model` 1/1 仍通过。`zr_vm_type_inference_test` 新增 source/binary import identity preservation 两条 PASS；全套仍有既有 7 failures。

> 进度更新（2026-06-17 20:55:47 +08:00）
>
> 状态：P0 metadata token 签名 codec 模块拆分已完成。新增 `compiler_metadata_signature.c/.h`，把 `zr.md.sig.v1\0` stable hash、type/symbol signature size/write、union type signature 识别和低层 signature 写入 helper 从 `compiler_metadata_token.c` 移出；原 token 文件保留 RID 分配、record pair、owner-chain、target signature 查询和 import effect 编排。
>
> 备注：这是无行为变化的结构整理，为继续实现 native/source/binary ref→def 绑定和 loader 诊断腾出边界；`compiler_metadata_token.c` 从 1103 行降到 811 行，`compiler_metadata_signature.c` 为 293 行。
>
> 验证：拆分前后 `zr_vm_metadata_token_model_test` 均为 5/0，`zr_vm_project_import_canonicalization_test` 均为 6/0；拆分后 `ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R metadata_token_model --output-on-failure` 1/1 通过。

> 进度更新（2026-06-17 21:13:25 +08:00）
>
> 状态：P2/P0 插件 guard runtime 目标签名哈希校验基础切片已完成。`using (var p = %import(...))` 的 guard lowering 不再走普通 `%import` native helper，而是发出 `ZrCore_Module_ImportGuardNativeEntry`；普通 `%import` 仍使用 `ZrCore_Module_ImportNativeEntry`。guard helper 复用 runtime 现有 `moduleEntryEffects` / `targetSignatureHash` 校验逻辑，provider export 的签名哈希与编译期目标哈希不一致时返回 null，guard 条件失败后进入 `else`。
>
> 备注：这是函数级 token/effect 事实对 runtime guard 的首个消费点；它还不是完整模块级 `AssemblyRef/TypeRef/MemberRef` ref→def 绑定，也没有覆盖不可用插件加载、`PluginLoad.Available`、scoped release 或插件类型逃逸。
>
> 验证：`cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_project_import_canonicalization_test -j1` 通过；`./build/codex-using-exhaustive-wsl-gcc-debug/bin/zr_vm_project_import_canonicalization_test` 7/0，新增 `targetSignatureHash` mismatch case 走 `else` 返回 77；`zr_vm_metadata_token_model_test` 5/0；`ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R metadata_token_model --output-on-failure` 1/1。

> 进度更新（2026-06-17 21:50:33 +08:00）
>
> 状态：P0 native import 编译期成员签名身份切片已完成。`type_inference_native.c` 会在从 `__zr_native_module_info` / native descriptor 建立 module prototype 时，为 module-level functions/constants/modules/types 分配 synthetic `MEMBER_DEF` 与 `SIGNATURE` token，并用 native module/member/value type/parameter typeName 序列计算稳定 signature hash。source/binary import 已保留 provider token/hash；此切片补齐 native import view，使三类 import 都能在 `SZrTypeMemberInfo` 上暴露非零 metadata/signature identity。
>
> runtime 补强：`module_init_analysis.c` 会把 exported callable summary 的 guarded import effects 复制到对应 child function；guard import 的 target hash 校验运行期跳过自身 native helper frame 后读取实际 caller function 的 `moduleEntryEffects`。因此 `pub func run(){ using (%import(...)) ... }` 这类嵌套 callable 内的签名漂移也会让 guard 返回 null 并进入 `else`。
>
> 验证：新增 `test_type_inference_native_import_function_member_preserves_metadata_signature_identity` 先红后绿并 PASS；完整 `zr_vm_type_inference_test` 仍为既有 7 failures。`zr_vm_project_import_canonicalization_test` 9/0，覆盖 nested caller target hash fallback；`zr_vm_metadata_token_model_test` 5/0；`ctest -R metadata_token_model` 1/1；`git diff --check` 退出码 0（仅 LF/CRLF 提示）。

> 进度更新（2026-06-17 22:13:28 +08:00）
>
> 状态：P0 def/ref canonical `MethodSig` / `FieldSig` 编码统一切片已完成。`compiler_metadata_signature.c/.h` 现在暴露共享的 `metadata_token_method_signature_*` 与 `metadata_token_field_signature_*` helper；typed export `MEMBER_DEF` / `SIGNATURE` 记录和 import `MEMBER_REF` 追加的目标子签名都走同一套 canonical `METHOD_SIG` / `FIELD_SIG` 字节流。`targetSignatureHash` 因此可以直接作为追加目标子签名的 `zr.md.sig.v1\0` stable hash，后续 loader/ref→def 绑定不再需要在 typed export def 与 source/binary import ref target 两套编码之间做兼容桥接。
>
> 验证：TDD RED 先确认 source guard import 中 `helper.add(i32,i32)` 的 `MEMBER_REF.targetSignatureHash` 不等于追加 `METHOD_SIG` 子签名的 hash；实现后 `zr_vm_project_import_canonicalization_test` 9/0，`zr_vm_metadata_token_model_test` 5/0，`ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R metadata_token_model --output-on-failure` 1/1。source/binary/native import identity 用例均 PASS；完整 `zr_vm_type_inference_test` 仍为既有 7 failures。

> 进度更新（2026-06-17 22:38:56 +08:00）
>
> 状态：P0/P2 guard runtime 的 hash-first + signature-confirmed 校验切片已完成。`module_loader.c` 在 `targetSignatureHash` 命中后，会从 caller function 的 `MEMBER_REF` metadata blob 截取追加的目标 `METHOD_SIG` / `FIELD_SIG` 子签名，并与 provider typed export 的 `signatureBlobHeap` 字节确认；只篡改目标签名 blob、保持 hash 不变时，guard import 也会返回 null 并进入 `else`。旧 metadata 缺少目标子签名时仍保留 hash-only 兼容路径。
>
> native 补强：`module_init_analysis.c` 在 source/binary summary 不可用时会只尝试 native compile info fallback，从 native module prototype member 读取 synthetic `MEMBER_DEF` / `SIGNATURE` token 和 signature hash；因此 `using (var math = %import("zr.math")) { math.abs(...) }` 也会在 module effect 上携带 native target signature identity。
>
> 验证：TDD RED 先复现只篡改 `MEMBER_REF` 目标签名 blob 会绕过 hash-only guard 并触发运行时断言；实现后 `zr_vm_project_import_canonicalization_test` 11/0，覆盖 blob mismatch fallback 与 native target signature hash；`zr_vm_metadata_token_model_test` 5/0；`ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R metadata_token_model --output-on-failure` 1/1。

> 进度更新（2026-06-17 22:40:36 +08:00）
>
> 状态：P0/P2 guard runtime 签名字节确认切片完成并重新验证。`module_loader.c` 的 guard 校验现在先按 `targetSignatureHash` 快速筛选 provider typed export，随后在 caller `MEMBER_REF` 记录存在追加 target 子签名时，把该 `METHOD_SIG` / `FIELD_SIG` 字节段与 provider export 的 signature blob 做长度和字节级比对。这样即使 test fixture 只篡改 caller target signature bytes、保持 effect hash 不变，也会返回 null 并执行 guard `else`。
>
> native 补强：native module 没有 source/binary summary 是正常情况，`module_init_analysis.c` 在 summary 不可用时会走 native compile-info fallback，从 native prototype member 读取 synthetic token/hash；`zr.math.abs` 的 guarded import effect 因此也能记录非零 target signature hash。
>
> 验证：`cmake --build build/codex-using-exhaustive-wsl-gcc-debug --target zr_vm_project_import_canonicalization_test zr_vm_metadata_token_model_test -j1` 成功；`zr_vm_project_import_canonicalization_test` 11/0；`zr_vm_metadata_token_model_test` 5/0；`ctest --test-dir build/codex-using-exhaustive-wsl-gcc-debug -R "metadata_token_model|project_import_canonicalization" --output-on-failure` 中 metadata token 1/1 通过；`zr_vm_type_inference_test` 的 source/binary/native import signature identity 用例 PASS，完整套件仍为既有 115 tests / 7 failures。

> 进度更新（2026-06-17 23:49:41 +08:00）
>
> 状态：P0 source/binary import 的同名函数签名候选切片已完成。typed export `MEMBER_DEF` 构建现在优先用 `callableChildIndex` 和 child function 源码范围定位对应 function declaration，避免同名 overload 共用第一个声明的参数签名；source/binary import 成员导入不再按名字去重丢弃同名函数，并且 runtime/IO parameter metadata 复制不会覆盖已从 typed export materialize 出来的 `parameterTypes`。
>
> 调用解析：新增 `type_inference_member_resolution.c`，member chain 在下一段是 call 时会走 signature-aware resolver。resolver 会按位置/命名/default 参数整理实参类型，用 exact/compatible/incompatible 评分选择候选；无匹配时报告 typed argument mismatch，存在 generic callable 时仍保留既有 generic diagnostic。编译路径的 member expression 同步使用这条调用级解析，普通非调用 member lookup 保持原逻辑。
>
> 验证：TDD RED 先确认 source/binary same-name import 只保留 1 个候选，随后确认两个候选仍错误复用第一个签名；GREEN 后新增 `Source Import Keeps Same Name Signature Candidates` 与 `Binary Import Keeps Same Name Signature Candidates` PASS。`zr_vm_type_inference_test` 117 tests / 7 baseline failures，其中 source/binary/native import mismatch 与 metadata identity 用例 PASS；`zr_vm_project_import_canonicalization_test` 11/0；`zr_vm_metadata_token_model_test` 5/0；`ctest -R metadata_token_model` 1/1。

> 进度更新（2026-06-18 00:09:54 +08:00）
>
> 状态：P0 required import runtime 签名校验基础切片已完成。`module_loader.c` 不再只在 guard helper 上消费 target signature identity；普通 `ZrCore_Module_ImportNativeEntry` 与 `ZrCore_Module_ImportGuardNativeEntry` 现在共用同一条 import 校验路径。loader 会跳过 import native helper frame，定位实际 caller function，再用 caller `moduleEntryEffects` 的 `targetSignatureHash` 与 provider typed export 的 `signatureHash` 做 hash-first 比对；如果 caller `MEMBER_REF` blob 含追加目标 `METHOD_SIG` / `FIELD_SIG`，继续与 provider export signature blob 做长度和字节确认。
>
> 运行期行为（历史中间态，已被 2026-06-18 00:48:54 结构化诊断切片取代）：required import 遇到 hash 或签名字节不匹配时返回 `null`，后续读取成员会按现有 runtime 错误路径失败；guard import 仍通过相同 mismatch 结果进入 `else`。后续切片已把 required mismatch 提升为 `import signature mismatch` 运行期诊断。
>
> 验证：新增 `test_required_import_runtime_rejects_signature_hash_mismatch` 先红后绿；RED 时篡改 `.helper.math.answer` 的 `targetSignatureHash` 后普通 `%import` 仍返回 provider 值；GREEN 后 `zr_vm_project_import_canonicalization_test` 12/0。回归：`zr_vm_metadata_token_model_test` 5/0，`ctest -R metadata_token_model` 1/1；完整 `zr_vm_type_inference_test` 仍为既有 117 tests / 7 baseline failures，source/binary/native import identity 与候选用例继续 PASS。

> 进度更新（2026-06-18 00:48:54 +08:00）
>
> 状态：P0 required import 签名 mismatch 结构化诊断切片已完成。`module_loader.c` 现在会在 shared import verification 中记录首个 mismatch 的 module/member 和 expected/actual signature hash；普通 required `%import` 失败时恢复 native helper frame 后直接通过 `ZrCore_Debug_RunError` 抛出 `import signature mismatch`，不再依赖后续 member access 报错。guard `%import` 保持可选插件语义，仍在同一 mismatch 结果下返回 `null` 并执行 `else`。
>
> 同步修正：插件 guard escape scanner 在扫描 `%import` guard body 时使用 import module name 解析成员元数据，只把 guard handle 本身或 callable member reference 视为可逃逸插件值；普通成员值和调用结果不再被误拒绝。这样 `return helper.answer` / `return math.abs(-3.0)` 可继续通过，而 `return math` / `return math.abs` 仍报告 `plugin_type_escape`。
>
> 验证：TDD RED 先确认 required mismatch 的 exception message 不含 `import signature mismatch`；实现后 `build-wsl-clang` 中 `zr_vm_project_import_canonicalization_test` 12/0，新增断言覆盖 module/member/hash 诊断文本。`zr_vm_metadata_token_model_test` 5/0，`ctest -R metadata_token_model` 1/1。`zr_vm_type_inference_test` 仍为既有 117 tests / 7 baseline failures。`zr_vm_compiler_integration_test` 的 `Ownership Builtin Compile Rejects Invalid Operands` 切片通过，完整二进制仍在后续既有 ownership runtime/assertion 处失败。

> 进度更新（2026-06-18 05:28:40 +08:00）
>
> 状态：P0/P2 runtime module ref table 直连校验切片完成。`module_import_signature.c` 不再只把 `moduleMetadataTokenRecords` 当作 effect identity/blob 的补充来源；shared verifier 会在 `moduleEntryEffects` 校验后直接遍历 entry-function 级 module ref table，解码匹配当前 import module 的 `MEMBER_REF` 为临时 effect，验证 `AssemblyRef <- TypeRef <- MemberRef` owner-chain，并按 target module hash、target metadata/signature token、target signature hash 和 target `METHOD_SIG` / `FIELD_SIG` bytes 重新绑定 provider typed export。
>
> 同步收紧：`SZrModuleImportSignatureMismatch` 新增 effect snapshot，required-import 诊断不再持有 ref-table 临时 effect 的栈指针；provider/effect 两侧 target tokens 都存在且不一致时直接 mismatch，只有旧产物缺 token 时才允许继续用 hash/blob fallback。
>
> 验证：新增 `test_using_import_guard_runtime_verifies_module_ref_table_without_entry_effects` 先 RED，清空 entry effects、只篡改 module ref table target signature bytes 时旧 runtime 返回 provider 值 `40`；GREEN 后 guard 走 `else` 返回 `77`。`build-wsl-clang` 中 `zr_vm_project_import_canonicalization_test` 21/0，`zr_vm_metadata_token_model_test` 6/0，metadata CTest 1/1；`git diff --check` 无 whitespace error，仅 LF/CRLF 提示。剩余：把 ref→def binding result 持久化/暴露为可诊断表、loader version compatibility 诊断、registry/DLL load+verify 和版本约束。

> 进度更新（2026-06-18 08:13:03 +08:00）
>
> 状态：P0 AssemblyRef semantic version constraints 切片已完成。`SZrFunction` / `SZrIoFunction` 新增 `moduleVersion`，module init analysis 从当前 project version 或 dependency module key（如 `$math@1.0.0/ops/sum`）推导 provider 版本；可签名 import effect 与 `AssemblyRef` metadata record 保存 `requestedModuleVersion`、`minModuleVersionInclusive`、`maxModuleVersionExclusive`。未显式写约束时默认区间为 `[compiledVersion, nextMajor.0.0)`。
>
> 落盘：`.zro` patch 升到 29；writer/reader 在函数 metadata token model 中写入 provider `moduleVersion`，并在 metadata token record 与 module effect 中写入 requested/min/max version 字符串；patch 28 及更早产物读取时这些字段为 null。`moduleSignatureHash` 的 `zr.md.mod.v1\0` 输入现在包含 provider module version，因此同导出形状但不同 module version 的 ABI fingerprint 会漂移。
>
> runtime：`module_import_signature.c` 在 target module ABI hash、target token、target signature hash 与 target bytes 校验前先检查 provider `moduleVersion` 是否落入 AssemblyRef version range。guard `%import` 版本不兼容时返回 null 并执行 `else`；required `%import` 抛出 `assembly_version_mismatch`，消息包含 canonical module/member、`minVersionInclusive`、`maxVersionExclusive` 和 `actualVersion`。
>
> 验证：新增 `test_required_import_runtime_rejects_assembly_ref_version_mismatch` 先 RED，篡改 `AssemblyRef` range 到 `[2.0.0, 3.0.0)` 时旧 runtime 仍接受 provider `1.0.0`；GREEN 后 required import 抛版本 mismatch。`zr_vm_project_import_canonicalization_test` 24/0，`zr_vm_metadata_token_model_test` 9/0，metadata CTest 1/1；`git diff --check` 仅既有 LF/CRLF 提示。剩余：显式版本约束语法、registry/DLL load+verify、`PluginLoad.Available` 和 scoped release/share lifecycle。

> 进度更新（2026-06-18 08:39:06 +08:00）
>
> 状态：P0/P2 native registry load/verify 诊断切片已完成。注册式 native provider 复用同一 `module_import_signature.c` verifier；guard `%import("zr.math")` 在 target signature hash drift 时返回 null 并执行 `else`。普通 required `%import("zr.math")` 在运行期 provider/registry 不可用且线程未已有异常时由 `module_loader.c` 抛出 `import_load_unavailable: module 'zr.math'`。
>
> 验证：新增 native target-hash drift guard 回归立即通过，确认 registered native provider 已经由 verifier 覆盖；新增 `test_required_import_runtime_reports_native_provider_unavailable` 先 RED 于缺少 `import_load_unavailable` 诊断，GREEN 后 `zr_vm_project_import_canonicalization_test` 26/0。剩余：显式版本约束语法、真实 descriptor-DLL last-error/loader source 透传、`PluginLoad.Available` 和 scoped release/share lifecycle。

> 进度更新（2026-06-18 09:06:55 +08:00）
>
> 状态：P0 dependency manifest 显式 AssemblyRef version range 切片已完成。依赖声明对象支持 `minVersionInclusive` / `maxVersionExclusive`，作为不改 `%import` parser 语法的显式版本约束 surface；默认 `[compiledVersion,nextMajor)` 仍用于没有声明 range 的依赖。
>
> 实现：`SZrLibrary_ProjectDependencyReference` 保存显式 min/max range，`project.c` 从 dependency declaration object 解析字段并拒绝同一 owner 对同一 package/version 使用不同 range；`ZrLibrary_Project_GetDependencyImportVersionRange()` 按 current module key 与 resolved `$pkg@version/path` 回查 requested/min/max；`module_init_analysis.c` 在 source/binary import effect 写入阶段覆盖默认 `AssemblyRef` range。
>
> 验证：新增 `test_project_compile_applies_dependency_import_version_range_to_assembly_ref` 先 RED，`AssemblyRef` record 仍写默认 `requested=1.2.3,min=1.2.3,max=2.0.0`；GREEN 后 record 写入 `requested=1.2.3,min=1.2.0,max=1.3.0`。`build-wsl-clang` 中 `zr_vm_project_import_canonicalization_test` 27/0，`zr_vm_metadata_token_model_test` 9/0，metadata CTest 1/1。后续 loader diagnostics、`PluginLoad.Available` 和 registry owner refcount API 已完成；剩余 descriptor DLL safe unload/cache invalidation、AOT descriptor 更细 diagnostics 和更完整 global/cross-region escape。

> 进度更新（2026-06-18 09:32:13 +08:00）
>
> 状态：P0/P2 loader source/descriptor diagnostics 基础切片已完成。`SZrGlobalState` 新增 module-load diagnostic buffer 与 clear/set/get API；`ZrCore_Module_ImportByPath()` 在真实加载开始时清空诊断，并在 source loader、binary reader、source reader、compiler unavailable/failed 等路径留下 loader detail。required `%import` 的 `import_load_unavailable` 会附带该 detail。project source loader miss 会记录 canonical module、source `.zr` 尝试路径和 binary `.zro` 尝试路径；native descriptor plugin loader 会把 registry last-error 透传为 `loader=native-plugin result=descriptor-load-failed ...`，且该真实 plugin 错误不会被后续 source miss 覆盖。
>
> 验证：新增 `test_required_import_runtime_reports_source_loader_attempts` 先 RED 于缺少 `loader=project-source`，GREEN 后 required import 异常包含 `source=...feature/app/helper/math.zr` 与 `binary=...feature/app/helper/math.zro`。新增 `test_required_import_runtime_reports_descriptor_plugin_load_error` 覆盖无效 `native/zrvm_native_acme_bad.*` descriptor plugin，断言 core module-load diagnostic 与 `ZrLibrary_NativeRegistry_GetLastErrorMessage()` 都包含 load error 和 plugin source path。`build-wsl-clang` 中 `zr_vm_project_import_canonicalization_test` 29/0。后续 registry owner refcount API 已完成；剩余 descriptor DLL safe unload/cache invalidation、AOT descriptor 更细 last-error/source 透传、`TypeSpec` 和 shared string heap 长期项。

> 进度更新（2026-06-18 10:04:14 +08:00）
>
> 状态：P0 TypeSpec baseline 切片已完成。新增 `compiler_metadata_type_spec.c/.h`，从导出 typed signature 的 return/parameter type 中扫描 nullable、ownership、array 和带泛型实参的闭型，为它们生成 `TYPE_SPEC` 实体 record 和配对 `SIGNATURE` record；records 继续使用现有 `SZrMetadataTokenRecord` 和 `signatureBlobHeap`，不新增 `.zro` patch schema。普通命名类型仍编码为 `TYPE_REF`。
>
> 实现：`compiler_metadata_signature.c` 把非 union 泛型闭型规范化为 `GENERIC_INST <open TYPE_REF> <args...>`，例如 `Box<int>` 不再落成裸 `TYPE_REF("Box<int>")`；已知 union 闭型如 `Option<int>` 保留现有 `UNION` signature node，以维持 union-specific ABI identity。`compiler_metadata_token.c` 只接入 TypeSpec plan/emit 边界，避免继续扩大 token orchestration 文件。
>
> 验证：新增 `test_union_return_type_emits_type_spec_token_record` 先 RED 于缺少 `TYPE_SPEC` record，GREEN 后 `Option<int>` 产生 `TYPE_SPEC` + paired `SIGNATURE`，signature blob 为 `UNION Option<int>`。新增 `test_generic_return_type_emits_generic_inst_type_spec` 先 RED 于 `Box<int>` 仍为 `TYPE_REF`，GREEN 后 TypeSpec signature 为 `GENERIC_INST(TYPE_REF("Box"), int)`。`build-wsl-clang` 中 `zr_vm_metadata_token_model_test` 11/0、`zr_vm_project_import_canonicalization_test` 29/0，metadata CTest 1/1。剩余：TypeSpec 去重策略、跨模块 TypeSpec ref→def binding、shared string heap 索引化、descriptor DLL safe unload/cache invalidation、AOT descriptor 更细 diagnostics 和更完整 global/cross-region escape。

> 进度更新（2026-06-18 10:17:58 +08:00）
>
> 状态：P0 TypeSpec 去重切片已完成。`compiler_metadata_type_spec.c` 的 plan/emit 路径现在先按 canonical type signature blob 去重，再写入 `TYPE_SPEC` 实体 record 和配对 `SIGNATURE` record；同一导出签名的 return/parameter 重复引用同一闭型时，只保留一组 TypeSpec 记录和一次 heap bytes。
>
> 验证：新增 `test_duplicate_generic_type_spec_is_deduplicated` 先 RED 于 `Box<int>` return 和 parameter 生成两个 `TYPE_SPEC` records；GREEN 后只生成一个 `TYPE_SPEC` record pair。`build-wsl-clang` 中 `zr_vm_metadata_token_model_test` 12/0、`zr_vm_project_import_canonicalization_test` 29/0，metadata CTest 1/1。剩余：跨模块 TypeSpec ref→def binding、shared string heap 索引化、descriptor safe unload/cache invalidation、AOT descriptor 更细 diagnostics 和更完整 global/cross-region escape。
>
> 进度更新（2026-06-18 10:52:36 +08:00）
>
> 状态：P0 TypeSpec 跨模块 binding baseline 已完成。`zr_vm_core/src/zr_vm_core/function_metadata_binding.c` 现在承载 module metadata binding 的 query/upsert 共享逻辑，并新增 `ZrCore_Function_BindMatchingTypeSpecMetadata()`。该 helper 遍历 caller 的 `TYPE_SPEC` records，按 canonical type signature hash 和 signature blob 字节匹配 provider `TYPE_SPEC` records，再把 caller `TYPE_SPEC` token、paired `SIGNATURE` token 和 provider resolved `TYPE_SPEC` / `SIGNATURE` identity 写入 `SZrFunction.moduleMetadataBindings`。`module_import_signature.c` 在 member import verifier 成功后调用该 helper，作为跨模块 TypeSpec binding baseline；它复用现有 sidecar，不新增 `.zro` patch。
>
> 验证：新增 `test_matching_type_spec_records_bind_by_canonical_signature` 先 RED 于链接失败（缺少 `ZrCore_Function_BindMatchingTypeSpecMetadata`），GREEN 后 WSL clang 和 WSL gcc 均通过 focused build/test：`zr_vm_metadata_token_model_test` 13/0、`zr_vm_project_import_canonicalization_test` 29/0、metadata CTest 1/1。`git diff --check` 仅报告既有 LF/CRLF normalization warnings。后续 shared string heap 索引化已在 2026-06-18 11:42:19 完成；剩余：descriptor safe unload/cache invalidation、AOT descriptor 更细 diagnostics、更严格 TypeSpec mismatch diagnostics 和更完整 global/cross-region/async escape。
>
> 进度更新（2026-06-18 11:40:20 +08:00）
>
> 状态：P0/P2 metadata shared string heap 验证读取收敛完成。shared string heap 索引化后，`tests/parser/test_project_import_canonicalization.c` 的 metadata blob helper 已按 `SZrFunction.metadataStringHeap` 解析 module/symbol/type string ref 索引，并保留旧 inline length-prefixed string 回退；`skip_metadata_blob_type_signature()` 递归携带 `SZrFunction`，因此 `MEMBER_REF` / `ASSEMBLY_REF` 与 TypeSig 中的字符串不会在验证侧被误读。
>
> 验证：RED 为 `zr_vm_project_import_canonicalization_test` 29 tests / 10 failures，失败集中在 helper 仍按内嵌字符串解析 shared heap 索引；GREEN 后 WSL GCC `zr_vm_project_import_canonicalization_test` 29/0、`zr_vm_metadata_token_model_test` 14/0、`zr_vm_gc_test` 66/0。`zr_vm_compiler_integration_test` 的 invalid-operands、`.share()`、scoped release、`PluginLoad.Available` 聚焦用例 PASS，完整 integration 仍为既有 115/23。该测试文件当前超过 2000 行，本次保持同一 project-import metadata helper 责任边界；继续扩展时应优先拆出 fixture/blob 读取辅助。剩余：descriptor safe unload/cache invalidation、AOT descriptor 更细 diagnostics、更严格 TypeSpec mismatch diagnostics 和更完整 global/cross-region/async escape。
>
> 进度更新（2026-06-18 11:42:19 +08:00）
>
> 状态：P0 shared metadata string heap indexing baseline 已完成。`SZrMetadataStringHeapEntry` 定义函数级 string index + value 表，`SZrFunction` / `SZrIoFunction` 持有 `metadataStringHeap`，signature blob 中的 module/type/symbol 字符串引用改为固定 `u32` content-stable index。`compiler_metadata_token.c` 在构建 token model 前收集导出名、module version、import ref、TypeSpec/generic/union type name 等字符串；`compiler_metadata_signature.c` 和 `compiler_metadata_type_spec.c` 共享同一 heap-indexed writer。
>
> 落盘：`.zro` patch 升到 30；writer 在 `signatureBlobHeap` 后写入 metadata string heap，reader 对 patch 30+ 读取 heap，patch 29 及更早产物保留空 heap 并继续按旧 inline 字符串 decode。`io_runtime.c` 复制 heap entry array 到 runtime function，`module_import_signature.c` 的 runtime verifier 同时支持 patch 30 heap-indexed 字符串和旧 patch inline 字符串。
>
> 验证：新增 `test_metadata_strings_are_indexed_in_shared_heap_through_binary_and_runtime` 先 RED 于 `SZrFunction` / `SZrIoFunction` 缺少 heap 字段；GREEN 后 WSL clang/gcc focused build/test 均通过：`zr_vm_metadata_token_model_test` 14/0、`zr_vm_project_import_canonicalization_test` 29/0、metadata CTest 1/1。`git diff --check` 仅报告既有 LF/CRLF normalization warnings。剩余：descriptor safe unload/cache invalidation、AOT descriptor 更细 diagnostics、更严格 TypeSpec mismatch diagnostics 和更完整 global/cross-region/async escape。
>
> 进度更新（2026-06-18 12:16:33 +08:00）
>
> 状态：P0 TypeSpec mismatch status diagnostics baseline 已完成。`SZrMetadataTypeSpecBindStatus` 提供 core 层 TypeSpec binding 汇总：caller `TYPE_SPEC` 数、成功匹配数、未匹配数、首个未匹配 caller token 与 signature hash。`ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus()` 会在 caller/provider canonical TypeSpec signature hash + blob 无匹配时填充该 status 并返回 false；原 `ZrCore_Function_BindMatchingTypeSpecMetadata()` 继续把未匹配视为 best-effort skip，保持当前 import verifier 不因 opportunistic TypeSpec sidecar 缺失而改变加载语义。
>
> 验证：新增 `test_type_spec_binding_reports_unmatched_caller_signature` 先 RED 于缺少 `SZrMetadataTypeSpecBindStatus` 和 `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus`；GREEN 后 WSL clang/gcc focused build/test 均通过：`zr_vm_metadata_token_model_test` 15/0、`zr_vm_project_import_canonicalization_test` 29/0、metadata CTest 1/1。warning grep 未发现 touched metadata/token warning/error；`git diff --check` 仅报告既有 LF/CRLF normalization warnings。后续 loader-facing TypeSpec mismatch diagnostics 已在 2026-06-18 13:00:54 +08:00 完成；剩余：descriptor safe unload/cache invalidation 和 AOT descriptor 更细 diagnostics。
>
> 进度更新（2026-06-18 12:49:27 +08:00）
>
> 状态：P0/P2 descriptor plugin safe unload/cache invalidation baseline 已完成。native registry 新增 `ZR_LIB_NATIVE_REGISTRY_ERROR_MODULE_IN_USE`；descriptor plugin source invalidation 会先扫描即将失效的 plugin-backed module records，任何 `ownerRefCount > 0` 都会拒绝清 module cache、关闭 plugin handle 和移除 descriptor record，并保留原状态。descriptor plugin reload 也会在替换同名 descriptor record / handle 前拒绝 live owner refs，避免旧动态库仍被 shared owner 引用时被卸载。
>
> 验证：新增 `zr_vm_native_registry_descriptor_invalidation_test` 先 RED 于缺少 in-use error/保护；GREEN 后测试通过真实 descriptor plugin fixture 和公开 `EnsureProjectDescriptorPlugin()` 注册路径，再模拟 live owner ref，确认 invalidation 返回 false、错误码为 `MODULE_IN_USE`、record 保留，ref 归零后 invalidation 成功并移除记录。WSL clang/gcc focused build/test 均通过：`zr_vm_native_registry_descriptor_invalidation_test` 1/0、`zr_vm_metadata_token_model_test` 15/0、`zr_vm_project_import_canonicalization_test` 29/0，CTest 覆盖 metadata token 与 native registry invalidation 2/2；warning grep 未发现 touched-file warning/error；`git diff --check` 仅报告既有 LF/CRLF normalization warnings。后续 loader-facing TypeSpec mismatch diagnostics 已在 2026-06-18 13:00:54 +08:00 完成；剩余：AOT descriptor 更细 diagnostics。
>
> 进度更新（2026-06-18 13:00:54 +08:00）
>
> 状态：P0/P2 loader-facing TypeSpec mismatch diagnostics 已完成。`module_import_signature.c` 在成功 member import verification 和 ref→def binding 后调用 `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus()`；当 caller/provider TypeSpec sidecar 存在 unmatched caller `TYPE_SPEC` 时，写入 global module-load diagnostic `type_spec_mismatch`，包含 module/member、caller/matched/unmatched 计数、首个未匹配 TypeSpec token 和 signature hash。该诊断只暴露 drift，不让 opportunistic TypeSpec sidecar 改变 import 成功语义。
>
> 验证：新增 `test_using_import_signature_reports_typespec_mismatch_diagnostic` 先 RED 于 import verifier 成功但 `moduleLoadDiagnostic` 为空；GREEN 后 WSL GCC focused build/test 通过：`zr_vm_project_import_canonicalization_test` 30/0、`zr_vm_metadata_token_model_test` 15/0。重新构建并运行 `zr_vm_compiler_integration_test` 时仍为既有 frame-layout/quickening/import baseline 失败；本切片聚焦验证通过。剩余：AOT descriptor 更细 diagnostics。
>
> 进度更新（2026-06-18 13:33:34 +08:00）
>
> 状态：P0/P2 AOT descriptor diagnostics 与 loader bridge 已完成。`ZrLibrary_AotRuntime_ModuleLoader()` 在 descriptor prepare 或 entry execute 失败时，会把 `ZrLibrary_AotRuntime_GetLastError()` 写入 global module-load diagnostic，诊断格式包含 `loader=aot-runtime`、`backend=aot-c|aot-llvm`、`result=descriptor-load-failed|module-execute-failed`、module 和 detail；AOT descriptor validation 的 lastError 继续覆盖 ABI/backend/moduleName/entryThunk/blob/thunk 等字段级失败。
>
> 验证：新增 `test_project_import_reports_aot_descriptor_load_error` 先 RED 于严格 AOT import 缺失 artifact 时 `moduleLoadDiagnostic` 为空；GREEN 后 WSL GCC `zr_vm_project_import_canonicalization_test` 31/0、`zr_vm_metadata_token_model_test` 15/0、`zr_vm_native_closure_value_test` 3/0、`zr_vm_aot_c_descriptor_diagnostics_test` 1/0，CTest `aot_c_descriptor_diagnostics|metadata_token_model` 2/2。剩余：更完整 cross-region/async escape、完整 load+verify 收尾和 owner payload move 语义。

> 进度更新（2026-06-18 14:09:55 +08:00）
>
> 状态：P0 本地 union `TYPE_DEF` baseline 已完成。新增 `compiler_metadata_type_def.c/.h`，metadata token builder 在导出 typed signature 的 return/parameter 类型中识别当前脚本 union 定义，并按 union base name + generic arity 去重后生成本模块定义侧 `TYPE_DEF` 实体 record 与配对 `SIGNATURE` record。TypeDef signature blob 当前为 `TYPE_DEF <metadataStringHeap name index> <genericArity>`，复用现有 `signatureBlobHeap`、`signatureHash`、record pair、shared metadata string heap 和 `.zro` patch 30 持久化路径，不新增 schema patch。
>
> 验证：新增 `test_union_return_type_emits_type_def_token_record` 先 RED 于 `Option<int>` 只有 `MEMBER_DEF` / `TYPE_SPEC` 而没有 `TYPE_DEF` record；GREEN 后断言 `TYPE_DEF` RID、related `SIGNATURE`、signature owner、hash、blob offset/length 和 `TYPE_DEF` signature node。WSL clang/gcc focused build/test 均通过：`zr_vm_metadata_token_model_test` 16/0、`zr_vm_project_import_canonicalization_test` 31/0、metadata CTest 1/1。后续 variant/field 契约已在 2026-06-18 14:35:52 +08:00 完成，layout identity 已在 2026-06-18 15:28:05 +08:00 完成；剩余：更完整 cross-region/async escape、完整 load+verify 收尾和 owner payload move 语义。
>
> 进度更新（2026-06-18 14:35:52 +08:00）
>
> 状态：P0 本地 union `TYPE_DEF` variant/field 契约已完成。`compiler_metadata_type_def.c` 的 TypeDef signature blob 从 `TYPE_DEF <nameIndex> <genericArity>` 扩展为 `TYPE_DEF <nameIndex> <genericArity> <variantCount> ...`，每个 variant 写入 metadata string heap variant name、variant kind、default using flag、field count；每个 payload field 写入 field name、passing mode 和 canonical TypeSig。`compiler_metadata_type_def_collect_strings()` 接入 token builder 的 shared string heap 收集，确保新增 name/type refs 经 patch 30 heap 索引编码；不新增 `.zro` patch。
>
> 验证：新增 variant/field contract 断言先 RED 于旧 blob 缺少 `variantCount`；GREEN 后 WSL clang/gcc focused build/test 均通过：`zr_vm_metadata_token_model_test` 16/0、`zr_vm_project_import_canonicalization_test` 31/0、metadata CTest 1/1。后续 TypeDef layout identity 已在 2026-06-18 15:28:05 +08:00 完成；剩余：更完整 cross-region/async escape、完整 load+verify 收尾和 owner payload move 语义。
>
> 复核（2026-06-18 14:54:16 +08:00）：MSVC Debug `zr_vm_metadata_token_model_test` 初次链接暴露 `compiler_build_function_metadata_tokens` 未从 parser DLL 导出；为既有 internal declaration/definition 补 `ZR_PARSER_API` 后，MSVC direct run 16/0。随后 WSL clang/gcc `zr_vm_metadata_token_model_test` rebuild + direct run 仍均为 16/0。MSVC 输出仍有既有 core/parser warnings，本阶段没有新增 Windows 测试失败。
>
> 进度更新（2026-06-18 15:28:05 +08:00）
>
> 状态：P0 本地 union `TYPE_DEF` layout identity 已完成。`SZrMetadataTokenRecord` 新增 `layoutVersion`、reserved slot 和 `layoutHash`，`.zro` patch 31 在 metadata record 的 `signatureHash` 后持久化这些字段，patch 30 及更早读取时置 0。`compiler_metadata_type_def_layout.c/.h` 负责从当前脚本 union AST 计算 layout fingerprint，输入只包含 tag size、payload offset/size/align、整体 size/align，以及各 variant payload field 的 offset/size/align；`TYPE_DEF` signature blob 不写入 layout 字段，继续只表达逻辑契约。
>
> 验证：新增 layout 字段断言和 binary/runtime roundtrip 用例先 RED 于 `SZrMetadataTokenRecord` 缺少 `layoutVersion` / `layoutHash`；GREEN 后 WSL clang/gcc `zr_vm_metadata_token_model_test` 17/0、`zr_vm_project_import_canonicalization_test` 31/0、metadata CTest 1/1，MSVC Debug `zr_vm_metadata_token_model_test` 17/0。为避免 `compiler_metadata_type_def.c` 超过模块化阈值，layout fingerprint 逻辑拆到 `compiler_metadata_type_def_layout.c/.h`，主 TypeDef emit 文件回落到 957 行。
>
> 进度更新（2026-06-18 16:11:43 +08:00）
>
> 状态：P0 TypeSpec→TypeDef definition/layout binding 已完成。`SZrMetadataTokenBinding` 新增 expected/resolved `layoutVersion` / `layoutHash`，`.zro` patch 32 在 binding result 中持久化 layout identity，patch 31 及更早读取时置 0。`ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus()` 在 caller/provider `TYPE_SPEC` canonical signature 匹配后，会对 union TypeSpec 继续定位对应 caller/provider `TYPE_DEF`，先确认 TypeDef signature hash 与 blob 字节一致，再确认 record-level layoutVersion/layoutHash 一致；只有这两层都通过，才同时写入 `TYPE_SPEC` binding 和对应 `TYPE_DEF` layout binding。
>
> 诊断：`SZrMetadataTypeSpecBindStatus` 新增 definition/layout mismatch 计数和首个 mismatch 的 expected/actual signature/layout context；loader-facing `type_spec_mismatch` 诊断同步输出这些字段。TypeDef contract 或 layout drift 会返回 false 并拒绝 partial binding，未匹配 TypeSpec 仍保持 best-effort diagnostic，不改变 import 成功语义。
>
> 验证：新增 `test_union_type_spec_binding_records_type_def_layout_identity` 先 RED 于 binding 缺 layout 字段；新增 `test_union_type_spec_binding_reports_layout_mismatch_without_partial_binding` 先 RED 于 layout mismatch 后仍留下 partial TypeSpec binding；新增 `test_union_type_spec_binding_reports_type_def_contract_mismatch_without_partial_binding` 先 RED 于 TypeDef signature drift 仍返回成功。GREEN 后 WSL clang `zr_vm_metadata_token_model_test` 20/0、`zr_vm_project_import_canonicalization_test` 31/0、metadata CTest 1/1；WSL gcc 同组 20/0、31/0、1/1；MSVC Debug metadata token 20/0。

> 进度更新（2026-06-18 17:08:15 +08:00）
>
> 状态：P0 module ABI TypeDef/TypeSpec identity hash 已完成。`compiler_metadata_module_hash.c/.h` 从 `compiler_metadata_token.c` 拆出 module ABI fingerprint 计算，并把 hash schema 升级为 `zr.md.mod.v2\0`。当前 `moduleSignatureHash` 输入包括 provider module version、按导出名排序的 public typed export identity/signature hash/blob，以及按 table/hash/blob/layout 排序的本模块 `TYPE_DEF` / `TYPE_SPEC` 实体 record identity；其中 `TYPE_DEF` 的 `layoutVersion` / `layoutHash` 作为 inline ABI identity 参与 module fingerprint，但仍不进入 `TYPE_DEF` logical signature hash。
>
> 验证：新增 `test_module_signature_hash_changes_with_union_type_def_contract` 先 RED：两个 provider 都导出 `choose(): Option<int>` 时 export signature hash 相同，但 `Option<T>` 变体契约漂移没有改变 `moduleSignatureHash`。GREEN 后同一 public function signature 保持相同 `signatureHash`，本地 union `TYPE_DEF` contract/layout identity 漂移会改变 module hash。WSL GCC `zr_vm_metadata_token_model_test` 21/0、`zr_vm_project_import_canonicalization_test` 31/0、metadata CTest 1/1；WSL clang 同样 21/0、31/0、metadata CTest 1/1；MSVC Debug metadata token 21/0。未新增 `.zro` patch，因为 patch 26 已持久化 `moduleSignatureHash` 字段。
>
> 复核（2026-06-18 10:29:22 +08:00）：补充 TypeSpec plan 空 state guard 与文档状态同步后，重新验证 `zr_vm_metadata_token_model_test` 12/0、`zr_vm_project_import_canonicalization_test` 29/0、metadata CTest 1/1；`git diff --check` 仅报告既有 LF/CRLF normalization warnings。

> 进度更新（2026-06-18 01:17:54 +08:00）
>
> 状态：P0 metadata token record 的 target token identity 补齐切片已完成。`SZrMetadataTokenRecord` 现在与 `SZrFunctionModuleEffect` 一样携带 `targetMetadataToken`、`targetSignatureToken`、`targetSignatureHash`；`MEMBER_REF` 实体 record 与配对 `SIGNATURE` record 会同时保留 provider def token、provider signature token 和期望签名 hash，为后续 ref→def 绑定留下完整目标身份，而不只留下 hash。
>
> 落盘：`.zro` patch 升到 24；writer/reader 在 metadata token record 中写入/读取 `targetMetadataToken` 和 `targetSignatureToken`，patch 23 及更早产物读取时两个 token 保守置 0；既有 patch 23 `targetSignatureHash` 读取逻辑保持兼容。runtime copy 通过 `SZrMetadataTokenRecord` raw copy 保留这两个字段。
>
> 验证：TDD RED 先把 `zr_vm_metadata_token_model_test` 扩展为断言 metadata record target tokens，构建失败于 `SZrMetadataTokenRecord` 无 `targetMetadataToken` / `targetSignatureToken` 字段；实现后 `build-wsl-clang` 中 `zr_vm_metadata_token_model_test` 5/0，`ctest -R metadata_token_model --output-on-failure` 1/1，`zr_vm_project_import_canonicalization_test` 12/0。完整模块级 ref 表、ref→def token binding、loader 版本/hash 诊断和模块 hash golden 仍待后续 P0。

> 进度更新（2026-06-18 01:53:27 +08:00）
>
> 状态：P0 module-level import ref 聚合表切片已完成。`SZrFunction` / `SZrIoFunction` 现在持有 `moduleMetadataTokenRecords` / `moduleMetadataTokenRecordLength`；新拆出的 `compiler_metadata_ref.c/.h` 从已生成的函数级 import ref records 派生 entry-function 级模块 ref 表，避免继续把 ref 查询/聚合职责堆回 `compiler_metadata_token.c`。
>
> 落盘：`.zro` patch 升到 25；writer 在函数级 metadata token block 和 signature heap 后写出 module ref record count + records，reader 在 patch 25+ 读取并兼容旧 patch 默认为空；runtime copy 会把该表复制到运行时 `SZrFunction`。该表复用同一 `signatureBlobHeap`，不复制签名字节。
>
> 语义：当前表聚合 `AssemblyRef` / `TypeRef` / `MemberRef` 及其配对 `SIGNATURE`，并按 metadata table、owner token、signature hash、target metadata/signature token、target signature hash 和 signature blob 字节去重。它是完整 ref→def binding 的结构前置，不等同于已经完成 loader 的 provider def 绑定。
>
> 验证：TDD RED 先扩展 `zr_vm_metadata_token_model_test` 断言 duplicate entry/callable import ref 会聚合为 module table，构建失败于 `SZrFunction` / `SZrIoFunction` 缺少 `moduleMetadataTokenRecords` 字段；实现后 `build-wsl-clang` 中 `zr_vm_metadata_token_model_test` 6/0，`zr_vm_project_import_canonicalization_test` 12/0，`ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure` 1/1。`git diff --check` 无 whitespace error，仅 LF/CRLF 提示。完整 ref→def token binding、loader 版本/hash 诊断、模块 hash golden 和 registry/DLL load+verify 仍待后续 P0。

> 进度更新（2026-06-18 02:25:41 +08:00）
>
> 状态：P0/P2 runtime 已开始消费 entry-function 级 module ref 聚合表。import signature verification 从 `module_loader.c` 抽到 `module_import_signature.c/.h`；查找 caller target `METHOD_SIG` / `FIELD_SIG` 子签名时先搜索 `moduleMetadataTokenRecords`，再回退函数级 `metadataTokenRecords`，保留旧 patch/旧结构兼容。
>
> 验证：新增 `test_using_import_guard_runtime_consumes_module_ref_signature_blob` 只篡改 module ref table 中匹配 `MEMBER_REF` 的 target signature bytes，并保持函数级 records 不变。RED 时旧 runtime 忽略 module ref table，guard 执行 provider 返回 `40`；GREEN 后 runtime 读取 module ref table 并让 guard 走 `else` 返回 `77`。`build-wsl-clang` 下 `zr_vm_project_import_canonicalization_test` 13/0、`zr_vm_metadata_token_model_test` 6/0、`ctest -R metadata_token_model` 1/1；`git diff --check` 无 whitespace error，仅 LF/CRLF 提示。完整 ref→def token binding、loader 版本/hash 诊断、模块 hash golden 和 registry/DLL load+verify 仍待后续 P0。

> 进度更新（2026-06-18 02:36:35 +08:00）
>
> 状态：P0/P2 runtime provider target token 绑定切片已完成。`module_import_signature.c` 在 hash/blob 校验前先比较 effect 的 `targetMetadataToken` / `targetSignatureToken` 与 provider typed export 的 `metadataToken` / `signatureToken`；两侧 token 均非零且不一致时判为 import signature mismatch。旧 patch 或 native/importer-local 只有 hash/blob 的路径仍保守回退，不因 0 token 失败。
>
> 验证：新增 `test_using_import_guard_runtime_rejects_target_token_mismatch` 只篡改 effect target metadata token，保持 target hash 与 target signature bytes 不变。RED 时旧 runtime 仍执行 provider 返回 `40`；GREEN 后 guard 进入 `else` 返回 `77`。`build-wsl-clang` 下 `zr_vm_project_import_canonicalization_test` 15/0、`zr_vm_metadata_token_model_test` 6/0、`ctest -R metadata_token_model` 1/1；`git diff --check` 无 whitespace error，仅 LF/CRLF 提示。完整跨模块 ref→def token table binding、loader 版本/hash 诊断、模块 hash golden 和 registry/DLL load+verify 仍待后续 P0。

> 进度更新（2026-06-18 03:09:16 +08:00）
>
> 状态：P0 module ABI signature hash golden 切片已完成。`SZrFunction` / `SZrIoFunction` 新增 `moduleSignatureHash`，`.zro` patch 26 在 signature blob heap 后写入/读取该字段，runtime copy 保留。`compiler_metadata_token.c` 按导出名排序 public typed exports，并以 `zr.md.mod.v1\0` 前缀、export name/access/symbol kind/export kind/readiness、typed export `signatureHash` 和 canonical signature blob bytes 计算稳定 `XXH3_64bits` module ABI hash。
>
> 验证：TDD RED 先扩展 `zr_vm_metadata_token_model_test` 断言 `moduleSignatureHash` 非零、同签名重建稳定、返回类型变化漂移、`.zro` 读回和 runtime copy 保留；构建失败于 `SZrFunction` / `SZrIoFunction` 缺少 `moduleSignatureHash` 字段。GREEN 后 `build-wsl-clang` 下 `zr_vm_metadata_token_model_test` 6/0、`zr_vm_project_import_canonicalization_test` 15/0、`ctest -R metadata_token_model` 1/1；`git diff --check` 无 whitespace error，仅 LF/CRLF 提示。完整跨模块 ref→def token table binding、loader 版本/hash 诊断和 registry/DLL load+verify 仍待后续 P0。

> 进度更新（2026-06-18 03:41:32 +08:00）
>
> 状态：P0/P2 target module ABI hash runtime 绑定切片已完成。`SZrFunctionModuleEffect` / `SZrIoFunctionModuleEffect` 和 `SZrMetadataTokenRecord` 新增 `targetModuleSignatureHash`；source/binary module-init summary 会保存 provider `moduleSignatureHash`，import effect 与 `AssemblyRef` metadata record 会携带编译期看到的 provider module ABI fingerprint。runtime verifier 在校验 member target token、target signature hash 和 target signature bytes 前，先把非零 expected target module hash 与 provider entry function `moduleSignatureHash` 对齐；guard mismatch 返回 null 进入 `else`，required mismatch 复用现有 `import signature mismatch` 诊断路径。
>
> 落盘：`.zro` patch 升到 27；writer/reader 在 module effect 和 metadata token record 中写入/读取 `targetModuleSignatureHash`，patch 26 及更早产物读取时保守置 0。runtime copy 保留 module effect 字段，metadata token record 通过 raw copy 保留该字段。当前 sidecar metadata fallback 仍可能缺失 provider module hash，此时目标 hash 为 0 并继续走既有 token/hash/blob 校验。
>
> 验证：TDD RED 先扩展 project import/runtime 和 metadata token model 覆盖，构建失败于 `SZrFunctionModuleEffect` 缺少 `targetModuleSignatureHash` 字段；GREEN 后 `build-wsl-clang` 中 `zr_vm_project_import_canonicalization_test` 17/0、`zr_vm_metadata_token_model_test` 6/0、`ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure` 1/1。完整 ref→def token binding、loader 版本/hash 诊断和 registry/DLL load+verify 仍待后续 P0。

> 进度更新（2026-06-18 04:03:33 +08:00）
>
> 状态：P0/P2 module ref table target identity fallback 切片已完成。`module_import_signature.c` 现在会先用 `moduleMetadataTokenRecords` 中匹配 module/symbol/effect kind 的 `MEMBER_REF` record 补齐缺失的 effect target identity；当 `moduleEntryEffects` 的 `targetModuleSignatureHash` / `targetMetadataToken` / `targetSignatureToken` / `targetSignatureHash` 为 0 时，runtime verifier 会使用 module ref record 的对应字段继续执行 provider module hash、typed export token/hash 和签名字节确认。这样 module ref table 不再只是 target bytes 来源，也能作为瘦身/旧 effect identity 路径的 expected binding 事实来源。
>
> 验证：新增 `test_using_import_guard_runtime_uses_module_ref_identity_when_effect_targets_are_missing` 先 RED：清空 matching effect 的 target identity，再只篡改复制出的 `moduleMetadataTokenRecords` target metadata token，旧 runtime 因 effect target hash 为 0 跳过校验并返回 provider 值 `40`。GREEN 后 runtime 从 module ref table 补齐 expected identity，guard 走 `else` 返回 `77`；`zr_vm_project_import_canonicalization_test` 19/0。完整跨模块 ref→def token binding、loader 版本/hash 诊断和 registry/DLL load+verify 仍待后续 P0。

> 进度更新（2026-06-18 04:19:39 +08:00）
>
> 状态：P0/P2 loader assembly signature mismatch 诊断切片已完成。`SZrModuleImportSignatureMismatch` 现在携带 mismatch kind；`module_import_signature.c` 把 target module ABI hash mismatch 标记为 assembly-level mismatch，`module_loader.c` 对 required `%import` 抛出 `assembly_signature_mismatch`，消息包含 canonical module/member 以及 `expectedModuleHash` / `actualModuleHash`。member token/hash/signature bytes mismatch 仍保留 `import signature mismatch`。
>
> 验证：`test_required_import_runtime_rejects_target_module_hash_mismatch` 先 RED：测试要求 `assembly_signature_mismatch` 和 module hash 字段时，旧 runtime 仍给 generic `import signature mismatch`。GREEN 后 `zr_vm_project_import_canonicalization_test` 19/0。完整 ref→def 表绑定、版本兼容诊断和 registry/DLL load+verify 仍待后续 P0。

> 进度更新（2026-06-18 05:01:39 +08:00）
>
> 状态：P0/P2 runtime 同名导出 ref→def 候选绑定切片已完成。source summary 现在保留同名函数 export，`module_init_analysis.c` 在记录 import-call effect 时会按实参类型选择匹配的 source/binary export，并把目标 metadata token、signature token、signature hash 和 module hash 写入 effect。`module_init_fill_type_ref_from_type_node()` 会把 `int` / `bool` 等 primitive annotation 规范化成无 typeName 的 `PRIMITIVE` signature 节点，避免 caller target `METHOD_SIG` 与 provider typed export `METHOD_SIG` 因 `TYPE_REF("bool")` vs `PRIMITIVE(bool)` 漂移。
>
> runtime：`module_import_signature.c` 现在会在 provider 的同名 typed exports 中按 `targetSignatureHash`、caller target signature blob 和 target token 选择候选；完整 token match 优先，blob 已确认时允许 token fallback，旧产物缺少 target bytes 时仍保留兼容回退。这样 source/binary provider 暴露 `pick(int)` 与 `pick(bool)` 时，`helper.pick(true)` 会绑定 `pick(bool)`，而不是首个同名导出。
>
> 验证：新增 `test_required_import_runtime_resolves_same_name_signature_candidate` 先 RED，旧 runtime 误取首个 `pick(int)` 并抛出 expected/actual signature mismatch；GREEN 后返回 42。`build-wsl-clang` 中 `zr_vm_project_import_canonicalization_test` 20/0、`zr_vm_metadata_token_model_test` 6/0、`ctest --test-dir build-wsl-clang -R metadata_token_model --output-on-failure` 1/1。完整跨模块 ref→def token 表持久绑定、loader version compatibility 诊断和 registry/DLL load+verify 仍待后续 P0。

> 进度更新（2026-06-18 06:11:16 +08:00）
>
> 状态：P0/P2 runtime ref→def binding result sidecar 切片已完成。`SZrMetadataTokenBinding` 新增为运行期绑定结果结构，`SZrFunction.moduleMetadataBindings` 保存 caller `MEMBER_REF` 到 provider `MEMBER_DEF` / `SIGNATURE` 的成功解析结果；该 sidecar 保留原始 ref token，并同时记录 expected 与 resolved metadata token、signature token、signature hash 和 module ABI hash。`module_import_signature.c` 在 target module hash、target token、target signature hash 和 target signature bytes 全部验证成功后写入或更新 binding；legacy/current effect 校验路径和 direct `moduleMetadataTokenRecords` 解码路径共用这套记录逻辑，因此即使 `moduleEntryEffects` 缺失，module ref table 直连校验成功后也能留下可查询绑定结果。
>
> 同步修复：source module summary identity 会在当前 source function 的 metadata tokens 构建完成后刷新为最终 typed export token/hash/value type/参数类型。这样 import ref 的 `targetMetadataToken` / `targetSignatureHash` 与 appended target `METHOD_SIG` / `FIELD_SIG` bytes 来自同一份最终 identity，避免正常 guard 因 summary stale 而误走 `else`。
>
> 验证：新增 `test_using_import_guard_runtime_records_module_ref_binding_result` 先 RED，缺少 `SZrMetadataTokenBinding` / `moduleMetadataBindings` 字段时 `zr_vm_project_import_canonicalization_test` 编译失败；GREEN 后 guard 正常返回 provider 值 `40`，binding sidecar 记录原始 `MEMBER_REF` token 并解析到 provider `MEMBER_DEF`。`build-wsl-clang` 中 `zr_vm_project_import_canonicalization_test` 22/0、`zr_vm_metadata_token_model_test` 6/0、metadata CTest 1/1。后续仍待把 binding result 持久化或暴露给 loader diagnostics、loader version compatibility 诊断和 registry/DLL load+verify。

> 进度更新（2026-06-18 17:36:50 +08:00）
>
> 状态：P0 AssemblyRef→MODULE runtime binding sidecar 已完成。新增 `module_import_signature_binding.c/.h` 承接成功 verifier 后的 binding sidecar 写入，避免继续扩大 `module_import_signature.c`。`module_import_signature.c` 在 import verifier 成功校验 AssemblyRef version range、target module hash、member token/hash 和 target signature bytes，并写入 `MEMBER_REF -> MEMBER_DEF` binding 后，会沿 caller module ref owner-chain 定位对应 `ASSEMBLY_REF`，再写入 `ASSEMBLY_REF -> MODULE` binding。该 binding 保留 caller `AssemblyRef` token、paired `SIGNATURE` token/hash、expected module ABI hash，并把 resolved metadata token 记录为虚拟 `ZR_METADATA_TABLE_MODULE` RID 1，resolved module hash 记录为 provider entry function `moduleSignatureHash`；resolved signature token/hash 保持 0，因为当前还没有落盘 provider `MODULE` signature record。该切片不新增 `.zro` patch，沿用 patch 28/32 的 binding list 持久化字段。
>
> 验证：扩展 `test_using_import_guard_runtime_records_module_ref_binding_result` 先 RED 于 `ZrCore_Function_FindModuleMetadataBinding(function, assemblyRefToken)` 返回 null；GREEN 后同一用例断言 `ASSEMBLY_REF` binding 的 ref/expected signature identity、expected module hash、resolved `MODULE` token 和 resolved module hash。WSL GCC `zr_vm_project_import_canonicalization_test` 31/0、`zr_vm_metadata_token_model_test` 21/0；WSL clang 同组 31/0、21/0；MSVC Debug 构建/链接通过且 `zr_vm_metadata_token_model_test` 21/0，project-import 全套仍有既有 Windows `test_required_import_runtime_reports_source_loader_attempts` 诊断文本断言失败，但本切片新增 AssemblyRef binding 用例 PASS。备注：本切片只补链首 module binding 事实；当前 import `TYPE_REF` 在 module-member ref 链中仍代表 symbol owner 占位，尚不声明为可稳定绑定到 provider `TYPE_DEF`。文件卫生：`module_import_signature.c` 从 1397 行降到 1252 行，新增 `module_import_signature_binding.c` 为 151 行；若继续扩 verifier，下一步应优先抽 direct module-ref scanning 或 provider candidate resolution。
>
> 后续收敛：2026-06-18 18:28:42 切片已补真实 provider `MODULE` record 和 paired `SIGNATURE`，AssemblyRef binding 在 provider record 可用时不再保持 resolved signature token/hash 为 0。

> 进度更新（2026-06-18 18:28:42 +08:00）
>
> 状态：P0 provider `MODULE` record 与 AssemblyRef resolved signature 已完成。新增 `compiler_metadata_module_record.c/.h`，metadata token builder 现在为 provider 写入真实 `MODULE` RID 1 记录及 paired `SIGNATURE` record；signature blob 使用新增 `ZR_METADATA_SIGNATURE_NODE_MODULE`，编码为 heap-indexed `MODULE <entryName> <moduleVersion>`。该签名不包含 `moduleSignatureHash`，避免把 module hash 写回自身签名造成递归身份。`module_import_signature_binding.c` 在 provider token stream 存在 `MODULE` record 时，把 caller `ASSEMBLY_REF -> MODULE` binding 的 resolved signature token/hash 解析为该 provider `MODULE` 的 paired `SIGNATURE`；旧 provider 没有 MODULE record 时保持 resolved signature token/hash 为 0 的兼容行为。
>
> 验证：TDD RED 先扩展 `test_metadata_tokens_and_signature_blob_roundtrip_through_binary_and_runtime`，要求 provider token records 中存在 `MODULE` record、paired `SIGNATURE`、`MODULE` signature node、entry name 和 module version string refs；旧代码构建失败于缺少 `ZR_METADATA_SIGNATURE_NODE_MODULE`。同轮扩展 `test_using_import_guard_runtime_records_module_ref_binding_result` 要求 AssemblyRef binding 的 resolved signature token 属于 `SIGNATURE` 且 hash 非零。GREEN 后 WSL GCC `zr_vm_metadata_token_model_test` 21/0、`zr_vm_project_import_canonicalization_test` 31/0；WSL clang 同组 21/0、31/0。MSVC 聚焦构建被无关 `zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c` 中 `execution_inline_frame_copy_slot_to_nested_field` 未声明/后续重定义错误阻塞，未执行到本切片测试。备注：不新增 `.zro` patch；现有 metadata token records、signature blob heap 与 patch 30 string heap 已能持久化该 MODULE 记录。`compiler_metadata_token.c` 当前仍约 1563 行，本切片将 provider MODULE 记录职责隔离在 131 行 helper 中；后续继续扩 metadata fixture 前应优先拆测试 helper。

> 进度更新（2026-06-18 19:02:36 +08:00）
>
> 状态：P0 TypeRef→TypeDef consumer-side binding 已完成。`zr_vm_core/src/zr_vm_core/function_metadata_binding.c` 新增 `ZrCore_Function_BindMatchingTypeRefMetadata()`，在 caller function 中查找 `targetMetadataToken` 指向 `ZR_METADATA_TABLE_TYPE_DEF` 的 `TYPE_REF` records，并按 provider `TYPE_DEF` token、paired `SIGNATURE` token/hash、target module hash、record-level layout identity 以及 TypeRef/TypeDef base name 做严格匹配；成功后把 caller `TYPE_REF`、caller paired `SIGNATURE`、expected provider TypeDef/signature/module/layout identity 和 resolved provider TypeDef/signature/module/layout identity 写入 `SZrFunction.moduleMetadataBindings`。`module_import_signature_binding.c` 在 import verifier 成功后 best-effort 调用该 helper，保持当前 runtime import 语义不因尚未生产的 TypeRef sidecar 而改变。
>
> 验证：新增 `tests/module/test_metadata_type_ref_binding.c` 和 CMake/CTest target `zr_vm_metadata_type_ref_binding_test`。TDD RED 先失败于缺少 `ZrCore_Function_BindMatchingTypeRefMetadata()`；GREEN 后 WSL GCC focused build/direct run 通过 metadata TypeRef binding 1/0、metadata token 21/0、project import 31/0，WSL clang 同组通过 1/0、21/0、31/0，MSVC Debug 新测试构建并运行通过 1/0。备注：不新增 `.zro` patch，复用 patch 28/32 binding sidecar；当前编译器生成的 import `TYPE_REF` owner-chain 仍主要代表 module-member owner 占位，不声明为 provider `TYPE_DEF` binding。后续若要完成稳定跨模块 type ref 生产，需要从 target `METHOD_SIG` / `FIELD_SIG` TypeSig 或显式 import type reference 中提取真实 provider type identity。

> 进度更新（2026-06-18 20:11:43 +08:00）
>
> 状态：P0 producer-side stable provider `TYPE_REF` 生成已完成。`compiler_metadata_type_ref.c/.h` 新增独立 plan/emit helper，metadata token builder 会在既有 import owner-chain `AssemblyRef <- TypeRef(symbol owner) <- MemberRef` 之外，从 import effect 已解析的 target `METHOD_SIG` / `FIELD_SIG` 类型信息中扫描返回值和参数类型。若类型名可在 provider module summary 的 `typeDefs` 中解析到 provider `TYPE_DEF`，builder 会追加新的稳定 `TYPE_REF` + paired `SIGNATURE` records，并把 record target identity 填为 provider `TYPE_DEF` token、paired `SIGNATURE` token/hash、provider module ABI hash、layoutVersion/layoutHash。该稳定 TypeRef 的 owner 是 provider `ASSEMBLY_REF`，不会复用 module-member owner placeholder。
>
> Summary：`SZrParserModuleInitSummary` 新增 `typeDefs`，`module_init_analysis.c` 在 source module finalize 后从最终 metadata token records 和 shared string heap 中提取 `TYPE_DEF` token、paired signature、signature hash 与 layout identity，刷新 provider summary，供 caller token builder 查询。旧 summary 或 provider 无 TypeDef 时 producer helper no-op，保持旧 artifact/旧 import 路径兼容。
>
> 验证：新增 producer 用例 `test_import_target_signature_emits_stable_provider_type_ref` 先 RED 于找不到 caller stable TypeRef；GREEN 后同一用例确认 caller 额外 `TYPE_REF` 指向 provider `TYPE_DEF`，target signature/module/layout identity 全部匹配，并能被 `ZrCore_Function_BindMatchingTypeRefMetadata()` 绑定。WSL GCC 新 build direct run：`zr_vm_metadata_type_ref_binding_test` 2/0、`zr_vm_metadata_token_model_test` 21/0、`zr_vm_project_import_canonicalization_test` 31/0；WSL clang 独立 build 同组 2/0、21/0、31/0；MSVC Debug 新 build `zr_vm_metadata_type_ref_binding_test` 2/0。不新增 `.zro` patch，复用现有 record target/layout 字段和 patch 28/32 binding sidecar。

> 进度更新（2026-06-18 20:44:02 +08:00）
>
> 状态：P0 nested generic provider `TYPE_DEF` / `TYPE_REF` extraction 已完成。前一阶段 stable TypeRef producer 覆盖 target signature 的 top-level return/parameter 类型；本阶段补齐递归扫描，使 `Box<Option<int>>` 这类外层类型未必是 provider TypeDef、但内层泛型实参是 provider union TypeDef 的签名也能产出稳定 identity。
>
> 实现：`compiler_metadata_type_def.c` 的 exported signature TypeDef 扫描现在递归进入 array element 与 generic argument type names，并同步把 `metadata_type_def_max_candidate_count()` 改为按递归类型形状估算候选容量，避免 nested TypeDef 被发现时 unique table 容量不足。`compiler_metadata_type_ref.c` 的 provider TypeRef producer 同样递归扫描 target `METHOD_SIG` / `FIELD_SIG` return/parameter 的泛型实参并去重；既有 import owner-chain placeholder `TYPE_REF` 仍保留为 module-member owner，不复用为 provider `TYPE_DEF` binding。
>
> 验证：新增 `test_nested_generic_import_target_signature_emits_provider_type_ref` 先 RED 于 provider `Box<Option<int>>` export signature 没有生成嵌套 `Option` 的 `TYPE_DEF`，caller 也没有 stable provider TypeRef。GREEN 后 WSL GCC `zr_vm_metadata_type_ref_binding_test` 3/0、`zr_vm_metadata_token_model_test` 21/0、`zr_vm_project_import_canonicalization_test` 31/0；WSL clang `zr_vm_metadata_type_ref_binding_test` 3/0；MSVC Debug `zr_vm_metadata_type_ref_binding_test` 3/0。不新增 `.zro` patch，复用现有 target/layout record fields 和 patch 28/32 binding sidecar。备注：当时剩余的显式 TypeRef surface 后续已由 2026-06-18 22:35:57 的 module-qualified typed annotation 和 2026-06-18 23:07:30 的 destructured/unqualified alias mapping 覆盖。

> 进度更新（2026-06-18 21:22:00 +08:00）
>
> 状态：P0 TypeRef binding mismatch status / loader diagnostic 已完成。`SZrMetadataTypeRefBindStatus` 与 `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus()` 现在给 stable provider `TYPE_REF -> TYPE_DEF` binding 暴露 caller/matched/unmatched、definition mismatch、layout mismatch 计数，以及首个 unmatched/definition/layout mismatch 的 expected/actual token、signature hash、layout version/hash context；发生 mismatch 时不会留下 partial `moduleMetadataBindings`。`module_import_signature_binding.c` 在成功 import verification 后调用 status helper，若 stable TypeRef binding 存在 unmatched、definition 或 layout drift，会把 `type_ref_mismatch` 写入 core module-load diagnostic channel，import 本身继续保持 best-effort 成功语义。
>
> 验证：新增 `test_type_ref_binding_reports_layout_mismatch_without_partial_binding` 先 RED 于缺 status API；新增 `test_type_ref_binding_mismatch_records_loader_diagnostic` 先 RED 于诊断为空。GREEN 后 WSL GCC/clang focused build/direct run 均通过 `zr_vm_metadata_type_ref_binding_test` 5/0、`zr_vm_metadata_token_model_test` 21/0、`zr_vm_project_import_canonicalization_test` 31/0；MSVC Debug `zr_vm_metadata_type_ref_binding_test` 5/0。不新增 `.zro` patch，复用既有 metadata record target/layout fields 与 patch 28/32 binding sidecar。

> 进度更新（2026-06-18 22:35:57 +08:00）
>
> 状态：P0 module-qualified explicit TypeRef annotation stable surface 已完成。`compiler_metadata_type_ref.c/.h` 现在可以拆分顶层 module-qualified type name，并把函数 `typedLocalBindings` 中的 `provider.Option<int>` 作为显式 provider type ref 输入；扫描逻辑递归进入 array/generic argument type names，解析 provider summary `TYPE_DEF` 后写入 stable provider `TYPE_REF` + paired `SIGNATURE`，target fields 保存 provider TypeDef token、paired signature token/hash、provider module ABI hash、layoutVersion/layoutHash。
>
> 实现：`compiler_metadata_token.c` 会从 typed local annotation 收集 explicit type modules，补 metadata string heap，并在没有 import member effect / `moduleEntryEffects` 为空时仍生成 explicit `ASSEMBLY_REF`，再通过 AssemblyRef RID resolver 让 `compiler_metadata_type_ref_emit()` 把 explicit TypeRef owner 指向正确 AssemblyRef。原有 import owner-chain placeholder 不变，显式 module-qualified typed annotation 走独立 stable TypeRef 记录；不新增 `.zro` patch，复用现有 metadata records、string heap、target/layout fields 和 patch 28/32 binding sidecar。
>
> 验证：新增 `test_explicit_import_type_annotation_emits_provider_type_ref` 先 RED 于本地 `provider.Option<int>` typed annotation 没有 provider-target `TYPE_REF`；去掉 import member effect 后又确认旧早退路径无法产生 explicit AssemblyRef。GREEN 后 WSL GCC/clang `zr_vm_metadata_type_ref_binding_test` 6/0、`zr_vm_metadata_token_model_test` 21/0、`zr_vm_project_import_canonicalization_test` 31/0；MSVC Debug `zr_vm_metadata_type_ref_binding_test` 6/0。当时剩余的 destructured/unqualified imported type alias source mapping 后续已在 2026-06-18 23:07:30 +08:00 完成。

> 进度更新（2026-06-18 23:07:30 +08:00）
>
> 状态：P0 destructured/unqualified imported type alias TypeRef mapping 已完成。`compiler_metadata_type_ref.c/.h` 新增 unqualified alias resolver：对 `Option<int>` 先提取 unqualified generic base `Option`，从编译期 `typeValueAliases` 读取 destructuring import 已登记的 `Option -> provider.Option`，再恢复为 provider module + member type `Option<int>`。外层不是 provider TypeDef 的形态也会继续递归扫描泛型实参，因此 `Box<Option<int>>` 能生成嵌套 provider `Option` 的 stable TypeRef。
>
> 实现：TypeRef producer 在 module-qualified split 失败且没有 import effect module context 时会尝试 alias resolver；token builder 的 metadata string heap 收集和 explicit AssemblyRef module 收集也复用同一 resolver，确保 alias typed local 在没有 import member effect 时仍能写入 `ASSEMBLY_REF` owner。该切片不新增 `.zro` patch，也不扩展 runtime typed local binding schema；alias source 继续来自现有编译期 `typeValueAliases`。
>
> 验证：新增 `test_unqualified_import_type_alias_annotation_emits_provider_type_ref` 先 RED 于 typed local `Option<int>` 找不到 provider-target `TYPE_REF`；新增 `test_nested_unqualified_import_type_alias_annotation_emits_provider_type_ref` 覆盖 `Box<Option<int>>`。GREEN 后 WSL GCC/clang `zr_vm_metadata_type_ref_binding_test` 8/0、`zr_vm_metadata_token_model_test` 21/0、`zr_vm_project_import_canonicalization_test` 31/0；MSVC Debug `zr_vm_metadata_type_ref_binding_test` 8/0。

> 进度更新（2026-06-18 06:39:26 +08:00）
>
> 状态：P0/P2 required import target token mismatch diagnostics 切片已完成。`SZrModuleImportSignatureMismatch` 现在保存 member-level target token mismatch 的 expected/actual metadata token 与 signature token；`module_import_signature.c` 在 effect/provider 双方 token 均存在且不一致时填充这些字段，`module_loader.c` 在 required import 的 `import signature mismatch` 消息中追加 token 字段。该切片不改变 `.zro` schema，也不持久化 binding result，只把已有 runtime token guard 的诊断从 hash-only 提升为 token-aware。
>
> 验证：新增 `test_required_import_runtime_reports_target_token_mismatch` 先 RED，旧 required import 只报告 `expectedHash` / `actualHash`，缺少 `expectedMetadataToken` / `actualMetadataToken`；GREEN 后同一用例同时篡改 target metadata token 和 target signature token，并断言 message 含 `expectedMetadataToken` / `actualMetadataToken` / `expectedSignatureToken` / `actualSignatureToken`。`build-wsl-clang` 中 `zr_vm_project_import_canonicalization_test` 23/0；相邻 metadata 回归仍为 `zr_vm_metadata_token_model_test` 6/0、metadata CTest 1/1。备注：当时仍待的 binding result 持久化、binding/query 暴露、loader version compatibility 与 registry/DLL load/verify 后续已分别由 06:59、07:15、07:34、08:13、09:37、12:49、13:33 等切片覆盖到当前计划边界。

> 进度更新（2026-06-18 06:59:44 +08:00）
>
> 状态：P0/P2 runtime binding query exposure 切片已完成。`function.h` / `function.c` 新增只读 API `ZrCore_Function_FindModuleMetadataBinding(function, refToken)`，按 caller `MEMBER_REF` token 查询运行期 `moduleMetadataBindings` sidecar，空 function、空 token 或未绑定时返回 `ZR_NULL`。这把 ref→def binding result 从测试私有扫描提升为 core 查询面，同时不改变 verifier 写入时机，也不改变 `.zro` schema。
>
> 验证：现有 `test_using_import_guard_runtime_records_module_ref_binding_result` 先改为调用该 API；RED 时 `zr_vm_project_import_canonicalization_test` 链接失败于 undefined reference `ZrCore_Function_FindModuleMetadataBinding`。GREEN 后同一用例断言 ref token、ref signature token/hash、resolved metadata/signature token/hash 和 resolved module hash；`build-wsl-clang` 中 `zr_vm_project_import_canonicalization_test` 23/0，`zr_vm_metadata_token_model_test` 6/0，metadata CTest 1/1。备注：当时仍待的 binding result `.zro` 持久化、loader version compatibility 诊断和 registry/DLL load/verify 后续已完成到当前 metadata/token 计划边界。

> 进度更新（2026-06-18 07:15:51 +08:00）
>
> 状态：P0 runtime ref→def binding result `.zro` 持久化切片已完成。`SZrIoFunction` 现在持有 `moduleMetadataBindingLength` / `moduleMetadataBindings`；`.zro` patch 28 在 metadata token model 和 module ref table 后追加 binding list，逐字段写入 `SZrMetadataTokenBinding` 的 caller ref、expected identity 和 resolved provider identity。reader 对 patch 27 及更早产物保持空 binding list；`io_runtime.c` 加载时复制 binding 数组到 `SZrFunction.moduleMetadataBindings` 并设置 capacity，继续由 `ZrCore_Function_FindModuleMetadataBinding()` 查询。
>
> 验证：新增 `test_module_metadata_binding_roundtrips_through_binary_and_runtime` 先 RED，`SZrIoFunction` 缺少 `moduleMetadataBindingLength` / `moduleMetadataBindings` 导致 metadata token model 测试编译失败；GREEN 后同一测试手工填充一条 binding，确认 `.zro` IO reader 和 runtime load 都保留 ref/resolved token/hash/module-hash 字段。`build-wsl-clang` 中 `zr_vm_metadata_token_model_test` 7/0，metadata CTest 1/1；相邻 `zr_vm_project_import_canonicalization_test` 23/0。备注：当时仍待的 loader version compatibility 诊断和 registry/DLL load/verify 后续已完成到当前 metadata/token 计划边界。

> 进度更新（2026-06-18 07:34:01 +08:00）
>
> 状态：P0 loader future patch compatibility diagnostic 切片已完成。`zr_io_conf.h` 新增 `ZR_IO_SOURCE_PATCH_CURRENT`，writer 和 reader 共用当前 `.zro` patch 常量。`ZrCore_Io_ReadSourceNew()` 在读取 header patch 后、读取任何 patch 相关 payload 前，拒绝高于当前 runtime 支持值的未来 `.zro`，并通过 `io source version patch is newer than this runtime` 报告 `actualPatch` / `supportedPatch`。旧 patch 读取仍沿既有分支兼容置 0 或空表。
>
> 验证：新增 `test_reader_rejects_future_metadata_patch_with_version_diagnostic` 先 RED，篡改 metadata token fixture 的 header patch 到 current+1 时旧 reader 没有报错；GREEN 后同一用例捕获 runtime error 并断言 future/supported patch 诊断。`build-wsl-clang` 中 `zr_vm_metadata_token_model_test` 8/0、metadata CTest 1/1，`zr_vm_project_import_canonicalization_test` 23/0。后续版本约束已在 08:13:03 默认 semver range 切片完成；剩余 registry/DLL load+verify 和 `PluginLoad.Available`。

## 1. 为什么需要 token

现状（[00-current-state.md](00-current-state.md) §5）：跨模块引用靠 `module.symbol` 字符串 + `stackSlot/callableChildIndex`。问题：
- 名字字符串无法稳定表达"泛型闭型 + 所有权种类 + union variant"的复合身份。
- slot 是物理布局，插件重编译后漂移，无法用于跨 DLL 稳定引用。
- 插件守卫需要一个**稳定、可比对的身份**来判断"加载到的类型是不是我编译时引用的那个类型"。

C# 的解法：每个 metadata 实体有 token = `表标签(1B) | RID(3B)`；跨程序集引用经 `AssemblyRef → TypeRef → MemberRef`，最终落到结构化**签名 blob**。我们照搬这套分层。

## 2. Token 模型

### 2.1 Token 编码

```
TZrMetadataToken = (table_tag << 24) | rid        // 32-bit，对齐 C# 习惯
```

表标签（首批必需）：

| 标签 | 表 | 内容 |
| --- | --- | --- |
| `MODULE` | Module | 本模块定义头（名字、版本、公钥/哈希） |
| `TYPE_DEF` | TypeDef | 本模块定义的 struct/class/interface/enum/**union** |
| `MEMBER_DEF` | MemberDef | 本模块定义的字段/方法/属性/variant |
| `ASSEMBLY_REF` | AssemblyRef | 引用的外部模块/DLL（名字 + 版本约束 + 签名哈希） |
| `TYPE_REF` | TypeRef | 引用的外部类型（→ AssemblyRef + 名字 + 签名） |
| `MEMBER_REF` | MemberRef | 引用的外部成员（→ TypeRef + 签名） |
| `TYPE_SPEC` | TypeSpec | 实例化签名（泛型闭型 / 所有权泛型 / 数组 / union） |
| `SIGNATURE` | Signature blob | 结构化签名字节串（见 §3） |

`TZrTypeId/TZrSymbolId` 等编译期 ID（`semantic_facts.h`）保持不变——它们是**编译期内部**身份；token 是**落盘 + 跨模块**身份。编译末期建立 `编译期 ID → token` 的映射，写入 `.zro`。

### 2.2 落盘位置

在 [io.h](../../../zr_vm_core/include/zr_vm_core/io.h) 的 `.zro` schema 中新增 metadata 区：
- 已落地：函数级 `metadataTokenRecords`，记录 token、关联 token、owner export index、签名 blob 范围。
- 已落地：函数级 `signatureBlobHeap`，保存 typed export 生成的紧凑签名字节堆。
- 已落地：函数级 import member effect 的 `ASSEMBLY_REF` / `TYPE_REF` / `MEMBER_REF` + `SIGNATURE` 前置记录，保存 guarded body/callable summary 中 canonical module member 引用的最小签名事实。
- 已落地：函数级 typed export symbol 与 metadata token record 的 `signatureHash`，使用 `zr.md.sig.v1\0` 前缀 + signature blob 的稳定 `XXH3_64bits`，`.zro` patch 21 持久化。
- 已落地：函数级 metadata token record 的 `ownerToken`，把 import ref 串成 `AssemblyRef <- TypeRef <- MemberRef`，并让配对 `SIGNATURE` 指回被签名实体；`.zro` patch 22 持久化，patch 20/21 读取时置 0。
- 已落地：函数级 import effect 和 metadata token record 的目标签名身份字段，包含 `targetMetadataToken`、`targetSignatureToken`、`targetSignatureHash`；module effect 目标身份在 `.zro` patch 23 持久化，metadata record 的 target token 字段在 patch 24 持久化，旧 patch 读取时置 0。
- 已落地：entry-function 级 `moduleMetadataTokenRecords` import ref 聚合表，复用函数级 `signatureBlobHeap` 并按 table/owner/hash/target identity/signature bytes 去重 `AssemblyRef` / `TypeRef` / `MemberRef`；`.zro` patch 25 持久化，旧 patch 读取时为空。
- 已落地：native/source/binary 三类 import 的 compile-time member view 均可携带 metadata/signature identity；source/binary import 会保留同名函数候选，member call 通过签名候选选择正确 overload；native view 使用 importer 内部 synthetic token/hash，不改变 native descriptor ABI 或 `.zro` schema。
- 已落地：exported callable summary 中的 guarded import effects 会在 `callableChildIndex` 解析后复制到对应 child function 的 `moduleEntryEffects`，让 guard runtime 在没有 module entry caller frame 时仍能校验当前 callable。
- 已落地：runtime import signature verifier 优先从 `moduleMetadataTokenRecords` 截取 caller 期望 target signature bytes，再兼容回退函数级 `metadataTokenRecords`；required import 和 guard import 继续共用该 verifier，required mismatch 抛出诊断，guard mismatch 走 `else`。
- 已落地：runtime verifier 会把非零的 effect target metadata/signature token 与 provider typed export metadata/signature token 做绑定校验；两侧 token 不一致时拒绝 import，0 token 仍兼容旧产物和 hash/blob-only fallback。
- 已落地：函数级 `moduleSignatureHash`，当前由 `compiler_metadata_module_hash.c` 以 `zr.md.mod.v2\0` 计算，输入为 module version、public typed export 的 canonical signature hash/blob 与导出身份，以及本模块 `TYPE_DEF` / `TYPE_SPEC` 实体 record 的 signature/layout identity；`.zro` patch 26 持久化，旧 patch 读取时为 0。
- 已落地：import effect 和 `AssemblyRef` metadata record 的 `targetModuleSignatureHash`，把编译期 provider `moduleSignatureHash` 带到 runtime；`.zro` patch 27 持久化，旧 patch 读取时为 0，runtime verifier 仅在 expected hash 非零时校验。
- 已落地：runtime verifier 可从 `moduleMetadataTokenRecords` 的匹配 `MEMBER_REF` record 补齐缺失的 effect target identity，再执行 target module hash、token、signature hash 和 signature bytes 校验。
- 已落地：required import 的 target module ABI hash mismatch 会报告 `assembly_signature_mismatch`，并带 `expectedModuleHash` / `actualModuleHash`；普通 member-level token/hash/bytes mismatch 继续报告 `import signature mismatch`，其中 target token mismatch 会附带 `expectedMetadataToken` / `actualMetadataToken` 和/或 `expectedSignatureToken` / `actualSignatureToken`。
- 已落地：runtime verifier 会在 provider 同名 typed exports 中按 target hash、target signature blob 和 target token 选择匹配候选；source summary type refs 对 primitive annotation 使用 `PRIMITIVE` signature 节点，保证 caller target signature 与 provider typed export signature 使用同一 canonical identity。
- 已落地：runtime ref→def binding result sidecar，成功校验后在 `SZrFunction.moduleMetadataBindings` 中记录 caller `MEMBER_REF` 到 provider `MEMBER_DEF` / `SIGNATURE` 的 expected/resolved token/hash/module-hash 绑定结果；`ZrCore_Function_FindModuleMetadataBinding()` 已按 caller ref token 暴露只读查询 API；`.zro` patch 28 持久化 binding list，旧 patch 读取时为空；future `.zro` patch 会在 IO header 阶段以 `actualPatch` / `supportedPatch` 诊断拒绝。
- 已落地：provider `moduleVersion` 与 AssemblyRef semver range，`.zro` patch 29 持久化 version fields，runtime required import 版本不兼容抛 `assembly_version_mismatch`，guard import 版本不兼容进入 `else`。
- 已落地：dependency manifest 显式 range surface。依赖声明对象的 `minVersionInclusive` / `maxVersionExclusive` 会覆盖默认 `[compiledVersion,nextMajor)`，并写入 import effect 与 `AssemblyRef` record。
- 已落地：函数级 `metadataStringHeap`，`.zro` patch 30 在 `signatureBlobHeap` 后写入 string index + value 表；签名 blob 中的字符串引用写固定 `u32` index，runtime verifier 支持 heap-indexed decode 并兼容旧 inline 字符串。
- 已落地：provider `MODULE` record baseline。`compiler_metadata_module_record.c/.h` 写入真实 `MODULE` RID 1 与 paired `SIGNATURE`，signature blob 为 `MODULE <entryName> <moduleVersion>` 且通过 patch 30 string heap 引用字符串；AssemblyRef runtime binding 在 provider record 可用时解析到该 paired signature token/hash。
- 已落地：本地 union `TYPE_DEF` baseline、variant/field 契约和 layout identity。`compiler_metadata_type_def.c/.h` 从导出 typed signature 中识别当前脚本 union 定义，写入 `TYPE_DEF` + paired `SIGNATURE` record；TypeDef signature 当前包含 shared string heap name index、generic arity、variant name/kind/default flag/field count 与 payload field name/passing mode/TypeSig。`compiler_metadata_type_def_layout.c/.h` 单独计算 `layoutVersion` / `layoutHash`，并把物理布局身份写入 metadata token record，而不是 signature blob。
- 已落地：TypeRef→TypeDef consumer-side binding surface、mismatch status、provider-summary backed producer、module-qualified explicit typed annotation surface，以及 destructured/unqualified imported type alias source mapping。`ZrCore_Function_BindMatchingTypeRefMetadata()` 会绑定已经携带 stable provider TypeDef target identity 的 caller `TYPE_REF` records；`ZrCore_Function_BindMatchingTypeRefMetadataWithStatus()` 会报告 unmatched、definition mismatch 和 layout mismatch，并在 mismatch 时拒绝 partial binding。runtime import verifier 成功后 opportunistically 调用该 status helper，稳定 TypeRef drift 会通过 `type_ref_mismatch` module-load diagnostic 暴露。现有 import owner-chain 中的 `TYPE_REF` 仍主要是 module-member owner 占位；provider summary backed target signature 的 stable provider TypeRef producer 已完成，覆盖 return/parameter 及嵌套 generic argument 里的 provider TypeDef；typed local annotation `provider.Option<int>` 和通过 `typeValueAliases` 映射到 `provider.Option` 的 `Option<int>` / `Box<Option<int>>` 都会在无 import effect 时产生 explicit `ASSEMBLY_REF` 与 stable provider `TYPE_REF`。
- 已落地/复核：共享 string heap 索引化已同步 project-import metadata blob 验证读取，`metadataStringHeap` 索引和旧 inline string fallback 都有 helper 覆盖；descriptor DLL safe unload/cache invalidation baseline、loader-facing TypeSpec mismatch diagnostics、loader-facing TypeRef mismatch diagnostics、AOT descriptor 更细 loader diagnostics、本地 union TypeDef variant/field 契约、TypeDef layout identity、union TypeSpec→TypeDef definition/layout binding、TypeRef→TypeDef consumer-side binding/status、producer-side stable provider TypeRef extraction（含嵌套泛型实参）、module-qualified explicit typed local annotation surface，以及 destructured/unqualified imported type alias mapping 已完成。后续：更完整 cross-region/async escape、完整 load+verify 收尾和 owner payload move 语义。

现有 `SZrIoFunctionTypedExportSymbol`（已含结构化签名）成为 `MEMBER_DEF` + 其 `SIGNATURE` 的数据来源——**不推翻已有 typed metadata，而是在其上锚定 token**。

## 3. 签名 blob 编码（DLL 间访问的契约）

签名是一个紧凑的 TLV 字节串，递归编码类型。这是"通过签名访问"的核心——两个 DLL 只要签名 blob 字节一致，就认定为同一契约。

```
Sig        := TypeSig
TypeSig    := PRIM <prim_code>                       // int/float/bool/string/...
            | TYPE_REF <token:TypeRef|TypeDef>       // 具名类型，按 token 解析
            | ARRAY <TypeSig> <rank> <constraints>   // T[]、T[N]、T[a..b]
            | TUPLE <count> <TypeSig>*
            | FUNC <retTypeSig> <paramCount> <TypeSig>*
            | GENERIC_INST <token:open> <argCount> <TypeSig>*   // Map<string,int>
            | OWNERSHIP <kind:Unique|Shared|Weak|Borrow|Loan> <TypeSig>  // 所有权泛型
            | UNION <token:TypeDef> <argCount> <TypeSig>*       // union 闭型
            | NULLABLE <TypeSig>                                // T?
AssemblyRefSig := ASSEMBLY_REF <moduleName>
ImportTypeRefSig := TYPE_REF <baseType=object> <symbolName>
MemberRefSig := MEMBER_REF <moduleName> <symbolName> <effectKind> (<MethodSig|FieldSig>)?
MethodSig  := <callConv> <genericArity> <retTypeSig> <paramCount> (<paramMode> <TypeSig>)*
```

要点：
- **所有权种类进入签名**（`OWNERSHIP` 节点）。因此 `Unique<Buffer>` 与 `Shared<Buffer>` 是**不同**签名——所有权是类型身份的一部分，[01 篇](01-ownership-as-generics.md) 的种类传播在跨模块边界也成立。
- **泛型闭型规范化**：`GENERIC_INST` 的实参递归编码；整数 const 泛型按编译期归约值编码（`Matrix<int,4>`），与 [00 §3](00-current-state.md) 的闭型身份规则一致。
- **union 闭型**（`UNION`）与泛型同构编码，supports `Option<T>` 这类泛型 union。

## 4. 跨 DLL 按签名访问的解析流程

```
模块 A 编译期引用 B.Device：
  1. A 生成 TypeRef(B.Device) → AssemblyRef(B, 版本约束, 期望签名哈希)
  2. A 把对 Device 成员的调用记为 MemberRef(签名 blob)
  3. 这些 ref + 期望签名哈希写入 A.zro

运行期 / 加载期解析（含插件守卫场景）：
  1. 加载 B（.zro 或 DLL 描述符插件）
  2. 用 AssemblyRef 的版本约束 + 签名哈希校验 B 的 Module 表
  3. 对每个 TypeRef：在 B 的 TypeDef 表按名字 + 签名 blob 匹配
        签名 blob 字节一致 → 绑定 token，建立 ref→def 解析
        不一致              → 解析失败（插件守卫据此走 else / 降级）
  4. MemberRef 同理按 MethodSig/FieldSig 比对
```

当前已落地的是函数级前置事实、provider `MODULE` record、entry-function 级 module ref 聚合表、调用级/加载期消费与运行期 binding result sidecar：guard/callable summary 中的 import member effect 会先生成 `AssemblyRef <- TypeRef <- MemberRef` owner-chain、配对签名/hash 和目标签名身份；provider token stream 现在也包含真实 `MODULE` RID 1 和 paired `SIGNATURE`，其 signature blob 记录 entry name 与 module version。可解析 provider export 时，`MemberRef` blob 会追加 `MethodSig` / `FieldSig` 子签名，`MEMBER_REF` record 及其配对 `SIGNATURE` record 会保留 provider `targetMetadataToken`、`targetSignatureToken` 和 `targetSignatureHash`。随后 `compiler_metadata_ref.c` 会把函数级 import refs 聚合到 `moduleMetadataTokenRecords`，重复 entry/callable effects 只保留一组实体 ref + signature record。runtime verifier 已经消费这张表来截取 caller 期望的 target `METHOD_SIG` / `FIELD_SIG` bytes，并在表不存在或旧 patch 没有记录时回退函数级 `metadataTokenRecords`；同时 verifier 也会直接遍历 `moduleMetadataTokenRecords`，把其中匹配当前 import module 的 `MEMBER_REF` record 解码为可校验 import effect，所以即使 `moduleEntryEffects` 缺失或被瘦身，module ref table 中的 target token/hash/bytes drift 也会让 guard 进入 `else` 或让 required import 抛 mismatch。source/binary import 会保留 provider token/hash 和同名函数签名候选，member call resolver 与 module-init import-call effect 都会按实参类型在候选中选择正确 overload 并拒绝不匹配签名；当前 source module 在 metadata token 构建后还会把最终 typed export token/hash/value type 回填到 source summary，避免 stale summary 让 target token/hash 与 appended target bytes 分裂。source summary 的 primitive annotation 会规范化为 `PRIMITIVE` signature 节点，避免 ref target 与 provider def 使用不同编码。native import 会给 module-level members 生成 importer-local synthetic `MEMBER_DEF`/`SIGNATURE` token 和 stable hash，native guarded import 也会把这些 target identity 写入 module effect，registered native provider 在运行期复用同一 verifier，因此 target hash drift 会走 guard `else`。`using (var p = %import(...))` 的 guard lowering 已接到 `ZrCore_Module_ImportGuardNativeEntry`；普通 required `%import` 和 guard `%import` 现在都会在运行期消费实际 caller function 上这些 effect 或 module ref records 的 target identity，再在存在 target 子签名时把 caller `MEMBER_REF` 的 `METHOD_SIG` / `FIELD_SIG` 字节与 provider typed export 签名 blob 做确认；provider 有多个同名 typed export 时，runtime verifier 会按 target hash/blob/token 选择匹配的导出，而不是按名字取第一个。双方 target token 都存在且不一致时直接 mismatch，只有旧产物缺 token 时才继续 hash/blob fallback；required import 的 member-level token mismatch 诊断会显示 expected/actual metadata token 和 signature token。校验成功后，`module_import_signature.c` 会把 caller ref token、expected identity 和 resolved provider `MEMBER_DEF` / `SIGNATURE` identity 记录到 `SZrFunction.moduleMetadataBindings`；`module_import_signature_binding.c` 同时把 caller `ASSEMBLY_REF` 绑定到 provider `MODULE` RID 1，并在 provider MODULE record 存在时记录其 paired `SIGNATURE` token/hash，供运行期查询或后续诊断消费；provider summary backed target signature 的 return/parameter 类型、`provider.Option<int>` 这类 module-qualified explicit typed local annotation，以及通过 destructuring import `typeValueAliases` 映射到 provider type 的 `Option<int>` / `Box<Option<int>>` unqualified alias annotation 现在会由 `compiler_metadata_type_ref.c` 递归扫描，包含 nested generic argument 中的 provider `TYPE_DEF`，并额外生成 stable provider `TYPE_REF` + paired `SIGNATURE` records；explicit/alias typed annotation 即使没有 import member effect 也会生成对应 `ASSEMBLY_REF` owner。`function_metadata_binding.c` 会把这些 caller `TYPE_REF` 绑定到 provider `TYPE_DEF` / paired `SIGNATURE` 并记录 module/layout identity，status helper 会报告 unmatched、definition mismatch 和 layout mismatch，并且在 drift 时不写 partial binding；import verifier 成功后会把 stable TypeRef drift 记录为 `type_ref_mismatch` module-load diagnostic。`.zro` patch 28 会持久化这些 binding sidecar，patch 32 继续持久化 layout fields。目标签名不一致时 required import 抛出 `import signature mismatch` 结构化运行期诊断（包含 module/member/expected/actual hash）；source/native provider 不可用时 required import 抛出 `import_load_unavailable`，并会附带 project source/binary 尝试路径或 native descriptor plugin last-error/source path；guard import 返回 null 并走 `else`；嵌套 callable 内的 guarded effects 会由 module init finalize 复制到对应 child function，再按同一 caller 校验路径执行。registry owner refcount API 已接入，descriptor plugin invalidation/reload 现在会在 live owner refs 存在时拒绝清 cache 或关闭 plugin handle；AOT descriptor loader diagnostics 已接入，descriptor prepare/entry execute 失败会把 AOT runtime lastError 写入 module-load diagnostic，并用 result 区分 `descriptor-load-failed` / `module-execute-failed`、用 backend 区分 `aot-c` / `aot-llvm`。当前 import owner-chain 的 `TYPE_REF` 仍主要是 module-member owner 占位；module-qualified explicit typed local annotation 和 destructured/unqualified alias mapping surfaces 都已覆盖。

这正是 [02 篇](02-using-scopes-and-plugin-guards.md) §4 "校验导出签名 token vs 本模块引用签名"的精确定义：**比对的是签名 blob，而非名字或布局**。

## 5. 与 C# 的对照与取舍

| 维度 | C# / ECMA-335 | 本设计 |
| --- | --- | --- |
| token 编码 | `表标签<<24 \| RID` | 相同 |
| 跨程序集 | AssemblyRef/TypeRef/MemberRef | 相同分层 |
| 签名 | `#Blob` heap，二进制签名 | `SZrIoSignatureBlobHeap`，TLV 签名 |
| 字符串 | `#Strings`/`#US` heap | 函数级 `metadataStringHeap`，签名 blob 写 content-stable `u32` string index；旧 patch inline 字符串兼容读取 |
| 版本绑定 | strong name / public key token | 模块名 + 版本 + 签名哈希 |
| 泛型 | MethodSpec/TypeSpec | `TYPE_SPEC` + `GENERIC_INST` 签名 |
| **额外** | 无所有权概念 | 签名内建 `OWNERSHIP` 节点 |
| **额外** | union 靠库类型 | 签名内建 `UNION` 节点 |
| AOT 消费 | IL2CPP 全量消费 | **分阶段**：先让 import/守卫消费，AOT 后续接入（见 05 篇） |

## 6. 编译落点

| 关注点 | 文件 | 改动 |
| --- | --- | --- |
| token 表 / 签名堆 schema | `metadata_token.h` / `function.h` / `io.h` | 已新增 32-bit token、table tag、signature node、函数级 token record、provider `MODULE` + paired `SIGNATURE` record、entry-function 级 `moduleMetadataTokenRecords` 聚合表、runtime `moduleMetadataBindings` binding sidecar、blob heap、metadata string heap、`signatureHash`、`moduleSignatureHash`、`moduleVersion`、`ownerToken`、`layoutVersion` / `layoutHash` 与 `targetMetadataToken` / `targetSignatureToken` / `targetSignatureHash` / `targetModuleSignatureHash` / `requestedModuleVersion` / `minModuleVersionInclusive` / `maxModuleVersionExclusive`；本地 union 定义会生成 `TYPE_DEF` + paired `SIGNATURE` record，并在 record 层保存 layout identity |
| 编译期 ID → token 映射 | `compiler_typed_metadata.c` / `compiler_metadata_token.c` | 已在 typed metadata 末期为 exported symbol 分配 `MEMBER_DEF` / `SIGNATURE` RID |
| 签名 blob 生成 | `compiler_metadata_signature.c` / `compiler_metadata_token.c` / `compiler_metadata_module_record.c` / `compiler_metadata_ref.c` / `compiler_metadata_module_hash.c` | `compiler_metadata_signature.c` 负责从 `SZrFunctionTypedTypeRef` 编码首批 TLV 节点：`METHOD_SIG`、`FIELD_SIG`、`PRIMITIVE`、`TYPE_REF`、`ARRAY`、`OWNERSHIP`、`UNION`、`NULLABLE`，并把字符串引用写为 metadata string heap index，同时提供 `zr.md.sig.v1\0` stable hash；`compiler_metadata_module_record.c` 负责 provider `MODULE` record 与 `MODULE <entryName> <moduleVersion>` signature；`compiler_metadata_token.c` 负责 import member effects 的 `ASSEMBLY_REF` / `TYPE_REF` / `MEMBER_REF` 编排、可解析 provider export 的目标 `METHOD_SIG` / `FIELD_SIG` 追加和 metadata string heap 收集；`compiler_metadata_ref.c` 负责把函数级 import ref records 聚合为 module-level ref table；`compiler_metadata_module_hash.c` 负责基于 `zr.md.mod.v2\0` 计算 module ABI hash，并把 public typed exports 与 `TYPE_DEF` / `TYPE_SPEC` record identity 一起纳入输入域 |
| plugin guard `%import` lowering | `compile_statement.c` / `compiler_bindings.c` / `compiler_internal.h` | 已为 guard-style `%import` 新增专用 emit 入口，编译到 `ZrCore_Module_ImportGuardNativeEntry`；普通 `%import` 保持原 native helper。两种 helper 现在共用 runtime import signature verification；guard mismatch 走 `else`，required mismatch 抛出 `import signature mismatch` 运行期诊断 |
| union prototypeData | `compiler_union.c` / `compiler.c` | 已序列化 union prototype、variant member metadata、payload 字段名/类型表和 tag/payload byte layout metadata；typed union local materialization 已直接消费这些 metadata，inline `switch`/`using` load/store 与 owner-aware drop/GC 仍后续补齐 |
| import 解析按签名比对 | `type_inference_import_metadata.c` / `type_inference_member_resolution.c` / `compile_expression_types.c` / `module_init_analysis.c` | 已完成 source/binary 同名函数候选保留、调用级签名选择、import-call effect 目标候选选择，以及 source summary primitive type ref canonicalization；后续：模块级 ref→def 改为签名 blob/hash 匹配，与名字匹配并行灰度 |
| 运行期解析 | `io_runtime.c` / `module_loader.c` / `module_import_signature.c` / `function_metadata_binding.c`，后续 `module_prototype.c` | 已复制 token records/blob heap、metadata string heap、owner chain、signature hash、module ABI hash、module version、AssemblyRef version range、target identity 和 binding sidecar 到 runtime function；普通 required import 与 guard import 都会消费实际 caller function module effects 的 `requestedModuleVersion` / `minModuleVersionInclusive` / `maxModuleVersionExclusive`、`targetModuleSignatureHash`、`targetMetadataToken` / `targetSignatureToken` / `targetSignatureHash`，先检查 provider `moduleVersion` 是否落入 AssemblyRef range，再在 expected module hash 非零时比较 provider entry function `moduleSignatureHash`，再在 provider同名 typed exports 中按 target hash/blob/token 选择匹配候选，随后与 provider typed export `signatureHash` 校验，存在 target 子签名时优先从 `moduleMetadataTokenRecords`、再从函数级 `metadataTokenRecords` 截取 caller `MEMBER_REF` 子 blob 并与 provider export 签名 blob 比较；patch 30+ 的 blob 字符串通过 metadata string heap index 解码，旧 patch 回退 inline string decode；dependency manifest 显式 min/max range 已在 compile-time 写入这些 fields；registered native provider 也走同一 verifier；验证成功后写入 `moduleMetadataBindings` runtime sidecar；TypeSpec binding 会在 matching union `TYPE_SPEC` 后继续绑定对应 `TYPE_DEF` definition/layout identity，TypeRef binding 会把已带 stable provider `TYPE_DEF` target identity 的 caller `TYPE_REF` 绑定到 provider `TYPE_DEF` / paired `SIGNATURE` / module/layout identity；TypeSpec definition/layout drift 作为 `type_spec_mismatch` diagnostic 暴露，TypeRef unmatched/definition/layout drift 作为 `type_ref_mismatch` diagnostic 暴露，二者都不写 partial binding；required member mismatch 抛出 `import signature mismatch`，required module hash mismatch 抛出 `assembly_signature_mismatch`，required version mismatch 抛出 `assembly_version_mismatch`，required provider unavailable 抛出 `import_load_unavailable`，guard mismatch 或 provider 不可用返回 null 进入 else；future `.zro` patch 会在 IO header 阶段拒绝并报告 actual/supported patch |
| writer / reader | `writer.c` / `io.c` | 已在 `.zro` patch 32 写入/读取 typed export token/hash 字段、函数级 metadata token block、`ownerToken`、record-level `layoutVersion` / `layoutHash`、binding-level expected/resolved `layoutVersion` / `layoutHash`、metadata record target token/hash/module-hash/version-range、module effect target token/hash/module-hash/version-range、`moduleVersion`、`moduleSignatureHash`、metadata string heap、`moduleMetadataTokenRecords` 和 `moduleMetadataBindings`；writer/reader 共用 `ZR_IO_SOURCE_PATCH_CURRENT`；旧 patch 读取时缺失字段置 0、null 或空表，未来 patch 读取时报告 actual/supported patch 并拒绝 |

## 7. 设计决策建议

### 7.1 签名哈希

首版已使用仓库已有的 `XXH3_64bits` 路线，落成 `TZrUInt64 signatureHash` 和 `TZrUInt64 moduleSignatureHash` 基础字段。当前 `moduleSignatureHash` 使用 `zr.md.mod.v2\0` schema，覆盖 public typed exports 的 canonical signature hash/blob 与导出身份，并把本模块 `TYPE_DEF` / `TYPE_SPEC` 实体 record 的 signature/layout identity 纳入 module ABI fingerprint；因此即使公开函数签名文本保持 `choose(): Option<int>`，本地 `Option<T>` TypeDef contract 或 layout 漂移也会让 provider module hash 漂移。它是**ABI fingerprint**，用于快速检测“编译期看到的契约”和“加载期拿到的契约”是否一致，不作为安全/防篡改边界；插件来源可信、签名验证、沙箱等安全问题应另列机制处理。若后续需要更低碰撞概率，可以扩展为两个 `TZrUInt64` 的 `XXH3_128bits`，但首版不引入新 crypto 依赖。

哈希输入必须是规范化签名流，不直接哈希整个 `.zro`：

- 固定前缀：`zr.md.sig.v1\0`，用于隔离未来编码版本。
- 模块身份：canonical module name、semantic version、public ABI mode。
- 公开 TypeDef / MemberDef：按稳定 declaration order 或 canonical qualified name 排序后写入 table tag、canonical name、visibility、generic arity、约束、签名 blob。
- ABI 相关 metadata：FFI lowering kind、extern/native call convention、owner mode、release hook、union variant 名称/顺序/字段契约等会影响跨模块调用或析构的字段。
- 排除项：源码行列、注释、doc string、非 ABI decorator、调试信息、物理 slot/offset、编译时间和本地路径。

实现上已先做 `compiler_metadata_signature.c` 内部的 `metadata_signature_hash_v1()`，并复用 `zr_vm_core/hash` 的稳定 hash API。签名 hash、type/symbol signature size/write、union type signature 识别和基础写入 helper 已从 `compiler_metadata_token.c` 拆出；module ABI hash 已进入 `compiler_metadata_module_hash.c/.h`；module ref 聚合已进入 `compiler_metadata_ref.c/.h`；runtime import signature verification 已进入 `module_import_signature.c/.h`，成功 verifier 后的 binding sidecar 写入已进入 `module_import_signature_binding.c/.h`。后续继续扩大 MethodSig/FieldSig、module hash 输入域或 ref→def 绑定时，应优先沿这些边界扩展，避免把 loader/ref/hash/binding 解析职责重新堆回 token 编排文件或已经缩小的 `module_loader.c`。

### 7.2 版本约束

建议 `AssemblyRef` 同时写：

- `moduleName`
- `requestedVersion`
- `minVersionInclusive`
- `maxVersionExclusive`
- `expectedSignatureHash`

默认规则已按 semver 落地：未显式约束时，`minVersionInclusive = 编译期版本`，`maxVersionExclusive = next-major.0.0`。显式约束 surface 已落在 dependency manifest：依赖声明对象可写 `minVersionInclusive` / `maxVersionExclusive`，由 project resolver 按 canonical `$pkg@version/path` 回查并写入 AssemblyRef。加载期先检查版本范围，再检查 module ABI hash 和 member signature identity。版本给人读、帮助诊断和分发；哈希给机器做最终 ABI 判定。

失败策略区分场景：

- 普通 required import：版本不兼容或 hash 不匹配是编译/加载错误。
- `using (var p = %import(...)) { ... } else { ... }` 插件守卫：版本不兼容、hash 不匹配、加载失败都走 `else`，同时保留诊断事实供 LSP/日志显示。

### 7.3 TypeDef 物理布局

TypeDef 的物理 byte offset 不进入签名身份。签名只表达逻辑契约：类型名、泛型实参、所有权种类、union variant、公开成员名、参数/返回类型、字段契约和 ABI 相关 decorator。这样 provider 模块可以重排私有布局，只要公开契约不变，import 侧不被迫失效。当前已落地的本地 union `TYPE_DEF` 写入 `TYPE_DEF <nameIndex> <genericArity> <variantCount> ...`，包含 variant name/kind/default flag/field count 与 payload field name/passing mode/TypeSig，作为定义侧 token 锚点和公开契约 baseline。

需要 inline value ABI 共享时，另加 `layoutHash` / `layoutVersion` 检查，输入来自 union layout 的 tag size、payload offset/size/align、整体 size/align 和各 variant payload field 的 offset/size/align。这个检查是“能否按值共享内存布局”的条件，不是“类型是否同一个契约”的条件。因此当前实现把 `layoutVersion` / `layoutHash` 放在 `SZrMetadataTokenRecord` record 字段并经 `.zro` patch 31 持久化，而不是把物理布局写入 `TYPE_DEF` signature blob 或 `signatureHash`。当跨模块 union `TYPE_SPEC` 匹配时，binding helper 会继续绑定对应 `TYPE_DEF`：TypeDef 逻辑 contract 的 signature hash/blob 必须一致，record-level layoutVersion/layoutHash 也必须一致；成功结果会把 expected/resolved layout identity 写入 `SZrMetadataTokenBinding` 并经 `.zro` patch 32 roundtrip。

### 7.4 签名 blob 规范化

typed export 与 import ref target 已统一使用当前 v1 canonical `METHOD_SIG` / `FIELD_SIG` 编码，AssemblyRef semver range 也已进入 patch 29 schema 和 runtime verification，且 dependency manifest 可显式覆盖 min/max range。TypeSpec baseline binding、loader-facing mismatch diagnostic、union TypeSpec→TypeDef definition/layout binding，以及 TypeRef→TypeDef mismatch status / `type_ref_mismatch` diagnostic 已落地；后续跨模块 ABI hash 扩大输入域时应继续沿现有 `compiler_metadata_type_spec.c` / `compiler_metadata_type_ref.c` / `function_metadata_binding.c` / `module_import_signature.c` 边界演进。当前 MethodSig 规范化编码为：

```
MethodSig := METHOD_SIG <sigVersion:u8=1> <callConv:u8> <genericArity:u32> <retTypeSig> <paramCount:u32> (<paramMode:u8> <TypeSig>)*
TypeSig   := PRIMITIVE <prim:u32>
          | TYPE_REF <assemblyRefRid:u32> <qualifiedName>
          | TYPE_DEF <qualifiedName>
          | ARRAY <rank:u32> <TypeSig>
          | TUPLE <count:u32> <TypeSig>*
          | GENERIC_INST <openTypeSig> <argCount:u32> <TypeSig>*
          | OWNERSHIP <kind:u32> <TypeSig>
          | UNION <qualifiedName> <argCount:u32> <TypeSig>*
          | NULLABLE <TypeSig>
          | MEMBER_REF <assemblyRefRid:u32> <ownerTypeSig?> <memberName> <MethodSig|FieldSig>
```

所有整数按 little-endian 固定宽度写入；patch 30+ 的字符串引用写 metadata string heap 的 `<stringIndex:u32>`，patch 29 及更早产物仍按旧 `<len:u32><utf8-bytes>` 解码。泛型实参和 union variants 按声明顺序写入。跨模块比较时不要把 provider 本地 `TYPE_DEF` RID 直接写进 hash；RID 是模块内索引，跨模块身份要通过 assembly ref + qualified name + signature blob 归一。

### 7.5 Token/RID 稳定性

Token 是落盘索引和 loader 绑定句柄，不是全局身份。建议：

- Def 表 RID 使用稳定声明顺序；同一源码重编译应保持 RID 不漂移。
- Ref 表 RID 使用 canonical first-use order；需要 hash 时使用 ref 记录的规范化内容，不直接哈希 RID。
- 函数级 ref 前置记录已经使用 `ownerToken` 表达层级：`AssemblyRef.ownerToken = 0`，`TypeRef.ownerToken = AssemblyRef.token`，`MemberRef.ownerToken = TypeRef.token`；配对 `SIGNATURE.ownerToken` 指回实体 token。后续模块级表应保留这一关系，避免通过 RID 范围或 record 顺序推断 owner。
- `SIGNATURE` RID 只索引 blob heap 范围；不同 token 可共享相同 blob，后续可以加去重，但首版先保持一 token 一 signature，便于调试。
- import 绑定结果单独记录 `refToken -> resolvedDefToken`，不要覆写原始 ref token，便于诊断“编译期引用”和“加载期绑定”的差异。
