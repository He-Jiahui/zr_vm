#include "host.h"
#include "backend.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    ZrgOptions options;
    ZrgHostVm vm = {0};
    if (!zrg_options_parse(argc, argv, &options)) return 2;
    if (options.help) {
        zrg_options_usage();
        return 0;
    }
    bool graphics = !strcmp(options.render, "gl");
    ZrgBackendDesc desc = {ZRG_RENDER_WIDTH, ZRG_RENDER_HEIGHT, options.headless};
    if (!zrg_platform_open(graphics, options.headless, options.interactive)) {
        fprintf(stderr, "SWARM_ERROR --render could not create GL window/context\n");
        return 2;
    }
    if (!zrg_backend_open(options.render, &desc)) {
        fprintf(stderr, "SWARM_ERROR --render backend init failed\n");
        zrg_platform_close();
        return 2;
    }
    if (!zrg_host_vm_open(&vm, &options)) {
        zrg_host_vm_close(&vm);
        zrg_backend_close();
        zrg_platform_close();
        return 2;
    }
    int status = zrg_host_loop(&vm, &options);
    zrg_host_vm_close(&vm);
    zrg_backend_close();
    zrg_platform_close();
    return status;
}
