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
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h # 12-S7ZZD SZrAotMemberTokenRemap ABI + descriptor/codeRegistration fields; 11-S7ZE/12-S7 SZrAotManifestExportEntry ABI + descriptor/codeRegistration fields
  - zr_vm_core/src/zr_vm_core/zrp_metadata.c # 12-S7ZZS manifestExports section validation; 12-S7ZZW/11-S7ZSF unbound manifest export row validation
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h # 12-S7ZZQ member-token remap count mirror for attached AOT metadata runtime; 12-S7 support/11-S7ZF manifest export table runtime mirror; 12-S7 support/11-S7ZG manifest export runtime view API; 12-S7 support/11-S7ZH manifest export binding gate API/status
  - zr_vm_core/src/zr_vm_core/metadata_runtime_manifest_exports.c # 12-S7 support/11-S7ZG manifest export runtime view implementation
  - zr_vm_core/src/zr_vm_core/metadata_runtime_binding_compatibility.c # 12-S7 support/11-S7ZH manifest export view + binding compatibility predicate
  - zr_vm_core/src/zr_vm_core/module/module.c # 12-S7ZZQ applies codeRegistration member-token remaps to loaded typed exports; 12-S7 support/11-S7ZF mirrors codeRegistration manifest export table into metadata runtime
  - zr_vm_library/src/zr_vm_library/aot_runtime.c # 12-S7ZZD/S7ZZE/S7ZZF descriptor/codeRegistration member-token remap pointer/count, entry, and duplicate validation; 11-S7ZE/12-S7 manifest export table validation; 11-S7ZL/12-S7 provider AOT load-request runtime consumption
  - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c # 12-S7ZZD/S7ZZE/S7ZZF mirrored member-token remap pointer/count, entry, and duplicate validation; 11-S7ZE/12-S7 mirrored manifest export table validation
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z28/11-S4BN retained nested primitive POD storage-width path matrix coverage
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z27/11-S4BM retained nested primitive POD representative path matrix coverage
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.c # 12-S5 support/10-S4Z26/11-S4BL retained nested primitive raw child leaf layout identity guard
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z26/11-S4BL retained nested primitive raw child leaf layout guard coverage
  - zr_vm_core/include/zr_vm_core/reflection.h # 12-S5 support/10-S4Z25/11-S4BK retained nested inline primitive POD path read/write API
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 12-S5 support/10-S4Z25/11-S4BK retained nested inline primitive POD path adapter
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.c # 12-S5 support/10-S4Z25/11-S4BK retained recursive nested inline primitive path traversal
  - zr_vm_core/src/zr_vm_core/reflection_field_value_primitive.c # 12-S5 support/10-S4Z25/11-S4BK retained primitive POD raw load/store guard
  - zr_vm_core/src/zr_vm_core/reflection_field_value_primitive.h # 12-S5 support/10-S4Z25/11-S4BK retained primitive POD raw load/store guard API
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z25/11-S4BK retained nested inline primitive POD path coverage
  - zr_vm_core/include/zr_vm_core/reflection.h # 12-S5 support/10-S4Z24/11-S4BJ retained nested inline VALUE_SLOT path write API
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 12-S5 support/10-S4Z24/11-S4BJ retained nested inline VALUE_SLOT path write adapter
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.c # 12-S5 support/10-S4Z24/11-S4BJ retained recursive nested inline layout path write traversal
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z24/11-S4BJ retained nested inline VALUE_SLOT path write coverage
  - zr_vm_core/include/zr_vm_core/reflection.h # 12-S5 support/10-S4Z23/11-S4BI retained nested inline VALUE_SLOT path read API
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 12-S5 support/10-S4Z23/11-S4BI retained nested inline VALUE_SLOT path adapter
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.c # 12-S5 support/10-S4Z23/11-S4BI retained recursive nested inline layout path traversal
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z23/11-S4BI retained nested inline VALUE_SLOT path read coverage
  - zr_vm_core/include/zr_vm_core/reflection.h # 12-S5 support/10-S4Z22/11-S4BH retained nested inline VALUE_SLOT write API
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 12-S5 support/10-S4Z22/11-S4BH retained nested inline VALUE_SLOT write path
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z22/11-S4BH retained nested inline VALUE_SLOT write coverage
  - zr_vm_core/include/zr_vm_core/reflection.h # 12-S5 support/10-S4Z21/11-S4BG retained nested inline VALUE_SLOT read API
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 12-S5 support/10-S4Z21/11-S4BG retained nested inline VALUE_SLOT read path
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z21/11-S4BG retained nested inline VALUE_SLOT read coverage
  - zr_vm_core/src/zr_vm_core/type_layout.c # 12-S5 support/10-S4Z20/11-S4BF retained inline aggregate nested VALUE_SLOT replacement/drop coverage
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z20/11-S4BF retained inline aggregate replacement/drop write coverage
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 12-S5 support/10-S4Z19/11-S4BE retained inline aggregate field-copy borrowed-source write path
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 12-S5 support/10-S4Z18/11-S4BD retained inline aggregate borrowed-source write path
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 12-S5 support/10-S4Z17/11-S4BC retained inline struct borrowed-view path
  - zr_vm_core/include/zr_vm_core/reflection.h         # 12-S5 support/10-S4Z16/11-S4BB FieldInfo object primitive POD coverage for retained metadata
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 12-S5 support/10-S4Z16/11-S4BB retained FieldInfo object-level primitive POD path
  - zr_vm_core/include/zr_vm_core/reflection.h         # 12-S5 support/10-S4Z15/11-S4BA FieldInfo object value write API for retained metadata
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 12-S5 support/10-S4Z15/11-S4BA retained FieldInfo object-level write adapter
  - zr_vm_core/include/zr_vm_core/reflection.h         # 12-S5 support/10-S4Z14/11-S4AZ FieldInfo object value read API for retained metadata
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 12-S5 support/10-S4Z14/11-S4AZ retained FieldInfo object-level read adapter
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.h   # 12-S2A reachability filter API；现状默认仍全量收集
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.c   # 12-S2A function table 可达项压缩
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c                  # shared AOT writer option normalization；12-S7Y default-min reflection metadata policy；12-S7ZU writer-level annotation warning suppression option helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_internal.h         # shared AOT writer option normalization API；12-S7Y reflection metadata policy option helper；12-S7ZU annotation warning suppression option API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c        # 12-S2B 稀疏 thunk/method-info 发射调度；12-S2D/S2E opt-in 裁剪接入与 root buffer；12-S4A manifest roots；12-S4C top-level callable flat-index resolver；12-S4H/S4I/S4J generic root diagnostics + TypeSpec token/hash/generic instance identity；12-S5A reflection annotation roots + code_stripping.annotationRoot markers；12-S5B/12-S7 annotation warning count/writer plumbing；12-S5C reason-text marker compatibility；12-S5D dynamic dependency function roots continue through annotationRoot markers；12-S5I/10-S5J dynamicDependencyTypeLayoutId root markers and root-aware type-layout stats/writers；12-S5J/10-S5K dynamicDependencyTypeToken metadata blob plumbing and typeLayout token table fallback；12-S5L/10-S5M bound TypeRef dynamicDependencyTypeToken roots；12-S5K/10-S5L dynamicDependencyFieldToken reuses type-layout root plumbing；12-S7A/S7B/S7C 函数裁剪统计；12-S7F embedded module byte statistic；12-S7H type-layout trim before/after 统计；12-S7I/S8 runtime fallback diagnostics 调度；12-S7K zrp metadata section/table/pool byte statistics；12-S7L type-layout payload byte trim delta；12-S7M runtime fallback warning suppression；12-S7O runtime fallback warning reason-mask suppression；12-S7R generated-C type-layout byte trim delta；12-S7S zrp metadata byte trim delta carrier；12-S7T delegates zrp metadata size accounting；12-S7U symbol-stripping option marker/plumbing；12-S7V method metadata generated-C byte trim delta；12-S7Y metadata policy marker/plumbing；12-S7Z emitted zrp metadata pruning plumbing；12-S7ZT visible/suppressed runtime fallback reason-mask aggregate markers；12-S7ZU annotation warning visible/suppressed count split and writer-level suppression plumbing；12-S8H/S8I full-AOT manifest generic TypeSpec/generic-instantiation gate；11-S7ZA/12-S7 manifest export declaration diagnostics；12-S7ZZX/11-S7ZSG final compacted .zrp metadata sidecar publication orchestration
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_annotation_warnings.h # 12-S5B/12-S7 requires-unreferenced-code annotation warning API; 12-S5C reason text stays on same warning API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_annotation_warnings.c # 12-S5B/12-S7 retained static-call scanner for `requiresUnreferencedCode: true` callee metadata; 12-S5C `requiresUnreferencedCodeReason` quoted marker output
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_size.h # 12-S7T zrp metadata size/delta accounting API；12-S7Z blob-based after-stats sampling；12-S7ZO section-level trim delta marker surface；12-S7ZP section count marker fields
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_size.c # 12-S7T zrp metadata header sampling + size/delta marker writing；12-S7Z pruned-blob stats；12-S7ZO per-section before/after/removed marker writing；12-S7ZP per-section count stats/delta marker writing
- zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.h # 12-S7Z emitted zrp metadata pruning API; 12-S7ZA token-record MethodDef remap surface; 12-S7ZB FieldDef shared MEMBER_DEF remap; 12-S7ZC GenericParam owner/range remap; 12-S7ZD remap module split surface; 12-S7ZG signature blob pool compaction orchestration; 12-S7ZH string-pool compaction orchestration; 12-S7ZI constant-pool orphan sweep API surface; 12-S7ZZV/11-S7ZSE TypeDef token remap sidecar state
- zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.c # 12-S7Z compacted blob rebuild orchestration; 12-S7ZD delegates token/range remap helpers; 12-S7ZE GenericParamConstraint section copy/compaction orchestration; 12-S7ZF MethodSpec section copy/compaction orchestration; 12-S7ZG signature blob pool compaction/rewrite orchestration; 12-S7ZH delegates shared section helpers and string-pool remap/copy; 12-S7ZI zero-retained constant-pool layout; 12-S7ZM post-remap identity skip so pool compaction runs without MethodDef pruning; 12-S7ZZC retained signature blob TypeDef token rewrite hook; 12-S7ZZJ retained SIGNATURE token rewrite hook; 12-S7ZZK prune failure cleanup; 12-S7ZZL GenericParamConstraint TypeSpec token remap; 12-S7ZZM GenericParamConstraint TypeSpec root context threading; 12-S7ZZN signature AssemblyRef rewrite context; 12-S7ZZP builds signature remap before ModuleRef count/string remap and threads retained signature roots into ModuleRef pruning; 12-S7ZZT/11-S7ZSC delegates manifestExports section rewrite; 12-S7ZZU/11-S7ZSD invokes declaration row publication after prepare/prune; 12-S7ZZV/11-S7ZSE builds TypeDef token remap before type declaration publication; 12-S7ZZZJ/11-S7ZSQ threads token-record context into MethodSpec count/copy/rewrite; 12-S7ZZZK/11-S7ZSR threads retained imported MEMBER_REF context into MethodSpec count/copy/rewrite
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_publication.h # 12-S7ZZX/11-S7ZSG final compacted .zrp metadata sidecar publication API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_publication.c # 12-S7ZZX/11-S7ZSG final metadata sidecar publication; 12-S7ZZZQ/11-S7ZSX header + definition-table validation and stale sidecar cleanup
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_member_token.h # 12-S7ZX/12-S7ZZD retained member-token sidecar API; 11-S7ZE/12-S7 manifest export table build API; 12-S7ZZV/11-S7ZSE TypeDef token remap query API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_member_token.c # 12-S7ZX/12-S7ZZD source-to-compacted member-token remap sidecar; 12-S7ZZG source duplicate guard; 12-S7ZZH token-shape guard; 12-S7ZZI retained-count consistency guard; 11-S7ZE/12-S7 manifest export table builder with compacted member-token rewrite; 11-S7ZSA/12-S7ZZR manifest export kind/token guard; 12-S7ZZZR/11-S7ZSY manifest export duplicate kind+target guard; 12-S7ZZV/11-S7ZSE manifest export type-token rewrite through TypeDef remap sidecar
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.h # 12-S7ZZT/11-S7ZSC persistent manifest export section copy/rewrite API; 12-S7ZZU/11-S7ZSD declaration row publication API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.c # 12-S7ZZT/11-S7ZSC remaps manifest export target string offsets, type tokens, and member tokens during pruned .zrp rebuild; 12-S7ZZU/11-S7ZSD appends bound method/field declaration rows with string-pool growth and member-token remap; 12-S7ZZV/11-S7ZSE appends bound type declaration rows with compacted TypeDef tokens; 12-S7ZZW/11-S7ZSF preserves/publishes unbound rows with zero flags/tokens
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_def.h # 12-S7ZZV/11-S7ZSE TypeDef token remap sidecar build/destroy API for manifest export type declaration publication; 12-S7ZZZG/11-S7ZSN TypeDef retained FieldDef owner-token root context
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_def.c # 12-S7ZZV/11-S7ZSE retained TypeDef source-to-compacted token remap sidecar construction and cleanup; 12-S7ZZZG/11-S7ZSN TypeDef token-record root guard rejects pruned/self-owner FieldDef roots
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_constant_pool.h # 12-S7ZV FieldDef default-value constant-pool retained-slice remap API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_constant_pool.c # 12-S7ZV retained constant-pool slice collection, compacted copy, identity check, and FieldDef offset rewrite
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.h # 12-S7ZD private zrp metadata token/range remap API; 12-S7ZE GenericParamConstraint remap/count/range API; 12-S7ZF MethodSpec remap/count API; 12-S7ZN export member-token remap surface; 12-S7ZZZJ/11-S7ZSQ MethodSpec imported MEMBER_REF token-record guard context; 12-S7ZZZK/11-S7ZSR MethodSpec imported MEMBER_REF retained-token-record context
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.c # 12-S7ZA..S7ZC MethodDef/FieldDef/GenericParam token/range remap implementation; 12-S7ZE GenericParamConstraint cascade implementation; 12-S7ZF MethodSpec method-token cascade implementation; 12-S7ZN exported MethodDef/FieldDef member token remap helper; 12-S7ZZI retained-count consistency helper; 12-S7ZZZJ/11-S7ZSQ imported MEMBER_REF token-record existence guard; 12-S7ZZZK/11-S7ZSR imported MEMBER_REF retained-token-record guard; 12-S7ZZZL/11-S7ZSS recursive imported MEMBER_REF retained-token-record guard
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_sections.h # 12-S7ZH shared zrp metadata section lookup/layout/copy API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_sections.c # 12-S7ZH shared section switch, layout writer, and raw-section copy helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_module_ref.h # 12-S7ZZ/12-S7ZZP ModuleRef row retention/count/compacted-token/remap API, including retained signature blob AssemblyRef roots
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_module_ref.c # 12-S7ZZ ModuleRef orphan sweep/AssemblyRef compaction; 12-S7ZZP retained signature blob scanner roots ModuleRef rows referenced only by signature payloads
- zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.h # 12-S7ZG signature blob remap/compaction API; 12-S7ZM signature remap identity API; 12-S7ZZC retained signature TypeDef token rewrite API; 12-S7ZZJ retained SIGNATURE token remap API; 12-S7ZZK retained SIGNATURE orphan rejection contract; 12-S7ZZN ModuleRef context for signature AssemblyRef token rewrite; 12-S7ZZP source signature-pool context for signature-rooted ModuleRef retention; 12-S7ZZZJ/11-S7ZSQ MethodSpec signature rewrite token-record guard context; 12-S7ZZZK/11-S7ZSR MethodSpec signature retained MEMBER_REF guard context
- zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.c # 12-S7ZG retained signature blob slice collection, MethodSpec signature rewrite, offset remap, and hash recomputation; 12-S7ZM signature remap identity helper; 12-S7ZZC recursive retained signature TypeDef token rewrite; 12-S7ZZJ retained signature-token-record order compaction/remap; 12-S7ZZK missing retained signature token rejection; 12-S7ZZM constraint-rooted TypeSpec signature blob retention; 12-S7ZZN retained signature AssemblyRef token rewrite; 12-S7ZZO retained signature MemberRef token rewrite; 12-S7ZZP signature rewrite context preserves source signature-pool remap for ModuleRef retained-root checks; 12-S7ZZZJ/11-S7ZSQ MethodSpec signature retention skips orphan imported MEMBER_REF records; 12-S7ZZZK/11-S7ZSR MethodSpec signature retention skips imported MEMBER_REF records with pruned member targets; 12-S7ZZZM/11-S7ZST retained TYPE_REF name string offset remap; 12-S7ZZZN/11-S7ZSU retained MODULE name/version string offset remap
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_spec.h # 12-S7ZW TypeSpec retained-row API; 12-S7ZY TypeSpec compacted-token/remap API; 12-S7ZZM GenericParamConstraint-root retention context
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_spec.c # 12-S7ZW TypeSpec orphan sweep; 12-S7ZY TypeSpec RID compaction; 12-S7ZZM retained GenericParamConstraint-rooted TypeSpec row detection
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_string_pool.h # 12-S7ZH string-pool remap/compaction API; 12-S7ZL duplicate retained string-slice remap support; 12-S7ZM string remap identity API; 12-S7ZZP signature-rooted ModuleRef string retention context; 12-S7ZZT/11-S7ZSC public manifest target string offset remap helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_string_pool.c # 12-S7ZH retained string slice collection, compacted string-pool copy, and row string-offset remap; 12-S7ZL content-level duplicate slice interning; 12-S7ZM string remap identity helper; 12-S7ZZP keeps ModuleRef name/version strings when only retained signature blobs reference the row; 12-S7ZZT/11-S7ZSC exports shared string-offset remapper for manifest export rows; 12-S7ZZZM/11-S7ZST retained signature TYPE_REF name string root scan; 12-S7ZZZN/11-S7ZSU retained signature MODULE name/version string root scan
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_monomorphization.h # 12-S7U generated-symbol stripping option API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_monomorphization.c # 12-S7U stable-ID private helper symbols for monomorphized generic value forms
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.h # 12-S7U generated-symbol stripping option API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.c # 12-S7U stable-ID private helper symbols/debug names for shared generic reference forms
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.h # 12-S7G generated method metadata emitter boundary；12-S7V generated method metadata byte sampling API；12-S7Y reflection metadata level emitter parameter
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c # 12-S7G generated signature/method-info byte span + table emission；12-S7V generated method metadata byte sampling helper；12-S7Y policy-driven MethodInfo reflection level emission；12-S7ZX/12-S7ZZD method-token table consumes retained member-token remap
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
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_runtime_fallback.h # 12-S7I/S8A-S8D runtime fallback warning/full-AOT closure diagnostics API；12-S7O reason-mask suppression API；12-S7ZT visible/suppressed reason-mask aggregation API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_runtime_fallback.c # 12-S7I runtime fallback warning reason scan；12-S7N sourceLineEnd marker；12-S7O reason-mask suppression filter；12-S7P sourceColumn/sourceColumnEnd marker；12-S7Q sourceFile attribution；12-S7ZJ reasonFlag marker；12-S7ZK quoted/escaped sourceFile marker；12-S7ZT visible/suppressed runtime fallback reason-mask aggregation；12-S8A/S8B/S8C dynamic deopt 与 12-S8D reflection full-AOT 闭合预检
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c # 12-S8E full-AOT generic METHOD slot static closure
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c   # 12-S7D/S7E type-layout/generated descriptor byte span + total；12-S7H distinct referenced inline type-layout count；12-S7L referenced inline type-layout payload bytes；12-S7R generated-C type-layout byte span sampling helper；12-S5I/10-S5J dynamicDependencyTypeLayoutId root-aware retention；12-S5K/10-S5L annotation metadata roots orchestration after helper split
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_metadata_roots.c # 12-S5J/10-S5K dynamicDependencyTypeToken TypeDef/TypeSpec token -> typeLayout root resolution; 12-S5L/10-S5M bound TypeRef token-record -> target TypeDef typeLayout root resolution; 12-S5K/10-S5L dynamicDependencyFieldToken FieldDef -> owner/field typeLayout roots
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_tokens.c # 12-S5J/10-S5K root-only TypeDef/TypeSpec token fallback for dynamic dependency type roots; 11-S4Q generated TypeSpec-backed token table population
  - zr_vm_core/src/zr_vm_core/metadata_runtime_layout_binding.c # 12-S5M/10-S5N runtime attached bound TypeRef token -> TypeDef layout resolver for annotation-root retained layouts
  - zr_vm_core/src/zr_vm_core/reflection.c # 10-S4F/11-S4T minimal FieldDef token FieldInfo object consumer for annotation-root-retained FieldDef metadata; 10-S4G/11-S4U FieldInfo declaring type carrier consumer; 10-S4H/11-S4V FieldInfo owner link consumer; 10-S4I/11-S4W FieldInfo moduleName carrier consumer; 10-S4J/11-S4X FieldInfo raw metadata flags carrier consumer; 10-S4K/11-S4Y FieldInfo raw signature blob coordinate carrier consumer; 10-S4L/11-S4Z FieldInfo validated field signature header carrier consumer; 10-S4M/11-S4AA FieldInfo field signature type-node summary carrier consumer; 10-S4N/11-S4AB FieldInfo primitive signature type carrier consumer; 10-S4O/11-S4AC FieldInfo primitive signature type object carrier consumer; 10-S4P/11-S4AD FieldInfo module reflection object carrier consumer; 10-S4Q/11-S4AE FieldInfo direct TypeDef signature token/layout carrier consumer; 10-S4R/11-S4AF FieldInfo direct TypeDef signature type object carrier consumer; 10-S4S/11-S4AG FieldInfo bound TypeRef signature token/layout/type object carrier consumer; 10-S4T/11-S4AH FieldInfo signature/layout consistency carrier consumer; 10-S4U/11-S4AI FieldInfo signature type-node object carrier consumer; 10-S4V/11-S4AJ FieldInfo signature base type-node object carrier consumer; 10-S4W/11-S4AK FieldInfo signature child type-node object list carrier consumer; 10-S4X/11-S4AL FieldInfo primitive child type-node semantic name consumer; 10-S4Y/11-S4AM FieldInfo direct TypeDef child/base type-node semantic token/layout/name consumer; 10-S4Z2/11-S4AN FieldInfo direct TypeRef child type-node semantic token/layout/name consumer; 10-S4Z3/11-S4AO FieldInfo recursive signature type-node type literal consumer; 10-S4Z13/11-S4AY FieldInfo metadata runtime native-pointer carrier consumer
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z4..10-S4Z12/11-S4AP..11-S4AX FieldInfo token value boundary for retained FieldDef/layout/signature metadata; VALUE_SLOT copy plus primitive POD raw scalar read/write with representative/full storage-width matrix coverage plus integer, float32 range, float32 NaN, and float32 precision guards
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_callable_provenance.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_callable_provenance.c # 12-S5B GET_SUB_FUNCTION callable-slot provenance for annotation warning target resolution
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.h     # 12-S1A mark state/reason API；12-S5A reflection annotation root reason；12-S5D dynamic dependency roots reuse this reason
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c     # 12-S1A BFS mark engine
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.h # 12-S2C static callable graph API；12-S2E caller-provided roots；12-S4A manifest root input；12-S5A reflection annotation root collection API；12-S5D dynamic dependency function root collection API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c # 12-S2C bytecode callable edge scan；12-S2E export callable roots；12-S4A manifest roots；12-S5A `reflectable: true` decorator metadata roots；12-S5D `dynamicDependencyFunctionIndex` decorator metadata roots；12-S5E/10-S5F `dynamicDependencyMethodToken` MEMBER_DEF token roots through root typed exported symbols；12-S5F/10-S5G `dynamicDependencyMethodName` exported method-name roots；12-S5G/10-S5H optional `dynamicDependencyMethodSignatureHash` signature disambiguation and ambiguous-name rejection；12-S5H/10-S5I current-module non-exported `dynamicDependencyMethodToken` function-symbol roots without requiring exported `exportKind`
  - zr_vm_parser/include/zr_vm_parser/writer.h # 12-S2D enableCodeStripping opt-in；12-S4A parsed manifest preserve function roots；12-S4C top-level callable flat-index resolve API；12-S4H/S4I/S4J/12-S4N/12-S8I manifest generic roots + TypeSpec/generic-instantiation/MethodSpec binding fields；11-S7ZA/12-S7 manifest export declaration writer options；12-S7M suppressRuntimeFallbackWarnings writer option；12-S7O suppressRuntimeFallbackWarningReasonMask writer option；12-S7U stripGeneratedSymbols writer option；12-S7ZU suppressAnnotationWarnings writer option；12-S7ZZX compactedZrpMetadataOutputPath sidecar option
  - zr_vm_library/include/zr_vm_library/project.h # 11-S7E/12-S4B parsed zrp preserve rule model；11-S7F parsed aotMode model；11-S7K/12-S4E preserve feature condition model；11-S7L/12-S4F feature switch map；11-S7M/12-S4G generic preserve arguments；11-S7Z/12-S7 export declaration model；11-S7ZK/12-S7 provider AOT load-request model
  - zr_vm_library/src/zr_vm_library/project/project.c # 11-S7ZN/12-S7 declared provider version-range guard
  - zr_vm_library/src/zr_vm_library/project/project_import_provider_location.c # 11-S7ZJ/12-S7 provider location discovery；11-S7ZK/12-S7 provider AOT load-request planning
  - tests/library/test_project_import_aot_provider_runtime.c # 11-S7ZL/12-S7 provider AOT runtime load-request consumption coverage
  - tests/library/test_project_import_provider_version_selection.c # 11-S7ZN/12-S7 provider multi-version exact alias selection + declared range guard
  - tests/parser/test_aot_c_provider_shared_library_smoke.c # 11-S7ZM/12-S7 provider AOT dynamic-library success fixture
  - zr_vm_library/src/zr_vm_library/project/project_features.c # 11-S7L/12-S4F feature switch parser
  - zr_vm_library/src/zr_vm_library/project/project_preserve.c # 11-S7E/12-S4B/11-S7K/12-S4E/11-S7M/12-S4G preserve declaration + feature condition + generic argument parser
  - zr_vm_library/src/zr_vm_library/project/project_exports.h # 11-S7Z/12-S7 zrp export declaration parser/free API
  - zr_vm_library/src/zr_vm_library/project/project_exports.c # 11-S7Z/12-S7 zrp export declaration parser; 12-S7ZZZS/11-S7ZSZ duplicate export kind+target parser guard
  - zr_vm_library/src/zr_vm_library/project/project_aot_options.c # 11-S7F aotMode declaration parser
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.h # 11-S7G/12-S8F project aotMode -> AOT writer option bridge API；12-S7X release/full-AOT symbol stripping policy API
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c # 11-S7G/12-S8F project aotMode -> requireFullAot injection helper；12-S7X full-AOT -> stripGeneratedSymbols injection；12-S7ZZY/11-S7ZSH optional AOT C cleanup removes derived compacted .zrp sidecar
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.h # 11-S7H/12-S4C/12-S4D/12-S4H..12-S4N/12-S8G/11-S7ZA AOT C emission + preserve/export writer root bridge API
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.c # 11-S7H/12-S4C/12-S4D/12-S4F/12-S4H..12-S4N/12-S8G/11-S7ZA embedded blob + method/type/generic preserve root injection + feature-conditioned root gating + generic TypeSpec synthesis/open-base generic-instantiation/MethodSpec binding + export declaration bridge hook；12-S7ZZY/11-S7ZSH publishable .zrp sidecar path gate; 12-S7ZZZ/11-S7ZSI stale sidecar removal/failure guard when current embedded blob is not publishable metadata
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.h # 11-S7ZA/12-S7 export declaration -> writer option bridge API
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.c # 11-S7ZA..11-S7ZD/12-S7 project export declaration -> AOT writer manifest declaration mapping and current-module type/method/field token binding; 12-S7ZZZU/11-S7ZTB duplicate export declaration bridge guard
  - zr_vm_cli/src/zr_vm_cli/command/command.h # 11-S7W/11-S7X/11-S7Y/12-S7ZQ/12-S7ZR/12-S7ZS CLI zrp metadata dump/diff/version-check mode carriers
  - zr_vm_cli/src/zr_vm_cli/command/command.c # 11-S7W/12-S7ZQ `--dump-zrp-metadata <file>`, 11-S7X/12-S7ZR `--diff-zrp-metadata <before> <after>`, and 11-S7Y/12-S7ZS `--check-zrp-metadata-version <file>` parse/exclusivity/help surface
  - zr_vm_cli/src/zr_vm_cli/app/app.c # 11-S7W/11-S7X/11-S7Y/12-S7ZQ/12-S7ZR/12-S7ZS CLI app dispatch to zrp metadata dump/diff/version-check runners
  - zr_vm_cli/src/zr_vm_cli/project/project.h # 12-S7ZZY/11-S7ZSH project AOT C path -> compacted .zrp sidecar path API
  - zr_vm_cli/src/zr_vm_cli/project/project.c # 12-S7ZZY/11-S7ZSH project AOT C path -> compacted .zrp sidecar path implementation
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.h # 11-S7W/11-S7X/11-S7Y/12-S7ZQ/12-S7ZR/12-S7ZS zrp metadata section summary, diff summary, and version-check API
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.c # 11-S7W/12-S7ZQ read-only zrp metadata section summary; 11-S7X/12-S7ZR before/after section byte/count diff summary; 11-S7Y/12-S7ZS header magic/version/header-size/section-count compatibility check implementation
  - zr_vm_language_server_extension/schemas/zrp.schema.json # 11-S7E/12-S4B preserve schema parity；11-S7F aotMode schema parity；11-S7K/12-S4E feature condition schema parity；11-S7L/12-S4F feature switch schema parity；11-S7M/12-S4G generic argument schema parity；11-S7Z/12-S7 export declaration schema parity；12-S7ZZZT/11-S7ZTA exports uniqueItems parity
  - zr_vm_parser/src/zr_vm_parser/compiler/   # 编译流程入口
  - tests/library/test_project_manifest_normalization.c # 11-S7E/12-S4B zrp preserve declaration parser gates；11-S7F aotMode gates；11-S7K/12-S4E preserve feature condition gates；11-S7L/12-S4F feature switch gates；11-S7M/12-S4G generic preserve argument gates；11-S7Z/12-S7 export declaration gates；12-S7ZZZS/11-S7ZSZ duplicate export target rejection
  - tests/module/test_zrp_metadata_format.c # 12-S7ZZS manifestExports section format validation; 12-S7ZZW/11-S7ZSF unbound manifest export row shape validation
  - tests/parser/test_aot_reachability.c      # 12-S1A/12-S2A/12-S2B/12-S2C/12-S2E/12-S4A focused reachability contracts；12-S5D dynamic dependency annotation-root collection contract；12-S5E/10-S5F dynamicDependencyMethodToken collection contract；12-S5F/10-S5G dynamicDependencyMethodName collection contract；12-S5G/10-S5H dynamicDependencyMethodName signature-hash disambiguation and ambiguous-name rejection contract；12-S5H/10-S5I non-exported dynamicDependencyMethodToken function-symbol contract
  - tests/parser/test_aot_c_code_stripping.c  # 12-S2D opt-in generated C filtering contract；12-S2E export root preservation；12-S4A manifest preservation；12-S5I/10-S5J dynamicDependencyTypeLayoutId type-layout retention；12-S5J/10-S5K dynamicDependencyTypeToken TypeDef/TypeSpec generated-C type-layout/token retention；12-S5L/10-S5M bound TypeRef token generated-C type-layout/token retention；12-S5K/10-S5L dynamicDependencyFieldToken owner/field type-layout retention；12-S7A/S7B/S7C 函数统计；12-S7H type-layout trim before/after stats；12-S7K zrp metadata section/table/pool byte statistics；12-S7L type-layout payload byte trim delta；12-S7R generated type-layout byte trim delta；12-S7S zrp metadata byte trim delta；12-S7V generated method metadata byte trim delta；12-S7Y stripped output reflection metadata level policy；12-S7Z zrp MethodDef metadata pruning；12-S7ZG signature blob pool after-trim delta；12-S7ZH string pool after-trim delta；12-S7ZI constant pool after-trim delta
  - tests/parser/test_aot_c_reflection_annotation_preserve.c # 12-S5A reflectable metadata annotation roots; 12-S5B/12-S7 requiresUnreferencedCode static-call trim annotation warning coverage; 12-S5C reason text escaping coverage; 12-S5D dynamicDependencyFunctionIndex function root coverage; 12-S5E/10-S5F dynamicDependencyMethodToken generated-C retention coverage; 12-S5F/10-S5G dynamicDependencyMethodName generated-C retention coverage; 12-S5G/10-S5H dynamicDependencyMethodName + signatureHash generated-C retention coverage; 12-S5H/10-S5I non-exported dynamicDependencyMethodToken generated-C retention coverage; 12-S7ZU suppressAnnotationWarnings keeps annotation warnings counted as suppressed without emitting per-warning entries
  - tests/parser/test_aot_c_zrp_metadata_pruning.c # 12-S7ZA direct zrp MethodDef/token-record pruning; 12-S7ZB FieldDef shared MEMBER_DEF remap; 12-S7ZC GenericParam owner/range remap; 12-S7ZE GenericParamConstraint cascade remap; 12-S7ZF MethodSpec method-token cascade remap; 12-S7ZG MethodSpec signature blob rewrite/compaction; 12-S7ZZL GenericParamConstraint TypeSpec constraint-token compaction; 12-S7ZZM GenericParamConstraint-only TypeSpec retention root fixture; 12-S7ZZO retained signature MemberRef token rewrite fixture; 12-S7ZZT/11-S7ZSC existing manifest export rows remap target strings/member tokens after pruning; 12-S7ZZW/11-S7ZSF unbound manifest export row/declaration persistence
  - tests/parser/test_aot_c_zrp_metadata_publication.c # 12-S7ZZX/11-S7ZSG writer-level compacted .zrp metadata sidecar publication; 12-S7ZZZQ/11-S7ZSX invalid definition-table rejection/stale sidecar cleanup
  - tests/parser/test_aot_c_zrp_metadata_export_token_remap.c # 12-S7ZN direct exported MethodDef/FieldDef member token remap coverage; 11-S7ZSA/12-S7ZZR manifest export kind/token mismatch guard; 12-S7ZZZR/11-S7ZSY manifest export duplicate kind+target guard
  - tests/parser/test_aot_c_zrp_metadata_size_deltas.c # 12-S7ZO direct section-level zrp metadata before/after/removed marker coverage; 12-S7ZP direct section count marker coverage
  - tests/parser/test_aot_c_zrp_metadata_pool_pruning.c # 12-S7ZH direct zrp string-pool compaction/remap; 12-S7ZI direct zrp orphan constant-pool sweep; 12-S7ZL retained duplicate string-slice compaction; 12-S7ZM no-MethodDef-prune pool compaction trigger; 12-S7ZZN retained signature AssemblyRef token rewrite fixture; 12-S7ZZP signature-rooted ModuleRef retention fixture
  - tests/parser/test_aot_c_zrp_metadata_pool_pruning.c # 12-S7ZV direct FieldDef default-value constant-pool retained-slice remap coverage
  - tests/parser/test_aot_c_source_contracts.c # 12-S7T zrp metadata size accounting module boundary；12-S7U public symbol-stripping option/emitter plumbing；12-S7V method metadata byte-delta source contract；12-S7Y metadata policy source contract；12-S7Z..12-S7ZI emitted zrp pruning/remap/signature/string-pool module contracts；12-S7ZV constant-pool remap module contract；12-S7ZN export-token remap helper source contract；12-S7ZO section-level delta marker source contract；12-S7ZP section-count marker source contract；12-S7ZU annotation warning suppression writer/emitter source contract；12-S7ZZM GenericParamConstraint-rooted TypeSpec source contract；12-S7ZZN signature AssemblyRef rewrite source contract；12-S7ZZO signature MemberRef rewrite source contract；12-S7ZZP signature-rooted ModuleRef retention source contract；12-S5E/10-S5F, 12-S5F/10-S5G, 12-S5G/10-S5H, 12-S5I/10-S5J, 12-S5J/10-S5K, 12-S5L/10-S5M, and 12-S5K/10-S5L dynamic dependency method/type-layout source contract
  - tests/module/test_metadata_runtime_manifest_exports.c # 12-S7 support/11-S7ZF mirror + 11-S7ZG runtime view + 11-S7ZH binding gate coverage
  - tests/module/test_metadata_runtime_typespec_layout.c # 12-S5M/10-S5N runtime TypeRef token -> TypeDef layout resolver/cache/identity mismatch coverage
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z19/11-S4BE retained inline aggregate field-copy borrowed-source write coverage
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z18/11-S4BD retained inline aggregate borrowed-source write coverage
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z17/11-S4BC retained inline struct borrowed-view coverage
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z16/11-S4BB retained FieldInfo object-level primitive POD read/write coverage
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z15/11-S4BA retained FieldInfo object-level write adapter coverage
  - tests/module/test_reflection_token_resolve.c # 12-S5 support/10-S4Z14/11-S4AZ retained FieldInfo object-level read adapter coverage
  - tests/module/test_reflection_token_resolve.c # 12-S5 support for retained FieldDef/layout metadata consumers, including 10-S4Z13/11-S4AY FieldInfo metadataRuntime native-pointer carrier coverage, 10-S4Z4/11-S4AP FieldInfo VALUE_SLOT inline read, 10-S4Z5/11-S4AQ FieldInfo VALUE_SLOT inline write, 10-S4Z6/11-S4AR FieldInfo primitive POD int32 raw inline read/write, 10-S4Z7/11-S4AS bool/uint32/double representative primitive POD raw inline read/write matrix, 10-S4Z8/11-S4AT int8/int16/int64/uint8/uint16/uint64/float32 storage-width primitive POD raw inline read/write matrix, 10-S4Z9/11-S4AU primitive POD integer range guard coverage, 10-S4Z10/11-S4AV primitive POD float32 range guard coverage, 10-S4Z11/11-S4AW primitive POD float32 NaN guard coverage, and 10-S4Z12/11-S4AX primitive POD float32 precision guard coverage
  - tests/cli/test_cli_args.c # 11-S7W/11-S7X/11-S7Y/12-S7ZQ/12-S7ZR/12-S7ZS CLI dump/diff/version-check mode parse/exclusivity coverage
  - tests/cli/test_cli_zrp_metadata_dump.c # 11-S7W/11-S7X/11-S7Y/12-S7ZQ/12-S7ZR/12-S7ZS zrp metadata dump/diff/version-check summary/path coverage
  - tests/CMakeLists.txt # 11-S7W/11-S7X/11-S7Y/12-S7ZQ/12-S7ZR/12-S7ZS CLI zrp metadata dump/diff/version-check target; 12-S7ZA/12-S7ZH/12-S7ZN/12-S7ZO/12-S7ZP/12-S7ZV Windows shared-DLL direct zrp pruning/remap/size tests link private pruning/remap/section/signature/string-pool/constant-pool/size modules; 12-S7ZZX/11-S7ZSG sidecar publication test target/CTest; 12-S7ZZZQ/11-S7ZSX publication failure-path regression in same target; 12-S7ZZZR/11-S7ZSY manifest export duplicate kind+target guard coverage in export-token remap target; 12-S7ZZY/11-S7ZSH CLI/project sidecar path bridge target/CTest + 12-S7ZZZ/11-S7ZSI stale sidecar cleanup/failure coverage; 11-S7ZM/12-S7 provider AOT shared-library smoke target; 11-S7ZN/12-S7 provider version-selection target
  - tests/cli/test_cli_aot_compacted_metadata_sidecar.c # 12-S7ZZY/11-S7ZSH CLI/project automatic compacted .zrp sidecar path derivation, publishable metadata gate, invalid definition-table guard; 12-S7ZZZ/11-S7ZSI non-publishable metadata stale sidecar cleanup and removal-failure fail-closed guard
  - tests/parser/test_aot_c_value_type_shared_library_smoke.c # 12-S7D/S7E generated type-layout byte statistics
  - tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c # 12-S7I/S7J runtime fallback trim warning reason/sourceLine；12-S7M warning suppression；12-S7N sourceLineEnd；12-S7O reason-mask suppression；12-S7P sourceColumn/sourceColumnEnd；12-S7Q sourceFile attribution；12-S7ZJ warning reasonFlag marker；12-S7ZK quoted/escaped sourceFile marker；12-S7ZT visible/suppressed reason-mask aggregate markers；12-S8A/S8B full-AOT dynamic call/value-access deopt bridge 拒绝
  - tests/parser/test_aot_c_iterator_shared_library_smoke.c # 12-S8C full-AOT dynamic iterator deopt 拒绝
  - tests/parser/test_aot_c_global_shared_library_smoke.c # 12-S8D full-AOT TYPEOF reflection runtime contract 拒绝；12-S7 reflection runtime fallback warning marker coverage for TYPEOF
  - tests/parser/test_aot_c_generic_call_typed.c # 12-S7F embedded module byte statistic；12-S7G generated method metadata byte statistics；12-S7U release generated-symbol stripping fixture；12-S8E full-AOT generic METHOD slot runtime branch closure
  - tests/parser/test_aot_llvm_symbol_stripping.c # 12-S7W default/stripped LLVM generated symbol contract
  - tests/cli/test_cli_project_incremental.c # 11-S7G/11-S7H/12-S8F/12-S8G manifest full-AOT writer option bridge + CLI AOT C emission；12-S7X release/full-AOT symbol stripping CLI policy；12-S8E/11-S6H CLI full-AOT metadata-drift guard assertion alignment
  - tests/cli/test_cli_aot_writer_options.c # 12-S3A..12-S3F/12-S4C/12-S4D/12-S4F/12-S4H..12-S4N/12-S8H..12-S8I/11-S7ZA parsed method/type/generic/feature-conditioned preserve -> writer manifest root binding + generic TypeSpec synthesis/open-base generic-instantiation/MethodSpec binding/full-AOT gate + export declaration writer-option bridge; 12-S7ZZZU/11-S7ZTB duplicate export declaration bridge rejection
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
| 12-S5 | 反射数据流分析 + 注解标记（§4，衔接 10-S5） | 🚧 2026-07-01 部分完成：12-S5A..12-S5K/10-S5A..10-S5L 已提供 reflectable/function/method/name/signature/type/field dynamic-dependency roots 与 requires-unreferenced-code warning carrier；12-S5 support/10-S4F..10-S4Z3/11-S4T..11-S4AO 已让 annotation-root 保留后的 FieldDef metadata 可被运行期最小 public `FieldInfo` object 消费，并逐步暴露 declaring type、owner、moduleName/module object、metadata flags、signature blob/header/type-node、primitive/direct TypeDef/bound TypeRef signature carrier、layout consistency、signature node object、base type-node object、child type-node object list、primitive child semantic name、direct TypeDef base/child semantic token/layout/name、direct TypeRef child semantic token/layout/name 和 recursive signature node type literal object；12-S5 support/10-S4Z4..10-S4Z12/11-S4AP..11-S4AX 已让保留后的 FieldDef/layout/signature metadata 支撑 same-runtime `VALUE_SLOT` inline read/write、primitive POD int32 raw inline read/write boundary、bool/uint32/double representative primitive POD raw inline read/write matrix、int8/int16/int64/uint8/uint16/uint64/float32 storage-width primitive POD raw inline read/write matrix，以及 integer range guard、float32 range guard、float32 NaN guard 和 float32 precision guard；12-S5 support/10-S4Z13..10-S4Z28/11-S4AY..11-S4BN 已让同一 FieldInfo object 携带 attached `metadataRuntime` native-pointer carrier，并支撑 object-level value read/write、inline aggregate borrowed view/source write、nested owned value replacement/drop、单级 nested VALUE_SLOT child read/write、第一条 multi-level nested VALUE_SLOT path read/write 和第一条 multi-level nested primitive POD raw child path read/write，并补充 nested primitive leaf layout identity guard 和 representative bool/uint32/double path matrix coverage 和 storage-width int8/int16/int64/uint8/uint16/uint64/float32 path matrix coverage；12-S7ZU/10-S5E 已提供 writer-level annotation warning suppression。完整 `@dynamically_accessed` 数据流、`@dynamic_dependency` 的跨模块规则、完整 primitive raw child matrix/signature-derived binding、object-level FieldInfo 完整行为、recursive/signature-derived field type binding、TypeRef/跨模块 provider signature binding、attribute/annotation-driven warning 抑制/提升、未注解反射 warning、类型/成员级 sweep 策略仍待后续 |
| 12-S6 | 元数据/反射级别按可达性收窄（衔接 10/11）（§5） | 默认产物最小；token 通道仍可用 |
| 12-S7 | trim 警告 + 体积统计；符号剥离选项（§6/§5） | 🚧 2026-07-01 部分完成：12-S7A 已在 opt-in AOT C 生成物中发布函数裁剪统计 `functionsBefore/After/Removed`，12-S7B 已为每个已发射函数输出 generated-C function body byte span，12-S7C 已输出 retained function body byte total，12-S7D 已为 generated type layout / GC descriptor block 输出 `aot_size.typeLayoutBytes[typeLayoutId]`，12-S7E 已输出 `aot_size.typeLayoutBytesTotal`，12-S7F 已输出随 AOT C module descriptor 嵌入的 `aot_size.embeddedModuleBytes`，12-S7G 已输出 generated method signature/info metadata 的 per-method 与 total byte span，12-S7H 已输出 opt-in code stripping 前后 distinct inline type-layout reference 计数，12-S7I 已输出 hybrid runtime fallback trim warning marker 并区分 dynamic-call / dynamic-value-access reason，12-S7J 已为 runtime fallback warning marker 输出 ExecIR `sourceLine`，12-S7K 已输出有效 zrp embedded metadata 的 total/token-record/definition-table/pool 字节统计与 12 个 section 明细，12-S7L 已输出 referenced inline type-layout payload bytes 的 before/after/removed 裁剪差值，12-S7M 已提供 writer-level runtime fallback warning suppression 并输出 suppressed count，12-S7N 已为 visible runtime fallback warning 输出 `sourceLineEnd` line-span marker，12-S7O 已提供 runtime fallback warning reason-mask suppression 并保持 visible/suppressed count 分离，12-S7P 已为 visible runtime fallback warning 输出 `sourceColumn/sourceColumnEnd` 并拆出 ExecIR source-location 推导模块，12-S7Q 已为 visible runtime fallback warning 输出 `sourceFile`，12-S7ZK 已把该 `sourceFile` 输出收紧为 quoted/escaped marker，12-S7ZT 已输出 visible/suppressed runtime fallback reason-mask aggregate markers，12-S7R 已输出 referenced inline type-layout 的 generated-C byte span before/after/removed 裁剪差值并校验 after 等于 `aot_size.typeLayoutBytesTotal`，12-S7S 已输出 zrp metadata total/token-record/definition-table/pool byte before/after/removed 裁剪差值载体（当前未重写 metadata，removed 为 0），12-S7T 已把 zrp metadata size/delta accounting 从 `backend_aot_c_emitter.c` 拆入 `backend_aot_c_zrp_metadata_size.{h,c}`，12-S7U 已提供 writer-level `stripGeneratedSymbols` option、`symbol_stripping.generatedSymbols` marker，并在启用时把泛型单态化/shared 私有 helper symbol 与 shared `debugName` 从类型名剥离为稳定 ID，12-S7V 已输出 generated method metadata 的 generated-C byte span before/after/removed 裁剪差值并校验 after 等于 `aot_size.methodMetadataBytesTotal`，12-S7W 已将 writer-level `stripGeneratedSymbols` 扩展到 LLVM 后端，默认保持 `@zr_aot_fn_<flatIndex>`，启用后 generated private function definition、thunk table、entry thunk 和静态 direct-call reference 改为 `@zr_fn_g<flatIndex>`，同时保留公开 `@ZrVm_GetAotCompiledModule`，12-S7X 已把 CLI/project `aotMode: "full-aot"` 默认映射到 `stripGeneratedSymbols`，让 full-AOT `--emit-aot-c` 产物默认启用生成符号剥离，hybrid/default 继续保留可读符号，12-S7Y 已在 opt-in code stripping 下把 generated MethodInfo `reflectionMetadataLevel` 降为 `ZR_AOT_REFLECTION_METADATA_NONE`，默认/非裁剪产物仍为 `RUNTIME_MAPPING`，并输出 `metadata_policy.reflectionLevel` marker；12-S5B/12-S7 已输出 `trim_warnings.annotationCount` 与 retained static caller 调用 `requiresUnreferencedCode: true` callee 的 `trim_warning.annotation[] reason=requires-unreferenced-code` marker，12-S5C/12-S7 已在该 marker 上支持 quoted/escaped `message="..."`，12-S7ZU 已提供 writer-level `suppressAnnotationWarnings`，输出 `trim_warnings.annotationSuppressedCount` 并隐藏逐条 annotation marker；12-S7Z..12-S7ZN 已逐步启用 emitted zrp metadata 的 MethodDef、token-record、FieldDef、GenericParam、GenericParamConstraint、MethodSpec method-token 剪枝级联，retained signature blob pool compaction/offset remap/hash recomputation/MethodSpec signature rewrite，string-pool retained-slice sweep、row offset remap 与 duplicate retained slice interning，当前 orphan constant-pool sweep，让 pool compaction 在 MethodDef count 不变时仍可触发，并提供 exported MethodDef/FieldDef member token 的 compacted-token remap surface；12-S7ZZN 已让 retained signature blob 内 `ASSEMBLY_REF` token 随 ModuleRef compaction 重写；12-S7ZZO 已让 retained signature blob 内 `MEMBER_REF` local member token 随 MethodDef/FieldDef compaction 重写；12-S7ZZP 已让 retained signature blob 内 `ASSEMBLY_REF` payload 作为唯一 root 时保留 ModuleRef row/name/version strings 并发布 compacted AssemblyRef RID；11-S7Z/11-S7ZA 已提供 `.zrp` `exports` 输入到 writer options/generated-C manifest diagnostics 的前置桥，11-S7ZB/11-S7ZC/11-S7ZD 已提供 current-module type/method/field export declaration type/member-token writer-input binding；12-S7ZR/11-S7X 已提供 standalone `--diff-zrp-metadata <before> <after>` zrp metadata section byte/count diff summary；完整 trim analyzer、attribute/annotation 抑制策略、持久 cross-module export manifest/table writer、compacted-token file publication、cross-module provider loading/version binding、完整 metadata sweep/pruning 和版本/ABI 漂移闭环仍待后续 |
| 12-S8 | full-AOT 闭合校验（§7） | 🚧 2026-06-25 部分完成：12-S8A 已在 `requireFullAot` 下预检 SemIR/显式 dynamic call 需要 deopt bridge 且无法静态解析 callee 的路径，12-S8B 已拒绝 SemIR dynamic member/index value-access deopt bridge，12-S8C 已拒绝 SemIR/显式 dynamic iterator deopt runtime boundary，12-S8D 已拒绝 `TYPEOF` reflection runtime contract，12-S8E 已让已静态收集 shared generic `CALL_TYPED` 在 full-AOT 下直接调用 AOT method entry 且不保留 METHOD slot null runtime branch；11-S7F 已提供 `.zrp` `aotMode: "full-aot"` project model；11-S7G/12-S8F 已提供 manifest 到 `requireFullAot` 的 CLI/compiler option 注入 helper；11-S7H/12-S8G 已提供 CLI AOT C 发射入口接线并让 full-AOT manifest policy 触发 writer 预检；12-S8H/11-S7P/08-S7E 已让 full-AOT writer 拒绝未绑定 TypeSpec 的 manifest generic preserve root；12-S8I/11-S7R/08-S7G/12-S3B 已让 full-AOT writer 继续拒绝 TypeSpec-only generic root，要求 generic instantiation identity；manifest 动态泛型实例、注解驱动反射保留和完整诊断仍待后续 |

