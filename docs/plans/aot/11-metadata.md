---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 参照 hybridclr/mono/roslyn 完善元数据
  - decision: 2026-06-20 元数据默认最小 + 注解保留；zrp 两段式（数据元数据 + 代码注册表）
references:
  - lua/hybridclr/libil2cpp/vm/GlobalMetadataFileInternals.h   # global-metadata.dat 头/表
  - lua/hybridclr/libil2cpp/il2cpp-metadata.h                  # CodeRegistration / MetadataRegistration 分离
  - lua/mono/mono/metadata/metadata-internals.h               # MonoImage / 表 / token 缓存
  - lua/mono/mono/mini/aot-runtime.h                          # MonoAotFileInfo
  - lua/runtime/src/coreclr/tools/aot/ILCompiler.MetadataTransform/ILCompiler/Metadata/MetadataTransform.cs
  - lua/roslyn/src/Compilers/Core/Portable/PEWriter/MetadataWriter.cs   # TypeSpec/MethodSpec
related_code:
  - zr_vm_core/include/zr_vm_core/metadata_token.h     # 8 表 token 体系 + 签名节点 + TokenBinding
  - zr_vm_core/src/zr_vm_core/function_metadata_query.c # low-level token record query reused by 11-S3A
  - zr_vm_core/include/zr_vm_core/zrp_metadata.h       # 11-S1A..11-S1J zrp metadata header + definition table directory + mmap section/pool/string view + row/range/signature-blob validation + pool/table payload writer ABI; 11-S4J TypeSpec row typeLayoutId binding
  - zr_vm_core/src/zr_vm_core/zrp_metadata.c           # 11-S1A..11-S1J header read/write/validate + section/pool/string view + definition-table/range validation + pool/table payload writer + signature blob structural validator
  - zr_vm_core/include/zr_vm_core/function.h           # SZrFunctionMetadata / ModuleEffect; 11-S4G function-level code-registration layout registry binding for GC inline-frame consumers
  - zr_vm_core/include/zr_vm_core/type_layout.h        # cTypeId / SZrTypeLayoutMetadata
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h   # 11-S2C minimal metadata runtime registration carrier; 11-S3A..11-S3M method/field/type/signature/TypeSpec record cache API + zrp metadata mmap attach/query + validated signature blob/header/type-node/generic TypeSpec signature/base-token/argument binding view + MethodSpec signature view; 11-S4A/11-S4C TypeDef token/cTypeId/layout binding view backed by code-registration layout registry; 11-S4D public typeLayoutId -> SZrTypeLayout resolver; 11-S4E generic dictionary consumer input; 11-S4F public typeLayoutId -> SZrAotGcDescriptor resolver; 11-S4G function-level layout resolver for GC inline-frame consumers; 11-S4H function+prototype -> registry-backed type layout resolver for reflection consumers; 11-S4I FieldDef token/row/offset/layout binding view; 11-S4J TypeSpec token/generic-binding/layout binding view; 11-S4K TypeDef/TypeSpec token -> layout cache resolver; 11-S4L typeLayoutId -> TypeDef/TypeSpec token reverse resolver; 11-S4M bounded multi-entry type-layout cache; 11-S4N cTypeId -> token resolver; 11-S4O code-registration type-layout token count mirror; 11-S6A token binding compatibility status/report API; 11-S6B function-level binding scan API
  - zr_vm_core/src/zr_vm_core/metadata_runtime.c       # 11-S3A..11-S3M ResolveMethodRecord/ResolveFieldRecord/ResolveTypeRecord/ResolveSignatureRecord lazy token record lookup/cache, local TypeSpec records, zrp section-view attach/query, signature blob validation, method/field signature header view parsing, nested type-node view parsing, generic TypeSpec signature view parsing, generic base-token binding, indexed generic argument binding, and MethodSpec signature view parsing; 11-S4D public type-layout resolver; 11-S4E/11-S4F generic dictionary and GC descriptor consumers reuse the same resolver; 11-S4G/11-S4H function/prototype-context layout resolver for attached AOT code-registration functions
  - zr_vm_core/src/zr_vm_core/metadata_runtime_layout_binding.c # 11-S4A..11-S4O split layout-binding implementation for TypeDef/TypeSpec/FieldDef row lookup, registry-backed binding views, TypeDef/TypeSpec token -> layout cache resolver, typeLayoutId/cTypeId -> token reverse lookup, bounded multi-entry cache, and code-registration token-table consumption
  - zr_vm_core/src/zr_vm_core/metadata_runtime_binding_compatibility.c # 11-S6A runtime ABI drift predicate for module version range + token/signature/module/layout compatibility; 11-S6B function binding scan over attached module metadata bindings
  - zr_vm_core/include/zr_vm_core/module.h             # 11-S2C module metadata runtime attach/query surface; 11-S4D typeLayoutCount runtime mirror
  - zr_vm_core/src/zr_vm_core/module/module.c          # 11-S2C module metadata runtime attach/query implementation; 11-S4D codeRegistration typeLayoutCount attach; 11-S4G function attach during module metadata runtime attach; 11-S4O typeLayoutTokenCount attach
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c             # 11-S2C module metadataRuntime.metadataFunction mark; 11-S4G mark inline-frame resolver prefers metadata runtime for attached AOT functions
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c            # 11-S2C module metadataRuntime.metadataFunction rewrite; 11-S4G rewrite inline-frame resolver prefers metadata runtime for attached AOT functions
  - zr_vm_core/src/zr_vm_core/reflection.c             # 10-S4A / 11-S4H reflection type/member layout consumers read registry-backed SZrTypeLayout when an AOT registry is attached
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h     # 11-S2A SZrAotCodeRegistration ABI + module carrier; 11-S4B code registration type-layout registry ABI; 11-S4F GC descriptor registry consumer source; 11-S4O type-layout token carrier ABI
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c # 12-S7Y default-min reflection metadata policy option helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_internal.h # 12-S7Y reflection metadata policy option API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h # 11-S6D..11-S6G i64/u64/f64/bool typed direct-call writer prototypes carry function-slot metadata for deopt fallback
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c # 11-S2A generated-C code registration emission; 11-S4B type-layout registry pointer/count emission; 11-S4O type-layout token table pointer/count emission; 12-S7K zrp metadata size attribution consumer; 12-S7Y metadata policy marker/plumbing; 12-S7Z emitted zrp metadata pruning plumbing
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c # 11-S6D no-arg i64 typed direct call passes function slot to guard/deopt writer
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c # 11-S6D..11-S6F i64/u64/f64 typed direct-call generated metadata guard and stack-call deopt fallback
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_bool_calls.c # 11-S6G bool-result typed direct-call generated metadata guard and stack-call deopt fallback
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.h # 11-S6D typed direct-call dispatch signature carries function slot for deopt fallback
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c # 11-S6D..11-S6G typed direct-call dispatch threads function slot into i64/u64/f64/bool guard/deopt writers
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c # 11-S6H inline-struct CALL_TYPED generated metadata guard and dynamic deopt bridge fallback
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_size.h # 12-S7T zrp metadata size accounting API; 12-S7Z pruned-blob stats input; 12-S7ZO section-level trim delta marker surface; 12-S7ZP section count marker fields
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_size.c # 12-S7T zrp metadata header sampling; 12-S7Z blob-based after-stats sampling; 12-S7ZO per-section trim delta marker writing; 12-S7ZP per-section count stats/delta marker writing
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
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.h # 11-S4B generated-C SZrTypeLayout descriptor + registry writer surface; 11-S4O token table writer surface; 11-S4P/11-S4Q generated layout resolver surface
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c # 11-S4B generated-C SZrTypeLayoutField/SZrTypeLayout descriptors + sparse cTypeId/typeLayoutId registry; 11-S4P struct/union descriptor emission and registry resolver; 11-S4R generated ownership offset arrays
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_tokens.c # 11-S4P/11-S4Q generated-C TypeDef/TypeSpec-backed typeLayout token population
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c # 11-S2A shared invoker table emission; 12-S7Y policy-driven MethodInfo reflection level emission
  - zr_vm_library/include/zr_vm_library/aot_runtime.h    # 11-S2B generated frame/context codeRegistration carrier; 11-S4E generic TYPE_LAYOUT/SIZEOF slot API takes SZrMetadataRuntime; 11-S6D..11-S6H typed direct-call guard/deopt runtime API
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_generic_dictionary.c # 11-S4E generic dictionary TYPE_LAYOUT/SIZEOF resolves via metadata runtime layout resolver
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_return.c # 11-S6D..11-S6H typed direct-call metadata compatibility guard plus scalar stack-call and inline-struct dynamic deopt helpers
  - zr_vm_library/src/zr_vm_library/aot_runtime.c        # 11-S2A descriptor/codeRegistration validation; 11-S2B runtime record/context registration consumption; 11-S2C metadata runtime attach; 11-S4B type-layout registry validation; 11-S4G loaded function table attaches function-level metadata registry; 11-S4O type-layout token table validation; 11-S6C dynamic AOT module-load binding compatibility reject
  - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c # 11-S2A mirrored descriptor/codeRegistration validation; 11-S4B mirrored type-layout registry validation; 11-S4O mirrored type-layout token table validation
  - tests/module/test_metadata_runtime_query.c         # 11-S2C module metadata runtime registration query; 11-S3A..11-S3M method/field/type/signature/TypeSpec token lazy/cache query + zrp metadata mmap attach/view + signature blob/header/type-node/generic TypeSpec signature/base-token/argument binding view + MethodSpec signature view query; 11-S4A/11-S4C TypeDef layout binding view + code-registration registry source proof; 11-S4D attach typeLayoutCount mirror check; 11-S4I FieldDef layout binding view and no-prototype-fallback regression
  - tests/module/test_metadata_runtime_binding_compatibility.c # 11-S6A/S6B token binding ABI drift predicate and function scan coverage for version range, module/member signature, token identity, layout identity, AssemblyRef->Module mapping, invalid binding, and first incompatible binding
  - tests/module/test_aot_runtime_typed_direct_call_compatibility.c # 11-S6D..11-S6H typed direct-call runtime guard coverage for caller/callee binding drift
  - tests/parser/test_aot_c_metadata_binding_loader.c # 11-S6C dynamic AOT module loader reject coverage for embedded/zro module metadata binding drift
  - tests/module/test_metadata_runtime_typespec_layout.c # 11-S4J focused TypeSpec layout binding view and no-prototype-fallback regression; 11-S4K TypeDef/TypeSpec token -> layout cache coverage; 11-S4L typeLayoutId -> token reverse lookup coverage; 11-S4M multi-entry cache coverage; 11-S4N cTypeId -> token coverage; 11-S4O code-registration token table coverage
  - tests/module/test_metadata_runtime_type_layout.c   # 11-S4D focused public typeLayoutId -> SZrTypeLayout resolver coverage; 11-S4F focused public typeLayoutId -> SZrAotGcDescriptor resolver coverage; 11-S4G function-level layout resolver coverage; 11-S4H prototype layout resolver + reflection consumer source contract
  - tests/gc/gc_tests.c                               # 11-S4G non-AOT inline-frame GC fallback regression
  - tests/parser/test_aot_c_frame_setup_contracts.c    # 11-S2A..11-S2C ABI/emitter/runtime source contract; 11-S4B type-layout registry ABI/emitter source contract; 11-S4O type-layout token carrier ABI/emitter source contract
  - tests/parser/test_aot_c_call_contracts.c           # 11-S6D..11-S6G generated-source contract for i64/u64/f64/bool typed direct-call metadata guard/deopt fallback
  - tests/parser/test_aot_c_value_semir_contracts.c    # 11-S6H inline-struct CALL_TYPED generated-source contract for metadata guard/dynamic deopt fallback
  - tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c # 11-S6D i64 typed direct-call shared-library regression for guarded direct-call path
  - tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c # 11-S6E u64 typed direct-call shared-library regression for guarded direct-call path
  - tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c # 11-S6F f64 typed direct-call shared-library regression for guarded direct-call path
  - tests/parser/test_aot_c_typed_direct_call_bool_shared_library_smoke.c # 11-S6G bool typed direct-call shared-library regression for guarded direct-call path
  - tests/parser/test_aot_c_type_layout_contracts.c    # 11-S4B/11-S4P/11-S4R generated type-layout descriptor/token/ownership-offset writer source contracts
  - tests/parser/test_aot_c_generic_reference_sharing.c # 08-S4 generic dictionary acceptance; 11-S4E metadata-runtime-backed TYPE_LAYOUT/SIZEOF resolver regression
  - tests/parser/test_aot_c_source_contracts.c         # 11-S2A public ABI source contract; 11-S4B public type-layout registry source contract; 11-S4O/11-S4P public token table source contract; 12-S7ZE GenericParamConstraint remap module contract; 12-S7ZF MethodSpec remap module contract; 12-S7ZG signature blob remap module contract; 12-S7ZH section/string-pool module contract; 12-S7ZO section-level delta marker source contract; 12-S7ZP section-count marker source contract
  - tests/parser/test_aot_c_shared_library_smoke.c     # 11-S2A runtime descriptor/codeRegistration assertion; 11-S4B empty registry assertion; 11-S4O empty token table assertion; 11-S6C normal empty-binding loader regression
  - tests/parser/test_aot_c_value_type_shared_library_smoke.c # 11-S4B generated type-layout registry + GC descriptor alignment assertion; 11-S4O generated token table shape assertion; 11-S4P TypeDef-backed nonzero union token assertion; 11-S4R generated ownership-offset table assertion
  - tests/parser/test_aot_c_generic_call_typed.c       # 08-S5 generic call typed coverage; 11-S4Q TypeSpec-backed generated type-layout token assertion; 11-S6H shared/full-AOT inline-struct CALL_TYPED metadata guard assertion
  - tests/parser/test_aot_c_code_stripping.c           # 12-S7K generated-C zrp metadata section/table/pool byte statistics; 12-S7Y default-min MethodInfo reflection metadata policy; 12-S7Z zrp MethodDef metadata pruning; 12-S7ZG signature blob pool after-trim delta; 12-S7ZH string pool after-trim delta; 12-S7ZI constant pool after-trim delta
  - tests/parser/test_aot_c_zrp_metadata_pruning.c     # 12-S7ZA direct zrp MethodDef/token-record pruning; 12-S7ZB FieldDef shared MEMBER_DEF remap; 12-S7ZC GenericParam owner/range remap; 12-S7ZE GenericParamConstraint cascade remap; 12-S7ZF MethodSpec method-token cascade remap; 12-S7ZG MethodSpec signature blob rewrite/compaction
  - tests/parser/test_aot_c_zrp_metadata_export_token_remap.c # 12-S7ZN direct exported MethodDef/FieldDef member token remap coverage
  - tests/parser/test_aot_c_zrp_metadata_size_deltas.c # 12-S7ZO direct section-level zrp metadata before/after/removed marker coverage; 12-S7ZP direct section count marker coverage
  - tests/parser/test_aot_c_zrp_metadata_pool_pruning.c # 12-S7ZH direct zrp string-pool compaction/remap; 12-S7ZI direct zrp orphan constant-pool sweep; 12-S7ZL retained duplicate string-slice compaction; 12-S7ZM no-MethodDef-prune pool compaction trigger
  - tests/cli/test_cli_args.c                         # 11-S7W/11-S7X/11-S7Y CLI dump/diff/version-check mode parse/exclusivity coverage
  - tests/cli/test_cli_zrp_metadata_dump.c            # 11-S7W/11-S7X/11-S7Y zrp metadata dump/diff/version-check summary/path coverage
  - tests/CMakeLists.txt                               # 11-S6A/S6B metadata runtime binding compatibility test target/CTest; 11-S6C AOT metadata binding loader target/CTest; 11-S6D AOT runtime typed direct-call compatibility target/CTest; 11-S7W/11-S7X/11-S7Y CLI zrp metadata dump/diff/version-check target; 12-S7ZA/12-S7ZH/12-S7ZN/12-S7ZO/12-S7ZP Windows shared-DLL direct zrp pruning/remap/size tests link private pruning/remap/section/signature/string-pool/size modules
  - tests/parser/test_aot_c_descriptor_diagnostics.c   # 11-S2A missing codeRegistration diagnostic
  - zr_vm_library/include/zr_vm_library/project.h      # 11-S7A..11-S7F/11-S7K..11-S7M project manifest normalized dependency/reference + identity + preserve + AOT mode/feature switch/generic preserve argument model
  - zr_vm_library/src/zr_vm_library/project/project.c  # 11-S7A..11-S7F/11-S7L manifestVersion + old dependencies/new references normalization + assembly identity gates + preserve/AOT/feature parser dispatch
  - zr_vm_library/src/zr_vm_library/project/project_import_resolver.c # 11-S7D dependency AssemblyRef identity query exposure
  - zr_vm_library/src/zr_vm_library/project/project_features.c # 11-S7L zrp feature switch parser
  - zr_vm_library/src/zr_vm_library/project/project_preserve.c # 11-S7E/11-S7K/11-S7M zrp preserve rule + feature condition + generic argument parser
  - zr_vm_library/src/zr_vm_library/project/project_aot_options.c # 11-S7F zrp aotMode declaration parser
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.h       # 11-S7G project aotMode -> AOT writer option bridge API
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c       # 11-S7G project aotMode -> requireFullAot injection helper
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.h   # 11-S7H..11-S7V AOT C emission + preserve root bridge API
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.c   # 11-S7H..11-S7V binary embedded blob + method/type/generic preserve bridge + feature-conditioned root gating + generic TypeSpec synthesis/open-base generic-instantiation/MethodSpec binding
  - zr_vm_cli/src/zr_vm_cli/command/command.h        # 11-S7W/11-S7X/11-S7Y CLI zrp metadata dump/diff/version-check mode carriers
  - zr_vm_cli/src/zr_vm_cli/command/command.c        # 11-S7W `--dump-zrp-metadata <file>`, 11-S7X `--diff-zrp-metadata <before> <after>`, and 11-S7Y `--check-zrp-metadata-version <file>` parse/exclusivity/help surface
  - zr_vm_cli/src/zr_vm_cli/app/app.c                # 11-S7W/11-S7X/11-S7Y CLI app dispatch to zrp metadata dump/diff/version-check runners
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.h # 11-S7W/11-S7X/11-S7Y zrp metadata dump summary, diff summary, and version-check API
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.c # 11-S7W read-only zrp metadata section summary; 11-S7X before/after section byte/count diff summary; 11-S7Y header magic/version/header-size/section-count compatibility check implementation
  - zr_vm_language_server_extension/schemas/zrp.schema.json # 11-S7C..11-S7F/11-S7K..11-S7M manifest schema parity for identity/dependency/preserve/aotMode/feature/generic fields
  - zr_vm_parser/include/zr_vm_parser/writer.h        # 11-S7I top-level callable flat-index resolve API；11-S7N..11-S7V manifest generic writer root carrier + TypeSpec/generic-instantiation/MethodSpec binding fields
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c # 11-S7I callable binding -> AOT flat index resolver；11-S7N/11-S7O manifest generic root diagnostics + TypeSpec token/hash comments；11-S7P full-AOT unbound generic TypeSpec gate；11-S7Q generic instantiation diagnostics；11-S7R full-AOT generic-instantiation gate；11-S7V MethodSpec diagnostics/full-AOT closure alternative；12-S7K zrp metadata size markers；12-S7Y metadata policy marker；12-S7Z..12-S7ZI emitted zrp metadata pruning
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_callable_provenance.h
  - tests/module/test_zrp_metadata_format.c            # 11-S1A..11-S1J round-trip + mmap-view + pool/table payload write + string view + definition table directory/view/token/range/signature-blob validation
  - tests/library/test_project_manifest_normalization.c # 11-S7A..11-S7F/11-S7K..11-S7M .zrp manifestVersion + mixed dependency/reference + assembly identity + preserve + aotMode/feature/generic normalization gates
  - tests/cli/test_cli_project_incremental.c          # 11-S7G/11-S7H manifest aotMode + CLI AOT C emission bridge
  - tests/cli/test_cli_aot_writer_options.c           # 11-S7I..11-S7V parsed method/type/generic/feature-conditioned preserve -> writer manifest root binding + generic TypeSpec synthesis/open-base generic-instantiation/MethodSpec binding/full-AOT gate
  - docs/parser-and-semantics/（zrp assembly manifest 设计）
---

# 11 · 元数据（zrp 两段式 + 运行期解析 + token↔C 三向表）

> 承接缺口：zr_vm 已有完整 token 体系（`metadata_token.h`：8 表 MODULE/TYPE_DEF/MEMBER_DEF/
> ASSEMBLY_REF/TYPE_REF/MEMBER_REF/TYPE_SPEC/SIGNATURE，签名节点含 `GENERIC_INST/OWNERSHIP/
> UNION/NULLABLE/...`，`SZrMetadataTokenRecord` + `SZrMetadataTokenBinding` 含版本/layoutHash）、
> function 元数据、AOT function table、callable provenance。**缺**：编译期元数据消除、
> token↔C type 显式映射、版本检查运行实现、泛型参数标准化编码、zrp manifest 实现、导出/导入 API。
> 本文确立 zrp 元数据格式与运行期解析，default 最小化（`12`）。

## 0. 两段式元数据（对标 il2cpp CodeRegistration / MetadataRegistration 分离）

zrp 装配产物的元数据分两段，职责清晰、可独立裁剪：

```
zrp assembly
├── 数据元数据（data metadata，只读、可 mmap、版本化）           ← 对标 global-metadata.dat / MonoImage 表
│     类型/方法/字段/泛型定义表 + 字符串池 + 签名 blob 池 + token 表
└── 代码注册表（code registration，AOT 编译产出、随 .so/.c 链接）  ← 对标 Il2CppCodeRegistration
      函数指针表 · invoker 表(10) · 泛型实例/字典表(08) · type layout 表 · type-layout token 表 · GC descriptor 表(09)
```

- 解释器/dynamic 路径主要读**数据元数据**；AOT/typed 路径主要用**代码注册表**；两者经 token 关联。
- 这与 il2cpp「静态数据 + 生成代码表」一致，也与 mono「MonoImage 表 + MonoAotFileInfo」对应。

## 1. 数据元数据格式（对标 ECMA 表 / global-metadata.dat）

- **头** `SZrZrpMetadataHeader`：magic、version、各表偏移+计数、池偏移（对标 `Il2CppGlobalMetadataHeader`）。
- **定义表**（沿用 `metadata_token.h` 的 8 表语义，落为紧凑只读数组）：
  TypeDef / MethodDef / FieldDef / GenericParam / GenericParamConstraint / TypeSpec / MethodSpec / ModuleRef。
- **池**：字符串池（名表，可被 `12` 裁剪）、签名 blob 池（`SZrMetadataTokenRecord.signatureBlob` 指入）、
  默认值/常量池。
- **token 编码**：沿用现有「高 8 位表 ID + 低 24 位 RID」（`metadata_token.h`），全程序唯一。

## 2. 代码注册表（对标 Il2CppCodeRegistration，编译期生成 C）

AOT 编译为每个 zrp 模块发射一份只读注册表（C 静态数组），是 typed 路径的入口：

```c
typedef struct SZrAotCodeRegistration {
    TZrUInt32 functionCount;
    const FZrAotEntryThunk *functionPointers;               /* 当前按 flat index；后续 token→函数 */
    const SZrAotMethodInfo *const *methodInfos;              /* 07§4，每函数描述符 */
    TZrUInt32 methodInfoCount;
    const FZrAotReflectionInvoker *invokers;                 /* 10§1，按签名分桶 */
    TZrUInt32 invokerCount;
    const SZrTypeLayout *const *typeLayouts;                 /* 11§4，按 typeLayoutId/cTypeId 索引 */
    TZrUInt32 typeLayoutCount;
    const TZrUInt32 *typeLayoutTokens;                       /* 11§4，按 typeLayoutId/cTypeId 索引 */
    TZrUInt32 typeLayoutTokenCount;
    const SZrAotGcDescriptor *const *gcDescriptors;          /* 09§1 sparse descriptor 表 */
    TZrUInt32 gcDescriptorCount;
} SZrAotCodeRegistration;
```

- 对标 il2cpp `genericMethodPointers/invokerPointers/codeGenModules/...`，但裁剪后只含可达项（`12`）。
- 注册表在模块加载时登记到运行期（`ZrLibrary_AotRuntime_*` 已有雏形，见 `backend_aot` 运行时）。
- 11-S2A 先落地 generated-C carrier：复用现有 sparse flat-index 函数表、MethodInfo 表、invoker 表与
  GC descriptor 表，并让运行期 descriptor validation 在解引用前拒绝缺失/不一致的 code registration；
- 11-S2B 将运行期加载记录、生成帧和生成模块上下文切到 `SZrAotCodeRegistration` 消费，
  让 MethodInfo、直接调用 thunk、meta 调用和 callable 常量物化都经同一注册表载体读取；
- 11-S2C 在 AOT 模块加载时为 `SZrObjectModule` attach 最小 `SZrMetadataRuntime`，携带 module、
  metadata function、代码注册表与 function/method/invoker/GC descriptor 计数，并让 GC 标记/搬迁维护
  metadata function 引用；token→函数/layout lazy 解析、结果缓存和 token↔layout 三向表留给后续
  11-S3/11-S4。

## 3. 运行期元数据解析（SZrMetadataRuntime，对标 il2cpp MetadataCache / mono MonoImage 缓存）

- 新增 `SZrMetadataRuntime`：持有 mmap 的数据元数据 + 代码注册表，提供 token → 运行期实体的 **lazy 解析**：
  `ResolveType/ResolveMethod/ResolveField(token)`，结果缓存（对标 mono `class_cache`/`method_cache`、
  il2cpp `MetadataCache::GetTypeInfoFromTypeIndex` 延迟初始化）。
- 这是 `10`（反射按 token）、`08`（泛型字典 slot 解析）、`12`（裁剪后实体查找）的共同底座。
- 11-S3A 先落地 method token 的 record 层 lazy resolve：`SZrMetadataRuntime` 通过 attached
  metadata function 的本地 `metadataTokenRecords` 解析 `MEMBER_DEF`，通过 module metadata ref 表解析
  `MEMBER_REF`，并缓存最近一次 `ResolveMethodRecord` 命中；`ResolveType`、`ResolveField`、data metadata
  mmap 查询、signature semantic resolution 和运行期实体物化留给后续 11-S3 切片。
- 11-S3B 以同一模式补 type token 的 record 层 lazy resolve：本地 `TYPE_DEF` 从 attached metadata
  function token 表解析，模块 `TYPE_REF` 从 module metadata ref 表解析，并缓存最近一次
  `ResolveTypeRecord` 命中；TypeSpec、字段 token、layout/entity materialization 仍不在本切片范围。
- 11-S3C 补 entity token → `SIGNATURE` record 的 lazy/cache 查询：先查 attached metadata function 的本地
  signature record，再查 module metadata ref 表的 signature record；这只取得签名记录，不解析签名 blob
  的 method/field/type 语义。
- 11-S3D 将 type record 层 lazy/cache 查询扩到本地 `TYPE_SPEC` token，让泛型/闭包类型签名先以
  token record 形式进入 `ResolveTypeRecord` 缓存；这仍不解析 `GENERIC_INST` signature blob、不绑定运行期
  generic instantiation，也不物化 layout/type entity。
- 11-S3E 补 field token 的 record 层 lazy resolve：字段定义/引用当前复用 `MEMBER_DEF` / `MEMBER_REF`
  token 表，因此 `ResolveFieldRecord` 先按 member token 从本地或 module metadata ref 表取 record，并使用独立
  field cache；这仍不解析 `FIELD_SIG` blob，也不区分 method/field 语义或物化字段实体。
- 11-S3F 把已验证的 zrp data metadata buffer/header 附加到 `SZrMetadataRuntime`，并提供
  `ZrCore_MetadataRuntime_GetZrpSectionView()` 从 runtime header 获取只读 mmap section view；本切片只暴露
  raw table/pool view，不解析 row 语义、不把数据元数据物化为运行期 type/method/field entity。
- 11-S3G 将 entity token → paired `SIGNATURE` record → signature blob pool slice 串起来：
  `ZrCore_MetadataRuntime_GetSignatureBlob()` 从 attached zrp metadata 的 signature blob pool 取只读 slice，
  并复用 11-S1J 的结构校验；本切片仍不构建 signature AST，也不解析 TypeSpec/generic/FIELD_SIG 语义。
- 11-S3H 在 validated signature blob slice 上补顶层 method/field signature header view：
  `ZrCore_MetadataRuntime_ReadSignatureView()` 读取 root node、调用约定/flags、generic parameter count、
  method parameter count、return type/parameter list/field type 在 blob 内的偏移；nested type node 仍只跳过不物化。
