---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 参照 hybridclr/mono/roslyn(runtime illink) 完善代码裁剪
  - decision: 2026-06-20 裁剪默认最小 + 注解保留（对标 NativeAOT/illink mark-and-sweep）
references:
  - lua/runtime/src/tools/illink/src/linker/Linker.Steps/MarkStep.cs        # mark-and-sweep 主循环
  - lua/runtime/src/tools/illink/src/linker/Linker/Annotations.cs           # marked_pending/processed 状态机
  - lua/runtime/src/tools/illink/src/linker/Linker.Steps/DescriptorMarker.cs # link.xml descriptor
  - lua/runtime/src/tools/illink/src/linker/Linker.Dataflow/ReflectionMarker.cs
  - lua/runtime/src/coreclr/tools/aot/ILCompiler.Compiler/Compiler/AnalysisBasedMetadataManager.cs
  - lua/hybridclr/libil2cpp/                                                # il2cpp + Unity linker 配合
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.h   # 12-S2A reachability filter API；现状默认仍全量收集
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.c   # 12-S2A function table 可达项压缩
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c                  # shared AOT writer option normalization；12-S7Y default-min reflection metadata policy
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_internal.h         # shared AOT writer option normalization API；12-S7Y reflection metadata policy option helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c        # 12-S2B 稀疏 thunk/method-info 发射调度；12-S2D/S2E opt-in 裁剪接入与 root buffer；12-S4A manifest roots；12-S4C top-level callable flat-index resolver；12-S4H/S4I/S4J generic root diagnostics + TypeSpec token/hash/generic instance identity；12-S7A/S7B/S7C 函数裁剪统计；12-S7F embedded module byte statistic；12-S7H type-layout trim before/after 统计；12-S7I/S8 runtime fallback diagnostics 调度；12-S7K zrp metadata section/table/pool byte statistics；12-S7L type-layout payload byte trim delta；12-S7M runtime fallback warning suppression；12-S7O runtime fallback warning reason-mask suppression；12-S7R generated-C type-layout byte trim delta；12-S7S zrp metadata byte trim delta carrier；12-S7T delegates zrp metadata size accounting；12-S7U symbol-stripping option marker/plumbing；12-S7V method metadata generated-C byte trim delta；12-S7Y metadata policy marker/plumbing；12-S7Z emitted zrp metadata pruning plumbing；12-S8H/S8I full-AOT manifest generic TypeSpec/generic-instantiation gate
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_size.h # 12-S7T zrp metadata size/delta accounting API；12-S7Z blob-based after-stats sampling；12-S7ZO section-level trim delta marker surface；12-S7ZP section count marker fields
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_size.c # 12-S7T zrp metadata header sampling + size/delta marker writing；12-S7Z pruned-blob stats；12-S7ZO per-section before/after/removed marker writing；12-S7ZP per-section count stats/delta marker writing
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.h # 12-S7Z emitted zrp metadata pruning API; 12-S7ZA token-record MethodDef remap surface; 12-S7ZB FieldDef shared MEMBER_DEF remap; 12-S7ZC GenericParam owner/range remap; 12-S7ZD remap module split surface; 12-S7ZG signature blob pool compaction orchestration; 12-S7ZH string-pool compaction orchestration; 12-S7ZI constant-pool orphan sweep API surface
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.c # 12-S7Z compacted blob rebuild orchestration; 12-S7ZD delegates token/range remap helpers; 12-S7ZE GenericParamConstraint section copy/compaction orchestration; 12-S7ZF MethodSpec section copy/compaction orchestration; 12-S7ZG signature blob pool compaction/rewrite orchestration; 12-S7ZH delegates shared section helpers and string-pool remap/copy; 12-S7ZI zero-retained constant-pool layout; 12-S7ZM post-remap identity skip so pool compaction runs without MethodDef pruning
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.h # 12-S7ZD private zrp metadata token/range remap API; 12-S7ZE GenericParamConstraint remap/count/range API; 12-S7ZF MethodSpec remap/count API; 12-S7ZN export member-token remap surface
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.c # 12-S7ZA..S7ZC MethodDef/FieldDef/GenericParam token/range remap implementation; 12-S7ZE GenericParamConstraint cascade implementation; 12-S7ZF MethodSpec method-token cascade implementation; 12-S7ZN exported MethodDef/FieldDef member token remap helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_sections.h # 12-S7ZH shared zrp metadata section lookup/layout/copy API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_sections.c # 12-S7ZH shared section switch, layout writer, and raw-section copy helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.h # 12-S7ZG signature blob remap/compaction API; 12-S7ZM signature remap identity API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.c # 12-S7ZG retained signature blob slice collection, MethodSpec signature rewrite, offset remap, and hash recomputation; 12-S7ZM signature remap identity helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_string_pool.h # 12-S7ZH string-pool remap/compaction API; 12-S7ZL duplicate retained string-slice remap support; 12-S7ZM string remap identity API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_string_pool.c # 12-S7ZH retained string slice collection, compacted string-pool copy, and row string-offset remap; 12-S7ZL content-level duplicate slice interning; 12-S7ZM string remap identity helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_monomorphization.h # 12-S7U generated-symbol stripping option API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_monomorphization.c # 12-S7U stable-ID private helper symbols for monomorphized generic value forms
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.h # 12-S7U generated-symbol stripping option API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.c # 12-S7U stable-ID private helper symbols/debug names for shared generic reference forms
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.h # 12-S7G generated method metadata emitter boundary；12-S7V generated method metadata byte sampling API；12-S7Y reflection metadata level emitter parameter
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c # 12-S7G generated signature/method-info byte span + table emission；12-S7V generated method metadata byte sampling helper；12-S7Y policy-driven MethodInfo reflection level emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_emitter.h # 12-S7W LLVM lowering context strip flag
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_emitter.c # 12-S7W LLVM symbol-stripping marker/option plumbing
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_emit.h # 12-S7W LLVM generated function symbol formatter API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_text_emit.c # 12-S7W LLVM generated function symbol formatter
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_function_body.h # 12-S7W private function definition symbol stripping API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_function_body.c # 12-S7W private function definition symbol stripping
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_lowering_function_calls.c # 12-S7W static direct-call symbol parity
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.h # 12-S7W thunk table/entry-thunk symbol parity API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.c # 12-S7W thunk table/entry-thunk symbol parity
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h          # 12-S7N/S7P runtime fallback source line/column span carrier
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c          # ExecIR build orchestration；12-S7P delegates source-location derivation
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_source_location.h # 12-S7P source-location derivation API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir_source_location.c # 12-S7P ExecIR debug line/column span derivation
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_runtime_fallback.h # 12-S7I/S8A-S8D runtime fallback warning/full-AOT closure diagnostics API；12-S7O reason-mask suppression API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_runtime_fallback.c # 12-S7I runtime fallback warning reason scan；12-S7N sourceLineEnd marker；12-S7O reason-mask suppression filter；12-S7P sourceColumn/sourceColumnEnd marker；12-S7Q sourceFile attribution；12-S7ZJ reasonFlag marker；12-S7ZK quoted/escaped sourceFile marker；12-S8A/S8B/S8C dynamic deopt 与 12-S8D reflection full-AOT 闭合预检
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c # 12-S8E full-AOT generic METHOD slot static closure
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c   # 12-S7D/S7E type-layout/generated descriptor byte span + total；12-S7H distinct referenced inline type-layout count；12-S7L referenced inline type-layout payload bytes；12-S7R generated-C type-layout byte span sampling helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_callable_provenance.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.h     # 12-S1A mark state/reason API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c     # 12-S1A BFS mark engine
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.h # 12-S2C static callable graph API；12-S2E caller-provided roots；12-S4A manifest root input
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c # 12-S2C bytecode callable edge scan；12-S2E export callable roots；12-S4A manifest roots
  - zr_vm_parser/include/zr_vm_parser/writer.h # 12-S2D enableCodeStripping opt-in；12-S4A parsed manifest preserve function roots；12-S4C top-level callable flat-index resolve API；12-S4H/S4I/S4J/12-S4N/12-S8I manifest generic roots + TypeSpec/generic-instantiation/MethodSpec binding fields；12-S7M suppressRuntimeFallbackWarnings writer option；12-S7O suppressRuntimeFallbackWarningReasonMask writer option；12-S7U stripGeneratedSymbols writer option
  - zr_vm_library/include/zr_vm_library/project.h # 11-S7E/12-S4B parsed zrp preserve rule model；11-S7F parsed aotMode model；11-S7K/12-S4E preserve feature condition model；11-S7L/12-S4F feature switch map；11-S7M/12-S4G generic preserve arguments
  - zr_vm_library/src/zr_vm_library/project/project_features.c # 11-S7L/12-S4F feature switch parser
  - zr_vm_library/src/zr_vm_library/project/project_preserve.c # 11-S7E/12-S4B/11-S7K/12-S4E/11-S7M/12-S4G preserve declaration + feature condition + generic argument parser
  - zr_vm_library/src/zr_vm_library/project/project_aot_options.c # 11-S7F aotMode declaration parser
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.h # 11-S7G/12-S8F project aotMode -> AOT writer option bridge API；12-S7X release/full-AOT symbol stripping policy API
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c # 11-S7G/12-S8F project aotMode -> requireFullAot injection helper；12-S7X full-AOT -> stripGeneratedSymbols injection
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.h # 11-S7H/12-S4C/12-S4D/12-S4H..12-S4N/12-S8G AOT C emission + preserve writer root bridge API
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.c # 11-S7H/12-S4C/12-S4D/12-S4F/12-S4H..12-S4N/12-S8G embedded blob + method/type/generic preserve root injection + feature-conditioned root gating + generic TypeSpec synthesis/open-base generic-instantiation/MethodSpec binding
  - zr_vm_cli/src/zr_vm_cli/command/command.h # 11-S7W/11-S7X/11-S7Y/12-S7ZQ/12-S7ZR/12-S7ZS CLI zrp metadata dump/diff/version-check mode carriers
  - zr_vm_cli/src/zr_vm_cli/command/command.c # 11-S7W/12-S7ZQ `--dump-zrp-metadata <file>`, 11-S7X/12-S7ZR `--diff-zrp-metadata <before> <after>`, and 11-S7Y/12-S7ZS `--check-zrp-metadata-version <file>` parse/exclusivity/help surface
  - zr_vm_cli/src/zr_vm_cli/app/app.c # 11-S7W/11-S7X/11-S7Y/12-S7ZQ/12-S7ZR/12-S7ZS CLI app dispatch to zrp metadata dump/diff/version-check runners
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.h # 11-S7W/11-S7X/11-S7Y/12-S7ZQ/12-S7ZR/12-S7ZS zrp metadata section summary, diff summary, and version-check API
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.c # 11-S7W/12-S7ZQ read-only zrp metadata section summary; 11-S7X/12-S7ZR before/after section byte/count diff summary; 11-S7Y/12-S7ZS header magic/version/header-size/section-count compatibility check implementation
  - zr_vm_language_server_extension/schemas/zrp.schema.json # 11-S7E/12-S4B preserve schema parity；11-S7F aotMode schema parity；11-S7K/12-S4E feature condition schema parity；11-S7L/12-S4F feature switch schema parity；11-S7M/12-S4G generic argument schema parity
  - zr_vm_parser/src/zr_vm_parser/compiler/   # 编译流程入口
  - tests/library/test_project_manifest_normalization.c # 11-S7E/12-S4B zrp preserve declaration parser gates；11-S7F aotMode gates；11-S7K/12-S4E preserve feature condition gates；11-S7L/12-S4F feature switch gates；11-S7M/12-S4G generic preserve argument gates
  - tests/parser/test_aot_reachability.c      # 12-S1A/12-S2A/12-S2B/12-S2C/12-S2E/12-S4A focused reachability contracts
  - tests/parser/test_aot_c_code_stripping.c  # 12-S2D opt-in generated C filtering contract；12-S2E export root preservation；12-S4A manifest preservation；12-S7A/S7B/S7C 函数统计；12-S7H type-layout trim before/after stats；12-S7K zrp metadata section/table/pool byte statistics；12-S7L type-layout payload byte trim delta；12-S7R generated type-layout byte trim delta；12-S7S zrp metadata byte trim delta；12-S7V generated method metadata byte trim delta；12-S7Y stripped output reflection metadata level policy；12-S7Z zrp MethodDef metadata pruning；12-S7ZG signature blob pool after-trim delta；12-S7ZH string pool after-trim delta；12-S7ZI constant pool after-trim delta
  - tests/parser/test_aot_c_zrp_metadata_pruning.c # 12-S7ZA direct zrp MethodDef/token-record pruning; 12-S7ZB FieldDef shared MEMBER_DEF remap; 12-S7ZC GenericParam owner/range remap; 12-S7ZE GenericParamConstraint cascade remap; 12-S7ZF MethodSpec method-token cascade remap; 12-S7ZG MethodSpec signature blob rewrite/compaction
  - tests/parser/test_aot_c_zrp_metadata_export_token_remap.c # 12-S7ZN direct exported MethodDef/FieldDef member token remap coverage
  - tests/parser/test_aot_c_zrp_metadata_size_deltas.c # 12-S7ZO direct section-level zrp metadata before/after/removed marker coverage; 12-S7ZP direct section count marker coverage
  - tests/parser/test_aot_c_zrp_metadata_pool_pruning.c # 12-S7ZH direct zrp string-pool compaction/remap; 12-S7ZI direct zrp orphan constant-pool sweep; 12-S7ZL retained duplicate string-slice compaction; 12-S7ZM no-MethodDef-prune pool compaction trigger
  - tests/parser/test_aot_c_source_contracts.c # 12-S7T zrp metadata size accounting module boundary；12-S7U public symbol-stripping option/emitter plumbing；12-S7V method metadata byte-delta source contract；12-S7Y metadata policy source contract；12-S7Z..12-S7ZI emitted zrp pruning/remap/signature/string-pool module contracts；12-S7ZN export-token remap helper source contract；12-S7ZO section-level delta marker source contract；12-S7ZP section-count marker source contract
  - tests/cli/test_cli_args.c # 11-S7W/11-S7X/11-S7Y/12-S7ZQ/12-S7ZR/12-S7ZS CLI dump/diff/version-check mode parse/exclusivity coverage
  - tests/cli/test_cli_zrp_metadata_dump.c # 11-S7W/11-S7X/11-S7Y/12-S7ZQ/12-S7ZR/12-S7ZS zrp metadata dump/diff/version-check summary/path coverage
  - tests/CMakeLists.txt # 11-S7W/11-S7X/11-S7Y/12-S7ZQ/12-S7ZR/12-S7ZS CLI zrp metadata dump/diff/version-check target; 12-S7ZA/12-S7ZH/12-S7ZN/12-S7ZO/12-S7ZP Windows shared-DLL direct zrp pruning/remap/size tests link private pruning/remap/section/signature/string-pool/size modules
  - tests/parser/test_aot_c_value_type_shared_library_smoke.c # 12-S7D/S7E generated type-layout byte statistics
  - tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c # 12-S7I/S7J runtime fallback trim warning reason/sourceLine；12-S7M warning suppression；12-S7N sourceLineEnd；12-S7O reason-mask suppression；12-S7P sourceColumn/sourceColumnEnd；12-S7Q sourceFile attribution；12-S7ZJ warning reasonFlag marker；12-S7ZK quoted/escaped sourceFile marker；12-S8A/S8B full-AOT dynamic call/value-access deopt bridge 拒绝
  - tests/parser/test_aot_c_iterator_shared_library_smoke.c # 12-S8C full-AOT dynamic iterator deopt 拒绝
  - tests/parser/test_aot_c_global_shared_library_smoke.c # 12-S8D full-AOT TYPEOF reflection runtime contract 拒绝
  - tests/parser/test_aot_c_generic_call_typed.c # 12-S7F embedded module byte statistic；12-S7G generated method metadata byte statistics；12-S7U release generated-symbol stripping fixture；12-S8E full-AOT generic METHOD slot runtime branch closure
  - tests/parser/test_aot_llvm_symbol_stripping.c # 12-S7W default/stripped LLVM generated symbol contract
  - tests/cli/test_cli_project_incremental.c # 11-S7G/11-S7H/12-S8F/12-S8G manifest full-AOT writer option bridge + CLI AOT C emission；12-S7X release/full-AOT symbol stripping CLI policy
  - tests/cli/test_cli_aot_writer_options.c # 12-S3A..12-S3F/12-S4C/12-S4D/12-S4F/12-S4H..12-S4N/12-S8H..12-S8I parsed method/type/generic/feature-conditioned preserve -> writer manifest root binding + generic TypeSpec synthesis/open-base generic-instantiation/MethodSpec binding/full-AOT gate
  - tests/parser/test_aot_c_frame_setup_contracts.c # 12-S2B sparse emitter source contract；12-S7U generic sharing emitter call option contract；12-S7V method-info emitter return signature contract；12-S7Y MethodInfo reflection-level emitter source contract
---

# 12 · 代码裁剪（mark-and-sweep 可达性 + 注解保留 + 泛型实例可达性）

> 承接缺口：**几乎完全缺失**。当前 AOT 全量生成（`backend_aot_function_table` 全收集，
> 仅有 `executable_subset`/`unsupported_instruction` 检查，无可达性分析、无 DCE、无体积统计、
> 无符号剥离）。本文按既定决策（**默认最小 + 注解保留**，对标 illink/NativeAOT）补齐裁剪管线，
> 它是 `08`(泛型实例)/`10`(反射级别)/`11`(元数据策略) 的**共同上游驱动**。

## 0. 定位：裁剪是统一上游

```
入口/导出/manifest 保留 + 注解
        │  mark-and-sweep 可达性（§1）
        ▼
   可达实体集合（函数/类型/字段/泛型实例/元数据）
   ┌──────────┬──────────┬──────────┬──────────┐
   ▼          ▼          ▼          ▼
 只生成     只生成      反射级别    元数据策略
 可达函数   可达 layout  (10§0)     (11§7)
 (AOT C)   + GC desc(09)
```

- 一次可达性分析，四处消费：决定 AOT 生成哪些函数、哪些类型 layout/descriptor、每实体反射级别、
  每实体元数据生成量。对标 illink `MarkStep` 一遍标记驱动后续 sweep/emit。

## 1. 可达性分析（mark-and-sweep，对标 illink MarkStep）

- **状态机**（对标 `Annotations`）：每实体三态 `unmarked / marked_pending / processed`，BFS 队列驱动。
- **根（roots）**：程序入口 `main`、模块导出（`SZrFunctionTopLevelCallableBinding` 导出位）、
  manifest 显式保留（§3）、注解保留目标（`10`§5）。
- **标记传播**（扫描函数体 SemIR / 字节码，对标 illink 扫 IL）：
  - 直接调用 → 标记被调函数；
  - 字段/类型使用 → 标记类型及其 layout；
  - 虚/接口调用 → 标记该接口所有**可达类型**上的覆盖实现（保守，对标 illink 虚方法处理）；
  - 泛型使用 → 收集并标记具体实例（§2）；
  - 反射点 → 经数据流分析标记（§4）。
- **依赖原因记录**（对标 `DependencyKind`）：每条标记记原因（DirectCall/FieldAccess/Virtual/
  XmlDescriptor/Reflection/Generic），供诊断与 trim 报告（§6）。
- 队列空 → 未标记实体即死代码，sweep 阶段不进入 AOT 产物与元数据。

## 2. 泛型实例可达性（衔接 08）

- 裁剪与 `08`§3 实例化收集是同一遍：标记到泛型使用点时，按实参类型收集具体实例，加入可达集。
- 传递闭包：实例内部用到的其它泛型实例递归标记（对标 mono full-AOT transitive closure、
  NativeAOT `ExactMethodInstantiationsNode`）。
