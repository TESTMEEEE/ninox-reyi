#include "owlvm.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * Run the vm file.
 **/
int
main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Expected filename: Usage owlvm <file.vm>\n");
        exit(EXIT_FAILURE);
    }
    owl_ctx_t* vm = owl_vm_make();

    if (vm != NULL && owl_vm_evaluate(vm, argv[1]) == 0) {
        owl_vm_run(vm);
    }

    owl_vm_free(vm);
    return 0;
}
