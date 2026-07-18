---
doc_type: plan-detail
plan_sources:
  - user: 2026-06-20 参照 hybridclr/mono/roslyn 完善反射
  - decision: 2026-06-20 反射默认最小 + 注解保留（对标 NativeAOT/illink）；分三级 None/RuntimeMapping/Description
references:
  - lua/hybridclr/libil2cpp/vm/Reflection.h          # invoker_method / 反射对象缓存
  - lua/hybridclr/libil2cpp/il2cpp-class-internals.h   # MethodInfo.invoker / Il2CppMethodPointer
  - lua/mono/mono/metadata/reflection.c               # 完整动态反射（备选，未采用为默认）
  - lua/runtime/src/coreclr/tools/aot/ILCompiler.Compiler/Compiler/AnalysisBasedMetadataManager.cs   # MetadataCategory 三态
  - lua/runtime/src/tools/illink/src/linker/Linker.Dataflow/FlowAnnotations.cs   # DynamicallyAccessedMembers
related_code:
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z28/11-S4BN FieldInfo nested primitive POD storage-width path matrix coverage
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z27/11-S4BM FieldInfo nested primitive POD representative path matrix coverage
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.c # 10-S4Z26/11-S4BL nested primitive raw child leaf layout identity guard
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z26/11-S4BL FieldInfo nested primitive raw child leaf layout guard coverage
  - zr_vm_core/include/zr_vm_core/reflection.h         # 10-S4Z25/11-S4BK FieldInfo nested inline primitive POD path read/write API
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z25/11-S4BK FieldInfo nested inline primitive POD path object/token adapter
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.c # 10-S4Z25/11-S4BK recursive nested inline primitive path traversal helper
  - zr_vm_core/src/zr_vm_core/reflection_field_value_primitive.c # 10-S4Z25/11-S4BK shared primitive POD raw load/store guard
  - zr_vm_core/src/zr_vm_core/reflection_field_value_primitive.h # 10-S4Z25/11-S4BK shared primitive POD raw load/store guard API
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z25/11-S4BK FieldInfo object-level nested inline primitive POD path coverage
  - zr_vm_core/include/zr_vm_core/reflection.h         # 10-S4Z24/11-S4BJ FieldInfo nested inline VALUE_SLOT path write API
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z24/11-S4BJ FieldInfo nested inline VALUE_SLOT path write adapter
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.c # 10-S4Z24/11-S4BJ recursive nested inline layout path write helper
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z24/11-S4BJ FieldInfo object-level nested inline VALUE_SLOT path write coverage
  - zr_vm_core/include/zr_vm_core/reflection.h         # 10-S4Z23/11-S4BI FieldInfo nested inline VALUE_SLOT path read API
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z23/11-S4BI FieldInfo nested inline VALUE_SLOT path read adapter
  - zr_vm_core/src/zr_vm_core/reflection_field_value_nested.c # 10-S4Z23/11-S4BI recursive nested inline layout traversal helper
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z23/11-S4BI FieldInfo object-level nested inline VALUE_SLOT path read coverage
  - zr_vm_core/include/zr_vm_core/reflection.h         # 10-S4Z22/11-S4BH FieldInfo nested inline VALUE_SLOT write API
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z22/11-S4BH FieldInfo nested inline VALUE_SLOT write path
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z22/11-S4BH FieldInfo object-level nested inline VALUE_SLOT write coverage
  - zr_vm_core/include/zr_vm_core/reflection.h         # 10-S4Z21/11-S4BG FieldInfo nested inline VALUE_SLOT read API
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z21/11-S4BG FieldInfo nested inline VALUE_SLOT read path
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z21/11-S4BG FieldInfo object-level nested inline VALUE_SLOT read coverage
  - zr_vm_core/src/zr_vm_core/type_layout.c            # 10-S4Z20/11-S4BF central VALUE_SLOT replacement/drop semantics consumed by FieldInfo inline aggregate write coverage
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z20/11-S4BF FieldInfo object-level inline aggregate replacement/drop borrowed-source write coverage
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z19/11-S4BE FieldInfo inline aggregate field-copy borrowed-source write
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z18/11-S4BD FieldInfo inline aggregate borrowed-source write copy
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z17/11-S4BC FieldInfo inline struct borrowed native-pointer view
  - zr_vm_core/include/zr_vm_core/reflection.h         # 10-S4Z16/11-S4BB FieldInfo object primitive POD adapter coverage
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z16/11-S4BB object-level FieldInfo primitive POD read/write delegates through token path
  - zr_vm_core/include/zr_vm_core/reflection.h         # 10-S4Z15/11-S4BA FieldInfo object value write API
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z15/11-S4BA FieldInfo object-level write adapter consumes FieldInfo.metadataRuntime + metadataToken
  - zr_vm_core/include/zr_vm_core/reflection.h         # 10-S4Z14/11-S4AZ FieldInfo object value read API
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z14/11-S4AZ FieldInfo object-level read adapter consumes FieldInfo.metadataRuntime + metadataToken
  - zr_vm_core/include/zr_vm_core/reflection.h         # TypeOfValue / BuildTypeLiteralObject / 成员反射; 10-S2B token-driven Method.Invoke dispatcher API; 10-S2C..10-S2H counted signature guard API; 10-S3B MethodSpec token method carrier; 10-S3C method signature identity carrier; 10-S3D/10-S3E method token public MethodInfo/function pointer/invoker carrier; 10-S4D/10-S4E generic parameter and MethodSpec generic argument public carriers; 10-S4Z43 public generic method definition object; 10-S4F FieldDef token -> FieldInfo object API; 10-S4Z4/11-S4AP FieldInfo value-slot read API; 10-S4Z5/11-S4AQ FieldInfo value-slot write API; 10-S4Z6/11-S4AR FieldInfo primitive POD read/write API; 10-S4Z7/11-S4AS primitive POD representative matrix coverage; 10-S4Z8/11-S4AT primitive POD full storage-width matrix coverage; 10-S4Z9/11-S4AU primitive POD integer range guard; 10-S4Z10/11-S4AV primitive POD float32 range guard; 10-S4Z11/11-S4AW primitive POD float32 NaN guard; 10-S4Z12/11-S4AX primitive POD float32 precision guard
  - zr_vm_core/src/zr_vm_core/reflection.c             # 缓存 + PIN; 10-S4A / 11-S4H registry-backed type/member layout consumer; 10-S4F minimal FieldDef token FieldInfo public object materialization; 10-S4G/11-S4U FieldInfo declaring type name/object link; 10-S4H/11-S4V FieldInfo owner object link; 10-S4I/11-S4W FieldInfo moduleName carrier; 10-S4J/11-S4X FieldInfo metadataFlags carrier; 10-S4K/11-S4Y FieldInfo signature blob coordinate carrier; 10-S4L/11-S4Z FieldInfo validated field signature header carrier; 10-S4M/11-S4AA FieldInfo field signature type-node summary carrier; 10-S4N/11-S4AB FieldInfo primitive signature type carrier; 10-S4O/11-S4AC FieldInfo primitive signature type object carrier; 10-S4P/11-S4AD FieldInfo module reflection object link; 10-S4Q/11-S4AE FieldInfo direct TypeDef signature token/layout carrier; 10-S4R/11-S4AF FieldInfo direct TypeDef signature type object carrier; 10-S4S/11-S4AG FieldInfo bound TypeRef signature carrier/type object; 10-S4T/11-S4AH FieldInfo signature/layout consistency carrier; 10-S4U/11-S4AI FieldInfo signature type-node object carrier; 10-S4V/11-S4AJ FieldInfo signature base type-node object carrier; 10-S4W/11-S4AK FieldInfo signature child type-node object list carrier; 10-S4X/11-S4AL FieldInfo primitive child type-node semantic name carrier; 10-S4Y/11-S4AM FieldInfo direct TypeDef child/base type-node semantic token/layout/name carrier; 10-S4Z2/11-S4AN FieldInfo direct TypeRef child type-node semantic token/layout/name carrier; 10-S4Z3/11-S4AO FieldInfo recursive signature type-node type literal carrier; 10-S4Z13/11-S4AY FieldInfo metadata runtime native-pointer carrier
  - zr_vm_core/src/zr_vm_core/reflection_field_value.c # 10-S4Z4..10-S4Z12/11-S4AP..11-S4AX FieldInfo token inline value boundary; VALUE_SLOT copy plus primitive POD raw scalar read/write with representative/full storage-width matrix coverage plus integer, float32 range, float32 NaN, and float32 precision guards
  - zr_vm_core/src/zr_vm_core/reflection_token_resolve.c # 10-S3A public token -> type/method/field resolver carrier; 10-S2B/10-S3F token-driven Method.Invoke dispatcher; 10-S2C/10-S3G signature arity guard; 10-S2D/10-S3H signature shape guard; 10-S2E/10-S3I fixed parameter base-type guard; 10-S2F/10-S3J return base-type guard; 10-S2G/10-S3K required return-slot reset guard; 10-S2H/10-S3L void return-slot canonicalization; 10-S3B MethodSpec token -> method carrier; 10-S3C MethodDef/MethodRef/MethodSpec signature identity carrier; 10-S3D MethodDef token MethodInfo/function pointer/invoker carrier; 10-S3E MethodSpec underlying-method binding carrier; 10-S4B TypeSpec generic base/argument carrier; 10-S4C FieldDef owner/type token carrier; 10-S4D GenericParam/constraint public carrier; 10-S4E MethodSpec generic argument public carrier
  - zr_vm_core/src/zr_vm_core/reflection_generic_type_object.c # 08-S6H..S6K/10-S4Z29..Z32 type objects; 08-S6P/10-S4Z37 MethodSpec context; 08-S6V/10-S4Z43 generic method definition object
  - zr_vm_core/src/zr_vm_core/reflection_interpreter_generic_instance.c # 08-S6L..S6U/10-S4Z33..Z42 reference/value instance + type/method call context + resolved/automatic VM execution
  - zr_vm_core/include/zr_vm_core/function.h           # 08-S6O/S6R/S6U and 10-S4Z36/Z39/Z42 type+method-context known-VM call plus flat graph lookup API
  - zr_vm_core/src/zr_vm_core/function_graph.c         # 10-S4Z42 AOT-compatible flat VM function graph lookup
  - zr_vm_core/src/zr_vm_core/object/object_call.c     # 08-S6O/S6R and 10-S4Z36/Z39 existing object-call pin/anchor/result path reuse
  - zr_vm_core/include/zr_vm_core/call_info.h          # 08-S6N/S6Q and 10-S4Z35/Z38 GC-visible type/method generic call-context carriers
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c             # 08-S6N/S6Q and 10-S4Z35/Z38 active call-context marking
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c            # 08-S6N/S6Q and 10-S4Z35/Z38 compacting-GC call-context forwarding rewrite
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h    # 11-S2B method token count mirror; 11-S2D method token -> MethodInfo/function pointer/invoker binding view; 11-S4I FieldDef token/row/offset/layout binding view for later token-driven field reflection entity materialization; 11-S4J TypeSpec layout binding view for later type-argument reflection; 11-S4K TypeDef/TypeSpec token -> layout cache resolver for future public token reflection lookup; 11-S4S same resolver accepts attached bound TypeRef token -> TypeDef layout; 11-S4L typeLayoutId -> token reverse resolver; 11-S4M bounded multi-entry cache; 11-S4N cTypeId -> token resolver; 11-S4O code-registration typeLayout token carrier count mirror; 11-S4P/11-S4Q generated TypeDef/TypeSpec-backed token population consumer path; 11-S4R registry-backed owner-field layout table consumer path; 11-S5/11-S5A GenericParam/GenericParamConstraint and owner-range runtime views, MethodSpec signature record carrier, and indexed MethodSpec generic argument view
  - zr_vm_core/src/zr_vm_core/metadata_runtime_generic_params.c # 11-S5A GenericParam owner-range view consumed by 10-S4Z43
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c # 11-S2D AOT method binding; 10-S4Z42/11-S2E local MethodDef.functionIndex -> VM binding
  - zr_vm_core/src/zr_vm_core/metadata_runtime_layout_binding.c # 11-S4I..11-S4O FieldDef/TypeSpec row binding validates row identity, resolves layouts through the AOT registry, exposes TypeDef/TypeSpec token -> layout cache lookup, 10-S5N/11-S4S resolves attached bound TypeRef token -> target TypeDef layout, provides typeLayoutId/cTypeId -> token reverse lookup, keeps bounded multi-entry cache hits, and consumes code-registration typeLayout token tables
  - zr_vm_core/include/zr_vm_core/object.h             # SZrMemberDescriptor / prototype
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h      # 10-S1A MethodInfo reflection level ABI; 10-S2A invoker ABI; 11-S2B method token table ABI carrier
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c # 12-S7Y default-min reflection metadata policy option helper; 12-S7ZU writer-level annotation warning suppression option helper
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_internal.h # 12-S7Y reflection metadata policy option API; 12-S7ZU annotation warning suppression option API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c # 10-S1A MethodInfo default; 10-S2A invoker emission dispatch; 10-S2I/10-S3M generated value.h include and function-table-fed reflection invoker emission callsite; 11-S2B method token table wiring; 12-S7Y metadata policy marker/plumbing; 10-S5A/12-S5A reflection annotation root collection + code_stripping.annotationRoot marker plumbing; 10-S5B/12-S5B annotation trim warning count/writer plumbing; 10-S5C/12-S5C annotation warning reason-text marker compatibility; 10-S5D/12-S5D dynamic dependency function roots continue through the same annotationRoot diagnostics; 10-S5J/12-S5I dynamicDependencyTypeLayoutId root marker/count plumbing; 10-S5K/12-S5J dynamicDependencyTypeToken metadata blob plumbing and typeLayout token table fallback; 10-S5M/12-S5L bound TypeRef dynamicDependencyTypeToken roots; 10-S5L/12-S5K dynamicDependencyFieldToken reuses type-layout root plumbing; 12-S7ZU annotation warning visible/suppressed count split and writer-level suppression plumbing
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_annotation_warnings.h # 10-S5B/12-S5B requires-unreferenced-code static-call trim annotation warning API; 10-S5C/12-S5C reason text remains behind same API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_annotation_warnings.c # 10-S5B/12-S5B retained static-call scanner for `requiresUnreferencedCode: true` callee metadata; 10-S5C/12-S5C `requiresUnreferencedCodeReason` string extraction and quoted marker output
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_callable_provenance.c # 10-S5B/12-S5B GET_SUB_FUNCTION callable-slot provenance for static-call annotation warnings
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.h # 10-S2A MethodInfo emitter API; 11-S2B method token table writer API; 12-S7Y policy-driven reflection level parameter
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c # 10-S2A shared entry-thunk MethodInfo binding; 11-S2B root exported function method token table emission; 12-S7Y MethodInfo NONE/RUNTIME_MAPPING emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_invokers.h # 10-S2-maint split reflection invoker emitter API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_invokers.c # 10-S2I/10-S3M generated i64 no-arg reflection return-boxing bucket; 10-S2J/10-S3N generated uint64 no-arg reflection return-boxing bucket; 10-S2K/10-S3O generated bool no-arg reflection return-boxing bucket; 10-S2L/10-S3P generated f64 no-arg reflection return-boxing bucket; 10-S2M/10-S3Q generated i64 one-arg reflection argument-unbox + return-boxing bucket; 10-S2N/10-S3R generated u64 one-arg reflection argument-unbox + return-boxing bucket; 10-S2O/10-S3S generated bool one-arg reflection argument-unbox + return-boxing bucket; 10-S2P/10-S3T generated f64 one-arg reflection argument-unbox + return-boxing bucket; 10-S2Q/10-S3U generated i64 two-arg reflection argument-unbox + return-boxing bucket; 10-S2R/10-S3V generated u64 two-arg reflection argument-unbox + return-boxing bucket; 10-S2S/10-S3W generated bool two-arg reflection argument-unbox + return-boxing bucket; 10-S2T/10-S3X generated f64 two-arg reflection argument-unbox + return-boxing bucket; 10-S2U/10-S3Y bool-return numeric comparison invoker orchestration; 10-S2V/10-S3Z i64 three-arg invoker orchestration; 10-S2W/10-S3AA u64 three-arg invoker orchestration; 10-S2X/10-S3AB f64 three-arg invoker orchestration; 10-S2Y/10-S3AC bool three-arg invoker orchestration
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_bool_numeric_invokers.h # 10-S2U/10-S3Y bool-return numeric two-arg reflection invoker emitter API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_bool_numeric_invokers.c # 10-S2U/10-S3Y generated bool(int,int), bool(uint,uint), and bool(float,float) comparison reflection argument-unbox + return-boxing buckets
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_numeric_three_arg_invokers.h # 10-S2V/10-S3Z, 10-S2W/10-S3AA, and 10-S2X/10-S3AB numeric three-arg reflection invoker emitter API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_numeric_three_arg_invokers.c # 10-S2V/10-S3Z generated int64(int64,int64,int64), 10-S2W/10-S3AA generated uint64(uint64,uint64,uint64), and 10-S2X/10-S3AB generated float(float,float,float) reflection argument-unbox + return-boxing buckets
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_bool_three_arg_invokers.h # 10-S2Y/10-S3AC bool three-arg reflection invoker emitter API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_bool_three_arg_invokers.c # 10-S2Y/10-S3AC generated bool(bool,bool,bool) reflection argument-unbox + return-boxing bucket
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_bool_thunks.h # 10-S2K/10-S3O typed bool no-arg, 10-S2O/10-S3S typed bool one-arg, 10-S2S/10-S3W typed bool two-arg, and 10-S2U/10-S3Y bool-return numeric comparison two-arg thunk eligibility predicates exposed to reflection invoker emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_bool_three_arg_thunks.h # 10-S2Y/10-S3AC typed bool three-arg thunk eligibility predicate exposed to reflection invoker emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_bool_three_arg_thunks.c # 10-S2Y/10-S3AC current short-circuit bool three-arg cleanup-reset shape recognized for typed thunk/reflection emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_f64_thunks.h # 10-S2L/10-S3P typed f64 no-arg, 10-S2P/10-S3T typed f64 one-arg, 10-S2T/10-S3X typed f64 two-arg/state-free, and 10-S2X/10-S3AB typed f64 three-arg/state-free thunk eligibility predicates exposed to reflection invoker emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_i64_thunks.h # 10-S2I/10-S3M typed i64 no-arg, 10-S2M/10-S3Q typed i64 one-arg, 10-S2Q/10-S3U typed i64 two-arg/state-free, and 10-S2V/10-S3Z typed i64 three-arg/state-free thunk eligibility predicates exposed to reflection invoker emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_thunks.h # 10-S2J/10-S3N typed u64 no-arg, 10-S2N/10-S3R typed u64 one-arg, and 10-S2R/10-S3V typed u64 two-arg/state-free thunk eligibility predicates exposed to reflection invoker emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_three_arg_thunks.h # 10-S2W/10-S3AA typed u64 three-arg/state-free thunk eligibility predicates exposed to reflection invoker emission
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_u64_three_arg_thunks.c # 10-S2W/10-S3AA typed u64 three-arg forward declaration/definition support consumed by generated reflection invoker buckets
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c # 11-S4R generated ownership offset table emission for struct/union owner-field layouts; 10-S5J/12-S5I dynamicDependencyTypeLayoutId root-aware type-layout retention; 10-S5L/12-S5K annotation metadata roots orchestration after helper split
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_metadata_roots.c # 10-S5K/12-S5J dynamicDependencyTypeToken TypeDef/TypeSpec token -> typeLayout root resolution; 10-S5M/12-S5L bound TypeRef token-record -> target TypeDef typeLayout root resolution; 10-S5L/12-S5K dynamicDependencyFieldToken FieldDef -> owner/field typeLayout roots
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_tokens.c # 11-S4Q generated TypeSpec-backed token table population for future type-argument reflection; 10-S5K/12-S5J root-only TypeDef/TypeSpec token fallback for dynamic dependency type roots
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.h # 10-S5A/12-S5A reflection annotation reachability reason; 10-S5D/12-S5D dynamic dependency roots reuse this reason
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.h # 10-S5A/12-S5A reflection annotation root API; 10-S5D/12-S5D dynamic dependency function root collection API
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability_function_graph.c # 10-S5A/12-S5A `reflectable: true` decorator metadata root scan; 10-S5D/12-S5D `dynamicDependencyFunctionIndex` metadata root scan; 10-S5F/12-S5E `dynamicDependencyMethodToken` MEMBER_DEF token root scan through root typed exported symbols; 10-S5G/12-S5F `dynamicDependencyMethodName` exported method-name root scan; 10-S5H/12-S5G optional `dynamicDependencyMethodSignatureHash` signature disambiguation and ambiguous-name rejection; 10-S5I/12-S5H current-module non-exported `dynamicDependencyMethodToken` function-symbol root scan without requiring exported `exportKind`
  - tests/parser/test_aot_c_frame_setup_contracts.c     # 10-S1A/10-S2A source contract; 10-S2-maint split reflection invoker source contract; 10-S2I/10-S3M generated i64 no-arg reflection return-boxing source contract; 10-S2J/10-S3N generated u64 no-arg reflection return-boxing source contract; 10-S2K/10-S3O generated bool no-arg reflection return-boxing source contract; 10-S2L/10-S3P generated f64 no-arg reflection return-boxing source contract; 10-S2M/10-S3Q generated i64 one-arg argument-unbox + return-boxing source contract; 10-S2N/10-S3R generated u64 one-arg argument-unbox + return-boxing source contract; 10-S2O/10-S3S generated bool one-arg argument-unbox + return-boxing source contract; 10-S2P/10-S3T generated f64 one-arg argument-unbox + return-boxing source contract; 10-S2Q/10-S3U generated i64 two-arg argument-unbox + return-boxing source contract; 10-S2R/10-S3V generated u64 two-arg argument-unbox + return-boxing source contract; 10-S2S/10-S3W generated bool two-arg argument-unbox + return-boxing source contract; 10-S2T/10-S3X generated f64 two-arg argument-unbox + return-boxing source contract; 10-S2U/10-S3Y generated bool-return numeric comparison two-arg source contract; 10-S2V/10-S3Z generated i64 three-arg source contract; 10-S2W/10-S3AA generated u64 three-arg source contract; 10-S2X/10-S3AB generated f64 three-arg source contract; 10-S2Y/10-S3AC generated bool three-arg source contract; 11-S2B method token ABI/emitter/runtime source contract; 12-S7Y policy-driven MethodInfo emitter source contract
  - tests/parser/test_aot_c_shared_library_smoke.c      # 10-S1A/10-S2A runtime descriptor assertion; 10-S2I/10-S3M generated answer(): int reflection return-boxing runtime assertion; 10-S2J/10-S3N generated unsigned_answer(): uint reflection return-boxing runtime assertion; 10-S2K/10-S3O generated truth(): bool reflection return-boxing runtime assertion; 10-S2L/10-S3P generated ratio(): float reflection return-boxing runtime assertion; 10-S2M/10-S3Q generated echo(value: int): int argument-unbox + return-boxing runtime assertion; 10-S2N/10-S3R generated echo_unsigned(value: uint): uint argument-unbox + return-boxing runtime assertion; 10-S2O/10-S3S generated echo_truth(value: bool): bool argument-unbox + return-boxing runtime assertion; 10-S2P/10-S3T generated echo_ratio(value: float): float argument-unbox + return-boxing runtime assertion; 10-S2Q/10-S3U generated sum_values(left: int, right: int): int two-arg argument-unbox + return-boxing runtime assertion; 10-S2R/10-S3V generated sum_unsigned(left: uint, right: uint): uint two-arg argument-unbox + return-boxing runtime assertion; 10-S2S/10-S3W generated same_truth(left: bool, right: bool): bool two-arg argument-unbox + return-boxing runtime assertion; 10-S2T/10-S3X generated sum_ratio(left: float, right: float): float two-arg argument-unbox + return-boxing runtime assertion; 10-S2U/10-S3Y generated less_values(left: int, right: int), unsigned_after(left: uint, right: uint), and ratio_equal(left: float, right: float) bool-return numeric comparison runtime assertions; 10-S2V/10-S3Z generated sum_three(left: int, middle: int, right: int): int three-arg runtime assertion; 10-S2W/10-S3AA generated sum_three_unsigned(left: uint, middle: uint, right: uint): uint three-arg runtime assertion; 10-S2X/10-S3AB generated sum_three_ratio(left: float, middle: float, right: float): float three-arg runtime assertion; 10-S2Y/10-S3AC generated all_truth(left: bool, middle: bool, right: bool): bool three-arg runtime assertion; 11-S2B runtime method token descriptor assertion; non-stripped RUNTIME_MAPPING guard
  - tests/parser/test_aot_c_code_stripping.c            # 12-S7Y opt-in stripping lowers generated MethodInfo reflection level to NONE; 10-S5J/12-S5I dynamicDependencyTypeLayoutId generated-C type-layout retention; 10-S5K/12-S5J dynamicDependencyTypeToken TypeDef/TypeSpec generated-C type-layout/token retention; 10-S5M/12-S5L bound TypeRef token generated-C type-layout/token retention; 10-S5L/12-S5K dynamicDependencyFieldToken owner/field type-layout retention
  - tests/parser/test_aot_c_reflection_annotation_preserve.c # 10-S5A/12-S5A reflectable decorator metadata preserves otherwise unreachable function root under code stripping; 10-S5B/12-S5B requiresUnreferencedCode static-call trim annotation warning coverage; 10-S5C/12-S5C reason text quoting coverage; 10-S5D/12-S5D dynamicDependencyFunctionIndex function root coverage; 10-S5F/12-S5E dynamicDependencyMethodToken generated-C retention coverage; 10-S5G/12-S5F dynamicDependencyMethodName generated-C retention coverage; 10-S5H/12-S5G dynamicDependencyMethodName + signatureHash generated-C retention coverage; 10-S5I/12-S5H non-exported dynamicDependencyMethodToken generated-C retention coverage; 12-S7ZU suppressAnnotationWarnings keeps annotation warnings counted as suppressed without emitting per-warning entries
  - tests/parser/test_aot_reachability.c                    # 10-S5D/12-S5D direct dynamicDependencyFunctionIndex annotation-root collection coverage; 10-S5F/12-S5E direct dynamicDependencyMethodToken annotation-root collection coverage; 10-S5G/12-S5F direct dynamicDependencyMethodName annotation-root collection coverage; 10-S5H/12-S5G direct signature-hash disambiguation and ambiguous-name rejection coverage; 10-S5I/12-S5H direct non-exported dynamicDependencyMethodToken function-symbol coverage
  - tests/parser/test_aot_c_source_contracts.c          # 11-S2B public method token ABI/emitter source contract; 12-S7Y metadata policy plumbing source contract; 12-S7ZU suppressAnnotationWarnings public writer option/source contract; 10-S5F/12-S5E, 10-S5G/12-S5F, 10-S5H/12-S5G, and 10-S5I/12-S5H dynamic dependency method token/name/signature/non-exported-token source contract; 10-S5J/12-S5I dynamicDependencyTypeLayoutId, 10-S5K/12-S5J and 10-S5M/12-S5L dynamicDependencyTypeToken, and 10-S5L/12-S5K dynamicDependencyFieldToken source contract
  - tests/parser/test_aot_c_descriptor_diagnostics.c    # 10-S1A/10-S2A ABI diagnostic version drift guard
  - tests/module/test_metadata_runtime_type_layout.c    # 10-S4A reflection layout source contract and 11-S4H prototype layout resolver coverage
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z19/11-S4BE FieldInfo object-level inline aggregate field-copy borrowed-source write coverage
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z18/11-S4BD FieldInfo object-level inline aggregate borrowed-source write coverage
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z17/11-S4BC FieldInfo object-level inline struct borrowed native-pointer view coverage
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z16/11-S4BB FieldInfo object-level primitive POD raw inline read/write coverage
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z15/11-S4BA FieldInfo object-level VALUE_SLOT write adapter coverage
  - tests/module/test_reflection_token_resolve.c        # 10-S4Z14/11-S4AZ FieldInfo object-level VALUE_SLOT read adapter coverage
  - tests/module/test_reflection_token_resolve.c        # 10-S3A public ResolveToken carrier coverage; 10-S2B token-driven Method.Invoke dispatcher coverage; 10-S2C counted signature arity guard coverage; 10-S2E arity fixture runtime argument type setup; 10-S2G invoker-written return fixture setup; 10-S3B MethodSpec ResolveToken coverage; 10-S3C method signature identity carrier coverage; 10-S3D/10-S3E method binding carrier coverage; 10-S4B TypeSpec generic argument coverage; 10-S4C FieldDef owner/type token carrier coverage; 10-S4D GenericParam/constraint carrier coverage; 10-S4E MethodSpec generic argument carrier coverage; 10-S4F..10-S4Z13 FieldDef token FieldInfo object, declaring type, owner link, moduleName, raw metadata flags, raw signature blob coordinates, validated field signature header, field signature type-node summary, primitive signature type carrier, primitive signature type object carrier, module reflection object link, direct TypeDef signature token/layout/type object carrier, bound TypeRef signature token/layout/type object, signature/layout consistency, signature type-node object, signature base type-node object, signature child type-node object list, primitive child type-node semantic name, direct TypeDef base/child type-node semantic token/layout/name, direct TypeRef child type-node semantic token/layout/name, recursive signature type-node type literal, FieldDef token metadataRuntime native-pointer carrier, FieldDef token VALUE_SLOT inline read/write, FieldDef token primitive POD int32 raw inline read/write, FieldDef token primitive POD bool/uint32/double representative raw inline read/write matrix, FieldDef token primitive POD int8/int16/int64/uint8/uint16/uint64/float32 storage-width raw inline read/write matrix, FieldDef token primitive POD integer range guard, FieldDef token primitive POD float32 range guard, FieldDef token primitive POD float32 NaN guard, and FieldDef token primitive POD float32 precision guard coverage
  - tests/module/test_reflection_dynamic_generic_instance.c # 08-S6B..S6M/10-S4Z29..Z34 dynamic generic route/object/interpreter context coverage
  - tests/module/test_reflection_dynamic_generic_instance_interpreter.h # 08-S6L..S6S/10-S4Z33..Z40 reference/value instance, context, substitution, copy, and execution scenarios
  - tests/module/test_reflection_dynamic_generic_method_context.h # 08-S6P..S6U/10-S4Z37..Z42 MethodSpec context/call-info/GC/resolved+automatic VM execution scenarios
  - tests/module/test_reflection_method_invoke.c        # 10-S2D/10-S3H token-driven Method.Invoke signature shape guard coverage; 10-S2E/10-S3I fixed parameter base-type guard coverage; 10-S2F/10-S3J return base-type guard coverage; 10-S2G/10-S3K required return-slot reset coverage; 10-S2H/10-S3L void return-slot canonicalization coverage
  - tests/module/test_metadata_runtime_query.c          # 11-S2B methodTokenCount attach mirror coverage; 11-S4I FieldDef layout binding view coverage; 11-S5 GenericParam/GenericParamConstraint runtime view, MethodSpec signature record carrier, and MethodSpec generic argument view coverage
  - tests/module/test_metadata_runtime_method_binding.c # 11-S2D method token -> MethodInfo/function pointer/invoker binding view coverage
  - tests/module/test_metadata_runtime_typespec_layout.c # 11-S4J TypeSpec layout binding view coverage; 11-S4K TypeDef/TypeSpec token -> layout cache coverage; 10-S5N/11-S4S attached bound TypeRef token -> TypeDef layout resolver/cache/identity mismatch coverage; 11-S4L typeLayoutId -> token reverse lookup coverage; 11-S4M multi-entry cache coverage; 11-S4N cTypeId -> token coverage; 11-S4O/11-S4P code-registration token table coverage
  - tests/parser/test_aot_c_generic_call_typed.c        # 11-S4Q generated TypeSpec token table coverage for generic layouts
  - tests/parser/test_aot_c_value_type_shared_library_smoke.c # 11-S4R generated struct/union ownership-offset table coverage
---

# 10 · 反射（分级元数据 + invoker thunk + 注解保留）

> 承接缺口：zr_vm 已有 `ZrCore_Reflection_TypeOfValue`、`BuildTypeLiteralObject`（按名查类型）、
> 成员查询（`SZrMemberDescriptor` + `FindMemberDescriptor`，含继承）、协议检查、反射对象缓存与 PIN。
> **缺**：按 token 反射、泛型参数反射、字段 offset 反射、统一动态调用入口（invoker）、
> 反射与裁剪的隔离。本文按既定决策（**默认最小 + 注解保留**）补齐，并与 `11`/`12` 联动。

## 0. 核心决策：反射三级（对标 NativeAOT MetadataCategory）

每个类型/方法/字段的反射能力分三级，由裁剪（`12`）可达性 + 注解决定，**默认按可达性取最小**：

| 级别 | 含义 | 产出 | 默认 |
|------|------|------|------|
| `NONE` | 无反射元数据 | 不可被反射发现 | 未被反射可达的实体 |
| `RUNTIME_MAPPING` | 可动态调用 / 查表，但不可完整枚举 | invoker thunk + token↔实体映射 | 被间接调用/泛型字典引用的实体 |
| `DESCRIPTION` | 完整签名，可枚举成员 | 完整元数据（参数名/类型/特性/offset） | 被注解或 manifest 显式要求 |

> 默认产物最小：未被注解、未被反射可达的类型只生成执行所需的 layout/函数，**不**带反射元数据。
> 这是「极致性能 + 小体积」与「按需反射」的平衡（对标 NativeAOT，反 il2cpp/mono 的默认全保留）。

## 1. 统一动态调用入口（invoker thunk，对标 il2cpp invoker_method）

反射调用（`Method.Invoke` 等价）需要把「`SZrValue[]` 参数 → 调具体 typed C 函数 → 打包返回」。
**这正是 `07`§6 的边界 marshaling**，复用而非新建：

```c
/* 每个 RUNTIME_MAPPING+ 的函数登记一个 invoker，签名统一 */
typedef void (*FZrAotReflectionInvoker)(struct SZrState *state,
                                        FZrAotEntryThunk target,
                                        const struct SZrAotMethodInfo *method,
                                        struct SZrTypeValue *self,        /* 实例方法的接收者，静态则 NULL */
                                        struct SZrTypeValue *args,        /* 入参数组（dynamic 表示） */
                                        struct SZrTypeValue *outReturn);  /* 返回打包目标 */
```

- invoker 内部：按 `method->signature` 把 `args[i]` unbox 成 typed 寄存器（`07`§6 入参解包）→
  调 `target` 的真实 C 函数 → 把返回寄存器 box 回 `outReturn`（返回打包）。
- invoker 按**签名分桶生成**（相同 C 签名共享一个 invoker，对标 il2cpp 按签名生成 invoker），
  避免每方法一份导致膨胀。
- 入口表登记到代码注册表（`11`§2），token/反射对象据此找到 invoker（对标 il2cpp `invokerPointers[]`）。
- 10-S2A 先按当前 AOT C 事实落地第一类签名桶：generated 函数仍统一暴露为
  `FZrAotEntryThunk(SZrState *)`，因此 `SZrAotMethodInfo.invoker` 先登记共享
  `zr_aot_invoker_entry_thunk`。它是后续 token/反射调用 API 可消费的 ABI carrier，不声明完整
  参数解包/返回打包已经完成。
- 10-S2C / 10-S3G 已给 token-driven `Invoke` 增加 counted entry：
  `ZrCore_Reflection_InvokeMethodTokenWithArgCount(...)` 会在调用 invoker 前读取
  `methodInfo->signature->parameterCount/hasVarArgs`，拒绝参数数量不足/过多和非空参数计数下的 null
  `args`。这只是 signature arity guard，不声明类型 unbox、返回 box 或完整签名类型校验完成。
- 10-S2D / 10-S3H 继续收紧 counted entry 的 signature shape：当 `parameterCount > 0` 时要求
  `methodInfo->signature->parameterTypes` 非空；当 `hasReturnValue` 为 true 时要求 `returnType` 非空。
  该 guard 只证明 invoker 前的签名描述形状完整，不执行 `SZrTypeValue` 到 typed 寄存器的类型匹配/unbox。
- 10-S2E / 10-S3I 在 counted entry 中继续增加 fixed 参数基础类型防线：对每个 declared fixed parameter，
  若 `parameterTypes[i].baseType` 是具体非 null/unknown `EZrValueType`，则要求 `args[i].type` 精确匹配；
  varargs 的额外参数、untyped/null/unknown signature slot、数值转换和真正 unbox/box 仍留给后续 invoker marshaling。
- 10-S2F / 10-S3J 在 counted entry 的 registered invoker 调用后，消费 `returnType->baseType` 做返回值基础类型
  post-dispatch guard：有 concrete 非 null/unknown return baseType 时要求 `outReturn->type` 匹配；这不是返回 box，
  也不把 invoker 副作用变成预检。
- 10-S2G / 10-S3K 在 counted entry 中、dispatch 前把 required `outReturn` 清为 null，
  使返回 post-guard 只能接受 invoker 本次写出的值；这关闭旧返回槽误通过，但仍不实现返回 box/unbox。
- 10-S2H / 10-S3L 在 counted entry 中把无返回值 signature 的最终 `outReturn` 规范为 null：
  即使 registered invoker 误写返回槽，void/无返回方法对外仍只暴露 null。该路径仍不实现返回 box/unbox。
- 10-S2I / 10-S3M 先落地 generated reflection invoker 的第一类返回 box：生成 C 中的
  `zr_aot_try_invoke_i64_no_arg(...)` 会检查 MethodInfo signature 的 return/base type/parameterCount 与
  `outReturn`，再按 `method->functionIndex` 调用已有 `zr_aot_typed_i64_fn_<index>()`，并用
  `ZrCore_Value_InitAsInt(...)` 打包 int64 返回值。unsupported case 仍回落到完整 `FZrAotEntryThunk`
  执行 thunk，且不把该 thunk 的返回值解释为业务返回值，因为完整执行 thunk 返回的是执行成功标志。
  该切片不声明 args unbox、bool/u64/f64/object/inline 返回、数值 widening、完整签名桶或 public `MethodInfo`
  对象完成。
- 10-S2J / 10-S3N 在同一 generated reflection invoker 中追加 uint64/no-arg 返回 box：
  `zr_aot_try_invoke_u64_no_arg(...)` 复用 MethodInfo signature return/base type、`parameterCount == 0`
  与 `outReturn` guard，按 `method->functionIndex` 调用已有 `zr_aot_typed_u64_fn_<index>()`，并用
  `ZrCore_Value_InitAsUInt(...)` 写出 boxed uint64。unsupported case 仍回落完整 execution thunk，不把
  execution thunk 返回值解释为业务返回。该切片不声明参数 unbox、bool/f64/object/inline 返回、numeric widening、
  完整签名桶、typed target ABI carrier 或 public `MethodInfo` 对象完成。