- 引用类型实例共享代码（`08`§1）→ 只需保留一份共享函数 + 各实例的泛型字典；值类型实例逐份保留。
- 运行期动态实例（反射 `MakeGenericType`）：默认不静态保留 → deopt 解释器（`08`§6）；
  若 manifest/注解声明 → 强制收集保留（对标 link.xml 预声明动态泛型实例）。

## 3. 保留规则 manifest（对标 link.xml descriptor，落在 zrp manifest 11§8）

```
preserve {
  type   "Foo"            all       # 类型 + 全部成员 + 完整元数据（10§0 DESCRIPTION）
  type   "Bar"            methods   # 仅方法
  method "Baz.run"                  # 单个成员
  generic "List" <"Foo">            # 预声明动态泛型实例
  feature "EnableX" = true { ... }  # feature switch 条件保留
}
```

- 由 `DescriptorMarker` 等价物解析，把声明项加入根集（对标 illink XML 驱动标记）。
- `feature/featurevalue` 支持条件裁剪（对标 illink feature switch）：按构建配置选择性保留。

## 4. 反射数据流分析（对标 illink FlowAnnotations / ReflectionMarker）

- `@dynamically_accessed(MemberTypes)`（`10`§5）标注参数/返回 → 追踪「哪些类型流向该反射点」，
  保留其相应成员（对标 `DynamicallyAccessedMembers` 流分析）。
- `@dynamic_dependency` → 直接把目标加入根（对标 `[DynamicDependency]`）。
- 无法静态决议的反射（`ResolveToken(动态值)`、按运行期 string 查类型）→ 该点标记为「unanalyzable」，
  产 trim 警告（§6），目标默认不保留（除非注解/ manifest）。

## 5. sweep 与产出收窄

- **AOT 函数**：只对可达函数发 C（取代 `backend_aot_function_table` 全量收集 → 可达过滤）。
- **类型 layout / GC descriptor（09）**：只为可达类型生成。
- **元数据（11）/ 反射（10）**：按可达性 + 级别生成，名表/签名 blob 中不可达项不写入池。
- **符号剥离选项**：release 模式可把生成 C 的函数/类型符号名替换为短稳定 ID（`zr_fn_<hash>`），
  仅保留 manifest 导出与反射 `DESCRIPTION` 级所需名字（对标 release 名称剥离）。

## 6. 裁剪诊断（trim warnings）与体积统计

- **trim 警告**（对标 illink/NativeAOT trim analyzer）：unanalyzable 反射、`@requires_unreferenced_code`
  调用点、被裁剪但运行期可能需要的目标 → 编译期警告，列出依赖原因（§1）。
- **体积统计**：报告每函数/类型/元数据在产物中的字节占用、裁剪前后对比（补现状「无体积统计」缺口）。

## 7. hybrid 安全网

- 裁剪是「typed/AOT 产物」的瘦身；**解释器 + 完整数据元数据（`11`）始终是兜底**：被裁剪的 typed
  目标若运行期被需要 → deopt 到解释器动态执行（`04`§6 / `08`§6）。
- 「full-AOT 模式」（`08`§6）关闭 deopt 兜底 → 裁剪必须证明闭合，否则编译期报错（对标 mono full-AOT）。

## 8. 落地切片

| 切片 | 内容 | 验收 |
|------|------|------|
| 12-S1 | 可达性分析引擎（状态机 + BFS + 依赖原因）（§1） | 🚧 2026-06-24 部分完成：12-S1A 已完成独立 AOT reachability mark 状态机、BFS 与依赖原因记录；接入 function table 后验证死函数不进产物仍待 12-S2 |
| 12-S2 | AOT 生成接入可达过滤（取代全量收集）（§5） | 🚧 2026-06-24 部分完成：12-S2A 已提供 function table 可达过滤 helper，12-S2B 已让 C emitter 支持原始 `flatIndex` 稀疏 thunk/method-info 表，12-S2C 已提供 `GET_CONSTANT`/`CREATE_CLOSURE`/`GET_SUB_FUNCTION` 静态 callable reachability graph 输入，12-S2D 已通过 `enableCodeStripping` opt-in 接入 C emitter 并证明不可达函数不发 C，12-S2E 已把 `SZrFunctionTopLevelCallableBinding` 导出 callable 子函数作为 `ROOT_EXPORT` 保留；默认过滤、完整 manifest/注解 roots、trim 诊断和体积统计仍待后续 |
| 12-S3 | 泛型实例可达性（与 08-S1 合一）（§2） | 🚧 2026-06-25 部分完成：12-S3A/11-S7Q/08-S7F 已把 TypeSpec-backed manifest generic preserve root 物化为 writer 可见的 generic instantiation identity（baseToken/cInstanceId/shareKind），并复用 08-S1 `SZrGenericInstantiationTable` 去重与 shareKind 判定；12-S3B/11-S7R/08-S7G/12-S8I 已让 full-AOT closure gate 消费该 identity，拒绝 TypeSpec-only generic roots；12-S3C/11-S7S/08-S7H/12-S4K 已让当前模块存在同名 `TYPE_REF` metadata 时使用 open generic base token 作为实例化 base token，缺失时回退 TypeSpec；12-S3D/11-S7T/08-S7I/12-S4L 已支持 `TYPE_DEF` base TypeSpec 并把 current-module TypeDef token 作为实例化 base token；12-S3E/11-S7U/08-S7J/12-S4M 已在缺失 TypeSpec 但存在同名 open `TYPE_DEF`/`TYPE_REF` metadata 时合成 current-function TypeSpec/signature binding 并继续物化 generic instantiation identity；12-S3F/11-S7V/08-S7K/12-S4N 已把 manifest generic method root 绑定到现有 MethodSpec 形态签名身份，并让 full-AOT gate 接受 MethodSpec-bound generic method root；完整静态可达闭包、传递 generic closure、跨模块 root 和动态实例 deopt 仍待后续 |
| 12-S4 | manifest 保留规则 + feature switch（§3） | 🚧 2026-06-25 部分完成：12-S4A 已提供已解析 manifest 函数 root 输入通道，`SZrAotWriterOptions` 可携带需要保留的 function flat index，reachability 以 `MANIFEST` reason 保留；12-S4B/11-S7E 已在 `.zrp` project manifest parser 中接受 declaration-level `preserve` array，并把 `type`/`method`/`generic` target 与 optional members 暴露到 project model；12-S4C/11-S7I 已把当前模块的 `method` preserve target 绑定到 entry function top-level callable flat index，并注入 writer manifest roots；12-S4D/11-S7J 已支持 dotted method target 精确匹配 callable name，并把 `type` preserve 的 `members: "methods"` / `"all"` 展开为同名前缀 callable roots；12-S4E/11-S7K 已为 preserve rule 添加 `feature` + boolean `featureValue` 条件声明模型、互相依赖校验与 schema parity；12-S4F/11-S7L 已解析 top-level `features` boolean switch map，并让当前 method/type preserve root 注入按 feature 条件匹配启停；12-S4G/11-S7M 已为 `generic` preserve 添加非空 `arguments` 声明模型与 schema parity；12-S4H/11-S7N 已把 generic preserve target+arguments 注入 AOT writer options 并在 generated C 清单中输出；12-S4I/11-S7O 已在当前函数 metadata 里匹配已有 `TYPE_SPEC` generic signature 并把 TypeSpec/signature token/hash 带到 generic root 诊断；12-S4J/11-S7Q/12-S3A 已把已绑定 TypeSpec 的 generic preserve root 物化为 generic instantiation identity；12-S4K/11-S7S/12-S3C 已在当前模块存在同名 `TYPE_REF` metadata 时把 manifest generic root 的实例化身份绑定到 open generic base token；12-S4L/11-S7T/12-S3D 已支持 `GENERIC_INST(TYPE_DEF target, args...)` manifest generic TypeSpec 绑定并使用 current-module TypeDef base token；12-S4M/11-S7U/12-S3E 已在缺失 TypeSpec 时从同名 open `TYPE_DEF`/`TYPE_REF` metadata 合成 current-function TypeSpec/signature binding；12-S4N/11-S7V/12-S3F 已把 generic method preserve root 绑定到 current-module MethodSpec 形态签名并输出诊断；跨模块 method/generic binding 与注解 roots 仍待后续 |
| 12-S5 | 反射数据流分析 + 注解标记（§4，衔接 10-S5） | 注解反射目标保留；未注解给警告 |
| 12-S6 | 元数据/反射级别按可达性收窄（衔接 10/11）（§5） | 默认产物最小；token 通道仍可用 |
| 12-S7 | trim 警告 + 体积统计；符号剥离选项（§6/§5） | 🚧 2026-06-27 部分完成：12-S7A 已在 opt-in AOT C 生成物中发布函数裁剪统计 `functionsBefore/After/Removed`，12-S7B 已为每个已发射函数输出 generated-C function body byte span，12-S7C 已输出 retained function body byte total，12-S7D 已为 generated type layout / GC descriptor block 输出 `aot_size.typeLayoutBytes[typeLayoutId]`，12-S7E 已输出 `aot_size.typeLayoutBytesTotal`，12-S7F 已输出随 AOT C module descriptor 嵌入的 `aot_size.embeddedModuleBytes`，12-S7G 已输出 generated method signature/info metadata 的 per-method 与 total byte span，12-S7H 已输出 opt-in code stripping 前后 distinct inline type-layout reference 计数，12-S7I 已输出 hybrid runtime fallback trim warning marker 并区分 dynamic-call / dynamic-value-access reason，12-S7J 已为 runtime fallback warning marker 输出 ExecIR `sourceLine`，12-S7K 已输出有效 zrp embedded metadata 的 total/token-record/definition-table/pool 字节统计与 12 个 section 明细，12-S7L 已输出 referenced inline type-layout payload bytes 的 before/after/removed 裁剪差值，12-S7M 已提供 writer-level runtime fallback warning suppression 并输出 suppressed count，12-S7N 已为 visible runtime fallback warning 输出 `sourceLineEnd` line-span marker，12-S7O 已提供 runtime fallback warning reason-mask suppression 并保持 visible/suppressed count 分离，12-S7P 已为 visible runtime fallback warning 输出 `sourceColumn/sourceColumnEnd` 并拆出 ExecIR source-location 推导模块，12-S7Q 已为 visible runtime fallback warning 输出 `sourceFile`，12-S7ZK 已把该 `sourceFile` 输出收紧为 quoted/escaped marker，12-S7R 已输出 referenced inline type-layout 的 generated-C byte span before/after/removed 裁剪差值并校验 after 等于 `aot_size.typeLayoutBytesTotal`，12-S7S 已输出 zrp metadata total/token-record/definition-table/pool byte before/after/removed 裁剪差值载体（当前未重写 metadata，removed 为 0），12-S7T 已把 zrp metadata size/delta accounting 从 `backend_aot_c_emitter.c` 拆入 `backend_aot_c_zrp_metadata_size.{h,c}`，12-S7U 已提供 writer-level `stripGeneratedSymbols` option、`symbol_stripping.generatedSymbols` marker，并在启用时把泛型单态化/shared 私有 helper symbol 与 shared `debugName` 从类型名剥离为稳定 ID，12-S7V 已输出 generated method metadata 的 generated-C byte span before/after/removed 裁剪差值并校验 after 等于 `aot_size.methodMetadataBytesTotal`，12-S7W 已将 writer-level `stripGeneratedSymbols` 扩展到 LLVM 后端，默认保持 `@zr_aot_fn_<flatIndex>`，启用后 generated private function definition、thunk table、entry thunk 和静态 direct-call reference 改为 `@zr_fn_g<flatIndex>`，同时保留公开 `@ZrVm_GetAotCompiledModule`，12-S7X 已把 CLI/project `aotMode: "full-aot"` 默认映射到 `stripGeneratedSymbols`，让 full-AOT `--emit-aot-c` 产物默认启用生成符号剥离，hybrid/default 继续保留可读符号，12-S7Y 已在 opt-in code stripping 下把 generated MethodInfo `reflectionMetadataLevel` 降为 `ZR_AOT_REFLECTION_METADATA_NONE`，默认/非裁剪产物仍为 `RUNTIME_MAPPING`，并输出 `metadata_policy.reflectionLevel` marker；12-S7Z..12-S7ZN 已逐步启用 emitted zrp metadata 的 MethodDef、token-record、FieldDef、GenericParam、GenericParamConstraint、MethodSpec method-token 剪枝级联，retained signature blob pool compaction/offset remap/hash recomputation/MethodSpec signature rewrite，string-pool retained-slice sweep、row offset remap 与 duplicate retained slice interning，当前 orphan constant-pool sweep，让 pool compaction 在 MethodDef count 不变时仍可触发，并提供 exported MethodDef/FieldDef member token 的 compacted-token remap surface；12-S7ZR/11-S7X 已提供 standalone `--diff-zrp-metadata <before> <after>` zrp metadata section byte/count diff summary；完整 trim analyzer、attribute/annotation 抑制策略、field/default-value backed constant-pool remap、cross-module export-token publication/rewrite、完整 metadata sweep/pruning 和版本检查仍待后续 |
| 12-S8 | full-AOT 闭合校验（§7） | 🚧 2026-06-25 部分完成：12-S8A 已在 `requireFullAot` 下预检 SemIR/显式 dynamic call 需要 deopt bridge 且无法静态解析 callee 的路径，12-S8B 已拒绝 SemIR dynamic member/index value-access deopt bridge，12-S8C 已拒绝 SemIR/显式 dynamic iterator deopt runtime boundary，12-S8D 已拒绝 `TYPEOF` reflection runtime contract，12-S8E 已让已静态收集 shared generic `CALL_TYPED` 在 full-AOT 下直接调用 AOT method entry 且不保留 METHOD slot null runtime branch；11-S7F 已提供 `.zrp` `aotMode: "full-aot"` project model；11-S7G/12-S8F 已提供 manifest 到 `requireFullAot` 的 CLI/compiler option 注入 helper；11-S7H/12-S8G 已提供 CLI AOT C 发射入口接线并让 full-AOT manifest policy 触发 writer 预检；12-S8H/11-S7P/08-S7E 已让 full-AOT writer 拒绝未绑定 TypeSpec 的 manifest generic preserve root；12-S8I/11-S7R/08-S7G/12-S3B 已让 full-AOT writer 继续拒绝 TypeSpec-only generic root，要求 generic instantiation identity；manifest 动态泛型实例、注解驱动反射保留和完整诊断仍待后续 |

## 9. 不变量校验

- **C 单一真相**：裁剪决策基于唯一 token/layout 图；可达性结果是 `08`/`10`/`11` 的共同输入，不各自重算。
- **D 环境隔离**：裁剪不改变 typed 函数体形态（`07`），只决定「生成哪些」，不影响「怎么生成」。
- hybrid 安全：默认保留 deopt 兜底，裁剪激进但不致运行期不可恢复的缺失（除显式 full-AOT）。

## 状态与产出记录

> 落地每个阶段或切片时在此追加：时间戳 · 切片号 · 状态 · 完成项目 · RED/GREEN · 测试结果 · 备注。

- 2026-06-27 08:35:30 +08:00 · 12-S7ZS / 11-S7Y zrp metadata version check ·
  状态：12-S7/11-S7 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制、field/default-value backed constant-pool remap、cross-module
  export-token publication/rewrite、完整 metadata sweep/pruning 和 11-S6 运行时 ABI 漂移版本检查仍待后续。
  完成项目：新增 CLI `--check-zrp-metadata-version <file>` 只读工具，读取 `.zrp` metadata header
  前缀并输出 actual/expected magic、version、headerBytes、sectionCount；当前 header shape 经
  完整 header 校验后输出 `status=ok`，版本或头形状不匹配输出 `status=unsupported` 并返回失败。
  解析层禁止该模式与 run/compile/debug/output modifiers 混用。
  RED/GREEN：RED 先由 CLI args 测试要求 version-check mode/path 后旧 command 结构缺 enum/字段而
  编译失败；随后 zrp metadata dump 测试要求 version-check summary/path API 后链接失败。GREEN 后
  `cli_args` 与 `cli_zrp_metadata_dump` 通过。
  验证：WSL gcc、WSL clang 与 Windows MSVC Debug 均构建 `zr_vm_cli_executable`；同组可执行测试通过；
  focused CTest `cli_args|cli_zrp_metadata_dump` 三套环境均为 2/2。WSL gcc 与 Windows MSVC help
  输出覆盖新增帮助文本。
  产出：`tests/acceptance/2026-06-27-aot-11-s7y-zrp-metadata-version-check.md`。
  备注：本切片只提供 standalone metadata header version/shape check，不声明完整 trim analyzer、
  annotation-driven policy、cross-module export-token rewrite、constant literal/default-value remap、
  11-S6 runtime ABI 漂移 deopt 或完整 metadata sweep/pruning 完成。

- 2026-06-27 08:14:35 +08:00 · 12-S7ZR / 11-S7X zrp metadata diff summary ·
  状态：12-S7/11-S7 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制、field/default-value backed constant-pool remap、cross-module
  export-token publication/rewrite、完整 metadata sweep/pruning 和版本检查仍待后续。
  完成项目：新增 CLI `--diff-zrp-metadata <before> <after>` 只读工具，读取并校验两个
  `.zrp` metadata header，输出 version/headerBytes/sectionCount 的 before/after，以及
  12 个 section 的 bytes/count before/after/removed diff summary；增长场景 removed 计数归零，
  避免无符号下溢。解析层禁止该模式与 run/compile/debug/output modifiers 混用。
  RED/GREEN：RED 先由 CLI args 测试要求 diff mode 和 before/after path 后旧 command
  结构缺 enum/字段而编译失败；随后 zrp metadata dump 测试要求 diff summary/path API 后链接失败。
  GREEN 后 `cli_args` 与 `cli_zrp_metadata_dump` 通过。
  验证：WSL gcc、WSL clang 与 Windows MSVC Debug 均构建 `zr_vm_cli_executable`；同组可执行测试通过；
  focused CTest `cli_args|cli_zrp_metadata_dump` 三套环境均为 2/2。WSL gcc 还运行
  `zr_vm_cli --help` 覆盖新增帮助文本。
  产出：`tests/acceptance/2026-06-27-aot-11-s7x-zrp-metadata-diff-summary.md`。
  备注：本切片只提供 standalone metadata diff summary，不声明完整 trim analyzer、annotation-driven
  policy、cross-module export-token rewrite、constant literal/default-value remap、版本兼容检查或
  完整 metadata sweep/pruning 完成。

- 2026-06-27 07:48:22 +08:00 · 12-S7ZQ / 11-S7W zrp metadata dump summary ·
  状态：12-S7/11-S7 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制、field/default-value backed constant-pool remap、cross-module
  export-token publication/rewrite、dump diff 和版本检查仍待后续。
  完成项目：新增 CLI `--dump-zrp-metadata <file>` 只读工具，读取并校验 `.zrp` metadata
  header，输出 version/headerBytes/sectionCount 与 12 个 section 的
  bytes/count/elementSize/offset summary；解析层禁止它与 run/compile/debug/output modifiers 混用。
  RED/GREEN：RED 先由 CLI args 测试要求 dump mode/path 后旧 command 结构缺 enum/字段而编译失败；
  随后新增 dump summary 目标后 CMake 因缺少 `zrp_metadata_dump.c` 失败。GREEN 后
  `cli_args` 与 `cli_zrp_metadata_dump` 通过。
  验证：WSL gcc、WSL clang 与 Windows MSVC Debug 均构建 `zr_vm_cli_executable`；同组可执行测试通过；
  focused CTest `cli_args|cli_zrp_metadata_dump` 三套环境均为 2/2。
  产出：`tests/acceptance/2026-06-27-aot-11-s7w-zrp-metadata-dump-summary.md`。
  备注：本切片只提供 standalone dump summary，不声明 metadata diff、版本兼容检查、完整
  trim analyzer 或 metadata sweep/pruning 完成。