> 注：12-S7ZU/10-S5E 已关闭 writer-level `suppressAnnotationWarnings` 全局抑制；上表中仍待的
> attribute/annotation 抑制策略指 per-warning/属性驱动策略和 promotion 规则。
>
> 2026-07-01 14:51:47 +08:00 状态补记：12-S7ZV 已关闭上表 12-S7 当前摘要中的
> FieldDef/default-value backed constant-pool remap 缺口；仍未关闭的是完整 trim analyzer、attribute/annotation
> 抑制策略、cross-module export-token publication/rewrite、完整 metadata sweep/pruning 和版本/ABI 漂移闭环。
>
> 2026-07-01 21:14:56 +08:00 状态补记：12-S7ZZK 已关闭 retained MethodSpec/local `SIGNATURE` orphan token 拒绝与
> prune failure output cleanup；仍未关闭的是完整 trim analyzer、attribute/annotation 抑制策略、cross-module
> export manifest/table rewrite/publication、完整 metadata sweep/pruning 和版本/ABI 漂移闭环。

> 2026-07-01 21:41:02 +08:00 状态补记：12-S7ZZL 已关闭 retained GenericParamConstraint 的 `TYPE_SPEC`
> constraint token 随 TypeSpec table pruning/compaction 重写；仍未关闭的是完整 trim analyzer、attribute/annotation
> 抑制策略、cross-module export manifest/table rewrite/publication、完整 metadata sweep/pruning 和版本/ABI 漂移闭环。

> 2026-07-01 22:11:32 +08:00 状态补记：12-S7ZZM 已关闭 retained GenericParamConstraint 作为唯一 live root 时的
> TypeSpec row/signature blob 保留与 compacted RID 发布；仍未关闭的是完整 trim analyzer、attribute/annotation
> 抑制策略、cross-module export manifest/table rewrite/publication、完整 metadata sweep/pruning 和版本/ABI 漂移闭环。

> 2026-07-01 22:34:08 +08:00 状态补记：12-S7ZZN 已关闭 retained signature blob 内 `ASSEMBLY_REF` token
> 随 ModuleRef pruning/compaction 重写到 compacted RID；仍未关闭的是完整 trim analyzer、attribute/annotation
> 抑制策略、cross-module export manifest/table rewrite/publication、完整 metadata sweep/pruning 和版本/ABI 漂移闭环。

> 2026-07-01 22:51:15 +08:00 状态补记：12-S7ZZO 已关闭 retained signature blob 内 `MEMBER_REF`
> local member token 随 MethodDef/FieldDef pruning/compaction 重写到 compacted RID；仍未关闭的是完整 trim analyzer、
> attribute/annotation 抑制策略、cross-module export manifest/table rewrite/publication、完整 metadata sweep/pruning
> 和版本/ABI 漂移闭环。

> 2026-07-01 23:36:12 +08:00 状态补记：12-S7ZZP 已关闭 retained signature blob 作为唯一 live root 时的
> ModuleRef row/name/version string 保留与 retained `ASSEMBLY_REF` token compacted RID 发布；仍未关闭的是完整
> trim analyzer、attribute/annotation 抑制策略、cross-module export manifest/table rewrite/publication、完整
> metadata sweep/pruning 和版本/ABI 漂移闭环。

> 2026-07-02 03:24:57 +08:00 状态补记：11-S7ZE/12-S7 已关闭 generated-C
> `SZrAotManifestExportEntry` 表发布、descriptor/codeRegistration wiring、runtime table validation，以及 method/field
> export bound `MEMBER_DEF` token 的 compacted remap 写入；仍未关闭的是完整 trim analyzer、
> attribute/annotation 抑制策略、cross-module provider loading/version binding、standalone provider manifest
> consumption、完整 metadata sweep/pruning 和版本/ABI 漂移闭环。

> 2026-07-02 03:57:34 +08:00 状态补记：11-S7ZF/12-S7 已关闭 attached metadata runtime
> 对 generated-C manifest export table 的 pointer/count mirror；仍未关闭的是完整 trim analyzer、
> attribute/annotation 抑制策略、cross-module provider loading/version binding、standalone provider import-path manifest
> consumption、完整 metadata sweep/pruning 和版本/ABI 漂移闭环。

> 2026-07-02 04:23:15 +08:00 状态补记：11-S7ZG/12-S7 已关闭 attached manifest export table 的 runtime
> view/query API：`ZrCore_MetadataRuntime_ReadManifestExportView()` 可按 `kind + target` 唯一读取导出 entry 和
> type/member token，并对重复声明、缺 required token 或 token shape mismatch fail closed；仍未关闭的是完整
> trim analyzer、attribute/annotation 抑制策略、cross-module provider loading/version binding、standalone provider
> import-path wiring、完整 metadata sweep/pruning 和版本/ABI 漂移闭环。

> 2026-07-02 04:46:47 +08:00 状态补记：11-S7ZH/12-S7 已关闭 attached manifest export view 的
> binding compatibility gate：runtime 可先按 `kind + target` 读取导出 token，再用现有 version/signature/module/layout
> predicate 和新增 export-token mismatch status 做 fail-closed 检查；仍未关闭的是完整 trim analyzer、
> attribute/annotation 抑制策略、cross-module provider loading/version binding、standalone provider import-path wiring、
> 完整 metadata sweep/pruning 和版本/ABI 漂移闭环。

> 2026-07-02 05:58:12 +08:00 状态补记：11-S7ZJ/12-S7 已关闭 standalone provider
> import-path location discovery：project resolver 可统一返回 declared assembly/version range、canonical provider
> module key，以及 `.zrm` archive entry 或 `.zrp` provider paths；仍未关闭的是完整 trim analyzer、
> attribute/annotation 抑制策略、provider runtime loading、multi-version selection、完整 metadata sweep/pruning
> 和版本/ABI 漂移闭环。

> 2026-07-02 06:13:21 +08:00 状态补记：11-S7ZK/12-S7 已关闭 standalone provider
> AOT load-request planning：project resolver 现在可把 provider location 转成后续 runtime/trim binding 可消费的 backend、
> canonical module key、descriptor-local module name、`.zrp` AOT library path 或 `.zrm` archive/entry view；仍未关闭的是
> 完整 trim analyzer、attribute/annotation 抑制策略、provider runtime dynamic loading、multi-version selection、完整 metadata
> sweep/pruning 和版本/ABI 漂移闭环。

> 2026-07-02 06:41:32 +08:00 状态补记：11-S7ZL/12-S7 已关闭 provider load request 在 strict AOT runtime
> 中的消费入口：canonical provider import 现在使用 provider `.zrp` 的 AOT library path 和 descriptor-local module name；
> `.zrm` archive entry 保持 fail-closed，不进入 filesystem dynamic-loader 路径。仍未关闭的是完整 trim analyzer、
> attribute/annotation 抑制策略、provider 动态库成功加载端到端、multi-version selection、完整 metadata sweep/pruning
> 和版本/ABI 漂移闭环。

> 2026-07-02 07:01:13 +08:00 状态补记：11-S7ZM/12-S7 已补齐 provider `.zrp` AOT dynamic-library success fixture：
> 新 smoke 会生成 provider module 的 `.zro` 与 AOT C，再编译成 runtime load request 指向的
> `/deps/math/bin/aot_c/lib/zrvm_aot_ops_sum.so`，并验证 strict AOT C canonical import 成功执行、缓存 canonical key、
> 发布 provider export。仍未关闭的是完整 trim analyzer、attribute/annotation 抑制策略、multi-version selection、
> 完整 metadata sweep/pruning 和版本/ABI 漂移闭环。

> 2026-07-02 07:25:55 +08:00 状态补记：11-S7ZN/12-S7 已关闭 provider exact alias/version
> selection 与 declared strict semver range guard：多版本 provider reference 可在 AOT load request 前按 alias/version
> 精确落到对应 `.zrp` source/binary/library path；strict `major.minor.patch` actual version 不满足 declared range 时
> manifest parse fail closed。仍未关闭的是完整 trim analyzer、attribute/annotation 抑制策略、automatic range-based
> candidate selection、完整 metadata sweep/pruning 和版本/ABI 漂移闭环。

> 2026-07-02 09:15:26 +08:00 状态补记：11-S7ZR/12-S7 已关闭 range-selected provider
> runtime export publication 支撑切片：candidate-selected provider alias 现在和 exact alias 一样，可由 strict AOT C
> runtime 加载、执行并发布普通 public export 与 attached manifest export metadata。仍未关闭的是完整 trim analyzer、
> attribute/annotation 抑制策略、完整 metadata sweep/pruning、compacted-token file publication 和版本/ABI 漂移闭环。

> 2026-07-02 09:38:04 +08:00 状态补记：11-S7ZSA/12-S7ZZR 已关闭 manifest export table builder 的
> kind/token shape guard：type export 拒绝 `MEMBER_DEF` binding，method/field export 拒绝 `TYPE_DEF`
> binding；未绑定 declaration 与 provider method/field member-token remap 路径保持通过。仍未关闭的是完整
> trim analyzer、attribute/annotation 抑制策略、持久 `.zrp` manifest export section、完整 metadata
> sweep/pruning 和版本/ABI 漂移闭环。

## 9. 不变量校验

- **C 单一真相**：裁剪决策基于唯一 token/layout 图；可达性结果是 `08`/`10`/`11` 的共同输入，不各自重算。
- **D 环境隔离**：裁剪不改变 typed 函数体形态（`07`），只决定「生成哪些」，不影响「怎么生成」。
- hybrid 安全：默认保留 deopt 兜底，裁剪激进但不致运行期不可恢复的缺失（除显式 full-AOT）。

## 状态与产出记录

> 落地每个阶段或切片时在此追加：时间戳 · 切片号 · 状态 · 完成项目 · RED/GREEN · 测试结果 · 备注。

- 2026-07-03 23:36:13 +08:00 · 12-S7ZZZU / 11-S7ZTB CLI export duplicate bridge guard ·
  状态：12-S7 writer-option 注入前的 export declaration 唯一性 guard 完成；07~12 总目标继续进行中，完整 trim analyzer、
  attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：CLI AOT writer bridge 现在拒绝内存态重复 `kind + target` export declarations，并在失败时清空 writer
  manifest export option 与 preserve-root scratch storage，防止重复 export key 进入 code stripping/export retention 输入。
  RED/GREEN：RED 为新增 CLI writer-options 负例失败 `Expected FALSE Was TRUE`；GREEN 后 focused direct run 19/0。
  测试结果：WSL GCC、WSL Clang、Windows MSVC Debug 均通过 CLI writer-options 19/0、project manifest normalization 29/0、
  manifest export/runtime view CTest 2/2、source contracts 24/0；`git diff --check` 通过（仅 LF/CRLF 提示），代码文件尾随空白扫描为空。
  产出：`tests/acceptance/2026-07-03-aot-11-s7ztb-12-s7zzzu-cli-export-duplicate-bridge-guard.md`。
  备注：本切片只关闭 bridge 层重复 export declaration fail-closed；完整 trim analyzer、完整 metadata sweep/pruning 或
  12-S7 总体仍未完成。