- 10-S2K / 10-S3O 在同一 generated reflection invoker 中继续追加 bool/no-arg 返回 box：
  `zr_aot_try_invoke_bool_no_arg(...)` 复用 MethodInfo signature return/base type、`parameterCount == 0`
  与 `outReturn` guard，按 `method->functionIndex` 调用已有 `zr_aot_typed_bool_fn_<index>()`，并用
  `ZrCore_Value_InitAsBool(...)` 写出 boxed bool。unsupported case 仍回落完整 execution thunk，不把
  execution thunk 返回值解释为业务返回。该切片不声明参数 unbox、f64/object/inline 返回、numeric widening、
  完整签名桶、typed target ABI carrier 或 public `MethodInfo` 对象完成。
- 10-S2L / 10-S3P 在同一 generated reflection invoker 中继续追加 f64/no-arg 返回 box：
  `zr_aot_try_invoke_f64_no_arg(...)` 复用 MethodInfo signature return/base type、`parameterCount == 0`
  与 `outReturn` guard，按 `method->functionIndex` 调用已有 `zr_aot_typed_f64_fn_<index>()`，并用
  `ZrCore_Value_InitAsFloat(...)` 写出 boxed double。unsupported case 仍回落完整 execution thunk，不把
  execution thunk 返回值解释为业务返回。该切片不声明参数 unbox、object/inline 返回、numeric widening、
  完整签名桶、typed target ABI carrier 或 public `MethodInfo` 对象完成。
- 10-S2M / 10-S3Q 在同一 generated reflection invoker 中开启第一类参数解包桶：`int64(int64)`。
  `zr_aot_try_invoke_i64_one_arg(...)` 要求 MethodInfo signature 为 int64 返回、单个 int64 参数、
  `args[0].type == ZR_VALUE_TYPE_INT64` 且 `outReturn` 非空；命中 `functionIndex` 后从
  `args[0].value.nativeObject.nativeInt64` 解出 `TZrInt64`，调用已有
  `zr_aot_typed_i64_fn_<index>(zr_aot_arg0)`，并用 `ZrCore_Value_InitAsInt(...)` 写回 boxed int64。
  该切片只关闭一参数 i64 桶，不声明多参数、其他标量、object/inline、numeric widening、实例 receiver 或
  public `MethodInfo` 对象完成。
- 10-S2N / 10-S3R 在 generated reflection invoker 中补齐相邻 unsigned 参数解包桶：`uint64(uint64)`。
  `zr_aot_try_invoke_u64_one_arg(...)` 要求 MethodInfo signature 为 uint64 返回、单个 uint64 参数、
  `args[0].type == ZR_VALUE_TYPE_UINT64` 且 `outReturn` 非空；命中 `functionIndex` 后从
  `args[0].value.nativeObject.nativeUInt64` 解出 `TZrUInt64`，调用已有
  `zr_aot_typed_u64_fn_<index>(zr_aot_arg0)`，并用 `ZrCore_Value_InitAsUInt(...)` 写回 boxed uint64。
  该切片只关闭一参数 u64 桶，不声明多参数、其他标量参数、object/inline、numeric widening、实例 receiver 或
  public `MethodInfo` 对象完成。
- 10-S2O / 10-S3S 在拆分后的 generated reflection invoker 模块中补齐 bool 参数解包桶：`bool(bool)`。
  `zr_aot_try_invoke_bool_one_arg(...)` 要求 MethodInfo signature 为 bool 返回、单个 bool 参数、
  `args[0].type == ZR_VALUE_TYPE_BOOL` 且 `outReturn` 非空；命中 `functionIndex` 后从
  `args[0].value.nativeObject.nativeBool` 解出 `TZrBool`，调用已有
  `zr_aot_typed_bool_fn_<index>(zr_aot_arg0)`，并用 `ZrCore_Value_InitAsBool(...)` 写回 boxed bool。
  该切片只关闭一参数 bool 桶，不声明多参数、其他标量参数、object/inline、numeric widening、实例 receiver 或
  public `MethodInfo` 对象完成。
- 10-S2P / 10-S3T 在 generated reflection invoker 中补齐 f64 参数解包桶：`float(float)`。
  `zr_aot_try_invoke_f64_one_arg(...)` 要求 MethodInfo signature 为 double/float 返回、单个 double/float 参数、
  `args[0].type == ZR_VALUE_TYPE_DOUBLE` 且 `outReturn` 非空；命中 `functionIndex` 后从
  `args[0].value.nativeObject.nativeDouble` 解出 `TZrFloat64`，调用已有
  `zr_aot_typed_f64_fn_<index>(zr_aot_arg0)`，并用 `ZrCore_Value_InitAsFloat(...)` 写回 boxed double。
  该切片只关闭一参数 f64 桶，不声明多参数、其他标量参数、object/inline、numeric widening、实例 receiver 或
  public `MethodInfo` 对象完成。
- 10-S2Q / 10-S3U 在 generated reflection invoker 中开启第一类二参数解包桶：`int64(int64, int64)`。
  `zr_aot_try_invoke_i64_two_arg(...)` 要求 MethodInfo signature 为 int64 返回、两个 int64 参数、
  `parameterTypes[0/1].baseType == ZR_VALUE_TYPE_INT64`、`args[0/1].type == ZR_VALUE_TYPE_INT64` 且
  `outReturn` 非空；命中 `functionIndex` 后从两个 `args` slot 解出 `TZrInt64`。state-free typed helper
  直接调用 `zr_aot_typed_i64_fn_<index>(zr_aot_arg0, zr_aot_arg1)`；除法/取模等仍需要 `state` 的 i64 二参
  helper 调用 `zr_aot_typed_i64_fn_<index>(state, zr_aot_arg0, zr_aot_arg1)`，保留 divide/modulo-by-zero
  诊断路径；返回值统一用 `ZrCore_Value_InitAsInt(...)` 写回 boxed int64。该切片只关闭首个 i64 二参数桶，
  不声明 u64/bool/f64 二参数桶、三参数及以上、object/inline、numeric widening、实例 receiver 或
  public `MethodInfo` 对象完成。
- 10-S2R / 10-S3V 在 generated reflection invoker 中补齐相邻 unsigned 二参数桶：`uint64(uint64, uint64)`。
  `zr_aot_try_invoke_u64_two_arg(...)` 要求 MethodInfo signature 为 uint64 返回、两个 uint64 参数、
  `parameterTypes[0/1].baseType == ZR_VALUE_TYPE_UINT64`、`args[0/1].type == ZR_VALUE_TYPE_UINT64` 且
  `outReturn` 非空；命中 `functionIndex` 后从两个 `args` slot 解出 `TZrUInt64`。state-free typed helper
  直接调用 `zr_aot_typed_u64_fn_<index>(zr_aot_arg0, zr_aot_arg1)`；除法/取模等仍需要 `state` 的 u64 二参
  helper 调用 `zr_aot_typed_u64_fn_<index>(state, zr_aot_arg0, zr_aot_arg1)`，保留 divide/modulo-by-zero
  诊断路径；返回值统一用 `ZrCore_Value_InitAsUInt(...)` 写回 boxed uint64。该切片只关闭 u64 二参数桶，
  不声明 bool/f64 二参数桶、三参数及以上、object/inline、numeric widening、实例 receiver 或
  public `MethodInfo` 对象完成。
- 10-S2S / 10-S3W 在 generated reflection invoker 中补齐 bool 二参数桶：`bool(bool, bool)`。
  `zr_aot_try_invoke_bool_two_arg(...)` 要求 MethodInfo signature 为 bool 返回、两个 bool 参数、
  `parameterTypes[0/1].baseType == ZR_VALUE_TYPE_BOOL`、`args[0/1].type == ZR_VALUE_TYPE_BOOL` 且
  `outReturn` 非空；命中 `functionIndex` 后从两个 `args` slot 解出 `TZrBool`，调用已有
  `zr_aot_typed_bool_fn_<index>(zr_aot_arg0, zr_aot_arg1)`，并用 `ZrCore_Value_InitAsBool(...)` 写回 boxed bool。
  该切片只关闭 bool(bool,bool) 二参数桶，不声明 bool-return numeric comparison buckets、f64 二参数桶、
  三参数及以上、object/inline、numeric widening、实例 receiver 或 public `MethodInfo` 对象完成。
- 10-S2T / 10-S3X 在 generated reflection invoker 中补齐 f64 二参数桶：`float(float, float)`。
  `zr_aot_try_invoke_f64_two_arg(...)` 要求 MethodInfo signature 为 double/float 返回、两个 double/float 参数、
  `parameterTypes[0/1].baseType == ZR_VALUE_TYPE_DOUBLE`、`args[0/1].type == ZR_VALUE_TYPE_DOUBLE` 且
  `outReturn` 非空；命中 `functionIndex` 后从两个 `args` slot 解出 `TZrFloat64`。state-free typed helper
  直接调用 `zr_aot_typed_f64_fn_<index>(zr_aot_arg0, zr_aot_arg1)`；除法/取模等仍需要 `state` 的 f64 二参
  helper 调用 `zr_aot_typed_f64_fn_<index>(state, zr_aot_arg0, zr_aot_arg1)`，保留 divide/modulo-by-zero
  诊断路径；返回值统一用 `ZrCore_Value_InitAsFloat(...)` 写回 boxed double。该切片只关闭 f64 二参数桶，
  不声明 bool-return numeric comparison buckets、三参数及以上、object/inline、numeric widening、实例 receiver 或
  public `MethodInfo` 对象完成。
- 10-S2U / 10-S3Y 在 generated reflection invoker 中补齐 bool-return numeric comparison 二参数桶：
  `bool(int, int)`、`bool(uint, uint)`、`bool(float, float)`。三个 generated helper 分别要求 MethodInfo
  signature 为 bool 返回、两个对应数值参数、`parameterTypes[0/1].baseType` 与 `args[0/1].type` 同时匹配
  INT64/UINT64/DOUBLE，且 `outReturn` 非空；命中 `functionIndex` 后从两个 `args` slot 解出
  `TZrInt64` / `TZrUInt64` / `TZrFloat64`，调用已有 `zr_aot_typed_bool_fn_<index>(zr_aot_arg0, zr_aot_arg1)`，
  并用 `ZrCore_Value_InitAsBool(...)` 写回 boxed bool。该切片只关闭已有 signed/unsigned/float comparison
  typed bool two-arg helper 的 reflection unbox/boxing 桶，不声明 numeric widening、跨数值类型比较、
  三参数及以上、object/inline、实例 receiver 或 public `MethodInfo` 对象完成。
- 10-S2V / 10-S3Z 在 generated reflection invoker 中开启第一类三参数解包桶：`int64(int64, int64, int64)`。
  `zr_aot_try_invoke_i64_three_arg(...)` 要求 MethodInfo signature 为 int64 返回、三个 int64 参数、
  `parameterTypes[0/1/2].baseType == ZR_VALUE_TYPE_INT64`、`args[0/1/2].type == ZR_VALUE_TYPE_INT64` 且
  `outReturn` 非空；命中 `functionIndex` 后从三个 `args` slot 解出 `TZrInt64`。state-free typed helper
  直接调用 `zr_aot_typed_i64_fn_<index>(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2)`；除法/取模等仍需要
  `state` 的 i64 三参 helper 调用 `zr_aot_typed_i64_fn_<index>(state, zr_aot_arg0, zr_aot_arg1, zr_aot_arg2)`，
  保留 divide/modulo-by-zero 诊断路径；返回值统一用 `ZrCore_Value_InitAsInt(...)` 写回 boxed int64。
  该切片只关闭 i64 三参数桶，不声明 u64/f64/bool 三参数桶、四参数及以上、object/inline、numeric widening、
  实例 receiver 或 public `MethodInfo` 对象完成。
- 10-S2W / 10-S3AA 在同一 numeric three-arg reflection invoker 模块中开启 `uint64(uint64, uint64, uint64)`。
  `zr_aot_try_invoke_u64_three_arg(...)` 要求 MethodInfo signature 为 uint64 返回、三个 uint64 参数、
  `parameterTypes[0/1/2].baseType == ZR_VALUE_TYPE_UINT64`、`args[0/1/2].type == ZR_VALUE_TYPE_UINT64` 且
  `outReturn` 非空；命中 `functionIndex` 后解出三个 `TZrUInt64`。state-free helper 直接调用
  `zr_aot_typed_u64_fn_<index>(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2)`；除法/取模等保留 stateful helper
  `zr_aot_typed_u64_fn_<index>(state, zr_aot_arg0, zr_aot_arg1, zr_aot_arg2)`，继续保留 divide/modulo-by-zero
  诊断路径；返回值统一用 `ZrCore_Value_InitAsUInt(...)` 写回 boxed uint64。该切片只关闭 u64 三参数桶，
  不声明 f64/bool 三参数桶、四参数及以上、object/inline、numeric widening、实例 receiver 或 public
  `MethodInfo` 对象完成。
- 10-S2X / 10-S3AB 在 numeric three-arg reflection invoker 模块中开启 `float(float, float, float)`。
  `zr_aot_try_invoke_f64_three_arg(...)` 要求 MethodInfo signature 为 double/float 返回、三个 double/float 参数、
  `parameterTypes[0/1/2].baseType == ZR_VALUE_TYPE_DOUBLE`、`args[0/1/2].type == ZR_VALUE_TYPE_DOUBLE` 且
  `outReturn` 非空；命中 `functionIndex` 后解出三个 `TZrFloat64`。state-free helper 直接调用
  `zr_aot_typed_f64_fn_<index>(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2)`；除法/取模等保留 stateful helper
  `zr_aot_typed_f64_fn_<index>(state, zr_aot_arg0, zr_aot_arg1, zr_aot_arg2)`，继续保留 divide/modulo-by-zero
  诊断路径；返回值统一用 `ZrCore_Value_InitAsFloat(...)` 写回 boxed double。该切片只关闭 f64 三参数桶，
  不声明 bool 三参数桶、四参数及以上、object/inline、numeric widening、实例 receiver 或 public `MethodInfo` 对象完成。
- 10-S2Y / 10-S3AC 在独立 bool three-arg reflection invoker 模块中开启 `bool(bool, bool, bool)`。
  `zr_aot_try_invoke_bool_three_arg(...)` 要求 MethodInfo signature 为 bool 返回、三个 bool 参数、
  `parameterTypes[0/1/2].baseType == ZR_VALUE_TYPE_BOOL`、`args[0/1/2].type == ZR_VALUE_TYPE_BOOL` 且
  `outReturn` 非空；命中 `functionIndex` 后解出三个 `TZrBool`，直接调用
  `zr_aot_typed_bool_fn_<index>(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2)`，再用
  `ZrCore_Value_InitAsBool(...)` 写回 boxed bool。为覆盖当前 `left && middle && right` 生成形态，
  bool three-arg typed thunk 识别同步补齐短路 AND cleanup-reset 形态。该切片只关闭 bool 三参数桶，
  不声明四参数及以上、object/inline、numeric widening、实例 receiver 或 public `MethodInfo` 对象完成。

## 2. token 驱动的反射解析（衔接 11）

现状只能按 string 名查类型；补 token 通道（对标 mono token→entity、il2cpp index→entity）：

- 反射 API 增 `ZrCore_Reflection_ResolveToken(metadataToken) -> 运行期实体`，经 `11` 的
  `SZrMetadataRuntime`（MetadataCache 等价）lazy 解析 token → 原型/方法/字段。
- 10-S3A 已先落地 public carrier：
  `ZrCore_Reflection_ResolveToken(SZrMetadataRuntime *, TZrMetadataToken, SZrReflectionResolvedToken *)`
  返回 token kind、原始 token record、TypeDef/TypeSpec row、registry-backed type layout、FieldDef row、
  owner/field layout 与 byte offset。TypeDef/TypeSpec 走 `11-S4` layout binding view，TypeRef 先返回
  record-only type entity，FieldDef 走 `11-S4I` field binding view，method token 先返回 method record。
  当前还未把该 carrier 物化为 public reflection object。
- 10-S3B 已让 `ResolveToken()` 识别 11-S5 MethodSpec `SIGNATURE` token：当 signature view 验证为
  `GENERIC_INST(MEMBER_REF methodToken, args...)` 时，返回 method-like carrier，`token/record` 指向 MethodSpec
  signature record，`methodToken/methodRecord` 指向 underlying method，并暴露 signature hash、argument count 与
  argument-list blob offset。普通 signature token 仍不被该入口误解析。
- 10-S3C 已让 method-like carrier 暴露方法签名身份：普通 MethodDef/MethodRef token 通过
  `ZrCore_MetadataRuntime_ResolveSignatureRecord()` 填充 `methodSignatureToken/methodSignatureRecord/methodSignatureHash`；
  MethodSpec token 则把自身 signature token/record/hash 作为方法签名身份。10-S3D/10-S3E 已把普通 method
  token 与 MethodSpec underlying method 的 AOT binding carrier 接上；10-S2B/10-S3F 已提供最小 token-driven
  `Method.Invoke` dispatcher 消费该 carrier。仍未物化 public method reflection object，也未完成参数/返回 marshaling。
- 11-S2B / 10-S2 support 已让 generated code registration 暴露 `methodTokens[functionIndex]` 载体，
  与 `methodInfos[]` 对齐。当前只为 root module typed exported function 填充可靠 `MEMBER_DEF` token，
  无法可靠绑定的 function slot 写 `0u`；该表为后续 token→`MethodInfo`/function pointer/invoker lookup 提供
  真实 AOT carrier。
- 11-S2D / 10-S2 support 已提供 runtime 内部只读 binding view：
  `ZrCore_MetadataRuntime_ReadMethodBindingView()` 可按唯一 local `MEMBER_DEF` token 从 code registration 反查
  `functionIndex`、`SZrAotMethodInfo`、entry thunk 和 reflection invoker；缺失表、非 method token、重复 token、
  MethodInfo slot 不一致或缺少 thunk/invoker 均返回 false 并清空输出。public `ResolveToken()` 已在
  10-S3D/10-S3E 消费该 view；10-S2B/10-S3F 的 `ZrCore_Reflection_InvokeMethodToken(...)` 已消费该 view
  调用 registered invoker，但参数/返回 marshaling 仍未完成。
- 10-S3D 已让 public `ZrCore_Reflection_ResolveToken()` 对普通 MethodDef token 消费 11-S2D binding view：
  `SZrReflectionResolvedToken` 现在在 method record/signature identity 之外可携带 `methodFunctionIndex`、
  `methodInfo`、`methodFunctionPointer` 和 `methodInvoker`。没有 AOT binding 时仍返回 method record carrier，
  绑定字段保持空。
- 10-S3E 已让 MethodSpec `SIGNATURE` token 的 public carrier 复用 underlying MethodDef 的 11-S2D binding：
  `ResolveToken(MethodSpec)` 保留自身 MethodSpec signature/hash/generic argument identity，同时在 underlying
  `methodToken` 能唯一绑定到 AOT code-registration slot 时填充同一组 `methodFunctionIndex`、`methodInfo`、
  entry thunk 与 invoker。该路径只提供调用载体，不声明泛型方法实例专用 code slot、public generic method
  reflection object 或 `Method.Invoke` 参数/返回 marshaling 已完成。
- 10-S2B / 10-S3F 已新增 public token-driven `ZrCore_Reflection_InvokeMethodToken(...)` dispatcher：入口先用
  `ZrCore_Reflection_ResolveToken()` 解析 method token，要求结果为 method-like entity 且同时带
  `methodInfo/methodFunctionPointer/methodInvoker`，再把 `state/target/method/self/args/outReturn` 直接交给
  registered AOT invoker。该 dispatcher 只验证 token→binding→invoker 的控制流；参数数组 unbox、返回 box、
  签名校验和 public `MethodInfo` 对象仍待完整 10-S2 后续。
- 10-S2C / 10-S3G 已新增 counted token-driven dispatcher consumer：
  `ZrCore_Reflection_InvokeMethodTokenWithArgCount(...)` 复用同一个 token→binding→invoker carrier，并在 dispatch
  前用 MethodInfo signature 做最小参数数量边界检查。非 varargs 必须精确匹配 `parameterCount`，varargs 允许
  `argCount >= parameterCount`；该入口仍未执行参数类型匹配、unbox/box 或 public `MethodInfo` 对象物化。
- 10-S2D / 10-S3H 已让 counted dispatcher 在 arity 之外检查 MethodInfo signature shape：缺少 fixed
  `parameterTypes` 或 required `returnType` 时拒绝 dispatch，避免把不完整 MethodInfo 交给后续 invoker。
  完整参数类型兼容、box/unbox 和 public reflection object 仍待后续。
- 10-S2E / 10-S3I 已让 counted dispatcher 消费 fixed `parameterTypes[i].baseType`，在 concrete base type 与
  `SZrTypeValue.type` 不匹配时拒绝 dispatch。该 guard 只覆盖已知 fixed 参数的 baseType 等值匹配，不处理
  varargs 额外参数、nullable/ownership/array/staticCType、数值 widening、参数 unbox/返回 box 或 public `MethodInfo` 对象。
- 10-S2F / 10-S3J 已让 counted dispatcher 在 registered invoker 写完 `outReturn` 后，对 concrete return
  `baseType` 与 `outReturn->type` 做等值 guard。该 guard 只验证返回 slot 的运行时类型，不负责生成/执行返回 box，
  也不声明 AOT/解释器结果等价闭环。
- 10-S2G / 10-S3K 已让 counted dispatcher 在 required return signature 下先清空 `outReturn`，再调用
  registered invoker；若 invoker 没有写入本次返回值，post-dispatch guard 会看到 null 并拒绝成功。该路径仍不负责
  返回 box、typed return register 捕获或 AOT/解释器结果等价闭环。
- 10-S2H / 10-S3L 已让 counted dispatcher 在无返回值 signature 下、registered invoker 返回后把 `outReturn`
  重置为 null。该 canonicalization 只定义 void/no-return 的 public output slot 形态，不把 invoker return boxing
  或 typed return register 捕获视为完成。
- 10-S2I / 10-S3M 已让 generated MethodInfo invoker 消费 11-S2D/10-S3D 暴露的 `functionIndex`：
  对 int64 no-arg signature，invoker switch 到已有 typed i64 helper 并写出 boxed `outReturn`。完整执行 thunk
  仍作为 unsupported fallback 执行，不参与 return boxing。该 consumer 不声明 MethodSpec 专用 code slot、
  参数数组 marshaling、其它返回类型或 cross-module token rewrite 完成。
- 10-S2J / 10-S3N 已让 generated MethodInfo invoker 继续消费同一个 `functionIndex` carrier：
  对 uint64 no-arg signature，invoker switch 到已有 typed u64 helper 并写出 boxed `outReturn`。该 consumer
  仍不声明 MethodSpec 专用 code slot、参数数组 marshaling、bool/f64/object/inline 返回或 cross-module token
  rewrite 完成。
- 10-S2K / 10-S3O 已让 generated MethodInfo invoker 继续消费同一个 `functionIndex` carrier：
  对 bool no-arg signature，invoker switch 到已有 typed bool helper 并写出 boxed `outReturn`。该 consumer
  仍不声明 MethodSpec 专用 code slot、参数数组 marshaling、f64/object/inline 返回或 cross-module token
  rewrite 完成。
- 10-S2L / 10-S3P 已让 generated MethodInfo invoker 继续消费同一个 `functionIndex` carrier：
  对 f64 no-arg signature，invoker switch 到已有 typed f64 helper 并写出 boxed `outReturn`。该 consumer
  仍不声明 MethodSpec 专用 code slot、参数数组 marshaling、object/inline 返回或 cross-module token
  rewrite 完成。
- string 名查找改为「名→token→实体」两段，token 是单一真相（不变量 C），名表可被裁剪（`12`）。

## 3. 字段 offset / 布局反射

- `DESCRIPTION` 级类型暴露字段偏移：直接读唯一 `SZrTypeLayout.fields[i].byteOffset`（`02`/`11`），
  **不**在反射层另存偏移（不变量 C）。
- 11-S4I 已补出后续 token-driven 字段反射实体可消费的只读绑定视图：
  `ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView()` 以 FieldDef token 为入口读取 zrp FieldDef row 的
  `byteOffset/typeLayoutId`，并要求 owner/field layout 都来自 code-registration layout registry。当前只是数据路径，
  尚未把该 view 接入 public reflection field entity。
- 10-S4C 已把 FieldDef binding view 的 owner/type 侧信息接入 public reflection carrier：
  `ZrCore_Reflection_ResolveToken()` 对 FieldDef token 现在同时暴露 owner type token/record/row、
  field type layout、field type token/record 与 byte offset。field type token 复用 11-S4L/11-S4N 的
  layoutId/cTypeId→TypeDef/TypeSpec token resolver，反射层不另存字段类型身份。当前仍未构造 public
  `FieldInfo` 对象，也未实现字段值读写 marshaling。
- 10-S4F 已把 FieldDef token carrier 物化为最小 public `FieldInfo` object：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 复用 `ResolveToken()` 的 FieldDef 结果、FieldDef/TypeDef row 的
  zrp string-pool offsets 和 11-S4 TypeDef layout binding view，填充 name/qualifiedName/typeName、metadata/owner/type
  tokens、offset/size/layout id、layout 子对象和 nested type literal。当前仍不实现字段值读写 marshaling、owner/module
  object links、cache policy 或完整 `FieldInfo` 方法行为。
- 10-S4G / 11-S4U 已让最小 public `FieldInfo` object 暴露声明类型身份：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在同时填充 `ownerTypeName`、`declaringTypeName`
  和 nested `declaringType` type literal object；这些值仍来自 owner TypeDef row 的 zrp string pool。完整 owner/module
  reflection object link、缓存策略和字段值读写仍留给后续。
- 10-S4H / 11-S4V 已让最小 public `FieldInfo` object 的 `owner` 指向同一个 nested `declaringType` type literal
  object。该切片只关闭 owner identity link，不构造 module reflection link、缓存或字段值读写行为。
- 10-S4I / 11-S4W 已让最小 public `FieldInfo` object 暴露 `moduleName` string carrier：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 从 attached metadata runtime 的 `module->moduleName` 读取名称，
  缺失时回退 `fullPath`。该切片只写入稳定字符串字段，不构造 module reflection object、缓存或字段值读写行为。
- 10-S4J / 11-S4X 已让最小 public `FieldInfo` object 暴露 raw `metadataFlags` integer carrier：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 从已解析的 `SZrZrpMetadataFieldDefRow.flags` 写入
  `FieldInfo.metadataFlags`。该切片不解释 flags 位含义，也不改变 `isStatic`/`isConst`。
- 10-S4K / 11-S4Y 已让最小 public `FieldInfo` object 暴露 raw signature blob coordinate carrier：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 从已解析的 `SZrZrpMetadataFieldDefRow.signatureBlobOffset` /
  `signatureBlobLength` 写入 `FieldInfo.signatureBlobOffset` 与 `FieldInfo.signatureBlobLength`。该切片不验证
  blob slice，也不解析 field signature 语义。
- 10-S4L / 11-S4Z 已让最小 public `FieldInfo` object 暴露 validated field signature header carrier：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 复用 `ZrCore_MetadataRuntime_ReadSignatureView()`，在 paired
  signature record 指向合法 `FIELD_SIG` blob 时写入 `FieldInfo.signatureRootNode`、`signatureFlags` 与
  `fieldTypeBlobOffset`。签名缺失、无效或不是 `FIELD_SIG` 时这些 validated header 字段保持 `0`；该切片仍不把
  field type node 绑定为语义类型，也不实现字段值读写。
- 10-S4M / 11-S4AA 已让最小 public `FieldInfo` object 消费 validated `FIELD_SIG` 的 field type-node view：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 在 header 合法时调用
  `ZrCore_MetadataRuntime_ReadSignatureTypeNode()`，并写入 `fieldTypeSignatureNode`、`fieldTypeSignatureBlobOffset`、
  `fieldTypeSignatureNextBlobOffset`、`fieldTypeSignaturePayload0/1`、`fieldTypeSignatureBaseTypeBlobOffset`、
  `fieldTypeSignatureChildCount` 与 `fieldTypeSignatureChildListBlobOffset`。签名缺失、无效或 type-node 读取失败时
  这些 summary 字段保持 `0`；该切片仍不把 signature node 绑定为 semantic field type，也不实现字段值读写。
- 10-S4N / 11-S4AB 已在上述 summary 之上补出 primitive signature type carrier：当 field type-node 为
  `PRIMITIVE` 时，`ZrCore_Reflection_BuildFieldInfoTokenObject()` 写入 `fieldTypeSignatureValueType` 与
  `fieldTypeSignatureTypeName`。该字段独立于 layout/token-derived `typeName`，当前只把 primitive payload
  显式暴露为 public carrier，不声明 TypeDef/TypeRef signature token binding、字段类型一致性校验或字段值读写完成。
- 10-S4O / 11-S4AC 已在 primitive signature type carrier 之上补出独立 type literal object：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 在 field type-node 为 `PRIMITIVE` 时，用
  `fieldTypeSignatureTypeName` 构造 `fieldTypeSignatureType` reflection object。该 object 独立于
  layout/token-derived `type`，当前只覆盖 primitive type literal，不声明递归 type-node object、TypeDef/TypeRef
  signature binding 或字段值读写完成。
- 10-S4P / 11-S4AD 已让最小 public `FieldInfo` object 的 `module` 链接到 attached runtime module 的
  reflection object：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 在 runtime module 是真实
  `ZR_OBJECT_INTERNAL_TYPE_MODULE` 时复用 `reflection_build_module_reflection()`，因此 `module.kind/name/qualifiedName`
  与现有 module reflection cache 保持一致。该切片只关闭 FieldInfo -> module identity link，不声明字段值读写、
  完整 FieldInfo 方法、跨模块 FieldRef/TypeRef 或 signature-derived semantic field type binding 完成。
- 10-S4Q / 11-S4AE 已让最小 public `FieldInfo` object 对 direct local `TYPE_DEF` field signature node 暴露
  token/layout carrier：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 将 validated `FIELD_SIG` field type-node
  与 attached metadata token records 的 direct type signature blob 做等值匹配，命中后复用
  `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` 写入 `fieldTypeSignatureTypeToken`、
  `fieldTypeSignatureTypeLayoutId` 与 `fieldTypeSignatureTypeSize`。该切片只关闭 direct local TypeDef carrier；
  不声明 recursive type-node object、TypeRef/cross-module signature binding、字段类型一致性校验或字段值读写完成。
- 10-S4R / 11-S4AF 已让上述 direct local `TYPE_DEF` signature identity 继续物化为独立 type literal object：
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 复用 TypeDef layout binding view 读取 zrp string-pool type name，
  将 `fieldTypeSignatureTypeName` 写为 `int`，并把 `fieldTypeSignatureType` 构造成 `kind == "type"` 的
  reflection object。该 object 仍独立于 layout/token-derived `type`；当前只覆盖 current-runtime local TypeDef，
  不声明 TypeRef/cross-module provider、recursive wrapper/generic type-node object 或字段值读写完成。
- 10-S4S / 11-S4AG 已让最小 public `FieldInfo` object 对 current-runtime bound `TYPE_REF` field signature node
  复用 11-S4S TypeRef token -> target TypeDef layout resolver：`fieldTypeSignatureTypeToken` 保持为 TypeRef token，
  `fieldTypeSignatureTypeLayoutId/Size` 来自目标 TypeDef layout，同时读取 target TypeDef row name 并物化
  `fieldTypeSignatureType` type literal object。该切片只覆盖已 attached 的当前 runtime TypeRef -> TypeDef 绑定；
  不声明跨模块 provider load/version compatibility、recursive wrapper/generic type-node object 或字段值读写完成。
- 10-S4T / 11-S4AH 已让最小 public `FieldInfo` object 对 signature-derived type 与 FieldDef layout 单一真相做
  只读一致性 carrier：`fieldTypeSignatureMatchesLayout` 只有在 signature-derived layout 存在、FieldDef layout
  存在、layout id 非 none，且 layout id 与 registry layout 指针都相同时才为 true。primitive signature 与
  layout-derived field type 不一致时保持 false；direct `TYPE_DEF` 与 bound `TYPE_REF` 命中同一 registry layout 时为 true。
  该切片不改变 token/layout 解析规则，也不实现字段值读写或 recursive semantic type object。
- 10-S4U / 11-S4AI 已让最小 public `FieldInfo` object 暴露 `fieldTypeSignatureNodeObject`：
  该对象以 `kind == "signatureTypeNode"` 承载已验证 field type-node 的 node/blob offset/payload/base/child summary，
  并同步 signature-derived token/layout/size/typeName 与 `matchesLayout`。它只把当前 flat carrier 聚合成 read-only
  object，尚不递归展开 wrapper/generic child nodes，也不实现字段值读写或完整 semantic field type binding。
- 10-S4V / 11-S4AJ 已让 `fieldTypeSignatureNodeObject` 对 wrapper/generic 的 base type node 暴露
  `baseTypeNodeObject`：当前验证 `FIELD_SIG(GENERIC_INST(TYPE_DEF(object, 17), int64))` 的 top-level generic node
  能递归读取同一 signature blob 中的 direct base `TYPE_DEF` 子节点并保留 node/blob/payload summary。该切片仍不物化
  generic argument child list，也不做 base/argument semantic token/layout binding。
- 10-S4W / 11-S4AK 已让 `fieldTypeSignatureNodeObject` 对 wrapper/generic/tuple/union 这类带 child list 的 signature
  node 暴露 `childNodeObjects` array：当前验证 `FIELD_SIG(GENERIC_INST(TYPE_DEF(object, 17), int64))` 的 generic
  argument list 能从同一 blob 读取 `PRIMITIVE(INT64)` 子节点并保留 structural node/blob/payload summary。该切片仍不做
  generic argument semantic token/layout binding、跨模块 provider 加载或完整 recursive field type binding。
- 10-S4X / 11-S4AL 已让 primitive generic argument child type-node object 复用 runtime builtin type name mapping：
  当前同一 `FIELD_SIG(GENERIC_INST(TYPE_DEF(object, 17), int64))` fixture 的 `childNodeObjects[0].typeName` 暴露为
  `int`，使 primitive argument 至少具备 semantic name carrier。该切片仍不绑定 direct TypeDef/TypeRef child 的
  semantic token/layout，也不声明完整 recursive field type binding。
- 10-S4Y / 11-S4AM 已让 recursive direct `TYPE_DEF` signature type-node object 复用 attached metadata record、
  registry layout resolver 和 TypeDef string-pool name：当前同一 fixture 扩展为
  `FIELD_SIG(GENERIC_INST(TYPE_DEF(object, 17), int64, TYPE_DEF(object, 17)))`，并让
  `fieldTypeSignatureNodeObject.baseTypeNodeObject` 与 `childNodeObjects[1]` 同时暴露
  `typeToken == TEST_FIELD_TYPE_DEF_TOKEN`、`typeLayoutId == 42`、`typeSize == 16`、`typeName == "int"`。
  该切片仍不绑定 direct `TYPE_REF` child、不声明跨模块 provider 或完整 recursive semantic field type binding。
- 10-S4Z2 / 11-S4AN 已让 recursive direct `TYPE_REF` signature child type-node object 复用 module metadata record、
  attached TypeRef→target TypeDef layout resolver 和 target TypeDef name：当前同一 fixture 继续扩展为
  `FIELD_SIG(GENERIC_INST(TYPE_DEF(object, 17), int64, TYPE_DEF(object, 17), TYPE_REF(object, 23)))`，并让
  `fieldTypeSignatureNodeObject.childNodeObjects[2]` 暴露 `typeToken == TEST_TYPE_REF_TOKEN`、`typeLayoutId == 42`、
  `typeSize == 16`、`typeName == "int"`。该切片仍不声明跨模块 provider loading、provider version compatibility
  或完整 recursive semantic field type binding。
- 10-S4Z3 / 11-S4AO 已让 recursive signature type-node object 在具备 semantic `typeName` 时同步物化 `type`
  type literal object：当前同一 generic fixture 锁定 `baseTypeNodeObject.type`、primitive `childNodeObjects[0].type`、
  direct TypeDef `childNodeObjects[1].type` 和 direct TypeRef `childNodeObjects[2].type` 均为 `kind == "type"`、
  `name/qualifiedName == "int"`。该切片沿用既有 FieldInfo/ParameterInfo 的 public `type` 字段惯例，不新增 metadata ABI；
  尚不声明跨模块 provider loading、字段值读写或完整 FieldInfo method surface。
- 10-S4Z13 / 11-S4AY 已让最小 public `FieldInfo` object 在 `metadataToken` 旁携带 `metadataRuntime`
  native-pointer carrier，形成后续 object-level `FieldInfo.GetValue/SetValue` 可复用的 same-runtime identity。
  该字段只承载 attached metadata runtime handle，不改变 token/metadata ABI，不声明完整 FieldInfo method surface 完成。
- 10-S4Z14 / 11-S4AZ 已把最小 public `FieldInfo` object 接到 read-only inline value adapter：
  `ZrCore_Reflection_ReadFieldInfoObjectValue()` 从 object 字段读取 `metadataRuntime` 与 `metadataToken`，
  验证 native-pointer/int token 形态后复用既有 token-driven read path。该切片只关闭 object-level read
  adapter，不声明 `SetValue`、nested field marshaling 或完整 FieldInfo method surface 完成。
- 10-S4Z15 / 11-S4BA 已把最小 public `FieldInfo` object 接到 object-level inline value write adapter：
  `ZrCore_Reflection_WriteFieldInfoObjectValue()` 从 object 字段恢复 same-runtime FieldDef context，
  再委托既有 token-driven write path。该切片只关闭 object-level write adapter boundary，不声明完整
  managed `FieldInfo.SetValue` method surface、nested field marshaling 或完整 metadata sweep 完成。
- 10-S4Z16 / 11-S4BB 已补齐最小 public `FieldInfo` object adapter 对 primitive POD raw inline 字段的
  读写覆盖：object-level read/write API 复用既有 token-driven primitive POD path，覆盖 int32 raw storage、
  type-mismatch reject 后原始字节保持，以及成功写回。该切片为覆盖补强，不新增 API。
- 10-S4Z17 / 11-S4BC 已建立 nested inline field marshaling 的第一条只读边界合约：非 `VALUE_SLOT`、
  非 GC/ownership 的 inline struct/union 字段，在 validated `FIELD_SIG(TYPE_DEF/TYPE_REF)` 和 resolved
  field type layout byte size 一致时，`ReadFieldInfoTokenValue()` / object adapter 返回借用的
  `ZR_VALUE_TYPE_NATIVE_POINTER` view，指向调用方 inline storage 中的字段地址。该 view 不拥有内存、
  不登记 GC、不复制字段；后续 10-S4Z18 在此基础上补了受限 borrowed-source 写回，完整 nested field 解构仍待后续。