- 11-S3I 在 validated signature blob slice 上补 nested type-node 只读 view：
  `ZrCore_MetadataRuntime_ReadSignatureTypeNode()` 从 blob 内任意 type-node 偏移读取 node kind、payload、
  base type/child list 偏移、child count 与 next offset；当前关闭 primitive、TYPE_REF/TYPE_DEF、GENERIC_INST
  等结构视图，不绑定 TypeSpec/generic 语义，也不物化运行期 type/layout/entity。
- 11-S3J 在 `TYPE_SPEC` token 上补 generic TypeSpec signature 只读 view：
  `ZrCore_MetadataRuntime_ReadTypeSpecSignatureView()` 串起 TypeSpec record、paired `SIGNATURE` record、
  validated signature blob slice 与 root `GENERIC_INST` type-node，暴露 TypeSpec/signature token、signature hash、
  generic root node、base type node、argument count 和 argument-list blob offset；当前仍不物化 base token、
  layout/type entity、generic dictionary 或 row-to-entity 缓存。
- 11-S3K 在 generic TypeSpec signature view 上补 base-token binding view：
  `ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView()` 将 root `GENERIC_INST` 的 base `TYPE_REF/TYPE_DEF`
  node 与现有 metadata type record 的签名 blob 匹配，暴露 base token/base record；当前仍不标准化 argument
  semantic binding，不物化 layout/type entity、generic dictionary、MethodSpec 或 row-to-entity 缓存。
- 11-S3L 在 generic TypeSpec binding view 上补 indexed argument view：
  `ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView()` 按参数索引遍历 `GENERIC_INST` argument list，
  暴露 argument type-node；直接 `TYPE_REF/TYPE_DEF` 参数会匹配现有 type record 并暴露 argument token/record，
  primitive/复合节点只保留节点视图。当前仍不递归绑定嵌套 generic argument，不物化 generic dictionary、
  layout/type entity、MethodSpec 或 row-to-entity 缓存。
- 11-S3M 在 `SIGNATURE` token 表上补 MethodSpec signature view：
  `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView()` 读取 MethodSpec direct signature record，要求其
  related/owner 指向 method token，签名体为 `GENERIC_INST(MEMBER_REF methodToken, args...)`，并暴露
  methodSpec token、method token/record、signature hash、method node、argument count 与 argument-list offset。
  当前仍不改 MethodSpec token 编码，不物化 method instantiation、generic dictionary 或 row-to-entity 缓存。

## 4. token ↔ cTypeId ↔ ZrLayout 三向映射（单一真相落地）

现状 `cTypeId` 存在但无公开「token → C type」表。补一张三向表（不变量 C 的物化）：

```
metadataToken  ⇄  cTypeId  ⇄  struct ZrLayout_<cTypeId>
        ↑ 数据元数据 TypeDef        ↑ 生成 C 类型 / layout / GC descriptor
```

- 反射（`10`）、泛型字典（`08` TYPE_LAYOUT slot）、GC descriptor（`09`）全部经此表取同一 layout，
  禁止各自硬编码偏移（`01`§不变量 C）。
- 编译期发射该表为代码注册表的一部分；运行期经 `SZrMetadataRuntime` 索引。
- 11-S4A 先落地 TypeDef-backed 的只读 identity view：
  `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()` 从 attached zrp metadata 的 `TYPE_DEFS` section
  匹配 `TYPE_DEF` token，绑定现有 type token record、`SZrZrpMetadataTypeDefRow.typeLayoutId`、`cTypeId`、
  layout version/hash，并且只在 metadata function 已有 `prototypeFrameTypeLayouts[typeLayoutId]` 且
  `SZrTypeLayout.cTypeId` 与 row `typeLayoutId` 一致时暴露 cached `SZrTypeLayout` 指针。当前仍不处理
  `TYPE_SPEC`/generic layout materialization，不构建 code-registration layout registry，不在 runtime 内
  产生 layout 构建副作用，也不声明反射/泛型/GC 已强制统一读取同一 layout 表。
- 11-S4B 将 code registration 扩展为 generated-C type layout registry：ABI 9 在
  `SZrAotCodeRegistration` / `ZrAotCompiledModule` 暴露 `typeLayouts/typeLayoutCount`；
  AOT C 后端为可达 inline struct layout 发射 `SZrTypeLayoutField`、`SZrTypeLayout` 和按
  `typeLayoutId/cTypeId` 索引的稀疏 `zr_aot_type_layouts[]`，并让 module descriptor 与 code registration
  指向同一表；runtime descriptor validation 拒绝空/非空形态不一致和 descriptor/codeRegistration
  指针或计数不一致。当前仍不处理 TypeSpec/generic layout materialization、ownership offset 表发射、
  runtime layout construction、反射/泛型/GC 消费端强制改读该表和完整 token/cTypeId/layout cache。
- 11-S4C 在不扩展 API 的前提下，将 `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()` 的
  `typeLayout` 来源切换为 `runtime->codeRegistration->typeLayouts[typeLayoutId]`，不再从
  `metadataFunction->prototypeFrameTypeLayouts` 返回 layout 指针；因此 TypeDef row、cTypeId、layout hash/version
  的 identity view 现在读取 11-S4B 发射并经 module load 绑定的同一 layout registry。当前仍不处理
  TypeSpec/generic layout materialization、ownership offset 表发射、runtime layout construction、
  反射/泛型/GC 消费端强制改读该表和完整 token/cTypeId/layout cache。
- 11-S4D 将 registry-backed layout lookup 提升为 runtime 公共入口：
  `ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, typeLayoutId)` 经 `SZrMetadataRuntime.typeLayoutCount`
  和 `runtime->codeRegistration->typeLayouts[typeLayoutId]` 解析 layout，只在 table 非空、索引未越界、
  非 `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`、稀疏项非空且 `SZrTypeLayout.cTypeId == typeLayoutId` 时返回指针；
  `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()` 改为复用该入口，prototype layout cache 不作为 fallback。
  当前仍不处理 TypeSpec/generic layout materialization、ownership offset 表发射、runtime layout construction、
  反射/泛型/GC 消费端强制改读该表和完整 token/cTypeId/layout cache。
- 11-S4E 将 08 的泛型字典 TYPE_LAYOUT/SIZEOF consumer 接入同一 runtime layout resolver：
  `ZrLibrary_AotRuntime_GenericSlot_TypeLayout()` 与 `TryGetSizeOf()` 不再接收 metadata function 或读取
  `metadataFunction->prototypeFrameTypeLayouts`，而是接收 `SZrMetadataRuntime*` 并调用
  `ZrCore_MetadataRuntime_ResolveTypeLayout()`；generated C 的 shared generic TYPE_LAYOUT 访问宏同步传入
  `metadataRuntime`。当前仍不处理 TypeSpec/generic layout materialization、ownership offset 表发射、
  runtime layout construction、反射/GC 消费端强制改读该表和完整 token/cTypeId/layout cache。
- 11-S4F 将 09 的 code-registration GC descriptor lookup 提升为 runtime 公共入口：
  `ZrCore_MetadataRuntime_ResolveGcDescriptor(runtime, typeLayoutId)` 经 `SZrMetadataRuntime.gcDescriptorCount`
  和 `runtime->codeRegistration->gcDescriptors[typeLayoutId]` 解析 descriptor；只有 descriptor 非空、
  非 `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`、索引未越界、`descriptor->typeLayoutId == typeLayoutId`，
  且同一 `typeLayoutId` 可由 `ZrCore_MetadataRuntime_ResolveTypeLayout()` 解析到 registry-backed layout 时
  才返回指针。descriptor lookup 不读取 `metadataFunction->prototypeFrameTypeLayouts`，也不会在 layout
  registry 缺失时 fallback 到 prototype cache。GC inline-frame scanning 迁移已由 11-S4G 后续关闭，反射消费端迁移
  已由 11-S4H 后续关闭；当前仍不处理 TypeSpec/generic layout materialization、ownership offset 表发射、
  runtime layout construction 和完整 token/cTypeId/layout cache。
- 11-S4G 将 GC inline-frame mark/rewrite consumer 迁到 function-level metadata runtime layout resolver：
  `ZrCore_MetadataRuntime_AttachFunction(runtime, function)` 把 static code-registration registry 指针和 layout/
  descriptor 计数附加到 `SZrFunction`，AOT 模块加载完成后对 loaded function table 全量 attach；
  `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(function, typeLayoutId)` 优先使用函数自身的 attached registry，
  未绑定时读取 `prototypeContextFunction` 上的 registry。GC mark/rewrite 的 inline-frame resolver 对已绑定
  AOT registry 的函数只读 code-registration layout table，registry layout 缺失时不 fallback 到
  prototype layout cache；普通 VM/interpreter 函数没有 attached registry 时保留旧
  `ZrCore_Function_ResolvePrototypeFrameTypeLayout()` fallback。反射消费端迁移已由 11-S4H 后续关闭；当前仍不处理
  TypeSpec/generic layout materialization、ownership offset 表发射、runtime layout construction 和完整
  token/cTypeId/layout cache。
- 11-S4H 将 10 的反射 type/member layout consumer 迁到 function+prototype metadata runtime layout resolver：
  `ZrCore_MetadataRuntime_ResolveFunctionPrototypeTypeLayout(function, prototype, outTypeLayoutId)` 通过函数自身或
  prototype-context entry function 的 prototype 实例表映射 `typeLayoutId`，再调用
  `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout()` 读取 attached code-registration layout registry；无 registry、
  无 prototype 映射或 registry layout 缺失时返回 null，不 fallback 到 prototype layout cache。`reflection.c`
  在构建脚本 type reflection 和 decorator target member reflection 时消费该 layout；类型级
  `layout.fieldCount/size/alignment` 从 `SZrTypeLayout` 写入，字段级 `offset/size/layout` 从
  `SZrTypeLayout.fields[i]` 按实例字段序号写入。当前仍不处理 TypeSpec/generic layout materialization、
  ownership offset 表发射、runtime layout construction 和完整 token/cTypeId/layout cache；10-S4 的泛型参数反射、
  token-driven 字段实体和类型实参暴露仍待后续。
- 11-S4I 新增 FieldDef-backed 的 layout binding view：
  `ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(runtime, fieldDefToken, outView)` 从 attached zrp metadata
  的 `FIELD_DEFS` section 匹配 `MEMBER_DEF` field token，绑定 field token record、FieldDef row、owner
  `TYPE_DEF` token/record/row、`byteOffset`、field `typeLayoutId` 和 owner `typeLayoutId`，并要求二者都能经
  `ZrCore_MetadataRuntime_ResolveTypeLayout()` 从 code-registration layout registry 解析；同时校验 field row
  索引落在 owner TypeDef row 的 `firstFieldDefIndex/fieldDefCount` 范围内。registry 缺失、field layout
  缺失或 owner layout 缺失时返回 false，不 fallback 到 `prototypeFrameTypeLayouts`。当前仍不处理
  TypeSpec/generic layout materialization、ownership offset 表发射、runtime layout construction、完整
  token/cTypeId/layout cache，且不声明 10-S4 的 token-driven 字段反射实体已经物化。
- 11-S4J 新增 TypeSpec-backed 的 layout binding view：
  `SZrZrpMetadataTypeSpecRow` 将原保留槽位收敛为 `typeLayoutId`，不改变 row 尺寸或 zrp metadata 版本；
  `ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(runtime, typeSpecToken, outView)` 从 `TYPE_SPEC` token
  解析 type record、paired signature record、zrp `TYPE_SPECS` row 和 11-S3K 的 generic base-token binding view，
  并要求 row 的 `signatureBlobOffset/signatureBlobLength/signatureHash` 与 signature/type token record 一致，
  再通过 `ZrCore_MetadataRuntime_ResolveTypeLayout()` 读取 code-registration layout registry 中的
  `SZrTypeLayout`。registry layout 缺失时返回 false，不 fallback 到 `prototypeFrameTypeLayouts`。当前仍只是
  TypeSpec token→row→generic binding→registry layout 的只读 view；不声明已完成泛型 layout 运行期构建、
  generic instantiation runtime materialization、ownership offset 表发射或完整 token/cTypeId/layout cache。
- 11-S4K 新增 TypeDef/TypeSpec token→layout 的 public resolver 和最近一次命中 cache：
  `ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, typeToken, outTypeLayoutId)` 只接受 `TYPE_DEF` 与
  `TYPE_SPEC` token。`TYPE_DEF` 通过 11-S4A/11-S4C 的 TypeDef layout binding view 解析，`TYPE_SPEC` 通过
  11-S4J 的 TypeSpec layout binding view 解析；两条路径都要求 code-registration registry 中存在
  `SZrTypeLayout`，不会从 `prototypeFrameTypeLayouts` fallback。成功后 runtime 缓存最近一次
  `typeToken/typeLayoutId/SZrTypeLayout*`，二次同 token 查询不再触碰 registry；失败时输出 layout id 重置为
  `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`。当前仍不是完整多项 token/cTypeId/layout cache，也不声明
  cTypeId→token 反查表、泛型 layout 运行期构建、ownership offset 表发射或 public generic reflection 完成。
- 11-S4L 在同一 cache 上补最小 layoutId→token 反查入口：
  `ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, typeLayoutId)` 先复用最近一次
  `typeToken/typeLayoutId/SZrTypeLayout*` 命中；未命中时扫描 attached zrp `TYPE_DEFS`，再扫描 `TYPE_SPECS`，
  并分别经 TypeDef/TypeSpec binding view 重新校验 row identity 与 registry-backed layout 存在后返回 token。
  `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`、缺 zrp metadata、缺 registry layout 或只有 stale prototype cache 时返回
  `0`。当前仍不是完整 cTypeId→token 索引表，也不声明跨模块反查、泛型 layout 运行期构建、ownership offset
  表发射或 public generic reflection 完成。
- 11-S4M 将 11-S4K/11-S4L 的最近一次命中扩展为 bounded multi-entry cache：`SZrMetadataRuntime` 现在保存
  `ZR_METADATA_RUNTIME_TYPE_LAYOUT_CACHE_CAPACITY == 8` 的 `typeLayoutCacheTokens/typeLayoutCacheIds/
  typeLayoutCacheLayouts` 数组和 round-robin `typeLayoutCacheNextIndex`。`ResolveTypeTokenLayout()` 先按 token
  查 cache，`ResolveTypeLayoutToken()` 先按 layout id 查 cache；未命中后仍经 TypeDef/TypeSpec binding view
  重新校验 registry-backed layout，再写回 cache。同一 runtime 可同时保留 TypeDef 与 TypeSpec 的
  token→layout 和 layoutId→token 命中，registry layout 被清空后仍能命中已缓存项；cache 满后按 bounded
  round-robin 覆盖。当前仍不是持久 cTypeId→token 索引表，也不声明 TypeSpec/generic layout materialization、
  ownership offset 表发射、runtime layout construction、跨模块 cache 或 public generic reflection 完成。
- 11-S4N 补出 public cTypeId→token 反查入口：
  `ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, cTypeId)` 复用当前 registry 约束下
  `cTypeId == typeLayoutId` 的三向表不变量，调用同一 bounded token/layout cache 与 TypeDef/TypeSpec
  binding-view 反查路径。该入口拒绝 null runtime、`ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE` 和缺 registry layout
  的 stale prototype cache 路径；TypeDef 与 TypeSpec cTypeId 命中可与 11-S4M cache 并存。当前仍不是持久
  cTypeId→token 索引表，也不声明 cTypeId 与 typeLayoutId 解耦、TypeSpec/generic layout materialization、
  ownership offset 表发射、runtime layout construction、跨模块 cache 或 public generic reflection 完成。
- 11-S4O 将 code registration 扩展为 typeLayout token carrier：ABI 10 在
  `SZrAotCodeRegistration` / `ZrAotCompiledModule` 暴露 `typeLayoutTokens/typeLayoutTokenCount`；
  generated C 发射按 `cTypeId/typeLayoutId` 索引的 `zr_aot_type_layout_tokens[]`，并让 module descriptor
  与 code registration 指向同一表。runtime descriptor validation 校验 descriptor/codeRegistration token 表
  指针/计数一致，且 token 表计数必须等于 `typeLayoutCount`。metadata runtime attach 镜像
  `typeLayoutTokenCount`，`ResolveTypeLayoutToken()` 与 `ResolveCTypeIdToken()` 在 zrp row scan fallback 前先读取
  code-registration token 表；表项只有在 token 为 TypeDef/TypeSpec 且对应 registry-backed layout 可解析时才被接受。
  本切片只关闭 ABI carrier、shape validation 和 runtime consumption path；真实 generated token 子集由 11-S4P 补齐。
- 11-S4P 将 generated `zr_aot_type_layout_tokens[]` 的可靠子集从占位 0 推进到真实 token：emitter 在同一
  generated-C function table 中按 `typeLayoutId/cTypeId` 解析 `SZrTypeLayout`，对 kind 为 struct/union 且能唯一匹配本地
  `TYPE_DEF` 记录的命名 layout 写入 `TYPE_DEF` token；缺 `TYPE_DEF` metadata、多重匹配、TypeSpec/generic layout
  仍保守写 0。AOT C type-layout descriptor 现在也能为 union layout 发射 runtime descriptor，以便本地 union TypeDef
  token 能绑定到实际 generated layout。当前仍不声明持久 cTypeId→token 索引表、TypeSpec/generic layout materialization、
  ownership offset 表发射或 runtime layout construction 完成。
- 11-S4Q 将 generated `zr_aot_type_layout_tokens[]` 的 TypeSpec-backed 可靠子集推进到真实 token：TypeDef token
  lookup 未命中后，token writer 会结构匹配同函数 `TYPE_SPEC` canonical signature blob，并为唯一匹配的 generated
  generic layout 写入真实 `TYPE_SPEC` token。缺 metadata、多重匹配、跨模块记录和 unsupported signature 仍保守写 0。
  当前仍不声明持久 cTypeId→token 索引表、runtime generic layout construction、cross-module token table 或 public
  generic reflection entity 完成。
- 11-S4R 将 generated `SZrTypeLayout` descriptor 的 ownership-offset 表从空指针占位推进到可消费数组：对
  `ownershipFieldCount > 0` 且 offset 可由 `ownershipFieldOffsets` 或 `fields[]` 中的
  `VALUE_SLOT | OWNERSHIP_VALUE` 字段安全导出的 struct layout，emitter 写出
  `ZrOwnershipOffsets_<typeLayoutId>[]`，并让 `.ownershipFieldOffsets` 指向该表；zero-count、union 和 unsafe/
  unsupported offset 路径保持 `ZR_NULL` 并输出显式 failure marker。当前仍不声明 union owner offset 表、持久
  cTypeId→token 索引表、runtime generic layout construction、cross-module token table 或 public reflection entity 完成。

## 5. 泛型参数标准化编码（衔接 08，对标 roslyn TypeSpec/MethodSpec）

- `GENERIC_INST` 签名节点标准化为：`baseToken + argCount + argSignatures[]`（对标 roslyn TypeSpec 签名）。
- 泛型方法实例 → MethodSpec 记录 `methodToken + instantiationSignature`（对标 MethodSpec）。
- 泛型参数定义/约束 → GenericParam / GenericParamConstraint 表（对标 ECMA、roslyn `ITypeParameterSymbol`）。
- 与 `08`§3 实例化表共用同一签名编码，去重键一致。

## 6. 版本检查运行实现（落地 SZrMetadataTokenBinding 既有字段）

现有 `SZrMetadataTokenRecord` 已有 `requestedModuleVersion/min/maxModuleVersion`，
`SZrMetadataTokenBinding` 已有 expected/resolved `layoutVersion/layoutHash/signatureHash/
moduleSignatureHash/token`，但无统一运行期校验流程。补：

- 模块加载/跨模块 token 解析时校验：版本落在 `[min, max)`、`layoutHash`/`signatureHash` 匹配。
- 不匹配处置（对标 mono AOT out_of_date）：dynamic 模块 → 拒绝加载/报错；typed 调用边界 → deopt 到
  解释器（`04`§6），用数据元数据动态解释。保证 ABI 漂移不致崩溃。

## 7. 元数据策略（对标 NativeAOT IMetadataPolicy，default 最小）

- 每实体「生成 def 还是仅 ref / 生成哪一级反射元数据」由策略决定：可达性（`12`）+ 反射级别（`10`§0）+ 注解。
- 默认：仅可达实体生成 def；被外部引用但内部不反射 → ref；未可达 → 不生成（对标
  `AnalysisBasedMetadataManager` + `MetadataCategory`）。

## 8. 导出/导入与 zrp manifest

- 形式化 `docs/parser-and-semantics/` 的 zrp assembly manifest：声明模块标识、版本、依赖、导出 token、
  保留规则（`12` link.xml 等价物）、AOT 模式（hybrid/full-AOT，`08`§6）。
- 提供 `zrp` 工具读写数据元数据（dump/diff/版本检查），便于跨模块 ABI 演进。

## 9. 落地切片

| 切片 | 内容 | 验收 |
|------|------|------|
| 11-S1 | zrp 数据元数据格式（头/表/池）+ 读写器（§1/§8） | 🚧 2026-06-24 部分完成：11-S1A 已定义 zrp metadata header/section ABI，并完成 header little-endian read/write/validate 与 mmap 只读视图校验；11-S1B 已把 header 目录扩展为 token + TypeDef/MethodDef/FieldDef/GenericParam/GenericParamConstraint/TypeSpec/MethodSpec/ModuleRef + 三个池，并定义紧凑定义表行结构与 element-size 校验；11-S1C 已提供按 section kind 解析只读 mmap payload 的 `SZrZrpMetadataSectionView`；11-S1D/11-S1E 已提供定义表 token/table tag 与 RID/range 基本一致性校验；11-S1F 已提供 string/signature/constant pool bounded slice 读取入口；11-S1G 已提供三类 pool payload 写入入口；11-S1H 已提供定义表 row payload 写入入口；11-S1I 已提供 string pool NUL-terminated view 解码入口；11-S1J 已提供 signature blob structural validation；编译期真实定义表/string/signature pool 导出、signature blob semantic/type/token resolution 与文件级 zrp manifest 读写仍待后续 |
| 11-S2 | 代码注册表发射 + 模块加载登记（§2） | 🚧 2026-06-25 部分完成：11-S2A 已完成 generated-C `SZrAotCodeRegistration` carrier、函数/method/invoker/GC descriptor 表指针登记、模块 descriptor 暴露，以及运行期 descriptor validation 对缺失/不一致 code registration 的拒绝；11-S2B 已让运行时 loaded-module record、generated module context、generated frame、direct/static/meta/callable thunk 路径经 `codeRegistration` 消费 function/method 表；11-S2C 已在 AOT 模块加载时 attach 最小 `SZrMetadataRuntime`，登记 module、metadata function、code registration 与表计数，并让 GC mark/rewrite 覆盖 metadata function；data metadata mmap attach、token→函数/layout lazy 解析和缓存仍待后续 11-S3/11-S4 |
| 11-S3 | `SZrMetadataRuntime` token lazy 解析 + 缓存（§3） | 🚧 2026-06-25 部分完成：11-S3A 已提供 `ZrCore_MetadataRuntime_ResolveMethodRecord()`，可从 attached metadata function 的本地 `MEMBER_DEF` 与 module `MEMBER_REF` token record 表 lazy 解析 method token，并用单项 cache 覆盖二次命中；11-S3B 已提供 `ZrCore_MetadataRuntime_ResolveTypeRecord()`，可从本地 `TYPE_DEF` 与 module `TYPE_REF` token record 表 lazy 解析 type token，并用独立单项 cache 覆盖二次命中；11-S3C 已提供 `ZrCore_MetadataRuntime_ResolveSignatureRecord()`，可按 entity token lazy 解析本地或 module `SIGNATURE` record 并缓存最近一次命中；11-S3D 已让本地 `TYPE_SPEC` record 进入 `ResolveTypeRecord()` 与同一个 type record cache；11-S3E 已提供 `ZrCore_MetadataRuntime_ResolveFieldRecord()`，按 `MEMBER_DEF` / `MEMBER_REF` record 解析字段 token 并使用独立 field cache；11-S3F 已提供 zrp data metadata mmap buffer/header attach 与 section-view 查询；11-S3G 已提供 entity token 到 validated signature blob pool slice 的查询；11-S3H 已提供 method/field signature 顶层 header view；11-S3I 已提供 nested signature type-node view（node/payload/base/child/next offsets）；11-S3J 已提供 generic TypeSpec signature view（TypeSpec/signature identity + GENERIC_INST base/argument offsets）；11-S3K 已提供 generic TypeSpec base-token binding view（base `TYPE_REF/TYPE_DEF` node 匹配现有 type record signature blob 并暴露 base token/record）；11-S3L 已提供 indexed generic argument binding view（argument type-node + direct `TYPE_REF/TYPE_DEF` argument token/record binding）；11-S3M 已提供 MethodSpec signature view（`SIGNATURE` token + `GENERIC_INST(MEMBER_REF methodToken, args...)` + method record binding）；recursive generic argument semantic binding、method instantiation materialization、row-to-entity materialization、token→运行期实体物化和完整缓存仍待后续 |
| 11-S4 | token↔cTypeId↔ZrLayout 三向表（§4） | 🚧 2026-06-26 部分完成：11-S4A 已提供 TypeDef token→zrp TypeDef row→typeLayoutId/cTypeId 的 runtime binding view；11-S4B 已把 generated-C code registration 扩展为 `typeLayouts/typeLayoutCount` registry，发射 `SZrTypeLayoutField`/`SZrTypeLayout` 静态描述符和稀疏 `zr_aot_type_layouts[]`，并让 runtime validation 校验 descriptor/codeRegistration registry 一致性；11-S4C 已让 TypeDef layout binding view 的 `typeLayout` 指针来自 `codeRegistration->typeLayouts[typeLayoutId]`，不再使用 prototype layout cache 作为运行期返回来源；11-S4D 已提供 `ZrCore_MetadataRuntime_ResolveTypeLayout()` 公共 runtime layout resolver，并让 TypeDef binding view 复用同一入口；11-S4E 已让泛型字典 TYPE_LAYOUT/SIZEOF slot 经 `SZrMetadataRuntime` 复用同一 resolver，不再 fallback 到 prototype layout cache；11-S4F 已提供 `ZrCore_MetadataRuntime_ResolveGcDescriptor()`，让 GC descriptor lookup 先经同一 runtime layout resolver 校验 layout registry；11-S4G 已让 GC inline-frame mark/rewrite 对 attached AOT registry 使用 metadata runtime layout resolver，并为非 AOT 函数保留 prototype fallback；11-S4H 已提供 function+prototype→registry-backed `SZrTypeLayout` resolver，并让反射 type/member layout 消费端在 attached registry 下读取 `SZrTypeLayout`；11-S4I 已提供 FieldDef token→zrp FieldDef row→owner/field layout 的 binding view，读取 FieldDef `byteOffset/typeLayoutId` 并强制 owner/field layout 都来自 code-registration registry；11-S4J 已提供 TypeSpec token→zrp TypeSpec row→generic base binding→registry layout 的 binding view，校验 row 与 signature/type record identity 一致并拒绝 prototype cache fallback；11-S4K 已提供 `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()`，按 TypeDef/TypeSpec token 解析 registry-backed layout，并缓存 token→layoutId/layout 命中；11-S4L 已提供 `ZrCore_MetadataRuntime_ResolveTypeLayoutToken()`，从 typeLayoutId 反查 TypeDef/TypeSpec token；11-S4M 已将 token/layout 命中扩展为 bounded 8-entry multi-entry cache，可同时保留 TypeDef 与 TypeSpec 的正向/反向命中；11-S4N 已提供 `ZrCore_MetadataRuntime_ResolveCTypeIdToken()`，按当前 cTypeId==typeLayoutId 不变量复用同一 cache/反查路径；11-S4O 已提供 code-registration `typeLayoutTokens/typeLayoutTokenCount` ABI carrier、generated-C token 表发射、runtime shape validation 和 table-first resolver 消费路径；11-S4P 已为唯一匹配本地 TypeDef 的 generated struct/union layout 写入真实 `TYPE_DEF` token，并为 union layout 发射 runtime descriptor；11-S4Q 已为同函数 metadata record stream 中可由 canonical signature 匹配的 generated generic/TypeSpec layout 写入真实 `TYPE_SPEC` token（例如 `Pair<int, int>` → `0x07000001u`），仍对缺 metadata、多重匹配、跨模块/unsupported signature 形态保守写 0。持久 cTypeId→token 索引内容、完整 TypeSpec/generic layout materialization、ownership offset 表和运行期 layout 构建仍待后续 |
| 11-S5 | 泛型参数标准化编码（GENERIC_INST/MethodSpec/约束）（§5） | 与 08 实例化去重键一致；解释器/AOT 同解 |
| 11-S6 | 版本检查运行实现 + 不匹配 deopt/拒绝（§6） | 🚧 2026-06-28 部分完成：11-S6A 已提供 `ZrCore_MetadataRuntime_CheckTokenBindingCompatibility()`，统一检查 token binding 的版本区间、module signature hash、metadata/signature token、signature hash、layoutVersion/layoutHash，并返回可供 dynamic 拒绝或 typed deopt 消费的 status/report；11-S6B 已提供 `ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility()`，可扫描 attached function 的 `moduleMetadataBindings` 并返回首个不兼容 binding/ref record/report；11-S6C 已将 function scan 接入 root AOT dynamic module loader，在 embedded/zro metadata binding 不兼容时拒绝加载并输出状态名/token/hash/layout 诊断；11-S6D 已为 i64 scalar typed direct-call 生成 runtime metadata guard，caller/callee binding 不兼容时经 `ZrLibrary_AotRuntime_DeoptTypedDirectCall()` 退回 `CallStackValue()` 并同步 i64 scalar local；11-S6E 已将同一 guard/deopt 形态扩展到 u64 scalar typed direct-call，兼容时保留 unsigned direct thunk，不兼容时同步 u64 scalar local；11-S6F 已将同一 guard/deopt 形态扩展到 f64 scalar typed direct-call，兼容时保留 float direct thunk，不兼容时同步 f64 scalar local；11-S6G 已将同一 guard/deopt 形态扩展到 bool-result typed direct-call，覆盖 bool/bool 与 i64/u64/f64 比较返回 bool，兼容时保留 bool direct thunk，不兼容时同步 bool scalar local；11-S6H 已将同一 guard/deopt 形态扩展到 value SemIR inline-struct `CALL_TYPED`，覆盖 ordinary direct call、shared generic METHOD-slot call 和 full-AOT collected shared callsite，兼容时保留 `CallInlineStruct()`，不兼容时经 `CallInlineStructDynamicDeoptBridge()` 回解释器并复制 inline return bytes；跨模块 token resolve 集成和更完整 no-crash drift 注入仍待后续 |
| 11-S7 | 元数据策略（默认最小）+ zrp manifest + 工具（§7/§8） | 🚧 2026-06-27 部分完成：11-S7A 已完成 `.zrp` project manifest normalization 的 `manifestVersion` 门禁，以及旧 `dependencies.$alias` 与新 `references.alias` 同值去重/冲突拒绝；11-S7B 已完成 assembly `publicKeyToken` 十六进制校验与小写归一化；11-S7C 已完成 legacy top-level `name`/`version` assembly identity shape gate 与 schema parity；11-S7D 已完成 legacy `dependencies.$alias.{assembly|name}` 声明 assembly 映射、目标 manifest identity mismatch 拒绝、alias package key 保持与 AssemblyRef identity 查询暴露；11-S7E 已完成 `.zrp` manifest `preserve` 规则的 declaration-level 解析、project model 暴露与 schema parity；11-S7F 已完成 `.zrp` manifest `aotMode` declaration-level 解析、project model 暴露与 schema parity；11-S7G 已完成 project `aotMode` 到 `SZrAotWriterOptions.requireFullAot` 的 CLI/compiler helper 注入；11-S7H 已完成 CLI `--emit-aot-c` AOT C 发射入口、binary input embedded blob 和 manifest v3 `aot_c` path tracking；11-S7I 已完成 `preserve` 的 `method` target 到 entry function top-level callable flat index 的绑定，并注入 writer manifest roots；11-S7J 已支持 dotted method target 精确匹配 callable name，并把 `type` preserve 的 `members: "methods"` / `"all"` 展开为同名前缀 callable roots；11-S7K 已为 `preserve` 规则添加 `feature` + boolean `featureValue` 条件声明模型、互相依赖校验与 schema parity；11-S7L 已新增 `.zrp` top-level `features` boolean switch map，并让 CLI AOT preserve root 注入按 feature 条件匹配启停；11-S7M 已为 `generic` preserve 添加非空 `arguments` 声明模型、project model 承载和 schema parity；11-S7N 已把 generic preserve target+arguments 注入 AOT writer options 并在 generated C 清单中输出；11-S7O 已把当前模块已有 `GENERIC_INST` `TYPE_SPEC` 记录绑定回 generic preserve root 的 TypeSpec/signature token/hash；11-S7P 已让 full-AOT writer 拒绝未绑定 TypeSpec 的 manifest generic preserve root；11-S7Q 已把 TypeSpec-backed generic preserve root 物化为 generic instantiation identity 并输出 base token / instance id / share kind；11-S7R 已让 full-AOT writer 拒绝 TypeSpec-only generic preserve root，要求 generic instantiation identity 同步存在；11-S7S 已让 current-module TypeSpec-backed generic instantiation 在存在同名 `TYPE_REF` metadata 时使用 open generic base token，并保留 closed TypeSpec 回退；11-S7T 已支持 `GENERIC_INST(TYPE_DEF target, args...)` TypeSpec binding，并让 current-module TypeDef base token 进入 generic instantiation identity；11-S7U 已在 manifest generic root 缺失 TypeSpec 但存在同名 open `TYPE_DEF`/`TYPE_REF` metadata 时合成 current-function TypeSpec/signature binding并继续物化 generic instantiation identity；11-S7V 已把 manifest generic method root 绑定到现有 current-module `GENERIC_INST(MEMBER_REF methodToken, args...)` MethodSpec 形态签名并输出 MethodSpec identity；11-S7W 已提供 CLI `--dump-zrp-metadata <file>` 只读工具，可输出 zrp metadata version/headerBytes/sectionCount 以及 12 个 section 的 bytes/count/elementSize/offset summary；11-S7X 已提供 CLI `--diff-zrp-metadata <before> <after>` 只读工具，可输出 zrp metadata version/headerBytes/sectionCount before/after 与 12 个 section 的 bytes/count before/after/removed diff summary；12-S7Y 已接入 generated MethodInfo 默认最小策略，opt-in code stripping 下输出 `reflectionMetadataLevel = NONE` 与 `metadata_policy.reflectionLevel` marker，默认/非裁剪仍保留 `RUNTIME_MAPPING`；12-S7Z..12-S7ZN 已逐步启用 emitted zrp metadata 的 MethodDef、token-record、FieldDef、GenericParam、GenericParamConstraint、MethodSpec method-token 剪枝级联，retained signature blob pool compaction/offset remap/hash recomputation/MethodSpec signature rewrite，string-pool retained-slice sweep、row offset remap 与 duplicate retained slice interning，当前 orphan constant-pool sweep，让 pool compaction 在 MethodDef count 不变时仍可触发，并提供 exported MethodDef/FieldDef member token 的 compacted-token remap surface；跨模块 target、cross-module export-token publication/rewrite、field/default-value backed constant-pool remap、完整 zrp metadata sweep/pruning 和版本检查仍待后续 |