- 2026-07-03 23:20:20 +08:00 · 12-S7ZZZT / 11-S7ZTA project export schema uniqueItems parity ·
  状态：12-S7 `.zrp` export schema 的基础重复对象约束完成；07~12 总目标继续进行中，完整 trim analyzer、
  attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：`exports` schema array 增加 `uniqueItems: true`，使完全重复 export declaration object 在 schema
  层 fail closed，补齐 parser/generated-table duplicate guard 的基础 schema parity。
  RED/GREEN：RED 为 schema 断言缺少 `uniqueItems` 失败 `AssertionError`；GREEN 后断言通过。
  测试结果：`python -c` schema uniqueItems 断言通过；`python -m json.tool zrp.schema.json` 通过。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zta-12-s7zzzt-project-export-schema-unique-items.md`。
  备注：本切片只关闭 schema 层完全重复 export object parity；完整 trim analyzer、完整 metadata sweep/pruning 或
  12-S7 总体仍未完成。
- 2026-07-03 23:14:39 +08:00 · 12-S7ZZZS / 11-S7ZSZ project manifest export duplicate target guard ·
  状态：12-S7 project manifest export parser 的重复目标 fail-closed guard 完成；07~12 总目标继续进行中，
  完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：项目 `exports` 数组现在拒绝同一 `kind + target` 的重复声明；歧义 export key 不再进入
  AOT writer options，也不会推迟到 generated table 或 runtime view 才失败。
  RED/GREEN：RED 为新增重复 `method`/`Widget.run` manifest fixture 后，WSL GCC
  project manifest normalization 失败 `Expected NULL`；GREEN 后 focused project fixture 29/0 通过。
  测试结果：WSL GCC focused project direct 29/0；WSL GCC 下游 CTest
  `aot_c_zrp_metadata_export_token_remap|metadata_runtime_manifest_exports` 2/2 与 source contracts 24/0 通过；
  WSL clang/MSVC Debug 同组 project direct 29/0、下游 CTest 2/2、source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsz-12-s7zzzs-project-export-duplicate-target-guard.md`。
  备注：本切片只关闭 project manifest export duplicate kind+target parser guard 缺口；不声明完整 trim analyzer、
  完整 metadata sweep/pruning 或 12-S7 总体完成。
- 2026-07-03 23:00:31 +08:00 · 12-S7ZZZR / 11-S7ZSY manifest export duplicate kind-target guard ·
  状态：12-S7 generated manifest export table 的重复目标 fail-closed guard 完成；07~12 总目标继续进行中，
  完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：manifest export table builder 现在拒绝重复 `kind + target` 声明；同一目标不再能被生成到
  compacted metadata/codeRegistration manifest export table 中，避免运行时查询时才发现歧义。
  RED/GREEN：RED 为新增 duplicate `METHOD + "Factory.make"` declaration fixture 后，WSL GCC
  export-token remap 测试失败 `Expected FALSE Was TRUE`；GREEN 后 focused fixture 11/0 通过。
  测试结果：WSL GCC/clang/MSVC Debug CTest
  `aot_c_zrp_metadata_export_token_remap|metadata_runtime_manifest_exports|aot_c_zrp_metadata_pruning`
  均 3/3；同三套直跑 source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsy-12-s7zzzr-manifest-export-duplicate-target-guard.md`。
  备注：本切片只关闭 generated manifest export duplicate kind+target guard 缺口；不声明完整 trim analyzer、
  完整 metadata sweep/pruning 或 12-S7 总体完成。
- 2026-07-03 22:45:34 +08:00 · 12-S7ZZZQ / 11-S7ZSX writer sidecar definition-table validation ·
  状态：12-S7 compacted `.zrp` metadata sidecar 发布的文件级 validation/stale cleanup 完成；07~12
  总目标继续进行中，完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：writer-level compacted sidecar helper 不再只接受可读 header；发布前必须通过 definition-table
  validation。无效 definition tables 会让 writer fail closed，并删除同路径旧 sidecar，避免 stale artifact 被保留在
  生成目录。
  RED/GREEN：RED 为新增 direct writer invalid definition-table sidecar fixture 后，WSL GCC
  `aot_c_zrp_metadata_publication` 失败 `Expected FALSE Was TRUE`；GREEN 后 publication focused fixture 2/0 通过。
  测试结果：WSL GCC/clang/MSVC Debug CTest
  `aot_c_zrp_metadata_publication|cli_aot_compacted_metadata_sidecar|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_methodspec_pruning|aot_c_zrp_metadata_pool_pruning`
  均 5/5；同三套直跑 source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsx-12-s7zzzq-zrp-sidecar-definition-table-validation.md`。
  备注：本切片只关闭 writer-level sidecar validation/stale cleanup 缺口；不声明完整 trim analyzer、完整
  metadata sweep/pruning 或 12-S7 总体完成。
- 2026-07-03 22:23:50 +08:00 · 12-S7ZZZP / 11-S7ZSW retained signature UNION string-pool sweep/remap ·
  状态：12-S7 compacted `.zrp` metadata pruning 中 retained signature `UNION` base-name string 保留与重写完成；
  07~12 总目标继续进行中，完整 trim analyzer、attribute/annotation promotion policy、完整 metadata
  sweep/pruning 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：compacted string-pool remap 现在扫描 retained signature `UNION` 节点的 base-name offset，
  signature blob copy 后同步重写该 offset。这样 MethodSpec 泛型实参为 union signature 时，不会留下指向旧字符串池的
  悬空 payload。
  RED/GREEN：RED 为新增 MethodSpec union fixture 后 WSL GCC 失败 `Expected 780 Was 773`；GREEN 后
  WSL GCC/clang 与 Windows MSVC Debug 同组回归通过。
  测试结果：WSL GCC/clang/MSVC Debug CTest
  `aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_methodspec_pruning|aot_c_zrp_metadata_typespec_pruning|aot_c_zrp_metadata_module_ref_pruning|aot_c_zrp_metadata_pool_pruning`
  均 5/5；同三套直跑 TypeDef pruning 2/0、source contracts 24/0；`git diff --check` 仅有 LF/CRLF 提示，尾随空白扫描干净。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsw-12-s7zzzp-signature-union-string-remap.md`。
  备注：本切片只关闭 retained signature `UNION` base-name string retention/remap 缺口；不声明完整 trim analyzer、
  完整 metadata sweep/pruning 或 12-S7 总体完成。
- 2026-07-03 22:04:51 +08:00 · 12-S7ZZZO / 11-S7ZSV ModuleRef retained-row generic context ·
  状态：12-S7 compacted `.zrp` metadata pruning 中 ModuleRef retained-row generic context 完成；
  07~12 总目标继续进行中，完整 trim analyzer、attribute/annotation promotion policy、完整 metadata
  sweep/pruning 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：ModuleRef retention/count/RID compaction/token remap 现在携带 GenericParam/Constraint context。
  当 retained import TypeRef 的 target TypeSpec 只通过 GenericParamConstraint 保活时，compacted `.zrp`
  会保留并压缩对应 AssemblyRef/ModuleRef row，同时重写 token record 的 `relatedToken`。
  RED/GREEN：RED 为新增 ModuleRef pruning fixture 后 WSL GCC 失败 `Expected 1 Was 0`；
  GREEN 后 WSL GCC/clang 与 Windows MSVC Debug 同组回归通过。
  测试结果：WSL GCC/clang/MSVC Debug CTest
  `aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_methodspec_pruning|aot_c_zrp_metadata_typespec_pruning|aot_c_zrp_metadata_module_ref_pruning|aot_c_zrp_metadata_pool_pruning`
  均 5/5；同三套直跑 TypeDef pruning 2/0、source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsv-12-s7zzzo-module-ref-generic-context.md`。
  备注：本切片只关闭 ModuleRef retained-row generic context 缺口；不声明完整 trim analyzer、
  完整 metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-03 21:20:45 +08:00 · 12-S7ZZZN / 11-S7ZSU retained signature MODULE string-pool sweep/remap ·
  状态：12-S7 compacted `.zrp` metadata pruning 中 retained signature `MODULE` name/version string retention/remap 完成；
  07~12 总目标继续进行中，完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整
  ABI drift/deopt coverage 仍待后续。
  完成项目：compacted string-pool 构建现在扫描 retained signature blobs 中的 `MODULE` name/version offsets，
  并在 retained signature blob rewrite 阶段把两个 offsets 重写到 compacted string-pool；这补齐了 module literal
  signature node 不经 ModuleRef row 时的字符串根。
  RED/GREEN：RED 为 MethodSpec pruning 新增 retained `MODULE("__entry","1.0.0")` fixture，WSL GCC 失败
  `Expected 778 Was 764`；GREEN 后 WSL GCC/clang 与 Windows MSVC Debug 同组回归通过。
  测试结果：WSL GCC/clang/MSVC Debug direct MethodSpec pruning 7/0、pool pruning 8/0、metadata pruning 22/0、
  TypeDef pruning 2/0、TypeSpec pruning 2/0、source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsu-12-s7zzzn-signature-module-string-remap.md`。
  备注：本切片只关闭 retained signature `MODULE` string-pool sweep/remap 缺口；不声明完整 trim analyzer、
  完整 metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-03 20:50:52 +08:00 · 12-S7ZZZM / 11-S7ZST retained signature TYPE_REF string-pool sweep/remap ·
  状态：12-S7 compacted `.zrp` metadata pruning 中 retained signature `TYPE_REF` name string retention/remap 完成；
  07~12 总目标继续进行中，完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整
  ABI drift/deopt coverage 仍待后续。
  完成项目：compacted string-pool 构建现在扫描 retained signature blobs 中的 `TYPE_REF` name string offsets，
  保留只被签名 payload 引用的外部类型名；signature blob copy 后会把 `TYPE_REF` payload 中的 string offset 重写到
  compacted string-pool offset。remap 表按需扩容，避免新增签名字符串 root 因容量估算过窄而 fail-closed。
  RED/GREEN：RED 为新增 MethodSpec 泛型实参 `TYPE_REF("ExternalArg")` 后，WSL GCC MethodSpec pruning
  失败 `Expected 776 Was 764`；GREEN 后 MethodSpec pruning 6/0。
  测试结果：WSL GCC/clang/Windows MSVC Debug 均通过 source contracts 24/0、pool pruning 8/0、
  pruning 22/0、TypeDef pruning 2/0、TypeSpec pruning 2/0、MethodSpec pruning 6/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zst-12-s7zzzm-signature-typeref-string-remap.md`。
  备注：本切片只关闭 retained signature `TYPE_REF` string-pool sweep/remap 缺口；不声明完整 trim analyzer、
  完整 metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-03 20:26:48 +08:00 · 12-S7ZZZL / 11-S7ZSS MethodSpec imported MEMBER_REF recursive retained-token-record guard ·
  状态：12-S7 compacted `.zrp` metadata pruning 中 MethodSpec imported `MEMBER_REF` recursive retained-token-record guard 完成；
  07~12 总目标继续进行中，完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整
  ABI drift/deopt coverage 仍待后续。
  完成项目：MethodSpec `methodToken = MEMBER_REF(...)` 与 retained token-record pruning 现在会递归检查 imported member
  record 中的 nested `MEMBER_REF` fields。若第一层 imported member record 仍存在但通过第二层 imported member record
  间接指向 pruned MethodDef/FieldDef，compacted `.zrp` 会裁掉 MethodSpec、`SIGNATURE` token record、signature blob
  和 dangling imported member records；合法自引用 imported member record 继续保留。
  RED/GREEN：RED 为新增 nested imported `MEMBER_REF` chain 指向已裁剪 MethodDef 的 fixture 后，WSL GCC MethodSpec pruning
  失败 `Expected 504 Was 639`；GREEN 后 MethodSpec pruning 5/0。
  测试结果：WSL GCC/clang/Windows MSVC Debug 均通过 source contracts 24/0、export token remap 10/0、
  pruning 22/0、MethodSpec pruning 5/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zss-12-s7zzzl-methodspec-memberref-recursive-retained-token-record-guard.md`。
  备注：本切片只关闭 MethodSpec imported `MEMBER_REF` recursive retained-token-record 完整性 guard；不声明完整 trim analyzer、
  完整 metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-03 19:54:49 +08:00 · 12-S7ZZZK / 11-S7ZSR MethodSpec imported MEMBER_REF retained-token-record guard ·
  状态：12-S7 compacted `.zrp` metadata pruning 中 MethodSpec imported `MEMBER_REF` retained-token-record guard 完成；
  07~12 总目标继续进行中，完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整
  ABI drift/deopt coverage 仍待后续。
  完成项目：MethodSpec `methodToken = MEMBER_REF(...)` 不再只验证源 token record 存在，还要求该 imported member
  record 的本地 member-token 字段指向 retained MethodDef/FieldDef；retained token-record pruning 对嵌套 `MEMBER_REF`
  也执行同样的 retained-record 检查。signature remap、MethodSpec copy 和 MethodSpec signature rewrite 已携带
  TypeDef/GenericParam/constraint context，因此带 pruned target 的 imported member record 会连同 MethodSpec、
  `SIGNATURE` token record 与 signature blob 被裁掉。
  RED/GREEN：RED 为新增 imported `MEMBER_REF` record 指向已裁剪 MethodDef 的 fixture 后，WSL GCC MethodSpec pruning
  失败 `Expected 600 Was 639`；GREEN 后 MethodSpec pruning 3/0。
  测试结果：WSL GCC/clang/Windows MSVC Debug 均通过 source contracts 24/0、export token remap 10/0、
  pruning 22/0、MethodSpec pruning 3/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsr-12-s7zzzk-methodspec-memberref-retained-token-record-guard.md`。
  备注：本切片只关闭 MethodSpec imported `MEMBER_REF` retained-token-record 完整性 guard；不声明完整 trim analyzer、
  完整 metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-03 19:29:47 +08:00 · 12-S7ZZZJ / 11-S7ZSQ MethodSpec imported MEMBER_REF token-record guard ·
  状态：12-S7 compacted `.zrp` metadata pruning 中 MethodSpec imported `MEMBER_REF` token-record guard 完成；
  07~12 总目标继续进行中，完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整
  ABI drift/deopt coverage 仍待后续。
  完成项目：MethodSpec `methodToken = MEMBER_REF(...)` 不再只按 table tag 保留，必须能在源 token record
  表中找到对应 imported member record；retained token-record remap 对 signature token record 的 related/owner/target
  imported member references 执行同样 guard。MethodSpec count/copy、signature remap 和 MethodSpec signature rewrite
  共享该判定，缺失 imported member record 时 compacted `.zrp` 不再发布 orphan MethodSpec、SIGNATURE record 或签名 blob。
  RED：新增缺失 imported `MEMBER_REF` token record fixture 后，WSL GCC
  `zr_vm_aot_c_zrp_metadata_methodspec_pruning_test` 报 `Expected 504 Was 639`。
  GREEN：WSL GCC/clang/Windows MSVC Debug `zr_vm_aot_c_source_contracts_test` 均 24/0，
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 均 10/0，`zr_vm_aot_c_zrp_metadata_pruning_test` 均 22/0，
  `zr_vm_aot_c_zrp_metadata_methodspec_pruning_test` 均 2/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsq-12-s7zzzj-methodspec-memberref-token-record-guard.md`。
  备注：本切片只关闭 MethodSpec imported `MEMBER_REF` token-record 完整性 guard；不声明完整 trim analyzer、完整
  metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-03 19:00:53 +08:00 · 12-S7ZZZI / 11-S7ZSP MethodSpec imported MEMBER_REF method-token retention ·
  状态：12-S7 compacted `.zrp` metadata pruning 中 MethodSpec imported `MEMBER_REF` method-token retention 完成；
  07~12 总目标继续进行中，完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整
  ABI drift/deopt coverage 仍待后续。
  完成项目：MethodSpec `methodToken` 现在只对本地 `MEMBER_DEF` 走 retained MethodDef compaction；导入
  `MEMBER_REF` method token 作为合法 imported method reference 原样保留，避免 MethodDef pruning 后误删跨模块
  generic method instantiation 的 MethodSpec row、signature blob 与 signature hash。
  RED：临时撤掉 `MEMBER_REF` 分支后，WSL GCC
  `zr_vm_aot_c_zrp_metadata_methodspec_pruning_test` 报 `Expected 735 Was 711`。
  GREEN：WSL GCC/clang/Windows MSVC Debug `zr_vm_aot_c_source_contracts_test` 均 24/0，
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 均 10/0，`zr_vm_aot_c_zrp_metadata_pruning_test` 均 22/0，
  `zr_vm_aot_c_zrp_metadata_methodspec_pruning_test` 均 1/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsp-12-s7zzzi-methodspec-memberref-retention.md`。
  备注：本切片只关闭 MethodSpec imported `MEMBER_REF` method-token retention；不声明完整 trim analyzer、完整
  metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-03 08:13:00 +08:00 · 12-S7ZZZH / 11-S7ZSO GenericParam TypeDef owner retained-row guard ·
  状态：12-S7 compacted `.zrp` metadata pruning 中 GenericParam TypeDef-owner retained-row refinement 完成；07~12
  总目标继续进行中，完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：GenericParam `TYPE_DEF` owner 现在必须通过 retained TypeDef remap 才能进入输出，并会写入压缩后的
  TypeDef RID；GenericParam retained-row/count/range 和 GenericParamConstraint remap/count/range 统一携带 TypeDef
  row、token-record、constraint context。TypeDef pruning 不再让 TypeDef-owned GenericParam 反向保活 owner TypeDef。
  RED：新增 pruned TypeDef-owned GenericParam fixture 后，WSL GCC `zr_vm_aot_c_zrp_metadata_pruning_test` 报
  `Expected Non-NULL`。
  GREEN：WSL GCC/clang/Windows MSVC Debug `zr_vm_aot_c_source_contracts_test` 均 24/0，
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 均 10/0，`zr_vm_aot_c_zrp_metadata_pruning_test` 均 22/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zso-12-s7zzzh-genericparam-typedef-owner-retention.md`。
  备注：本切片只关闭 GenericParam TypeDef owner retained-row/remap；不声明完整 trim analyzer、完整
  metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-03 07:42:47 +08:00 · 12-S7ZZZG / 11-S7ZSN TypeDef retained FieldDef owner-token guard ·
  状态：12-S7 compacted `.zrp` metadata pruning 中 TypeDef token-record retained-root refinement 完成；07~12
  总目标继续进行中，完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：TypeDef root 判定现在用 TypeDef-root 专用 retained member-token check；MethodDef `MEMBER_DEF`
  roots 必须 retained，FieldDef `MEMBER_DEF` roots 必须 retained 且不能把自己的 owner TypeDef 作为剪枝根反向保活。
  TypeDef retained predicate 的调用方已补齐 TypeDef row/count context，避免 pruning/count/remap/string/signature roots
  回退到 source FieldDef index。
  RED：新增 dead FieldDef owner-TypeDef token-record fixture 后，WSL GCC
  `zr_vm_aot_c_zrp_metadata_pruning_test` 报 `Expected Non-NULL`。
  GREEN：WSL GCC/clang/Windows MSVC Debug `zr_vm_aot_c_source_contracts_test` 均 24/0，
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 均 10/0，`zr_vm_aot_c_zrp_metadata_pruning_test` 均 21/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsn-12-s7zzzg-typedef-fielddef-owner-token-retention.md`。
  备注：本切片只关闭 TypeDef retained FieldDef owner-token root guard；不声明完整 trim analyzer、完整
  metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-03 07:22:01 +08:00 · 12-S7ZZZF / 11-S7ZSM Method-only member-token guard ·
  状态：12-S7 compacted `.zrp` metadata pruning 中 GenericParam/MethodSpec method-only member-token refinement
  完成；07~12 总目标继续进行中，完整 trim analyzer、attribute/annotation promotion policy、完整 metadata
  sweep/pruning 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：GenericParam `MEMBER_DEF` owner 和 MethodSpec `methodToken` 现在只通过 retained MethodDef remap；
  retained FieldDef tokens 仍会在普通 token-record/FieldDef path 中压缩发布，但放在 method-only slots 时会使对应
  GenericParam/MethodSpec row 被裁剪，MethodSpec signature blob 不再被错误保留。
  RED：新增 FieldDef-as-method-owner fixture 后，WSL GCC `zr_vm_aot_c_zrp_metadata_pruning_test` 报
  `Expected TRUE Was FALSE`。
  GREEN：WSL GCC/clang/Windows MSVC Debug `zr_vm_aot_c_source_contracts_test` 均 24/0，
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 均 10/0，`zr_vm_aot_c_zrp_metadata_pruning_test` 均 20/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsm-12-s7zzzf-method-only-member-token-retention.md`。
  备注：本切片只关闭 GenericParam/MethodSpec MethodDef-only member-token guard；不声明完整 trim analyzer、完整
  metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-03 06:53:16 +08:00 · 12-S7ZZZE / 11-S7ZSL TypeSpec retained FieldDef token-record guard ·
  状态：12-S7 compacted `.zrp` metadata pruning 中 TypeSpec retained-token refinement 完成；07~12 总目标继续进行中，
  完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整 ABI drift/deopt coverage
  仍待后续。
  完成项目：TypeSpec retention 现在使用 retained token-record remap，并把 TypeDef row context 传入 TypeSpec、ModuleRef、
  signature、string-pool 和 prune orchestration；只被 pruned FieldDef token-record 建根的 TypeSpec rows/signature blobs 会被删除，
  被 retained FieldDef token-record 建根的 TypeSpec rows 会保留并压缩到 retained `TYPE_SPEC` RID。
  RED：新增 TypeSpec FieldDef-token-record fixture 后，WSL GCC `zr_vm_aot_c_zrp_metadata_pruning_test` 报
  `Expected 765 Was 794`。
  GREEN：WSL GCC/clang/Windows MSVC Debug `zr_vm_aot_c_source_contracts_test` 均 24/0，
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 均 10/0，`zr_vm_aot_c_zrp_metadata_pruning_test` 均 19/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsl-12-s7zzze-typespec-fielddef-retention.md`。
  备注：本切片只关闭 TypeSpec retained FieldDef token-record guard；不声明完整 trim analyzer、完整 metadata sweep/pruning 或
  12-S7 总体完成。

- 2026-07-03 06:24:32 +08:00 · 12-S7ZZZD / 11-S7ZSK FieldDef member-token retained-row guard ·
  状态：12-S7 emitted metadata prune pipeline 的 FieldDef `MEMBER_DEF` token retained-row guard 子切片完成；07~12
  总目标继续进行中，完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整 ABI
  drift/deopt coverage 仍待后续。
  完成项目：token-record pruning 和 manifest export row remap 现在对 FieldDef member tokens 使用 retained FieldDef
  判定与 retained-order compaction；pruned FieldDef 相关 token records 会被删除，后续 live FieldDef 不再保留 source RID。
  RED：新增 FieldDef-before-live fixture 后，WSL GCC `zr_vm_aot_c_zrp_metadata_pruning_test` 报
  `Expected 640 Was 736`。
  GREEN：WSL GCC/clang/Windows MSVC Debug `zr_vm_aot_c_zrp_metadata_pruning_test` 均 18/0，
  `zr_vm_aot_c_source_contracts_test` 均 24/0，`zr_vm_aot_c_zrp_metadata_export_token_remap_test` 均 10/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsk-12-s7zzzd-fielddef-token-retention.md`。
  备注：本切片只关闭 FieldDef member-token retained-row guard；不声明完整 trim analyzer、完整 metadata sweep/pruning 或
  12-S7 总体完成。

- 2026-07-03 05:50:58 +08:00 · 12-S7ZZT / 11-S7ZSC manifest export target-only string retention ·
  状态：12-S7 compacted `.zrp` metadata pruning 中 manifest export target string 保留子切片完成；07~12 总目标继续进行中，
  完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整 ABI drift/deopt coverage
  仍待后续。
  完成项目：string-pool compaction roots 现在覆盖 retained `manifestExports[].targetStringOffset`，避免 target 名称只存在于
  manifest export row 时被裁剪；retention 判定与 manifest export section rewrite 使用同一套 type/member token remap 规则，保证只为可保留 row
  收集 target string；source contract 增加 manifest export retention helper 与 targetStringOffset collection guard。
  RED：新增 target-only string fixture 后，WSL GCC `zr_vm_aot_c_zrp_metadata_pruning_test` 报 `Expected TRUE Was FALSE`，
  旧 compacted metadata prepare 在 remap manifest export target string 时找不到 `"unused"` 的 string remap。
  GREEN：WSL GCC/clang/Windows MSVC Debug `zr_vm_aot_c_zrp_metadata_pruning_test` 均 17/0，`zr_vm_aot_c_source_contracts_test`
  均 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsc-12-s7zzt-manifest-export-target-string-retention.md`。
  备注：本切片只关闭 manifest export target-only string 的 string-pool pruning 缺口；不声明完整 12-S7 或 07~12 总目标完成。

- 2026-07-03 05:30:18 +08:00 · 12-S7ZZZC / 10-S5P annotation warning source attribution ·
  状态：12-S7 trim warning diagnostics 中 annotation warning 的源位置信息子切片完成；07~12 总目标继续进行中，
  完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整 ABI drift/deopt coverage
  仍待后续。
  完成项目：visible `trim_warning.annotation[]` marker 现在包含 caller `sourceFile`、`sourceLine/sourceLineEnd`、
  `sourceColumn/sourceColumnEnd`，与 runtime fallback warning 的可定位诊断字段对齐；字段来自既有 ExecIR/function
  source-location 推导 helper，reason/message 输出保持兼容。
  RED：新增 source attribution 断言后，WSL GCC `aot_c_reflection_annotation_preserve` 报 `Expected Non-NULL`；旧 generated C
  只输出 `function/instruction/targetFunction/reason`，source contract 也缺少 ExecIR source-location helper guard。
  GREEN：WSL GCC/clang/Windows MSVC Debug `aot_c_reflection_annotation_preserve` 均 12/0，`aot_c_source_contracts` 均 24/0。
  产出：`tests/acceptance/2026-07-03-aot-10-s5p-12-s7zzzc-annotation-warning-source-attribution.md`。
  备注：本切片只关闭 annotation warning source attribution；不声明完整 attribute/annotation 策略、未注解反射 warning、
  完整 trim analyzer 或 12-S7 总体完成。

- 2026-07-03 05:13:46 +08:00 · 12-S7ZZZB / 10-S5O callsite `requires-unreferenced-code` annotation warning suppression ·
  状态：12-S7 trim warning diagnostics 中 annotation warning 的调用方级抑制子切片完成；07~12 总目标继续进行中，
  完整 trim analyzer、attribute/annotation promotion policy、完整 metadata sweep/pruning 和更完整 ABI drift/deopt
  coverage 仍待后续。
  完成项目：annotation warning scanner 现在分离 visible 与 suppressed warnings；caller metadata
  `suppressRequiresUnreferencedCodeWarning: true` 会把该 caller 静态调用 `requiresUnreferencedCode` callee 的 warning
  计入 suppressed count，并隐藏逐条 `trim_warning.annotation[]` marker。writer-level `suppressAnnotationWarnings`
  继续作为全局开关，把全部 annotation warnings 折叠到 suppressed count。
  RED：新增 callsite suppression fixture 后，WSL GCC `aot_c_reflection_annotation_preserve` 显示 `Expected Non-NULL`；
  生成物仍为 `trim_warnings.annotationCount = 1`、`trim_warnings.annotationSuppressedCount = 0` 并含 visible warning。
  GREEN：WSL GCC/clang/Windows MSVC Debug `aot_c_reflection_annotation_preserve` 均 12/0，`aot_c_source_contracts`
  均 24/0。
  产出：`tests/acceptance/2026-07-03-aot-10-s5o-12-s7zzzb-callsite-annotation-warning-suppression.md`。
  备注：本切片只关闭 callsite-level annotation warning suppression；不声明完整 attribute/annotation 策略、
  未注解反射 warning、完整 trim analyzer 或 12-S7 总体完成。

- 2026-07-03 05:04:05 +08:00 · 12-S7ZZZA / 11-S7ZSJ emitted `.zrp` FieldDef-owned orphan metadata sweep/pruning ·
  状态：12-S7 emitted metadata prune pipeline 的 FieldDef owner-TypeDef 孤儿行清扫子切片完成；07~12 总目标继续进行中，
  完整 trim analyzer、attribute/annotation 抑制策略、完整 metadata sweep/pruning 和更完整 ABI drift/deopt coverage
  仍待后续。
  完成项目：pruned `.zrp` header 的 `FIELD_DEFS` section 现在按 retained owner TypeDef 计数；不可达 TypeDef 只拥有
  FieldDef 时不会再被字段 owner 反向保活。复制 retained FieldDef 时会压缩 `MEMBER_DEF` token 并重写 owner/name/
  signature/constant 引用，TypeDef 的 field range 同步改写到 compacted FieldDef token 空间；string/signature/
  constant pools 和 member-token remap table 不再采纳 orphan field rows。
  RED：新增 orphan TypeDef-with-FieldDef metadata pruning fixture 后，WSL GCC `aot_c_zrp_metadata_pruning` 显示
  `Expected 576 Was 692`。
  GREEN：WSL GCC pruning 16/0、pool pruning 8/0、typedef pruning 2/0、typespec pruning 2/0；WSL clang 同组 16/0、
  8/0、2/0、2/0；Windows MSVC Debug pruning 16/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsj-12-s7zzza-fielddef-owner-sweep.md`。
  备注：本切片只关闭 FieldDef owner-TypeDef orphan sweep；不声明完整 trim analyzer、attribute policy、
  完整 metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-03 04:18:41 +08:00 · 12-S7ZZZ / 11-S7ZSI CLI/project non-publishable `.zrp` sidecar stale cleanup ·
  状态：12-S7 emitted metadata sidecar stale artifact 清理子切片完成；07~12 总目标继续进行中，完整 trim analyzer、
  attribute/annotation 抑制策略、完整 metadata sweep/pruning 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：CLI AOT 生成现在在当前 embedded blob 不能作为 publishable `.zrp` metadata 发布时删除派生 sidecar path
  上的旧文件；生成 `.c` 成功不再意味着旧 `.zrp` sidecar 可以继续存在。该路径覆盖 invalid definition-table fixture，
  也保护后续普通 `.zro` 或非 metadata 重新生成；旧 sidecar 无法删除时会 fail closed 并拒绝写新 `.c`。
  RED：新增 stale sidecar fixture 后 WSL GCC `cli_aot_compacted_metadata_sidecar` 失败 `Expected FALSE Was TRUE`。
  追加 blocked stale sidecar fixture 后再次 RED `Expected FALSE Was TRUE`。
  GREEN：WSL GCC/clang/Windows MSVC Debug focused CTest
  `cli_aot_compacted_metadata_sidecar|cli_aot_writer_options|aot_c_zrp_metadata_publication|aot_c_zrp_metadata_pruning|zrp_metadata_format`
  均 5/5。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsi-12-s7zzz-cli-zrp-sidecar-stale-cleanup.md`。
  备注：本切片只关闭 stale sidecar fail-closed 清理，不声明完整 trim analyzer、完整 metadata sweep/pruning 或
  12-S7 总体完成；`compiler_aot.c` 已超过大文件阈值，本次只扩展既有 AOT C artifact orchestration 分支。

- 2026-07-03 03:56:32 +08:00 · 12-S8E / 11-S6H CLI full-AOT metadata-drift assertion alignment ·
  状态：12-S8E full-AOT generic METHOD-slot closure 的 CLI project 验收与 11-S6H metadata-drift fallback 对齐完成；
  07~12 总目标继续进行中。
  完成项目：CLI project full-AOT generated-C 用例继续要求 `zr_aot_generic_call_typed_full_aot_no_deopt`，并禁止
  missing-instance deopt marker/reason；同时不再禁止 11-S6H metadata-drift bridge，而是显式要求 metadata guard、
  `CanUseTypedDirectCall()` 和 `"typed inline struct direct call metadata drift"` marker。这样 12-S8E 只关闭
  METHOD-slot missing-instance runtime branch，不误删 ABI drift fallback。
  RED：`cli_project_incremental` broad dynamic-bridge anti-needle 在当前 11-S6H 语义下失败 `Expected NULL`。
  GREEN：WSL GCC/clang/Windows MSVC Debug focused CTest
  `cli_project_incremental|cli_aot_compacted_metadata_sidecar|aot_runtime_typed_direct_call_compatibility|aot_c_generic_call_typed`
  均 4/4。
  产出：`tests/acceptance/2026-07-03-aot-11-s6h-12-s8e-cli-full-aot-metadata-drift-assertion.md`。
  备注：本切片只校正测试语义，不修改 AOT 生成器；不声明完整 full-AOT closure diagnostics 或 12-S8 总体完成。

