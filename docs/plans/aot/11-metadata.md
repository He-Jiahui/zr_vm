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
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h   # 11-S4BO signature type-node -> attached type record resolver API
  - zr_vm_core/src/zr_vm_core/metadata_runtime_type_node_binding.c # 11-S4BO direct/nested local signature-node record binding
  - zr_vm_core/src/zr_vm_core/reflection_generic_type_object.c # 11-S4BO/10-S4Z32 metadata node binding consumer
  - tests/module/test_reflection_dynamic_generic_instance.c # 11-S4BO/10-S4Z32 nested/compound token-only consumer coverage
  - tests/module/test_reflection_token_resolve.c       # 11-S4BN/10-S4Z28 nested primitive POD storage-width path matrix coverage
  - tests/module/test_reflection_token_resolve.c       # 11-S4BM/10-S4Z27 nested primitive POD representative path matrix coverage
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.c # 11-S4BL/10-S4Z26 nested primitive raw child leaf layout identity guard consumer
  - tests/module/test_reflection_token_resolve.c       # 11-S4BL/10-S4Z26 nested primitive raw child leaf layout guard coverage
  - zr_vm_core/include/zr_vm_core/reflection.h         # 11-S4BK/10-S4Z25 FieldInfo nested inline primitive POD path read/write API consumer
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 11-S4BK/10-S4Z25 nested inline primitive POD path object/token adapter
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.c # 11-S4BK/10-S4Z25 recursive nested inline primitive path consumer
  - zr_vm_core/src/zr_vm_core/reflection_field_value_primitive.c # 11-S4BK/10-S4Z25 shared primitive POD raw load/store guard
  - zr_vm_core/src/zr_vm_core/reflection_field_value_primitive.h # 11-S4BK/10-S4Z25 shared primitive POD raw load/store guard API
  - tests/module/test_reflection_token_resolve.c       # 11-S4BK/10-S4Z25 nested inline primitive POD path consumer coverage
  - zr_vm_core/include/zr_vm_core/reflection.h         # 11-S4BJ/10-S4Z24 FieldInfo nested inline VALUE_SLOT path write API consumer
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 11-S4BJ/10-S4Z24 nested inline VALUE_SLOT path write adapter
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.c # 11-S4BJ/10-S4Z24 recursive nested inline layout path write consumer
  - tests/module/test_reflection_token_resolve.c       # 11-S4BJ/10-S4Z24 nested inline VALUE_SLOT path write consumer coverage
  - zr_vm_core/include/zr_vm_core/reflection.h         # 11-S4BI/10-S4Z23 FieldInfo nested inline VALUE_SLOT path read API consumer
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 11-S4BI/10-S4Z23 nested inline VALUE_SLOT path adapter
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.c # 11-S4BI/10-S4Z23 recursive nested inline layout path consumer
  - tests/module/test_reflection_token_resolve.c       # 11-S4BI/10-S4Z23 nested inline VALUE_SLOT path read consumer coverage
  - zr_vm_core/include/zr_vm_core/reflection.h         # 11-S4BH/10-S4Z22 FieldInfo nested inline VALUE_SLOT write API consumer
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 11-S4BH/10-S4Z22 nested inline VALUE_SLOT layout-field write consumer
  - tests/module/test_reflection_token_resolve.c       # 11-S4BH/10-S4Z22 nested inline VALUE_SLOT write consumer coverage
  - zr_vm_core/include/zr_vm_core/reflection.h         # 11-S4BG/10-S4Z21 FieldInfo nested inline VALUE_SLOT read API consumer
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 11-S4BG/10-S4Z21 nested inline VALUE_SLOT layout-field consumer
  - tests/module/test_reflection_token_resolve.c       # 11-S4BG/10-S4Z21 nested inline VALUE_SLOT consumer coverage
  - zr_vm_core/src/zr_vm_core/type_layout.c            # 11-S4BF/10-S4Z20 nested VALUE_SLOT replacement/drop semantics consumed by inline aggregate write coverage
  - tests/module/test_reflection_token_resolve.c       # 11-S4BF/10-S4Z20 inline aggregate replacement/drop consumer coverage
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 11-S4BE/10-S4Z19 inline aggregate field-copy borrowed-source write consumer
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 11-S4BD/10-S4Z18 inline aggregate borrowed-source write consumer
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 11-S4BC/10-S4Z17 inline struct signature/layout consumer for FieldInfo borrowed view
  - zr_vm_core/include/zr_vm_core/reflection.h         # 11-S4BB/10-S4Z16 FieldInfo object primitive POD coverage consumer
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 11-S4BB/10-S4Z16 object-level FieldInfo primitive POD consumer path
  - zr_vm_core/include/zr_vm_core/reflection.h         # 11-S4BA/10-S4Z15 FieldInfo object value write API consumer
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 11-S4BA/10-S4Z15 object-level FieldInfo write consumer for attached runtime/token carriers
  - zr_vm_core/include/zr_vm_core/reflection.h         # 11-S4AZ/10-S4Z14 FieldInfo object value read API consumer
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 11-S4AZ/10-S4Z14 object-level FieldInfo read consumer for attached runtime/token carriers
  - zr_vm_core/include/zr_vm_core/metadata_token.h     # 8 表 token 体系 + 签名节点 + TokenBinding
  - zr_vm_core/src/zr_vm_core/function_metadata_query.c # low-level token record query reused by 11-S3A
  - zr_vm_core/include/zr_vm_core/zrp_metadata.h       # 11-S1A..11-S1J zrp metadata header + definition table directory + mmap section/pool/string view + row/range/signature-blob validation + pool/table payload writer ABI; 11-S4J TypeSpec row typeLayoutId binding
  - zr_vm_core/src/zr_vm_core/zrp_metadata.c           # 11-S1A..11-S1J header read/write/validate + section/pool/string view + definition-table/range validation + pool/table payload writer + signature blob structural validator; 11-S7ZSF/12-S7ZZW unbound manifest export row validation
  - zr_vm_core/include/zr_vm_core/function.h           # SZrFunctionMetadata / ModuleEffect; 11-S2E flat function-graph resolver API; 11-S4G function-level code-registration layout registry binding for GC inline-frame consumers
  - zr_vm_core/src/zr_vm_core/function_graph.c         # 11-S2E AOT-order root/constant/child flat function graph resolver
  - zr_vm_core/include/zr_vm_core/type_layout.h        # cTypeId / SZrTypeLayoutMetadata
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h   # 11-S2B method token count mirror; 11-S2C minimal metadata runtime registration carrier; 11-S2D method token -> MethodInfo/function pointer/invoker binding view; 11-S3A..11-S3M method/field/type/signature/TypeSpec record cache API + zrp metadata mmap attach/query + validated signature blob/header/type-node/generic TypeSpec signature/base-token/argument binding view + MethodSpec signature view; 11-S4A/11-S4C TypeDef token/cTypeId/layout binding view backed by code-registration layout registry; 11-S4D public typeLayoutId -> SZrTypeLayout resolver; 11-S4E generic dictionary consumer input; 11-S4F public typeLayoutId -> SZrAotGcDescriptor resolver; 11-S4G function-level layout resolver for GC inline-frame consumers; 11-S4H function+prototype -> registry-backed type layout resolver for reflection consumers; 11-S4I FieldDef token/row/offset/layout binding view; 11-S4J TypeSpec token/generic-binding/layout binding view; 11-S4K TypeDef/TypeSpec token -> layout cache resolver; 11-S4S same resolver accepts attached bound TypeRef token -> TypeDef layout; 11-S4L typeLayoutId -> TypeDef/TypeSpec token reverse resolver; 11-S4M bounded multi-entry type-layout cache; 11-S4N cTypeId -> token resolver; 11-S4O code-registration type-layout token count mirror; 11-S5 GenericParam/GenericParamConstraint runtime view API, MethodSpec signature record carrier, and indexed MethodSpec generic argument view API; 11-S5A exact GenericParam owner-range view; 11-S6A token binding compatibility status/report API; 11-S6B function-level binding scan API; 11-S7/12-S7ZZQ member-token remap count mirror; 11-S7ZF/12-S7 manifest export table runtime mirror
  - zr_vm_core/src/zr_vm_core/metadata_runtime.c       # 11-S3A..11-S3M ResolveMethodRecord/ResolveFieldRecord/ResolveTypeRecord/ResolveSignatureRecord lazy token record lookup/cache, local TypeSpec records, zrp section-view attach/query, signature blob validation, method/field signature header view parsing, nested type-node view parsing, generic TypeSpec signature view parsing, generic base-token binding, indexed generic argument binding, MethodSpec signature view parsing with MethodSpec record carrier, and indexed MethodSpec generic argument binding; 11-S4D public type-layout resolver; 11-S4E/11-S4F generic dictionary and GC descriptor consumers reuse the same resolver; 11-S4G/11-S4H function/prototype-context layout resolver for attached AOT code-registration functions
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h   # 11-S7ZG/12-S7 manifest export runtime view API
  - zr_vm_core/src/zr_vm_core/metadata_runtime_manifest_exports.c # 11-S7ZG/12-S7 manifest export runtime view implementation
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c # 11-S2D method token code-registration binding; 11-S2E MethodDef.functionIndex interpreter VM binding
  - zr_vm_core/src/zr_vm_core/metadata_runtime_generic_params.c # 11-S5 GenericParam/GenericParamConstraint views; 11-S5A public exact owner-range view
  - zr_vm_core/src/zr_vm_core/metadata_runtime_layout_binding.c # 11-S4A..11-S4O split layout-binding implementation for TypeDef/TypeSpec/FieldDef row lookup, registry-backed binding views, TypeDef/TypeSpec token -> layout cache resolver, 11-S4S attached bound TypeRef token -> TypeDef layout resolver, typeLayoutId/cTypeId -> token reverse lookup, bounded multi-entry cache, and code-registration token-table consumption
  - zr_vm_core/src/zr_vm_core/metadata_runtime_binding_compatibility.c # 11-S6A runtime ABI drift predicate for module version range + token/signature/module/layout compatibility; 11-S6B function binding scan over attached module metadata bindings; 11-S6J canonical TypeSpec RID mapping compatibility; 11-S7ZH manifest export binding gate predicate
  - zr_vm_core/include/zr_vm_core/module.h             # 11-S2C module metadata runtime attach/query surface; 11-S4D typeLayoutCount runtime mirror
  - zr_vm_core/src/zr_vm_core/module/module.c          # 11-S2B methodTokenCount attach; 11-S2C module metadata runtime attach/query implementation; 11-S4D codeRegistration typeLayoutCount attach; 11-S4G function attach during module metadata runtime attach; 11-S4O typeLayoutTokenCount attach; 11-S7/12-S7ZZQ codeRegistration member-token remap writeback to typed exports; 11-S7ZF/12-S7 codeRegistration manifest export table attach mirror
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c             # 11-S2C module metadataRuntime.metadataFunction mark; 11-S4G mark inline-frame resolver prefers metadata runtime for attached AOT functions
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c            # 11-S2C module metadataRuntime.metadataFunction rewrite; 11-S4G rewrite inline-frame resolver prefers metadata runtime for attached AOT functions
  - zr_vm_core/src/zr_vm_core/reflection.c             # 10-S4A / 11-S4H reflection type/member layout consumers read registry-backed SZrTypeLayout when an AOT registry is attached; 10-S4F/11-S4T FieldDef token FieldInfo object consumer reads FieldDef/TypeDef string-pool names and layout binding; 10-S4G/11-S4U FieldInfo declaring type name/object link consumer; 10-S4H/11-S4V FieldInfo owner object link consumer; 10-S4I/11-S4W FieldInfo moduleName consumer; 10-S4J/11-S4X FieldInfo FieldDef row flags consumer; 10-S4K/11-S4Y FieldInfo FieldDef row signature blob coordinate consumer; 10-S4L/11-S4Z FieldInfo validated field signature header consumer; 10-S4M/11-S4AA FieldInfo field signature type-node summary consumer; 10-S4N/11-S4AB FieldInfo primitive signature type consumer; 10-S4O/11-S4AC FieldInfo primitive signature type object consumer; 10-S4P/11-S4AD FieldInfo module reflection object consumer; 10-S4Q/11-S4AE FieldInfo direct TypeDef signature token/layout consumer; 10-S4R/11-S4AF FieldInfo direct TypeDef signature type object consumer; 10-S4S/11-S4AG FieldInfo bound TypeRef signature token/layout/type object consumer; 10-S4T/11-S4AH FieldInfo signature/layout consistency consumer; 10-S4U/11-S4AI FieldInfo signature type-node object consumer; 10-S4V/11-S4AJ FieldInfo signature base type-node object consumer; 10-S4W/11-S4AK FieldInfo signature child type-node object list consumer; 10-S4X/11-S4AL FieldInfo primitive child type-node semantic name consumer; 10-S4Y/11-S4AM FieldInfo direct TypeDef child/base type-node semantic token/layout/name consumer; 10-S4Z2/11-S4AN FieldInfo direct TypeRef child type-node semantic token/layout/name consumer; 10-S4Z3/11-S4AO FieldInfo recursive signature type-node type literal consumer; 10-S4Z13/11-S4AY FieldInfo metadata runtime native-pointer consumer
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z4..10-S4Z12/11-S4AP..11-S4AX FieldInfo token inline value consumer; primitive POD path consumes FieldDef layout binding plus validated FIELD_SIG(PRIMITIVE) metadata, with representative/full storage-width matrix coverage plus integer, float32 range, float32 NaN, and float32 precision guards
  - zr_vm_core/src/zr_vm_core/reflection_token_resolve.c # 10-S3A/10-S2B/10-S2C/10-S2D/10-S2E/10-S2F/10-S2G/10-S2H/10-S2I/10-S2J/10-S2K/10-S2L/10-S3B/10-S3C/10-S3D/10-S3F/10-S3G/10-S3H/10-S3I/10-S3J/10-S3K/10-S3L/10-S3M/10-S3N/10-S3O/10-S3P/10-S4B/10-S4C/10-S4D/10-S4E public reflection carrier consumers for token, MethodSpec token, method signature identity, method binding, token-driven Method.Invoke dispatch, signature arity/shape/base-type/return/return-slot/void-return guards, generated int64/uint64/bool/f64 no-arg return-boxing buckets, TypeSpec generic, FieldDef owner/type, GenericParam/constraint, and MethodSpec generic argument metadata
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h     # 11-S2A SZrAotCodeRegistration ABI + module carrier; 11-S2B method token table ABI; 11-S4B code registration type-layout registry ABI; 11-S4F GC descriptor registry consumer source; 11-S4O type-layout token carrier ABI; 11-S7/12-S7ZZD member-token remap ABI; 11-S7ZE/12-S7 manifest export table ABI
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c # 12-S7Y default-min reflection metadata policy option helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_internal.h # 12-S7Y reflection metadata policy option API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h # 11-S6D..11-S6G i64/u64/f64/bool typed direct-call writer prototypes carry function-slot metadata for deopt fallback
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c # 11-S2A generated-C code registration emission; 11-S2B method token table emission/wiring; 10-S2I/10-S3M generated value.h include and function-table-fed reflection invoker emission callsite reused by 10-S2J/10-S3N, 10-S2K/10-S3O, 10-S2L/10-S3P, 10-S2M/10-S3Q, 10-S2N/10-S3R, 10-S2O/10-S3S, 10-S2P/10-S3T, 10-S2Q/10-S3U, 10-S2R/10-S3V, 10-S2S/10-S3W, 10-S2T/10-S3X, 10-S2U/10-S3Y, 10-S2V/10-S3Z, 10-S2W/10-S3AA, 10-S2X/10-S3AB, and 10-S2Y/10-S3AC; 11-S4B type-layout registry pointer/count emission; 11-S4O type-layout token table pointer/count emission; 07-S3/S4 generated metadata_runtime include for frame cleanup registry lookup; 12-S7K zrp metadata size attribution consumer; 12-S7Y metadata policy marker/plumbing; 12-S7Z emitted zrp metadata pruning plumbing; 11-S7/12-S7ZZD member-token remap ABI table emission; 11-S7ZE/12-S7 manifest export table emission; 11-S7ZSG/12-S7ZZX final compacted .zrp metadata sidecar publication orchestration
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_publication.h # 11-S7ZSG/12-S7ZZX final compacted .zrp metadata sidecar publication API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_publication.c # 11-S7ZSG/12-S7ZZX final metadata sidecar publication; 11-S7ZSX/12-S7ZZZQ header + definition-table validation and stale sidecar cleanup
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_invokers.h # 10-S2-maint split reflection invoker emitter API consumed by generated-C code-registration invoker table emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_invokers.c # 10-S2I..10-S2T/10-S3M..10-S3X generated reflection invoker buckets, 10-S2U/10-S3Y bool-return numeric dispatch orchestration, 10-S2V/10-S3Z i64 three-arg dispatch orchestration, 10-S2W/10-S3AA u64 three-arg dispatch orchestration, 10-S2X/10-S3AB f64 three-arg dispatch orchestration, and 10-S2Y/10-S3AC bool three-arg dispatch orchestration consuming 11-S2D MethodInfo/functionIndex binding
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_bool_numeric_invokers.h # 10-S2U/10-S3Y bool-return numeric two-arg reflection invoker emitter API consumed by generated-C code-registration invoker table emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_bool_numeric_invokers.c # 10-S2U/10-S3Y bool(int,int), bool(uint,uint), and bool(float,float) generated Method.Invoke buckets consuming 11-S2D MethodInfo/functionIndex binding
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_numeric_three_arg_invokers.h # 10-S2V/10-S3Z, 10-S2W/10-S3AA, and 10-S2X/10-S3AB numeric three-arg reflection invoker emitter API consumed by generated-C code-registration invoker table emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_numeric_three_arg_invokers.c # 10-S2V/10-S3Z int64(int64,int64,int64), 10-S2W/10-S3AA uint64(uint64,uint64,uint64), and 10-S2X/10-S3AB float(float,float,float) generated Method.Invoke buckets consuming 11-S2D MethodInfo/functionIndex binding
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_bool_three_arg_invokers.h # 10-S2Y/10-S3AC bool three-arg reflection invoker emitter API consumed by generated-C code-registration invoker table emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_bool_three_arg_invokers.c # 10-S2Y/10-S3AC bool(bool,bool,bool) generated Method.Invoke bucket consuming 11-S2D MethodInfo/functionIndex binding
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c # 07-S3/S4 value-frame cleanup drop lookup consumes 11-S4G function-level metadata runtime layout resolver
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.c # 07-S3/S4 value SemIR inline-copy layout lookup consumes 11-S4G function-level metadata runtime layout resolver
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_fields.c # 07-S3/S4 value SemIR nested inline-field transfer consumes 11-S4G function-level metadata runtime layout resolver
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c # 11-S6D no-arg i64 typed direct call passes function slot to guard/deopt writer
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c # 11-S6D..11-S6F i64/u64/f64 typed direct-call generated metadata guard and stack-call deopt fallback
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_bool_calls.c # 11-S6G bool-result typed direct-call generated metadata guard and stack-call deopt fallback
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.h # 11-S6D typed direct-call dispatch signature carries function slot for deopt fallback
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c # 11-S6D..11-S6G typed direct-call dispatch threads function slot into i64/u64/f64/bool guard/deopt writers
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_bool_thunks.h # 10-S2K/10-S3O typed bool no-arg, 10-S2O/10-S3S typed bool one-arg, 10-S2S/10-S3W typed bool two-arg, and 10-S2U/10-S3Y bool-return numeric comparison two-arg thunk eligibility predicates exported for generated reflection invoker buckets
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_bool_three_arg_thunks.h # 10-S2Y/10-S3AC typed bool three-arg thunk eligibility predicate exported for generated reflection invoker buckets
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_bool_three_arg_thunks.c # 10-S2Y/10-S3AC cleanup-reset short-circuit bool three-arg shape recognized for generated Method.Invoke bucket eligibility
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_f64_thunks.h # 10-S2L/10-S3P typed f64 no-arg, 10-S2P/10-S3T typed f64 one-arg, 10-S2T/10-S3X typed f64 two-arg/state-free, and 10-S2X/10-S3AB typed f64 three-arg/state-free thunk eligibility predicates exported for generated reflection invoker buckets
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.h # 10-S2I/10-S3M typed i64 no-arg, 10-S2M/10-S3Q typed i64 one-arg, 10-S2Q/10-S3U typed i64 two-arg/state-free, and 10-S2V/10-S3Z typed i64 three-arg/state-free thunk eligibility predicates exported for generated reflection invoker buckets
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunks.h # 10-S2J/10-S3N typed u64 no-arg, 10-S2N/10-S3R typed u64 one-arg, and 10-S2R/10-S3V typed u64 two-arg/state-free thunk eligibility predicates exported for generated reflection invoker buckets
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_three_arg_thunks.h # 10-S2W/10-S3AA typed u64 three-arg/state-free thunk eligibility predicates exported for generated reflection invoker buckets
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_three_arg_thunks.c # 10-S2W/10-S3AA typed u64 three-arg forward declaration/definition support consumed by generated reflection invoker buckets
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c # 11-S6H inline-struct CALL_TYPED generated metadata guard and dynamic deopt bridge fallback
- zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_size.h # 12-S7T zrp metadata size accounting API; 12-S7Z pruned-blob stats input; 12-S7ZO section-level trim delta marker surface; 12-S7ZP section count marker fields
- zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_size.c # 12-S7T zrp metadata header sampling; 12-S7Z blob-based after-stats sampling; 12-S7ZO per-section trim delta marker writing; 12-S7ZP per-section count stats/delta marker writing
- zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.h # 12-S7Z emitted zrp metadata pruning API; 12-S7ZA token-record MethodDef remap surface; 12-S7ZB FieldDef shared MEMBER_DEF remap; 12-S7ZC GenericParam owner/range remap; 12-S7ZD remap module split surface; 12-S7ZG signature blob pool compaction orchestration; 12-S7ZH string-pool compaction orchestration; 12-S7ZI constant-pool orphan sweep API surface; 11-S7ZSE/12-S7ZZV TypeDef token remap sidecar state
- zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.c # 12-S7Z compacted blob rebuild orchestration; 12-S7ZD delegates token/range remap helpers; 12-S7ZE GenericParamConstraint section copy/compaction orchestration; 12-S7ZF MethodSpec section copy/compaction orchestration; 12-S7ZG signature blob pool compaction/rewrite orchestration; 12-S7ZH delegates shared section helpers and string-pool remap/copy; 12-S7ZI zero-retained constant-pool layout; 12-S7ZM post-remap identity skip so pool compaction runs without MethodDef pruning; 12-S7ZZC retained signature blob TypeDef token rewrite hook; 12-S7ZZJ retained SIGNATURE token rewrite hook; 12-S7ZZK prune failure cleanup; 12-S7ZZL GenericParamConstraint TypeSpec token remap; 12-S7ZZM GenericParamConstraint TypeSpec root context threading; 12-S7ZZN signature AssemblyRef rewrite context; 12-S7ZZP builds signature remap before ModuleRef count/string remap and threads retained signature roots into ModuleRef pruning; 11-S7ZSC/12-S7ZZT delegates manifestExports section rewrite; 11-S7ZSD/12-S7ZZU invokes declaration row publication after prepare/prune; 11-S7ZSE/12-S7ZZV builds TypeDef token remap before type declaration publication; 11-S7ZSQ/12-S7ZZZJ threads token-record context into MethodSpec count/copy/rewrite; 11-S7ZSR/12-S7ZZZK threads retained imported MEMBER_REF context into MethodSpec count/copy/rewrite
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_member_token.h # 11-S7/12-S7ZX/12-S7ZZD retained member-token remap sidecar API; 11-S7ZE/12-S7 manifest export table build API; 11-S7ZSE/12-S7ZZV TypeDef token remap query API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_member_token.c # 11-S7/12-S7ZX/12-S7ZZD source-to-compacted member-token remap sidecar; 12-S7ZZG source duplicate guard; 12-S7ZZH token-shape guard; 12-S7ZZI retained-count consistency guard; 11-S7ZE/12-S7 manifest export table builder with compacted member-token rewrite; 11-S7ZSA/12-S7ZZR manifest export kind/token guard; 11-S7ZSY/12-S7ZZZR manifest export duplicate kind+target guard; 11-S7ZSE/12-S7ZZV manifest export type-token rewrite through TypeDef remap sidecar
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.h # 11-S7ZSC/12-S7ZZT persistent manifest export section copy/rewrite API; 11-S7ZSD/12-S7ZZU declaration row publication API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_manifest_export.c # 11-S7ZSC/12-S7ZZT remaps manifest export target string offsets, type tokens, and member tokens during pruned .zrp rebuild; 11-S7ZSD/12-S7ZZU appends bound method/field declaration rows with string-pool growth and member-token remap; 11-S7ZSE/12-S7ZZV appends bound type declaration rows with compacted TypeDef tokens; 11-S7ZSF/12-S7ZZW preserves/publishes unbound rows with zero flags/tokens
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_constant_pool.h # 12-S7ZV FieldDef default-value constant-pool retained-slice remap API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_constant_pool.c # 12-S7ZV retained constant-pool slice collection, compacted copy, identity check, and FieldDef offset rewrite
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.h # 12-S7ZD private zrp metadata token/range remap API; 12-S7ZE GenericParamConstraint remap/count/range API; 12-S7ZF MethodSpec remap/count API; 12-S7ZN export member-token remap surface; 11-S7ZSQ/12-S7ZZZJ MethodSpec imported MEMBER_REF token-record guard context; 11-S7ZSR/12-S7ZZZK MethodSpec imported MEMBER_REF retained-token-record context
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.c # 12-S7ZA..S7ZC MethodDef/FieldDef/GenericParam token/range remap implementation; 12-S7ZE GenericParamConstraint cascade implementation; 12-S7ZF MethodSpec method-token cascade implementation; 12-S7ZN exported MethodDef/FieldDef member token remap helper; 12-S7ZZI retained-count consistency helper; 11-S7ZSQ/12-S7ZZZJ imported MEMBER_REF token-record existence guard; 11-S7ZSR/12-S7ZZZK imported MEMBER_REF retained-token-record guard; 11-S7ZSS/12-S7ZZZL recursive imported MEMBER_REF retained-token-record guard
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_sections.h # 12-S7ZH shared zrp metadata section lookup/layout/copy API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_sections.c # 12-S7ZH shared section switch, layout writer, and raw-section copy helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_module_ref.h # 12-S7ZZ/12-S7ZZP ModuleRef row retention/count/compacted-token/remap API, including retained signature blob AssemblyRef roots
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_module_ref.c # 12-S7ZZ ModuleRef orphan sweep/AssemblyRef compaction; 12-S7ZZP retained signature blob scanner roots ModuleRef rows referenced only by signature payloads
- zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.h # 12-S7ZG signature blob remap/compaction API; 12-S7ZM signature remap identity API; 12-S7ZZC retained signature TypeDef token rewrite API; 12-S7ZZJ retained SIGNATURE token remap API; 12-S7ZZK retained SIGNATURE orphan rejection contract; 12-S7ZZN ModuleRef context for signature AssemblyRef token rewrite; 12-S7ZZP source signature-pool context for signature-rooted ModuleRef retention; 11-S7ZSQ/12-S7ZZZJ MethodSpec signature rewrite token-record guard context; 11-S7ZSR/12-S7ZZZK MethodSpec signature retained MEMBER_REF guard context
- zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.c # 12-S7ZG retained signature blob slice collection, MethodSpec signature rewrite, offset remap, and hash recomputation; 12-S7ZM signature remap identity helper; 12-S7ZZC recursive retained signature TypeDef token rewrite; 12-S7ZZJ retained signature-token-record order compaction/remap; 12-S7ZZK missing retained signature token rejection; 12-S7ZZM constraint-rooted TypeSpec signature blob retention; 12-S7ZZN retained signature AssemblyRef token rewrite; 12-S7ZZO retained signature MemberRef token rewrite; 12-S7ZZP signature rewrite context preserves source signature-pool remap for ModuleRef retained-root checks; 11-S7ZSQ/12-S7ZZZJ MethodSpec signature retention skips orphan imported MEMBER_REF records; 11-S7ZSR/12-S7ZZZK MethodSpec signature retention skips imported MEMBER_REF records with pruned member targets; 11-S7ZST/12-S7ZZZM retained TYPE_REF name string offset remap; 11-S7ZSU/12-S7ZZZN retained MODULE name/version string offset remap
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_spec.h # 12-S7ZW TypeSpec retained-row API; 12-S7ZY TypeSpec compacted-token/remap API; 12-S7ZZM GenericParamConstraint-root retention context
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_spec.c # 12-S7ZW TypeSpec orphan sweep; 12-S7ZY TypeSpec RID compaction; 12-S7ZZM retained GenericParamConstraint-rooted TypeSpec row detection
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_def.h # 11-S7ZSE/12-S7ZZV TypeDef token remap sidecar build/destroy API for manifest export type declaration publication; 11-S7ZSN/12-S7ZZZG TypeDef retained FieldDef owner-token root context
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_type_def.c # 11-S7ZSE/12-S7ZZV retained TypeDef source-to-compacted token remap sidecar construction and cleanup; 11-S7ZSN/12-S7ZZZG TypeDef token-record root guard rejects pruned/self-owner FieldDef roots
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_string_pool.h # 12-S7ZH string-pool remap/compaction API; 12-S7ZL duplicate retained string-slice remap support; 12-S7ZM string remap identity API; 12-S7ZZP signature-rooted ModuleRef string retention context; 11-S7ZSC/12-S7ZZT public manifest target string offset remap helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_string_pool.c # 12-S7ZH retained string slice collection, compacted string-pool copy, and row string-offset remap; 12-S7ZL content-level duplicate slice interning; 12-S7ZM string remap identity helper; 12-S7ZZP keeps ModuleRef name/version strings when only retained signature blobs reference the row; 11-S7ZSC/12-S7ZZT exports shared string-offset remapper for manifest export rows; 11-S7ZST/12-S7ZZZM retained signature TYPE_REF name string root scan; 11-S7ZSU/12-S7ZZZN retained signature MODULE name/version string root scan
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.h # 11-S4B generated-C SZrTypeLayout descriptor + registry writer surface; 11-S4O token table writer surface; 11-S4P/11-S4Q generated layout resolver surface
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c # 11-S4B generated-C SZrTypeLayoutField/SZrTypeLayout descriptors + sparse cTypeId/typeLayoutId registry; 11-S4P struct/union descriptor emission and registry resolver; 11-S4R generated struct/union ownership offset arrays
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_tokens.c # 11-S4P/11-S4Q generated-C TypeDef/TypeSpec-backed typeLayout token population
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c # 11-S2A shared invoker table emission support; 11-S2B root exported function method token table emission; 12-S7Y policy-driven MethodInfo reflection level emission
  - zr_vm_library/include/zr_vm_library/aot_runtime.h    # 11-S2B generated frame/context codeRegistration carrier; 11-S4E generic TYPE_LAYOUT/SIZEOF slot API takes SZrMetadataRuntime; 11-S6D..11-S6H typed direct-call guard/deopt runtime API
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_generic_dictionary.c # 11-S4E generic dictionary TYPE_LAYOUT/SIZEOF resolves via metadata runtime layout resolver
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c # 07-S3/S4 CopyStack inline-struct fallback consumes 11-S4G function-level metadata runtime layout resolver
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_return.c # 11-S6D..11-S6H typed direct-call metadata compatibility guard plus scalar stack-call and inline-struct dynamic deopt helpers; 07-S3/S4 runtime inline-struct call/return layout lookup consumes 11-S4G function-level metadata runtime layout resolver
  - zr_vm_library/src/zr_vm_library/aot_runtime.c        # 11-S2A descriptor/codeRegistration validation; 11-S2B runtime record/context registration consumption and method token table validation; 11-S2C metadata runtime attach; 11-S4B type-layout registry validation; 11-S4G loaded function table attaches function-level metadata registry; 11-S4O type-layout token table validation; 11-S6C dynamic AOT module-load binding compatibility reject; 11-S7/12-S7ZZD/S7ZZE/S7ZZF member-token remap table, entry, and duplicate validation; 11-S7ZE/12-S7 manifest export table validation; 11-S7ZL/12-S7 provider AOT load-request runtime consumption
  - zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c # 11-S2A mirrored descriptor/codeRegistration validation; 11-S2B mirrored method token table validation; 11-S4B mirrored type-layout registry validation; 11-S4O mirrored type-layout token table validation; 11-S7/12-S7ZZD/S7ZZE/S7ZZF mirrored member-token remap table, entry, and duplicate validation; 11-S7ZE/12-S7 mirrored manifest export table validation
  - tests/module/test_metadata_runtime_query.c         # 11-S2B/11-S2C module metadata runtime registration query and methodTokenCount mirror check; 11-S3A..11-S3M method/field/type/signature/TypeSpec token lazy/cache query + zrp metadata mmap attach/view + signature blob/header/type-node/generic TypeSpec signature/base-token/argument binding view + MethodSpec signature record/generic argument view query; 11-S4A/11-S4C TypeDef layout binding view + code-registration registry source proof; 11-S4D attach typeLayoutCount mirror check; 11-S4I FieldDef layout binding view and no-prototype-fallback regression; 11-S5 GenericParam/GenericParamConstraint runtime views; 11-S7/12-S7ZZQ runtime export member-token remap writeback coverage
  - tests/module/test_metadata_runtime_manifest_exports.c # 11-S7ZF/12-S7 mirror + 11-S7ZG/12-S7 runtime view coverage + 11-S7ZH/12-S7 manifest export binding gate coverage
  - tests/module/test_metadata_runtime_method_binding.c # 11-S2D method token -> MethodInfo/function pointer/invoker binding view coverage
  - tests/module/test_reflection_dynamic_generic_method_context.h # 11-S2E MethodDef interpreter binding and MethodSpec automatic execution coverage
  - tests/module/test_reflection_token_resolve.c       # 11-S4BE/10-S4Z19 inline aggregate field-copy borrowed-source write consumer coverage
  - tests/module/test_reflection_token_resolve.c       # 11-S4BD/10-S4Z18 inline aggregate borrowed-source write consumer coverage
  - tests/module/test_reflection_token_resolve.c       # 11-S4BC/10-S4Z17 inline struct signature/layout consumer coverage for FieldInfo borrowed view
  - tests/module/test_reflection_token_resolve.c       # 11-S4BB/10-S4Z16 FieldInfo object-level primitive POD consumer coverage
  - tests/module/test_reflection_token_resolve.c       # 11-S4BA/10-S4Z15 FieldInfo object-level write consumer coverage
  - tests/module/test_reflection_token_resolve.c       # 11-S4AZ/10-S4Z14 FieldInfo object-level read consumer coverage
  - tests/module/test_reflection_token_resolve.c       # 10-S3A/10-S2B/10-S2C/10-S2G/10-S3B/10-S3C/10-S3D/10-S3E/10-S3F/10-S3G/10-S3K/10-S4B..10-S4Z12 public reflection consumer coverage for 11-S2/11-S3/11-S4/11-S5 token/layout/method-signature/method-binding/token-driven Method.Invoke/signature-arity/return fixture/generic-param/MethodSpec/FieldInfo binding plus field signature header, type-node summary, primitive signature type views, primitive signature type object views, module reflection object link, direct TypeDef signature token/layout/type object carrier, bound TypeRef signature token/layout/type object carrier, signature/layout consistency carrier, signature type-node object carrier, signature base type-node object carrier, signature child type-node object list carrier, primitive child type-node semantic name, direct TypeDef base/child type-node semantic token/layout/name carrier, direct TypeRef child type-node semantic token/layout/name carrier, recursive signature type-node type literal consumer, FieldDef token VALUE_SLOT inline read/write consumers, FieldDef token primitive POD int32 raw inline read/write consumer, FieldDef token primitive POD bool/uint32/double representative raw inline read/write consumers, FieldDef token primitive POD int8/int16/int64/uint8/uint16/uint64/float32 storage-width raw inline read/write consumers, FieldDef token primitive POD integer range guard consumer, FieldDef token primitive POD float32 range guard consumer, FieldDef token primitive POD float32 NaN guard consumer, and FieldDef token primitive POD float32 precision guard consumer
  - tests/module/test_reflection_method_invoke.c       # 10-S2D/10-S3H Method.Invoke signature shape guard coverage; 10-S2E/10-S3I fixed parameter base-type guard coverage; 10-S2F/10-S3J return base-type guard coverage; 10-S2G/10-S3K required return-slot reset coverage; 10-S2H/10-S3L void return-slot canonicalization coverage
  - tests/module/test_metadata_runtime_binding_compatibility.c # 11-S6A/S6B token binding ABI drift predicate and function scan coverage for version range, module/member signature, token identity, layout identity, AssemblyRef->Module mapping, invalid binding, and first incompatible binding
  - tests/module/test_aot_runtime_typed_direct_call_compatibility.c # 11-S6D..11-S6H typed direct-call runtime guard coverage for caller/callee binding drift
  - tests/parser/test_aot_c_metadata_binding_loader.c # 11-S6C dynamic AOT module loader reject coverage for embedded/zro module metadata binding drift
  - tests/module/test_metadata_runtime_typespec_layout.c # 11-S4J focused TypeSpec layout binding view and no-prototype-fallback regression; 11-S4K TypeDef/TypeSpec token -> layout cache coverage; 11-S4S attached bound TypeRef token -> TypeDef layout resolver/cache/identity mismatch coverage; 11-S4L typeLayoutId -> token reverse lookup coverage; 11-S4M multi-entry cache coverage; 11-S4N cTypeId -> token coverage; 11-S4O code-registration token table coverage
  - tests/module/test_metadata_runtime_type_layout.c   # 11-S4D focused public typeLayoutId -> SZrTypeLayout resolver coverage; 11-S4F focused public typeLayoutId -> SZrAotGcDescriptor resolver coverage; 11-S4G function-level layout resolver coverage; 11-S4H prototype layout resolver + reflection consumer source contract
  - tests/gc/gc_tests.c                               # 11-S4G non-AOT inline-frame GC fallback regression
  - tests/parser/test_aot_c_frame_setup_contracts.c    # 11-S2A..11-S2C ABI/emitter/runtime source contract; 11-S2B method token table ABI/emitter/runtime source contract; 10-S2I/10-S3M generated reflection invoker i64 return-boxing source contract; 10-S2J/10-S3N generated reflection invoker u64 return-boxing source contract; 10-S2K/10-S3O generated reflection invoker bool return-boxing source contract; 10-S2L/10-S3P generated reflection invoker f64 return-boxing source contract; 10-S2M/10-S3Q generated reflection invoker i64 one-arg unbox source contract; 10-S2N/10-S3R generated reflection invoker u64 one-arg unbox source contract; 10-S2O/10-S3S generated reflection invoker bool one-arg unbox source contract; 10-S2P/10-S3T generated reflection invoker f64 one-arg unbox source contract; 10-S2Q/10-S3U generated reflection invoker i64 two-arg unbox source contract; 10-S2R/10-S3V generated reflection invoker u64 two-arg unbox source contract; 10-S2S/10-S3W generated reflection invoker bool two-arg unbox source contract; 10-S2T/10-S3X generated reflection invoker f64 two-arg unbox source contract; 10-S2U/10-S3Y generated reflection invoker bool-return numeric comparison two-arg source contract; 10-S2V/10-S3Z generated reflection invoker i64 three-arg source contract; 10-S2W/10-S3AA generated reflection invoker u64 three-arg source contract; 10-S2X/10-S3AB generated reflection invoker f64 three-arg source contract; 10-S2Y/10-S3AC generated reflection invoker bool three-arg source contract; 11-S4B type-layout registry ABI/emitter source contract; 11-S4O type-layout token carrier ABI/emitter source contract
  - tests/parser/test_aot_c_return_contracts.c         # 07-S3/S4 runtime inline-struct call/return metadata-runtime resolver contract; 07-S5 direct return boundary contract
  - tests/parser/test_aot_c_call_contracts.c           # 11-S6D..11-S6G generated-source contract for i64/u64/f64/bool typed direct-call metadata guard/deopt fallback
  - tests/parser/test_aot_c_value_semir_contracts.c    # 11-S6H inline-struct CALL_TYPED generated-source contract for metadata guard/dynamic deopt fallback; 07-S3/S4 inline-struct field transfer metadata-runtime resolver contract
  - tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c # 11-S6D i64 typed direct-call shared-library regression for guarded direct-call path
  - tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c # 11-S6E u64 typed direct-call shared-library regression for guarded direct-call path
  - tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c # 11-S6F f64 typed direct-call shared-library regression for guarded direct-call path
  - tests/parser/test_aot_c_typed_direct_call_bool_shared_library_smoke.c # 11-S6G bool typed direct-call shared-library regression for guarded direct-call path
  - tests/parser/test_aot_c_type_layout_contracts.c    # 11-S4B/11-S4P/11-S4R generated type-layout descriptor/token/ownership-offset writer source contracts
  - tests/parser/test_aot_c_generic_reference_sharing.c # 08-S4 generic dictionary acceptance; 11-S4E metadata-runtime-backed TYPE_LAYOUT/SIZEOF resolver regression
  - tests/parser/test_aot_c_source_contracts.c         # 11-S2A public ABI source contract; 11-S2B public method token ABI/emitter source contract; 11-S4B public type-layout registry source contract; 11-S4O/11-S4P public token table source contract; 07-S3/S4 frame cleanup metadata-runtime resolver source contract; 07-S3/S4 value SemIR copy/field metadata-runtime resolver source contract; 07-S3/S4 runtime CopyStack metadata-runtime resolver source contract; 12-S7ZE GenericParamConstraint remap module contract; 12-S7ZF MethodSpec remap module contract; 12-S7ZG signature blob remap module contract; 12-S7ZH section/string-pool module contract; 12-S7ZV constant-pool remap module contract; 12-S7ZO section-level delta marker source contract; 12-S7ZP section-count marker source contract; 12-S7ZZM GenericParamConstraint-rooted TypeSpec source contract; 12-S7ZZN signature AssemblyRef rewrite source contract; 12-S7ZZO signature MemberRef rewrite source contract; 12-S7ZZP signature-rooted ModuleRef retention source contract
  - tests/parser/test_aot_c_shared_library_smoke.c     # 11-S2A runtime descriptor/codeRegistration assertion; 11-S2B generated method token table runtime assertion; 10-S2I/10-S3M generated `answer(): int` reflection return-boxing runtime assertion; 10-S2J/10-S3N generated `unsigned_answer(): uint` reflection return-boxing runtime assertion; 10-S2K/10-S3O generated `truth(): bool` reflection return-boxing runtime assertion; 10-S2L/10-S3P generated `ratio(): float` reflection return-boxing runtime assertion; 10-S2M/10-S3Q generated `echo(value: int): int` argument-unbox + return-boxing runtime assertion; 10-S2N/10-S3R generated `echo_unsigned(value: uint): uint` argument-unbox + return-boxing runtime assertion; 10-S2O/10-S3S generated `echo_truth(value: bool): bool` argument-unbox + return-boxing runtime assertion; 10-S2P/10-S3T generated `echo_ratio(value: float): float` argument-unbox + return-boxing runtime assertion; 10-S2Q/10-S3U generated `sum_values(left: int, right: int): int` two-arg argument-unbox + return-boxing runtime assertion; 10-S2R/10-S3V generated `sum_unsigned(left: uint, right: uint): uint` two-arg argument-unbox + return-boxing runtime assertion; 10-S2S/10-S3W generated `same_truth(left: bool, right: bool): bool` two-arg argument-unbox + return-boxing runtime assertion; 10-S2T/10-S3X generated `sum_ratio(left: float, right: float): float` two-arg argument-unbox + return-boxing runtime assertion; 10-S2U/10-S3Y generated `less_values(left: int, right: int)`, `unsigned_after(left: uint, right: uint)`, and `ratio_equal(left: float, right: float)` bool-return numeric comparison runtime assertions; 10-S2V/10-S3Z generated `sum_three(left: int, middle: int, right: int): int` three-arg runtime assertion; 10-S2W/10-S3AA generated `sum_three_unsigned(left: uint, middle: uint, right: uint): uint` three-arg runtime assertion; 10-S2X/10-S3AB generated `sum_three_ratio(left: float, middle: float, right: float): float` three-arg runtime assertion; 10-S2Y/10-S3AC generated `all_truth(left: bool, middle: bool, right: bool): bool` three-arg runtime assertion; 11-S4B empty registry assertion; 11-S4O empty token table assertion; 11-S6C normal empty-binding loader regression
  - tests/parser/test_aot_c_value_type_shared_library_smoke.c # 11-S4B generated type-layout registry + GC descriptor alignment assertion; 11-S4O generated token table shape assertion; 11-S4P TypeDef-backed nonzero union token assertion; 11-S4R generated struct/union ownership-offset table assertion
  - tests/parser/test_aot_c_generic_call_typed.c       # 08-S5 generic call typed coverage; 11-S4Q TypeSpec-backed generated type-layout token assertion; 11-S6H shared/full-AOT inline-struct CALL_TYPED metadata guard assertion
  - tests/parser/test_aot_c_code_stripping.c           # 12-S7K generated-C zrp metadata section/table/pool byte statistics; 12-S7Y default-min MethodInfo reflection metadata policy; 12-S7Z zrp MethodDef metadata pruning; 12-S7ZG signature blob pool after-trim delta; 12-S7ZH string pool after-trim delta; 12-S7ZI constant pool after-trim delta
  - tests/parser/test_aot_c_zrp_metadata_pruning.c     # 12-S7ZA direct zrp MethodDef/token-record pruning; 12-S7ZB FieldDef shared MEMBER_DEF remap; 12-S7ZC GenericParam owner/range remap; 12-S7ZE GenericParamConstraint cascade remap; 12-S7ZF MethodSpec method-token cascade remap; 12-S7ZG MethodSpec signature blob rewrite/compaction; 12-S7ZZL GenericParamConstraint TypeSpec constraint-token compaction; 12-S7ZZM GenericParamConstraint-only TypeSpec retention root fixture; 12-S7ZZO retained signature MemberRef token rewrite fixture; 11-S7ZSC/12-S7ZZT existing manifest export rows remap target strings/member tokens after pruning; 11-S7ZSF/12-S7ZZW unbound manifest export row/declaration persistence
  - tests/parser/test_aot_c_zrp_metadata_publication.c # 11-S7ZSG/12-S7ZZX writer-level compacted .zrp metadata sidecar publication; 11-S7ZSX/12-S7ZZZQ invalid definition-table rejection/stale sidecar cleanup
  - tests/parser/test_aot_c_zrp_metadata_export_token_remap.c # 12-S7ZN direct exported MethodDef/FieldDef member token remap coverage; 11-S7ZSA/12-S7ZZR manifest export kind/token mismatch guard; 11-S7ZSY/12-S7ZZZR manifest export duplicate kind+target guard
  - tests/parser/test_aot_c_zrp_metadata_size_deltas.c # 12-S7ZO direct section-level zrp metadata before/after/removed marker coverage; 12-S7ZP direct section count marker coverage
  - tests/parser/test_aot_c_zrp_metadata_pool_pruning.c # 12-S7ZH direct zrp string-pool compaction/remap; 12-S7ZI direct zrp orphan constant-pool sweep; 12-S7ZL retained duplicate string-slice compaction; 12-S7ZM no-MethodDef-prune pool compaction trigger; 12-S7ZZN retained signature AssemblyRef token rewrite fixture; 12-S7ZZP signature-rooted ModuleRef retention fixture
  - tests/parser/test_aot_c_zrp_metadata_pool_pruning.c # 12-S7ZV direct FieldDef default-value constant-pool retained-slice remap coverage
  - tests/cli/test_cli_args.c                         # 11-S7W/11-S7X/11-S7Y CLI dump/diff/version-check mode parse/exclusivity coverage
  - tests/cli/test_cli_zrp_metadata_dump.c            # 11-S7W/11-S7X/11-S7Y zrp metadata dump/diff/version-check summary/path coverage
  - tests/CMakeLists.txt                               # 11-S6A/S6B metadata runtime binding compatibility test target/CTest; 11-S6C AOT metadata binding loader target/CTest; 11-S6D AOT runtime typed direct-call compatibility target/CTest; 11-S7W/11-S7X/11-S7Y CLI zrp metadata dump/diff/version-check target; 12-S7ZA/12-S7ZH/12-S7ZN/12-S7ZO/12-S7ZP/12-S7ZV Windows shared-DLL direct zrp pruning/remap/size tests link private pruning/remap/section/signature/string-pool/constant-pool/size modules; 11-S7ZSG/12-S7ZZX sidecar publication test target/CTest; 11-S7ZSX/12-S7ZZZQ publication failure-path regression in same target; 11-S7ZSY/12-S7ZZZR manifest export duplicate kind+target guard coverage in export-token remap target; 11-S7ZSH/12-S7ZZY CLI/project sidecar path bridge target/CTest + 11-S7ZSI/12-S7ZZZ stale sidecar cleanup/failure coverage; 11-S7ZM provider AOT shared-library smoke target; 11-S7ZN provider version-selection target
  - tests/cli/test_cli_aot_compacted_metadata_sidecar.c # 11-S7ZSH/12-S7ZZY CLI/project automatic compacted .zrp sidecar path derivation, publishable metadata gate, invalid definition-table guard; 11-S7ZSI/12-S7ZZZ non-publishable metadata stale sidecar cleanup and removal-failure fail-closed guard
  - tests/parser/test_aot_c_descriptor_diagnostics.c   # 11-S2A missing codeRegistration diagnostic
  - zr_vm_library/include/zr_vm_library/project.h      # 11-S7A..11-S7F/11-S7K..11-S7M/11-S7Z project manifest normalized dependency/reference + identity + preserve + AOT mode/feature switch/generic preserve argument/export declaration model
  - zr_vm_library/src/zr_vm_library/project/project.c  # 11-S7A..11-S7F/11-S7L/11-S7Z manifestVersion + old dependencies/new references normalization + assembly identity gates + preserve/AOT/feature/export parser dispatch; 11-S7ZN declared provider version-range guard
  - zr_vm_library/src/zr_vm_library/project/project_import_resolver.c # 11-S7D dependency AssemblyRef identity query exposure
  - zr_vm_library/src/zr_vm_library/project/project_import_provider_location.c # 11-S7ZJ provider import location discovery; 11-S7ZK provider AOT load-request planning
  - tests/library/test_project_import_aot_provider_runtime.c # 11-S7ZL provider AOT runtime load-request consumption coverage
  - tests/library/test_project_import_provider_version_selection.c # 11-S7ZN provider multi-version exact alias selection + declared range guard
  - tests/parser/test_aot_c_provider_shared_library_smoke.c # 11-S7ZM provider AOT dynamic-library success fixture
  - zr_vm_library/src/zr_vm_library/project/project_features.c # 11-S7L zrp feature switch parser
  - zr_vm_library/src/zr_vm_library/project/project_preserve.c # 11-S7E/11-S7K/11-S7M zrp preserve rule + feature condition + generic argument parser
  - zr_vm_library/src/zr_vm_library/project/project_exports.h # 11-S7Z zrp export declaration parser/free API
  - zr_vm_library/src/zr_vm_library/project/project_exports.c # 11-S7Z zrp export declaration parser; 11-S7ZSZ/12-S7ZZZS duplicate export kind+target parser guard
  - zr_vm_library/src/zr_vm_library/project/project_aot_options.c # 11-S7F zrp aotMode declaration parser
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.h       # 11-S7G project aotMode -> AOT writer option bridge API
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c       # 11-S7G project aotMode -> requireFullAot injection helper; 11-S7ZSH/12-S7ZZY optional AOT C cleanup removes derived compacted .zrp sidecar
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.h   # 11-S7H..11-S7ZA AOT C emission + preserve/export root bridge API
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot.c   # 11-S7H..11-S7ZA binary embedded blob + method/type/generic preserve bridge + feature-conditioned root gating + generic TypeSpec synthesis/open-base generic-instantiation/MethodSpec binding + export declaration bridge hook; 11-S7ZSH/12-S7ZZY publishable .zrp sidecar path gate; 11-S7ZSI/12-S7ZZZ stale sidecar removal/failure guard when current embedded blob is not publishable metadata
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.h # 11-S7ZA export declaration -> writer option bridge API
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler_aot_exports.c # 11-S7ZA..11-S7ZD project export declaration -> AOT writer manifest declaration mapping and current-module type/method/field token binding; 11-S7ZTB/12-S7ZZZU duplicate export declaration bridge guard
  - zr_vm_cli/src/zr_vm_cli/command/command.h        # 11-S7W/11-S7X/11-S7Y CLI zrp metadata dump/diff/version-check mode carriers
  - zr_vm_cli/src/zr_vm_cli/command/command.c        # 11-S7W `--dump-zrp-metadata <file>`, 11-S7X `--diff-zrp-metadata <before> <after>`, and 11-S7Y `--check-zrp-metadata-version <file>` parse/exclusivity/help surface
  - zr_vm_cli/src/zr_vm_cli/app/app.c                # 11-S7W/11-S7X/11-S7Y CLI app dispatch to zrp metadata dump/diff/version-check runners
  - zr_vm_cli/src/zr_vm_cli/project/project.h        # 11-S7ZSH/12-S7ZZY project AOT C path -> compacted .zrp sidecar path API
  - zr_vm_cli/src/zr_vm_cli/project/project.c        # 11-S7ZSH/12-S7ZZY project AOT C path -> compacted .zrp sidecar path implementation
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.h # 11-S7W/11-S7X/11-S7Y zrp metadata dump summary, diff summary, and version-check API
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.c # 11-S7W read-only zrp metadata section summary; 11-S7X before/after section byte/count diff summary; 11-S7Y header magic/version/header-size/section-count compatibility check implementation
  - zr_vm_language_server_extension/schemas/zrp.schema.json # 11-S7C..11-S7F/11-S7K..11-S7M/11-S7Z manifest schema parity for identity/dependency/preserve/aotMode/feature/generic/export fields; 11-S7ZTA/12-S7ZZZT exports uniqueItems parity
  - zr_vm_parser/include/zr_vm_parser/writer.h        # 11-S7I top-level callable flat-index resolve API；11-S7N..11-S7V manifest generic writer root carrier + TypeSpec/generic-instantiation/MethodSpec binding fields；11-S7ZA manifest export declaration writer option carrier；11-S7ZSG compactedZrpMetadataOutputPath sidecar option
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c # 11-S7I callable binding -> AOT flat index resolver；11-S7N/11-S7O manifest generic root diagnostics + TypeSpec token/hash comments；11-S7P full-AOT unbound generic TypeSpec gate；11-S7Q generic instantiation diagnostics；11-S7R full-AOT generic-instantiation gate；11-S7V MethodSpec diagnostics/full-AOT closure alternative；11-S7ZA manifest export declaration diagnostics；12-S7K zrp metadata size markers；12-S7Y metadata policy marker；12-S7Z..12-S7ZV emitted zrp metadata pruning
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_callable_provenance.h
  - tests/module/test_zrp_metadata_format.c            # 11-S1A..11-S1J round-trip + mmap-view + pool/table payload write + string view + definition table directory/view/token/range/signature-blob validation; 11-S7ZSF/12-S7ZZW unbound manifest export row shape validation
  - tests/library/test_project_manifest_normalization.c # 11-S7A..11-S7F/11-S7K..11-S7M/11-S7Z .zrp manifestVersion + mixed dependency/reference + assembly identity + preserve + aotMode/feature/generic/export normalization gates; 11-S7ZSZ/12-S7ZZZS duplicate export target rejection
  - tests/cli/test_cli_project_incremental.c          # 11-S7G/11-S7H manifest aotMode + CLI AOT C emission bridge; 11-S6H/12-S8E CLI full-AOT metadata-drift guard assertion alignment
  - tests/cli/test_cli_aot_writer_options.c           # 11-S7I..11-S7ZA parsed method/type/generic/feature-conditioned preserve -> writer manifest root binding + generic TypeSpec synthesis/open-base generic-instantiation/MethodSpec binding/full-AOT gate + export declaration writer-option bridge; 11-S7ZTB/12-S7ZZZU duplicate export declaration bridge rejection
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
    const TZrUInt32 *methodTokens;                           /* 11-S2B，按 functionIndex 索引 */
    TZrUInt32 methodTokenCount;
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
- 11-S2B method token carrier 将 code registration 扩展为 `methodTokens[functionIndex]` 表，
  与 `methodInfos[]` 对齐并在生成 C 中由 root module typed exported function 的可靠 `MEMBER_DEF` token 填充；
  runtime descriptor validation 检查 descriptor/codeRegistration 的指针、计数和空/非空形态一致。
- 11-S2D 在 `SZrMetadataRuntime` 上提供只读 method binding view：
  `ZrCore_MetadataRuntime_ReadMethodBindingView(runtime, methodToken, outView)` 扫描 `methodTokens[]`，
  只接受唯一 local `MEMBER_DEF` token，并返回对应 `functionIndex`、`SZrAotMethodInfo`、entry thunk 和 invoker。
  缺失表、重复 token、非 method token、MethodInfo/function slot 不一致、缺 thunk 或缺 invoker 都失败并清空输出。
  10-S2B/10-S3F 已在反射层提供 `ZrCore_Reflection_InvokeMethodToken(...)` 作为最小 public dispatcher，
  消费该 view 中的 MethodInfo/function pointer/invoker 并交给 registered invoker；10-S2C/10-S3G 又提供
  counted dispatcher `ZrCore_Reflection_InvokeMethodTokenWithArgCount(...)`，在 dispatch 前消费 MethodInfo
  signature 的 `parameterCount/hasVarArgs` 做参数数量边界检查；10-S2D/10-S3H 继续在 counted dispatcher 中
  拒绝缺少 fixed parameter `parameterTypes` 或 required `returnType` 的 incomplete signature shape，避免把
  不完整 MethodInfo shape 派发给 invoker；10-S2E/10-S3I 又对 fixed parameter 的 concrete `baseType` 与
  对应 `SZrTypeValue.type` 做 invoker 前等值 guard；10-S2F/10-S3J 在 invoker 写出 `outReturn` 后对 concrete
  `returnType->baseType` 做 post-dispatch guard；10-S2G/10-S3K 又在 required return dispatch 前清空
  `outReturn`，避免旧返回槽绕过 post-guard；10-S2H/10-S3L 在 no-return dispatch 后把最终 `outReturn`
  规范为 null；10-S2I/10-S3M、10-S2J/10-S3N、10-S2K/10-S3O 与 10-S2L/10-S3P 在 generated reflection invoker 中用
  `functionIndex` 选择已有 `zr_aot_typed_i64_fn_<index>()`、`zr_aot_typed_u64_fn_<index>()`、
  `zr_aot_typed_bool_fn_<index>()` 或 `zr_aot_typed_f64_fn_<index>()`，为 int64/uint64/bool/f64 no-arg
  signature 写出 boxed return。code registration
  中的 `functionPointers` 仍是完整执行 thunk，返回执行成功标志，不作为业务返回 carrier。当前仍不提供
  public method reflection object、完整 `Method.Invoke` 参数类型 unbox 或完整返回 marshaling。
- 11-S2C 在 AOT 模块加载时为 `SZrObjectModule` attach 最小 `SZrMetadataRuntime`，携带 module、
  metadata function、代码注册表与 function/method/invoker/GC descriptor 计数，并让 GC 标记/搬迁维护
  metadata function 引用；token→函数/layout lazy 解析、结果缓存和 token↔layout 三向表留给后续
  11-S3/11-S4。
- 11-S2E 在同一 method-binding 模块提供解释器 MethodDef binding view：读取 attached zrp MethodDef section，
  要求 method token 为唯一 local `MEMBER_DEF` 且同时存在 runtime method record，再将 row 的 `functionIndex`
  交给 `ZrCore_Function_ResolveGraphFunctionByFlatIndex()`。函数图顺序与 AOT function table 一致：root、
  constant-referenced function、child function 深度优先，并按指针或 metadata identity 去重。返回函数必须是
  non-native、instruction-backed VM function；重复/缺失 row、越界/超大 index 或畸形函数均清 view 并失败。
  visited storage 只随实际遍历节点增长，因此不可信 index 不会直接驱动超大分配。

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
- 11-S4AA / 10-S4M 已让最小 public `FieldInfo` object 消费该 nested type-node 只读 view：
  反射层在 validated `FIELD_SIG` header 的 `fieldTypeBlobOffset` 处调用
  `ZrCore_MetadataRuntime_ReadSignatureTypeNode()`，把 node、payload、base/child offsets 与 next offset 暴露为
  public summary carrier。该 consumer 仍不把 signature type-node 绑定为 semantic field type，也不物化 runtime type/layout/entity。
- 11-S4AB / 10-S4N 已让最小 public `FieldInfo` object 在 primitive node 上继续消费 payload：
  `PRIMITIVE` field type-node 的 payload0 会作为 `fieldTypeSignatureValueType` 暴露，并经反射层内置名称表写出
  `fieldTypeSignatureTypeName`。该 consumer 仍不把 TypeDef/TypeRef signature node 绑定到 metadata token/layout，
  也不改变 FieldDef layout-derived `typeName`。
- 11-S4AC / 10-S4O 已让最小 public `FieldInfo` object 把 primitive payload 消费到独立 type literal object：
  `fieldTypeSignatureTypeName` 会作为 `fieldTypeSignatureType` reflection object 的 name/qualifiedName 来源。该
  consumer 仍不把 TypeDef/TypeRef signature node 绑定到 metadata token/layout，也不改变 FieldDef layout-derived
  `type` object。
- 11-S4AD / 10-S4P 已让最小 public `FieldInfo` object 消费 attached runtime module identity：
  运行期 FieldInfo builder 在 `runtime->module` 是真实 module object 时复用现有 module reflection builder，把
  `FieldInfo.module` 指向 `kind == "module"` 的 reflection object。该 consumer 只连接 module identity/cache，
  不新增 zrp row、code-registration ABI 或跨模块 provider 解析。
- 11-S4AE / 10-S4Q 已让最小 public `FieldInfo` object 消费 direct local `TYPE_DEF` field signature type identity：
  运行期 FieldInfo builder 将 validated `FIELD_SIG` 的 field type-node 与 attached metadata token records 的
  direct type signature blob 做等值匹配，命中后复用 `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` 暴露
  signature-derived token/layout/size carrier。该 consumer 只连接当前 runtime 已 attached 的 TypeDef token 与
  registry layout，不新增 zrp row、code-registration ABI、TypeRef provider lookup 或递归 type-node materialization。
- 11-S4AF / 10-S4R 已让该 direct local TypeDef identity 继续消费 zrp TypeDef row name：
  FieldInfo builder 在 token/layout 命中后复用 `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()`，
  从 TypeDef row 的 string-pool offset 读取 `fieldTypeSignatureTypeName`，并物化 `fieldTypeSignatureType`
  type literal object。该 consumer 不新增 metadata row 或 ABI，不改变 TypeRef provider lookup，也不递归物化
  wrapper/generic signature node。
- 11-S4AG / 10-S4S 已让 FieldInfo builder 消费 11-S4S 的 attached bound TypeRef token -> target TypeDef layout resolver：
  Field signature 的 `TYPE_REF` node 会匹配 module metadata token record，public carrier 保留 TypeRef token identity，
  layout/id/size 来自 resolver 校验后的 target TypeDef；builder 再读取 target TypeDef row name 并物化
  `fieldTypeSignatureType`。该 consumer 不新增 metadata ABI，不加载跨模块 provider，也不递归物化 wrapper/generic
  signature node。
- 11-S4AH / 10-S4T 已让 FieldInfo builder 对 signature-derived layout 与 FieldDef layout binding view 的结果做
  只读一致性暴露：`fieldTypeSignatureMatchesLayout` 复用同一个 registry-backed `SZrTypeLayout` 指针和 layout id，
  只在二者都存在且相同的时候为 true。该 consumer 不新增 metadata ABI，也不改变 TypeRef resolver 或 FieldDef binding view。
- 11-S4AI / 10-S4U 已让 FieldInfo builder 把 11-S3I 已验证的 field signature type-node view 聚合为
  `fieldTypeSignatureNodeObject`：对象承载 node/blob/payload/base/child summary，并同步当前 signature-derived
  token/layout/typeName/matchesLayout。该 consumer 不新增 metadata ABI，不改变 TypeRef resolver，也不递归物化
  wrapper/generic child nodes。
- 11-S4AJ / 10-S4V 已让 FieldInfo builder 继续复用 11-S3I type-node view 里的 `baseTypeBlobOffset`，在
  `fieldTypeSignatureNodeObject` 下物化 `baseTypeNodeObject`。该 consumer 只从同一个 validated signature blob 结构化读取
  base node，不新增 metadata ABI，不改变 TypeRef resolver，也不绑定 generic base/argument semantic token/layout。
- 11-S4AK / 10-S4W 已让 FieldInfo builder 继续复用 11-S3I type-node view 里的 `childListBlobOffset` 和
  `childCount`，在 `fieldTypeSignatureNodeObject` 下物化 `childNodeObjects` array。该 consumer 只从同一个
  validated signature blob 结构化读取 child nodes，不新增 metadata ABI，不改变 TypeRef resolver，也不绑定
  generic argument semantic token/layout。
- 11-S4AL / 10-S4X 已让 FieldInfo builder 对 primitive child type-node 复用 runtime builtin type name mapping，
  在 `fieldTypeSignatureNodeObject.childNodeObjects[0].typeName` 上暴露 `PRIMITIVE(INT64)` 的 semantic name `int`。
  该 consumer 不新增 metadata ABI，不改变 TypeRef resolver，也不绑定 direct TypeDef/TypeRef child token/layout。
- 11-S4AM / 10-S4Y 已让 FieldInfo builder 对 recursive direct `TYPE_DEF` base/child type-node 复用 runtime signature
  record matcher、TypeDef token→layout resolver 和 TypeDef row name view，在 `baseTypeNodeObject` 与
  `childNodeObjects[1]` 上同时暴露 semantic `typeToken/typeLayoutId/typeSize/typeName`。该 consumer 不新增 metadata
  ABI，不改变 TypeRef resolver，也不声明 direct TypeRef/cross-module child binding。
- 11-S4AN / 10-S4Z2 已让 FieldInfo builder 对 recursive direct `TYPE_REF` child type-node 复用 module signature
  record matcher、attached TypeRef token→target TypeDef layout resolver 和 target TypeDef row name view，在
  `childNodeObjects[2]` 上暴露 semantic `typeToken/typeLayoutId/typeSize/typeName`。该 consumer 不新增 metadata
  ABI，也不声明 cross-module provider loading/version compatibility。
- 11-S4AO / 10-S4Z3 已让 FieldInfo builder 对 recursive signature type-node 在已有 semantic `typeName` 时物化
  public `type` type literal object；base TypeDef node、primitive child、direct TypeDef child 和 bound TypeRef child
  都复用同一 `type` 字段惯例。该 consumer 不新增 metadata ABI，也不改变 TypeRef resolver 或 cross-module provider policy。
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
- 10-S3C 已在 public reflection consumer 侧复用 11-S3C/11-S3M 的签名单一真相：普通 MethodDef/MethodRef
  token 通过 `ZrCore_MetadataRuntime_ResolveSignatureRecord()` 暴露 paired signature record/hash，MethodSpec
  token 通过 `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView()` 暴露自身 signature record/hash 作为 method
  signature identity。当前仍不物化 MethodInfo/function pointer/invoker binding 或 public method entity。
- 11-S5 / 10-S4E 在 MethodSpec signature view 之上补 indexed generic argument view：
  `ZrCore_MetadataRuntime_ReadMethodSpecGenericArgumentView()` 按 MethodSpec token + argument index 读取
  `GENERIC_INST(MEMBER_REF methodToken, args...)` 的 primitive 或 direct TypeDef/TypeRef 实参节点，并在可直接绑定时
  暴露 argument token/record；`ZrCore_Reflection_ResolveMethodSpecGenericArgument()` 将同一信息复制到 public
  reflection carrier。当前仍不物化 method instantiation、generic dictionary、递归泛型实参对象或运行期 generic layout。
- 11-S5A / 08-S6V / 10-S4Z43 将 GenericParam 读取器内部的 owner range 提升为 public read-only view：
  `ZrCore_MetadataRuntime_ReadGenericOwnerView()` 对 TypeDef/MethodDef 返回 owner token/record、对应 definition row、
  `firstGenericParamIndex` 与 `genericParamCount`，失败先清 output。现有 parameter reader 复用该入口，反射消费者可按
  声明数量验证连续物理 row 与 logical parameter index，避免把损坏范围静默截短。zrp row/section ABI 不变。
- 11-S4Z / 10-S4L 已把 11-S3H 的 field signature header view 接入 public `FieldInfo` consumer：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 通过 `ZrCore_MetadataRuntime_ReadSignatureView()` 读取 FieldDef
  token 的 validated `FIELD_SIG` header，并暴露 `signatureRootNode`、`signatureFlags` 与 `fieldTypeBlobOffset`。
  该消费端不新增 metadata parser，不改变 zrp row/ABI，也不把 field type node 绑定为 semantic type。

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
- 10-S4C 已把 public FieldDef reflection carrier 接到上述三向表：FieldDef 解析在读取
  `ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView()` 后，用 `ZrCore_MetadataRuntime_ResolveTypeLayoutToken()`
  从 field type layout id 反查 TypeDef/TypeSpec token，再通过 `ResolveTypeRecord()` 暴露 field type record。
  这是 11-S4L/11-S4N resolver 的反射侧消费，不新增 zrp row 或 code-registration ABI。
- 10-S4F/11-S4T 已让最小 public FieldInfo object 消费上述 FieldDef binding view 与 zrp string pool：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 读取 FieldDef row 的 field name、owner TypeDef row 的 name，
  并在 field type 为 TypeDef 时通过 TypeDef layout binding view 读取 field-type name；layout/token/offset/size
  均继续来自 11-S4 的 FieldDef carrier 单一真相。该 consumer 不新增 metadata row，也不改变 code-registration ABI。
- 10-S4G/11-S4U 已把同一 owner TypeDef row name 继续暴露为 public FieldInfo 的 `ownerTypeName`、
  `declaringTypeName` 和 nested `declaringType` type literal object。该 consumer 仍只读取既有 zrp string pool 与
  TypeDef binding carrier，不新增 metadata row，也不改变 code-registration ABI。
- 10-S4H/11-S4V 已让同一 nested `declaringType` object 作为 public FieldInfo 的 `owner` link。该 consumer
  仍不新增 metadata row，也不改变 code-registration ABI；module reflection link 和缓存策略留给后续。
- 10-S4I/11-S4W 已让 public FieldInfo 消费 attached metadata runtime module 的名称：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 读取 `runtime->module->moduleName`，缺失时回退 `fullPath`，
  并写入 `moduleName`。该 consumer 不新增 metadata row，也不改变 code-registration ABI；完整 module reflection
  object link 和缓存策略留给后续。
- 10-S4J/11-S4X 已让 public FieldInfo 消费 FieldDef row 的 raw `flags`：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 读取 resolved `SZrZrpMetadataFieldDefRow.flags` 并写入
  `metadataFlags`。该 consumer 不新增 metadata row，不改变 code-registration ABI，也不解释 flags 位语义。
- 10-S4K/11-S4Y 已让 public FieldInfo 消费 FieldDef row 的 raw signature blob 坐标：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 读取 resolved `signatureBlobOffset` / `signatureBlobLength` 并写入
  `signatureBlobOffset` / `signatureBlobLength`。该 consumer 不新增 metadata row，不改变 code-registration ABI，也不验证
  或解析 field signature blob。
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
  `VALUE_SLOT | OWNERSHIP_VALUE` 字段安全导出的 struct/union layout，emitter 写出
  `ZrOwnershipOffsets_<typeLayoutId>[]`，并让 `.ownershipFieldOffsets` 指向该表；zero-count 和 unsafe/
  unsupported offset 路径保持 `ZR_NULL` 并输出显式 failure marker。union layout 的 active payload 判定仍由
  `SZrTypeLayoutField.activeTag` 与 tag metadata 承担，本表只暴露 owner payload byte offsets。当前仍不声明持久
  cTypeId→token 索引表、runtime generic layout construction、cross-module token table 或 public reflection entity 完成。
- 11-S4BO 提供 `ZrCore_MetadataRuntime_ResolveSignatureTypeNodeRecord()`：direct TypeDef/TypeRef 节点和 nested
  `GENERIC_INST` 节点按完整节点字节跨度匹配 attached metadata record；nested generic 仅返回真实本地 TypeSpec
  record，不根据名称或浅 payload 伪造 token。旧 direct-node 匹配从 1036 行 `metadata_runtime.c` 抽到独立
  `metadata_runtime_type_node_binding.c`，原 TypeSpec/MethodSpec binding view 复用新入口。跨模块 TypeSpec
  canonical identity 和 provider token remap 不在本切片内。

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
| 11-S2 | 代码注册表发射 + 模块加载登记（§2） | 🚧 2026-06-30 部分完成：11-S2A 已完成 generated-C `SZrAotCodeRegistration` carrier、函数/method/invoker/GC descriptor 表指针登记、模块 descriptor 暴露，以及运行期 descriptor validation 对缺失/不一致 code registration 的拒绝；11-S2B 已让运行时 loaded-module record、generated module context、generated frame、direct/static/meta/callable thunk 路径经 `codeRegistration` 消费 function/method 表，并暴露 `methodTokens[functionIndex]` carrier；11-S2C 已在 AOT 模块加载时 attach 最小 `SZrMetadataRuntime`，登记 module、metadata function、code registration 与表计数，并让 GC mark/rewrite 覆盖 metadata function；11-S2D 已提供 runtime 内部 method token→MethodInfo/function pointer/invoker binding view，10-S3D 已让普通 MethodDef/MethodRef public token resolver carrier 消费该 view，10-S3E 已让 MethodSpec public carrier 复用 underlying MethodDef binding，10-S2B/10-S3F 已提供 public token-driven invoke dispatcher 消费该 binding view，10-S2C/10-S3G 已让 counted dispatcher 用 MethodInfo signature 做参数数量/varargs guard，10-S2D/10-S3H 已让 counted dispatcher 拒绝缺少 `parameterTypes` 或 required `returnType` 的 incomplete signature shape，10-S2E/10-S3I 已让 counted dispatcher 对 fixed parameter concrete `baseType` 与 `SZrTypeValue.type` 做等值 guard，10-S2F/10-S3J 已让 counted dispatcher 对 concrete return baseType 与 invoker 写出的 `outReturn->type` 做 post-dispatch guard，10-S2G/10-S3K 已让 counted dispatcher 在 required return dispatch 前清空 `outReturn` 以拒绝 stale 返回槽，10-S2H/10-S3L 已让 counted dispatcher 在 void/no-return dispatch 后把最终 `outReturn` 规范为 null，10-S2I/10-S3M 已让 generated reflection invoker 对 int64 no-arg method 用 `functionIndex` 调 typed i64 helper 并 box 返回值，10-S2J/10-S3N 已让 generated reflection invoker 对 uint64 no-arg method 用 `functionIndex` 调 typed u64 helper 并 box 返回值，10-S2K/10-S3O 已让 generated reflection invoker 对 bool no-arg method 用 `functionIndex` 调 typed bool helper 并 box 返回值，10-S2L/10-S3P 已让 generated reflection invoker 对 f64 no-arg method 用 `functionIndex` 调 typed f64 helper 并 box 返回值，10-S2M/10-S3Q 已让 generated reflection invoker 对 int64(int64) method 从 `args[0]` 解包 `TZrInt64`、调用 typed i64 one-arg helper 并 box 返回值，10-S2N/10-S3R 已让 generated reflection invoker 对 uint64(uint64) method 从 `args[0]` 解包 `TZrUInt64`、调用 typed u64 one-arg helper 并 box 返回值；public method reflection object、更多 `Method.Invoke` 参数类型 unbox、object/inline 返回、data metadata mmap attach、token→layout lazy 解析和完整缓存仍待后续 10/11 |
| 11-S3 | `SZrMetadataRuntime` token lazy 解析 + 缓存（§3） | 🚧 2026-06-30 部分完成：11-S3A 已提供 `ZrCore_MetadataRuntime_ResolveMethodRecord()`，可从 attached metadata function 的本地 `MEMBER_DEF` 与 module `MEMBER_REF` token record 表 lazy 解析 method token，并用单项 cache 覆盖二次命中；11-S3B 已提供 `ZrCore_MetadataRuntime_ResolveTypeRecord()`，可从本地 `TYPE_DEF` 与 module `TYPE_REF` token record 表 lazy 解析 type token，并用独立单项 cache 覆盖二次命中；11-S3C 已提供 `ZrCore_MetadataRuntime_ResolveSignatureRecord()`，可按 entity token lazy 解析本地或 module `SIGNATURE` record 并缓存最近一次命中，且 10-S3C 已将它接入 public method signature carrier；11-S3D 已让本地 `TYPE_SPEC` record 进入 `ResolveTypeRecord()` 与同一个 type record cache；11-S3E 已提供 `ZrCore_MetadataRuntime_ResolveFieldRecord()`，按 `MEMBER_DEF` / `MEMBER_REF` record 解析字段 token 并使用独立 field cache；11-S3F 已提供 zrp data metadata mmap buffer/header attach 与 section-view 查询；11-S3G 已提供 entity token 到 validated signature blob pool slice 的查询；11-S3H 已提供 method/field signature 顶层 header view；11-S3I 已提供 nested signature type-node view（node/payload/base/child/next offsets）；11-S3J 已提供 generic TypeSpec signature view（TypeSpec/signature identity + GENERIC_INST base/argument offsets）；11-S3K 已提供 generic TypeSpec base-token binding view（base `TYPE_REF/TYPE_DEF` node 匹配现有 type record signature blob 并暴露 base token/record）；11-S3L 已提供 indexed generic argument binding view（argument type-node + direct `TYPE_REF/TYPE_DEF` argument token/record binding）；11-S3M 已提供 MethodSpec signature view（`SIGNATURE` token + `GENERIC_INST(MEMBER_REF methodToken, args...)` + method record binding），10-S3C 已把 MethodSpec signature record/hash 接入 public method signature carrier，10-S3E 已让 public MethodSpec carrier 用该 underlying method token 读取 11-S2D AOT binding；recursive generic argument semantic binding、method instantiation materialization、row-to-entity materialization、token→运行期实体物化和完整缓存仍待后续 |
| 11-S4 | token↔cTypeId↔ZrLayout 三向表（§4） | 🚧 2026-07-01 部分完成：11-S4A..11-S4R/11-S4R-union 已完成 TypeDef/TypeSpec/FieldDef registry-backed layout binding views、runtime layout/GC resolver、bounded token/layout cache、code-registration typeLayout token carrier、generated TypeDef/TypeSpec token population 和 ownership offset table；10-S4F..10-S4Z3/11-S4T..11-S4AO 已让最小 public `FieldInfo` object 消费 FieldDef binding view、TypeDef binding view、zrp string pool、raw FieldDef flags/signature coordinates、validated FIELD_SIG header/type-node view、primitive/direct TypeDef/bound TypeRef signature carrier、layout consistency、signature node object、base type-node object、child type-node object list、primitive child semantic name、direct TypeDef base/child semantic token/layout/name、direct TypeRef child type-node semantic token/layout/name 和 recursive signature node type literal object；10-S4Z4..10-S4Z12/11-S4AP..11-S4AX 已让 FieldDef layout/signature metadata 支撑 same-runtime `VALUE_SLOT` inline read/write、primitive POD int32 raw inline read/write boundary、bool/uint32/double representative raw inline read/write matrix、int8/int16/int64/uint8/uint16/uint64/float32 storage-width raw inline read/write matrix，以及 integer range guard、float32 range guard、float32 NaN guard 和 float32 precision guard；10-S4Z13..10-S4Z28/11-S4AY..11-S4BN 已让 FieldInfo object 携带 attached metadata runtime native-pointer carrier，并用 object-level adapter 覆盖 `VALUE_SLOT` 与 primitive POD raw inline read/write、inline aggregate borrowed view/source write、nested owned value replacement/drop、单级 nested VALUE_SLOT child read/write、第一条 multi-level nested VALUE_SLOT path read/write，以及第一条 multi-level nested primitive POD raw child path read/write，并补充 nested primitive leaf layout identity guard 和 representative bool/uint32/double path matrix coverage 和 storage-width int8/int16/int64/uint8/uint16/uint64/float32 path matrix coverage。持久 cTypeId→token 索引内容、完整 TypeSpec/generic layout materialization、运行期 layout 构建、recursive/signature-derived field type binding、TypeRef/跨模块 provider signature binding、完整 primitive raw child matrix/signature-derived binding 和 managed object-level FieldInfo method surface 仍待后续 |
| 11-S5 | 泛型参数标准化编码（GENERIC_INST/MethodSpec/约束）（§5） | 🚧 2026-06-30 部分完成：已新增 GenericParam/GenericParamConstraint attached zrp runtime 只读 view，按 TypeDef/MethodDef owner range 校验泛型参数定义，暴露参数 name/flags/constraint range，并按 index 读取约束 type token/record 与可选 validated signature blob；10-S4D 已把这些 view 接入 public reflection carrier，可按 owner+parameter/constraint index 暴露 GenericParam 与 GenericParamConstraint 信息；11-S5/10-S4E 已提供 MethodSpec indexed generic argument runtime view 和 public reflection carrier，可从 `GENERIC_INST(MEMBER_REF methodToken, args...)` 读取 primitive 或 direct TypeDef/TypeRef 实参；10-S3B 已让 MethodSpec signature view 携带 MethodSpec record 并接入 public `ResolveToken()` method-like carrier；10-S3E 已让 MethodSpec public carrier 复用 underlying MethodDef 的 AOT binding carrier；与 08 实例化去重键的全链路统一、运行期泛型实体/layout 构建和 public generic reflection object 仍待后续 |
| 11-S6 | 版本检查运行实现 + 不匹配 deopt/拒绝（§6） | 🚧 2026-06-28 部分完成：11-S6A 已提供 `ZrCore_MetadataRuntime_CheckTokenBindingCompatibility()`，统一检查 token binding 的版本区间、module signature hash、metadata/signature token、signature hash、layoutVersion/layoutHash，并返回可供 dynamic 拒绝或 typed deopt 消费的 status/report；11-S6B 已提供 `ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility()`，可扫描 attached function 的 `moduleMetadataBindings` 并返回首个不兼容 binding/ref record/report；11-S6C 已将 function scan 接入 root AOT dynamic module loader，在 embedded/zro metadata binding 不兼容时拒绝加载并输出状态名/token/hash/layout 诊断；11-S6D 已为 i64 scalar typed direct-call 生成 runtime metadata guard，caller/callee binding 不兼容时经 `ZrLibrary_AotRuntime_DeoptTypedDirectCall()` 退回 `CallStackValue()` 并同步 i64 scalar local；11-S6E 已将同一 guard/deopt 形态扩展到 u64 scalar typed direct-call，兼容时保留 unsigned direct thunk，不兼容时同步 u64 scalar local；11-S6F 已将同一 guard/deopt 形态扩展到 f64 scalar typed direct-call，兼容时保留 float direct thunk，不兼容时同步 f64 scalar local；11-S6G 已将同一 guard/deopt 形态扩展到 bool-result typed direct-call，覆盖 bool/bool 与 i64/u64/f64 比较返回 bool，兼容时保留 bool direct thunk，不兼容时同步 bool scalar local；11-S6H 已将同一 guard/deopt 形态扩展到 value SemIR inline-struct `CALL_TYPED`，覆盖 ordinary direct call、shared generic METHOD-slot call 和 full-AOT collected shared callsite，兼容时保留 `CallInlineStruct()`，不兼容时经 `CallInlineStructDynamicDeoptBridge()` 回解释器并复制 inline return bytes；跨模块 token resolve 集成和更完整 no-crash drift 注入仍待后续 |
| 11-S7 | 元数据策略（默认最小）+ zrp manifest + 工具（§7/§8） | 🚧 2026-06-27 部分完成：11-S7A 已完成 `.zrp` project manifest normalization 的 `manifestVersion` 门禁，以及旧 `dependencies.$alias` 与新 `references.alias` 同值去重/冲突拒绝；11-S7B 已完成 assembly `publicKeyToken` 十六进制校验与小写归一化；11-S7C 已完成 legacy top-level `name`/`version` assembly identity shape gate 与 schema parity；11-S7D 已完成 legacy `dependencies.$alias.{assembly|name}` 声明 assembly 映射、目标 manifest identity mismatch 拒绝、alias package key 保持与 AssemblyRef identity 查询暴露；11-S7E 已完成 `.zrp` manifest `preserve` 规则的 declaration-level 解析、project model 暴露与 schema parity；11-S7F 已完成 `.zrp` manifest `aotMode` declaration-level 解析、project model 暴露与 schema parity；11-S7G 已完成 project `aotMode` 到 `SZrAotWriterOptions.requireFullAot` 的 CLI/compiler helper 注入；11-S7H 已完成 CLI `--emit-aot-c` AOT C 发射入口、binary input embedded blob 和 manifest v3 `aot_c` path tracking；11-S7I 已完成 `preserve` 的 `method` target 到 entry function top-level callable flat index 的绑定，并注入 writer manifest roots；11-S7J 已支持 dotted method target 精确匹配 callable name，并把 `type` preserve 的 `members: "methods"` / `"all"` 展开为同名前缀 callable roots；11-S7K 已为 `preserve` 规则添加 `feature` + boolean `featureValue` 条件声明模型、互相依赖校验与 schema parity；11-S7L 已新增 `.zrp` top-level `features` boolean switch map，并让 CLI AOT preserve root 注入按 feature 条件匹配启停；11-S7M 已为 `generic` preserve 添加非空 `arguments` 声明模型、project model 承载和 schema parity；11-S7N 已把 generic preserve target+arguments 注入 AOT writer options 并在 generated C 清单中输出；11-S7O 已把当前模块已有 `GENERIC_INST` `TYPE_SPEC` 记录绑定回 generic preserve root 的 TypeSpec/signature token/hash；11-S7P 已让 full-AOT writer 拒绝未绑定 TypeSpec 的 manifest generic preserve root；11-S7Q 已把 TypeSpec-backed generic preserve root 物化为 generic instantiation identity 并输出 base token / instance id / share kind；11-S7R 已让 full-AOT writer 拒绝 TypeSpec-only generic preserve root，要求 generic instantiation identity 同步存在；11-S7S 已让 current-module TypeSpec-backed generic instantiation 在存在同名 `TYPE_REF` metadata 时使用 open generic base token，并保留 closed TypeSpec 回退；11-S7T 已支持 `GENERIC_INST(TYPE_DEF target, args...)` TypeSpec binding，并让 current-module TypeDef base token 进入 generic instantiation identity；11-S7U 已在 manifest generic root 缺失 TypeSpec 但存在同名 open `TYPE_DEF`/`TYPE_REF` metadata 时合成 current-function TypeSpec/signature binding并继续物化 generic instantiation identity；11-S7V 已把 manifest generic method root 绑定到现有 current-module `GENERIC_INST(MEMBER_REF methodToken, args...)` MethodSpec 形态签名并输出 MethodSpec identity；11-S7W 已提供 CLI `--dump-zrp-metadata <file>` 只读工具，可输出 zrp metadata version/headerBytes/sectionCount 以及 12 个 section 的 bytes/count/elementSize/offset summary；11-S7X 已提供 CLI `--diff-zrp-metadata <before> <after>` 只读工具，可输出 zrp metadata version/headerBytes/sectionCount before/after 与 12 个 section 的 bytes/count before/after/removed diff summary；11-S7Z 已提供 `.zrp` `exports` 的 `type`/`method`/`field` declaration input；11-S7ZA 已把 project export declarations 注入 `SZrAotWriterOptions.manifestExportDeclarations` 并在 generated C 输出 `manifest.exports` / `manifest.export[i]` 诊断；11-S7ZB/11-S7ZC/11-S7ZD 已把 current-module type/method/field export declaration 分别绑定到 `TYPE_DEF` token 或 typed exported function/variable 的 `MEMBER_DEF` token，并输出 `typeToken` / `memberToken` 诊断；12-S7Y 已接入 generated MethodInfo 默认最小策略，opt-in code stripping 下输出 `reflectionMetadataLevel = NONE` 与 `metadata_policy.reflectionLevel` marker，默认/非裁剪仍保留 `RUNTIME_MAPPING`；12-S7Z..12-S7ZN 已逐步启用 emitted zrp metadata 的 MethodDef、token-record、FieldDef、GenericParam、GenericParamConstraint、MethodSpec method-token 剪枝级联，retained signature blob pool compaction/offset remap/hash recomputation/MethodSpec signature rewrite，string-pool retained-slice sweep、row offset remap 与 duplicate retained slice interning，当前 orphan constant-pool sweep，让 pool compaction 在 MethodDef count 不变时仍可触发，并提供 exported MethodDef/FieldDef member token 的 compacted-token remap surface；持久 export manifest/table writer、compacted-token file publication、cross-module provider loading/version binding、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer 仍待后续 |

> 2026-07-19 03:53:01 +08:00 状态补记：11-S5A 已公开 GenericParam owner range，并由 10-S4Z43
> 消费为 public generic method definition/parameter objects；11-S5 仍开放 type-argument -> MethodSpec 匹配、
> constructed method entity、运行期 generic layout 与实例化去重键全链路统一。

> 2026-07-01 14:51:47 +08:00 状态补记：12-S7ZV 已关闭上表 11-S7 当前摘要中的
> FieldDef default-value backed constant-pool remap 缺口；仍未关闭的是跨模块 target、
> cross-module export-token publication/rewrite、完整 zrp metadata sweep/pruning 和后续版本/ABI 漂移闭环。

> 2026-07-02 03:24:57 +08:00 状态补记：11-S7ZE/12-S7 已关闭 generated-C
> `SZrAotManifestExportEntry` 表发布、descriptor/codeRegistration wiring、runtime table validation，以及 method/field
> export bound `MEMBER_DEF` token 的 compacted remap 写入；仍未关闭的是 cross-module provider loading/version
> binding、standalone provider manifest consumption、完整 metadata sweep/pruning、full trim analyzer 和版本/ABI 漂移闭环。

> 2026-07-02 03:57:34 +08:00 状态补记：11-S7ZF/12-S7 已关闭 attached `SZrMetadataRuntime`
> 对 codeRegistration `manifestExports/manifestExportCount` 的 runtime mirror，manifest export table 现在可在模块 attach
> 后经 metadata runtime 只读消费；仍未关闭的是 cross-module provider loading/version binding、standalone provider
> import-path manifest consumption、完整 metadata sweep/pruning、full trim analyzer 和版本/ABI 漂移闭环。

> 2026-07-02 04:23:15 +08:00 状态补记：11-S7ZG/12-S7 已关闭 attached manifest export table 的 runtime
> view/query API：`ZrCore_MetadataRuntime_ReadManifestExportView()` 可按 `kind + target` 唯一读取导出 entry 和
> type/member token，并对重复声明、缺 required token 或 token shape mismatch fail closed；仍未关闭的是
> cross-module provider loading/version binding、standalone provider import-path wiring、完整 metadata sweep/pruning、
> full trim analyzer 和版本/ABI 漂移闭环。

> 2026-07-02 04:46:47 +08:00 状态补记：11-S7ZH/12-S7 已关闭 attached manifest export view 到
> binding compatibility 的本地 gate：`ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility()` 先按
> `kind + target` 读取 manifest export，再复用 token/signature/module/layout 兼容性检查，最后要求 binding 的
> resolved metadata token 与导出表发布的 type/member token 一致；仍未关闭的是 cross-module provider
> loading/version binding、standalone provider import-path wiring、完整 metadata sweep/pruning、full trim analyzer
> 和版本/ABI 漂移闭环。

> 2026-07-02 05:58:12 +08:00 状态补记：11-S7ZJ/12-S7 已关闭 standalone provider
> import-path location discovery：`ZrLibrary_Project_ResolveImportProviderLocation()` 可把 raw import specifier
> 解析为 canonical `$alias@version/module` key、declared assembly/version range，以及 `.zrm` archive entry 或
> `.zrp` source/binary/intermediate provider paths；仍未关闭的是 provider runtime loading、multi-version selection、
> export metadata attach、完整 metadata sweep/pruning、full trim analyzer 和版本/ABI 漂移闭环。

> 2026-07-02 06:13:21 +08:00 状态补记：11-S7ZK/12-S7 已关闭 standalone provider
> AOT load-request planning：`ZrLibrary_Project_ResolveImportProviderAotLoadRequest()` 可把 provider location
> 转成 backend、canonical module key、descriptor-local module name、`.zrp` provider AOT library path，或 `.zrm`
> archive/entry view；仍未关闭的是 provider runtime dynamic loading、multi-version selection、export metadata attach、
> 完整 metadata sweep/pruning、full trim analyzer 和版本/ABI 漂移闭环。

> 2026-07-02 06:41:32 +08:00 状态补记：11-S7ZL/12-S7 已关闭 strict AOT runtime 对 standalone provider
> load request 的消费入口：canonical `$alias@version/module` 会通过
> `ZrLibrary_Project_ResolveImportProviderAotLoadRequest()` 映射到 provider `.zrp` 的 source/binary/library path，
> descriptor 校验使用 provider-local module name；`.zrm` archive entry 在 runtime 侧 fail-closed。仍未关闭的是
> provider 动态库成功加载端到端、multi-version selection、export metadata attach、完整 metadata sweep/pruning、
> full trim analyzer 和版本/ABI 漂移闭环。

> 2026-07-02 07:01:13 +08:00 状态补记：11-S7ZM/12-S7 已补齐 `.zrp` provider AOT 动态库成功路径验收：
> 新增 smoke fixture 会生成 provider `.zr`、`.zro`、AOT C 与 `zrvm_aot_ops_sum.so`，再在 strict AOT C 下导入
> canonical `$mathLocal@2.1.0/ops/sum`，确认 descriptor 使用 provider-local `ops/sum`，module cache 使用 canonical key，
> executed-via 为 AOT C，且 provider export 正常发布。仍未关闭的是 multi-version selection、export metadata attach、
> 完整 metadata sweep/pruning、full trim analyzer 和版本/ABI 漂移闭环。

> 2026-07-02 07:25:55 +08:00 状态补记：11-S7ZN/12-S7 已关闭 standalone provider exact alias/version
> selection 与 declared strict semver range guard：同一 root manifest 可同时引用同一 assembly 的 2.1.0 与 3.1.0
> provider 并按 alias/version 精确解析 AOT load request；`.zrp`/`.zrm` provider reference 的 strict
> `major.minor.patch` declared range 不包含 actual version 时 manifest parse fail closed。仍未关闭的是 automatic
> range-based candidate selection、export metadata attach、完整 metadata sweep/pruning、full trim analyzer 和版本/ABI
> 漂移闭环。

## 10. 不变量校验

- **C 单一真相**：token↔layout 三向表是偏移/大小/签名的唯一来源；反射/泛型/GC/序列化全部经它。
- **B 纯降级**：元数据是数据 + 边界能力，typed 函数体不读元数据（纯标量函数 `methodInfo` 都不读，`07`§4.1）。
- 与 `12` 协同：元数据生成量由可达性 + 注解决定，二者是同一裁剪管线的输出。
- 11-S4BG / 10-S4Z21 已让 retained FieldDef layout/signature metadata 驱动第一条 recursive nested field consumer：
  runtime reflection 通过 FieldInfo identity 恢复 `metadataRuntime` 与 FieldDef token，复用 `FIELD_SIG(TYPE_DEF/TYPE_REF)`
  type-node validation 和 resolved field type layout，然后按 `SZrTypeLayoutField` index 读取 nested `VALUE_SLOT` child。
  该消费边界不新增 metadata row、token ABI、code-registration ABI 或 provider ABI；完整 recursive binding、cross-module
  FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
- 11-S4BH / 10-S4Z22 已让同一 retained FieldDef layout/signature metadata 驱动第一条 recursive nested field write consumer：
  runtime reflection 继续通过 FieldInfo identity 和 FieldDef token 绑定 owner field、signature type-node 与 resolved
  field type layout，按 `SZrTypeLayoutField` index 写入 nested `VALUE_SLOT` child，并复用 `ZrCore_Value_Copy()` 的
  replacement/drop 语义。该 consumer 仍不新增 metadata row、token ABI、code-registration ABI 或 provider ABI；多级
  recursive path、primitive raw child binding、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
- 11-S4BI / 10-S4Z23 已让同一 retained FieldDef layout/signature metadata 驱动第一条 multi-level recursive nested field
  path read consumer：runtime reflection 通过 FieldInfo identity 和 FieldDef token 恢复 outer owner field、validated
  `FIELD_SIG(TYPE_DEF/TYPE_REF)` 与 resolved field type layout，随后逐级读取 `SZrTypeLayoutField.typeLayoutIndex` 并经
  `ZrCore_MetadataRuntime_ResolveTypeLayout()` 解析 child layout，最后只在 leaf `VALUE_SLOT` child 上物化 `SZrTypeValue`。
  该 consumer 不新增 metadata row、token ABI、code-registration ABI 或 provider ABI；nested path write、primitive raw
  child binding、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
- 11-S4BJ / 10-S4Z24 已让同一 retained FieldDef layout/signature metadata 驱动第一条 multi-level recursive nested field
  path write consumer：runtime reflection 通过 FieldInfo identity 和 FieldDef token 恢复 outer owner field、validated
  `FIELD_SIG(TYPE_DEF/TYPE_REF)` 与 resolved field type layout，随后逐级读取 `SZrTypeLayoutField.typeLayoutIndex` 并经
  `ZrCore_MetadataRuntime_ResolveTypeLayout()` 解析 child layout，最后只在 leaf `VALUE_SLOT` child 上写入 `SZrTypeValue`。
  该 consumer 复用 `ZrCore_Value_Copy()` 的 replacement/drop 语义，不新增 metadata row、token ABI、code-registration
  ABI 或 provider ABI；primitive raw child binding、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
- 11-S4BK / 10-S4Z25 已让同一 retained FieldDef layout/signature metadata 驱动第一条 multi-level recursive nested
  primitive raw child path read/write consumer：runtime reflection 通过 FieldInfo identity 和 FieldDef token 恢复 outer
  owner field、validated `FIELD_SIG(TYPE_DEF/TYPE_REF)` 与 resolved field type layout，随后逐级读取
  `SZrTypeLayoutField.typeLayoutIndex` 并经 `ZrCore_MetadataRuntime_ResolveTypeLayout()` 解析 child layout，最后在 leaf
  raw child 上复用 shared primitive POD raw load/store guard。该 consumer 不新增 metadata row、token ABI、code-registration
  ABI 或 provider ABI；完整 primitive width/signature-derived matrix、cross-module FieldRef/TypeRef provider 和 metadata
  sweep 仍未关闭。
- 11-S4BL / 10-S4Z26 已让 retained FieldDef layout consumer 在 nested primitive leaf 上区分 raw child 与 inline aggregate
  child：leaf `SZrTypeLayoutField.typeLayoutIndex` 现在必须是 `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`，否则 read/write
  均拒绝并保留原始字节。该 consumer 仍不新增 metadata row、token ABI、code-registration ABI 或 provider ABI；完整
  primitive width/signature-derived matrix、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
- 11-S4BM / 10-S4Z27 已补齐 nested primitive path 的代表性 metadata consumer 覆盖：同一 retained FieldDef
  layout/signature consumer 现在用 focused fixture 验证 bool、uint32 和 double raw child path read/write。该 coverage GREEN
  不新增 metadata row、token ABI、code-registration ABI 或 provider ABI；完整 primitive width/signature-derived matrix、
  cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
- 11-S4BN / 10-S4Z28 已补齐 nested primitive path 的 storage-width metadata consumer 覆盖：同一 retained FieldDef
  layout/signature consumer 现在用 focused fixture 验证 int8、int16、int64、uint8、uint16、uint64 和 float32 raw child
  path read/write。该 coverage GREEN 不新增 metadata row、token ABI、code-registration ABI 或 provider ABI；完整
  signature-derived binding、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。

> 2026-07-02 09:15:26 +08:00 状态补记：11-S7ZR/12-S7 已关闭 range-selected provider
> runtime export publication 支撑切片：同一 provider dynamic library 现在可经 exact alias/version 与 candidate-selected
> alias 同时被 strict AOT C runtime 导入，并为两条 alias 都发布普通 public export 与 attached manifest export metadata。
> 仍未关闭的是完整 metadata sweep/pruning、compacted-token file publication、完整 trim analyzer、annotation/promotion
> 策略和更完整的 ABI drift/deopt 闭环。

> 2026-07-02 09:38:04 +08:00 状态补记：11-S7ZSA/12-S7ZZR 已关闭 manifest export table
> builder 的 kind/token shape guard：type export 拒绝 `MEMBER_DEF` binding，method/field export 拒绝
> `TYPE_DEF` binding；未绑定 declaration 与 provider method/field member-token remap 路径保持通过。仍未关闭的是
> 持久 `.zrp` manifest export section、完整 metadata sweep/pruning、完整 trim analyzer 与更完整的 ABI drift/deopt
> 闭环。

## 状态与产出记录

> 落地每个阶段或切片时在此追加：时间戳 · 切片号 · 状态 · 完成项目 · RED/GREEN · 测试结果 · 备注。

- 2026-07-19 03:53:01 +08:00 · 11-S5A / 08-S6V / 10-S4Z43 GenericParam owner range and method definition object ·
  状态：11-S5 runtime generic metadata 子切片完成；完整 11-S5、08-S6 与 10-S4 仍为部分完成。完成项目：
  新增 `SZrMetadataRuntimeGenericOwnerView` 与 public reader，直接复用原私有 TypeDef/MethodDef range resolver，
  暴露 owner record、definition row、first index/count 并统一失败清零；现有 GenericParam reader 改为复用该入口。
  反射 builder 据此严格逐项读取并验证 owner/range/index 后物化方法定义对象，名称来自现有 string pool。
  RED 为 metadata owner reader 与 reflection builder 两个 unresolved symbols；首轮 GREEN metadata query 25/0、
  dynamic reflection 25/0。真实 string-pool name review RED 25/1 后最终 GREEN 25/0。GCC/Clang/MSVC 聚焦 CTest
  各 6/6，GC 66/0、指令执行 31/0、指令表 95/0；本切片实现源诊断为空。产出：
  `tests/acceptance/2026-07-19-aot-08-s6v-10-s4z43-11-s5a-generic-method-definition-object.md`。
  分层记录：`docs/plans/aot/07-12-codegen/2026-07-19-08-s6v-10-s4z43-11-s5a.md`。
  备注：zrp section/row 与 registration ABI 均不变，不创建/缓存 MethodSpec，不声明完整 11-S5 或脚本
  `MakeGenericMethod` 完成。

- 2026-07-19 02:56:02 +08:00 · 11-S2E / 08-S6U / 10-S4Z42 attached MethodDef interpreter VM binding ·
  状态：11-S2 runtime binding 子切片完成；完整 11-S2、08-S6 与 10-S4 仍为部分完成。完成项目：新增
  `SZrMetadataRuntimeInterpreterMethodBindingView` 与 public reader，扫描 attached zrp MethodDef rows 并拒绝重复
  token，联合现有 method record 和 MethodDef `functionIndex`，通过新 function-graph resolver 返回本地 VM function。
  resolver 与 AOT function table 使用同一 root/constant/child DFS 和 identity 去重规则，动态 visited storage 不由
  元数据 index 决定容量。只接受 non-native、instruction-backed function；所有失败先清 output。反射层据此把
  MethodSpec underlying method token 自动绑定并执行。RED 为两个缺失 API 的 MSVC link failure；GREEN 为动态
  泛型反射 24/0。最终 GCC/Clang/MSVC 聚焦 CTest 各 5/5，GC 66/0、指令执行 31/0、指令表 95/0，
  本切片源诊断为空。产出：
  `tests/acceptance/2026-07-19-aot-08-s6u-10-s4z42-11-s2e-methodspec-method-token-vm-function-resolution.md`。
  备注：复用现有 zrp MethodDef row 和函数图，不改 metadata/code-registration ABI，不声明跨模块 method binding、
  MethodSpec 专用 code slot 或完整 11-S2 完成。

- 2026-07-19 01:39:29 +08:00 · 11-S6J / 08-S6T / 10-S4Z41 canonical TypeSpec RID mapping compatibility ·
  状态：11-S6 runtime binding compatibility 子切片完成；完整 11-S6/08-S6/10-S4 仍为部分完成。完成项目：
  `ZrCore_MetadataRuntime_CheckTokenBindingCompatibility()` 现在识别 binder 已有的 canonical TypeSpec->TypeSpec
  RID remap：仅当 expected/resolved metadata token 同为 TypeSpec、expected/resolved paired token 同为 Signature
  时跳过 token RID equality，module version/hash、signature hash 与 layout identity 检查继续执行。cross-module
  reflection consumer 进一步解析两侧 records/views 并做 canonical signature exact-byte equality，防止缓存 hash 未变的
  provider blob 漂移，并要求 binding `ref*`/`expected*` 三元组都精确对应 requester。RED 为 compatibility 17/1，返回 metadata-token mismatch；GREEN 为 17/0。consumer 的
  same-hash signature drift RED 为 24/1，exact-byte gate 后 GREEN 24/0。WSL GCC、Clang、MSVC 聚焦 CTest 各 4/4，
  GC 66/0、指令执行 31/0、指令表 95/0；变更文件诊断扫描为空。产出：
  `tests/acceptance/2026-07-19-aot-08-s6t-10-s4z41-11-s6j-bound-provider-generic-typespec-identity.md`。
  备注：本切片不改变 binding/zrp/registration ABI，不为 MemberRef/MethodSpec 等其他 token mapping 放宽规则，
  也不声明 provider 自动加载或完整 ABI drift/deopt 闭环完成。

- 2026-07-18 18:56:26 +08:00 · 11-S4BO / 08-S6K / 10-S4Z32 signature type-node record binding ·
  状态：11-S4 metadata binding 子切片完成；完整 11-S4、10-S4 与 08-S6 仍为部分完成。完成项目：新增 public
  signature-node record resolver，按完整节点跨度解析 attached direct TypeDef/TypeRef 与本地 nested TypeSpec record；
  TypeSpec/MethodSpec direct argument binding 复用该单一入口，token-only public generic object 消费它递归物化全部当前
  compound nodes。模块拆分后 `metadata_runtime.c` 从 1036 行降至 979 行，新 binding module 为 98 行。
  RED/GREEN：consumer RED 为 15 tests/2 failures，GREEN 为 15/0。WSL GCC 11.4、Clang 14.0、MSVC 19.44
  的 `reflection_dynamic_generic_instance|metadata_runtime_typespec_layout|reflection_token_resolve` CTest 均 3/3，
  binding/object modules 无 GCC/Clang 自身告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6k-10-s4z32-11-s4bo-token-only-compound-generic-type-object.md`。
  备注：本入口不做跨模块 canonical identity、provider remap 或动态 layout construction，不关闭完整 11-S4。

- 2026-07-03 23:36:13 +08:00 · 11-S7ZTB / 12-S7ZZZU CLI export duplicate bridge guard ·
  状态：11-S7 CLI writer bridge 内存态 export declaration 唯一性 guard 完成；07~12 总目标继续进行中，完整 metadata
  sweep/pruning、完整 trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt coverage 仍待后续。
  完成项目：`ZrCli_Compiler_ApplyProjectAotExportDeclarations()` 现在在复制 project export declarations 到 writer options
  前拒绝重复 `kind + target`，失败时释放 CLI-owned scratch storage，并保持 `manifestExportDeclarations` 为空，避免绕过
  project parser/schema 后把歧义 export set 送入 AOT writer。
  RED/GREEN：RED 为 `test_cli_aot_writer_options_rejects_duplicate_manifest_export_declarations` 失败
  `Expected FALSE Was TRUE`；GREEN 后 WSL GCC focused direct run 19/0。
  测试结果：WSL GCC、WSL Clang、Windows MSVC Debug 均通过 `zr_vm_cli_aot_writer_options_test` 19/0、
  `zr_vm_project_manifest_normalization_test` 29/0、`aot_c_zrp_metadata_export_token_remap|metadata_runtime_manifest_exports`
  CTest 2/2、`zr_vm_aot_c_source_contracts_test` 24/0；`git diff --check` 通过（仅既有 LF/CRLF 提示），代码文件尾随空白扫描为空。
  产出：`tests/acceptance/2026-07-03-aot-11-s7ztb-12-s7zzzu-cli-export-duplicate-bridge-guard.md`。
  备注：本切片只关闭 CLI writer bridge 的重复 export declaration fail-closed 缺口；项目解析、schema、generated table
  已分别有独立记录，完整 11-S7、12-S7 或 07~12 总目标仍未完成。
- 2026-07-03 23:20:20 +08:00 · 11-S7ZTA / 12-S7ZZZT project export schema uniqueItems parity ·
  状态：11-S7 `.zrp` schema export declaration 基础重复对象约束完成；07~12 总目标继续进行中，完整 metadata
  sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`zrp.schema.json` 的 `exports` array 现在声明 `uniqueItems: true`，让完全重复的 export
  declaration 对象在编辑器/schema 层先被拒绝，并与 project parser/generated table 的重复导出 fail-closed 方向保持一致。
  RED/GREEN：RED 为 schema 断言 `properties.exports.uniqueItems == true` 失败 `AssertionError`；GREEN 后同断言通过。
  测试结果：`python -c` schema uniqueItems 断言通过；`python -m json.tool zrp.schema.json` 通过。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zta-12-s7zzzt-project-export-schema-unique-items.md`。
  备注：本切片只关闭 schema 层完全重复 export object parity；JSON Schema 仍不表达“同 kind+target 但额外字段不同”的完整唯一性，
  该约束由 project parser/generated table 继续 fail-closed。
- 2026-07-03 23:14:39 +08:00 · 11-S7ZSZ / 12-S7ZZZS project manifest export duplicate target guard ·
  状态：11-S7 project manifest `exports` 上游唯一性 guard 完成；07~12 总目标继续进行中，完整 metadata
  sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`library_project_parse_export_declarations()` 在解析每个 export 声明时扫描已解析条目，
  同一 `kind + target` 重复声明会让项目加载 fail closed，避免歧义声明进入 CLI writer bridge 和
  generated manifest export table。
  RED/GREEN：RED 为新增重复 `method`/`Widget.run` manifest fixture 后，WSL GCC
  `zr_vm_project_manifest_normalization_test` 失败 `Expected NULL`；GREEN 后 project manifest normalization 29/0 通过。
  测试结果：WSL GCC focused project direct 29/0；WSL GCC 下游 CTest
  `aot_c_zrp_metadata_export_token_remap|metadata_runtime_manifest_exports` 2/2 与 source contracts 24/0 通过；
  WSL clang/MSVC Debug 同组 project direct 29/0、下游 CTest 2/2、source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsz-12-s7zzzs-project-export-duplicate-target-guard.md`。
  备注：本切片只关闭 project manifest export duplicate kind+target parser guard 缺口；不声明完整 11-S7、
  12-S7 或 07~12 总目标完成。
- 2026-07-03 23:00:31 +08:00 · 11-S7ZSY / 12-S7ZZZR manifest export duplicate kind-target guard ·
  状态：11-S7 generated manifest export table 唯一性 guard 完成；07~12 总目标继续进行中，完整 metadata
  sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`backend_aot_c_zrp_manifest_export_table_build()` 在写入条目前扫描已写 manifest export
  entries，遇到相同 `kind + target` 声明时 fail closed 并保持 metadata table 为空，和
  `ZrCore_MetadataRuntime_ReadManifestExportView()` 的运行时重复目标拒绝语义对齐。
  RED/GREEN：RED 为新增 duplicate `METHOD + "Factory.make"` declaration fixture 后，WSL GCC
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 失败 `Expected FALSE Was TRUE`，证明旧生成端会接受歧义导出；
  GREEN 后 export-token remap focused fixture 11/0 通过。
  测试结果：WSL GCC/clang/MSVC Debug CTest
  `aot_c_zrp_metadata_export_token_remap|metadata_runtime_manifest_exports|aot_c_zrp_metadata_pruning`
  均 3/3；同三套直跑 source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsy-12-s7zzzr-manifest-export-duplicate-target-guard.md`。
  备注：本切片只关闭 generated manifest export duplicate kind+target guard 缺口；不声明完整 11-S7、
  12-S7 或 07~12 总目标完成。
- 2026-07-03 22:45:34 +08:00 · 11-S7ZSX / 12-S7ZZZQ writer sidecar definition-table validation ·
  状态：11-S7 final compacted `.zrp` metadata sidecar 发布边界完成 header + definition-table fail-closed 校验；
  07~12 总目标继续进行中，完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt 闭环仍待后续。
  完成项目：`backend_aot_c_publish_compacted_zrp_metadata()` 在写 sidecar 前复用
  `ZrCore_ZrpMetadata_ValidateDefinitionTables()`，直接 writer 传入无效 definition tables 时返回失败并删除同路径旧
  sidecar，避免仅 header 可读的无效 `.zrp` 被当作最终 compacted metadata 发布。
  RED/GREEN：RED 为新增 direct writer invalid definition-table sidecar fixture 后，WSL GCC
  `aot_c_zrp_metadata_publication` 失败 `Expected FALSE Was TRUE`，证明旧 helper 会留下/发布无效 sidecar；
  GREEN 后 WSL GCC focused publication 2/0 通过。
  测试结果：WSL GCC/clang/MSVC Debug CTest
  `aot_c_zrp_metadata_publication|cli_aot_compacted_metadata_sidecar|aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_methodspec_pruning|aot_c_zrp_metadata_pool_pruning`
  均 5/5；同三套直跑 source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsx-12-s7zzzq-zrp-sidecar-definition-table-validation.md`。
  备注：本切片只关闭 writer-level compacted `.zrp` sidecar 文件级 validation/stale cleanup 缺口；不声明完整 11-S7、
  12-S7 或 07~12 总目标完成。
- 2026-07-03 22:23:50 +08:00 · 11-S7ZSW / 12-S7ZZZP retained signature UNION string-pool sweep/remap ·
  状态：11-S7 emitted metadata 中 retained signature `UNION` base-name string 保留与重写完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt 闭环仍待后续。
  完成项目：retained signature string-pool sweep 现在把 `UNION(valueType, baseNameStringOffset, args...)`
  的 base-name offset 纳入 compacted string roots；signature blob copy 后同一 `UNION` payload 通过 compacted
  string remap 重写，避免 retained MethodSpec signature 指向被剪掉的旧 string-pool offset。
  RED/GREEN：RED 为新增 MethodSpec union fixture 后 WSL GCC 失败 `Expected 780 Was 773`，暴露旧 sweep
  少保留 `"Option"` 字符串；GREEN 后 WSL GCC/clang 与 Windows MSVC Debug 同组回归通过。
  测试结果：WSL GCC/clang/MSVC Debug CTest
  `aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_methodspec_pruning|aot_c_zrp_metadata_typespec_pruning|aot_c_zrp_metadata_module_ref_pruning|aot_c_zrp_metadata_pool_pruning`
  均 5/5；同三套直跑 TypeDef pruning 2/0、source contracts 24/0；`git diff --check` 仅有 LF/CRLF 提示，尾随空白扫描干净。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsw-12-s7zzzp-signature-union-string-remap.md`。
  备注：本切片只关闭 retained signature `UNION` base-name string retention/remap 缺口；不声明完整 11-S7、12-S7
  或 07~12 总目标完成。
- 2026-07-03 22:04:51 +08:00 · 11-S7ZSV / 12-S7ZZZO ModuleRef retained-row generic context ·
  状态：11-S7 emitted metadata 中 ModuleRef retained-row generic context 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt 闭环仍待后续。
  完成项目：ModuleRef retained/count/compact/remap helpers 现在贯穿 `GenericParam` 与
  `GenericParamConstraint` context；ModuleRef root 扫描在判断引用它的 TypeRef/MemberRef token record 是否 retained 时，
  会按同一上下文重映射 TypeSpec token。这样通过 GenericParamConstraint 保留的 TypeSpec 可以继续保留对应
  AssemblyRef/ModuleRef row，并把 token record 的 `relatedToken` 改写到 compacted AssemblyRef RID。
  RED/GREEN：RED 为新增 ModuleRef pruning fixture 后 WSL GCC 失败
  `Expected 1 Was 0`，暴露旧 ModuleRef retained predicate 在 TypeSpec generic-constraint retention 路径中传空上下文；
  GREEN 后 WSL GCC/clang 与 Windows MSVC Debug 同组回归通过。
  测试结果：WSL GCC/clang/MSVC Debug CTest
  `aot_c_zrp_metadata_pruning|aot_c_zrp_metadata_methodspec_pruning|aot_c_zrp_metadata_typespec_pruning|aot_c_zrp_metadata_module_ref_pruning|aot_c_zrp_metadata_pool_pruning`
  均 5/5；同三套直跑 TypeDef pruning 2/0、source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsv-12-s7zzzo-module-ref-generic-context.md`。
  备注：本记录只关闭 ModuleRef retained-row generic context 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 21:20:45 +08:00 · 11-S7ZSU / 12-S7ZZZN retained signature MODULE string-pool sweep/remap ·
  状态：11-S7 emitted metadata 中 retained signature `MODULE` name/version string sweep/remap 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt 闭环仍待后续。
  完成项目：retained signature blob 扫描现在把 `MODULE` 节点中的 module name/version string offsets 作为 string-pool roots；
  signature blob rewrite 在 compacted copy 中重写两个 offsets，避免 `__entry`/`1.0.0` 这类仅由 signature 引用的字符串被裁掉。
  RED/GREEN：RED 为 MethodSpec pruning 新增 retained `MODULE("__entry","1.0.0")` fixture，WSL GCC 失败
  `Expected 778 Was 764`；GREEN 后 WSL GCC/clang 与 Windows MSVC Debug 同组回归通过。
  测试结果：WSL GCC/clang/MSVC Debug direct MethodSpec pruning 7/0、pool pruning 8/0、metadata pruning 22/0、
  TypeDef pruning 2/0、TypeSpec pruning 2/0、source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsu-12-s7zzzn-signature-module-string-remap.md`。
  备注：本记录只关闭 retained signature `MODULE` string-pool sweep/remap；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 20:50:52 +08:00 · 11-S7ZST / 12-S7ZZZM retained signature TYPE_REF string-pool sweep/remap ·
  状态：11-S7 emitted metadata 中 retained signature `TYPE_REF` name string sweep/remap 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt 闭环仍待后续。
  完成项目：retained signature blob 扫描现在把 `TYPE_REF` 节点中的 name string offset 作为 string-pool root；
  compacted signature rewrite 在保留 base type payload 后，把 name string offset 重写为 compacted string-pool offset。
  string-pool remap 支持新增签名字符串 root 时按需扩容；`TYPE_DEF` signature node 仍保持既有 TypeDef token rewrite 语义。
  RED/GREEN：RED 为新增 MethodSpec `GENERIC_INST(MEMBER_REF, TYPE_REF("ExternalArg"))` fixture 后，WSL GCC MethodSpec pruning
  失败 `Expected 776 Was 764`；GREEN 后 MethodSpec pruning 6/0。
  测试结果：WSL GCC/clang/Windows MSVC Debug 均通过 source contracts 24/0、pool pruning 8/0、
  pruning 22/0、TypeDef pruning 2/0、TypeSpec pruning 2/0、MethodSpec pruning 6/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zst-12-s7zzzm-signature-typeref-string-remap.md`。
  备注：本记录只关闭 retained signature `TYPE_REF` string-pool sweep/remap；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 20:26:48 +08:00 · 11-S7ZSS / 12-S7ZZZL MethodSpec imported MEMBER_REF recursive retained-token-record guard ·
  状态：11-S7 emitted metadata 中 MethodSpec imported `MEMBER_REF` recursive retained-token-record guard 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt 闭环仍待后续。
  完成项目：MethodSpec imported `MEMBER_REF` retained-record 判定现在会递归检查 nested imported member records，
  不再只检查第一层 record 的本地 member-token 字段。`MEMBER_REF(1) -> MEMBER_REF(2) -> pruned MethodDef` 会被视为
  dangling chain 并裁掉 MethodSpec row、signature token record、signature blob 以及相关 imported member records；
  合法自引用 imported member record 仍可保留。
  RED/GREEN：RED 为新增 nested imported `MEMBER_REF` chain 指向 pruned MethodDef 的 fixture 后，WSL GCC MethodSpec pruning
  失败 `Expected 504 Was 639`；GREEN 后 MethodSpec pruning 5/0。
  测试结果：WSL GCC/clang/Windows MSVC Debug 均通过 source contracts 24/0、export token remap 10/0、
  pruning 22/0、MethodSpec pruning 5/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zss-12-s7zzzl-methodspec-memberref-recursive-retained-token-record-guard.md`。
  备注：本记录只关闭 MethodSpec imported `MEMBER_REF` recursive retained-token-record guard；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 19:54:49 +08:00 · 11-S7ZSR / 12-S7ZZZK MethodSpec imported MEMBER_REF retained-token-record guard ·
  状态：11-S7 emitted metadata 中 MethodSpec imported `MEMBER_REF` retained-token-record guard 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt 闭环仍待后续。
  完成项目：MethodSpec imported `MEMBER_REF` method-token retention 现在要求对应 token record 本身也能 retained；
  即 record 的 `token` / `relatedToken` / `ownerToken` / `targetMetadataToken` / `targetSignatureToken` 中的本地
  MethodDef/FieldDef 引用必须通过 retained-row remap。MethodSpec count/copy 与 MethodSpec signature rewrite
  贯穿 TypeDef、GenericParam、GenericParamConstraint context，避免 imported member record 指向已裁剪成员时仍保留
  MethodSpec row、`SIGNATURE` token record 或 signature blob。
  RED/GREEN：RED 为新增 imported `MEMBER_REF` record 指向 pruned MethodDef 的 fixture 后，WSL GCC MethodSpec pruning
  失败 `Expected 600 Was 639`；GREEN 后 MethodSpec pruning 3/0。
  测试结果：WSL GCC/clang/Windows MSVC Debug 均通过 source contracts 24/0、export token remap 10/0、
  pruning 22/0、MethodSpec pruning 3/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsr-12-s7zzzk-methodspec-memberref-retained-token-record-guard.md`。
  备注：本记录只关闭 MethodSpec imported `MEMBER_REF` retained-token-record guard；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 19:29:47 +08:00 · 11-S7ZSQ / 12-S7ZZZJ MethodSpec imported MEMBER_REF token-record guard ·
  状态：11-S7 emitted metadata 中 MethodSpec imported `MEMBER_REF` token-record guard 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt 闭环仍待后续。
  完成项目：MethodSpec imported `MEMBER_REF` method-token retention 现在要求源 token record 表中存在对应
  imported member record；`backend_aot_c_zrp_remap_retained_token_record()` 也拒绝指向缺失 imported `MEMBER_REF`
  token record 的 signature/owner/target 字段。signature remap、MethodSpec count/copy 和 signature rewrite
  统一携带 token-record context，缺失导入 member record 时会同时剪掉 MethodSpec row、`SIGNATURE` token record
  和 signature blob。
  RED/GREEN：RED 为
  `test_aot_c_zrp_metadata_methodspec_pruning_drops_orphan_imported_member_ref_method_token` 在 WSL GCC 失败
  `Expected 504 Was 639`，暴露旧逻辑留下 orphan MethodSpec/signature。GREEN 后 WSL GCC/clang/Windows MSVC Debug
  source contracts 24/0、export token remap 10/0、pruning 22/0、MethodSpec pruning 2/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsq-12-s7zzzj-methodspec-memberref-token-record-guard.md`。
  备注：本记录只关闭 MethodSpec imported `MEMBER_REF` token-record guard；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 19:00:53 +08:00 · 11-S7ZSP / 12-S7ZZZI MethodSpec imported MEMBER_REF method-token retention ·
  状态：11-S7 emitted metadata 中 MethodSpec imported `MEMBER_REF` method-token retention 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt 闭环仍待后续。
  完成项目：`backend_aot_c_zrp_remap_method_spec_row()` 对 `METHOD_SPEC.methodToken` 的 `MEMBER_REF`
  分支直接保留合法导入方法引用；`MEMBER_DEF` 分支仍走 retained MethodDef remap，确保本地方法定义裁剪后 RID
  压缩不回退。新增聚焦 `test_aot_c_zrp_metadata_methodspec_pruning.c` 与 CTest 目标覆盖 imported generic method
  instantiation。
  RED/GREEN：RED 为临时撤掉 `MEMBER_REF` 分支后
  `test_aot_c_zrp_metadata_methodspec_pruning_keeps_imported_member_ref_method_token` 失败 `Expected 735 Was 711`，
  暴露旧逻辑误丢 imported MethodSpec row。GREEN 后 WSL GCC/clang/Windows MSVC Debug source contracts 24/0、
  export token remap 10/0、pruning 22/0、MethodSpec pruning 1/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsp-12-s7zzzi-methodspec-memberref-retention.md`。
  备注：本记录只关闭 MethodSpec imported `MEMBER_REF` method-token 保留缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 08:13:00 +08:00 · 11-S7ZSO / 12-S7ZZZH GenericParam TypeDef owner retained-row guard ·
  状态：11-S7 emitted metadata 中 GenericParam TypeDef-owner retained-row refinement 完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`backend_aot_c_zrp_remap_generic_param_owner_token()` 现在对 `TYPE_DEF` owner 使用 retained TypeDef
  remap；GenericParam retained-row/count/range 与 GenericParamConstraint remap/count/range helper 贯穿 TypeDef row、
  token-record 和 constraint context，防止 pruned TypeDef 的 GenericParam row 留在 compacted `.zrp` 中。
  TypeDef retention 同步移除 TypeDef-owned GenericParam 对 owner TypeDef 的反向自保活 root。
  RED/GREEN：RED 为
  `test_aot_c_zrp_metadata_pruning_drops_generic_params_owned_by_pruned_type_defs` 在 WSL GCC 失败 `Expected Non-NULL`，
  暴露旧逻辑接受任意 TypeDef owner 或通过 GenericParam 自保活 dead TypeDef。GREEN 后 WSL GCC/clang/Windows
  MSVC Debug source contracts 24/0、export token remap 10/0、pruning 22/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zso-12-s7zzzh-genericparam-typedef-owner-retention.md`。
  备注：本记录只关闭 GenericParam TypeDef owner retained-row/remap 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 07:42:47 +08:00 · 11-S7ZSN / 12-S7ZZZG TypeDef retained FieldDef owner-token guard ·
  状态：11-S7 emitted metadata 中 TypeDef token-record retained-root refinement 完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`backend_aot_c_zrp_type_def_row_is_retained()` 现在接收 TypeDef row context；TypeDef token-record
  roots 对 `MEMBER_DEF` token 做 retained-row aware 判定，MethodDef 必须仍 retained，FieldDef 必须仍由 retained
  owner TypeDef 保活，且不能用 FieldDef token record 反向保活该 FieldDef 自己的 owner TypeDef。
  RED/GREEN：RED 为
  `test_aot_c_zrp_metadata_pruning_drops_typedef_rooted_only_by_pruned_field_owner_token` 在 WSL GCC 失败
  `Expected Non-NULL`，暴露旧 TypeDef root 判定把 pruned FieldDef token record 当作有效根。
  GREEN 后 WSL GCC/clang/Windows MSVC Debug source contracts 24/0、export token remap 10/0、pruning 21/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsn-12-s7zzzg-typedef-fielddef-owner-token-retention.md`。
  备注：本记录只关闭 TypeDef retained FieldDef owner-token root guard 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 07:22:01 +08:00 · 11-S7ZSM / 12-S7ZZZF Method-only member-token guard ·
  状态：11-S7 emitted metadata 中 GenericParam/MethodSpec method-only member-token refinement 完成；07~12
  总目标继续进行中，完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI
  drift/deopt 闭环仍待后续。
  完成项目：新增 MethodDef-only remap helper；`backend_aot_c_zrp_remap_method_spec_row()` 不再接受 FieldDef
  tokens 作为 method token，`backend_aot_c_zrp_remap_generic_param_owner_token()` 只允许 TypeDef owner 或 retained
  MethodDef owner。FieldDef row/token-record retained compaction 保持不变，只有放进 method-only slot 的 FieldDef token
  被视为 malformed row 并剪掉。
  RED/GREEN：RED 为
  `test_aot_c_zrp_metadata_pruning_drops_field_def_method_only_member_tokens` 在 WSL GCC 失败
  `Expected TRUE Was FALSE`，暴露旧 MethodSpec retention 接受 FieldDef methodToken 并让 metadata prepare 失败。
  GREEN 后 WSL GCC/clang/Windows MSVC Debug source contracts 24/0、export token remap 10/0、pruning 20/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsm-12-s7zzzf-method-only-member-token-retention.md`。
  备注：本记录只关闭 MethodDef-only GenericParam/MethodSpec member-token guard 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 06:53:16 +08:00 · 11-S7ZSL / 12-S7ZZZE TypeSpec retained FieldDef token-record guard ·
  状态：11-S7 emitted metadata 中 TypeSpec retained-token refinement 完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：TypeSpec row retention 现在通过 `backend_aot_c_zrp_remap_retained_token_record()` 检查 token-record root；
  TypeSpec-rooting records 中的 `MEMBER_DEF` FieldDef owners/targets 必须先通过 retained FieldDef row 判定；TypeDef row context
  已贯穿 TypeSpec、ModuleRef、signature 和 string-pool remap/copy path，避免 TypeSpec retention 复用 source FieldDef index。
  RED/GREEN：RED 为
  `test_aot_c_zrp_metadata_pruning_drops_typespec_rooted_only_by_pruned_field_token_record` 在 WSL GCC 失败
  `Expected 765 Was 794`。GREEN 后 WSL GCC/clang/Windows MSVC Debug source contracts 24/0、
  export token remap 10/0、pruning 19/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsl-12-s7zzze-typespec-fielddef-retention.md`。
  备注：本记录只关闭 TypeSpec retained FieldDef token-record guard 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 06:24:32 +08:00 · 11-S7ZSK / 12-S7ZZZD FieldDef member-token retained-row guard ·
  状态：11-S7 emitted metadata 中 FieldDef `MEMBER_DEF` token retained-row guard 子切片完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`backend_aot_c_zrp_remap_retained_token_record()` 与
  `backend_aot_c_zrp_remap_retained_export_member_token()` 为 pruning/manifest export copy path 提供完整 row context；
  FieldDef token remap 复用 `backend_aot_c_zrp_field_def_row_is_retained()` 和
  `backend_aot_c_zrp_compacted_retained_field_def_token()`，避免按 source field index 错误穿过已裁剪 FieldDef。
  RED/GREEN：RED 为
  `test_aot_c_zrp_metadata_pruning_drops_pruned_field_def_member_tokens_before_live_fields` 在 WSL GCC 失败
  `Expected 640 Was 736`。GREEN 后 WSL GCC/clang/Windows MSVC Debug pruning 18/0、source contracts 24/0、
  export token remap 10/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsk-12-s7zzzd-fielddef-token-retention.md`。
  备注：本记录只关闭 FieldDef member-token row retention/compaction 缺口；不声明完整 11-S7、12-S7 或
  07~12 总目标完成。

- 2026-07-03 05:50:58 +08:00 · 11-S7ZSC / 12-S7ZZT persistent manifest export target-only string retention ·
  状态：11-S7 persistent manifest export section copy/rewrite 的目标字符串保留子切片完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：manifest export 模块暴露 `backend_aot_c_zrp_manifest_export_row_is_retained()`，以不修改源 row 的方式复用既有
  target/member token remap 判定；string-pool remap builder 现在接收 manifest export rows/count，仅对最终 retained 的 row 收集
  `targetStringOffset`；prune orchestration 把 manifest export count 纳入 string-remap capacity，并将 manifest export view 传入 remap 构建。
  RED/GREEN：RED 为
  `test_aot_c_zrp_metadata_pruning_keeps_manifest_export_target_only_strings` 在 WSL GCC 失败 `Expected TRUE Was FALSE`，
  暴露只被 manifest export target 引用的 `"unused"` 字符串没有进入 compacted string-pool remap。GREEN 后 WSL GCC/clang/Windows
  MSVC Debug pruning 17/0，source contracts 24/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsc-12-s7zzt-manifest-export-target-string-retention.md`。
  备注：本记录只关闭 manifest export target string root 缺口；不声明完整 11-S7、12-S7 或 07~12 总目标完成。

- 2026-07-03 05:04:05 +08:00 · 11-S7ZSJ / 12-S7ZZZA emitted `.zrp` FieldDef-owned orphan metadata sweep/pruning ·
  状态：11-S7 emitted metadata 中 FieldDef owner-TypeDef 孤儿行清扫子切片完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：TypeDef 保活图不再把 FieldDef `ownerTypeToken` 反向当作 TypeDef root；FieldDef 保活改为依赖 owner
  TypeDef 是否已保活，pruned header 的 `FIELD_DEFS` section 使用 retained FieldDef count。FieldDef copy path 会跳过
  orphan rows，重写 `MEMBER_DEF` token、owner TypeDef token、name string、signature blob 与 constant-pool 引用；
  TypeDef copy path 同步重写 compacted `firstFieldDefIndex/fieldDefCount`；string/signature/constant pools 与
  member-token remap sidecar 均按 retained FieldDef 集合构建。
  RED/GREEN：RED 为
  `test_aot_c_zrp_metadata_pruning_drops_orphan_type_defs_with_fields` 在 WSL GCC 失败 `Expected 576 Was 692`。
  GREEN 后 WSL GCC direct pruning 16/0、pool pruning 8/0、typedef pruning 2/0、typespec pruning 2/0；WSL clang 同组
  16/0、8/0、2/0、2/0；Windows MSVC Debug pruning 16/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsj-12-s7zzza-fielddef-owner-sweep.md`。
  备注：本记录只关闭 owner-TypeDef 驱动的 FieldDef orphan sweep/range/pool/remap 压缩；不声明完整 11-S7 或
  07~12 总目标完成。

- 2026-07-03 04:18:41 +08:00 · 11-S7ZSI / 12-S7ZZZ CLI/project non-publishable `.zrp` sidecar stale cleanup ·
  状态：11-S7 CLI/project compacted metadata sidecar stale artifact 清理子切片完成；07~12 总目标继续进行中，完整
  metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`ZrCli_Compiler_WriteAotCFileForModule()` 现在先从 AOT C 输出路径派生 compacted metadata sidecar path；
  若 embedded blob 不能同时通过 `.zrp` header read 与 definition-table validation，则不注入
  `compactedZrpMetadataOutputPath`，并主动删除同名旧 `.zrp` sidecar。这样 stale sidecar 不会跨一次非 metadata 或无效
  metadata 的 AOT C 重新生成残留；如果删除失败，则拒绝写入新的 AOT C artifact。
  RED/GREEN：RED 为 `test_cli_aot_writer_removes_stale_sidecar_when_metadata_is_not_publishable` 在 WSL GCC 失败
  `Expected FALSE Was TRUE`；追加 `test_cli_aot_writer_fails_when_stale_sidecar_cannot_be_removed` 后再次 RED
  `Expected FALSE Was TRUE`；GREEN 后 WSL GCC/clang/Windows MSVC Debug focused CTest
  `cli_aot_compacted_metadata_sidecar|cli_aot_writer_options|aot_c_zrp_metadata_publication|aot_c_zrp_metadata_pruning|zrp_metadata_format`
  均 5/5。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsi-12-s7zzz-cli-zrp-sidecar-stale-cleanup.md`。
  备注：本切片只关闭 stale sidecar 清理，不声明完整 11-S7、12-S7 或 07~12 总目标完成；`compiler_aot.c`
  已超过大文件阈值，本次只扩展既有 AOT C artifact orchestration，后续继续增长 sidecar policy 时应抽出独立 helper/module。

- 2026-07-03 03:56:32 +08:00 · 11-S6H / 12-S8E CLI full-AOT metadata-drift assertion alignment ·
  状态：11-S6H inline-struct typed-call metadata guard/deopt 的 CLI project full-AOT 验收对齐完成；07~12 总目标继续进行中。
  完成项目：`tests/cli/test_cli_project_incremental.c` 的 full-AOT `--emit-aot-c` 项目断言从“禁止任何
  `CallInlineStructDynamicDeoptBridge()`”收窄为“禁止 shared generic missing-instance deopt，同时要求 11-S6H metadata
  guard/deopt surface”。生成物仍必须包含 `zr_aot_generic_call_typed_full_aot_no_deopt`，且不得包含
  `zr_aot_generic_call_typed_missing_instance_deopt` 或 `"generic call typed missing AOT instance"`；同时必须包含
  `zr_aot_value_exec_call_typed_metadata_guard`、`ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, ...)` 和
  `"typed inline struct direct call metadata drift"`。
  RED/GREEN：RED 为 WSL GCC `cli_project_incremental` 失败 `Expected NULL`，原因是旧断言把 11-S6H 要求保留的
  metadata-drift fallback 当作 full-AOT missing-instance deopt；GREEN 后 WSL GCC/clang/Windows MSVC Debug focused
  CTest `cli_project_incremental|cli_aot_compacted_metadata_sidecar|aot_runtime_typed_direct_call_compatibility|aot_c_generic_call_typed`
  均 4/4。
  产出：`tests/acceptance/2026-07-03-aot-11-s6h-12-s8e-cli-full-aot-metadata-drift-assertion.md`。
  备注：未修改生产代码；本切片只让 CLI project 验收与 11-S6H 当前计划语义一致，不声明完整 11-S6/12-S8 或 07~12
  总目标完成。

- 2026-07-03 03:45:20 +08:00 · 11-S7ZSH / 12-S7ZZY CLI/project compacted `.zrp` metadata sidecar path bridge ·
  状态：11-S7 CLI/project 自动派生 compacted `.zrp` metadata sidecar artifact 子切片完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、full trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`ZrCli_Project_ResolveAotCompactedMetadataPathFromAotCPath()` 由最终 AOT C path 派生同目录 `.zrp`
  sidecar path，`ZrCli_Project_ResolveAotCompactedMetadataPath()` 复用 project/module AOT C 路径解析；CLI AOT writer
  bridge 只在 embedded blob 同时满足 `.zrp` header 和 `ZrCore_ZrpMetadata_ValidateDefinitionTables()` 时设置
  `compactedZrpMetadataOutputPath`；普通 `.zro`、空/无效 metadata 或伪造 header 不发布 sidecar；optional AOT C
  输出关闭和 removed-module reconcile 会同步删除派生 sidecar。
  RED/GREEN：RED 为新增 `zr_vm_cli_aot_compacted_metadata_sidecar_test` 后 WSL GCC 链接失败，缺少 project sidecar
  path helper；追加 invalid definition-table fixture 后旧 header-only 判定失败 `Expected FALSE Was TRUE`。GREEN 后
  WSL GCC/clang/Windows MSVC Debug focused CTest
  `cli_aot_compacted_metadata_sidecar|cli_aot_writer_options|aot_c_zrp_metadata_publication|aot_c_zrp_metadata_pruning|zrp_metadata_format`
  均 5/5。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsh-12-s7zzy-cli-zrp-sidecar-path-bridge.md`。
  备注：本切片验证时相邻 `cli_project_incremental` 暴露 full-AOT dynamic bridge broad anti-needle 失败；该运行未生成
  普通项目 `.zrp` sidecar，随后 2026-07-03 03:56:32 +08:00 的 11-S6H/12-S8E 测试语义对齐已收敛该失败。本切片
  只关闭 CLI/project sidecar path bridge，不声明完整 11-S7/12-S7 或 07~12 总目标完成。

- 2026-07-03 02:47:37 +08:00 · 11-S7ZSG / 12-S7ZZX writer-level compacted `.zrp` metadata sidecar publication ·
  状态：11-S7/12-S7 final embedded metadata sidecar 文件发布子切片完成；07~12 总目标继续进行中，
  CLI/project 自动派生 sidecar artifact path、完整 metadata sweep/pruning、full trim analyzer、
  annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`SZrAotWriterOptions.compactedZrpMetadataOutputPath` 新增 writer-level 输出路径；AOT C writer 在
  generated C 文件关闭成功后调用 `backend_aot_c_publish_compacted_zrp_metadata()`，把经过 pruning/manifest export
  publication 后的最终 embedded `.zrp` blob 原样写到 sidecar；发布 helper 会先读取 `.zrp` header 做形态校验，
  并在短写或 close failure 时删除 partial sidecar，writer 同步删除 generated C 后返回 false。
  RED/GREEN：RED 为新增 `test_aot_c_writer_publishes_compacted_zrp_metadata_file` 后 WSL GCC 编译失败，
  `SZrAotWriterOptions` 缺少 `compactedZrpMetadataOutputPath`；GREEN 后 WSL GCC/clang/Windows MSVC Debug focused
  CTest `aot_c_zrp_metadata_publication|aot_c_zrp_metadata_pruning|aot_c_code_stripping|zrp_metadata_format|cli_aot_writer_options`
  均 5/5。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsg-12-s7zzx-zrp-compacted-metadata-publication.md`。
  备注：本记录只关闭 writer option 指定路径下的最终 `.zrp` metadata 文件发布；不声明 CLI/project 默认路径、
  完整 11-S7、12-S7 或 07~12 总目标完成。

- 2026-07-03 02:04:51 +08:00 · 11-S7ZSF / 12-S7ZZW `.zrp` manifest export unbound declaration row publication ·
  状态：11-S7/12-S7 unbound manifest export declaration 持久 row 发布与文件校验子切片完成；07~12 总目标继续进行中，
  完整 metadata sweep/pruning、compacted-token file publication、full trim analyzer、annotation/promotion policy 和
  更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`ZrCore_ZrpMetadata_ValidateDefinitionTables()` 对 `manifestExports` section 新增 unbound row 形态：
  kind 为 type/method/field、已知 flags 为 0、`typeToken/memberToken` 均为 0 时合法，未知 flags、错配 kind/token 或
  未标记但携带 token 仍拒绝。`backend_aot_c_zrp_copy_manifest_exports()` 在剪裁重写既有持久 rows 时保留这类 unbound
  rows 并只 remap target string offset；`backend_aot_c_zrp_publish_manifest_export_declarations()` 现在把 writer
  declarations 中未绑定的 type/method/field export 追加为持久 rows，target string 复用或追加到 string pool，token
  flags 和 token 字段保持 0。运行期 manifest export binding gate 仍只对 token-bound export view 做 ABI drift 校验。
  RED/GREEN：RED 为新增
  `test_zrp_metadata_manifest_exports_roundtrip_and_validate_token_shapes` 的 unbound rows 后，WSL GCC
  `zrp_metadata_format` 失败 `Expected TRUE Was FALSE`；新增
  `test_aot_c_zrp_metadata_pruning_publishes_unbound_manifest_export_declarations_as_rows` 后，旧 prepared blob
  长度为 708 而非包含 `api.dynamic`/`api.DynamicType`/`api.value` rows 和字符串后的 806。GREEN 后 focused pruning
  扩展为 15/0，format 扩展为 13/0。
  验证：WSL GCC focused CTest
  `zrp_metadata_format|aot_c_zrp_metadata|aot_c_code_stripping|aot_c_guardrail_contracts` 8/8；WSL clang 同组 8/8；
  Windows MSVC Debug 同组 8/8。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsf-12-s7zzw-zrp-manifest-export-unbound-declaration-publication.md`。
  备注：本记录只关闭 unbound manifest export declarations/rows 到持久 `.zrp` 的发布、保留和文件校验；不声明完整
  11-S7、12-S7 或 07~12 总目标完成。

- 2026-07-03 01:33:09 +08:00 · 11-S7ZSE / 12-S7ZZV `.zrp` manifest export type declaration row publication ·
  状态：11-S7/12-S7 token-bound type manifest export declaration 持久 row 发布子切片完成；07~12 总目标继续进行中，
  unbound declaration file policy、完整 metadata sweep/pruning、compacted-token file publication、full trim analyzer、
  annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：TypeDef pruning 现在在 prepared embedded metadata 上构建 source `TYPE_DEF` -> compacted `TYPE_DEF`
  remap sidecar；`backend_aot_c_zrp_publish_manifest_export_declarations()` 对 `TYPE` declarations 使用该 sidecar
  将源 type token 改写为 compacted token 后追加持久 `manifestExports` row。新增 type declaration fixture 覆盖 source
  `TYPE_DEF(2)` 在前置 orphan `TYPE_DEF(1)` 被剪除后发布为 compacted `TYPE_DEF(1)`，并校验 row kind、`HAS_TYPE_TOKEN`、
  target string-pool 更新和 member token 清零。
  RED/GREEN：RED 为新增
  `test_aot_c_zrp_metadata_pruning_publishes_type_manifest_export_declarations_as_rows` 后，WSL GCC focused pruning
  在 type declaration row 长度断言处失败，旧 prepared blob 为 526 而非包含 `api.LiveType` row/string 的 559。GREEN 后
  focused pruning 扩展为 13/0。
  验证：WSL GCC focused CTest `aot_c_zrp_metadata|aot_c_code_stripping|aot_c_guardrail_contracts` 7/7；WSL clang
  同组 7/7；Windows MSVC Debug 同组 7/7。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zse-12-s7zzv-zrp-manifest-export-type-declaration-publication.md`。
  备注：本记录只关闭 token-bound type declarations 到持久 `.zrp` manifest export rows 的发布；不声明 unbound
  declarations、完整 11-S7、12-S7 或 07~12 总目标完成。

- 2026-07-03 00:43:21 +08:00 · 11-S7ZSD / 12-S7ZZU `.zrp` manifest export declaration row publication ·
  状态：11-S7/12-S7 token-bound method/field manifest export declaration 持久 row 发布子切片完成；07~12 总目标继续进行中，
  type export 持久 row compaction、未绑定 declaration file policy、完整 metadata sweep/pruning、compacted-token file
  publication、full trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：`backend_aot_c_zrp_publish_manifest_export_declarations()` 现在在 embedded `.zrp` prepare/prune 后保留既有
  `manifestExports` rows，并把 writer declarations 中带 `MEMBER_DEF` binding 的 method/field export 追加为持久 rows；
  target string 会复用现有 string-pool slice 或追加到 string pool，source member token 会通过 retained member-token
  sidecar remap 为 compacted `MEMBER_DEF`，最终重新计算 section layout 并校验 definition tables。
  `backend_aot_c_prepare_embedded_zrp_metadata()` 不再只在 code stripping 开启时提前返回，而是在有 blob 时完成 pruning
  后统一调用 declaration row publication，使 emitter 后续 size/blob sampling 看到最终文件级 manifest export rows。
  RED/GREEN：RED 为新增
  `test_aot_c_zrp_metadata_pruning_publishes_manifest_export_declarations_as_rows` 后，MSVC focused pruning 失败，
  旧 prepared blob 长度为 708 而不是追加 `api.kept` / `api.field` rows 和字符串后的 767。GREEN 后 focused pruning
  扩展为 12/0。
  验证：WSL GCC direct pruning 12/0、export-token remap 10/0、code stripping 10/0；WSL clang direct 同组三项
  12/0、10/0、10/0；Windows MSVC Debug direct 同组三项 12/0、10/0、10/0。
  产出：`tests/acceptance/2026-07-03-aot-11-s7zsd-12-s7zzu-zrp-manifest-export-declaration-publication.md`。
  备注：本记录只关闭 token-bound method/field declarations 到持久 `.zrp` manifest export rows 的发布；不声明 type
  export、unbound declaration、完整 11-S7、12-S7 或 07~12 总目标完成。

- 2026-07-02 10:42:54 +08:00 · 11-S7ZSC / 12-S7ZZT `.zrp` manifest export pruning rewrite ·
  状态：11-S7/12-S7 manifest export 持久 section 的剪裁后重写子切片完成；07~12 总目标继续进行中，
  writer 端生成/写入持久 manifest export rows、完整 metadata sweep/pruning、compacted-token file publication、
  full trim analyzer、annotation/promotion policy 和更完整 ABI drift/deopt 闭环仍待后续。
  完成项目：emitted `.zrp` pruner 现在读取 `manifestExports` section，在 rebuild 后按 compacted string-pool
  remap 重写 `targetStringOffset`，按 retained member-token remap 重写 method/field `memberToken`，按 TypeDef
  token remap 重写 type export `typeToken`，并对 kind/flag/token shape mismatch fail closed。新增
  `backend_aot_c_zrp_metadata_manifest_export.{h,c}`，公开复用 string-offset remapper，并让 Windows shared-DLL
  focused test target 显式编入该 helper。
  RED/GREEN：RED 为 WSL GCC direct `zr_vm_aot_c_zrp_metadata_pruning_test` 新增 manifest export 剪裁 fixture 后
  11/1，旧 pruner raw-copy section 导致目标字符串 offset 仍为 25 而非 compacted 后 17。GREEN 后同一测试 11/0。
  验证：WSL GCC direct pruning 11/0、export-token remap 10/0、code stripping 10/0；WSL clang direct 同组三项
  11/0、10/0、10/0；Windows MSVC Debug direct 同组三项 11/0、10/0、10/0。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zsc-12-s7zzt-zrp-manifest-export-pruning.md`。
  备注：本记录只关闭“已有持久 manifest export row 在 `.zrp` metadata pruning 后正确重写/校验”；不声明 writer
  已从 declarations 生成持久 rows，也不声明完整 11-S7、12-S7 或 07~12 总目标完成。

- 2026-07-02 10:15:47 +08:00 · 11-S7ZSB / 12-S7ZZS `.zrp` manifest export persistent section format ·
  状态：11-S7/12-S7 manifest export 持久 section 的格式层支撑子切片完成；完整 11-S7 仍未关闭，
  generator 写入持久 manifest export rows、pruner 中完整发布/裁剪该 section、compacted-token file publication、
  完整 metadata sweep/pruning 和 full trim analyzer 仍待后续。
  完成项目：`.zrp` metadata 升级到 v4，header 从 208 扩展到 224 字节，section count 从 12 扩展到 13，
  在既有 string/signature/constant pool 后追加 `manifestExports` section。新增 `SZrZrpMetadataManifestExportRow`
  持久 row，header read/write/validate、section view、definition-table payload writer、CLI dump/diff、AOT size stats
  与 generated marker/delta 输出均识别该 section。
  RED/GREEN：RED 为 WSL GCC focused `zr_vm_zrp_metadata_format_test` 编译失败，缺少
  `ZR_ZRP_METADATA_SECTION_MANIFEST_EXPORTS`、`SZrZrpMetadataManifestExportRow` 和 header `manifestExports` 字段；
  相邻 RED 为 code-stripping focused 10/1，旧 stats/registration 仍按 12-section 形态断言。GREEN 后格式层、
  dump、size delta 和 code-stripping focused 均通过。
  验证：WSL GCC direct `zr_vm_zrp_metadata_format_test` 13/0、`zr_vm_cli_zrp_metadata_dump_test` 通过、
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 10/0、`zr_vm_aot_c_zrp_metadata_size_deltas_test` 2/0、
  `zr_vm_aot_c_code_stripping_test` 10/0；WSL clang 与 Windows MSVC Debug 同组 focused direct 验证通过。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zsb-12-s7zzs-zrp-manifest-export-section.md`。
  备注：本记录只关闭持久 `.zrp` manifest export section 的格式/可视化/统计支撑；不声明 generator 已把
  manifest export table 写入 `.zrp` 或完整裁剪发布路径完成。

- 2026-07-02 09:38:04 +08:00 · 11-S7ZSA / 12-S7ZZR manifest export kind/token guard ·
  状态：11-S7/12-S7 manifest export table builder 支撑子切片完成；完整 11-S7 仍未关闭，持久
  export manifest/table file publication、完整 metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer
  仍待后续。
  完成项目：`backend_aot_c_zrp_manifest_export_table_build()` 现在按 export kind 收紧 token binding 形态：
  type export 只能携带 `TYPE_DEF` binding，method/field export 只能携带 `MEMBER_DEF` binding；未绑定 declaration
  与既有 method/field member-token remap provider 路径保持可用。
  RED/GREEN：RED 为新增
  `test_aot_c_zrp_metadata_manifest_export_table_rejects_kind_token_mismatch` 后，旧 builder 接受 type export
  携带 `MEMBER_DEF` token，WSL GCC focused 失败 10/1；GREEN 后同一测试 10/0。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 均 10/0；
  WSL GCC direct source contracts 24/0；WSL GCC/clang provider shared-library smoke 均 1/0。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zsa-12-s7zzr-manifest-export-kind-token-guard.md`。
  备注：本切片只关闭 manifest export table builder 的 kind/token shape guard；不声明持久 `.zrp` manifest
  export section、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 09:15:26 +08:00 · 11-S7ZR / 12-S7 range-selected provider runtime export publication ·
  状态：11-S7 cross-module provider candidate runtime/export publication 支撑子切片完成；完整 11-S7 仍未关闭，
  完整 metadata sweep/pruning、版本/ABI 漂移闭环、compacted-token file publication 和 full trim analyzer 仍待后续。
  完成项目：`zr_vm_aot_c_provider_shared_library_smoke_test` 现在用同一 provider `.zrp` 同时覆盖 exact
  `$mathLocal@2.1.0/ops/sum` 和 candidate-selected `$mathRange@2.1.0/ops/sum`；`mathRange` 通过 2.0.5/2.1.0
  candidates 选择 2.1.0。runtime 加载 range-selected alias 后可同时读取普通 `seed` public export 与 attached
  manifest export metadata (`add`/`seed`)。
  RED：WSL GCC provider smoke 在 `$mathRange@2.1.0/ops/sum` 上失败，manifest export view 已存在但
  `ZrCore_Module_GetPubExport(..., "seed")` 返回 NULL；根因是 `PublishModuleExports()` 对没有 `frame.recordHandle`
  的 generated frame 回退到 first equivalent function record，命中了已执行的 exact alias record。
  GREEN：`ZrLibrary_AotRuntime_PublishModuleExports()` 优先消费匹配当前 function 的 `runtimeState->activeRecord`，
  再回退 frame handle / function lookup；WSL GCC/clang provider shared-library smoke 均 1/0。WSL GCC/clang/MSVC
  Debug direct provider version-selection 4/0、resolver 9/0、manifest normalization 28/0、provider runtime 1/0、
  source contracts 24/0、frame setup contracts 1/0；MSVC provider smoke 编译通过且 Unix-only 分支 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zr-range-selected-provider-export-publication.md`。
  备注：本记录只关闭 range-selected provider runtime export publication；不声明完整 metadata sweep/pruning、
  full trim analyzer 或 ABI drift/deopt 闭环完成。

- 2026-07-02 08:51:27 +08:00 · 11-S7ZQ / 12-S7 legacy dependency AssemblyRef identity canonicalization ·
  状态：11-S7 provider/reference metadata support 测试校准子切片完成；完整 11-S7 仍未关闭，完整 metadata
  sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：关闭 11-S7ZL 记录中遗留的 `zr_vm_project_import_canonicalization_test` 35/1 探测失败；
  `test_project_compile_applies_dependency_import_version_range_to_assembly_ref` 现在断言 static import/module effect
  保持 canonical `$math@1.2.3/ops/sum`，但 AssemblyRef 行名使用 normalized provider assembly identity `math`，
  且不存在以 canonical module key 命名的 AssemblyRef。生产代码未修改。
  RED：旧测试把 canonical import key 当成 AssemblyRef identity，WSL GCC canonicalization 失败 35/1；临时 RED guard
  证明 module effect 已携带 `$math@1.2.3/ops/sum` + `math` assembly identity。
  GREEN：WSL GCC/clang/Windows MSVC Debug direct canonicalization 均 35/0；provider version-selection 4/0、
  resolver 9/0、manifest normalization 28/0、provider runtime 1/0；provider shared-library smoke 在 WSL GCC/clang
  均 1/0，Windows MSVC Debug 按 Unix-only dynamic-loader 分支 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zq-legacy-dependency-assembly-ref-identity.md`。
  备注：本记录只关闭 stale AssemblyRef identity 断言；不声明完整 provider metadata binding、metadata sweep/pruning
  或 ABI drift/deopt 闭环完成。

- 2026-07-02 08:28:32 +08:00 · 11-S6I malformed binding table fail-closed ·
  状态：11-S6 no-crash ABI drift injection 支撑子切片完成；完整 11-S6 仍未关闭，cross-module token resolve
  与更完整 end-to-end ABI drift injection 仍待后续。
  完成项目：`ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility()` 现在把非零
  `moduleMetadataBindingLength` 搭配 NULL `moduleMetadataBindings` 判为
  `ZR_METADATA_RUNTIME_BINDING_STATUS_INVALID_ARGUMENT`，并填充失败 report；旧的 loop-time NULL table break 已移除。
  `ZrLibrary_AotRuntime_CanUseTypedDirectCall()` 通过既有非兼容状态消费路径对 malformed caller binding table 返回 false，
  避免把 malformed metadata 当作可 direct call。
  RED：新增 malformed binding table 单测后，旧扫描路径在 WSL GCC metadata runtime binding compatibility 中返回 compatible，
  `16 Tests 1 Failure`。
  GREEN：WSL GCC/clang/Windows MSVC Debug 上 metadata runtime binding compatibility 均 16/0，typed direct-call compatibility
  均 4/0。
  产出：`tests/acceptance/2026-07-02-aot-11-s6i-malformed-binding-table-fail-closed.md`。
  备注：本记录只关闭 malformed binding table fail-closed；不声明 cross-module token resolve 或完整 ABI drift/deopt
  闭环完成。

- 2026-07-02 08:16:21 +08:00 · 11-S7ZP / 12-S7 provider automatic range-based candidate selection ·
  状态：11-S7 provider candidate-set range selection 支撑子切片完成；完整 11-S7 仍未关闭，完整 metadata
  sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：`.zrp` `references.alias` 现在可用 `candidates[]` 替代 exact `path`；loader 会按 declared
  assembly、可选 exact version、`[minVersionInclusive, maxVersionExclusive)` 和 strict `major.minor.patch`
  版本排序筛选候选，选择范围内最高版本，并复用既有 reference/package parse path 生成 canonical
  `$alias@version/module`。未选中的 candidates 只探测、不加入 dependency package 表；无匹配或 malformed
  candidate set fail-closed。`zrp.schema.json` 同步新增 `path`/`candidates` 互斥校验和 `mathRange` 示例。
  RED：新增 candidate-only reference 测试后旧 loader 因缺少 required `path` 返回 NULL，positive 选择用例失败。
  GREEN：补齐 candidate selector 后 WSL GCC/clang/Windows MSVC Debug focused version-selection 4/0；resolver、
  manifest normalization、provider runtime 与 provider shared-library smoke 相邻回归通过；schema JSON parse 通过。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zp-provider-range-candidate-selection.md`。
  备注：本记录关闭 automatic range-based candidate selection；不声明完整 metadata sweep/pruning、完整 ABI drift/deopt
  或 full trim analyzer 完成。

- 2026-07-02 08:13:22 +08:00 · 11-S7ZO / 12-S7 provider export metadata attach fixture ·
  状态：11-S7 provider manifest export metadata attach 端到端覆盖子切片完成；完整 11-S7 仍未关闭，
  automatic range-based candidate selection、完整 metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer
  仍待后续。
  完成项目：`zr_vm_aot_c_provider_shared_library_smoke_test` 的 provider `.zrp` 新增 method `add` 与 field `seed`
  exports；fixture 在生成 AOT C 前调用 `ZrCli_Compiler_ApplyProjectAotExportDeclarations()`，断言 writer options
  绑定 `MEMBER_DEF` token，并检查 generated C 发布 manifest export table、member-token flags 与
  descriptor/codeRegistration 指针。运行时加载 provider 动态库后，测试查询 attached metadata runtime manifest
  export view，确认 `add`/`seed` 均带 `HAS_MEMBER_TOKEN` 与有效 member token。
  RED/基线：上一成功 fixture 未断言 provider export declarations 到 generated manifest table 再到 runtime view 的
  attach 链路；本切片补齐覆盖，生产路径已由既有 manifest export table/runtime mirror 支撑。
  GREEN：WSL GCC/clang provider shared-library smoke 均 1/0；Windows MSVC Debug 目标编译通过，Unix-only
  dynamic-loader 分支 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zo-provider-export-metadata-attach-fixture.md`。
  备注：本记录关闭 provider dynamic-library path 的 export metadata attach 验证缺口；不声明完整 metadata
  sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 07:25:55 +08:00 · 11-S7ZN / 12-S7 provider multi-version exact selection + range guard ·
  状态：11-S7 provider exact alias/version selection 与 declared strict semver range guard 支撑子切片完成；
  完整 11-S7 仍未关闭，automatic range-based candidate selection、export metadata attach、完整 metadata
  sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：新增 `tests/library/test_project_import_provider_version_selection.c` 与
  `zr_vm_project_import_provider_version_selection_test`，验证 root `.zrp` 可同时引用同一 assembly 的
  `mathV2@2.1.0` 与 `mathV3@3.1.0` provider，且 AOT load request 精确落到各自 canonical module key、
  source/binary path 和 backend-specific library path；`project.c` 新增 strict `major.minor.patch`
  declared range guard，覆盖 legacy dependencies、`references` `.zrp` 和 `.zrm` provider references。
  RED：负向用例先失败于 `Expected NULL`，旧 manifest parser 接受了 `version = 3.1.0` 但 range
  `[2.0.0, 3.0.0)` 的 provider reference。
  GREEN：补齐 manifest-time range guard 后 focused test 2/0；WSL GCC/clang direct provider version-selection、
  resolver、manifest normalization、provider runtime 与 provider shared-library smoke 均通过；Windows MSVC Debug
  direct version-selection/resolver/manifest/runtime 均通过，provider shared-library smoke 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zn-provider-version-selection-range-guard.md`。
  备注：非 strict/legacy version 字符串保持兼容；本切片不声明自动范围候选选择、cross-provider export metadata
  attach 或完整 metadata sweep/pruning 完成。

- 2026-07-02 07:01:13 +08:00 · 11-S7ZM / 12-S7 provider AOT dynamic-library success fixture ·
  状态：11-S7 standalone provider AOT dynamic-library success 支撑子切片完成；完整 11-S7 仍未关闭，
  multi-version selection、export metadata attach、完整 metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer
  仍待后续。
  完成项目：新增 `tests/parser/test_aot_c_provider_shared_library_smoke.c` 和
  `zr_vm_aot_c_provider_shared_library_smoke_test`，在 fixture 内生成 provider `.zrp`、provider source、`.zro`、
  AOT C 与 `/deps/math/bin/aot_c/lib/zrvm_aot_ops_sum.so`，并通过 strict AOT C 导入
  `$mathLocal@2.1.0/ops/sum`；断言 provider-local descriptor name、canonical module cache key、AOT C executed-via
  和 `seed = 37` export。
  RED/基线：11-S7ZL 前只证明 missing-provider diagnostic，未证明真实 provider library 成功加载；新增 target 初次需要
  重新 configure。配置后当前实现直接通过，作为 11-S7ZL runtime consumption 的成功路径验收补强。
  GREEN：WSL GCC/clang direct `zr_vm_aot_c_provider_shared_library_smoke_test` 均 1/0；Windows MSVC Debug target
  编译通过且 Unix-only dynamic-loader 分支 1 ignored。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zm-provider-aot-dynamic-library-success.md`。
  备注：本切片未修改生产 runtime；不声明多版本 provider 选择、cross-provider export metadata attach 或完整 metadata
  sweep/pruning 完成。

- 2026-07-02 06:41:32 +08:00 · 11-S7ZL / 12-S7 provider AOT runtime load-request consumption ·
  状态：11-S7 standalone provider AOT runtime load-request consumption 支撑子切片完成；完整 11-S7 仍未关闭，
  provider 动态库成功加载端到端、multi-version selection、export metadata attach、完整 metadata sweep/pruning、
  版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：`aot_runtime_prepare_record()` 对 canonical provider import 调用
  `ZrLibrary_Project_ResolveImportProviderAotLoadRequest()`，消费 `.zrp` provider source/binary/library path；
  descriptor 校验改用 provider-local `descriptorModuleName`，runtime record/cache 仍保留 canonical module key；
  `.zrm` provider request 明确 fail-closed 为 archive entry 不是 dynamic library。
  RED：新增 `zr_vm_project_import_aot_provider_runtime_test` 后，WSL GCC direct test 失败于 `lastError == null`，
  未报告 `/deps/math/bin/aot_c/lib/zrvm_aot_ops_sum.*` provider library path。
  GREEN：补齐 runtime 消费逻辑后 WSL GCC/clang/Windows MSVC Debug direct
  `zr_vm_project_import_aot_provider_runtime_test` 均 1/0；WSL GCC `zr_vm_project_import_resolver_test` 9/0。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zl-provider-aot-runtime-load-request.md`。
  备注：额外探测的 WSL GCC `zr_vm_project_import_canonicalization_test` 曾有独立 35/1 失败；后续 11-S7ZQ 已确认
  这是 stale AssemblyRef identity 断言并关闭。

- 2026-07-02 06:13:21 +08:00 · 11-S7ZK / 12-S7 provider AOT load request ·
  状态：11-S7 standalone provider AOT load-request 支撑子切片完成；完整 11-S7 仍未关闭，
  provider runtime dynamic loading、multi-version selection、export metadata attach、完整 metadata sweep/pruning、
  版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：project import resolver 层新增 provider AOT load-request API：
  `SZrLibrary_ProjectImportProviderAotLoadRequest` 与
  `ZrLibrary_Project_ResolveImportProviderAotLoadRequest()`；该 API 复用 provider location discovery，输出
  canonical `$alias@version/module`、descriptor-local module name、backend kind、declared assembly/version range，
  并为 `.zrp` provider 生成 source/binary/intermediate path 与 backend-specific `aot_c` / `aot_llvm` library path；
  `.zrm` provider 保留 archive/entry view 且不伪造 filesystem dynamic library path。
  RED：focused WSL GCC build 在新增测试调用缺失 struct/API 时编译失败。
  GREEN：补齐 public API/实现后 WSL GCC/clang/Windows MSVC Debug direct
  `zr_vm_project_import_resolver_test` 均 9/0 通过；该 focused target 当前未作为独立 CTest 注册。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zk-provider-aot-load-request.md`。
  备注：不声明 provider runtime dynamic loading、multi-version selection、export metadata attach、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-02 05:58:12 +08:00 · 11-S7ZJ / 12-S7 provider import location discovery ·
  状态：11-S7 standalone provider import-path discovery 支撑子切片完成；完整 11-S7 仍未关闭，
  provider runtime loading、multi-version selection、export metadata attach、完整 metadata sweep/pruning、
  版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：project import resolver 层新增 provider location API：`SZrLibrary_ProjectImportProviderLocation`
  携带 artifact kind、assembly/version range、`.zrm` archive entry 指针以及 `.zrp` source/binary/intermediate
  provider paths；`project_import_provider_location.c` 用现有 resolver/query/entry lookup 组合实现，避免继续扩大
  已超过 1100 行的 `project_import_resolver.c`。
  RED/GREEN：RED 为 import resolver focused 测试新增 provider location 调用后，WSL GCC 编译失败于缺少
  `SZrLibrary_ProjectImportProviderLocation` 和 `ZrLibrary_Project_ResolveImportProviderLocation()`；GREEN 后
  `.zrp` project reference 与 `.zrm` assembly reference 均能返回 canonical provider module key、declared
  assembly/version range 和对应加载位置。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_project_import_resolver_test` 均 9/0；该 focused target
  当前未作为独立 CTest 注册。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zj-provider-import-location-discovery.md`。
  备注：本切片只关闭 import-path location discovery；不声明 provider runtime loading、版本候选选择、
  export metadata attach、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 05:34:27 +08:00 · 11-S7ZI / 12-S7 provider manifest export binding gate ·
  状态：11-S7 provider manifest export import-verifier gate 支撑子切片完成；完整 11-S7 仍未关闭，
  standalone provider import-path discovery/loading/version selection、完整 metadata sweep/pruning、版本/ABI 漂移闭环
  和 full trim analyzer 仍待后续。
  完成项目：import signature verifier 现在在选中 provider typed export 后，读取 provider module attached metadata runtime
  的 manifest export table，并复用 11-S7ZH 的 binding compatibility gate；新增
  `zr_module_import_signature_verify_manifest_export_binding()` 把 effect kind/export kind 映射到
  `TYPE/METHOD/FIELD` manifest export kind，构造 `SZrMetadataTokenBinding` snapshot，并把 manifest export token mismatch
  转换为既有 import signature mismatch 诊断路径。无 attached manifest export table 的旧 provider 保持兼容通过。
  RED/GREEN：RED 为 provider manifest export token drift 用例先失败，`zr_vm_metadata_type_ref_binding_test`
  接受了 manifest table 与 typed export symbol 之间的 `MEMBER_DEF` 漂移；GREEN 后该测试扩展为 9/0，且相邻
  manifest export runtime/binding compatibility 测试保持通过。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_metadata_type_ref_binding_test` 9/0、
  `zr_vm_metadata_runtime_manifest_exports_test` 7/0、`zr_vm_metadata_runtime_binding_compatibility_test` 15/0；同三套工具链
  CTest `metadata_type_ref_binding|metadata_runtime_manifest_exports|metadata_runtime_binding_compatibility` 均通过。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zi-provider-manifest-export-binding-gate.md`。
  备注：本切片只关闭真实 import verifier 对 attached provider manifest export table 的 token identity gate；不声明
  provider discovery/loading/version selection、standalone import-path wiring、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 04:46:47 +08:00 · 11-S7ZH / 12-S7 manifest export binding gate ·
  状态：11-S7 manifest export provider-binding 前置 gate 支撑子切片完成；完整 11-S7 仍未关闭，
  cross-module provider loading/version binding、standalone provider import-path wiring、完整 metadata sweep/pruning、
  版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：`metadata_runtime.h` 新增 `MANIFEST_EXPORT_NOT_FOUND` 与
  `MANIFEST_EXPORT_TOKEN_MISMATCH` status，并声明
  `ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility()`；`metadata_runtime_binding_compatibility.c`
  先读取 manifest export view，再复用既有 binding compatibility predicate，最后比对 export 发布 token 与
  binding resolved token，给后续 provider binding/load path 一个单点 fail-closed gate。
  RED/GREEN：RED 为 manifest export focused 测试新增 success、export-token mismatch、missing export 与 version drift
  用例后，WSL GCC 构建因缺少 API/status 编译失败；GREEN 后测试扩展为 7/0，且既有 binding compatibility 15/0
  继续通过。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_metadata_runtime_manifest_exports_test` 7/0、
  `zr_vm_metadata_runtime_binding_compatibility_test` 15/0；同三套工具链 CTest
  `metadata_runtime_manifest_exports` 与 `metadata_runtime_binding_compatibility` 均通过。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zh-manifest-export-binding-gate.md`。
  备注：本切片只关闭本地 attached runtime 的 export view + binding compatibility gate；不声明真实跨模块 provider
  选择/加载、standalone provider import-path wiring、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 04:23:15 +08:00 · 11-S7ZG / 12-S7 manifest export runtime view ·
  状态：11-S7 manifest export table runtime view 支撑子切片完成；完整 11-S7 仍未关闭，
  cross-module provider loading/version binding、standalone provider import-path wiring、完整 metadata sweep/pruning、
  版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：新增 `SZrMetadataRuntimeManifestExportView` 与
  `ZrCore_MetadataRuntime_ReadManifestExportView()`；runtime 现在可按 `kind + target` 从 attached manifest export
  table 唯一读取导出 entry、index、target、`typeToken` 和 `memberToken`；重复声明、缺 required token 或 token
  shape mismatch 均 fail closed。实现位于 `metadata_runtime_manifest_exports.c`，避免继续扩大 `metadata_runtime.c`。
  RED/GREEN：RED 为 focused manifest export runtime view 测试编译失败，缺少 public view type 与 API；GREEN 后
  `zr_vm_metadata_runtime_manifest_exports_test` 覆盖 mirror、成功查询、重复 target/kind 拒绝和缺 required token
  拒绝共 4/0。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_metadata_runtime_manifest_exports_test` 4/0、
  `zr_vm_metadata_runtime_query_test` 25/0、`zr_vm_metadata_runtime_binding_compatibility_test` 15/0；同三套工具链 CTest
  `metadata_runtime_manifest_exports`、`metadata_runtime_query`、`metadata_runtime_binding_compatibility` 分别通过。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zg-manifest-export-runtime-view.md`。
  备注：本切片只关闭 attached manifest export table 的 metadata runtime lookup/view API；不声明跨模块
  provider/version binding、standalone provider import-path wiring、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-02 03:57:34 +08:00 · 11-S7ZF / 12-S7 manifest export runtime mirror ·
  状态：11-S7 manifest export table runtime mirror 支撑子切片完成；完整 11-S7 仍未关闭，
  cross-module provider loading/version binding、standalone provider import-path manifest consumption、完整 metadata
  sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：`SZrMetadataRuntime` 新增尾部追加的 `manifestExports/manifestExportCount` mirror 字段；
  `ZrCore_Module_AttachMetadataRuntime()` 从 attached `SZrAotCodeRegistration` 复制 manifest export table pointer/count；
  新增 `tests/module/test_metadata_runtime_manifest_exports.c` 和 CTest `metadata_runtime_manifest_exports`，覆盖 type
  export `TYPE_DEF` token 与 method export `MEMBER_DEF` token 在 runtime mirror 中保持可读。
  RED/GREEN：RED 为新 focused 测试编译失败，`SZrMetadataRuntime` 缺少 manifest export mirror 字段；初始实现把字段插入
  既有计数字段中间，相邻 query 旧布局测试暴露错位风险，改为尾部追加后 GREEN。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_metadata_runtime_manifest_exports_test` 1/0、
  `zr_vm_metadata_runtime_query_test` 25/0、`zr_vm_metadata_runtime_binding_compatibility_test` 15/0；同三套工具链 CTest
  `metadata_runtime_manifest_exports`、`metadata_runtime_query`、`metadata_runtime_binding_compatibility` 均 3/3。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zf-manifest-export-runtime-mirror.md`。
  备注：本切片只关闭 attached metadata runtime 对已发布 generated-C manifest export table 的 pointer/count mirror；
  不声明 provider/version binding、完整 metadata sweep/pruning、standalone provider import-path manifest consumption 或
  full trim analyzer 完成。

- 2026-07-02 03:24:57 +08:00 · 11-S7ZE / 12-S7 manifest export table publication ·
  状态：11-S7 manifest export table generated-C publication 支撑子切片完成；完整 11-S7 仍未关闭，
  cross-module provider loading/version binding、standalone provider manifest consumption、完整 metadata sweep/pruning、
  版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：公共 AOT ABI 新增 `SZrAotManifestExportEntry` kind/flag/table entry 与
  `manifestExports/manifestExportCount`；AOT C emitter 在旧 comment diagnostics 之外生成
  `zr_aot_manifest_exports[]`，并把 table pointer/count 写入 descriptor 与 code registration；manifest export table
  builder 会保留 type export 的 `TYPE_DEF` token、将 method/field export 的 source `MEMBER_DEF` token remap 到
  compacted retained token，并拒绝 invalid kind、null target、invalid bound token 和无 retained remap 的 pruned member。
  RED/GREEN：RED 为 `test_aot_c_zrp_metadata_manifest_export_table_publishes_remapped_member_tokens` 要求 source
  `MEMBER_DEF(7)` 生成 compacted `MEMBER_DEF(2)` table entry，但旧实现只有注释诊断；GREEN 后 focused remap、
  writer-options generated C 和 source contract 均锁定 static table、marker、descriptor/codeRegistration wiring
  与 runtime validation。
  验证：WSL GCC direct `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 9/0、
  `zr_vm_cli_aot_writer_options_test` 18/0、`zr_vm_aot_c_source_contracts_test` 24/0；WSL GCC/clang/Windows MSVC Debug
  focused CTest `aot_c_zrp_metadata_export_token_remap|cli_aot_writer_options` 均 2/2，source contracts 均 24/0。
  产出：`tests/acceptance/2026-07-02-aot-11-s7ze-manifest-export-table-publication.md`。
  备注：本切片只关闭 generated-C descriptor/codeRegistration manifest export table 发布；不声明 provider/version
  binding、完整 metadata sweep/pruning、standalone provider manifest consumption 或 full trim analyzer 完成。

- 2026-07-02 02:22:47 +08:00 · 11-S7ZD / 12-S7 type export declaration type-token binding ·
  状态：11-S7 manifest type export declaration 的 current-module `TYPE_DEF` token binding 支撑子切片完成；
  完整 11-S7 仍未关闭，持久 export manifest/table writer、compacted-token file publication、cross-module provider
  loading/version binding、完整 metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：`SZrAotManifestExportDeclaration` 新增 `hasTypeTokenBinding` / `typeToken`；`compiler_aot_exports.c`
  扫描当前函数 metadata token records，只接受非零 `TYPE_DEF` token，并用 `TYPE_DEF(valueType,stringIndex)`
  signature blob 匹配 exact 或当前 module 前缀的 type export target；generated C 新增
  `manifest.export[i].typeToken = 0x...` 诊断，source contract 固定 writer 字段和诊断格式。
  RED/GREEN：RED 为 `test_cli_aot_writer_options_binds_type_export_declaration_to_type_token` 先引用缺失的
  `hasTypeTokenBinding` / `typeToken` 字段，WSL GCC focused build 编译失败；GREEN 实现后首次 direct run 暴露
  generated-C 期望把 TypeDef 表号误写为 `0x01000001`，修正为项目实际 `TYPE_DEF` token `0x02000001` 后通过。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_cli_aot_writer_options_test` 均 18/0、
  `zr_vm_aot_c_source_contracts_test` 均 24/0、`zr_vm_aot_c_code_stripping_test` 均 10/0；同三套工具链 focused CTest
  `cli_aot_writer_options|aot_c_code_stripping` 均 2/2。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zd-type-export-declaration-type-token-binding.md`。
  备注：本切片只关闭 current-module type export declaration 到当前函数已有 `TYPE_DEF` metadata token 的
  writer-input binding；不声明持久 export table writer、compacted-token file publication、provider version binding
  或完整 11-S7/12-S7 关闭。

- 2026-07-02 01:57:22 +08:00 · 11-S7ZC / 12-S7 field export declaration member-token binding ·
  状态：11-S7 manifest field export declaration 的 current-module `MEMBER_DEF` token binding 支撑子切片完成；
  完整 11-S7 仍未关闭，持久 export manifest/table writer、type export token binding、compacted-token file
  publication、cross-module provider loading/version binding、完整 metadata sweep/pruning、版本/ABI 漂移闭环和 full
  trim analyzer 仍待后续。
  完成项目：`compiler_aot_exports.c` 现在按 writer export declaration kind 选择 typed export symbol kind：
  method declaration 绑定 function symbol，field declaration 绑定 variable symbol，并继续只接受非零 `MEMBER_DEF`
  token；generated C 复用 `manifest.export[i].memberToken = 0x...` 诊断输出 field token。
  RED/GREEN：RED 为 `test_cli_aot_writer_options_binds_field_export_declaration_to_member_token` 在 WSL GCC direct
  run 中失败于 `Expected TRUE Was FALSE`，证明 field declaration 尚未绑定；GREEN 后 `Widget.value` field export
  declaration 绑定到 `0x03000002` 并出现在 generated-C manifest diagnostics。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_cli_aot_writer_options_test` 均 17/0、
  `zr_vm_aot_c_source_contracts_test` 均 24/0、`zr_vm_aot_c_code_stripping_test` 均 10/0；同三套工具链 focused CTest
  `cli_aot_writer_options|aot_c_code_stripping` 均 2/2。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zc-field-export-declaration-member-token-binding.md`。
  备注：本切片只关闭 current-module field export declaration 到已存在 typed exported variable `MEMBER_DEF` token 的
  writer-input binding；不声明持久 export table writer、type target binding、compacted-token file publication、
  provider version binding 或完整 11-S7/12-S7 关闭。

- 2026-07-02 01:43:16 +08:00 · 11-S7ZB / 12-S7 export declaration member-token binding ·
  状态：11-S7 manifest method export declaration 的 current-module `MEMBER_DEF` token binding 支撑子切片完成；
  完整 11-S7 仍未关闭，持久 export manifest/table writer、type/field export token binding、compacted-token file
  publication、cross-module provider loading/version binding、完整 metadata sweep/pruning、版本/ABI 漂移闭环和 full
  trim analyzer 仍待后续。
  完成项目：`SZrAotManifestExportDeclaration` 新增 `hasMemberTokenBinding` / `memberToken`；`compiler_aot_exports.c`
  在 method export declaration target 匹配当前函数 `typedExportedSymbols` 的 function symbol 时绑定非零 `MEMBER_DEF`
  token，并接受 exact target 或当前 module 前缀 target；generated C 新增
  `manifest.export[i].memberToken = 0x...` 诊断，source contract 固定 writer 字段和诊断格式。
  RED/GREEN：RED 为 `test_cli_aot_writer_options_binds_method_export_declaration_to_member_token` 先引用缺失的
  `hasMemberTokenBinding` / `memberToken` 字段，WSL GCC focused build 编译失败；GREEN 后该用例验证 `Factory.make`
  method export declaration 绑定到 `0x03000001` 并出现在 generated-C manifest diagnostics。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_cli_aot_writer_options_test` 均 16/0、
  `zr_vm_aot_c_source_contracts_test` 均 24/0、`zr_vm_aot_c_code_stripping_test` 均 10/0；同三套工具链 focused CTest
  `cli_aot_writer_options|aot_c_code_stripping` 均 2/2。
  产出：`tests/acceptance/2026-07-02-aot-11-s7zb-export-declaration-member-token-binding.md`。
  备注：本切片只关闭 current-module method export declaration 到已存在 typed exported `MEMBER_DEF` token 的 writer-input
  binding；不声明持久 export table writer、type/field target binding、compacted-token file publication、provider version
  binding 或完整 11-S7/12-S7 关闭。

- 2026-07-02 01:09:04 +08:00 · 11-S7ZA / 12-S7 export declaration writer option bridge ·
  状态：11-S7 manifest export declaration writer bridge 支撑子切片完成；完整 11-S7 仍未关闭，持久 export
  manifest/table writer、export target token binding、compacted-token file publication、cross-module provider
  loading/version binding、完整 metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：`SZrAotWriterOptions` 新增 `manifestExportDeclarations` / `manifestExportDeclarationCount`；
  `SZrCliAotPreserveRoots` 新增 CLI-owned export declaration scratch storage；新增 `compiler_aot_exports.{h,c}`
  把 `SZrLibrary_ProjectExportDeclaration` 映射到 writer-level `SZrAotManifestExportDeclaration`；generated C
  输出 `manifest.exports` 和逐项 `manifest.export[i] kind=... target=...` 诊断。
  RED/GREEN：RED 为 `test_cli_aot_writer_options_bridges_manifest_export_declarations` 先引用缺失的 writer option
  字段、preserve-root scratch storage 和 `ZR_AOT_MANIFEST_EXPORT_DECLARATION_*` enum，WSL GCC focused build 编译失败；
  GREEN 后 method/type/field 三类 export declaration 均进入 writer options 并出现在 generated-C manifest diagnostics。
  验证：WSL GCC/clang/Windows MSVC Debug direct `zr_vm_cli_aot_writer_options_test` 均 15/0、
  `zr_vm_aot_c_source_contracts_test` 均 24/0、`zr_vm_aot_c_code_stripping_test` 均 10/0；同三套工具链 focused CTest
  `cli_aot_writer_options|aot_c_source_contracts|aot_c_code_stripping` 匹配已注册的 `cli_aot_writer_options` 与
  `aot_c_code_stripping`，均 2/2。
  产出：`tests/acceptance/2026-07-02-aot-11-s7za-export-declaration-writer-options.md`。
  备注：本切片只关闭 project export declaration -> writer options -> generated-C diagnostics；不声明 token binding、
  持久 export table writer、provider version binding 或完整 11-S7/12-S7 关闭。

- 2026-07-02 00:37:35 +08:00 · 11-S7Z / 12-S7 export manifest declaration model ·
  状态：11-S7 zrp manifest export declaration 输入模型子切片完成；完整 11-S7 仍未关闭，持久 export manifest/table writer、
  export target token binding、compacted-token file publication、cross-module provider loading/version binding、
  完整 metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：`SZrLibrary_Project` 新增 `SZrLibrary_ProjectExportDeclaration` 表、kind 枚举和计数字段；
  新增 `project_exports.{h,c}` 解析/释放模块，接受 `.zrp` 顶层 `exports` 的 `type`/`method`/`field`
  dotted target，并拒绝 unsupported kind 或 unsafe target；`ZrLibrary_Project_New()` / `Free()` 接入该模型，
  `zrp.schema.json` 同步 schema parity。
  RED/GREEN：RED 在 `test_project_manifest_normalization.c` 新增 export declaration 解析/拒绝用例后，WSL GCC
  focused build 编译失败于缺失 `exportDeclarationCount`、`exportDeclarations` 和
  `ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_*`；GREEN 后独立 parser 模块、project model、schema 和释放路径补齐。
  验证：WSL GCC direct `zr_vm_project_manifest_normalization_test` 28/0；WSL clang direct 同 28/0；
  Windows MSVC Debug direct 同 28/0。该 executable 当前未注册成 CTest，`ctest -R project_manifest_normalization`
  未匹配测试。
  产出：`tests/acceptance/2026-07-02-aot-11-s7z-zrp-manifest-export-declarations.md`。
  备注：本切片只关闭 manifest declaration input；不声明真实持久 export table writer、token remap 写入文件、
  provider version/ABI binding 或完整 11-S7/12-S7 关闭。

- 2026-07-02 00:11:02 +08:00 · 11-S7 support / 12-S7ZZQ runtime export member-token publication ·
  状态：11-S7 export token publication 支撑子切片完成；完整 11-S7 仍未关闭，持久 export manifest/table writer、
  更完整 cross-module provider loading/version binding、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer
  仍待后续。
  完成项目：`ZrCore_Module_AttachMetadataRuntime()` 现在镜像 `memberTokenRemapCount`，并在 attach AOT metadata runtime 时把
  `SZrAotCodeRegistration.memberTokenRemaps` 写回 loaded entry function 的 `typedExportedSymbols`。AOT-loaded provider 对外暴露的
  `MEMBER_DEF` token 因而从 source RID 切换为 retained compacted RID，现有跨模块 import signature matcher 可直接读取 compacted
  export token；非 member token 和没有 remap 的 retained token 保持原值。
  RED/GREEN：RED 在 `test_metadata_runtime_query.c` 新增 runtime export member-token remap 用例后，WSL GCC 编译失败于
  `SZrMetadataRuntime` 缺少 `memberTokenRemapCount`；GREEN 后新增 runtime mirror 字段与 module attach writeback helper。
  验证：WSL GCC/clang direct `zr_vm_metadata_runtime_query_test` 均为 25/0；WSL GCC/clang CTest
  `metadata_runtime_query|metadata_runtime_binding_compatibility|metadata_type_ref_binding` 均为 3/3；
  Windows MSVC Debug direct `zr_vm_metadata_runtime_query_test` 25/0，同 metadata CTest 3/3；WSL GCC/clang 与 MSVC Debug CTest
  `aot_c_code_stripping|aot_c_zrp_metadata_export_token_remap|aot_c_descriptor_diagnostics` 均为 3/3。
  产出：`tests/acceptance/2026-07-02-aot-12-s7zzq-runtime-export-member-token-publication.md`。
  备注：本切片只关闭 codeRegistration sidecar 到 runtime provider typed export table 的写回；不声明独立持久 export manifest/table
  writer、完整 provider version binding、完整 metadata sweep/pruning 或 full trim analyzer 完成。

- 2026-07-01 23:36:12 +08:00 · 11-S7 support / 12-S7ZZP signature-rooted ModuleRef retention ·
  状态：11-S7 emitted zrp metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target/provider binding、
  真实 export manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer
  仍待后续。
  完成项目：ModuleRef retained-row 判断现在除 retained import `TYPE_REF`/`MEMBER_REF` token-record root 外，还会扫描
  retained signature blob remap 中的 `ASSEMBLY_REF` 节点；signature remap 在 ModuleRef retained count/string remap 前构建，
  所以 signature-only ModuleRef row、name string 和 requested/min/max version strings 都会随 compacted metadata 保留。
  RED/GREEN：RED 使用 retained `FIELD_SIG(ASSEMBLY_REF source RID2)` 且无 retained import token-record root 的 fixture，
  旧 pruning 未把 signature blob 当作 ModuleRef root，WSL GCC pool pruning 失败为 `Expected TRUE Was FALSE`；GREEN 后
  `backend_aot_c_zrp_retained_signature_blobs_reference_module_ref()` 锁定该 root 路径，source contract 覆盖
  `ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF` scanner。
  验证：WSL GCC direct zrp metadata pruning 10/0、TypeSpec pruning 2/0、pool pruning 8/0、export token remap 8/0、source
  contracts 24/0，focused metadata CTest 4/4；WSL Clang 同 direct set 10/0、2/0、8/0、8/0、24/0，focused metadata
  CTest 4/4；Windows MSVC Debug direct 同 set 10/0、2/0、8/0、8/0、24/0，focused metadata CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzp-signature-rooted-module-ref-retention.md`。
  备注：本切片不声明跨模块 provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-01 22:51:15 +08:00 · 11-S7 support / 12-S7ZZO signature MemberRef token rewrite ·
  状态：11-S7 emitted zrp metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target/provider binding、
  真实 export manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer
  仍待后续。
  完成项目：retained signature blob rewrite 现在在 `MEMBER_REF` signature node 上读取内嵌 token，复用
  `backend_aot_c_zrp_remap_token_record()` 的 MethodDef/FieldDef compacted member-token 语义，并把 compacted RID 写回
  signature blob 后再进入结构校验和 signature hash recomputation。
  RED/GREEN：RED 使用 retained MethodDef source RID2 + pruned MethodDef RID1 的 fixture，旧 signature rewrite 保留 RID2，
  WSL GCC zrp metadata pruning 失败为 `Expected 50331649 Was 50331650`；GREEN 后 signature blob 内 token 写为 compacted
  RID1，source contract 锁定 MemberRef rewrite helper 与 token-record remap 调用。
  验证：WSL GCC direct zrp metadata pruning 10/0、TypeSpec pruning 2/0、pool pruning 7/0、export token remap 8/0、source
  contracts 24/0，focused metadata CTest 4/4；WSL Clang 同 direct set 10/0、2/0、7/0、8/0、24/0，focused metadata
  CTest 4/4；Windows MSVC Debug direct 同 set 10/0、2/0、7/0、8/0、24/0，focused metadata CTest 4/4；focused code
  `git diff --check` 无 whitespace error。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzo-signature-member-ref-token-rewrite.md`。
  备注：本切片不声明跨模块 provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-01 22:34:08 +08:00 · 11-S7 support / 12-S7ZZN signature AssemblyRef token rewrite ·
  状态：11-S7 emitted zrp metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target/provider binding、
  真实 export manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer
  仍待后续。
  完成项目：retained signature blob rewrite context 现在接收 TypeSpec/ModuleRef rows；`ASSEMBLY_REF` signature node
  不再只跳过 token，而是通过 ModuleRef retained/compacted remap helper 写回 compacted AssemblyRef RID。
  RED/GREEN：RED 使用 live source `ASSEMBLY_REF` RID2 加 orphan RID1 的 ModuleRef fixture，旧 signature rewrite 保留 RID2，
  WSL GCC pool pruning 失败为 `Expected 67108865 Was 67108866`；GREEN 后 signature blob 内 token 写为 compacted RID1，
  source contract 锁定 ModuleRef context 与 signature rewrite helper。
  验证：WSL GCC direct zrp metadata pruning 9/0、TypeSpec pruning 2/0、pool pruning 7/0、export token remap 8/0、source
  contracts 24/0，focused metadata CTest 4/4；WSL Clang 同 direct set 9/0、2/0、7/0、8/0、24/0，focused metadata CTest
  4/4；Windows MSVC Debug direct 同 set 9/0、2/0、7/0、8/0、24/0，focused metadata CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzn-signature-assembly-ref-token-rewrite.md`。
  备注：本切片不声明跨模块 provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-01 22:11:32 +08:00 · 11-S7 support / 12-S7ZZM GenericParamConstraint TypeSpec root retention ·
  状态：11-S7 emitted zrp metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target/provider binding、
  真实 export manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer
  仍待后续。
  完成项目：TypeSpec retained-row predicate/count/remap/copy helper 现在接收 GenericParamConstraint 表上下文，并把 surviving
  GenericParamConstraint 约束行视为 TypeSpec 保留根；signature blob remap 同步使用该上下文保留 constraint-rooted TypeSpec
  signature payload。
  RED/GREEN：RED 使用没有 retained TypeSpec token record、仅由 retained GenericParamConstraint 指向 source `TYPE_SPEC`
  RID2 的 fixture，旧实现删除该 TypeSpec row 后 pruning 返回 false（`Expected TRUE Was FALSE`）；GREEN 后 TypeSpec row
  压缩为 RID1，constraint token 与 signature blob offset 均同步 remap，source contract 锁定 constraint-rooted TypeSpec helper。
  验证：WSL GCC direct zrp metadata pruning 9/0、TypeSpec pruning 2/0、pool pruning 6/0、export token remap 8/0、source
  contracts 24/0，focused metadata CTest 4/4；WSL Clang 同 direct set 9/0、2/0、6/0、8/0、24/0，focused metadata CTest
  4/4；Windows MSVC Debug direct 同 set 9/0、2/0、6/0、8/0、24/0，focused metadata CTest 4/4；focused code
  `git diff --check` 无 whitespace error。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzm-generic-param-constraint-typespec-root-retention.md`。
  备注：本切片不声明跨模块 provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-01 21:41:02 +08:00 · 11-S7 support / 12-S7ZZL GenericParamConstraint TypeSpec token remap ·
  状态：11-S7 emitted zrp metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target/provider binding、
  真实 export manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer
  仍待后续。
  完成项目：GenericParamConstraint copy path 现在接收 TypeSpec rows/count，并在 existing direct TypeDef token remap 后，
  对 retained `constraintTypeToken` 调用 TypeSpec remap helper；当 TypeSpec table 因 orphan row 删除而压缩时，retained
  constraint row 会同步发布 compacted TypeSpec RID。
  RED/GREEN：RED 使用 source TypeSpec RID1 deleted / RID2 retained 的 generic parameter constraint fixture，旧 pruner 在
  WSL GCC zrp metadata pruning 中输出 source RID2，失败为 `Expected 117440513 Was 117440514`；GREEN 后该路径 8/0，并由
  source contract 锁定 `backend_aot_c_zrp_remap_type_spec_token(&row.constraintTypeToken, ...)`。
  验证：WSL GCC direct zrp metadata pruning 8/0、TypeSpec pruning 2/0、pool pruning 6/0、source contracts 24/0，focused
  metadata CTest 4/4；WSL Clang 同 direct set 8/0、2/0、6/0、24/0，focused metadata CTest 4/4；Windows MSVC Debug direct
  同 set 8/0、2/0、6/0、24/0，focused metadata CTest 4/4；focused code `git diff --check` 无 whitespace error。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzl-generic-param-constraint-typespec-remap.md`。
  备注：本切片不声明跨模块 provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-01 21:14:56 +08:00 · 11-S7 support / 12-S7ZZK signature-token orphan rejection ·
  状态：11-S7 emitted zrp metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target/provider binding、
  真实 export manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer
  仍待后续。
  完成项目：retained signature token remap helper 现在要求每个 retained local `SIGNATURE` token 都能在 retained
  signature token-record 顺序中找到对应发布项；缺失时 pruning fail-closed。`prepare_embedded_zrp_metadata` 的失败路径同步
  release 输出，确保 caller 不会在 false 返回后继续看到 source blob/length。
  RED/GREEN：RED 使用 retained MethodSpec row 引用 `SIGNATURE` RID11、但 token-record stream 不保留该 signature record
  的 fixture，旧 pruner 在 WSL GCC zrp metadata pruning 中返回 true；实现拒绝后第二个 RED 暴露输出清理缺口，失败时 length
  仍为 659；GREEN 后缺失 signature token-record 会使 pruning 返回 false 且 embedded metadata 输出清空。
  验证：WSL GCC direct zrp metadata pruning 7/0、typedef pruning 2/0、pool pruning 6/0、source contracts 24/0，focused
  metadata CTest 4/4；WSL Clang direct 同组 7/0、2/0、6/0、24/0，focused metadata CTest 4/4；Windows MSVC Debug direct
  同组 7/0、2/0、6/0、24/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzk-signature-token-orphan-rejection.md`。
  备注：本切片不声明跨模块 provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-01 20:55:12 +08:00 · 11-S7 support / 12-S7ZZJ retained SIGNATURE token RID compaction ·
  状态：11-S7 emitted zrp metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target/provider binding、
  真实 export manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer
  仍待后续。
  完成项目：retained signature token remap helper 现在根据经过 member/TypeDef/TypeSpec/ModuleRef pruning 后仍保留的
  signature token-record 顺序，为本地 `SIGNATURE` token 生成 compacted RID；token-record copy 和 MethodSpec row copy
  使用同一 helper 改写 retained signature token 字段，避免 pruned metadata 中 signature token table 出现 source RID 空洞。
  RED/GREEN：RED 将 FieldDef signature、MethodSpec token record 和 MethodSpec row token 从 source RID9/RID11 期望改为
  compacted RID1 后，旧 pruner 在 WSL GCC zrp metadata pruning 中失败 2/6；GREEN 后 FieldDef signature、MethodSpec signature、
  AssemblyRef signature 与 TypeRef signature token records 均发布 compacted local `SIGNATURE` RIDs。
  验证：WSL GCC direct zrp metadata pruning 6/0、typedef pruning 2/0、pool pruning 6/0、source contracts 24/0，focused
  metadata CTest 4/4；WSL Clang direct 同组 6/0、2/0、6/0、24/0，focused metadata CTest 4/4；Windows MSVC Debug direct
  同组 6/0、2/0、6/0、24/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzj-signature-token-rid-compaction.md`。
  备注：本切片不声明跨模块 provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-01 20:17:40 +08:00 · 11-S7 support / 12-S7ZZI member-token remap retained-count guard ·
  状态：11-S7 metadata publication/remap sidecar 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target/provider
  binding、真实 export manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和
  full trim analyzer 仍待后续。
  完成项目：direct export-token remap helper 与 retained member-token remap sidecar build path 现在都用
  `backend_aot_c_zrp_count_retained_method_defs()` 重新计算实际 retained MethodDef count，并拒绝与传入
  `retainedMethodDefCount` 不一致的调用，防止 generated ABI remap 表或 direct export-token rewrite 返回带 compacted target
  RID 空洞的 member-token 映射；source contract 锁定两侧 consistency guard。
  RED/GREEN：RED 使用实际 retained MethodDef count 为 1、传入 count 为 2 的 MethodDef+FieldDef fixture，旧 direct helper
  与 builder 均返回 true 并会把 FieldDef target 推到 RID3，WSL GCC 两次失败 `Expected FALSE Was TRUE`；GREEN 后 direct
  remap 失败且保留原 token，sidecar build 失败并保持 metadata remap state 清空。
  验证：WSL GCC direct export-token remap 8/0、source contracts 24/0、registered remap CTest 1/1；WSL Clang direct
  export-token remap 8/0、source contracts 24/0、registered remap CTest 1/1；Windows MSVC Debug direct export-token remap
  8/0、source contracts 24/0、registered remap CTest 1/1。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzi-member-token-remap-sidecar-retained-count-guard.md`。
  备注：本切片不声明跨模块 provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-01 19:57:11 +08:00 · 11-S7 support / 12-S7ZZH member-token remap sidecar token-shape guard ·
  状态：11-S7 metadata publication/remap sidecar 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target/provider
  binding、真实 export manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和
  full trim analyzer 仍待后续。
  完成项目：retained member-token remap sidecar 的 append path 现在要求 source/target token 都是非零 RID 的
  `MEMBER_DEF`，生成侧在 ABI carrier 发布前拒绝非 member token 或 RID 0 token；focused export-token remap test 新增
  `TYPE_DEF` source token 与 `MEMBER_DEF` RID 0 source token 两条失败路径，source contract 锁定 source/target shape guard。
  RED/GREEN：RED 先后用 retained MethodDef 的 `TYPE_DEF` source token 与 `MEMBER_DEF` RID 0 source token 触发，旧 builder
  均返回 true，WSL GCC 两次失败 `Expected FALSE Was TRUE`；GREEN 后 build 失败并保持 metadata remap state 清空。
  验证：WSL GCC direct export-token remap 6/0、source contracts 24/0、registered remap CTest 1/1；WSL Clang direct
  export-token remap 6/0、source contracts 24/0、registered remap CTest 1/1；Windows MSVC Debug direct export-token remap
  6/0、source contracts 24/0、registered remap CTest 1/1。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzh-member-token-remap-sidecar-token-shape-guard.md`。
  备注：本切片不声明跨模块 provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-01 19:43:02 +08:00 · 11-S7 support / 12-S7ZZG member-token remap sidecar source duplicate guard ·
  状态：11-S7 metadata publication/remap sidecar 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target/provider
  binding、真实 export manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和
  full trim analyzer 仍待后续。
  完成项目：retained member-token remap sidecar 的 build path 现在在 append 阶段拒绝重复 `sourceToken`，避免后续
  `SZrAotMemberTokenRemap` ABI carrier 发布 ambiguous source-token mapping；focused export-token remap test 新增
  MethodDef/FieldDef 重复 source token 拒绝路径，source contract 锁定 helper 内部比较。
  RED/GREEN：RED 使用同一 `MEMBER_DEF` token 同时作为 retained MethodDef 和 FieldDef source，旧 builder 返回 true，
  WSL GCC 失败 `Expected FALSE Was TRUE`；GREEN 后 build 失败并保持 metadata remap state 清空。
  验证：WSL GCC direct export-token remap 4/0、source contracts 24/0、registered remap CTest 1/1；WSL Clang direct
  export-token remap 4/0、source contracts 24/0、registered remap CTest 1/1；Windows MSVC Debug direct export-token remap
  4/0、source contracts 24/0、registered remap CTest 1/1；`git diff --check` exit 0，仅输出既有 LF/CRLF 提示。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzg-member-token-remap-sidecar-source-duplicate-guard.md`。
  备注：本切片不声明跨模块 provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-01 19:27:51 +08:00 · 11-S7 support / 12-S7ZZF member-token remap ABI duplicate validation ·
  状态：11-S7 metadata publication/ABI drift validation 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target/provider
  binding、真实 export manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和
  full trim analyzer 仍待后续。
  完成项目：已发布的 `SZrAotMemberTokenRemap` ABI surface 现在由 root runtime 与 mirrored AOT runtime 校验唯一性；
  `sourceToken` 和 `targetToken` 在同一 remap table 内都不能重复，否则 descriptor validation 在执行前失败并带出
  index/previousIndex/token。source contract 同步锁定 root/mirrored runtime 的 duplicate source/target guards。
  RED/GREEN：RED 使用两条合法 `MEMBER_DEF` entry 但 source 或 target 重复的手写 descriptor 动态库，旧实现没有
  duplicate guard，WSL GCC 两个新用例均失败 `Expected FALSE Was TRUE`；GREEN 后分别报错
  `member token remap duplicate sourceToken` 与 `member token remap duplicate targetToken`。
  验证：WSL GCC direct descriptor diagnostics 5/0、source contracts 24/0、focused CTest 4/4；WSL Clang direct
  descriptor diagnostics 5/0、source contracts 24/0、focused CTest 4/4；Windows MSVC Debug descriptor diagnostics
  5/0/5 ignored、source contracts 24/0、focused CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzf-member-token-remap-duplicate-validation.md`。
  备注：本切片不声明跨模块 provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-01 19:04:27 +08:00 · 11-S7 support / 12-S7ZZE member-token remap ABI entry validation ·
  状态：11-S7 metadata publication/ABI drift validation 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target/provider
  binding、真实 export manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和
  full trim analyzer 仍待后续。
  完成项目：已发布的 `SZrAotMemberTokenRemap` ABI surface 现在由 root runtime 与 mirrored AOT runtime 逐项校验；
  `sourceToken/targetToken` 必须是非零本地 `MEMBER_DEF` token，否则 descriptor validation 在执行前失败并带出
  index/sourceToken/targetToken。source contract 同步锁定 root/mirrored runtime 的 entry validator。
  RED/GREEN：RED 使用 ABI pointer/count 形态合法但 `sourceToken=0x02000001` 的手写 descriptor 动态库，旧实现没有
  entry table/RID 校验，WSL GCC 失败 `Expected FALSE Was TRUE`；GREEN 后报错 `member token remap entry invalid`。
  验证：WSL GCC direct descriptor diagnostics 3/0、source contracts 24/0、focused CTest 4/4；WSL Clang direct
  descriptor diagnostics 3/0、source contracts 24/0、focused CTest 4/4；Windows MSVC Debug descriptor diagnostics
  3/0/3 ignored、source contracts 24/0、focused CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zze-member-token-remap-entry-validation.md`。
  备注：本切片不声明跨模块 provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-01 18:43:00 +08:00 · 11-S7 support / 12-S7ZZD member-token remap ABI publication ·
  状态：11-S7 cross-module metadata publication/rewrite 支撑子切片完成；完整 11-S7 仍未关闭，跨模块
  target/provider binding、真实 export manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI
  漂移闭环和 full trim analyzer 仍待后续。
  完成项目：`SZrAotCEmbeddedZrpMetadata` 里的 retained member-token remap sidecar 现在不只供
  `zr_aot_method_tokens[]` 内部改写使用，还通过公共 ABI 发布为 `SZrAotMemberTokenRemap` 表；`SZrAotCodeRegistration`
  和 `ZrAotCompiledModule` 都携带 `memberTokenRemaps/memberTokenRemapCount`，runtime descriptor validation 对表指针、
  数量和 null/count 形态做一致性校验。
  RED/GREEN：RED 使用 MethodDef source RID2 被 pruning compact 到 RID1 的 generated-C fixture，旧实现没有 ABI remap
  table；GREEN 后生成 C 同时包含 `code_stripping.memberTokenRemaps = 1` marker、source/target token marker、
  `zr_aot_member_token_remaps[]` 和两处 descriptor/codeRegistration 绑定。
  验证：WSL GCC direct code stripping 10/0、source contracts 24/0、export-token remap 3/0，focused CTest 4/4；
  WSL Clang direct source contracts 24/0、focused CTest 4/4；Windows MSVC Debug direct source contracts 24/0、
  focused CTest 4/4。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzd-member-token-remap-abi-publication.md`。
  备注：本切片不声明跨模块 provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning
  或 full trim analyzer 完成。

- 2026-07-01 18:09:50 +08:00 · 11-S7 support / 12-S7ZZC signature blob embedded TypeDef token rewrite ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target/provider binding、cross-module export
  manifest/table publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：`backend_aot_c_zrp_metadata_signature.{h,c}` 新增 retained signature blob TypeDef token rewrite API；pruned blob
  在 signature pool copy 后递归扫描 retained `METHOD_SIG`、`FIELD_SIG` 与 standalone type-node blob，将 direct `TYPE_DEF`
  type-node payload token 改写为 compacted TypeDef RID，并对重写后的 blob 做结构校验。该步骤复用 `backend_aot_c_zrp_metadata_type_def`
  的 retained-row/compacted-token 规则，且发生在 token-record signature hash 重算前。
  RED/GREEN：RED 使用 retained FieldDef `FIELD_SIG(TYPE_DEF(... source RID2))` fixture，source RID1 被 pruning 后旧实现保留
  embedded RID2，WSL GCC 失败 `Expected 33554433 Was 33554434`。GREEN 后 embedded token、FieldDef owner、token record owner
  均收敛到 compacted TypeDef RID1，signature hash 从最终 retained blob 重新计算。
  验证：WSL GCC direct TypeDef pruning 2/0；WSL Clang direct TypeDef pruning 2/0；Windows MSVC Debug direct TypeDef pruning 2/0；
  focused CTest `zrp_metadata|aot_c_zrp_metadata|metadata_module_hash` 在 WSL GCC、WSL Clang、Windows MSVC Debug 均为 8/8。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzc-signature-typedef-token-rewrite.md`。
  备注：本切片只关闭 retained signature blob 内 direct `TYPE_DEF` token 的 pruning-time rewrite；不声明 TypeRef/provider
  binding、cross-module export manifest/table publication、完整 metadata sweep/pruning、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 17:44:55 +08:00 · 11-S7 support / 12-S7ZZB TypeDef RID compaction ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、cross-module export manifest/table
  publication/rewrite、完整 zrp metadata sweep/pruning、版本/ABI 漂移闭环和 full trim analyzer 仍待后续。
  完成项目：TypeDef retained 统计从 prefix count 改为 retained row count，并新增 compacted TypeDef token/remap/copy helper；pruned
  token records、MethodDef.ownerTypeToken、FieldDef.ownerTypeToken、GenericParam.ownerToken 与 GenericParamConstraint.constraintTypeToken
  均对 direct TypeDef token 做 compacted RID rewrite；TypeDef section copy 现在跳过 interior orphan rows，string/signature pool 收集也按
  retained TypeDef row 判定。
  RED/GREEN：RED 使用 source RID1 orphan / RID2 live 的 TypeDef fixture，旧实现因 prefix 模型直接 identity-exit，失败为 owned pruned blob
  `Expected Non-NULL`。GREEN 后 retained TypeDef row 发布为 compacted RID1，token record owner 和 MethodDef owner 均重写到 compacted RID，
  stringPool 只保留 live type/method 字符串。
  验证：Windows MSVC Debug direct runs：TypeDef pruning 1/0、direct zrp pruning 6/0、TypeSpec pruning 2/0、pool pruning 6/0、source contracts
  24/0；WSL GCC/Clang 均完成配置、focused builds 与同五个 direct runs，结果均为 1/0、6/0、2/0、6/0、24/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zzb-typedef-rid-compaction.md`。
  备注：本切片不声明 cross-module target/provider binding、export manifest/table publication、完整 metadata sweep/pruning 或 full trim
  analyzer 完成。

- 2026-07-01 17:10:03 +08:00 · 11-S7 support / 12-S7ZZA TypeDef trailing orphan sweep ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、cross-module export manifest/table
  publication/rewrite、TypeDef RID compaction、完整 zrp metadata sweep/pruning 和版本/ABI 漂移闭环仍待后续。
  完成项目：`backend_aot_c_zrp_metadata_type_def.{h,c}` 新增 retained TypeDef prefix 统计；retained root 来源为 retained token
  record、retained MethodDef owner、FieldDef owner 和 TypeDef-owned GenericParam。pruned metadata header 现在重写 TypeDef
  count/byteLength，TypeDef copy 仅复制 retained prefix，string/signature pool 收集也只保留 prefix 内 TypeDef 引用的 slices。
  RED/GREEN：RED 为 trailing orphan TypeDef fixture 期望 TypeDef rows 2→1、orphan type strings 消失；旧实现 raw-copy TypeDef
  section 并保留 580-byte blob，失败为 `Expected 510 Was 580`。GREEN 后 retained prefix 输出 1 条 TypeDef row，MethodDef owner
  token 保持 source RID1，stringPool 收缩为 live type namespace + method name。
  验证：Windows MSVC Debug direct runs：direct zrp pruning 6/0、pool pruning 6/0、TypeSpec pruning 2/0、export-token remap 3/0、
  zrp size deltas 2/0、code stripping 10/0、source contracts 24/0、frame setup contracts 1/0；Windows focused CTest 6/6；
  WSL GCC/Clang focused builds 均通过，focused CTest 各 6/6，并显式 direct 通过 source contracts 24/0、frame setup contracts 1/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zza-typedef-trailing-orphan-sweep.md`。
  备注：这只关闭 trailing TypeDef suffix sweep；interior TypeDef RID compaction/remap、cross-module target/provider binding、
  export manifest/table publication、完整 metadata sweep/pruning 和 full trim analyzer 仍未完成。

- 2026-07-01 16:40:57 +08:00 · 11-S7 support / 12-S7ZZ ModuleRef orphan sweep ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、cross-module export manifest/table
  publication/rewrite、完整 zrp metadata sweep/pruning 和版本/ABI 漂移闭环仍待后续。
  完成项目：新增 `backend_aot_c_zrp_metadata_module_ref.{h,c}`，按 retained `TYPE_REF`/`MEMBER_REF` token record root 判断
  ModuleRef row 是否保留；pruned token-record count/copy 在 member-token 与 TypeSpec remap 后继续 remap AssemblyRef token 字段；
  ModuleRef section copy 现在丢弃 self-only orphan AssemblyRef rows，并把 retained row token/name/version offsets 重写到 compacted
  metadata；string/signature pool 收集逻辑同步使用 ModuleRef-aware retained-token-record 判定。
  RED/GREEN：RED 为 orphan ModuleRef fixture 期望删除 self-only AssemblyRef/signature token records、删除 orphan ModuleRef row，并把
  live AssemblyRef RID2 改写为 compacted RID1；旧实现 raw-copy ModuleRef section 并 identity-exit，失败为 owned pruned blob
  `Expected Non-NULL`。GREEN 后 retained ModuleRef row/token-record 全部发布 compacted `ASSEMBLY_REF` RID1。
  验证：Windows MSVC Debug direct runs：pool pruning 6/0、direct zrp pruning 5/0、TypeSpec pruning 2/0、export-token remap 3/0、
  zrp size deltas 2/0、code stripping 10/0、source contracts 24/0、frame setup contracts 1/0；Windows focused CTest 6/6；
  WSL GCC/Clang focused CTest 各 6/6，并显式 direct 通过 source contracts 24/0、frame setup contracts 1/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zz-module-ref-orphan-sweep.md`。
  备注：这只关闭 retained import root 引用下的 ModuleRef row sweep 与 AssemblyRef RID compaction；不声明 cross-module export
  manifest/table publication、cross-module target/provider binding、完整 metadata sweep/pruning、annotation policy 或 full trim analyzer
  完成。

- 2026-07-01 16:14:53 +08:00 · 11-S7 support / 12-S7ZY TypeSpec RID compaction ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、cross-module export manifest/table
  publication/rewrite、完整 zrp metadata sweep/pruning 和版本/ABI 漂移闭环仍待后续。
  完成项目：retained TypeSpec rows 现在用 compacted TypeSpec RID 重写 row token；token-record pruning/copy 现在在 MethodDef/FieldDef
  member-token remap 后继续 remap TypeSpec token 字段，并在 source TypeSpec row 已被 pruning 时丢弃对应 token record；
  signature-pool collection 使用同一 retained-token-record 判定。
  RED/GREEN：RED 为 retained source `TYPE_SPEC` RID2 在第一条 TypeSpec row 被 pruning 后仍发布为 `0x07000002u`；GREEN 后 row/token
  record 均发布 compacted `0x07000001u`，source contracts 锁定 TypeSpec remap helper 边界。
  验证：Windows MSVC Debug direct runs：TypeSpec pruning 2/0、direct zrp pruning 5/0、pool pruning 5/0、export-token remap 3/0、
  zrp size deltas 2/0、code stripping 10/0、source contracts 24/0、frame setup contracts 1/0；Windows focused CTest 6/6；
  WSL GCC/Clang focused CTest 各 6/6，并显式 direct 通过 source contracts 24/0、frame setup contracts 1/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zy-typespec-rid-compaction.md`。
  备注：这只关闭 retained TypeSpec row/token-record RID compaction；不声明 cross-module export manifest/table publication、
  cross-module target/provider binding、完整 metadata sweep/pruning、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 15:57:26 +08:00 · 11-S7 support / 12-S7ZX published export member-token remap ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、cross-module export manifest/table
  publication/rewrite、TypeSpec RID compaction、完整 zrp metadata sweep/pruning 和版本/ABI 漂移闭环仍待后续。
  完成项目：`SZrAotCEmbeddedZrpMetadata` 新增 retained member-token remap sidecar；非 identity metadata pruning 会为 retained
  MethodDef/FieldDef rows 生成 compacted `sourceToken -> targetToken` 映射；generated method-token table 使用该映射发布
  compacted MethodDef token，并在 token 已被 pruning 丢弃时输出 `0u`。
  RED/GREEN：RED 为 focused export-token remap test 因缺失
  `backend_aot_c_zrp_metadata_member_token.h` 编译失败，且 code-stripping generated-C 断言覆盖 stale RID2 输出；GREEN 后
  member-token helper、prune sidecar、method metadata writer 和 source/frame contracts 通过。
  验证：Windows MSVC Debug direct runs：export-token remap 3/0、code stripping 10/0、source contracts 24/0、frame setup
  contracts 1/0、direct zrp pruning 5/0、TypeSpec pruning 1/0、pool pruning 5/0；Windows focused CTest 6/6；WSL GCC/Clang
  focused CTest 各 6/6，并显式 direct 通过 source contracts 24/0、frame setup contracts 1/0。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zx-published-export-member-token-remap.md`。
  备注：这只关闭 generated method-token publication 的 compacted retained member-token rewrite；不声明 cross-module export
  manifest/table publication、TypeSpec RID compaction、完整 metadata sweep/pruning、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 15:30:06 +08:00 · 11-S7 support / 12-S7ZW TypeSpec orphan sweep ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、cross-module export-token
  publication/rewrite、完整 zrp metadata sweep/pruning 和版本/ABI 漂移闭环仍待后续。
  完成项目：`backend_aot_c_zrp_metadata_type_spec.{h,c}` 现在提供 retained TypeSpec row predicate/count/copy helper；
  pruning layout 以 retained `TYPE_SPEC` token record 为 root 计算 TypeSpec section count，copy 阶段丢弃失去 live token
  record 的 TypeSpec rows，并通过 signature blob remap 重写 retained row offset/hash。signature-pool 收集逻辑改为只收集
  retained TypeSpec rows 的 signature slices，避免 orphan TypeSpec blob 留在 after-trim pool。
  RED/GREEN：RED 为新增 TypeSpec pruning fixture 期望 token-record-orphaned TypeSpec row 被删除、signatureBlobPool 归零，
  但旧 pruner 仍 raw-copy TypeSpec section，长度为 517 bytes；GREEN 后 focused zrp pruning set 通过。
  验证：Windows MSVC Debug direct runs：TypeSpec pruning 1/0、direct zrp pruning 5/0、pool pruning 5/0、source
  contracts 24/0、code stripping 10/0、zrp size deltas 2/0、export-token remap 2/0；WSL GCC/Clang 同组 build/direct
  run 通过；focused CTest 三套环境均为 6/6；`git diff --check` exit 0，仅有既有 line-ending warnings。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zw-typespec-orphan-sweep.md`。
  备注：这只关闭 TypeSpec row/signature blob 的 token-record-rooted orphan sweep；不声明 TypeSpec RID compaction、
  cross-module export-token publication、完整 metadata sweep/pruning、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 14:51:47 +08:00 · 11-S7 support / 12-S7ZV FieldDef default-value constant-pool remap ·
  状态：11-S7 metadata pruning 支撑子切片完成；完整 11-S7 仍未关闭，跨模块 target、cross-module export-token
  publication/rewrite、完整 zrp metadata sweep/pruning 和版本/ABI 漂移闭环仍待后续。
  完成项目：zrp metadata row ABI 升到 v3，FieldDef row 持久化 default-value constant-pool offset/length；
  `ZrCore_ZrpMetadata_Validate()` 校验 empty slice 与 non-empty slice 范围；AOT C emitted metadata pruner 新增
  constant-pool remap 模块，收集 retained FieldDef default slices、构建 compacted constantPool、重写 retained FieldDef
  offset/length，并继续移除未被 row 引用的 orphan constant payload。
  RED/GREEN：RED 为 focused MSVC build 因 `SZrZrpMetadataFieldDefRow` 缺少
  `defaultValueConstantPoolOffset/defaultValueConstantPoolLength` 而失败；GREEN 后 format/pool pruning/pruning/source
  contracts/code stripping/size deltas/CLI set 通过。
  验证：Windows MSVC Debug direct runs：metadata format 12/0、pool pruning 5/0、direct zrp pruning 5/0、source
  contracts 24/0、code stripping 10/0、zrp size deltas 2/0、CLI dump/version 与 CLI args 通过；WSL GCC/Clang 同组
  build/direct run 通过；focused CTest 三套环境均为 7/7；`git diff --check` exit 0，仅有既有 line-ending warnings。
  产出：`tests/acceptance/2026-07-01-aot-12-s7zv-field-default-constant-pool-remap.md`。
  备注：这只关闭 FieldDef default-value constant-pool retained-slice remap；不声明 cross-module export-token publication、
  完整 metadata sweep/pruning、annotation policy 或 full trim analyzer 完成。

- 2026-07-01 14:05:25 +08:00 · 11-S4BN / 10-S4Z28 FieldInfo nested primitive POD storage-width path matrix consumer ·
  状态：11-S4 attached metadata runtime 的 FieldDef signature/layout consumer 已覆盖 nested primitive path 的
  int8、int16、int64、uint8、uint16、uint64、float32 read/write；完整 signature-derived binding、cross-module
  FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
  完成项目：focused fixture 复用同一 FieldInfo object/native-pointer carrier、FieldDef token、outer/intermediate
  `SZrTypeLayoutField.typeLayoutIndex` 和 leaf `typeLayoutIndex == NONE` guard，通过
  `configure_nested_primitive_path_field_sizes()` 调整 retained layout byte-size/align，验证 storage-width raw child
  均可通过 shared primitive POD guard 读写。未新增 metadata row、token ABI 或 code-registration ABI。
  RED/GREEN：coverage GREEN；新增 storage-width matrix 后 Windows MSVC Debug focused `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均构建
  `zr_vm_reflection_token_resolve_test`、`zr_vm_metadata_runtime_query_test`、`zr_vm_metadata_runtime_typespec_layout_test`；
  focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z28-fieldinfo-nested-primitive-pod-width-matrix.md`。
  备注：本记录只声明 retained metadata consumer 的 nested primitive storage-width matrix coverage；不声明
  signature-derived recursive field binding、provider ABI、managed FieldInfo method surface 或 full metadata sweep 完成。

- 2026-07-01 14:00:03 +08:00 · 11-S4BM / 10-S4Z27 FieldInfo nested primitive POD representative path matrix consumer ·
  状态：11-S4 attached metadata runtime 的 FieldDef signature/layout consumer 已覆盖 nested primitive path 的 bool、uint32、
  double 代表性 read/write；完整 primitive matrix/signature-derived binding、cross-module FieldRef/TypeRef provider 和 metadata
  sweep 仍未关闭。
  完成项目：focused fixture 复用同一 FieldInfo object/native-pointer carrier、FieldDef token、outer/intermediate
  `SZrTypeLayoutField.typeLayoutIndex` 和 leaf `typeLayoutIndex == NONE` guard，动态调整 retained layout byte-size/align，
  验证 bool、uint32、double raw child 均可通过 shared primitive POD guard 读写。未新增 metadata row、token ABI 或
  code-registration ABI。
  RED/GREEN：coverage GREEN；新增矩阵覆盖后 Windows MSVC Debug focused `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均构建
  `zr_vm_reflection_token_resolve_test`、`zr_vm_metadata_runtime_query_test`、`zr_vm_metadata_runtime_typespec_layout_test`；
  focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z27-fieldinfo-nested-primitive-pod-path-matrix.md`。
  备注：本记录只声明 retained metadata consumer 的 nested primitive representative matrix coverage；不声明完整 primitive
  宽度矩阵、signature-derived recursive field binding、provider ABI、managed FieldInfo method surface 或 full metadata sweep 完成。

- 2026-07-01 13:51:37 +08:00 · 11-S4BL / 10-S4Z26 FieldInfo nested primitive POD leaf layout identity guard consumer ·
  状态：11-S4 attached metadata runtime 的 FieldDef signature/layout consumer 已拒绝带 registered child layout identity 的
  nested primitive leaf；完整 primitive matrix/signature-derived binding、cross-module FieldRef/TypeRef provider 和 metadata
  sweep 仍未关闭。
  完成项目：`reflection_field_value_nested.c` 在 nested primitive path read/write leaf 分支中要求
  `SZrTypeLayoutField.typeLayoutIndex == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`，避免把 inline aggregate child 当成 raw
  primitive POD child 读取或写入。未新增 metadata row、token ABI 或 code-registration ABI。
  RED/GREEN：RED 为 Windows MSVC Debug focused `reflection_token_resolve` 30 tests / 1 failure，
  `Expected FALSE Was TRUE`；GREEN 后 Windows focused `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均构建
  `zr_vm_reflection_token_resolve_test`、`zr_vm_metadata_runtime_query_test`、`zr_vm_metadata_runtime_typespec_layout_test`；
  focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z26-fieldinfo-nested-primitive-pod-leaf-layout-guard.md`。
  备注：本记录只声明 retained metadata consumer 的 nested primitive leaf identity guard；不声明完整 primitive 宽度矩阵、
  signature-derived recursive field binding、provider ABI、managed FieldInfo method surface 或 full metadata sweep 完成。

- 2026-07-01 13:37:45 +08:00 · 11-S4BK / 10-S4Z25 FieldInfo nested inline primitive POD path read/write consumer ·
  状态：11-S4 attached metadata runtime 的 FieldDef signature/layout consumer 已支撑第一条 multi-level nested inline
  primitive POD raw child path read/write；完整 primitive matrix/signature-derived binding、cross-module FieldRef/TypeRef provider
  和 metadata sweep 仍未关闭。
  完成项目：public FieldInfo primitive path API 从 object/native-pointer carrier 和 FieldDef token 解析 same-runtime metadata；
  `reflection_field_value_nested.c` 逐级消费 retained `SZrTypeLayoutField.typeLayoutIndex` 和 runtime layout resolver，
  并在 leaf raw child 上委托 `reflection_field_value_primitive.{h,c}` 的 shared primitive load/store guard。
  正向 INT32 path 证明 FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)`、outer field layout 和 intermediate `typeLayoutIndex`
  已足够驱动 nested primitive raw child consumer；short storage、zero-length path、final out-of-range、missing intermediate layout、
  intermediate flags、leaf VALUE_SLOT、byte-size mismatch 和 write type mismatch 均被拒绝。未新增 metadata row、token ABI 或
  code-registration ABI。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（缺少 object-level nested primitive path read/write API，C4013 + LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 30/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z25-fieldinfo-nested-primitive-pod-path-read-write.md`。
  备注：本记录只声明 retained metadata 的 representative INT32 primitive raw child path consumer；不声明完整 primitive 宽度矩阵、
  signature-derived recursive field binding、provider ABI、managed FieldInfo method surface 或 full metadata sweep 完成。

- 2026-07-01 13:14:27 +08:00 · 11-S4BJ / 10-S4Z24 FieldInfo nested inline VALUE_SLOT path write consumer ·
  状态：11-S4 attached metadata runtime 的 FieldDef signature/layout consumer 已支撑第一条 multi-level nested inline
  `VALUE_SLOT` path write；primitive raw child binding、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
  完成项目：`reflection_field_value.c` 从 FieldInfo object/native-pointer carrier 和 FieldDef token 解析 same-runtime
  metadata，`reflection_field_value_nested.c` 逐级消费 retained `SZrTypeLayoutField.typeLayoutIndex` 和 runtime layout
  resolver，要求中间 child layout identity/size/kind 可验证，并只在 leaf `VALUE_SLOT` 上用 `ZrCore_Value_Copy()`
  写入 `SZrTypeValue`。short storage、zero-length path、final child out-of-range、missing intermediate layout 和中间 child
  GC/ownership flags 被拒绝；合法 path write 证明 replacement/drop 不需要新增 metadata ABI。未新增 metadata row、
  token ABI 或 code-registration ABI。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 29/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 29/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z24-fieldinfo-nested-value-slot-path-write.md`。
  备注：本记录是 11-S4 metadata consumer 子切片，不改变 zrp row、token table、code-registration 表或 provider ABI。
  `reflection_field_value.c` 已接近下一次拆分阈值，后续 FieldInfo method/raw child work 应继续拆出 adapter/fixture。

- 2026-07-01 13:00:18 +08:00 · 11-S4BI / 10-S4Z23 FieldInfo nested inline VALUE_SLOT path read consumer ·
  状态：11-S4 attached metadata runtime 的 FieldDef signature/layout consumer 已支撑第一条 multi-level nested inline
  `VALUE_SLOT` path read；nested path write、primitive raw child binding、cross-module FieldRef/TypeRef provider 和
  metadata sweep 仍未关闭。
  完成项目：`reflection_field_value.c` 从 FieldInfo object/native-pointer carrier 和 FieldDef token 解析 same-runtime
  metadata，`reflection_field_value_nested.c` 逐级消费 retained `SZrTypeLayoutField.typeLayoutIndex` 和 runtime layout
  resolver，要求中间 child layout identity/size/kind 可验证，并只在 leaf `VALUE_SLOT` 上复制 `SZrTypeValue`。zero-length path、
  final child out-of-range、missing intermediate layout 和中间 child GC/ownership flags 被拒绝。未新增 metadata row、token ABI
  或 code-registration ABI。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 28/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 28/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z23-fieldinfo-nested-value-slot-path-read.md`。
  备注：本记录是 11-S4 metadata consumer 子切片，不改变 zrp row、token table、code-registration 表或 provider ABI。
  `reflection_field_value.c` 的 nested traversal helper 已拆入 `reflection_field_value_nested.{h,c}`，避免继续堆叠大型文件。

- 2026-07-01 12:40:09 +08:00 · 11-S4BH / 10-S4Z22 FieldInfo nested inline VALUE_SLOT write consumer ·
  状态：11-S4 attached metadata runtime 的 FieldDef signature/layout consumer 已支撑第一条 nested inline `VALUE_SLOT`
  child write；完整 recursive field binding、primitive raw child binding、cross-module FieldRef/TypeRef provider 和 metadata
  sweep 仍未关闭。
  完成项目：`reflection_field_value.c` 在 FieldInfo object adapter 和 FieldDef token adapter 中复用 retained
  `FIELD_SIG(TYPE_DEF/TYPE_REF)`、resolved `fieldTypeLayout`、`SZrTypeLayoutField` flags/offset/byteSize，并只对 nested
  `VALUE_SLOT` child 调用 `ZrCore_Value_Copy()` 写入目标 slot。short storage、out-of-range index 与外层 GC/ownership
  flags 被拒绝；合法 write 证明旧 unique-owned value 会释放并归一 destination ownership。未新增 metadata row、token ABI
  或 code-registration ABI。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_WriteFieldInfoObjectNestedValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 27/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 27/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z22-fieldinfo-nested-value-slot-write.md`。
  备注：本切片只消费现有 FieldDef/layout metadata 关闭 nested value-slot child write 的最小边界；不声明完整 nested
  marshaling、`@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 12:24:56 +08:00 · 11-S4BG / 10-S4Z21 FieldInfo nested inline VALUE_SLOT consumer ·
  状态：11-S4 attached metadata runtime 的 FieldDef signature/layout consumer 已支撑第一条 nested inline `VALUE_SLOT`
  child read；完整 recursive field binding、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
  完成项目：`reflection_field_value.c` 在 FieldInfo object adapter 和 FieldDef token adapter 中复用 retained
  `FIELD_SIG(TYPE_DEF/TYPE_REF)`、resolved `fieldTypeLayout`、`SZrTypeLayoutField` flags/offset/byteSize，并只对 nested
  `VALUE_SLOT` child 调用 `ZrCore_Value_Copy()` 返回 `SZrTypeValue`。short storage、out-of-range index 与外层
  GC/ownership flags 被拒绝；未新增 metadata row、token ABI 或 code-registration ABI。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_ReadFieldInfoObjectNestedValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 26/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 26/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z21-fieldinfo-nested-value-slot-read.md`。
  备注：本切片只消费现有 FieldDef/layout metadata 关闭 nested value-slot child read 的最小边界；不声明完整 nested
  marshaling、`@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 12:09:54 +08:00 · 11-S4BF / 10-S4Z20 FieldInfo inline aggregate replacement/drop consumer coverage ·
  状态：11-S4 attached metadata runtime 的 FieldDef signature/layout consumer 已有 inline aggregate replacement/drop
  coverage；完整 recursive field binding、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
  完成项目：coverage fixture 继续消费现有 FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)`、resolved `fieldTypeLayout`
  和 `FIELD_COPY/FIELD_DROP` type-layout metadata；通过 nested `VALUE_SLOT | GC_VALUE | OWNERSHIP_VALUE` field
  证明 public FieldInfo write 会走 central value-copy replacement/drop，释放旧 owner 并归一 destination ownership。
  该 consumer 不新增 metadata row、token ABI 或 code-registration ABI。
  RED/GREEN：coverage GREEN；新增覆盖后 Windows focused `reflection_token_resolve` 直接通过 25/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 25/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z20-fieldinfo-inline-aggregate-replacement-drop-write.md`。
  备注：本切片只消费既有 FieldDef/layout metadata 补齐 replacement/drop 防退化覆盖；不声明完整 nested
  marshaling、`@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 11:52:49 +08:00 · 11-S4BE / 10-S4Z19 FieldInfo inline aggregate field-copy borrowed-source write consumer ·
  状态：11-S4 attached metadata runtime 的 FieldDef signature/layout consumer 已支撑 layout-aware inline aggregate source write；
  完整 recursive field binding、destination drop/replacement lifecycle、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
  完成项目：FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)` type-node、resolved `fieldTypeLayout` 和 central type-layout copy
  helper 共同驱动写入；当目标字段不是 `VALUE_SLOT`、不含 GC/ownership flags、field layout byte size 与 type layout
  byte size 一致、类型 layout 为 struct/union，且 native-pointer source 非空时，public FieldInfo write 可对 raw-copy
  layout 和 `FIELD_COPY` layout 执行 `ZrCore_TypeLayout_CopyInline()`。该 consumer 不新增 metadata row、token ABI 或
  code-registration ABI。
  RED/GREEN：RED 为 Windows focused `reflection_token_resolve` 将 non-blittable field-copy source 写入期望改为成功后
  失败 1/24；GREEN 后 `reflection_token_resolve` 24/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 24/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z19-fieldinfo-inline-aggregate-field-copy-write.md`。
  备注：本切片只消费现有 FieldDef signature/layout metadata 和 type-layout copy metadata 建立 field-copy source write boundary；
  不声明完整 nested marshaling、destination ownership replacement/drop、`@dynamically_accessed` dataflow、
  DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 11:34:10 +08:00 · 11-S4BD / 10-S4Z18 FieldInfo inline aggregate borrowed-source write consumer ·
  状态：11-S4 attached metadata runtime 的 FieldDef signature/layout consumer 已支撑受限 inline aggregate byte-copy write；
  完整 recursive field binding、field-copy/drop 语义、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
  完成项目：FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)` type-node 与 resolved `fieldTypeLayout` 共同驱动写入判定；
  当目标字段不是 `VALUE_SLOT`、不含 GC/ownership flags、field layout byte size 与 type layout byte size 一致、
  类型 layout 为 struct/union 且 `blittable` 为真时，public FieldInfo write 接受非空 native-pointer source 并复制字段 bytes。
  该 consumer 不新增 metadata row、token ABI 或 code-registration ABI。
  RED/GREEN：RED 为 Windows focused `reflection_token_resolve` 在 inline struct fixture 增加 native-pointer source
  写入期望后失败 1/24；GREEN 后 `reflection_token_resolve` 24/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 24/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z18-fieldinfo-inline-aggregate-borrowed-source-write.md`。
  备注：本切片只消费现有 FieldDef signature/layout metadata 建立 blittable aggregate borrowed-source write boundary；
  不声明完整 nested marshaling、non-blittable struct semantics、`@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 11:26:41 +08:00 · 11-S4BC / 10-S4Z17 FieldInfo inline struct borrowed view consumer ·
  状态：11-S4 attached metadata runtime 的 FieldDef signature/layout consumer 已支撑 inline aggregate read first-contract；
  完整 recursive field binding、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
  完成项目：FieldDef `FIELD_SIG(TYPE_DEF/TYPE_REF)` type-node 与 resolved `fieldTypeLayout` 共同驱动读取判定；
  当字段不是 `VALUE_SLOT`、不含 GC/ownership flags、field layout byte size 与 type layout byte size 一致且类型 layout 为
  struct/union 时，public FieldInfo read 返回 inline storage 字段地址的 borrowed native-pointer view。该 consumer 不新增
  metadata row、token ABI 或 code-registration ABI。
  RED/GREEN：RED 为 Windows focused `reflection_token_resolve` 新增 inline struct object fixture 后失败 1/24；
  GREEN 后 `reflection_token_resolve` 24/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 24/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z17-fieldinfo-inline-struct-borrowed-view.md`。
  备注：本切片只消费现有 FieldDef signature/layout metadata 建立 borrowed-view boundary；不声明完整 nested
  marshaling、struct write/copy、`@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 11:15:23 +08:00 · 11-S4BB / 10-S4Z16 FieldInfo object primitive POD consumer coverage ·
  状态：11-S4 attached metadata runtime + FieldDef token carriers 的 object-level primitive POD consumer 覆盖子切片完成；
  nested inline field marshaling、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
  完成项目：focused fixture 通过 public `FieldInfo` object 恢复 attached runtime/token context，再沿既有
  primitive POD FieldDef layout/signature consumer 读取和写入 int32 raw inline field；覆盖 type-mismatch 写入拒绝后
  raw bytes 保持。该切片不新增 metadata row 或 ABI。
  RED/GREEN：coverage GREEN；现有 object adapter + token-driven primitive POD consumer 已满足路径，
  Windows focused `reflection_token_resolve` 23/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 23/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z16-fieldinfo-object-primitive-pod.md`。
  备注：本切片只补 object-level primitive POD consumer coverage；不声明完整 `FieldInfo.SetValue` method surface、
  TypeRef/provider signature binding、`@dynamically_accessed` dataflow 或完整 metadata sweep 完成。

- 2026-07-01 11:06:55 +08:00 · 11-S4BA / 10-S4Z15 FieldInfo object value write consumer ·
  状态：11-S4 attached metadata runtime + FieldDef token carriers 的 object-level write consumer 子切片完成；
  nested inline field marshaling、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_WriteFieldInfoObjectValue()` 从 public `FieldInfo` object 中读取
  `metadataRuntime` native pointer 与 `metadataToken` int，再复用 FieldDef token inline value writer。
  该 consumer 证明 11-S4AY/S4AZ 的 same-runtime object identity 可驱动 `VALUE_SLOT` write，不新增 metadata row
  或 ABI。
  RED/GREEN：RED 为 Windows MSVC Debug build 链接失败，缺少
  `ZrCore_Reflection_WriteFieldInfoObjectValue`；GREEN 后 Windows focused `reflection_token_resolve` 22/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 22/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z15-fieldinfo-object-value-write.md`。
  备注：本切片只关闭 object-level write consumer；不声明完整 `FieldInfo.SetValue` method surface、
  TypeRef/provider signature binding、`@dynamically_accessed` dataflow 或完整 metadata sweep 完成。

- 2026-07-01 10:52:48 +08:00 · 11-S4AZ / 10-S4Z14 FieldInfo object value read consumer ·
  状态：11-S4 attached metadata runtime + FieldDef token carriers 的 object-level read consumer 子切片完成；
  object-level write、nested inline field marshaling、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_ReadFieldInfoObjectValue()` 从 public `FieldInfo` object 中读取
  `metadataRuntime` native pointer 与 `metadataToken` int，再复用 FieldDef token inline value resolver。
  该 consumer 证明 11-S4AY 的 same-runtime identity carrier 可直接驱动 `VALUE_SLOT` read，不新增 metadata row 或 ABI。
  RED/GREEN：RED 为 Windows MSVC Debug build 链接失败，缺少
  `ZrCore_Reflection_ReadFieldInfoObjectValue`；GREEN 后 Windows focused `reflection_token_resolve` 21/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 21/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z14-fieldinfo-object-value-read.md`。
  备注：本切片只关闭 read-only object-level consumer；不声明 `SetValue`、完整 FieldInfo methods、
  TypeRef/provider signature binding、`@dynamically_accessed` dataflow 或完整 metadata sweep 完成。

- 2026-07-01 10:36:14 +08:00 · 11-S4AY / 10-S4Z13 FieldInfo metadata runtime carrier consumer ·
  状态：11-S4 attached metadata runtime identity 作为 public `FieldInfo` object native-pointer carrier 的消费路径完成；
  nested inline field marshaling、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 把当前 `SZrMetadataRuntime *` 写入 `metadataRuntime`
  字段，测试验证该字段为 `ZR_VALUE_TYPE_NATIVE_POINTER` 且指向同一 runtime。该 consumer 让后续 object-level
  FieldInfo 字段读写能从对象字段恢复 same-runtime token context，不新增 metadata table 或 ABI 字段。
  RED/GREEN：RED 为 focused `FieldInfo` object fixture 新增 `metadataRuntime` native-pointer 断言后，
  Windows MSVC Debug `reflection_token_resolve` 20 个测试失败 1 个；GREEN 后同一 focused run 20/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 20/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z13-fieldinfo-metadata-runtime-carrier.md`。
  备注：本切片只关闭 `metadataRuntime` carrier consumer；不声明 object-level FieldInfo methods、
  nested POD marshaling 或完整 metadata sweep 完成。

- 2026-07-01 10:21:28 +08:00 · 11-S4AX / 10-S4Z12 FieldInfo primitive POD float32 precision guard consumer ·
  状态：11-S4 FieldDef layout binding 与 11-S3 field signature view 的 primitive POD float32 raw write
  precision/no-loss guard 完成；nested inline field marshaling、cross-module FieldRef/TypeRef provider 和 metadata sweep
  仍未关闭。
  完成项目：runtime reflection 的 raw primitive consumer 现在对 `FIELD_SIG(PRIMITIVE(FLOAT))` 且 byte size 为 float32
  的字段写入先拒绝 NaN，再拒绝 `|value| > FLT_MAX`，最后要求 double source 可经 `TZrFloat32` 无损 round-trip。
  不能无损保存的 source 在 cast 前失败，保持原 inline raw bytes；该 consumer 不新增 zrp row、token record、
  code-registration 字段或 metadata ABI。
  RED/GREEN：RED 为新增 precision-loss raw write consumer 覆盖后 Windows MSVC Debug focused run 失败 1/20；
  GREEN 后同一 focused run 20/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 20/0、`metadata_runtime_query` 24/0、
  `metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z12-fieldinfo-primitive-pod-float32-precision-guard.md`。
  备注：本记录只声明既有 FieldDef layout/signature metadata 可支撑 primitive POD float32 precision/no-loss guard；
  不声明 nested POD marshaling、object-level FieldInfo methods 或完整 metadata sweep 完成。

- 2026-07-01 10:11:20 +08:00 · 11-S4AW / 10-S4Z11 FieldInfo primitive POD float32 NaN guard consumer ·
  状态：11-S4 FieldDef layout binding 与 11-S3 field signature view 的 primitive POD float32 raw write NaN guard 完成；
  float32 precision semantics、nested inline field marshaling、cross-module FieldRef/TypeRef provider 和 metadata sweep
  仍未关闭。
  完成项目：runtime reflection 的 raw primitive consumer 现在对 `FIELD_SIG(PRIMITIVE(FLOAT))` 且 byte size 为 float32
  的字段写入先拒绝 NaN，再执行既有 `FLT_MAX` range guard。NaN source 在 cast 前失败，保持原 inline raw bytes；
  该 consumer 不新增 zrp row、token record、code-registration 字段或 metadata ABI。
  RED/GREEN：RED 为新增 NaN raw write consumer 覆盖后 Windows MSVC Debug focused run 失败 1/19；
  GREEN 后同一 focused run 19/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 19/0、`metadata_runtime_query` 24/0、
  `metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z11-fieldinfo-primitive-pod-float32-nan-guard.md`。
  备注：本记录只声明既有 FieldDef layout/signature metadata 可支撑 primitive POD float32 NaN guard；
  不声明 precision policy、nested POD marshaling、object-level FieldInfo methods 或完整 metadata sweep 完成。

- 2026-07-01 09:58:56 +08:00 · 11-S4AV / 10-S4Z10 FieldInfo primitive POD float32 range guard consumer ·
  状态：11-S4 FieldDef layout binding 与 11-S3 field signature view 的 primitive POD float32 raw write range guard 完成；
  float32 NaN/precision semantics、nested inline field marshaling、cross-module FieldRef/TypeRef provider 和 metadata sweep
  仍未关闭。
  完成项目：runtime reflection 的 raw primitive consumer 现在对 `FIELD_SIG(PRIMITIVE(FLOAT))` 且 byte size 为 float32
  的字段写入执行 `FLT_MAX` range guard。double source 超过 float32 finite storage range 时在 cast 前拒绝，保持原
  inline raw bytes；该 consumer 不新增 zrp row、token record、code-registration 字段或 metadata ABI。
  RED/GREEN：RED 为 focused reflection token resolver 新增 float32 越界写入后 Windows MSVC Debug 失败 1/18
  （`Expected FALSE Was TRUE`）；GREEN 后同一 focused run 通过 18/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 18/0、`metadata_runtime_query` 24/0、
  `metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z10-fieldinfo-primitive-pod-float32-range-guard.md`。
  备注：本记录只声明既有 FieldDef layout/signature metadata 可支撑 primitive POD float32 storage range guard；
  不声明 NaN/precision policy、nested POD marshaling、object-level FieldInfo methods 或完整 metadata sweep 完成。

- 2026-07-01 09:49:10 +08:00 · 11-S4AU / 10-S4Z9 FieldInfo primitive POD integer range guard consumer ·
  状态：11-S4 FieldDef layout binding 与 11-S3 field signature view 的 primitive POD integer raw write range guard 完成；
  float32 narrowing/finite semantics、nested inline field marshaling、cross-module FieldRef/TypeRef provider 和 metadata sweep
  仍未关闭。
  完成项目：runtime reflection 的 raw primitive consumer 现在按 FieldDef signature primitive type 与 field layout byte size
  计算整数目标范围：signed width 使用 min/max，unsigned width 使用 max，signed negative 写 unsigned 和 unsigned 超 signed max
  写 signed 均被拒绝。该 consumer 不新增 zrp row、token record、code-registration 字段或 metadata ABI；它只验证 11-S3/11-S4
  既有 metadata 足以支撑 range-safe integer raw field write boundary。
  RED/GREEN：RED 为 focused reflection token resolver 新增越界写入 preserve-storage 用例后，Windows MSVC Debug 17 个测试中
  该用例失败（旧实现截断写入）；GREEN 后 focused run 通过 17/0。验证：WSL GCC/Clang/Windows MSVC Debug 均通过
  `reflection_token_resolve` 17/0、`metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z9-fieldinfo-primitive-pod-integer-range-guard.md`。
  备注：本切片只改变 runtime consumer 的 integer range safety，不改变 11 metadata ABI，也不声明 cross-module provider、
  nested POD marshaling、object-level FieldInfo methods、float32 narrowing/finite semantics 或完整 metadata sweep 完成。

- 2026-07-01 09:35:02 +08:00 · 11-S4AT / 10-S4Z8 FieldInfo primitive POD width matrix consumer ·
  状态：11-S4 FieldDef layout binding 与 11-S3 field signature view 的 primitive POD raw read/write storage-width 消费覆盖完成；
  primitive numeric overflow/range semantics、nested inline field marshaling、cross-module FieldRef/TypeRef provider 和 metadata
  sweep 仍未关闭。
  完成项目：`test_reflection_reads_and_writes_field_info_primitive_pod_width_matrix` 复用 same-runtime FieldDef metadata
  fixture，分别构造 int8、int16、int64、uint8、uint16、uint64、float32 的 `FIELD_SIG(PRIMITIVE(...))`，并要求 runtime
  reflection 通过同一 FieldDef row byteOffset/typeLayoutId 与 owner-field layout metadata 读写 raw inline storage。该 consumer
  不新增 zrp row、token record、code-registration 字段或 metadata ABI；它只验证 11-S3/11-S4 既有 metadata 足以支撑
  storage-width primitive POD raw field value boundary。
  RED/GREEN：coverage GREEN，新增宽度矩阵后 Windows MSVC Debug focused `reflection_token_resolve` 通过 16/0；测试 helper
  中的 unreachable-code warning 修正后，无需生产修复。验证：WSL GCC/Clang/Windows MSVC Debug 均通过
  `reflection_token_resolve` 16/0、`metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z8-fieldinfo-primitive-pod-width-matrix.md`。
  备注：本切片只扩大 consumer coverage，不改变 11 metadata ABI，也不声明 cross-module provider、nested POD marshaling、
  object-level FieldInfo methods、numeric overflow/range semantics 或完整 metadata sweep 完成。

- 2026-07-01 05:06:21 +08:00 · 11-S4AS / 10-S4Z7 FieldInfo primitive POD representative matrix consumer ·
  状态：11-S4 FieldDef layout binding 与 11-S3 field signature view 的 primitive POD raw read/write 代表性消费覆盖完成；
  全量 primitive 宽度/溢出语义、nested inline field marshaling、cross-module FieldRef/TypeRef provider 和 metadata sweep
  仍未关闭。
  完成项目：`test_reflection_reads_and_writes_field_info_primitive_pod_matrix` 复用 same-runtime FieldDef metadata fixture，
  分别构造 `FIELD_SIG(PRIMITIVE(BOOL))`、`FIELD_SIG(PRIMITIVE(UINT32))`、`FIELD_SIG(PRIMITIVE(DOUBLE))`，并要求 runtime
  reflection 通过同一 FieldDef row byteOffset/typeLayoutId 与 owner-field layout metadata 读写 raw inline storage。该 consumer
  不新增 zrp row、token record、code-registration 字段或 metadata ABI；它只验证 11-S3/11-S4 既有 metadata 足以支撑
  representative primitive POD raw field value boundary。
  RED/GREEN：coverage GREEN，新增矩阵后 Windows MSVC Debug focused `reflection_token_resolve` 直接通过 15/0，说明
  10-S4Z6 的 generic primitive loader/store 已满足该代表性矩阵，无需生产修复。验证：WSL GCC/Clang/Windows MSVC Debug
  均通过 `reflection_token_resolve` 15/0、`metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused
  CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z7-fieldinfo-primitive-pod-matrix.md`。
  备注：本切片只扩大 consumer coverage，不改变 11 metadata ABI，也不声明 cross-module provider、nested POD marshaling、
  object-level FieldInfo methods 或完整 metadata sweep 完成。

- 2026-07-01 04:45:19 +08:00 · 11-S4AR / 10-S4Z6 FieldInfo primitive POD read/write consumer ·
  状态：11-S4 FieldDef layout binding 与 11-S3 field signature view 的 primitive POD raw read/write consumer 子切片完成；
  broader primitive matrix、nested inline field marshaling、cross-module FieldRef/TypeRef provider 和 metadata sweep
  仍未关闭。
  完成项目：runtime reflection 新增 `reflection_field_value.c` 专门承载 FieldInfo token value APIs；该 consumer 不新增
  zrp row、token record、code-registration 字段或 metadata ABI，而是复用既有 `ResolveToken()`/FieldDef binding view 的 owner
  layout、field layout id 与 byte offset，并复用 `ZrCore_MetadataRuntime_ReadSignatureView()` /
  `ZrCore_MetadataRuntime_ReadSignatureTypeNode()` 读取 `FIELD_SIG(PRIMITIVE(...))`。raw primitive 路径拒绝
  `VALUE_SLOT`、GC 和 ownership field flags，要求 layout byte size 与 primitive C byte size 一致，再对 inline storage
  raw bytes 做 load/store。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 primitive POD read/write 调用后，Windows MSVC Debug 运行
  14 个测试并在新用例 primitive POD read 处失败（`Expected TRUE Was FALSE`）；GREEN 后 same-runtime FieldDef token 可从
  offset 40 读取 raw int32 `-12345`，拒绝 bool 写入，写入 int `2048` 并读回。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 14/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z6-fieldinfo-primitive-pod-read-write.md`。
  备注：本记录只声明既有 FieldDef layout/signature metadata 可支撑 primitive POD int32 raw inline field boundary；不声明
  metadata 格式扩展、完整 primitive variant matrix、nested marshaling、cross-module provider loading/version compatibility、
  DESCRIPTION promotion 或完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 04:12:42 +08:00 · 11-S4AQ / 10-S4Z5 FieldInfo value-slot write consumer ·
  状态：11-S4 FieldDef layout binding 的 FieldInfo `VALUE_SLOT` write consumer 子切片完成；POD/nested inline field
  marshaling、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
  完成项目：runtime reflection 新增 `ZrCore_Reflection_WriteFieldInfoTokenValue()`，并让 read/write 共用同一
  FieldDef token owner-field resolver；该 resolver 只消费既有 `ResolveToken()` / FieldDef binding view 暴露的 owner
  layout、field layout id 与 byte offset，不新增 zrp row、token record、code-registration 字段或 metadata ABI。
  写入路径要求 owner `SZrTypeLayoutField` 与 resolved FieldDef offset/type-layout 匹配、字段带
  `ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT` 且调用方 inline storage range 足够，再复制 `SZrTypeValue` 到 inline slot。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 value-slot field write 调用后，Windows MSVC Debug
  构建/链接失败，缺少 public reflection API；GREEN 后 same-runtime FieldDef token 可将 inline storage offset 32
  从 int `11` 写为 int `271828` 并读回，同时 null/invalid/short-storage guard 均返回 false。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 13/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z5-fieldinfo-value-slot-write.md`。
  备注：本记录只声明 FieldDef layout metadata 的 `VALUE_SLOT` write consumer；不声明 metadata 格式扩展、
  raw POD/nested marshaling、cross-module provider loading/version compatibility、DESCRIPTION promotion 或完整
  metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 03:49:06 +08:00 · 11-S4AP / 10-S4Z4 FieldInfo value-slot read consumer ·
  状态：11-S4 FieldDef layout binding 的 FieldInfo `VALUE_SLOT` read consumer 子切片完成；完整 FieldInfo write、
  POD/nested inline field marshaling、cross-module FieldRef/TypeRef provider 和 metadata sweep 仍未关闭。
  完成项目：runtime reflection 新增 `ZrCore_Reflection_ReadFieldInfoTokenValue()`，消费既有
  `ResolveToken()` / FieldDef binding view 暴露的 owner layout、field layout id 与 byte offset；实现只在 owner
  `SZrTypeLayoutField` 与 resolved FieldDef offset/type-layout 匹配、字段带 `ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT`、
  且调用方 inline storage range 足够时复制 `SZrTypeValue`。该路径不新增 zrp row、token record、code-registration
  字段或 metadata ABI。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 value-slot field read 调用后，WSL GCC 构建/链接
  失败，缺少 public reflection API；GREEN 后 same-runtime FieldDef token 可从 inline storage offset 32 读取 int
  `314159`，同时 null/invalid/short-storage guard 均返回 false。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 12/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z4-fieldinfo-value-slot-read.md`。
  备注：本记录只声明 FieldDef layout metadata 的 `VALUE_SLOT` read consumer；不声明 metadata 格式扩展、field write、
  raw POD/nested marshaling、cross-module provider loading/version compatibility、DESCRIPTION promotion 或完整
  metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 03:17:49 +08:00 · 11-S4AO / 10-S4Z3 FieldInfo recursive signature type-node type literal consumer ·
  状态：11-S4 runtime metadata FieldInfo recursive signature type-node type literal consumer 子切片完成；
  cross-module provider binding、recursive field type binding 和完整 FieldInfo 字段读写仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 的 signature node object builder 现在在 recursive node
  已有 semantic `typeName` 时构造 public `type` type literal object；该 consumer 复用现有 `typeName` 来源，不新增
  metadata row、token record 或 ABI。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 nested node `type` object 断言后，WSL GCC
  运行失败 1/11（`Expected Non-NULL`）；GREEN 后 generic FieldInfo signature node 的 base/child type literal 断言通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z3-fieldinfo-signature-node-type-literal.md`。
  备注：本记录只声明 recursive signature node type literal consumer；不声明跨模块 provider loading/version compatibility、
  字段 read/write marshaling、DESCRIPTION promotion、trim analyzer 或完整 metadata sweep 完成。Clang 仍报告既有
  `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 03:04:36 +08:00 · 11-S4AN / 10-S4Z2 FieldInfo direct TypeRef child type-node semantic consumer ·
  状态：11-S4 runtime metadata FieldInfo recursive direct TypeRef child type-node semantic consumer 子切片完成；
  cross-module provider binding、recursive field type binding 和完整 FieldInfo 字段读写仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 的 signature node object builder 现在在递归 `TYPE_REF`
  node 时保留 runtime 上下文，匹配 module signature token record，解析 TypeRef token 的 target TypeDef registry-backed
  layout，并读取 target TypeDef row name。当前不新增 metadata ABI。
  RED/GREEN：RED 为 focused reflection token resolver 测试把 generic fixture 扩展为
  `GENERIC_INST(TYPE_DEF(...), int64, TYPE_DEF(...), TYPE_REF(...))` 并要求 TypeRef child node 带 token/layout/name 后，
  WSL GCC 运行失败 1/11；GREEN 后 FieldDef token object 暴露 direct TypeRef child node semantic carrier。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z2-fieldinfo-signature-typeref-child-node-semantic.md`。
  备注：本切片不声明跨模块 provider loading/version compatibility、字段值读写、完整 FieldInfo methods、数据流、
  DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 02:47:33 +08:00 · 11-S4AM / 10-S4Y FieldInfo direct TypeDef child/base type-node semantic consumer ·
  状态：11-S4 runtime metadata FieldInfo recursive direct TypeDef base/child type-node semantic consumer 子切片完成；
  direct TypeRef generic argument semantic token/layout binding、recursive field type binding、TypeRef/cross-module
  provider、完整 FieldInfo 字段读写仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 的 signature node object builder 现在在递归 `TYPE_DEF`
  node 时保留 runtime 上下文，匹配 existing signature token record，解析 TypeDef token 的 registry-backed layout，
  并读取 TypeDef row name。当前不新增 metadata ABI，不改变 existing TypeRef resolver 或 FieldDef binding view。
  RED/GREEN：RED 为 focused reflection token resolver 测试把 generic fixture 扩展为
  `GENERIC_INST(TYPE_DEF(...), int64, TYPE_DEF(...))` 并要求 base/child TypeDef node 带 token/layout/name 后，
  WSL GCC 运行失败 1/11；GREEN 后 FieldDef token object 暴露 direct TypeDef base 与 child node semantic carrier。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4y-fieldinfo-signature-typedef-child-node-semantic.md`。
  备注：本切片不声明 direct TypeRef child token/layout binding、字段值读写、完整 FieldInfo methods、数据流、
  DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 02:19:00 +08:00 · 11-S4AL / 10-S4X FieldInfo signature primitive child type-node semantic name consumer ·
  状态：11-S4 runtime metadata FieldInfo primitive child type-node semantic name consumer 子切片完成；direct TypeDef/TypeRef
  generic argument semantic token/layout binding、recursive field type binding、TypeRef/cross-module provider、完整
  FieldInfo 字段读写仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 的 signature node object builder 在递归 child node 时，
  对 `ZR_METADATA_SIGNATURE_NODE_PRIMITIVE` 使用 `reflection_builtin_type_name()` 补出 `typeName`。当前不新增 metadata ABI，
  也不改变 existing TypeRef resolver 或 FieldDef binding view。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 generic `GENERIC_INST(TYPE_DEF(...), int64)` child
  `typeName == "int"` 断言后，WSL GCC 运行失败 1/11；GREEN 后 FieldDef token object 暴露 child
  `PRIMITIVE(INT64)` node object 的 semantic name。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4x-fieldinfo-signature-primitive-child-node-semantic.md`。
  备注：本切片不声明 direct TypeDef/TypeRef child token/layout binding、字段值读写、完整 FieldInfo methods、数据流、
  DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 02:00:10 +08:00 · 11-S4AK / 10-S4W FieldInfo signature child type-node object list consumer ·
  状态：11-S4 runtime metadata FieldInfo signature child type-node object list consumer 子切片完成；generic argument
  semantic binding、recursive field type binding、TypeRef/cross-module provider、完整 FieldInfo 字段读写仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 的 signature node object builder 现在消费
  `SZrMetadataRuntimeSignatureTypeNodeView.childListBlobOffset/childCount`，从同一 validated signature blob 顺序读取
  child type-node，并写入 `fieldTypeSignatureNodeObject.childNodeObjects` array。当前只承载 structural node/blob/payload
  summary，不新增 metadata ABI。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 generic `GENERIC_INST(TYPE_DEF(...), int64)` child object
  断言后，WSL GCC 运行失败 1/11；GREEN 后 FieldDef token object 暴露 top-level generic node、nested base `TYPE_DEF`
  node object 和 child `PRIMITIVE(INT64)` node object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4w-fieldinfo-signature-child-node-objects.md`。
  备注：本切片不改变 existing TypeRef resolver 或 FieldDef binding view；不声明 generic argument semantic binding、
  字段值读写、完整 FieldInfo methods、数据流、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 01:41:50 +08:00 · 11-S4AJ / 10-S4V FieldInfo signature base type-node object consumer ·
  状态：11-S4 runtime metadata FieldInfo signature base type-node object consumer 子切片完成；generic argument list、
  recursive field type binding、TypeRef/cross-module provider、完整 FieldInfo 字段读写仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在把 11-S3I validated signature blob 传给
  signature node object builder，并在 node view 存在 `baseTypeBlobOffset` 时读取同一 blob 的 base type-node，写入
  `fieldTypeSignatureNodeObject.baseTypeNodeObject`。当前只承载 structural node/blob/payload summary，不新增 metadata ABI。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 generic `GENERIC_INST(TYPE_DEF(...), int64)` base object 断言后，
  WSL GCC 运行失败 1/11；GREEN 后 FieldDef token object 暴露 top-level generic node 与 nested base `TYPE_DEF` node object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4v-fieldinfo-signature-base-type-node-object.md`。
  备注：本切片不改变 existing TypeRef resolver 或 FieldDef binding view；不声明 generic argument materialization、
  semantic token/layout binding、字段值读写、完整 FieldInfo methods、数据流、DESCRIPTION promotion 或完整 metadata sweep 完成。
  Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 01:17:21 +08:00 · 11-S4AI / 10-S4U FieldInfo signature type-node object consumer ·
  状态：11-S4 runtime metadata FieldInfo signature type-node object consumer 子切片完成；recursive field type binding、
  TypeRef/cross-module provider、完整 FieldInfo 字段读写仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在复用 11-S3I 的 validated signature type-node view、
  11-S4 signature-derived token/layout resolver 结果和 10-S4T 的 layout match bool，暴露
  `fieldTypeSignatureNodeObject`。该 object 承载 node/blob offset/payload/base/child summary，并同步
  `typeToken/typeLayoutId/typeSize/typeName/matchesLayout`，让后续递归 semantic type object 有稳定承载面。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增该 object 字段断言后，WSL GCC 运行失败 3/10；
  GREEN 后 FieldDef token object 暴露 primitive、direct TypeDef、bound TypeRef 三种 signature node object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 10/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4u-fieldinfo-signature-type-node-object.md`。
  备注：本切片不新增 metadata ABI，不改变 existing TypeRef resolver 或 FieldDef binding view；不声明 recursive
  wrapper/generic materialization、字段值读写、完整 FieldInfo methods、数据流、DESCRIPTION promotion 或完整 metadata sweep 完成。
  Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 00:55:33 +08:00 · 11-S4AH / 10-S4T FieldInfo signature/layout consistency consumer ·
  状态：11-S4 runtime metadata FieldInfo signature-derived layout 与 FieldDef layout binding view 一致性 consumer 子切片完成；
  recursive field type binding、TypeRef/cross-module provider、完整 FieldInfo 字段读写仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在复用 11-S4 FieldDef layout binding view 与
  signature-derived `ResolveTypeTokenLayout()` 结果，暴露 `fieldTypeSignatureMatchesLayout`。该 bool 只在 signature
  layout 和 FieldDef layout 都来自同一 registry layout id/指针时为 true；primitive bool signature 对 int FieldDef
  layout 保持 false，direct TypeDef 与 bound TypeRef 命中同一 layout 时为 true。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增该 bool 字段断言后，WSL GCC 运行失败 3/10；
  GREEN 后 FieldDef token object 暴露 primitive false、TypeDef true、TypeRef true。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 10/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4t-fieldinfo-signature-layout-consistency.md`。
  备注：本切片不新增 metadata ABI，不改变 existing TypeRef resolver 或 FieldDef binding view；不声明 recursive
  wrapper/generic materialization、字段值读写、完整 FieldInfo methods、数据流、DESCRIPTION promotion 或完整 metadata sweep 完成。
  Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 00:37:26 +08:00 · 11-S4AG / 10-S4S FieldInfo bound TypeRef signature consumer ·
  状态：11-S4 runtime metadata current-runtime bound `TYPE_REF` field signature consumer 子切片完成；cross-module
  provider lookup、recursive type-node materialization、完整 signature-derived field type binding 和完整 FieldInfo
  字段读写仍未关闭。
  完成项目：FieldInfo builder 现在可消费 attached module metadata `TYPE_REF` token record；该 record 的 signature blob
  与 `FIELD_SIG` 的 `TYPE_REF` field type-node 匹配后，继续复用
  `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` 的 TypeRef -> target TypeDef resolver 暴露 token/layout/size，
  并从 target TypeDef row 读取 `fieldTypeSignatureTypeName`、物化 `fieldTypeSignatureType`。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 bound `TYPE_REF` signature 暴露 type name/object 后
  运行失败（名称仍为 null）；GREEN 后 FieldInfo 同时暴露 TypeRef token identity、target layout/size 和 target TypeDef
  type object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 10/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4s-fieldinfo-signature-typeref-carrier.md`。
  备注：本切片只复用既有 TypeRef token/layout resolver 与 TypeDef row/name binding，不新增 metadata ABI；
  不声明 cross-module provider loading、recursive wrapper/generic materialization、字段值读写、完整 FieldInfo methods、
  数据流、DESCRIPTION promotion 或完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 00:15:36 +08:00 · 11-S4AF / 10-S4R FieldInfo direct TypeDef signature type object consumer ·
  状态：11-S4 runtime metadata direct local `TYPE_DEF` field signature type object consumer 子切片完成；TypeRef
  provider lookup、recursive type-node materialization、完整 signature-derived field type binding 和完整 FieldInfo
  字段读写仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 在 direct TypeDef signature token/layout carrier
  命中后，额外消费 `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()` 的 TypeDef row name，把 zrp string-pool
  中的 `int` 写入 `fieldTypeSignatureTypeName`，并物化 `fieldTypeSignatureType` type literal object。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 direct `TYPE_DEF` signature 也有
  `fieldTypeSignatureType` object 后运行失败（字段仍为 null）；GREEN 后 FieldInfo 同时暴露 token/layout/size、
  signature type name 和 type object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 9/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4r-fieldinfo-signature-typedef-type-object.md`。
  备注：本切片只复用既有 TypeDef row/name/layout binding，不新增 metadata ABI；不声明 TypeRef/cross-module
  signature binding、recursive wrapper/generic materialization、字段值读写、完整 FieldInfo methods、数据流、
  DESCRIPTION promotion 或完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-06-30 21:45:03 +08:00 · 11-S4AE / 10-S4Q FieldInfo direct TypeDef signature token/layout consumer ·
  状态：11-S4 runtime metadata direct local `TYPE_DEF` field signature consumer 子切片完成；TypeRef provider lookup、
  recursive type-node materialization、完整 signature-derived field type binding 和完整 FieldInfo 字段读写仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在通过 reflection-local matcher 消费
  `ZrCore_MetadataRuntime_GetSignatureBlob()` 与 `ZrCore_MetadataRuntime_ReadSignatureTypeNode()`，将 validated
  `FIELD_SIG` 的 direct `TYPE_DEF` node 匹配到 attached metadata token record，再经
  `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` 暴露 registry-backed token/layout/size carrier。该路径不新增
  zrp row，不改变 code-registration ABI，也不引入跨模块 provider lookup。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `fieldTypeSignatureTypeToken` 后运行失败；
  GREEN 后 direct local `TYPE_DEF` signature node 暴露 token/layout carrier。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 9/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4q-fieldinfo-signature-typedef-carrier.md`。
  备注：本记录只声明 direct local TypeDef signature token/layout carrier；不声明 TypeRef/cross-module signature
  binding、recursive type-node reflection object、字段类型一致性校验、字段值读写、DESCRIPTION promotion 或完整
  metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-06-30 21:20:26 +08:00 · 11-S4AD / 10-S4P FieldInfo module reflection object consumer ·
  状态：11-S4 attached module identity public `FieldInfo` consumer 子切片完成；完整字段值读写、
  TypeDef/TypeRef signature binding、recursive type-node reflection object、TypeSpec/generic layout materialization
  和 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在在 `runtime->module` 是真实
  `ZR_OBJECT_INTERNAL_TYPE_MODULE` 时复用既有 module reflection builder/cache，并把 `FieldInfo.module` 指向
  module reflection object。该路径不新增 zrp row，不改变 code-registration ABI，也不引入跨模块 provider lookup。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `FieldInfo.module` object 后运行失败；
  GREEN 后 FieldDef token object 在 `geometry` module fixture 上暴露 `module.name == "geometry"`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4p-fieldinfo-module-reflection-link.md`。
  备注：本记录只声明 attached module identity 可被最小 public FieldInfo 链接为 module reflection object；
  不声明完整 signature-derived field type binding、TypeDef/TypeRef token/layout binding、字段值 read/write marshaling、
  跨模块 FieldRef/TypeRef、DESCRIPTION promotion、trim analyzer 或完整 11-S4 关闭。Clang 仍报告既有
  `reflection.c` 中 `callerName` unused warning。

- 2026-06-30 21:02:11 +08:00 · 11-S4AC / 10-S4O FieldInfo primitive signature type object consumer ·
  状态：11-S3I signature type-node view 的 primitive payload public `FieldInfo` type object consumer 子切片完成；完整字段值读写、
  TypeDef/TypeRef signature binding、recursive type-node reflection object、TypeSpec/generic layout materialization、
  module full reflection object link 和 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在在已验证 `FIELD_SIG` field type-node 为
  `ZR_METADATA_SIGNATURE_NODE_PRIMITIVE` 时，使用 `fieldTypeSignatureTypeName` 构造 public
  `FieldInfo.fieldTypeSignatureType` type literal object。该路径复用既有 type-node view 和反射内置 type-name 表，
  不新增 zrp row，不改变 code-registration ABI，也不覆盖 FieldDef layout-derived `type` object。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 primitive signature type object 后运行失败；
  GREEN 后 FieldDef token object 在 `PRIMITIVE(BOOL)` fixture 上暴露 `fieldTypeSignatureType.name == "bool"`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4o-fieldinfo-field-signature-type-object.md`。
  备注：本记录只声明 primitive signature type-node payload 可被最小 public FieldInfo 物化为独立 type literal object；
  不声明完整 signature-derived field type binding、TypeDef/TypeRef token/layout binding、字段值 read/write marshaling、
  跨模块 FieldRef/TypeRef、DESCRIPTION promotion、trim analyzer 或完整 11-S4 关闭。

- 2026-06-30 20:46:10 +08:00 · 11-S4AB / 10-S4N FieldInfo primitive signature type consumer ·
  状态：11-S3I signature type-node view 的 primitive payload public `FieldInfo` consumer 子切片完成；完整字段值读写、
  TypeDef/TypeRef signature binding、recursive type-node reflection object、TypeSpec/generic layout materialization、
  module full reflection object link 和 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在在已验证 `FIELD_SIG` field type-node 为
  `ZR_METADATA_SIGNATURE_NODE_PRIMITIVE` 时，把 type-node payload0 作为 `FieldInfo.fieldTypeSignatureValueType`
  暴露，并写出 `FieldInfo.fieldTypeSignatureTypeName`。该路径复用既有 type-node view，不新增 zrp row，
  不改变 code-registration ABI，也不覆盖 FieldDef layout-derived `typeName`。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 primitive signature value type/name 字段后运行失败；
  GREEN 后 FieldDef token object 在 `PRIMITIVE(BOOL)` fixture 上暴露 `ZR_VALUE_TYPE_BOOL` 与 `"bool"`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4n-fieldinfo-field-signature-primitive-type.md`。
  备注：本记录只声明 primitive signature type-node payload 可被最小 public FieldInfo 消费；不声明完整
  signature-derived field type binding、TypeDef/TypeRef token/layout binding、字段值 read/write marshaling、跨模块
  FieldRef/TypeRef、DESCRIPTION promotion、trim analyzer 或完整 11-S4 关闭。

- 2026-06-30 20:32:08 +08:00 · 11-S4AA / 10-S4M FieldInfo field signature type-node summary consumer ·
  状态：11-S3I signature type-node view 的 public `FieldInfo` consumer 子切片完成；完整字段值读写、
  signature-derived field type binding、recursive type-node reflection object、TypeSpec/generic layout materialization、
  module full reflection object link 和 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在在 FieldDef token 的 validated `FIELD_SIG`
  header 成功后，继续复用 `ZrCore_MetadataRuntime_ReadSignatureTypeNode(&signatureView.blob, fieldTypeBlobOffset, ...)`
  读取 field type node，并写入 public `FieldInfo.fieldTypeSignatureNode`、`fieldTypeSignatureBlobOffset`、
  `fieldTypeSignatureNextBlobOffset`、`fieldTypeSignaturePayload0/1`、`fieldTypeSignatureBaseTypeBlobOffset`、
  `fieldTypeSignatureChildCount` 与 `fieldTypeSignatureChildListBlobOffset`。该路径不新增 zrp row，不改变
  code-registration ABI。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 field signature type-node summary 字段后运行失败；
  GREEN 后 FieldDef token object 暴露 fixture 中 `PRIMITIVE(BOOL)` type-node 的 node、offset、next offset 与 payload。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4m-fieldinfo-field-signature-type-node.md`。
  备注：本记录只声明既有 validated signature type-node view 可被最小 public FieldInfo 消费；不声明
  signature-derived field type binding、recursive type-node reflection object、字段值 read/write marshaling、跨模块
  FieldRef/TypeRef、DESCRIPTION promotion、trim analyzer 或完整 11-S4 关闭。

- 2026-06-30 20:13:27 +08:00 · 11-S4Z / 10-S4L FieldInfo validated field signature header consumer ·
  状态：11-S3 field signature header view 的 public `FieldInfo` consumer 子切片完成；完整字段值读写、
  signature-derived field type binding、TypeSpec/generic layout materialization、module full reflection object link 和
  metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在对 FieldDef token 调用
  `ZrCore_MetadataRuntime_ReadSignatureView()`，并只在 validated root 为 `ZR_METADATA_SIGNATURE_NODE_FIELD_SIG`
  时写入 public `FieldInfo.signatureRootNode`、`FieldInfo.signatureFlags` 与 `FieldInfo.fieldTypeBlobOffset`。
  该路径复用既有 signature blob validation/header view，不新增 zrp row，不改变 code-registration ABI。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 validated field signature header 字段后运行失败；
  GREEN 后 FieldDef token object 暴露 fixture 中的 `FIELD_SIG` root、flags `1` 和 field type blob offset `2`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4l-fieldinfo-field-signature-header.md`。
  备注：本记录只声明既有 validated field signature header view 可被最小 public FieldInfo 消费；不声明
  signature-derived field type binding、recursive type-node reflection object、字段值 read/write marshaling、跨模块
  FieldRef/TypeRef、DESCRIPTION promotion、trim analyzer 或完整 11-S4 关闭。

- 2026-06-30 19:54:58 +08:00 · 11-S4Y / 10-S4K FieldInfo FieldDef signature blob coordinate carrier ·
  状态：11-S4 FieldDef row 的 public `FieldInfo` raw signature blob coordinate consumer 子切片完成；完整字段值读写、
  field signature semantic binding、module full reflection object link、TypeSpec/generic layout materialization 和
  metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在读取已解析 FieldDef row 的
  `signatureBlobOffset` / `signatureBlobLength`，并写入 public `FieldInfo.signatureBlobOffset` /
  `FieldInfo.signatureBlobLength`。该路径不新增 zrp row，不改变 code-registration ABI，只把既有 row 字段作为
  raw coordinate consumer 暴露出来。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `signatureBlobOffset` / `signatureBlobLength`
  后运行失败；GREEN 后 FieldDef token object 暴露 fixture 中的 raw coordinate `4/7`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4k-fieldinfo-fielddef-signature-blob.md`。
  备注：本记录只声明 FieldDef row raw signature blob offset/length 可被最小 public FieldInfo 消费；不声明
  blob slice validation、field signature parser、signature-derived field type binding、字段值 read/write marshaling、
  跨模块 FieldRef/TypeRef、DESCRIPTION promotion、trim analyzer 或完整 11-S4 关闭。

- 2026-06-30 19:39:14 +08:00 · 11-S4X / 10-S4J FieldInfo FieldDef flags carrier ·
  状态：11-S4 FieldDef row 的 public `FieldInfo` raw flags consumer 子切片完成；完整字段值读写、
  module full reflection object link、TypeSpec/generic layout materialization 和 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在读取已解析 FieldDef row 的 `flags`，并写入
  public `FieldInfo.metadataFlags`。该路径不新增 zrp row，不改变 code-registration ABI，只把既有 row 字段作为
  raw integer consumer 暴露出来。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `metadataFlags` 后运行失败；GREEN 后 FieldDef
  token object 暴露 fixture 中的 raw flags `0xA5`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4j-fieldinfo-fielddef-flags.md`。
  备注：本记录只声明 FieldDef row raw flags 可被最小 public FieldInfo 消费；不声明 flags 位语义、
  `isStatic`/`isConst` 映射、字段值 read/write marshaling、跨模块 FieldRef/TypeRef、DESCRIPTION promotion、
  trim analyzer 或完整 11-S4 关闭。

- 2026-06-30 19:26:41 +08:00 · 11-S4W / 10-S4I FieldInfo moduleName carrier ·
  状态：11-S4 FieldDef binding view 的 public `FieldInfo` module name consumer 子切片完成；完整字段值读写、
  module full reflection object link、TypeSpec/generic layout materialization 和 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在从 attached metadata runtime 的 module 读取
  `moduleName`，缺失时回退 `fullPath`，并写入 public `FieldInfo.moduleName`。该路径不新增 zrp row，不改变
  code-registration ABI，只扩大 public consumer 暴露面。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `moduleName` string 后运行失败；GREEN 后 FieldDef
  token object 暴露 synthetic module name `geometry`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4i-fieldinfo-module-name.md`。
  备注：本记录只声明 FieldDef binding view/string-pool consumer 可暴露最小 module name；不声明 module
  reflection object/cache、字段值 read/write marshaling、跨模块 FieldRef/TypeRef、DESCRIPTION promotion、
  trim analyzer 或完整 11-S4 关闭。

- 2026-06-30 19:11:22 +08:00 · 11-S4V / 10-S4H FieldInfo owner object link ·
  状态：11-S4 FieldDef binding view 的 public `FieldInfo` owner link consumer 子切片完成；完整字段值读写、
  module full reflection object link、TypeSpec/generic layout materialization 和 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在把 public `owner` 字段链接到同一
  `declaringType` type literal object。该路径不新增 zrp row，不改变 code-registration ABI，只扩大 public consumer
  暴露面。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `owner` object link 后运行失败；GREEN 后 FieldDef
  token object 的 owner 与 declaringType 指针一致。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4h-fieldinfo-owner-link.md`。
  备注：本记录只声明 FieldDef binding view/string-pool consumer 可暴露最小 owner link；不声明字段值
  read/write marshaling、跨模块 FieldRef/TypeRef、DESCRIPTION promotion、trim analyzer 或完整 11-S4 关闭。

- 2026-06-30 18:58:39 +08:00 · 11-S4U / 10-S4G FieldInfo declaring type object link ·
  状态：11-S4 FieldDef binding view 的 public `FieldInfo` owner identity consumer 子切片完成；完整字段值读写、
  owner/module full reflection object links、TypeSpec/generic layout materialization 和 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 继续消费 FieldDef owner TypeDef row 的 zrp string-pool
  name，并将其写入 `ownerTypeName`、`declaringTypeName` 和 nested `declaringType` type literal object。该路径不新增
  zrp row，不改变 code-registration ABI，只扩大 public consumer 暴露面。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 declaring type 字段后运行失败；GREEN 后 FieldDef
  token object 的 field `type` 与 `declaringType` nested reflection objects 均通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4g-fieldinfo-declaring-type-object.md`。
  备注：本记录只声明 FieldDef binding view/string-pool consumer 可暴露最小 declaring type carrier；不声明字段值
  read/write marshaling、跨模块 FieldRef/TypeRef、DESCRIPTION promotion、trim analyzer 或完整 11-S4 关闭。

- 2026-06-30 18:38:42 +08:00 · 11-S4T / 10-S4F minimum FieldDef token FieldInfo public object ·
  状态：11-S4 FieldDef binding view 的 public reflection consumer 子切片完成；完整字段值读写、
  owner/module object links、完整 `FieldInfo` 行为、TypeSpec/generic layout materialization 和 metadata sweep 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 以 FieldDef token 为入口，复用
  `ZrCore_Reflection_ResolveToken()` 暴露的 FieldDef row、owner TypeDef row、field type token/layout id、offset/size
  与 11-S4 TypeDef layout binding view；同时通过 attached zrp string pool 解码 field/owner/field-type name，
  构造最小 public `FieldInfo` reflection object。该消费路径不新增 zrp row，不改变 code-registration ABI。
  RED/GREEN：RED 为 focused reflection token resolver 测试链接缺失新 API；GREEN 后 FieldDef token object 形态、
  null runtime/state 和 wrong token 负向路径均通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s4f-fielddef-token-fieldinfo-object.md`。
  备注：本记录只声明 FieldDef binding view/string-pool consumer 已能物化最小 FieldInfo object；不声明字段值
  read/write marshaling、跨模块 FieldRef/TypeRef、DESCRIPTION promotion、trim analyzer 或完整 11-S4 关闭。

- 2026-06-30 18:17:22 +08:00 · 11-S4S / 10-S5N / 12-S5M runtime bound TypeRef token layout resolver ·
  状态：11-S4 runtime layout resolver 支撑子切片完成；完整跨模块 provider runtime lookup/context、metadata sweep、
  public FieldInfo/字段值读写和 annotation dataflow 仍未关闭。
  完成项目：metadata runtime 的 `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` 在既有 TypeDef/TypeSpec 之外新增
  attached `TYPE_REF` 分支；它读取 `SZrMetadataTokenRecord.targetMetadataToken`，要求目标为当前 runtime 可解析的 `TYPE_DEF`，
  复用 TypeDef layout binding view，并校验 target signature token/hash、target module signature hash、layout version/hash
  后缓存 TypeRef token 命中。
  RED/GREEN：RED1 为 TypeRef 正向 focused test 得到 NULL；GREEN1 后 resolver 返回目标 TypeDef layout/id 并保留 cache。
  RED2 为 module identity mismatch 用例仍返回非 NULL；GREEN2 后 module hash 不匹配会拒绝。layout version/hash mismatch
  负向路径同步覆盖。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `metadata_runtime_typespec_layout` 17/0、`metadata_runtime_query`
  24/0、`reflection_token_resolve` 7/0、`metadata_type_ref_binding` 8/0；三套 focused CTest 均为 4/4。WSL GCC 初次
  combined CTest 曾出现 transient wrapper `No such file or directory`，直接执行、单项 CTest 和重跑 combined CTest 均通过。
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5n-runtime-bound-typeref-token-layout-resolver.md`。
  备注：这是 attached/current-runtime bound TypeRef -> TypeDef layout 解析；不声明跨模块 provider selection/version
  compatibility、embedded zrp token-record 独立扫描、public reflection object、字段值 marshaling 或完整 11-S4 关闭。

- 2026-06-30 10:49:57 +08:00 · 10-S2Y / 10-S3AC / 11-S2D generated Method.Invoke bool three-arg consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 `bool(bool,bool,bool)`
  参数解包 + 返回装箱子切片完成；完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：新增 `backend_aot_c_reflection_bool_three_arg_invokers.h/.c`，按
  `backend_aot_c_can_emit_typed_bool_three_arg_thunk()` 选择 generated cases；`backend_aot_c_typed_bool_three_arg_thunks.c`
  补齐当前 cleanup-reset 短路 AND 形态识别；generated helper 校验 bool return、三个 bool parameter base type、
  三个 runtime `args` 类型和 `outReturn`，再按 `method->functionIndex` 调用
  `zr_aot_typed_bool_fn_<index>(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2)` 并写入 boxed `SZrTypeValue`。
  该 consumer 只读取既有 method binding/functionIndex，不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 bool three-arg reflection bucket 时，WSL gcc 失败在
  `reflectionBoolThreeArgInvokersSourceText` 非空断言；GREEN 后 frame setup contracts 1/0、shared-library smoke 13/0。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2y-method-invoke-bool-three-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated bool three-arg MethodInfo/functionIndex consumer；不声明 public `MethodInfo` 对象、
  四参数及以上 `Method.Invoke` marshaling、numeric widening、MethodSpec runtime instance binding、
  cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 10:13:18 +08:00 · 10-S2X / 10-S3AB / 11-S2D generated Method.Invoke f64 three-arg consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 `float(float,float,float)`
  参数解包 + 返回装箱子切片完成；完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：扩展 `backend_aot_c_reflection_numeric_three_arg_invokers.h/.c`，按
  `backend_aot_c_can_emit_typed_f64_three_arg_thunk()` 和
  `backend_aot_c_can_emit_typed_f64_three_arg_state_free_thunk()` 选择 generated cases；
  `backend_aot_c_typed_f64_thunks.h` 暴露既有 three-arg eligibility predicates；generated helper 校验 double return、
  三个 double parameter base type、三个 runtime `args` 类型和 `outReturn`，再按 `method->functionIndex`
  调用 state-free 或 stateful `zr_aot_typed_f64_fn_<index>(...)` 并写入 boxed `SZrTypeValue`。
  该 consumer 只读取既有 method binding/functionIndex，不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 f64 numeric three-arg reflection bucket 时，WSL gcc 失败在缺少
  `static TZrBool backend_aot_c_method_metadata_has_f64_three_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2x-method-invoke-f64-three-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated f64 three-arg MethodInfo/functionIndex consumer；不声明 public `MethodInfo` 对象、
  bool 三参数 `Method.Invoke` marshaling、numeric widening、MethodSpec runtime instance binding、
  cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 09:52:48 +08:00 · 10-S2W / 10-S3AA / 11-S2D generated Method.Invoke uint64 three-arg consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 `uint64(uint64,uint64,uint64)`
  参数解包 + 返回装箱子切片完成；完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：扩展 `backend_aot_c_reflection_numeric_three_arg_invokers.h/.c`，按
  `backend_aot_c_can_emit_typed_u64_three_arg_thunk()` 和
  `backend_aot_c_can_emit_typed_u64_three_arg_state_free_thunk()` 选择 generated cases；
  generated helper 校验 uint64 return、三个 uint64 parameter base type、三个 runtime `args` 类型和 `outReturn`，
  再按 `method->functionIndex` 调用 state-free 或 stateful `zr_aot_typed_u64_fn_<index>(...)` 并写入 boxed
  `SZrTypeValue`。该 consumer 只读取既有 method binding/functionIndex，不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 u64 numeric three-arg reflection bucket 时，WSL gcc 失败在缺少
  `#include "backend_aot_c_typed_u64_three_arg_thunks.h"`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2w-method-invoke-uint64-three-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated u64 three-arg MethodInfo/functionIndex consumer；不声明 public `MethodInfo` 对象、
  f64/bool 三参数 `Method.Invoke` marshaling、numeric widening、MethodSpec runtime instance binding、
  cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 09:32:55 +08:00 · 10-S2V / 10-S3Z / 11-S2D generated Method.Invoke int64 three-arg consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 `int64(int64,int64,int64)`
  参数解包 + 返回装箱子切片完成；完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：新增 `backend_aot_c_reflection_numeric_three_arg_invokers.h/.c`，按
  `backend_aot_c_can_emit_typed_i64_three_arg_thunk()` 和
  `backend_aot_c_can_emit_typed_i64_three_arg_state_free_thunk()` 选择 generated cases；
  generated helper 校验 int64 return、三个 int64 parameter base type、三个 runtime `args` 类型和 `outReturn`，
  再按 `method->functionIndex` 调用 state-free 或 stateful `zr_aot_typed_i64_fn_<index>(...)` 并写入 boxed
  `SZrTypeValue`。该 consumer 只读取既有 method binding/functionIndex，不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 numeric three-arg reflection bucket 时，WSL gcc 失败在缺少
  `#include "backend_aot_c_reflection_numeric_three_arg_invokers.h"`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2v-method-invoke-int64-three-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated i64 three-arg MethodInfo/functionIndex consumer；不声明 public `MethodInfo` 对象、
  u64/f64/bool 三参数 `Method.Invoke` marshaling、numeric widening、MethodSpec runtime instance binding、
  cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 09:05:37 +08:00 · 10-S2U / 10-S3Y / 11-S2D generated Method.Invoke bool-return numeric comparison two-arg consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 `bool(int,int)`、`bool(uint,uint)`、
  `bool(float,float)` 参数解包 + 返回装箱子切片完成；完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：新增 `backend_aot_c_reflection_bool_numeric_invokers.h/.c`，按
  `backend_aot_c_can_emit_typed_bool_i64_two_arg_thunk()`、
  `backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk()`、
  `backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk()` 筛选 signed/unsigned/float comparison typed helper，
  生成 `zr_aot_try_invoke_bool_i64_two_arg(...)`、`zr_aot_try_invoke_bool_u64_two_arg(...)`、
  `zr_aot_try_invoke_bool_f64_two_arg(...)`，按 `method->functionIndex` 调
  `zr_aot_typed_bool_fn_<index>(zr_aot_arg0, zr_aot_arg1)` 并写入 boxed `SZrTypeValue`。
  该 consumer 只读取既有 method binding/functionIndex，不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 bool-return numeric reflection buckets 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_bool_i64_two_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2u-method-invoke-bool-numeric-two-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated bool-return numeric comparison two-arg MethodInfo/functionIndex consumer；不声明 public `MethodInfo` 对象、
  三参数及以上 `Method.Invoke` marshaling、numeric widening、MethodSpec runtime instance binding、cross-module token publication/rewrite
  或完整 11-S2 关闭。

- 2026-06-30 08:35:02 +08:00 · 10-S2T / 10-S3X / 11-S2D generated Method.Invoke f64 two-arg argument-unbox consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 float(float, float) 参数解包 + 返回装箱子切片完成；
  完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_f64_thunks.h` 暴露
  `backend_aot_c_can_emit_typed_f64_two_arg_thunk()` 和
  `backend_aot_c_can_emit_typed_f64_two_arg_state_free_thunk()`；`backend_aot_c_reflection_invokers.c` 筛选可发射的
  f64 two-arg typed helper，生成 `zr_aot_try_invoke_f64_two_arg(...)`，按 `method->functionIndex` 调
  state-free `zr_aot_typed_f64_fn_<index>(zr_aot_arg0, zr_aot_arg1)` 或 stateful divide/modulo helper，
  并写入 boxed `SZrTypeValue`。该 consumer 只读取既有 method binding/functionIndex，不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 f64 two-arg reflection bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_f64_two_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `sum_ratio(left: float, right: float): float` 的 reflection invoker 写出
  `ZR_VALUE_TYPE_DOUBLE`/3.75。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、reflection token resolve、
  reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2t-method-invoke-f64-two-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated f64(float,float) two-arg MethodInfo/functionIndex consumer；不声明 public `MethodInfo` 对象、
  bool-return numeric comparison buckets、三参数及以上 `Method.Invoke` marshaling、MethodSpec runtime instance binding、
  cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 08:18:43 +08:00 · 10-S2S / 10-S3W / 11-S2D generated Method.Invoke bool two-arg argument-unbox consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 bool(bool, bool) 参数解包 + 返回装箱子切片完成；
  完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_bool_thunks.h` 暴露 `backend_aot_c_can_emit_typed_bool_two_arg_thunk()`；
  `backend_aot_c_reflection_invokers.c` 筛选可发射的 bool two-arg typed helper，生成
  `zr_aot_try_invoke_bool_two_arg(...)`，按 `method->functionIndex` 调
  `zr_aot_typed_bool_fn_<index>(zr_aot_arg0, zr_aot_arg1)`，并写入 boxed `SZrTypeValue`。该 consumer 只读取既有
  method binding/functionIndex，不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 bool two-arg reflection bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_bool_two_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `same_truth(left: bool, right: bool): bool` 的 reflection invoker 写出
  `ZR_VALUE_TYPE_BOOL`/true。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、reflection token resolve、
  reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2s-method-invoke-bool-two-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated bool(bool,bool) two-arg MethodInfo/functionIndex consumer；不声明 public `MethodInfo` 对象、
  bool-return numeric comparison buckets、f64 二参数桶、三参数及以上 `Method.Invoke` marshaling、
  MethodSpec runtime instance binding、cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 08:03:24 +08:00 · 10-S2R / 10-S3V / 11-S2D generated Method.Invoke uint64 two-arg argument-unbox consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 uint64(uint64, uint64) 参数解包 + 返回装箱子切片完成；
  完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_u64_thunks.h` 暴露 `backend_aot_c_can_emit_typed_u64_two_arg_thunk()` 与
  `backend_aot_c_can_emit_typed_u64_two_arg_state_free_thunk()`；`backend_aot_c_reflection_invokers.c` 筛选可发射的
  u64 two-arg typed helper，生成 `zr_aot_try_invoke_u64_two_arg(...)`，按 `method->functionIndex` 调
  `zr_aot_typed_u64_fn_<index>(zr_aot_arg0, zr_aot_arg1)` 或保留 stateful helper 的
  `zr_aot_typed_u64_fn_<index>(state, zr_aot_arg0, zr_aot_arg1)`，并写入 boxed `SZrTypeValue`。该 consumer 只读取既有
  method binding/functionIndex，不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 u64 two-arg reflection bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_u64_two_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `sum_unsigned(left: uint, right: uint): uint` 的 reflection invoker 写出
  `ZR_VALUE_TYPE_UINT64`/123。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、reflection token resolve、
  reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2r-method-invoke-uint64-two-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated u64 two-arg MethodInfo/functionIndex consumer；不声明 public `MethodInfo` 对象、
  bool/f64 二参数桶、三参数及以上 `Method.Invoke` marshaling、MethodSpec runtime instance binding、
  cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 07:45:38 +08:00 · 10-S2Q / 10-S3U / 11-S2D generated Method.Invoke int64 two-arg argument-unbox consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 int64(int64, int64) 参数解包 + 返回装箱子切片完成；
  完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_i64_thunks.h` 暴露 `backend_aot_c_can_emit_typed_i64_two_arg_thunk()` 与
  `backend_aot_c_can_emit_typed_i64_two_arg_state_free_thunk()`；`backend_aot_c_reflection_invokers.c` 筛选可发射的
  i64 two-arg typed helper，生成 `zr_aot_try_invoke_i64_two_arg(...)`，按 `method->functionIndex` 调
  `zr_aot_typed_i64_fn_<index>(zr_aot_arg0, zr_aot_arg1)` 或保留 stateful helper 的
  `zr_aot_typed_i64_fn_<index>(state, zr_aot_arg0, zr_aot_arg1)`，并写入 boxed `SZrTypeValue`。该 consumer 只读取既有
  method binding/functionIndex，不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 i64 two-arg reflection bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_i64_two_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `sum_values(left: int, right: int): int` 的 reflection invoker 写出
  `ZR_VALUE_TYPE_INT64`/42。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2q-method-invoke-int64-two-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated i64 two-arg MethodInfo/functionIndex consumer；不声明 public `MethodInfo` 对象、
  更多标量二参数桶、三参数及以上 `Method.Invoke` marshaling、MethodSpec runtime instance binding、
  cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 07:21:33 +08:00 · 10-S2P / 10-S3T / 11-S2D generated Method.Invoke f64 one-arg argument-unbox consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 float(float) 参数解包 + 返回装箱子切片完成；
  完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_f64_thunks.h` 暴露
  `backend_aot_c_can_emit_typed_f64_one_arg_thunk()`；`backend_aot_c_reflection_invokers.c` 筛选可发射的 one-arg f64
  typed helper，生成 `zr_aot_try_invoke_f64_one_arg(...)`，按 `method->functionIndex` 调
  `zr_aot_typed_f64_fn_<index>(zr_aot_arg0)` 并写入 boxed `SZrTypeValue`。该 consumer 只读取既有
  method binding/functionIndex，不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 f64 one-arg reflection bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_f64_one_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `echo_ratio(value: float): float` 的 reflection invoker 写出
  `ZR_VALUE_TYPE_DOUBLE`/1.75。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2p-method-invoke-f64-one-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated f64 one-arg MethodInfo/functionIndex consumer；不声明 public `MethodInfo` 对象、
  更多 `Method.Invoke` marshaling、MethodSpec runtime instance binding、cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 07:05:20 +08:00 · 10-S2O / 10-S3S / 11-S2D generated Method.Invoke bool one-arg argument-unbox consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 bool(bool) 参数解包 + 返回装箱子切片完成；
  完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_bool_thunks.h` 暴露
  `backend_aot_c_can_emit_typed_bool_one_arg_thunk()`；`backend_aot_c_reflection_invokers.c` 筛选可发射的 one-arg bool
  typed helper，生成 `zr_aot_try_invoke_bool_one_arg(...)`，按 `method->functionIndex` 调
  `zr_aot_typed_bool_fn_<index>(zr_aot_arg0)` 并写入 boxed `SZrTypeValue`。该 consumer 只读取既有
  method binding/functionIndex，不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 bool one-arg reflection bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_bool_one_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `echo_truth(value: bool): bool` 的 reflection invoker 写出 `ZR_VALUE_TYPE_BOOL`/false。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2o-method-invoke-bool-one-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated bool one-arg MethodInfo/functionIndex consumer；不声明 public `MethodInfo` 对象、
  更多 `Method.Invoke` marshaling、MethodSpec runtime instance binding、cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 06:45:51 +08:00 · 10-S2-maint / 10-S3-maint / 11-S2D reflection invoker emitter split ·
  状态：generated reflection invoker emitter 已从 MethodInfo metadata emitter 中拆出；这是继续消费 11-S2D
  MethodInfo/functionIndex binding 的维护前置，完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：新增 `backend_aot_c_reflection_invokers.h/.c` 承载 `backend_aot_write_c_reflection_invokers(...)` 和当前
  i64/u64/bool/f64 no-arg、i64/u64 one-arg generated invoker buckets；`backend_aot_c_method_metadata.h/.c`
  不再暴露 invoker emitter API，只保留 MethodInfo/signature/method-token/GC-root metadata 发射职责。
  `backend_aot_c_method_metadata.c` 从 948 行降为 552 行，新 invoker 文件 397 行。
  RED/GREEN：以既有 frame setup source contract + shared-library smoke 作为 refactor guard；拆分后 WSL gcc GREEN 为
  frame setup contracts 1/0、shared-library smoke 13/0，CMake glob 重新纳入新源文件。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2-maint-reflection-invoker-emitter-split.md`。
  备注：本记录只关闭模块边界整理；不新增 code-registration ABI、不改变 11-S2D binding view，不声明 public `MethodInfo`
  对象、更多 `Method.Invoke` marshaling、MethodSpec runtime instance binding、cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 06:25:21 +08:00 · 10-S2N / 10-S3R / 11-S2D generated Method.Invoke uint64 one-arg argument-unbox consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 uint64(uint64) 参数解包 + 返回装箱子切片完成；
  完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_u64_thunks.h` 暴露
  `backend_aot_c_can_emit_typed_u64_one_arg_thunk()`；`backend_aot_c_method_metadata.c` 筛选可发射的 one-arg u64
  typed helper，生成 `zr_aot_try_invoke_u64_one_arg(...)`，按 `method->functionIndex` 调
  `zr_aot_typed_u64_fn_<index>(zr_aot_arg0)` 并写入 boxed `SZrTypeValue`。该 consumer 只读取既有
  method binding/functionIndex，不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 u64 one-arg reflection bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_u64_one_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `echo_unsigned(value: uint): uint` 的 reflection invoker 写出 `ZR_VALUE_TYPE_UINT64`/101。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2n-method-invoke-uint64-one-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated u64 one-arg MethodInfo/functionIndex consumer；不声明 public `MethodInfo` 对象、
  更多 `Method.Invoke` marshaling、MethodSpec runtime instance binding、cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 06:05:32 +08:00 · 10-S2M / 10-S3Q / 11-S2D generated Method.Invoke int64 one-arg argument-unbox consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 int64(int64) 参数解包 + 返回装箱子切片完成；
  完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_i64_thunks.h` 暴露
  `backend_aot_c_can_emit_typed_i64_one_arg_thunk()`；`backend_aot_c_method_metadata.c` 筛选可发射的 one-arg i64
  typed helper，生成 `zr_aot_try_invoke_i64_one_arg(...)`，按 `method->functionIndex` 调
  `zr_aot_typed_i64_fn_<index>(zr_aot_arg0)` 并写入 boxed `SZrTypeValue`。该 consumer 只读取既有
  method binding/functionIndex，不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 i64 one-arg reflection bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_i64_one_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `echo(value: int): int` 的 reflection invoker 写出 `ZR_VALUE_TYPE_INT64`/99。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2m-method-invoke-int64-one-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated i64 one-arg MethodInfo/functionIndex consumer；不声明 public `MethodInfo` 对象、
  更多 `Method.Invoke` marshaling、MethodSpec runtime instance binding、cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 05:44:51 +08:00 · 10-S2L / 10-S3P / 11-S2D generated Method.Invoke f64 no-arg return boxing consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 f64 no-arg return-boxing 子切片完成；
  完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_f64_thunks.h` 暴露
  `backend_aot_c_can_emit_typed_f64_no_arg_thunk()`；`backend_aot_c_method_metadata.c` 筛选可发射的 no-arg f64
  typed helper，生成 `zr_aot_try_invoke_f64_no_arg(...)`，按 `method->functionIndex` 调
  `zr_aot_typed_f64_fn_<index>()` 并写入 boxed `SZrTypeValue`。该 consumer 只读取既有 method binding/functionIndex，
  不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 f64 reflection bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_f64_no_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `ratio(): float` 的 reflection invoker 写出 `ZR_VALUE_TYPE_DOUBLE`/2.5。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2l-method-invoke-f64-no-arg-return-boxing.md`。
  备注：本记录只关闭 generated f64 no-arg return-boxing consumer；参数 unbox、object/inline 返回、完整签名桶、
  public method reflection object、MethodSpec 专用 code slot、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 05:26:08 +08:00 · 10-S2K / 10-S3O / 11-S2D generated Method.Invoke bool no-arg return boxing consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 bool no-arg return-boxing 子切片完成；
  完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_bool_thunks.h` 暴露
  `backend_aot_c_can_emit_typed_bool_no_arg_thunk()`；`backend_aot_c_method_metadata.c` 筛选可发射的 no-arg bool
  typed helper，生成 `zr_aot_try_invoke_bool_no_arg(...)`，按 `method->functionIndex` 调
  `zr_aot_typed_bool_fn_<index>()` 并写入 boxed `SZrTypeValue`。该 consumer 只读取既有 method binding/functionIndex，
  不改变 code-registration ABI。
  RED/GREEN：RED 为 frame setup source contract 要求 bool reflection bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_bool_no_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `truth(): bool` 的 reflection invoker 写出 `ZR_VALUE_TYPE_BOOL`/true。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2k-method-invoke-bool-no-arg-return-boxing.md`。
  备注：本记录只关闭 generated bool no-arg return-boxing consumer；参数 unbox、f64/object/inline 返回、完整签名桶、
  public method reflection object、MethodSpec 专用 code slot、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 05:06:13 +08:00 · 10-S2J / 10-S3N / 11-S2D generated Method.Invoke uint64 no-arg return boxing consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 uint64 no-arg return-boxing 子切片完成；
  完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_method_metadata.c` 现在筛选可发射的 no-arg u64 typed helper，
  生成 `zr_aot_try_invoke_u64_no_arg(...)`，按 `method->functionIndex` 调 `zr_aot_typed_u64_fn_<index>()` 并写入
  `ZrCore_Value_InitAsUInt(...)`。code registration 的 `functionPointers` 仍指完整执行 thunk；该 thunk 的返回值
  是执行成功标志，unsupported fallback 不写 `outReturn`。
  RED/GREEN：RED 为 generated source contract 要求 u64 reflection bucket 后缺少
  `backend_aot_c_method_metadata_has_u64_no_arg_reflection_case(`；GREEN 后 source contract 1/0、shared-library smoke
  13/0，并通过 `unsigned_answer(): uint` 的 generated invoker 实际写出 `ZR_VALUE_TYPE_UINT64`/13。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke
  13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding
  2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  `git diff --check` 退出 0，仅有既有 LF/CRLF 规范化警告。
  产出：`tests/acceptance/2026-06-30-aot-10-s2j-method-invoke-uint64-no-arg-return-boxing.md`。
  备注：本记录只消费已有 11-S2D MethodInfo/functionIndex carrier 做 generated uint64 no-arg return box；不声明
  typed target ABI carrier、参数 unbox、bool/f64/object/inline 返回、public method reflection object 或完整 11-S2。

- 2026-06-30 04:45:07 +08:00 · 10-S2I / 10-S3M / 11-S2D generated Method.Invoke int64 no-arg return boxing consumer ·
  状态：generated reflection invoker 消费 11-S2D MethodInfo/functionIndex binding 的 int64 no-arg return-boxing 子切片完成；
  完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_method_metadata.c` 现在接收 function table，筛选可发射的 no-arg i64 typed helper，
  生成 `zr_aot_try_invoke_i64_no_arg(...)`，按 `method->functionIndex` 调 `zr_aot_typed_i64_fn_<index>()` 并写入
  `ZrCore_Value_InitAsInt(...)`。code registration 的 `functionPointers` 仍指完整执行 thunk；该 thunk 的返回值
  是执行成功标志，unsupported fallback 不写 `outReturn`。
  RED/GREEN：初始 RED 缺 generated `value.h` include；naive raw target-return 方案在 shared-library smoke 中得到
  `Expected 42 Was 1`，确认 full entry thunk 不是业务返回 carrier；修正后的 RED 缺 function-table-fed invoker emitter API；
  GREEN 后 source contract 1/0、shared-library smoke 13/0。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke
  13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding
  2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  `git diff --check` 退出 0，仅有既有 LF/CRLF 规范化警告。
  产出：`tests/acceptance/2026-06-30-aot-10-s2i-method-invoke-int64-no-arg-return-boxing.md`。
  备注：本记录只消费已有 11-S2D MethodInfo/functionIndex carrier 做 generated int64 no-arg return box；不声明
  typed target ABI carrier、参数 unbox、其它返回类型、public method reflection object 或完整 11-S2。

- 2026-06-30 04:17:27 +08:00 · 10-S2H / 10-S3L / 11-S2D Method.Invoke void return-slot canonicalization consumer ·
  状态：public counted dispatcher 消费 11-S2D MethodInfo binding 中 no-return signature 的子切片完成；完整
  11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`reflection_token_resolve.c` 在 registered invoker dispatch 后，若 MethodInfo signature 声明无返回值，
  则把最终 `outReturn` 重置为 null，避免 invoker 误写或旧槽值作为 void 方法返回值泄出。
  RED/GREEN：RED 为 focused test 让 no-return signature 下的 synthetic invoker 写出 int64，旧 dispatcher 调用成功但
  `outReturn.type` 仍为 INT64；WSL gcc 失败在 `Expected 0 Was 5`。GREEN 后调用成功且最终 return slot 为 null。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection method invoke 5/0、reflection token resolve 7/0、
  method binding 2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding|metadata_runtime_query` 均 4/4。
  产出：`tests/acceptance/2026-06-30-aot-10-s2h-method-invoke-void-return-slot.md`。
  备注：本记录只消费已有 11-S2D MethodInfo signature carrier 来规范 void/no-return output slot；不声明返回 box、
  typed return register 捕获、参数 unbox、numeric widening、nullable/ownership/staticCType 兼容、public method reflection object 或完整 11-S2。

- 2026-06-30 04:06:53 +08:00 · 10-S2G / 10-S3K / 11-S2D Method.Invoke required return-slot reset consumer ·
  状态：public counted dispatcher 消费 11-S2D MethodInfo binding 中 required return signature 的子切片完成；完整
  11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`reflection_token_resolve.c` 在 MethodInfo signature 声明 return value 且前置 guard 通过后，调用
  registered invoker 前将 `outReturn` 清为 null，使 10-S2F/10-S3J return base-type post-guard 不能再接受调用前
  预填的旧返回值；token resolve arity fixture 改为 synthetic invoker 显式写入返回，保留原有 counted success 覆盖。
  RED/GREEN：RED 为 focused test 预填 bool `outReturn`、但 synthetic invoker 不写返回值时旧 dispatcher 仍返回 true；
  WSL gcc 失败在 `Expected FALSE Was TRUE`。GREEN 后 required return slot 会先被清空，未写返回值返回 false。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection method invoke 4/0、reflection token resolve 7/0、
  method binding 2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding|metadata_runtime_query` 均 4/4。
  产出：`tests/acceptance/2026-06-30-aot-10-s2g-method-invoke-return-slot-reset.md`。
  备注：本记录只消费已有 11-S2D MethodInfo signature return carrier 来关闭 stale return slot；不声明返回 box、
  typed return register 捕获、参数 unbox、numeric widening、nullable/ownership/staticCType 兼容、public method reflection object 或完整 11-S2。

- 2026-06-30 03:47:49 +08:00 · 10-S2F / 10-S3J / 11-S2D Method.Invoke return base-type guard consumer ·
  状态：public counted dispatcher 消费 11-S2D MethodInfo binding 中 return baseType 的子切片完成；完整
  11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`reflection_token_resolve.c` 在 counted dispatcher 调用 registered invoker 后，若 signature 声明 concrete
  非 null/unknown `returnType->baseType`，要求 `outReturn->type` 匹配；越界 return baseType 拒绝成功结果。
  RED/GREEN：RED 为 focused test 让 invoker 在 bool return signature 下写出 int64，旧 dispatcher 仍返回 true；
  WSL gcc 失败在 `Expected FALSE Was TRUE`。GREEN 后 mismatch 返回 false，bool return 返回 true。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection method invoke 3/0、reflection token resolve 7/0、
  method binding 2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding` 均 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s2f-method-invoke-return-base-type-guard.md`。
  备注：本记录只消费已有 11-S2D MethodInfo signature return carrier，不声明返回 box、无副作用预检、
  typed unbox、numeric widening、nullable/ownership/staticCType 兼容、public method reflection object 或完整 11-S2。

- 2026-06-30 03:37:13 +08:00 · 10-S2E / 10-S3I / 11-S2D Method.Invoke fixed parameter base-type guard consumer ·
  状态：public counted dispatcher 消费 11-S2D MethodInfo binding 中 fixed parameter baseType 的子切片完成；完整
  11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`reflection_token_resolve.c` 在 token→MethodInfo/function pointer/invoker binding、arity 和 shape 通过后，
  对每个 fixed `parameterTypes[i].baseType` 做 concrete baseType guard：非 null/unknown 且在 `EZrValueType`
  范围内时必须匹配 `args[i].type`，越界 baseType 直接拒绝。untyped/null/unknown slot 和 varargs 额外参数暂不强制。
  RED/GREEN：RED 为 `test_reflection_method_invoke.c` 新增 bool 参数签名但传入 int64 的 focused 用例后，旧 dispatcher
  仍返回 true 并调用 invoker，WSL gcc 失败在 `Expected FALSE Was TRUE`；GREEN 后错类型被拒绝，修正为 bool 后派发。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection method invoke 2/0、reflection token resolve 7/0、
  method binding 2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding` 均 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s2e-method-invoke-parameter-base-type-guard.md`。
  备注：本记录只消费已有 11-S2D MethodInfo signature carrier，不声明 typed unbox、numeric widening、
  nullable/ownership/staticCType 兼容、返回 box、public method reflection object 或完整 11-S2。

- 2026-06-30 03:22:37 +08:00 · 10-S2D / 10-S3H / 11-S2D Method.Invoke signature shape guard consumer ·
  状态：public counted dispatcher 消费 11-S2D MethodInfo binding 中 signature shape 的子切片完成；完整
  11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`reflection_token_resolve.c` 在 token→MethodInfo/function pointer/invoker binding 成功后、调用
  registered invoker 前，检查 `methodInfo->signature` 的结构完整性：`parameterCount > 0` 时必须有
  `parameterTypes`，`hasReturnValue` 时必须有 `returnType`；缺失时直接拒绝并保持 invoker 未调用。
  新增 `tests/module/test_reflection_method_invoke.c` 作为独立 focused target，避免继续扩大既有
  reflection token resolve 聚合测试。
  RED/GREEN：RED 为新 focused 测试要求 incomplete signature shape 返回 false，但旧 counted dispatcher 仍调用
  invoker，WSL gcc 失败在 `Expected FALSE Was TRUE`；GREEN 后补齐 `parameterTypes` / `returnType` 才允许派发。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection method invoke 1/0、reflection token resolve 7/0、
  method binding 2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding` 均 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s2d-method-invoke-signature-shape-guard.md`。
  备注：本记录只关闭 MethodInfo signature shape 的防御边界；不声明 typed argument compatibility、参数 unbox、
  返回 box、public method reflection object、MethodSpec 专用 code slot、cross-module token rewrite 或完整 11-S2。

- 2026-06-30 03:09:55 +08:00 · 10-S2C / 10-S3G / 11-S2D Method.Invoke signature arity guard consumer ·
  状态：public counted dispatcher 消费 11-S2D MethodInfo binding 中 signature arity 信息的子切片完成；完整
  11-S2/10-S2/10-S3 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_InvokeMethodTokenWithArgCount(...)`，在 10-S2B/10-S3F dispatcher 已解析出
  `methodInfo/methodFunctionPointer/methodInvoker` 后，读取 `methodInfo->signature->parameterCount` 与
  `hasVarArgs`。非 varargs 参数数量必须精确匹配，varargs 允许大于等于 fixed count，非零参数数量下 null `args`
  直接拒绝且不调用 registered invoker。
  RED/GREEN：RED 为 focused reflection token resolve 测试引用缺失 counted dispatcher API 导致 WSL gcc
  implicit declaration / undefined reference；GREEN 后 exact arity、too few、too many、null args 与 varargs 额外参数
  路径均按预期通过/拒绝。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection token resolve 7/0、method binding 2/0、metadata runtime
  query 24/0；WSL gcc 与 MSVC Debug CTest `reflection_token_resolve|metadata_runtime_method_binding` 均 2/2。
  产出：`tests/acceptance/2026-06-30-aot-10-s2c-method-invoke-signature-arity-guard.md`。
  备注：本记录只消费已有 11-S2D MethodInfo binding view 中的 signature pointer；不声明 signature-aware typed
  invoker buckets、参数类型 unbox、返回 box、public method reflection object、cross-module token rewrite 或完整
  11-S2 关闭。

- 2026-06-30 02:57:41 +08:00 · 10-S2B / 10-S3F / 11-S2D token-driven Method.Invoke dispatcher consumer ·
  状态：public dispatcher 消费 11-S2D method binding view 子切片完成；完整 11-S2/10-S2/10-S3 仍未关闭。
  完成项目：`ZrCore_Reflection_InvokeMethodToken(...)` 以 `SZrMetadataRuntime` 和 method token 为入口，
  复用 `ZrCore_Reflection_ResolveToken()` 取得 method binding carrier，并要求 MethodInfo、entry thunk 和
  invoker 均存在后才调用 registered AOT invoker。测试在 synthetic code registration 上证明 11-S2D 的
  method token→MethodInfo/function pointer/invoker view 可被 public invoke dispatcher 实际消费。
  RED/GREEN：RED 为 reflection token resolve focused 测试引用缺失的 dispatcher API 导致 WSL gcc
  implicit declaration / undefined reference；GREEN 后 dispatcher 成功把 state、target、method、self、args、
  outReturn 传入 invoker。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection token resolve 6/0、method binding 2/0、
  metadata runtime query 24/0；WSL gcc 与 MSVC Debug CTest
  `reflection_token_resolve|metadata_runtime_method_binding` 均 2/2。
  产出：`tests/acceptance/2026-06-30-aot-10-s2b-token-driven-method-invoke-dispatcher.md`。
  备注：本记录只消费已有 11-S2D binding view，不声明 public method reflection object、参数/返回 marshaling、
  signature-aware invoker buckets、cross-module token rewrite 或完整 11-S2 关闭。

- 2026-06-30 02:05:51 +08:00 · 10-S3E / 11-S2D MethodSpec public binding carrier consumer ·
  状态：public MethodSpec token resolver 复用 11-S2D underlying method binding view 子切片完成；完整
  11-S2/11-S3/11-S5 仍未关闭。
  完成项目：`reflection_token_resolve.c` 在 `ZrCore_Reflection_ResolveToken(MethodSpec)` 路径中，复用
  11-S3M/11-S5 MethodSpec signature view 的 `methodToken`，再调用
  `ZrCore_MetadataRuntime_ReadMethodBindingView()` 读取 underlying MethodDef 的 AOT function slot、
  `SZrAotMethodInfo`、entry thunk 和 invoker，并写入 public resolved-token carrier。MethodSpec 自身的
  signature record/hash 与 generic argument carrier 保持不变；没有 AOT binding 时仍只返回 metadata carrier。
  RED/GREEN：RED 为 `zr_vm_reflection_token_resolve_test` 要求 MethodSpec resolved token 暴露 underlying
  MethodDef 的 MethodInfo/function pointer/invoker 后失败在 `methodFunctionIndex == 0`；GREEN 后 MethodSpec
  绑定 carrier 和既有 generic argument carrier 同时通过。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection token resolve 5/0、method binding 2/0、
  metadata runtime query 24/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s3e-methodspec-underlying-method-binding-carrier.md`。
  备注：本记录只消费已有 11-S2D binding view，不声明 MethodSpec 专用 generated function slot、
  public generic method reflection object、`Method.Invoke` marshaling、cross-module token rewrite 或完整 11-S5。

- 2026-06-30 01:54:46 +08:00 · 10-S3D / 11-S2D public method binding carrier consumer ·
  状态：public reflection token resolver 消费 11-S2D method binding view 子切片完成；完整 11-S2/10-S3 仍未关闭。
  完成项目：`SZrReflectionResolvedToken` 新增 method binding 字段；`ZrCore_Reflection_ResolveToken()` 对普通
  MethodDef/MethodRef method token 复用 `ZrCore_MetadataRuntime_ReadMethodBindingView()`，在存在 code-registration
  method token binding 时把 `functionIndex`、`SZrAotMethodInfo`、entry thunk 与 invoker 复制到 public carrier；
  缺 binding 时保持 method record/signature carrier 成功返回并让 binding 字段为空。
  RED/GREEN：RED 为 `zr_vm_reflection_token_resolve_test` 新增 MethodDef token binding 断言后缺少 public 字段；
  GREEN 后有 binding 与无 binding 两种路径均通过。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection token resolve 5/0、method binding 2/0、metadata runtime
  query 24/0；WSL gcc 与 MSVC Debug CTest `reflection_token_resolve|metadata_runtime_method_binding` 均 2/2。
  产出：`tests/acceptance/2026-06-30-aot-10-s3d-method-binding-reflection-carrier.md`。
  备注：本记录只消费 11-S2D binding view，不声明 public method reflection object、`Method.Invoke` marshaling、
  MethodSpec runtime instance binding、cross-module token rewrite 或完整 11-S2 关闭。

- 2026-06-30 01:38:26 +08:00 · 11-S2D / 10-S2 method token binding view ·
  状态：11-S2 code-registration method token runtime binding 子切片完成；完整 11-S2/10-S2 仍未关闭，
  public method reflection object、`Method.Invoke` 参数/返回 marshaling、cross-module token rewrite 和 trim
  diagnostics 仍待后续。
  完成项目：新增 `SZrMetadataRuntimeMethodBindingView` 与
  `ZrCore_MetadataRuntime_ReadMethodBindingView()`，从 attached runtime 的 `SZrAotCodeRegistration`
  读取 `methodTokens[]`、`methodInfos[]` 和 `functionPointers[]`，把唯一 local `MEMBER_DEF` token 绑定到
  function slot、`SZrAotMethodInfo`、entry thunk 与 `methodInfo->invoker`。实现拆到
  `metadata_runtime_method_binding.c`，避免继续扩张已有 metadata runtime 主文件。测试新增
  `tests/module/test_metadata_runtime_method_binding.c` 和 CTest `metadata_runtime_method_binding`，覆盖成功查找、
  null/zero/type token/unknown token、缺 token 表、重复 token 与 stale MethodInfo functionIndex。
  RED/GREEN：RED 为 focused 测试编译失败，缺少 `SZrMetadataRuntimeMethodBindingView` 和
  `ZrCore_MetadataRuntime_ReadMethodBindingView()`；GREEN 后 runtime binding 视图和负向防御路径均通过。
  验证：WSL gcc 通过 `zr_vm_metadata_runtime_method_binding_test` 2/0、
  `zr_vm_metadata_runtime_query_test` 24/0、`zr_vm_reflection_token_resolve_test` 4/0，以及 CTest
  `metadata_runtime_method_binding` 1/1；WSL clang 通过三项 2/0、24/0、4/0；Windows MSVC Debug 通过三项
  2/0、24/0、4/0，并通过 CTest `metadata_runtime_method_binding` 1/1。
  产出：`tests/acceptance/2026-06-30-aot-11-s2d-method-token-binding-view.md`。
  备注：本记录只关闭 runtime binding view，不声明 public reflection carrier 消费、`Method.Invoke`
  marshaling、MethodSpec runtime instance binding、cross-module token publication/rewrite 或完整 11-S2 关闭。

- 2026-06-30 01:05:20 +08:00 · 11-S2B / 10-S2 method token code-registration carrier ·
  状态：11-S2 code-registration method token carrier 子切片完成；完整 11-S2/10-S2 仍未关闭，
  token→MethodInfo/function pointer/invoker resolver、public method reflection object、`Method.Invoke` 参数/返回
  marshaling、cross-module token rewrite 和 trim annotation diagnostics 仍待后续。
  完成项目：公共 AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 11u`；`SZrAotCodeRegistration` 与
  `ZrAotCompiledModule` 新增 `methodTokens/methodTokenCount`；`SZrMetadataRuntime` mirror `methodTokenCount`；
  AOT C 发射 `zr_aot_method_tokens[]`，按 `functionIndex` 对齐 `methodInfos[]`，并只为 root module typed exported
  function 填充可靠 `MEMBER_DEF` token；runtime descriptor validation 拒绝 descriptor/codeRegistration method token
  指针/计数不一致、空/非空形态错误或 `methodTokenCount != methodInfoCount`。
  RED/GREEN：RED 为 `zr_vm_aot_c_frame_setup_contracts_test` 要求 method token ABI/emitter/runtime validation 后缺少
  codeRegistration method token mismatch 文本；GREEN 后 ABI、生成器、metadata runtime mirror、runtime validation 和
  generated shared-library method token 断言通过。
  验证：WSL gcc 与 WSL clang 均通过 metadata runtime query 24/0、AOT C source contracts 22/0、frame setup contracts
  1/0、shared-library smoke 13/0、descriptor diagnostics 2/0；Windows MSVC Debug 通过前三项 24/0、22/0、1/0，
  shared-library smoke 13 项 ignored，descriptor diagnostics 2 项 ignored。
  产出：`tests/acceptance/2026-06-30-aot-11-s2b-method-token-carrier.md`。
  备注：本记录不声明 token→MethodInfo lookup、MethodInfo/function pointer/invoker binding、public method reflection
  object、token-driven `Invoke` 或完整 11-S2 关闭。

- 2026-06-30 00:31:26 +08:00 · 11-S3 support / 10-S3C method signature reflection carrier ·
  状态：11-S3 signature record 与 MethodSpec signature view 的 public reflection consumer 子切片完成；完整
  11-S3 仍未关闭，method instantiation materialization、MethodInfo/function pointer/invoker binding、
  row-to-entity materialization、token→运行期实体物化和完整缓存仍待后续。
  完成项目：`ZrCore_Reflection_ResolveToken()` 对 MethodDef/MethodRef method-like carrier 复用
  `ZrCore_MetadataRuntime_ResolveSignatureRecord()` 暴露 paired `SIGNATURE` token record/hash；对 MethodSpec
  `SIGNATURE` token 复用 `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView()`，把 MethodSpec token/record/hash
  作为 method signature identity 复制到 public carrier。metadata runtime 底层 API 未新增新语义，只把既有
  11-S3C/11-S3M 结果接入 10-S3 public carrier。
  RED/GREEN：RED 为 `zr_vm_reflection_token_resolve_test` 要求 MethodDef 与 MethodSpec resolved token 暴露
  method signature carrier 后缺少 `methodSignatureToken/methodSignatureRecord/methodSignatureHash` 字段导致
  WSL gcc 编译失败；GREEN 后普通方法签名 identity 与 MethodSpec 签名 identity 均通过。
  验证：WSL gcc、WSL clang、Windows MSVC Debug 均通过 `zr_vm_metadata_runtime_query_test` 24/0、
  `zr_vm_reflection_token_resolve_test` 4/0、`zr_vm_metadata_runtime_typespec_layout_test` 14/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s3c-method-signature-reflection-carrier.md`。
  备注：本记录不声明 public method reflection object、token-driven `Invoke`、MethodInfo/function pointer
  绑定、cross-module token rewrite、trim analyzer 或完整 11-S3 关闭。

- 2026-06-30 00:17:04 +08:00 · 11-S5 support / 10-S3B MethodSpec token resolver carrier ·
  状态：11-S5 MethodSpec signature view 的 public token resolver consumer 子切片完成；完整 11-S5 仍未关闭，
  与 08 实例化去重键的全链路统一、运行期 generic entity/layout materialization、public generic reflection
  object 和跨模块 generic binding 仍待后续。
  完成项目：`SZrMetadataRuntimeMethodSpecSignatureView` 新增 `methodSpecRecord`，保留已验证 MethodSpec
  `SIGNATURE` token record；`ZrCore_Reflection_ResolveToken()` 对 `GENERIC_INST(MEMBER_REF methodToken, args...)`
  MethodSpec signature token 返回 method-like resolved carrier，暴露 MethodSpec record、underlying method
  token/record、signature hash、argument count 与 argument-list blob offset。
  RED/GREEN：RED 为 `zr_vm_reflection_token_resolve_test` 要求 `ResolveToken(MethodSpec)` 后缺少
  `SZrReflectionResolvedToken.methodToken/methodRecord` 编译失败；随后 `zr_vm_metadata_runtime_query_test` 要求
  MethodSpec signature view 暴露 `methodSpecRecord` 并得到预期编译失败。GREEN 后 MethodSpec signature record
  carrier 与 public token resolver 均通过。
  验证：WSL gcc、WSL clang、Windows MSVC Debug 均通过 metadata runtime query 24/0、reflection token resolve 4/0、
  metadata runtime TypeSpec layout 14/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s3b-methodspec-token-resolve-carrier.md`。
  备注：本记录不声明 MethodSpec runtime instance materialization、generic dictionary、递归泛型实参对象、
  runtime generic layout construction、cross-module token publication/rewrite、full trim analyzer 或完整 11-S5 关闭。

- 2026-06-29 23:56:22 +08:00 · 11-S5 / 10-S4E MethodSpec generic argument view and reflection carrier ·
  状态：11-S5 MethodSpec indexed argument view 与 10-S4E public reflection carrier 子切片完成；完整 11-S5
  仍未关闭，与 08 实例化去重键的全链路统一、运行期 generic entity/layout materialization、public generic
  reflection object 和跨模块 generic binding 仍待后续。
  完成项目：新增 `SZrMetadataRuntimeMethodSpecGenericArgumentView` 与
  `ZrCore_MetadataRuntime_ReadMethodSpecGenericArgumentView()`，在既有 MethodSpec signature view 上按 argument
  index 读取 `GENERIC_INST(MEMBER_REF methodToken, args...)` 的实参节点，并为 direct TypeDef/TypeRef argument
  绑定 argument token/record；`reflection.h` 新增 `SZrReflectionResolvedMethodSpecGenericArgument` 与
  `ZrCore_Reflection_ResolveMethodSpecGenericArgument()`，公开 methodSpec token、method token/record、signature hash、
  argument node/payload 和可选 argument token/record。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 与 `zr_vm_reflection_token_resolve_test` 新增 MethodSpec
  argument fixture 后缺少 runtime view / public carrier 类型与 API 导致 WSL gcc 编译失败；GREEN 后 primitive
  argument、TypeRef argument、null/未 attach/wrong token/out-of-range 负向路径均通过。
  验证：WSL gcc 与 WSL clang 均通过 metadata runtime query 24/0、reflection token resolve 4/0、
  metadata runtime TypeSpec layout 14/0；Windows MSVC Debug 通过同三项 24/0、4/0、14/0。
  产出：`tests/acceptance/2026-06-29-aot-11-s5-methodspec-generic-argument-view.md`。
  备注：本记录不声明 MethodSpec runtime instance materialization、generic dictionary、递归泛型实参对象、
  runtime generic layout construction、cross-module token publication/rewrite、full trim analyzer 或完整 11-S5 关闭。

- 2026-06-29 23:34:03 +08:00 · 11-S5 support / 10-S4D public GenericParam reflection carrier ·
  状态：11-S5 runtime view 的 public reflection consumer 子切片完成；完整 11-S5 仍未关闭，MethodSpec 标准化实例签名、
  与 08 实例化去重键的全链路统一、运行期 generic entity/layout materialization、public generic reflection
  object 和跨模块 generic binding 仍待后续。
  完成项目：`ZrCore_Reflection_ResolveGenericParameter()` 和
  `ZrCore_Reflection_ResolveGenericParameterConstraint()` 复用
  `ZrCore_MetadataRuntime_ReadGenericParamView()` /
  `ZrCore_MetadataRuntime_ReadGenericParamConstraintView()`，把 11-S5 的 GenericParam owner/name/flags/constraint
  range、constraint type token/record 与可选 signature blob 暴露到 public reflection carrier。反射层只复制 view
  内容，不新建泛型参数索引表。
  RED/GREEN：RED 为 `zr_vm_reflection_token_resolve_test` 引用缺失 GenericParam public carrier 类型/API 后
  WSL gcc 编译失败；GREEN 后 TypeDef owner 参数、MethodDef owner 参数、约束 type record、signature blob 与
  out-of-range constraint 负向路径通过。
  验证：WSL gcc/clang 均通过 `zr_vm_reflection_token_resolve_test` 3/0；WSL gcc 通过
  `zr_vm_metadata_runtime_query_test` 23/0 与 `zr_vm_metadata_runtime_typespec_layout_test` 14/0；Windows MSVC
  Debug 通过同三项 3/0、23/0、14/0。
  产出：`tests/acceptance/2026-06-29-aot-10-s4d-generic-parameter-reflection-carrier.md`。
  备注：本记录不声明 MethodSpec runtime instance binding、runtime generic layout construction、
  cross-module token publication/rewrite、full trim analyzer 或完整 11-S5 关闭。

- 2026-06-29 23:20:13 +08:00 · 11-S5 generic parameter runtime metadata views ·
  状态：11-S5 泛型参数/约束运行时只读 view 子切片完成；完整 11-S5 仍未关闭，MethodSpec 标准化实例签名、
  与 08 实例化去重键的全链路统一、运行期 generic entity/layout materialization、public generic reflection
  object 和跨模块 generic binding 仍待后续。
  完成项目：新增 `SZrMetadataRuntimeGenericParamView`、`SZrMetadataRuntimeGenericParamConstraintView`、
  `ZrCore_MetadataRuntime_ReadGenericParamView()` 与
  `ZrCore_MetadataRuntime_ReadGenericParamConstraintView()`；新增
  `metadata_runtime_generic_params.c`，从 attached zrp `GENERIC_PARAMS` /
  `GENERIC_PARAM_CONSTRAINTS` section 读取 TypeDef/MethodDef owner 的泛型参数定义，校验 row index
  落在 owner range 内，解析 owner/type token record，并对约束 signature blob 做可选 bounded slice +
  structural validation；null runtime、未 attach zrp、非 TypeDef/MethodDef owner、越界参数/约束和不匹配
  constraint owner 均返回 false。
  RED/GREEN：先在 `test_metadata_runtime_query.c` 增加泛型参数/约束 view 测试，RED 为缺失 view 类型和 API
  的编译失败；实现新头文件/API/实现文件后 GREEN，并补充 `nameStringOffset`、flags 与零长度约束 signature
  blob 断言。
  验证：WSL gcc/clang 均通过 `zr_vm_metadata_runtime_query_test` 23/0；Windows MSVC Debug 通过同一测试
  23/0；同一生产变更下 WSL gcc 与 Windows MSVC Debug 均通过
  `zr_vm_reflection_token_resolve_test` 2/0 和 `zr_vm_metadata_runtime_typespec_layout_test` 14/0。
  产出：`tests/acceptance/2026-06-29-aot-11-s5-generic-param-runtime-views.md`。
  备注：本记录不声明 MethodSpec 运行期实例绑定、泛型 layout 构建、public generic reflection object、
  full trim analyzer、cross-module token publication/rewrite 或完整 11-S5 关闭。

- 2026-06-29 14:21:20 +08:00 · 11-S4G support / 07-S3/S4 runtime CopyStack registry layout resolver ·
  状态：function-level layout resolver consumer 子切片完成；完整 11-S4 仍进行中，持久 cTypeId→token
  索引内容、完整 TypeSpec/generic layout materialization、runtime layout construction 和跨模块 token
  publication/rewrite 仍待后续。
  完成项目：`ZrLibrary_AotRuntime_CopyStack()` inline-struct stack-copy fallback 现在复用
  `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function, destinationLayout->typeLayoutId)`，
  从 attached code-registration layout registry 校验 generated-frame layout，不再回退到 function prototype
  layout cache。source contract 要求 `aot_runtime_values.c` include metadata runtime resolver 头并禁止旧
  `ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame->function`。
  RED/GREEN：source contract 新增 metadata-runtime include/resolver needle 后得到 RED；替换 runtime helper
  include 与 resolver 后 GREEN。
  验证：WSL gcc/clang 均通过 source contracts 22/0、call shared-library smoke 5/0、value-type shared-library
  smoke 5/0；Windows MSVC Debug 通过 source contracts 22/0、call smoke 5/0/5 ignored、value-type smoke
  5/0/1 ignored。
  产出：`tests/acceptance/2026-06-29-aot-07-s3-s4-runtime-copy-stack-registry-layout.md`。
  备注：字段 layout resolver、public reflection object materialization、runtime generic layout construction、
  cross-module token publication/rewrite 和完整 metadata sweep/pruning 仍待后续。

- 2026-06-29 14:11:34 +08:00 · 11-S4G support / 07-S3/S4 runtime inline-struct call/return registry layout resolver ·
  状态：function-level layout resolver consumer 子切片完成；完整 11-S4 仍进行中，持久 cTypeId→token
  索引内容、完整 TypeSpec/generic layout materialization、runtime layout construction 和跨模块 token
  publication/rewrite 仍待后续。
  完成项目：AOT runtime inline-struct call/return helpers 现在复用
  `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame->function, typeLayoutId)`：
  `CallInlineStruct()`、`CallInlineStructDynamicDeoptBridge()` 和 `ReturnInlineStruct()` 均从 attached
  code-registration layout registry 校验 generated-frame inline struct layout，不再回退到 function prototype
  layout cache。return contract 同步当前 `codeRegistration->functionPointers` thunk carrier ABI。
  RED/GREEN：return contract 先暴露既有 descriptor-thunk needle 漂移；修正为 codeRegistration ABI 后，
  新增 metadata-runtime resolver needle 得到 RED；改 runtime helper 后 GREEN，并禁止
  `ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame->function`。
  验证：WSL gcc/clang 均通过 return contracts 1/0、source contracts 22/0、value SemIR contracts 4/0、
  call shared-library smoke 5/0、value-type shared-library smoke 5/0；Windows MSVC Debug 通过 return
  contracts 1/0、source contracts 22/0、value SemIR contracts 4/0、call smoke 5/0/5 ignored、value-type
  smoke 5/0/1 ignored。
  产出：`tests/acceptance/2026-06-29-aot-07-s3-s4-runtime-inline-struct-call-return-registry-layout.md`。
  备注：字段 layout resolver、public reflection object materialization、runtime generic layout construction、
  cross-module token publication/rewrite 和完整 metadata sweep/pruning 仍待后续。

- 2026-06-29 13:57:57 +08:00 · 11-S4G support / 07-S3/S4 value SemIR registry layout resolver ·
  状态：function-level layout resolver consumer 子切片完成；完整 11-S4 仍进行中，持久 cTypeId→token
  索引内容、完整 TypeSpec/generic layout materialization、runtime layout construction 和跨模块 token
  publication/rewrite 仍待后续。
  完成项目：AOT C value SemIR inline `COPY_VALUE` 与 nested inline-struct field load/store 现在复用
  `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame.function, typeLayoutId)`。这把 inline copy、
  field-aware non-POD copy 与 cleanup/GC inline-frame consumer 接到同一 code-registration layout registry，
  不再让这些 generated value SemIR helper 回退到 function prototype layout cache。
  RED/GREEN：RED 为 source/value SemIR contracts 新增 metadata-runtime resolver needle 后，
  `zr_vm_aot_c_source_contracts_test` 仍缺少该 resolver；GREEN 后 source contracts 22/0、value SemIR
  contracts 4/0，并禁止 inline copy/field transfer 发射
  `ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function`。
  验证：WSL gcc/clang 均通过 source contracts 22/0、value SemIR contracts 4/0、value-type shared-library
  smoke 5/0；Windows MSVC Debug 通过 source contracts 22/0、value SemIR contracts 4/0、value-type smoke
  5/0/1 ignored。生成产物检查确认 value-type generated C 中这些 lookup 只使用 metadata-runtime resolver。
  产出：`tests/acceptance/2026-06-29-aot-07-s3-s4-value-semir-registry-layout.md`。
  备注：字段 layout 解析 `ZrCore_Function_ResolvePrototypeFrameFieldLayout(state, ...)` 未在本切片迁移；
  typed call/return resolver、public reflection object materialization、runtime generic layout construction、
  cross-module token publication/rewrite 和完整 metadata sweep/pruning 仍待后续。

- 2026-06-29 13:44:33 +08:00 · 11-S4G support / 07-S3/S4 frame cleanup registry layout resolver ·
  状态：function-level layout resolver consumer 子切片完成；完整 11-S4 仍进行中，持久 cTypeId→token
  索引内容、完整 TypeSpec/generic layout materialization、runtime layout construction 和跨模块 token
  publication/rewrite 仍待后续。
  完成项目：AOT C value-frame cleanup drop lookup 现在复用
  `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame.function, typeLayoutId)`，并由 generated C
  include `zr_vm_core/metadata_runtime.h` 提供 public resolver surface。这样 generated cleanup、GC
  inline-frame mark/rewrite、generic dictionary 与 GC descriptor consumer 都读 code-registration layout
  registry，而 cleanup 不再回退到 function prototype layout cache。
  RED/GREEN：RED 为 source contract 新增 generated metadata runtime include 与 cleanup resolver needle 后
  缺少 `#include \"zr_vm_core/metadata_runtime.h\"`；GREEN 后 source contract 22/0 并禁止 cleanup source
  发射 `ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function`。
  验证：WSL gcc/clang 均通过 source contracts 22/0、frame setup contracts 1/0、value-type shared-library
  smoke 5/0；Windows MSVC Debug 通过 source contracts 22/0、frame setup contracts 1/0、value-type smoke
  5/0/1 ignored。生成产物检查确认 cleanup blocks 使用 metadata-runtime resolver；其它 value SemIR helper
  中的 prototype resolver 保持未迁移。
  产出：`tests/acceptance/2026-06-29-aot-07-s3-s4-frame-cleanup-registry-layout.md`。
  备注：本记录不声明 runtime generic layout construction、public reflection entity、cross-module token
  publication/rewrite、完整 metadata sweep/pruning 或 07 frame-free closure 完成。

- 2026-06-29 13:22:39 +08:00 · 11-S4R-union generated union ownership offset table ·
  状态：11-S4 owner-field layout 子切片完成；完整 11-S4 仍进行中，持久 cTypeId→token 索引内容、
  完整 TypeSpec/generic layout materialization、runtime layout construction 和跨模块 token publication/rewrite
  仍待后续。
  完成项目：generated `SZrTypeLayout` descriptor 现在可为 union owner payload fields 生成
  `ZrOwnershipOffsets_<typeLayoutId>[]`，并让 `.ownershipFieldOffsets` 指向该表；zero-count 与 unsafe/
  unsupported offset 路径仍保持 `ZR_NULL` 和 failure marker。union active payload 语义继续由
  `SZrTypeLayoutField.activeTag` 与 tag metadata 表达，本切片只发布 owner payload byte offset table。
  RED/GREEN：RED 为 `zr_vm_aot_c_value_type_shared_library_smoke_test` 新增 `Shared<Box>` union payload
  fixture 后失败，缺少 generated `ZrOwnershipOffsets_`；GREEN 后 generated union descriptor 包含
  `/* zr_aot_ownership_offsets layout=... count=1 */`、`ZrOwnershipOffsets_<id>[]`、
  `.kind = 2u`、`.ownershipFieldCount = 1u` 和 `.ownershipFieldOffsets = ZrOwnershipOffsets_<id>`。
  验证：WSL gcc 通过 direct value-type smoke 5/0、source contracts 22/0 和
  `aot_c_type_layout_contracts` CTest 1/1；WSL clang 通过同组 5/0、22/0、1/1；Windows MSVC Debug
  通过 source contracts 22/0、value-type smoke 5/0/1 ignored 和同组 CTest 1/1。
  产出：`tests/acceptance/2026-06-29-aot-11-s4r-union-ownership-offset-table.md`。
  备注：本记录不声明 runtime generic layout construction、MethodSpec materialization、cross-module
  token-table policy、public reflection entity 或完整 trim analyzer 完成。

- 2026-06-29 13:03:17 +08:00 · 11-S4L/11-S4N support / 10-S4C FieldDef owner/type reflection consumer ·
  状态：FieldDef token/layout resolver consumer 子切片完成；完整 11-S4 仍进行中，持久 cTypeId→token
  索引内容、完整 TypeSpec/generic layout materialization、runtime layout construction 和跨模块 token
  publication/rewrite 仍待后续。
  完成项目：public reflection FieldDef carrier 现在消费现有 `ZrCore_MetadataRuntime_ResolveTypeLayoutToken()`：
  FieldDef token 解析在保留 owner token/row/layout 与 byte offset 的同时，按 field type layout id 反查
  TypeDef/TypeSpec token，并把 field type token/record 暴露给后续 public `FieldInfo` 物化。本切片未新增
  zrp section/row，也未改变 code-registration ABI。
  RED/GREEN：RED 为 `zr_vm_reflection_token_resolve_test` 要求 FieldDef owner type record/row 与 field
  type token/record 后编译失败；GREEN 后 synthetic owner TypeDef + field-type TypeDef + FieldDef 路径通过，
  TypeSpec generic argument 既有路径保持通过。
  验证：WSL gcc/clang 与 Windows MSVC Debug 均通过
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout`
  CTest 4/4。
  产出：`tests/acceptance/2026-06-29-aot-10-s4-fielddef-public-reflection-carrier.md`。
  备注：这是 11-S4 resolver 的 public reflection consumer，不声明跨模块 token 表、完整 metadata sweep
  或 public field reflection object 完成。

- 2026-06-29 12:51:49 +08:00 · 11-S3L/11-S4J support / 10-S4B public TypeSpec argument consumer ·
  状态：TypeSpec generic binding consumer 子切片完成；完整 11-S3/11-S4 仍进行中，recursive generic
  argument semantic binding、TypeSpec/generic runtime materialization、持久 cTypeId→token 索引和跨模块 token
  publication/rewrite 仍待后续。
  完成项目：public reflection carrier 现在消费既有 `ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView()`：
  TypeSpec `ResolveToken` 暴露 generic base token、signature token/hash 和 argument count；indexed
  `ZrCore_Reflection_ResolveTypeSpecGenericArgument(...)` 暴露 primitive signature argument 或 direct
  TypeRef/TypeDef argument token/record。本切片未新增 zrp section/row，也未改变 signature blob 编码。
  RED/GREEN：RED 为 reflection token resolver 测试引用缺失 TypeSpec generic carrier 字段和 indexed argument API
  后编译失败；GREEN 后 TypeSpec base/argument-count、primitive argument、TypeRef argument 和 out-of-range
  argument 负向路径通过。
  验证：WSL gcc/clang 与 Windows MSVC Debug 均通过
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout`
  CTest 4/4。
  产出：`tests/acceptance/2026-06-29-aot-10-s4-public-typespec-generic-argument-reflection.md`。
  备注：这是 metadata runtime 已有 TypeSpec generic binding view 的 public consumer，不声明完整 TypeSpec
  materialization 或跨模块 remap 完成。

- 2026-06-29 12:37:45 +08:00 · 11-S4 support / 10-S3A public reflection token consumer ·
  状态：11-S4 consumer bridge 子切片完成；完整 11-S4 仍进行中，持久 cTypeId→token 索引、
  完整 TypeSpec/generic layout materialization、运行期 layout 构建和跨模块 token publication/rewrite
  仍待后续。
  完成项目：public `ZrCore_Reflection_ResolveToken(...)` 现在作为 11-S4 binding view 的首个反射侧消费者：
  TypeDef/TypeSpec 解析走 registry-backed layout binding view，FieldDef 解析走 FieldDef row→owner/field
  layout binding view，并把 token record、row 指针、layout id、cTypeId、byte offset 与 layout 指针集中在
  `SZrReflectionResolvedToken`。本切片未新增 zrp 格式字段，也未改变现有 metadata row 编码。
  RED/GREEN：RED 为新增 reflection token resolver 测试引用缺失 public carrier/API 后编译失败；GREEN 后
  TypeDef/FieldDef/MethodDef 解析和 invalid input 清空路径通过。
  验证：WSL gcc/clang 与 Windows MSVC Debug 均通过
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout`
  CTest 4/4。
  产出：`tests/acceptance/2026-06-29-aot-10-s3-public-reflection-token-resolve.md`。
  备注：这是 metadata runtime 既有 binding view 的 public consumer，不声明新增 metadata format、cross-module
  remap 或完整 reflection entity materialization 完成。

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