- 10-S4Z18 / 11-S4BD 已在 S4Z17 borrowed view 上补出受限 inline aggregate 写回边界：写入值必须是非空
  `ZR_VALUE_TYPE_NATIVE_POINTER` source，目标字段仍要求非 `VALUE_SLOT`、非 GC/ownership、validated
  `FIELD_SIG(TYPE_DEF/TYPE_REF)`、resolved struct/union type layout byte size 匹配且 `blittable == true`。
  满足条件时按 field byte size `memcpy` 到调用方 inline storage；null source、scalar source、non-blittable layout
  或带 GC/ownership flags 的字段均拒绝并保持原始 bytes。
- 10-S4Z20 / 11-S4BF 覆盖了 S4Z19 layout-aware write over existing owned nested value 的 replacement/drop 语义：
  focused fixture 通过 object-level FieldInfo write 把 `FIELD_COPY/FIELD_DROP` inline aggregate 中的 nested
  `SZrTypeValue` 从 unique-owned old string 替换为 native-pointer source 中的 plain new string，验证 old owner
  strong ref 释放到 0、destination ownership metadata 归一为空，且 new source 不被新增持有。生产代码未改动，
  该语义来自 `ZrCore_TypeLayout_CopyInline()` 对 value-slot field 调用 `ZrCore_Value_Copy()`。
- 10-S4Z21 / 11-S4BG 已建立 recursive nested inline field marshaling 的第一条 value-slot child read 边界：
  `ZrCore_Reflection_ReadFieldInfoTokenNestedValue()` 和 object adapter 从 retained FieldDef token/FieldInfo identity
  恢复 owner field、validated `FIELD_SIG(TYPE_DEF/TYPE_REF)` 和 resolved field type layout 后，按 `SZrTypeLayoutField`
  index 读取 nested `VALUE_SLOT` child 并用 `ZrCore_Value_Copy()` 物化 `SZrTypeValue`。该边界拒绝 short storage、
  out-of-range index，以及外层带 GC/ownership flags 的 inline aggregate；当前不声明多级路径、nested write、
  primitive raw child、managed `FieldInfo.GetValue/SetValue` method surface 或跨模块 provider 完成。
- 10-S4Z22 / 11-S4BH 已在 S4Z21 read 边界旁补上第一条 layout-indexed nested value-slot child write 边界：
  `ZrCore_Reflection_WriteFieldInfoTokenNestedValue()` 和 object adapter 复用同一 FieldInfo identity、FieldDef resolver、
  `FIELD_SIG(TYPE_DEF/TYPE_REF)` validation 与 inline aggregate borrowed-view 判定，按 `SZrTypeLayoutField` index 定位
  nested `VALUE_SLOT` child，并用 `ZrCore_Value_Copy()` 写入目标 slot。该边界覆盖 replacement/drop：旧 unique-owned
  nested value 会释放，destination ownership metadata 归一；short storage、out-of-range index 和外层 GC/ownership flags
  保持拒绝。当前仍不声明多级路径、primitive raw child write、managed `FieldInfo.SetValue` method surface 或完整 recursive
  nested marshaling 完成。
- 10-S4Z23 / 11-S4BI 已把 S4Z21/S4Z22 的单级 nested child 扩展为第一条 multi-level recursive nested path read 边界：
  `ZrCore_Reflection_ReadFieldInfoTokenNestedPathValue()` 和 object adapter 接收 `nestedFieldIndices[] + count`，
  从 retained FieldInfo identity 恢复 outer FieldDef/layout/signature 后逐级消费 `SZrTypeLayoutField.typeLayoutIndex` 和
  `ZrCore_MetadataRuntime_ResolveTypeLayout()`，要求中间 child 非 `VALUE_SLOT`/GC/ownership、byte size 与 child layout 一致、
  child layout 为 struct/union，最终节点仍只读 `VALUE_SLOT` child 并用 `ZrCore_Value_Copy()` 物化。当前不声明 nested path
  write、primitive raw child marshaling、managed `FieldInfo.GetValue/SetValue` methods 或 full recursive field type binding 完成。
- 10-S4Z24 / 11-S4BJ 已把 S4Z23 path read 边界补成第一条 multi-level recursive nested path write 边界：
  `ZrCore_Reflection_WriteFieldInfoTokenNestedPathValue()` 和 object adapter 接收 `nestedFieldIndices[] + count`，
  沿同一 `typeLayoutIndex`/runtime resolver 逐级定位 child layout，要求中间 child 非 `VALUE_SLOT`/GC/ownership、
  byte size 与 child layout 一致、child layout 为 struct/union，最终节点只写 `VALUE_SLOT` child 并复用
  `ZrCore_Value_Copy()` 的 replacement/drop 语义。当前不声明 primitive raw child marshaling、managed
  `FieldInfo.GetValue/SetValue` methods 或 full recursive field type binding 完成。
- 10-S4Z25 / 11-S4BK 已把同一 multi-level recursive nested path traversal 扩展到第一条 representative primitive raw child
  read/write 边界：`ZrCore_Reflection_ReadFieldInfoTokenNestedPathPrimitiveValue()` /
  `ZrCore_Reflection_WriteFieldInfoTokenNestedPathPrimitiveValue()` 和 object adapters 接收 `nestedFieldIndices[] + count`
  以及 caller-supplied primitive value type，先沿 `typeLayoutIndex`/runtime resolver 解析中间 struct/union child layout，
  再在 leaf raw child 上调用 shared `reflection_field_value_primitive.{h,c}` guard。该 guard 与 top-level primitive POD
  FieldInfo read/write 共用 byte-size、range、float32、NaN、precision 和 type-mismatch 规则。当前只声明代表性 INT32
  nested primitive path read/write；完整 primitive width/signature-derived matrix、managed `FieldInfo.GetValue/SetValue`
  methods 或 full recursive field type binding 仍待后续。
- 10-S4Z26 / 11-S4BL 已补上 nested primitive raw child 的 leaf layout identity guard：multi-level primitive path
  read/write 到达 leaf 后，除 byte-size 与 primitive POD guard 外，还要求 leaf `SZrTypeLayoutField.typeLayoutIndex ==
  ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`。这样不会把仍指向 registered child layout 的 inline aggregate child 误解释成
  raw primitive；top-level primitive FieldInfo path 保持原有 field-type identity 行为。当前仍不声明完整 primitive
  width/signature-derived matrix、managed `FieldInfo.GetValue/SetValue` methods 或 full recursive field type binding 完成。
- 10-S4Z27 / 11-S4BM 已把 nested primitive path 的代表性类型覆盖从 INT32 扩展到 bool、uint32 和 double：同一
  `nestedFieldIndices[]` path 在 leaf byte-size/align 调整后复用 S4Z25/S4Z26 的 object adapter、layout traversal、
  leaf identity guard 和 shared primitive POD guard。该切片是 coverage GREEN，不新增 public API 或 production code；
  完整 primitive width/signature-derived matrix、managed `FieldInfo.GetValue/SetValue` methods 或 full recursive field
  type binding 仍待后续。
- 10-S4Z28 / 11-S4BN 已把同一 nested primitive path 的 coverage 扩展到 storage-width matrix：focused fixture 通过
  `configure_nested_primitive_path_field_sizes()` 复用 S4Z25/S4Z26/S4Z27 的 two-level `{0,0}` object path、leaf identity
  guard 和 shared primitive POD guard，覆盖 int8、int16、int64、uint8、uint16、uint64 和 float32 raw child read/write。
  该切片仍是 coverage GREEN，不新增 public API 或 production code；完整 signature-derived binding、managed
  `FieldInfo.GetValue/SetValue` methods 或 full recursive field type binding 仍待后续。
- 10-S4Z19 / 11-S4BE 已把 S4Z18 的 borrowed-source 写回从 raw/blittable byte copy 扩展为 layout-aware copy：
  目标仍必须通过 S4Z17 inline aggregate borrowed-view 判定；当 resolved field type layout 可 raw copy 时走
  `ZrCore_TypeLayout_CopyInline()` 的 raw-copy 分支，当 `copyKind == ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY` 时走逐字段
  copy。当前只覆盖 non-GC/non-ownership inline aggregate source write，不声明 destination drop/replacement ownership
  lifecycle、recursive field access API 或完整 managed `FieldInfo.SetValue`。
- 10-S4Z4..10-S4Z12 / 11-S4AP..11-S4AX 已把 FieldDef token 的 inline 字段值边界接入 public
  runtime reflection API：`ZrCore_Reflection_ReadFieldInfoTokenValue()` / `WriteFieldInfoTokenValue()` 共用
  `reflection_field_value.c` 的 owner-field resolver。`VALUE_SLOT` 字段仍按 FieldDef token、owner field offset、
  field type layout id、flag 和调用方 inline storage range 校验后复制 `SZrTypeValue`；primitive POD raw 字段则额外要求
  validated `FIELD_SIG(PRIMITIVE(...))`、无 `VALUE_SLOT`/GC/ownership flags、字段 byte size 与 primitive C size 精确匹配，
  再用 `memcpy` 读写 raw scalar。当前验收覆盖 same-runtime int32 raw inline read/write、bool/uint32/double
  代表性 raw primitive matrix、int8/int16/int64/uint8/uint16/uint64/float32 storage-width matrix，并追加 integer
  range guard、float32 range guard、float32 NaN guard 和 float32 precision guard；不声明 nested field marshaling、object-level
  `FieldInfo.GetValue/SetValue` 或跨模块 provider。
- 反射读写字段值 = 在边界处按 offset 构造/解构 `SZrValue`（`07`§6），typed 内部仍是 `.`/偏移。

## 4. 泛型反射（衔接 08）

- 暴露泛型实例的类型实参：反射对象记 `baseToken + argTokens[]`（来自 `08`§3 实例化表）。
- 11-S4J 已补出后续类型实参反射可消费的只读绑定视图：
  `ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView()` 以 TypeSpec token 为入口，把 zrp TypeSpec row、
  11-S3K generic base-token binding 和 code-registration registry layout 连起来，并校验 signature identity。
  当前只是数据路径，尚未把该 view 接入 public generic type reflection 或 `MakeGenericType`。
- 10-S4B 已把 TypeSpec generic base/argument 信息接入 public reflection carrier：
  `ZrCore_Reflection_ResolveToken()` 对 TypeSpec token 填充 base token、signature token/hash 和 argument count；
  `ZrCore_Reflection_ResolveTypeSpecGenericArgument()` 可按 index 暴露 primitive argument signature 或
  direct TypeDef/TypeRef argument token/record。当前仍未构造 public generic type reflection object。
- 11-S5 已补出 GenericParam/GenericParamConstraint 的 attached zrp runtime view：
  `ZrCore_MetadataRuntime_ReadGenericParamView()` 按 TypeDef/MethodDef owner + parameter index 暴露 row、name、
  flags 与 constraint range；`ZrCore_MetadataRuntime_ReadGenericParamConstraintView()` 再按 constraint index 暴露
  constraint type token/record 和可选 validated signature blob。
- 10-S4D 已把 11-S5 泛型参数/约束 view 接入 public reflection carrier：
  `ZrCore_Reflection_ResolveGenericParameter()` 暴露 owner record、row、parameter index、name/flags 和约束范围；
  `ZrCore_Reflection_ResolveGenericParameterConstraint()` 暴露 constraint row、constraint type token/record 与签名
  blob slice。当前仍未构造 public generic parameter/constraint reflection object，也未实现 `MakeGenericType`。
- 10-S4E 已把 MethodSpec indexed argument view 接入 public reflection carrier：
  `ZrCore_Reflection_ResolveMethodSpecGenericArgument()` 可按 MethodSpec token + argument index 暴露 method token/record、
  signature hash、primitive argument node payload，或 direct TypeDef/TypeRef argument token/record。当前仍未物化 public
  generic method reflection object，也未实现泛型方法实例专用 runtime materialization。
- 11-S4K 已把 TypeDef/TypeSpec token→layout 查询收敛为
  `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()`，并缓存最近一次 token→layout 命中；这为后续
  `ResolveToken`/泛型类型实参反射提供 public token lookup 入口，但当前仍未物化 public reflection entity。
- 11-S4L 补出 `ZrCore_MetadataRuntime_ResolveTypeLayoutToken()`，可从 registry-backed `typeLayoutId`
  反查 TypeDef/TypeSpec token 并复用最近一次 cache；这为后续 layout-driven reflection entity 回写 token
  提供底座，但仍不是 public reflection API。
- 11-S4M 将上述 token/layout cache 扩展为 bounded 8-entry multi-entry cache，同一 runtime 可同时保留
  TypeDef 与 TypeSpec 的 token→layout 和 layoutId→token 命中；这避免后续类型实参枚举、layout-driven entity
  回写 token 与 public `ResolveToken` 互操作在同一反射流程中互相覆盖 cache，但仍不是 public reflection API。
- 11-S4N 补出 `ZrCore_MetadataRuntime_ResolveCTypeIdToken()`，在当前 `cTypeId == typeLayoutId` 的 registry
  不变量下让后续 layout-driven reflection entity 可直接按 generated C type id 回写 TypeDef/TypeSpec token；
  它复用 11-S4M cache 和 no-prototype-fallback 行为，但仍不是 public reflection API。
- 11-S4O 补出 code-registration `typeLayoutTokens/typeLayoutTokenCount` carrier，metadata runtime 可在 zrp scan
  fallback 前先从 registration 表把 cTypeId/typeLayoutId 解析为 TypeDef/TypeSpec token；这为后续 layout-driven
  reflection entity 提供更直接的 token 回写路径。
- 11-S4P 已把 generated token table 的可靠子集填为真实 `TYPE_DEF` token：本地 named struct/union layout
  能唯一匹配 TypeDef metadata 时，cTypeId/typeLayoutId→token resolver 可直接命中 registration 表；缺 metadata、
  多重匹配、TypeSpec/generic layout 仍为 0 且 fallback 到 zrp scan。该入口仍未接入 public reflection entity。
- `MakeGenericType`/运行期构造泛型 → 若实例已静态收集（`08`§3）返回其原型；否则解释器动态实例化
  （`08`§6 deopt），动态实例反射级别为 `RUNTIME_MAPPING`。
- 泛型参数约束（`08`§4）在 `DESCRIPTION` 级暴露。

- 10-S4Z29 / 08-S6H 已把 request-resolved AOT/interpreter-deopt generic carrier 物化为 public
  `kind == "type"` 对象；route/token/layout/same-runtime 与递归 generic argument identity 同步复制到 GC 对象图，
  不再借用调用方 descriptor 生命周期。token-only generic object、`MakeGenericType` 和执行消费者仍开放。
- 10-S4Z30 / 08-S6I 已提供 C 级 public `ZrCore_Reflection_MakeGenericTypeObject()`，让 open base + arguments 在
  单一边界内完成 resolver 路由与对象物化。脚本对象方法和解释器执行消费者仍开放。
- 10-S4Z31 / 08-S6J 已让同一 public builder 接受 token-only existing-TypeSpec carrier，并从 metadata argument
  view 物化 primitive/direct TypeDef/TypeRef 参数；复合 metadata node 保持 fail closed，等待递归对象化。
- 10-S4Z32 / 08-S6K / 11-S4BO 已递归对象化 token-only nested generic、array、tuple、ownership、nullable、union
  参数；nested generic 只在完整节点跨度命中 attached runtime 的真实本地 TypeSpec record 时生成 type-token 对象。
- 10-S4Z33 / 08-S6L 已让 public interpreter consumer 对重新校验的未收集 reference-class 请求创建普通对象；
  对象沿用 open generic class prototype，并拥有深拷贝 generic type object 上下文。AOT route 与 struct/union
  prototype fail closed；参数替换和方法执行不在该子切片内。
- 10-S4Z34 / 08-S6M 已把 11-S5 GenericParam owner/index view 与实例 genericArguments context 连通；same-runtime、
  open-base 和 arity 全部一致时，解释器按 metadata parameterIndex 取得 concrete argument type object，错误 owner
  或越界 index fail closed。该路径不修改 metadata 格式，也尚未进入 generic method call frame。
- 10-S4Z35 / 08-S6N 已把 generic instance context 绑定为 VM call-info 内的 GC-visible `SZrTypeValue`；
  可从 call-info 回读 type object 并按既有 GenericParam owner/index 解析 concrete argument。所有 call-info
  初始化/重用路径清零该 carrier，活动调用已纳入 GC mark/compact rewrite；实际 generic method 执行仍开放。
- 10-S4Z36 / 08-S6O 已提供 deopt generic type instance 的 public resolved VM method invoke；入口校验
  metadata runtime、open owner token 和 fixed arity，复用现有 object-call 边界，在 `PreCall` 后注入 context 再
  `Execute`。字节码方法执行期间可从 current call-info 解析 concrete GenericParam，但 MethodSpec 自身泛型仍开放。
- 10-S4Z37 / 08-S6P 已将既有 11-S5 MethodSpec signature/argument view 物化为 GC-managed
  `genericMethodContext` reflection object，携带 methodSpec/method token、完整 signature hash、runtime 和递归
  type-argument objects。该对象尚未进入 call info 或 MethodSpec execution。
- 10-S4Z38 / 08-S6Q 已将 MethodSpec context 绑定到独立的 VM call-info `SZrTypeValue` carrier；public
  bind/get/parameter resolver 校验 same runtime、underlying method owner、GenericParam range、context arity 与
  parameter index，活动 frame 的 method context 已纳入 compact full-GC mark/rewrite。MethodSpec execution 仍开放。
- 10-S4Z39 / 08-S6R 已执行 caller-resolved MethodSpec VM function：入口验证 MethodSpec/method GenericParam
  精确 arity 与 fixed VM function，复用现有 object-call pin/anchor/argument/result 路径，在 `PreCall` 后注入 method
  context。真实字节码函数从活动帧解析 method GenericParam 并返回显式参数；method-token/function 解析和脚本级
  `MakeGenericMethod` 仍开放。
- 10-S4Z40 / 08-S6S 已让 interpreter-deopt generic struct 使用既有 boxed dynamic struct 表示；实例拥有同一
  generic type context，支持 GenericParam 替换、`ZrCore_Value_Copy` 深复制隔离与带 type context 的 resolved VM
  方法执行。该路径不生成 typed/AOT layout，AOT route 仍 fail closed。
- 10-S4Z41 / 08-S6T / 11-S6J 已让跨模块 constructed-generic reflection 消费既有 TypeSpec binding，并以
  requester/provider canonical signature exact bytes 校验 provider-owned token/layout identity；该路径不引入全局 registry。
- 10-S4Z42 / 08-S6U / 11-S2E 已让本地 MethodSpec invoke 自动解析 underlying MethodDef VM function：metadata
  runtime 按唯一 MethodDef row 的 `functionIndex` 查询与 AOT function table 相同顺序的 root/constant/child 图，
  去重后只接受 instruction-backed non-native VM function，再复用 Z39 的 MethodSpec context 执行边界。错误/重复
  token、越界或超大 index、错误 arity 均 fail closed。脚本对象级 `MakeGenericMethod` 仍开放。
- 10-S4Z43 / 08-S6V / 11-S5A 已物化 public 开放泛型方法定义对象：metadata runtime 暴露 MethodDef 的
  GenericParam owner range，reflection builder 按声明数量验证每个 row 的 owner/物理 index/parameter index，并发布
  method token/runtime/flags/signature/declaring type、真实 zrp 方法名、参数名、constraint range 与参数数组。
  零参数、错误 token、超出 section 的声明数量和损坏 owner fail closed；type-argument 匹配与 MethodSpec 构造仍开放。

## 5. 反射与裁剪的注解（对标 DynamicallyAccessedMembers / DynamicDependency / RequiresUnreferencedCode）

在 zr 语言层引入保留注解，驱动 `12` 的标记，使「静态裁剪 + 动态反射」共存：

- `@reflectable(members: methods|fields|all)`：把某类型/成员提升到 `DESCRIPTION` 级（对标 `preserve`）。
- `@dynamically_accessed(MemberTypes)` 标注参数/返回：数据流分析（`12`）据此保留被反射访问的成员
  （对标 `DynamicallyAccessedMembers` + `FlowAnnotations`）。
- `@dynamic_dependency("Member", Type)`：手工保留特定成员（对标 `[DynamicDependency]`）。
- `@requires_unreferenced_code("reason")`：标记「内部用反射、裁剪下不安全」的 API，编译器在调用点
  给裁剪警告（对标 `RequiresUnreferencedCode` + analyzer，衔接 `12` trim warnings）。
- 未注解的动态反射点（`ResolveToken`/按名查未知类型）→ 裁剪后该目标可能 `NONE` → 返回空/抛错
  并产出 trim 警告（对标 illink「未注解反射」诊断）。

## 6. 反射对象缓存与 GC（沿用现状 + 对接 09）

- 沿用现有反射对象缓存（`__zr_reflection_cache`）+ PIN（`reflection_pin_raw_object`）。
- PIN 与 `09` 移动 GC 协同：反射对象进 `pinned` region 或登记为不可移动根，避免 compact 失效。

## 7. 落地切片

| 切片 | 内容 | 验收 |
|------|------|------|
| 10-S1 | 反射三级模型 + 实体级别标注（默认按可达性最小）（§0） | 🚧 2026-06-26 部分完成：10-S1A 已完成 AOT MethodInfo 级 `NONE`/`RUNTIME_MAPPING`/`DESCRIPTION` ABI carrier，默认/非裁剪生成方法保持 `RUNTIME_MAPPING`；12-S7Y 已让 opt-in code stripping 产物的 generated MethodInfo `reflectionMetadataLevel` 降为 `NONE` 并输出 `metadata_policy.reflectionLevel` marker；实体级 annotation/DESCRIPTION 提升、类型级默认最小与完整体积对比仍待 `12`/`10` 后续 |
| 10-S2 | 按签名分桶 invoker thunk（复用 07§6）+ 注册表登记（§1） | 🚧 2026-06-30 部分完成：10-S2A 已完成 `FZrAotEntryThunk` 当前签名桶 invoker ABI carrier、生成物共享 invoker 和 MethodInfo 登记；11-S2A/11-S2B 已提供 generated-C code registration 与 method token carrier；11-S2D 已提供 runtime 内部 method token→MethodInfo/function pointer/invoker binding view；10-S3D/10-S3E 已让 public token resolver carrier 消费该 view；10-S2B/10-S3F 已提供 `ZrCore_Reflection_InvokeMethodToken(...)` token-driven dispatcher，把已解析 method binding 交给 registered invoker；10-S2C/10-S3G 已提供 `ZrCore_Reflection_InvokeMethodTokenWithArgCount(...)` counted dispatcher，在调用 invoker 前检查 MethodInfo signature 的参数数量与 varargs 边界；10-S2D/10-S3H 已拒绝缺少 `parameterTypes` 或 required `returnType` 的不完整 signature shape；10-S2E/10-S3I 已对 fixed 参数的 concrete baseType 与 `SZrTypeValue.type` 做 invoker 前等值 guard；10-S2F/10-S3J 已对 concrete return baseType 与 invoker 写出的 `outReturn->type` 做 post-dispatch guard；10-S2G/10-S3K 已在 required return dispatch 前清空 `outReturn` 以拒绝 stale 返回槽误通过；10-S2H/10-S3L 已在 void/no-return dispatch 后把最终 `outReturn` 规范为 null；10-S2I/10-S3M 已生成 int64 no-arg reflection return-boxing bucket，按 `functionIndex` 调已有 typed i64 helper 并写入 boxed `outReturn`；10-S2J/10-S3N 已生成 uint64 no-arg reflection return-boxing bucket，按 `functionIndex` 调已有 typed u64 helper 并写入 boxed `outReturn`；10-S2K/10-S3O 已生成 bool no-arg reflection return-boxing bucket，按 `functionIndex` 调已有 typed bool helper 并写入 boxed `outReturn`；10-S2L/10-S3P 已生成 f64 no-arg reflection return-boxing bucket，按 `functionIndex` 调已有 typed f64 helper 并写入 boxed `outReturn`；10-S2M/10-S3Q 已生成 int64(int64) reflection 参数解包 + 返回装箱桶，从 `args[0]` 解出 `TZrInt64` 后调用 typed i64 one-arg helper；10-S2N/10-S3R 已生成 uint64(uint64) reflection 参数解包 + 返回装箱桶，从 `args[0]` 解出 `TZrUInt64` 后调用 typed u64 one-arg helper；object/inline 返回、更多签名桶、numeric widening、实例 receiver、签名类型兼容校验和 AOT/解释器结果等价仍待后续 |
| 10-S3 | token 驱动反射解析（衔接 11）（§2） | 🚧 2026-06-30 部分完成：10-S3A 已提供 public `ZrCore_Reflection_ResolveToken(...)` carrier，可按 token 返回 TypeDef/TypeSpec/TypeRef type entity、FieldDef field entity 和 MethodDef/MethodRef method record，并直接消费 11-S4 layout/field binding views；10-S3B 已让 `ResolveToken()` 接受 11-S5 MethodSpec `SIGNATURE` token，返回 MethodSpec signature record、underlying method token/record、signature hash 和 generic argument count；10-S3C 已让 method-like carrier 暴露 MethodDef/MethodRef paired signature record/hash，并把 MethodSpec 自身 signature record/hash 作为方法签名身份；10-S3D 已让普通 MethodDef token 的 carrier 在存在 AOT code-registration binding 时暴露 MethodInfo/function pointer/invoker；10-S3E 已让 MethodSpec carrier 复用 underlying MethodDef AOT binding；10-S3F 已提供 token-driven invoke dispatcher consumer；10-S3G 已提供 counted invoke signature arity guard consumer；10-S3H 已提供 counted invoke signature shape guard consumer；10-S3I 已提供 counted invoke fixed parameter base-type guard consumer；10-S3J 已提供 counted invoke return base-type guard consumer；10-S3K 已提供 counted invoke required return-slot reset guard consumer；10-S3L 已提供 counted invoke void return-slot canonicalization consumer；10-S3M 已提供 generated int64 no-arg invoker return-boxing consumer；10-S3N 已提供 generated uint64 no-arg invoker return-boxing consumer；10-S3O 已提供 generated bool no-arg invoker return-boxing consumer；10-S3P 已提供 generated f64 no-arg invoker return-boxing consumer；10-S3Q 已提供 generated int64 one-arg invoker argument-unbox + return-boxing consumer；10-S3R 已提供 generated uint64 one-arg invoker argument-unbox + return-boxing consumer；名表→token 重写、反射对象物化、完整 `Invoke` marshaling、MethodSpec runtime instance binding、裁剪诊断和完整 token-only 可用性仍待后续 |
| 10-S4 | 字段 offset / 泛型参数反射（§3/§4） | 🚧 2026-07-18 部分完成：10-S4A 已让脚本类型与字段 layout/offset 反射在 attached AOT registry 下读取 11-S4H 的 registry-backed `SZrTypeLayout`；11-S4I 已提供后续 DESCRIPTION 级 FieldDef token-driven 字段实体可消费的 FieldDef row→`byteOffset/typeLayoutId`→owner/field layout binding view；11-S4J 已提供后续类型实参反射可消费的 TypeSpec row→generic base binding→registry layout binding view；11-S4K 已提供后续 public token reflection 可复用的 TypeDef/TypeSpec token→layout resolver；11-S4L 已提供 layoutId→TypeDef/TypeSpec token 反查入口；11-S4M 已将 token/layout 命中扩展为 bounded multi-entry cache；11-S4N 已提供 cTypeId→TypeDef/TypeSpec token 反查入口；11-S4O 已提供 code-registration typeLayout token carrier 和 table-first cTypeId/typeLayoutId→token 消费路径；11-S4P 已让唯一匹配本地 TypeDef 的 generated struct/union entries 写入真实 token；11-S4Q 已让唯一匹配同函数 TypeSpec 的 generated generic entries 写入真实 token；11-S4R/11-S4R-union 已让 generated struct/union owner fields 暴露 ownership offset table；10-S4B 已让 public `ResolveToken` 暴露 TypeSpec base token/signature/argument count，并新增 indexed generic argument carrier；10-S4C 已让 FieldDef public carrier 暴露 owner type record/row 与 field type token/record；10-S4D 已让 public reflection carrier 暴露 GenericParam 与 GenericParamConstraint 的 owner/name/flags/constraint type/signature 信息；10-S4E 已让 MethodSpec public carrier 暴露 method token/record、signature hash 和 indexed generic argument node/token/record；10-S4F 已提供最小 FieldDef token -> public `FieldInfo` object，填充 name/type/token/layout/offset/size；10-S4G..10-S4V/11-S4U..11-S4AJ 已逐步补齐 declaring type、owner、moduleName、metadataFlags、signature blob/header/type-node、primitive/type object、module object、direct TypeDef、bound TypeRef、layout consistency、signature node object 和 base type-node object carriers；10-S4W/11-S4AK 已让 generic/wrapper signature node object 暴露 `childNodeObjects` structural list；10-S4X/11-S4AL 已让 primitive generic argument child node 暴露 semantic `typeName`；10-S4Y/11-S4AM 已让 recursive direct TypeDef base/child signature node object 暴露 semantic token/layout/name；10-S4Z2/11-S4AN 已让 recursive direct TypeRef child signature node object 暴露 semantic token/layout/name；10-S4Z3/11-S4AO 已让 recursive signature type-node object 在有 semantic `typeName` 时物化 `type` type literal；10-S4Z4..10-S4Z12/11-S4AP..11-S4AX 已提供 FieldDef token `VALUE_SLOT` inline read/write、primitive POD int32 raw inline read/write 边界、bool/uint32/double representative primitive POD raw inline read/write matrix、int8/int16/int64/uint8/uint16/uint64/float32 storage-width primitive POD raw inline read/write matrix，以及 integer range guard、float32 range guard、float32 NaN guard 和 float32 precision guard；10-S4Z13/11-S4AY 已让 FieldInfo object 携带 `metadataRuntime` native-pointer carrier，为后续 object-level 字段方法提供 same-runtime identity；10-S4Z14..10-S4Z28/11-S4AZ..11-S4BN 已让 object-level FieldInfo adapter 覆盖 `VALUE_SLOT`/primitive POD read-write、inline aggregate borrowed view/source write、nested owned value replacement/drop、单级 nested VALUE_SLOT child read/write、第一条 multi-level nested VALUE_SLOT path read/write，以及第一条 multi-level nested primitive POD raw child path read/write，并补充 nested primitive leaf layout identity guard 和 representative bool/uint32/double path matrix coverage 和 storage-width int8/int16/int64/uint8/uint16/uint64/float32 path matrix coverage；08-S6H..S6K/10-S4Z29..Z32 已提供 request/token public generic type object、C 级 `MakeGenericTypeObject` 入口及完整本地 compound argument 对象化；08-S6L/10-S4Z33 已提供未收集 reference-class 的解释器普通对象上下文和回读入口；08-S6M/10-S4Z34 已让 11-S5 GenericParam owner/index 解析到实例 concrete argument type object；08-S6N/10-S4Z35 已让 VM call-info 携带 GC-safe instance generic context，支持回读与 GenericParam substitution，并覆盖重用清零和 compact rewrite；public generic method reflection object、完整 recursive/signature-derived semantic field type binding、完整 primitive raw child matrix/signature-derived binding、完整 `FieldInfo` 行为、TypeRef 跨模块 provider signature binding、脚本对象级 generic 构造和 generic method execution 仍待后续 |
| 10-S5 | 保留注解（@reflectable/@dynamically_accessed/@dynamic_dependency/@requires_unreferenced_code）驱动 12（§5） | 🚧 2026-06-30 部分完成：10-S5A/12-S5A 已复用现有 compile-time decorator metadata 作为首个 `@reflectable` 承载面；函数 metadata 中 `reflectable: true` 会作为 reflection annotation root 注入 12 的 reachability graph，使 otherwise-unreachable function 在 opt-in code stripping 后仍保留，并输出 `code_stripping.annotationRoots`/`annotationRoot[]` 诊断；10-S5B/12-S5B 已复用 function decorator metadata 中的 `requiresUnreferencedCode: true` 作为首个 `@requires_unreferenced_code` carrier，retained caller 静态调用该 callee 时输出 `trim_warnings.annotationCount` 与逐条 `trim_warning.annotation[] reason=requires-unreferenced-code` marker；10-S5C/12-S5C 已读取同一 metadata 中的 `requiresUnreferencedCodeReason` 字符串，并在 warning marker 中追加 quoted/escaped `message="..."`；10-S5D/12-S5D 已读取 function decorator metadata 中的 `dynamicDependencyFunctionIndex`，把目标 flat function 作为 reflection annotation root 注入 12 的 reachability graph，使 otherwise-unreachable function 在 opt-in code stripping 后保留；10-S5F/12-S5E 已读取 `dynamicDependencyMethodToken` MEMBER_DEF token，并经 root module typed exported symbols 解析为当前模块 exported function flat index 后注入同一 annotation root；10-S5G/12-S5F 已读取 `dynamicDependencyMethodName` string，并经 root module typed exported symbols 的 exported function name 解析为当前模块 exported function flat index 后注入同一 annotation root；10-S5H/12-S5G 已读取可选 `dynamicDependencyMethodSignatureHash` uint64，与 `dynamicDependencyMethodName` 一起按 exported function name + signatureHash 唯一解析 root module typed exported symbols，重复同名且未提供 signature hash 的 metadata 不再静默选择第一个；10-S5I/12-S5H 已将 `dynamicDependencyMethodToken` 的当前模块解析扩展为 root module typed function symbols，不再要求 `exportKind == FUNCTION`，因此非导出 method token 也可把 callable child 注入同一 annotation root；10-S5J/12-S5I 已读取当前模块 `dynamicDependencyTypeLayoutId` uint32 metadata，把目标 generated type-layout id 加入独立 type-layout root 集合，使 owning function 被裁剪时对应 `SZrTypeLayout` descriptor 和 code-registration `typeLayouts[]` entry 仍可保留；10-S5K/12-S5J 已读取当前模块 `dynamicDependencyTypeToken` uint32 metadata，把 embedded zrp TypeDef/TypeSpec row 的 `typeLayoutId` 映射为同一 type-layout root，并让 root-only `zr_aot_type_layout_tokens[]` 回填对应 TYPE_DEF/TYPE_SPEC token；10-S5L/12-S5K 已读取当前模块 `dynamicDependencyFieldToken` uint32 metadata，把 embedded zrp FieldDef row 映射为 owner TypeDef `typeLayoutId` 与 field `typeLayoutId` 两个 type-layout roots；12-S7ZU/10-S5E 已提供 writer-level annotation warning suppression，使 `requiresUnreferencedCode` warning 可转入 suppressed count 且不输出逐条 marker；`@dynamically_accessed` 数据流、`@dynamic_dependency` 的跨模块规则、字段值读写/FieldInfo 完整行为、TypeRef/跨模块 type token、attribute/annotation-driven warning suppression/promotion、类型/成员级 DESCRIPTION 提升和未注解反射 warning 仍待后续 |

> 2026-07-19 02:56:02 +08:00 状态补记：10-S4 主行末尾的 generic method execution 缺口已由
> 10-S4Z39 与 10-S4Z42 关闭本地 C API 执行链；当前仍开放的是脚本对象级 generic 构造/`MakeGenericMethod`、
> 跨模块 method binding 和完整 public generic method reflection surface。

> 2026-07-19 03:53:01 +08:00 状态补记：10-S4Z43 已关闭 public generic method definition/parameter object
> 的第一条完整表面；当前仍开放 constructed generic method object、type-argument -> MethodSpec 精确匹配、脚本
> `MakeGenericMethod`、跨模块 method binding 和完整 managed method reflection 行为。

## 8. 不变量校验

- **B 纯降级**：反射是边界能力，invoker = 边界 marshaling；typed 函数体本身不含反射代码。
- **C 单一真相**：偏移/签名/类型实参全部读唯一 layout + token 记录，反射层不另存。
- **D 环境隔离**：invoker 是独立边界函数，与 typed 函数体物理分离（可被 `07`§9 grep 排除）。
- 与 `12` 协同：反射级别即裁剪可达性的产物，注解是二者唯一接口。

## 状态与产出记录

> 落地每个阶段或切片时在此追加：时间戳 · 切片号 · 状态 · 完成项目 · RED/GREEN · 测试结果 · 备注。

- 2026-07-19 03:53:01 +08:00 · 10-S4Z43 / 08-S6V / 11-S5A public generic method definition object ·
  状态：10-S4 generic method reflection object 子切片完成；完整 10-S4、08-S6 与 11-S5 仍为部分完成。完成项目：
  public `ZrCore_Reflection_BuildGenericMethodDefinitionObject()` 消费精确 GenericParam owner range，创建 GC-managed
  `genericMethodDefinition` 和 `genericMethodParameter` objects；方法对象携带 method/declaring-type token、runtime、
  flags、signature blob 坐标、parameter/argument count 与参数数组，参数对象携带 owner token、logical/physical index、
  name offset、flags 与 constraint range。方法和参数 `name` 从 zrp string pool 建立，缺失池时保守回退占位名。
  RED 为两个缺失 API unresolved symbols；首轮 GREEN 25/0 + 25/0。review RED 以真实 string pool 得到 25/1，
  修复后动态反射 25/0。最终 GCC/Clang/MSVC 聚焦 CTest 各 6/6，GC 66/0、指令执行 31/0、指令表 95/0，
  本切片实现源诊断为空。产出：
  `tests/acceptance/2026-07-19-aot-08-s6v-10-s4z43-11-s5a-generic-method-definition-object.md`。
  分层记录：`docs/plans/aot/07-12-codegen/2026-07-19-08-s6v-10-s4z43-11-s5a.md`。
  备注：不构造 MethodSpec，不接受 type argument objects，不新增脚本方法分派；constructed method object、脚本
  `MakeGenericMethod`、跨模块 method binding 与 full-AOT reflection closure 仍开放。

- 2026-07-19 02:56:02 +08:00 · 10-S4Z42 / 08-S6U / 11-S2E local MethodSpec method-token/VM-function auto resolution ·
  状态：10-S4 MethodSpec reflection execution 子切片完成；完整 10-S4、08-S6 与 11-S2 仍为部分完成。完成项目：
  public `ZrCore_Reflection_InvokeInterpreterGenericMethodSpec()` 读取 MethodSpec view 的 underlying method token，
  通过 interpreter MethodDef binding view 将 row `functionIndex` 解析为 AOT-order VM function，并复用 Z39 的
  GenericParam arity、context materialization、pin/stack-anchor、argument staging 与 result restore。函数图 resolver
  按 root/constant/child 深度优先顺序并按 AOT identity 去重，动态 visited 容量只随实际节点增长；越界、
  `UINT32_MAX-1`、重复/缺失 MethodDef、native/无指令函数均拒绝。RED 为两个 MSVC unresolved symbols，GREEN
  动态泛型反射 24/0。最终 GCC/Clang/MSVC 聚焦 CTest 各 5/5，GC 66/0、指令执行 31/0、指令表 95/0，
  本切片源诊断为空。产出：
  `tests/acceptance/2026-07-19-aot-08-s6u-10-s4z42-11-s2e-methodspec-method-token-vm-function-resolution.md`。
  备注：本入口仅解析 attached runtime 的本地 MethodDef，不生成 MethodSpec code slot，不覆盖跨模块 method
  binding 或脚本对象级 `MakeGenericMethod`。