- 2026-07-03 03:45:20 +08:00 · 12-S7ZZY / 11-S7ZSH CLI/project compacted `.zrp` metadata sidecar path bridge ·
  状态：12-S7 emitted metadata sidecar 自动 artifact path 子切片完成；07~12 总目标继续进行中，
  完整 trim analyzer、attribute/annotation 抑制策略、完整 metadata sweep/pruning 和更完整 ABI drift/deopt coverage
  仍待后续。
  完成项目：project/CLI 现在从 AOT C 输出路径自动派生同名 `.zrp` sidecar path，并把该 path 注入 writer-level
  `compactedZrpMetadataOutputPath`；注入条件要求 embedded blob 同时通过 `.zrp` header 读取和 definition-table
  validation，防止普通 `.zro` 或无效 compacted metadata 触发文件发布；关闭 `emitAotC` 或清理 removed module 时，
  派生 `.zrp` sidecar 与 `.c` artifact 一起删除。
  RED：新增 CLI sidecar target 后缺少 project path helper 导致 WSL GCC 链接失败；invalid definition-table fixture
  暴露 header-only gate 会误生成 sidecar，失败 `Expected FALSE Was TRUE`。
  GREEN：WSL GCC/clang/Windows MSVC Debug focused CTest
  `cli_aot_compacted_metadata_sidecar|cli_aot_writer_options|aot_c_zrp_metadata_publication|aot_c_zrp_metadata_pruning|zrp_metadata_format`
  均 5/5。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsh-12-s7zzy-cli-zrp-sidecar-path-bridge.md`。
  备注：本切片验证时相邻 `cli_project_incremental` 暴露 full-AOT dynamic bridge broad anti-needle 失败，随后
  2026-07-03 03:56:32 +08:00 的 11-S6H/12-S8E 测试语义对齐已收敛该失败；本切片只关闭 sidecar path bridge 与
  invalid metadata gate，不声明完整 trim analyzer、完整 metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-03 02:47:37 +08:00 · 12-S7ZZX / 11-S7ZSG writer-level compacted `.zrp` metadata sidecar publication ·
  状态：12-S7 emitted metadata publication 文件输出子切片完成；07~12 总目标继续进行中，
  CLI/project 自动派生 sidecar artifact path、完整 trim analyzer、attribute/annotation 抑制策略、
  完整 metadata sweep/pruning 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：`SZrAotWriterOptions.compactedZrpMetadataOutputPath` 可让 AOT C writer 把最终 prepared embedded
  `.zrp` metadata blob 写入独立 sidecar 文件；发布 helper 先验证 header，再写出当前 compacted blob 的 exact bytes，
  short write/close failure 会删除 partial sidecar；emitter 在 generated C `fclose` 成功后才发布 sidecar，
  sidecar 发布失败时删除 generated C 并 fail closed。
  RED/GREEN：RED 为新增 sidecar publication 测试后 WSL GCC 编译失败，
  `SZrAotWriterOptions` 缺少 `compactedZrpMetadataOutputPath`；GREEN 后 WSL GCC/clang/Windows MSVC Debug focused
  CTest `aot_c_zrp_metadata_publication|aot_c_zrp_metadata_pruning|aot_c_code_stripping|zrp_metadata_format|cli_aot_writer_options`
  均 5/5。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsg-12-s7zzx-zrp-compacted-metadata-publication.md`。
  备注：本记录只关闭 writer option 指定路径的 final metadata sidecar publication；不声明 CLI/project 自动 artifact
  policy、完整 12-S7、11-S7 或 07~12 总目标完成。

- 2026-07-03 02:04:51 +08:00 · 12-S7ZZW / 11-S7ZSF `.zrp` manifest export unbound declaration row publication ·
  状态：12-S7/11-S7 unbound export declarations 进入持久 `.zrp` manifestExports rows 的文件级策略子切片完成；
  完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制策略、完整 metadata sweep/pruning、
  compacted-token file publication 和更完整 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：`.zrp` `manifestExports` validator 现在允许 type/method/field rows 以 `flags = 0`、`typeToken = 0`、
  `memberToken = 0` 表示 unbound export declaration；剪裁重写既有 rows 时会保留该形态并继续 remap target string
  offset；writer declaration publication 会把未绑定 type/method/field declarations 追加为持久 rows。kind/token 错配、
  unknown flags、未设置 flag 却携带 token 的 rows 仍 fail closed。
  RED：新增 format unbound row 覆盖后 WSL GCC `zrp_metadata_format` 显示 `Expected TRUE Was FALSE`；新增 pruning
  unbound declaration publication 覆盖后旧 blob 未追加 rows，显示 `Expected 806 Was 708`。
  GREEN：WSL GCC/clang/Windows MSVC Debug focused CTest
  `zrp_metadata_format|aot_c_zrp_metadata|aot_c_code_stripping|aot_c_guardrail_contracts` 均 8/8。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsf-12-s7zzw-zrp-manifest-export-unbound-declaration-publication.md`。
  备注：本记录只关闭 unbound declaration 的文件级持久 row 发布/保留/校验；不声明完整 trim analyzer、
  完整 metadata sweep/pruning、runtime binding gate 扩展或 12-S7 总体完成。

- 2026-07-03 01:33:09 +08:00 · 12-S7ZZV / 11-S7ZSE `.zrp` manifest export type declaration row publication ·
  状态：12-S7/11-S7 writer-bound type export declarations 进入持久 `.zrp` manifestExports rows 的支撑子切片完成；
  完整 12-S7 仍未关闭，完整 trim analyzer、unbound declaration file policy、attribute/annotation 抑制策略、
  完整 metadata sweep/pruning 和更完整 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：TypeDef pruning 现在生成 source `TYPE_DEF` 到 compacted `TYPE_DEF` 的 sidecar remap，供
  `backend_aot_c_zrp_publish_manifest_export_declarations()` 在 append `TYPE` declaration rows 前改写 type token。
  该发布路径会保留既有 manifest export rows，按需扩展 string pool，重建 header/section offsets，并校验最终 blob。
  RED：新增 pruning 用例后旧 blob 未追加 type declaration row，WSL GCC focused 显示 `Expected 559 Was 526`。
  GREEN：WSL GCC/clang/Windows MSVC Debug focused CTest
  `aot_c_zrp_metadata|aot_c_code_stripping|aot_c_guardrail_contracts` 均 7/7。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zse-12-s7zzv-zrp-manifest-export-type-declaration-publication.md`。
  备注：本记录只关闭 bound type export declaration 的文件级 row 发布；不声明完整 trim analyzer、
  unbound declaration 持久化、完整 metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-03 00:43:21 +08:00 · 12-S7ZZU / 11-S7ZSD `.zrp` manifest export declaration row publication ·
  状态：12-S7/11-S7 writer-bound method/field export declarations 进入持久 `.zrp` manifestExports rows 的支撑子切片完成；
  完整 12-S7 仍未关闭，完整 trim analyzer、type export 持久 row compaction、未绑定 declaration file policy、
  attribute/annotation 抑制策略、完整 metadata sweep/pruning 和更完整 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：`backend_aot_c_zrp_publish_manifest_export_declarations()` 在 prepared embedded metadata 上追加可发布的
  method/field declarations：保留既有 manifest export rows，按 retained member-token sidecar 把 source `MEMBER_DEF`
  改写为 compacted token，按需扩展 string pool，重建 header/section offsets，并用
  `ZrCore_ZrpMetadata_ValidateDefinitionTables()` 校验最终 blob。`backend_aot_c_prepare_embedded_zrp_metadata()`
  会在有 `.zrp` blob 时执行该发布步骤，使 code-stripping stats 和 emitter 后续采样读取最终 rows。
  RED：新增 pruning 用例后旧 blob 未追加 writer declaration rows，MSVC focused 显示 `Expected 767 Was 708`。
  GREEN：WSL GCC/clang/Windows MSVC Debug direct pruning 均 12/0；同三套 focused export-token remap 10/0、code
  stripping 10/0 均通过。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsd-12-s7zzu-zrp-manifest-export-declaration-publication.md`。
  备注：本记录只关闭 bound method/field export declaration 的文件级 row 发布；不声明完整 trim analyzer、
  type/unbound declaration 持久化、完整 metadata sweep/pruning 或 12-S7 总体完成。

- 2026-07-02 10:42:54 +08:00 · 12-S7ZZT / 11-S7ZSC `.zrp` manifest export pruning rewrite ·
  状态：12-S7/11-S7 manifest export 持久 section 的剪裁后重写子切片完成；完整 12-S7 仍未关闭，完整
  trim analyzer、writer 端持久 manifest export row 生成/写入、attribute/annotation 抑制策略、完整 metadata
  sweep/pruning 和更完整 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：`.zrp` pruned-blob rebuild 现在对 `manifestExports` section 执行结构化 copy/rewrite，而不是 raw-copy。
  `targetStringOffset` 使用 compacted string-pool remap，method/field `memberToken` 使用 retained member-token
  remap，type export `typeToken` 使用 TypeDef token remap；无效 kind/flag/token shape 或丢失 remap 会 fail closed。
  新增 `backend_aot_c_zrp_metadata_manifest_export.{h,c}`，并让 MSVC shared-DLL focused targets 编入该 helper。
  RED：WSL GCC focused pruning 11/1，旧实现保留 raw target offset 25，未重写为 compacted string offset 17。
  GREEN：WSL GCC/clang/Windows MSVC Debug direct pruning 11/0；同三套 focused export-token remap 10/0、code stripping
  10/0 均通过。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zsc-12-s7zzt-zrp-manifest-export-pruning.md`。
  备注：本记录只关闭已有持久 manifest export rows 的 pruning rewrite，不声明完整 trim analyzer 或 writer 持久行生成完成。

- 2026-07-02 10:15:47 +08:00 · 12-S7ZZS / 11-S7ZSB `.zrp` manifest export persistent section format ·
  状态：12-S7/11-S7 manifest export 持久 section 的格式层支撑子切片完成；完整 12-S7 仍未关闭，完整
  trim analyzer、持久 manifest export row 生成、pruner 对该 section 的完整 rewrite/copy 策略、
  attribute/annotation 抑制策略、完整 metadata sweep/pruning 和更完整 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：`.zrp` metadata v4 追加第 13 个 `manifestExports` definition-table section，并把该 section 接入
  immutable/mutable section helpers、CLI metadata dump/diff、AOT metadata size/count stats、generated-C 正常统计 marker
  以及 code-stripping before/after/removed delta marker。row 校验覆盖 kind/flag/token shape 与 string-pool target。
  RED：WSL GCC focused metadata format 编译失败暴露旧 header 没有 manifest export section；随后 code-stripping
  focused 10/1 暴露旧统计与 generated registration 仍只期待 12 sections。
  GREEN：补齐 format/view/validation/stats/dump 后，WSL GCC/clang/MSVC Debug direct
  `zr_vm_zrp_metadata_format_test`、`zr_vm_cli_zrp_metadata_dump_test`、
  `zr_vm_aot_c_zrp_metadata_size_deltas_test`、`zr_vm_aot_c_code_stripping_test` 均通过；export-token remap 相邻验证 10/0。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zsb-12-s7zzs-zrp-manifest-export-section.md`。
  备注：本记录只关闭 `.zrp` section 格式和裁剪统计可见性；不声明完整 trim analyzer 或持久 manifest export row
  生成/裁剪发布完成。

- 2026-07-02 09:38:04 +08:00 · 12-S7ZZR / 11-S7ZSA manifest export kind/token guard ·
  状态：12-S7/11-S7 manifest export table builder 支撑子切片完成；完整 12-S7 仍未关闭，完整
  trim analyzer、attribute/annotation 抑制策略、持久 `.zrp` manifest export section、完整 metadata
  sweep/pruning 和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：`backend_aot_c_zrp_manifest_export_table_build()` 新增 declaration kind 与绑定 token shape 校验；
  type export 只接受未绑定或 `TYPE_DEF` binding，method/field export 只接受未绑定或 `MEMBER_DEF` binding。
  已绑定错误 token kind 的 declaration fail closed，且不会留下 partially-built table metadata。
  RED：WSL GCC focused export-token remap 测试新增 kind/token mismatch 用例后失败 10/1；旧 builder 接受了携带
  `MEMBER_DEF(1)` 的 type export。
  GREEN：新增 guard 后 WSL GCC/clang/MSVC Debug direct `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 均 10/0；
  WSL GCC direct `zr_vm_aot_c_source_contracts_test` 24/0；WSL GCC/clang provider shared-library smoke 均 1/0。
  验证：MSVC focused build 通过；`zr_vm_library/src/zr_vm_library/aot_runtime.c` 仍有既有 C4267/C4702 warning 噪声。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zsa-12-s7zzr-manifest-export-kind-token-guard.md`。
  备注：本记录只关闭 generated-C manifest export table builder 的 kind/token shape guard；不声明完整 trim analyzer、
  持久 `.zrp` manifest export section、完整 metadata sweep/pruning 或 ABI drift/deopt 闭环完成。

- 2026-07-02 09:15:26 +08:00 · 12-S7 support / 11-S7ZR range-selected provider runtime export publication ·
  状态：12-S7 cross-module provider candidate runtime/export publication 支撑子切片完成；完整 12-S7 仍未关闭，
  完整 trim analyzer、attribute/annotation 抑制策略、完整 metadata sweep/pruning、compacted-token file publication 和
  更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：provider shared-library smoke 现在在同一 root project 中声明 exact `$mathLocal@2.1.0/ops/sum` 与
  range-selected `$mathRange@2.1.0/ops/sum` 两条 provider import；`mathRange` 从 2.0.5/2.1.0 candidates 中选择 2.1.0。
  strict AOT C runtime 导入 candidate-selected alias 后可读取普通 `seed` public export 和 attached manifest export
  metadata (`add`/`seed`)。
  RED：WSL GCC provider smoke 在 `$mathRange@2.1.0/ops/sum` 上失败，metadata view 已 attach，但 public export
  `seed` 未发布；根因是 export publication 对缺失 `frame.recordHandle` 的 generated frame 回退到 first equivalent
  function record，复用了 earlier exact alias record。
  GREEN：`ZrLibrary_AotRuntime_PublishModuleExports()` 优先使用匹配当前 function 的 `runtimeState->activeRecord`；
  WSL GCC/clang provider shared-library smoke 均 1/0。WSL GCC/clang/MSVC Debug direct provider version-selection 4/0、
  resolver 9/0、manifest normalization 28/0、provider runtime 1/0、source contracts 24/0、frame setup contracts 1/0；
  MSVC provider smoke 编译通过且 Unix-only 分支 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zr-range-selected-provider-export-publication.md`。
  备注：本记录只关闭 range-selected provider runtime export publication，不声明完整 trim analyzer、
  完整 metadata sweep/pruning 或 ABI drift/deopt 闭环完成。

- 2026-07-02 08:51:27 +08:00 · 12-S7 support / 11-S7ZQ legacy dependency AssemblyRef identity canonicalization ·
  状态：12-S7 cross-module provider/reference metadata support 测试校准子切片完成；完整 12-S7 仍未关闭，完整
  trim analyzer、attribute/annotation 抑制策略、完整 metadata sweep/pruning 和更完整的 runtime ABI drift/deopt
  coverage 仍待后续。
  完成项目：关闭 11-S7ZL 探测留下的 canonicalization 35/1 失败；legacy dependency 的 static import/module effect
  仍使用 canonical `$math@1.2.3/ops/sum`，AssemblyRef 行名则按 normalized provider manifest identity 使用 `math`。
  测试同步断言不存在 canonical key 命名的 AssemblyRef。生产代码未修改。
  RED：旧断言按 canonical import key 查 AssemblyRef，WSL GCC canonicalization 失败 35/1；临时 guard 证明 assembly
  identity 已在 module effect 中绑定。
  GREEN：WSL GCC/clang/Windows MSVC Debug direct canonicalization 均 35/0；provider version-selection 4/0、
  resolver 9/0、manifest normalization 28/0、provider runtime 1/0；provider shared-library smoke 在 WSL GCC/clang
  均 1/0，Windows MSVC Debug 按 Unix-only dynamic-loader 分支 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zq-legacy-dependency-assembly-ref-identity.md`。
  备注：本记录只关闭 stale AssemblyRef identity 断言；不声明完整 trim analyzer、完整 metadata sweep/pruning
  或 ABI drift/deopt 闭环完成。

- 2026-07-02 08:16:21 +08:00 · 12-S7 support / 11-S7ZP provider automatic range-based candidate selection ·
  状态：12-S7 cross-module provider candidate-set selection 支撑子切片完成；完整 12-S7 仍未关闭，完整
  trim analyzer、attribute/annotation 抑制策略、完整 metadata sweep/pruning 和更完整的 runtime ABI drift/deopt
  coverage 仍待后续。
  完成项目：`.zrp` `references.alias.candidates[]` 支持在没有 exact `path` 时自动选择 provider。project loader
  会 probe 候选 `.zrp`/`.zrm`，按 declared assembly、可选 exact version、declared `[min, max)` 和 strict semver
  排序筛选，选择最高 in-range 版本；非选中候选不进入 normalized dependency table，no-match set fail-closed。
  `zrp.schema.json` 同步表达 `path` 与 `candidates` 的互斥形态。
  RED：candidate-only reference 缺少 required `path`，旧 loader 在 positive in-range 选择用例返回 NULL。
  GREEN：WSL GCC/clang/Windows MSVC Debug focused version-selection 4/0；resolver、manifest normalization、
  provider runtime 与 provider shared-library smoke 相邻回归通过；schema JSON parse 通过。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zp-provider-range-candidate-selection.md`。
  备注：本记录只关闭 automatic range-based provider candidate selection，不声明完整 trim analyzer、完整 metadata
  sweep/pruning 或 ABI drift/deopt 闭环完成。

- 2026-07-02 08:13:22 +08:00 · 12-S7 support / 11-S7ZO provider export metadata attach fixture ·
  状态：12-S7 provider export metadata attach 覆盖子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、automatic range-based candidate selection、完整 metadata sweep/pruning 和更完整的
  runtime ABI drift/deopt coverage 仍待后续。
  完成项目：provider shared-library smoke 现在生成带 `exports` manifest 的 provider `.zrp`，把 method `add` 和
  field `seed` 绑定到 writer manifest export declarations；generated C 断言 manifest export table 与
  member-token flags，runtime import 后断言 attached provider metadata runtime 可查到同一 method/field export view。
  RED/基线：此前只验证 provider 动态库成功加载和普通 export value publication，未覆盖 manifest export metadata attach。
  GREEN：WSL GCC/clang provider shared-library smoke 均 1/0；Windows MSVC Debug 目标编译通过且 Unix-only
  dynamic-loader 分支 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zo-provider-export-metadata-attach-fixture.md`。
  备注：本记录只关闭 provider generated shared-library export metadata attach fixture，不声明完整 trim analyzer 或
  完整 metadata sweep/pruning 完成。

- 2026-07-02 07:25:55 +08:00 · 12-S7 support / 11-S7ZN provider multi-version exact selection + range guard ·
  状态：12-S7 cross-module provider exact alias/version selection 与 declared range fail-closed 支撑子切片完成；
  完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制策略、automatic range-based candidate
  selection、完整 metadata sweep/pruning 和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：新增 `zr_vm_project_import_provider_version_selection_test`，锁定同一 assembly 多版本 provider
  reference 在 strict AOT load request 前的精确 alias/version 路径选择；manifest parse 对 strict
  `major.minor.patch` declared min/max range 执行 actual version fail-closed，覆盖 `.zrp` 与 `.zrm`
  reference 输入面。
  RED：负向 provider reference 的 actual `3.1.0` 不在 `[2.0.0, 3.0.0)` 内，但旧 parser 仍创建 project。
  GREEN：补齐 range guard 后 focused test 2/0；WSL GCC/clang direct version-selection、project import resolver、
  manifest normalization、provider runtime、provider shared-library smoke 均通过；Windows MSVC Debug direct
  version-selection/resolver/manifest/runtime 均通过，provider shared-library smoke 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zn-provider-version-selection-range-guard.md`。
  备注：本记录只关闭 exact selection 与 declared range guard，不声明完整 metadata sweep/pruning、full trim analyzer、
  automatic candidate resolver 或 export metadata attach 完成。

- 2026-07-02 07:01:13 +08:00 · 12-S7 support / 11-S7ZM provider AOT dynamic-library success fixture ·
  状态：12-S7 cross-module provider AOT success-path 支撑子切片完成；完整 12-S7 仍未关闭，
  完整 trim analyzer、attribute/annotation 抑制策略、multi-version selection、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：新增 `zr_vm_aot_c_provider_shared_library_smoke_test`，真实生成 provider `.zrp`、`ops/sum.zr`、
  `.zro`、AOT C 与 `zrvm_aot_ops_sum.so`，并通过 strict AOT C 导入 canonical provider key；断言 provider
  动态库路径、provider-local descriptor name、canonical cache key、AOT C executed-via 和 `seed` export。
  RED/基线：之前仅有 missing-provider diagnostic 覆盖，未有成功路径 fixture；新增 target 在重新 configure 后通过，
  证明 11-S7ZL runtime load-request consumption 已能消费真实生成的 provider library。
  GREEN：WSL GCC/clang direct `zr_vm_aot_c_provider_shared_library_smoke_test` 均 1/0；Windows MSVC Debug target
  编译通过且 Unix-only dynamic-loader 分支 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zm-provider-aot-dynamic-library-success.md`。
  备注：本记录只关闭 `.zrp` provider dynamic-library success coverage，不声明 multi-version selection、
  export metadata attach、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 06:41:32 +08:00 · 12-S7 support / 11-S7ZL provider AOT runtime load-request consumption ·
  状态：12-S7 cross-module provider AOT runtime-load input consumption 支撑子切片完成；完整 12-S7 仍未关闭，
  完整 trim analyzer、attribute/annotation 抑制策略、provider 动态库成功加载端到端、multi-version selection、
  完整 metadata sweep/pruning 和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：strict AOT module loader 现在消费
  `ZrLibrary_Project_ResolveImportProviderAotLoadRequest()` 输出，使用 provider `.zrp` 的 source/binary/library path
  和 provider-local descriptor module name；`.zrm` provider archive entry 明确不进入 dynamic library path。
  RED：focused WSL GCC direct test 失败于 canonical provider import 未产生 provider library diagnostic。
  GREEN：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_project_import_aot_provider_runtime_test` 均 1/0；
  WSL GCC `zr_vm_project_import_resolver_test` 9/0。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zl-provider-aot-runtime-load-request.md`。
  备注：额外 WSL GCC `zr_vm_project_import_canonicalization_test` 曾有独立 35/1 失败；后续 11-S7ZQ 已确认
  这是 stale AssemblyRef identity 断言并关闭。

- 2026-07-02 06:13:21 +08:00 · 12-S7 support / 11-S7ZK provider AOT load request ·
  状态：12-S7 cross-module provider AOT load-request input 支撑子切片完成；完整 12-S7 仍未关闭，
  完整 trim analyzer、attribute/annotation 抑制策略、provider runtime dynamic loading、multi-version selection、
  完整 metadata sweep/pruning 和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：`ZrLibrary_Project_ResolveImportProviderAotLoadRequest()` 为后续裁剪后的 provider binding/export metadata
  consumption 提供统一输入，输出 canonical `$alias@version/module`、descriptor-local module name、backend-specific AOT
  library path 和 declared assembly/version range；`.zrm` provider 保留 archive/entry view，不把 archive entry 错判为
  filesystem dynamic library。
  RED：focused WSL GCC build 在新增测试引用缺失 load-request struct/API 时编译失败。
  GREEN：补齐 public API/实现后 WSL GCC/clang/Windows MSVC Debug direct
  `zr_vm_project_import_resolver_test` 均 9/0 通过；该 focused target 当前未作为独立 CTest 注册。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zk-provider-aot-load-request.md`。
  备注：不声明 provider runtime dynamic loading、multi-version selection、export metadata attach、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-02 05:58:12 +08:00 · 12-S7 support / 11-S7ZJ provider import location discovery ·
  状态：12-S7 cross-module provider path/version input 支撑子切片完成；完整 12-S7 仍未关闭，
  完整 trim analyzer、attribute/annotation 抑制策略、provider runtime loading、multi-version selection、
  完整 metadata sweep/pruning 和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：`ZrLibrary_Project_ResolveImportProviderLocation()` 为后续裁剪后的 export manifest provider binding
  提供统一 location input：它返回 canonical `$alias@version/module`、declared assembly/version range、
  `.zrm` archive/module entry 或 `.zrp` source/binary/intermediate path，避免 trim/provider binding 层重复解析
  project manifest dependency/reference 规则。
  RED/GREEN：RED 为 focused import resolver 测试新增 provider location 覆盖后编译失败于缺少 public type/API；
  GREEN 后 `.zrp` project reference 和 `.zrm` assembly reference 两类 provider import location 均被覆盖。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_project_import_resolver_test` 均 9/0；该 focused target
  当前未作为独立 CTest 注册。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zj-provider-import-location-discovery.md`。
  备注：本切片只提供 trim/provider binding 需要的 import location input；不声明 provider runtime loading、
  版本候选选择、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 05:34:27 +08:00 · 12-S7 support / 11-S7ZI provider manifest export binding gate ·
  状态：12-S7 generated-C export manifest table provider import-verifier gate 支撑子切片完成；完整 12-S7 仍未关闭，
  完整 trim analyzer、attribute/annotation 抑制策略、standalone provider import-path discovery/loading/version selection、
  完整 metadata sweep/pruning 和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：已发布到 `SZrAotCodeRegistration.manifestExports` 的 generated-C export manifest table 现在会被真实
  provider import signature verifier 消费；若 provider runtime 已 attached manifest export table，typed export symbol
  的 metadata token 必须与 table 中同 `kind + target` entry 发布的 type/member token 一致，防止裁剪/remap 后 table
  与 provider symbol 身份分裂。无 manifest export table 的旧 provider 继续走 legacy 兼容路径。
  RED/GREEN：RED 为 focused TypeRef/import binding 测试新增 manifest export token drift fixture 后，旧 verifier
  返回 true；GREEN 后 `module_import_signature_manifest_export.c/.h` 接入 gate，同一测试 9/0。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_metadata_type_ref_binding_test` 9/0、
  `zr_vm_metadata_runtime_manifest_exports_test` 7/0、`zr_vm_metadata_runtime_binding_compatibility_test` 15/0；同三套工具链
  CTest `metadata_type_ref_binding|metadata_runtime_manifest_exports|metadata_runtime_binding_compatibility` 均通过。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zi-provider-manifest-export-binding-gate.md`。
  备注：本切片只让 generated-C manifest export table 进入 provider import verifier 的 fail-closed token identity gate；
  不声明完整 metadata sweep/pruning、standalone provider discovery/loading/version selection 或 full trim analyzer 完成。

- 2026-07-02 04:46:47 +08:00 · 12-S7 support / 11-S7ZH manifest export binding gate ·
  状态：12-S7 generated-C export manifest table runtime binding gate 支撑子切片完成；完整 12-S7 仍未关闭，
  完整 trim analyzer、attribute/annotation 抑制策略、cross-module provider loading/version binding、standalone
  provider import-path wiring、完整 metadata sweep/pruning 和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：`ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility()` 现在把 attached
  `SZrAotCodeRegistration.manifestExports` 的 `kind + target` lookup 与 runtime binding compatibility 串联起来；
  missing export、export token mismatch 与 module-version drift 分别有可测试 status/report，避免后续裁剪后的
  export table 被 provider binding path 当作裸 token 使用。
  RED/GREEN：RED 为 manifest export focused 测试新增 binding gate 覆盖后编译失败于缺少 public API/status；GREEN 后
  mirror/view/binding gate 共 7/0。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_metadata_runtime_manifest_exports_test` 7/0、
  `zr_vm_metadata_runtime_binding_compatibility_test` 15/0；同三套工具链 CTest
  `metadata_runtime_manifest_exports` 与 `metadata_runtime_binding_compatibility` 均通过。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zh-manifest-export-binding-gate.md`。
  备注：本切片只让已发布 export manifest table 能进入 runtime binding compatibility gate；不声明 provider
  loading/version binding、import-path wiring、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 04:23:15 +08:00 · 12-S7 support / 11-S7ZG manifest export runtime view ·
  状态：12-S7 generated-C export manifest table runtime view 支撑子切片完成；完整 12-S7 仍未关闭，
  完整 trim analyzer、attribute/annotation 抑制策略、cross-module provider loading/version binding、standalone
  provider import-path wiring、完整 metadata sweep/pruning 和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：attached `SZrMetadataRuntime` 现在可通过 `ZrCore_MetadataRuntime_ReadManifestExportView()` 按
  `kind + target` 唯一消费 `SZrAotCodeRegistration.manifestExports`；返回 view 带只读 entry、index、target、
  `typeToken`/`memberToken`，并对重复 export、缺 required token 和 token shape mismatch fail closed。
  RED/GREEN：RED 为新 runtime view 测试编译失败于缺少 public view type/API；GREEN 后 mirror、成功 lookup、
  duplicate reject 和 missing-token reject 覆盖 4/0。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_metadata_runtime_manifest_exports_test` 4/0、
  `zr_vm_metadata_runtime_query_test` 25/0、`zr_vm_metadata_runtime_binding_compatibility_test` 15/0；同三套工具链 CTest
  `metadata_runtime_manifest_exports`、`metadata_runtime_query`、`metadata_runtime_binding_compatibility` 分别通过。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zg-manifest-export-runtime-view.md`。
  备注：本切片只让已发布的 export manifest table 有 runtime lookup/view API；不声明 provider loading/version binding、
  import-path wiring、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 03:57:34 +08:00 · 12-S7 support / 11-S7ZF manifest export runtime mirror ·
  状态：12-S7 generated-C export manifest table runtime mirror 支撑子切片完成；完整 12-S7 仍未关闭，
  完整 trim analyzer、attribute/annotation 抑制策略、cross-module provider loading/version binding、standalone
  provider import-path manifest consumption、完整 metadata sweep/pruning 和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：attached `SZrMetadataRuntime` 现在镜像 `SZrAotCodeRegistration.manifestExports/manifestExportCount`；
  `ZrCore_Module_AttachMetadataRuntime()` 在模块 attach 时发布该只读 table view；新增 focused metadata runtime manifest
  export 测试和 CTest 注册，确保裁剪后已发布的 type/member export table 能被运行期 metadata consumer 读取。
  RED/GREEN：RED 为新 runtime mirror 测试编译失败于缺少 `manifestExports/manifestExportCount`；GREEN 后 mirror 字段、
  attach wiring 和尾部追加结构布局修正通过相邻 metadata runtime query 回归。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_metadata_runtime_manifest_exports_test` 1/0、
  `zr_vm_metadata_runtime_query_test` 25/0、`zr_vm_metadata_runtime_binding_compatibility_test` 15/0；同三套工具链 CTest
  `metadata_runtime_manifest_exports`、`metadata_runtime_query`、`metadata_runtime_binding_compatibility` 均 3/3。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zf-manifest-export-runtime-mirror.md`。
  备注：本切片不实现跨模块 provider/version binding、standalone provider import-path manifest consumption、完整 metadata
  sweep/pruning 或 full trim analyzer。