## 10. 不变量校验

- **C 单一真相**：token↔layout 三向表是偏移/大小/签名的唯一来源；反射/泛型/GC/序列化全部经它。
- **B 纯降级**：元数据是数据 + 边界能力，typed 函数体不读元数据（纯标量函数 `methodInfo` 都不读，`07`§4.1）。
- 与 `12` 协同：元数据生成量由可达性 + 注解决定，二者是同一裁剪管线的输出。

## 状态与产出记录

> 落地每个阶段或切片时在此追加：时间戳 · 切片号 · 状态 · 完成项目 · RED/GREEN · 测试结果 · 备注。

- 2026-06-28 04:22:47 +08:00 · 11-S6H inline-struct CALL_TYPED metadata guard/deopt ·
  状态：11-S6 inline-struct typed-call deopt 子切片完成；完整 11-S6 仍进行中，cross-module
  token resolve 与更完整 no-crash ABI drift injection 仍未关闭。
  完成项目：value SemIR inline-struct `CALL_TYPED` 直接调用现在统一发射
  `zr_aot_value_exec_call_typed_metadata_guard`，在 ordinary direct call、shared generic
  METHOD-slot call 和 full-AOT collected shared callsite 进入 `CallInlineStruct()` 前调用
  `ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, callee)`；binding drift 时发射
  `zr_aot_value_exec_call_typed_metadata_deopt` 并经
  `ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge()` 回解释器，将 inline return bytes
  复制回 generated destination slot。full-AOT shared generic 测试收窄为继续禁止 missing-instance
  deopt，同时要求 metadata drift guard/deopt surface。
  RED/GREEN：RED 由 value SemIR source contract 要求 guard marker 后，WSL gcc 失败
  4 tests / 1 failure，缺失 `zr_aot_value_exec_call_typed_metadata_guard`；GREEN 后 runtime
  guard、value SemIR contracts 和 generic CALL_TYPED 通过。
  验证：WSL gcc runtime guard 3/0、value SemIR contracts 4/0、generic CALL_TYPED 7/0、
  focused CTest 2/2；WSL clang 同组 3/0、4/0、7/0、CTest 2/2；Windows MSVC Debug
  同组 3/0、4/0、generic CALL_TYPED 7/0（3 个既有 Unix-only shared-library case ignored）、
  CTest 2/2。
  产出：`tests/acceptance/2026-06-28-aot-11-s6h-inline-struct-typed-call-deopt.md`。
  备注：本切片不声明 cross-module token resolve 或完整 ABI drift injection 完成；验证时
  `test_aot_c_call_shared_library_smoke` 暴露的既有 scalar no-arg typed direct-call runtime
  failure 已由后续 07-S2 signed binary scalar-operands consumer alignment 修复，不计入
  11-S6H 验收。

- 2026-06-28 03:43:43 +08:00 · 11-S6G bool typed direct-call metadata guard/deopt ·
  状态：11-S6 typed direct-call deopt 子切片完成；完整 11-S6 仍进行中，inline-struct
  typed boundary、跨模块 token resolve 集成和更完整 no-crash ABI drift 注入仍待后续。
  完成项目：bool no/one/two/three-arg typed direct-call writer、以及 i64/u64/f64 comparison -> bool
  two-arg writer 现在携带 caller function slot，生成
  `zr_aot_static_bool_*_direct_call_metadata_guard` 与
  `zr_aot_static_{i64,u64,f64}_bool_two_arg_direct_call_metadata_guard`；兼容时保持 bool direct thunk，
  不兼容时复用 `ZrLibrary_AotRuntime_DeoptTypedDirectCall()` -> `CallStackValue()`，
  再用 `ZrLibrary_AotRuntime_SyncBoolLocal()` 同步 bool scalar local。call contract 同时锁定
  i64/u64/f64/bool guard/deopt source shape。
  RED/GREEN：RED 由新 source contract 要求 bool metadata guard marker 后，WSL gcc call contracts
  失败 8 tests / 1 failure，缺失 `zr_aot_static_bool_one_arg_direct_call_metadata_guard`。
  GREEN 后 runtime guard 3/0、call contracts 8/0、typed call contracts 4/0、bool typed direct-call
  shared-library smoke 28/0。
  验证：WSL gcc direct runtime guard 3/0、call contracts 8/0、typed call contracts 4/0、
  bool typed direct-call smoke 28/0，focused CTest `aot_runtime_typed_direct_call_compatibility` 1/1；
  WSL clang 同四项 direct 3/0、8/0、4/0、28/0 且 CTest 1/1；Windows MSVC Debug direct
  runtime guard 3/0、call contracts 8/0、typed call contracts 4/0、bool typed direct-call smoke
  0 failures / 28 ignored（既有 Unix shared-library 分支），CTest 1/1。
  产出：`tests/acceptance/2026-06-28-aot-11-s6g-bool-typed-direct-call-deopt.md`。
  备注：本切片只关闭 bool-result scalar typed direct-call 的 runtime guard/deopt fallback；没有声明
  inline struct writeback、跨模块 token resolve 或完整 ABI drift injection 完成。

- 2026-06-28 03:14:20 +08:00 · 11-S6F f64 typed direct-call metadata guard/deopt ·
  状态：11-S6 typed direct-call deopt 子切片完成；完整 11-S6 仍进行中，bool/inline-struct
  typed boundary、跨模块 token resolve 集成和更完整 no-crash ABI drift 注入仍待后续。
  完成项目：f64 no/one/two/three-arg typed direct-call writer 现在携带 caller function slot，生成
  `zr_aot_static_f64_*_direct_call_metadata_guard`；兼容时保持原有 float direct thunk 和 stateful
  divide/modulo 路径，不兼容时复用 `ZrLibrary_AotRuntime_DeoptTypedDirectCall()` -> `CallStackValue()`，
  再用 `ZrLibrary_AotRuntime_SyncFloatLocal()` 同步 f64 scalar local。call contract 同时锁定
  i64/u64/f64 guard/deopt source shape，并移除 broad call-lowering contract 对 f64 scalar sync 文本的
  全文件禁用，避免误拒 typed deopt fallback 的合法同步。
  RED/GREEN：RED 先由 `test_aot_c_source_wraps_f64_typed_direct_calls_with_metadata_guard`
  要求 f64 metadata guard marker 后，WSL gcc call contracts 失败 7 tests / 1 failure，缺失
  `zr_aot_static_f64_one_arg_direct_call_metadata_guard`。GREEN 后 runtime guard 3/0、call
  contracts 7/0、f64 typed direct-call shared-library smoke 19/0。
  验证：WSL gcc direct runtime guard 3/0、call contracts 7/0、f64 typed direct-call smoke 19/0，
  focused CTest `aot_runtime_typed_direct_call_compatibility` 1/1；WSL clang 同三项 direct
  3/0、7/0、19/0 且 CTest 1/1；Windows MSVC Debug direct runtime guard 3/0、call contracts 7/0、
  f64 typed direct-call smoke 0 failures / 19 ignored（既有 Unix shared-library 分支），CTest 1/1。
  产出：`tests/acceptance/2026-06-28-aot-11-s6f-f64-typed-direct-call-deopt.md`。
  备注：本切片只关闭 f64 scalar typed direct-call 的 runtime guard/deopt fallback；没有声明
  bool typed boundary、inline struct writeback、跨模块 token resolve 或完整 ABI drift injection 完成。

- 2026-06-28 02:54:58 +08:00 · 11-S6E u64 typed direct-call metadata guard/deopt ·
  状态：11-S6 typed direct-call deopt 子切片完成；完整 11-S6 仍进行中，bool/f64/inline-struct
  typed boundary、跨模块 token resolve 集成和更完整 no-crash ABI drift 注入仍待后续。
  完成项目：u64 no/one/two/three-arg typed direct-call writer 现在携带 caller function slot，生成
  `zr_aot_static_u64_*_direct_call_metadata_guard`；兼容时保持原有 unsigned direct thunk 和 stateful
  divide/modulo 路径，不兼容时复用 `ZrLibrary_AotRuntime_DeoptTypedDirectCall()` -> `CallStackValue()`，
  再用 `ZrLibrary_AotRuntime_SyncUnsignedIntLocal()` 同步 u64 scalar local。call contract 同时锁定
  i64/u64 guard/deopt source shape，并移除 broad call-lowering contract 对 signed/unsigned scalar sync
  文本的全文件禁用，避免误拒 typed deopt fallback 的合法同步。
  RED/GREEN：RED 先由 `test_aot_c_source_wraps_u64_typed_direct_calls_with_metadata_guard`
  要求 u64 metadata guard marker 后，WSL gcc call contracts 失败 6 tests / 1 failure，缺失
  `zr_aot_static_u64_one_arg_direct_call_metadata_guard`。GREEN 后 runtime guard 3/0、call
  contracts 6/0、u64 typed direct-call shared-library smoke 25/0。
  验证：WSL gcc direct runtime guard 3/0、call contracts 6/0、u64 typed direct-call smoke 25/0，
  focused CTest `aot_runtime_typed_direct_call_compatibility` 1/1；WSL clang 同三项 direct
  3/0、6/0、25/0 且 CTest 1/1；Windows MSVC Debug direct runtime guard 3/0、call contracts 6/0、
  u64 typed direct-call smoke 0 failures / 25 ignored（既有 Unix shared-library 分支），CTest 1/1。
  产出：`tests/acceptance/2026-06-28-aot-11-s6e-u64-typed-direct-call-deopt.md`。
  备注：本切片只关闭 u64 scalar typed direct-call 的 runtime guard/deopt fallback；没有声明
  bool/f64 typed boundary、inline struct writeback、跨模块 token resolve 或完整 ABI drift injection 完成。

- 2026-06-28 02:16:03 +08:00 · 11-S6D i64 typed direct-call metadata guard/deopt ·
  状态：11-S6 typed direct-call deopt 子切片完成；完整 11-S6 仍进行中，bool/u64/f64/inline-struct
  typed boundary、跨模块 token resolve 集成和更完整 no-crash ABI drift 注入仍待后续。
  完成项目：新增 `ZrLibrary_AotRuntime_CanUseTypedDirectCall()` 与
  `ZrLibrary_AotRuntime_DeoptTypedDirectCall()`，在直接调用前检查 caller/callee function 的
  `moduleMetadataBindings`；i64 no/one/two/three-arg typed direct-call writer 生成
  `zr_aot_static_i64_*_direct_call_metadata_guard`，兼容时保持 state-free direct thunk 调用，不兼容时走
  `DeoptTypedDirectCall()` -> `CallStackValue()` 并通过 `SyncSignedIntLocal()` 把解释器结果同步回
  i64 scalar local。
  RED/GREEN：RED 先由 `tests/module/test_aot_runtime_typed_direct_call_compatibility.c`
  引用缺失的 typed direct-call guard API，WSL gcc 出现隐式声明并链接失败。GREEN 后 runtime guard
  覆盖空 caller/callee binding 兼容、caller drift 降级、callee drift 降级 3/0；source contract
  锁定 runtime helper、metadata predicate、deopt bridge 和 i64 generated guard markers 5/0；既有
  i64 typed direct-call shared-library smoke 继续 5/0。
  验证：WSL gcc direct runtime guard 3/0、call contracts 5/0、typed direct-call smoke 5/0，
  focused CTest `aot_runtime_typed_direct_call_compatibility` 1/1；WSL clang 同三项 direct
  3/0、5/0、5/0 且 CTest 1/1；Windows MSVC Debug direct runtime guard 3/0、call contracts 5/0、
  typed direct-call smoke 0 failures / 5 ignored（既有 Unix shared-library 分支），CTest 1/1。
  Clang/MSVC 仍输出既有 project const qualifier 与 `aot_runtime.c` unreachable/size_t 警告。
  产出：`tests/acceptance/2026-06-28-aot-11-s6d-i64-typed-direct-call-deopt.md`。
  备注：本切片只关闭 i64 scalar typed direct-call 的 runtime guard/deopt fallback；没有声明
  bool/u64/f64 typed boundary、inline struct writeback、跨模块 token resolve 或完整 ABI drift injection 完成。

- 2026-06-28 01:18:38 +08:00 · 11-S6C dynamic AOT module-load binding reject ·
  状态：11-S6 dynamic loader reject 子切片完成；完整 11-S6 仍进行中，typed 调用边界
  deopt、跨模块 token resolve 集成和更完整 no-crash ABI drift 注入仍待后续。
  完成项目：`zr_vm_library/src/zr_vm_library/aot_runtime.c` 在 AOT module load 期间加载
  embedded/zro metadata function、构建 function table 并 attach metadata runtime 后，调用
  `ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility()` 扫描每个 function 的
  `moduleMetadataBindings`；首个不兼容 binding 会拒绝加载，关闭 dynamic library，避免 materialize
  reflection/prototype 或写入 runtime record，并在 last-error 中输出 status 名称、function index、
  ref token、metadata/signature token、signature hash、module signature hash 和 layout version/hash。
  RED/GREEN：RED 新增 `tests/parser/test_aot_c_metadata_binding_loader.c`，先注入 embedded `.zro`
  会保留的 signature-hash drift，旧 loader 仍返回成功，WSL gcc 失败为 `Expected FALSE Was TRUE`。
  GREEN 后同测试确认 loader 返回失败，错误文本包含 `AOT metadata binding compatibility failed`、
  `module 'main'` 与 `SIGNATURE_HASH_MISMATCH`；既有 shared-library smoke 的空 binding 正常路径仍 8/0。
  验证：WSL gcc direct loader reject 1/0、CTest `aot_c_metadata_binding_loader` 1/1，并通过
  `zr_vm_aot_c_shared_library_smoke_test` 8/0；WSL clang direct loader reject 1/0、同 CTest 1/1；
  Windows MSVC Debug 构建通过，direct test 0 failures / 1 ignored（既有 Unix shared-library 分支），
  同 CTest 1/1。MSVC 仍输出既有 `aot_runtime.c` size_t 转换和 unreachable-code 警告。
  产出：`tests/acceptance/2026-06-28-aot-11-s6c-dynamic-loader-binding-reject.md`。
  备注：本切片只关闭 root AOT runtime dynamic module-load reject；`zr_vm_aot/.../aot_runtime.c`
  静态副本当前没有对应 core metadata-runtime 头/实现链路，未强行接入新依赖。typed-boundary deopt、
  跨模块 token resolve 和更广 no-crash ABI drift injection 仍未完成。

- 2026-06-28 00:56:57 +08:00 · 11-S6B function token binding compatibility scan ·
  状态：11-S6 支撑子切片完成；完整 11-S6 仍进行中，dynamic 模块加载拒绝、typed 调用边界
  deopt、跨模块 token resolve 集成和无崩溃端到端漂移注入仍待后续。
  完成项目：新增 `ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility()`，按
  `SZrFunction.moduleMetadataBindings` 顺序扫描 binding，复用 11-S6A 单 binding predicate，
  返回首个不兼容 binding、对应的 local/module ref record 和完整 report。扫描入口会先查
  function-local metadata token records，再查 module metadata token records，使 AssemblyRef/module-ref
  的 version range 能参与运行期漂移判定；空 binding 表返回 compatible 并清空输出指针。
  RED/GREEN：RED 为测试先引用缺失的 function-level API，WSL gcc 链接失败。GREEN 后 focused test
  新增 3 个扫描用例，覆盖首个不兼容 binding 选择、module ref record version range 使用，以及空表
  输出清理；同文件累计 15/0。
  验证：WSL gcc、WSL clang、Windows MSVC Debug 均构建并直接运行
  `zr_vm_metadata_runtime_binding_compatibility_test`，结果 15/0；三套环境 focused CTest
  `metadata_runtime_binding_compatibility` 均 1/1。WSL gcc/clang 构建输出仍有既有 computed-goto
  extension 警告，MSVC 构建输出仍有既有 execution/object/reflection 警告；本切片新增实现未引入新告警。
  产出：`tests/acceptance/2026-06-28-aot-11-s6b-function-binding-compatibility-scan.md`。
  备注：本切片只关闭 function-level scan/report 支撑层；不声明 11-S6 的 dynamic loader reject、
  typed deopt、跨模块 token resolve 连接或端到端 ABI drift 注入完成。

- 2026-06-27 09:04:36 +08:00 · 11-S6A runtime metadata token binding compatibility ·
  状态：11-S6 支撑子切片完成；完整 11-S6 仍进行中，dynamic 模块加载拒绝、typed 调用边界
  deopt、跨模块 token resolve 集成和无崩溃端到端漂移注入仍待后续。
  完成项目：新增 `ZrCore_MetadataRuntime_CheckTokenBindingCompatibility()`，将
  `SZrMetadataTokenRecord` 的 module version `[min,max)` 区间与 `SZrMetadataTokenBinding`
  的 expected/resolved module signature hash、metadata token、signature token、signature hash、
  layoutVersion/layoutHash 统一成运行期 ABI 漂移判定入口。新增
  `SZrMetadataRuntimeBindingCompatibilityReport`，保留 expected/actual 字段和 version 指针，
  供后续 loader reject、typed deopt 或诊断复用。版本语义与现有 import signature guard 保持一致：
  actual/min/max 均为合法三段 semver 时严格比较，缺失或旧格式版本按兼容处理。
  RED/GREEN：RED 先由 `tests/module/test_metadata_runtime_binding_compatibility.c` 引用缺失的
  status/report/API 触发 WSL gcc 编译失败。GREEN 后 focused test 覆盖 compatible、version mismatch、
  legacy version compatible、module signature mismatch、AssemblyRef->Module 合法映射、metadata token mismatch、signature token mismatch、
  signature hash mismatch、layout version/hash mismatch、missing layout side mismatch 和 null binding invalid
  argument，共 12/0。
  验证：WSL gcc、WSL clang、Windows MSVC Debug 均构建并直接运行
  `zr_vm_metadata_runtime_binding_compatibility_test`，结果 12/0；三套环境 focused CTest
  `metadata_runtime_binding_compatibility` 均 1/1。WSL gcc/clang 构建输出仍有既有 computed-goto
  extension 警告，MSVC 构建输出仍有既有 execution/metadata/object 警告；本切片新增文件未引入新告警。
  产出：`tests/acceptance/2026-06-27-aot-11-s6a-runtime-binding-compatibility.md`。
  备注：本切片只关闭 runtime binding ABI 漂移 predicate；不声明 11-S6 的 dynamic 拒绝、
  typed deopt、跨模块 token resolve 连接或端到端 ABI drift 注入完成。

- 2026-06-27 08:35:30 +08:00 · 11-S7Y zrp metadata version check / 12-S7ZS support ·
  状态：11-S7 工具子切片完成；完整 11-S7 仍未关闭，跨模块 target、cross-module
  export-token publication/rewrite、field/default-value backed constant-pool remap、完整
  metadata sweep/pruning 和 11-S6 运行时 ABI 漂移版本检查仍待后续。
  完成项目：CLI 新增 `--check-zrp-metadata-version <file>` 只读模式，解析层保存
  `zrpMetadataVersionCheckPath`，更新 help，并拒绝与 run/compile/debug/output modifiers 混用；
  app 层分发到 `ZrCli_ZrpMetadataDump_RunVersionCheckPath()`。metadata dump 模块新增
  `ZrCli_ZrpMetadataDump_WriteVersionCheck()`，先读取 zrp header 前 16 字节，再输出
  `zrp.metadata.versionCheck.status`、actual/expected magic、version、headerBytes 和
  sectionCount；当前 header shape 通过完整 `SZrZrpMetadataHeader` 校验时报 `ok`，不匹配时报
  `unsupported` 并返回失败。
  RED/GREEN：RED 先由 `tests/cli/test_cli_args.c` 要求 version-check mode/path 后，旧 CLI
  command 结构缺少 enum/字段而编译失败；随后由 `tests/cli/test_cli_zrp_metadata_dump.c`
  要求 version-check summary/path API 后链接失败。GREEN 后 `cli_args` 与
  `cli_zrp_metadata_dump` 均通过。
  验证：WSL gcc、WSL clang 与 Windows MSVC Debug 均构建 `zr_vm_cli_executable`，并通过
  `zr_vm_cli_args_test`、`zr_vm_cli_zrp_metadata_dump_test`；focused CTest
  `cli_args|cli_zrp_metadata_dump` 三套环境均为 2/2。WSL gcc 与 Windows MSVC help 输出均确认
  新增 `--check-zrp-metadata-version`。
  产出：`tests/acceptance/2026-06-27-aot-11-s7y-zrp-metadata-version-check.md`。
  备注：本切片只提供 standalone zrp metadata header version/shape 检查；不声明 11-S6 runtime
  binding 的 ABI 漂移 deopt/拒绝路径、cross-module export-token rewrite、retained constant
  default-value remap 或完整 metadata sweep/pruning 完成。

- 2026-06-27 08:14:35 +08:00 · 11-S7X zrp metadata diff summary / 12-S7ZR support ·
  状态：11-S7 工具子切片完成；完整 11-S7 仍未关闭，跨模块 target、cross-module
  export-token publication/rewrite、field/default-value backed constant-pool remap、完整
  metadata sweep/pruning 和版本检查仍待后续。
  完成项目：CLI 新增 `--diff-zrp-metadata <before> <after>` 只读模式，解析层保存
  `zrpMetadataBeforePath`/`zrpMetadataAfterPath`，更新 help，并拒绝与 run/compile/debug/output
  modifiers 混用；app 层分发到 `ZrCli_ZrpMetadataDump_RunDiffPath()`。metadata dump 模块新增
  `ZrCli_ZrpMetadataDump_WriteDiffSummary()`，在校验两个 `SZrZrpMetadataHeader` 后输出
  version/headerBytes/sectionCount before/after，以及 12 个 zrp metadata section 的
  bytes/count before/after/removed、elementSize 与 offset 对照。
  RED/GREEN：RED 先由 `tests/cli/test_cli_args.c` 要求 diff mode/before/after path 后，旧 CLI
  command 结构缺少 enum/字段而编译失败；随后由 `tests/cli/test_cli_zrp_metadata_dump.c`
  要求 diff summary/path API 后链接失败。GREEN 后 `cli_args` 与 `cli_zrp_metadata_dump` 均通过。
  验证：WSL gcc、WSL clang 与 Windows MSVC Debug 均构建 `zr_vm_cli_executable`，并通过
  `zr_vm_cli_args_test`、`zr_vm_cli_zrp_metadata_dump_test`；focused CTest
  `cli_args|cli_zrp_metadata_dump` 三套环境均为 2/2。WSL gcc 还运行了 `zr_vm_cli --help`
  确认新增 diff mode 出现在帮助文本中。
  产出：`tests/acceptance/2026-06-27-aot-11-s7x-zrp-metadata-diff-summary.md`。
  备注：本切片只提供 standalone section byte/count diff summary；不声明版本兼容检查、
  cross-module export-token rewrite、retained constant default-value remap 或完整 metadata
  sweep/pruning 完成。

