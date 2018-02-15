#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {

    sf_mem_init();



double *ptr = sf_malloc(sizeof(double));
sf_snapshot();

sf_malloc(1 );
sf_snapshot();

sf_malloc(1 );
sf_snapshot();

sf_malloc(2 );
sf_snapshot();

sf_malloc(3 );
sf_snapshot();


double *ptr2=sf_malloc(4 );
sf_snapshot();

sf_malloc(5);
sf_snapshot();

sf_malloc(33);
sf_snapshot();

sf_free(ptr);
sf_snapshot();

sf_free(ptr2);
sf_snapshot();

    sf_mem_fini();

    return EXIT_SUCCESS;
}
