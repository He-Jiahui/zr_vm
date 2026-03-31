#include "zr_vm_cli.h"

#include "zr_vm_cli/app.h"

void ZrCli_Main(const int argc, char **argv) {
    (void) ZrCli_App_Run(argc, argv);
}

int main(const int argc, char **argv) {
    return ZrCli_App_Run(argc, argv);
}