- 2026-06-27 07:20:00 +08:00 · 12-S7ZP zrp section count delta markers ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制、
  field/default-value backed constant-pool remap、cross-module export-token publication/rewrite
  和 dump/diff 仍待后续。
  完成项目：`SZrAotZrpMetadataSizeStats` 现在采样每个 `.zrp` metadata section 的 row/count；
  `backend_aot_write_zrp_metadata_size_stats()` 输出 `aot_size.zrpMetadataSectionCounts.<section>`，
  `backend_aot_write_code_stripping_zrp_metadata_size_deltas()` 输出
  `code_stripping.zrpMetadataSectionCounts.<section>Before/After/Removed`，为后续 dump/diff
  同时提供字节维度与 row/count 维度。
  RED/GREEN：RED 为 direct size-delta 测试新增 count marker 后，旧 stats 结构没有 section count
  字段，WSL gcc 编译失败；GREEN 后 size-delta 2/0、source contracts 21/0、code stripping 5/0、
  direct zrp pruning 5/0、pool pruning 4/0、export-token remap 2/0。
  验证：WSL gcc、WSL clang 与 Windows MSVC Debug 同组可执行测试均通过；三套环境 focused CTest
  `aot_c_zrp_metadata_size_deltas|aot_c_zrp_metadata_export_token_remap|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping`
  均为 5/5。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zp-zrp-section-count-delta-markers.md`。
  备注：本切片只补 generated-C 注释级 section count 可观测性，不改变 `.zrp` ABI，也不声明
  annotation-driven policy、cross-module export-token rewrite、constant literal/default-value remap
  或独立 dump/diff 工具完成。

- 2026-06-27 06:51:55 +08:00 · 12-S7ZO zrp section-level trim delta markers ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制、
  field/default-value backed constant-pool remap、cross-module export-token publication/rewrite
  和 dump/diff 仍待后续。
  完成项目：`backend_aot_write_code_stripping_zrp_metadata_size_deltas()` 在既有 zrp metadata
  总量、token-record、definition-table、pool delta marker 外，新增 12 个 section 的
  `code_stripping.zrpMetadataSectionBytes.<section>Before/After/Removed` marker，覆盖
  tokenRecords、TypeDef/MethodDef/FieldDef、GenericParam/Constraint、TypeSpec/MethodSpec、
  ModuleRef、string/signature/constant pool，便于后续 dump/diff 工具直接定位裁剪收益来源。
  RED/GREEN：RED 为新增 `zr_vm_aot_c_zrp_metadata_size_deltas_test` 后，旧实现缺少
  `zrpMetadataSectionBytes.tokenRecordsBefore` 等 marker，WSL gcc 失败 1/1；GREEN 后
  size-delta 1/0、source contracts 21/0、code stripping 5/0、direct zrp pruning 5/0、
  pool pruning 4/0、export-token remap 2/0。
  验证：WSL gcc、WSL clang 与 Windows MSVC Debug 同组可执行测试均通过；三套环境 focused CTest
  `aot_c_zrp_metadata_size_deltas|aot_c_zrp_metadata_export_token_remap|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping`
  均为 5/5。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zo-zrp-section-delta-markers.md`。
  备注：本切片只补 generated-C 注释级 section delta 可观测性，不声明完整 trim analyzer、
  annotation-driven policy、cross-module export-token rewrite、constant literal/default-value
  remap 或独立 dump/diff 工具完成。

- 2026-06-27 06:30:32 +08:00 · 12-S7ZN export member-token remap surface ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制、
  field/default-value backed constant-pool remap、cross-module export-token publication/rewrite
  和 dump/diff 仍待后续。
  完成项目：`backend_aot_c_zrp_metadata_remap.{h,c}` 新增
  `backend_aot_c_zrp_remap_export_member_token()`，复用现有 MethodDef/FieldDef compacting
  规则，把保留的 exported `MEMBER_DEF` token 映射到裁剪后 RID，并拒绝已经被 MethodDef
  pruning 删除的导出方法 token；字段导出 token 会排在 retained MethodDef 之后重新编号。
  RED/GREEN：RED 为 direct pruning 测试先要求导出方法旧 RID2 在 RID1/RID3 删除后映射到
  compacted RID1，旧实现缺少 helper，WSL gcc 链接失败；GREEN 后独立
  `test_aot_c_zrp_metadata_export_token_remap.c` 覆盖 retained MethodDef export token 与
  FieldDef export token remap，export-token remap 2/0、direct zrp pruning 5/0、pool pruning
  4/0、code stripping 5/0、source contracts 21/0。
  验证：WSL gcc、WSL clang 与 Windows MSVC Debug 同组通过，focused CTest
  `aot_c_zrp_metadata_export_token_remap|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping`
  4/4。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zn-export-member-token-remap.md`。
  备注：本切片只提供 emitted zrp metadata pruning 后的 exported member token remap surface；
  尚未把该 remap 写回跨模块 `.zrp` export manifest/table，也不声明完整 metadata sweep、
  annotation-driven policy、constant literal/default-value remap 或 dump/diff 完成。

- 2026-06-27 05:57:45 +08:00 · 12-S7ZM zrp pool compaction without MethodDef pruning ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制、
  export token、field/default-value backed constant-pool remap 和 dump/diff 仍待后续。
  完成项目：emitted zrp metadata pruner 不再只因 `retainedMethodDefCount == methodDefCount`
  就跳过 blob rebuild；现在先构建 signature/string remap，并通过各 remap 模块的 identity helper
  判断是否可跳过，仅在 token/table counts、constant-pool retained bytes、signature remap 和 string
  remap 都是 identity 时跳过。这样没有 MethodDef 删除时，
  string/signature pool compaction 仍能生效。
  RED/GREEN：RED 为 direct pool-pruning fixture 保留两个 MethodDef 但要求 duplicate retained
  string compaction 后，旧实现 `ownedBlob` 为空，focused WSL gcc pool pruning 失败 1/4；
  GREEN 后 pool pruning 4/0、direct zrp pruning 5/0、code stripping 5/0、source contracts 21/0。
  验证：WSL gcc 与 WSL clang 同组通过，focused CTest
  `aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping` 2/2；Windows MSVC Debug 同组通过，
  focused CTest 2/2。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zm-zrp-pool-compaction-without-method-pruning.md`。
  备注：本切片只修正 zrp pool compaction 的触发条件，不声明完整 metadata sweep、constant
  literal/default-value remap、export token 或 trim analyzer 完成。

- 2026-06-27 05:46:58 +08:00 · 12-S7ZL zrp string-pool duplicate slice compaction ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制、
  export token、field/default-value backed constant-pool remap 和 dump/diff 仍待后续。
  完成项目：`backend_aot_c_zrp_metadata_string_pool.c` 的 string remap 现在按内容合并 retained
  NUL-terminated string slices：当两个保留 row 指向不同旧 offset 但字节内容完全相同时，只写入一份
  emitted string-pool payload；每个旧 offset 仍有 remap entry，因此 TypeDef/MethodDef/FieldDef/
  GenericParam/ModuleRef 的 offset rewrite 仍可解析原始旧 offset。
  RED/GREEN：RED 为 `test_aot_c_zrp_metadata_pool_pruning.c` 新增 duplicate retained string fixture
  后，旧 offset-only remap 使 focused WSL gcc pool pruning 失败 1/3（Expected 540 Was 547）；
  GREEN 后 pool pruning 3/0、direct zrp pruning 5/0、code stripping 5/0、source contracts 21/0。
  验证：WSL gcc 与 WSL clang 同组通过，focused CTest
  `aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping` 2/2；Windows MSVC Debug 同组通过，
  focused CTest 2/2。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zl-zrp-string-pool-duplicate-slice-compaction.md`。
  备注：本切片只把已有 string-pool sweep 从 old-offset dedupe 收紧到 content-level duplicate
  slice interning；不声明完整 metadata sweep、constant literal/default-value remap、export token 或
  trim analyzer 完成。

- 2026-06-27 05:31:19 +08:00 · 12-S7ZK trim warning sourceFile quoted escaping ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制、
  export token 和 dump/diff 仍待后续。
  完成项目：runtime fallback trim warning comment 的 `sourceFile` 字段现在输出为 quoted/escaped
  marker：普通文件为 `sourceFile="dynamic_deopt_bridge.zr"`，路径中的反斜杠、双引号和控制字符会
  分别写为 `\\`、`\"`、`\n`/`\r`/`\t` 或 `\xNN`，避免带空格、引号或反斜杠的源文件名破坏
  后续机器解析。
  RED/GREEN：RED 为 `test_aot_c_dynamic_deopt_bridge_smoke.c` 把既有 warning 期望改为 quoted
  `sourceFile`，并新增 `src\quoted "module".zr` 转义 fixture 后，旧 `%s` 原样输出导致 focused WSL gcc
  dynamic deopt bridge smoke 失败 4/7；GREEN 后 dynamic deopt bridge smoke 7/0、source contracts 21/0、
  code stripping 5/0。
  验证：WSL gcc dynamic deopt bridge smoke 7/0、source contracts 21/0、code stripping 5/0、CTest
  `aot_c_code_stripping` 1/1；WSL clang 同组通过；Windows MSVC Debug dynamic deopt bridge smoke
  0 failures/7 ignored、source contracts 21/0、code stripping 5/0、CTest `aot_c_code_stripping` 1/1。
  `git diff --check` 退出 0，仅报告既有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zk-trim-warning-source-file-escaping.md`。
  备注：本切片只让已有 warning 的 source file 字段可可靠解析；`@requires_unreferenced_code`、
  reflection data-flow annotation、annotation-based warning suppression/promotion 和完整 analyzer 仍待后续。

- 2026-06-27 05:19:43 +08:00 · 12-S7ZJ trim warning reasonFlag marker ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制、
  export token 和 dump/diff 仍待后续。
  完成项目：runtime fallback trim warning comment 现在在 textual `reason=...` 之外同步输出
  `reasonFlag=<mask>`，该值复用 `ZR_AOT_RUNTIME_FALLBACK_WARNING_*` suppression mask 映射，便于
  诊断消费端把单条 warning 与 reason-mask suppression/统计精确对应。dynamic-call warning 输出
  `reasonFlag=1`，dynamic-value-access warning 输出 `reasonFlag=2`。
  RED/GREEN：RED 为 `test_aot_c_dynamic_deopt_bridge_smoke.c` 将现有 warning 字符串期望升级为
  `reasonFlag=... reason=...` 后，旧生成器缺少字段，focused WSL gcc dynamic deopt bridge smoke
  失败 3/6；GREEN 后 dynamic deopt bridge smoke 6/0、source contracts 21/0、code stripping 5/0。
  验证：WSL gcc dynamic deopt bridge smoke 6/0、source contracts 21/0、code stripping 5/0；WSL clang
  同组通过；Windows MSVC Debug dynamic deopt bridge smoke 0 failures/6 ignored、source contracts 21/0、
  code stripping 5/0，CTest `aot_c_code_stripping` 1/1。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zj-trim-warning-reason-flag.md`。
  备注：本切片只增强已有 runtime-fallback warning 的 reason 可消费性；`@requires_unreferenced_code`、
  reflection data-flow annotation、warning suppression annotation 和完整 analyzer 仍待后续。

- 2026-06-27 05:07:26 +08:00 · 12-S7ZI zrp constant-pool orphan sweep ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，trim analyzer、attribute/annotation 抑制、
  export token 和 dump/diff 仍待后续。
  完成项目：在当前 11-S1 zrp row ABI 没有 constant-pool offset 字段的前提下，emitted zrp
  MethodDef pruning 会把 constantPool section 视为无 retained 引用的 orphan payload；pruned header
  rebuild 新增 retained constant-pool byte 参数并在当前路径传入 0，使 after-trim `constantPool`
  的 byteLength/count/elementSize 均为 0，raw section copy 自动 no-op。code stripping MethodDef
  pruning 集成 fixture 同步校验 after-trim constantPool 从 5 bytes 降到 0，并把这 5 bytes 计入
  pool/metadata removed delta。
  RED/GREEN：RED 为 `test_aot_c_zrp_metadata_pool_pruning.c` 新增 orphan constant-pool fixture 后，
  旧实现仍保留 5 bytes，focused WSL gcc pool pruning 失败 1/2（Expected 488 Was 493）；GREEN 后
  pool pruning 2/0、direct zrp pruning 5/0、code stripping 5/0。
  验证：WSL gcc pool pruning 2/0、direct zrp pruning 5/0、code stripping 5/0、source contracts 21/0、
  frame setup 1/0、typed scalar 1/0、shared-library smoke 8/0，focused CTest 4/4；WSL clang 同组通过，
  仍仅有既有 generated generic-conversion `-Wlogical-not-parentheses` warning；Windows MSVC Debug
  pool pruning 2/0、direct zrp pruning 5/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、
  typed scalar 0 failures/1 ignored、shared-library smoke 0 failures/8 ignored，focused CTest 4/4。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zi-zrp-constant-pool-orphan-sweep.md`。
  备注：本切片只关闭当前无 constant 引用 row 模型下的 orphan payload sweep；若后续 ABI 增加
  constantPool offset/length 字段，还需要新增 retained constant slice remap/compaction。

- 2026-06-27 04:42:55 +08:00 · 12-S7ZH zrp string-pool sweep/compaction ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，trim analyzer、attribute/annotation 抑制、
  constant pool sweep/compaction、export token 和 dump/diff 仍待后续。
  完成项目：新增 `backend_aot_c_zrp_metadata_string_pool.{h,c}`，把 retained TypeDef、retained MethodDef、
  FieldDef、retained GenericParam 与 ModuleRef row 引用的 NUL-terminated string slices 收集、按旧 offset 去重、
  compacted string pool copy 和 row string-offset remap 从 prune orchestration 中拆出；新增
  `backend_aot_c_zrp_metadata_sections.{h,c}` 承载共享 section lookup/layout/copy helper，让
  `backend_aot_c_zrp_metadata_prune.c` 继续聚焦剪枝编排。code stripping MethodDef pruning 集成 fixture
  同步校验 after-trim stringPool 从 6 bytes 降到 1 byte，pool delta 同时统计 string 与 signature pool 移除量。
  RED/GREEN：RED 为新 `test_aot_c_zrp_metadata_pool_pruning.c` 要求 MethodDef 裁剪后 string pool
  40->25、保留 MethodDef name offset 重映射、移除 method/unused string 后，旧实现仍保留原池，
  focused WSL gcc pool pruning 失败 1/1；GREEN 后 pool pruning 1/0、zrp pruning 5/0、code stripping 5/0，
  source contracts 21/0，并由 source contract 锁定 section/string-pool helper 边界。
  验证：WSL gcc pool pruning 1/0、direct zrp pruning 5/0、code stripping 5/0、source contracts 21/0、
  frame setup 1/0、typed scalar 1/0、shared-library smoke 8/0，focused CTest 4/4；WSL clang 同组通过，
  仍仅有既有 generated generic-conversion `-Wlogical-not-parentheses` warning；Windows MSVC Debug
  pool pruning 1/0、direct zrp pruning 5/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、
  typed scalar 0 failures/1 ignored、shared-library smoke 0 failures/8 ignored，focused CTest 4/4。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zh-zrp-string-pool-compaction.md`。
  备注：本切片完成当前 retained-row 集合的 string-pool compaction/remap；constant pool、跨模块/export token、
  annotation promotion/suppression、完整 trim analyzer 和 dump/diff 后续再闭环。

