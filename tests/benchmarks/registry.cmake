include(CMakeParseArguments)

set(ZR_VM_BENCHMARK_CASE_NAMES "")
set(ZR_VM_BENCHMARK_IMPLEMENTATION_ORDER
        "c;zr_interp;zr_binary;zr_aot_c;zr_aot_llvm;python;node;rust;dotnet")

set(ZR_VM_BENCHMARK_TIER_SCALE_smoke 1)
set(ZR_VM_BENCHMARK_TIER_SCALE_core 4)
set(ZR_VM_BENCHMARK_TIER_SCALE_stress 16)

set(ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS "${ZR_VM_BENCHMARK_IMPLEMENTATION_ORDER}")

function(zr_vm_register_benchmark_case name)
    set(options REQUIRE_REAL_AOT)
    set(oneValueArgs
            DESCRIPTION
            PASS_BANNER
            CHECKSUM_SMOKE
            CHECKSUM_CORE
            CHECKSUM_STRESS)
    set(multiValueArgs
            TIERS
            IMPLEMENTATIONS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT ARG_DESCRIPTION OR NOT ARG_PASS_BANNER)
        message(FATAL_ERROR "zr_vm_register_benchmark_case(${name}) requires DESCRIPTION and PASS_BANNER")
    endif ()
    if (NOT ARG_TIERS)
        message(FATAL_ERROR "zr_vm_register_benchmark_case(${name}) requires TIERS")
    endif ()
    if (NOT DEFINED ARG_CHECKSUM_SMOKE OR NOT DEFINED ARG_CHECKSUM_CORE OR NOT DEFINED ARG_CHECKSUM_STRESS)
        message(FATAL_ERROR "zr_vm_register_benchmark_case(${name}) requires CHECKSUM_SMOKE/CHECKSUM_CORE/CHECKSUM_STRESS")
    endif ()

    if (NOT ARG_IMPLEMENTATIONS)
        set(ARG_IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}")
    endif ()

    list(APPEND ZR_VM_BENCHMARK_CASE_NAMES "${name}")
    set(ZR_VM_BENCHMARK_CASE_NAMES "${ZR_VM_BENCHMARK_CASE_NAMES}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_DESCRIPTION_${name}" "${ARG_DESCRIPTION}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_PASS_BANNER_${name}" "${ARG_PASS_BANNER}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_TIERS_${name}" "${ARG_TIERS}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_IMPLEMENTATIONS_${name}" "${ARG_IMPLEMENTATIONS}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_CHECKSUM_${name}_smoke" "${ARG_CHECKSUM_SMOKE}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_CHECKSUM_${name}_core" "${ARG_CHECKSUM_CORE}" PARENT_SCOPE)
    set("ZR_VM_BENCHMARK_CHECKSUM_${name}_stress" "${ARG_CHECKSUM_STRESS}" PARENT_SCOPE)
    if (ARG_REQUIRE_REAL_AOT)
        set("ZR_VM_BENCHMARK_REQUIRE_REAL_AOT_${name}" TRUE PARENT_SCOPE)
    else ()
        set("ZR_VM_BENCHMARK_REQUIRE_REAL_AOT_${name}" FALSE PARENT_SCOPE)
    endif ()
endfunction()

zr_vm_register_benchmark_case(
        numeric_loops
        DESCRIPTION "Integer arithmetic and branch-heavy loops."
        PASS_BANNER "BENCH_NUMERIC_LOOPS_PASS"
        TIERS "smoke;core;stress"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        REQUIRE_REAL_AOT
        CHECKSUM_SMOKE "48943705"
        CHECKSUM_CORE "793446923"
        CHECKSUM_STRESS "664618747")

zr_vm_register_benchmark_case(
        dispatch_loops
        DESCRIPTION "Polymorphic dispatch loops aligned with interface-call pressure."
        PASS_BANNER "BENCH_DISPATCH_LOOPS_PASS"
        TIERS "core;stress"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        REQUIRE_REAL_AOT
        CHECKSUM_SMOKE "522873290"
        CHECKSUM_CORE "320214929"
        CHECKSUM_STRESS "684820768")

zr_vm_register_benchmark_case(
        container_pipeline
        DESCRIPTION "Linked-list, set, and map aggregation pipeline."
        PASS_BANNER "BENCH_CONTAINER_PIPELINE_PASS"
        TIERS "core;stress"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        REQUIRE_REAL_AOT
        CHECKSUM_SMOKE "23535464"
        CHECKSUM_CORE "672287189"
        CHECKSUM_STRESS "314490530")

zr_vm_register_benchmark_case(
        sort_array
        DESCRIPTION "Array sorting across random, descending, duplicates, and near-sorted patterns."
        PASS_BANNER "BENCH_SORT_ARRAY_PASS"
        TIERS "smoke;core;stress"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        REQUIRE_REAL_AOT
        CHECKSUM_SMOKE "253437194"
        CHECKSUM_CORE "706926966"
        CHECKSUM_STRESS "57346602")

zr_vm_register_benchmark_case(
        prime_trial_division
        DESCRIPTION "Prime trial division with branch-heavy modulus checks."
        PASS_BANNER "BENCH_PRIME_TRIAL_DIVISION_PASS"
        TIERS "smoke;core;stress"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        REQUIRE_REAL_AOT
        CHECKSUM_SMOKE "77881285"
        CHECKSUM_CORE "32196849"
        CHECKSUM_STRESS "531306230")

zr_vm_register_benchmark_case(
        matrix_add_2d
        DESCRIPTION "Two-dimensional matrix add/copy pressure."
        PASS_BANNER "BENCH_MATRIX_ADD_2D_PASS"
        TIERS "core;stress"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        REQUIRE_REAL_AOT
        CHECKSUM_SMOKE "139381755"
        CHECKSUM_CORE "76802768"
        CHECKSUM_STRESS "284499645")

zr_vm_register_benchmark_case(
        string_build
        DESCRIPTION "Repeated string assembly with deterministic fragment mixes."
        PASS_BANNER "BENCH_STRING_BUILD_PASS"
        TIERS "core;stress"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        REQUIRE_REAL_AOT
        CHECKSUM_SMOKE "681635505"
        CHECKSUM_CORE "353247225"
        CHECKSUM_STRESS "591385617")

zr_vm_register_benchmark_case(
        map_object_access
        DESCRIPTION "Field updates plus deterministic map construction and lookup."
        PASS_BANNER "BENCH_MAP_OBJECT_ACCESS_PASS"
        TIERS "core;stress"
        IMPLEMENTATIONS "${ZR_VM_BENCHMARK_DEFAULT_IMPLEMENTATIONS}"
        REQUIRE_REAL_AOT
        CHECKSUM_SMOKE "751187833"
        CHECKSUM_CORE "173458768"
        CHECKSUM_STRESS "785582717")