- 2026-06-27 07:48:22 +08:00 · 11-S7W zrp metadata dump summary / 12-S7ZQ support ·
  状态：11-S7 工具子切片完成；完整 11-S7 仍未关闭，跨模块 target、cross-module
  export-token publication/rewrite、field/default-value backed constant-pool remap、dump diff
  和版本检查仍待后续。
  完成项目：CLI 新增 `--dump-zrp-metadata <file>` 只读模式，解析层保存
  `zrpMetadataPath` 并拒绝与 run/compile/debug/output modifiers 混用；app 层分发到
  `ZrCli_ZrpMetadataDump_RunPath()`。新 metadata dump 模块读取 `.zrp` 文件、校验
  `SZrZrpMetadataHeader`，输出 `zrp.metadata.version/headerBytes/sectionCount` 以及
  `zrp.metadata.section.<section> bytes=<n> count=<n> elementSize=<n> offset=<n>`，覆盖 12 个
  zrp metadata sections。
  RED/GREEN：RED 先由 `tests/cli/test_cli_args.c` 要求 dump mode/path 后，旧 CLI command
  结构缺少 enum/字段而编译失败；随后新增 dump summary 目标后 CMake 因缺少
  `zrp_metadata_dump.c` 失败。GREEN 后 `cli_args` 与 `cli_zrp_metadata_dump` 均通过。
  验证：WSL gcc、WSL clang 与 Windows MSVC Debug 均构建 `zr_vm_cli_executable`，并通过
  `zr_vm_cli_args_test`、`zr_vm_cli_zrp_metadata_dump_test`；focused CTest
  `cli_args|cli_zrp_metadata_dump` 三套环境均为 2/2。
  产出：`tests/acceptance/2026-06-27-aot-11-s7w-zrp-metadata-dump-summary.md`。
  备注：本切片只提供 section summary dump；不声明 metadata diff、版本兼容检查、跨模块
  export-token rewrite 或完整 metadata sweep/pruning 完成。

- 2026-06-27 07:20:00 +08:00 · 11-S7 support / 12-S7ZP zrp section count delta markers ·
  状态：11-S7 metadata pruning/dump-diff 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、
  cross-module export-token publication/rewrite、annotation promotion、field/default-value backed
  constant-pool remap 和 dump/diff 工具仍待后续。
  完成项目：zrp metadata size accounting 模块现在从 header section directory 采样 count，
  并输出 `aot_size.zrpMetadataSectionCounts.<section>` 与
  `code_stripping.zrpMetadataSectionCounts.<section>Before/After/Removed`。这为 metadata
  sweep/dump-diff 增加 row/count 维度，且不改变 `.zrp` ABI。
  RED/GREEN：RED 为 direct size-delta 测试新增 section count marker 后，旧 stats 结构缺少
  count 字段导致 WSL gcc 编译失败；GREEN 后 size-delta 2/0、source contracts 21/0、
  code stripping 5/0、direct zrp pruning 5/0、pool pruning 4/0、export-token remap 2/0。
  验证：WSL gcc、WSL clang 与 Windows MSVC Debug 同组可执行测试均通过；focused CTest
  `aot_c_zrp_metadata_size_deltas|aot_c_zrp_metadata_export_token_remap|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping`
  三套环境均为 5/5。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zp-zrp-section-count-delta-markers.md`。
  备注：本切片是 metadata sweep/dump-diff 前的 section-count carrier；未改变 `.zrp` ABI，
  也不声明跨模块 metadata publication 或默认保留策略完成。

- 2026-06-27 06:51:55 +08:00 · 11-S7 support / 12-S7ZO zrp section-level trim delta markers ·
  状态：11-S7 metadata pruning/dump-diff 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、
  cross-module export-token publication/rewrite、annotation promotion、field/default-value backed
  constant-pool remap 和 dump/diff 工具仍待后续。
  完成项目：zrp metadata size accounting 模块现在为裁剪前后每个 section 输出
  `code_stripping.zrpMetadataSectionBytes.<section>Before/After/Removed`，使 emitted `.zrp`
  metadata pruning 的 token records、definition tables 与三类 pools 变化都能被生成 C 直接审计。
  RED/GREEN：RED 为新增 direct size-delta 测试后缺少 section marker 而失败 1/1；GREEN 后
  size-delta 1/0、source contracts 21/0、code stripping 5/0、direct zrp pruning 5/0、
  pool pruning 4/0、export-token remap 2/0。
  验证：WSL gcc、WSL clang 与 Windows MSVC Debug 同组可执行测试均通过；focused CTest
  `aot_c_zrp_metadata_size_deltas|aot_c_zrp_metadata_export_token_remap|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping`
  三套环境均为 5/5。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zo-zrp-section-delta-markers.md`。
  备注：本切片是 metadata sweep/dump-diff 前的 section-level delta carrier；未改变 `.zrp` ABI，
  也不声明跨模块 metadata publication 或默认保留策略完成。

- 2026-06-27 06:30:32 +08:00 · 11-S7 support / 12-S7ZN export member-token remap surface ·
  状态：11-S7 metadata pruning/export-token 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、
  cross-module export-token publication/rewrite、annotation promotion、field/default-value backed
  constant-pool remap 和 dump/diff 工具仍待后续。
  完成项目：zrp metadata remap 模块公开 `backend_aot_c_zrp_remap_export_member_token()`，
  让 `.zrp` export manifest/table 后续可在 MethodDef pruning 之后把 exported `MEMBER_DEF`
  token 同步映射到 compacted RID；保留方法 token 进入新 MethodDef RID，字段 token 按
  retained MethodDef count 后移，已删除方法 token 返回 false 供上层拒绝或诊断。
  RED/GREEN：RED 为 direct pruning 测试先要求导出方法旧 RID2 在 RID1/RID3 删除后映射到
  compacted RID1，旧实现缺 helper，WSL gcc 链接失败；GREEN 后独立 export-token remap
  测试 2/0，direct zrp pruning 5/0、pool pruning 4/0、code stripping 5/0、source contracts 21/0。
  验证：WSL gcc、WSL clang 与 Windows MSVC Debug 同组通过，focused CTest
  `aot_c_zrp_metadata_export_token_remap|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping`
  4/4。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zn-export-member-token-remap.md`。
  备注：本切片只提供 pruning 后导出 member token remap surface；真实跨模块 manifest/table
  写回、版本校验闭环和 dump/diff 对比仍按 11-S7 后续推进。

- 2026-06-27 05:57:45 +08:00 · 11-S7 support / 12-S7ZM zrp pool compaction without MethodDef pruning ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、
  导出 token、annotation promotion、field/default-value backed constant-pool remap 和 dump/diff 工具仍待后续。
  完成项目：zrp metadata prune orchestration 的 skip gate 现在从 early MethodDef-count gate 改为
  post-remap identity gate；identity 检查由 signature/string remap 模块提供。即使没有 MethodDef row
  被删除，只要 string/signature/constant pool 需要收缩，也会生成 owned compacted blob。
  RED/GREEN：RED 为 direct pool-pruning no-MethodDef-prune fixture 要求 duplicate retained
  string compaction 后，旧实现 `ownedBlob` 为空，focused WSL gcc pool pruning 失败 1/4；
  GREEN 后 pool pruning 4/0、zrp pruning 5/0、code stripping 5/0。
  验证：同 12-S7ZM focused gcc/clang/MSVC set（pool pruning/zrp pruning/code stripping/source +
  focused CTest 2/2）。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zm-zrp-pool-compaction-without-method-pruning.md`。
  备注：这仍不是完整 11-S7 metadata sweep；后续仍需 cross-module/export token、annotation-driven
  metadata policy、constant literal/default-value retained pool remap 和 dump/diff 闭环。

- 2026-06-27 05:46:58 +08:00 · 11-S7 support / 12-S7ZL zrp string-pool duplicate slice compaction ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、
  导出 token、annotation promotion、field/default-value backed constant-pool remap 和 dump/diff 工具仍待后续。
  完成项目：string-pool remap 支撑模块现在在保留每个旧 offset 可重写性的同时，按 retained
  string slice 内容进行 interning；不同 old offset 中完全相同的 NUL-terminated payload 会共用同一
  emitted string-pool offset，避免裁剪后仍携带重复 string payload。
  RED/GREEN：RED 为 direct pool-pruning duplicate string fixture 要求 retained TypeDef/FieldDef
  共享 `Shared` 字符串后，旧实现仍按 old offset 写两份，focused WSL gcc pool pruning 失败 1/3
  （Expected 540 Was 547）；GREEN 后 pool pruning 3/0、zrp pruning 5/0、code stripping 5/0。
  验证：同 12-S7ZL focused gcc/clang/MSVC set（pool pruning/zrp pruning/code stripping/source +
  focused CTest 2/2）。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zl-zrp-string-pool-duplicate-slice-compaction.md`。
  备注：这仍不是完整 11-S7 metadata sweep；后续仍需 cross-module/export token、annotation-driven
  metadata policy、constant literal/default-value retained pool remap 和 dump/diff 闭环。

- 2026-06-27 05:07:26 +08:00 · 11-S7 support / 12-S7ZI zrp constant-pool orphan sweep ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、
  导出 token、annotation promotion 和 dump/diff 工具仍待后续。
  完成项目：在现有 zrp metadata row ABI 尚未提供 constant-pool offset/length 字段时，
  emitted zrp pruning 将 constantPool payload 归类为无 retained row 引用的 orphan section；pruned header
  rebuild 增加 retained constant-pool byte 输入并在当前 MethodDef pruning 路径写入 0，生成空
  constantPool section。code stripping 集成统计同步把 after-trim constantPool 从 5 bytes 降为 0，
  并计入 pool/metadata removed delta。
  RED/GREEN：RED 为 direct pool-pruning fixture 要求 orphan constant pool 被清空后，旧实现仍保留原
  5 bytes，focused WSL gcc pool pruning 失败 1/2（Expected 488 Was 493）；GREEN 后 pool pruning 2/0、
  zrp pruning 5/0、code stripping 5/0。
  验证：同 12-S7ZI focused gcc/clang/MSVC set（pool pruning/zrp pruning/code stripping/source/frame/typed/shared +
  focused CTest 4/4；Windows typed/shared 为既有 ignored 形态；clang 仍有既有 generated generic-conversion warning）。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zi-zrp-constant-pool-orphan-sweep.md`。
  备注：这仍不是完整 11-S7 metadata sweep；如果后续持久化 constant literal/default-value row 引用，
  仍需增加 retained constant slice remap/compaction、cross-module/export token、annotation/dump-diff 闭环。

- 2026-06-27 04:42:55 +08:00 · 11-S7 support / 12-S7ZH zrp string-pool sweep/compaction ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、
  导出 token、constant pool sweep/compaction、annotation promotion 和 dump/diff 工具仍待后续。
  完成项目：emitted zrp metadata pruning 新增 string-pool remap 模块，收集 retained TypeDef、retained MethodDef、
  FieldDef、retained GenericParam 与 ModuleRef row 引用的 NUL-terminated string slices，按旧 offset 去重后复制为
  compacted string pool，并在 TypeDef/MethodDef/FieldDef/GenericParam/ModuleRef row copy 时重写 string offsets；
  新增 section helper 模块承载 zrp section lookup/layout/raw-copy 共享逻辑，避免 pruning orchestration 继续膨胀。
  RED/GREEN：RED 为新 direct string-pool fixture 要求 string pool 40->25、保留 MethodDef name offset 重映射、
  removed/unused strings 被剔除后，旧实现仍保留原池，focused WSL gcc pool pruning 失败 1/1；GREEN 后
  pool pruning 1/0、zrp pruning 5/0、code stripping 5/0，source contract 锁定 section/string-pool helper API。
  验证：同 12-S7ZH focused gcc/clang/MSVC set（pool pruning/zrp pruning/code stripping/source/frame/typed/shared +
  focused CTest 4/4；Windows typed/shared 为既有 ignored 形态）。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zh-zrp-string-pool-compaction.md`。
  备注：这仍不是完整 11-S7 metadata sweep；constant pool、cross-module/export token、annotation/dump-diff 后续再闭环。

- 2026-06-27 03:49:57 +08:00 · 11-S7 support / 12-S7ZG zrp MethodSpec signature-pool rewrite/compaction ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、
  导出 token、非 signature pool sweep/compaction、annotation promotion 和 dump/diff 工具仍待后续。
  完成项目：emitted zrp metadata pruning 新增 signature blob remap 模块，收集 retained token/type/method/field/
  constraint/typespec/methodspec 引用的 signature blob slices，去重后复制为 compacted signature pool；
  MethodSpec generic-inst signature 会把 `GENERIC_INST(MEMBER_REF methodToken, args...)` 内的 method token payload
  重写到 compacted `MEMBER_DEF` token，随后重算 token record `signatureHash` 与 MethodSpec `instantiationHash`。
  RED/GREEN：RED 为 MethodSpec signature fixture 要求 signature pool 30->15、MethodSpec signature 内 RID 2->1、
  offset/hash 重算后，旧实现仍保留原池，focused WSL gcc zrp pruning 失败 1/5；GREEN 后 zrp pruning 5/0，
  source contract 锁定 signature remap/copy/rewrite/hash API。
  验证：同 12-S7ZG focused gcc/clang/MSVC set（zrp pruning/code stripping/source/frame/typed/shared + focused CTest 3/3）。
  产出：`tests/acceptance/2026-06-27-aot-12-s7zg-zrp-methodspec-signature-pool-rewrite.md`。
  备注：这仍不是完整 11-S7 metadata sweep；string/constant pool sweep、cross-module/export token、annotation/dump-diff 后续再闭环。

- 2026-06-26 08:38:24 +08:00 · 11-S7 support / 12-S7ZF zrp MethodSpec method-token cascade ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，signature-pool rewrite、
  pool compaction、跨模块 target、导出 token、annotation promotion 和 dump/diff 工具仍待后续。
  完成项目：emitted zrp metadata pruning 现在会随 MethodDef table 压缩同步处理 MethodSpec table；
  MethodSpec row 的 `methodToken` 若指向被删除 MethodDef 则删除该 MethodSpec，若指向保留 MethodDef 则改写到 compacted
  `MEMBER_DEF` token。signature blob pool 当前保持不压缩，保留 MethodSpec 的 instantiation blob offset/length/hash 不变。
  RED/GREEN：RED 为 MethodSpec-present direct zrp fixture 要求 owned pruned blob、MethodSpec count 2->1、
  methodToken RID 2->1 和 signature blob pool 保留后，旧 MethodSpec guard 保留原 blob，focused WSL gcc zrp pruning 失败 1/5；
  GREEN 后 zrp pruning 5/0，并通过 source-contract MethodSpec remap/count/copy 路径锁定。
  验证：同 12-S7ZF focused gcc/clang/MSVC set（zrp pruning/code stripping/source/frame/typed/shared + focused CTest 3/3）。
  产出：`tests/acceptance/2026-06-26-aot-12-s7zf-zrp-methodspec-method-token-cascade.md`。
  备注：这仍不是完整 11-S7 metadata sweep；MethodSpec signature-pool rewrite、pool compaction、annotation/dump-diff 后续再闭环。

- 2026-06-26 08:15:19 +08:00 · 11-S7 support / 12-S7ZE zrp GenericParamConstraint cascade ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，MethodSpec、pool rewrite、
  跨模块 target、导出 token、annotation promotion 和 dump/diff 工具仍待后续。
  完成项目：emitted zrp metadata pruning 现在会随 GenericParam table 压缩同步处理 GenericParamConstraint table；
  指向被删除 MethodDef-owned GenericParam 的 constraints 会被删除，保留 constraints 的 `genericParamIndex`
  会重映射到 compacted GenericParam index，保留 GenericParam 的 constraint range 同步重算。
  RED/GREEN：RED 为 GenericParamConstraint-present direct zrp fixture 要求 owned pruned blob、constraint section count 4->3、
  `genericParamIndex` 2->1 和 constraint range compaction 后，旧 guard 保留原 blob，focused WSL gcc zrp pruning 失败 1/4；
  GREEN 后 zrp pruning 4/0，并通过 source-contract constraint remap/count/range/copy 路径锁定。
  验证：同 12-S7ZE focused gcc/clang/MSVC set（zrp pruning/code stripping/source/frame/typed/shared + focused CTest 3/3）。
  产出：`tests/acceptance/2026-06-26-aot-12-s7ze-zrp-generic-param-constraint-cascade.md`。
  备注：这仍不是完整 11-S7 metadata sweep；MethodSpec method-token/signature-pool rewrite、pool compaction、annotation/dump-diff
  后续再闭环。

- 2026-06-26 07:55:51 +08:00 · 11-S7 support / 12-S7ZD zrp metadata remap module split ·
  状态：11-S7 metadata pruning 支撑 refinement 完成；完整 11-S7 仍未关闭，GenericParamConstraint、MethodSpec、pool rewrite、
  跨模块 target、导出 token、annotation promotion 和 dump/diff 工具仍待后续。
  完成项目：把 emitted zrp metadata pruning 的 token/range 依赖重写集中到
  `backend_aot_c_zrp_metadata_remap.{h,c}`；MethodDef reachability retention、shared `MEMBER_DEF` remap、
  TokenRecord retention/remap、GenericParam owner/range compaction 现在由私有 remap 模块承载，剪枝模块继续负责编排 section/header/copy。
  RED/GREEN：无行为变化拆分，复用 MethodDef-token-record、FieldDef shared-member-token、GenericParam owner/range 三个 direct zrp fixture；
  GREEN 后 source-contract 要求 remap module 独立存在并被 prune orchestration 引用。
  验证：WSL gcc/clang direct zrp pruning 3/0、code stripping 5/0、source contracts 21/0、frame setup 1/0、typed scalar 1/0、
  shared-library smoke 8/0，focused CTest 3/3；Windows MSVC Debug direct zrp pruning 3/0、code stripping 5/0、source contracts 21/0、
  frame setup 1/0、typed scalar 0 failures/1 ignored、shared-library smoke 0 failures/8 ignored，focused CTest 3/3。
  产出：`tests/acceptance/2026-06-26-aot-12-s7zd-zrp-metadata-remap-module-split.md`。
  备注：这是后续 GenericParamConstraint/MethodSpec/pool cascade 的模块边界准备，不声明完整 11-S7 metadata sweep 完成。

- 2026-06-26 07:30:55 +08:00 · 11-S7 support / 12-S7ZC zrp GenericParam owner remap ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，GenericParamConstraint、MethodSpec、pool rewrite、
  跨模块 target、导出 token、annotation promotion 和 dump/diff 工具仍待后续。
  完成项目：无 GenericParamConstraint 的 GenericParam rows 可随 MethodDef pruning 重写；TypeDef-owned 泛型参数保留，
  retained MethodDef/FieldDef-owned 泛型参数 owner token 跟随共享 `MEMBER_DEF` remap，被删除 MethodDef 拥有的泛型参数会被移除，
  TypeDef/MethodDef generic-param ranges 同步压缩。
  RED/GREEN：RED 为 GenericParam-present direct zrp fixture 要求 owned pruned blob、owner remap 和 range compaction 后，
  旧 guard 保留原 blob，focused WSL gcc zrp pruning 失败 1/3；GREEN 后 zrp pruning 3/0，并通过 source-contract GenericParam
  remap/count/range/copy 路径锁定。
  验证：同 12-S7ZC focused gcc/clang/MSVC set（zrp pruning/code stripping/source/frame/typed/shared + focused CTest 3/3）。
  产出：`tests/acceptance/2026-06-26-aot-12-s7zc-zrp-generic-param-owner-remap.md`。
  备注：这仍不是完整 11-S7 metadata sweep；GenericParamConstraint cascade、MethodSpec signature-pool rewrite、annotation/dump-diff
  后续再闭环。

- 2026-06-26 07:14:57 +08:00 · 11-S7 support / 12-S7ZB zrp FieldDef member-token remap ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，GenericParam/MethodSpec/pool rewrite、跨模块 target、
  导出 token、annotation promotion 和 dump/diff 工具仍待后续。
  完成项目：MethodDef/FieldDef 共用 `MEMBER_DEF` token 空间的 emitted blob 重排已接入；MethodDef rows 被 reachability 删除后，
  FieldDef rows 会保留并重写到保留 MethodDef RID 之后，tokenRecords 内 FieldDef 相关 member-token 字段同步 remap，指向被删除
  MethodDef 的 token record 仍会被删除。
  RED/GREEN：RED 为 FieldDef-present direct zrp fixture 要求 owned pruned blob 和 FieldDef RID 3->2 remap 后，旧 guard 保留原 blob，
  focused WSL gcc zrp pruning 失败 1/2；GREEN 后 zrp pruning 2/0，并通过 source-contract FieldDef remap/copy 路径锁定。
  验证：同 12-S7ZB focused gcc/clang/MSVC set（zrp pruning/code stripping/source/frame/typed/shared + focused CTest 3/3）。
  产出：`tests/acceptance/2026-06-26-aot-12-s7zb-zrp-fielddef-member-token-remap.md`。
  备注：这仍不是完整 11-S7 metadata sweep；GenericParam owner、MethodSpec method token、pool compaction、annotation/dump-diff
  后续再闭环。

- 2026-06-26 06:58:15 +08:00 · 11-S7 support / 12-S7ZA zrp token-record MethodDef pruning/remap ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、导出 token、FieldDef/generic/MethodSpec
  跟随重写、pool rewrite 和 dump/diff 工具仍待后续。
  完成项目：emitted zrp metadata pruning 现在会在 MethodDef row 删除后同步压缩 tokenRecords section；
  保留 MethodDef row 与 token record 内的 MethodDef 引用会重写为同一组紧凑 `MEMBER_DEF` token，指向已删除 MethodDef 的 token record 被删除。
  由于 MethodDef/FieldDef 当前共用 `MEMBER_DEF` token 表，含 FieldDef rows 的 zrp blob 暂不改写，避免产生不完整 member-token 重排。
  RED/GREEN：RED 为新增直接 zrp pruning 单测后，旧 token-record MEMBER_DEF guard 保留原 blob，focused WSL gcc 失败 1/1；
  GREEN 后 zrp pruning 单测 2/0，并通过 source-contract remap/copy 路径锁定。
  验证：同 12-S7ZA focused gcc/clang/MSVC set（zrp pruning/code stripping/source/frame/typed/shared + focused CTest 3/3）。
  产出：`tests/acceptance/2026-06-26-aot-12-s7za-zrp-token-record-methoddef-pruning.md`。
  备注：这仍不是完整 11-S7 metadata sweep；FieldDef/generic/MethodSpec/pool/annotation/dump-diff 后续再闭环。

- 2026-06-26 06:30:39 +08:00 · 11-S7 support / 12-S7Z zrp MethodDef metadata pruning ·
  状态：11-S7 默认最小/裁剪支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、导出 token、
  完整 zrp metadata sweep/pruning 和 dump/diff 工具仍待后续。
  完成项目：AOT C generated embedded zrp data metadata 现在可由 code stripping 结果驱动重写；
  当前安全子集裁剪 MethodDef rows 中已移除函数的 `functionIndex`，并让 descriptor length 与 zrp size/delta markers
  反映 emitted blob，而非原始 input blob。
  RED/GREEN：RED 为 code-stripping fixture 需要 MethodDef after-size/removed bytes 后失败；GREEN 后保留函数 1 的 MethodDef，
  删除函数 2 的 MethodDef，并把 total/definition-table removed 均记为 36 bytes。
  验证：同 S7Z focused gcc/clang/MSVC set（code stripping/source/frame/typed/shared + focused CTest）。
  产出：`tests/acceptance/2026-06-26-aot-12-s7z-zrp-methoddef-metadata-pruning.md`。
  备注：这不是完整 11-S7 元数据策略闭环；token record、generic params、MethodSpec、pool rewrite、annotation promotion 和 dump/diff 仍未完成。

- 2026-06-26 06:00:16 +08:00 · 11-S7 support / 12-S7Y default-min MethodInfo metadata policy ·
  状态：11-S7 默认最小策略子切片完成；完整 11-S7 仍未关闭，跨模块 target、导出 token、
  实际 zrp metadata sweep/pruning 和 dump/diff 工具仍待后续。
  完成项目：generated C MethodInfo metadata policy 现在可随裁剪结果收窄；默认/非裁剪输出
  `ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING`，opt-in code stripping 输出
  `ZR_AOT_REFLECTION_METADATA_NONE`，文件头通过 `metadata_policy.reflectionLevel` 暴露当前策略。
  RED/GREEN：RED 为 code-stripping fixture 首次要求 stripped MethodInfo `NONE` 和 policy marker 后失败；
  GREEN 后 option normalization、emitter marker、method metadata emitter 与 byte-sampling helper 均使用同一
  reflection metadata level。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 focused code stripping、source contracts、frame setup contracts、
  typed scalar、shared-library smoke 与对应 focused CTest；Windows typed scalar/shared-library smoke 的 ignored
  项均为既有 Unix shared-library guard。
  产出：`tests/acceptance/2026-06-26-aot-12-s7y-default-min-reflection-metadata-policy.md`。
  备注：本切片只调整 generated MethodInfo policy，不改 embedded `.zrp` metadata blob；实际数据元数据裁剪、
  row/table/pool rewrite 与 dump/diff 仍未完成。

- 2026-06-26 01:40:40 +08:00 · 11 support / 12-S7K zrp metadata section/table/pool byte statistics ·
  状态：11 元数据支撑记录完成；完整 11-S1/11-S7 仍未关闭，真实 compiler zrp 表/池导出、
  默认最小 metadata 策略、跨模块 metadata policy 和 dump/diff 工具仍待后续。
  完成项目：generated AOT C 现在复用 `ZrCore_ZrpMetadata_ReadHeader()` 识别已嵌入的 zrp data metadata blob，
  并按 11-S1 header/section ABI 输出 total、token-record、definition-table、pool 与每个 section 的字节统计；
  非 zrp embedded blob 保持零值统计，不改变 `.zro` 嵌入路径。
  RED/GREEN：RED 为 code-stripping fixture 中有效 zrp metadata blob 缺少 zrp 统计 marker；
  GREEN 后 generated C 输出 374/96/52/18 的总量与分组统计，并列出 12 个 section 明细。
  验证：WSL gcc、WSL clang、Windows MSVC Debug 的 CTest
  `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format` 均为 3/3；三套环境的
  `zr_vm_aot_c_source_contracts_test` 均为 19/0。产出：
  `tests/acceptance/2026-06-26-aot-12-s7k-zrp-metadata-section-byte-statistics.md`。
  备注：这是 12-S7 对 11-S1 zrp section directory 的 size attribution 消费，不声明 11-S1 真实数据导出、
  11-S7 默认最小策略或 zrp dump/diff 工具完成。

- 2026-06-26 01:14:40 +08:00 · 11-S4R generated ownership offset table emission ·
  状态：11-S4 子切片完成；完整 11-S4/10-S4 仍未关闭，union owner offset 表、持久 cTypeId→token 索引、
  runtime generic layout construction、cross-module token table 和 public reflection entity 接入仍待后续。
  完成项目：generated `SZrTypeLayout` descriptor 现在会为可安全导出 offset 的 struct owner fields 写出
  `ZrOwnershipOffsets_<typeLayoutId>[]`，并将 `.ownershipFieldOffsets` 指向该表；零 ownership 字段、union 和
  unsafe/unsupported offset 路径保持 `ZR_NULL` 并输出 failure marker。RED/GREEN：RED 为
  `test_aot_c_type_layout_contracts.c` 新增 ownership-offset writer source contract 后缺少
  `backend_aot_c_type_layout_can_emit_ownership_offsets(`；GREEN 后 `Unique<string>` 字段的 `Holder` generated C
  出现 `/* zr_aot_ownership_offsets layout=0 count=1 */`、`ZrOwnershipOffsets_0[]`、`.ownershipFieldCount = 1u`
  和 `.ownershipFieldOffsets = ZrOwnershipOffsets_0`。验证：WSL gcc/clang 均通过 CTest `aot_c_type_layout_contracts`
  1/1、source contracts 19/0、value-type smoke 4/0；Windows MSVC Debug 通过同组 CTest 1/1、source contracts 19/0、
  value-type smoke 3/0/1 ignored。产出：
  `tests/acceptance/2026-06-26-aot-11-s4r-generated-ownership-offset-table.md`。
  备注：本记录不声明 union ownership offset 表、runtime generic layout construction、MethodSpec materialization、
  cross-module token-table policy 或 public reflection entity 完成。

- 2026-06-26 00:42:14 +08:00 · 11-S4Q generated TypeSpec-backed type-layout token population ·
  状态：11-S4 子切片完成；完整 11-S4/10-S4 仍未关闭，持久 cTypeId→token 索引、ownership offset 表、
  runtime layout construction、cross-module token table 和 public reflection entity 接入仍待后续。
  完成项目：`backend_aot_c_type_layout_tokens.c` 现在会在 TypeDef token 未命中时扫描同函数
  `metadataTokenRecords` 中的 `TYPE_SPEC` 记录，把 generated layout 的 `typeName` 与 canonical signature blob
  做结构匹配；支持 `GENERIC_INST(TYPE_REF/TYPE_DEF base, args...)`、`UNION`、direct `TYPE_REF/TYPE_DEF`、
  primitive argument、nullable/ownership/array wrapper 的保守递归匹配。唯一匹配时，generated
  `zr_aot_type_layout_tokens[typeLayoutId]` 写入真实 `TYPE_SPEC` token；无匹配或多重匹配仍写 0 并保留 runtime
  zrp scan fallback。RED/GREEN：RED 为 `test_aot_c_generic_call_typed.c` 要求 `Pair<int, int>` generated
  layout token 表含 `0x07000001u` 后失败，旧生成物在对应 table 中全为 `0u`；GREEN 后生成物第 4 号 layout
  slot 写入 `0x07000001u`，shared library 编译 smoke 同步通过。验证：WSL gcc 和 WSL clang 均通过
  `aot_c_type_layout_contracts|aot_c_generic_call_typed` CTest 2/2，并直接运行 AOT source contracts 19/0
  与 value-type shared-library smoke 3/0；Windows MSVC Debug 通过同组 CTest 2/2、source contracts 19/0、
  value-type smoke 2/0/1 ignored。产出：`tests/acceptance/2026-06-26-aot-11-s4q-generated-typespec-type-layout-token-population.md`。
  备注：本切片只关闭 current-function generated TypeSpec token population；不声明跨模块 TypeSpec token 表、
  runtime generic layout synthesis、ownership offsets、MethodSpec materialization 或 public 泛型反射对象完成。

- 2026-06-26 00:13:32 +08:00 · 11-S4P generated TypeDef-backed type-layout token population ·
  状态：11-S4 子切片完成；完整 11-S4/10-S4 仍未关闭，TypeSpec/generic layout materialization、持久
  cTypeId→token 索引、ownership offset 表、runtime layout construction、public reflection entity 接入仍待后续。
  完成项目：新增 `backend_aot_c_type_layout_tokens.c`，把 generated token table 写入逻辑从 descriptor emitter 拆出；
  generated C 现在对能在本地 metadata token record 中唯一匹配的 named struct/union `SZrTypeLayout` 写入真实
  `TYPE_DEF` token，缺 metadata、重复匹配、TypeSpec/generic 仍保守写 0；`backend_aot_c_type_layouts.c` 暴露
  generated table layout resolver，并为 union layout 发射 runtime descriptor，让本地 union TypeDef token 可绑定到
  generated `SZrTypeLayout`。RED/GREEN：RED 为新增 union `Shape` generated-C smoke 后，生成物缺少
  `ZrTypeLayout_` descriptor 和非零 token；GREEN 后生成物包含 union layout registry、`zr_aot_type_layout_tokens[]`
  和 `0x02000001u`，且不依赖 debug-only `typeLayoutToken` 注释。验证：WSL gcc 与 WSL clang 均通过 metadata runtime
  TypeSpec layout 14/0、AOT type-layout contracts 1/0、source contracts 19/0、frame setup contracts 1/0、
  shared-library smoke 8/0、value-type smoke 3/0；Windows MSVC Debug 通过同组 metadata 14/0、type-layout contracts 1/0、
  source contracts 19/0、frame setup contracts 1/0、shared-library smoke 0/0/8 ignored、value-type smoke 2/0/1 ignored。
  产出：`tests/acceptance/2026-06-26-aot-11-s4p-generated-type-layout-token-population.md`。
  备注：本切片只填充可靠的 TypeDef-backed generated token 子集；不声明 TypeSpec/generic token population、跨模块 token
  table、public reflection API 或完整 metadata policy 完成。验证中仍有既有 generated-C logical-not 括号告警，未形成失败。

- 2026-06-25 23:13:20 +08:00 · 11-S4O code-registration type layout token carrier ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，真实 token 填充/持久 cTypeId→token 索引内容、
  TypeSpec/generic layout materialization、ownership offset 表发射和 runtime layout construction 仍待后续。
  完成项目：AOT ABI 升到 10；`SZrAotCodeRegistration` 与 `ZrAotCompiledModule` 新增
  `typeLayoutTokens/typeLayoutTokenCount`；generated C 发射 `zr_aot_type_layout_tokens[]` 并把 module descriptor
  与 code registration 连接到同一表；runtime loader 校验 descriptor/codeRegistration token 表形态一致且
  `typeLayoutTokenCount == typeLayoutCount`；metadata runtime attach 镜像 token 表计数；`ResolveTypeLayoutToken()`
  与 `ResolveCTypeIdToken()` 会先读取 code-registration token 表，再 fallback 到 zrp TypeDef/TypeSpec row scan。
  表项只有在 token 为 TypeDef/TypeSpec 且 registry-backed layout 可解析时才被接受。
  RED/GREEN：RED 为 focused ABI/source/runtime tests 后缺少 `typeLayoutTokens/typeLayoutTokenCount` 字段与
  `SZrMetadataRuntime.typeLayoutTokenCount`；GREEN 后 ABI/source 契约、descriptor validation、手工非零 token 表消费、
  非 type token 拒绝、缺 layout 拒绝和 generated table shape 均通过。
  验证：WSL gcc 与 WSL clang 直接运行通过 metadata TypeSpec layout 14/0、AOT source contracts 19/0、
  frame setup contracts 1/0、shared-library smoke 8/0、value-type shared-library smoke 2/0；Windows MSVC Debug
  直接运行通过 metadata TypeSpec layout 14/0、source contracts 19/0、frame setup 1/0、shared-library smoke
  0/0/8 ignored、value-type smoke 1/0/1 ignored。当前 CTest filter 只登记并匹配
  `metadata_runtime_typespec_layout`，其余 AOT C target 以可执行文件直接验证。产出：
  `tests/acceptance/2026-06-25-aot-11-s4o-type-layout-token-carrier.md`。
  备注：generated token table 当前零填充，因为 emitter 尚未为每个 emitted layout 连接可靠 TypeDef/TypeSpec token
  来源；本切片不声明完整持久 cTypeId→token 索引、generic layout 构造、ownership offsets 或 public 泛型反射实体完成。

- 2026-06-25 22:22:13 +08:00 · 11-S4N cTypeId to TypeDef/TypeSpec token resolver ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，持久 cTypeId→token 索引表、TypeSpec/generic layout
  materialization、ownership offset 表发射和 runtime layout construction 仍待后续。
  完成项目：新增 `ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, cTypeId)`。该入口在当前
  code-registration registry 约束的 `cTypeId == typeLayoutId` 不变量下复用
  `ZrCore_MetadataRuntime_ResolveTypeLayoutToken()`，因此共享 11-S4M 的 bounded multi-entry cache 和
  TypeDef/TypeSpec binding-view registry-backed 校验路径；null runtime、`ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`
  和只有 stale `prototypeFrameTypeLayouts` 的路径返回 0。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_typespec_layout_test` 新增 cTypeId→token 用例后，缺少
  `ZrCore_MetadataRuntime_ResolveCTypeIdToken()` 导致隐式声明和链接失败；GREEN 后 TypeDef/TypeSpec cTypeId
  正向反查、多项 cache 命中和 no-prototype-fallback 负向用例均通过。
  验证：WSL gcc 与 WSL clang 均通过 CTest
  `metadata_runtime_typespec_layout|metadata_runtime_query|metadata_runtime_type_layout|zrp_metadata_format` 4/4；
  Windows MSVC Debug 通过 `zr_vm_metadata_runtime_typespec_layout_test` 12/0、
  `zr_vm_zrp_metadata_format_test` 11/0、`zr_vm_metadata_runtime_query_test` 22/0、
  `zr_vm_metadata_runtime_type_layout_test` 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4n-ctype-id-token-resolver.md`。
  备注：本切片只关闭 public cTypeId→token 运行期入口；不声明持久 reverse index、cTypeId/typeLayoutId 解耦、
  generic layout 构造、ownership offsets、跨模块 cache 或 public 泛型反射实体完成。验证中仍有既有
  generated-dispatch label、MSVC unreachable-code 和 `metadata_runtime.c` 可能未初始化告警，未形成失败。

- 2026-06-25 22:13:54 +08:00 · 11-S4M bounded multi-entry type layout cache ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，持久 cTypeId→token 索引表、TypeSpec/generic layout
  materialization、ownership offset 表发射和 runtime layout construction 仍待后续。
  完成项目：`SZrMetadataRuntime` 将 11-S4K/11-S4L 的单项最近命中扩展为 8 项 bounded cache：
  `typeLayoutCacheTokens[]`、`typeLayoutCacheIds[]`、`typeLayoutCacheLayouts[]` 和
  `typeLayoutCacheNextIndex`。`ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` 先按 token 查 cache，
  `ZrCore_MetadataRuntime_ResolveTypeLayoutToken()` 先按 layout id 查 cache；未命中时仍经 TypeDef/TypeSpec
  binding view 重新校验 row identity 与 code-registration registry-backed `SZrTypeLayout`，成功后写回 cache。
  cache 可同时保留 TypeDef 与 TypeSpec 的 token→layout 和 layoutId→token 命中，registry 表项被清空后仍可命中
  已缓存项，cache 满后按 bounded round-robin 覆盖。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_typespec_layout_test` 新增两项多项 cache 用例后，旧单项 cache
  在先缓存 TypeDef、再缓存 TypeSpec 后丢失前一个 token/layout 命中，且 reverse lookup 同样只保留最后一次命中；
  GREEN 后 TypeDef/TypeSpec 正向 cache 与 layoutId→token reverse cache 可并存。
  验证：WSL gcc 与 WSL clang 均通过 CTest
  `metadata_runtime_typespec_layout|metadata_runtime_query|metadata_runtime_type_layout|zrp_metadata_format` 4/4；
  Windows MSVC Debug 通过 `zr_vm_metadata_runtime_typespec_layout_test` 10/0、
  `zr_vm_zrp_metadata_format_test` 11/0、`zr_vm_metadata_runtime_query_test` 22/0、
  `zr_vm_metadata_runtime_type_layout_test` 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4m-multi-entry-type-layout-cache.md`。
  备注：本切片只关闭 bounded runtime cache；不声明持久 reverse index、generic layout 构造、ownership offsets、
  跨模块 cache 或 public 泛型反射实体完成。验证中仍有既有 generated-dispatch label、MSVC unreachable-code
  和 `metadata_runtime.c` 可能未初始化告警，未形成失败。

- 2026-06-25 21:53:56 +08:00 · 11-S4L typeLayoutId to TypeDef/TypeSpec token reverse resolver ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，完整多项 token/cTypeId/layout cache、持久
  cTypeId→token 索引表、TypeSpec/generic layout materialization、ownership offset 表发射和 runtime layout
  construction 仍待后续。
  完成项目：新增 `ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, typeLayoutId)`。该入口先检查
  `SZrMetadataRuntime` 中最近一次 `typeToken/typeLayoutId/typeLayout` cache；未命中时扫描 attached zrp
  `TYPE_DEFS`，再扫描 `TYPE_SPECS`，并分别复用 TypeDef/TypeSpec binding view 校验 row identity 与
  code-registration registry-backed `SZrTypeLayout` 存在后返回 token。`ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`、
  null runtime、缺 zrp metadata、缺 registry layout 或只有 stale `prototypeFrameTypeLayouts` 时返回 0；成功后写回
  最新 cache。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_typespec_layout_test` 新增 layoutId→token 用例后，缺少
  `ZrCore_MetadataRuntime_ResolveTypeLayoutToken()` 导致隐式声明和链接失败；GREEN 后 TypeDef layoutId→token、
  TypeSpec layoutId→token、最新 cache 二次命中和 no-prototype-fallback 负向用例均通过。
  验证：WSL gcc 与 WSL clang 均通过 CTest
  `metadata_runtime_typespec_layout|metadata_runtime_query|metadata_runtime_type_layout|zrp_metadata_format` 4/4；
  Windows MSVC Debug 通过 `zr_vm_metadata_runtime_typespec_layout_test` 8/0、
  `zr_vm_zrp_metadata_format_test` 11/0、`zr_vm_metadata_runtime_query_test` 22/0、
  `zr_vm_metadata_runtime_type_layout_test` 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4l-layout-id-token-reverse-cache.md`。
  备注：本切片只关闭最小 layoutId→token 反查入口；不声明完整多项 cache、持久 reverse table、
  generic layout 构造、ownership offsets 或 public 泛型反射实体完成。验证中仍有既有 generated-dispatch label
  和 reflection unused-local warning，未形成失败。

- 2026-06-25 21:37:38 +08:00 · 11-S4K TypeDef/TypeSpec token layout cache resolver ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，完整多项 token/cTypeId/layout cache、cTypeId→token
  反查表、TypeSpec/generic layout materialization、ownership offset 表发射和 runtime layout construction
  仍待后续。
  完成项目：新增 `ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, typeToken, outTypeLayoutId)`，为
  `TYPE_DEF` 和 `TYPE_SPEC` token 提供 public token→layout 解析入口。TypeDef 路径复用
  `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()`，TypeSpec 路径复用
  `ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView()`；两者都要求 code-registration layout registry
  返回非空 `SZrTypeLayout`，不会 fallback 到 `prototypeFrameTypeLayouts`。runtime 现在缓存最近一次成功的
  `typeToken/typeLayoutId/typeLayout`，同 token 二次查询可在 registry 项被清空后仍返回缓存命中；失败或非 type
  token 会把输出 layout id 复位为 `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_typespec_layout_test` 新增 TypeDef/TypeSpec token cache 用例后，
  缺少 public resolver 导致隐式声明与链接失败；GREEN 后 TypeDef token cache、TypeSpec token cache、null/非 type
  token 拒绝、以及缺 registry layout 时不借用 stale prototype cache 的负向用例均通过。
  验证：WSL gcc 与 WSL clang 均通过 CTest
  `metadata_runtime_typespec_layout|metadata_runtime_query|metadata_runtime_type_layout|zrp_metadata_format` 4/4；
  Windows MSVC Debug 通过 `zr_vm_metadata_runtime_typespec_layout_test` 5/0、
  `zr_vm_zrp_metadata_format_test` 11/0、`zr_vm_metadata_runtime_query_test` 22/0、
  `zr_vm_metadata_runtime_type_layout_test` 10/0。第一次 WSL gcc CTest 曾因 header 结构变化后
  `metadata_runtime_query` 二进制未重建而失败；重建相关目标后同组通过。
  产出：`tests/acceptance/2026-06-25-aot-11-s4k-type-token-layout-cache.md`。
  备注：本切片只关闭最近一次 token→layout cache resolver；不声明 reverse cTypeId→token table、
  多项 cache、generic layout 构造、ownership offsets 或 public 泛型反射实体完成。

- 2026-06-25 21:18:46 +08:00 · 11-S4J TypeSpec layout binding view ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，TypeSpec/generic layout materialization、
  ownership offset 表发射、runtime layout construction 和完整 token/cTypeId/layout cache 仍待后续。
  完成项目：`SZrZrpMetadataTypeSpecRow` 的保留槽位改为 `typeLayoutId`；新增
  `SZrMetadataRuntimeTypeSpecLayoutBindingView` 与
  `ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(runtime, typeSpecToken, outView)`，从 `TYPE_SPEC`
  token 绑定 type token record、paired signature record、zrp `TYPE_SPECS` row、11-S3K generic base-token
  binding view、`typeLayoutId/cTypeId/signatureHash` 和 registry-backed `SZrTypeLayout`。reader 校验 zrp row 的
  `signatureBlobOffset/signatureBlobLength/signatureHash` 与 token/signature record 一致，layout 只经
  `ZrCore_MetadataRuntime_ResolveTypeLayout()` 从 code-registration registry 解析。实现时把 TypeDef/TypeSpec/FieldDef
  layout-binding row lookup 与 view reader 从 `metadata_runtime.c` 拆到
  `metadata_runtime_layout_binding.c`，避免继续放大已接近阈值的 runtime 主文件。RED/GREEN：RED 为新增
  `zr_vm_metadata_runtime_typespec_layout_test` 后编译失败，缺少 `typeLayoutId` 字段、view type 和 API；GREEN 后
  正向 TypeSpec binding 视图通过，且缺 registry layout 但存在 stale `prototypeFrameTypeLayouts[42]` 时返回 false。
  验证：WSL gcc、WSL clang 与 Windows MSVC Debug 均通过 `zr_vm_metadata_runtime_typespec_layout_test` 2/0、
  `zr_vm_zrp_metadata_format_test` 11/0、`zr_vm_metadata_runtime_query_test` 22/0、
  `zr_vm_metadata_runtime_type_layout_test` 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4j-typespec-layout-binding-view.md`。
  备注：本切片只关闭 TypeSpec row/layout binding 的只读入口；不声明 generic layout 运行期构造、
  generic instantiation materialization、ownership offset 表或 public 泛型反射实体完成。验证中仍有既有
  generated-dispatch unused label、generated-C logical-not 和 MSVC const/unreachable-code 告警，未形成失败。