- 2026-07-19 01:39:29 +08:00 · 10-S4Z41 / 08-S6T / 11-S6J bound provider generic TypeSpec identity ·
  状态：10-S4 cross-module generic reflection consumer 子切片完成；完整 10-S4/08-S6/11-S6 仍为部分完成。
  完成项目：public resolver 从 requester metadata function 的现有 TypeSpec binding 定位 provider TypeSpec，校验
  requester/ref 与 provider/resolved token、paired signature、canonical hash、module signature hash，并逐字节重验
  两侧 TypeSpec signature view；成功对象继续由 provider runtime/layout registry 物化，未复制或猜测跨模块 layout。
  unbound/wrong/same provider、畸形 expected identity 与绑定后同 hash 签名字节漂移清 output 后拒绝。compatibility predicate 增加窄
  TypeSpec->TypeSpec canonical RID remap 规则，其他 token mismatch 不变。RED 为 reflection 24/1、compatibility
  17/1，修复 remap 后转 24/0、17/0；signature drift 追加 RED 24/1，exact blob compare 后最终 24/0。
  WSL GCC、Clang、MSVC 聚焦 CTest 各 4/4，GC 66/0、指令执行 31/0、指令表 95/0；变更文件无编译诊断。
  产出：`tests/acceptance/2026-07-19-aot-08-s6t-10-s4z41-11-s6j-bound-provider-generic-typespec-identity.md`。
  备注：无全局 registry、metadata 格式或 layout 合成；method-token/function 自动解析和脚本对象级
  `MakeGenericType`/generic method surface 仍开放。

- 2026-07-19 00:13:22 +08:00 · 10-S4Z40 / 08-S6S interpreter generic boxed value instance ·
  状态：10-S4 interpreter generic reflection consumer 子切片完成；完整 10-S4 与 08-S6 仍为部分完成。
  完成项目：interpreter-deopt + open struct prototype 创建既有 `ZR_OBJECT_INTERNAL_TYPE_STRUCT` boxed dynamic
  value；generic context getter、instance GenericParam resolver 和 resolved method invoke 接受 class/struct 两种
  实例。测试验证 context/TypeDef argument、`ZrCore_Value_Copy`/`ZrCore_Object_CloneStruct` 后不同对象 payload
  17/29 隔离，以及真实 VM 方法在活动 type context 中返回 int64 117。RED 为 23/1（struct 创建返回 null），
  GREEN 为 23/0。WSL GCC、Clang、MSVC 聚焦 CTest 各 3/3，共享 GC 各 66/0、指令执行各 31/0；变更文件无
  编译告警。产出：
  `tests/acceptance/2026-07-19-aot-08-s6s-10-s4z40-interpreter-generic-value-instance.md`。备注：复用既有
  dynamic value/clone/GC 语义，不生成 typed/AOT layout；未改 metadata 格式/runtime API，故不新增 11 状态。

- 2026-07-18 23:46:42 +08:00 · 10-S4Z39 / 08-S6R caller-resolved MethodSpec VM function execution ·
  状态：10-S4 MethodSpec reflection execution 子切片完成；完整 10-S4 与 08-S6 仍为部分完成。完成项目：
  public invoke 读取既有 MethodSpec view，验证 underlying method GenericParam 精确 arity、VM/non-vararg/fixed
  function 与 explicit argument count；内部 function/object-call 边界可同时注入独立 type/method contexts，旧
  type-only API 保持兼容。MethodSpec context 在 `PreCall` 后进入活动 frame，真实 VM identity function 解析
  GenericParam[1] 为 TypeRef 并返回 int64 109；错误 token/arity 清 result。RED 为 1 个 MSVC unresolved symbol；
  GREEN 为 22/0。WSL GCC、Clang、MSVC 聚焦 CTest 各 3/3，共享 GC 各 66/0、指令执行各 31/0。新增实现无
  GCC/Clang 告警，`object_call.c` 仅保留 2 个既有 Clang unused-helper 告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6r-10-s4z39-methodspec-vm-function-execution.md`。备注：当前 API
  接受 caller-resolved function，method-token/function 解析与脚本级 `MakeGenericMethod` 仍开放；未改 metadata
  格式/runtime API，故不新增 11 状态。

- 2026-07-18 23:13:13 +08:00 · 10-S4Z38 / 08-S6Q MethodSpec call-info context + method GenericParam substitution ·
  状态：10-S4 MethodSpec reflection consumer 子切片完成；完整 10-S4 与 08-S6 仍为部分完成。完成项目：
  `SZrCallInfo.interpreterGenericMethodContext` 独立承载 S6P context；public bind/get/resolver 提供活动 frame
  回读和 method-owned GenericParam 替换，并校验 metadata runtime、underlying method token、owner range、arity
  与 index。所有 call-info 初始化/复用路径清零该 carrier，GC mark/compact rewrite 覆盖活动 frame；失败 bind
  清除旧值。RED 为 3 个 MSVC unresolved symbols；GREEN 为 21/0。中间 21/2 是测试夹具 MethodDef owner table
  不完整，补齐真实 TypeDef/MethodDef/generic range 后通过，生产校验未放宽。WSL GCC、Clang、MSVC 聚焦
  CTest 各 3/3，共享 GC 各 66/0、指令执行各 31/0；本阶段文件无 GCC/Clang 告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6q-10-s4z38-methodspec-callinfo-context.md`。备注：尚未执行
  MethodSpec；未改 metadata 格式/runtime API，因此不新增 11 阶段状态。

- 2026-07-18 22:36:52 +08:00 · 10-S4Z37 / 08-S6P MethodSpec generic method context object ·
  状态：10-S4 MethodSpec reflection consumer 子切片完成；完整 10-S4 与 08-S6 仍为部分完成。完成项目：
  public builder 从 11-S5 MethodSpec view 物化 `genericMethodContext` reflection object，包含 MethodSpec token、
  underlying method token、完整 uint64 signature hash、runtime、generic flags 与 2 个 argument objects；primitive
  与 TypeRef 复用已有 recursive metadata-node builder。非 MethodSpec token/null state fail closed。RED 为 1 个 MSVC
  unresolved symbol；GREEN 为 20/0。中间 20/1 是测试 fixture 缺失非空 code registration，修正 fixture
  后通过，未改生产契约。WSL GCC、Clang、MSVC 聚焦 CTest 各 3/3，对象化实现无 GCC/Clang
  告警。产出：`tests/acceptance/2026-07-18-aot-08-s6p-10-s4z37-methodspec-generic-context-object.md`。
  备注：call-info method context、method GenericParam substitution 和 MethodSpec execution 仍开放；未改 metadata 格式/API。

- 2026-07-18 22:13:24 +08:00 · 10-S4Z36 / 08-S6O interpreter generic instance resolved VM method execution ·
  状态：10-S4 public/interpreter generic reflection consumer 子切片完成；完整 10-S4 与 08-S6 仍为部分完成。
  完成项目：public resolved-method invoke 校验 deopt instance 的 runtime/open-owner identity、VM function 与
  receiver + fixed arguments arity，wrong runtime/owner/arity fail closed 并清空 result。它复用现有对象调用的
  GC pin、stack anchor、argument staging 与 result restore，在 `PreCall` 与 `Execute` 间注入已固定 context。
  真实 VM identity 方法从活动帧解析 GenericParam[1] 并返回 int64 参数 73。RED 为 1 个 MSVC
  unresolved symbol；GREEN 为 19/0。WSL GCC、Clang、MSVC 最终聚焦 CTest 各 3/3，共享层 GC
  各 66/0、指令执行各 31/0。变更实现无新 GCC/Clang 告警，`object_call.c` 仅有 2 个既有
  Clang unused-helper 告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6o-10-s4z36-interpreter-generic-instance-vm-method-execution.md`。
  备注：本切片不物化/执行 MethodSpec 自身泛型方法，不改 metadata 格式/API。

- 2026-07-18 20:55:12 +08:00 · 10-S4Z35 / 08-S6N interpreter generic call-info context carrier ·
  状态：10-S4 public/interpreter generic reflection consumer 子切片完成；完整 10-S4 与 08-S6 仍为部分完成。
  完成项目：public bind/get/call-info substitution API 把 S6L 实例上下文放入 `SZrCallInfo` 的
  GC-visible `SZrTypeValue`，并复用 S6M 的 same-runtime/open-base/arity/owner/index 规则返回 concrete
  argument type object。非 VM call-info 与无效实例 fail closed；调用初始化、hot path 与 tail reuse 全部
  清零 carrier，GC mark/compact 维持对象存活与转发后地址。RED 为 3 个新入口导致 3 个 MSVC
  unresolved symbols；GREEN 为 18/0，包含 full GC 后 call-info 参数解析。WSL GCC、Clang、MSVC
  聚焦 CTest 各 3/3，共享层 GC 各 66/0、指令执行各 31/0，变更实现无 GCC/Clang 自身告警。
  产出：`tests/acceptance/2026-07-18-aot-08-s6n-10-s4z35-interpreter-generic-callinfo-context.md`。
  备注：这是 GC-safe call-frame context，不是 uncollected generic method execution 或 script-object 方法。

- 2026-07-18 19:48:30 +08:00 · 10-S4Z34 / 08-S6M interpreter GenericParam substitution lookup ·
  状态：10-S4 public/interpreter generic reflection consumer 子切片完成；完整 10-S4 与 08-S6 仍为部分完成。
  完成项目：public lookup 复用 11-S5 GenericParam owner/index metadata view，验证 instance type context 的
  `metadataRuntime`、`genericBaseToken`、`genericArgumentCount` 和 `genericArguments` 一致后，按 parameter index
  返回 concrete argument reflection type object；错误 owner、越界、非实例和被篡改边界 fail closed。解释器测试
  职责拆入 122 行专用 header，主测试降至 944 行。RED 为 1 个缺失 symbol；GREEN 为 17/0，WSL GCC、Clang、
  MSVC 三项 CTest 各 3/3，变更模块无 GCC/Clang 自身告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6m-10-s4z34-interpreter-generic-parameter-substitution.md`。
  备注：这是参数 type-object lookup，不是 generic method execution、call-frame context 或 script-object 方法。

- 2026-07-18 19:25:44 +08:00 · 10-S4Z33 / 08-S6L interpreter reference-class generic instance context ·
  状态：10-S4 public/interpreter generic reflection consumer 子切片完成；完整 10-S4 与 08-S6 仍为部分完成。
  完成项目：public revalidation 拒绝陈旧 generic carrier；解释器对象工厂仅接受重新解析仍为 deopt 的
  reference-class 请求和 class prototype，创建普通对象并把深拷贝 type object 作为实例拥有的 generic context；
  getter 可回读上下文，原型成员继续走既有对象查找。AOT route、struct/union prototype、prototype object 均
  fail closed，分配期间对象按 ignore-root 约束保护。RED 为两个入口缺失导致 2 个 MSVC unresolved symbols；
  GREEN 为 16/0，WSL GCC、Clang、MSVC 三项 CTest 各 3/3，三个实现文件无 GCC/Clang 自身告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6l-10-s4z33-interpreter-generic-reference-instance-context.md`。
  备注：这不是 generic substitution/execution，也未完成值类型、跨模块 identity 或脚本对象方法。

- 2026-07-18 18:56:26 +08:00 · 10-S4Z32 / 08-S6K / 11-S4BO token-only compound generic type object ·
  状态：10-S4 public generic reflection object 子切片完成；完整 10-S4/08-S6/11-S4 仍为部分完成。完成项目：
  token-only public generic object 递归覆盖当前编码器全部 compound type nodes；nested generic 通过完整签名节点跨度
  绑定真实本地 TypeSpec token，缺 record 时 fail closed。metadata binding 逻辑拆入独立 98 行模块，测试断言也从
  近 1000 行主场景文件拆出。RED 为 15 tests/2 failures；GREEN 为 15/0，WSL GCC、Clang、MSVC 三项 CTest
  各 3/3，变更模块无 GCC/Clang 自身告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6k-10-s4z32-11-s4bo-token-only-compound-generic-type-object.md`。
  备注：脚本对象方法、跨模块 identity 与解释器 generic substitution/execution 仍开放。

- 2026-07-18 18:34:53 +08:00 · 10-S4Z31 / 08-S6J token-only direct-argument generic type object ·
  状态：10-S4 public generic reflection object 子切片完成；完整 10-S4 与 08-S6 仍为部分完成。完成项目：
  token-only existing-TypeSpec carrier 可经同一 builder 重新校验并生成 public type object；metadata 中的 primitive
  与 direct TypeDef/TypeRef 参数进入 `genericArguments`，compound node 不做浅层降级。RED 为聚焦测试
  13 tests/1 failure；GREEN 为 13/0，WSL GCC、Clang、MSVC 三项 CTest 各 3/3，新模块无 GCC/Clang 自身告警。
  产出：`tests/acceptance/2026-07-18-aot-08-s6j-10-s4z31-token-only-direct-generic-type-object.md`。
  备注：token-only compound 参数、脚本对象级 `MakeGenericType`、跨模块 identity 与解释器执行仍开放。

- 2026-07-18 18:25:07 +08:00 · 10-S4Z30 / 08-S6I public MakeGenericType object entry ·
  状态：10-S4 public generic reflection construction 子切片完成；完整 10-S4 与 08-S6 仍为部分完成。
  完成项目：public C API 接受 open generic base token 和 recursive arguments，复用唯一 resolver 与 Z29 builder，
  一次返回 AOT 或 interpreter-deopt public type object；无重复身份/路由算法。RED 为 MSVC 链接缺失入口；GREEN 为
  聚焦目标 13/0，WSL GCC、Clang、MSVC 三项 CTest 各 3/3，模块无自身告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6i-10-s4z30-make-generic-type-object.md`。
  备注：脚本对象级方法、token-only 对象、解释器 generic substitution/execution 与跨模块 identity 仍开放。

- 2026-07-18 18:13:41 +08:00 · 10-S4Z29 / 08-S6H public constructed-generic type object ·
  状态：10-S4 public generic reflection object 子切片完成；完整 10-S4 与 08-S6 仍为部分完成。
  完成项目：public builder 将 request-resolved AOT/interpreter-deopt carrier 重新校验后物化为 `kind == "type"`
  对象，携带 route、base/TypeSpec token、layout、same-runtime pointer 与递归 `genericArguments`；对象图同步复制
  primitive/token/array/tuple/ownership/nullable/union descriptor，不持有调用方 borrowed 指针。新实现独立为 462 行
  `reflection_generic_type_object.c`，并用 GC ignore-root/父字段顺序保护分配中的对象。
  RED/GREEN：RED 为 MSVC 链接缺失 builder；GREEN 为聚焦目标 13/0，WSL GCC、Clang、MSVC 三项 CTest 各 3/3，
  新模块无自身编译告警。产出：
  `tests/acceptance/2026-07-18-aot-08-s6h-10-s4z29-public-generic-type-object.md`。
  备注：token-only object、`MakeGenericType` 用户入口、解释器参数替换/执行与跨模块 identity 仍待后续。

- 2026-07-03 05:30:18 +08:00 · 10-S5P / 12-S7ZZZC annotation warning source attribution ·
  状态：10-S5 trim annotation warning 的 source attribution 子切片完成；完整 `@dynamically_accessed` 数据流、
  `@dynamic_dependency` 跨模块规则、annotation/promotion policy、未注解反射 warning 和完整 trim analyzer 仍待后续。
  完成项目：`backend_aot_c_annotation_warnings.c` 现在对 visible `requires-unreferenced-code` warning 复用
  `backend_aot_exec_ir_debug_*_for_instruction(...)`，并在 `trim_warning.annotation[]` marker 中输出 caller
  `sourceFile`、`sourceLine/sourceLineEnd`、`sourceColumn/sourceColumnEnd`。reason 文本仍沿用既有 quoted/escaped
  `message="..."` 输出，callsite/global suppression 仍隐藏逐条 visible marker。
  RED/GREEN：RED 为 existing visible warning fixture 增加 source attribution 断言后 WSL GCC
  `aot_c_reflection_annotation_preserve` 失败 `Expected Non-NULL`，旧 marker 缺少 source file/line/column；source contract
  也因缺少 ExecIR source-location helper needle 失败。GREEN 后 WSL GCC/clang/Windows MSVC Debug annotation preserve 均 12/0，
  source contracts 均 24/0。
  产出：`tests/acceptance/2026-07-03-aot-10-s5p-12-s7zzzc-annotation-warning-source-attribution.md`。
  备注：本切片只让 retained static caller -> annotated callee 的 annotation warning 可定位到源文件/行列；不声明完整
  annotation/dataflow/warning 策略、未注解反射 warning 或完整 trim analyzer 完成。

- 2026-07-03 05:13:46 +08:00 · 10-S5O / 12-S7ZZZB callsite `requires-unreferenced-code` annotation warning suppression ·
  状态：10-S5 trim annotation warning 的调用方级抑制子切片完成；完整 `@dynamically_accessed` 数据流、
  `@dynamic_dependency` 跨模块规则、annotation/promotion policy、未注解反射 warning 和完整 trim analyzer 仍待后续。
  完成项目：function decorator metadata 新增消费 `suppressRequiresUnreferencedCodeWarning`；调用方带该布尔标记时，
  其静态调用 `requiresUnreferencedCode` callee 产生的 `requires-unreferenced-code` warning 不再输出为 visible
  `trim_warning.annotation[]`，而是计入 `trim_warnings.annotationSuppressedCount`。全局 writer-level
  `suppressAnnotationWarnings` 保持更高优先级，会把 visible 与 callsite-suppressed warnings 都合并为 suppressed count。
  RED/GREEN：RED 为
  `test_aot_c_suppresses_trim_annotation_warning_from_callsite_annotation` 在 WSL GCC 失败 `Expected Non-NULL`，
  generated C 仍输出 visible warning。GREEN 后 WSL GCC/clang/Windows MSVC Debug
  `aot_c_reflection_annotation_preserve` 均 12/0，`aot_c_source_contracts` 均 24/0。
  产出：`tests/acceptance/2026-07-03-aot-10-s5o-12-s7zzzb-callsite-annotation-warning-suppression.md`。
  备注：本记录只关闭 requires-unreferenced-code warning 的调用方级抑制；不声明完整 10-S5、12-S5/12-S7 或
  07~12 总目标完成。

- 2026-07-01 14:05:25 +08:00 · 10-S4Z28 / 11-S4BN FieldInfo nested primitive POD storage-width path matrix coverage ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` recursive nested primitive path storage-width matrix coverage
  子切片完成；managed `FieldInfo.GetValue/SetValue` method surface、完整 signature-derived field type binding 仍未关闭。
  完成项目：`tests/module/test_reflection_token_resolve.c` 在同一 S4Z25/S4Z26 focused fixture 中新增
  `configure_nested_primitive_path_field_sizes()`，并把 two-level `{0,0}` nested primitive path 扩展到
  int8、int16、int64、uint8、uint16、uint64 和 float32 raw child read/write。该覆盖复用 object adapter、nested traversal、
  leaf layout identity guard 和 shared primitive POD guard。未新增 public API 或 production code。
  RED/GREEN：coverage GREEN；新增 storage-width matrix 后 Windows MSVC Debug focused `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均构建
  `zr_vm_reflection_token_resolve_test`、`zr_vm_metadata_runtime_query_test`、`zr_vm_metadata_runtime_typespec_layout_test`；
  focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z28-fieldinfo-nested-primitive-pod-width-matrix.md`。
  备注：本记录只关闭 nested primitive path storage-width int8/int16/int64/uint8/uint16/uint64/float32 coverage；不声明
  signature-derived binding、完整 managed FieldInfo methods、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 14:00:03 +08:00 · 10-S4Z27 / 11-S4BM FieldInfo nested primitive POD representative path matrix coverage ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` recursive nested primitive path representative matrix coverage
  子切片完成；managed `FieldInfo.GetValue/SetValue` method surface、完整 primitive width/signature-derived field type binding
  仍未关闭。
  完成项目：`tests/module/test_reflection_token_resolve.c` 扩展 S4Z25/S4Z26 focused fixture，在同一 two-level `{0,0}`
  nested primitive path 上覆盖 bool、uint32 和 double raw child read/write。测试通过动态调整 leaf/intermediate layout
  byte-size/align，证明 object adapter、nested traversal、leaf layout identity guard 和 shared primitive POD guard 可复用到
  代表性非 INT32 类型。未新增 public API 或 production code。
  RED/GREEN：coverage GREEN；新增矩阵覆盖后 Windows MSVC Debug focused `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均构建
  `zr_vm_reflection_token_resolve_test`、`zr_vm_metadata_runtime_query_test`、`zr_vm_metadata_runtime_typespec_layout_test`；
  focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z27-fieldinfo-nested-primitive-pod-path-matrix.md`。
  备注：本记录只关闭 nested primitive path representative bool/uint32/double coverage；不声明完整 primitive 宽度矩阵、
  signature-derived binding、完整 managed FieldInfo methods、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 13:51:37 +08:00 · 10-S4Z26 / 11-S4BL FieldInfo nested primitive POD leaf layout identity guard ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` recursive nested primitive raw child guard 子切片完成；
  managed `FieldInfo.GetValue/SetValue` method surface、完整 primitive matrix/signature-derived field type binding 仍未关闭。
  完成项目：`reflection_field_value_nested.c` 的 nested primitive path read/write leaf 分支现在要求 raw primitive child
  `typeLayoutIndex == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`，并在该检查之后才委托 shared primitive POD guard。该规则只作用于
  nested leaf，不改变 top-level primitive FieldInfo path 的 field-type identity 读取方式。新增 focused fixture 覆盖 leaf
  `typeLayoutIndex = 42u` 时 read/write 均拒绝，且原始 INT32 字节保持 `-12345`。
  RED/GREEN：RED 为 Windows MSVC Debug focused `reflection_token_resolve` 30 tests / 1 failure，
  `Expected FALSE Was TRUE`，证明带 child layout identity 的 leaf 被误接受；GREEN 后 Windows focused
  `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均构建
  `zr_vm_reflection_token_resolve_test`、`zr_vm_metadata_runtime_query_test`、`zr_vm_metadata_runtime_typespec_layout_test`；
  focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z26-fieldinfo-nested-primitive-pod-leaf-layout-guard.md`。
  备注：本记录只关闭 nested primitive raw child leaf layout identity guard；不声明完整 primitive 宽度矩阵、
  signature-derived binding、完整 managed FieldInfo methods、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 13:37:45 +08:00 · 10-S4Z25 / 11-S4BK FieldInfo nested inline primitive POD path read/write ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` recursive nested inline primitive raw child first-contract 子切片完成；
  managed `FieldInfo.GetValue/SetValue` method surface、完整 primitive matrix/signature-derived field type binding 仍未关闭。
  完成项目：`reflection.h` 新增 `ZrCore_Reflection_ReadFieldInfoTokenNestedPathPrimitiveValue()` /
  `ZrCore_Reflection_WriteFieldInfoTokenNestedPathPrimitiveValue()` 及 object adapter；新增
  `reflection_field_value_primitive.{h,c}`，把 primitive POD raw load/store guard 从 `reflection_field_value.c` 拆出共享；
  `reflection_field_value_nested.c` 复用 multi-level path traversal，在 leaf raw child 上用 shared primitive guard 读写。
  正向覆盖 two-level `{0,0}` INT32 raw child read/write；拒绝 short storage、zero-length path、final child out-of-range、
  missing intermediate registered layout、中间 VALUE_SLOT/GC/ownership flags、leaf VALUE_SLOT flag、primitive byte-size mismatch、
  bool write mismatch，并验证失败写入不破坏原始字节。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（新增测试调用缺失的
  `ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue()` /
  `ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue()`，C4013 + LNK2019）；GREEN 后 Windows focused
  `reflection_token_resolve` 30/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 30/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z25-fieldinfo-nested-primitive-pod-path-read-write.md`。
  备注：本记录只关闭 retained metadata 下代表性 INT32 primitive raw child path read/write 的最小边界；不声明
  完整 primitive width/signature-derived matrix、完整 managed FieldInfo methods、trim analyzer、DESCRIPTION promotion 或完整 metadata sweep 完成。

- 2026-07-01 13:14:27 +08:00 · 10-S4Z24 / 11-S4BJ FieldInfo nested inline VALUE_SLOT path write ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` recursive nested inline field path write first-contract 子切片完成；
  primitive raw child marshaling、managed `FieldInfo.GetValue/SetValue` method surface 和 full recursive field type binding
  仍未关闭。
  完成项目：`reflection.h` 新增 `ZrCore_Reflection_WriteFieldInfoTokenNestedPathValue()` 与
  `ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue()`；`reflection_field_value.c` 作为 FieldInfo object/token adapter，
  委托 `reflection_field_value_nested.c` 的 path writer。path traversal 复用 S4Z23 的 child layout resolver，逐级消费
  `SZrTypeLayoutField.typeLayoutIndex` 与 `ZrCore_MetadataRuntime_ResolveTypeLayout()`，要求中间 child 非
  `VALUE_SLOT`/GC/ownership、byte size 与 child layout 一致且 child layout 为 struct/union，最终节点只接受
  `VALUE_SLOT` child 并用 `ZrCore_Value_Copy()` 写入。focused fixture 覆盖合法两级 `{0,0}` path、short storage、
  zero-length path、final child out-of-range、missing intermediate layout 和中间 child GC/ownership flags 拒绝，
  并验证 replacement/drop 会释放旧 unique-owned owner、destination ownership metadata 归一且 plain new source 不被持有。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 29/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 29/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z24-fieldinfo-nested-value-slot-path-write.md`。
  备注：本切片只关闭 retained metadata 下 multi-level nested value-slot path write 的最小边界；不声明
  primitive raw child read/write、完整 managed FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、
  完整 trim analyzer 或 metadata sweep 完成。`reflection_field_value.c` 按项目 line check 已到约 999 行；
  nested traversal 仍在 `reflection_field_value_nested.{h,c}`，后续继续扩展 FieldInfo 行为时应优先拆 adapter/test fixture。

- 2026-07-01 13:00:18 +08:00 · 10-S4Z23 / 11-S4BI FieldInfo nested inline VALUE_SLOT path read ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` recursive nested inline field path read first-contract 子切片完成；
  nested path write、primitive raw child marshaling、managed `FieldInfo.GetValue/SetValue` method surface 和 full recursive
  field type binding 仍未关闭。
  完成项目：`reflection.h` 新增 `ZrCore_Reflection_ReadFieldInfoTokenNestedPathValue()` 与
  `ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue()`；`reflection_field_value.c` 作为 FieldInfo object/token adapter，
  新增 `reflection_field_value_nested.{h,c}` 承接 recursive inline layout traversal。path traversal 逐级消费
  `SZrTypeLayoutField.typeLayoutIndex` 与 `ZrCore_MetadataRuntime_ResolveTypeLayout()`，要求中间 child 非 `VALUE_SLOT`/GC/ownership、
  byte size 与 child layout 一致且 child layout 为 struct/union，最终节点只接受 `VALUE_SLOT` child 并复制为 `SZrTypeValue`。
  focused fixture 覆盖合法两级 `{0,0}` path、zero-length path、final child out-of-range、missing intermediate layout 和中间
  child GC/ownership flags 拒绝。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 28/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 28/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z23-fieldinfo-nested-value-slot-path-read.md`。
  备注：本切片只关闭 retained metadata 下 multi-level nested value-slot path read 的最小边界；不声明 path write、
  primitive raw child read/write、完整 managed FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、
  完整 trim analyzer 或 metadata sweep 完成。`reflection_field_value.c` 一度超过 1100 行，已把 nested traversal helper
  拆到 `reflection_field_value_nested.{h,c}`，当前约 935 行；`tests/module/test_reflection_token_resolve.c` 仍是大型反射
  token fixture，后续最小测试拆分边界为独立 FieldInfo inline-storage fixture/test target。

- 2026-07-01 12:40:09 +08:00 · 10-S4Z22 / 11-S4BH FieldInfo nested inline VALUE_SLOT write ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` recursive nested inline field write first-contract 子切片完成；
  完整 recursive path API、primitive raw child marshaling 和 managed `FieldInfo.GetValue/SetValue` method surface 仍未关闭。
  完成项目：`reflection.h` 新增 `ZrCore_Reflection_WriteFieldInfoTokenNestedValue()` 与
  `ZrCore_Reflection_WriteFieldInfoObjectNestedValue()`；`reflection_field_value.c` 在 S4Z21 nested read 的 layout-index
  resolver 上补 write path，对 nested `VALUE_SLOT` child 调用 `ZrCore_Value_Copy()` 写入目标 slot。新增 focused fixture
  验证 short storage、out-of-range index 与外层 GC/ownership flags 均拒绝，合法 nested child write 会把 unique-owned old
  string 释放到 strong ref 0，并把 destination slot 替换为 plain new string、ownership metadata 归一为空。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_WriteFieldInfoObjectNestedValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 27/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 27/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z22-fieldinfo-nested-value-slot-write.md`。
  备注：本切片只关闭 nested value-slot child write 的最小 layout-index 边界；不声明 full recursive nested marshaling、
  primitive raw child write、完整 managed FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、
  完整 trim analyzer 或完整 metadata sweep 完成。`reflection_field_value.c` 现为约 915 行，仍保持单一 FieldInfo value
  boundary 责任，本切片暂不拆分；后续最小生产拆分边界为 nested field layout read/write helper。大型测试文件暂不拆分，
  后续最小测试拆分边界为独立 FieldInfo inline-storage fixture/test target。

- 2026-07-01 12:24:56 +08:00 · 10-S4Z21 / 11-S4BG FieldInfo nested inline VALUE_SLOT read ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` recursive nested inline field read first-contract 子切片完成；
  完整 recursive path API、nested write、primitive raw child marshaling 和 managed `FieldInfo.GetValue/SetValue`
  method surface 仍未关闭。
  完成项目：`reflection.h` 新增 `ZrCore_Reflection_ReadFieldInfoTokenNestedValue()` 与
  `ZrCore_Reflection_ReadFieldInfoObjectNestedValue()`；`reflection_field_value.c` 在既有 FieldInfo identity/FieldDef resolver 上，
  复用 inline aggregate borrowed-view 判定后按 `SZrTypeLayoutField` index 读取 nested `VALUE_SLOT` child，并用
  `ZrCore_Value_Copy()` 返回 `SZrTypeValue`。新增 focused fixture 验证 short storage、out-of-range index 与外层
  GC/ownership flags 均拒绝，合法 nested child read 返回 int `314159`。
  RED/GREEN：RED 为 Windows MSVC Debug 构建失败（`ZrCore_Reflection_ReadFieldInfoObjectNestedValue` 未定义/LNK2019）；
  GREEN 后 Windows focused `reflection_token_resolve` 26/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 26/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z21-fieldinfo-nested-value-slot-read.md`。
  备注：本切片只关闭 nested value-slot child read 的最小 layout-index 边界；不声明 full recursive nested marshaling、
  完整 managed FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、
  完整 trim analyzer 或完整 metadata sweep 完成。大型测试文件暂不拆分，后续最小拆分边界为独立 FieldInfo
  inline-storage fixture/test target。

- 2026-07-01 12:09:54 +08:00 · 10-S4Z20 / 11-S4BF FieldInfo inline aggregate replacement/drop borrowed-source write coverage ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` inline aggregate replacement/drop coverage 子切片完成；
  完整 nested field 解构、recursive field API 和 managed `FieldInfo.SetValue` method surface 仍未关闭。
  完成项目：新增 `test_reflection_writes_field_info_object_inline_struct_drops_replaced_owned_value_field()`，
  以 `FIELD_COPY/FIELD_DROP` inline aggregate + nested `SZrTypeValue` field 覆盖 replacement/drop：destination
  先持有 unique-owned old string，source borrowed native pointer 指向 plain new string，object-level write 后验证
  old strong ref 释放、destination 复制 new string 且 ownership metadata 清空。生产代码未改动，复用 S4Z19 write path、
  `ZrCore_TypeLayout_CopyInline()` 和 `ZrCore_Value_Copy()`。
  RED/GREEN：coverage GREEN；新增覆盖后 Windows focused `reflection_token_resolve` 直接通过 25/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 25/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z20-fieldinfo-inline-aggregate-replacement-drop-write.md`。
  备注：本切片只补齐 inline aggregate field-copy write 的 replacement/drop 防退化覆盖；不声明 recursive nested
  field marshaling、完整 managed FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、
  完整 trim analyzer 或完整 metadata sweep 完成。大型测试文件暂不拆分，后续最小拆分边界为独立 FieldInfo
  inline-storage fixture/test target。

- 2026-07-01 11:52:49 +08:00 · 10-S4Z19 / 11-S4BE FieldInfo inline aggregate field-copy borrowed-source write ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` nested inline field marshaling support 子切片完成；
  完整 nested field 解构、destination drop/replacement lifecycle 和 managed `FieldInfo.SetValue` method surface 仍未关闭。
  完成项目：`reflection_field_value.c` 的 non-primitive write path 现在只在 shared inline aggregate borrowed-view
  判定通过后接受 native-pointer source，并委托 `ZrCore_TypeLayout_CopyInline()` 写入字段 storage；POD/raw-copy layout
  仍走 raw-copy 分支，non-blittable `FIELD_COPY` layout 走逐字段 copy。null source、scalar source、GC/ownership flagged
  字段和 unsupported copy layout 仍拒绝。
  RED/GREEN：RED 为 Windows focused `reflection_token_resolve` 将 non-blittable field-copy source 写入期望改为成功后
  失败 1/24（`Expected TRUE Was FALSE`）；GREEN 后 Windows focused `reflection_token_resolve` 24/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 24/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z19-fieldinfo-inline-aggregate-field-copy-write.md`。
  备注：本切片只关闭 inline aggregate borrowed-source write 对 field-copy layout 的核心 copy 语义；不声明
  recursive nested field marshaling、完整 managed FieldInfo methods、`@dynamically_accessed` dataflow、
  DESCRIPTION promotion、完整 trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 11:34:10 +08:00 · 10-S4Z18 / 11-S4BD FieldInfo inline aggregate borrowed-source write ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` nested inline field marshaling second-contract 子切片完成；
  完整 nested field 解构、field-copy/drop 语义和 managed `FieldInfo.SetValue` method surface 仍未关闭。
  完成项目：`reflection_field_value.c` 的 non-primitive write path 复用 shared field type-node reader；当目标是
  non-`VALUE_SLOT`、non-GC/non-ownership、validated `FIELD_SIG(TYPE_DEF/TYPE_REF)`、resolved struct/union layout
  byte size 匹配且 blittable 的 inline aggregate 字段时，接受非空 `ZR_VALUE_TYPE_NATIVE_POINTER` source，并按字段
  byte size `memcpy` 到调用方 inline storage。null native pointer、scalar value、non-blittable layout 与
  GC/ownership flagged 字段均保持拒绝。
  RED/GREEN：RED 为 Windows focused `reflection_token_resolve` 在 inline struct fixture 增加 native-pointer
  source 写入期望后失败 1/24（`Expected TRUE Was FALSE`）；GREEN 后 Windows focused `reflection_token_resolve` 24/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 24/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z18-fieldinfo-inline-aggregate-borrowed-source-write.md`。
  备注：本切片只关闭 blittable inline aggregate 的 borrowed-source byte copy；不声明 recursive nested field
  marshaling、non-blittable field-copy/drop、`@dynamically_accessed` dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 11:26:41 +08:00 · 10-S4Z17 / 11-S4BC FieldInfo inline struct borrowed view ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` nested inline field marshaling first-contract 子切片完成；
  完整 nested field 解构/写回、managed `FieldInfo.GetValue/SetValue` method surface 和完整 metadata sweep 仍未关闭。
  完成项目：`reflection_field_value.c` 抽出 shared field type-node reader；非 `VALUE_SLOT`、非 GC/ownership、
  validated `FIELD_SIG(TYPE_DEF/TYPE_REF)` 且 resolved field type layout byte size 匹配的 struct/union inline 字段，
  读取时返回 `ZR_VALUE_TYPE_NATIVE_POINTER` borrowed view，指向调用方 inline storage 的字段地址。写入路径仍只支持
  `VALUE_SLOT` 或 primitive POD，非 primitive inline struct write 保持拒绝。
  RED/GREEN：RED 为 Windows focused `reflection_token_resolve` 在新增
  `test_reflection_reads_field_info_object_inline_struct_borrowed_view()` 后失败 1/24（`Expected TRUE Was FALSE`）；
  GREEN 后 Windows focused `reflection_token_resolve` 24/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 24/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z17-fieldinfo-inline-struct-borrowed-view.md`。
  备注：本切片只关闭 inline aggregate borrowed-view read boundary；不声明 recursive nested field marshaling、
  struct copy/write、`@dynamically_accessed` dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 11:15:23 +08:00 · 10-S4Z16 / 11-S4BB FieldInfo object primitive POD coverage ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` object-level primitive POD raw inline 字段读写覆盖子切片完成；
  nested field marshaling、完整 managed `FieldInfo.SetValue`/method surface 和完整 metadata sweep 仍未关闭。
  完成项目：新增 focused fixture 通过 `ZrCore_Reflection_ReadFieldInfoObjectValue()` /
  `ZrCore_Reflection_WriteFieldInfoObjectValue()` 读取 int32 raw primitive field、拒绝 bool 写入并验证原始
  bytes 保持、再写入 int `4096` 并读回。生产代码未新增 API，复用 S4Z14/S4Z15 object adapter 到既有
  token-driven primitive POD path。
  RED/GREEN：coverage GREEN；现有 adapter 已满足该 primitive POD 路径，Windows focused
  `reflection_token_resolve` 23/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 23/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z16-fieldinfo-object-primitive-pod.md`。
  备注：本切片只补 object-level primitive POD coverage；不声明 nested struct/POD field marshaling、
  `@dynamically_accessed` dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 11:06:55 +08:00 · 10-S4Z15 / 11-S4BA FieldInfo object value write adapter ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` object-level `VALUE_SLOT` 写入 adapter 子切片完成；
  nested field marshaling、完整 managed `FieldInfo.SetValue`/method surface 和完整 metadata sweep 仍未关闭。
  完成项目：`reflection.h` 新增 `ZrCore_Reflection_WriteFieldInfoObjectValue()`；`reflection_field_value.c`
  复用 `FieldInfo.metadataRuntime` native pointer 与 `metadataToken` int carrier 解析 same-runtime identity，
  再委托既有 `ZrCore_Reflection_WriteFieldInfoTokenValue()`。新增 focused fixture 验证 null state/null
  FieldInfo/null storage/null value/short storage 失败、失败后原值保持，以及成功通过 object adapter 写入
  inline `VALUE_SLOT` int。
  RED/GREEN：RED 为 Windows MSVC Debug build 链接失败，缺少
  `ZrCore_Reflection_WriteFieldInfoObjectValue`；GREEN 后 Windows focused `reflection_token_resolve` 22/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 22/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z15-fieldinfo-object-value-write.md`。
  备注：本切片只关闭 object-level write adapter；不声明 nested struct/POD field marshaling、
  `@dynamically_accessed` dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 10:52:48 +08:00 · 10-S4Z14 / 11-S4AZ FieldInfo object value read adapter ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` object-level `VALUE_SLOT` 读取 adapter 子切片完成；
  object-level write、nested field marshaling、完整 `FieldInfo` 行为和完整 metadata sweep 仍未关闭。
  完成项目：`reflection.h` 新增 `ZrCore_Reflection_ReadFieldInfoObjectValue()`；`reflection_field_value.c`
  从 `FieldInfo.metadataRuntime` native pointer 与 `metadataToken` int carrier 提取 same-runtime identity，
  再委托既有 `ZrCore_Reflection_ReadFieldInfoTokenValue()`，并在读取 object 字段时使用 native-call pinning。
  新增 focused fixture 验证 null state/null FieldInfo/short storage 失败和成功读回 inline `VALUE_SLOT` int。
  RED/GREEN：RED 为 Windows MSVC Debug build 链接失败，缺少
  `ZrCore_Reflection_ReadFieldInfoObjectValue`；GREEN 后 Windows focused `reflection_token_resolve` 21/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 21/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z14-fieldinfo-object-value-read.md`。
  备注：本切片只关闭 read-only object adapter；不声明 `SetValue`、nested struct/POD field marshaling、
  `@dynamically_accessed` dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 10:36:14 +08:00 · 10-S4Z13 / 11-S4AY FieldInfo metadata runtime carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` object 的 same-runtime metadata runtime identity
  carrier 子切片完成；object-level `FieldInfo.GetValue/SetValue` method surface、nested field marshaling、
  cross-module provider binding 和完整 `FieldInfo` 行为仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在在既有 `metadataToken` 旁写入
  `metadataRuntime` native pointer；该值指向当前 attached `SZrMetadataRuntime`，让后续 FieldInfo object-level
  字段读写方法可从对象自身恢复同一 runtime/token identity。本切片不新增 public API，不改变 metadata ABI。
  RED/GREEN：RED 为 focused `FieldInfo` object fixture 新增 `metadataRuntime` native-pointer 断言后，
  Windows MSVC Debug `reflection_token_resolve` 20 个测试失败 1 个，缺少该字段；GREEN 后同一 focused run 20/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 20/0、`metadata_runtime_query`
  24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z13-fieldinfo-metadata-runtime-carrier.md`。
  备注：本切片只关闭 `metadataRuntime` carrier；不声明 object-level FieldInfo methods、nested struct marshaling、
  `@dynamically_accessed` dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整 metadata sweep 完成。

