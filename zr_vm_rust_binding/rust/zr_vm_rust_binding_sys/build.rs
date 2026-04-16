use std::env;

fn main() {
    println!("cargo:rerun-if-env-changed=ZR_VM_RUST_BINDING_LIB_DIR");

    let lib_dir = env::var("ZR_VM_RUST_BINDING_LIB_DIR")
        .expect("ZR_VM_RUST_BINDING_LIB_DIR must point to the zr_vm_rust_binding import library directory");
    println!("cargo:rustc-link-search=native={lib_dir}");
    println!("cargo:rustc-link-lib=dylib=zr_vm_rust_binding");
}