- 2026-06-25 20:43:52 +08:00 · 11-S4I FieldDef layout binding view ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，TypeSpec/generic layout materialization、
  ownership offset 表发射、runtime layout construction 和完整 token/cTypeId/layout cache 仍待后续。
  完成项目：新增 `SZrMetadataRuntimeFieldDefLayoutBindingView` 与
  `ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(runtime, fieldDefToken, outView)`；runtime 从 attached zrp
  metadata 的 `FIELD_DEFS` section 匹配 `MEMBER_DEF` field token，绑定 field token record、FieldDef row、
  owner `TYPE_DEF` token/record/row、FieldDef `byteOffset/typeLayoutId`、owner `typeLayoutId` 以及 field/owner
  `SZrTypeLayout` 指针；owner/field layout 都必须经 `ZrCore_MetadataRuntime_ResolveTypeLayout()` 从
  code-registration layout registry 解析，且 field row index 必须落在 owner TypeDef row 的
  `firstFieldDefIndex/fieldDefCount` 范围内。RED/GREEN：RED 为 metadata runtime query 新增 FieldDef binding view
  用例后缺少 view type/API 导致编译失败；GREEN 后新增正向 binding 视图和 stale prototype cache 负向用例，
  证明缺 field layout 时不会从 `prototypeFrameTypeLayouts[42]` 回退。验证：WSL gcc/clang 与 Windows MSVC Debug
  均通过 `zr_vm_metadata_runtime_query_test` 22/0；同三平台通过相关
  `zr_vm_metadata_runtime_type_layout_test` 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4i-fielddef-layout-binding-view.md`。
  备注：本切片只关闭 FieldDef token/row/offset/typeLayoutId 到 registry layout 的只读绑定入口；
  不声明 token-driven field reflection entity、TypeSpec/generic layout materialization 或运行期 layout 构建完成。

- 2026-06-25 20:27:41 +08:00 · 11-S4H / 10-S4A reflection registry-backed layout consumer ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，TypeSpec/generic layout materialization、
  runtime layout construction、ownership offset 表发射和完整三向缓存仍待后续。10-S4 仅关闭字段
  layout/offset 消费端的第一步迁移，泛型参数反射、DESCRIPTION 级字段 token entity 和完整类型实参暴露仍待后续。
  完成项目：新增
  `ZrCore_MetadataRuntime_ResolveFunctionPrototypeTypeLayout(function, prototype, outTypeLayoutId)`，
  通过函数自身或 prototype-context entry function 的 prototype 实例表映射 `typeLayoutId`，再从 attached
  code-registration layout registry 解析 `SZrTypeLayout`，且不 fallback 到 prototype layout cache；
  `reflection.c` 在构建 type reflection 与 decorator target member reflection 时解析同一 registry-backed
  layout；`reflection_apply_type_layout_to_layout_object()` 将类型级 `fieldCount/size/alignment` 写回反射
  `layout` 对象；脚本字段反射按实例字段序号读取 `SZrTypeLayout.fields[i]`，再通过
  `reflection_apply_field_layout_to_member()` 写回成员 `offset/size/layout`。没有 attached registry 或无匹配
  field 时保留原有 prototype/member 序列化数据 fallback。RED/GREEN：RED 为
  `zr_vm_metadata_runtime_type_layout_test` 新增 prototype layout resolver 用例后编译/链接失败，缺少
  `ZrCore_MetadataRuntime_ResolveFunctionPrototypeTypeLayout()`；GREEN 后 attached registry 返回
  code-registration layout、detached 函数即使存在 stale prototype layout cache 也返回 null，且源码契约锁定
  reflection include、resolver 调用、类型 layout 写入和按实例字段序号消费 registry field。验证：WSL gcc/clang
  均通过 `zr_vm_metadata_runtime_type_layout_test` 10/0、`zr_vm_metadata_runtime_query_test` 20/0、
  `zr_vm_aot_c_type_layout_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0；
  Windows MSVC Debug 同组通过 10/0、20/0、1/0、19/0。
  产出：`tests/acceptance/2026-06-25-aot-11-s4h-10-s4a-reflection-layout-registry-consumer.md`。
  备注：`reflection.c` 已超过大文件阈值；本切片只增加 metadata runtime 接线和两个小 helper，未新增独立职责，
  因此未在本切片拆分文件。验证中既有 compiler/generated-code 告警未形成测试失败。

- 2026-06-25 19:31:46 +08:00 · 11-S4G GC inline-frame metadata-runtime layout resolver ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，TypeSpec/generic layout materialization、
  runtime layout construction、ownership offset 表发射、反射消费端迁移和完整三向缓存仍待后续。
  完成项目：`SZrFunction` 新增 attached metadata code-registration layout registry 载体；
  `ZrCore_MetadataRuntime_AttachFunction()` 绑定 runtime 的 code-registration/typeLayout/gcDescriptor 计数；
  `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout()` 从函数自身或 prototype-context entry function 的
  attached registry 解析 `SZrTypeLayout`；function tombstone/reset 路径清空 attached registry 字段；
  AOT loader 在 module attach 后对 loaded function table 全量绑定；
  GC mark/rewrite inline-frame resolver 对 attached AOT registry 使用 metadata runtime layout table，对未绑定的
  VM/interpreter 函数保留 prototype resolver fallback。RED/GREEN：首个 RED 为
  `zr_vm_metadata_runtime_type_layout_test` 新增 function-level resolver 用例后编译/链接失败，缺少 attach/resolve API；
  第二个 RED 为 `zr_vm_gc_test` 的
  `test_gc_minor_collection_rewrites_inline_frame_value_with_layout_visitor` 失败（期望 work=2，实际 work=1），证明
  未绑定 AOT registry 的解释器 inline-frame GC 不能丢失 prototype fallback。GREEN 后 attached registry
  返回 code-registration layout 并拒绝 stale prototype cache，GC interpreter fallback 恢复。验证：WSL gcc/clang
  均通过 metadata type-layout 7/0、metadata query 20/0、AOT GC root-frame 5/0、GC 66/0、value-type runtime 14/0、
  frame setup 1/0、source contracts 19/0、value-type smoke 2/0、shared-library smoke 8/0、descriptor diagnostics 2/0、
  generic reference sharing 4/0；Windows MSVC Debug 同组通过，其中 value-type smoke 1 ignored、
  shared-library smoke 8 ignored、descriptor diagnostics 2 ignored 均为既有 Unix-only 分支。
  产出：`tests/acceptance/2026-06-25-aot-11-s4g-gc-inline-frame-runtime-layout-resolver.md`。
  备注：`metadata_runtime.c`、`gc_mark.c`、`gc_cycle.c` 和 `aot_runtime.c` 已接近或超过大文件阈值；本切片只做
  resolver/attach 接线和兼容性修复，没有新增独立职责，因此未在本切片拆分文件。验证中既有编译器告警未形成失败。

- 2026-06-25 18:45:50 +08:00 · 11-S4F GC descriptor metadata-runtime resolver ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，TypeSpec/generic layout materialization、
  runtime layout construction、ownership offset 表发射、反射消费端迁移、GC inline-frame scanning 迁移和
  完整三向缓存仍待后续。完成项目：新增
  `ZrCore_MetadataRuntime_ResolveGcDescriptor(runtime, typeLayoutId)`，通过 runtime 的
  code-registration `gcDescriptors[typeLayoutId]` 解析 `SZrAotGcDescriptor`，并要求 descriptor
  的 `typeLayoutId` 与查询 id 一致，同时同一 id 必须能经
  `ZrCore_MetadataRuntime_ResolveTypeLayout()` 解析到 registry-backed `SZrTypeLayout`；空 runtime、
  `NONE` id、越界 id、稀疏空 descriptor、descriptor/layout registry 脱节和 descriptor id 不匹配均返回 null。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_type_layout_test` 新增 GC descriptor resolver 用例后编译失败，
  缺少 `ZrCore_MetadataRuntime_ResolveGcDescriptor()`；GREEN 后 resolver 返回 code-registration registry 中的
  matching descriptor，并证明即使存在 stale prototype layout cache 或单独 descriptor 表，也不会绕过
  runtime layout resolver。验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_type_layout_test` 5/0、
  `zr_vm_metadata_runtime_query_test` 20/0、`zr_vm_aot_gc_root_frame_test` 5/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0、`zr_vm_aot_c_shared_library_smoke_test` 8/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 2/0、
  `zr_vm_aot_c_generic_reference_sharing_test` 4/0；Windows MSVC Debug 同组通过 type-layout runtime 5/0、
  metadata query 20/0、root-frame 5/0、frame setup 1/0、source contracts 19/0、value-type smoke 2/0
  （1 ignored Unix-only 分支）、shared-library smoke 8/0（8 ignored Unix-only 分支）、
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）、generic reference sharing 4/0。
  产出：`tests/acceptance/2026-06-25-aot-11-s4f-gc-descriptor-runtime-resolver.md`。
  备注：本切片只关闭 code-registration GC descriptor lookup 到 metadata runtime layout resolver 的公共入口；
  不声明 `SZrAotGcRootMap` frame-byte-offset 根扫描、optional local-address roots、runtime layout construction、
  TypeSpec/generic layout materialization 或反射 consumer 迁移完成。验证中 gcc/clang/MSVC 仍有既有
  unused/const/unreachable-code/generated-C logical-not 等告警，均未形成失败。