- 2026-07-02 03:24:57 +08:00 · 12-S7 support / 11-S7ZE manifest export table publication ·
  状态：12-S7 persistent generated-C export manifest table 支撑子切片完成；完整 12-S7 仍未关闭，
  完整 trim analyzer、attribute/annotation 抑制策略、cross-module provider loading/version binding、standalone
  provider manifest consumption、完整 metadata sweep/pruning 和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：AOT ABI 新增 `SZrAotManifestExportEntry` 和 descriptor/codeRegistration table fields；generated C 现在输出
  `manifest.exportTableEntries`、`zr_aot_manifest_exports[]`，并把 table pointer/count 写入 descriptor 与 code registration；
  method/field export entries 通过 retained member-token remap sidecar 发布 compacted `MEMBER_DEF` token，type export
  entries 保留 `TYPE_DEF` token；runtime descriptor validation 校验 pointer/count、kind/flags、type/member token shape。
  RED/GREEN：RED 为 manifest export table focused test 要求旧 source member token 经 pruning/remap 后写入 table，
  旧生成物没有 persistent table；GREEN 后 writer-options generated C 输出 unbound/type/member-bound table entries，
  focused remap test 验证 source `MEMBER_DEF(7)` -> compacted `MEMBER_DEF(2)`。
  验证：WSL GCC direct `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 9/0、
  `zr_vm_cli_aot_writer_options_test` 18/0、`zr_vm_aot_c_source_contracts_test` 24/0；WSL GCC/clang/Windows MSVC Debug
  focused CTest `aot_c_zrp_metadata_export_token_remap|cli_aot_writer_options` 均 2/2，source contracts 均 24/0。
  产出：`tests/acceptance/2026-07-02-aot-11-s7ze-manifest-export-table-publication.md`。
  备注：本切片不实现跨模块 provider/version binding、standalone provider manifest consumption、完整 metadata sweep/pruning
  或 full trim analyzer。

- 2026-07-02 02:22:47 +08:00 · 12-S7 support / 11-S7ZD type export declaration type-token binding ·
  状态：12-S7 persistent export manifest/table 的 type export token writer-input 支撑子切片完成；完整 12-S7 仍未关闭，
  完整 trim analyzer、attribute/annotation 抑制策略、持久 cross-module export manifest/table writer、
  compacted-token file publication、cross-module provider loading/version binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：type `exports` declaration 现在可在 writer options 中携带 current-module `TYPE_DEF` token binding；
  CLI bridge 从当前函数 metadata token records 解析 matching TypeDef signature record，generated C 输出 per-export
  `typeToken` 诊断，为后续 retained type export metadata 的持久 table 写入提供 token-ready 输入。
  RED/GREEN：RED 为 WSL GCC focused build 编译失败，新增测试引用尚不存在的
  `SZrAotManifestExportDeclaration.hasTypeTokenBinding/typeToken`；GREEN 后 type export declaration 绑定与
  generated-C token marker 补齐，且测试期望对齐项目实际 TypeDef token 编码 `0x02000001`。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_cli_aot_writer_options_test` 均 18/0、
  `zr_vm_aot_c_source_contracts_test` 均 24/0、`zr_vm_aot_c_code_stripping_test` 均 10/0；同三套工具链 focused CTest
  `cli_aot_writer_options|aot_c_code_stripping` 均 2/2。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zd-type-export-declaration-type-token-binding.md`。
  备注：本切片不发射持久 export table、不把 compacted tokens 写回 `.zrp` 文件、不处理 provider version binding，
  也不声明 full trim analyzer 完成。

- 2026-07-02 01:57:22 +08:00 · 12-S7 support / 11-S7ZC field export declaration member-token binding ·
  状态：12-S7 persistent export manifest/table 的 field export token writer-input 支撑子切片完成；完整 12-S7 仍未关闭，
  完整 trim analyzer、attribute/annotation 抑制策略、持久 cross-module export manifest/table writer、type export
  token binding、compacted-token file publication、cross-module provider loading/version binding、完整 metadata
  sweep/pruning 和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：field `exports` declaration 现在可在 writer options 中携带 current-module `MEMBER_DEF` token binding；
  CLI bridge 从当前函数 typed exported variable symbols 解析 token，generated C 输出 per-export `memberToken`
  诊断，为后续 retained field export metadata 的持久 table 写入提供 token-ready 输入。
  RED/GREEN：RED 为 WSL GCC direct `zr_vm_cli_aot_writer_options_test` 中新增 field binding 用例失败于
  `Expected TRUE Was FALSE`；GREEN 后 field export declaration 绑定与 generated-C token marker 补齐。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_cli_aot_writer_options_test` 均 17/0、
  `zr_vm_aot_c_source_contracts_test` 均 24/0、`zr_vm_aot_c_code_stripping_test` 均 10/0；同三套工具链 focused CTest
  `cli_aot_writer_options|aot_c_code_stripping` 均 2/2。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zc-field-export-declaration-member-token-binding.md`。
  备注：本切片不发射持久 export table、不把 compacted tokens 写回 `.zrp` 文件、不处理 type export token binding，
  也不声明 full trim analyzer 完成。

- 2026-07-02 01:43:16 +08:00 · 12-S7 support / 11-S7ZB export declaration member-token binding ·
  状态：12-S7 persistent export manifest/table 的 method export token writer-input 支撑子切片完成；完整 12-S7 仍未关闭，
  完整 trim analyzer、attribute/annotation 抑制策略、持久 cross-module export manifest/table writer、type/field export
  token binding、compacted-token file publication、cross-module provider loading/version binding、完整 metadata
  sweep/pruning 和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：method `exports` declaration 现在可在 writer options 中携带 current-module `MEMBER_DEF` token binding；
  CLI bridge 从当前函数 typed exported function symbols 解析 token，generated C 输出 per-export `memberToken`
  诊断，为后续 retained export metadata 的持久 table 写入提供 token-ready 输入。
  RED/GREEN：RED 为 WSL GCC `zr_vm_cli_aot_writer_options_test` 编译失败，新增测试引用尚不存在的
  `SZrAotManifestExportDeclaration.hasMemberTokenBinding/memberToken`；GREEN 后 method export declaration 绑定与
  generated-C token marker 补齐。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_cli_aot_writer_options_test` 均 16/0、
  `zr_vm_aot_c_source_contracts_test` 均 24/0、`zr_vm_aot_c_code_stripping_test` 均 10/0；同三套工具链 focused CTest
  `cli_aot_writer_options|aot_c_code_stripping` 均 2/2。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zb-export-declaration-member-token-binding.md`。
  备注：本切片不发射持久 export table、不把 compacted tokens 写回 `.zrp` 文件、不处理 type/field export token binding，
  也不声明 full trim analyzer 完成。

- 2026-07-02 01:09:04 +08:00 · 12-S7 support / 11-S7ZA export declaration writer option bridge ·
  状态：12-S7 persistent export manifest/table 的 writer-input 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、持久 cross-module export manifest/table writer、export target token binding、
  compacted-token file publication、cross-module provider loading/version binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：project-level `exports` declaration 现在可进入 `SZrAotWriterOptions.manifestExportDeclarations`，
  CLI scratch storage 管理 writer option 生命周期，generated C 输出 export declaration 数量与 method/type/field
  target diagnostics。这为后续把 retained export metadata 写入持久 manifest/table 提供 writer-visible 输入。
  RED/GREEN：RED 为 WSL GCC `zr_vm_cli_aot_writer_options_test` 编译失败，新增测试引用了尚不存在的 writer export
  declaration fields/enums；GREEN 后 export declaration bridge 与 generated-C diagnostics 补齐。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_cli_aot_writer_options_test` 均 15/0、
  `zr_vm_aot_c_source_contracts_test` 均 24/0、`zr_vm_aot_c_code_stripping_test` 均 10/0；同三套工具链 focused CTest
  `cli_aot_writer_options|aot_c_source_contracts|aot_c_code_stripping` 匹配已注册的 `cli_aot_writer_options` 与
  `aot_c_code_stripping`，均 2/2。
  产出：`tests/acceptance/2026-07-02-aot-11-s7za-export-declaration-writer-options.md`。
  备注：本切片不执行 export target token binding、不发射持久 export table、不把 compacted tokens 写回 `.zrp` 文件，
  也不声明 full trim analyzer 完成。

- 2026-07-02 00:37:35 +08:00 · 12-S7 support / 11-S7Z export manifest declaration model ·
  状态：12-S7 cross-module export manifest/table 的输入模型支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、持久 cross-module export manifest/table writer、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：`.zrp` manifest 现在可声明 `exports` 数组并规范化为 `SZrLibrary_ProjectExportDeclaration`；
  parser 接受 `type`/`method`/`field` target，拒绝 unsafe target 和 unsupported kind，schema 同步该声明形态。
  这为后续把裁剪后的 retained export metadata 写入持久 manifest/table 提供稳定 project-level 输入。
  RED/GREEN：RED 为 WSL GCC `zr_vm_project_manifest_normalization_test` 编译失败，新增测试引用了尚不存在的
  export declaration fields/enums；GREEN 后 project model、`project_exports.{h,c}`、schema parity 和释放路径补齐。
  验证：WSL GCC/clang direct project manifest normalization 均 28/0；Windows MSVC Debug direct 同 28/0；
  该 executable 当前未注册成 CTest，focused CTest regex 未匹配测试。
  产出：`tests/acceptance/2026-07-02-aot-11-s7z-zrp-manifest-export-declarations.md`。
  备注：本切片不执行 export target token binding、不发射持久 export table、不消费 12-S7ZZQ 的 runtime remap sidecar，
  也不声明 full trim analyzer 完成。

- 2026-07-02 00:11:02 +08:00 · 12-S7ZZQ / 11-S7 runtime export member-token publication ·
  状态：12-S7 emitted metadata/export token publication 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、持久 cross-module export manifest/table writer、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：generated ABI 已发布的 `memberTokenRemaps/memberTokenRemapCount` 现在进入 module metadata runtime attach 路径；
  runtime 会在 loaded entry function `typedExportedSymbols` 上把 source `MEMBER_DEF` token 写回为 retained compacted token，
  使跨模块 import signature 匹配不再看见已被 pruning/compaction 淘汰的 source RID。
  RED/GREEN：RED 为 WSL GCC `zr_vm_metadata_runtime_query_test` 编译失败，新增断言引用缺失的
  `SZrMetadataRuntime.memberTokenRemapCount`；GREEN 后 `metadata_runtime.h` 增加 mirror 字段，`module.c` 在 attach 时执行
  member-token remap writeback。
  验证：WSL GCC/clang direct metadata runtime query 均 25/0；WSL GCC/clang metadata CTest 3/3；
  Windows MSVC Debug direct metadata runtime query 25/0、metadata CTest 3/3；WSL GCC/clang/MSVC Debug AOT CTest
  `aot_c_code_stripping|aot_c_zrp_metadata_export_token_remap|aot_c_descriptor_diagnostics` 均 3/3。
  产出：`tests/acceptance/2026-07-02-aot-12-s7zzq-runtime-export-member-token-publication.md`。
  备注：本切片只关闭 runtime typed export table 的 compacted member-token publication；不声明持久 manifest/table 文件重写、
  完整 provider target 解析、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 23:36:12 +08:00 · 12-S7ZZP / 11-S7 signature-rooted ModuleRef retention ·
  状态：12-S7 emitted zrp metadata pruning 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、cross-module export manifest/table target/provider binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：retained signature blob 中的 `ASSEMBLY_REF` payload 现在会作为 ModuleRef row 的 live root；pruning 先构建
  signature remap，再统计/复制 ModuleRef rows，并把同一 retained-signature context 传入 string-pool remap 与 signature
  rewrite，避免 signature-only ModuleRef name/version strings 被提前裁掉。
  RED/GREEN：RED 使用 retained signature blob 唯一引用 source ModuleRef RID2 的 fixture，旧逻辑只看 retained
  `TYPE_REF`/`MEMBER_REF` token-record root，WSL GCC pool pruning 失败为 `Expected TRUE Was FALSE`；GREEN 后 ModuleRef
  scanner 识别 `ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF` 并让 compacted AssemblyRef RID 写回 signature blob。
  验证：WSL GCC direct zrp metadata pruning 10/0、TypeSpec pruning 2/0、pool pruning 8/0、export token remap 8/0、source
  contracts 24/0，focused metadata CTest 4/4；WSL Clang 同 direct set 10/0、2/0、8/0、8/0、24/0，focused metadata
  CTest 4/4；Windows MSVC Debug direct 同 set 10/0、2/0、8/0、8/0、24/0，focused metadata CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzp-signature-rooted-module-ref-retention.md`。
  备注：本切片只关闭 retained signature blob `ASSEMBLY_REF` payload 作为唯一 root 时的 ModuleRef retention；不声明跨模块
  provider target 解析、真实 export manifest/table rewrite/publication、完整 metadata sweep、annotation policy 或 full
  trim analyzer 完成。

- 2026-07-01 22:51:15 +08:00 · 12-S7ZZO / 11-S7 signature MemberRef token rewrite ·
  状态：12-S7 emitted zrp metadata pruning 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、cross-module export manifest/table target/provider binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：retained signature blob 的 `MEMBER_REF` signature node 内嵌 token 会随 MethodDef/FieldDef row
  pruning/compaction 重写为 compacted local member RID；signature hash 在 rewrite 后重新计算。
  RED/GREEN：RED 为 WSL GCC zrp metadata pruning 1/10 失败，新增 fixture 中 live source `MEMBER_DEF` RID2 被保留并压缩，
  source RID1 被裁剪，旧 signature blob 仍输出 RID2（`Expected 50331649 Was 50331650`）；GREEN 后 blob 内 token 输出
  compacted RID1，并由 source contract 锁定 MemberRef remap 调用。
  验证：WSL GCC direct zrp metadata pruning 10/0、TypeSpec pruning 2/0、pool pruning 7/0、export token remap 8/0、source
  contracts 24/0，focused metadata CTest 4/4；WSL Clang 同 direct set 10/0、2/0、7/0、8/0、24/0，focused metadata
  CTest 4/4；Windows MSVC Debug direct 同 set 10/0、2/0、7/0、8/0、24/0，focused metadata CTest 4/4；focused code
  `git diff --check` 无 whitespace error。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzo-signature-member-ref-token-rewrite.md`。
  备注：本切片只关闭 retained signature blob `MEMBER_REF` local member-token compaction；不声明跨模块 provider target
  解析、真实 export manifest/table rewrite/publication、完整 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 22:34:08 +08:00 · 12-S7ZZN / 11-S7 signature AssemblyRef token rewrite ·
  状态：12-S7 emitted zrp metadata pruning 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、cross-module export manifest/table target/provider binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：retained signature blob rewrite context 现在携带 TypeSpec/ModuleRef rows；`ASSEMBLY_REF` signature node
  内嵌 token 会随 ModuleRef row pruning/compaction 重写为 compacted AssemblyRef RID。
  RED/GREEN：RED 为 WSL GCC pool pruning 1/7 失败，新增 fixture 中 live source `ASSEMBLY_REF` RID2 被 retained `TYPE_REF`
  引用且 ModuleRef RID1 被裁剪，旧 signature blob 仍输出 RID2（`Expected 67108865 Was 67108866`）；GREEN 后 blob 内 token
  输出 compacted RID1，并由 source contract 锁定 ModuleRef remap 调用。
  验证：WSL GCC direct zrp metadata pruning 9/0、TypeSpec pruning 2/0、pool pruning 7/0、export token remap 8/0、source
  contracts 24/0，focused metadata CTest 4/4；WSL Clang 同 direct set 9/0、2/0、7/0、8/0、24/0，focused metadata CTest
  4/4；Windows MSVC Debug direct 同 set 9/0、2/0、7/0、8/0、24/0，focused metadata CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzn-signature-assembly-ref-token-rewrite.md`。
  备注：本切片只关闭 retained signature blob `ASSEMBLY_REF` token compaction；不声明跨模块 provider target 解析、
  真实 export manifest/table rewrite/publication、完整 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 22:11:32 +08:00 · 12-S7ZZM / 11-S7 GenericParamConstraint TypeSpec root retention ·
  状态：12-S7 emitted zrp metadata pruning 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、cross-module export manifest/table target/provider binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：pruned zrp metadata 的 TypeSpec retained-row 判定现在把 retained GenericParamConstraint constraint
  `TYPE_SPEC` token 作为保留根；当 TypeSpec row 没有 retained token record 但仍被 retained constraint 引用时，row、
  signature blob 和 constraint token 都会随 pruning/compaction 保留下来并发布 compacted RID。
  RED/GREEN：RED 为 WSL GCC zrp metadata pruning 1/9 失败，新增 fixture 中 source `TYPE_SPEC` RID2 只被 retained
  GenericParamConstraint 引用，旧实现先删除 row 并使 pruning 返回 false（`Expected TRUE Was FALSE`）；GREEN 后 row 保留为
  compacted TypeSpec RID1，signature blob offset 也重写为 compacted pool offset。
  验证：WSL GCC direct zrp metadata pruning 9/0、TypeSpec pruning 2/0、pool pruning 6/0、export token remap 8/0、source
  contracts 24/0，focused metadata CTest 4/4；WSL Clang 同 direct set 9/0、2/0、6/0、8/0、24/0，focused metadata CTest
  4/4；Windows MSVC Debug direct 同 set 9/0、2/0、6/0、8/0、24/0，focused metadata CTest 4/4；focused code
  `git diff --check` 无 whitespace error。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzm-generic-param-constraint-typespec-root-retention.md`。
  备注：本切片只关闭 retained GenericParamConstraint-rooted TypeSpec row/signature retention；不声明跨模块 provider target
  解析、真实 export manifest/table rewrite/publication、完整 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 21:41:02 +08:00 · 12-S7ZZL / 11-S7 GenericParamConstraint TypeSpec token remap ·
  状态：12-S7 emitted zrp metadata pruning 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、cross-module export manifest/table target/provider binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：pruned zrp metadata 现在会在复制 retained GenericParamConstraint rows 时 remap `constraintTypeToken` 中的
  `TYPE_SPEC` token；source TypeSpec row 被删除导致 RID 压缩后，约束行会发布 compacted TypeSpec RID，而不是旧 source RID。
  RED/GREEN：RED 为 WSL GCC zrp metadata pruning 1/8 失败，新增 fixture 中 source `TYPE_SPEC` RID1 被裁剪、RID2 被保留，
  旧实现仍输出 constraint token RID2（`Expected 117440513 Was 117440514`）；GREEN 后 constraint token 改写为 compacted
  TypeSpec RID1，并由 source contract 锁定该 remap 调用。
  验证：WSL GCC direct zrp metadata pruning 8/0、TypeSpec pruning 2/0、pool pruning 6/0、source contracts 24/0，focused
  metadata CTest 4/4；WSL Clang 同 direct set 8/0、2/0、6/0、24/0，focused metadata CTest 4/4；Windows MSVC Debug direct
  同 set 8/0、2/0、6/0、24/0，focused metadata CTest 4/4；focused code `git diff --check` 无 whitespace error。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzl-generic-param-constraint-typespec-remap.md`。
  备注：本切片只关闭 retained GenericParamConstraint `TYPE_SPEC` token remap；不声明跨模块 provider target 解析、
  真实 export manifest/table rewrite/publication、完整 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 21:14:56 +08:00 · 12-S7ZZK / 11-S7 signature-token orphan rejection ·
  状态：12-S7 emitted zrp metadata pruning 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、cross-module export manifest/table target/provider binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：pruned zrp metadata 现在拒绝 retained MethodSpec row 或其他 retained signature-token consumer 引用未被
  retained signature token-record 发布的本地 `SIGNATURE` token；pruning 失败时 prepared embedded metadata 输出会被清空，
  不再泄漏原始 source blob/length。
  RED/GREEN：RED 为 WSL GCC zrp metadata pruning 1/7 失败，旧实现在缺失 signature token-record 时仍接受 retained
  MethodSpec row 的 `SIGNATURE` token；第二个 RED 证明失败路径没有清空输出 length 659；GREEN 后 orphan signature token
  触发 pruning false，输出 blob/length/remap state 均回到空状态。
  验证：WSL GCC direct zrp metadata pruning 7/0、typedef pruning 2/0、pool pruning 6/0、source contracts 24/0，focused
  metadata CTest 4/4；WSL Clang direct 同组 7/0、2/0、6/0、24/0，focused metadata CTest 4/4；Windows MSVC Debug direct
  同组 7/0、2/0、6/0、24/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzk-signature-token-orphan-rejection.md`。
  备注：本切片只关闭 retained local `SIGNATURE` orphan token rejection 与 pruning failure cleanup；不声明跨模块
  provider target 解析、真实 export manifest/table rewrite/publication、完整 metadata sweep、annotation policy 或 full
  trim analyzer 完成。

