#ifndef TUI_H
#define TUI_H

#include <stddef.h>

struct TorrentResult;

/*
 * Loop interativo até o utilizador sair (q).
 * *results / *count são propriedade da TUI após a chamada: em nova busca (/),
 * a lista anterior é libertada e substituída.
 * Retorno: 0 sucesso; != 0 erro (ncurses ou busca fatal após init).
 */
int tui_run (struct TorrentResult **results, size_t *count);

#endif