- 2026-06-25 18:22:45 +08:00 · 11-S4E generic dictionary metadata-runtime type-layout resolver ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，TypeSpec/generic layout materialization、
  runtime layout construction、ownership offset 表发射、反射/GC 强制统一消费者和完整三向缓存仍待后续。
  完成项目：`ZrLibrary_AotRuntime_GenericSlot_TypeLayout()` 与
  `ZrLibrary_AotRuntime_GenericSlot_TryGetSizeOf()` 的 TYPE_LAYOUT/SIZEOF 解析入口改为接收
  `SZrMetadataRuntime*`，并通过 `ZrCore_MetadataRuntime_ResolveTypeLayout()` 读取 code-registration
  layout registry；prototype frame layout cache 不再作为 fallback。generated C 的
  `ZrAot_GenericSlot_TypeLayout(metadataRuntime, dict, slotIndex)` 宏与 shared-reference generic 函数签名
  同步携带 `metadataRuntime`。
  RED/GREEN：RED 为 `zr_vm_aot_c_generic_reference_sharing_test` 新增 runtime 断言后失败，期望字典
  typeLayoutId 42 返回 `codeRegistration->typeLayouts[42]`，实际仍返回 null/旧 prototype 路径；
  GREEN 后 TYPE_LAYOUT/SIZEOF 返回 metadata runtime registry 中的 layout/size，并证明缺失 registry 时不会
  fallback 到 metadata function prototype layout cache。
  验证：WSL gcc/clang 均通过 `zr_vm_aot_c_generic_reference_sharing_test` 4/0、
  `zr_vm_aot_c_generic_call_typed_test` 6/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_metadata_runtime_type_layout_test` 3/0、
  `zr_vm_metadata_runtime_query_test` 20/0、`zr_vm_aot_c_shared_library_smoke_test` 8/0、
  `zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 2/0；Windows MSVC Debug 同组通过 reference sharing 4/0、
  generic call typed 6/0（3 ignored Unix-only 分支）、source contracts 19/0、frame setup 1/0、
  type-layout runtime 3/0、metadata runtime query 20/0、shared-library smoke 8/0（8 ignored Unix-only 分支）、
  value-type shared-library smoke 2/0（1 ignored Unix-only 分支）、descriptor diagnostics 2/0
  （2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s4e-generic-dictionary-type-layout-runtime-resolver.md`。
  备注：本切片只关闭泛型字典 TYPE_LAYOUT/SIZEOF consumer 到 metadata runtime resolver 的接线；不声明
  共享 generic 函数运行期调度形态、TypeSpec materialization、runtime layout construction 或反射/GC
  consumer 统一迁移完成。验证中 clang generated-C logical-not 括号告警、MSVC const/unreachable-code 告警为既有告警。

- 2026-06-25 17:54:28 +08:00 · 11-S4D metadata runtime public type-layout resolver ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，TypeSpec/generic layout materialization、
  runtime layout construction、ownership offset 表发射、反射/泛型/GC 强制统一消费者和完整三向缓存仍待后续。
  完成项目：`SZrMetadataRuntime` attach 时镜像 `typeLayoutCount`；新增
  `ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, typeLayoutId)`，经 `SZrMetadataRuntime.typeLayoutCount`
  和 `codeRegistration->typeLayouts[typeLayoutId]` 解析 layout；空 runtime、`NONE` layout id、越界 id、
  稀疏空项、缺失 registry 和 `layout->cTypeId` 不匹配均返回 null；`ReadTypeDefLayoutBindingView()` 复用同一入口。
  新增独立 `zr_vm_metadata_runtime_type_layout_test`，避免继续扩张已超过 1000 行的 metadata runtime query 测试文件。
  RED/GREEN：RED 为新增 focused 测试目标后编译失败，缺少 `SZrMetadataRuntime.typeLayoutCount`
  和 `ZrCore_MetadataRuntime_ResolveTypeLayout()`；GREEN 后 public resolver 返回 code-registration registry 中的
  matching layout，并证明不会 fallback 到 metadata function prototype layout cache。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_type_layout_test` 3/0、
  `zr_vm_metadata_runtime_query_test` 20/0、`zr_vm_aot_c_frame_setup_contracts_test` 1/0、
  `zr_vm_aot_c_source_contracts_test` 19/0、`zr_vm_aot_c_shared_library_smoke_test` 8/0、
  `zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 2/0；Windows MSVC Debug 通过 type-layout runtime 3/0、
  metadata runtime query 20/0、frame setup 1/0、source contracts 19/0、shared-library smoke 8/0
  （8 ignored Unix-only 分支）、value-type shared-library smoke 2/0（1 ignored Unix-only 分支）、
  descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s4d-metadata-runtime-public-type-layout-resolver.md`。
  备注：本切片只关闭 runtime public layout registry lookup 入口和 TypeDef binding 复用；不声明
  TypeSpec/generic layout、runtime layout 构建副作用、ownership offsets、reflection/generic/GC 消费端
  统一迁移或 metadata policy 完成。验证中 clang 的 generated-C logical-not 括号告警为既有警告，未形成测试失败。

- 2026-06-25 17:32:12 +08:00 · 11-S4C metadata runtime registry-backed TypeDef layout binding ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，TypeSpec/generic layout materialization、
  runtime layout construction、ownership offset 表发射、反射/泛型/GC 强制统一消费者和完整三向缓存仍待后续。
  完成项目：`ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()` 保留 TypeDef token→zrp TypeDef row→
  `typeLayoutId/cTypeId`、layout version/hash 的 identity view，但 `typeLayout` 指针现在只从
  `runtime->codeRegistration->typeLayouts[typeLayoutId]` 读取；当 registry 缺失、越界、稀疏空项或
  `layout->cTypeId` 不匹配时不暴露 layout 指针。S4A 中的 prototype layout cache 不再作为运行期返回来源。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增 registry-backed 断言后失败，
  期望返回 `codeRegistration->typeLayouts[42]`，实际仍返回 metadata function 的 prototype layout 指针；
  GREEN 后同一 TypeDef row 优先绑定 generated/module code registration registry，并保留 cTypeId/hash/version 绑定。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 20/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、
  `zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 2/0；Windows MSVC Debug 通过 metadata runtime query 20/0、
  frame setup 1/0、source contracts 19/0、shared-library smoke 8/0（8 ignored Unix-only 分支）、
  value-type shared-library smoke 2/0（1 ignored Unix-only 分支）、descriptor diagnostics 2/0
  （2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s4c-metadata-runtime-registry-backed-layout-binding.md`。
  备注：本切片只关闭 TypeDef binding view 对 code-registration layout registry 的运行期读取；
  不声明 TypeSpec/generic layout、runtime layout 构建副作用、ownership offsets、reflection/generic/GC 消费端
  统一迁移或 metadata policy 完成。验证中 clang 的 generated-C logical-not 括号告警和 MSVC 的
  `argumentNode` 可能未初始化告警为既有警告，未形成测试失败。

- 2026-06-25 17:17:50 +08:00 · 11-S4B code-registration type layout registry ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，TypeSpec/generic layout materialization、
  runtime layout construction、ownership offset 表发射、反射/泛型/GC 强制统一消费者和完整三向缓存仍待后续。
  完成项目：`ZR_VM_AOT_ABI_VERSION` 升至 9；`SZrAotCodeRegistration` 与 `ZrAotCompiledModule`
  新增 `typeLayouts/typeLayoutCount`；generated C 为可达 inline struct layout 发射
  `SZrTypeLayoutField`、`SZrTypeLayout` 静态描述符和 `zr_aot_type_layouts[]` 稀疏表，按
  `typeLayoutId/cTypeId` 索引；code registration 和 module descriptor 指向同一 registry；
  runtime descriptor validation 校验 registry 指针/计数一致性和空表形态。
  RED/GREEN：RED 为 source contract 要求 `typeLayouts/typeLayoutCount` 后失败，缺少
  `const struct SZrTypeLayout *const *typeLayouts;`；GREEN 后无 layout 模块暴露 null/0，一般值类型模块暴露
  非空 registry，动态库 smoke 验证 `layout->cTypeId == descriptor->typeLayoutId` 且 GC offsets 对齐。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 19/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、
  `zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 2/0；Windows MSVC Debug 通过 metadata runtime query 19/0、
  frame setup 1/0、source contracts 19/0、shared-library smoke 8/0（8 ignored Unix-only 分支）、
  value-type shared-library smoke 2/0（1 ignored Unix-only 分支）、descriptor diagnostics 2/0
  （2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s4b-code-registration-type-layout-registry.md`。
  备注：本切片只关闭 generated-C code-registration layout registry 的 carrier/emission/validation；
  不声明 TypeSpec/generic layout、runtime layout 构建副作用、ownership offsets、reflection entity materialization、
  generic dictionary layout lookup 或 code stripping metadata policy 完成。

- 2026-06-25 16:39:14 +08:00 · 11-S4A metadata runtime TypeDef layout binding view ·
  状态：11-S4 子切片完成；完整 11-S4 仍未关闭，code-registration layout 表发射、
  TypeSpec/generic layout materialization、runtime layout 构建和反射/泛型/GC 强制统一读取仍待后续。
  完成项目：新增 `SZrMetadataRuntimeTypeDefLayoutBindingView` 与
  `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()`；runtime 从 attached zrp metadata 的
  `TYPE_DEFS` section 线性匹配 `TYPE_DEF` token，绑定现有 type token record、zrp
  `SZrZrpMetadataTypeDefRow`、`typeLayoutId/cTypeId`、layout version/hash，并且只在 metadata function
  已有 `prototypeFrameTypeLayouts[typeLayoutId]` 且 `SZrTypeLayout.cTypeId` 与 row `typeLayoutId` 一致时
  暴露 cached `SZrTypeLayout` 指针。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增 TypeDef layout binding view 用例后编译失败，
  缺少 `SZrMetadataRuntimeTypeDefLayoutBindingView` 与
  `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()`；GREEN 后空 runtime/out、非 `TYPE_DEF` token、
  未 attached zrp metadata 均拒绝，合法 TypeDef row 可绑定到 token record、cTypeId 和匹配 cached layout。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 19/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 19/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s4a-metadata-runtime-typedef-layout-binding-view.md`。
  备注：本切片只关闭 TypeDef-backed metadata token → cTypeId/layout identity view；不声明
  TypeSpec/generic layout、code registration layout registry、layout 构建副作用、GC descriptor lookup、
  reflection entity materialization 或 code stripping metadata policy 完成。

- 2026-06-25 16:16:39 +08:00 · 11-S3M metadata runtime MethodSpec signature view ·
  状态：11-S3 子切片完成；完整 11-S3 仍未关闭，recursive generic argument semantic binding、
  method instantiation materialization、row-to-entity materialization、token→运行期实体物化和完整缓存仍待后续。
  完成项目：新增 `SZrMetadataRuntimeMethodSpecSignatureView` 与
  `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView()`；MethodSpec 继续使用 `SIGNATURE` token，
  runtime 直接读取该 signature record 的 zrp signature blob，要求 related/owner 指向 method token，
  签名体为 `GENERIC_INST(MEMBER_REF methodToken, args...)`，并暴露 methodSpec token、method token、
  method record、signature hash、method node、argument count 和 argument-list blob offset。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增 MethodSpec signature view 用例后编译失败，
  缺少 `SZrMetadataRuntimeMethodSpecSignatureView` 与
  `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView()`；GREEN 后空 out view、非 `SIGNATURE` token、
  未 attached zrp metadata 均拒绝，合法 MethodSpec signature view 可绑定到本地 `MEMBER_DEF` method record。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 18/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 18/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3m-metadata-runtime-methodspec-signature-view.md`。
  备注：本切片只关闭 MethodSpec signature 的只读身份/结构 view 与 method record binding；不声明
  MethodSpec token 编码变更、method instantiation 实体、generic dictionary、argument recursive binding、
  row-to-runtime entity materialization、反射实体构造或 code stripping metadata policy 完成。

- 2026-06-25 16:04:29 +08:00 · 11-S3L metadata runtime generic TypeSpec argument binding view ·
  状态：11-S3 子切片完成；完整 11-S3 仍未关闭，recursive generic argument semantic binding、
  MethodSpec runtime binding、row-to-entity materialization、token→运行期实体物化和完整缓存仍待后续。
  完成项目：新增 `SZrMetadataRuntimeTypeSpecGenericArgumentView` 与
  `ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView()`；复用 11-S3K 的 TypeSpec generic binding view，
  按 argument index 遍历 `GENERIC_INST` argument list，暴露 argument type-node，并在直接
  `TYPE_REF/TYPE_DEF` argument 上匹配现有 type record、暴露 argument token/record。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增 generic argument binding view 用例后编译失败，
  缺少 `SZrMetadataRuntimeTypeSpecGenericArgumentView` 与
  `ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView()`；GREEN 后空 out view、非 `TYPE_SPEC` token、
  未 attached zrp metadata、越界 argument index 均拒绝，`GENERIC_INST(TYPE_REF base, INT64, TYPE_REF arg)`
  能读取 primitive argument 节点并把第二个 `TYPE_REF` argument 绑定到 module type record。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 17/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 17/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3l-metadata-runtime-generic-typespec-argument-binding.md`。
  备注：本切片只关闭 TypeSpec generic indexed argument 节点读取与直接 `TYPE_REF/TYPE_DEF` argument token
  绑定；不声明嵌套/递归 argument 语义绑定、baseToken 编码标准化、layout/type entity 物化、
  generic dictionary 解析、MethodSpec 解析、row-to-runtime entity materialization、反射实体构造或
  code stripping metadata policy 完成。

- 2026-06-25 15:48:39 +08:00 · 11-S3K metadata runtime generic TypeSpec base-token binding view ·
  状态：11-S3 子切片完成；完整 11-S3 仍未关闭，generic argument semantic binding、
  MethodSpec runtime binding、row-to-entity materialization、token→运行期实体物化和完整缓存仍待后续。
  完成项目：新增 `SZrMetadataRuntimeTypeSpecGenericBindingView` 与
  `ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView()`；复用 11-S3J 的 TypeSpec signature view，
  将 root `GENERIC_INST` 的 base `TYPE_REF/TYPE_DEF` node 与候选 type record 的 signature blob 匹配，
  暴露 base token/base record 以及原 signature view。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增 base-token binding view 用例后编译失败，
  缺少 `SZrMetadataRuntimeTypeSpecGenericBindingView` 与
  `ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView()`；GREEN 后空 out view、非 `TYPE_SPEC` token、
  未 attached zrp metadata 均拒绝，`GENERIC_INST(TYPE_REF, INT64)` 与 `GENERIC_INST(TYPE_DEF, INT64)`
  分别绑定到 module `TYPE_REF` record 和本地 `TYPE_DEF` record。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 16/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 16/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3k-metadata-runtime-generic-typespec-base-token-binding.md`。
  备注：本切片只关闭 TypeSpec generic base node 到现有 type record 的只读绑定 view；不声明 baseToken
  编码标准化、generic argument 语义绑定、layout/type entity 物化、generic dictionary 解析、MethodSpec 解析、
  row-to-runtime entity materialization、反射实体构造或 code stripping metadata policy 完成。

- 2026-06-25 15:34:40 +08:00 · 11-S3J metadata runtime generic TypeSpec signature view ·
  状态：11-S3 子切片完成；完整 11-S3 仍未关闭，generic semantic binding、
  row-to-entity materialization、token→运行期实体物化和完整缓存仍待后续。
  完成项目：新增 `SZrMetadataRuntimeTypeSpecSignatureView` 与
  `ZrCore_MetadataRuntime_ReadTypeSpecSignatureView()`；在 `TYPE_SPEC` token 上验证 paired
  `SIGNATURE` record、validated signature blob、root `GENERIC_INST` type-node 与 base `TYPE_REF/TYPE_DEF`
  type-node，并暴露 TypeSpec token、signature token/hash、blob slice、generic root node、base node、
  argument count 和 argument-list blob offset。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增
  `test_metadata_runtime_reads_generic_type_spec_signature_view` 后编译失败，缺少
  `SZrMetadataRuntimeTypeSpecSignatureView` 与 `ZrCore_MetadataRuntime_ReadTypeSpecSignatureView()`；
  GREEN 后空 runtime/out view、非 `TYPE_SPEC` token、未 attached zrp metadata 均拒绝，合法
  `GENERIC_INST(TYPE_REF, INT64)` TypeSpec 签名 view 通过。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 14/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 14/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3j-metadata-runtime-generic-typespec-signature-view.md`。
  备注：本切片只关闭 TypeSpec generic signature 的只读身份/结构 view；不声明 base token 标准化、
  layout/type entity 物化、generic dictionary 解析、MethodSpec 解析、row-to-runtime entity materialization、
  反射实体构造或 code stripping metadata policy 完成。

- 2026-06-25 15:18:58 +08:00 · 11-S3I metadata runtime signature type-node view ·
  状态：11-S3 子切片完成；完整 11-S3 仍未关闭，TypeSpec/generic 语义解析、
  row-to-entity materialization、token→运行期实体物化和完整缓存仍待后续。
  完成项目：新增 `SZrMetadataRuntimeSignatureTypeNodeView` 与
  `ZrCore_MetadataRuntime_ReadSignatureTypeNode()`；在 `GetSignatureBlob()` 的 validated slice 上按 blob offset
  读取 primitive、TYPE_REF/TYPE_DEF、GENERIC_INST 等 type-node 的 node kind、payload、base type offset、
  child count、child list offset 与 next blob offset；失败路径清零输出 view。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增
  `test_metadata_runtime_reads_signature_type_node_views` 后编译失败，缺少
  `SZrMetadataRuntimeSignatureTypeNodeView` 与 `ZrCore_MetadataRuntime_ReadSignatureTypeNode()`；GREEN 后空 blob/
  空 out view/越界 offset 拒绝、method return/parameter primitive type node、TypeSpec `GENERIC_INST` base/argument
  子节点 view 均通过。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 13/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 13/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3i-metadata-runtime-signature-type-node-view.md`。
  备注：本切片只关闭 signature type-node 的只读结构 view；不声明 TypeSpec/generic 语义绑定、
  FIELD_SIG 类型实体解析、运行期 method/field/type entity 物化、layout 三向表、反射实体构造、泛型字典解析或
  code stripping metadata policy 完成。

- 2026-06-25 15:03:19 +08:00 · 11-S3H metadata runtime method/field signature header view ·
  状态：11-S3 子切片完成；完整 11-S3 仍未关闭，nested signature type-node materialization、
  TypeSpec/generic 语义解析、row-to-entity materialization 和 token→运行期实体物化仍待后续。
  完成项目：新增 `SZrMetadataRuntimeSignatureView` 与 `ZrCore_MetadataRuntime_ReadSignatureView()`；
  在 `GetSignatureBlob()` 的 validated slice 上读取 method signature 的 root node、calling convention、flags、
  generic parameter count、parameter count、return type blob offset 与 parameter list blob offset；读取 field
  signature 的 root node、flags 和 field type blob offset；nested type node 只用于跳过以定位后续 header 字段。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增
  `test_metadata_runtime_reads_method_and_field_signature_views` 后编译失败，缺少
  `SZrMetadataRuntimeSignatureView` 与 `ZrCore_MetadataRuntime_ReadSignatureView()`；GREEN 后空 runtime/空 out view/
  未 attached runtime 拒绝、method header view、field header view 和 blob 内偏移均通过。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 12/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 12/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3h-metadata-runtime-signature-header-view.md`。
  备注：本切片只关闭 method/field signature 顶层 header view；不声明 nested type-node AST、
  FIELD_SIG 类型实体解析、TypeSpec/generic 语义绑定、运行期 method/field/type entity 物化、layout 三向表、
  反射实体构造、泛型字典解析或 code stripping metadata policy 完成。

- 2026-06-25 14:51:27 +08:00 · 11-S3G metadata runtime validated signature blob view ·
  状态：11-S3 子切片完成；完整 11-S3 仍未关闭，signature blob semantic node materialization、
  FIELD_SIG/TypeSpec/generic 语义解析、row-to-entity materialization 和 token→运行期实体物化仍待后续。
  完成项目：新增 `ZrCore_MetadataRuntime_GetSignatureBlob()`；runtime 先要求已 attach zrp metadata，再按
  entity token 复用 `ResolveSignatureRecord()` 找 paired `SIGNATURE` record，随后从 signature blob pool 取
  bounded slice，并调用 `ZrCore_ZrpMetadata_ValidateSignatureBlob()` 拒绝截断/非法结构；失败时输出 slice 清零。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增
  `test_metadata_runtime_gets_validated_signature_blob_view` 后链接失败，缺少
  `ZrCore_MetadataRuntime_GetSignatureBlob()`；GREEN 后空 runtime/空 token/空 out slice/未 attached runtime 拒绝、
  method signature blob view 查询、payload 指针/长度匹配和截断 signature 拒绝均通过。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 11/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 11/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3g-metadata-runtime-signature-blob-view.md`。
  备注：本切片只关闭 entity token 到 validated signature blob pool slice 的查询；不声明 signature AST、
  method/field/type signature semantic parsing、FIELD_SIG/TypeSpec/generic 语义绑定、运行期实体物化、layout 三向表、
  反射实体构造、泛型字典解析或 code stripping metadata policy 完成。

- 2026-06-25 14:40:17 +08:00 · 11-S3F metadata runtime zrp metadata mmap attach ·
  状态：11-S3 子切片完成；完整 11-S3 仍未关闭，signature blob semantic parsing、
  FIELD_SIG/TypeSpec/generic 语义解析、row-to-entity materialization 和 token→运行期实体物化仍待后续。
  完成项目：`SZrMetadataRuntime` 新增 `hasZrpMetadata`、`zrpMetadataBuffer`、`zrpMetadataBufferLength`
  与 `zrpMetadataHeader`；新增 `ZrCore_MetadataRuntime_AttachZrpMetadata()` 校验并保存只读 zrp data metadata
  buffer/header；新增 `ZrCore_MetadataRuntime_GetZrpSectionView()` 从 runtime header 查询 section view；
  空 runtime、空 buffer、短 header 和未 attached runtime 均被拒绝。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增
  `test_metadata_runtime_attaches_zrp_metadata_view` 后编译/链接失败，缺少 runtime zrp metadata 字段和
  attach/view API；实现后补齐 `zr_vm_core/memory.h` include，GREEN 后 header validation、buffer/header 挂载、
  typeDefs section view 和空 runtime 拒绝均通过。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 10/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 10/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3f-metadata-runtime-zrp-mmap-attach.md`。
  备注：本切片只关闭 data metadata mmap attach 与 raw section view 查询；不声明定义表 row 语义解析、
  string/constant/signature pool 语义解析、signature blob semantic parsing、token→运行期实体物化、
  layout 三向表、反射实体构造、泛型字典解析或 code stripping metadata policy 完成。

- 2026-06-25 14:24:52 +08:00 · 11-S3E metadata runtime field record lazy cache ·
  状态：11-S3 子切片完成；完整 11-S3 仍未关闭，data metadata mmap 查询、signature blob semantic parsing、
  FIELD_SIG/TypeSpec/generic 语义解析和 token→运行期实体物化仍待后续。
  完成项目：`SZrMetadataRuntime` 新增 `fieldRecordCacheToken` 与 `fieldRecordCache`；新增
  `ZrCore_MetadataRuntime_ResolveFieldRecord()`；字段 token 当前复用 `MEMBER_DEF` / `MEMBER_REF`，
  因此 resolver 按 member token 从 attached metadata function 的本地 records 或 module metadata ref records
  取 field record；field cache 独立于 method cache，method lookup 覆盖 method cache 后字段二次查询仍命中 field cache。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增
  `test_metadata_runtime_resolves_field_records_with_independent_cache` 后链接失败，缺少
  `ZrCore_MetadataRuntime_ResolveFieldRecord()`；GREEN 后 local field record、imported field record、非法 token
  拒绝和独立 field cache 命中均通过。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 9/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 9/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3e-metadata-runtime-field-record-cache.md`。
  备注：本切片只关闭 field token record 层 lazy/cache 查询；不声明 `FIELD_SIG` blob 解析、method/field
  语义区分、运行期 field entity 物化、data metadata mmap、layout 三向表、反射实体构造、泛型字典解析或
  code stripping metadata policy 完成。

- 2026-06-25 14:10:48 +08:00 · 11-S3D metadata runtime TypeSpec type record cache ·
  状态：11-S3 子切片完成；完整 11-S3 仍未关闭，`ResolveField`、data metadata mmap
  查询、signature blob semantic parsing、TypeSpec/generic 语义解析和 token→运行期实体物化仍待后续。
  完成项目：`ZrCore_MetadataRuntime_ResolveTypeRecord()` 现在接受 `TYPE_SPEC` token，并从 attached
  metadata function 的本地 `metadataTokenRecords` lazy resolve TypeSpec record；二次查询复用既有
  `typeRecordCacheToken` / `typeRecordCache`；非法非 type token 拒绝语义保持不变。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增
  `test_metadata_runtime_resolves_type_spec_records_as_type_records` 后返回 null；GREEN 后 local TypeSpec
  record 查询和 TypeSpec type cache 命中均通过。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 8/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 8/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3d-metadata-runtime-typespec-record-cache.md`。
  备注：本切片只关闭 TypeSpec token record 层 lazy/cache 查询；不声明 signature blob 语义解析、
  generic instantiation 绑定、运行期 type/layout entity 物化、field resolve、data metadata mmap、layout 三向表、
  反射实体构造、泛型字典解析或 code stripping metadata policy 完成。

- 2026-06-25 14:02:23 +08:00 · 11-S3C metadata runtime signature record lazy cache ·
  状态：11-S3 子切片完成；完整 11-S3 仍未关闭，`ResolveField`、TypeSpec、data metadata mmap
  查询、signature blob semantic parsing 和 token→运行期实体物化仍待后续。
  完成项目：`SZrMetadataRuntime` 新增 `signatureRecordCacheEntityToken` 与
  `signatureRecordCache`；新增 `ZrCore_MetadataRuntime_ResolveSignatureRecord()`；按 entity token 先从
  attached metadata function 的本地 signature record 查询，再从 `moduleMetadataTokenRecords` signature
  record 查询；SIGNATURE token、空 runtime 和空 token 被拒绝；二次查询命中 signature record cache。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增
  `test_metadata_runtime_resolves_signature_records_lazily` 后链接失败，缺少
  `ZrCore_MetadataRuntime_ResolveSignatureRecord()`；GREEN 后 local signature record、imported signature
  record、非法 token 拒绝和缓存命中均通过。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 7/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 7/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3c-metadata-runtime-signature-record-cache.md`。
  备注：本切片只关闭 entity token → SIGNATURE record 的 lazy/cache 查询；不声明 field resolve、TypeSpec、
  data metadata mmap、signature blob semantic parsing、layout 三向表、反射实体构造、泛型字典解析或
  code stripping metadata policy 完成。

- 2026-06-25 13:52:15 +08:00 · 11-S3B metadata runtime type record lazy cache ·
  状态：11-S3 子切片完成；完整 11-S3 仍未关闭，`ResolveField`、TypeSpec、data metadata mmap
  查询、signature semantic resolution 和 token→运行期实体物化仍待后续。
  完成项目：`SZrMetadataRuntime` 新增 `typeRecordCacheToken` 与 `typeRecordCache`；
  新增 `ZrCore_MetadataRuntime_ResolveTypeRecord()`；`TYPE_DEF` 从 attached metadata function 的
  `metadataTokenRecords` lazy resolve，`TYPE_REF` 从 `moduleMetadataTokenRecords` lazy resolve；
  非 type token、空 runtime 和空 token 被拒绝；二次查询命中 type record cache。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增
  `test_metadata_runtime_resolves_type_records_lazily` 后链接失败，缺少
  `ZrCore_MetadataRuntime_ResolveTypeRecord()`；GREEN 后 local type record、imported type record、
  非法 token 拒绝和缓存命中均通过。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 6/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 6/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3b-metadata-runtime-type-record-cache.md`。
  备注：本切片只关闭 type token record 层 lazy/cache 查询；不声明 field resolve、TypeSpec、
  data metadata mmap、layout 三向表、反射实体构造、泛型字典解析或 code stripping metadata policy 完成。

- 2026-06-25 13:40:40 +08:00 · 11-S3A metadata runtime method record lazy cache ·
  状态：11-S3 子切片完成；完整 11-S3 仍未关闭，`ResolveType`、`ResolveField`、
  data metadata mmap 查询、signature semantic resolution 和 token→运行期实体物化仍待后续。
  完成项目：`SZrMetadataRuntime` 新增 `methodRecordCacheToken` 与 `methodRecordCache`；
  新增 `ZrCore_MetadataRuntime_ResolveMethodRecord()`；`MEMBER_DEF` 从 attached metadata function 的
  `metadataTokenRecords` lazy resolve，`MEMBER_REF` 从 `moduleMetadataTokenRecords` lazy resolve；
  非 method token、空 runtime 和空 token 被拒绝；二次查询命中 method record cache。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 新增
  `test_metadata_runtime_resolves_method_records_lazily` 后链接失败，缺少
  `ZrCore_MetadataRuntime_ResolveMethodRecord()`；GREEN 后 local method record、imported method record、
  非法 token 拒绝和缓存命中均通过。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 5/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 5/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s3a-metadata-runtime-method-record-cache.md`。
  备注：本切片只关闭 method token record 层 lazy/cache 查询；不声明 type/field resolve、data metadata mmap、
  layout 三向表、反射实体构造、泛型字典解析或 code stripping metadata policy 完成。

- 2026-06-25 13:19:32 +08:00 · 11-S2C module metadata runtime registration carrier ·
  状态：11-S2 子切片完成；AOT 模块加载时的最小 `SZrMetadataRuntime` attach 载体已落地。
  完整 token→函数/layout lazy 解析、缓存和 token↔layout 三向表仍留给 11-S3/11-S4。
  完成项目：新增 `SZrMetadataRuntime`，在 `SZrObjectModule` 上保存 module、metadata function、
  `const SZrAotCodeRegistration *codeRegistration` 与 function/method/invoker/GC descriptor 计数；
  新增 `ZrCore_Module_AttachMetadataRuntime()` / `ZrCore_Module_GetMetadataRuntime()`；AOT runtime
  模块加载记录通过 `ZrCore_Module_AttachMetadataRuntime(record.module, record.moduleFunction, record.codeRegistration)`
  登记运行期元数据载体；GC mark/rewrite 覆盖 metadata runtime 内保存的 `metadataFunction`；新增
  module query 用例和 frame setup runtime source contract。
  RED/GREEN：RED 先由 `zr_vm_metadata_runtime_query_test` 暴露缺少
  `zr_vm_core/metadata_runtime.h` 与 attach/query API；实现 core carrier 后，frame setup source contract
  继续 RED，要求 AOT loader 出现
  `ZrCore_Module_AttachMetadataRuntime(record.module, record.moduleFunction, record.codeRegistration)`。
  GREEN 后模块创建、attach/query、AOT loader attach、失败诊断和 GC 引用维护均有 focused 覆盖。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 4/0、
  `zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；
  Windows MSVC Debug 通过 metadata runtime query 4/0、frame setup 1/0、source contracts 19/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics 2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s2c-metadata-runtime-registration.md`。
  备注：本切片只关闭 module-load metadata runtime registration carrier；不声明 data metadata mmap attach、
  token→函数/layout lazy resolve/cache、token↔layout 三向表、默认最小 metadata 策略或 dump/diff 工具完成。

