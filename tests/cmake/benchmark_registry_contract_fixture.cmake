# 由 registry 契约测试在独立 cmake -P 进程中调用；MODE 隔离预期失败，避免
# zr_vm_register_benchmark_case 的 FATAL_ERROR 直接终止父测试进程。
if (NOT DEFINED REGISTRY_FILE OR REGISTRY_FILE STREQUAL "")
    message(FATAL_ERROR "REGISTRY_FILE is required")
endif ()
if (NOT DEFINED MODE OR MODE STREQUAL "")
    message(FATAL_ERROR "MODE is required")
endif ()

include("${REGISTRY_FILE}")

if (MODE STREQUAL "valid")
    # 已注册 case 保持缺省采样下限，新增 case 的显式下限则由注册接口原样传播。
    foreach (case_name IN LISTS ZR_VM_BENCHMARK_CASE_NAMES)
        if (NOT ZR_VM_BENCHMARK_MIN_SAMPLE_MS_${case_name} STREQUAL "750")
            message(FATAL_ERROR
                    "${case_name} expected default MIN_SAMPLE_MS=750, got ${ZR_VM_BENCHMARK_MIN_SAMPLE_MS_${case_name}}")
        endif ()
    endforeach ()

    zr_vm_register_benchmark_case(
            registry_contract_explicit
            DESCRIPTION "Registry contract fixture."
            PASS_BANNER "REGISTRY_CONTRACT_PASS"
            MIN_SAMPLE_MS 1234
            TIERS smoke
            IMPLEMENTATIONS c
            CHECKSUM_SMOKE 1
            CHECKSUM_CORE 1
            CHECKSUM_PROFILE 1
            CHECKSUM_STRESS 1)
    if (NOT ZR_VM_BENCHMARK_MIN_SAMPLE_MS_registry_contract_explicit STREQUAL "1234")
        message(FATAL_ERROR "explicit MIN_SAMPLE_MS was not preserved")
    endif ()
elseif (MODE STREQUAL "zero" OR MODE STREQUAL "negative" OR MODE STREQUAL "non_integer")
    if (MODE STREQUAL "zero")
        set(invalid_value 0)
    elseif (MODE STREQUAL "negative")
        set(invalid_value -1)
    else ()
        set(invalid_value 750ms)
    endif ()
    zr_vm_register_benchmark_case(
            registry_contract_invalid
            DESCRIPTION "Registry invalid contract fixture."
            PASS_BANNER "REGISTRY_CONTRACT_INVALID"
            MIN_SAMPLE_MS "${invalid_value}"
            TIERS smoke
            IMPLEMENTATIONS c
            CHECKSUM_SMOKE 1
            CHECKSUM_CORE 1
            CHECKSUM_PROFILE 1
            CHECKSUM_STRESS 1)
else ()
    message(FATAL_ERROR "unknown MODE: ${MODE}")
endif ()
