# Configure the production modules with only the native declaration regression.
# Pass -DBUILD_TESTS=OFF -DCMAKE_PROJECT_ZR_VM_INCLUDE=<this file> to the root project.
function(native_declaration_add_focused_tests)
    if (NOT TARGET zr_vm_native_declaration_signatures_test)
        include(${CMAKE_SOURCE_DIR}/tests/cmake/native-declaration-signatures-tests.cmake)
    endif ()
endfunction()
cmake_language(DEFER CALL native_declaration_add_focused_tests)