- 2026-06-25 12:58:28 +08:00 · 11-S2B runtime code registration context carrier ·
  状态：11-S2 子切片完成；完整 11-S2 仍未关闭，模块加载登记、`SZrMetadataRuntime`
  token→函数/layout 解析和缓存仍待后续。
  完成项目：`SZrLibraryAotLoadedModule` 现在保存 `codeRegistration`；加载记录初始化时从
  descriptor 绑定注册表；generated module context 与 generated frame 均携带
  `const SZrAotCodeRegistration *codeRegistration`；`methodInfo` 解析、frame/context 的
  `functionThunks` 兼容字段、callable 常量物化、native direct call、meta call 和 static direct call
  均改为从 `record->codeRegistration->methodInfos/functionPointers` 读取；AOT C frame setup 在发布
  export context 时把 `zr_aot_context.codeRegistration` 传入 generated frame。
  RED/GREEN：RED 为 frame setup source contract 要求 runtime header/context/frame 出现
  `const SZrAotCodeRegistration *codeRegistration;`、runtime source 出现
  `context->codeRegistration = record->codeRegistration;`、`record->codeRegistration->methodInfos`、
  `record->codeRegistration->functionPointers`，以及生成帧写入
  `frame.codeRegistration = zr_aot_context.codeRegistration;`；初始 RED 缺少 header/context 载体，
  第二次 RED 暴露记录绑定仍是栈值赋值而非 record 指针语义。GREEN 后运行时记录、上下文、生成帧和
  直接调用入口均通过 code registration 载体消费。
  验证：WSL gcc/clang 均通过 `zr_vm_aot_c_frame_setup_contracts_test` 1/0、
  `zr_vm_aot_c_source_contracts_test` 19/0、`zr_vm_aot_c_shared_library_smoke_test` 8/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 2/0；Windows MSVC Debug 通过 frame setup 1/0、
  source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics
  2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s2b-runtime-registration-context-carrier.md`。
  备注：本切片只关闭运行时 record/context/frame 对 code registration 的消费载体；不声明 data metadata 表导出、
  `SZrMetadataRuntime` lazy resolve、token↔layout 三向表或模块加载自动登记完成。

- 2026-06-25 12:41:09 +08:00 · 11-S2A generated-C code registration carrier ·
  状态：11-S2 子切片完成；完整 11-S2 仍未关闭，模块加载登记、`SZrMetadataRuntime`
  token→函数/layout 解析和缓存仍待后续。
  完成项目：公共 AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 8u`；新增
  `SZrAotCodeRegistration`，承载 `functionPointers`、`methodInfos`、`invokers`、
  `gcDescriptors` 及其计数；`ZrAotCompiledModule` 追加 `codeRegistration`；
  generated C 现在发射 `zr_aot_reflection_invokers[]` 与 `zr_aot_code_registration`，
  并让模块 descriptor 指向该注册表，保留旧 `functionThunks`/`methodInfos`/`gcDescriptors`
  字段作为兼容入口；运行期 descriptor validation 现在会在解引用前拒绝
  `codeRegistration == ZR_NULL`、注册表指针/计数与 legacy descriptor 字段不一致，以及 invoker 表缺失。
  RED/GREEN：RED 为 `zr_vm_aot_c_shared_library_smoke_test` 新增
  `module->codeRegistration` 断言后编译失败，frame/source 契约缺少 ABI v8、注册表结构和
  generated-C 注册表文本；补强 RED 为 descriptor diagnostics 构造 `codeRegistration = ZR_NULL`
  的 ABI v8 模块时旧 runtime 会在校验前崩溃；GREEN 后 source contract 与 shared-library descriptor
  均能看到同一套 function/method/invoker/GC descriptor 表，runtime 对缺失/不一致注册表给出诊断并拒绝加载。
  验证：WSL gcc/clang 均通过 `zr_vm_aot_c_frame_setup_contracts_test` 1/0、
  `zr_vm_aot_c_source_contracts_test` 19/0、`zr_vm_aot_c_shared_library_smoke_test` 8/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 2/0；Windows MSVC Debug 通过 frame setup 1/0、
  source contracts 19/0，shared-library smoke 8/0（8 ignored Unix-only 分支），descriptor diagnostics
  2/0（2 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-11-s2a-code-registration-carrier.md`。
  备注：本切片只关闭 generated-C code registration carrier 与 descriptor-safety validation；不声明 data metadata 表导出、
  `SZrMetadataRuntime` lazy resolve、token↔layout 三向表或模块加载自动登记完成。

- 2026-06-25 06:26:16 +08:00 · 11-S7V / 12-S3F / 12-S4N / 08-S7K manifest generic MethodSpec binding ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，跨模块 target、导出 token、默认最小 metadata 策略和
  zrp metadata dump/diff 工具仍待后续。
  完成项目：`SZrAotManifestGenericRoot` 新增 MethodSpec identity 字段；CLI AOT preserve bridge 会把
  manifest generic method target 匹配到 current-module typed exported symbol 的 method token，并扫描
  `GENERIC_INST(MEMBER_REF methodToken, args...)` 签名记录，记录 method-spec token、method token 和
  instantiation signature hash。generated C manifest 诊断同步输出这些字段，full-AOT gate 接受
  MethodSpec-bound generic method root。
  RED/GREEN：RED 为新增 MethodSpec 用例编译失败，因为 writer root 缺少 `hasMethodSpecBinding`、
  `methodSpecToken`、`methodSpecMethodToken` 和 `methodSpecSignatureHash` 字段；GREEN 后
  `Factory.make<Foo>` 绑定到 `0x08000002` / `0x03000001`。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 14/0；WSL gcc、WSL clang、Windows MSVC Debug 的 CTest
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model` 均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7v-12-s3f-manifest-generic-methodspec-binding.md`。
  备注：这是 current-module writer-visible MethodSpec 绑定；不声明 zrp MethodSpec table 持久导出、
  泛型方法代码体传递闭包、跨模块 generic method target 或默认最小 metadata 策略完成。

- 2026-06-25 06:03:45 +08:00 · 11-S7U / 12-S3E / 12-S4M / 08-S7J manifest generic synthesized TypeSpec binding ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，MethodSpec token 绑定、跨模块 target、
  导出 token、默认最小 metadata 策略和 zrp metadata dump/diff 工具仍待后续。
  完成项目：manifest generic preserve root 没有现成 `TYPE_SPEC` 时，CLI AOT preserve bridge 会在当前函数
  metadata 中查找同名 open `TYPE_DEF` 或 `TYPE_REF` record；找到后追加 writer-visible synthesized
  `TYPE_SPEC` / paired `SIGNATURE` record 与签名 blob，并用该绑定继续物化 generic instantiation identity。
  RED/GREEN：RED 为新增 full-AOT `List<Foo>` 用例仅存在 open `TYPE_REF(List)` metadata 时
  `hasTypeSpecBinding` 仍为 false；GREEN 后合成 `TYPE_SPEC` token `0x07000001`、`SIGNATURE`
  token `0x08000002`，并生成 open-base generic instantiation identity。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 13/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7u-12-s3e-manifest-generic-synthesized-typespec.md`。
  备注：本切片只合成 current-function writer 可见的 TypeSpec/signature binding；不声明完整 `.zro`
  metadata 持久化策略完成，不解析 MethodSpec，不绑定跨模块 generic target，也不关闭默认最小 metadata 策略。

- 2026-06-25 05:41:31 +08:00 · 11-S7T / 12-S3D / 12-S4L / 08-S7I generic instantiation TypeDef base token ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，MethodSpec token 绑定、缺失 TypeSpec 合成、
  跨模块 target、导出 token、默认最小 metadata 策略和 zrp metadata dump/diff 工具仍待后续。
  完成项目：current-module generic preserve TypeSpec binding 现在接受 `GENERIC_INST(TYPE_DEF target, args...)`；
  generic instantiation identity 的 base token 会按 TypeSpec base 节点选择同名 `TYPE_DEF` 或 `TYPE_REF` 记录，
  找不到时仍回退 TypeSpec token。
  RED/GREEN：RED 为 `test_cli_aot_writer_options` 新增 TypeDef-base TypeSpec 用例后，`hasTypeSpecBinding`
  仍为 false；GREEN 后 TypeSpec 绑定成功，base token 为 `0x02000001`，TypeRef open-base 与 TypeSpec fallback
  路径保持通过。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 12/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7t-12-s3d-generic-instantiation-typedef-base-token.md`。
  备注：这是 current-module TypeDef-backed open base token 绑定；不合成 TypeSpec，不解析 MethodSpec，
  不绑定跨模块 generic target，也不声明默认最小 metadata 策略完成。

- 2026-06-25 05:28:38 +08:00 · 11-S7S / 12-S3C / 12-S4K / 08-S7H generic instantiation open base token ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，MethodSpec token 绑定、缺失 TypeSpec 合成、
  跨模块 target、导出 token、默认最小 metadata 策略和 zrp metadata dump/diff 工具仍待后续。
  完成项目：generic preserve root 的 generic-instantiation identity 不再只能以 closed `TYPE_SPEC` token
  作为 base token；当前函数 metadata records 中存在同名 `TYPE_REF` 签名时，CLI preserve bridge 使用该
  open generic base token，缺失匹配时仍回退到 TypeSpec token 以兼容已有产物。
  RED/GREEN：RED 为 `test_cli_aot_writer_options` 新增 `TYPE_REF(List)` + `TYPE_SPEC(List<Foo>)` 用例后失败，
  实际 base token 仍是 `0x07000001`；GREEN 后 base token 为 `0x05000001`，旧 TypeSpec-only metadata fallback
  仍保持通过。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 11/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7s-12-s3c-generic-instantiation-open-base-token.md`。
  备注：这是 current-module TypeRef-backed open base token 绑定；不合成 TypeSpec，不解析 MethodSpec，
  不绑定跨模块 generic target，也不声明默认最小 metadata 策略完成。

- 2026-06-25 05:08:49 +08:00 · 11-S7R / 12-S8I / 12-S3B / 08-S7G full-AOT generic instantiation closure gate ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，MethodSpec token 绑定、缺失 TypeSpec 合成、
  跨模块 target、导出 token、默认最小 metadata 策略和 zrp metadata dump/diff 工具仍待后续。
  完成项目：full-AOT writer metadata 闭包现在把 generic preserve root 的 generic-instantiation identity
  纳入必备条件；TypeSpec/signature token/hash 存在但 `hasGenericInstantiationBinding` 为 false 时仍拒绝生成。
  RED/GREEN：RED 为 `test_cli_aot_writer_options` 新增 TypeSpec-only full-AOT generic root 负例后失败；
  GREEN 后该负例返回 false，既有 TypeSpec-backed `List<Foo>` materialization 和 full-AOT 未绑定 TypeSpec gate 均保持通过。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 10/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7r-12-s8i-full-aot-generic-instantiation-closure-gate.md`。
  备注：这是 writer-side metadata identity gate，不合成 TypeSpec，不解析 MethodSpec，不绑定跨模块 generic target，
  也不声明默认最小 metadata 策略完成。

- 2026-06-25 04:50:01 +08:00 · 11-S7Q / 12-S3A / 12-S4J / 08-S7F manifest generic TypeSpec-backed instantiation root ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，MethodSpec token 绑定、缺失 TypeSpec 合成、
  跨模块 target、导出 token、默认最小 metadata 策略和 zrp metadata dump/diff 工具仍待后续。
  完成项目：generic preserve root 在已有 TypeSpec/signature token/hash 之外，新增 writer 可见的
  generic instantiation identity。CLI preserve bridge 使用当前 root 的 concrete arguments 构造
  `SZrGenericInstantiationTypeArgument`，并通过 `SZrGenericInstantiationTable_GetOrAddResolved()` 记录
  TypeSpec-backed closed instance；writer root 保存 `genericInstantiationBaseToken`、`genericInstantiationInstanceId`
  和 `genericInstantiationShareKind`。generated C manifest 诊断输出这三个字段。
  RED/GREEN：RED 为 `test_cli_aot_writer_options` 新增实例化绑定断言后编译失败，因为 writer root 尚无字段；
  GREEN 后 `List<Foo>` 绑定到 `TYPE_SPEC` token `0x07000001`、generic instance id `1`、shared-reference kind `1`。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 9/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7q-12-s3a-manifest-generic-preserve-instantiation-root.md`。
  备注：这是当前模块已有 `TYPE_SPEC` 的 instantiation identity materialization；暂不解析 MethodSpec，
  不生成缺失 TypeSpec，不绑定跨模块 generic target，也不声明默认最小 metadata 策略完成。

- 2026-06-25 04:14:31 +08:00 · 11-S7P / 12-S8H / 08-S7E full-AOT manifest generic TypeSpec closure gate ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，MethodSpec token 绑定、真实 generic instantiation
  可达 roots、缺失 TypeSpec 合成、跨模块 target、导出 token、默认最小 metadata 策略和 zrp metadata dump/diff
  工具仍待后续。
  完成项目：writer 现在把 generic preserve root 的 TypeSpec 绑定结果纳入 full-AOT metadata 闭包要求；
  full-AOT 模式下，只要 `.zrp` generic preserve root 仍只有 target/arguments 文本而没有 `hasTypeSpecBinding`，
  AOT C writer 就拒绝生成。hybrid 模式保持原先的诊断清单输出。
  RED/GREEN：RED 为 `test_cli_aot_writer_options` 新增 full-AOT 未绑定 generic preserve root 用例后，
  writer 仍返回 true；GREEN 后该用例返回 false，既有 TypeSpec 绑定诊断用例继续通过。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 8/0；WSL gcc、WSL clang、Windows MSVC Debug 的
  CTest `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5。
  产出：`tests/acceptance/2026-06-25-aot-11-s7p-12-s8h-full-aot-generic-preserve-typespec-closure-gate.md`。
  备注：这是 full-AOT 对 manifest generic preserve metadata 身份的门禁，不是 MethodSpec 解析、TypeSpec 合成、
  generic instantiation 可达表、跨模块 generic target 或默认最小 metadata 策略完成。

- 2026-06-25 04:00:47 +08:00 · 11-S7O / 12-S4I / 08-S7D manifest generic preserve TypeSpec binding ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，MethodSpec token 绑定、真实 generic instantiation
  可达 roots、缺失 TypeSpec 合成、跨模块 target、导出 token、默认最小 metadata 策略和 zrp metadata dump/diff
  工具仍待后续。
  完成项目：`SZrAotManifestGenericRoot` 现在可携带 `hasTypeSpecBinding`、`typeSpecToken`、
  `signatureToken`、`signatureHash`；`ZrCli_Compiler_ApplyProjectAotPreserveRules()` 在注入 generic
  preserve writer root 时，会用 target/arguments 匹配当前函数 metadata record stream 中已有的
  `GENERIC_INST` `TYPE_SPEC` signature，并把匹配到的 entity/signature identity 写入 writer options；
  generated C manifest 诊断同步输出 TypeSpec token、signature token 和 signature hash。
  RED/GREEN：RED 为 `test_cli_aot_writer_options` 新增 TypeSpec binding 断言后编译失败，因为 writer root
  尚无 token/hash 字段；GREEN 后手工构造的 `List<Foo>` TypeSpec metadata 可被 generic preserve root 解析并输出。
  验证：WSL gcc `zr_vm_cli_aot_writer_options_test` 7/0；WSL gcc/clang 与 Windows MSVC Debug 的 CTest
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed|generic_instantiation|metadata_token_model`
  均为 5/5；`git diff --check` 退出 0（仅 LF/CRLF 提示）。产出：
  `tests/acceptance/2026-06-25-aot-11-s7o-12-s4i-manifest-generic-preserve-typespec-binding.md`。
  备注：这是现有 TypeSpec 的本模块绑定，不是 MethodSpec 解析、zrp metadata table 导出、跨模块 generic target
  解析或默认最小 metadata 策略完成。

- 2026-06-25 03:27:16 +08:00 · 11-S7N / 12-S4H / 08-S7C manifest generic preserve writer roots ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，MethodSpec/TypeSpec token 绑定、真实 generic instantiation
  可达 roots、metadata token 绑定、跨模块 target、导出 token、默认最小 metadata 策略和 zrp metadata dump/diff
  工具仍待后续。
  完成项目：`SZrAotManifestGenericRoot` 和 `SZrAotWriterOptions.manifestPreserveGenericRoots` /
  `manifestPreserveGenericRootCount` 提供 writer 侧 generic preserve root carrier；
  `SZrCliAotPreserveRoots` 扩展为同时持有 function flat-index roots 与 generic roots；
  `ZrCli_Compiler_ApplyProjectAotPreserveRules()` 在 feature 条件匹配后将 `.zrp` generic preserve target
  和 `arguments` 参数文本注入 writer options；AOT C emitter 在 generated C 头部输出
  `manifest.genericRoots` 与逐参数记录。
  RED/GREEN：RED 为 CLI writer options 测试引用缺失的 generic root fields 后编译失败；GREEN 后
  `List<Foo, Bar.Baz>` 进入 writer options，generated C 输出 `manifest.genericRoot[0]` target 与两个参数。
  验证：WSL gcc/clang `zr_vm_cli_aot_writer_options_test` 均 6/0，并且 CTest
  `cli_aot_writer_options|aot_c_code_stripping|aot_c_generic_call_typed` 均 3/3；
  Windows MSVC Debug 同目标 6/0，同 CTest 过滤 3/3；`python -m json.tool zrp.schema.json` 通过；
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。产出：
  `tests/acceptance/2026-06-25-aot-11-s7n-12-s4h-manifest-generic-preserve-writer-roots.md`。
  备注：本记录只关闭 generic preserve declaration 到 writer/generated-C manifest 清单的 bridge；不声明
  MethodSpec/TypeSpec token resolution、generic instantiation table materialization、跨模块 generic target 或
  default-minimal metadata 输出完成。

- 2026-06-25 03:02:14 +08:00 · 11-S7M / 12-S4G generic preserve argument model ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，generic instantiation root 绑定、metadata token 绑定、
  跨模块 target、导出 token、默认最小 metadata 策略、zrp metadata dump/diff 工具仍待后续。
  完成项目：`SZrLibrary_ProjectPreserveRule` 新增 `genericArguments`、`genericArgumentCount` 与
  `genericArgumentCapacity`；`.zrp` `preserve` 中 `kind: "generic"` 现在要求非空 `arguments` 数组，
  每个参数沿用 safe dotted target 校验并存入 project model；非 generic rule 携带 `arguments` 会拒绝。
  `zrp.schema.json` 同步 `arguments` 数组、`minItems: 1`，并通过条件 schema 要求 generic rule 带参数、
  带参数的 rule 必须是 generic。
  RED/GREEN：RED 先为 manifest normalization 新增 generic argument 断言后编译失败，缺
  `genericArgumentCount` / `genericArguments`；GREEN 后 `List<Foo, Bar.Baz>` 解析到 project model。
  随后新增无 `arguments`、空数组、非法参数名与非 generic rule 携带 `arguments` 的 RED，
  分别从被接受转为拒绝 manifest。
  验证：WSL gcc `zr_vm_project_manifest_normalization_test` 25/0；WSL clang 同目标 25/0；
  Windows MSVC Debug 同目标 25/0；`python -m json.tool zrp.schema.json` 通过；
  WSL gcc CTest `cli_aot_writer_options|aot_c_code_stripping` 2/2；`git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-25-aot-11-s7m-12-s4g-generic-preserve-argument-model.md`。
  备注：本记录只关闭 generic preserve 的 declaration/model/schema gate；不声明这些 generic arguments
  已绑定到 MethodSpec/TypeSpec token、泛型实例可达 roots、跨模块目标或默认最小 metadata 输出。

- 2026-06-25 02:40:15 +08:00 · 11-S7L / 12-S4F feature switch preserve root gating ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，generic preserve、metadata token 绑定、
  跨模块 target、导出 token、默认最小 metadata 策略、zrp metadata dump/diff 工具仍待后续。
  完成项目：`.zrp` manifest 新增 top-level `features` boolean switch map，`project_features.{h,c}`
  负责解析、校验 safe dotted feature 名称、存入 `SZrLibrary_ProjectFeatureSwitch` 数组并清理内存；
  `compiler_aot.c` 在注入 preserve roots 前检查 rule 的 `feature` / `featureValue`，只有显式声明的
  feature 值与期望相等时才执行该 preserve rule，未声明 feature 不匹配。
  RED/GREEN：RED 为 manifest normalization 测试缺 `featureSwitches` 模型、CLI writer options 测试缺
  `SZrLibrary_ProjectFeatureSwitch` 与 feature root gating 而编译失败；GREEN 后 manifest `features`
  解析为 true/false 开关，非法 feature 名拒绝，feature 匹配时保留 `Widget.kept`，不匹配时不注入 root
  且 generated C 裁剪 `zr_aot_fn_2`。
  验证：WSL gcc `zr_vm_project_manifest_normalization_test` 19/0、`zr_vm_cli_aot_writer_options_test` 5/0；
  WSL clang 同两目标 19/0、5/0；Windows MSVC Debug 同两目标 19/0、5/0；
  WSL gcc CTest `cli_aot_writer_options|aot_c_code_stripping` 2/2；
  `python -m json.tool zrp.schema.json` 通过；`git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-25-aot-11-s7l-12-s4f-feature-switch-preserve-root-gating.md`。
  备注：本记录关闭的是 `.zrp` feature switch 配置与当前 method/type preserve root gating；
  generic roots、metadata-token resolution、跨模块 target、annotation roots 和默认最小 metadata 仍开放。