- 2026-07-01 20:55:12 +08:00 · 12-S7ZZJ / 11-S7 retained SIGNATURE token RID compaction ·
  状态：12-S7 emitted zrp metadata pruning 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、cross-module export manifest/table target/provider binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：pruned zrp metadata 现在按 retained signature token-record 顺序压缩本地 `SIGNATURE` token namespace；copy
  retained token records 时会改写 signature token 字段，copy retained MethodSpec rows 时会改写 row `token`，使 signature
  blob 压缩后的 token 身份也与 retained table count 对齐。该路径覆盖 FieldDef signature、MethodSpec signature、
  AssemblyRef signature 和 TypeRef signature token records。
  RED/GREEN：RED 为 WSL GCC zrp metadata pruning 2/6 失败，旧实现仍输出 source `SIGNATURE` RID9/RID11；GREEN 后
  retained signature records 和 MethodSpec row token 输出 compacted RID1..N，pool/module/type signature token references
  同步更新。
  验证：WSL GCC direct zrp metadata pruning 6/0、typedef pruning 2/0、pool pruning 6/0、source contracts 24/0，focused
  metadata CTest 4/4；WSL Clang direct 同组 6/0、2/0、6/0、24/0，focused metadata CTest 4/4；Windows MSVC Debug direct
  同组 6/0、2/0、6/0、24/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzj-signature-token-rid-compaction.md`。
  备注：本切片只关闭 retained local `SIGNATURE` token RID compaction；不声明跨模块 provider target 解析、
  真实 export manifest/table rewrite/publication、完整 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 20:17:40 +08:00 · 12-S7ZZI / 11-S7 member-token remap retained-count guard ·
  状态：12-S7 generated zrp metadata remap sidecar 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、cross-module export manifest/table target/provider binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：`backend_aot_c_zrp_remap_export_member_token()` 与 `backend_aot_c_zrp_member_token_remap_build()` 都验证
  caller-supplied `retainedMethodDefCount` 等于从 MethodDef rows/function table 重新计算出的实际 retained count，避免
  FieldDef target token 使用错误的 retained-method 偏移并跳过 compacted `MEMBER_DEF` RID。sidecar 检查在 entry capacity
  计算和 malloc 前执行。
  RED/GREEN：RED 覆盖 actual retained count 1 / supplied retained count 2 的 direct remap 和 sidecar fixtures，旧代码
  均返回 true 并可能生成 target RID 空洞，WSL GCC 均失败 `Expected FALSE Was TRUE`；GREEN 后 direct remap 返回 false 且
  保留原 token，builder 返回 false 且 `memberTokenRemapEntries`、`ownedMemberTokenRemapEntries` 和 `memberTokenRemapCount`
  保持清空。
  验证：WSL GCC direct export-token remap 8/0、source contracts 24/0、registered remap CTest 1/1；WSL Clang direct
  export-token remap 8/0、source contracts 24/0、registered remap CTest 1/1；Windows MSVC Debug direct export-token remap
  8/0、source contracts 24/0、registered remap CTest 1/1。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzi-member-token-remap-sidecar-retained-count-guard.md`。
  备注：本切片只收紧生成侧 member-token remap retained-count consistency；不声明跨模块 provider target 解析、
  真实 export manifest/table rewrite/publication、完整 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 19:57:11 +08:00 · 12-S7ZZH / 11-S7 member-token remap sidecar token-shape guard ·
  状态：12-S7 generated zrp metadata remap sidecar 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、cross-module export manifest/table target/provider binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：`backend_aot_c_zrp_member_token_remap_append()` 写入 sidecar entry 前校验 source/target token 都是
  非零 RID 的 `MEMBER_DEF`；`backend_aot_c_zrp_member_token_is_member_def()` 同步拒绝 RID 0。export-token remap test
  覆盖 retained MethodDef 带非 member source token 和 `MEMBER_DEF` RID 0 source token 的拒绝路径。
  RED/GREEN：RED 为 invalid source sidecar fixtures，旧 builder 返回 true 并可能生成非法 source token entries，WSL GCC
  分别失败 `Expected FALSE Was TRUE`；GREEN 后 builder 返回 false 且 `memberTokenRemapEntries`、`ownedMemberTokenRemapEntries`
  和 `memberTokenRemapCount` 保持清空。
  验证：WSL GCC direct export-token remap 6/0、source contracts 24/0、registered remap CTest 1/1；WSL Clang direct
  export-token remap 6/0、source contracts 24/0、registered remap CTest 1/1；Windows MSVC Debug direct export-token remap
  6/0、source contracts 24/0、registered remap CTest 1/1。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzh-member-token-remap-sidecar-token-shape-guard.md`。
  备注：本切片只收紧生成侧 member-token remap sidecar token 形态；不声明跨模块 provider target 解析、真实
  export manifest/table rewrite/publication、完整 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 19:43:02 +08:00 · 12-S7ZZG / 11-S7 member-token remap sidecar source duplicate guard ·
  状态：12-S7 generated zrp metadata remap sidecar 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、cross-module export manifest/table target/provider binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：`backend_aot_c_zrp_member_token_remap_append()` 写入新 entry 前扫描已写 entries，拒绝重复
  `sourceToken`，让 sidecar build 在发布 ambiguous generated remap table 前失败并清空 metadata remap 指针/count；
  export-token remap test 覆盖 retained MethodDef 与 FieldDef 共享同一 source member token。
  RED/GREEN：RED 为 duplicate source sidecar fixture，旧 builder 返回 true 并生成两条相同 source token entries，WSL GCC
  失败 `Expected FALSE Was TRUE`；GREEN 后 builder 返回 false 且 `memberTokenRemapEntries`、`ownedMemberTokenRemapEntries`
  和 `memberTokenRemapCount` 保持清空。
  验证：WSL GCC direct export-token remap 4/0、source contracts 24/0、registered remap CTest 1/1；WSL Clang direct
  export-token remap 4/0、source contracts 24/0、registered remap CTest 1/1；Windows MSVC Debug direct export-token remap
  4/0、source contracts 24/0、registered remap CTest 1/1；`git diff --check` exit 0，仅输出既有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzg-member-token-remap-sidecar-source-duplicate-guard.md`。
  备注：本切片只收紧生成侧 member-token remap sidecar source 唯一性；不声明跨模块 provider target 解析、真实
  export manifest/table rewrite/publication、完整 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 19:27:51 +08:00 · 12-S7ZZF / 11-S7 member-token remap ABI duplicate validation ·
  状态：12-S7 runtime ABI drift/descriptor validation 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、cross-module export manifest/table target/provider binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：root runtime 与 mirrored AOT runtime 现在在 remap entry 形态校验后扫描已发布
  `descriptor->memberTokenRemaps`，拒绝重复 `sourceToken` 或重复 `targetToken`，避免 remap table 出现一对多或多对一
  绑定歧义；descriptor diagnostics 新增 duplicate source 与 duplicate target 两个手写 descriptor shared-library 用例。
  RED/GREEN：RED 为两条合法 `MEMBER_DEF` entry 但 source 或 target 重复的 bad descriptor，旧 runtime 接受 descriptor
  并执行成功，WSL GCC 两个新用例均失败 `Expected FALSE Was TRUE`；GREEN 后 validation 报出
  `member token remap duplicate sourceToken index=1 previousIndex=0 sourceToken=0x03000001` 或
  `member token remap duplicate targetToken index=1 previousIndex=0 targetToken=0x03000001`。
  验证：WSL GCC direct descriptor diagnostics 5/0、source contracts 24/0、focused CTest 4/4；WSL Clang direct
  descriptor diagnostics 5/0、source contracts 24/0、focused CTest 4/4；Windows MSVC Debug descriptor diagnostics
  5/0/5 ignored、source contracts 24/0、focused CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzf-member-token-remap-duplicate-validation.md`。
  备注：本切片只收紧已发布 remap ABI 的唯一性约束；不声明跨模块 provider target 解析、真实 export manifest/table
  rewrite/publication、完整 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 19:04:27 +08:00 · 12-S7ZZE / 11-S7 member-token remap ABI entry validation ·
  状态：12-S7 runtime ABI drift/descriptor validation 支撑子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、
  attribute/annotation 抑制策略、cross-module export manifest/table target/provider binding、完整 metadata sweep/pruning
  和更完整的 runtime ABI drift/deopt coverage 仍待后续。
  完成项目：root runtime 与 mirrored AOT runtime 现在扫描 `descriptor->memberTokenRemaps`，拒绝 source/target token
  为 0、非 `MEMBER_DEF` table 或 RID 为 0 的 remap entry；descriptor diagnostics 新增坏 descriptor 动态库 fixture，
  用有效 embedded `.zro` 触达 validation，再断言错误包含 index/sourceToken/targetToken。
  RED/GREEN：RED 为 `tests/parser/test_aot_c_descriptor_diagnostics.c` 新增 source `0x02000001` -> target
  `0x03000001` 的 invalid remap entry，旧 runtime 接受 descriptor 并执行成功，WSL GCC 失败
  `Expected FALSE Was TRUE`；GREEN 后 runtime 在 descriptor validation 阶段拒绝并输出
  `member token remap entry invalid index=0 sourceToken=0x02000001 targetToken=0x03000001`。
  验证：WSL GCC direct descriptor diagnostics 3/0、source contracts 24/0、focused CTest 4/4；WSL Clang direct
  descriptor diagnostics 3/0、source contracts 24/0、focused CTest 4/4；Windows MSVC Debug descriptor diagnostics
  3/0/3 ignored、source contracts 24/0、focused CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zze-member-token-remap-entry-validation.md`。
  备注：本切片只收紧已发布 remap ABI 的 entry 形态；不声明跨模块 provider target 解析、真实 export manifest/table
  rewrite/publication、完整 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 18:43:00 +08:00 · 12-S7ZZD / 11-S7 member-token remap ABI publication ·
  状态：12-S7 cross-module export-token publication/rewrite 支撑子切片完成；完整 12-S7 仍未关闭，完整
  trim analyzer、attribute/annotation 抑制策略、cross-module export manifest/table target/provider binding、完整
  metadata sweep/pruning 和 runtime ABI drift deopt coverage 仍待后续。
  完成项目：公共 AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 12u`，新增 `SZrAotMemberTokenRemap`，并在
  `SZrAotCodeRegistration` 与 `ZrAotCompiledModule` 发布 `memberTokenRemaps/memberTokenRemapCount`。
  AOT C emitter 现在把 emitted zrp metadata pruning 的 retained MethodDef/FieldDef `sourceToken -> targetToken`
  sidecar 写成 `zr_aot_member_token_remaps[]`，并让 descriptor/codeRegistration 同时指向同一张表；生成 C
  同步输出 `code_stripping.memberTokenRemaps` 和逐项 source/target token marker。runtime loader 校验
  descriptor/codeRegistration remap 指针、数量和 null/count 形态一致。
  RED/GREEN：RED 为 `tests/parser/test_aot_c_code_stripping.c` 在 MethodDef RID2->RID1 pruning fixture 上要求
  generated C 发布 `SZrAotMemberTokenRemap` 表和两处 ABI 绑定，旧实现只改写 `zr_aot_method_tokens[]`，失败
  `Expected Non-NULL`；GREEN 后 generated C 输出 source `0x03000002` -> target `0x03000001` 的 ABI remap 表。
  验证：WSL GCC direct code stripping 10/0、source contracts 24/0、export-token remap 3/0，focused CTest 4/4；
  WSL Clang direct source contracts 24/0、focused CTest 4/4；Windows MSVC Debug direct source contracts 24/0、
  focused CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzd-member-token-remap-abi-publication.md`。
  备注：本切片只发布 pruning sidecar 到 generated C ABI surface；不声明跨模块 provider target 解析、真实 export
  manifest/table rewrite/publication、完整 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 18:09:50 +08:00 · 12-S7ZZC / 11-S7 signature blob embedded TypeDef token rewrite ·
  状态：12-S7 emitted zrp metadata pruning 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制策略、
  cross-module export manifest/table publication/rewrite、cross-module target/provider binding、完整 metadata sweep/pruning 和 runtime
  ABI drift deopt coverage 仍待后续。
  完成项目：retained signature blob pool 复制完成后，`backend_aot_c_zrp_metadata_signature.{h,c}` 现在递归扫描 retained `METHOD_SIG`、
  `FIELD_SIG` 与 standalone type-node signature blob，遇到 direct `TYPE_DEF` type node 时把 payload token 重写为 compacted TypeDef
  RID；重写后再次使用 zrp signature structural validator 校验，后续 token record / row hash 继续从 final emitted bytes 重新计算。
  `backend_aot_c_zrp_metadata_prune.c` 在 pool copy 后、MethodSpec `MEMBER_REF` method-token rewrite 前调用该步骤，因此 TypeDef
  RID compaction 与已有 MethodSpec rewrite 可以叠加。
  RED/GREEN：RED 为 `tests/parser/test_aot_c_zrp_metadata_typedef_pruning.c` 新增 FieldDef `FIELD_SIG(TYPE_DEF(... source RID2))`
  fixture，source RID1 被裁剪后旧实现仍复制 embedded RID2，WSL GCC 失败 `Expected 33554433 Was 33554434`；GREEN 后 embedded
  token 改写为 compacted `TYPE_DEF` RID1，signature hash 从最终 blob bytes 重新计算。
  验证：WSL GCC direct TypeDef pruning 2/0；WSL Clang direct TypeDef pruning 2/0；Windows MSVC Debug direct TypeDef pruning 2/0；
  focused CTest `zrp_metadata|aot_c_zrp_metadata|metadata_module_hash` 在 WSL GCC、WSL Clang、Windows MSVC Debug 均为 8/8。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzc-signature-typedef-token-rewrite.md`。
  备注：本切片只关闭 retained signature blob 内 direct `TYPE_DEF` token 的 compaction rewrite；不声明 `TYPE_REF`/跨模块 provider
  binding、cross-module export manifest/table rewrite/publication、完整 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 17:44:55 +08:00 · 12-S7ZZB / 11-S7 TypeDef RID compaction ·
  状态：12-S7 emitted zrp metadata pruning 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制策略、
  cross-module export manifest/table publication/rewrite、cross-module target/provider binding 和完整 metadata sweep/pruning 仍待后续。
  完成项目：after-trim metadata 现在允许 retained TypeDef rows 穿过 interior orphan holes 并发布 compacted `TYPE_DEF` RID；direct token
  record TypeDef 字段、MethodDef/FieldDef owner、GenericParam owner 和 GenericParamConstraint constraintTypeToken 均同步重写到 compacted
  TypeDef token；TypeDef string/signature pool retention 从 prefix 改为 retained-row 判定。
  RED/GREEN：RED 为 `tests/parser/test_aot_c_zrp_metadata_typedef_pruning.c` 新增 source TypeDef RID1 orphan、RID2 live fixture，旧实现
  identity-exit 且 pruned blob 为 NULL；GREEN 后 TypeDef rows 2→1，live row/token-record/MethodDef owner 全部发布 compacted RID1，orphan
  strings 被移除。
  验证：Windows MSVC Debug direct runs：TypeDef pruning 1/0、direct zrp pruning 6/0、TypeSpec pruning 2/0、pool pruning 6/0、source contracts
  24/0；WSL GCC/Clang 均完成配置、focused builds 与同五个 direct runs，结果均为 1/0、6/0、2/0、6/0、24/0；`git diff --check` exit 0，仅有
  既有 LF/CRLF warnings。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzb-typedef-rid-compaction.md`。
  备注：本切片只关闭 TypeDef interior RID compaction/direct rewrite；不声明 cross-module export manifest/table rewrite/publication、全量
  metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 17:10:03 +08:00 · 12-S7ZZA / 11-S7 TypeDef trailing orphan sweep ·
  状态：12-S7 emitted zrp metadata pruning 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation
  抑制策略、cross-module export manifest/table publication/rewrite、TypeDef RID compaction 和完整 metadata sweep/pruning 仍待后续。
  完成项目：emitted metadata pruning 现在在 after-trim blob 中删除最后一个 retained TypeDef root 之后的 TypeDef rows；
  retained root 覆盖 retained token record、retained MethodDef owner、FieldDef owner 与 TypeDef-owned GenericParam。TypeDef
  section count/byteLength、type name/namespace string slices 和 TypeDef signature blob slices 随 retained prefix 同步收缩。
  RED/GREEN：RED 为 `tests/parser/test_aot_c_zrp_metadata_pruning.c` 新增 live+trailing orphan TypeDef fixture，旧实现输出
  580-byte raw-copy blob 而非 510-byte compacted blob；GREEN 后 TypeDef rows 2→1，stringPool 只保留 live type/method 字符串。
  验证：Windows MSVC Debug direct runs：direct zrp pruning 6/0、pool pruning 6/0、TypeSpec pruning 2/0、export-token remap 3/0、
  zrp size deltas 2/0、code stripping 10/0、source contracts 24/0、frame setup contracts 1/0；Windows focused CTest 6/6；
  WSL GCC/Clang focused builds 均通过，focused CTest 各 6/6，并显式 direct 通过 source contracts 24/0、frame setup contracts 1/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zza-typedef-trailing-orphan-sweep.md`。
  备注：本切片只关闭 TypeDef trailing suffix sweep；不声明 TypeDef RID compaction、cross-module export manifest/table
  rewrite/publication、全量 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 16:40:57 +08:00 · 12-S7ZZ / 11-S7 ModuleRef orphan sweep ·
  状态：12-S7 emitted zrp metadata pruning 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation
  抑制策略、cross-module export manifest/table publication/rewrite 和完整 metadata sweep/pruning 仍待后续。
  完成项目：emitted metadata pruning 现在按 retained import ref roots 保留 ModuleRef rows；只有仍被 retained `TYPE_REF`/`MEMBER_REF`
  token record 引用的 `ASSEMBLY_REF` row 会复制到 after-trim blob。retained ModuleRef row token、token-record AssemblyRef 字段和
  module name/version string offsets 被重写到 compacted sections，self-only AssemblyRef/signature token records 与 orphan ModuleRef
  strings 不再扩大 after-trim metadata。
  RED/GREEN：RED 为 `tests/parser/test_aot_c_zrp_metadata_pool_pruning.c` 新增 orphan/live ModuleRef fixture，旧实现 raw-copy
  ModuleRef rows 并 identity-exit，失败为 pruned blob `Expected Non-NULL`；GREEN 后 tokenRecords 从 8 compact 到 6，ModuleRef rows
  从 2 compact 到 1，live AssemblyRef 从 source RID2 发布为 compacted RID1。
  验证：Windows MSVC Debug direct runs：pool pruning 6/0、direct zrp pruning 5/0、TypeSpec pruning 2/0、export-token remap 3/0、
  zrp size deltas 2/0、code stripping 10/0、source contracts 24/0、frame setup contracts 1/0；Windows focused CTest 6/6；
  WSL GCC/Clang focused CTest 各 6/6，并显式 direct 通过 source contracts 24/0、frame setup contracts 1/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zz-module-ref-orphan-sweep.md`。
  备注：本切片只关闭 ModuleRef orphan sweep 与 retained AssemblyRef RID compaction；不声明 cross-module export manifest/table
  rewrite/publication、全量 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 16:14:53 +08:00 · 12-S7ZY / 11-S7 TypeSpec RID compaction ·
  状态：12-S7 emitted zrp metadata pruning 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation
  抑制策略、cross-module export manifest/table publication/rewrite 和完整 metadata sweep/pruning 仍待后续。
  完成项目：emitted metadata pruning 删除早期 TypeSpec row 后，后续 retained TypeSpec row token 和 token-record TypeSpec 字段会
  重新映射到 compacted RID；signature-pool retained-token-record collection 也使用 TypeSpec-aware 判定，避免 dropped TypeSpec
  token record 继续扩大 after-trim signature pool。
  RED/GREEN：RED 为 retained TypeSpec source RID2 未 compact，focused test 失败 `Expected 117440513 Was 117440514`；GREEN 后
  TypeSpec row/token-record 输出 compacted RID1，contracts 同步锁定 helper。
  验证：Windows MSVC Debug direct runs：TypeSpec pruning 2/0、direct zrp pruning 5/0、pool pruning 5/0、export-token remap 3/0、
  zrp size deltas 2/0、code stripping 10/0、source contracts 24/0、frame setup contracts 1/0；Windows focused CTest 6/6；
  WSL GCC/Clang focused CTest 各 6/6，并显式 direct 通过 source contracts 24/0、frame setup contracts 1/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zy-typespec-rid-compaction.md`。
  备注：本切片只关闭 retained TypeSpec row/token-record RID compaction；不声明 cross-module export manifest/table
  rewrite/publication、全量 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 15:57:26 +08:00 · 12-S7ZX / 11-S7 published export member-token remap ·
  状态：12-S7 emitted zrp metadata pruning 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation
  抑制策略、cross-module export manifest/table publication/rewrite、TypeSpec RID compaction 和完整 metadata sweep/pruning 仍待后续。
  完成项目：emitted metadata pruning 现在把 retained MethodDef/FieldDef row 的 source member token 映射到 compacted target
  token；`zr_aot_method_tokens[]` 生成时消费该 sidecar，因此 code stripping 删除早期 MethodDef row 后，typed exported child method
  从 source `0x03000002u` 发布为 compacted `0x03000001u`，而不再保留 stale RID。
  RED/GREEN：RED 为 export-token remap helper 缺失导致 focused MSVC build 失败，并由 code-stripping generated-C fixture 锁定
  stale `0x03000002u` 输出；GREEN 后 generated C、sidecar helper、source contracts 和 frame setup contracts 通过。
  验证：Windows MSVC Debug direct runs：export-token remap 3/0、code stripping 10/0、source contracts 24/0、frame setup
  contracts 1/0、direct zrp pruning 5/0、TypeSpec pruning 1/0、pool pruning 5/0；Windows focused CTest 6/6；WSL GCC/Clang
  focused CTest 各 6/6，并显式 direct 通过 source contracts 24/0、frame setup contracts 1/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zx-published-export-member-token-remap.md`。
  备注：本切片只关闭 generated method-token table 的 retained member-token remap；不声明 cross-module export manifest/table
  rewrite/publication、TypeSpec RID compaction、全量 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 15:30:06 +08:00 · 12-S7ZW / 11-S7 TypeSpec orphan sweep ·
  状态：12-S7 emitted zrp metadata pruning 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation
  抑制策略、cross-module export-token publication/rewrite 和完整 metadata sweep/pruning 仍待后续。
  完成项目：AOT C emitted metadata pruning 现在按 retained `TYPE_SPEC` token record 保留 TypeSpec rows，不再 raw-copy
  已经失去 live token record 的 TypeSpec rows；`backend_aot_c_zrp_metadata_type_spec.{h,c}` 负责 retained-row 计数和 copy，
  并把 retained TypeSpec signature offsets/hash 重新映射到 compacted signatureBlobPool。signature-pool pruning 同步跳过
  orphan TypeSpec rows，因此 after-trim blob 不再保留无 row root 的 TypeSpec signature payload。
  RED/GREEN：RED 为 `tests/parser/test_aot_c_zrp_metadata_typespec_pruning.c` 期望 TypeSpec row count 为 0、
  signatureBlobPool byteLength 为 0、pruned length 为 488 bytes，但旧实现仍输出 517 bytes；GREEN 后 focused build/direct
  tests 通过。
  验证：Windows MSVC Debug direct runs：TypeSpec pruning 1/0、direct zrp pruning 5/0、pool pruning 5/0、source
  contracts 24/0、code stripping 10/0、zrp size deltas 2/0、export-token remap 2/0；WSL GCC/Clang 同组 build/direct
  run 通过；focused CTest 三套环境均为 6/6；`git diff --check` exit 0，仅有既有 line-ending warnings。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zw-typespec-orphan-sweep.md`。
  备注：本切片只关闭 token-record-rooted TypeSpec row/signature-pool orphan sweep；不声明 TypeSpec RID compaction、
  cross-module export-token rewrite、全量 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 14:51:47 +08:00 · 12-S7ZV / 11-S7 FieldDef default-value constant-pool remap ·
  状态：12-S7 emitted zrp metadata pruning 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation
  抑制策略、cross-module export-token publication/rewrite 和完整 metadata sweep/pruning 仍待后续。
  完成项目：`.zrp` v3 FieldDef row 现在携带 default-value constant-pool offset/length；`backend_aot_c_zrp_metadata_constant_pool.{h,c}`
  从 retained FieldDef rows 收集 default-value constant slices，生成 compacted constantPool，重写 retained FieldDef
  offset/length，并让 orphan constant payload 在 after-trim blob 中继续消失。source contracts 锁定新模块边界，CLI
  dump/version 期望同步到 metadata version 3。
  RED/GREEN：RED 为 metadata format 与 pool-pruning focused 测试引入新 FieldDef fields 后 MSVC build 编译失败；GREEN 后
  focused build/direct tests 通过。
  验证：Windows MSVC Debug direct runs：metadata format 12/0、pool pruning 5/0、direct zrp pruning 5/0、source
  contracts 24/0、code stripping 10/0、zrp size deltas 2/0、CLI dump/version 与 CLI args 通过；WSL GCC/Clang 同组
  build/direct run 通过；focused CTest 三套环境均为 7/7；`git diff --check` exit 0，仅有既有 line-ending warnings。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zv-field-default-constant-pool-remap.md`。
  备注：本切片只关闭 FieldDef default-value retained constant-pool slice remap；不声明 cross-module export-token rewrite、
  全量 metadata sweep、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 14:05:25 +08:00 · 12-S5 support / 10-S4Z28 / 11-S4BN FieldInfo nested primitive POD storage-width path matrix ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 现在有 nested primitive POD path
  int8/int16/int64/uint8/uint16/uint64/float32 storage-width matrix support coverage；12-S5 的完整 FieldInfo 行为和完整
  metadata sweep 仍未关闭。
  完成项目：retained FieldDef/layout metadata 在同一 public FieldInfo object adapter 下可驱动 storage-width raw child path
  read/write；该 coverage 复用 S4Z25/S4Z26/S4Z27 的 traversal、leaf guard、representative matrix fixture 和 shared primitive
  POD guard，不改变裁剪 root 规则、zrp metadata pruning 规则或 metadata policy。
  RED/GREEN：coverage GREEN；新增 storage-width matrix 后 Windows MSVC Debug focused `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均构建
  `zr_vm_reflection_token_resolve_test`、`zr_vm_metadata_runtime_query_test`、`zr_vm_metadata_runtime_typespec_layout_test`；
  focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z28-fieldinfo-nested-primitive-pod-width-matrix.md`。
  备注：本记录只说明 12-S5 retained metadata 的运行期 consumer coverage 继续前进；完整 `@dynamically_accessed` dataflow、
  cross-module dependency/provider、type/member sweep policy、signature-derived binding 和 full FieldInfo methods 仍未完成。

- 2026-07-01 14:00:03 +08:00 · 12-S5 support / 10-S4Z27 / 11-S4BM FieldInfo nested primitive POD representative path matrix ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 现在有 nested primitive POD path
  bool、uint32、double representative matrix support coverage；12-S5 的完整 FieldInfo 行为、完整 primitive matrix 和完整
  metadata sweep 仍未关闭。
  完成项目：retained FieldDef/layout metadata 在同一 public FieldInfo object adapter 下可驱动 bool、uint32 和 double raw
  child path read/write；该 coverage 复用 S4Z25/S4Z26 的 traversal、leaf guard 和 shared primitive POD guard，不改变裁剪 root
  规则、zrp metadata pruning 规则或 metadata policy。
  RED/GREEN：coverage GREEN；新增矩阵覆盖后 Windows MSVC Debug focused `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均构建
  `zr_vm_reflection_token_resolve_test`、`zr_vm_metadata_runtime_query_test`、`zr_vm_metadata_runtime_typespec_layout_test`；
  focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z27-fieldinfo-nested-primitive-pod-path-matrix.md`。
  备注：本记录只说明 12-S5 retained metadata 的运行期 consumer coverage 继续前进；完整 `@dynamically_accessed` dataflow、
  cross-module dependency/provider、type/member sweep policy、完整 primitive raw child matrix 和 full FieldInfo methods 仍未完成。

- 2026-07-01 13:51:37 +08:00 · 12-S5 support / 10-S4Z26 / 11-S4BL FieldInfo nested primitive POD leaf layout identity guard ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 现在会拒绝带 registered child layout identity 的
  nested primitive POD raw leaf；12-S5 的完整 FieldInfo 行为、完整 primitive matrix 和完整 metadata sweep 仍未关闭。
  完成项目：retained `SZrTypeLayoutField.typeLayoutIndex` 在 nested primitive path leaf 上必须为
  `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`，否则 public FieldInfo object adapter 的 read/write 均返回 false 且保留原始字节。
  该路径不改变裁剪 root 规则、zrp metadata pruning 规则或 metadata policy。
  RED/GREEN：RED 为 Windows MSVC Debug focused `reflection_token_resolve` 30 tests / 1 failure，
  `Expected FALSE Was TRUE`；GREEN 后 Windows focused `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均构建
  `zr_vm_reflection_token_resolve_test`、`zr_vm_metadata_runtime_query_test`、`zr_vm_metadata_runtime_typespec_layout_test`；
  focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z26-fieldinfo-nested-primitive-pod-leaf-layout-guard.md`。
  备注：本记录只说明 12-S5 retained metadata 的运行期 consumer guard 继续前进；完整 `@dynamically_accessed` dataflow、
  cross-module dependency/provider、type/member sweep policy、完整 primitive raw child matrix 和 full FieldInfo methods 仍未完成。

- 2026-07-01 13:37:45 +08:00 · 12-S5 support / 10-S4Z25 / 11-S4BK FieldInfo nested inline primitive POD path read/write ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 现在有 retained multi-level nested
  primitive POD raw child path read/write support contract；12-S5 的完整 FieldInfo 行为、完整 primitive matrix 和完整 metadata
  sweep 仍未关闭。
  完成项目：保留下来的 FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)`、registry-backed outer field type layout、中间
  `SZrTypeLayoutField.typeLayoutIndex` metadata 和 leaf raw child byte-size 可驱动 public `FieldInfo` object adapter 逐级解析
  inline aggregate child layout，并通过 shared primitive POD guard 读取/写入代表性 INT32 raw child。该路径不改变裁剪 root 规则、
  zrp metadata pruning 规则或 metadata policy。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（nested primitive path object API 缺失，C4013 + LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 30/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z25-fieldinfo-nested-primitive-pod-path-read-write.md`。
  备注：本记录只说明 12-S5 retained metadata 的运行期 consumer 能力继续前进；完整 `@dynamically_accessed` dataflow、
  cross-module dependency/provider、type/member sweep policy、完整 primitive raw child matrix 和 full FieldInfo methods 仍未完成。

- 2026-07-01 13:14:27 +08:00 · 12-S5 support / 10-S4Z24 / 11-S4BJ FieldInfo nested inline VALUE_SLOT path write ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 现在有 retained multi-level nested
  `VALUE_SLOT` path write support contract；12-S5 的完整 FieldInfo 行为、primitive raw child marshaling 和完整 metadata
  sweep 仍未关闭。
  完成项目：保留下来的 FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)`、registry-backed outer field type layout 和中间
  `SZrTypeLayoutField.typeLayoutIndex` metadata 可驱动 public `FieldInfo` object adapter 逐级解析 inline aggregate child
  layout，并在 leaf `VALUE_SLOT` child 上写入 `SZrTypeValue`。该路径不改变裁剪 root 规则、zrp metadata pruning 规则或
  metadata policy；replacement/drop 继续由 `ZrCore_Value_Copy()` 管理。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 29/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 29/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z24-fieldinfo-nested-value-slot-path-write.md`。
  备注：本记录只说明 12-S5 retained metadata 的运行期 consumer 能力继续前进；完整 `@dynamically_accessed` dataflow、
  cross-module dependency/provider、type/member sweep policy、primitive raw child marshaling 和 full FieldInfo methods 仍未完成。

- 2026-07-01 13:00:18 +08:00 · 12-S5 support / 10-S4Z23 / 11-S4BI FieldInfo nested inline VALUE_SLOT path read ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 现在有 retained multi-level nested
  `VALUE_SLOT` path read support contract；12-S5 的完整 nested field marshaling、完整 FieldInfo 行为和完整 metadata sweep
  仍未关闭。
  完成项目：保留下来的 FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)`、registry-backed outer field type layout 和中间
  `SZrTypeLayoutField.typeLayoutIndex` metadata 可驱动 public `FieldInfo` object adapter 逐级解析 inline aggregate child
  layout，并在 leaf `VALUE_SLOT` child 上复制 `SZrTypeValue`。该路径不改变裁剪 root 规则、zrp metadata pruning 规则或
  metadata policy。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 28/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 28/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z23-fieldinfo-nested-value-slot-path-read.md`。
  备注：本记录只说明 12-S5 retained metadata 的运行期 consumer 能力继续前进；完整 `@dynamically_accessed` dataflow、
  cross-module dependency/provider、type/member sweep policy、nested path write 和 primitive raw child marshaling 仍未完成。

