#include "host.h"

int main(int argc, char **argv) {
    ZrgOptions options;
    ZrgHostVm vm = {0};
    if (!zrg_options_parse(argc, argv, &options)) return 2;
    if (options.help) {
        zrg_options_usage();
        return 0;
    }
    if (!zrg_host_vm_open(&vm, &options)) {
        zrg_host_vm_close(&vm);
        return 2;
    }
    int status = zrg_host_loop(&vm, &options);
    zrg_host_vm_close(&vm);
    return status;
}
