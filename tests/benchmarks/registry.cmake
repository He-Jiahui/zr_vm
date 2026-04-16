include(CMakeParseArguments)

set(ZR_VM_BENCHMARK_CASE_NAMES "")
set(ZR_VM_BENCHMARK_IMPLEMENTATION_ORDER
        "c"
        "zr_interp"
        "zr_binary"
        "python"
        "node"
        "qjs"
        "lua"
        "rust"
        "dotnet"
        "java")

set(ZR_VM_BENCHMARK_TIER_SCALE_smoke 1)
set(ZR_VM_BENCHMARK_TIER_SCALE_core 4)
set(ZR_VM_BENCHMARK_TIER_SCALE_stress 16)
set(ZR_VM_BENCHMARK_TIER_SCALE_profile 1)

set(ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS "${ZR_VM_BENCHMARK_IMPLEMENTATION_ORDER}")
set(ZR_VM_BENCHMARK_DEFAULT_CORE_IMPLEMENTATIONS
        "c"
        "zr_interp"
        "zr_binary"
        "python")

function(zr_vm_register_benchmark_case name)
    set(options "")
    set(oneValueArgs
            DESCRIPTION
            PASS_BANNER
            WORKLOAD_TAG
            PROFILE_SCALE
            CHECKSUM_SMOKE
            CHECKSUM_CORE
            CHECKSUM_PROFILE
            CHECKSUM_STRESS)
    set(multiValueArgs
            TIERS
            IMPLEMENTATIONS
            CORE_IMPLEMENTATIONS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT ARG_DESCRIPTION OR NOT ARG_PASS_BANNER)
        message(FATAL_ERROR "zr_vm_register_benchmark_case(${name}) requires DESCRIPTION and PASS_BANNER")
    endif ()
    if (NOT ARG_TIERS)
        message(FATAL_ERROR "zr_vm_register_benchmark_case(${name}) requires TIERS")
    endif ()
    if (NOT DEFINED ARG_CHECKSUM_SMOKE OR
            NOT DEFINED ARG_CHECKSUM_CORE OR
            NOT DEFINED ARG_CHECKSUM_PROFILE OR
            NOT DEFINED ARG_CHECKSUM_STRESS)
        message(FATAL_ERROR "zr_vm_register_benchmark_case(${name}) requires CHECKSUM_SMOKE/CHECKSUM_CORE/CHECKSUM_PROFILE/CHECKSUM_STRESS")
    endif ()

    if (NOT ARG_IMPLEMENTATIONS)
        set(ARG_IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}")
    endif ()
    if (NOT ARG_WORKLOAD_TAG)
        set(ARG_WORKLOAD_TAG "general")
    endif ()
    if (NOT ARG_PROFILE_SCALE)
        set(ARG_PROFILE_SCALE 1)
    endif ()
    if (NOT ARG_CORE_IMPLEMENTATIONS)
        set(ARG_CORE_IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_CORE_IMPLEMENTATIONS}")
    endif ()

    list(APPEND ZR_VM_BENCHMARK_CASE_NAMES "${name}")
    set(ZR_VM_BENCHMARK_CASE_NAMES "${ZR_VM_BENCHMARK_CASE_NAMES}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_DESCRIPTION_${name}" "${ARG_DESCRIPTION}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_PASS_BANNER_${name}" "${ARG_PASS_BANNER}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_WORKLOAD_TAG_${name}" "${ARG_WORKLOAD_TAG}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_PROFILE_SCALE_${name}" "${ARG_PROFILE_SCALE}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_TIERS_${name}" "${ARG_TIERS}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_IMPLEMENTATIONS_${name}" "${ARG_IMPLEMENTATIONS}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_CORE_IMPLEMENTATIONS_${name}" "${ARG_CORE_IMPLEMENTATIONS}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_CHECKSUM_${name}_smoke" "${ARG_CHECKSUM_SMOKE}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_CHECKSUM_${name}_core" "${ARG_CHECKSUM_CORE}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_CHECKSUM_${name}_profile" "${ARG_CHECKSUM_PROFILE}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_CHECKSUM_${name}_stress" "${ARG_CHECKSUM_STRESS}" PARENT_SCOPE)
endfunction()

zr_vm_register_benchmark_case(
        numeric_loops
        DESCRIPTION "Integer arithmetic and branch-heavy loops."
        PASS_BANNER "BENCH_NUMERIC_LOOPS_PASS"
        WORKLOAD_TAG "arith,branch,loop"
        PROFILE_SCALE 2
        TIERS "smoke;core;stress;profile"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        CHECKSUM_SMOKE "48943705"
        CHECKSUM_CORE "793446923"
        CHECKSUM_PROFILE "200014259"
        CHECKSUM_STRESS "664618747")