- 2026-07-01 10:21:28 +08:00 · 10-S4Z12 / 11-S4AX FieldInfo primitive POD float32 precision guard ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` primitive POD raw inline float32 字段 precision/no-loss
  写入守卫子切片完成；nested field marshaling、object-level `FieldInfo.GetValue/SetValue` method
  surface、cross-module provider binding 和完整 `FieldInfo` 行为仍未关闭。
  完成项目：`reflection_field_value_float32_can_store_losslessly()` 先拒绝 NaN，再拒绝 `|value| > FLT_MAX`，
  最后要求 `(TZrDouble)(TZrFloat32)value == value`；不能无损 round-trip 的 double source 会在 cast 和
  `memcpy` 前失败，并保持原 raw float32 bytes 不变。本切片不新增 public API。
  RED/GREEN：RED 为新增 precision-loss raw write 用例后 Windows MSVC Debug focused run 失败 1/20，
  `Expected FALSE Was TRUE`；GREEN 后同一 focused run 20/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 20/0、`metadata_runtime_query` 24/0、
  `metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z12-fieldinfo-primitive-pod-float32-precision-guard.md`。
  备注：本切片只关闭 primitive POD float32 precision/no-loss guard，不声明 nested struct marshaling、
  完整 FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整
  metadata sweep 完成。

- 2026-07-01 10:11:20 +08:00 · 10-S4Z11 / 11-S4AW FieldInfo primitive POD float32 NaN guard ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` primitive POD raw inline float32 字段 NaN 写入守卫子切片完成；
  float32 precision policy、nested field marshaling、object-level `FieldInfo.GetValue/SetValue` method
  surface、cross-module provider binding 和完整 `FieldInfo` 行为仍未关闭。
  完成项目：`reflection_field_value_float32_is_in_range()` 先用 self-inequality 拒绝 NaN，再执行既有 `FLT_MAX`
  range guard；NaN source 会在 float32 cast 和 `memcpy` 前失败，并保持原 raw float32 bytes 不变。本切片不新增 public API。
  RED/GREEN：RED 为新增 NaN raw write 用例后 Windows MSVC Debug focused run 失败 1/19，`Expected FALSE Was TRUE`；
  GREEN 后同一 focused run 19/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 19/0、`metadata_runtime_query` 24/0、
  `metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z11-fieldinfo-primitive-pod-float32-nan-guard.md`。
  备注：本切片只关闭 primitive POD float32 NaN guard，不声明 float32 precision semantics、nested struct
  marshaling、完整 FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整
  metadata sweep 完成。

- 2026-07-01 09:58:56 +08:00 · 10-S4Z10 / 11-S4AV FieldInfo primitive POD float32 range guard ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` primitive POD raw inline float32 字段写入范围守卫子切片完成；
  float32 NaN/precision policy、nested field marshaling、object-level `FieldInfo.GetValue/SetValue` method
  surface、cross-module provider binding 和完整 `FieldInfo` 行为仍未关闭。
  完成项目：`reflection_field_value.c` 在 `ZR_VALUE_TYPE_FLOAT` raw primitive 写入路径中以 `FLT_MAX`
  守卫 float32 storage range；double source 若大于 `FLT_MAX` 或小于 `-FLT_MAX`，会在 float32 cast 和
  `memcpy` 前被拒绝，失败写入保持原 raw inline storage bytes。NaN 与精度收窄策略不在本切片声明。
  RED/GREEN：RED 为新增 `test_reflection_rejects_out_of_range_field_info_primitive_pod_float32_writes` 后，
  Windows MSVC Debug focused run 18 个测试中该用例失败（`Expected FALSE Was TRUE`）；GREEN 后同一 focused
  run 通过 18/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 18/0、`metadata_runtime_query` 24/0、
  `metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z10-fieldinfo-primitive-pod-float32-range-guard.md`。
  备注：本切片只关闭 primitive POD float32 storage range guard，不声明 float32 NaN/precision semantics、nested struct
  marshaling、完整 FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整
  metadata sweep 完成。

- 2026-07-01 09:49:10 +08:00 · 10-S4Z9 / 11-S4AU FieldInfo primitive POD integer range guard ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` primitive POD raw inline 整数字段写入范围守卫子切片完成；
  float32 narrowing/finite semantics、nested field marshaling、object-level `FieldInfo.GetValue/SetValue` method
  surface、cross-module provider binding 和完整 `FieldInfo` 行为仍未关闭。
  完成项目：`reflection_field_value.c` 新增 primitive integer storage-width min/max guard：signed target 拒绝小于 min 或
  大于 max 的写入，unsigned target 拒绝 signed negative 和大于 unsigned max 的写入，unsigned-to-signed 超过 signed max
  也在 cast 前拒绝。失败写入发生在任何 `memcpy` 前，因此保留原始 raw inline storage bytes。
  RED/GREEN：RED 为新增 `test_reflection_rejects_out_of_range_field_info_primitive_pod_integer_writes` 后，Windows MSVC
  Debug focused run 17 个测试中该用例失败（`Expected FALSE Was TRUE`）；GREEN 后同一 focused run 通过 17/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 17/0、`metadata_runtime_query` 24/0、
  `metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z9-fieldinfo-primitive-pod-integer-range-guard.md`。
  备注：本切片只关闭 primitive POD integer range guard，不声明 float32 narrowing/finite semantics、nested struct
  marshaling、完整 FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整
  metadata sweep 完成。

- 2026-07-01 09:35:02 +08:00 · 10-S4Z8 / 11-S4AT FieldInfo primitive POD width matrix ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` primitive POD raw inline 字段读写的 full storage-width 覆盖子切片完成；
  primitive numeric overflow/range semantics、nested field marshaling、object-level `FieldInfo.GetValue/SetValue` method
  surface、cross-module provider binding 和完整 `FieldInfo` 行为仍未关闭。
  完成项目：`tests/module/test_reflection_token_resolve.c` 新增 signed/unsigned/float32 raw primitive width helper，并在同一
  `ZrCore_Reflection_ReadFieldInfoTokenValue()` / `WriteFieldInfoTokenValue()` API 边界下覆盖 int8、int16、int64、
  uint8、uint16、uint64、float32 raw inline storage。该覆盖证明 10-S4Z6 拆出的通用 primitive raw path 已覆盖剩余
  C storage widths，未新增生产代码。
  RED/GREEN：本切片是上一通用实现后的 coverage GREEN；Windows MSVC Debug focused run 在新增宽度矩阵后通过 16/0。
  首次宽度矩阵构建暴露 MSVC C4702 unreachable-code warning，移除测试 helper 默认分支中的 unreachable return 后复验通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 16/0、`metadata_runtime_query` 24/0、
  `metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z8-fieldinfo-primitive-pod-width-matrix.md`。
  备注：本切片只关闭 primitive POD storage-width matrix 覆盖，不声明 numeric overflow/range semantics、nested struct
  marshaling、完整 FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整
  metadata sweep 完成。

- 2026-07-01 05:06:21 +08:00 · 10-S4Z7 / 11-S4AS FieldInfo primitive POD representative matrix ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` primitive POD raw inline 字段读写的代表性矩阵覆盖子切片完成；
  全量 primitive variant 穷举、nested field marshaling、object-level `FieldInfo.GetValue/SetValue` method surface、
  cross-module provider binding 和完整 `FieldInfo` 行为仍未关闭。
  完成项目：`tests/module/test_reflection_token_resolve.c` 新增 shared raw primitive FieldDef fixture helper，并在同一
  `ZrCore_Reflection_ReadFieldInfoTokenValue()` / `WriteFieldInfoTokenValue()` API 边界下覆盖 bool、uint32、double
  三类代表性 raw inline storage：bool 读 true、拒绝 int 写、写 false；uint32 读 `0xFEDC1234` 为 `UINT64`、拒绝 bool 写、
  写 `0xAABBCCDD`；double 读 `6.25` 为 `DOUBLE`、拒绝 bool 写、写 `-12.5`。该覆盖证明 10-S4Z6 拆出的通用
  primitive raw path 已跨 signed/unsigned/bool/float family 工作，未新增生产代码。
  RED/GREEN：本切片是上一通用实现后的 coverage GREEN；Windows MSVC Debug focused run 在新增矩阵后直接通过 15/0，
  因而无生产 RED 修复。验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 15/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z7-fieldinfo-primitive-pod-matrix.md`。
  备注：本切片只关闭代表性 primitive POD matrix 覆盖，不声明所有 primitive 宽度/溢出语义穷举、nested struct
  marshaling、完整 FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整
  metadata sweep 完成。

- 2026-07-01 04:45:19 +08:00 · 10-S4Z6 / 11-S4AR FieldInfo primitive POD read/write boundary ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` primitive POD raw inline 字段读写子切片完成；broader primitive
  matrix、nested field marshaling、object-level `FieldInfo.GetValue/SetValue` method surface、cross-module provider binding
  和完整 `FieldInfo` 行为仍未关闭。
  完成项目：新增 `zr_vm_core/src/zr_vm_core/reflection_field_value.c` 作为 FieldInfo token value boundary；既有
  `ZrCore_Reflection_ReadFieldInfoTokenValue()` / `WriteFieldInfoTokenValue()` 现在先解析 FieldDef token、owner field
  offset/type-layout 和 inline storage range，再按字段 flag 分支：`VALUE_SLOT` 路径继续复制 `SZrTypeValue`，raw primitive
  POD 路径读取 validated `FIELD_SIG(PRIMITIVE(...))`，拒绝 `VALUE_SLOT`/GC/ownership flags，校验 exact primitive byte size，
  并用 `memcpy` 读写 raw scalar。focused fixture 覆盖 int32 raw storage：读取 `-12345`、拒绝 bool 写、写入 `2048` 后读回。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 primitive POD 用例后，Windows MSVC Debug 构建并运行
  14 个测试，其中新用例在 primitive POD read 处失败（`Expected TRUE Was FALSE`）；GREEN 后同一测试通过 14/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 14/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z6-fieldinfo-primitive-pod-read-write.md`。
  备注：本切片只关闭 same-runtime FieldDef token 的 primitive POD int32 raw inline boundary；不声明完整 primitive
  variant matrix、nested struct marshaling、完整 FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、
  完整 trim analyzer 或完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 04:12:42 +08:00 · 10-S4Z5 / 11-S4AQ FieldInfo value-slot write boundary ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` 字段值写入的只写 `VALUE_SLOT` 子切片完成；
  raw POD/nested inline struct marshaling、object-level `FieldInfo.SetValue` method surface、cross-module provider binding
  和完整 `FieldInfo` 行为仍未关闭。
  完成项目：新增 `ZrCore_Reflection_WriteFieldInfoTokenValue(state, runtime, fieldToken, inlineStorage,
  inlineStorageByteSize, value)`；同时把 read/write 共用的 FieldDef token、owner `SZrTypeLayoutField`、offset/type-layout、
  `VALUE_SLOT` flag 和 inline storage range 校验抽到 `reflection_resolve_field_value_slot_layout()`。写入成功时用
  `ZrCore_Value_Copy()` 将调用方 `SZrTypeValue` 复制到 inline storage 的字段槽。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 FieldInfo value-slot write 用例后，Windows MSVC Debug
  构建/链接失败，缺少 `ZrCore_Reflection_WriteFieldInfoTokenValue`；GREEN 后同一测试通过 13/0，新增用例覆盖 null state、
  null runtime、非 FieldDef token、null inline storage、null value、短 inline storage、写前读回 int `11` 和写后读回 int
  `271828`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 13/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z5-fieldinfo-value-slot-write.md`。
  备注：本切片只关闭 token-driven FieldInfo write 的 inline `VALUE_SLOT` 边界；不声明 POD/nested struct field
  marshaling、完整 FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、完整 trim analyzer
  或完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 03:49:06 +08:00 · 10-S4Z4 / 11-S4AP FieldInfo value-slot read boundary ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` 字段值读取的只读 `VALUE_SLOT` 子切片完成；
  write、raw POD/nested inline struct marshaling、object-level FieldInfo method surface、cross-module provider binding
  和完整 `FieldInfo` 行为仍未关闭。
  完成项目：新增 `ZrCore_Reflection_ReadFieldInfoTokenValue(state, runtime, fieldToken, inlineStorage,
  inlineStorageByteSize, outValue)`；该入口复用 `ZrCore_Reflection_ResolveToken()` 的 FieldDef owner/field layout、
  offset 和 type-layout id，按 owner `SZrTypeLayoutField` 校验 offset/type-layout/`VALUE_SLOT` flag/byte range 后，
  用 `ZrCore_Value_Copy()` 从 inline storage 复制 `SZrTypeValue` 到输出槽。失败路径会把输出槽重置为 null 并返回
  `ZR_FALSE`。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 FieldInfo value-slot read 用例后，WSL GCC 构建/链接
  失败，缺少 `ZrCore_Reflection_ReadFieldInfoTokenValue`；GREEN 后同一测试通过 12/0，新增用例覆盖 null state、
  null runtime、非 FieldDef token、短 inline storage 和成功读取 int `314159`。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 12/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z4-fieldinfo-value-slot-read.md`。
  备注：本切片只关闭 token-driven FieldInfo read 的 inline `VALUE_SLOT` 边界；不声明 field write、POD/nested
  struct field marshaling、完整 FieldInfo methods、`@dynamically_accessed` dataflow、DESCRIPTION promotion、
  完整 trim analyzer 或完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 03:17:49 +08:00 · 10-S4Z3 / 11-S4AO FieldInfo recursive signature type-node type literal carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` signature type-node object 的 recursive semantic type literal
  carrier 子切片完成；cross-module provider binding、字段值读写和完整 `FieldInfo` 行为仍未关闭。
  完成项目：`reflection_build_signature_type_node_object_internal()` 现在在 recursive signature node 已有 semantic
  `typeName` 时沿用既有 public `type` 字段构造 type literal object；generic fixture 锁定 base TypeDef node、
  primitive child、direct TypeDef child 和 direct TypeRef child 的 `type.kind == "type"` 且 `name/qualifiedName == "int"`。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 nested node `type` object 断言后，WSL GCC
  运行失败 1/11（`Expected Non-NULL`）；GREEN 后同一测试通过 11/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z3-fieldinfo-signature-node-type-literal.md`。
  备注：本切片只关闭 recursive signature node 的 same-runtime type literal carrier；不声明跨模块 provider
  loading/version compatibility、字段 read/write marshaling、完整 FieldInfo method surface、`@dynamically_accessed`
  dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c`
  中 `callerName` unused warning。

- 2026-07-01 03:04:36 +08:00 · 10-S4Z2 / 11-S4AN FieldInfo direct TypeRef child type-node semantic token/layout/name carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` signature type-node object 的 recursive direct TypeRef child
  semantic carrier 子切片完成；cross-module provider binding、recursive field type binding、字段值读写和完整
  `FieldInfo` 行为仍未关闭。
  完成项目：`reflection_build_signature_type_node_object_internal()` 现在对递归 `TYPE_REF` node 复用 existing module
  signature token record、`ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` 的 TypeRef→target TypeDef layout path，
  以及 target TypeDef string-pool name。focused fixture 扩展为
  `FIELD_SIG(GENERIC_INST(TYPE_DEF(object, 17), int64, TYPE_DEF(object, 17), TYPE_REF(object, 23)))`，锁定
  `childNodeObjects[2]` 携带 `TEST_TYPE_REF_TOKEN`、layout 42、size 16、name `int`。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 direct TypeRef child semantic carrier 断言后，
  WSL GCC 运行失败 1/11（`Expected 83886081 Was 0`，即 nested TypeRef `typeToken` 缺失）；GREEN 后同一测试通过 11/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4z2-fieldinfo-signature-typeref-child-node-semantic.md`。
  备注：本切片只关闭 same-runtime bound TypeRef child semantic token/layout/name carrier；不声明跨模块 provider
  loading/version compatibility、字段 read/write marshaling、完整 FieldInfo method surface、`@dynamically_accessed`
  dataflow、DESCRIPTION promotion、完整 trim analyzer 或完整 metadata sweep 完成。Clang 仍报告既有 `reflection.c`
  中 `callerName` unused warning。

- 2026-07-01 02:47:33 +08:00 · 10-S4Y / 11-S4AM FieldInfo direct TypeDef child/base type-node semantic token/layout/name carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` signature type-node object 的 recursive direct TypeDef base/child
  semantic carrier 子切片完成；direct TypeRef child semantic token/layout binding、recursive field type binding、
  字段值读写、完整 `FieldInfo` 行为和跨模块 provider 规则仍未关闭。
  完成项目：`reflection_build_signature_type_node_object_internal()` 现在接收 runtime 上下文；递归读取到
  `TYPE_DEF` node 时，用现有 signature record matcher 反查 TypeDef token，再复用
  `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` 和 `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()` 写出
  `typeToken/typeLayoutId/typeSize/typeName`。focused fixture 扩展为
  `FIELD_SIG(GENERIC_INST(TYPE_DEF(object, 17), int64, TYPE_DEF(object, 17)))`，锁定
  `baseTypeNodeObject` 和 `childNodeObjects[1]` 均携带 `TEST_FIELD_TYPE_DEF_TOKEN`、layout 42、size 16、name `int`。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 direct TypeDef base/child semantic carrier 断言后，
  WSL GCC 运行失败 1/11（`Expected 33554434 Was 0`，即 nested `typeToken` 缺失）；GREEN 后同一测试通过 11/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4y-fieldinfo-signature-typedef-child-node-semantic.md`。
  备注：本切片只关闭 same-runtime direct TypeDef base/child semantic token/layout/name carrier；不声明 direct TypeRef
  child binding、字段 read/write marshaling、完整 FieldInfo method surface、`@dynamically_accessed` dataflow、
  DESCRIPTION promotion、完整 trim analyzer 或跨模块 provider 完成。Clang 仍报告既有 `reflection.c` 中 `callerName`
  unused warning。

- 2026-07-01 02:19:00 +08:00 · 10-S4X / 11-S4AL FieldInfo signature primitive child type-node semantic name carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` signature type-node object 的 primitive generic argument
  semantic name 子切片完成；direct TypeDef/TypeRef child semantic token/layout binding、recursive field type binding、
  字段值读写、完整 `FieldInfo` 行为和跨模块 provider 规则仍未关闭。
  完成项目：`reflection_build_signature_type_node_object_internal()` 现在在调用方未传入 `typeName` 且 node 为
  `PRIMITIVE` 时复用 `reflection_builtin_type_name()`；focused fixture 在
  `FIELD_SIG(GENERIC_INST(TYPE_DEF(object, 17), int64))` 上锁定 `childNodeObjects[0].typeName == "int"`，同时保留
  structural node/blob/payload summary 与空 token/layout carrier。
  RED/GREEN：RED 为 focused reflection token resolver 测试把 generic child `typeName` 从 null 改为 `int` 后，WSL GCC
  运行失败 1/11（`Expected 12 Was 0`，即字符串字段缺失）；GREEN 后同一测试通过 11/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4x-fieldinfo-signature-primitive-child-node-semantic.md`。
  备注：本切片只关闭 primitive child semantic name carrier；不声明 direct TypeDef/TypeRef child token/layout binding、
  字段 read/write marshaling、完整 FieldInfo method surface、`@dynamically_accessed` dataflow、DESCRIPTION promotion、
  完整 trim analyzer 或跨模块 provider 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 02:00:10 +08:00 · 10-S4W / 11-S4AK FieldInfo signature child type-node object list carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` signature type-node object 的 child list structural carrier 子切片完成；
  generic argument semantic binding、recursive field type binding、字段值读写、完整 `FieldInfo` 行为和跨模块 provider 规则仍未关闭。
  完成项目：`reflection_build_signature_type_node_object_internal()` 现在为 signature node object 写出 `childNodeObjects` array；
  对存在 `childListBlobOffset/childCount` 的节点，从同一 validated signature blob 顺序读取 child type-node 并物化为
  `kind == "signatureTypeNode"` 的 structural object。focused fixture 在
  `FIELD_SIG(GENERIC_INST(TYPE_DEF(object, 17), int64))` 上锁定 `childNodeObjects[0]` 为 `PRIMITIVE(INT64)`。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 child node array 断言后，WSL GCC 运行失败 1/11
  （`childNodeObjects` 缺失导致 `Expected Non-NULL`）；GREEN 后同一测试通过 11/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4w-fieldinfo-signature-child-node-objects.md`。
  备注：本切片只关闭 same-blob child type-node structural list；不声明 child semantic token/layout binding、
  字段 read/write marshaling、完整 FieldInfo method surface、`@dynamically_accessed` dataflow、DESCRIPTION promotion、
  完整 trim analyzer 或跨模块 provider 完成。

- 2026-07-01 01:41:50 +08:00 · 10-S4V / 11-S4AJ FieldInfo signature base type-node object carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` signature type-node object 的 base type 子节点 carrier 子切片完成；
  generic argument child list、recursive semantic field type binding、字段值读写、完整 `FieldInfo` 行为和跨模块 provider 规则仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在把 field signature blob 传入 signature node object builder；
  `fieldTypeSignatureNodeObject` 对 `GENERIC_INST`/wrapper 这类带 `baseTypeBlobOffset` 的节点递归读取同一 blob，并写出
  `baseTypeNodeObject`。focused fixture 新增 `FIELD_SIG(GENERIC_INST(TYPE_DEF(object, 17), int64))`，锁定 top-level
  generic node 的 base offset/child summary 和 nested base `TYPE_DEF` 的 node/blob/payload summary。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 generic base type-node object 断言后，WSL GCC
  运行失败 1/11（`baseTypeNodeObject` 缺失导致 `Expected Non-NULL`）；GREEN 后同一测试通过 11/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 11/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4v-fieldinfo-signature-base-type-node-object.md`。
  备注：本切片只关闭 signature node object 的 base-child structural carrier；不声明 generic argument child list、
  semantic token/layout binding、字段 read/write marshaling、完整 FieldInfo method surface、`@dynamically_accessed` dataflow、
  DESCRIPTION promotion 或完整 trim analyzer 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 01:17:21 +08:00 · 10-S4U / 11-S4AI FieldInfo signature type-node object carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` signature type-node object carrier 子切片完成；
  recursive semantic field type binding、字段值读写、完整 `FieldInfo` 行为和跨模块 provider 规则仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 新增 `fieldTypeSignatureNodeObject`，用
  `kind == "signatureTypeNode"` 的嵌套 object 聚合已验证 field type-node 的 node/blob offset/payload/base/child summary，
  并同步 signature-derived `typeToken/typeLayoutId/typeSize/typeName/matchesLayout`。focused fixture 现在锁定
  primitive `BOOL`、direct local `TYPE_DEF` 和 bound `TYPE_REF` 三种 signature node object 与既有 flat fields 一致。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增 `fieldTypeSignatureNodeObject` 断言后，WSL GCC
  运行失败 3/10（字段缺失导致 `Expected Non-NULL`）；GREEN 后同一测试通过并锁定 primitive false、TypeDef true、
  TypeRef true 的 node object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 10/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4u-fieldinfo-signature-type-node-object.md`。
  备注：本切片只关闭顶层 signature type-node read-only object carrier；不声明 recursive wrapper/generic child
  object、字段 read/write marshaling、完整 FieldInfo method surface、`@dynamically_accessed` dataflow、DESCRIPTION promotion
  或完整 trim analyzer 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 00:55:33 +08:00 · 10-S4T / 11-S4AH FieldInfo signature/layout consistency carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` signature-derived type 与 FieldDef layout 一致性 carrier 子切片完成；
  recursive semantic field type binding、字段值读写、完整 `FieldInfo` 行为和跨模块 provider 规则仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 新增 `fieldTypeSignatureMatchesLayout` bool 字段，只有当
  signature-derived layout 与 FieldDef layout 都存在、layout id 非 none，且二者的 layout id 和 registry layout 指针一致时才为 true。
  focused fixture 现在锁定 primitive bool signature 对 int FieldDef layout 为 false，direct local `TYPE_DEF` 与
  bound `TYPE_REF` 解析到同一 layout id `42` 时为 true。
  RED/GREEN：RED 为 focused reflection token resolver 测试新增该 bool 字段断言后，WSL GCC 运行失败 3/10
  （字段缺失导致 `Expected Non-NULL`）；GREEN 后同一测试通过并锁定 primitive false、TypeDef true、TypeRef true。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 10/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4t-fieldinfo-signature-layout-consistency.md`。
  备注：本切片只关闭 signature/layout 一致性只读 carrier；不声明 recursive wrapper/generic type-node object、
  字段 read/write marshaling、完整 FieldInfo method surface、`@dynamically_accessed` dataflow、DESCRIPTION promotion
  或完整 trim analyzer 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 00:37:26 +08:00 · 10-S4S / 11-S4AG FieldInfo bound TypeRef signature carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` current-runtime bound `TYPE_REF` signature carrier 子切片完成；
  跨模块 provider lookup/version compatibility、recursive type-node reflection object、字段值读写、完整 `FieldInfo`
  行为和完整 signature-derived semantic field type binding 仍未关闭。
  完成项目：新增 `FIELD_SIG(TYPE_REF(object, 23))` fixture，构造 attached module `TYPE_REF` token record、paired
  TypeRef signature blob 和 target local `TYPE_DEF` layout binding。`ZrCore_Reflection_BuildFieldInfoTokenObject()`
  现在在 TypeRef token/layout 解析成功后读取 target TypeDef row name，写出 `fieldTypeSignatureTypeToken ==
  TEST_TYPE_REF_TOKEN`、layout id `42`、size `16`、`fieldTypeSignatureTypeName == "int"`，并物化
  `fieldTypeSignatureType.kind/name/qualifiedName == type/int/int`。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 bound `TYPE_REF` signature 提供 type name/object 后，
  WSL GCC 运行失败（`fieldTypeSignatureTypeName` 仍为 null）；GREEN 后同一测试通过并同时锁定 TypeRef token、
  layout/size 与 target TypeDef type object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 10/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4s-fieldinfo-signature-typeref-carrier.md`。
  备注：本切片只关闭 current-runtime attached bound TypeRef signature carrier；不声明 cross-module signature
  binding、recursive wrapper/generic type-node object、字段类型一致性校验、字段 read/write marshaling、完整
  FieldInfo method surface、`@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 trim analyzer 完成。
  Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-07-01 00:15:36 +08:00 · 10-S4R / 11-S4AF FieldInfo direct TypeDef signature type object ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` direct local `TYPE_DEF` signature type object 子切片完成；
  TypeRef/跨模块 signature binding、recursive type-node reflection object、字段值读写、完整 `FieldInfo` 行为和
  完整 signature-derived semantic field type binding 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 在 direct local `TYPE_DEF` signature node 已匹配到
  token/layout 后，复用 TypeDef layout binding view 读取 zrp string-pool type name，写出
  `fieldTypeSignatureTypeName == "int"`，并把 `fieldTypeSignatureType` 物化为
  `kind/name/qualifiedName == type/int/int` 的 reflection object。该 object 仍独立于 FieldDef layout-derived
  `type` object。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 direct `TYPE_DEF` signature 也提供
  `fieldTypeSignatureType` object 后，WSL GCC 运行失败（字段仍为 null）；GREEN 后同一测试通过并同时锁定
  `fieldTypeSignatureTypeName` 与 type object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 9/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-07-01-aot-10-s4r-fieldinfo-signature-typedef-type-object.md`。
  备注：本切片只关闭 current-runtime local `TYPE_DEF` signature type object；不声明 TypeRef/cross-module
  provider lookup、recursive wrapper/generic type-node object、字段类型一致性校验、字段 read/write marshaling、
  完整 FieldInfo method surface、`@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 trim analyzer 完成。
  Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-06-30 21:45:03 +08:00 · 10-S4Q / 11-S4AE FieldInfo direct TypeDef signature token/layout carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` direct local `TYPE_DEF` signature carrier 子切片完成；
  recursive type-node reflection object、TypeRef/跨模块 signature binding、字段值读写、完整 `FieldInfo` 行为和
  完整 signature-derived semantic field type binding 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在可把 validated `FIELD_SIG` field type-node
  与 attached metadata records 中 direct `TYPE_DEF` signature blob 做等值匹配，命中后复用
  `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` 写入 `fieldTypeSignatureTypeToken`、
  `fieldTypeSignatureTypeLayoutId` 与 `fieldTypeSignatureTypeSize`。focused fixture 新增
  `FIELD_SIG(TYPE_DEF(object, 17))`，并断言 token 为 `TEST_FIELD_TYPE_DEF_TOKEN`、layout id 为 `42`、size 为 `16`。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `fieldTypeSignatureTypeToken` 后，WSL GCC 运行失败
  （字段仍缺失）；GREEN 后 FieldDef token object 暴露 direct local TypeDef signature token/layout carrier。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 9/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4q-fieldinfo-signature-typedef-carrier.md`。
  备注：本切片只关闭 direct local `TYPE_DEF` signature token/layout carrier；不声明 recursive type-node object、
  TypeRef/cross-module signature binding、字段类型一致性校验、字段 read/write marshaling、完整 FieldInfo method
  surface、`@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 trim analyzer 完成。Clang 仍报告既有
  `reflection.c` 中 `callerName` unused warning。

- 2026-06-30 21:20:26 +08:00 · 10-S4P / 11-S4AD FieldInfo module reflection object link ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` module reflection object link 子切片完成；字段值读写、
  完整 signature-derived semantic field type binding、完整 `FieldInfo` 行为、recursive type-node reflection object、
  泛型对象化和 `MakeGenericType` 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在在 attached runtime module 是真实
  `ZR_OBJECT_INTERNAL_TYPE_MODULE` 时复用既有 `reflection_build_module_reflection()`，并把返回对象写入
  `FieldInfo.module`。focused fixture 改为用 `ZrCore_Module_Create()` 创建模块并断言
  `module.kind/name/qualifiedName == module/geometry/geometry`。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `FieldInfo.module` object 后，WSL GCC 运行失败
  （字段仍为 null）；GREEN 后 FieldDef token object 暴露最小 module reflection object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4p-fieldinfo-module-reflection-link.md`。
  备注：本切片只关闭 FieldInfo -> module identity link；不声明字段值 read/write marshaling、完整 FieldInfo method
  surface、TypeDef/TypeRef signature binding、跨模块 FieldRef/TypeRef、`@dynamically_accessed` dataflow、
  DESCRIPTION promotion 或完整 trim analyzer 完成。Clang 仍报告既有 `reflection.c` 中 `callerName` unused warning。

- 2026-06-30 21:02:11 +08:00 · 10-S4O / 11-S4AC FieldInfo primitive signature type object carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` primitive field signature type object carrier 子切片完成；字段值读写、
  完整 signature-derived semantic field type binding、完整 `FieldInfo` 行为、recursive type-node reflection object、
  module full reflection object link、泛型对象化和 `MakeGenericType` 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在在 validated `FIELD_SIG` field type-node 为
  `PRIMITIVE` 时，复用 `fieldTypeSignatureTypeName` 构造 public `FieldInfo.fieldTypeSignatureType` type literal object。
  focused fixture 继续保持 layout/token-derived `FieldInfo.type` 为 `"int"`，同时断言 signature-derived
  `fieldTypeSignatureType.kind/name/qualifiedName` 为 `type/bool/bool`。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `fieldTypeSignatureType` object 后，WSL GCC 运行失败
  （字段仍缺失）；GREEN 后 FieldDef token object 暴露 `PRIMITIVE(BOOL)` 的独立 type literal object。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4o-fieldinfo-field-signature-type-object.md`。
  备注：本切片只关闭 primitive signature type object carrier；不声明 TypeDef/TypeRef signature binding、字段类型一致性校验、
  recursive type-node reflection object、字段值 read/write marshaling、cache policy、跨模块 FieldRef/TypeRef、
  `@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 trim analyzer 完成。

- 2026-06-30 20:46:10 +08:00 · 10-S4N / 11-S4AB FieldInfo primitive signature type carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` primitive field signature type carrier 子切片完成；字段值读写、
  完整 signature-derived semantic field type binding、完整 `FieldInfo` 行为、recursive type-node reflection object、
  module full reflection object link、泛型对象化和 `MakeGenericType` 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在在 validated `FIELD_SIG` field type-node 为
  `PRIMITIVE` 时，继续把 payload0 暴露为 public `FieldInfo.fieldTypeSignatureValueType`，并通过
  `reflection_builtin_type_name()` 写出 `FieldInfo.fieldTypeSignatureTypeName`。该 carrier 与 layout/token-derived
  `typeName` 分离，当前 fixture 保持 `typeName == "int"`，同时 signature primitive type name 为 `"bool"`。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `fieldTypeSignatureValueType` 与
  `fieldTypeSignatureTypeName` 后，WSL GCC 运行失败（字段仍缺失）；GREEN 后 FieldDef token object 暴露
  `PRIMITIVE(BOOL)` 的 value type 与 type name。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4n-fieldinfo-field-signature-primitive-type.md`。
  备注：本切片只关闭 primitive signature type carrier；不声明 TypeDef/TypeRef signature binding、字段类型一致性校验、
  recursive type-node reflection object、字段值 read/write marshaling、cache policy、跨模块 FieldRef/TypeRef、
  `@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 trim analyzer 完成。

- 2026-06-30 20:32:08 +08:00 · 10-S4M / 11-S4AA FieldInfo field signature type-node summary carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` validated field signature type-node summary 子切片完成；字段值读写、
  signature-derived semantic field type binding、完整 `FieldInfo` 行为、recursive type-node reflection object、module full
  reflection object link、泛型对象化和 `MakeGenericType` 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在在 FieldDef paired signature record 可解析为
  `FIELD_SIG` header 后，继续调用 `ZrCore_MetadataRuntime_ReadSignatureTypeNode()` 读取 `fieldTypeBlobOffset`
  处的 type-node，并把 node/blobOffset/nextBlobOffset/payload0/payload1/baseTypeBlobOffset/childCount/childListBlobOffset
  写入 public `FieldInfo.fieldTypeSignature*` summary 字段；签名缺失、无效、root 不是 `FIELD_SIG` 或 type-node
  读取失败时这些字段保持 `0`。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `fieldTypeSignatureNode` 等 summary 字段后，WSL GCC
  运行失败（字段仍缺失）；GREEN 后 FieldDef token object 暴露 fixture 中 `PRIMITIVE(BOOL)` field type node 的 offset、
  next offset 与 payload。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4m-fieldinfo-field-signature-type-node.md`。
  备注：本切片只关闭最小 FieldInfo field signature type-node summary carrier；不声明 signature-derived semantic
  field type binding、recursive type-node reflection object、字段值 read/write marshaling、cache policy、跨模块
  FieldRef/TypeRef、`@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 trim analyzer 完成。

