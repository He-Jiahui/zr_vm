if (NOT DEFINED C_COMPILER)
    message(FATAL_ERROR "C_COMPILER is required")
endif ()
if (NOT DEFINED ASM_SOURCE)
    message(FATAL_ERROR "ASM_SOURCE is required")
endif ()
if (NOT DEFINED ASM_OUTPUT)
    message(FATAL_ERROR "ASM_OUTPUT is required")
endif ()
if (NOT DEFINED GEN_DIR)
    message(FATAL_ERROR "GEN_DIR is required")
endif ()
if (NOT DEFINED VENDOR_ROOT)
    message(FATAL_ERROR "VENDOR_ROOT is required")
endif ()

get_filename_component(ASM_OUTPUT_DIR "${ASM_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${ASM_OUTPUT_DIR}")

execute_process(
        COMMAND "${C_COMPILER}"
        /nologo
        /EP
        /TC
        /D_M_AMD64=1
        /DX86_WIN64=1
        /DFFI_BUILDING=1
        /DFFI_STATIC_BUILD=1
        /I${GEN_DIR}
        /I${VENDOR_ROOT}/include
        /I${VENDOR_ROOT}/src
        /I${VENDOR_ROOT}/src/x86
        "${ASM_SOURCE}"
        OUTPUT_FILE "${ASM_OUTPUT}"
        ERROR_VARIABLE preprocess_stderr
        RESULT_VARIABLE preprocess_result
)

if (NOT preprocess_result EQUAL 0)
    message(FATAL_ERROR "libffi asm preprocess failed: ${preprocess_stderr}")
endif ()
