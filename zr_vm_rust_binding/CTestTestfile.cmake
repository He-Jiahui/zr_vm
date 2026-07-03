# CMake generated Testfile for 
# Source directory: /mnt/e/Git/zr_vm/zr_vm_rust_binding
# Build directory: /mnt/e/Git/zr_vm/zr_vm_rust_binding
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(zr_vm_rust_binding_cargo_check "/usr/bin/cmake" "-E" "env" "CARGO_TARGET_DIR=/mnt/e/Git/zr_vm/cargo/zr_vm_rust_binding" "ZR_VM_RUST_BINDING_LIB_DIR=/mnt/e/Git/zr_vm/lib" "LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/lib:" "/home/hejiahui/.cargo/bin/cargo" "check" "--workspace")
set_tests_properties(zr_vm_rust_binding_cargo_check PROPERTIES  WORKING_DIRECTORY "/mnt/e/Git/zr_vm/zr_vm_rust_binding/rust" _BACKTRACE_TRIPLES "/mnt/e/Git/zr_vm/zr_vm_rust_binding/CMakeLists.txt;127;add_test;/mnt/e/Git/zr_vm/zr_vm_rust_binding/CMakeLists.txt;0;")
add_test(zr_vm_rust_binding_cargo_test "/usr/bin/cmake" "-E" "env" "CARGO_TARGET_DIR=/mnt/e/Git/zr_vm/cargo/zr_vm_rust_binding" "ZR_VM_RUST_BINDING_LIB_DIR=/mnt/e/Git/zr_vm/lib" "LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/lib:" "/home/hejiahui/.cargo/bin/cargo" "test" "--workspace")
set_tests_properties(zr_vm_rust_binding_cargo_test PROPERTIES  WORKING_DIRECTORY "/mnt/e/Git/zr_vm/zr_vm_rust_binding/rust" _BACKTRACE_TRIPLES "/mnt/e/Git/zr_vm/zr_vm_rust_binding/CMakeLists.txt;139;add_test;/mnt/e/Git/zr_vm/zr_vm_rust_binding/CMakeLists.txt;0;")