- 2026-06-30 20:13:27 +08:00 · 10-S4L / 11-S4Z FieldInfo validated field signature header carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` validated field signature header carrier 子切片完成；字段值读写、
  signature-derived field type binding、完整 `FieldInfo` 行为、field type-node reflection object、module full
  reflection object link、泛型对象化和 `MakeGenericType` 仍未关闭。
  完成项目：FieldInfo fixture 现在在 signature blob pool 的 raw `4/7` 坐标处放入合法 `FIELD_SIG` blob；
  `ZrCore_Reflection_BuildFieldInfoTokenObject()` 复用 `ZrCore_MetadataRuntime_ReadSignatureView()`，在 FieldDef
  paired signature record 可解析为 `FIELD_SIG` 时写入 public `FieldInfo.signatureRootNode`、
  `FieldInfo.signatureFlags` 与 `FieldInfo.fieldTypeBlobOffset`。签名缺失、无效或 root 不是 `FIELD_SIG` 时这些
  validated header 字段保持 `0`。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `signatureRootNode` / `signatureFlags` /
  `fieldTypeBlobOffset` 后，WSL GCC 运行失败（字段仍缺失）；GREEN 后 FieldDef token object 的 validated field
  signature header 通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4l-fieldinfo-field-signature-header.md`。
  备注：本切片只关闭最小 FieldInfo validated field signature header carrier；不声明 signature-derived field
  type binding、recursive type-node reflection object、字段值 read/write marshaling、cache policy、跨模块
  FieldRef/TypeRef、`@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 trim analyzer 完成。

- 2026-06-30 19:54:58 +08:00 · 10-S4K / 11-S4Y FieldInfo FieldDef signature blob coordinate carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` raw signature blob coordinate carrier 子切片完成；字段值读写、
  完整 `FieldInfo` 行为、field signature 语义解析、module full reflection object link、泛型对象化和
  `MakeGenericType` 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在把 resolved FieldDef row 的
  `signatureBlobOffset` / `signatureBlobLength` 原样写入 public `FieldInfo.signatureBlobOffset` 与
  `FieldInfo.signatureBlobLength`；focused fixture 将 FieldDef row 坐标设为 `4/7` 并断言 public object 暴露同一
  raw integer pair。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `signatureBlobOffset` / `signatureBlobLength`
  后，WSL GCC 运行失败（字段仍缺失）；GREEN 后 FieldDef token object 的 signature blob coordinate 通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4k-fieldinfo-fielddef-signature-blob.md`。
  备注：本切片只关闭最小 FieldInfo raw FieldDef signature blob offset/length carrier；不声明 blob slice
  validation、field signature parser、signature-derived field type binding、字段值 read/write marshaling、cache policy、
  跨模块 FieldRef/TypeRef、`@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 trim analyzer 完成。

- 2026-06-30 19:39:14 +08:00 · 10-S4J / 11-S4X FieldInfo FieldDef flags carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` raw metadata flags carrier 子切片完成；字段值读写、完整
  `FieldInfo` 行为、module full reflection object link、泛型对象化和 `MakeGenericType` 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在把 resolved FieldDef row 的 `flags` 原样写入
  public `FieldInfo.metadataFlags`；focused fixture 将 FieldDef row flags 设为 `0xA5` 并断言 public object
  暴露同一 raw integer。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `metadataFlags` 后，WSL GCC 运行失败
  （字段仍缺失）；GREEN 后 FieldDef token object 的 metadataFlags 通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4j-fieldinfo-fielddef-flags.md`。
  备注：本切片只关闭最小 `FieldInfo.metadataFlags` raw FieldDef row carrier；不声明 flags 位语义、
  `isStatic`/`isConst` 映射、字段值 read/write marshaling、cache policy、跨模块 FieldRef/TypeRef、
  `@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 trim analyzer 完成。

- 2026-06-30 19:26:41 +08:00 · 10-S4I / 11-S4W FieldInfo moduleName carrier ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` module name carrier 子切片完成；字段值读写、完整
  `FieldInfo` 行为、module full reflection object link、泛型对象化和 `MakeGenericType` 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在从 attached metadata runtime 的
  `module->moduleName` 读取 module name，缺失时回退 `module->fullPath`，并写入 public `moduleName` 字段；
  focused test 给 synthetic module 设置 `geometry` 并断言 FieldInfo 暴露该值。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `moduleName` string 后，WSL GCC 运行失败
  （字段仍缺失）；GREEN 后 FieldDef token object 的 moduleName 通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4i-fieldinfo-module-name.md`。
  备注：本切片只关闭最小 `FieldInfo.moduleName` string carrier；不声明 module reflection link、字段值
  read/write marshaling、cache policy、跨模块 FieldRef/TypeRef、`@dynamically_accessed` dataflow、
  DESCRIPTION promotion 或完整 trim analyzer 完成。

- 2026-06-30 19:11:22 +08:00 · 10-S4H / 11-S4V FieldInfo owner object link ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` owner link 子切片完成；字段值读写、完整 `FieldInfo`
  行为、module full reflection object link、泛型对象化和 `MakeGenericType` 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在在成功构造 `declaringType` type literal 后，
  将 public `owner` 字段指向同一对象；focused test 断言 `owner` 为 object 且与 `declaringType` 指针一致。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `owner` object link 后，WSL GCC 运行失败
  （`owner` 仍为 null）；GREEN 后 FieldDef token object 的 owner link 通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4h-fieldinfo-owner-link.md`。
  备注：本切片只关闭最小 `FieldInfo.owner` -> declaring type identity link；不声明 module reflection link、
  字段值 read/write marshaling、cache policy、跨模块 FieldRef/TypeRef、`@dynamically_accessed` dataflow、
  DESCRIPTION promotion 或完整 trim analyzer 完成。

- 2026-06-30 18:58:39 +08:00 · 10-S4G / 11-S4U FieldInfo declaring type object link ·
  状态：10-S4/11-S4 FieldDef token public `FieldInfo` owner identity 子切片完成；字段值读写、完整 `FieldInfo`
  行为、owner/module full reflection object links、泛型对象化和 `MakeGenericType` 仍未关闭。
  完成项目：`ZrCore_Reflection_BuildFieldInfoTokenObject()` 现在在既有 name/type/token/layout/offset/size 基础上，
  从 owner TypeDef row 的 zrp string pool 填充 `ownerTypeName`、`declaringTypeName`，并构造 nested
  `declaringType` type literal object；既有 nested field `type` type literal 也由 focused test 明确覆盖。
  RED/GREEN：RED 为 focused reflection token resolver 测试要求 `ownerTypeName`、`declaringTypeName` 与
  `declaringType` 后，WSL GCC 运行失败（新字段仍为 null）；GREEN 后 FieldDef token object 的 field type 与
  declaring type nested objects 均通过。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s4g-fieldinfo-declaring-type-object.md`。
  备注：本切片只关闭最小 `FieldInfo` 的 declaring type name/object carrier；不声明字段值 read/write marshaling、
  owner/module full reflection object links、cache policy、跨模块 FieldRef/TypeRef、`@dynamically_accessed` dataflow、
  DESCRIPTION promotion 或完整 trim analyzer 完成。

- 2026-06-30 18:38:42 +08:00 · 10-S4F / 11-S4T minimum FieldDef token FieldInfo public object ·
  状态：10-S4/11-S4 FieldDef token public reflection object 子切片完成；字段值读写、完整 `FieldInfo`
  行为、泛型对象化和 `MakeGenericType` 仍未关闭。
  完成项目：`reflection.h` 新增 `ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, fieldToken)`；
  `reflection.c` 复用 `ZrCore_Reflection_ResolveToken()` 的 FieldDef carrier 和 11-S4 TypeDef layout binding view，
  从 attached zrp string pool 读取 FieldDef/owner/field-type name，并构造包含 name、qualifiedName、kind、typeName、
  metadataToken、ownerTypeToken、fieldTypeToken、offset、size、typeLayoutId、fieldTypeLayoutId、ownerTypeLayoutId、
  layout 子对象和 nested type literal 的最小 public `FieldInfo` reflection object。
  RED/GREEN：RED 为 focused reflection token resolver 测试引用缺失
  `ZrCore_Reflection_BuildFieldInfoTokenObject()`，WSL GCC 链接失败；GREEN 后 FieldDef token object 正向与
  null/wrong-token 负向路径通过，reflection token resolve 8/0。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `reflection_token_resolve` 8/0、
  `metadata_runtime_query` 24/0、`metadata_runtime_typespec_layout` 17/0；focused CTest
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 三套环境均为 3/3。
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s4f-fielddef-token-fieldinfo-object.md`。
  备注：本切片只关闭 FieldDef token -> 最小 public FieldInfo object；不声明字段值 read/write marshaling、
  owner/module object links、cache policy、完整 FieldInfo methods、public generic reflection object、跨模块 FieldRef/TypeRef、
  `@dynamically_accessed` dataflow、DESCRIPTION promotion 或完整 trim analyzer 完成。

- 2026-06-30 18:17:22 +08:00 · 10-S5N / 11-S4S / 12-S5M runtime bound TypeRef token layout resolver ·
  状态：10-S5/11-S4/12-S5 的运行期 TypeRef->TypeDef layout resolver 支撑子切片完成；
  完整跨模块 provider runtime load、跨模块 TypeRef provider context、字段值读写、FieldInfo 物化、数据流和
  trim analyzer 仍未关闭。
  完成项目：`ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` 现在接受已附加 metadata runtime 中的 `TYPE_REF`
  token record；当 `targetMetadataToken` 指向当前 runtime 可解析的 `TYPE_DEF` row 时，复用 TypeDef layout binding
  view，校验 optional target signature token/hash、target module signature hash 以及 layout version/hash，并把命中缓存到
  TypeRef token。
  RED/GREEN：RED1 为新增 TypeRef 正向用例后，旧 resolver 对 `TYPE_REF` 返回 NULL；GREEN1 后 bound TypeRef->TypeDef
  返回 layout/id 并可命中 cache。RED2 为 module identity mismatch 用例期望 NULL 但旧实现仍接受；GREEN2 后
  `targetModuleSignatureHash` 不匹配会拒绝并清空 out layout id。
  验证：WSL GCC/Clang/Windows MSVC Debug 均通过 `metadata_runtime_typespec_layout` 17/0、`metadata_runtime_query`
  24/0、`reflection_token_resolve` 7/0、`metadata_type_ref_binding` 8/0；focused CTest
  `metadata_runtime_typespec_layout|metadata_runtime_query|reflection_token_resolve|metadata_type_ref_binding` 三套环境均为
  4/4。WSL GCC 初次 combined CTest 曾出现 `metadata_type_ref_binding` wrapper `No such file or directory`，直接执行、
  单项 CTest 和重跑 combined CTest 均通过，未复现。`git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5n-runtime-bound-typeref-token-layout-resolver.md`。
  备注：本切片只覆盖 attached/current-runtime bound TypeRef record -> TypeDef layout；不声明跨模块 provider
  lookup/version compatibility、zrp `TOKEN_RECORDS` 独立 runtime scan、FieldInfo/字段值读写、`@dynamically_accessed`
  dataflow、warning policy、DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 17:56:52 +08:00 · 10-S5M / 12-S5L dynamic-dependency bound TypeRef type token root ·
  状态：10-S5/12-S5 的 `@dynamic_dependency` 当前 embedded zrp TypeRef token-record binding 子切片完成；
  完整跨模块 provider load/compatibility、runtime TypeRef layout resolution、数据流和 trim analyzer 仍未关闭。
  完成项目：`backend_aot_c_type_layout_metadata_roots.c` 的 `dynamicDependencyTypeToken` 解析现在接受
  `TYPE_REF` token，要求 embedded zrp `TOKEN_RECORDS` 中有唯一 matching `SZrMetadataTokenRecord`，且
  `record->targetMetadataToken` 指向当前 metadata blob 的 `TYPE_DEF` token。解析成功后继续复用 TypeDef row
  的唯一 non-none `typeLayoutId` 作为 annotation type-layout root，因此生成 C 在引用该 layout 的函数被裁剪时仍保留
  目标 TypeDef layout descriptor、registration entry、type-layout stats、root marker 和 root-only token table 条目。
  RED/GREEN：RED 为新增 TypeRef token generated-C fixture 写入 `dynamicDependencyTypeToken = 0x05000001`，
  旧 helper 因不支持 `TYPE_REF` 让 writer 返回 false；GREEN 后该 TypeRef 通过 `targetMetadataToken = 0x02000001`
  解析到 `typeLayoutId = 2`，function 2 被裁剪但 `ZrTypeLayout_2` 与 `zr_aot_type_layout_tokens[2] = 0x02000001u`
  保留。source/type-layout contracts 同步锁定 `TOKEN_RECORDS` section、`SZrMetadataTokenRecord`、
  `record->targetMetadataToken` 和 `ZR_METADATA_TABLE_TYPE_REF` gate。
  验证：WSL gcc/clang focused code stripping 10/0、source contracts 24/0、type-layout contracts 1/0，
  focused CTest `aot_c_code_stripping|aot_c_type_layout_contracts` 2/2，global shared-library smoke 10/0、
  call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；Windows MSVC Debug focused code stripping 10/0、
  source contracts 24/0、type-layout contracts 1/0，focused CTest 2/2；Windows shared-library smoke binaries
  returned OK with 10/5/7 ignored and 0 failures。`git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5m-dynamic-dependency-bound-typeref-token-layout-root.md`。
  备注：本切片只覆盖当前 embedded metadata 中已绑定到当前 TypeDef row 的 TypeRef；不声明跨模块 provider
  加载/版本兼容、运行期 TypeRef->layout resolver、字段值读写、FieldInfo object、数据流、warning policy、
  DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 17:31:53 +08:00 · 10-S5L / 12-S5K dynamic-dependency field token layout roots ·
  状态：10-S5/12-S5 的 `@dynamic_dependency` 当前模块 FieldDef field-token carrier 子切片完成；
  完整字段值读写/FieldInfo 物化、TypeRef/跨模块 type token、数据流和 trim analyzer 仍未关闭。
  完成项目：新增 `backend_aot_c_type_layout_metadata_roots.{h,c}`，把 embedded zrp TypeDef/TypeSpec/FieldDef token
  -> type-layout root 解析从 `backend_aot_c_type_layouts.c` 拆出。root collector 现在读取
  `dynamicDependencyFieldToken` uint32 decorator metadata，仅接受当前模块 `MEMBER_DEF` FieldDef row，校验 owner
  TypeDef row 的 field range，并把 owner TypeDef `typeLayoutId` 与 FieldDef `typeLayoutId` 追加到同一 type-layout root
  集合。生成 C 在 owning function 被裁剪时仍保留 owner/field `SZrTypeLayout` descriptor、registration entry、统计和
  `code_stripping.annotationTypeLayoutRoot[]` markers。
  RED/GREEN：RED 为新增 generated-C FieldDef token fixture 期望 `ZrTypeLayout_2` 后失败 `Expected Non-NULL`；
  GREEN 后 TypeDef/TypeSpec/FieldDef fixtures 均通过，FieldDef fixture 确认 function 2 被裁剪但 root markers 为 1/2。
  验证：WSL gcc/clang focused code stripping 9/0、source contracts 24/0、type-layout contracts 1/0，
  focused CTest `aot_c_code_stripping|aot_c_type_layout_contracts` 2/2，global shared-library smoke 10/0、
  call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；Windows MSVC Debug focused code stripping 9/0、
  source contracts 24/0、type-layout contracts 1/0，focused CTest 2/2；Windows shared-library smoke binaries
  returned OK with 10/5/7 ignored and 0 failures。`git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5l-dynamic-dependency-field-token-layout-roots.md`。
  备注：本切片只覆盖当前模块 FieldDef token -> owner/field type-layout roots；不声明字段值读写、FieldInfo object、
  TypeRef/跨模块 type token、数据流、warning policy、DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 16:58:17 +08:00 · 10-S5K / 12-S5J dynamic-dependency type token root ·
  状态：10-S5/12-S5 的 `@dynamic_dependency` 当前模块 TypeDef/TypeSpec type-token carrier 子切片完成；
  完整 field dependency、TypeRef/跨模块 type token、数据流和 trim analyzer 仍未关闭。
  完成项目：AOT C emitter/root collector now passes embedded metadata blob and scans `dynamicDependencyTypeToken`
  uint32 decorator metadata. `backend_aot_c_type_layouts.c` resolves only `TYPE_DEF`/`TYPE_SPEC` tokens through
  embedded zrp TypeDef/TypeSpec rows to a unique non-none `typeLayoutId`, verifies an existing resolver function,
  and appends the same type-layout root set used by stats/declarations/registration. `backend_aot_c_type_layout_tokens.c`
  now uses metadata blob fallback for root-only layouts so generated `zr_aot_type_layout_tokens[]` keeps
  `0x02000001u` / `0x07000001u`.
  RED/GREEN：RED 为新增 TypeDef token generated-C fixture 期望 `ZrTypeLayout_2` 后失败 `Expected Non-NULL`；
  GREEN 后 TypeDef 与 TypeSpec fixtures 都确认 function 2 仍被裁剪，但 `ZrTypeLayout_2`、annotation type-layout root
  marker、type-layout stats 和 `zr_aot_type_layout_tokens[2]` 均保留。
  验证：WSL gcc/clang focused code stripping 8/0、source contracts 24/0、type-layout contracts 1/0，
  global shared-library smoke 10/0、call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；
  WSL gcc/clang focused CTest `aot_c_code_stripping|aot_c_type_layout_contracts` 2/2；
  Windows MSVC Debug focused code stripping 8/0、source contracts 24/0、type-layout contracts 1/0，
  focused CTest 2/2；Windows shared-library smoke binaries returned OK with 10/5/7 ignored and 0 failures。
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5k-dynamic-dependency-type-token-root.md`。
  备注：本切片只覆盖当前模块 TypeDef/TypeSpec token-to-typeLayoutId mapping；不声明 FieldDef、
  TypeRef/跨模块 type token、field dependency、数据流、warning policy、DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 16:25:31 +08:00 · 10-S5J / 12-S5I dynamic-dependency type-layout root ·
  状态：10-S5/12-S5 的 `@dynamic_dependency` 当前模块 type-layout-id carrier 子切片完成；
  完整 TypeDef/TypeSpec token dependency、field dependency、跨模块 annotation、数据流和 trim analyzer 仍未关闭。
  完成项目：AOT C emitter 在 code stripping 前扫描 function decorator metadata 的
  `dynamicDependencyTypeLayoutId` uint32 字段，收集并去重 type-layout roots。type-layout count/payload/generated-byte
  统计、layout declaration emission、type-layout index space、GC descriptor index space 和 code-registration
  `typeLayouts[]` writer 都接受 root 集合。生成 C 新增 `code_stripping.annotationTypeLayoutRoots` 与
  `code_stripping.annotationTypeLayoutRoot[index]` marker；测试确认 function 2 被裁剪时 `ZrTypeLayout_2` 与
  `zr_aot_type_layouts[2] = &ZrTypeLayout_2` 仍保留。
  RED/GREEN：RED 为新增 `test_aot_c_code_stripping_preserves_dynamic_dependency_type_layout_metadata` 期望
  `ZrTypeLayout_2` 后，旧实现返回 `Expected Non-NULL`；GREEN 后同一 fixture 保留 type-layout metadata，
  但不保留被裁剪的 `zr_aot_fn_2`。
  验证：WSL gcc/clang focused code stripping 6/0、source contracts 24/0、type-layout contracts 1/0，
  global shared-library smoke 10/0、call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；
  WSL gcc/clang focused CTest `aot_c_code_stripping|aot_c_type_layout_contracts` 2/2；
  Windows MSVC Debug focused code stripping 6/0、source contracts 24/0、type-layout contracts 1/0。
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。
  产出：`tests/acceptance/2026-06-30-aot-10-s5j-dynamic-dependency-type-layout-root.md`。
  备注：本切片只覆盖当前模块 numeric `dynamicDependencyTypeLayoutId` carrier 对 generated type-layout metadata
  的保留；不声明 TypeDef/TypeSpec token dependency、field dependency、跨模块 annotation、非方法 member token、
  `@dynamically_accessed` 数据流、warning promotion/per-warning suppression、未注解反射 warning、
  类型/成员级 DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 15:31:50 +08:00 · 10-S5I / 12-S5H dynamic-dependency non-exported MethodDef token root ·
  状态：10-S5/12-S5 的 `@dynamic_dependency` 当前模块非导出 method token carrier 子切片完成；
  完整跨模块 dependency、field/type dependency、数据流和 trim analyzer 仍未关闭。
  完成项目：reflection annotation root collector 的 `dynamicDependencyMethodToken` 解析现在按 root module
  `typedExportedSymbols` 中的 typed function symbol + `MEMBER_DEF` token 精确匹配 callable child，不再要求
  `exportKind == ZR_MODULE_EXPORT_KIND_FUNCTION`。token 解析仍要求唯一匹配、有效 child index 和 pre-trim function-table
  映射；按名 `dynamicDependencyMethodName` 路径继续只匹配 exported function name。
  RED/GREEN：RED 为 reachability fixture 用 `exportKind = ZR_MODULE_EXPORT_KIND_VALUE` 的 typed function symbol 承载
  `dynamicDependencyMethodToken = 0x03000008` 后返回 `Expected TRUE Was FALSE`；generated-C fixture 同样无法保留
  otherwise-unreachable target。GREEN 后 token 解析到 flat index 2，`annotationRoot[0] = 2` 与 `zr_aot_fn_2`
  均可见。source contract 锁定 non-exported token helper、`symbolKind`/`MEMBER_DEF` gate、matched token count
  和 function-table flat-index lookup。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；
  WSL gcc/clang focused reachability 14/0、annotation preserve 11/0、source contracts 23/0、global shared-library smoke 10/0、
  call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0；Windows MSVC Debug 同组 CTest 3/3、reachability 14/0、
  annotation preserve 11/0、source contracts 23/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s5i-dynamic-dependency-non-exported-method-token-root.md`。
  备注：本切片只覆盖当前模块 `MEMBER_DEF` method token 经 typed function symbol 绑定到非导出 callable child；
  不声明跨模块 annotation、field/type dependency、非方法 member token、`@dynamically_accessed` 数据流、
  warning promotion/per-warning suppression、未注解反射 warning、类型/成员级 DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 14:38:53 +08:00 · 10-S5H / 12-S5G dynamic-dependency method-name signature-hash disambiguation ·
  状态：10-S5/12-S5 的 `@dynamic_dependency` 当前模块 exported method name 签名消歧子切片完成；
  完整跨模块 dependency、非导出成员 token、field/type dependency、数据流和 trim analyzer 仍未关闭。
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

- 2026-06-30 14:01:25 +08:00 · 10-S5G / 12-S5F dynamic-dependency exported method-name root ·
  状态：10-S5/12-S5 的 `@dynamic_dependency` 当前模块 exported method name carrier 子切片完成；
  完整跨模块 dependency、非导出成员 token、field/type dependency、数据流和 trim analyzer 仍未关闭。
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

- 2026-06-30 12:54:26 +08:00 · 10-S5E / 12-S7ZU annotation warning suppression ·
  状态：10-S5/12-S7 的 writer-level annotation warning suppression 子切片完成；完整 attribute/annotation-driven
  抑制/提升、数据流和 trim analyzer 仍未关闭。
  完成项目：`SZrAotWriterOptions` 新增 `suppressAnnotationWarnings`；AOT C emitter 在 opt-in code stripping 下
  仍扫描 `requiresUnreferencedCode: true` 静态调用 warning，但在该选项开启时把可见
  `trim_warnings.annotationCount` 置 0、把总数转入 `trim_warnings.annotationSuppressedCount`，并跳过逐条
  `trim_warning.annotation[]` marker。runtime fallback warning count、suppressed count 与 reason mask 不受该选项影响。
  RED/GREEN：RED 为 suppressed fixture 编译失败在 `SZrAotWriterOptions` 缺少 `suppressAnnotationWarnings`；
  GREEN 后 suppressed generated C 输出 annotationCount=0、annotationSuppressedCount=1，且不含逐条 annotation marker。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；
  WSL gcc/clang focused annotation preserve 7/0、global shared-library smoke 10/0、call shared-library smoke 5/0、
  dynamic deopt bridge smoke 7/0、source contracts 22/0；Windows MSVC Debug 同组 CTest 3/3，annotation preserve 7/0，
  source contracts 22/0，三个 Unix-only smoke 为 0 failures / 10 ignored、5 ignored、7 ignored。最终快速复验：
  WSL gcc/clang source contracts 22/0 + annotation preserve 7/0，MSVC Debug source contracts 22/0 + annotation preserve 7/0。
  产出：`tests/acceptance/2026-06-30-aot-12-s7zu-annotation-warning-suppression.md`。
  备注：本切片只提供 writer-level 全局 annotation warning 抑制；不声明属性级 per-warning 抑制、warning promotion、
  `@dynamically_accessed` 数据流、按 token/按名 dynamic dependency、跨模块 annotation、未注解反射 warning 或完整
  metadata sweep 完成。另补 `TYPEOF` reflection runtime fallback warning marker 回归断言，该行为为既有实现的测试补强。

- 2026-06-30 13:22:30 +08:00 · 10-S5F / 12-S5E dynamic-dependency MethodDef token root ·
  状态：10-S5/12-S5 的 `@dynamic_dependency` 当前模块 exported MethodDef token carrier 子切片完成；
  完整按名成员、非导出成员 token、跨模块 dependency、数据流和 trim analyzer 仍未关闭。
  完成项目：reflection annotation root collector 现在读取 function decorator metadata
  `dynamicDependencyMethodToken` uint 字段，要求 token table 为 `MEMBER_DEF`，并只通过 root module
  `typedExportedSymbols` 中的 function export token 解析为 callable child 的 flat function index。解析成功后目标复用
  既有 annotation root 去重与 `ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION` 路径，生成 C 继续输出
  `code_stripping.annotationRoots` / `code_stripping.annotationRoot[index]`。
  RED/GREEN：RED 为 reachability fixture 写入 `dynamicDependencyMethodToken = 0x03000007` 后 annotation root count
  仍为 0；generated-C fixture 也无法保留 otherwise-unreachable target。GREEN 后 token 解析到 exported child flat index 2，
  `annotationRoot[0] = 2` 和 `zr_aot_fn_2` 均可见。source contract 同步保护 `MEMBER_DEF` gate、typed exported symbol
  lookup 与 function-table flat-index lookup。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；
  WSL gcc/clang focused reachability 9/0、annotation preserve 8/0、global shared-library smoke 10/0、
  call shared-library smoke 5/0、dynamic deopt bridge smoke 7/0、source contracts 23/0；Windows MSVC Debug 同组 CTest
  3/3、reachability 9/0、annotation preserve 8/0、source contracts 23/0，三个 Unix-only smoke 为 0 failures /
  10 ignored、5 ignored、7 ignored。
  产出：`tests/acceptance/2026-06-30-aot-10-s5f-dynamic-dependency-method-token-root.md`。
  备注：本切片只覆盖当前模块 exported `MEMBER_DEF` method token 到 function root 的保留；不声明按名成员 dependency、
  field/type dependency、非导出 member token、跨模块 annotation、`@dynamically_accessed` 数据流、warning promotion/
  per-warning suppression、未注解反射 warning、类型/成员级 DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 12:38:52 +08:00 · 10-S5D / 12-S5D dynamic-dependency function root ·
  状态：10-S5/12-S5 的 `@dynamic_dependency` 函数级首个 carrier 子切片完成；完整成员/类型/token
  依赖、数据流和 trim analyzer 仍未关闭。
  完成项目：`backend_aot_collect_reflection_annotation_roots(...)` 现在共享 decorator metadata 字段读取，
  保留既有 `reflectable: true` 行为，同时读取 `dynamicDependencyFunctionIndex` uint metadata，校验目标
  flat function index 存在于 pre-trim function table，去重后把目标加入 annotation roots。生成 C 继续输出
  `code_stripping.annotationRoots` / `code_stripping.annotationRoot[index]`，并通过现有
  `ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION` root reason 进入 reachability。
  RED/GREEN：RED 为 generated-C fixture 在 root function 上写入 `dynamicDependencyFunctionIndex = 2` 后，
  otherwise-unreachable child 仍被裁剪，`annotationRoot[0] = 2` / `zr_aot_fn_2` 断言失败；GREEN 后该 child
  被保留，未标注 prune 路径、reflectable root、requires-unreferenced warning 与 reason text 路径保持通过。
  验证：WSL gcc focused annotation preserve 6/0、reachability 8/0；WSL gcc/clang CTest
  `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；WSL gcc/clang call
  shared-library smoke 5/0、dynamic deopt bridge smoke 7/0、source contracts 22/0；Windows MSVC Debug 同组
  CTest 3/3，Unix-only smoke 为 0 failures / 5 ignored 与 0 failures / 7 ignored，source contracts 22/0；
  `git diff --check` 退出 0（仅 LF/CRLF 提示）。产出：
  `tests/acceptance/2026-06-30-aot-10-s5d-dynamic-dependency-function-root.md`。
  备注：本切片只覆盖单个 flat function index metadata carrier，不声明按 token/按名成员依赖、跨模块
  dependency、`@dynamically_accessed` 数据流、warning 抑制/提升、未注解反射 warning、类型/成员级
  DESCRIPTION 提升或完整 metadata sweep 完成。

- 2026-06-30 12:22:15 +08:00 · 10-S5C / 12-S5C requires-unreferenced-code reason text marker ·
  状态：10-S5/12-S5 的 `@requires_unreferenced_code("reason")` reason 文本透传子切片完成；完整 annotation/dataflow/warning
  策略仍未关闭。
  完成项目：`backend_aot_c_annotation_warnings.c` 现在从 function decorator metadata 中读取
  `requiresUnreferencedCodeReason` 字符串；当 retained caller 静态调用 `requiresUnreferencedCode: true`
  callee 且 reason 非空时，逐条 annotation warning 在既有
  `reason=requires-unreferenced-code` 后追加 quoted/escaped `message="..."`。缺失或非字符串 reason 保持 10-S5B
  的旧 marker 形态，annotation warning 计数仍与 runtime fallback warning 计数/掩码分离。
  RED/GREEN：RED 为新增 reason fixture 期望
  `message="uses \"name\" lookup"` 后，旧 writer 只输出布尔 warning marker；GREEN 后 reason 文本带双引号可正确转义，
  bool-only callee 仍保持无 `message=` 的旧格式，未标注 callee 仍为 0 条 annotation warning。
  验证：WSL gcc focused annotation preserve 5/0；WSL gcc/clang CTest
  `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；WSL gcc/clang call shared-library
  smoke 5/0、dynamic deopt bridge smoke 7/0；WSL gcc/clang source contracts 22/0；Windows MSVC Debug 同组 CTest 3/3，
  两个 Unix-only smoke 为 0 failures / 5 ignored 与 0 failures / 7 ignored，source contracts 22/0。产出：
  `tests/acceptance/2026-06-30-aot-10-s5c-requires-unreferenced-code-reason.md`。
  备注：本切片只覆盖已静态解析的 retained caller -> annotated callee reason 文本诊断，不声明
  `@dynamically_accessed`、`@dynamic_dependency`、warning 抑制/提升、跨模块 annotation、未注解反射 warning 或完整
  trim analyzer 完成。

- 2026-06-30 11:59:59 +08:00 · 10-S5B / 12-S5B requires-unreferenced-code static call warning ·
  状态：10-S5/12-S5 的 `@requires_unreferenced_code` 调用点 warning 子切片完成；完整 annotation/dataflow/warning
  策略仍未关闭。
  完成项目：新增 `backend_aot_c_annotation_warnings.{h,c}`，在 opt-in code stripping 下扫描 retained function table
  中的静态 call 指令；当 caller 静态调用 function decorator metadata 含 `requiresUnreferencedCode: true` 的 callee 时，
  生成 C 输出 `trim_warnings.annotationCount` 与
  `trim_warning.annotation[index] function=<flatIndex> instruction=<index> targetFunction=<flatIndex> reason=requires-unreferenced-code`。
  `backend_aot_resolve_callable_slot_function_index_before_instruction(...)` 同步支持 `GET_SUB_FUNCTION` 写入的 callable slot，
  让子函数静态调用可解析为原始 flat function index。annotation warning 与 `trim_warnings.runtimeFallback*` 计数/掩码分离。
  RED/GREEN：RED 为新增 requires-unreferenced static-call fixture 缺少 annotation warning marker；GREEN 后标注 callee 输出
  1 条 annotation warning，未标注 callee 输出 `trim_warnings.annotationCount = 0` 且没有逐条 annotation warning。
  验证：WSL gcc focused annotation preserve 4/0；WSL gcc/clang CTest
  `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；WSL gcc/clang call shared-library
  smoke 5/0、dynamic deopt bridge smoke 7/0；Windows MSVC Debug 同组 CTest 3/3，两个 Unix-only smoke 为 0 failures
  / 5 ignored 与 0 failures / 7 ignored；WSL gcc/clang source contracts 22/0。产出：
  `tests/acceptance/2026-06-30-aot-10-s5b-requires-unreferenced-code-warning.md`。
  备注：本切片只覆盖 retained static caller -> annotated static callee 的 warning marker；不声明动态反射数据流、
  `@dynamic_dependency`、用户 reason 字符串透传、warning 抑制策略、跨模块 annotation 或完整 trim analyzer 完成。

- 2026-06-30 11:25:44 +08:00 · 10-S5A / 12-S5A reflection annotation function roots ·
  状态：10-S5/12-S5 首个保留注解 root 子切片完成；完整 annotation/dataflow/warning 策略仍未关闭。
  完成项目：AOT reachability reason 新增 `ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION`；
  `backend_aot_collect_reflection_annotation_roots(...)` 扫描 function decorator metadata 中的 `reflectable: true`，
  C emitter 在 opt-in code stripping 前收集这些 flat function roots、传入静态 callable reachability graph，并在生成 C
  文件头部输出 `code_stripping.annotationRoots` 与 `code_stripping.annotationRoot[index]` 诊断。当前使用现有
  compile-time decorator metadata 作为计划级 `@reflectable` 的首个承载面，不新增语法。
  RED/GREEN：RED 为新增 `zr_vm_aot_c_reflection_annotation_preserve_test` 失败在缺少 annotation root marker；
  GREEN 后带 `reflectable: true` metadata 的不可达 child function 保留，去掉 metadata 后同一 child 被裁剪。
  验证：WSL gcc/clang CTest `aot_reachability|aot_c_code_stripping|aot_c_reflection_annotation_preserve` 3/3；
  Windows MSVC Debug 同组 3/3；WSL gcc/clang `zr_vm_aot_c_source_contracts_test` 22/0；`git diff --check` 退出 0
  （仅 LF/CRLF 提示）。产出：`tests/acceptance/2026-06-30-aot-10-s5a-reflectable-annotation-function-root.md`。
  备注：本切片只覆盖函数级 bool metadata root；`@dynamically_accessed` 数据流、`@dynamic_dependency`、
  `@requires_unreferenced_code`、类型/成员级 DESCRIPTION 提升、未注解反射 warning 与完整 trim analyzer 仍待后续。

- 2026-06-30 10:49:57 +08:00 · 10-S2Y / 10-S3AC generated Method.Invoke bool three-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker `bool(bool, bool, bool)` 参数解包与返回装箱子切片完成；
  完整 10-S2/10-S3 仍未关闭。
  完成项目：新增 `backend_aot_c_reflection_bool_three_arg_invokers.h/.c`，生成
  `zr_aot_try_invoke_bool_three_arg(...)` helper；`backend_aot_c_reflection_invokers.c` 在 bool two-arg helper 后写出该 helper，
  并在 entry thunk 中接入 bool three-arg 调度；`backend_aot_c_typed_bool_three_arg_thunks.c` 补齐
  `left && middle && right` 当前 cleanup-reset 短路 AND 形态识别。helper 校验 bool return、三个 bool parameter base type、
  三个 `args` 类型和 `outReturn`，按 `functionIndex` 调用 bool three-arg typed helper，再用
  `ZrCore_Value_InitAsBool(...)` 写回 boxed bool。
  RED/GREEN：RED 为 frame setup source contract 要求 bool three-arg reflection bucket 时，WSL gcc 失败在
  `reflectionBoolThreeArgInvokersSourceText` 非空断言，因为新 invoker source file 尚不存在；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `all_truth(true,true,true)` 返回 `ZR_VALUE_TYPE_BOOL`/true。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2y-method-invoke-bool-three-arg-unbox-return-boxing.md`。
  备注：最终 `backend_aot_c_reflection_invokers.c` 为 953 行，`backend_aot_c_reflection_bool_three_arg_invokers.c`
  为 85 行，`backend_aot_c_typed_bool_three_arg_thunks.c` 为 335 行，`backend_aot_c_reflection_numeric_three_arg_invokers.c`
  为 296 行，`backend_aot_c_reflection_bool_numeric_invokers.c` 为 240 行，`backend_aot_c_method_metadata.c` 为 646 行。
  仍未覆盖四参数及以上、object/inline 返回、numeric widening、实例 receiver、public `MethodInfo` 对象、
  MethodSpec 专用 code slot、cross-module token rewrite 和 full trim analyzer。

- 2026-06-30 10:13:18 +08:00 · 10-S2X / 10-S3AB generated Method.Invoke f64 three-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker `float(float, float, float)` 参数解包与返回装箱子切片完成；
  完整 10-S2/10-S3 仍未关闭。
  完成项目：扩展 `backend_aot_c_reflection_numeric_three_arg_invokers.h/.c`，新增
  `zr_aot_try_invoke_f64_three_arg(...)` generated helper；`backend_aot_c_typed_f64_thunks.h` 暴露 existing
  three-arg/state-free eligibility predicates；`backend_aot_c_reflection_invokers.c` 在 f64 two-arg helper 后写出该 helper，
  并在 entry thunk 中接入 f64 three-arg 调度。helper 校验 double return、三个 double parameter base type、
  三个 `args` 类型和 `outReturn`，按 `functionIndex` 调用 state-free 或 stateful f64 three-arg typed helper，
  再用 `ZrCore_Value_InitAsFloat(...)` 写回 boxed double。
  RED/GREEN：RED 为 frame setup source contract 要求 f64 three-arg reflection bucket 时，WSL gcc 失败在缺少
  `static TZrBool backend_aot_c_method_metadata_has_f64_three_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `sum_three_ratio(1.5,2.25,3.25)` 返回 `ZR_VALUE_TYPE_DOUBLE`/7.0。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2x-method-invoke-f64-three-arg-unbox-return-boxing.md`。
  备注：最终 `backend_aot_c_reflection_invokers.c` 为 948 行，`backend_aot_c_reflection_numeric_three_arg_invokers.c`
  为 296 行，`backend_aot_c_reflection_bool_numeric_invokers.c` 为 240 行，`backend_aot_c_method_metadata.c` 为 646 行。
  仍未覆盖 bool 三参数桶、四参数及以上、object/inline 返回、numeric widening、实例 receiver、public `MethodInfo`
  对象、MethodSpec 专用 code slot、cross-module token rewrite 和 full trim analyzer。

- 2026-06-30 09:52:48 +08:00 · 10-S2W / 10-S3AA generated Method.Invoke uint64 three-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker `uint64(uint64, uint64, uint64)` 参数解包与返回装箱子切片完成；
  完整 10-S2/10-S3 仍未关闭。
  完成项目：扩展 `backend_aot_c_reflection_numeric_three_arg_invokers.h/.c`，新增
  `zr_aot_try_invoke_u64_three_arg(...)` generated helper；`backend_aot_c_reflection_invokers.c` 在 u64
  two-arg helper 后写出该 helper，并在 entry thunk 中接入 u64 three-arg 调度。helper 校验 uint64 return、三个
  uint64 parameter base type、三个 `args` 类型和 `outReturn`，按 `functionIndex` 调用 state-free 或 stateful
  u64 three-arg typed helper，再用 `ZrCore_Value_InitAsUInt(...)` 写回 boxed uint64。
  RED/GREEN：RED 为 frame setup source contract 要求 u64 numeric three-arg reflection bucket 时，WSL gcc 失败在缺少
  `#include "backend_aot_c_typed_u64_three_arg_thunks.h"`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `sum_three_unsigned(50,20,5)` 返回 `ZR_VALUE_TYPE_UINT64`/75。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2w-method-invoke-uint64-three-arg-unbox-return-boxing.md`。
  备注：最终 `backend_aot_c_reflection_invokers.c` 为 944 行，`backend_aot_c_reflection_numeric_three_arg_invokers.c`
  为 198 行，`backend_aot_c_reflection_bool_numeric_invokers.c` 为 240 行，`backend_aot_c_method_metadata.c` 为 646 行。
  仍未覆盖 f64/bool 三参数桶、四参数及以上、object/inline 返回、numeric widening、实例 receiver、public `MethodInfo`
  对象、MethodSpec 专用 code slot、cross-module token rewrite 和 full trim analyzer。