- 2026-07-01 12:40:09 +08:00 · 12-S5 support / 10-S4Z22 / 11-S4BH FieldInfo nested inline VALUE_SLOT write ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 现在有 retained nested `VALUE_SLOT`
  child write support contract；12-S5 的完整 nested field marshaling、完整 FieldInfo 行为和完整 metadata sweep 仍未关闭。
  完成项目：保留下来的 FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)`、registry-backed field type layout 和 nested
  `SZrTypeLayoutField` metadata 可驱动 public `FieldInfo` object adapter 按 layout index 写入 inline aggregate 内的
  nested `VALUE_SLOT` child，并用 `ZrCore_Value_Copy()` 完成 replacement/drop。该路径不改变裁剪 root 规则、
  zrp metadata pruning 规则或 metadata policy。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_WriteFieldInfoObjectNestedValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 27/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 27/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z22-fieldinfo-nested-value-slot-write.md`。
  备注：本切片只证明 retained metadata 的 nested value-slot child write 最小边界；不声明 full recursive nested
  marshaling、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 12:24:56 +08:00 · 12-S5 support / 10-S4Z21 / 11-S4BG FieldInfo nested inline VALUE_SLOT read ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 现在有 retained nested `VALUE_SLOT`
  child read support contract；12-S5 的完整 nested field marshaling、完整 FieldInfo 行为和完整 metadata sweep 仍未关闭。
  完成项目：保留下来的 FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)`、registry-backed field type layout 和 nested
  `SZrTypeLayoutField` metadata 可驱动 public `FieldInfo` object adapter 按 layout index 读取 inline aggregate 内的
  nested `VALUE_SLOT` child，并用 `ZrCore_Value_Copy()` 返回 `SZrTypeValue`。该路径不改变裁剪 root 规则、
  zrp metadata pruning 规则或 metadata policy。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_ReadFieldInfoObjectNestedValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 26/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 26/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z21-fieldinfo-nested-value-slot-read.md`。
  备注：本切片只证明 retained metadata 的 nested value-slot child read 最小边界；不声明 full recursive nested
  marshaling、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 12:09:54 +08:00 · 12-S5 support / 10-S4Z20 / 11-S4BF FieldInfo inline aggregate replacement/drop coverage ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 现在有 retained replacement/drop
  coverage；12-S5 的完整 nested field marshaling、完整 FieldInfo 行为和完整 metadata sweep 仍未关闭。
  完成项目：保留下来的 FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)`、registry-backed field type layout 和 field-copy
  metadata 可驱动 public `FieldInfo` object write 覆盖 nested owned value replacement：旧 owner 被释放，destination
  slot 被替换并归一 ownership metadata。该路径不改变裁剪 root 规则、zrp metadata pruning 规则或 metadata policy。
  RED/GREEN：coverage GREEN；新增覆盖后 Windows focused `reflection_token_resolve` 直接通过 25/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 25/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z20-fieldinfo-inline-aggregate-replacement-drop-write.md`。
  备注：本切片只证明 retained metadata 的 inline aggregate replacement/drop write coverage；不声明 recursive
  nested field marshaling、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 11:52:49 +08:00 · 12-S5 support / 10-S4Z19 / 11-S4BE FieldInfo inline aggregate field-copy borrowed-source write ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 现在有 layout-aware inline aggregate write
  support contract；12-S5 的完整 nested field marshaling、完整 FieldInfo 行为和完整 metadata sweep 仍未关闭。
  完成项目：保留下来的 FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)`、registry-backed field type layout 和 type-layout copy metadata
  可在运行期驱动 public `FieldInfo` object write；当目标为 non-GC/non-ownership inline struct/union 字段且 source 是非空
  native pointer 时，可通过 `ZrCore_TypeLayout_CopyInline()` 处理 raw-copy 与 `FIELD_COPY` layout。该路径不改变裁剪 root
  规则、zrp metadata pruning 规则或 metadata policy。
  RED/GREEN：RED 为 Windows focused `reflection_token_resolve` 将 non-blittable field-copy source 写入期望改为成功后
  失败 1/24；GREEN 后 `reflection_token_resolve` 24/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 24/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z19-fieldinfo-inline-aggregate-field-copy-write.md`。
  备注：本切片只证明 retained metadata 的 inline aggregate field-copy borrowed-source write consumer；不声明 recursive
  nested field marshaling、destination ownership replacement/drop、`@dynamically_accessed` dataflow、warning policy、
  DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 11:34:10 +08:00 · 12-S5 support / 10-S4Z18 / 11-S4BD FieldInfo inline aggregate borrowed-source write ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 现在有受限 inline aggregate write
  first-contract；12-S5 的完整 nested field marshaling、完整 FieldInfo 行为和完整 metadata sweep 仍未关闭。
  完成项目：保留下来的 FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)` 与 registry-backed field type layout 可在运行期驱动
  public `FieldInfo` object write；当目标为 non-GC/non-ownership、blittable inline struct/union 字段且 source 是非空
  native pointer 时，按 field byte size 复制 bytes。该路径不改变裁剪 root 规则、zrp metadata pruning 规则或 metadata policy。
  RED/GREEN：RED 为 Windows focused `reflection_token_resolve` 在 inline struct fixture 增加 native-pointer source
  写入期望后失败 1/24；GREEN 后 `reflection_token_resolve` 24/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 24/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z18-fieldinfo-inline-aggregate-borrowed-source-write.md`。
  备注：本切片只证明 retained metadata 的 blittable aggregate borrowed-source write consumer；不声明 recursive
  nested field marshaling、non-blittable struct semantics、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 11:26:41 +08:00 · 12-S5 support / 10-S4Z17 / 11-S4BC FieldInfo inline struct borrowed view ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 现在有 inline aggregate read
  first-contract；12-S5 的完整 nested field marshaling、完整 FieldInfo 行为和完整 metadata sweep 仍未关闭。
  完成项目：保留下来的 FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)` 与 registry-backed field type layout 可在运行期驱动
  public `FieldInfo` object read，返回 non-GC/non-ownership inline struct/union 字段的 borrowed native-pointer view。
  该路径不改变裁剪 root 规则、zrp metadata pruning 规则或 metadata policy。
  RED/GREEN：RED 为 Windows focused `reflection_token_resolve` 新增 inline struct object fixture 后失败 1/24；
  GREEN 后 `reflection_token_resolve` 24/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 24/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z17-fieldinfo-inline-struct-borrowed-view.md`。
  备注：本切片只证明 retained metadata 的 inline aggregate borrowed-view read consumer；不声明 `SetValue`、
  recursive nested field marshaling、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 11:15:23 +08:00 · 12-S5 support / 10-S4Z16 / 11-S4BB FieldInfo object primitive POD coverage ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 现在有 object-level primitive POD
  raw inline 字段读写覆盖；12-S5 的 nested field marshaling、完整 FieldInfo 行为和完整 metadata sweep 仍未关闭。
  完成项目：focused fixture 证明保留下来的 FieldDef layout/signature metadata 可通过 public `FieldInfo` object
  驱动 primitive POD int32 raw inline read/write，包含 type-mismatch write reject 后 raw bytes 保持。该切片不改变
  裁剪 root 规则或 zrp metadata pruning 规则。
  RED/GREEN：coverage GREEN；现有 object adapter 已委托 token-driven primitive POD path，
  Windows focused `reflection_token_resolve` 23/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 23/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z16-fieldinfo-object-primitive-pod.md`。
  备注：本切片不改变裁剪 root 规则；不声明完整 `FieldInfo.SetValue` method surface、nested field marshaling、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 11:06:55 +08:00 · 12-S5 support / 10-S4Z15 / 11-S4BA FieldInfo object value write adapter ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout metadata 现在可通过 public `FieldInfo` object
  直接驱动 object-level inline value write adapter；12-S5 的 nested field marshaling、完整 FieldInfo 行为和
  完整 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_WriteFieldInfoObjectValue()` 读取 `FieldInfo.metadataRuntime` 与 `metadataToken`
  后委托 token-driven write path，让保留下来的 FieldDef/layout/signature metadata 不只支持 token API，也支持
  object-level write adapter。该切片不改变裁剪 root 规则或 zrp metadata pruning 规则。
  RED/GREEN：RED 为 Windows MSVC Debug build 链接失败，缺少
  `ZrCore_Reflection_WriteFieldInfoObjectValue`；GREEN 后 Windows focused `reflection_token_resolve` 22/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 22/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z15-fieldinfo-object-value-write.md`。
  备注：本切片不改变裁剪 root 规则；不声明完整 `FieldInfo.SetValue` method surface、nested field marshaling、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 10:52:48 +08:00 · 12-S5 support / 10-S4Z14 / 11-S4AZ FieldInfo object value read adapter ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout metadata 现在可通过 public `FieldInfo` object
  直接驱动 read-only inline value read adapter；12-S5 的 object-level write、nested field marshaling、完整
  FieldInfo 行为和完整 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_ReadFieldInfoObjectValue()` 读取 `FieldInfo.metadataRuntime` 与 `metadataToken`
  后委托 token-driven read path，让保留下来的 FieldDef/layout/signature metadata 不只支持 token API，也支持
  object-level read adapter。该切片不改变裁剪 root 规则或 zrp metadata pruning 规则。
  RED/GREEN：RED 为 Windows MSVC Debug build 链接失败，缺少
  `ZrCore_Reflection_ReadFieldInfoObjectValue`；GREEN 后 Windows focused `reflection_token_resolve` 21/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 21/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z14-fieldinfo-object-value-read.md`。
  备注：本切片不改变裁剪 root 规则；不声明 `FieldInfo.SetValue`、nested field marshaling、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 10:36:14 +08:00 · 12-S5 support / 10-S4Z13 / 11-S4AY FieldInfo metadata runtime carrier ·
  状态：annotation-root 或 manifest 保留后的 FieldDef metadata 运行期 public `FieldInfo` object 现在携带
  `metadataRuntime` same-runtime identity carrier；12-S5 的 dataflow、warning policy、cross-module dependency/provider
  compatibility 和 sweep 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 在 FieldInfo object 上写入 attached
  `SZrMetadataRuntime *` native pointer，作为后续 object-level 字段读写复用 token context 的对象内承载。该变更不新增裁剪
  root，不改变 dynamic dependency 或 metadata pruning 策略。
  RED/GREEN：RED 为 focused `FieldInfo` object fixture 新增 `metadataRuntime` native-pointer 断言后，
  Windows MSVC Debug `reflection_token_resolve` 20 个测试失败 1 个；GREEN 后同一 focused run 20/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 20/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z13-fieldinfo-metadata-runtime-carrier.md`。
  备注：本切片不改变裁剪 root 规则；不声明 object-level FieldInfo methods、nested field marshaling、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion、trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 10:21:28 +08:00 · 12-S5 support / 10-S4Z12 / 11-S4AX FieldInfo primitive POD float32 precision guard ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 可被 runtime reflection 用于
  precision/no-loss-safe primitive POD float32 raw inline 字段写入；12-S5 的 dataflow、warning policy、
  cross-module dependency/provider compatibility 和 sweep 策略仍未关闭。
  完成项目：runtime reflection 新增 float32 precision/no-loss guard，拒绝不能经 `TZrFloat32` 无损 round-trip 的
  double source raw float32 写入，并在失败时保留原 raw bytes。该变更不新增裁剪 root，不改变 dynamic dependency
  field-token root 规则，也不改变 metadata pruning/remap；只消费已保留的 FieldDef layout/signature metadata。
  RED/GREEN：RED 为新增 precision-loss raw write coverage 后 Windows MSVC Debug focused run 失败 1/20；
  GREEN 后同一 focused run 20/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 20/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z12-fieldinfo-primitive-pod-float32-precision-guard.md`。
  备注：本切片不改变裁剪 root 规则；不声明 nested field marshaling、`@dynamically_accessed` dataflow、
  warning policy、DESCRIPTION promotion、trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 10:11:20 +08:00 · 12-S5 support / 10-S4Z11 / 11-S4AW FieldInfo primitive POD float32 NaN guard ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 可被 runtime reflection 用于 NaN-safe
  primitive POD float32 raw inline 字段写入；12-S5 的 dataflow、warning policy、cross-module dependency/provider compatibility
  和 sweep 策略仍未关闭。
  完成项目：runtime reflection 新增 float32 NaN guard，拒绝 NaN double source 的 raw float32 写入，并在失败时保留原
  raw bytes。该变更不新增裁剪 root，不改变 dynamic dependency field-token root 规则，也不改变 metadata pruning/remap；
  只消费已保留的 FieldDef layout/signature metadata。
  RED/GREEN：RED 为新增 NaN raw write coverage 后 Windows MSVC Debug focused run 失败 1/19；
  GREEN 后同一 focused run 19/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 19/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z11-fieldinfo-primitive-pod-float32-nan-guard.md`。
  备注：本切片不改变裁剪 root 规则；不声明 float32 precision semantics、nested field marshaling、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion、trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 09:58:56 +08:00 · 12-S5 support / 10-S4Z10 / 11-S4AV FieldInfo primitive POD float32 range guard ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 可被 runtime reflection 用于 range-safe
  primitive POD float32 raw inline 字段写入；12-S5 的 dataflow、warning policy、cross-module dependency/provider compatibility
  和 sweep 策略仍未关闭。
  完成项目：runtime reflection 新增 float32 storage range guard，拒绝 `|double source| > FLT_MAX` 的 raw float32 写入，并在失败时保留原
  raw bytes。该变更不新增裁剪 root，不改变 dynamic dependency field-token root 规则，也不改变 metadata pruning/remap；
  它只扩大 retained metadata 的运行期消费安全性。
  RED/GREEN：RED 为 focused reflection token resolver 新增 float32 越界写入后 Windows MSVC Debug 失败 1/18；GREEN 后同一 focused
  run 通过 18/0。验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 18/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z10-fieldinfo-primitive-pod-float32-range-guard.md`。
  备注：本切片不改变裁剪 root 规则；不声明 float32 NaN/precision semantics、nested field marshaling、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion、trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 09:49:10 +08:00 · 12-S5 support / 10-S4Z9 / 11-S4AU FieldInfo primitive POD integer range guard ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 可被 runtime reflection 用于 range-safe
  primitive POD integer raw inline 字段写入；12-S5 的 dataflow、warning policy、cross-module dependency/provider compatibility
  和 sweep 策略仍未关闭。
  完成项目：runtime reflection 新增 integer range guard，拒绝 signed/unsigned source 与 target primitive storage width 不兼容的
  写入，并在失败时保留原 raw bytes。该变更不新增裁剪 root，不改变 dynamic dependency field-token root 规则，也不改变
  metadata pruning/remap；它只扩大 retained metadata 的运行期消费安全性。
  RED/GREEN：RED 为 focused reflection token resolver 新增越界写入后 Windows MSVC Debug 失败 1/17；GREEN 后同一 focused
  run 通过 17/0。验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 17/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z9-fieldinfo-primitive-pod-integer-range-guard.md`。
  备注：本切片不改变裁剪 root 规则；不声明 float32 narrowing/finite semantics、nested field marshaling、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion、trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 09:35:02 +08:00 · 12-S5 support / 10-S4Z8 / 11-S4AT FieldInfo primitive POD width matrix ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 可被 runtime reflection 用于 storage-width
  primitive POD raw inline 字段读写矩阵；12-S5 的 dataflow、warning policy、cross-module dependency/provider compatibility
  和 sweep 策略仍未关闭。
  完成项目：focused reflection token resolver 新增 int8/int16/int64/uint8/uint16/uint64/float32 raw inline FieldDef value
  width matrix，证明保留后的 FieldDef row、owner/field layout metadata 与 `FIELD_SIG(PRIMITIVE(...))` 足以驱动 supported
  primitive C storage widths。该变更不新增裁剪 root，不改变 dynamic dependency field-token root 规则，也不改变 metadata
  pruning/remap；它只扩大 retained metadata 的运行期消费覆盖。
  RED/GREEN：coverage GREEN，新增宽度矩阵后 Windows MSVC Debug focused `reflection_token_resolve` 通过 16/0；测试 helper
  的 unreachable-code warning 修正后，无生产修复。验证：WSL GCC/Clang/Windows MSVC Debug 均通过
  `reflection_token_resolve` 16/0、`metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z8-fieldinfo-primitive-pod-width-matrix.md`。
  备注：本切片不改变裁剪 root 规则；不声明 numeric overflow/range semantics、nested field marshaling、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion、trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 05:06:21 +08:00 · 12-S5 support / 10-S4Z7 / 11-S4AS FieldInfo primitive POD representative matrix ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 可被 runtime reflection 用于 representative
  primitive POD raw inline 字段读写矩阵；12-S5 的 dataflow、warning policy、cross-module dependency/provider compatibility
  和 sweep 策略仍未关闭。
  完成项目：focused reflection token resolver 新增 bool/uint32/double raw inline FieldDef value matrix，证明保留后的
  FieldDef row、owner/field layout metadata 与 `FIELD_SIG(PRIMITIVE(...))` 足以驱动 signed/unsigned/bool/float-family
  representative scalar read/write boundary。该变更不新增裁剪 root，不改变 dynamic dependency field-token root 规则，也不改变
  metadata pruning/remap；它只扩大 retained metadata 的运行期消费覆盖。
  RED/GREEN：coverage GREEN，新增矩阵后 Windows MSVC Debug focused `reflection_token_resolve` 直接通过 15/0，无生产修复。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 15/0、`metadata_runtime_query` 24/0、
  `metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z7-fieldinfo-primitive-pod-matrix.md`。
  备注：本切片不改变裁剪 root 规则；不声明全量 primitive width/overflow matrix、nested field marshaling、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion、trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 04:45:19 +08:00 · 12-S5 support / 10-S4Z6 / 11-S4AR FieldInfo primitive POD read/write boundary ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout/signature metadata 可被 runtime reflection 用于 primitive POD
  raw inline 字段读写；12-S5 的 dataflow、warning policy、cross-module dependency/provider compatibility 和 sweep 策略仍未关闭。
  完成项目：`reflection_field_value.c` 通过 retained FieldDef token metadata 解析 owner layout、field layout id 与 offset，
  通过 retained field signature metadata 读取 `FIELD_SIG(PRIMITIVE(...))`，并对调用方 inline storage 执行 range、flag
  和 exact byte-size 校验后读写 raw scalar。该变更不新增裁剪 root，不改变 dynamic dependency field-token root 规则，也不改变
  metadata pruning/remap；它只扩大被保留 FieldDef/layout/signature metadata 的运行期消费边界。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 primitive POD read/write API 路径后运行失败；
  GREEN 后 retained FieldDef token 可从 raw int32 inline storage 读取 `-12345`，拒绝 bool 写入，写入 `2048` 并读回。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 14/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z6-fieldinfo-primitive-pod-read-write.md`。
  备注：本切片不改变裁剪 root 规则；不声明完整 primitive variant matrix、nested field marshaling、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion、trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 04:12:42 +08:00 · 12-S5 support / 10-S4Z5 / 11-S4AQ FieldInfo value-slot write boundary ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout metadata 可被 runtime reflection 用于写入 `VALUE_SLOT`
  字段值；12-S5 的 dataflow、warning policy、cross-module dependency/provider compatibility 和 sweep 策略仍未关闭。
  完成项目：`ZrCore_Reflection_WriteFieldInfoTokenValue()` 通过 retained FieldDef token metadata 解析 owner layout、
  field layout id 与 offset，然后对调用方传入的 inline storage 执行共享的 `VALUE_SLOT` range/flag 校验并复制
  `SZrTypeValue`。该变更不新增裁剪 root，不改变 dynamic dependency field-token root 规则，也不改变 metadata
  pruning/remap。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 FieldInfo value-slot write API 调用后构建/链接失败；
  GREEN 后 retained FieldDef token object 的配套 write boundary 能把 int `11` 写为 int `271828` 并读回，且拒绝
  null/invalid/short-storage failure paths。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 13/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z5-fieldinfo-value-slot-write.md`。
  备注：本切片不改变裁剪 root 规则；不声明 POD/nested field marshaling、`@dynamically_accessed` dataflow、
  warning policy、DESCRIPTION promotion、trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 03:49:06 +08:00 · 12-S5 support / 10-S4Z4 / 11-S4AP FieldInfo value-slot read boundary ·
  状态：annotation-root 或 manifest 保留后的 FieldDef/layout metadata 可被 runtime reflection 用于只读 `VALUE_SLOT`
  字段值访问；12-S5 的 dataflow、warning policy、cross-module dependency/provider compatibility 和 sweep 策略仍未关闭。
  完成项目：`ZrCore_Reflection_ReadFieldInfoTokenValue()` 通过 retained FieldDef token metadata 解析 owner layout、
  field layout id 与 offset，然后对调用方传入的 inline storage 执行 `VALUE_SLOT` range/flag 校验并复制 `SZrTypeValue`。
  该变更不新增裁剪 root，不改变 dynamic dependency field-token root 规则，也不改变 metadata pruning/remap。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 FieldInfo value-slot read API 调用后构建/链接失败；
  GREEN 后 retained FieldDef token object 的配套 read boundary 能读取 int `314159`，并拒绝 null/invalid/short-storage
  failure paths。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 12/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z4-fieldinfo-value-slot-read.md`。
  备注：本切片不改变裁剪 root 规则；不声明 field write、POD/nested field marshaling、`@dynamically_accessed`
  dataflow、warning policy、DESCRIPTION promotion、trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 03:17:49 +08:00 · 12-S5 support / 10-S4Z3 / 11-S4AO FieldInfo recursive signature type-node type literal carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` recursive signature type-node type literal
  consumer 子切片完成；12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在可把 retained FieldDef metadata 中 validated generic
  signature node 的 semantic `typeName` 继续物化为 nested `type` type literal object；base node 与 primitive/TypeDef/TypeRef
  child nodes 都沿用同一 public `type` 字段惯例。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 nested node `type` object 断言后运行失败 1/11；
  GREEN 后 FieldDef token object 暴露 recursive signature node type literal carrier。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z3-fieldinfo-signature-node-type-literal.md`。
  备注：本切片不改变裁剪 root 规则；不声明 cross-module signature provider binding、字段值读写、完整 FieldInfo methods、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 03:04:36 +08:00 · 12-S5 support / 10-S4Z2 / 11-S4AN FieldInfo direct TypeRef child type-node semantic carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` direct TypeRef child type-node semantic
  consumer 子切片完成；12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在可把 retained FieldDef metadata 中 validated generic
  signature 的 direct TypeRef child type-node 暴露为 semantic `typeToken/typeLayoutId/typeSize/typeName` carrier。
  该变更复用 runtime TypeRef token record 与 target TypeDef layout/name binding，不改变裁剪 root 规则，也不新增 metadata ABI。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 generic direct TypeRef child semantic carrier 断言后运行失败 1/11；
  GREEN 后 FieldDef token object 暴露 `childNodeObjects[2]` 的 TypeRef semantic carrier。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z2-fieldinfo-signature-typeref-child-node-semantic.md`。
  备注：本切片不改变裁剪 root 规则；不声明 cross-module signature provider binding、字段值读写、完整 FieldInfo methods、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 02:47:33 +08:00 · 12-S5 support / 10-S4Y / 11-S4AM FieldInfo direct TypeDef child/base type-node semantic carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` direct TypeDef base/child type-node semantic
  consumer 子切片完成；12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在可把 retained FieldDef metadata 中 validated generic
  signature 的 direct TypeDef base 与 child type-node 暴露为 semantic `typeToken/typeLayoutId/typeSize/typeName`
  carrier。该变更复用 runtime signature record/layout/name binding，不改变裁剪 root 规则，也不新增 metadata ABI。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 generic direct TypeDef base/child semantic carrier
  断言后运行失败 1/11；GREEN 后 FieldDef token object 暴露 `baseTypeNodeObject` 与 `childNodeObjects[1]` 的
  direct TypeDef semantic carrier。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4y-fieldinfo-signature-typedef-child-node-semantic.md`。
  备注：本切片不改变裁剪 root 规则；不声明 direct TypeRef child semantic token/layout binding、cross-module signature
  binding、字段值读写、完整 FieldInfo methods、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion
  或完整 metadata sweep 完成。

- 2026-07-01 02:19:00 +08:00 · 12-S5 support / 10-S4X / 11-S4AL FieldInfo signature primitive child type-node semantic name carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` primitive child type-node semantic name
  consumer 子切片完成；12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在可把 retained FieldDef metadata 中 validated generic
  signature 的 primitive child type-node name 暴露为 `fieldTypeSignatureNodeObject.childNodeObjects[0].typeName == "int"`。
  该变更复用运行期 builtin type name mapping，不改变裁剪 root 规则，也不新增 metadata ABI。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 generic primitive child semantic name 断言后运行失败 1/11；
  GREEN 后 FieldDef token object 暴露 child `PRIMITIVE(INT64)` node object 的 `typeName`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4x-fieldinfo-signature-primitive-child-node-semantic.md`。
  备注：本切片不改变裁剪 root 规则；不声明 direct TypeDef/TypeRef child semantic token/layout binding、
  cross-module signature binding、字段值读写、完整 FieldInfo methods、`@dynamically_accessed` dataflow、warning policy、
  DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 02:00:10 +08:00 · 12-S5 support / 10-S4W / 11-S4AK FieldInfo signature child type-node object list carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` signature child type-node object list consumer 子切片完成；
  12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在可把 retained FieldDef metadata 中 validated
  generic/wrapper signature 的 child type-node list 暴露为
  `fieldTypeSignatureNodeObject.childNodeObjects`。该 array 只承载 child node 的 structural node/blob/payload summary；
  不改变裁剪 root 规则，也不新增 metadata ABI。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 generic child node object 后运行失败 1/11；
  GREEN 后 FieldDef token object 暴露 top-level generic node、nested base `TYPE_DEF` 和 child `PRIMITIVE(INT64)` node object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4w-fieldinfo-signature-child-node-objects.md`。
  备注：本切片不改变裁剪 root 规则；不声明 child semantic binding、cross-module signature binding、字段值读写、
  完整 FieldInfo methods、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 01:41:50 +08:00 · 12-S5 support / 10-S4V / 11-S4AJ FieldInfo signature base type-node object carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` signature base type-node object consumer 子切片完成；
  12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在可把 retained FieldDef metadata 中 validated
  `GENERIC_INST`/wrapper field signature 的 base type-node 暴露为
  `fieldTypeSignatureNodeObject.baseTypeNodeObject`。该对象只承载 base node 的 structural node/blob/payload summary；
  不改变裁剪 root 规则，也不新增 metadata ABI。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 generic base type-node object 后运行失败 1/11；
  GREEN 后 FieldDef token object 暴露 top-level generic node 与 nested base `TYPE_DEF` node object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4v-fieldinfo-signature-base-type-node-object.md`。
  备注：本切片不改变裁剪 root 规则；不声明 generic argument child object、cross-module signature binding、
  semantic type binding、字段值读写、完整 FieldInfo methods、`@dynamically_accessed` dataflow、warning policy、
  DESCRIPTION promotion 或完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 01:17:21 +08:00 · 12-S5 support / 10-S4U / 11-S4AI FieldInfo signature type-node object carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` signature type-node object consumer 子切片完成；
  12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在可把 retained FieldDef metadata 中 validated
  field signature type-node 暴露为 `fieldTypeSignatureNodeObject`。该 object 聚合 node/blob offset/payload/base/child
  summary，并同步 signature-derived token/layout/typeName/matchesLayout，让 annotation-root 保留的 FieldDef signature
  metadata 在运行期具备后续 recursive semantic type object 的顶层承载面；不改变裁剪 root 规则。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 `fieldTypeSignatureNodeObject` 后运行失败 3/10；
  GREEN 后 FieldDef token object 暴露 primitive、TypeDef、TypeRef 三种 signature node object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 10/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4u-fieldinfo-signature-type-node-object.md`。
  备注：本切片不改变裁剪 root 规则；不声明 cross-module signature binding、recursive child type-node object、
  字段值读写、完整 FieldInfo methods、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或
  完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 00:55:33 +08:00 · 12-S5 support / 10-S4T / 11-S4AH FieldInfo signature/layout consistency carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` signature/layout consistency consumer 子切片完成；
  12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在可把 retained FieldDef metadata 中 signature-derived
  type layout 与 FieldDef layout 单一真相的匹配结果暴露为 `fieldTypeSignatureMatchesLayout`。该 carrier 让
  annotation-root 保留的 FieldDef signature metadata 在运行期可区分 primitive 不一致、direct TypeDef 一致和 bound
  TypeRef target layout 一致；不改变裁剪 root 规则。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 `fieldTypeSignatureMatchesLayout` 后运行失败 3/10；
  GREEN 后 FieldDef token object 暴露 primitive false、TypeDef true、TypeRef true。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 10/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4t-fieldinfo-signature-layout-consistency.md`。
  备注：本切片不改变裁剪 root 规则；不声明 cross-module signature binding、recursive type-node reflection object、
  字段值读写、完整 FieldInfo methods、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或
  完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 00:37:26 +08:00 · 12-S5 support / 10-S4S / 11-S4AG FieldInfo bound TypeRef signature carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` bound TypeRef signature consumer 子切片完成；
  12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在可把 retained FieldDef metadata 中 direct bound
  `TYPE_REF` field signature identity 映射为 TypeRef token、target TypeDef layout/size、target TypeDef name 和
  `fieldTypeSignatureType` type literal object。该路径复用 12-S5M/10-S5N/11-S4S 的 attached bound TypeRef
  resolver，让已由 annotation roots 保留的 TypeRef/target TypeDef metadata 在运行期可被 FieldInfo 展示；不改变
  裁剪 root 规则。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 bound `TYPE_REF` signature 暴露 type name/object 后
  运行失败；GREEN 后 FieldDef token object 暴露 TypeRef signature carrier 和 target TypeDef type object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 10/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4s-fieldinfo-signature-typeref-carrier.md`。
  备注：本切片不改变裁剪 root 规则；不声明 cross-module signature binding、recursive type-node reflection object、
  字段类型一致性校验、字段值读写、完整 FieldInfo methods、`@dynamically_accessed` dataflow、warning policy、
  DESCRIPTION promotion 或完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 00:15:36 +08:00 · 12-S5 support / 10-S4R / 11-S4AF FieldInfo direct TypeDef signature type object ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` direct local TypeDef signature type
  object consumer 子切片完成；12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在可把 retained FieldDef metadata 中 direct local
  `TYPE_DEF` field signature identity 继续物化为 `fieldTypeSignatureTypeName` 和 `fieldTypeSignatureType` type
  literal object。该路径让 `dynamicDependencyFieldToken` 保留下来的 FieldDef signature metadata 在运行期不只携带
  token/layout/size，还携带可展示的 type object；不改变裁剪 root 规则。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 direct `TYPE_DEF` signature 也有
  `fieldTypeSignatureType` object 后运行失败；GREEN 后 FieldDef token object 暴露 local TypeDef signature type
  object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 9/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4r-fieldinfo-signature-typedef-type-object.md`。
  备注：本切片不改变裁剪 root 规则；不声明 TypeRef/cross-module signature binding、recursive type-node reflection
  object、字段类型一致性校验、字段值读写、完整 FieldInfo methods、`@dynamically_accessed` dataflow、warning policy、
  DESCRIPTION promotion 或完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-06-30 21:45:03 +08:00 · 12-S5 support / 10-S4Q / 11-S4AE FieldInfo direct TypeDef signature token/layout carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` direct local TypeDef signature carrier
  子切片完成；12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在可把 validated `FIELD_SIG` direct local `TYPE_DEF`
  field type-node 映射为 `fieldTypeSignatureTypeToken/fieldTypeSignatureTypeLayoutId/fieldTypeSignatureTypeSize`。
  该路径让 `dynamicDependencyFieldToken` 保留下来的 FieldDef signature metadata 在运行期能携带 direct TypeDef
  signature identity；不改变裁剪 root 规则。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `fieldTypeSignatureTypeToken` 后运行失败；
  GREEN 后 FieldDef token object 暴露 local TypeDef signature token/layout carrier。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 9/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4q-fieldinfo-signature-typedef-carrier.md`。
  备注：本切片不改变裁剪 root 规则；不声明 TypeRef/cross-module signature binding、recursive type-node reflection
  object、字段类型一致性校验、字段值读写、完整 FieldInfo methods、`@dynamically_accessed` dataflow、warning policy、
  DESCRIPTION promotion 或完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-06-30 21:20:26 +08:00 · 12-S5 support / 10-S4P / 11-S4AD FieldInfo module reflection object link ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` module reflection object consumer
  子切片完成；12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在可把 `FieldInfo.module` 指向 attached runtime
  module 的最小 module reflection object。该路径让 `dynamicDependencyFieldToken` 保留下来的 FieldDef metadata
  在运行期能携带模块身份 object，而不是只有 `moduleName` 字符串；不改变裁剪 root 规则。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `FieldInfo.module` object 后运行失败；
  GREEN 后 FieldDef token object 暴露 `geometry` module reflection object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4p-fieldinfo-module-reflection-link.md`。
  备注：本切片不改变裁剪 root 规则；不声明字段值读写、完整 FieldInfo methods、完整 signature-derived field type
  binding、TypeDef/TypeRef signature binding、recursive type-node reflection object、跨模块 FieldRef/TypeRef、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。Clang 仍报告
  既有 `reflection.c` 中 `callerName` unused warning。

- 2026-06-30 21:02:11 +08:00 · 12-S5 support / 10-S4O / 11-S4AC FieldInfo primitive signature type object carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` primitive field signature type object consumer
  子切片完成；12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在在 validated `FIELD_SIG` 的 field type-node 为
  `PRIMITIVE` 时，写出独立 `FieldInfo.fieldTypeSignatureType` type literal object。该路径让
  `dynamicDependencyFieldToken` 保留下来的 FieldDef signature metadata 在运行期不仅能暴露 primitive type
  name/value carrier，也能暴露最小 type object；不改变裁剪 root 规则。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 primitive signature type object 后运行失败；
  GREEN 后 FieldDef token object 暴露 `PRIMITIVE(BOOL)` 的 type literal object，且 layout-derived `type` 仍为 `int`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4o-fieldinfo-field-signature-type-object.md`。
  备注：本切片不改变裁剪 root 规则；不声明完整 signature-derived field type binding、TypeDef/TypeRef signature
  binding、recursive type-node reflection object、字段值读写、完整 FieldInfo methods、module reflection link、跨模块
  FieldRef/TypeRef、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-06-30 20:46:10 +08:00 · 12-S5 support / 10-S4N / 11-S4AB FieldInfo primitive signature type carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` primitive field signature type consumer
  子切片完成；12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在在 validated `FIELD_SIG` 的 field type-node 为
  `PRIMITIVE` 时，写出 `FieldInfo.fieldTypeSignatureValueType` 与 `FieldInfo.fieldTypeSignatureTypeName`。
  该路径让 `dynamicDependencyFieldToken` 保留下来的 FieldDef signature metadata 在运行期不仅能暴露 raw type-node
  shape，也能暴露 primitive type name/value carrier；不改变裁剪 root 规则。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 primitive signature value type/name 字段后运行失败；
  GREEN 后 FieldDef token object 暴露 `PRIMITIVE(BOOL)` 的 `ZR_VALUE_TYPE_BOOL` 和 `"bool"`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4n-fieldinfo-field-signature-primitive-type.md`。
  备注：本切片不改变裁剪 root 规则；不声明完整 signature-derived field type binding、TypeDef/TypeRef signature
  binding、recursive type-node reflection object、字段值读写、完整 FieldInfo methods、module reflection link、跨模块
  FieldRef/TypeRef、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-06-30 20:32:08 +08:00 · 12-S5 support / 10-S4M / 11-S4AA FieldInfo field signature type-node summary carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` field signature type-node summary
  consumer 子切片完成；12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在通过 attached metadata runtime 的
  `ZrCore_MetadataRuntime_ReadSignatureTypeNode()` 读取 validated `FIELD_SIG` 的 field type-node，并写入 public
  `FieldInfo.fieldTypeSignatureNode`、offset、next offset、payload 与 child/base summary 字段。该路径让
  `dynamicDependencyFieldToken` 保留下来的 FieldDef signature metadata 在运行期不仅能暴露 header，也能暴露最小
  type-node shape。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 field signature type-node summary 字段后运行失败；
  GREEN 后 FieldDef token object 暴露 `PRIMITIVE(BOOL)` field type node summary。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4m-fieldinfo-field-signature-type-node.md`。
  备注：本切片不改变裁剪 root 规则；不声明 signature-derived field type binding、recursive type-node reflection
  object、字段值读写、完整 FieldInfo methods、module reflection link、跨模块 FieldRef/TypeRef、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-06-30 20:13:27 +08:00 · 12-S5 support / 10-S4L / 11-S4Z FieldInfo validated field signature header carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` validated field signature header
  consumer 子切片完成；12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在通过 attached metadata runtime 的
  `ZrCore_MetadataRuntime_ReadSignatureView()` 读取 FieldDef paired signature record，并在 validated root 为
  `FIELD_SIG` 时写入 public `FieldInfo.signatureRootNode`、`FieldInfo.signatureFlags` 与
  `FieldInfo.fieldTypeBlobOffset`。该路径让保留下来的 FieldDef metadata 在运行期可暴露 validated field signature
  header，同时不改变裁剪 root 规则。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 validated field signature header 字段后运行失败；
  GREEN 后 FieldDef token object 暴露 `FIELD_SIG` root、flags `1` 和 field type blob offset `2`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4l-fieldinfo-field-signature-header.md`。
  备注：本切片不改变裁剪 root 规则；不声明 signature-derived field type binding、recursive type-node reflection
  object、字段值读写、完整 FieldInfo methods、module reflection link、跨模块 FieldRef/TypeRef、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-06-30 19:54:58 +08:00 · 12-S5 support / 10-S4K / 11-S4Y FieldInfo FieldDef signature blob coordinate carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` raw signature blob coordinate consumer
  子切片完成；12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在把 attached metadata runtime 中已解析 FieldDef
  row 的 `signatureBlobOffset` / `signatureBlobLength` 写入 public `FieldInfo.signatureBlobOffset` /
  `FieldInfo.signatureBlobLength`。该路径让保留下来的 FieldDef metadata 在运行期可暴露 raw field-signature
  blob 坐标，同时不验证或解析 blob。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `signatureBlobOffset` / `signatureBlobLength`
  后运行失败；GREEN 后 FieldDef token object 暴露 fixture raw coordinates `4/7`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4k-fieldinfo-fielddef-signature-blob.md`。
  备注：本切片不改变裁剪 root 规则；不声明 blob slice validation、field signature parser、
  signature-derived field type binding、字段值读写、完整 FieldInfo methods、module reflection link、跨模块
  FieldRef/TypeRef、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-06-30 19:39:14 +08:00 · 12-S5 support / 10-S4J / 11-S4X FieldInfo FieldDef flags carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` raw flags consumer 子切片完成；
  12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在把 attached metadata runtime 中已解析 FieldDef
  row 的 `flags` 写入 public `FieldInfo.metadataFlags`。该路径让保留下来的 FieldDef metadata 在运行期可暴露
  raw metadata flags，同时不解释 flags 位语义。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `metadataFlags` 后运行失败；GREEN 后 FieldDef
  token object 暴露 fixture raw flags `0xA5`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4j-fieldinfo-fielddef-flags.md`。
  备注：本切片不改变裁剪 root 规则；不声明 flags 位语义、`isStatic`/`isConst` 映射、字段值读写、
  完整 FieldInfo methods、module reflection link、跨模块 FieldRef/TypeRef、`@dynamically_accessed` dataflow、
  warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-06-30 19:26:41 +08:00 · 12-S5 support / 10-S4I / 11-S4W FieldInfo moduleName carrier ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` module name consumer 子切片完成；
  12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在从 attached metadata runtime 的 module 读取
  `moduleName`，缺失时回退 `fullPath`，并写入 public `FieldInfo.moduleName`。该路径让保留下来的 FieldDef
  metadata 在运行期能携带最小 module identity 字符串。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `moduleName` string 后运行失败；GREEN 后 FieldDef
  token object 暴露 synthetic module name `geometry`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4i-fieldinfo-module-name.md`。
  备注：本切片不改变裁剪 root 规则；不声明字段值读写、完整 FieldInfo methods、module reflection link、
  跨模块 FieldRef/TypeRef、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整
  metadata sweep 完成。

- 2026-06-30 19:11:22 +08:00 · 12-S5 support / 10-S4H / 11-S4V FieldInfo owner object link ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` owner link consumer 子切片完成；
  12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在把 public `owner` 字段链接到同一
  `declaringType` type literal object。该路径让 `dynamicDependencyFieldToken` 保留下来的 owner TypeDef metadata
  在运行期可形成最小 owner identity link。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `owner` object link 后运行失败；GREEN 后 FieldDef
  token object 的 owner 与 declaringType 指针一致。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4h-fieldinfo-owner-link.md`。
  备注：本切片不改变裁剪 root 规则；不声明字段值读写、完整 FieldInfo methods、module reflection link、
  跨模块 FieldRef/TypeRef、`@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-06-30 18:58:39 +08:00 · 12-S5 support / 10-S4G / 11-S4U FieldInfo declaring type object link ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public `FieldInfo` declaring type consumer 子切片完成；
  12-S5 的数据流、跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在把 owner TypeDef row 的 string-pool name 作为
  `ownerTypeName`、`declaringTypeName` 和 nested `declaringType` type literal 暴露出来。该路径让
  `dynamicDependencyFieldToken` 保留下来的 owner TypeDef metadata 在运行期不仅能给 FieldInfo 提供 token/layout，
  也能提供最小声明类型 identity。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 declaring type 字段后运行失败；GREEN 后 FieldDef
  token object 的 `type` 与 `declaringType` nested objects 均通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4g-fieldinfo-declaring-type-object.md`。
  备注：本切片不改变裁剪 root 规则；不声明字段值读写、完整 FieldInfo methods、跨模块 FieldRef/TypeRef、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-06-30 18:38:42 +08:00 · 12-S5 support / 10-S4F / 11-S4T minimum FieldDef token FieldInfo public object ·
  状态：annotation-root 保留后的 FieldDef metadata 运行期 public reflection consumer 子切片完成；12-S5 的数据流、
  跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 可从 attached metadata runtime 的 FieldDef token
  构造最小 public `FieldInfo` object，复用 11-S4 FieldDef binding view、TypeDef binding view、layout/token/offset
  单一真相和 zrp string pool。该路径让 `dynamicDependencyFieldToken` 保留下来的 FieldDef/owner/type layout metadata
  在运行期具备最小 public reflection object 消费端。
  RED/GREEN：RED 为 focused reflection token resolver 测试链接缺失新 API；GREEN 后 FieldDef token object
  positive/null/wrong-token paths 通过，reflection token resolve 8/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s4f-fielddef-token-fieldinfo-object.md`。
  备注：本切片不改变裁剪 root 规则；不声明字段值读写、完整 FieldInfo methods、跨模块 FieldRef/TypeRef、
  `@dynamically_accessed` dataflow、warning policy、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-06-30 18:17:22 +08:00 · 12-S5M / 10-S5N / 11-S4S runtime bound TypeRef token layout resolver ·
  状态：12-S5 annotation-root 保留后的运行时 TypeRef layout 解析支撑子切片完成；完整反射数据流、
  跨模块 dependency/provider compatibility 和 warning 策略仍未关闭。
  完成项目：runtime `ResolveTypeTokenLayout()` 现在可消费前一切片保留下来的 attached `TYPE_REF` token record；
  当它绑定到当前 runtime 可解析的 `TYPE_DEF` 时，返回目标 TypeDef layout/id，并对 target signature/module/layout identity
  做一致性校验。这样 `dynamicDependencyTypeToken` 保留的 bound TypeRef layout 在运行期也有同一 resolver 入口。
  RED/GREEN：RED1 为 TypeRef token layout focused test 返回 NULL；GREEN1 后 bound TypeRef 命中目标 TypeDef layout 并缓存。
  RED2 为 module hash mismatch 未拒绝；GREEN2 后拒绝并清空 out layout id。layout identity mismatch 负向路径同步通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `metadata_runtime_typespec_layout` 17/0、`metadata_runtime_query`
  24/0、`reflection_token_resolve` 7/0、`metadata_type_ref_binding` 8/0；三套 focused CTest
  `metadata_runtime_typespec_layout|metadata_runtime_query|reflection_token_resolve|metadata_type_ref_binding` 均为 4/4。
  WSL GCC 初次 combined CTest 的 `metadata_type_ref_binding` wrapper transient 未复现。`git diff --check` 退出 0（仅
  LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5n-runtime-bound-typeref-token-layout-resolver.md`。
  备注：本切片只覆盖 attached/current-runtime TypeRef record -> TypeDef layout；不声明跨模块 provider 加载/版本兼容、
  `@dynamically_accessed` dataflow、未注解反射 warning、DESCRIPTION promotion、FieldInfo/字段读写或完整 metadata sweep 完成。

- 2026-06-30 17:56:52 +08:00 · 12-S5L / 10-S5M dynamic-dependency bound TypeRef type token root ·
  状态：12-S5 annotation root 中的当前 embedded zrp TypeRef token-record binding 子切片完成；完整反射数据流、
  跨模块 provider load/compatibility、runtime TypeRef layout resolution、跨模块 dependency 和 warning 策略仍未关闭。
  完成项目：`dynamicDependencyTypeToken` type-layout root collector 现在可读取 embedded zrp `TOKEN_RECORDS`
  section，匹配唯一 `TYPE_REF` `SZrMetadataTokenRecord`，并要求 `record->targetMetadataToken` 指向当前 blob
  的 `TYPE_DEF` row。该目标 TypeDef row 的唯一 non-none `typeLayoutId` 进入 annotation type-layout root set，
  让 generated C 在函数裁剪后仍保留目标 TypeDef layout descriptor、registration entry、统计、root marker，
  以及 root-only `zr_aot_type_layout_tokens[]` 中的 `TYPE_DEF` token。
  RED/GREEN：RED 为 TypeRef generated-C fixture 使用 `dynamicDependencyTypeToken = 0x05000001` 后 writer 返回 false；
  GREEN 后 TypeRef token record 的 `targetMetadataToken = 0x02000001` 解析到 `typeLayoutId = 2`，
  `zr_aot_fn_2` 仍被裁剪，但 `ZrTypeLayout_2`、root marker、type-layout stats 和 token table fallback 保留。
  source/type-layout contracts 同步锁定 `TOKEN_RECORDS` section、`SZrMetadataTokenRecord`、`record->targetMetadataToken`
  和 `ZR_METADATA_TABLE_TYPE_REF` gate。
  验证：WSL gcc/clang focused code stripping 10/0、source contracts 24/0、type-layout contracts 1/0，
  focused CTest `aot_c_code_stripping|aot_c_type_layout_contracts` 2/2，global shared-library smoke 10/0、
  call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；Windows MSVC Debug focused code stripping 10/0、
  source contracts 24/0、type-layout contracts 1/0，focused CTest 2/2；Windows shared-library smoke binaries
  returned OK with 10/5/7 ignored and 0 failures。`git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5m-dynamic-dependency-bound-typeref-token-layout-root.md`。
  备注：本切片只覆盖当前 embedded metadata 中已绑定到当前 TypeDef row 的 TypeRef；不声明跨模块 provider
  加载/版本兼容、运行期 TypeRef->layout resolver、字段值读写、FieldInfo object、数据流、warning policy、
  DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 17:31:53 +08:00 · 12-S5K / 10-S5L dynamic-dependency field token layout roots ·
  状态：12-S5 annotation root 中的当前模块 FieldDef field-token carrier 子切片完成；完整反射数据流、
  字段值读写/FieldInfo 物化、TypeRef/跨模块 type token、跨模块 dependency 和 warning 策略仍未关闭。
  完成项目：新增 `backend_aot_c_type_layout_metadata_roots.{h,c}`，把 embedded zrp metadata 的 TypeDef/TypeSpec/FieldDef
  token -> type-layout root 解析从 type-layout emitter 拆出。AOT C root collector 现在读取 `dynamicDependencyFieldToken`
  uint32，仅接受当前模块 `MEMBER_DEF` FieldDef row，校验 owner TypeDef row 的 field range，并把 owner TypeDef
  `typeLayoutId` 与 FieldDef `typeLayoutId` 一起加入 annotation type-layout root set。生成 C 在目标函数被裁剪时仍保留
  owner/field layout descriptors、registration entries、type-layout stats 和 root markers。
  RED/GREEN：RED 为新增 generated-C FieldDef token fixture 期望保留 `ZrTypeLayout_2` 后失败 `Expected Non-NULL`；
  GREEN 后 fixture 确认 function 2 被裁剪，annotation type-layout roots 为 1/2，type-layout count/payload/generated-byte
  stats 全部保留。
  验证：WSL gcc/clang focused code stripping 9/0、source contracts 24/0、type-layout contracts 1/0，
  focused CTest `aot_c_code_stripping|aot_c_type_layout_contracts` 2/2，global shared-library smoke 10/0、
  call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；Windows MSVC Debug focused code stripping 9/0、
  source contracts 24/0、type-layout contracts 1/0，focused CTest 2/2；Windows shared-library smoke binaries
  returned OK with 10/5/7 ignored and 0 failures。`git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5l-dynamic-dependency-field-token-layout-roots.md`。
  备注：本切片只覆盖当前模块 FieldDef token -> owner/field type-layout roots；不声明字段值读写、FieldInfo object、
  TypeRef/跨模块 type token、数据流、warning policy、DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 16:58:17 +08:00 · 12-S5J / 10-S5K dynamic-dependency type token root ·
  状态：12-S5 annotation root 中的当前模块 TypeDef/TypeSpec type-token carrier 子切片完成；完整反射数据流、
  FieldDef/field dependency、TypeRef/跨模块 type token、跨模块 dependency 和 warning 策略仍未关闭。
  完成项目：AOT C emitter 在裁剪前把 embedded metadata blob 交给 dynamic type-layout root collector，
  从 function decorator metadata 读取 `dynamicDependencyTypeToken` uint32，只接受 `TYPE_DEF`/`TYPE_SPEC` token，
  并通过 embedded zrp TypeDef/TypeSpec row 的唯一 non-none `typeLayoutId` 追加 type-layout root。
  root-only type layout 的 token table 现在可从 metadata blob 反查并写出 `0x02000001u` / `0x07000001u`。
  RED/GREEN：RED 为新增 TypeDef token generated-C fixture 期望保留 `ZrTypeLayout_2` 后失败 `Expected Non-NULL`；
  GREEN 后 TypeDef 与 TypeSpec fixtures 都确认 `zr_aot_fn_2` 被裁剪，`ZrTypeLayout_2`、root marker、
  type-layout stats 和 `zr_aot_type_layout_tokens[2]` 保留。
  验证：WSL gcc/clang focused code stripping 8/0、source contracts 24/0、type-layout contracts 1/0，
  global shared-library smoke 10/0、call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；
  WSL gcc/clang focused CTest `aot_c_code_stripping|aot_c_type_layout_contracts` 2/2；
  Windows MSVC Debug focused code stripping 8/0、source contracts 24/0、type-layout contracts 1/0，
  focused CTest 2/2；Windows shared-library smoke binaries returned OK with 10/5/7 ignored and 0 failures。
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5k-dynamic-dependency-type-token-root.md`。
  备注：本切片只覆盖当前模块 TypeDef/TypeSpec token-to-typeLayoutId mapping；不声明 FieldDef、
  TypeRef/跨模块 type token、field dependency、数据流、warning policy、DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 16:25:31 +08:00 · 12-S5I / 10-S5J dynamic-dependency type-layout root ·
  状态：12-S5 annotation root 中的当前模块 type-layout-id carrier 子切片完成；完整反射数据流、
  TypeDef/TypeSpec token dependency、field dependency、跨模块 dependency 和 warning 策略仍未关闭。
  完成项目：AOT C emitter 在裁剪前从 function decorator metadata 收集
  `dynamicDependencyTypeLayoutId` uint32 root，传给 type-layout 统计、layout declaration emission、index-space
  计算、GC descriptor table 和 code-registration `typeLayouts[]` writer。生成 C 输出
  `code_stripping.annotationTypeLayoutRoots` / `code_stripping.annotationTypeLayoutRoot[index]`，并在目标函数被裁剪时
  仍保留对应 `SZrTypeLayout` descriptor 与 registration table entry。
  RED/GREEN：RED 为新增 generated-C fixture 期望保留 `ZrTypeLayout_2` 后失败 `Expected Non-NULL`；
  GREEN 后 `zr_aot_fn_2` 仍被裁剪，但 `ZrTypeLayout_2`、`&ZrTypeLayout_2` registration entry、
  type-layout count/payload/generated-byte stats 和 annotation type-layout root marker 均保留。
  验证：WSL gcc/clang focused code stripping 6/0、source contracts 24/0、type-layout contracts 1/0，
  global shared-library smoke 10/0、call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；
  WSL gcc/clang focused CTest `aot_c_code_stripping|aot_c_type_layout_contracts` 2/2；
  Windows MSVC Debug focused code stripping 6/0、source contracts 24/0、type-layout contracts 1/0。
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5j-dynamic-dependency-type-layout-root.md`。
  备注：本切片只覆盖当前模块 numeric `dynamicDependencyTypeLayoutId` 对 generated type-layout metadata 的保留；
  不声明 TypeDef/TypeSpec token dependency、field dependency、跨模块 annotation、非方法 member token、
  `@dynamically_accessed` 数据流、warning promotion/per-warning suppression、未注解反射 warning、
  类型/成员级 DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 15:31:50 +08:00 · 12-S5H / 10-S5I dynamic-dependency non-exported MethodDef token root ·
  状态：12-S5 annotation root 中的当前模块非导出 method token carrier 子切片完成；完整反射数据流、
  跨模块 dependency、field/type dependency 和 warning 策略仍未关闭。
  完成项目：reflection annotation root collector 的 `dynamicDependencyMethodToken` resolver 现在按 root module
  `typedExportedSymbols` 中的 typed function symbol + `MEMBER_DEF` token 精确匹配 callable child，不再把
  `exportKind == ZR_MODULE_EXPORT_KIND_FUNCTION` 作为 token root 的必要条件。token 解析仍要求唯一匹配、有效 child index
  和 pre-trim function-table 映射；按名 dependency 仍保持 exported-name 限制。
  RED/GREEN：RED 为 reachability fixture 用非导出 typed function symbol 承载 `dynamicDependencyMethodToken = 0x03000008`
  后返回 `Expected TRUE Was FALSE`；generated-C fixture 同样无法保留 otherwise-unreachable target。GREEN 后
  `annotationRoot[0] = 2` 与 `zr_aot_fn_2` 均可见。source contract 锁定 non-exported token helper、
  `symbolKind`/`MEMBER_DEF` gate、matched token count 和 function-table flat-index lookup。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；
  WSL gcc/clang focused reachability 14/0、annotation preserve 11/0、source contracts 23/0、global shared-library smoke 10/0、
  call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；Windows MSVC Debug 同组 CTest 3/3、reachability 14/0、
  annotation preserve 11/0、source contracts 23/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s5i-dynamic-dependency-non-exported-method-token-root.md`。
  备注：本切片只覆盖当前模块 `MEMBER_DEF` method token 经 typed function symbol 绑定到非导出 callable child；
  不声明跨模块 annotation、field/type dependency、非方法 member token、`@dynamically_accessed` 数据流、
  warning promotion/per-warning suppression、未注解反射 warning、类型/成员级 DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 14:38:53 +08:00 · 12-S5G / 10-S5H dynamic-dependency method-name signature-hash disambiguation ·
  状态：12-S5 annotation root 中的当前模块 exported method name 签名消歧子切片完成；完整反射数据流、
  跨模块/非导出成员依赖和 warning 策略仍未关闭。
  完成项目：reflection annotation root collector 现在读取可选
  `dynamicDependencyMethodSignatureHash` uint64 字段，并把它与 `dynamicDependencyMethodName` 一起用于 root module
  `typedExportedSymbols` 的 exported function name + `signatureHash` 唯一匹配。未提供 signature hash 时，
  重复同名 exported function 不再静默绑定第一个匹配项，而是把 metadata 视为歧义并拒绝收集。
  RED/GREEN：RED 为两个同名 exported `target` 方法写入不同 `signatureHash` 后，旧 resolver 仍保留第一个
  child，reachability 失败为 `Expected 2 Was 1`；GREEN 后 `dynamicDependencyMethodSignatureHash = 0x2222`
  正确解析到 flat index 2，并新增未提供 hash 的同名歧义负向用例。generated-C fixture 同步证明
  `annotationRoot[0] = 2` 与 `zr_aot_fn_2` 保留。source contract 锁定 uint64 metadata helper、signature hash
  field、显式 zero hash、name+signature resolver、`symbol->signatureHash` match 与 matched-symbol count。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；
  WSL gcc/clang focused reachability 13/0、annotation preserve 10/0、source contracts 23/0、global shared-library smoke 10/0、
  call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；Windows MSVC Debug 同组 CTest 3/3、reachability 13/0、
  annotation preserve 10/0、source contracts 23/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s5h-dynamic-dependency-method-name-signature-hash-root.md`。
  备注：本切片只覆盖当前模块 exported method name 的 optional signature-hash 消歧与同名歧义拒绝；
  不声明跨模块 annotation、非导出 member token、field/type dependency、`@dynamically_accessed` 数据流、
  warning promotion/per-warning suppression、未注解反射 warning、类型/成员级 DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 14:01:25 +08:00 · 12-S5F / 10-S5G dynamic-dependency exported method-name root ·
  状态：12-S5 annotation root 中的当前模块 exported method name carrier 子切片完成；完整反射数据流、
  跨模块/非导出成员依赖和 warning 策略仍未关闭。
  完成项目：reflection annotation root collector 现在读取 function decorator metadata
  `dynamicDependencyMethodName` string 字段，并只通过 root module `typedExportedSymbols` 中的 exported function
  name 解析为 callable child 的 flat function index。解析成功后目标复用既有 annotation root 去重与
  `ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION` 路径，生成 C 继续输出
  `code_stripping.annotationRoots` / `code_stripping.annotationRoot[index]`。
  RED/GREEN：RED 为 reachability fixture 写入 `dynamicDependencyMethodName = "target"` 后 annotation root count
  仍为 0；generated-C fixture 也无法保留 otherwise-unreachable target。GREEN 后 name 解析到 exported child flat index 2，
  `annotationRoot[0] = 2` 和 `zr_aot_fn_2` 均可见。source contract 同步保护 string metadata helper、method-name
  resolver、typed exported symbol name match 与 function-table flat-index lookup。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；
  WSL gcc/clang focused reachability 10/0、annotation preserve 9/0、source contracts 23/0、global shared-library smoke 10/0、
  call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；Windows MSVC Debug 同组 CTest 3/3、reachability 10/0、
  annotation preserve 9/0、source contracts 23/0，三个 Unix-only smoke 为 0 failures / 10 ignored、5 ignored、7 ignored。
  产出：`tests/acceptance/2026-06-30-aot-10-s5g-dynamic-dependency-method-name-root.md`。
  备注：本切片只覆盖当前模块 exported method name 到 function root 的保留；不声明跨模块 annotation、非导出 member token、
  field/type dependency、重载签名消歧、`@dynamically_accessed` 数据流、warning promotion/per-warning suppression、
  未注解反射 warning、类型/成员级 DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 13:22:30 +08:00 · 12-S5E / 10-S5F dynamic-dependency MethodDef token root ·
  状态：12-S5 annotation root 中的当前模块 exported MethodDef token carrier 子切片完成；完整反射数据流、
  按名/跨模块/非导出成员依赖和 warning 策略仍未关闭。
  完成项目：reflection annotation root collector 现在读取 function decorator metadata
  `dynamicDependencyMethodToken` uint 字段，要求 token table 为 `MEMBER_DEF`，并只通过 root module
  `typedExportedSymbols` 中的 function export token 解析为 callable child 的 flat function index。解析成功后目标复用
  既有 annotation root 去重与 `ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION` 路径，生成 C 继续输出
  `code_stripping.annotationRoots` / `code_stripping.annotationRoot[index]`。
  RED/GREEN：RED 为 reachability fixture 写入 `dynamicDependencyMethodToken = 0x03000007` 后 annotation root count
  仍为 0；generated-C fixture 也无法保留 otherwise-unreachable target。GREEN 后 token 解析到 exported child flat index 2，
  `annotationRoot[0] = 2` 和 `zr_aot_fn_2` 均可见。source contract 同步保护 `MEMBER_DEF` gate、typed exported symbol
  lookup 与 function-table flat-index lookup。
  验证：WSL gcc/clang 均通过 focused build、CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve`
  3/3、reachability 9/0、annotation preserve 8/0、global shared-library smoke 10/0、call shared-library smoke 5/0、
  dynamic deopt bridge smoke 7/0、source contracts 23/0；Windows MSVC Debug 同组 build 与 CTest 3/3、reachability 9/0、
  annotation preserve 8/0、source contracts 23/0，Unix-only smoke 为 0 failures / ignored。一次 WSL gcc 全量串行验证
  因超时中断，随后已拆分重跑并通过。
  产出：`tests/acceptance/2026-06-30-aot-10-s5f-dynamic-dependency-method-token-root.md`。
  备注：本切片只覆盖当前模块 exported `MEMBER_DEF` method token 到 function root 的保留；不声明按名成员 dependency、
  field/type dependency、非导出 member token、跨模块 annotation、`@dynamically_accessed` 数据流、warning promotion/
  per-warning suppression、未注解反射 warning、类型/成员级 DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 12:54:26 +08:00 · 12-S7ZU / 10-S5E annotation warning suppression ·
  状态：12-S7 trim warning 中的 writer-level annotation warning suppression 子切片完成；完整 attribute/annotation-driven
  warning policy 和 trim analyzer 仍未关闭。
  完成项目：`SZrAotWriterOptions.suppressAnnotationWarnings` 让 opt-in code stripping 继续扫描 annotation warnings，
  但把可见 `trim_warnings.annotationCount` 置 0，并把总数写入新增
  `trim_warnings.annotationSuppressedCount`。开启后不输出逐条 `trim_warning.annotation[]` marker；未开启时保持
  `requires-unreferenced-code` 与 `message="..."` marker 兼容形态。runtime fallback warning 的 visible/suppressed count
  与 reason mask 不受该选项影响。
  RED/GREEN：RED 为新 suppressed annotation warning fixture 首先在编译期失败，因为 `SZrAotWriterOptions`
  缺少 `suppressAnnotationWarnings`；GREEN 后 generated C 输出 annotationCount=0、annotationSuppressedCount=1，
  不包含 `trim_warning.annotation[0]`，runtime fallback suppressed count 仍为 0。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；
  WSL gcc/clang focused annotation preserve 7/0、global shared-library smoke 10/0、call shared-library smoke 5/0、
  dynamic deopt bridge smoke 7/0、source contracts 22/0；Windows MSVC Debug 同组 CTest 3/3，annotation preserve 7/0，
  source contracts 22/0，global/call/dynamic Unix-only smoke 为 0 failures / 10 ignored、5 ignored、7 ignored。
  追加 source-contract marker 后，WSL gcc/clang source contracts 22/0 + annotation preserve 7/0，MSVC Debug
  source contracts 22/0 + annotation preserve 7/0。
  产出：`tests/acceptance/2026-06-30-aot-12-s7zu-annotation-warning-suppression.md`。
  备注：本切片只提供 writer-level 全局 suppression；不声明属性级 per-warning suppression、warning promotion、
  `@dynamically_accessed` 数据流、按 token/按名 dynamic dependency、跨模块 annotation、未注解反射 warning、
  field/default-value backed constant-pool remap 或完整 metadata sweep。另补 `TYPEOF` reflection runtime fallback
  warning marker 回归断言，该断言覆盖既有 runtime fallback reason=reflection 行为。