zr_vm_register_benchmark_case(
        dispatch_loops
        DESCRIPTION "Polymorphic dispatch loops aligned with interface-call pressure."
        PASS_BANNER "BENCH_DISPATCH_LOOPS_PASS"
        WORKLOAD_TAG "dispatch,call,loop"
        PROFILE_SCALE 2
        TIERS "core;stress;profile"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        CHECKSUM_SMOKE "522873290"
        CHECKSUM_CORE "320214929"
        CHECKSUM_PROFILE "64790779"
        CHECKSUM_STRESS "684820768")

zr_vm_register_benchmark_case(
        container_pipeline
        DESCRIPTION "Linked-list, set, and map aggregation pipeline."
        PASS_BANNER "BENCH_CONTAINER_PIPELINE_PASS"
        WORKLOAD_TAG "container,object,copy"
        PROFILE_SCALE 2
        TIERS "core;stress;profile"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        CHECKSUM_SMOKE "23535464"
        CHECKSUM_CORE "672287189"
        CHECKSUM_PROFILE "953809920"
        CHECKSUM_STRESS "314490530")

zr_vm_register_benchmark_case(
        sort_array
        DESCRIPTION "Array sorting across random, descending, duplicates, and near-sorted patterns."
        PASS_BANNER "BENCH_SORT_ARRAY_PASS"
        WORKLOAD_TAG "index,branch,loop"
        PROFILE_SCALE 2
        TIERS "smoke;core;stress;profile"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        CHECKSUM_SMOKE "253437194"
        CHECKSUM_CORE "706926966"
        CHECKSUM_PROFILE "637904378"
        CHECKSUM_STRESS "57346602")

zr_vm_register_benchmark_case(
        prime_trial_division
        DESCRIPTION "Prime trial division with branch-heavy modulus checks."
        PASS_BANNER "BENCH_PRIME_TRIAL_DIVISION_PASS"
        WORKLOAD_TAG "branch,arith,loop"
        PROFILE_SCALE 2
        TIERS "smoke;core;stress;profile"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        CHECKSUM_SMOKE "77881285"
        CHECKSUM_CORE "32196849"
        CHECKSUM_PROFILE "278812750"
        CHECKSUM_STRESS "531306230")

zr_vm_register_benchmark_case(
        matrix_add_2d
        DESCRIPTION "Two-dimensional matrix add/copy pressure."
        PASS_BANNER "BENCH_MATRIX_ADD_2D_PASS"
        WORKLOAD_TAG "index,copy,loop"
        PROFILE_SCALE 1
        TIERS "core;stress;profile"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        CHECKSUM_SMOKE "139381755"
        CHECKSUM_CORE "76802768"
        CHECKSUM_PROFILE "139381755"
        CHECKSUM_STRESS "284499645")

zr_vm_register_benchmark_case(
        string_build
        DESCRIPTION "Repeated string assembly with deterministic fragment mixes."
        PASS_BANNER "BENCH_STRING_BUILD_PASS"
        WORKLOAD_TAG "string,object,copy"
        PROFILE_SCALE 2
        TIERS "core;stress;profile"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        CORE_IMPLEMENTATIONS "c" "zr_interp" "zr_binary" "python"
        CHECKSUM_SMOKE "681635505"
        CHECKSUM_CORE "353247225"
        CHECKSUM_PROFILE "62693157"
        CHECKSUM_STRESS "591385617")

zr_vm_register_benchmark_case(
        map_object_access
        DESCRIPTION "Field updates plus deterministic map construction and lookup."
        PASS_BANNER "BENCH_MAP_OBJECT_ACCESS_PASS"
        WORKLOAD_TAG "object,index,branch"
        PROFILE_SCALE 2
        TIERS "core;stress;profile"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        CHECKSUM_SMOKE "751187833"
        CHECKSUM_CORE "173458768"
        CHECKSUM_PROFILE "317868026"
        CHECKSUM_STRESS "785582717")

zr_vm_register_benchmark_case(
        fib_recursive
        DESCRIPTION "Recursive Fibonacci call pressure with deterministic checksum folding."
        PASS_BANNER "BENCH_FIB_RECURSIVE_PASS"
        WORKLOAD_TAG "call,recursion"
        PROFILE_SCALE 1
        TIERS "smoke;core;stress;profile"
        IMPLEMENTATIONS "c" "zr_interp" "python" "node" "java"
        CORE_IMPLEMENTATIONS "c" "zr_interp" "python"
        CHECKSUM_SMOKE "79101464"
        CHECKSUM_CORE "110316398"
        CHECKSUM_PROFILE "79101464"
        CHECKSUM_STRESS "647395402")

