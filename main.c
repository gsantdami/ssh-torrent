#include <stdio.h>
#include <stdlib.h>
#include "url.h"
#include "fetch.h"

int main(int argc, char *argv[]) {

    printf("total de argumentos %d\n", argc);
    char *url = build_url("https://apibay.org/q.php?q=", argc, argv);

    printf("%s\n", url);

    char *resposta = fetch(url);
    free(url);

    if(!resposta) {
        printf("Erro na busca");
        return 1;
    }
    parseResultados(resposta);
    free(resposta);

    return 0;

}