- 2026-06-30 12:38:52 +08:00 · 12-S5D / 10-S5D dynamic-dependency function root ·
  状态：12-S5 注解标记中 `@dynamic_dependency` 的函数级首个 carrier 子切片完成；完整反射数据流分析、
  成员/类型依赖和 warning 策略仍未关闭。
  完成项目：reflection annotation root collector 现在读取 `dynamicDependencyFunctionIndex` uint metadata，
  校验该 flat function index 存在并去重加入 annotation root 集合；opt-in code stripping 由同一 root buffer
  驱动 reachability，生成 C 继续输出 `code_stripping.annotationRoots` 与 `code_stripping.annotationRoot[index]`。
  RED/GREEN：RED 为 generated-C dynamic dependency fixture 缺少 `annotationRoot[0] = 2` 且目标函数被裁剪；
  GREEN 后 `zr_aot_fn_2` 被保留，既有 reflectable、unannotated prune、requires-unreferenced warning 和 reason
  text 用例全部保持通过。
  验证：WSL gcc focused annotation preserve 6/0、reachability 8/0；WSL gcc/clang CTest
  `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；WSL gcc/clang call shared-library
  smoke 5/0、dynamic deopt bridge smoke 7/0、source contracts 22/0；Windows MSVC Debug 同组 CTest 3/3，
  Unix-only smoke 为 0 failures / 5 ignored 与 0 failures / 7 ignored，source contracts 22/0；`git diff --check`
  退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5d-dynamic-dependency-function-root.md`。
  备注：本切片只覆盖单个 function flat-index dependency，不声明按 token/按名成员 dependency、跨模块 annotation
  roots、`@dynamically_accessed` 流分析、未注解反射 warning、warning 抑制/提升或完整 metadata sweep 完成。

- 2026-06-30 12:22:15 +08:00 · 12-S5C / 10-S5C requires-unreferenced-code reason text marker ·
  状态：12-S5 注解标记与 12-S7 trim warning 的 reason 文本子切片完成；完整反射数据流分析、warning 抑制策略和
  完整 trim analyzer 仍未关闭。
  完成项目：annotation warning scanner 现在读取 `requiresUnreferencedCodeReason` string metadata，并在 retained
  static caller 的 `trim_warning.annotation[] reason=requires-unreferenced-code` marker 上追加 quoted/escaped
  `message="..."`。缺失、空或非字符串 reason 不输出 `message=`，保持 12-S5B 的 marker 兼容形态。
  RED/GREEN：RED 为 reason fixture 期望带双引号内容的 escaped `message` 后失败；GREEN 后 reason 字符串
  `uses "name" lookup` 输出为 `message="uses \"name\" lookup"`，bool-only 和 unannotated 负向路径保持既有结果。
  验证：WSL gcc focused annotation preserve 5/0；WSL gcc/clang CTest
  `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；WSL gcc/clang call shared-library
  smoke 5/0、dynamic deopt bridge smoke 7/0；WSL gcc/clang source contracts 22/0；Windows MSVC Debug 同组 CTest 3/3，
  Unix-only smoke 为 0 failures / 5 ignored 和 0 failures / 7 ignored，source contracts 22/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s5c-requires-unreferenced-code-reason.md`。
  备注：本切片只补 annotation warning 诊断的 reason 文本，不声明 `@dynamically_accessed` 数据流、
  `@dynamic_dependency`、warning suppression/promotion、跨模块 annotation、未注解反射 warning 或完整 metadata sweep 完成。

- 2026-06-30 11:59:59 +08:00 · 12-S5B / 10-S5B requires-unreferenced-code static call warning ·
  状态：12-S5 注解标记与 12-S7 trim warning 的交叉子切片完成；完整反射数据流分析、warning 抑制策略和完整
  trim analyzer 仍未关闭。
  完成项目：新增 `backend_aot_c_annotation_warnings.{h,c}`，在 opt-in code stripping 下扫描 retained caller 的静态
  call 指令；若 callee function decorator metadata 含 `requiresUnreferencedCode: true`，生成 C 头部输出
  `trim_warnings.annotationCount` 和逐条
  `trim_warning.annotation[index] function=<flatIndex> instruction=<index> targetFunction=<flatIndex> reason=requires-unreferenced-code`
  marker。`backend_aot_resolve_callable_slot_function_index_before_instruction(...)` 支持 `GET_SUB_FUNCTION`，使静态子函数
  call slot 可解析回原始 flat function index。该 marker 与 runtime fallback warning count/reason mask 分离。
  RED/GREEN：RED 为 requires-unreferenced static-call fixture 缺少 `trim_warnings.annotationCount = 1` 与
  `trim_warning.annotation[0]`；GREEN 后标注 callee 输出 1 条 annotation warning，未标注 callee 输出 0 条。
  验证：WSL gcc focused annotation preserve 4/0；WSL gcc/clang CTest
  `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；WSL gcc/clang call shared-library
  smoke 5/0、dynamic deopt bridge smoke 7/0；Windows MSVC Debug 同组 CTest 3/3，两个 Unix-only smoke 为 0 failures
  / 5 ignored 与 0 failures / 7 ignored；WSL gcc/clang source contracts 22/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s5b-requires-unreferenced-code-warning.md`。
  备注：本切片只覆盖 retained static caller -> annotated static callee 的 trim annotation warning；不声明
  `@dynamically_accessed` 数据流、`@dynamic_dependency`、用户 reason 字符串、warning suppression/promotion、
  跨模块 annotation 或完整 metadata sweep 完成。

- 2026-06-30 11:25:44 +08:00 · 12-S5A / 10-S5A reflectable annotation function roots ·
  状态：12-S5 首个 annotation-root 子切片完成；完整反射数据流分析和 warning 策略仍未关闭。
  完成项目：静态 callable reachability 现在接受 reflection annotation root 输入，并用
  `ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION` 记录首次标记原因；AOT C emitter 从裁剪前 function table 收集
  `reflectable: true` decorator metadata roots，将它们与 entry/export/manifest roots 一起参与 BFS，且生成
  `code_stripping.annotationRoots` / `code_stripping.annotationRoot[index]` markers。
  RED/GREEN：RED 为新 annotation preserve 测试缺少 marker 且不可达 reflectable function 被裁剪；GREEN 后 reflectable
  function 保留，未标注同函数仍被裁剪。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；
  Windows MSVC Debug 同组 3/3；WSL gcc/clang source contracts 22/0；`git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5a-reflectable-annotation-function-root.md`。
  备注：本切片不声明 `@dynamically_accessed` 流分析、`@dynamic_dependency`、未注解反射 warning、跨模块 annotation
  roots 或完整 trim analyzer 完成。

- 2026-06-30 02:28:55 +08:00 · 12-S7ZT runtime fallback warning reason-mask aggregate markers ·
  状态：12-S7 子切片完成；完整 12-S7 仍未关闭，完整 trim analyzer、attribute/annotation 抑制策略、
  field/default-value backed constant-pool remap、cross-module export-token publication/rewrite、
  完整 metadata sweep/pruning 和 annotation-driven warning policy 仍待后续。
  完成项目：AOT C 文件头部现在在既有 `trim_warnings.runtimeFallbackCount` 与
  `trim_warnings.runtimeFallbackSuppressedCount` 旁输出
  `trim_warnings.runtimeFallbackReasonMask` 和
  `trim_warnings.runtimeFallbackSuppressedReasonMask`。可见 warning 聚合未被 suppression 过滤的
  `ZR_AOT_RUNTIME_FALLBACK_WARNING_*` reason bits；被抑制 warning 聚合已被 writer-level 或 reason-mask
  suppression 隐藏的 reason bits，使消费端无需解析逐条 visible warning 就能区分“没有 warning”和“被抑制”。
  RED/GREEN：RED 为 dynamic deopt bridge smoke 先要求 dynamic-call visible mask=1、全局 suppression
  后 suppressed mask=1、reason-mask suppression 后 suppressed mask=1、dynamic-value-access visible
  mask=2，旧生成器缺少新 header marker，focused WSL gcc 失败 4/7。GREEN 后 runtime fallback
  diagnostics helper 聚合 visible/suppressed reason mask，C emitter 输出两个 marker，既有逐条 warning
  记录、full-AOT 拒绝和 hybrid fallback 生成行为保持不变。
  验证：WSL gcc/clang 均通过 dynamic deopt bridge smoke 7/0 与 code stripping 5/0；Windows MSVC
  Debug 构建同组目标，dynamic deopt bridge smoke 为 0 failures/7 ignored，code stripping 5/0；
  focused CTest `aot_c_code_stripping` 在 WSL gcc 与 Windows MSVC Debug 均为 1/1。`git diff --check`
  退出 0，仅报告既有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-06-30-aot-12-s7zt-runtime-fallback-reason-mask-aggregates.md`。
  备注：本切片只补 runtime fallback trim warning 的 header-level reason 可观测性，不实现
  `@requires_unreferenced_code`、reflection data-flow annotation、annotation-based warning
  suppression/promotion、完整 trim analyzer 或新的裁剪决策。

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