- 2026-06-25 02:23:14 +08:00 · 11-S7K / 12-S4E preserve feature condition model ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，generic preserve、metadata token 绑定、
  跨模块 target、feature 条件求值/构建配置接入、导出 token、默认最小 metadata 策略、
  zrp metadata dump/diff 工具仍待后续。
  完成项目：`SZrLibrary_ProjectPreserveRule` 新增 `feature`、`hasFeatureValue` 与 `featureValue`，
  `.zrp` `preserve` rule parser 接受 safe dotted `feature` 名称与 boolean `featureValue`，
  并要求二者成对出现；`zrp.schema.json` 同步 `feature` / `featureValue` 字段与互相依赖约束。
  RED/GREEN：RED 为 manifest normalization 测试引用缺失的 preserve feature fields 后编译失败；
  GREEN 后 `featureValue: true/false` 均能保存在 project model 中，缺少任一半的 feature 条件会拒绝 manifest。
  验证：WSL gcc `zr_vm_project_manifest_normalization_test` 17/0；WSL clang 同目标 17/0；
  Windows MSVC Debug 同目标 17/0；`python -m json.tool zrp.schema.json` 通过；
  WSL gcc CTest `cli_aot_writer_options|aot_c_code_stripping` 2/2；`git diff --check` 退出 0
  （仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-25-aot-11-s7k-12-s4e-preserve-feature-condition-model.md`。
  备注：本记录只关闭 manifest 声明模型与 schema gate，不声明 feature switch 构建配置、
  按 feature 条件启停 roots、generic 实参 roots、metadata token resolution 或默认最小 metadata 完成。

- 2026-06-25 02:09:47 +08:00 · 11-S7J / 12-S4D dotted and type-member preserve roots ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，generic preserve、metadata token 绑定、
  跨模块 target、导出 token、默认最小 metadata 策略、zrp metadata dump/diff 工具仍待后续。
  完成项目：`method` preserve 现在先按完整 target 精确匹配 top-level callable name，可保留
  `Widget.kept` 这类 dotted callable；若未命中，再兼容 `module.method` 形式剥离当前模块前缀。
  `type` preserve 在 `members: "methods"` 或 `"all"` 时，会把 entry function 中以
  `<type>.` 为前缀的 top-level callable bindings 展开为 manifest roots，并继续去重后写入
  `SZrAotWriterOptions.manifestPreserveFunctionFlatIndices`。
  RED/GREEN：RED 为 `tests/cli/test_cli_aot_writer_options.c` 新增 dotted method target 与
  type-members prefix 两个用例后，preserve roots 数量仍为 0；GREEN 后 `Widget.kept` 精确 method rule
  解析到 flat index 2，`type Widget methods` 解析到 flat indices 1 和 2，生成 C 保留全部 3 个函数。
  验证：WSL gcc CTest `cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` 3/3；
  WSL clang 同组 3/3；Windows MSVC Debug 同组 3/3。
  产出：`tests/acceptance/2026-06-25-aot-11-s7j-12-s4d-dotted-type-method-preserve-roots.md`。
  备注：本记录只扩展当前模块 callable 命名策略和 type-to-method roots 展开；不声明 generic 实参 roots、
  metadata token resolution、跨模块 method target、注解 roots、feature switch 或默认最小 metadata 完成。

- 2026-06-25 01:53:48 +08:00 · 11-S7I / 12-S4C method preserve root binding ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，type/generic preserve、metadata token 绑定、
  导出 token、默认最小 metadata 策略、zrp metadata dump/diff 工具仍待后续。
  完成项目：新增 `ZrParser_Writer_ResolveTopLevelCallableFlatIndex()`，用 AOT function table 把 entry function
  的 `SZrFunctionTopLevelCallableBinding` 名称解析为稳定 flat index；CLI AOT helper 新增
  `SZrCliAotPreserveRoots` 与 `ZrCli_Compiler_ApplyProjectAotPreserveRules()`，把 `.zrp` `preserve`
  中当前模块的 `method` target（如 `main.kept` 或无点号本地名）绑定为
  `SZrAotWriterOptions.manifestPreserveFunctionFlatIndices`，并随 `--emit-aot-c` 发射路径传入 writer。
  RED/GREEN：RED 为 `tests/cli/test_cli_aot_writer_options.c` 先引用缺失的 preserve root helper、
  writer callable flat-index resolver 与 `SZrCliAotPreserveRoots` 后编译失败；GREEN 后 method preserve 可在
  opt-in code stripping 下保留原本不可达的 top-level callable child。
  验证：WSL gcc CTest `cli_args|cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` 4/4；
  WSL clang CTest `cli_project_incremental|cli_aot_writer_options|aot_c_code_stripping` 3/3；
  Windows MSVC Debug 同组 3/3。
  产出：`tests/acceptance/2026-06-25-aot-11-s7i-12-s4c-preserve-method-root-binding.md`。
  备注：本记录只关闭 declaration-level `method` preserve 到当前模块 top-level callable flat index 的
  writer root 注入；不声明 type/generic preserve、metadata token resolution、跨模块方法绑定、feature switch、
  默认最小 metadata 或 dump/diff 工具完成。

- 2026-06-25 01:13:27 +08:00 · 11-S7H CLI AOT C emission entry ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，preserve symbol/token/flat-index 绑定、
  导出 token、默认最小 metadata 策略、zrp metadata dump/diff 工具仍待后续。
  完成项目：CLI command model/parser/help 新增 `--emit-aot-c`；项目路径层新增
  `ZrCli_Project_ResolveAotCPath()`，普通模块输出到 `bin/aot_c/src/<module>.c`，依赖模块输出到依赖包
  `bin/aot_c/src/...`；incremental manifest 升级为 v3 并写入 `aot_c` 路径，用于移除模块时清理旧 C；
  编译流程在 `.zro` 写出和 hash 后读取 binary blob，并用 `inputKind = ZR_AOT_INPUT_KIND_BINARY`
  调用 AOT C writer。
  RED/GREEN：RED 为 CLI args 测试新增 `emitAotC` 断言后编译失败；GREEN 后 CLI args 与
  project incremental 新增/既有用例通过，manifest v3 断言同步更新。
  验证：WSL gcc `cli_args|cli_project_incremental` CTest 2/2；WSL clang 同组 2/2；
  Windows MSVC Debug 同组 2/2；Windows MSVC CLI `--compile --emit-aot-c --incremental`
  删除 `main.c` 后重新生成 `bin/aot_c/src/main.c`（114478 bytes）；`git diff --check` 退出 0，仅 LF/CRLF 提示。
  产出：`tests/acceptance/2026-06-25-aot-11-s7h-cli-aot-c-emission-entry.md`。
  备注：本记录只关闭 CLI/project 发射入口和产物路径/manifest tracking；preserve 规则尚未绑定到
  writer roots，默认最小 metadata 和 dump/diff 工具仍未完成。

- 2026-06-25 00:29:49 +08:00 · 11-S7G zrp project manifest AOT mode writer injection ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，preserve symbol/token/flat-index 绑定、导出 token、
  CLI AOT C 发射入口接线、默认最小 metadata 策略、zrp metadata dump/diff 工具仍待后续。
  完成项目：CLI/compiler 新增 `ZrCli_Compiler_ApplyProjectAotWriterOptions()`，把
  `SZrCliProjectContext.libraryProject->aotMode` 映射到 `SZrAotWriterOptions.requireFullAot`；
  `aotMode: "full-aot"` 写入 `ZR_TRUE`，缺省/`hybrid` 写入 `ZR_FALSE`，并保持
  `requireExecutableLowering`、`enableCodeStripping` 等其他 writer options 不变。
  RED/GREEN：RED 为 CLI project incremental 测试新增 full-AOT/hybrid writer option 用例后链接失败，
  缺少 `ZrCli_Compiler_ApplyProjectAotWriterOptions`；GREEN 后两个新用例通过，CLI project incremental
  测试为 10/0。
  验证：WSL gcc `zr_vm_cli_project_incremental_test` 10/0；WSL clang 同目标 10/0；
  Windows MSVC Debug `zr_vm_cli_project_incremental_test` 10/0；Windows MSVC CLI smoke
  `hello_world` 输出 `hello world`。
  产出：`tests/acceptance/2026-06-25-aot-11-s7g-zrp-project-manifest-aot-mode-writer-injection.md`。
  备注：这是 project manifest policy 到 AOT writer option 的注入入口；当前 CLI 仍没有 AOT C 发射模式，
  不声明 full-AOT 闭合诊断、manifest 动态泛型实例或 preserve writer roots 完成。

- 2026-06-25 00:08:34 +08:00 · 11-S7F zrp project manifest AOT mode parsing ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，preserve symbol/token 绑定、导出 token、
  AOT mode 到 writer/CLI 的自动注入、默认最小 metadata 策略、zrp metadata dump/diff 工具仍待后续。
  完成项目：`.zrp` project loader 现在接受 top-level `aotMode` string，缺省为
  `ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID`；`"hybrid"` 显式保持 hybrid，`"full-aot"` 写入
  `ZR_LIBRARY_PROJECT_AOT_MODE_FULL_AOT`。非法类型或未知值会拒绝 manifest。解析结果暴露在
  `SZrLibrary_Project.aotMode`。`zrp.schema.json` 同步声明 `aotMode` enum。实现放入
  `project_aot_options.{h,c}`，避免继续扩大已超过 1000 行的 `project.c`。
  RED/GREEN：RED 为 manifest normalization 测试新增 AOT mode 用例后编译失败：project model 缺少
  `aotMode` 字段与 AOT mode enum。GREEN 后缺省 hybrid、显式 full-AOT 和非法 mode 拒绝均通过，
  manifest normalization 测试提升到 14/0。
  验证：WSL gcc `zr_vm_project_manifest_normalization_test` 14/0 与 `zr_vm_project_import_resolver_test` 9/0；
  WSL clang 同两目标分别 14/0、9/0；`python -m json.tool zrp.schema.json` 通过；
  Windows MSVC `zr_vm_project_manifest_normalization_test` 14/0、`zr_vm_project_import_resolver_test` 9/0，
  CLI smoke `hello_world` 输出 `hello world`。
  产出：`tests/acceptance/2026-06-25-aot-11-s7f-zrp-project-manifest-aot-mode.md`。
  备注：这是 declaration-level manifest AOT mode parser；该 11-S7F 切片本身不声明 CLI/project compiler
  已把 `full-aot` 自动映射到 `SZrAotWriterOptions.requireFullAot`，也不声明完整 full-AOT 闭合诊断或动态泛型实例保留完成。

- 2026-06-24 23:36:19 +08:00 · 11-S7E zrp project manifest preserve rule parsing ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，preserve symbol/token 绑定、导出 token、
  AOT mode、默认最小 metadata 策略、zrp metadata dump/diff 工具仍待后续。
  完成项目：`.zrp` project loader 现在接受 top-level `preserve` array，每条规则要求 object 形态和
  string `kind`/`target`；`kind` 支持 `type`、`method`、`generic`，`members` 可选并支持
  `all`/`methods`，缺省为 default。解析结果写入 `SZrLibrary_ProjectPreserveRule` 列表并暴露
  `preserveRuleCount`；非法 target（空值、空白、路径分隔、`@`/`$`、非法点段）会拒绝 manifest。
  `zrp.schema.json` 同步声明 `preserve` schema。实现放入 `project_preserve.{h,c}`，避免继续扩大
  已超过 1000 行的 `project.c`。
  RED/GREEN：RED 为 manifest normalization 测试新增 preserve 用例后编译失败：project model 缺少
  `preserveRuleCount`、`preserveRules` 与 preserve enum。GREEN 后合法 preserve 两条规则解析为 type/all
  与 method/default，非法 target 被拒绝，manifest normalization 测试提升到 12/0。
  验证：WSL gcc `zr_vm_project_manifest_normalization_test` 12/0 与 `zr_vm_project_import_resolver_test` 9/0；
  WSL clang 同两目标分别 12/0、9/0；`python -m json.tool zrp.schema.json` 通过；
  Windows MSVC `zr_vm_project_manifest_normalization_test` 12/0、`zr_vm_project_import_resolver_test` 9/0，
  CLI smoke `hello_world` 输出 `hello world`。
  产出：`tests/acceptance/2026-06-24-aot-11-s7e-zrp-project-manifest-preserve-rule-parsing.md`。
  备注：这是 declaration-level manifest preserve parser，不声明将 target 解析为 metadata token、
  function flat index、generic instantiation root 或 code-stripping writer option 已完成；feature switch 也仍开放。

- 2026-06-24 22:56:42 +08:00 · 11-S7D zrp project manifest legacy declared assembly mapping ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，manifest preserve 规则解析、导出 token、
  AOT mode、默认最小 metadata 策略、zrp metadata dump/diff 工具仍待后续。
  完成项目：旧 `dependencies.$alias` object 现在接受 `assembly` 或 legacy `name` 声明目标 assembly
  identity，并使用与 `assembly.name` 一致的 assembly-name shape gate；`assembly`/`name` 同时存在但
  不一致会拒绝 manifest。声明 assembly 与目标 `.zrp` manifest 实际 identity 不一致时拒绝加载；
  声明 assembly 含点段时，dependency package 仍使用 `$alias` 的 alias key 生成 import module key，
  真实 assembly identity 写入 package/ref 元数据，并通过 `ZrLibrary_Project_GetDependencyImportVersionRange()`
  暴露给后续 AssemblyRef 元数据路径。`zrp.schema.json` 同步为旧 `dependencies.$alias` object 增加
  `assembly` 与 `name` 字段。
  RED/GREEN：RED 为 manifest normalization 测试扩展 legacy declared assembly 用例后 10 个用例中
  2 个失败：`assembly: "zr.math"` 被拒绝，`name: "math"` 与目标 `physics` 不一致却被接受；随后
  RED refinement 发现版本范围查询未返回真实 assembly identity。GREEN 后 declared assembly accepted、
  mismatch rejected、AssemblyRef identity query 均通过，测试保持 10/0。
  验证：WSL gcc `zr_vm_project_manifest_normalization_test` 10/0 与 `zr_vm_project_import_resolver_test` 9/0；
  WSL clang 同两目标分别 10/0、9/0；`python3 -m json.tool zrp.schema.json` 通过；Windows MSVC CLI smoke
  `hello_world` 输出 `hello world`。
  备注：MSVC 和 clang/gcc 仍报告 project loader/resolver 既有 const qualifier warning；`project.c` 与
  `project_import_resolver.c` 已超过 1000 行，本窄切片未强行拆分，后续应在 manifest surface 稳定后独立拆分。
  产出：`tests/acceptance/2026-06-24-aot-11-s7d-zrp-project-manifest-legacy-declared-assembly-mapping.md`。
  备注：这是 §8 `.zrp` manifest compatibility mapping 的 legacy dependency declared assembly gate，不声明
  preserve 规则 DSL、按 symbol/token 保留、AOT mode、runtime binding 诊断、默认最小 metadata 策略或 dump/diff 工具完成。

- 2026-06-24 22:39:26 +08:00 · 11-S7C zrp project manifest legacy identity/schema parity ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，manifest preserve 规则解析、导出 token、
  AOT mode、默认最小 metadata 策略、zrp metadata dump/diff 工具仍待后续。
  完成项目：`.zrp` project loader 现在把 compatibility mapping 中的 top-level `name` 当作 assembly identity
  做同一套 assembly-name shape 校验；存在但不是 string、为空或含空白/路径/`@`/`$`/非法点段的旧 `name`
  会拒绝 manifest。top-level `version` 存在但不是 string/null 时也会拒绝；缺省 assembly version 仍规范化为
  `0.0.0`，`culture` 默认为 `neutral`，`kind` 默认为 `library`，`publicKeyToken: null` 仍按无 token 处理。
  `zrp.schema.json` 同步收紧 `manifestVersion` 只允许 1、legacy `name` pattern、`publicKeyToken` hex/null 与
  `kind` enum。
  RED/GREEN：RED 为 manifest normalization 测试扩展 legacy identity 用例后 7 个用例中 1 个失败：旧
  top-level `name: "app render"` 被接受；GREEN 后 invalid legacy name 被拒绝，identity defaults 保持通过，
  测试扩展为 8/0。
  验证：WSL gcc `zr_vm_project_manifest_normalization_test` 8/0 与 `zr_vm_project_import_resolver_test` 9/0；
  WSL clang 同两目标分别 8/0、9/0；`python3 -m json.tool zrp.schema.json` 通过；Windows MSVC CLI smoke
  `hello_world` 输出 `hello world`。
  备注：MSVC 和 clang/gcc 仍报告 `project.c` 既有 const qualifier warning，本切片未扩大处理范围。
  产出：`tests/acceptance/2026-06-24-aot-11-s7c-zrp-project-manifest-legacy-identity-schema-parity.md`。
  备注：这是 §8 `.zrp` manifest Layer 1 identity/schema parity gate，不声明 preserve 规则 DSL、按 symbol/token
  保留、AOT mode、runtime binding 诊断、默认最小 metadata 策略或 dump/diff 工具完成。

- 2026-06-24 22:26:53 +08:00 · 11-S7B zrp project manifest publicKeyToken normalization ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，manifest preserve 规则解析、导出 token、
  AOT mode、默认最小 metadata 策略、zrp metadata dump/diff 工具仍待后续。
  完成项目：`.zrp` project loader 现在把 `assembly.publicKeyToken` 作为 manifest identity text 做十六进制校验；
  大写 `A-F` 会在解析期原地归一化为小写，非法字符会拒绝 manifest，`null` 仍按无 token 处理。
  RED/GREEN：RED 为 manifest normalization 测试新增 publicKeyToken 用例后 2 个失败：大写 token 未归一化、
  非 hex token 被接受；GREEN 后 publicKeyToken 小写化与非法 token 拒绝均通过。
  验证：WSL gcc `zr_vm_project_manifest_normalization_test` 5/0 与 `zr_vm_project_import_resolver_test` 9/0；
  WSL clang 同两目标分别 5/0、9/0；Windows MSVC CLI smoke `hello_world` 输出 `hello world`。
  备注：MSVC 和 clang/gcc 仍报告 `project.c` 既有 const qualifier warning，本切片未扩大处理范围。
  产出：`tests/acceptance/2026-06-24-aot-11-s7b-zrp-project-manifest-public-key-token-normalization.md`。
  备注：这是 §8 `.zrp` project manifest identity normalization gate，不声明 strong-name 验证、签名哈希验证、
  runtime binding 诊断、preserve 规则解析、默认最小 metadata 策略或 dump/diff 工具完成。

- 2026-06-24 22:19:43 +08:00 · 11-S7A zrp project manifest normalization gates ·
  状态：11-S7 子切片完成；完整 11-S7 仍未关闭，manifest preserve 规则解析、导出 token、
  AOT mode、默认最小 metadata 策略、zrp metadata dump/diff 工具仍待后续。
  完成项目：`.zrp` project loader 现在显式校验 `manifestVersion`，只接受缺省或 `1`；
  旧 `dependencies.$alias` 与新 `references.alias` 归一化到同一 package / assembly / version range 时只保留一条
  dependency ref，并保留新 reference 所需的 alias-for-module-key 语义；同 alias 但 package 或 version range 不同仍拒绝。
  RED/GREEN：RED 为新增 manifest normalization 测试后 3 个用例中 2 个失败：同值 old/new reference 被拒绝、
  unsupported `manifestVersion: 2` 被接受；GREEN 后 identical old/new reference 只产生一条 ref，conflicting old/new
  reference 被拒绝，`manifestVersion: 2` 被拒绝。
  验证：WSL gcc `zr_vm_project_manifest_normalization_test` 3/0 与 `zr_vm_project_import_resolver_test` 9/0；
  WSL clang 同两目标分别 3/0、9/0；Windows MSVC CLI smoke `hello_world` 输出 `hello world`。
  产出：`tests/acceptance/2026-06-24-aot-11-s7a-zrp-project-manifest-normalization-gates.md`。
  备注：这是 §8 `.zrp` project manifest 的 loader normalization gate，不声明 preserve 规则 DSL、按 symbol/token
  保留、AOT 模式、跨模块 runtime binding 诊断、数据元数据文件 dump/diff 或 `12-S4` 后端 preserve root 解析完成。

- 2026-06-24 21:56:34 +08:00 · 11-S1J zrp signature blob structural validator ·
  状态：11-S1 子切片完成；完整 11-S1 仍未关闭，编译期真实定义表/string/signature pool 导出、
  signature blob semantic/type/token resolution、zrp manifest 文件读写与 dump/diff 工具仍待后续。
  完成项目：新增 `ZrCore_ZrpMetadata_ValidateSignatureBlob()`，对独立 signature blob 做边界安全的结构校验：
  method signature、field signature 与常用 type node 会被递归解析，param count / type arg count / tuple count
  必须与 payload 边界一致；截断 payload、未知 node、非法 root、嵌套 method/field signature 与尾随字节均会失败。
  RED/GREEN：RED 为 format 测试要求 signature blob validator 后链接失败；GREEN 后合法 method/field/generic-inst
  blob 通过，空 blob、null blob、尾随字节、截断 method signature 与未知 node 均被拒绝。
  验证：WSL gcc `zr_vm_zrp_metadata_format_test` 11/0；WSL gcc CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；WSL clang
  `zr_vm_zrp_metadata_format_test` 11/0；WSL clang CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；Windows MSVC CLI smoke
  `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s1j-zrp-signature-blob-structural-validator.md`。
  备注：这是 §1 signature blob 的结构层校验，不声明 token/type/string 语义解析、compiler 真实 signature pool
  导出、文件级 zrp manifest 或 dump/diff 工具完成。

- 2026-06-24 21:46:03 +08:00 · 11-S1I zrp string pool view decoder ·
  状态：11-S1 子切片完成；完整 11-S1 仍未关闭，编译期真实定义表导出、签名 blob
  标准化解析、zrp manifest 文件读写与 dump/diff 工具仍待后续。
  完成项目：新增 `SZrZrpMetadataStringView` 与 `ZrCore_ZrpMetadata_GetString()`，调用方可从已验证
  zrp metadata buffer 的 string pool 中按 offset 取回 NUL-terminated 字符串 view；返回的 `byteLength`
  不包含终止 NUL；offset 越界、缺少终止 NUL、空输出指针均失败并清空输出。
  RED/GREEN：RED 为 format 测试要求 string view 类型/API 后编译失败；GREEN 后可解析 `Zr`、`VM`
  与空字符串，offset 等于 pool 长度被拒绝，缺少终止 NUL 的 entry 被拒绝。
  验证：WSL gcc `zr_vm_zrp_metadata_format_test` 10/0；WSL gcc CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；WSL clang
  `zr_vm_zrp_metadata_format_test` 10/0；WSL clang CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；Windows MSVC CLI smoke
  `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s1i-zrp-string-pool-view-decoder.md`。
  备注：这是 §1 string pool 的只读 view 解码入口，不声明 UTF-8 语义校验、字符串 intern、
  compiler 真实 string pool 导出、签名 blob 解析、文件级 zrp manifest 或 dump/diff 工具完成。

- 2026-06-24 21:33:41 +08:00 · 11-S1H zrp definition table payload writer ·
  状态：11-S1 子切片完成；完整 11-S1 仍未关闭，编译期真实定义表导出、字符串解码、签名 blob
  标准化解析、zrp manifest 文件读写与 dump/diff 工具仍待后续。
  完成项目：新增 `ZrCore_ZrpMetadata_WriteDefinitionTablePayload()`，调用方可在已验证 zrp metadata
  buffer 上把完整 TypeDef、MethodDef、FieldDef、GenericParam、GenericParamConstraint、TypeSpec、
  MethodSpec、ModuleRef row payload 写入对应 definition-table section；非 definition-table section、
  非空 row payload 空指针、row count / element size 与 section 目录不一致、截断 buffer 均会失败。
  RED/GREEN：RED 为 format 测试要求 definition-table payload writer 后链接失败；GREEN 后 TypeDef 与
  MethodDef payload 可写入并经 `GetSectionView()` 读回，`ValidateDefinitionTables()` 接受合法行，非表
  section、空 row payload、count 不一致、element size 不一致与截断 buffer 均被拒绝。
  验证：WSL gcc `zr_vm_zrp_metadata_format_test` 9/0；WSL gcc CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；WSL clang
  `zr_vm_zrp_metadata_format_test` 9/0；WSL clang CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；Windows MSVC CLI smoke
  `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s1h-zrp-definition-table-payload-writer.md`。
  备注：这是 §1 定义表 row payload 的完整写入入口，不声明 compiler 已从真实 symbol/type/function
  metadata 导出定义表内容，也不声明字符串解码、签名 blob 语义解析、文件级 zrp manifest 或 dump/diff
  工具完成。

- 2026-06-24 21:25:01 +08:00 · 11-S1G zrp pool payload writer ·
  状态：11-S1 子切片完成；完整 11-S1 仍未关闭，定义表内容导出、字符串解码、签名 blob
  标准化解析、zrp manifest 文件读写与 dump/diff 工具仍待后续。
  完成项目：新增 `ZrCore_ZrpMetadata_WritePoolPayload()`，调用方可在已验证 zrp metadata buffer
  上把完整 payload 写入 string pool、signature blob pool、constant pool 三类 byte pool；非 pool section、
  非空 payload 空指针、payload 长度与 section byteLength/count 不一致、截断 buffer 均会失败；0 长度空 pool
  可用 `ZR_NULL` payload 写入并保持 no-op。
  RED/GREEN：RED 为 format 测试要求 pool payload writer 后链接失败；GREEN 后 string/signature pool
  payload 可写入并经 `GetPoolSlice()` 读回，空 constant pool 写入合法，非 pool、空 payload、长度不一致与
  截断 buffer 均被拒绝。
  验证：WSL gcc `zr_vm_zrp_metadata_format_test` 8/0；WSL gcc CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；WSL clang
  `zr_vm_zrp_metadata_format_test` 8/0；WSL clang CTest
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3；Windows MSVC CLI smoke
  `hello_world` 输出 `hello world`。产出：
  `tests/acceptance/2026-06-24-aot-11-s1g-zrp-pool-payload-writer.md`。
  备注：这是 §1 三类池的完整 byte payload 写入入口，不声明字符串解码、签名 blob 语义解析、
  定义表导出、文件级 zrp manifest 或 dump/diff 工具完成。

- 2026-06-24 17:50:57 +08:00 · 11-S1F zrp pool slice view API ·
  状态：11-S1 子切片完成；完整 11-S1 仍未关闭，真实池内容写入、字符串解码、签名 blob
  标准化解析、zrp manifest 文件读写与 dump/diff 工具仍待后续。
  完成项目：新增 `SZrZrpMetadataPoolSliceView` 与
  `ZrCore_ZrpMetadata_GetPoolSlice()`，调用方可在已验证 zrp metadata buffer 上从 string pool、
  signature blob pool、constant pool 三类 byte pool 中按 offset/length 获取只读 slice；合法 0 长度
  边界 slice 可返回池尾指针；非池 section、越界 slice 会失败并清空输出。
  RED/GREEN：RED 为 format 测试要求 pool slice API 后编译失败；GREEN 后 string/signature/constant
  三个 pool slice 均解析到预期 payload，池尾 0 长度 slice 合法，非池 section 与越界 slice 被拒绝。
  验证：`zr_vm_zrp_metadata_format_test` 7/0。产出：
  `tests/acceptance/2026-06-24-aot-11-s1f-zrp-pool-slice-view.md`。
  备注：这是 §1 三类池的边界安全读取入口，不声明池内容生成或签名/字符串语义解析完成。

- 2026-06-24 17:46:08 +08:00 · 11-S1E zrp definition-table RID/range validation ·
  状态：11-S1 子切片完成；完整 11-S1 仍未关闭，真实定义表/池内容导出、跨模块 import
  解析、zrp manifest 文件读写与 dump/diff 工具仍待后续。
  完成项目：`ZrCore_ZrpMetadata_ValidateDefinitionTables()` 在 11-S1D token/tag 校验基础上补充
  基本 RID 与 range 校验：MethodDef/FieldDef owner TYPE_DEF RID 必须落在 TypeDef 表范围内；
  GenericParam owner RID 必须落在 TypeDef 或成员定义范围内；GenericParamConstraint 的
  `genericParamIndex` 必须存在；TypeDef 的 method/field/generic-param 子表 range 必须落在对应 section
  count 内。RED/GREEN：RED 为新增 cross-table range 测试中越界 owner/range/constraint 仍被旧实现接受，
  运行失败；GREEN 后合法 payload 通过，错误 owner RID、越界 method range、越界 generic-param
  constraint 均被拒绝。
  验证：`zr_vm_zrp_metadata_format_test` 6/0。产出：
  `tests/acceptance/2026-06-24-aot-11-s1e-zrp-definition-table-range-validation.md`。
  备注：这是格式层 RID/range 护栏，不声明完整 resolver 或跨模块 symbol/token 解析完成。

- 2026-06-24 17:40:49 +08:00 · 11-S1D zrp definition-table token validation ·
  状态：11-S1 子切片完成；完整 11-S1 仍未关闭，真实定义表/池内容导出、跨表 RID 范围校验、
  zrp manifest 文件读写与 dump/diff 工具仍待后续。
  完成项目：新增 `ZrCore_ZrpMetadata_ValidateDefinitionTables()`，在已验证 section view 上检查
  TypeDef、MethodDef、FieldDef、GenericParam、GenericParamConstraint、TypeSpec、MethodSpec、ModuleRef
  行的 token/table tag 基本一致性：TypeDef 行必须携带 TYPE_DEF token；MethodDef/FieldDef 行必须携带
  MEMBER_DEF token 且 owner 为 TYPE_DEF；generic param owner 必须是 TYPE_DEF 或 MEMBER_DEF；
  constraint type 必须是 TYPE_DEF/TYPE_REF/TYPE_SPEC；TypeSpec 行必须是 TYPE_SPEC token；MethodSpec
  行使用 SIGNATURE token 且 methodToken 指向 member def/ref；ModuleRef 行必须是 ASSEMBLY_REF token。
  RED/GREEN：RED 为 format 测试要求定义表 token 校验 API 后链接失败；GREEN 后合法定义表 payload
  校验通过，错误 TypeDef token、错误 MethodDef owner、错误 MethodSpec method token 均被拒绝。
  验证：`zr_vm_zrp_metadata_format_test` 5/0。产出：
  `tests/acceptance/2026-06-24-aot-11-s1d-zrp-definition-table-token-validation.md`。
  备注：这是定义表行语义的最小格式护栏，不声明 resolver 已完成 token→实体 lazy 解析，也不做跨表 RID
  存在性校验。

- 2026-06-24 17:34:08 +08:00 · 11-S1C zrp mmap section view API ·
  状态：11-S1 子切片完成；完整 11-S1 仍未关闭，真实定义表/池内容导出、zrp manifest
  文件读写与 dump/diff 工具仍待后续。
  完成项目：新增 `SZrZrpMetadataSectionView` 与
  `ZrCore_ZrpMetadata_GetSectionView()`，调用方可从已验证的只读 zrp metadata buffer 中按
  `EZrZrpMetadataSectionKind` 取回 section 目录、payload 指针、字节长度、count 与 element size；
  空 section 返回合法空 view，截断 buffer 或未知 section kind 会失败且清空输出 view。实现复用
  `ValidateHeader()`，避免跳过 magic/version/header/section 边界检查直接暴露 mmap payload 指针。
  RED/GREEN：RED 为 format 测试要求 `SZrZrpMetadataSectionView` 与
  `ZrCore_ZrpMetadata_GetSectionView()` 后编译失败；GREEN 后 TypeDef 与 string pool payload 可解析，
  空 constant pool 返回空 view，截断 buffer 和非法 section kind 被拒绝。
  验证：`zr_vm_zrp_metadata_format_test` 4/0。产出：
  `tests/acceptance/2026-06-24-aot-11-s1c-zrp-section-view.md`。
  备注：这是 §1 的 mmap 只读访问入口，不声明高层 token/type/method/field 解析缓存完成。

- 2026-06-24 17:28:29 +08:00 · 11-S1B zrp definition table directory ABI ·
  状态：11-S1 子切片完成；完整 11-S1 仍未关闭，真实 TypeDef/MethodDef/FieldDef/GenericParam
  行导出、字符串池/签名 blob 池/常量池内容物化、文件级 zrp manifest 读写仍待后续。
  完成项目：`ZR_ZRP_METADATA_VERSION` 升到 2，`ZR_ZRP_METADATA_HEADER_SIZE` 扩展到 208，
  `ZR_ZRP_METADATA_SECTION_COUNT` 扩展到 12；header 目录现在固定包含 token records、
  TypeDef、MethodDef、FieldDef、GenericParam、GenericParamConstraint、TypeSpec、MethodSpec、
  ModuleRef、string pool、signature blob pool、constant pool。新增
  `SZrZrpMetadataTypeDefRow`、`MethodDefRow`、`FieldDefRow`、`GenericParamRow`、
  `GenericParamConstraintRow`、`TypeSpecRow`、`MethodSpecRow`、`ModuleRefRow`，并在
  `ValidateHeader()` 中按 section kind 校验 counted section 的 element size，防止 mmap 视图把错误表宽
  当成合法定义表。
  RED/GREEN：RED 为 `zr_vm_zrp_metadata_format_test` 要求 12 个 section、定义表 section kind、
  row 类型和 header 字段后编译失败；GREEN 后完整 12-section header round-trip、定义表目录回读、
  错误 definition-table element-size / byte-length 拒绝均通过。
  验证：`zr_vm_zrp_metadata_format_test` 3/0。产出：
  `tests/acceptance/2026-06-24-aot-11-s1b-zrp-definition-table-directory.md`。
  备注：这是 §1 中定义表目录与行 ABI 的落地，不声明编译器已经导出真实表内容，也不声明
  zrp manifest/dump/diff 工具完成。

- 2026-06-24 14:53:23 +08:00 · 11-S1A zrp metadata header/section format ·
  状态：11-S1 子切片完成；完整 11-S1 仍未关闭，TypeDef/MethodDef/FieldDef/GenericParam 等定义表、
  字符串池/签名 blob 池的实际导出、文件级 zrp manifest 读写仍待后续。
  完成项目：新增 `zr_vm_core/zrp_metadata.h` 与 `zrp_metadata.c`，定义
  `SZrZrpMetadataHeader`、`SZrZrpMetadataSection`、`ZR_ZRP_METADATA_MAGIC`、
  `ZR_ZRP_METADATA_VERSION`、固定 80 字节 header 和四个 section 目录
  （token records、string pool、signature blob pool、constant pool）；提供
  `ZrCore_ZrpMetadata_InitHeader()`、`ValidateHeader()`、`WriteHeader()`、`ReadHeader()`，
  以 little-endian 字段写入/读取，支持对 mmap 只读 buffer 做 magic/version/header/section 边界校验。
  RED/GREEN：RED 先为新增 CTest 目标未配置，重新生成后红在缺少 `zr_vm_core/zrp_metadata.h`；
  GREEN 后 header 能 round-trip token table 与池目录，坏 magic、future version、错误 header size、
  section 越过 header/缺 element size 均被拒绝。
  验证：`zr_vm_zrp_metadata_format_test` 2/0、`zr_vm_metadata_runtime_query_test` 3/0、
  `zr_vm_metadata_token_model_test` 21/0；CTest 过滤
  `zrp_metadata_format|metadata_runtime_query|metadata_token_model` 3/3。产出：
  `tests/acceptance/2026-06-24-aot-11-s1a-zrp-metadata-header.md`。
  备注：这只是 zrp 数据元数据段的稳定头部/目录载体；完整表项编码、跨模块 zrp 文件读写、
  代码注册表和 token→layout 解析分别留给 11-S1 后续、11-S2、11-S3/11-S4。