zr_vm_register_benchmark_case(
        call_chain_polymorphic
        DESCRIPTION "Layered function chains plus polymorphic callable dispatch and tail recursion."
        PASS_BANNER "BENCH_CALL_CHAIN_POLYMORPHIC_PASS"
        WORKLOAD_TAG "call,dispatch"
        PROFILE_SCALE 1
        TIERS "smoke;core;stress;profile"
        IMPLEMENTATIONS "c" "zr_interp" "python" "node" "java"
        CORE_IMPLEMENTATIONS "c" "zr_interp" "python"
        CHECKSUM_SMOKE "47250207"
        CHECKSUM_CORE "190794245"
        CHECKSUM_PROFILE "47250207"
        CHECKSUM_STRESS "773219295")

zr_vm_register_benchmark_case(
        object_field_hot
        DESCRIPTION "High-frequency object member reads and writes on a hot mutable record."
        PASS_BANNER "BENCH_OBJECT_FIELD_HOT_PASS"
        WORKLOAD_TAG "object,member"
        PROFILE_SCALE 1
        TIERS "smoke;core;stress;profile"
        IMPLEMENTATIONS "c" "zr_interp" "python" "node" "java"
        CORE_IMPLEMENTATIONS "c" "zr_interp" "python"
        CHECKSUM_SMOKE "623146080"
        CHECKSUM_CORE "487175651"
        CHECKSUM_PROFILE "623146080"
        CHECKSUM_STRESS "997005878")

zr_vm_register_benchmark_case(
        array_index_dense
        DESCRIPTION "Dense indexed loads and stores across a mutable integer buffer."
        PASS_BANNER "BENCH_ARRAY_INDEX_DENSE_PASS"
        WORKLOAD_TAG "index,array"
        PROFILE_SCALE 1
        TIERS "smoke;core;stress;profile"
        IMPLEMENTATIONS "c" "zr_interp" "python" "node" "java"
        CORE_IMPLEMENTATIONS "c" "zr_interp" "python"
        CHECKSUM_SMOKE "175707665"
        CHECKSUM_CORE "723012102"
        CHECKSUM_PROFILE "175707665"
        CHECKSUM_STRESS "563950751")

zr_vm_register_benchmark_case(
        branch_jump_dense
        DESCRIPTION "Branch-heavy control flow with nested conditionals and loop-carried state."
        PASS_BANNER "BENCH_BRANCH_JUMP_DENSE_PASS"
        WORKLOAD_TAG "branch,loop"
        PROFILE_SCALE 1
        TIERS "smoke;core;stress;profile"
        IMPLEMENTATIONS "c" "zr_interp" "python" "node" "java"
        CORE_IMPLEMENTATIONS "c" "zr_interp" "python"
        CHECKSUM_SMOKE "237632615"
        CHECKSUM_CORE "994956831"
        CHECKSUM_PROFILE "237632615"
        CHECKSUM_STRESS "693776936")

zr_vm_register_benchmark_case(
        mixed_service_loop
        DESCRIPTION "Mixed service-style loop combining calls, object fields, array indexing, and branching."
        PASS_BANNER "BENCH_MIXED_SERVICE_LOOP_PASS"
        WORKLOAD_TAG "call,object,index,branch"
        PROFILE_SCALE 1
        TIERS "smoke;core;stress;profile"
        IMPLEMENTATIONS "c" "zr_interp" "python" "node" "java"
        CORE_IMPLEMENTATIONS "c" "zr_interp" "python"
        CHECKSUM_SMOKE "408940136"
        CHECKSUM_CORE "408679425"
        CHECKSUM_PROFILE "408940136"
        CHECKSUM_STRESS "537757139")

zr_vm_register_benchmark_case(
        gc_fragment_baseline
        DESCRIPTION "String and container churn without explicit GC forcing; baseline for GC overhead deltas."
        PASS_BANNER "BENCH_GC_FRAGMENT_BASELINE_PASS"
        WORKLOAD_TAG "gc,string,container,baseline"
        PROFILE_SCALE 1
        TIERS "core;stress;profile"
        IMPLEMENTATIONS "c" "zr_interp" "zr_binary"
        CORE_IMPLEMENTATIONS "c" "zr_interp" "zr_binary"
        CHECKSUM_SMOKE "829044624"
        CHECKSUM_CORE "857265678"
        CHECKSUM_PROFILE "829044624"
        CHECKSUM_STRESS "47994849")

zr_vm_register_benchmark_case(
        gc_fragment_stress
        DESCRIPTION "Explicit GC-heavy string and container churn with sustained survivor pressure."
        PASS_BANNER "BENCH_GC_FRAGMENT_STRESS_PASS"
        WORKLOAD_TAG "gc,string,container"
        PROFILE_SCALE 1
        TIERS "core;stress;profile"
        IMPLEMENTATIONS "c" "zr_interp" "zr_binary"
        CORE_IMPLEMENTATIONS "c" "zr_interp" "zr_binary"
        CHECKSUM_SMOKE "829044624"
        CHECKSUM_CORE "857265678"
        CHECKSUM_PROFILE "829044624"
        CHECKSUM_STRESS "47994849")