- 2026-06-30 09:32:55 +08:00 · 10-S2V / 10-S3Z generated Method.Invoke int64 three-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker `int64(int64, int64, int64)` 参数解包与返回装箱子切片完成；
  完整 10-S2/10-S3 仍未关闭。
  完成项目：新增 `backend_aot_c_reflection_numeric_three_arg_invokers.h/.c`，承载 numeric three-arg invoker
  case 选择和 generated helper 发射；`backend_aot_c_reflection_invokers.c` 在 i64 two-arg 后写出
  `zr_aot_try_invoke_i64_three_arg(...)` 并在 entry thunk 中优先调度；`backend_aot_c_typed_i64_thunks.h`
  暴露已有 three-arg 与 three-arg state-free eligibility predicates。helper 校验 int64 return、三个 int64
  parameter base type、三个 `args` 类型和 `outReturn`，按 `functionIndex` 调用 state-free 或 stateful i64
  three-arg typed helper，再用 `ZrCore_Value_InitAsInt(...)` 写回 boxed int64。
  RED/GREEN：RED 为 frame setup source contract 要求 numeric three-arg reflection bucket 时，WSL gcc 失败在缺少
  `#include "backend_aot_c_reflection_numeric_three_arg_invokers.h"`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `sum_three(10,20,12)` 返回 `ZR_VALUE_TYPE_INT64`/42。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2v-method-invoke-int64-three-arg-unbox-return-boxing.md`。
  备注：为避免继续扩张 935 行的反射 invoker 文件，本切片拆出 100 行 numeric three-arg 子模块；最终
  `backend_aot_c_reflection_invokers.c` 为 940 行，`backend_aot_c_reflection_bool_numeric_invokers.c` 为 240 行，
  `backend_aot_c_method_metadata.c` 为 646 行。仍未覆盖 u64/f64/bool 三参数桶、四参数及以上、object/inline 返回、
  numeric widening、实例 receiver、public `MethodInfo` 对象、MethodSpec 专用 code slot、cross-module token rewrite
  和 full trim analyzer。

- 2026-06-30 09:05:37 +08:00 · 10-S2U / 10-S3Y generated Method.Invoke bool-return numeric comparison two-arg argument unbox + return boxing buckets ·
  状态：generated reflection invoker `bool(int,int)`、`bool(uint,uint)`、`bool(float,float)` 参数解包与返回装箱子切片完成；
  完整 10-S2/10-S3 仍未关闭。
  完成项目：新增 `backend_aot_c_reflection_bool_numeric_invokers.h/.c`，承载 bool-return numeric comparison two-arg invoker
  case 选择和 generated helper 发射；`backend_aot_c_reflection_invokers.c` 只编排三组 helper 并在 entry thunk 中按
  i64/u64/f64 顺序调度；`backend_aot_c_typed_bool_thunks.h` 暴露已有
  `backend_aot_c_can_emit_typed_bool_i64_two_arg_thunk()`、
  `backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk()` 和
  `backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk()`。
  RED/GREEN：RED 为 frame setup source contract 要求 bool-return numeric reflection buckets 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_bool_i64_two_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `less_values(3,7)`、`unsigned_after(9,4)`、`ratio_equal(2.5,2.5)` 均返回 boxed bool true。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2u-method-invoke-bool-numeric-two-arg-unbox-return-boxing.md`。
  备注：为避免继续扩张 922 行的反射 invoker 文件，本切片拆出 240 行 bool numeric 子模块；最终
  `backend_aot_c_reflection_invokers.c` 为 935 行，`backend_aot_c_method_metadata.c` 为 646 行。
  仍未覆盖三参数及以上、object/inline 返回、numeric widening、实例 receiver、public `MethodInfo` 对象、
  MethodSpec 专用 code slot、cross-module token rewrite 和 full trim analyzer。

- 2026-06-30 08:35:02 +08:00 · 10-S2T / 10-S3X generated Method.Invoke f64 two-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker float(float, float) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_f64_thunks.h` 暴露 two-arg 与 state-free eligibility predicate；
  `backend_aot_c_reflection_invokers.c` 现在发射 `zr_aot_try_invoke_f64_two_arg(...)`，检查 MethodInfo signature 的
  double return、`parameterCount == 2`、两个 double parameter base type、两个 `args` 类型与 `outReturn`，
  按 `method->functionIndex` switch 到既有 f64 two-arg typed helper；state-free helper 走
  `zr_aot_typed_f64_fn_<index>(zr_aot_arg0, zr_aot_arg1)`，stateful divide/modulo helper 保留 `state` 参数。
  RED/GREEN：RED 为 frame setup source contract 要求 f64 two-arg bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_f64_two_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `sum_ratio(left: float, right: float): float` 的 reflection invoker 对实参 1.25/2.5
  写出 `ZR_VALUE_TYPE_DOUBLE`/3.75。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、reflection token resolve、
  reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2t-method-invoke-f64-two-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated f64(float,float) two-arg bucket；bool-return numeric comparison buckets、
  三参数及以上、object/inline 返回、numeric widening、实例 receiver、public `MethodInfo` 对象、
  MethodSpec 专用 code slot、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 08:18:43 +08:00 · 10-S2S / 10-S3W generated Method.Invoke bool two-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker bool(bool, bool) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_bool_thunks.h` 暴露 two-arg eligibility predicate；
  `backend_aot_c_reflection_invokers.c` 现在发射 `zr_aot_try_invoke_bool_two_arg(...)`，检查 MethodInfo signature 的
  bool return、`parameterCount == 2`、两个 bool parameter base type、两个 `args` 类型与 `outReturn`，
  按 `method->functionIndex` switch 到既有 bool two-arg typed helper 并写回 boxed bool。
  RED/GREEN：RED 为 frame setup source contract 要求 bool two-arg bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_bool_two_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `same_truth(left: bool, right: bool): bool` 的 reflection invoker 对实参 true/true
  写出 `ZR_VALUE_TYPE_BOOL`/true。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、reflection token resolve、
  reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2s-method-invoke-bool-two-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated bool(bool,bool) two-arg bucket；bool-return numeric comparison buckets、f64 二参数桶、
  三参数及以上、object/inline 返回、numeric widening、实例 receiver、public `MethodInfo` 对象、
  MethodSpec 专用 code slot、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 08:03:24 +08:00 · 10-S2R / 10-S3V generated Method.Invoke uint64 two-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker uint64(uint64, uint64) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_u64_thunks.h` 暴露 two-arg 与 state-free eligibility predicate；
  `backend_aot_c_reflection_invokers.c` 现在发射 `zr_aot_try_invoke_u64_two_arg(...)`，检查 MethodInfo signature 的
  uint64 return、`parameterCount == 2`、两个 uint64 parameter base type、两个 `args` 类型与 `outReturn`，
  按 `method->functionIndex` switch 到既有 u64 two-arg typed helper；state-free helper 直接传两个 `TZrUInt64`
  参数，需要 `state` 的 divide/modulo helper 仍传 `state` 保留错误路径。
  RED/GREEN：RED 为 frame setup source contract 要求 u64 two-arg bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_u64_two_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `sum_unsigned(left: uint, right: uint): uint` 的 reflection invoker 对实参 100/23
  写出 `ZR_VALUE_TYPE_UINT64`/123。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke 13/0
  （MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding 2/0、
  metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、reflection token resolve、
  reflection method invoke、typed direct-call compatibility、metadata binding loader 和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2r-method-invoke-uint64-two-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated u64 two-arg bucket；bool/f64 二参数桶、三参数及以上、object/inline 返回、
  numeric widening、实例 receiver、public `MethodInfo` 对象、MethodSpec 专用 code slot、cross-module token rewrite
  和 full trim analyzer 仍待后续。

- 2026-06-30 07:45:38 +08:00 · 10-S2Q / 10-S3U generated Method.Invoke int64 two-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker int64(int64, int64) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_i64_thunks.h` 暴露 two-arg 与 state-free eligibility predicate；
  `backend_aot_c_reflection_invokers.c` 现在发射 `zr_aot_try_invoke_i64_two_arg(...)`，检查 MethodInfo signature 的
  int64 return、`parameterCount == 2`、两个 int64 parameter base type、两个 `args` 类型与 `outReturn`，
  按 `method->functionIndex` switch 到既有 i64 two-arg typed helper；state-free helper 直接传两个 `TZrInt64`
  参数，需要 `state` 的 divide/modulo helper 仍传 `state` 保留错误路径。
  RED/GREEN：RED 为 frame setup source contract 要求 i64 two-arg bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_i64_two_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `sum_values(left: int, right: int): int` 的 reflection invoker 对实参 20/22
  写出 `ZR_VALUE_TYPE_INT64`/42。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  产出：`tests/acceptance/2026-06-30-aot-10-s2q-method-invoke-int64-two-arg-unbox-return-boxing.md`。
  备注：本记录只关闭首个 generated i64 two-arg bucket；u64/bool/f64 二参数桶、三参数及以上、object/inline 返回、
  numeric widening、实例 receiver、public `MethodInfo` 对象、MethodSpec 专用 code slot、cross-module token rewrite
  和 full trim analyzer 仍待后续。

- 2026-06-30 07:21:33 +08:00 · 10-S2P / 10-S3T generated Method.Invoke f64 one-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker float(float) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：`backend_aot_c_typed_f64_thunks.h` 暴露 one-arg eligibility predicate；`backend_aot_c_reflection_invokers.c`
  现在发射 `zr_aot_try_invoke_f64_one_arg(...)`，检查 MethodInfo signature 的 double return、`parameterCount == 1`、
  `parameterTypes[0].baseType == ZR_VALUE_TYPE_DOUBLE`、`args[0].type == ZR_VALUE_TYPE_DOUBLE` 与 `outReturn`，
  按 `method->functionIndex` switch 到既有 `zr_aot_typed_f64_fn_<index>(zr_aot_arg0)`，从 `args[0]` 解出
  `TZrFloat64` 并用 `ZrCore_Value_InitAsFloat(...)` 打包返回值。
  RED/GREEN：RED 为 frame setup source contract 要求 f64 one-arg bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_f64_one_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `echo_ratio(value: float): float` 的 reflection invoker 对实参 1.75 写出
  `ZR_VALUE_TYPE_DOUBLE`/1.75。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2p-method-invoke-f64-one-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated f64 one-arg bucket；多参数、object/inline 返回、numeric widening、实例 receiver、
  public `MethodInfo` 对象、MethodSpec 专用 code slot、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 07:05:20 +08:00 · 10-S2O / 10-S3S generated Method.Invoke bool one-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker bool(bool) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：拆分后的 `backend_aot_c_reflection_invokers.c` 现在发射 `zr_aot_try_invoke_bool_one_arg(...)`，
  检查 MethodInfo signature 的 bool return、`parameterCount == 1`、`parameterTypes[0].baseType == ZR_VALUE_TYPE_BOOL`、
  `args[0].type == ZR_VALUE_TYPE_BOOL` 与 `outReturn`，按 `method->functionIndex` switch 到既有
  `zr_aot_typed_bool_fn_<index>(zr_aot_arg0)`，从 `args[0]` 解出 `TZrBool` 并用
  `ZrCore_Value_InitAsBool(...)` 打包返回值。
  RED/GREEN：RED 为 frame setup source contract 要求 bool one-arg bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_bool_one_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `echo_truth(value: bool): bool` 的 reflection invoker 对实参 false 写出
  `ZR_VALUE_TYPE_BOOL`/false。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2o-method-invoke-bool-one-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated bool one-arg bucket；多参数、其他标量参数、object/inline 返回、numeric widening、
  实例 receiver、public `MethodInfo` 对象、MethodSpec 专用 code slot、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 06:45:51 +08:00 · 10-S2-maint / 10-S3-maint reflection invoker emitter module split ·
  状态：generated reflection invoker 发射逻辑从 MethodInfo metadata 发射文件中拆出；这是后续 10-S2/10-S3 签名桶的维护前置，
  不关闭完整 10-S2/10-S3。
  完成项目：新增 `backend_aot_c_reflection_invokers.h/.c`，把 `backend_aot_write_c_reflection_invokers(...)`、
  scalar no-arg return-boxing buckets 和 i64/u64 one-arg argument-unbox buckets 迁入独立模块；
  `backend_aot_c_emitter.c` 直接包含 invoker 头；`backend_aot_c_method_metadata.c` 保留 MethodInfo/signature/method-token/GC-root
  发射职责，行数从 948 降为 552，新 invoker 文件 397 行。
  RED/GREEN：拆分前先以现有 u64 one-arg source contract 和 shared-library smoke 作为行为基线；拆分后 GREEN 为 WSL gcc
  frame setup contracts 1/0、shared-library smoke 13/0，CMake glob 重新纳入 `backend_aot_c_reflection_invokers.c`。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2-maint-reflection-invoker-emitter-split.md`。
  备注：本记录只关闭模块边界整理；不新增签名桶、不改变 generated ABI，不声明 public `MethodInfo` 对象、MethodSpec 专用 code slot、
  cross-module token rewrite 或 full trim analyzer 完成。

- 2026-06-30 06:25:21 +08:00 · 10-S2N / 10-S3R generated Method.Invoke uint64 one-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker uint64(uint64) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：generated C 现在发射 `zr_aot_try_invoke_u64_one_arg(...)`，检查 MethodInfo signature 的 uint64 return、
  `parameterCount == 1`、`parameterTypes[0].baseType == ZR_VALUE_TYPE_UINT64`、`args[0].type == ZR_VALUE_TYPE_UINT64`
  与 `outReturn`，按 `method->functionIndex` switch 到既有 `zr_aot_typed_u64_fn_<index>(zr_aot_arg0)`，
  从 `args[0]` 解出 `TZrUInt64` 并用 `ZrCore_Value_InitAsUInt(...)` 打包返回值。
  RED/GREEN：RED 为 frame setup source contract 要求 u64 one-arg bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_u64_one_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `echo_unsigned(value: uint): uint` 的 reflection invoker 对实参 101 写出
  `ZR_VALUE_TYPE_UINT64`/101。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2n-method-invoke-uint64-one-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated uint64 one-arg bucket；多参数、其他标量参数、object/inline 返回、numeric widening、
  实例 receiver、public `MethodInfo` 对象、MethodSpec 专用 code slot、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 06:05:32 +08:00 · 10-S2M / 10-S3Q generated Method.Invoke int64 one-arg argument unbox + return boxing bucket ·
  状态：generated reflection invoker int64(int64) 参数解包与返回装箱子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：generated C 现在发射 `zr_aot_try_invoke_i64_one_arg(...)`，检查 MethodInfo signature 的 int64 return、
  `parameterCount == 1`、`parameterTypes[0].baseType == ZR_VALUE_TYPE_INT64`、`args[0].type == ZR_VALUE_TYPE_INT64`
  与 `outReturn`，按 `method->functionIndex` switch 到既有 `zr_aot_typed_i64_fn_<index>(zr_aot_arg0)`，
  从 `args[0]` 解出 `TZrInt64` 并用 `ZrCore_Value_InitAsInt(...)` 打包返回值。
  RED/GREEN：RED 为 frame setup source contract 要求 i64 one-arg bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_i64_one_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `echo(value: int): int` 的 reflection invoker 对实参 99 写出
  `ZR_VALUE_TYPE_INT64`/99。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2m-method-invoke-int64-one-arg-unbox-return-boxing.md`。
  备注：本记录只关闭 generated int64 one-arg bucket；多参数、其他标量参数、object/inline 返回、numeric widening、
  实例 receiver、public `MethodInfo` 对象、MethodSpec 专用 code slot、cross-module token rewrite 和 full trim analyzer 仍待后续。

