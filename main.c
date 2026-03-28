#include <stdio.h>
#include <stdlib.h>
#include "url.h"

int main(int argc, char *argv[]) {

    printf("total de argumentos %d\n", argc);
    char *url = build_url("https://apibay.org/q.php?q=", argc, argv);

    printf("%s\n", url);
    

    for(int i = 0; i < argc; i++) {
        printf("argv[%d] = %s", i, argv[i]);
    }

}