- 2026-06-27 03:49:57 +08:00 · 12-S7ZG zrp MethodSpec signature-pool rewrite/compaction ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，trim analyzer、attribute/annotation 抑制、
  非 signature pool sweep/compaction、export token 和 dump/diff 仍待后续。
  完成项目：新增 `backend_aot_c_zrp_metadata_signature.{h,c}`，把 retained signature blob slice
  收集、去重、compacted pool copy、signature blob offset remap、stable hash recomputation 和
  MethodSpec `GENERIC_INST(MEMBER_REF methodToken, args...)` signature rewrite 从 prune orchestration 中拆出；
  emitted zrp pruning 现在会按保留 token/type/method/field/constraint/typespec/methodspec 引用压缩
  signature blob pool，重写保留 MethodSpec 的 method token payload，并同步更新 token record / MethodSpec
  offset 与 hash。`test_aot_c_code_stripping.c` 的 MethodDef pruning 集成 fixture 同步校验 after-trim
  pool bytes 从 18 降到 11、signatureBlobPool 从 7 降到 0。
  RED/GREEN：RED 为 MethodSpec-present direct zrp fixture 要求 signature blob pool 30->15、
  MethodSpec signature 内 method token RID 2->1、token record/MethodSpec hash 重算后，旧实现仍保留原 pool，
  focused WSL gcc zrp pruning 失败 1/5；GREEN 后 zrp pruning 5/0，并由 source contract 锁定 signature module API。
  验证：WSL gcc direct zrp pruning 5/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、
  typed scalar 1/0、shared-library smoke 8/0，focused CTest 3/3；WSL clang 同组通过，仍仅有既有
  generated generic-conversion `-Wlogical-not-parentheses` warning；Windows MSVC Debug direct zrp pruning 5/0、
  code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 0 failures/1 ignored、
  shared-library smoke 0 failures/8 ignored，focused CTest 3/3。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zg-zrp-methodspec-signature-pool-rewrite.md`。
  备注：本切片完成 signature blob pool compaction/rewrite 的 retained-slice 安全集；完整 metadata sweep、
  string/constant pool sweep、跨模块/export token 和 annotation/dump-diff 后续再闭环。

- 2026-06-26 08:38:24 +08:00 · 12-S7ZF zrp MethodSpec method-token cascade ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，signature-pool compaction/rewrite、
  pool compaction、attribute/annotation 抑制、export token 和 dump/diff 仍待后续。
  完成项目：emitted zrp MethodDef pruning 现在可处理 MethodSpec rows；MethodSpec `methodToken`
  指向被删除 MethodDef 时整行删除，指向保留 MethodDef 时随 compacted `MEMBER_DEF` RID 重映射；
  MethodSpec section 的 count/byteLength 随剪枝压缩，当前 signature blob pool 原样保留，以避免在未实现池重写前破坏
  `instantiationBlobOffset`/`instantiationBlobLength`/`instantiationHash`。
  RED/GREEN：RED 为带 3 个 MethodDef / 2 个 MethodSpec / signature blob pool 的 direct zrp fixture 要求 owned pruned blob 后失败 1/5；
  GREEN 后保留 MethodSpec 的 `methodToken` 从旧 RID 2 改写为 compact RID 1，被删除 MethodDef 的 MethodSpec 被移除，
  signature blob pool 6 bytes 原样保留。
  验证：WSL gcc direct zrp pruning 5/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 1/0、
  shared-library smoke 8/0，focused CTest 3/3；WSL clang 同组通过，仍有既有 generated generic-conversion
  `-Wlogical-not-parentheses` warning；Windows MSVC Debug direct zrp pruning 5/0、code stripping 5/0、
  source contracts 21/0、frame setup 1/0、typed scalar 0 failures/1 ignored、
  shared-library smoke 0 failures/8 ignored，focused CTest 3/3。
  产出：`tests/acceptance/2026-06-26-aot-12-s7zf-zrp-methodspec-method-token-cascade.md`。
  备注：这是 MethodSpec method-token 级联，不声明 MethodSpec signature-pool rewrite、pool compaction 或完整 metadata sweep 完成。

- 2026-06-26 08:15:19 +08:00 · 12-S7ZE zrp GenericParamConstraint cascade ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，MethodSpec signature-pool rewrite、
  pool compaction、attribute/annotation 抑制、export token 和 dump/diff 仍待后续。
  完成项目：emitted zrp MethodDef pruning 现在可处理 GenericParamConstraint rows；被删除 MethodDef 拥有的 GenericParam
  及其 constraints 一并移除，保留 GenericParam 的 `firstConstraintIndex`/`constraintCount` 按压缩后的 constraint section
  重算，constraint row 的 `genericParamIndex` 同步映射到 compacted GenericParam index。
  RED/GREEN：RED 为带 3 个 GenericParam / 4 个 GenericParamConstraint 的 direct zrp fixture 要求 owned pruned blob 后失败 1/4；
  GREEN 后 zrp pruning 4/0，并由 source contract 锁定 constraint copy/remap/count/range 路径。
  验证：WSL gcc/clang direct zrp pruning 4/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、
  typed scalar 1/0、shared-library smoke 8/0，focused CTest 3/3；Windows MSVC Debug direct zrp pruning 4/0、
  code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 0 failures/1 ignored、
  shared-library smoke 0 failures/8 ignored，focused CTest 3/3。
  产出：`tests/acceptance/2026-06-26-aot-12-s7ze-zrp-generic-param-constraint-cascade.md`。
  备注：这是 GenericParam 依赖表的剪枝级联，不声明完整 metadata sweep 或 MethodSpec/pool rewrite 完成。

- 2026-06-26 07:55:51 +08:00 · 12-S7ZD zrp metadata remap module split ·
  状态：12-S7 支持性 refinement 完成；完整 12-S7 仍未关闭，GenericParamConstraint cascade、MethodSpec signature-pool rewrite、
  pool compaction、attribute/annotation 抑制、export token 和 dump/diff 仍待后续。
  完成项目：新增 `backend_aot_c_zrp_metadata_remap.{h,c}`，把 MethodDef/FieldDef 共享 `MEMBER_DEF`
  token 压缩、TokenRecord remap/filter、GenericParam owner remap 与 TypeDef/MethodDef range 压缩从
  `backend_aot_c_zrp_metadata_prune.c` 拆出；prune 模块只保留 section/header/copy/orchestration，行数从 982 降到 549，
  新 remap 模块为 434 行。
  RED/GREEN：本切片是无行为变化的模块边界拆分，复用已通过的 zrp pruning 三个 fixture 和 source-contract module-boundary
  断言作回归保护；GREEN 后 source contract 明确要求 remap header/source 存在并承载 token/range helper。
  验证：WSL gcc direct zrp pruning 3/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 1/0、
  shared-library smoke 8/0，focused CTest 3/3；WSL clang 同组通过，仍仅有既有 generated generic-conversion
  `-Wlogical-not-parentheses` warning；Windows MSVC Debug direct zrp pruning 3/0、code stripping 5/0、source contracts 21/0、
  frame setup 1/0、typed scalar 0 failures/1 ignored、shared-library smoke 0 failures/8 ignored，focused CTest 3/3。
  产出：`tests/acceptance/2026-06-26-aot-12-s7zd-zrp-metadata-remap-module-split.md`。
  备注：这是为后续 GenericParamConstraint/MethodSpec/pool cascade 继续扩展前做的模块边界收敛，不改变当前 emitted zrp metadata pruning 语义。

- 2026-06-26 07:30:55 +08:00 · 12-S7ZC zrp GenericParam owner remap ·
  状态：12-S7/11-S7 交叉子切片完成；完整 12-S7 仍未关闭，GenericParamConstraint cascade、MethodSpec signature-pool rewrite、
  pool compaction、attribute/annotation 抑制、export token 和 dump/diff 仍待后续。
  完成项目：emitted zrp MethodDef pruning 现在可处理无 GenericParamConstraint 的 GenericParam rows；TypeDef-owned 泛型参数保留，
  retained MethodDef/FieldDef-owned 泛型参数 owner token 跟随共享 `MEMBER_DEF` RID remap，被裁剪 MethodDef 拥有的 GenericParam row
  会删除，并重算 TypeDef 与 retained MethodDef 的 `firstGenericParamIndex`/`genericParamCount`。
  RED/GREEN：RED 为 direct zrp fixture 要求 GenericParam-present blob 返回 owned pruned blob 后，旧 GenericParam guard 让 focused
  WSL gcc zrp pruning 失败 1/3；GREEN 后 zrp pruning 3/0，删除 removed MethodDef 拥有的 GenericParam，保留方法/类型泛型参数并重排 range。
  验证：WSL gcc direct zrp pruning 3/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 1/0、
  shared-library smoke 8/0，focused CTest 3/3；WSL clang 同组通过，仍有既有 generated generic-conversion
  `-Wlogical-not-parentheses` warning；Windows MSVC Debug direct zrp pruning 3/0、code stripping 5/0、source contracts 21/0、
  frame setup 1/0、typed scalar 0 failures/1 ignored、shared-library smoke 0 failures/8 ignored，focused CTest 3/3。
  产出：`tests/acceptance/2026-06-26-aot-12-s7zc-zrp-generic-param-owner-remap.md`。
  备注：GenericParam owner/range remap 已覆盖无 constraints 场景；GenericParamConstraint、MethodSpec、pool rewrite、annotation
  promotion 和 dump/diff 仍按计划后续推进。

- 2026-06-26 07:14:57 +08:00 · 12-S7ZB zrp FieldDef member-token remap ·
  状态：12-S7/11-S7 交叉子切片完成；完整 12-S7 仍未关闭，GenericParam/MethodSpec cascade、pool compaction、
  attribute/annotation 抑制、export token 和 dump/diff 仍待后续。
  完成项目：emitted zrp MethodDef pruning 现在可以处理含 FieldDef rows 的 blob；MethodDef/FieldDef 共用的 `MEMBER_DEF`
  RID 空间会在删除 MethodDef 后重排，FieldDef row token 与 token record 内 `token`、`relatedToken`、`ownerToken`、
  `targetMetadataToken`、`targetSignatureToken` 的 FieldDef 引用会同步改写到保留 MethodDef 之后。
  RED/GREEN：RED 为 direct zrp fixture 要求 FieldDef-present blob 也返回 owned pruned blob 后，旧 FieldDef guard 让 focused
  WSL gcc zrp pruning 失败 1/2；GREEN 后 zrp pruning 2/0，FieldDef token 从 `MEMBER_DEF` RID 3 下移到 RID 2，并删除已裁剪 MethodDef
  的 token record。
  验证：WSL gcc direct zrp pruning 2/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 1/0、
  shared-library smoke 8/0，focused CTest 3/3；WSL clang 同组通过，仍有既有 generated generic-conversion
  `-Wlogical-not-parentheses` warning；Windows MSVC Debug direct zrp pruning 2/0、code stripping 5/0、source contracts 21/0、
  frame setup 1/0、typed scalar 0 failures/1 ignored、shared-library smoke 0 failures/8 ignored，focused CTest 3/3。
  产出：`tests/acceptance/2026-06-26-aot-12-s7zb-zrp-fielddef-member-token-remap.md`。
  备注：FieldDef shared member-token remap 已关闭；GenericParam owner、MethodSpec method token、pool rewrite、annotation
  promotion 和 dump/diff 仍按计划后续推进。

- 2026-06-26 06:58:15 +08:00 · 12-S7ZA zrp token-record MethodDef pruning/remap ·
  状态：12-S7/11-S7 交叉子切片完成；完整 12-S7 仍未关闭，FieldDef token remap、GenericParam/MethodSpec cascade、pool compaction、
  attribute/annotation 抑制和 dump/diff 仍待后续。
  完成项目：12-S7Z 的 emitted zrp MethodDef row pruning 现在同步处理 tokenRecords section；
  保留 MethodDef rows 会重写为紧凑 `MEMBER_DEF` RID，token record 的 `token`、`relatedToken`、`ownerToken`、
  `targetMetadataToken`、`targetSignatureToken` 中指向保留 MethodDef 的字段会同步重映射，指向被裁剪 MethodDef 的 token record 会被删除。
  由于当前 method/field 共用 `MEMBER_DEF` token 空间，含 FieldDef row 的 blob 会保守保留原始元数据，避免只移动 MethodDef 而破坏字段 token。
  RED/GREEN：RED 为新增 `zr_vm_aot_c_zrp_metadata_pruning_test` 后 focused WSL gcc 失败 1/1，
  旧保护遇到 token record `MEMBER_DEF` 引用时返回原始 blob，`ownedBlob` 为空；GREEN 后 zrp 直测 2/0，并保留 FieldDef guard。
  验证：WSL gcc direct zrp pruning 2/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 1/0、
  shared-library smoke 8/0，focused CTest 3/3；WSL clang 同 direct set 与 focused CTest 3/3（仍有既有 generated generic-conversion
  `-Wlogical-not-parentheses` warning）；Windows MSVC Debug direct zrp pruning 2/0、code stripping 5/0、source contracts 21/0、
  frame setup 1/0、typed scalar 0 failures/1 ignored、shared-library smoke 0 failures/8 ignored，focused CTest 3/3。
  产出：`tests/acceptance/2026-06-26-aot-12-s7za-zrp-token-record-methoddef-pruning.md`。
  备注：Windows shared-lib 构建下该 direct unit test 将内部 prune module 编入测试目标；生产 API 仍保持内部 backend 边界。

- 2026-06-26 06:30:39 +08:00 · 12-S7Z zrp MethodDef metadata pruning ·
  状态：12-S7/11-S7 交叉子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略和更完整的跨表/池 metadata sweep/pruning 仍待后续。
  完成项目：opt-in AOT C code stripping 现在在 reachability filter 后准备 emitted embedded zrp metadata blob；
  对安全子集中的 MethodDef rows，若 `functionIndex` 不在裁剪后的 `SZrAotFunctionTable` 中，则从 emitted blob 删除，
  重新布局 zrp section offsets，并调整 TypeDef 的 method range。descriptor embedded length、`aot_size.embeddedModuleBytes`、
  zrp after stats 和 before/after/removed deltas 均读取实际发射 blob。
  RED/GREEN：RED 为 code-stripping zrp fixture 新增 retained/removable MethodDef rows 后，
  generated C 仍输出 before=after、removed=0 和原始 descriptor length，focused WSL gcc 失败 1/5；GREEN 后
  MethodDef 从 72 bytes 降到 36 bytes，zrp metadata 从 446 bytes 降到 410 bytes，definition table removed=36。
  验证：WSL gcc/clang direct code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 1/0、
  shared-library smoke 8/0，focused CTest 2/2；Windows MSVC Debug direct code stripping 5/0、source contracts 21/0、
  frame setup 1/0、typed scalar 0 failures/1 ignored、shared-library smoke 0 failures/8 ignored，focused CTest 2/2。
  产出：`tests/acceptance/2026-06-26-aot-12-s7z-zrp-methoddef-metadata-pruning.md`。
  备注：本切片只裁剪无 member-token/generic/MethodSpec 依赖的 MethodDef row 安全集；token record 重写、
  GenericParam/MethodSpec 跟随剪枝、pool compaction、跨模块 token remapping、注解保留和 dump/diff 仍待后续。

- 2026-06-26 06:00:16 +08:00 · 12-S7Y default-min reflection metadata policy ·
  状态：12-S7/10-S1/11-S7 交叉子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略和实际 metadata sweep/pruning 仍待后续。
  完成项目：AOT C writer 现在通过 shared option helper 选择 generated MethodInfo 的
  `reflectionMetadataLevel`；默认/非裁剪产物保持 `ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING`，
  opt-in `enableCodeStripping` 产物降为 `ZR_AOT_REFLECTION_METADATA_NONE`，并在文件头输出
  `/* metadata_policy.reflectionLevel = 0/1 */`。method metadata byte sampling 复用同一 policy，
  因此 `code_stripping.methodMetadataGeneratedBytesAfter` 仍与实际发射的 `aot_size.methodMetadataBytesTotal`
  保持一致。
  RED/GREEN：RED 为 `zr_vm_aot_c_code_stripping_test` 在普通不可达 child 裁剪 fixture 中新增
  `metadata_policy.reflectionLevel = 0` 和 MethodInfo `NONE` 断言后失败 1/4；GREEN 后新增
  `backend_aot_option_reflection_metadata_level()`，并把 policy 穿过 emitter 与 method metadata emitter。
  验证：WSL gcc direct code stripping 4/0、source contracts 21/0、frame setup contract 1/0、
  typed scalar 1/0、shared-library smoke 8/0，focused CTest 2/2；WSL clang 同组 direct 4/0、21/0、1/0、
  1/0、8/0，focused CTest 2/2；Windows MSVC Debug 同组 direct code stripping 4/0、
  source contracts 21/0、frame setup 1/0、typed scalar 1 test 0 failures/1 ignored、
  shared-library smoke 8 tests 0 failures/8 ignored，focused CTest 2/2。
  产出：`tests/acceptance/2026-06-26-aot-12-s7y-default-min-reflection-metadata-policy.md`。
  备注：本切片关闭 generated MethodInfo reflection level 的默认最小 policy 接线；尚未重写 embedded zrp
  metadata pool，也未实现注解驱动 `DESCRIPTION` 提升或 analyzer 级 warning suppression。

- 2026-06-26 05:41:31 +08:00 · 12-S7X release symbol stripping CLI policy ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制策略、
  实际 metadata sweep/pruning 和默认最小 metadata policy 仍待后续。
  完成项目：`ZrCli_Compiler_ApplyProjectAotWriterOptions()` 现在把 `.zrp` `aotMode: "full-aot"`
  同时映射为 `requireFullAot = ZR_TRUE` 和 `stripGeneratedSymbols = ZR_TRUE`；缺省/`hybrid`
  映射为二者均 false。CLI `--emit-aot-c` 的 full-AOT project fixture 生成 C 现在包含
  `/* symbol_stripping.generatedSymbols = 1 */`。
  RED/GREEN：RED 为 `zr_vm_cli_project_incremental_test` 新增 full-AOT/hybrid writer option
  expectations 和 full-AOT generated-C marker expectation 后失败 3/11；GREEN 为 existing project AOT
  option helper 增加 `stripGeneratedSymbols` 策略映射。
  验证：WSL gcc direct CLI incremental 11/0、generic call typed 7/0、LLVM symbol stripping 2/0，
  focused CTest 3/3；WSL clang 同组 direct 11/0、7/0、2/0，focused CTest 3/3；Windows MSVC Debug
  同组 direct CLI 11/0、generic call typed 7 tests 0 failures/3 ignored、LLVM 2/0，focused CTest 3/3。
  产出：`tests/acceptance/2026-06-26-aot-12-s7x-release-symbol-stripping-cli-policy.md`。
  备注：本切片把当前 CLI 可见 release policy 定义为已有 full-AOT project mode，不新增单独
  `release` manifest 字段；`compiler.c` 与 `test_cli_project_incremental.c` 已超过 1000 行，但本次只扩展
  现有策略断言/映射，未新增模块职责，后续继续扩 CLI AOT 策略时应拆出 AOT project-mode 测试边界。

- 2026-06-26 05:30:36 +08:00 · 12-S7W LLVM generated-symbol stripping parity ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制策略、
  实际 metadata sweep/pruning、默认最小 metadata policy 和 release-mode 默认策略/CLI 接线仍待后续。
  完成项目：LLVM writer 现在复用 `stripGeneratedSymbols` option；生成 `.ll` 头部输出
  `; symbol_stripping.generatedSymbols = 0/1`。默认保持 `@zr_aot_fn_<flatIndex>`；开启后 generated
  private function definition、function thunk table、entry thunk 和静态 direct-call references 改为稳定
  `@zr_fn_g<flatIndex>`，公开导出 `@ZrVm_GetAotCompiledModule` 不变。
  RED/GREEN：RED 为新增 `zr_vm_aot_llvm_symbol_stripping_test`，先要求 marker 与 stripped
  `@zr_fn_g0/@zr_fn_g1` 后失败 2/2；GREEN 后新增 LLVM function-symbol formatter 并把 strip flag
  穿过 emitter/function body/static direct call/module artifacts。
  验证：WSL gcc direct LLVM 2/0、code stripping 4/0、generic call typed 7/0，focused CTest 3/3；
  WSL clang 同组 direct 2/0、4/0、7/0，focused CTest 3/3；Windows MSVC Debug 同组 direct
  LLVM 2/0、code stripping 4/0、generic call typed 7 tests 0 failures/3 ignored，focused CTest 3/3。
  产出：`tests/acceptance/2026-06-26-aot-12-s7w-llvm-generated-symbol-stripping.md`。
  备注：本切片只剥离 LLVM private generated function symbols/references，不重命名 basic-block labels；
  public ABI/export symbols 继续稳定可见。

- 2026-06-26 05:06:33 +08:00 · 12-S7V method metadata generated byte trim delta ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制策略、
  实际 metadata sweep/pruning、默认最小 metadata policy、release-mode 默认策略/CLI 接线和跨后端符号剥离审计仍待后续。
  完成项目：AOT C 文件头部新增
  `code_stripping.methodMetadataGeneratedBytesBefore/After/Removed`；`backend_aot_write_c_method_infos()`
  返回实际发射的 method signature/info metadata byte total，新增
  `backend_aot_c_method_metadata_generated_bytes_referenced()` 用临时文件按当前 function table 采样裁剪前后
  generated-C method metadata 字节量；code-stripping 测试校验 before >= after、removed = before - after，
  且 after 等于 `aot_size.methodMetadataBytesTotal`。普通不可达 child 裁剪路径要求 removed > 0，
  export/manifest root 保留路径要求 removed = 0。
  RED/GREEN：RED 为 `zr_vm_aot_c_code_stripping_test` 先因缺少
  `code_stripping.methodMetadataGeneratedBytesBefore` marker 失败 3/4，source contract 同步要求
  emitter/header/source 中的 method metadata byte-delta plumbing。GREEN 后 emitter 在 reachability
  过滤前后采样 method metadata generated bytes 并输出 before/after/removed markers。
  验证：WSL gcc 直接 code stripping 4/0、generic call typed 7/0、source contracts 21/0、
  frame setup contract 1/0，CTest `aot_c_code_stripping|aot_c_generic_call_typed|aot_c_frame_setup_contracts`
  2/2；WSL clang direct code stripping 4/0、generic call typed 7/0、source contracts 21/0、
  frame setup contract 1/0，focused CTest 2/2；Windows MSVC Debug 同组通过。产出：
  `tests/acceptance/2026-06-26-aot-12-s7v-method-metadata-generated-byte-trim-delta.md`。
  备注：本切片只统计 generated-C MethodInfo/signature 发射字节的 trim delta；尚未执行真实 metadata
  pool sweep/pruning，也不改变 runtime reflection metadata policy。

- 2026-06-26 04:49:28 +08:00 · 12-S7U release generated-symbol stripping option ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制策略、
  实际 metadata sweep/pruning、默认最小 metadata policy、release-mode 默认策略/CLI 接线和跨后端符号剥离审计仍待后续。
  完成项目：`SZrAotWriterOptions` 新增 `stripGeneratedSymbols`，AOT C 生成文件头部新增
  `symbol_stripping.generatedSymbols`；默认模式继续输出可读的 `zr_fn_pair__1` / `zr_fn_box__shared`
  与 `Box<RefA>` debugName；开启后泛型单态化 helper 改为 `zr_fn_g1__1`，
  shared generic helper 改为 `zr_fn_g1__shared`，dictionary slot `debugName` 改为 `generic#<id>`，
  且 stripped 生成物不再在这些私有 helper/comment/debugName 表面暴露 `Pair`/`Box` 类型名。
  RED/GREEN：RED 为 generic generated-C fixture 设置 `options.stripGeneratedSymbols = ZR_TRUE`
  后因 writer options 缺少该字段编译失败；source contract 同时要求 public option 与 emitter
  option plumbing。GREEN 后 option 经 `backend_aot_option_strip_generated_symbols()` 进入 emitter，
  并传给 generic monomorphization/sharing emitters。
  验证：WSL gcc 直接 `zr_vm_aot_c_code_stripping_test` 4/0，CTest
  `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 3/3，
  source contracts 20/0，frame setup contract 1/0；WSL clang clean rebuild 后同组通过；
  Windows MSVC Debug rebuild 后同组通过。产出：
  `tests/acceptance/2026-06-26-aot-12-s7u-release-generated-symbol-stripping.md`。
  备注：clang 初次 fast-target 验证暴露 stale `test_aot_c_code_stripping.c.o` 使用旧 writer-options
  layout，表现为 manifest root 被误裁剪；clean rebuild 后确认是构建产物过期，不是本切片逻辑缺陷。

- 2026-06-26 04:21:08 +08:00 · 12-S7T zrp metadata size module split ·
  状态：12-S7 支持性 refinement 完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制策略、
  实际 metadata sweep/pruning、默认最小 metadata policy 和 release 符号剥离仍待后续。
  完成项目：新增 `backend_aot_c_zrp_metadata_size.{h,c}`，把 `SZrAotZrpMetadataSizeStats`、zrp metadata
  header sampling、最终 `aot_size.zrpMetadata*` marker 写入和 `code_stripping.zrpMetadata*Before/After/Removed`
  marker 写入从 `backend_aot_c_emitter.c` 拆出；emitter 只保留采样/写入调度，行数从 893 收回到 763，
  新模块为 116 行。
  RED/GREEN：RED 为 source contract 要求新模块存在并禁止 emitter 直接包含 `zr_vm_core/zrp_metadata.h`/
  `ZrCore_ZrpMetadata_ReadHeader()` 后，`zr_vm_aot_c_source_contracts_test` 20 个用例中 1 个失败；
  GREEN 后新模块被 parser shared 编译链接，code-stripping zrp metadata byte markers 行为保持 12-S7S 语义。
  验证：WSL gcc 直接运行 `zr_vm_aot_c_code_stripping_test` 为 4/0；WSL gcc、WSL clang 和
  Windows MSVC Debug 的 CTest `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format`
  均为 3/3；三套环境的 `zr_vm_aot_c_source_contracts_test` 均为 20/0。产出：
  `tests/acceptance/2026-06-26-aot-12-s7t-zrp-metadata-size-module-split.md`。
  备注：WSL clang 初次 fast build 因 CMake glob 未重新生成而缺新 `.c` 链接符号，重新 `cmake -S . -B build-wsl-clang`
  后通过；本切片只整理模块边界，不声明 metadata sweep/pruning 或 release 符号剥离完成。

- 2026-06-26 04:04:49 +08:00 · 12-S7S zrp metadata byte trim delta carrier ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制策略、
  实际 metadata sweep/pruning、默认最小 metadata policy 和 release 符号剥离仍待后续。
  完成项目：`backend_aot_c_emitter.c` 将 zrp metadata size 统计收敛为可复用采样结构，并在 generated C
  header 输出 `code_stripping.zrpMetadataBytes*`、`zrpMetadataTokenRecordBytes*`、
  `zrpMetadataDefinitionTableBytes*` 和 `zrpMetadataPoolBytes*` 的 before/after/removed marker。
  当前 writer 仍嵌入同一 zrp blob，因此 before=after、removed=0，后续 metadata sweep 可直接复用该载体。
  RED/GREEN：RED 为 zrp metadata size fixture 要求 code-stripping metadata byte delta markers 后，
  旧生成物缺 marker 导致 1 个用例失败；GREEN 后 total/token-record/definition-table/pool 四组
  before/after/removed marker 均输出且 removed 为 0。
  验证：WSL gcc/clang 直接运行 `zr_vm_aot_c_code_stripping_test` 均为 4/0；WSL gcc/clang 和
  Windows MSVC Debug 的 CTest `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format`
  均为 3/3；三套环境的 `zr_vm_aot_c_source_contracts_test` 均为 19/0。产出：
  `tests/acceptance/2026-06-26-aot-12-s7s-zrp-metadata-byte-trim-delta.md`。
  备注：WSL clang 的普通构建入口仍被当前工作区无关 `tests/CMakeLists.txt` 缺失测试源引用阻塞；
  本切片使用 parser/test fast target 完成聚焦验证，不声明全量 clang 重新生成健康。

- 2026-06-26 03:52:50 +08:00 · 12-S7R generated type-layout byte trim delta ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制策略、
  metadata sweep diff 和 release 符号剥离仍待后续。
  完成项目：`backend_aot_c_type_layout_generated_bytes_referenced()` 复用真实 generated-C type-layout
  发射循环，通过 scratch file 采样 referenced inline type-layout 的 generated-C byte span；
  `backend_aot_c_emitter.c` 在 opt-in code stripping 前后采样并输出
  `code_stripping.typeLayoutGeneratedBytesBefore/After/Removed`，同时保持 after 与
  `aot_size.typeLayoutBytesTotal` 一致。
  RED/GREEN：RED 为 code-stripping 生成 C 测试要求 generated-byte markers 后，旧生成物缺 marker 导致
  3 个用例失败；GREEN 后普通 trim fixture 报告 `1072 -> 536`、removed `536`，export/manifest
  root fixture 报告 `1072 -> 1072`、removed `0`。
  验证：WSL gcc/clang 直接运行 `zr_vm_aot_c_code_stripping_test` 均为 4/0；WSL gcc/clang 和
  Windows MSVC Debug 的 CTest `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format`
  均为 3/3；三套环境的 `zr_vm_aot_c_source_contracts_test` 均为 19/0。产出：
  `tests/acceptance/2026-06-26-aot-12-s7r-generated-type-layout-byte-trim-delta.md`。
  备注：WSL clang 的普通构建入口被当前工作区无关 `tests/CMakeLists.txt` 缺失测试源引用阻塞；
  本切片使用 parser/test fast target 完成聚焦验证，不声明全量 clang 重新生成健康。

- 2026-06-26 03:24:43 +08:00 · 12-S7Q runtime fallback warning source file attribution ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制策略、
  pre-trim generated-C type/layout byte span attribution、metadata sweep diff 和 release 符号剥离仍待后续。
  完成项目：runtime fallback warning marker 现在从 `SZrFunction.sourceCodeList` 读取 source file identity，
  并输出 `sourceFile=<file>`；缺失或空 source file 回退为 `<unknown>`。
  RED/GREEN：RED 为动态 deopt fixture 设置 `sourceCodeList=dynamic_deopt_bridge.zr` 并要求 marker 含
  `sourceFile=dynamic_deopt_bridge.zr` 后，旧生成物只输出 line/column span；GREEN 后 dynamic-call 和
  dynamic-value-access warning marker 均输出 `sourceFile=dynamic_deopt_bridge.zr sourceLine=41 sourceLineEnd=43 sourceColumn=7 sourceColumnEnd=19`，
  reason-mask suppressed fixture 继续只输出 suppressed count。
  验证：WSL gcc/clang 直接运行 `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 均为 6/0；
  WSL gcc/clang 和 Windows MSVC Debug 的 CTest
  `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 均为 3/3；三套环境的
  `zr_vm_aot_c_source_contracts_test` 均为 19/0。Windows 动态 deopt shared-library 用例按既有规则
  6 ignored。产出：`tests/acceptance/2026-06-26-aot-12-s7q-runtime-fallback-warning-source-file.md`。
  备注：本切片只发布 source file marker，不实现 source path escaping、完整 trim analyzer、注解 suppress 或 release 符号剥离。

- 2026-06-26 03:14:26 +08:00 · 12-S7P runtime fallback warning column span ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、source file attribution、
  attribute/annotation 抑制策略、pre-trim generated-C type/layout byte span attribution 和 release 符号剥离仍待后续。
  完成项目：`SZrAotExecIrInstruction` 新增 `debugColumn/debugColumnEnd`；ExecIR source-location 推导拆入
  `backend_aot_exec_ir_source_location.{h,c}`；runtime fallback warning marker 现在输出
  `sourceColumn=<start> sourceColumnEnd=<end>`。
  RED/GREEN：RED 为动态 deopt fixture 改为要求 `sourceColumn=7 sourceColumnEnd=19` 后，旧生成物只输出 line span；
  GREEN 后 dynamic-call 和 dynamic-value-access warning marker 均输出
  `sourceLine=41 sourceLineEnd=43 sourceColumn=7 sourceColumnEnd=19`，reason-mask suppressed fixture 继续只输出 suppressed count。
  验证：WSL gcc/clang 直接运行 `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 均为 6/0；
  WSL gcc/clang 和 Windows MSVC Debug 的 CTest
  `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 均为 3/3；三套环境的
  `zr_vm_aot_c_source_contracts_test` 均为 19/0。Windows 动态 deopt shared-library 用例按既有规则
  6 ignored。产出：`tests/acceptance/2026-06-26-aot-12-s7p-runtime-fallback-warning-column-span.md`。
  备注：初始实现把 `backend_aot_exec_ir.c` 推到 1023 行，验收前已按模块边界拆出 source-location helper；
  当前 `backend_aot_exec_ir.c` 为 906 行，新 helper `.c/.h` 为 114/17 行。

- 2026-06-26 02:46:07 +08:00 · 12-S7O runtime fallback warning reason-mask suppression ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、file/column source span、
  attribute/annotation 抑制策略、pre-trim generated-C type/layout byte span attribution 和 release 符号剥离仍待后续。
  完成项目：`EZrAotRuntimeFallbackWarningFlag` 新增 dynamic-call/value-access/iterator/reflection reason bit；
  `SZrAotWriterOptions.suppressRuntimeFallbackWarningReasonMask` 支持按 reason 抑制 runtime fallback warning；
  既有 `suppressRuntimeFallbackWarnings` 继续作为 all-reasons shortcut；warning 统计分离 visible count 与 suppressed count，
  单条 `trim_warning.runtimeFallback[...]` marker 只输出未被 mask 的 reason。
  RED/GREEN：RED 为动态 deopt smoke 新增 reason-mask 用例后，WSL gcc 构建失败于缺少
  `suppressRuntimeFallbackWarningReasonMask` 成员和 `ZR_AOT_RUNTIME_FALLBACK_WARNING_DYNAMIC_CALL` 常量；
  GREEN 后 dynamic-call 生成物输出 `runtimeFallbackCount = 0`、`runtimeFallbackSuppressedCount = 1` 且无 visible marker，
  同一 mask 下 dynamic-value-access 生成物仍输出 `runtimeFallbackCount = 1`、`sourceLine=41 sourceLineEnd=43 reason=dynamic-value-access`。
  验证：WSL gcc/clang 直接运行 `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 均为 6/0；
  WSL gcc/clang 和 Windows MSVC Debug 的 CTest
  `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 均为 3/3；三套环境的
  `zr_vm_aot_c_source_contracts_test` 均为 19/0。Windows 动态 deopt shared-library 用例按既有规则
  6 ignored。产出：`tests/acceptance/2026-06-26-aot-12-s7o-runtime-fallback-warning-reason-mask-suppression.md`。
  备注：本切片只完成 writer-level reason mask，不完成注解/属性驱动 suppress、source file/column、完整 trim analyzer 或 release 符号剥离。

- 2026-06-26 02:26:36 +08:00 · 12-S7N runtime fallback source line span ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、file/column source span、
  per-warning 抑制策略、pre-trim generated-C type/layout byte span attribution 和 release 符号剥离仍待后续。
  完成项目：`SZrAotExecIrInstruction` 新增 `debugLineEnd`；ExecIR 构建从 execution-location end line、
  per-instruction line list、function end/start line 中推导 end line，并保证 end 不早于 start；runtime fallback
  warning marker 现在输出 `sourceLine=<start> sourceLineEnd=<end>`。
  RED/GREEN：RED 为动态 deopt fixture 改为要求 `sourceLineEnd=43` 后，旧生成物只输出 `sourceLine=41`；
  GREEN 后 dynamic-call 和 dynamic-value-access warning marker 均输出 `sourceLine=41 sourceLineEnd=43`，
  suppressed fixture 继续只输出 suppressed count 而不输出单条 warning entry。
  验证：WSL gcc/clang 直接运行 `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 均为 5/0；
  WSL gcc/clang 和 Windows MSVC Debug 的 CTest
  `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 均为 3/3；三套环境的
  `zr_vm_aot_c_source_contracts_test` 均为 19/0。Windows 动态 deopt shared-library 用例按既有规则
  5 ignored。产出：
  `tests/acceptance/2026-06-26-aot-12-s7n-runtime-fallback-source-line-span.md`。
  备注：本切片只补 line-span；source file、column、AST range、完整 trim analyzer、per-warning suppression policy
  和 release 符号剥离仍未完成。`backend_aot_exec_ir.c` 为 972 行，后续若继续增长，应将 source-location/span
  推导抽出为独立 helper 模块。

- 2026-06-26 02:12:56 +08:00 · 12-S7M runtime fallback warning suppression ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、source span、
  per-warning 抑制策略、pre-trim generated-C type/layout byte span attribution 和 release 符号剥离仍待后续。
  完成项目：`SZrAotWriterOptions` 新增 `suppressRuntimeFallbackWarnings`；AOT C writer 在 hybrid 模式下
  继续计算 runtime fallback diagnostics，但抑制时把数量转入 `trim_warnings.runtimeFallbackSuppressedCount`，
  同时将 `trim_warnings.runtimeFallbackCount` 置 0 且不输出单条 `trim_warning.runtimeFallback[...]` marker。
  RED/GREEN：RED 为新增动态 deopt fixture 编译失败，`SZrAotWriterOptions` 缺
  `suppressRuntimeFallbackWarnings` 字段；GREEN 后 suppressed generated C 输出 visible count=0、suppressed count=1，
  保留 `ZrLibrary_AotRuntime_CallDynamicDeoptBridge(state,`，且不输出 warning entry。
  验证：WSL gcc/clang 直接运行 `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 均为 5/0；
  WSL gcc/clang 和 Windows MSVC Debug 的 CTest
  `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 均为 3/3；三套环境的
  `zr_vm_aot_c_source_contracts_test` 均为 19/0。Windows 动态 deopt shared-library 用例按既有规则
  5 ignored。产出：
  `tests/acceptance/2026-06-26-aot-12-s7m-runtime-fallback-warning-suppression.md`。
  备注：本切片只提供 writer-level 全局 suppression；full-AOT runtime closure rejection 仍先于输出阶段执行且不受抑制影响，
  per-warning/attribute-based suppression、完整 trim analyzer、完整 source span 和 release 符号剥离仍未完成。

- 2026-06-26 01:56:33 +08:00 · 12-S7L type-layout payload byte trim delta ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、source span、warning 抑制、
  pre-trim generated-C type/layout byte span attribution 和 release 符号剥离仍待后续。
  完成项目：`backend_aot_c_type_layouts.c` 新增
  `backend_aot_c_type_layout_payload_bytes_referenced()`，复用 distinct inline layout 遍历口径，按
  `frameSlotLayout.byteSize` 聚合 referenced layout payload bytes；AOT C writer 在 reachability filter 前后采样并输出
  `code_stripping.typeLayoutPayloadBytesBefore/After/Removed`。
  RED/GREEN：RED 为 code-stripping 生成 C fixture 只输出 type-layout 数量、不输出 payload byte delta；
  GREEN 后普通裁剪 fixture 输出 before=16、after=8、removed=8，export root 与 manifest root 输出 before=16、
  after=16、removed=0。
  验证：WSL gcc、WSL clang、Windows MSVC Debug 的 CTest
  `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 均为 3/3；三套环境的
  `zr_vm_aot_c_source_contracts_test` 均为 19/0。产出：
  `tests/acceptance/2026-06-26-aot-12-s7l-type-layout-payload-byte-trim-delta.md`。
  备注：本切片统计 referenced inline layout 的 payload byteSize，不声明 pre-trim generated-C emission byte span、
  metadata sweep diff、默认最小 metadata 策略或 release 符号剥离完成。

- 2026-06-26 01:40:40 +08:00 · 12-S7K zrp metadata section/table/pool byte statistics ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、source span、warning 抑制、
  字节级裁剪前后 type/layout 对比和 release 符号剥离仍待后续。
  完成项目：AOT C writer 在 `aot_size.embeddedModuleBytes` 后读取有效 `SZrZrpMetadataHeader`，
  输出 `aot_size.zrpMetadataBytes`、`aot_size.zrpMetadataTokenRecordBytes`、
  `aot_size.zrpMetadataDefinitionTableBytes`、`aot_size.zrpMetadataPoolBytes`，以及
  `aot_size.zrpMetadataSectionBytes.<section>` 的 12 个 section 明细；无 blob、空 blob 或非 zrp blob
  保持稳定 0 值统计。
  RED/GREEN：RED 为新增 zrp metadata size fixture 只看到 embedded module bytes、缺 zrp 统计 marker；
  GREEN 后生成物包含 total=374、tokenRecords=96、definitionTable=52、pool=18，以及 tokenRecords/typeDefs/
  stringPool/signatureBlobPool/constantPool 等 section marker。
  验证：WSL gcc、WSL clang、Windows MSVC Debug 的 CTest
  `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 均为 3/3；三套环境的
  `zr_vm_aot_c_source_contracts_test` 均为 19/0。产出：
  `tests/acceptance/2026-06-26-aot-12-s7k-zrp-metadata-section-byte-statistics.md`。
  备注：这是 generated AOT C 对已嵌入 zrp data metadata 的 size attribution，不声明 metadata 裁剪前后 diff、
  默认最小 metadata 策略、zrp dump/diff 工具或 release 符号剥离完成。

- 2026-06-25 06:26:16 +08:00 · 12-S3F / 12-S4N / 11-S7V / 08-S7K manifest generic MethodSpec binding ·
  状态：12-S3 与 12-S4 子切片完成；完整 12-S3/12-S4 仍未关闭，跨模块 method/generic binding、
  传递泛型可达闭包、动态实例 deopt 和注解 roots 仍待后续。
  完成项目：manifest generic method preserve root 可以绑定到 current-module `GENERIC_INST(MEMBER_REF methodToken, args...)`
  MethodSpec 形态签名；writer root 记录 method-spec token、method token 和 instantiation signature hash；
  generated C manifest 诊断输出这些字段，full-AOT generic closure gate 接受 MethodSpec-bound method root。
  RED/GREEN：RED 为新用例引用 MethodSpec root 字段后编译失败；GREEN 后 `Factory.make<Foo>` 绑定到
  `0x08000002` method-spec token 和 `0x03000001` method token。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 14/0；WSL gcc、WSL clang、Windows MSVC Debug 的 CTest
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model` 均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7v-12-s3f-manifest-generic-methodspec-binding.md`。
  备注：该记录只覆盖 current-module writer-visible MethodSpec binding；跨模块 generic method root、
  annotation roots、泛型方法代码体传递闭包或完整 closure checker 仍未完成。

- 2026-06-25 06:03:45 +08:00 · 12-S3E / 12-S4M / 11-S7U / 08-S7J manifest generic synthesized TypeSpec binding ·
  状态：12-S3 与 12-S4 子切片完成；完整 12-S3/12-S4 仍未关闭，MethodSpec 绑定、
  跨模块 method/generic binding、传递泛型可达闭包、动态实例 deopt 和注解 roots 仍待后续。
  完成项目：manifest generic preserve root 缺失已有 TypeSpec 时，CLI AOT preserve bridge 会基于当前函数
  metadata 中同名 open `TYPE_DEF` 或 `TYPE_REF` record 追加 writer-visible synthesized `TYPE_SPEC` /
  paired `SIGNATURE` record，并继续把该 root 物化为 generic instantiation identity；full-AOT gate 因而能接受
  这个已合成且已物化的 current-module generic root。
  RED/GREEN：RED 为 full-AOT `List<Foo>` 用例在只有 open `TYPE_REF(List)` metadata 时仍缺
  TypeSpec binding；GREEN 后 TypeSpec token 为 `0x07000001`、signature token 为 `0x08000002`、
  open base token 为 `0x05000001`。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 13/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7u-12-s3e-manifest-generic-synthesized-typespec.md`。
  备注：该记录只覆盖 current-module writer-visible TypeSpec synthesis；MethodSpec、跨模块 generic root、
  annotation roots 或完整 closure checker 仍未完成。

- 2026-06-25 05:41:31 +08:00 · 12-S3D / 12-S4L / 11-S7T / 08-S7I generic instantiation TypeDef base token ·
  状态：12-S3 与 12-S4 子切片完成；完整 12-S3/12-S4 仍未关闭，MethodSpec 绑定、
  跨模块 method/generic binding、传递泛型可达闭包、动态实例 deopt 和注解 roots 仍待后续。
  完成项目：manifest generic preserve root 的 TypeSpec binding 不再只接受 `TYPE_REF` base；
  `GENERIC_INST(TYPE_DEF target, args...)` 现在可绑定 TypeSpec，并把同名 current-module TypeDef token
  写入 generic instantiation identity。generated C manifest 诊断同步输出 TypeDef base token。
  RED/GREEN：RED 为 TypeDef-base TypeSpec 用例仍无法设置 `hasTypeSpecBinding`；GREEN 后 TypeSpec 绑定成功，
  `genericInstantiationBaseToken` 为 `0x02000001`。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 12/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7t-12-s3d-generic-instantiation-typedef-base-token.md`。
  备注：该记录只覆盖 current-module TypeDef-backed open base token；MethodSpec、跨模块 generic root、
  TypeSpec synthesis、annotation roots 或完整 closure checker 仍未完成。

- 2026-06-25 05:28:38 +08:00 · 12-S3C / 12-S4K / 11-S7S / 08-S7H generic instantiation open base token ·
  状态：12-S3 与 12-S4 子切片完成；完整 12-S3/12-S4 仍未关闭，MethodSpec 绑定、
  跨模块 method/generic binding、传递泛型可达闭包、动态实例 deopt 和注解 roots 仍待后续。
  完成项目：manifest generic preserve root 的 writer-visible generic instance identity 现在可从 current-module
  `TYPE_REF` metadata 记录取得 open generic base token；若没有同名 TypeRef 记录，则保持前一切片的
  TypeSpec-backed fallback。后续 mark-and-sweep generic closure 可区分 `List` open base 与 `List<Foo>` TypeSpec。
  RED/GREEN：RED 为 CLI writer options 测试新增 `TYPE_REF(List)` + `TYPE_SPEC(List<Foo>)` 元数据后，
  `genericInstantiationBaseToken` 仍为 `0x07000001`；GREEN 后为 `0x05000001`，generated C manifest 诊断同步输出。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 11/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7s-12-s3c-generic-instantiation-open-base-token.md`。
  备注：该记录只覆盖 current-module TypeRef-backed open base token；MethodSpec、跨模块 generic root、
  TypeSpec synthesis、annotation roots 或完整 closure checker 仍未完成。

- 2026-06-25 05:08:49 +08:00 · 12-S8I / 12-S3B / 11-S7R / 08-S7G full-AOT generic instantiation closure gate ·
  状态：12-S8 与 12-S3 子切片完成；完整 12-S8/12-S3 仍未关闭，manifest 动态泛型实例、
  注解驱动反射保留、传递泛型可达闭包、MethodSpec 和完整 mark-and-sweep 诊断仍待后续。
  完成项目：AOT C writer 的 full-AOT manifest generic root 预检从 TypeSpec gate 收紧为
  TypeSpec + generic-instantiation identity gate；`SZrAotManifestGenericRoot` 即便已有 TypeSpec/signature token/hash，
  只要没有 `hasGenericInstantiationBinding`，writer 仍拒绝生成。
  RED/GREEN：RED 为 direct writer-options 测试构造 TypeSpec-only generic root 后仍成功生成；
  GREEN 后该 root 返回 false，CLI `.zrp` 路径物化出的 `List<Foo>` generic instance root 继续通过。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 10/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7r-12-s8i-full-aot-generic-instantiation-closure-gate.md`。
  备注：该门禁只消费 writer-visible generic instantiation identity；不声明跨模块 generic root、
  MethodSpec、TypeSpec synthesis、annotation roots 或完整 closure checker 完成。

- 2026-06-25 04:50:01 +08:00 · 12-S3A / 12-S4J / 11-S7Q / 08-S7F manifest generic TypeSpec-backed instantiation root ·
  状态：12-S3 与 12-S4 子切片完成；完整 12-S3/12-S4 仍未关闭，MethodSpec 绑定、
  跨模块 method/generic binding、传递泛型可达闭包、动态实例 deopt 和注解 roots 仍待后续。
  完成项目：CLI AOT preserve 注入现在会在 manifest generic root 已绑定 `TYPE_SPEC` 后，
  把 root arguments 转成 `SZrGenericInstantiationTypeArgument` 并写入临时 `SZrGenericInstantiationTable`；
  writer root 保存 generic instance base token、C instance id 与 share kind。AOT C manifest 诊断输出
  `manifest.genericRoot[i].genericInstance.baseToken/id/shareKind`，让后续 mark-and-sweep generic closure
  可从 manifest root 追踪到实例身份。
  RED/GREEN：RED 为 CLI writer options 测试引用缺失 generic-instantiation fields 后编译失败；
  GREEN 后 `List<Foo>` root 生成 TypeSpec token `0x07000001`、generic instance id `1` 和 shared-reference share kind `1`。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 9/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7q-12-s3a-manifest-generic-preserve-instantiation-root.md`。
  备注：该记录只覆盖 current-module TypeSpec-backed manifest generic preserve root 的实例身份物化；
  不声明完整泛型实例可达集合、缺失 TypeSpec 合成、MethodSpec、跨模块 generic target 或 annotation roots 完成。

- 2026-06-25 04:14:31 +08:00 · 12-S8H / 11-S7P / 08-S7E full-AOT manifest generic TypeSpec closure gate ·
  状态：12-S8 子切片完成；完整 12-S8 仍未关闭，manifest 动态泛型实例、注解驱动反射保留、
  完整 closure diagnostics 和 mark-and-sweep 诊断仍待后续。
  完成项目：AOT C writer 的 full-AOT 预检现在额外覆盖 manifest generic preserve roots：
  `options.requireFullAot` 为 true 且任一 `SZrAotManifestGenericRoot.hasTypeSpecBinding` 为 false 时，
  writer 在发射前返回 `ZR_FALSE`。这把 12-S4I 的 TypeSpec 绑定结果接入 12-S8 的闭包校验。
  RED/GREEN：RED 为 full-AOT `.zrp` generic preserve `List<Foo>` 没有匹配 TypeSpec metadata 时仍通过 writer；
  GREEN 后未绑定 root 被拒绝，hybrid 未绑定 root 仍输出 manifest 诊断，已绑定 root 继续输出 TypeSpec/signature/hash。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 8/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7p-12-s8h-full-aot-generic-preserve-typespec-closure-gate.md`。
  备注：该门禁只消费已有 generic root TypeSpec binding；不声明 manifest dynamic generic instance materialization、
  MethodSpec 绑定、TypeSpec 合成、annotation roots 或完整 full-AOT 闭包检查完成。

- 2026-06-25 04:00:47 +08:00 · 12-S4I / 11-S7O / 08-S7D manifest generic preserve TypeSpec binding ·
  状态：12-S4 子切片完成；完整 12-S4 仍未关闭，真实 generic instantiation roots、MethodSpec 绑定、
  跨模块 method/generic binding 与注解 roots 仍待后续。
  完成项目：generic preserve writer root 在 target/arguments 文本之外新增当前模块 `TYPE_SPEC` 绑定结果；
  CLI AOT preserve 注入会匹配已有 `GENERIC_INST` signature，并把 TypeSpec token、paired signature token 与 hash
  传给 AOT writer；generated C manifest 清单输出 `manifest.genericRoot[i].typeSpecToken`、
  `signatureToken` 和 `signatureHash`，让后续 mark-and-sweep generic closure 有可审计 token 输入。
  RED/GREEN：RED 为 writer options 测试缺 TypeSpec binding fields 编译失败；GREEN 后 `List<Foo>` root 绑定并发射
  `0x07000001` / `0x08000001` / `0x123456789abcdef0`。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 7/0；WSL gcc/clang 与 Windows MSVC Debug 的 CTest
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5；`git diff --check` 退出 0（仅 LF/CRLF 提示）。产出：
  `tests/acceptance/2026-06-25-aot-11-s7o-12-s4i-manifest-generic-preserve-typespec-binding.md`。
  备注：该记录不声明 generic instantiation table materialization、缺失 TypeSpec 生成、MethodSpec 解析、
  annotation roots 或 full-AOT 缺失实例诊断完成。

- 2026-06-25 03:27:16 +08:00 · 12-S4H / 11-S7N / 08-S7C manifest generic preserve writer roots ·
  状态：12-S4 子切片完成；完整 12-S4 仍未关闭，真实 generic instantiation roots、metadata token 绑定、
  跨模块 method binding 与注解 roots 仍待后续。
  完成项目：writer options 新增 manifest generic root carrier，CLI preserve root bridge 在 feature 条件匹配后
  把 `.zrp` generic preserve target 与 `arguments` 文本注入 `SZrAotWriterOptions`；generated C 头部输出
  `manifest.genericRoots`、每个 `manifest.genericRoot[i]` target 和 argument 清单，为后续 mark-and-sweep
  generic instantiation 收集提供稳定输入面。
  RED/GREEN：RED 为 CLI writer options 测试引用缺失 generic root carrier 后编译失败；GREEN 后
  `List<Foo, Bar.Baz>` 被传入 writer options 且 generated C 可审计。
  验证：WSL gcc/clang `zr_vm_cli_aot_writer_options_test` 均 6/0，并且 CTest
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed` 均 3/3；
  Windows MSVC Debug 同目标 6/0，同 CTest 过滤 3/3；`python -m json.tool zrp.schema.json` 通过；
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。产出：
  `tests/acceptance/2026-06-25-aot-11-s7n-12-s4h-manifest-generic-preserve-writer-roots.md`。
  备注：这是 generic preserve 的 writer-root bridge，不是最终 generic instantiation reachability closure；
  MethodSpec/TypeSpec token resolution、泛型实例表 materialization、跨模块 generic target 与 annotation roots 仍开放。

- 2026-06-25 03:02:14 +08:00 · 12-S4G / 11-S7M generic preserve argument model ·
  状态：12-S4 子切片完成；完整 12-S4 仍未关闭，generic instantiation roots、metadata token 绑定、
  跨模块 method binding 与注解 roots 仍待后续。
  完成项目：`.zrp` `preserve` rule 中的 `kind: "generic"` 现在拥有明确的非空 `arguments` 数组，
  project model 以 `SZrString **genericArguments` + count/capacity 承载具体类型实参；parser 拒绝
  generic rule 缺少 `arguments`、空参数数组、非数组参数、非法参数名，以及非 generic rule 携带 `arguments`。
  schema 同步 `arguments`、`minItems: 1` 和 generic-only 条件约束。
  RED/GREEN：RED 先为 generic argument model 新增断言后编译失败，随后补充无参数/空参数和
  非 generic rule 携带参数的拒绝用例，旧实现会错误接受这些 manifest；GREEN 后合法
  `List<Foo, Bar.Baz>` 被解析，非法形态全部拒绝。
  验证：WSL gcc `zr_vm_project_manifest_normalization_test` 25/0；WSL clang 同目标 25/0；
  Windows MSVC Debug 同目标 25/0；`python -m json.tool zrp.schema.json` 通过；
  WSL gcc CTest `cli_aot_writer_options|aot_c_code_stripping` 2/2；`git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-25-aot-11-s7m-12-s4g-generic-preserve-argument-model.md`。
  备注：本切片只让 generic preserve roots 具有可验证的 manifest 参数承载；还没有把它们注入 reachability、
  MethodSpec/TypeSpec token、泛型实例集合或跨模块绑定。

- 2026-06-25 02:40:15 +08:00 · 12-S4F / 11-S7L feature switch preserve root gating ·
  状态：12-S4 子切片完成；完整 12-S4 仍未关闭，generic preserve、metadata token 绑定、
  跨模块 method binding 与注解 roots 仍待后续。
  完成项目：`.zrp` top-level `features` object 作为本地构建 feature switch 表解析到 project model；
  feature 名称使用 preserve target 同级 safe dotted 校验，值必须是 boolean。CLI AOT preserve root 注入在处理
  method/type rules 前检查 `feature` / `featureValue`，只有 switch 存在且值匹配时才把目标加入 writer
  manifest roots；未声明 switch 或值不匹配时跳过该 preserve rule。
  RED/GREEN：RED 为 manifest normalization 测试缺 feature switch model、CLI writer options 测试缺
  feature-conditioned root gating 而编译失败；GREEN 后 `EnableFastAot=true` 会保留 `Widget.kept`，
  `EnableFastAot=false` 会让相同 preserve rule 被跳过，generated C 保持 `zr_aot_fn_2` 被裁剪。
  验证：WSL gcc `zr_vm_project_manifest_normalization_test` 19/0、`zr_vm_cli_aot_writer_options_test` 5/0；
  WSL clang 同两目标 19/0、5/0；Windows MSVC Debug 同两目标 19/0、5/0；
  WSL gcc CTest `cli_aot_writer_options|aot_c_code_stripping` 2/2；
  `python -m json.tool zrp.schema.json` 通过；`git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-25-aot-11-s7l-12-s4f-feature-switch-preserve-root-gating.md`。
  备注：本切片只将 feature switch 作用到当前已支持的 method/type preserve writer roots；
  generic roots、metadata-token binding、cross-module roots、annotation roots 和默认裁剪策略仍开放。

- 2026-06-25 02:23:14 +08:00 · 12-S4E / 11-S7K preserve feature condition model ·
  状态：12-S4 子切片完成；完整 12-S4 仍未关闭，generic preserve、metadata token 绑定、
  跨模块 method binding、注解 roots 与 feature 条件求值/构建配置接入仍待后续。
  完成项目：`.zrp` `preserve` rule model 新增 feature 条件承载字段，可解析
  `{ "feature": "EnableFastAot", "featureValue": true/false }`，并拒绝只有 `feature`
  或只有 `featureValue` 的半声明；schema 同步字段类型、safe dotted feature name pattern 与互相依赖关系。
  RED/GREEN：RED 为 manifest normalization 测试先因 `SZrLibrary_ProjectPreserveRule` 缺少 feature
  字段编译失败；GREEN 后两个条件 preserve rules 分别保留 `true` 与 `false` 期望值，缺字段组合被拒绝。
  验证：WSL gcc `zr_vm_project_manifest_normalization_test` 17/0；WSL clang 同目标 17/0；
  Windows MSVC Debug 同目标 17/0；`python -m json.tool zrp.schema.json` 通过；
  WSL gcc CTest `cli_aot_writer_options|aot_c_code_stripping` 2/2；`git diff --check` 退出 0
  （仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-25-aot-11-s7k-12-s4e-preserve-feature-condition-model.md`。
  备注：这是 feature switch 的 manifest 条件声明层，不是按构建配置选择性保留 roots 的执行层；
  writer root 过滤、默认裁剪策略、generic roots、metadata-token binding 与 annotation roots 仍开放。

- 2026-06-25 02:09:47 +08:00 · 12-S4D / 11-S7J dotted and type-member preserve roots ·
  状态：12-S4 子切片完成；完整 12-S4 仍未关闭，generic preserve、metadata token 绑定、
  跨模块 method binding、注解 roots 与 feature switch 仍待后续。
  完成项目：manifest `method` root binding 现在优先按完整 dotted target 精确匹配 callable name，
  支持 `Widget.kept` 这类 type/member 形态；`type` preserve 的 `members: "methods"` 或 `"all"`
  会扫描 entry function 的 top-level callable bindings，把 `<type>.` 前缀下的方法全部加入 writer
  manifest roots。上述 roots 继续复用 12-S4A 的 `MANIFEST` reachability reason。
  RED/GREEN：RED 为 dotted method 和 type-members 测试在 helper 返回后 root count 仍为 0；
  GREEN 后 dotted method 保留 flat index 2，type members 保留 flat indices 1/2，三条 generated C
  fixture 均输出 `functionsBefore/After/Removed = 3/3/0`。
  验证：WSL gcc CTest `cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` 3/3；
  WSL clang 同组 3/3；Windows MSVC Debug 同组 3/3。
  产出：`tests/acceptance/2026-06-25-aot-11-s7j-12-s4d-dotted-type-method-preserve-roots.md`。
  备注：本切片不新增默认裁剪 feature switch，也不声明 generic manifest roots、metadata token resolution、
  注解 roots 或跨模块 target 完成。

- 2026-06-25 01:53:48 +08:00 · 12-S4C / 11-S7I method preserve root binding ·
  状态：12-S4 子切片完成；完整 12-S4 仍未关闭，type/generic preserve、metadata token 绑定、
  跨模块 method binding、注解 roots 与 feature switch 仍待后续。
  完成项目：AOT writer 暴露 `ZrParser_Writer_ResolveTopLevelCallableFlatIndex()`，把 entry function
  的 top-level callable binding name 解析到 AOT flat function index；CLI AOT C helper 在
  `ZrCli_Compiler_WriteAotCFileForModule()` 中读取 `.zrp` `preserve` rules，将当前模块 `method`
  target 去重后写入 `SZrAotWriterOptions.manifestPreserveFunctionFlatIndices`。在 `enableCodeStripping`
  打开时，这些 roots 复用 12-S4A 的 `MANIFEST` reachability reason，保留原本不可达的 callable child。
  RED/GREEN：RED 为新增 CLI AOT writer options 测试先因缺失 preserve root container/helper 与
  callable flat-index resolver 编译失败；GREEN 后 `main.kept` preserve rule 让 generated C 同时保留
  `zr_aot_fn_0`、`zr_aot_fn_1`、`zr_aot_fn_2`，且 `functionsBefore/After/Removed` 为 `3/3/0`。
  验证：WSL gcc CTest `cli_args|cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` 4/4；
  WSL clang CTest `cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` 3/3；
  Windows MSVC Debug 同组 3/3。
  产出：`tests/acceptance/2026-06-25-aot-11-s7i-12-s4c-preserve-method-root-binding.md`。
  备注：本切片复用现有 opt-in code-stripping writer roots；尚未新增默认裁剪 feature switch，也不声明
  `type`/`generic` preserve、metadata token resolution、注解保留或跨模块 method target 完成。

- 2026-06-25 01:13:27 +08:00 · 12-S8G / 11-S7H CLI full-AOT AOT C emission entry ·
  状态：12-S8 CLI 入口子切片完成；完整 12-S8 仍未关闭，manifest 动态泛型实例、注解驱动反射保留、
  完整 full-AOT closure diagnostics 和 mark-and-sweep 诊断仍待后续。
  完成项目：CLI `--emit-aot-c` 会在项目编译流程中用 `.zro` binary blob 调用
  `ZrParser_Writer_WriteAotCFileWithOptions()`；`.zrp` `aotMode: "full-aot"` 通过
  writer options 进入既有 full-AOT pre-emission guard；缺失 AOT C 输出会让 incremental dirty check
  触发重编译；关闭 `--emit-aot-c` 会清理可达模块旧 `.c`，removed manifest entries 会清理 v3 `aot_c` 路径。
  RED/GREEN：RED 为新增 CLI/project AOT C 发射测试引用缺失 command field/path resolver 后编译失败；
  GREEN 后 full-AOT generic project 输出 `descriptor.inputKind = 2` 与
  `zr_aot_generic_call_typed_full_aot_no_deopt`，且不含 missing-instance deopt bridge。
  验证：WSL gcc/clang `cli_args|cli_project_incremental` CTest 2/2；Windows MSVC Debug 同组 2/2；
  Windows MSVC CLI 实际命令删除 `main.c` 后 `--compile --emit-aot-c --incremental` 重新生成 C 文件；
  `git diff --check` 退出 0，仅 LF/CRLF 提示。产出：
  `tests/acceptance/2026-06-25-aot-11-s7h-cli-aot-c-emission-entry.md`。
  备注：本记录不声明 manifest generic roots、reflection preservation、annotation flow 或完整 closure
  diagnostic 已完成。

- 2026-06-25 00:29:49 +08:00 · 12-S8F / 11-S7G manifest full-AOT writer option bridge ·
  状态：12-S8 manifest policy 注入子切片完成；完整 12-S8 仍未关闭，CLI AOT C 发射入口接线、
  manifest 动态泛型实例、注解驱动反射保留和完整 full-AOT 闭合诊断仍待后续。
  完成项目：CLI/compiler 新增 `ZrCli_Compiler_ApplyProjectAotWriterOptions()`，将 `.zrp`
  解析出的 `SZrLibrary_Project.aotMode` 注入 `SZrAotWriterOptions.requireFullAot`；
  full-AOT manifest 置 true，hybrid/default 置 false，其他 writer options 不变。
  RED/GREEN：RED 为 CLI project incremental 测试新增 helper 调用后链接失败；GREEN 后
  full-AOT 与 hybrid/default 两条 writer option bridge 用例通过。
  验证：WSL gcc/clang `zr_vm_cli_project_incremental_test` 均 10/0；Windows MSVC Debug 同目标 10/0；
  Windows MSVC CLI smoke `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-25-aot-11-s7g-zrp-project-manifest-aot-mode-writer-injection.md`。
  备注：本记录不声明 CLI 已有 AOT C 输出命令，也不声明 manifest generic roots、reflection preservation
  或完整 closure diagnostics 完成。

- 2026-06-25 00:08:34 +08:00 · 12-S8 / 11-S7F manifest full-AOT mode declaration parser ·
  状态：12-S8 前置 manifest 子切片完成；完整 12-S8 仍未关闭，manifest 到 writer option 自动注入、
  manifest 动态泛型实例、注解驱动反射保留和完整 full-AOT 闭合诊断仍待后续。
  完成项目：`.zrp` project manifest loader 新增 top-level `aotMode` 解析，缺省 `hybrid`，显式
  `"full-aot"` 写入 `SZrLibrary_Project.aotMode`；非法 mode 拒绝 manifest。`zrp.schema.json`
  同步 `aotMode` enum。
  RED/GREEN：RED 为 manifest normalization 测试引用缺失 AOT mode project model 后编译失败；GREEN 后
  缺省 hybrid、显式 full-AOT 和非法 mode 拒绝均通过。
  验证：WSL gcc/clang `zr_vm_project_manifest_normalization_test` 14/0 与
  `zr_vm_project_import_resolver_test` 9/0；schema JSON 解析通过；Windows MSVC 同两 focused 测试 14/0、9/0，
  CLI smoke `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-25-aot-11-s7f-zrp-project-manifest-aot-mode.md`。
  备注：这是 full-AOT policy 的 manifest declaration surface，不表示 CLI/compiler 已消费该字段或完整闭合诊断已完成。

- 2026-06-24 23:36:19 +08:00 · 12-S4B / 11-S7E zrp manifest preserve declaration parser bridge ·
  状态：12-S4 子切片桥接完成；完整 12-S4 仍未关闭，symbol/token/flat-index 绑定、generic 实参、
  注解 roots、feature switch 与 writer option 自动注入仍待后续。
  完成项目：`.zrp` project manifest loader 新增 top-level `preserve` array 解析，`kind` 支持
  `type`、`method`、`generic`，`target` 使用 declaration-level 安全形态校验，`members` 可选并支持
  `all`/`methods`。解析结果保存在 `SZrLibrary_Project.preserveRules` / `preserveRuleCount`，
  为 12-S4A 已存在的 manifest function-root 输入通道提供 manifest 文件层前置数据结构。
  RED/GREEN：RED 为 project manifest normalization 测试新增 preserve 规则用例后编译失败，因为 project model
  尚无 preserve fields / enum；GREEN 后合法 type+method preserve 被解析，非法 target 被拒绝。
  验证：WSL gcc `zr_vm_project_manifest_normalization_test` 12/0 与 `zr_vm_project_import_resolver_test` 9/0；
  WSL clang 同两目标分别 12/0、9/0；`python -m json.tool zrp.schema.json` 通过；
  Windows MSVC `zr_vm_project_manifest_normalization_test` 12/0、`zr_vm_project_import_resolver_test` 9/0，
  CLI smoke `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s7e-zrp-project-manifest-preserve-rule-parsing.md`。
  备注：本记录只关闭 manifest declaration parser，不把 preserve target 映射到 token/function flat index，
  不触发 code-stripping reachability roots，也不实现 feature switch。

- 2026-06-24 21:04:24 +08:00 · 12-S7J runtime fallback warning source line ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、完整 source span、warning 抑制、
  zrp section/table/pool metadata 字节统计、字节级裁剪前后 type/layout 对比与 release 符号剥离仍待后续。
  完成项目：`trim_warning.runtimeFallback[...]` marker 现在在 `instruction` 与 `reason` 之间输出
  `sourceLine=<debugLine>`，由 `backend_aot_c_runtime_fallback.c` 从 ExecIR instruction 的 `debugLine`
  读取；没有 ExecIR 行号时输出 0。focused fixture 固定 source line 41，覆盖 dynamic call 与
  dynamic member/index value-access fallback warning。
  RED/GREEN：RED 为 dynamic deopt bridge smoke 要求 `sourceLine=41` 后 dynamic call/value-access
  hybrid warning marker 断言失败；GREEN 后三类 warning marker 带 sourceLine，full-AOT 拒绝路径保持通过。
  验证：`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 4/0；`zr_vm_aot_c_generic_call_typed_test` 6/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；`zr_vm_aot_c_code_stripping_test` 3/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7j-runtime-fallback-warning-source-line.md`。
  备注：本切片只把已有 ExecIR debug line 带到 warning marker，不实现列号/range、跨调用链依赖路径、
  warning 抑制、注解数据流、zrp 元数据裁剪或 release 符号剥离。

- 2026-06-24 20:48:06 +08:00 · 12-S7I runtime fallback diagnostics module split ·
  状态：12-S7I 支持性 refinement 完成；完整 12-S7 仍未关闭，完整 trim analyzer、zrp section/table/pool
  metadata 字节统计、字节级裁剪前后 type/layout 对比与 release 符号剥离仍待后续。
  完成项目：runtime fallback warning reason 扫描、`trim_warnings.runtimeFallbackCount` 统计、
  `trim_warning.runtimeFallback[...]` 输出以及 12-S8A-S8D full-AOT runtime closure 预检从
  `backend_aot_c_emitter.c` 拆入 `backend_aot_c_runtime_fallback.{h,c}`；emitter 保留写文件调度，
  从 897 行收回到 520 行，新模块为 294 行。
  RED/GREEN：本次为保持行为的支持拆分，未新增 RED；GREEN 复跑 focused 与相关 AOT 回归。
  验证：`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 4/0；`zr_vm_aot_c_generic_call_typed_test` 6/0；
  `zr_vm_aot_c_source_contracts_test` 19/0；`zr_vm_aot_c_code_stripping_test` 3/0。
  备注：本记录只整理 S7I/S8 runtime fallback diagnostics 归属，不新增 source span、warning 抑制、
  注解数据流、zrp 元数据裁剪或 release 符号剥离能力。

- 2026-06-24 20:28:06 +08:00 · 12-S7I runtime fallback trim warning reason classification ·
  状态：12-S7 子切片 refinement 完成；完整 12-S7 仍未关闭，完整 trim analyzer、zrp section/table/pool
  metadata 字节统计、字节级裁剪前后 type/layout 对比与 release 符号剥离仍待后续。
  完成项目：`trim_warning.runtimeFallback[...]` 的 `reason` 从单一 `dynamic-runtime` 文本拆为分类枚举，
  当前输出 `dynamic-call`、`dynamic-value-access`、`dynamic-iterator`、`reflection` 四类；focused 验收锁住
  SemIR dynamic call deopt bridge 的 `dynamic-call` 与 dynamic member/index value-access deopt bridge 的
  `dynamic-value-access`。
  RED/GREEN：RED 为 dynamic value-access hybrid smoke 要求 `reason=dynamic-value-access` 后失败；GREEN 后
  dynamic call/value-access hybrid 生成物均带分类 warning marker，full-AOT 拒绝路径保持通过。
  验证：`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 4/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7i-runtime-fallback-warning-reason-classification.md`。
  备注：本切片只细化 warning reason，不实现 source span、warning 抑制、注解数据流、zrp 元数据裁剪或
  release 符号剥离。

- 2026-06-24 20:17:59 +08:00 · 12-S7I runtime fallback trim warning markers ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、zrp section/table/pool metadata
  字节统计、字节级裁剪前后 type/layout 对比与 release 符号剥离仍待后续。
  完成项目：AOT C writer 在 hybrid 生成文件头部新增 `trim_warnings.runtimeFallbackCount`，并为每个会让
  full-AOT closure 拒绝、但 hybrid 仍保留 runtime fallback 的指令输出
  `trim_warning.runtimeFallback[index] function=<flatIndex> instruction=<instructionIndex> reason=dynamic-runtime`。
  当前复用 full-AOT runtime fallback 扫描，所以 dynamic call/value-access/iterator/reflection runtime boundary
  均进入同一 warning 类；focused 验收锁住 SemIR dynamic call deopt bridge 的第一条 marker。
  RED/GREEN：RED 为 dynamic deopt bridge hybrid smoke 要求 runtime fallback trim warning 后失败；GREEN 后
  hybrid dynamic deopt bridge 仍生成并可编译，full-AOT 拒绝用例保持通过。
  验证：`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 4/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7i-runtime-fallback-trim-warning-markers.md`。
  备注：本切片只发布 fallback warning marker，不实现注解数据流、warning 分级/去重、source span 诊断、
  zrp 元数据裁剪或 release 符号剥离。

- 2026-06-24 20:06:57 +08:00 · 12-S7H type-layout trim before/after statistics ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，trim warning、zrp section/table/pool metadata 字节统计、
  字节级裁剪前后 type/layout 对比与 release 符号剥离仍待后续。
  完成项目：AOT C writer 在 `enableCodeStripping` 生成文件头部新增
  `code_stripping.typeLayoutsBefore/After/Removed`，由 `backend_aot_c_type_layout_count_referenced()` 统计
  function table 中 distinct inline `typeLayoutId` 引用在 reachability filter 前后的变化；普通不可达 child
  裁剪路径验证 2→1/removed=1，export root 与 manifest root 保留路径验证 2→2/removed=0。
  RED/GREEN：RED 为 code-stripping 用例要求 type-layout before/after/removed marker 后 3 个用例失败；
  GREEN 后 3 条 opt-in 裁剪路径均通过。
  验证：`zr_vm_aot_c_code_stripping_test` 3/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7h-type-layout-trim-before-after-statistics.md`。
  备注：本切片只做 type-layout 引用数量级裁剪前后对比，不声明 generated layout byte delta、zrp 内部
  section/table/pool 明细、trim warning 或 release 符号剥离完成。

- 2026-06-24 19:35:15 +08:00 · 12-S7G generated method metadata byte statistics ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，trim warning、zrp section/table/pool metadata 字节统计、
  裁剪前后 type/layout 对比与 release 符号剥离仍待后续。
  完成项目：AOT C method-info emitter 已拆入 `backend_aot_c_method_metadata.{h,c}`，现在围绕每个
  `zr_aot_signature_<flatIndex>` 与
  `zr_aot_method_info_<flatIndex>` 输出块计算 byte span，并追加
  `aot_size.methodMetadataBytes[flatIndex] = <bytes>`；同一轮发射结束后输出
  `aot_size.methodMetadataBytesTotal = <bytes>`。RED/GREEN：RED 为 generic call typed 生成 C 用例要求
  method metadata byte marker 后失败；GREEN 后 source/binary/full-AOT 共享泛型用例仍通过，且生成物包含
  per-method 与 total method metadata size marker。
  验证：`zr_vm_aot_c_generic_call_typed_test` 6/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7g-method-metadata-byte-statistics.md`。
  备注：本切片只统计 generated AOT C 中的 signature/method-info descriptor metadata；后续整理把主
  `backend_aot_c_emitter.c` 收回到 691 个非空行，新 method metadata 模块为 326 个非空行。不声明 zrp 内部
  section/table/pool 明细、trim 前后 metadata 对比、trim warning 或 release 符号剥离完成。

- 2026-06-24 19:24:50 +08:00 · 12-S7F embedded module byte statistic ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，trim warning、zrp section/table/pool metadata 字节统计、
  裁剪前后 type/layout 对比与 release 符号剥离仍待后续。
  完成项目：AOT C writer 在生成文件头部继续保留 `descriptor.embeddedModuleBlobLength`，并新增
  `aot_size.embeddedModuleBytes = <bytes>`，数值来自 `SZrAotWriterOptions.embeddedModuleBlobLength`，
  用于把随 C 产物嵌入的 `.zro/.zrp` module blob 纳入 size attribution。RED/GREEN：RED 为
  generic call typed binary-AOT 共享泛型用例要求 `aot_size.embeddedModuleBytes` 后失败；GREEN 后生成 C
  同时含 descriptor length 与 size marker，运行时共享泛型 smoke 仍返回 `42`。
  验证：`zr_vm_aot_c_generic_call_typed_test` 6/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7f-embedded-module-byte-statistic.md`。
  备注：本切片只统计 embedded module blob carrier，不声明 zrp 内部 section/table/pool 明细、trim 前后
  metadata 对比、trim warning 或 release 符号剥离完成。

- 2026-06-24 19:10:02 +08:00 · 12-S8E full-AOT generic METHOD slot static closure ·
  状态：12-S8 子切片完成；完整 12-S8 仍未关闭，manifest 动态泛型实例、注解驱动反射保留与完整
  closure 诊断仍待后续。
  完成项目：full-AOT shared generic `CALL_TYPED` 在已静态解析 callee function index 的情况下不再保留
  `ZrAot_GenericSlot_Method()` runtime lookup 和 `if (... == ZR_NULL)` 缺失分支，直接调用
  `ZrLibrary_AotRuntime_CallInlineStruct(..., zr_aot_fn_<callee>)`；默认 hybrid 仍保留 METHOD slot lazy 解析与
  missing-instance deopt bridge。
  RED/GREEN：RED 为 generic call typed full-AOT 用例要求移除 METHOD slot null runtime branch 后失败；GREEN
  后 full-AOT 生成 C 仍包含 `zr_aot_generic_call_typed_full_aot_no_deopt`，但不再包含 METHOD slot null branch、
  missing-instance deopt marker 或动态 deopt bridge。
  验证：`zr_vm_aot_c_generic_call_typed_test` 6/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s8e-full-aot-generic-method-slot-closure.md`。
  备注：本切片只关闭已静态收集 shared generic callsite 的运行期 METHOD slot 缺失分支；不声明反射
  `MakeGenericType`、manifest 预声明动态泛型实例或完整“收集不全编译期报错”完成。

- 2026-06-24 18:52:22 +08:00 · 12-S8D full-AOT TYPEOF reflection runtime contract guard ·
  状态：12-S8 子切片完成；完整 12-S8 仍未关闭，泛型实例、manifest/注解驱动反射保留与裁剪闭包的完整
  full-AOT 诊断仍待后续。
  完成项目：full-AOT runtime-closure 预检从 dynamic deopt 扩展到 `TYPEOF` reflection runtime contract；
  当 bytecode/ExecIR/SemIR 命中 `TYPEOF` 时，writer 在 C 发射前返回 `ZR_FALSE` 并删除半成品 C 文件。
  默认 hybrid 路径继续生成并编译 `ZrLibrary_AotRuntime_TypeOf()` runtime boundary。RED/GREEN：RED 为
  global shared-library smoke 的 full-AOT TYPEOF fixture 仍成功生成；GREEN 后 full-AOT TYPEOF 产物被拒绝，
  hybrid TYPEOF runtime boundary 仍生成并编译。
  验证：`zr_vm_aot_c_global_shared_library_smoke_test` 10/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s8d-full-aot-typeof-reflection-closure.md`。
  备注：本切片只关闭未注解 `TYPEOF` runtime reflection boundary 的 full-AOT 缺口，不声明 10-S2 invoker、
  10-S3 token 解析、10-S5 注解/数据流或完整 mark-and-sweep closure checker 完成。

- 2026-06-24 18:42:16 +08:00 · 12-S8C full-AOT dynamic iterator deopt closure guard ·
  状态：12-S8 子切片完成；完整 12-S8 仍未关闭，反射/泛型实例与裁剪闭包的完整 full-AOT 诊断仍待后续。
  完成项目：`ZrParser_Writer_WriteAotCFileWithOptions()` 的 full-AOT dynamic-deopt 预检覆盖
  `DYN_ITER_INIT` / `DYN_ITER_MOVE_NEXT` / `SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE`，以及 ExecIR/SemIR 的
  `DYN_ITER_INIT` / `DYN_ITER_MOVE_NEXT`。这些 iterator runtime boundary 在 full-AOT 下会让 writer 返回
  `ZR_FALSE` 并删除半成品 C；默认 hybrid 路径继续生成并编译 iterator runtime helper 调用。RED/GREEN：
  RED 为 iterator shared-library smoke 的 full-AOT 动态迭代 fixture 仍成功生成；GREEN 后 full-AOT
  动态迭代产物被拒绝，hybrid iterator helper 产物仍生成并编译。
  验证：`zr_vm_aot_c_iterator_shared_library_smoke_test` 2/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s8c-full-aot-dynamic-iterator-closure.md`。
  备注：本切片只关闭 dynamic iterator deopt runtime boundary 的 full-AOT 缺口，不声明反射、dynamic generic
  instance 或完整 mark-and-sweep closure checker 完成。

- 2026-06-24 18:33:16 +08:00 · 12-S8B full-AOT dynamic value-access deopt closure guard ·
  状态：12-S8 子切片完成；完整 12-S8 仍未关闭，dynamic iterator、反射/泛型实例与裁剪闭包的完整
  full-AOT 诊断仍待后续。
  完成项目：`ZrParser_Writer_WriteAotCFileWithOptions()` 的 full-AOT 预检从 dynamic-call 闭包扩展为
  dynamic-deopt 闭包；当 ExecIR/SemIR 对应 `META_GET` / `META_SET` / `DYN_INDEX_GET` /
  `DYN_INDEX_SET` 时，writer 在 C 发射前返回 `ZR_FALSE` 并删除半成品 C 文件。默认 hybrid 路径继续生成
  `zr_aot_value_dynamic_deopt_bridge` 并链接运行时 member/index helper。RED/GREEN：RED 为 full-AOT
  dynamic value-access fixture 仍成功生成；GREEN 后 member/index 两类 full-AOT 产物均被拒绝，同时 hybrid
  value-access deopt bridge 仍生成并编译。
  验证：`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 4/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s8b-full-aot-dynamic-value-access-closure.md`。
  备注：本切片只关闭 dynamic member/index value-access deopt bridge 的 full-AOT 缺口，不声明 dynamic iterator、
  反射、dynamic generic instance 或完整 mark-and-sweep closure checker 完成。

- 2026-06-24 18:22:58 +08:00 · 12-S8A full-AOT dynamic-call deopt closure guard ·
  状态：12-S8 子切片完成；完整 12-S8 仍未关闭，dynamic member/index/iterator、反射/泛型实例与裁剪闭包的完整
  full-AOT 诊断仍待后续。
  完成项目：`ZrParser_Writer_WriteAotCFileWithOptions()` 在 `requireFullAot` 下、进入 C 发射前扫描当前
  `SZrAotFunctionTable` 与 ExecIR；如果 `FUNCTION_CALL` / `FUNCTION_TAIL_CALL` 的 SemIR 已是
  `DYN_CALL` / `DYN_TAIL_CALL`，或显式 `DYN_CALL` / `SUPER_DYN_*` call 仍无法通过 callable provenance
  静态解析出 callee，则关闭 dynamic deopt 兜底，返回 `ZR_FALSE` 并删除已打开的半成品 C 文件。RED/GREEN：
  RED 为 dynamic-deopt bridge smoke 在 full-AOT 选项下仍成功生成 `CallDynamicDeoptBridge` 产物；
  GREEN 后默认 hybrid 产物仍生成 deopt bridge，full-AOT 动态调用产物被拒绝。
  验证：`zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` 3/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s8a-full-aot-dynamic-call-closure.md`。
  备注：本切片只关闭 dynamic call deopt bridge 的 full-AOT 缺口，不声明 dynamic value access、反射、
  dynamic generic instance 或完整 mark-and-sweep closure checker 完成。

- 2026-06-24 18:06:18 +08:00 · 12-S7E generated type-layout byte total ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，trim warning、metadata 字节统计、裁剪前后
  type/layout 对比与 release 符号剥离仍待后续。
  完成项目：`backend_aot_c_type_layout_emit_one()` 改为返回单个 layout/generated descriptor block 的
  emitted byte span，`backend_aot_write_c_type_layout_declarations()` 累加这些 span 并在 type layout
  声明区末尾输出 `aot_size.typeLayoutBytesTotal = <bytes>`。RED/GREEN：RED 为 value-type shared-library
  smoke 要求 ref/POD 两类生成物含 type-layout 总量统计后失败；GREEN 后两类生成物均输出 per-layout
  和 total 统计。
  验证：`zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7e-type-layout-byte-total.md`。
  备注：这是 generated type-layout 总量统计，不声明 trim 前后对比、metadata 体积统计或符号剥离完成。

- 2026-06-24 17:58:34 +08:00 · 12-S7D generated type-layout byte statistics ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，trim warning、metadata 字节统计、裁剪前后
  type/layout 对比与 release 符号剥离仍待后续。
  完成项目：`backend_aot_c_type_layouts.c` 在每个 `ZrLayout_<typeLayoutId>` 与其同组
  generated GC descriptor block 发射完成后追加
  `aot_size.typeLayoutBytes[<typeLayoutId>] = <bytes>` 注释，字节数按进入
  `backend_aot_c_type_layout_emit_one()` 后、layout typedef/static asserts/descriptor 发射前后的输出位置差计算。
  POD layout 没有 GC descriptor 时仍输出 layout block 字节；引用字段 layout 输出包含 descriptor 的 block 字节。
  RED/GREEN：RED 为 value-type shared-library smoke 要求 ref/POD 两类生成物含 type-layout byte 统计后失败；
  GREEN 后两类生成物均输出 `aot_size.typeLayoutBytes[...]`，现有 GC descriptor ref/POD 行为保持不变。
  验证：`zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7d-type-layout-byte-statistics.md`。
  备注：这是 §6 体积统计中的 type-layout/generated-descriptor 字节入口，不声明 metadata pool/definition-table
  字节统计、trim analyzer warning 或符号剥离完成。

- 2026-06-24 17:17:43 +08:00 · 12-S7C retained function body byte total ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，trim warning、类型/layout/元数据字节统计与 release 符号剥离仍待后续。
  完成项目：opt-in AOT C code stripping 在所有已发射函数体之后追加
  `code_stripping.functionBodyBytesTotal = <bytes>` 注释，聚合 12-S7B 中每个保留函数的
  `backend_aot_write_c_function_body()` 输出跨度；被裁剪函数不会进入总量。RED/GREEN：RED 为
  code-stripping 生成 C 测试要求总函数字节统计后 3 个用例均失败；GREEN 后普通裁剪、export root、
  manifest root 三条路径都输出总函数字节统计。验证：`zr_vm_aot_c_code_stripping_test` 3/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7c-function-body-byte-total.md`。
  备注：这是 §6 体积统计的 retained-function 生成体总量入口；后续仍需裁剪前估算、类型/layout/metadata
  占用、trim analyzer warning 与 release 符号剥离。

- 2026-06-24 17:10:37 +08:00 · 12-S7B emitted function body byte statistics ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，trim warning、类型/layout/元数据字节统计与 release 符号剥离仍待后续。
  完成项目：opt-in AOT C code stripping 在每个已发射函数体后追加
  `code_stripping.functionBodyBytes[flatIndex] = <bytes>` 注释，字节数按调用
  `backend_aot_write_c_function_body()` 前后的输出位置差计算；被裁剪的函数不输出该项，export/manifest roots
  保留的函数会输出该项。RED/GREEN：RED 为 code-stripping 生成 C 测试要求每个保留函数的 body-byte
  统计后 3 个用例均失败；GREEN 后普通裁剪用例只含 0/1 的 body-byte 统计且不含 2，export/manifest 保留用例
  均含 0/1/2。验证：`zr_vm_aot_c_code_stripping_test` 3/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7b-function-body-byte-statistics.md`。
  备注：这是 §6 体积统计的 generated-C 函数体字节入口；后续仍需聚合总量、类型/layout/metadata 占用、
  trim analyzer warning 与 release 符号剥离。

- 2026-06-24 17:01:12 +08:00 · 12-S7A function stripping statistics ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，trim warning、类型/元数据字节级体积统计与 release 符号剥离仍待后续。
  完成项目：opt-in AOT C code stripping 在生成 C 文件头部输出
  `code_stripping.enabled`、`code_stripping.functionsBefore`、`code_stripping.functionsAfter`、
  `code_stripping.functionsRemoved` 注释，统计在 function table 过滤前后采样，保留 export/manifest root 时
  `functionsRemoved` 为 0，普通不可达 child 被移除时为 1。RED/GREEN：RED 为
  `zr_vm_aot_c_code_stripping_test` 要求统计注释后 3 个用例均失败，旧 generated C 没有任何裁剪统计；
  GREEN 后 opt-in 裁剪、export root、manifest root 三条路径都写出正确函数计数。
  验证：`zr_vm_aot_c_code_stripping_test` 3/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s7a-function-stripping-statistics.md`。
  备注：这是 §6 体积统计的最小函数级入口，不声明 trim analyzer、按类型/元数据字节占用统计或符号剥离完成。

- 2026-06-24 16:34:54 +08:00 · 12-S4A manifest preserve function roots ·
  状态：12-S4 子切片完成；完整 12-S4 仍未关闭，zrp manifest 解析、按 symbol/token 保留、
  注解 roots、feature switch 与 trim 诊断仍待后续。
  完成项目：`SZrAotWriterOptions` 新增
  `manifestPreserveFunctionFlatIndices` 与 `manifestPreserveFunctionFlatIndexCount`，用于承载已经解析好的
  manifest preserve 函数根；`backend_aot_compute_static_callable_reachability()` 接收这组 roots，并用
  `ZR_AOT_REACHABILITY_REASON_MANIFEST` 加入 BFS，重复 root 仍保留先到达原因，非法 flat index 或表中不存在的
  index 会被拒绝。opt-in C code stripping 将 writer options 中的 manifest roots 传入 graph helper。
  RED/GREEN：RED 为 focused reachability 测试改用 manifest-root 版 helper 后编译失败，当前 graph helper
  只有 entry/export roots；GREEN 后 manifest root 被保留为 `MANIFEST`，无效 manifest root 被拒绝，
  generated-C 测试证明 otherwise-unused `zr_aot_fn_2` 因 manifest 保留而继续发射。
  验证：`zr_vm_aot_reachability_test` 6/0、`zr_vm_aot_c_code_stripping_test` 3/0、
  CTest `aot_c_code_stripping|aot_reachability` 2/2、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s4a-manifest-preserve-function-roots.md`。
  备注：这是 manifest 规则的后端输入通道，不是 zrp manifest 文件格式或 feature switch 的完整实现。

- 2026-06-24 16:20:39 +08:00 · 12-S2E export callable roots ·
  状态：12-S2 子切片完成；完整 12-S2 仍未关闭，manifest roots、默认启用、trim 诊断与体积统计仍待后续。
  完成项目：`backend_aot_compute_static_callable_reachability()` 改为接收调用方提供的 root/root-reason
  缓冲区，继续以 entry flat index 0 作为 `ROOT_ENTRY`，并扫描 entry function 的
  `SZrFunctionTopLevelCallableBinding`；其中 `exportKind == ZR_MODULE_EXPORT_KIND_FUNCTION` 且
  `callableChildIndex` 有效的导出 callable 子函数会被加入 root 集并标记为
  `ZR_AOT_REACHABILITY_REASON_ROOT_EXPORT`。`backend_aot_apply_code_stripping()` 同步分配并传递 root
  缓冲区。focused reachability 测试证明未被 entry bytecode 引用但被导出的 child 不会被裁剪；
  generated-C opt-in 裁剪测试证明导出的 otherwise-unused child 仍会发射 `zr_aot_fn_2`，并进入 thunk/MethodInfo 表。
  RED/GREEN：RED 为 focused reachability 测试改用 root buffer 参数后编译失败，graph helper 仍是固定 entry-only root；
  GREEN 后 entry root 与 export root 同时进入 BFS，普通未引用 child 仍可裁剪。
  验证：`zr_vm_aot_reachability_test` 5/0、`zr_vm_aot_c_code_stripping_test` 2/0、
  CTest `aot_c_code_stripping|aot_reachability` 2/2、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s2e-export-callable-roots.md`。
  备注：本切片关闭 12-S2 的 export callable root 安全边界；manifest/注解保留、默认 writer 裁剪、
  trim warning、体积统计和 full-AOT 闭合仍开放。

- 2026-06-24 16:10:41 +08:00 · 12-S2D opt-in AOT C code stripping emitter ·
  状态：12-S2 子切片完成；完整 12-S2 仍未关闭，默认 AOT C writer 尚未启用裁剪，export/manifest
  roots、trim 诊断与体积统计仍待后续。
  完成项目：`SZrAotWriterOptions` 新增 `enableCodeStripping`，`backend_aot_option_enable_code_stripping()`
  提供后端统一读取；`ZrParser_Writer_WriteAotCFileWithOptions()` 在 opt-in 时调用 12-S2C 静态 callable
  reachability graph，随后用 12-S2A filter 压缩 function table，同时沿用 12-S2B `indexSpace`，让 thunk 与
  MethodInfo 表对不可达洞位输出 `ZR_NULL`。新增 `zr_vm_aot_c_code_stripping_test` 与 CTest
  `aot_c_code_stripping`，手工构造 root + reachable child + unused child，验证生成 C 中 `zr_aot_fn_2` 不再发射。
  RED/GREEN：RED 为新生成 C 测试编译失败，`SZrAotWriterOptions` 缺少 `enableCodeStripping`；GREEN 后
  opt-in 裁剪保留 `zr_aot_fn_0`/`zr_aot_fn_1`，删除 `zr_aot_fn_2`，并在 thunk/MethodInfo 表保留 `ZR_NULL` 洞位。
  验证：`cmake -S . -B build-wsl-gcc` 通过；`zr_vm_aot_c_code_stripping_test` 1/0、
  `zr_vm_aot_reachability_test` 4/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、CTest `aot_c_code_stripping|aot_reachability` 2/2。
  产出：`tests/acceptance/2026-06-24-aot-12-s2d-opt-in-code-stripping-emitter.md`。
  备注：本切片首次证明真实不可达函数可以不进入 generated C；为了避免误删导出/manifest/反射保留目标，
  默认 writer 路径仍保持全量生成。

- 2026-06-24 15:50:18 +08:00 · 12-S2C static callable reachability graph helper ·
  状态：12-S2 子切片完成；完整 12-S2 仍未关闭，默认 AOT C emitter 尚未启用过滤，export/manifest
  roots、死函数不进生成 C 与体积统计仍待后续。
  完成项目：新增 `backend_aot_reachability_function_graph.{h,c}` 与
  `backend_aot_compute_static_callable_reachability()`，以 entry flat index 0 为根，扫描真实 AOT bytecode 中的
  `GET_CONSTANT`、`CREATE_CLOSURE`、`GET_SUB_FUNCTION` 静态 callable materialization，并把可静态解析的目标
  写成 `DIRECT_CALL` 边后复用 12-S1A BFS 生成 `SZrAotReachabilityMark`。新增 focused 测试验证
  `GET_SUB_FUNCTION` 会标记子函数，同时未引用函数保持 `UNMARKED`，且 edge buffer 容量不足会被拒绝。
  RED/GREEN：RED 为 `zr_vm_aot_reachability_test` 引入新图 helper 后编译失败，缺少
  `backend_aot_reachability_function_graph.h`；GREEN 后 root+child 标记、edge reason/predecessor、未用函数未标记
  和容量拒绝均通过。
  验证：`cmake --build build-wsl-gcc --target zr_vm_aot_reachability_test -j2` 通过、
  `zr_vm_aot_reachability_test` 4/0、CTest `aot_reachability` 1/1、`zr_vm_aot_c_source_contracts_test` 19/0。
  产出：`tests/acceptance/2026-06-24-aot-12-s2c-static-callable-reachability-graph.md`。
  备注：本切片只提供默认过滤所需的第一类真实 graph input；为了避免误删导出/manifest/反射保留目标，
  仍未把过滤结果接入默认 C emitter。

- 2026-06-24 15:37:08 +08:00 · 12-S2B sparse AOT thunk/method-info index space ·
  状态：12-S2 子切片完成；完整 12-S2 仍未关闭，默认 AOT C emitter 尚未把真实 reachability graph
  接入过滤决策，死函数不进产物和体积下降统计仍待后续。
  完成项目：`SZrAotFunctionTable` 新增 `indexSpace`，`backend_aot_build_function_table()` 保存原始
  function index 空间，`backend_aot_filter_function_table_by_reachability()` 只压缩可发射 entries 而不缩短
  index 空间；`backend_aot_function_table_index_space()` 为后端提供稳定索引跨度。C emitter 的 forward decl、
  `zr_aot_function_thunks[]`、`zr_aot_method_infos[]` 和 descriptor count 改用原始 `flatIndex`/`functionIndexSpace`，
  对不可达洞位输出 `ZR_NULL`，避免后续裁剪把运行期按 function index 访问的 thunk 表重排。
  RED/GREEN：RED 为新增 `backend_aot_function_table_index_space()` 调用后链接失败，以及 frame setup source
  contract 要求 sparse emitter helper/`functionIndexSpace`/`ZR_NULL` 洞位时缺少对应文本；GREEN 后过滤表
  保持 index space=4，C emitter source contract 命中按 `entry->flatIndex` 发射和稀疏表洞位。
  验证：`zr_vm_aot_reachability_test` 3/0、`zr_vm_aot_c_frame_setup_contracts_test` 1/0、
  `zr_vm_aot_c_source_contracts_test` 19/0、`zr_vm_aot_c_shared_library_smoke_test` 8/0、CTest
  `aot_reachability` 1/1。产出：
  `tests/acceptance/2026-06-24-aot-12-s2b-sparse-aot-index-space.md`。
  备注：本切片是 12-S2 默认裁剪接入的 ABI 前置条件；尚未扫描真实函数体或按 root/export/manifest
  生成 reachability marks。

- 2026-06-24 15:13:07 +08:00 · 12-S2A function table reachability filter helper ·
  状态：12-S2 子切片完成；完整 12-S2 仍未关闭，默认 AOT C emitter 尚未消费可达性结果，真实死函数不进
  生成 C 与体积下降统计仍待后续。
  完成项目：`backend_aot_function_table.{h,c}` 新增
  `backend_aot_filter_function_table_by_reachability()`，对既有 `SZrAotFunctionTable` 原地压缩，只保留
  `SZrAotReachabilityMark` 中非 `UNMARKED` 的函数项；过滤时保留原始 `flatIndex`，避免提前破坏
  method table、direct-call index 和诊断中依赖的稳定编号；当表结构非法或 `flatIndex >= markCount` 时返回失败。
  RED/GREEN：RED 为 `test_aot_reachability.c` 新增 function table 过滤用例后链接失败，缺少
  `backend_aot_filter_function_table_by_reachability`；GREEN 后 4 个函数项按 mark 结果压缩为 0/2 两个
  可达项，`flatIndex` 分别保持 0/2，且 mark 数不足的输入被拒绝。
  验证：`zr_vm_aot_reachability_test` 3/0、CTest `aot_reachability` 1/1、
  `zr_vm_aot_c_source_contracts_test` 19/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s2a-function-table-reachability-filter.md`。
  备注：本切片只建立 sweep 入口的最小可验证单元；函数体扫描、root/export/manifest 图构建、默认
  emitter 过滤、体积统计和 full-AOT 闭合诊断仍开放。

- 2026-06-24 15:04:13 +08:00 · 12-S1A AOT reachability state machine + BFS ·
  状态：12-S1 子切片完成；完整 12-S1 仍未关闭，扫描 SemIR/字节码收集边、虚/接口/反射/泛型传播、
  以及“死函数不进产物”的生成器接入验收仍待 12-S2/后续。
  完成项目：新增 `backend_aot_reachability.{h,c}`，定义
  `ZR_AOT_REACHABILITY_STATE_UNMARKED` / `MARKED_PENDING` / `PROCESSED` 三态，
  `ROOT_ENTRY`、`ROOT_EXPORT`、`MANIFEST`、`DIRECT_CALL`、`FIELD_ACCESS`、`VIRTUAL_CALL`、
  `REFLECTION`、`GENERIC_INSTANCE` dependency reason，以及 `SZrAotReachabilityEdge` /
  `SZrAotReachabilityMark`；`backend_aot_reachability_compute()` 对调用方提供的 mark/queue 缓冲区执行
  BFS，记录首次标记原因和 predecessor，并拒绝越界 root/edge 或容量不足的队列。
  RED/GREEN：RED 为新增测试配置后缺少 `backend_aot_reachability.h`；GREEN 后根、直接调用、字段访问
  传播集合正确，未连接节点保持 unmarked，manifest root 的原因不被后续 direct-call 边覆盖，非法图被拒绝。
  验证：`zr_vm_aot_reachability_test` 2/0、CTest `aot_reachability` 1/1、
  `zr_vm_aot_c_source_contracts_test` 19/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s1a-reachability-engine.md`。
  备注：本切片只提供裁剪上游的通用标记引擎；尚未扫描真实函数体，也尚未过滤
  `backend_aot_function_table` 的全量收集结果。