- 2026-06-30 05:44:51 +08:00 · 10-S2L / 10-S3P generated Method.Invoke f64 no-arg return boxing bucket ·
  状态：generated reflection invoker f64/no-arg return boxing 子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：generated C 现在发射 `zr_aot_try_invoke_f64_no_arg(...)`，检查 MethodInfo signature 的 return/base type、
  `parameterCount == 0` 与 `outReturn`，按 `method->functionIndex` switch 到既有 `zr_aot_typed_f64_fn_<index>()`，
  并用 `ZrCore_Value_InitAsFloat(...)` 打包返回值。unsupported case 仍执行完整 entry thunk，但不会把
  entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：RED 为 frame setup source contract 要求 f64 bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_f64_no_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `ratio(): float` 的 reflection invoker 写出 `ZR_VALUE_TYPE_DOUBLE`/2.5。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2l-method-invoke-f64-no-arg-return-boxing.md`。
  备注：本记录只关闭 generated f64 no-arg return boxing bucket；args unbox、object/inline 返回、
  numeric widening、完整签名桶、public `MethodInfo` 对象、MethodSpec 专用 code slot、cross-module token rewrite
  和 full trim analyzer 仍待后续。

- 2026-06-30 05:26:08 +08:00 · 10-S2K / 10-S3O generated Method.Invoke bool no-arg return boxing bucket ·
  状态：generated reflection invoker bool/no-arg return boxing 子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：generated C 现在发射 `zr_aot_try_invoke_bool_no_arg(...)`，检查 MethodInfo signature 的 return/base type、
  `parameterCount == 0` 与 `outReturn`，按 `method->functionIndex` switch 到既有 `zr_aot_typed_bool_fn_<index>()`，
  并用 `ZrCore_Value_InitAsBool(...)` 打包返回值。unsupported case 仍执行完整 entry thunk，但不会把
  entry thunk 的执行成功返回值当成业务返回。
  RED/GREEN：RED 为 frame setup source contract 要求 bool bucket 时，WSL gcc 失败在缺少
  `backend_aot_c_method_metadata_has_bool_no_arg_reflection_case(`；GREEN 后 frame setup contracts 1/0、
  shared-library smoke 13/0，并验证 `truth(): bool` 的 reflection invoker 写出 `ZR_VALUE_TYPE_BOOL`/true。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、
  shared-library smoke 13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、
  metadata runtime method binding 2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、
  method binding、reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  工具检查：`git diff --check` 通过；仅报告 Git 换行转换提示，无空白错误。
  产出：`tests/acceptance/2026-06-30-aot-10-s2k-method-invoke-bool-no-arg-return-boxing.md`。
  备注：本记录只关闭 generated bool no-arg return boxing bucket；args unbox、f64/object/inline 返回、
  numeric widening、完整签名桶、public `MethodInfo` 对象、MethodSpec 专用 code slot、cross-module token rewrite
  和 full trim analyzer 仍待后续。

- 2026-06-30 05:06:13 +08:00 · 10-S2J / 10-S3N generated Method.Invoke uint64 no-arg return boxing bucket ·
  状态：generated reflection invoker uint64/no-arg return boxing 子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：generated C 现在发射 `zr_aot_try_invoke_u64_no_arg(...)`，检查 MethodInfo signature 的 return/base type、
  `parameterCount == 0` 与 `outReturn`，按 `method->functionIndex` switch 到既有 `zr_aot_typed_u64_fn_<index>()`，
  并用 `ZrCore_Value_InitAsUInt(...)` 打包返回值。unsupported case 仍执行完整 entry thunk，但不会把
  `FZrAotEntryThunk` 的返回值写入 `outReturn`。
  RED/GREEN：RED 为 generated source contract 要求 `backend_aot_c_method_metadata_has_u64_no_arg_reflection_case(`
  后缺少 u64 reflection bucket；GREEN 后 source contract 1/0、shared-library smoke 13/0，并通过
  `unsigned_answer(): uint` 的 generated invoker 实际写出 `ZR_VALUE_TYPE_UINT64`/13。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke
  13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding
  2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  `git diff --check` 退出 0，仅有既有 LF/CRLF 规范化警告。
  产出：`tests/acceptance/2026-06-30-aot-10-s2j-method-invoke-uint64-no-arg-return-boxing.md`。
  备注：本记录只关闭 generated uint64 no-arg return boxing bucket；args unbox、bool/f64/object/inline 返回、
  numeric widening、完整签名桶、public `MethodInfo` 对象、MethodSpec 专用 code slot、cross-module token rewrite
  和 full trim analyzer 仍待后续。

- 2026-06-30 04:45:07 +08:00 · 10-S2I / 10-S3M generated Method.Invoke int64 no-arg return boxing bucket ·
  状态：generated reflection invoker int64/no-arg return boxing 子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：generated C 现在发射 `zr_aot_try_invoke_i64_no_arg(...)`，检查 MethodInfo signature 的 return/base type、
  `parameterCount == 0` 与 `outReturn`，按 `method->functionIndex` switch 到既有 `zr_aot_typed_i64_fn_<index>()`，
  并用 `ZrCore_Value_InitAsInt(...)` 打包返回值。unsupported case 仍执行完整 entry thunk，但不会把
  `FZrAotEntryThunk` 的返回值写入 `outReturn`。
  RED/GREEN：初始 RED 为 generated source contract 缺少 `zr_vm_core/value.h`；随后 naive raw target-return 捕获在
  shared-library smoke 中暴露 `Expected 42 Was 1`，确认完整 entry thunk 返回执行成功标志而非业务返回；修正后的 RED
  要求 invoker emitter 接收 function table；GREEN 后 source contract 1/0、shared-library smoke 13/0。
  验证：WSL gcc/clang/MSVC Debug 均通过 frame setup contracts 1/0、source contracts 22/0、shared-library smoke
  13/0（MSVC 为 13 ignored Unix-only）、reflection method invoke 5/0、reflection token resolve 7/0、method binding
  2/0、metadata runtime query 24/0；三平台 CTest 覆盖 metadata runtime query、method binding、
  reflection token resolve、reflection method invoke、typed direct-call compatibility、metadata binding loader
  和 method info signature，均 7/7。
  `git diff --check` 退出 0，仅有既有 LF/CRLF 规范化警告。
  产出：`tests/acceptance/2026-06-30-aot-10-s2i-method-invoke-int64-no-arg-return-boxing.md`。
  备注：本记录只关闭 generated int64 no-arg return boxing bucket；args unbox、bool/u64/f64/object/inline 返回、
  numeric widening、完整签名桶、public `MethodInfo` 对象、MethodSpec 专用 code slot、cross-module token rewrite
  和 full trim analyzer 仍待后续。

- 2026-06-30 04:17:27 +08:00 · 10-S2H / 10-S3L Method.Invoke void return-slot canonicalization ·
  状态：public counted token-driven `Method.Invoke` void/no-return return-slot canonicalization 子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：`reflection_token_resolve.c` 在 counted dispatcher 完成 registered invoker dispatch 后，若 MethodInfo signature
  声明 `hasReturnValue == 0`，则把最终 `outReturn` 重置为 null。即使 invoker 误写返回槽，void/no-return 方法对外也不会
  暴露 stale 或伪造返回值。focused `zr_vm_reflection_method_invoke_test` 新增 invoker 误写 void 返回槽的规范化用例。
  RED/GREEN：RED 为 no-return signature 下 synthetic invoker 写出 int64 后旧 dispatcher 仍保留 `outReturn.type=INT64`，
  WSL gcc 失败在 `Expected 0 Was 5`；GREEN 后调用仍成功、invoker 被调用一次，但 `outReturn.type` 规范为 null。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection method invoke 5/0、reflection token resolve 7/0、
  method binding 2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding|metadata_runtime_query` 均 4/4。
  产出：`tests/acceptance/2026-06-30-aot-10-s2h-method-invoke-void-return-slot.md`。
  备注：本记录只定义 void/no-return 的 public `outReturn` 形态；不声明返回 box、typed return register 捕获、参数 unbox、
  numeric widening、nullable/ownership/staticCType 兼容、public method reflection object、cross-module token rewrite 或 full trim analyzer。

- 2026-06-30 04:06:53 +08:00 · 10-S2G / 10-S3K Method.Invoke required return-slot reset guard ·
  状态：public counted token-driven `Method.Invoke` required return-slot reset 子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：`reflection_token_resolve.c` 在 counted dispatcher 通过 token binding、arity、shape 和参数 guard 后，
  若 MethodInfo signature 声明 required return，则在调用 registered invoker 前把 `outReturn` 重置为 null。
  这样 10-S2F/10-S3J 的 return base-type post-guard 只能接受 invoker 本次写出的返回值，预填旧值不再误通过。
  focused `zr_vm_reflection_method_invoke_test` 新增 stale return slot 用例；旧 arity fixture 改为由 synthetic invoker
  明确写出 int64 返回，避免继续依赖预填返回槽。
  RED/GREEN：RED 为 bool return signature 下 `outReturn` 预填 bool、但 synthetic invoker 不写返回值时旧 dispatcher
  仍返回 true，WSL gcc 失败在 `Expected FALSE Was TRUE`；GREEN 后 dispatch 会先清空 return slot，未写返回值返回 false。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection method invoke 4/0、reflection token resolve 7/0、
  method binding 2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding|metadata_runtime_query` 均 4/4。
  产出：`tests/acceptance/2026-06-30-aot-10-s2g-method-invoke-return-slot-reset.md`。
  备注：本记录只关闭 required return slot stale-value guard；不声明返回 box、typed return register 捕获、参数 unbox、
  numeric widening、nullable/ownership/staticCType 兼容、public method reflection object、cross-module token rewrite 或 full trim analyzer。

- 2026-06-30 03:47:49 +08:00 · 10-S2F / 10-S3J Method.Invoke return base-type guard ·
  状态：public counted token-driven `Method.Invoke` return base-type guard 子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：`reflection_token_resolve.c` 在 counted dispatcher 调用 registered invoker 后，若 MethodInfo signature
  声明 concrete 非 null/unknown `returnType->baseType`，则要求 `outReturn->type` 匹配；越界 return baseType 拒绝成功结果。
  focused `zr_vm_reflection_method_invoke_test` 新增 wrong return type 用例，证明 post-dispatch guard 生效。
  RED/GREEN：RED 为 invoker 在 bool return signature 下写出 int64 时旧 dispatcher 仍返回 true，WSL gcc 失败在
  `Expected FALSE Was TRUE`；GREEN 后 mismatched return type 返回 false，写出 bool 后返回 true。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection method invoke 3/0、reflection token resolve 7/0、
  method binding 2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding` 均 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s2f-method-invoke-return-base-type-guard.md`。
  备注：本记录只关闭 concrete return baseType 的 post-dispatch guard；不声明返回 box、无副作用预检、typed unbox、
  numeric widening、nullable/ownership/staticCType 兼容、public method reflection object、cross-module token rewrite 或 full trim analyzer。

- 2026-06-30 03:37:13 +08:00 · 10-S2E / 10-S3I Method.Invoke fixed parameter base-type guard ·
  状态：public counted token-driven `Method.Invoke` fixed parameter base-type guard 子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：`reflection_token_resolve.c` 将 counted dispatcher 的签名检查扩展为 argument guard：在通过 token→binding、
  arity 和 shape 后，遍历 fixed `parameterTypes[]`，对 concrete 非 null/unknown `baseType` 要求对应 `args[i].type`
  精确匹配；baseType 越界会拒绝 dispatch，varargs 额外参数和 untyped/null/unknown slot 暂不强制。focused
  `zr_vm_reflection_method_invoke_test` 新增错类型参数不调用 invoker 的用例，旧 arity fixture 同步填入真实运行时参数类型。
  RED/GREEN：RED 为新 focused 测试声明第二个参数 `baseType=BOOL`、实际 `SZrTypeValue.type=INT64` 时旧 dispatcher
  仍返回 true 并调用 invoker，WSL gcc 失败在 `Expected FALSE Was TRUE`；GREEN 后错类型被拒绝，改为 bool 后正常派发。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection method invoke 2/0、reflection token resolve 7/0、
  method binding 2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding` 均 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s2e-method-invoke-parameter-base-type-guard.md`。
  备注：本记录只关闭 known fixed parameter baseType 的 invoker 前 guard；不声明 typed unbox、numeric widening、
  nullable/ownership/staticCType 兼容、返回 box、public method reflection object、cross-module token rewrite 或 full trim analyzer。

- 2026-06-30 03:22:37 +08:00 · 10-S2D / 10-S3H Method.Invoke signature shape guard ·
  状态：public counted token-driven `Method.Invoke` signature shape guard 子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：新增 focused `zr_vm_reflection_method_invoke_test` 目标；`ZrCore_Reflection_InvokeMethodTokenWithArgCount(...)`
  在 arity guard 外继续检查 MethodInfo signature shape，`parameterCount > 0` 时要求 `parameterTypes` 非空，
  `hasReturnValue` 时要求 `returnType` 非空，拒绝不完整 shape 且不调用 registered invoker。
  RED/GREEN：RED 为新 focused 测试证明缺 `parameterTypes` 时旧 counted dispatcher 仍返回 true 并调用 invoker；
  GREEN 后缺参数类型表、缺返回类型描述均被拒绝，补齐 shape 后可正常 dispatch。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection method invoke 1/0、reflection token resolve 7/0、method
  binding 2/0、metadata runtime query 24/0；三平台 CTest
  `reflection_method_invoke|reflection_token_resolve|metadata_runtime_method_binding` 均 3/3。
  产出：`tests/acceptance/2026-06-30-aot-10-s2d-method-invoke-signature-shape-guard.md`。
  备注：本记录只关闭 counted dispatcher 的 signature shape guard；不声明参数类型兼容匹配、typed unbox、返回 box、
  public method reflection object、MethodSpec 专用 code slot、cross-module token rewrite 或 full trim analyzer 完成。

- 2026-06-30 03:09:55 +08:00 · 10-S2C / 10-S3G token-driven Method.Invoke signature arity guard ·
  状态：public counted token-driven `Method.Invoke` arity guard 子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：新增 `ZrCore_Reflection_InvokeMethodTokenWithArgCount(...)`，复用 10-S2B/10-S3F 的
  token→binding→registered invoker path，并在 dispatch 前读取 `methodInfo->signature->parameterCount` 与
  `hasVarArgs`。非 varargs 精确匹配参数数量，varargs 接受 `argCount >= parameterCount`，非零参数数量下 null
  `args` 会被拒绝且不会调用 invoker。
  RED/GREEN：RED 为 focused reflection token resolve 测试先引用缺失的 counted dispatcher API，WSL gcc 出现
  implicit declaration 和 undefined reference；GREEN 后 exact arity、mismatch arity、null args 和 varargs 扩展路径通过，
  当前 reflection token resolve 测试数升为 7/0。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection token resolve 7/0、method binding 2/0、metadata runtime
  query 24/0；WSL gcc 与 MSVC Debug CTest `reflection_token_resolve|metadata_runtime_method_binding` 均 2/2。
  产出：`tests/acceptance/2026-06-30-aot-10-s2c-method-invoke-signature-arity-guard.md`。
  备注：本记录只关闭 token-driven invoke 的 signature arity guard；不声明参数类型 unbox、返回 box、public
  method reflection object、MethodSpec 专用 code slot、cross-module token rewrite 或 full trim analyzer 完成。

- 2026-06-30 02:57:41 +08:00 · 10-S2B / 10-S3F token-driven Method.Invoke dispatcher ·
  状态：public token-driven `Method.Invoke` dispatcher 子切片完成；完整 10-S2/10-S3 仍未关闭。
  完成项目：`reflection.h` 新增 `ZrCore_Reflection_InvokeMethodToken(...)`；`reflection_token_resolve.c`
  先用 `ZrCore_Reflection_ResolveToken()` 解析 method token，再要求 resolved token 带
  `methodInfo`、`methodFunctionPointer` 和 `methodInvoker`，最后把 state、target、method、self、args、
  outReturn 原样派发到 registered AOT invoker。新增 focused 测试捕获 invoker 的全部实参，并覆盖 null
  state/runtime/outReturn 与非 method token 负向路径。
  RED/GREEN：RED 为 `test_reflection_token_resolve.c` 先调用缺失的
  `ZrCore_Reflection_InvokeMethodToken(...)`，WSL gcc 编译出现 implicit declaration 和 undefined reference；
  GREEN 后 dispatcher 成功调用 synthetic AOT invoker，reflection token resolve 目标通过 6/0。
  验证：WSL gcc 通过 reflection token resolve 6/0、method binding 2/0、metadata runtime query 24/0；WSL clang
  通过同三项 6/0、2/0、24/0；Windows MSVC Debug 通过同三项 6/0、2/0、24/0。WSL gcc 与 MSVC Debug
  CTest `reflection_token_resolve|metadata_runtime_method_binding` 均 2/2。
  产出：`tests/acceptance/2026-06-30-aot-10-s2b-token-driven-method-invoke-dispatcher.md`。
  备注：本记录不声明参数数组 unbox、返回 box、签名校验、public `MethodInfo` 对象、MethodSpec-specific code slot、
  AOT/解释器等价、cross-module token rewrite 或完整 trim analyzer。

- 2026-06-30 02:05:51 +08:00 · 10-S3E / 11-S2D MethodSpec underlying method binding carrier ·
  状态：public MethodSpec token resolver 消费 underlying MethodDef AOT binding 子切片完成；完整 10-S2/10-S3
  仍未关闭。
  完成项目：`ZrCore_Reflection_ResolveToken()` 对 MethodSpec `SIGNATURE` token 继续保留 MethodSpec 自身
  signature identity、generic signature hash、argument count 和 argument-list blob offset，同时用
  `view.methodToken` 复用 11-S2D `ZrCore_MetadataRuntime_ReadMethodBindingView()`，在 underlying MethodDef 有
  code-registration binding 时把 `methodFunctionIndex`、`methodInfo`、`methodFunctionPointer` 和 `methodInvoker`
  复制到 public carrier。无 binding 时仍保留既有 MethodSpec metadata carrier 兼容行为。
  RED/GREEN：RED 为 MethodSpec focused 测试新增 AOT MethodInfo/function pointer/invoker 断言后，旧实现返回
  `methodFunctionIndex == 0`；GREEN 后 MethodSpec carrier 同时带 MethodSpec 签名/实参身份和 underlying
  MethodDef 调用载体。
  验证：WSL gcc/clang/Windows MSVC Debug 均通过 reflection token resolve 5/0、method binding 2/0、
  metadata runtime query 24/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s3e-methodspec-underlying-method-binding-carrier.md`。
  备注：本记录不声明 public generic method reflection object、泛型方法实例专用 code slot、`Method.Invoke`
  marshaling、cross-module token rewrite 或完整 10-S3 关闭。

- 2026-06-30 01:54:46 +08:00 · 10-S3D / 11-S2D method binding public reflection carrier ·
  状态：public `ResolveToken()` method binding carrier 子切片完成；完整 10-S2/10-S3 仍未关闭，
  public method reflection object、`Method.Invoke` 参数/返回 marshaling、MethodSpec runtime binding、cross-module
  token rewrite 和 trim diagnostics 仍待后续。
  完成项目：`SZrReflectionResolvedToken` 新增 `methodFunctionIndex`、`methodInfo`、
  `methodFunctionPointer` 和 `methodInvoker`；`reflection_token_resolve.c` 对普通 MethodDef/MethodRef method
  token 先保持原有 method record/signature carrier，再尝试消费 11-S2D
  `ZrCore_MetadataRuntime_ReadMethodBindingView()` 填充 AOT MethodInfo/function pointer/invoker。没有 AOT binding
  时 `ResolveToken()` 仍返回 method record，绑定字段保持空。测试扩展
  `test_reflection_token_resolve.c` 覆盖有/无 AOT binding 两种 MethodDef token 解析。
  RED/GREEN：RED 为 reflection token resolve 测试要求新增 public method binding 字段，WSL gcc 编译失败；
  GREEN 后 public carrier 成功暴露绑定字段并保持无 binding 场景兼容。
  验证：WSL gcc 与 WSL clang 均通过 `zr_vm_reflection_token_resolve_test` 5/0、
  `zr_vm_metadata_runtime_method_binding_test` 2/0、`zr_vm_metadata_runtime_query_test` 24/0；Windows MSVC Debug
  通过同三项 5/0、2/0、24/0。WSL gcc 与 Windows MSVC Debug CTest
  `reflection_token_resolve|metadata_runtime_method_binding` 均 2/2。
  产出：`tests/acceptance/2026-06-30-aot-10-s3d-method-binding-reflection-carrier.md`。
  备注：本记录不声明 public method reflection object、`Method.Invoke` marshaling、MethodSpec runtime instance
  binding、cross-module token rewrite 或完整 10-S2/10-S3 关闭。

- 2026-06-30 01:38:26 +08:00 · 11-S2D / 10-S2 support method token binding view ·
  状态：runtime 内部 method token→MethodInfo/function pointer/invoker binding view 子切片完成；完整
  10-S2/10-S3 仍未关闭，public method reflection object、public `ResolveToken()` method binding 消费、
  `Method.Invoke` 参数/返回 marshaling、cross-module token rewrite 和 trim diagnostics 仍待后续。
  完成项目：`metadata_runtime.h` 新增 `SZrMetadataRuntimeMethodBindingView` 与
  `ZrCore_MetadataRuntime_ReadMethodBindingView()`；新实现文件
  `metadata_runtime_method_binding.c` 从 attached `SZrMetadataRuntime` 的 code registration 扫描
  `methodTokens[]`，只接受唯一 local `MEMBER_DEF` token，并返回对应 `functionIndex`、
  `SZrAotMethodInfo`、entry thunk 和 reflection invoker；缺失表、重复 token、非 method token、
  MethodInfo slot 不一致、缺 thunk 或缺 invoker 均失败并清空输出。新增 focused 测试目标
  `zr_vm_metadata_runtime_method_binding_test` 与 CTest `metadata_runtime_method_binding`。
  RED/GREEN：RED 为新增 method binding 测试先要求缺失的 view 类型/API，WSL gcc 编译失败；
  GREEN 后 runtime binding 视图与负向形态均通过。
  验证：WSL gcc 通过 method binding 2/0、metadata runtime query 24/0、reflection token resolve 4/0，
  且 CTest `metadata_runtime_method_binding` 1/1；WSL clang 通过同三项 2/0、24/0、4/0；Windows MSVC
  Debug 通过同三项 2/0、24/0、4/0，并通过 CTest `metadata_runtime_method_binding` 1/1。
  产出：`tests/acceptance/2026-06-30-aot-11-s2d-method-token-binding-view.md`。
  备注：本记录不声明 public `ResolveToken()` 已暴露 MethodInfo/function pointer，不声明 `Method.Invoke`
  marshaling、public method reflection object、MethodSpec runtime instance binding 或完整 10-S2/10-S3 关闭。

- 2026-06-30 01:05:20 +08:00 · 11-S2B / 10-S2 support method token code-registration carrier ·
  状态：11-S2 code-registration method token carrier 子切片完成；完整 10-S2/10-S3 仍未关闭，
  `Method.Invoke` 参数/返回 marshaling、token→MethodInfo/function pointer/invoker resolver、public method
  reflection object、trim annotation diagnostics 仍待后续。
  完成项目：公共 AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 11u`；`SZrAotCodeRegistration` 与
  `ZrAotCompiledModule` 新增 `methodTokens/methodTokenCount`；`SZrMetadataRuntime` mirror `methodTokenCount`；
  AOT C 发射按 `functionIndex` 对齐的 `zr_aot_method_tokens[]`，root module typed exported function 填充真实
  `MEMBER_DEF` token（smoke 中为 `0x03000001u`），不可靠槽位写 `0u`；runtime descriptor validation 拒绝
  method token 指针/计数与 code registration 不一致、空/非空形态错误或计数不等于 `methodInfoCount`。
  RED/GREEN：RED 为 frame setup source contract 要求 method token ABI/emitter/runtime validation 后缺少
  codeRegistration method token mismatch 文本；GREEN 后 ABI、生成器、runtime validation 和 shared-library runtime
  断言均通过。
  验证：WSL gcc 与 WSL clang 均通过 `zr_vm_metadata_runtime_query_test` 24/0、
  `zr_vm_aot_c_source_contracts_test` 22/0、`zr_vm_aot_c_frame_setup_contracts_test` 1/0、
  `zr_vm_aot_c_shared_library_smoke_test` 13/0、`zr_vm_aot_c_descriptor_diagnostics_test` 2/0；Windows MSVC Debug
  通过前三项 24/0、22/0、1/0，shared-library smoke 13 项 ignored，descriptor diagnostics 2 项 ignored。
  产出：`tests/acceptance/2026-06-30-aot-11-s2b-method-token-carrier.md`。
  备注：本记录不声明 token→MethodInfo lookup、function pointer/invoker binding、public method reflection object、
  token-driven `Invoke`、cross-module token rewrite 或完整 10-S2/10-S3 关闭。

- 2026-06-30 00:31:26 +08:00 · 10-S3C / 11-S3 method signature reflection carrier ·
  状态：10-S3 token 驱动方法签名 carrier 子切片完成；完整 10-S3 仍未关闭，名表→token 重写、
  public method reflection object、`Invoke` 注册表消费、MethodInfo/function pointer/invoker 绑定、trim warning
  和 annotation flow 仍待后续。
  完成项目：`SZrReflectionResolvedToken` 新增 `methodSignatureToken`、`methodSignatureRecord` 与
  `methodSignatureHash`；普通 MethodDef/MethodRef token 解析时通过 11-S3C
  `ZrCore_MetadataRuntime_ResolveSignatureRecord()` 暴露 paired method signature record/hash；MethodSpec
  `SIGNATURE` token 则复用 11-S3M/11-S5 MethodSpec signature view，把 MethodSpec token/record/hash 作为
  method signature identity。
  RED/GREEN：RED 为 `zr_vm_reflection_token_resolve_test` 要求 MethodDef 与 MethodSpec resolved token 暴露
  method signature carrier 后缺少 `methodSignatureToken/methodSignatureRecord/methodSignatureHash` 字段导致
  WSL gcc 编译失败；GREEN 后普通方法签名 identity 与 MethodSpec 签名 identity 均通过。
  验证：WSL gcc、WSL clang、Windows MSVC Debug 均通过 `zr_vm_metadata_runtime_query_test` 24/0、
  `zr_vm_reflection_token_resolve_test` 4/0、`zr_vm_metadata_runtime_typespec_layout_test` 14/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s3c-method-signature-reflection-carrier.md`。
  备注：本记录不声明 public method reflection object、token-driven `Invoke`、MethodInfo/function pointer
  绑定、cross-module token rewrite、trim analyzer 或完整 10-S3 关闭。

- 2026-06-30 00:17:04 +08:00 · 10-S3B / 11-S5 MethodSpec token resolver carrier ·
  状态：10-S3 token 驱动 MethodSpec resolver 子切片完成；完整 10-S3 仍未关闭，名表→token 重写、反射对象物化、
  `Invoke` 注册表消费、trim warning/annotation flow 和完整 token-only 可用性仍待后续。
  完成项目：`SZrReflectionResolvedToken` 新增 `methodToken` / `methodRecord`，普通 method token 与 MethodSpec
  `SIGNATURE` token 都会填充 method identity；`ZrCore_Reflection_ResolveToken()` 对通过 11-S5 signature view
  验证的 `GENERIC_INST(MEMBER_REF methodToken, args...)` MethodSpec 返回 method-like carrier，并保留 MethodSpec
  signature record、signature hash、generic argument count 与 argument-list blob offset。
  RED/GREEN：RED 为 reflection token resolve 测试新增 `ResolveToken(MethodSpec)` 断言后缺少
  `methodToken/methodRecord` 字段编译失败；随后 metadata runtime query 要求 MethodSpec signature view 暴露
  `methodSpecRecord` 并再次得到预期编译失败。GREEN 后 MethodSpec token resolver 与既有 indexed argument carrier
  同时通过。
  验证：WSL gcc、WSL clang、Windows MSVC Debug 均通过 `zr_vm_metadata_runtime_query_test` 24/0、
  `zr_vm_reflection_token_resolve_test` 4/0、`zr_vm_metadata_runtime_typespec_layout_test` 14/0。
  产出：`tests/acceptance/2026-06-30-aot-10-s3b-methodspec-token-resolve-carrier.md`。
  备注：本记录不声明 public generic method reflection object、MethodSpec runtime instance binding、runtime generic
  layout construction、cross-module token publication/rewrite 或 full trim analyzer 完成。

- 2026-06-29 23:56:22 +08:00 · 10-S4E / 11-S5 MethodSpec generic argument reflection carrier ·
  状态：10-S4 MethodSpec generic argument public carrier 子切片完成；完整 10-S4 仍未关闭，public generic
  reflection object、public generic method reflection object、`MakeGenericType` runtime construction、public
  `FieldInfo` 对象、字段值读写 marshaling 和注解驱动保留策略仍待后续。
  完成项目：`metadata_runtime.h` 新增 `SZrMetadataRuntimeMethodSpecGenericArgumentView` 与
  `ZrCore_MetadataRuntime_ReadMethodSpecGenericArgumentView()`；`reflection.h` 新增
  `SZrReflectionResolvedMethodSpecGenericArgument` 与 `ZrCore_Reflection_ResolveMethodSpecGenericArgument()`。
  `reflection_token_resolve.c` 复用 11-S5 MethodSpec argument view，暴露 methodSpec token、method token/record、
  signature hash、argument index、argument node kind/payload，以及 direct TypeDef/TypeRef argument token/record。
  RED/GREEN：RED 为 `zr_vm_metadata_runtime_query_test` 与 `zr_vm_reflection_token_resolve_test` 新增
  MethodSpec generic argument fixture 后缺少 runtime/public 类型与 API 导致 WSL gcc 编译失败；GREEN 后 primitive
  argument、TypeRef argument、null/未 attach/wrong token/out-of-range 负向路径均通过。
  验证：WSL gcc 与 WSL clang 均通过 `zr_vm_metadata_runtime_query_test` 24/0、
  `zr_vm_reflection_token_resolve_test` 4/0、`zr_vm_metadata_runtime_typespec_layout_test` 14/0；Windows MSVC
  Debug 通过同三项 24/0、4/0、14/0。
  产出：`tests/acceptance/2026-06-29-aot-11-s5-methodspec-generic-argument-view.md`。
  备注：本记录不声明 public generic method reflection object、MethodSpec runtime instance binding、runtime generic
  layout construction、cross-module token publication/rewrite 或 full trim analyzer 完成。

- 2026-06-29 23:34:03 +08:00 · 10-S4D / 11-S5 public GenericParam constraint reflection carrier ·
  状态：10-S4 泛型参数/约束 public carrier 子切片完成；完整 10-S4 仍未关闭，public generic reflection
  object、`MakeGenericType` runtime construction、public `FieldInfo` 对象、字段值读写 marshaling 和注解驱动保留策略仍待后续。
  完成项目：`reflection.h` 新增 `SZrReflectionResolvedGenericParameter`、
  `SZrReflectionResolvedGenericParameterConstraint`、`ZrCore_Reflection_ResolveGenericParameter()` 与
  `ZrCore_Reflection_ResolveGenericParameterConstraint()`；`reflection_token_resolve.c` 复用 11-S5 runtime view，
  暴露 TypeDef/MethodDef owner record、GenericParam row/index/name/flags/constraint range，以及 constraint
  row、constraint type token/record 和可选 signature blob slice。
  RED/GREEN：RED 为 `zr_vm_reflection_token_resolve_test` 新增 synthetic TypeDef/MethodDef GenericParam +
  GenericParamConstraint fixture 后缺少 public 类型/API 导致 WSL gcc 编译失败；GREEN 后 TypeDef 参数、MethodDef
  参数、constraint type record、signature blob 和 out-of-range 负向路径均通过。
  验证：WSL gcc/clang 均通过 `zr_vm_reflection_token_resolve_test` 3/0；WSL gcc 通过
  `zr_vm_metadata_runtime_query_test` 23/0 与 `zr_vm_metadata_runtime_typespec_layout_test` 14/0；Windows MSVC
  Debug 通过同三项 3/0、23/0、14/0。
  产出：`tests/acceptance/2026-06-29-aot-10-s4d-generic-parameter-reflection-carrier.md`。
  备注：本记录不声明 public generic reflection object、MethodSpec runtime instance binding、runtime generic layout
  construction、cross-module token publication/rewrite 或 full trim analyzer 完成。

- 2026-06-29 13:22:39 +08:00 · 10-S4 support / 11-S4R-union generated union ownership offset table ·
  状态：10-S4/11-S4 owner-field layout 支撑子切片完成；完整 10-S4 仍未关闭，public `FieldInfo` 对象、
  字段值读写 marshaling、public generic type reflection object 与注解驱动保留策略仍待后续。
  完成项目：generated C 的 `SZrTypeLayout` descriptor 现在可为 union owner payload fields 暴露
  `ZrOwnershipOffsets_<typeLayoutId>[]`，并让 `.ownershipFieldOffsets` 指向该表。union layout 仍保留
  `SZrTypeLayoutField.activeTag` 作为 active-payload 判定来源，反射/metadata consumer 继续从同一
  registry-backed layout 读取 offset，不在反射层另存 union payload offset。
  RED/GREEN：RED 为 `zr_vm_aot_c_value_type_shared_library_smoke_test` 新增 `Shared<Box>` union payload
  fixture 后失败，缺少 generated `ZrOwnershipOffsets_`；GREEN 后 generated union descriptor 出现
  `/* zr_aot_ownership_offsets layout=... count=1 */`、`ZrOwnershipOffsets_<id>[]`、
  `.kind = 2u`、`.ownershipFieldCount = 1u` 和 `.ownershipFieldOffsets = ZrOwnershipOffsets_<id>`。
  验证：WSL gcc 通过 direct value-type smoke 5/0、source contracts 22/0 和
  `aot_c_type_layout_contracts` CTest 1/1；WSL clang 通过同组 5/0、22/0、1/1；Windows MSVC Debug
  通过 source contracts 22/0、value-type smoke 5/0/1 ignored 和同组 CTest 1/1。
  产出：`tests/acceptance/2026-06-29-aot-11-s4r-union-ownership-offset-table.md`。
  备注：本切片只关闭 generated union ownership-offset descriptor emission；不声明 public reflection
  entity、runtime generic layout construction、cross-module token publication/rewrite 或 trim analyzer 完成。

- 2026-06-29 13:03:17 +08:00 · 10-S4C public FieldDef owner/type token carrier ·
  状态：10-S4 字段实体支撑子切片完成；完整 10-S4 仍未关闭，public `FieldInfo` 对象、
  字段值读写 marshaling、union owner offsets、public generic type reflection object 与注解驱动保留策略仍待后续。
  完成项目：`SZrReflectionResolvedToken` 的 FieldDef 分支现在暴露 `ownerTypeRecord`、
  `ownerTypeDefRow`、`fieldTypeToken` 和 `fieldTypeRecord`。`fieldTypeToken` 通过现有
  `ZrCore_MetadataRuntime_ResolveTypeLayoutToken()` 从 FieldDef row 的 field type layout id 反查
  TypeDef/TypeSpec token，继续把 offset、owner layout、field layout 和 type identity 绑定到 11-S4 的
  metadata/layout 单一真相。
  RED/GREEN：RED 为 expanded `zr_vm_reflection_token_resolve_test` 在 synthetic owner TypeDef +
  field-type TypeDef + FieldDef metadata 下引用缺失 FieldDef owner/type carrier 字段后编译失败；GREEN 后
  TypeDef/FieldDef/MethodDef、FieldDef owner token/record/row、field type token/record/layout、TypeSpec
  generic argument 和 out-of-range generic argument 负向路径通过。
  验证：WSL gcc/clang 与 Windows MSVC Debug 均通过
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout`
  CTest 4/4。
  产出：`tests/acceptance/2026-06-29-aot-10-s4-fielddef-public-reflection-carrier.md`。
  备注：本切片只关闭 FieldDef public carrier 的 owner/type identity；不声明 public field reflection
  object、字段值反射访问、跨模块 token publication/rewrite 或完整 trim analyzer 完成。

- 2026-06-29 12:51:49 +08:00 · 10-S4B public TypeSpec generic argument carrier ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection object、
  `MakeGenericType` runtime construction、泛型参数约束反射、public field entity 接入和注解驱动保留策略仍待后续。
  完成项目：`SZrReflectionResolvedToken` 对 TypeSpec token 现在填充 `genericSignatureToken`、
  `genericSignatureHash`、`genericBaseToken`、`genericBaseRecord`、`genericArgumentCount` 和
  `genericArgumentListBlobOffset`；新增 public
  `ZrCore_Reflection_ResolveTypeSpecGenericArgument(...)` 与 `SZrReflectionResolvedGenericArgument`，
  可按 argument index 暴露 primitive signature argument 或 direct TypeRef/TypeDef argument token/record。
  RED/GREEN：RED 为扩展后的 `zr_vm_reflection_token_resolve_test` 引用缺失
  `SZrReflectionResolvedGenericArgument`、TypeSpec generic carrier 字段和 indexed argument API 后编译失败；
  GREEN 后 TypeDef/FieldDef/MethodDef、TypeSpec base/argument-count、primitive argument、TypeRef argument
  与 out-of-range argument 负向路径通过。
  验证：WSL gcc/clang 与 Windows MSVC Debug 均通过
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout`
  CTest 4/4。
  产出：`tests/acceptance/2026-06-29-aot-10-s4-public-typespec-generic-argument-reflection.md`。
  备注：本切片只关闭 TypeSpec generic argument 的 public carrier；不声明泛型反射对象、TypeSpec
  runtime materialization、跨模块 token publication/rewrite 或 `MakeGenericType` 完成。

- 2026-06-29 12:37:45 +08:00 · 10-S3A public reflection token resolver carrier ·
  状态：10-S3 支撑子切片完成；完整 10-S3 仍未关闭，名表→token 重写、public reflection object
  materialization、`Invoke`/invoker registry consumption、trim warning 与 annotation flow 仍待后续。
  完成项目：新增 public `SZrReflectionResolvedToken` 与
  `ZrCore_Reflection_ResolveToken(SZrMetadataRuntime *, TZrMetadataToken, SZrReflectionResolvedToken *)`，
  以 `SZrMetadataRuntime` 为入口解析 TypeDef/TypeSpec/TypeRef、FieldDef 和 MethodDef/MethodRef。
  TypeDef/TypeSpec 直接复用 11-S4 registry-backed layout binding view，FieldDef 复用 11-S4I
  FieldDef row/owner/field layout binding view，method token 先返回 method record，unsupported/null 输入会清空
  output 并返回 false。
  RED/GREEN：RED 为新增 `zr_vm_reflection_token_resolve_test` 引用缺失的
  `SZrReflectionResolvedToken`、`ZrCore_Reflection_ResolveToken()` 与 resolved-kind enum 后编译失败；
  GREEN 后 TypeDef、FieldDef、MethodDef 和 invalid input 覆盖通过。
  验证：WSL gcc/clang 与 Windows MSVC Debug 均通过
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout`
  CTest 4/4。
  产出：`tests/acceptance/2026-06-29-aot-10-s3-public-reflection-token-resolve.md`。
  备注：`MEMBER_DEF` 当前先按 FieldDef binding view 判定字段，否则退到 method record；本切片不声明
  public 反射对象构造、按名查找裁剪安全或动态调用完成。

- 2026-06-26 06:00:16 +08:00 · 10-S1 support / 12-S7Y default-min generated MethodInfo reflection policy ·
  状态：10-S1 支撑子切片完成；完整 10-S1 仍未关闭，annotation 驱动 `DESCRIPTION` 提升、
  类型/字段/泛型实体级默认最小策略和完整体积下降验收仍待后续。
  完成项目：generated AOT C MethodInfo 的 `reflectionMetadataLevel` 现在由 writer policy 决定：
  默认/非裁剪产物继续输出 `ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING`，opt-in code stripping 产物输出
  `ZR_AOT_REFLECTION_METADATA_NONE`，文件头同步记录 `metadata_policy.reflectionLevel`。
  RED/GREEN：RED 为 code-stripping fixture 要求裁剪产物 MethodInfo 降为 `NONE` 后失败 1/4；GREEN 后
  shared option helper 统一计算 reflection metadata level，method metadata emitter 按该 level 输出常量名。
  验证：WSL gcc/clang 均通过 code stripping 4/0、source contracts 21/0、frame setup contract 1/0、
  typed scalar 1/0、shared-library smoke 8/0；Windows MSVC Debug 同组通过，其中 typed scalar/shared-library
  smoke 按既有 Unix-only guard 分别 1 ignored / 8 ignored。
  产出：`tests/acceptance/2026-06-26-aot-12-s7y-default-min-reflection-metadata-policy.md`。
  备注：本切片只关闭 generated MethodInfo 反射级别的默认最小接线；未实现 public reflection entity
  物化、注解语义或运行期按 token 反射。

- 2026-06-26 01:14:40 +08:00 · 10-S4 support / 11-S4R generated ownership offset table emission ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体、union owner offsets 和注解驱动保留策略
  仍待后续 10/11/12 切片。完成项目：generated C 的 `SZrTypeLayout` descriptor 现在可为 struct owner fields
  暴露 `ZrOwnershipOffsets_<typeLayoutId>[]`，让后续 layout-driven reflection/metadata consumers 能在读取 registry-backed
  layout 时看到 owner-field offset table；unsafe/unsupported/union 路径保持 `ZR_NULL`。RED/GREEN：RED 为
  type-layout source contract 要求 ownership-offset writer helper 后缺少
  `backend_aot_c_type_layout_can_emit_ownership_offsets(`；GREEN 后 `Unique<string>` 字段 generated layout 输出
  `ownershipFieldCount = 1u` 和 `.ownershipFieldOffsets = ZrOwnershipOffsets_0`。验证：WSL gcc/clang 均通过
  `aot_c_type_layout_contracts` CTest 1/1、source contracts 19/0、value-type smoke 4/0；Windows MSVC Debug 通过
  CTest 1/1、source contracts 19/0、value-type smoke 3/0/1 ignored。产出：
  `tests/acceptance/2026-06-26-aot-11-s4r-generated-ownership-offset-table.md`。
  备注：本记录不声明 public reflection entity、泛型反射对象、union owner offset 表、runtime generic layout construction
  或 cross-module token-table policy 完成。

- 2026-06-26 00:42:14 +08:00 · 10-S4 support / 11-S4Q generated TypeSpec token population ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：generated C 的 `zr_aot_type_layout_tokens[]` 现在可为同函数 metadata 中
  canonical `TYPE_SPEC` signature 可唯一匹配的 generated generic layout 写入真实 `TYPE_SPEC` token；后续
  layout-driven reflection entity 可从 code-registration table-first resolver 直接拿到 closed generic type token，
  而不是只能通过 zrp row scan fallback。RED/GREEN：RED 为 `Pair<int, int>` generated-C smoke 要求
  `0x07000001u` 时旧 token 表全为 0；GREEN 后 generated `zr_aot_type_layout_tokens[4] == 0x07000001u`。
  验证：WSL gcc/clang 均通过 `aot_c_type_layout_contracts|aot_c_generic_call_typed` CTest 2/2、source contracts
  19/0、value-type smoke 3/0；Windows MSVC Debug 通过同组 CTest 2/2、source contracts 19/0、value-type smoke
  2/0/1 ignored。产出：`tests/acceptance/2026-06-26-aot-11-s4q-generated-typespec-type-layout-token-population.md`。
  备注：本记录不声明 public generic reflection API、跨模块 TypeSpec token 表、runtime generic layout synthesis
  或泛型实参枚举对象完成。

- 2026-06-26 00:13:32 +08:00 · 10-S4 support / 11-S4P generated type-layout token population ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：generated C 的 `zr_aot_type_layout_tokens[]` 现在对唯一匹配本地 TypeDef metadata 的
  named struct/union layout 写入真实 `TYPE_DEF` token；metadata runtime 的 table-first cTypeId/typeLayoutId→token
  resolver 因此可在这类 generated layout 上直接命中 registration 表，缺 metadata、多重匹配、TypeSpec/generic 仍为 0
  并保留 zrp scan fallback。RED/GREEN：RED 为 union `Shape` generated-C smoke 缺 runtime layout descriptor 和非零
  token；GREEN 后 generated union layout registry、token 表和 `0x02000001u` token entry 通过。验证：WSL gcc/clang
  均通过 metadata TypeSpec layout 14/0、AOT type-layout contracts 1/0、source contracts 19/0、frame setup 1/0、
  shared-library smoke 8/0、value-type smoke 3/0；Windows MSVC Debug 通过 metadata 14/0、type-layout contracts 1/0、
  source contracts 19/0、frame setup 1/0，shared/value-type smoke 的 Unix-only 分支按既有规则 ignored。产出：
  `tests/acceptance/2026-06-26-aot-11-s4p-generated-type-layout-token-population.md`。
  备注：本记录不声明 public reflection entity、泛型反射对象、TypeSpec/generic token population 或运行期泛型实例构造完成。

- 2026-06-25 23:13:20 +08:00 · 10-S4 support / 11-S4O type layout token carrier ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：code registration 新增 `typeLayoutTokens/typeLayoutTokenCount`，metadata runtime
  在 cTypeId/typeLayoutId→token 解析时可先读 registration token 表，再 fallback 到 zrp row scan；该路径为后续
  layout-driven reflection entity 回写 token 提供更直接 carrier，但当前 generated 表项仍为 0，尚未完成真实 token
  population，也尚未接入 public reflection entity。RED/GREEN：RED 为 focused ABI/source/runtime tests 后缺少 token
  carrier 字段和 runtime count mirror；GREEN 后手工非零 token 表消费、非 type token/缺 layout 拒绝、generated table
  shape 和 descriptor validation 均通过。验证：WSL gcc/clang 与 Windows MSVC Debug 均通过 metadata runtime TypeSpec
  layout 14/0、AOT source contracts 19/0、frame setup contracts 1/0；WSL gcc/clang 通过 shared-library smoke 8/0
  和 value-type smoke 2/0，MSVC 对应 Unix-only smoke 分支为 ignored。产出：
  `tests/acceptance/2026-06-25-aot-11-s4o-type-layout-token-carrier.md`。
  备注：本记录不声明泛型反射对象、类型实参枚举 UI/API、字段反射实体、真实 token 填充或运行期泛型实例构造完成。

- 2026-06-25 22:22:13 +08:00 · 10-S4 support / 11-S4N cTypeId token resolver ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：metadata runtime 新增
  `ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, cTypeId)`，在当前 registry 的 `cTypeId == typeLayoutId`
  不变量下复用 bounded token/layout cache 与 TypeDef/TypeSpec token 反查路径。该入口为后续 layout-driven
  reflection entity 按 generated C type id 回写 token 提供底座，但尚未接入 public reflection entity。
  RED/GREEN：RED 为 cTypeId→token focused tests 后缺少 public resolver/API；GREEN 后 TypeDef/TypeSpec cTypeId
  反查、多项 cache 命中和 no-prototype-fallback 负向用例通过。验证：WSL gcc/clang 与 Windows MSVC Debug 均通过
  metadata runtime TypeSpec layout 12/0、metadata runtime query 22/0、metadata runtime type-layout 10/0；
  zrp metadata format 11/0 同组通过。产出：
  `tests/acceptance/2026-06-25-aot-11-s4n-ctype-id-token-resolver.md`。
  备注：本记录不声明泛型反射对象、类型实参枚举 UI/API、字段反射实体或运行期泛型实例构造完成。

- 2026-06-25 22:13:54 +08:00 · 10-S4 support / 11-S4M bounded multi-entry type layout cache ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：metadata runtime 的 TypeDef/TypeSpec token/layout cache 从单项最近命中扩展为
  8 项 bounded cache，`ResolveTypeTokenLayout()` 与 `ResolveTypeLayoutToken()` 可在同一 runtime 中同时保留
  TypeDef 和 TypeSpec 的正向/反向命中。该入口为后续类型实参枚举、layout-driven reflection entity 回写 token
  和 public `ResolveToken` 互操作提供更稳定的缓存底座，但尚未接入 public reflection entity。
  RED/GREEN：RED 为多项 cache 用例暴露旧单项 cache 会覆盖前一个 TypeDef/TypeSpec 命中；GREEN 后多项
  token→layout 与 layoutId→token 命中均通过。验证：WSL gcc/clang 与 Windows MSVC Debug 均通过 metadata runtime
  TypeSpec layout 10/0、metadata runtime query 22/0、metadata runtime type-layout 10/0；zrp metadata format 11/0
  同组通过。产出：
  `tests/acceptance/2026-06-25-aot-11-s4m-multi-entry-type-layout-cache.md`。
  备注：本记录不声明泛型反射对象、类型实参枚举 UI/API、字段反射实体或运行期泛型实例构造完成。

- 2026-06-25 21:53:56 +08:00 · 10-S4 support / 11-S4L typeLayoutId to token reverse resolver ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：metadata runtime 新增
  `ZrCore_MetadataRuntime_ResolveTypeLayoutToken(runtime, typeLayoutId)`，可从 registry-backed layout id 反查
  TypeDef/TypeSpec token，并复用最近一次 token/layout cache。该入口为后续 layout-driven reflection entity
  回写 token、类型实参枚举和 public `ResolveToken` 互操作提供底座，但尚未接入 public reflection entity。
  RED/GREEN：RED 为新增 layoutId→token focused test 后缺少 public resolver/API；GREEN 后 TypeDef/TypeSpec
  layoutId 反查、cache 命中和 stale prototype cache 负向用例通过。验证：WSL gcc/clang 与 Windows MSVC Debug
  均通过 metadata runtime TypeSpec layout 8/0、metadata runtime query 22/0、metadata runtime type-layout 10/0。
  产出：`tests/acceptance/2026-06-25-aot-11-s4l-layout-id-token-reverse-cache.md`。
  备注：本记录不声明泛型反射对象、类型实参枚举 UI/API、字段反射实体或运行期泛型实例构造完成。

- 2026-06-25 21:37:38 +08:00 · 10-S4 support / 11-S4K TypeDef/TypeSpec token layout cache resolver ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、`MakeGenericType`
  runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略仍待后续
  10/11/12 切片。完成项目：metadata runtime 新增
  `ZrCore_MetadataRuntime_ResolveTypeTokenLayout(runtime, typeToken, outTypeLayoutId)`，把 `TYPE_DEF` 和
  `TYPE_SPEC` token 解析为 registry-backed `SZrTypeLayout`，并缓存最近一次 token→layoutId/layout 命中。
  该入口为后续 `ResolveToken`、字段/类型实参反射和 generic type reflection 提供统一 token lookup 底座，
  但尚未接入 public reflection entity。RED/GREEN：RED 为新增 TypeDef/TypeSpec token cache focused test 后
  缺少 public resolver/API；GREEN 后 TypeDef token cache、TypeSpec token cache、非 type token 拒绝和 stale
  prototype cache 负向用例通过。验证：WSL gcc/clang 与 Windows MSVC Debug 均通过 metadata runtime TypeSpec
  layout 5/0、metadata runtime query 22/0、metadata runtime type-layout 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4k-type-token-layout-cache.md`。
  备注：本记录不声明泛型反射对象、类型实参枚举 UI/API、字段反射实体或运行期泛型实例构造完成。

- 2026-06-25 21:18:46 +08:00 · 10-S4 support / 11-S4J TypeSpec layout binding view ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public generic type reflection、
  `MakeGenericType` runtime construction、泛型参数约束反射、public token-driven 字段反射实体和注解驱动保留策略
  仍待后续 10/11/12 切片。完成项目：metadata runtime 新增 TypeSpec layout binding view，从 zrp `TYPE_SPECS`
  row 读取 `typeLayoutId/signatureHash`，复用 11-S3K generic base-token binding，并通过 code-registration layout
  registry 解析 closed TypeSpec 的 `SZrTypeLayout`；该 view 为后续类型实参/泛型实例反射提供 token→row→generic
  binding→layout 的单一真相入口，但尚未接入 public reflection entity。RED/GREEN：RED 为新增 TypeSpec layout
  focused test 后缺少 zrp row 字段、view type/API；GREEN 后正向 binding 与 stale prototype cache 负向用例通过。
  验证：WSL gcc/clang 与 Windows MSVC Debug 均通过 metadata runtime TypeSpec layout 2/0、metadata runtime query
  22/0、metadata runtime type-layout 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4j-typespec-layout-binding-view.md`。
  备注：本记录不声明泛型反射对象、类型实参枚举 UI/API 或运行期泛型实例构造完成。

- 2026-06-25 20:43:52 +08:00 · 10-S4 support / 11-S4I FieldDef layout binding view ·
  状态：10-S4 支撑子切片完成；完整 10-S4 仍未关闭，public token-driven 字段反射实体、
  泛型参数反射、类型实参枚举和注解驱动保留策略仍待后续 10/11/12 切片。
  完成项目：metadata runtime 新增 FieldDef token binding view，从 zrp `FIELD_DEFS` row 读取
  `byteOffset/typeLayoutId`，绑定 owner `TYPE_DEF` row，并通过 code-registration layout registry 解析
  field/owner `SZrTypeLayout`；该 view 为 DESCRIPTION 级字段 offset 反射提供 token→row→layout 的单一真相入口，
  但尚未接入 public reflection field entity。RED/GREEN：RED 为 metadata runtime query 新增 FieldDef binding view
  用例后缺少 view type/API；GREEN 后正向 binding 与 stale prototype cache 负向用例通过。验证：WSL gcc/clang 与
  Windows MSVC Debug 均通过 metadata runtime query 22/0 和 metadata runtime type-layout 10/0。产出：
  `tests/acceptance/2026-06-25-aot-11-s4i-fielddef-layout-binding-view.md`。
  备注：本记录不声明 `FieldInfo` 等 public 反射实体、字段读写 marshaling 或泛型反射完成。

- 2026-06-25 20:27:41 +08:00 · 10-S4A / 11-S4H reflection field layout registry consumer ·
  状态：10-S4 子切片完成；完整 10-S4 仍未关闭，泛型参数反射、DESCRIPTION 级 token-driven 字段实体、
  类型实参枚举和注解驱动保留策略仍待后续 10/11/12 切片。
  完成项目：反射层在构建脚本 type reflection 时通过
  `ZrCore_MetadataRuntime_ResolveFunctionPrototypeTypeLayout(entryFunction, prototype, &typeLayoutId)` 获取
  attached AOT registry 中的 `SZrTypeLayout`；类型级 `layout.fieldCount/size/alignment` 从该 layout 写入；
  字段级 `offset/size/layout` 由 `SZrTypeLayout.fields[i].byteOffset/byteSize` 写入，并按实例字段序号而非旧
  serialized `member->fieldOffset` 反查 registry field；decorator target member reflection 使用同一 helper。
  未 attached registry、native type entry 或无匹配 field 时继续保留既有非 AOT 反射行为。RED/GREEN：RED 为
  focused metadata runtime type-layout 测试在新增 prototype layout resolver/反射消费契约后编译/链接失败；
  GREEN 后源码契约证明反射消费端包含 metadata runtime、调用 registry-backed resolver、类型 layout 写入 helper
  和字段 layout 写入 helper，并锁定字段按实例序号消费 registry。验证：WSL gcc/clang 均通过
  `zr_vm_metadata_runtime_type_layout_test` 10/0、`zr_vm_metadata_runtime_query_test` 20/0、
  `zr_vm_aot_c_type_layout_contracts_test` 1/0、`zr_vm_aot_c_source_contracts_test` 19/0；
  Windows MSVC Debug 同组通过 10/0、20/0、1/0、19/0。
  产出：`tests/acceptance/2026-06-25-aot-11-s4h-10-s4a-reflection-layout-registry-consumer.md`。
  备注：本记录只关闭字段 offset/layout 的 registry-backed 读取入口；不声明完整 token 反射解析、泛型参数反射、
  `DESCRIPTION` metadata policy 或裁剪注解完成。

- 2026-06-25 12:05:39 +08:00 · 10-S2A MethodInfo reflection invoker carrier ·
  状态：10-S2 子切片完成；完整 10-S2 仍未关闭，`Method.Invoke` 参数解包/返回打包、token 注册表消费、
  typed-target 分桶和 AOT/解释器结果等价验收仍待后续。
  完成项目：公共 AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 7u`；新增
  `FZrAotReflectionInvoker(state, target, method, self, args, outReturn)`；`SZrAotMethodInfo`
  增加 `invoker` 字段；AOT C 生成物在 MethodInfo 表前发射共享 `zr_aot_invoker_entry_thunk`，
  当前按统一 `FZrAotEntryThunk(SZrState *)` 签名分桶，并让每个 generated MethodInfo 登记
  `.invoker = zr_aot_invoker_entry_thunk`。
  RED/GREEN：RED 为 `zr_vm_aot_c_shared_library_smoke_test` 在新增 `methodInfos[0]->invoker`
  断言后编译失败，且 frame setup 源契约缺少 `struct SZrTypeValue` / invoker ABI 文本；
  GREEN 后 ABI/source 契约通过，生成共享库 descriptor 暴露非空 MethodInfo invoker。
  验证：WSL gcc/clang 均通过 focused 组：`zr_vm_aot_c_frame_setup_contracts_test` 1/0、
  `zr_vm_aot_c_shared_library_smoke_test` 8/0、`zr_vm_aot_c_source_contracts_test` 19/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 1/0、`zr_vm_aot_c_return_contracts_test` 1/0；
  Windows MSVC Debug 构建通过，frame setup 1/0、source contracts 19/0、return contracts 1/0，
  shared-library smoke 8/0（8 ignored Unix-only 分支）、descriptor diagnostics 1/0（1 ignored Unix-only 分支）。
  产出：`tests/acceptance/2026-06-25-aot-10-s2a-reflection-invoker-carrier.md`。
  备注：本切片只提供 invoker ABI carrier 和 generated-C 登记点；不声明完整反射调用 API、参数数组 marshaling、
  token 解析、字段/generic DESCRIPTION 元数据或注解裁剪完成。

- 2026-06-24 18:52:22 +08:00 · 12-S8D / 10 handoff full-AOT TYPEOF reflection runtime contract guard ·
  状态：12-S8 反射相关子切片完成；10 阶段继续进行中，10-S2 invoker、10-S3 token 解析、10-S5 注解/数据流
  仍未完成。
  完成项目：AOT C writer 在 `requireFullAot` 下把 `TYPEOF` 视为未注解 reflection runtime contract；
  若当前产物仍需 `ZrLibrary_AotRuntime_TypeOf()` / `ZrCore_Reflection_TypeOfValue` runtime boundary，则 writer
  编译期返回 `ZR_FALSE` 并删除半成品 C。默认 hybrid 路径继续允许 TYPEOF runtime boundary。
  RED/GREEN：RED 为 full-AOT TYPEOF smoke 仍成功生成；GREEN 后 full-AOT TYPEOF 被拒绝，hybrid TYPEOF
  仍生成并编译。
  验证：`zr_vm_aot_c_global_shared_library_smoke_test` 10/0。产出：
  `tests/acceptance/2026-06-24-aot-12-s8d-full-aot-typeof-reflection-closure.md`。
  备注：这是 12-S8 full-AOT 护栏在反射 runtime boundary 上的第一条落地，不声明 invoker thunk、token 反射解析、
  字段/generic DESCRIPTION 元数据或保留注解完成。

- 2026-06-24 14:37:23 +08:00 · 10-S1A MethodInfo reflection metadata level carrier ·
  状态：10-S1 子切片完成；完整 10-S1 仍未关闭，实体级可达性/裁剪驱动默认最小与体积下降验收仍待
  `12` 可达性图和后续 10 反射策略接入。
  完成项目：AOT ABI 升到 `ZR_VM_AOT_ABI_VERSION 5u`；新增
  `EZrAotReflectionMetadataLevel`，包含 `ZR_AOT_REFLECTION_METADATA_NONE`、
  `ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING`、`ZR_AOT_REFLECTION_METADATA_DESCRIPTION`；
  `SZrAotMethodInfo` 增加 `reflectionMetadataLevel` 与保留字节；AOT C 生成的 MethodInfo
  默认写入 `ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING`，为后续 invoker/token mapping 保留执行级元数据能力。
  RED/GREEN：RED 为 AOT MethodInfo 无反射 metadata level，生成共享库无法声明当前方法只保留 runtime mapping；
  GREEN 后源契约锁定 ABI/枚举/字段/生成默认值，共享库 descriptor runtime assertion 验证导出 MethodInfo
  的 `reflectionMetadataLevel` 为 `RUNTIME_MAPPING`，ABI mismatch 诊断跟随当前版本号而非硬编码旧值。
  验证：`zr_vm_aot_c_frame_setup_contracts_test` 1/0、`zr_vm_aot_c_shared_library_smoke_test` 8/0、
  `zr_vm_aot_c_descriptor_diagnostics_test` 1/0。产出：
  `tests/acceptance/2026-06-24-aot-10-s1a-reflection-metadata-level.md`。
  备注：本记录只提供 MethodInfo 级三态 carrier；未反射可达类型不带反射元数据、体积下降可测、注解/manifest
  提升到 `DESCRIPTION` 均仍属后续 10/12 工作。
