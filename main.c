#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "url.h"
#include "fetch.h"
#include "tui.h"

int main (int argc, char *argv[]) {
    char *url = build_url("https://apibay.org/q.php?q=", argc, argv);
    if (!url) {
        fprintf(stderr, "Erro: falha ao montar URL.\n");
        return 1;
    }

    char *resposta = fetch(url);
    free(url);

    if (!resposta) {
        fprintf(stderr, "Erro na busca.\n");
        return 1;
    }

    TorrentResult *list = NULL;
    size_t n = 0;
    if (torrent_results_from_json(resposta, &list, &n) != 0) {
        free(resposta);
        return 1;
    }
    free(resposta);

    if (isatty(STDOUT_FILENO)) {
        int tr = tui_run(&list, &n);
        torrent_results_free(list, n);
        return tr;
    }

    char sizebuf[64];
    for (size_t i = 0; i < n; i++) {
        torrent_format_size(list[i].size_bytes, sizebuf, sizeof sizebuf);
        printf("[SEEDERS: %d]     ", list[i].seeders);
        printf("%s     ", list[i].name);
        printf("SIZE: %s\n\n", sizebuf);
        printf("MAGNET LINK: %s\n\n", list[i].magnet);
    }
    torrent_results_free(list, n);
    return 0;
}
