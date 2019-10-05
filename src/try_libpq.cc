/*
2019-10-05T11:22:43Z
morpheus@localhost§~/subu_land/only-one/src§
> g++ try_libpq.cc -o try_libpq -lpq
> ./try_libpq
Version of libpq: 110005

*/


#include <stdio.h>
#include <libpq-fe.h>

int main() {
    
    int lib_ver = PQlibVersion();

    printf("Version of libpq: %d\n", lib_ver);
    
    return 0;
}
