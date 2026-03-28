/* Alinha análise do IDE/clangd ao Makefile: ncurses + POSIX (evita falsos erros em KEY_RESIZE, ERR, etc.). */
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#endif
#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif

#include "tui.h"
#include "fetch.h"
#include "url.h"
#include <ctype.h>
#include <curses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#define API_BASE "https://apibay.org/q.php?q="
#define QUERY_MAX 512

/* Mesma semântica que build_url com argv fictício a partir da linha de busca. */
static char *build_url_from_line (const char *base, const char *line) {
    if (!line || !base)
        return NULL;

    size_t linelen = strlen(line);
    char *copy = (char *)malloc(linelen + 1);
    if (!copy)
        return NULL;
    memcpy(copy, line, linelen + 1);

    char *argv[128];
    int argc = 1;
    argv[0] = "torrent-search";

    char *save = NULL;
    char *tok = strtok_r(copy, " \t\r\n", &save);
    while (tok && argc < (int)(sizeof(argv) / sizeof(argv[0])) - 1) {
        argv[argc++] = tok;
        tok = strtok_r(NULL, " \t\r\n", &save);
    }

    if (argc < 2) {
        free(copy);
        return NULL;
    }

    char *url = build_url(base, argc, argv);
    free(copy);
    return url;
}

static volatile sig_atomic_t g_resize;

/* Último tamanho já aplicado em layout_create (evita relayout duplicado em SIGWINCH+KEY_RESIZE). */
static int s_term_rows;
static int s_term_cols;

static WINDOW *s_left;
static WINDOW *s_right;
static WINDOW *s_footer;

static void on_sigwinch (int signo) {
    (void)signo;
    g_resize = 1;
}

static void init_pair_highlight (void) {
    if (!has_colors())
        return;
    if (start_color() != OK)
        return;
    if (use_default_colors() == OK)
        init_pair(1, COLOR_WHITE, -1);
    else
        init_pair(1, COLOR_WHITE, COLOR_BLUE);
}

static void destroy_windows (void) {
    if (s_left) {
        delwin(s_left);
        s_left = NULL;
    }
    if (s_right) {
        delwin(s_right);
        s_right = NULL;
    }
    if (s_footer) {
        delwin(s_footer);
        s_footer = NULL;
    }
}

static int layout_create (void) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    if (rows < 4 || cols < 30)
        return -1;

    int fh = 1;
    int mh = rows - fh;
    if (mh < 2)
        return -1;

    int lw = (cols * 2) / 5;
    if (lw < 16)
        lw = 16;
    if (lw > cols - 18)
        lw = cols - 18;
    if (lw < 8)
        return -1;

    int rw = cols - lw - 1;
    if (rw < 10)
        return -1;

    s_left = newwin(mh, lw, 0, 0);
    s_right = newwin(mh, rw, 0, lw + 1);
    s_footer = newwin(fh, cols, mh, 0);
    if (!s_left || !s_right || !s_footer) {
        destroy_windows();
        return -1;
    }
    keypad(s_left, TRUE);
    keypad(s_right, TRUE);
    keypad(s_footer, TRUE);
    /* Evita wgetch()==ERR por timeout ao resolver teclas especiais / escapes. */
    notimeout(s_left, TRUE);
    notimeout(s_right, TRUE);
    notimeout(s_footer, TRUE);
    return 0;
}

/*
 * Aplica novo tamanho ao ncurses e recria painéis só se ioctl reportar dimensões
 * diferentes das já em uso. Evita piscar ao arrastar a janela (muitos eventos
 * repetidos) e remove refresh() intermédio que pintava um frame vazio.
 * Retorno: 0 ok, -1 falha. *did_change == 1 se houve relayout real.
 */
static int try_relayout (int *did_change) {
    int tr = 0;
    int tc = 0;

    *did_change = 0;

#ifdef TIOCGWINSZ
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        tr = (int)ws.ws_row;
        tc = (int)ws.ws_col;
    } else
#endif
    {
        if (!stdscr)
            return 0;
        getmaxyx(stdscr, tr, tc);
    }

    if (tr == s_term_rows && tc == s_term_cols)
        return 0;

    if (resizeterm(tr, tc) == ERR)
        return -1;

    destroy_windows();
    werase(stdscr);
    /* Sem refresh() aqui — só doupdate() em full_redraw evita flash branco. */

    if (layout_create() != 0)
        return -1;

    s_term_rows = tr;
    s_term_cols = tc;
    *did_change = 1;
    return 0;
}

static void draw_separator (int mh, int lw) {
    if (lw <= 0 || mh <= 0)
        return;
    for (int r = 0; r < mh; r++)
        mvaddch(r, lw, ACS_VLINE);
}

static void trunc_fit (const char *s, int maxcols, char *out, size_t outsiz) {
    if (maxcols < 1 || outsiz == 0) {
        if (outsiz)
            out[0] = '\0';
        return;
    }
    if (!s) {
        out[0] = '\0';
        return;
    }
    size_t len = strlen(s);
    if ((int)len <= maxcols) {
        strncpy(out, s, outsiz - 1);
        out[outsiz - 1] = '\0';
        return;
    }
    if (maxcols < 4) {
        strncpy(out, s, (size_t)maxcols);
        out[(size_t)maxcols] = '\0';
        return;
    }
    snprintf(out, outsiz, "%.*s...", maxcols - 3, s);
}

static void open_magnet_xdg (const char *magnet) {
    if (!magnet || !magnet[0])
        return;

    fflush(NULL);
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = {"xdg-open", (char *)magnet, NULL};
        execvp("xdg-open", argv);
        _exit(127);
    }
    if (pid > 0) {
        int st = 0;
        (void)waitpid(pid, &st, 0);
    }
}

/* Quebra por palavras; segmentos maiores que width partem à força (útil para magnet sem espaços). */
static int print_wrapped (WINDOW *w, int y, int x0, int ymax, int width, const char *text) {
    if (!text || width < 1 || y >= ymax)
        return y;

    const char *p = text;
    while (*p && y < ymax) {
        char line[512];
        line[0] = '\0';
        int col = 0;

        while (*p && y < ymax) {
            while (*p == ' ')
                p++;
            if (!*p)
                break;

            const char *ws = p;
            while (*p && *p != ' ')
                p++;
            int wlen = (int)(p - ws);

            if (wlen > width) {
                if (col > 0) {
                    p = ws;
                    break;
                }
                const char *cur = ws;
                int remain = wlen;
                while (remain > 0 && y < ymax) {
                    int chunk = remain < width ? remain : width;
                    char tmp[512];
                    if (chunk >= (int)sizeof(tmp))
                        chunk = (int)sizeof(tmp) - 1;
                    memcpy(tmp, cur, (size_t)chunk);
                    tmp[chunk] = '\0';
                    mvwprintw(w, y++, x0, "%s", tmp);
                    wclrtoeol(w);
                    cur += chunk;
                    remain -= chunk;
                }
                continue;
            }

            int need = wlen + (col > 0 ? 1 : 0);
            if (col + need > width) {
                p = ws;
                break;
            }

            if (col > 0) {
                size_t bl = strlen(line);
                if (bl + 1 >= sizeof(line))
                    break;
                line[bl] = ' ';
                line[bl + 1] = '\0';
                col++;
            }

            {
                size_t bl = strlen(line);
                if ((size_t)wlen >= sizeof(line) - bl) {
                    p = ws;
                    break;
                }
                memcpy(line + bl, ws, (size_t)wlen);
                line[bl + (size_t)wlen] = '\0';
                col += wlen;
            }
        }

        if (line[0] != '\0' && y < ymax) {
            mvwprintw(w, y++, x0, "%s", line);
            wclrtoeol(w);
        }
    }
    return y;
}

static void draw_details (WINDOW *w, const TorrentResult *r) {
    werase(w);
    box(w, 0, 0);

    if (!r) {
        mvwprintw(w, 1, 2, "(sem resultado)");
        wnoutrefresh(w);
        return;
    }

    int inner_h = getmaxy(w) - 2;
    int inner_w = getmaxx(w) - 4;
    if (inner_w < 4 || inner_h < 2) {
        wnoutrefresh(w);
        return;
    }

    char sizestr[64];
    torrent_format_size(r->size_bytes, sizestr, sizeof sizestr);

    int ymax = 1 + inner_h;
    int row = 1;

    mvwprintw(w, row++, 2, "Nome:");
    row = print_wrapped(w, row, 2, ymax, inner_w, r->name);

    if (row < ymax)
        mvwprintw(w, row++, 2, "Seeders: %d", r->seeders);
    if (row < ymax)
        mvwprintw(w, row++, 2, "Tamanho: %s", sizestr);
    if (row < ymax)
        row++;
    if (row < ymax)
        mvwprintw(w, row++, 2, "Magnet Link:");
    if (row < ymax)
        row = print_wrapped(w, row, 2, ymax, inner_w, r->magnet);

    wnoutrefresh(w);
}

static void draw_list (WINDOW *w, const TorrentResult *items, size_t n,
                       size_t sel, size_t scroll, int *out_scroll) {
    werase(w);
    box(w, 0, 0);

    int h = getmaxy(w);
    int inner_w = getmaxx(w) - 4;
    int rows = h - 2;
    if (rows < 1 || n == 0) {
        if (n == 0)
            mvwprintw(w, 1, 2, "(vazio — use / para buscar)");
        wnoutrefresh(w);
        *out_scroll = 0;
        return;
    }

    if (sel >= n)
        sel = n - 1;

    if (sel >= scroll + (size_t)rows)
        scroll = sel + 1 - (size_t)rows;
    if (sel < scroll)
        scroll = sel;

    *out_scroll = (int)scroll;

    char sizebuf[64];
    char linebuf[512];
    char rowtxt[1024];
    for (int r = 0; r < rows; r++) {
        size_t idx = scroll + (size_t)r;
        if (idx >= n)
            break;

        const TorrentResult *it = &items[idx];
        torrent_format_size(it->size_bytes, sizebuf, sizeof sizebuf);

        if (snprintf(linebuf, sizeof linebuf, "[Seeds: %d] ", it->seeders) < 0)
            linebuf[0] = '\0';
        int prefix = (int)strlen(linebuf);
        int rest = inner_w - prefix - 1 - (int)strlen(sizebuf);
        if (rest < 4)
            rest = 4;

        char namecut[256];
        trunc_fit(it->name, rest, namecut, sizeof namecut);
        snprintf(rowtxt, sizeof rowtxt, "%s%s %s", linebuf, namecut, sizebuf);

        if (idx == sel) {
            if (has_colors())
                wattron(w, COLOR_PAIR(1) | A_BOLD);
            else
                wattron(w, A_REVERSE);
        }
        mvwprintw(w, r + 1, 2, "%s", rowtxt);
        if (idx == sel) {
            if (has_colors())
                wattroff(w, COLOR_PAIR(1) | A_BOLD);
            else
                wattroff(w, A_REVERSE);
            mvwaddch(w, r + 1, 1, ACS_RARROW);
        }
        wclrtoeol(w);
    }
    wnoutrefresh(w);
}

static void draw_footer (WINDOW *w, size_t sel_idx, size_t total) {
    werase(w);
    wattron(w, A_REVERSE);
    mvwprintw(w, 0, 0,
                " [Up/Down] Move | [Enter] Open | [/] Search | [Q] Exit ");
    int cols = getmaxx(w);
    char tail[48];
    snprintf(tail, sizeof tail, "%zu/%zu", total ? sel_idx + 1 : 0, total);
    int tl = (int)strlen(tail);
    if (tl < cols)
        mvwprintw(w, 0, cols - tl - 1, "%s", tail);
    wattroff(w, A_REVERSE);
    wnoutrefresh(w);
}

static void full_redraw (const TorrentResult *items, size_t n, size_t sel, int *scroll) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)cols;

    int mh = rows - 1;
    int lw = getmaxx(s_left);

    /*
     * stdscr cobre o terminal inteiro. Se chamarmos wnoutrefresh(stdscr)
     * depois dos painéis, o ecrã fica só com o stdscr apagado + separador
     * e apaga lista/detalhes. Ordem correta: fundo em stdscr primeiro,
     * depois os newwin() por cima.
     */
    werase(stdscr);
    draw_separator(mh, lw);
    wnoutrefresh(stdscr);

    draw_list(s_left, items, n, sel, (size_t)*scroll, scroll);
    if (n && sel < n)
        draw_details(s_right, &items[sel]);
    else
        draw_details(s_right, NULL);
    draw_footer(s_footer, sel, n);
    doupdate();
}

static int read_new_query (char *buf, size_t buflen) {
    fflush(stdout);
    fflush(stderr);
    FILE *tty_out = fopen("/dev/tty", "w");
    FILE *tty_in = fopen("/dev/tty", "r");
    if (!tty_in || !tty_out) {
        if (tty_in)
            fclose(tty_in);
        if (tty_out)
            fclose(tty_out);
        return -1;
    }
    fputc('\n', tty_out);
    fprintf(tty_out, "Busca: ");
    fflush(tty_out);
    if (!fgets(buf, (int)buflen, tty_in)) {
        buf[0] = '\0';
    }
    fclose(tty_in);
    fclose(tty_out);
    return 0;
}

static int run_search_replace (TorrentResult **results, size_t *count,
                               const char *query_line) {
    char *url = build_url_from_line(API_BASE, query_line);
    if (!url) {
        return -1;
    }

    char *resp = fetch(url);
    free(url);
    if (!resp) {
        return -1;
    }

    TorrentResult *new_list = NULL;
    size_t new_n = 0;
    if (torrent_results_from_json(resp, &new_list, &new_n) != 0) {
        free(resp);
        return -1;
    }
    free(resp);

    torrent_results_free(*results, *count);
    *results = new_list;
    *count = new_n;
    return 0;
}

int tui_run (struct TorrentResult **results, size_t *count) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_sigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGWINCH, &sa, NULL) != 0)
        return 1;

    if (!initscr()) {
        return 1;
    }

    int rc = 1;
    cbreak();
    noecho();
    nonl();
    curs_set(0);
    keypad(stdscr, TRUE);
    init_pair_highlight();

    if (layout_create() != 0) {
        destroy_windows();
        endwin();
        return 1;
    }
    getmaxyx(stdscr, s_term_rows, s_term_cols);

    size_t sel = 0;
    int scroll = 0;
    int ch;
    int need_redraw = 1;

    rc = 0;

    for (;;) {
        if (g_resize) {
            g_resize = 0;
            int chg = 0;
            if (try_relayout(&chg) != 0) {
                rc = 1;
                break;
            }
            if (chg)
                need_redraw = 1;
        }

        if (*count && sel >= *count)
            sel = *count - 1;

        if (need_redraw) {
            full_redraw(*results, *count, sel, &scroll);
            need_redraw = 0;
        }

        ch = wgetch(s_left);

        /* ERR sem resize: não repintar (evita loop apertado e ecrã a piscar). */
        if (ch == ERR) {
            if (g_resize) {
                g_resize = 0;
                int chg = 0;
                if (try_relayout(&chg) != 0) {
                    rc = 1;
                    break;
                }
                if (chg)
                    need_redraw = 1;
            }
            continue;
        }

        if (ch == KEY_RESIZE || g_resize) {
            g_resize = 0;
            int chg = 0;
            if (try_relayout(&chg) != 0) {
                rc = 1;
                break;
            }
            if (chg)
                need_redraw = 1;
            continue;
        }

        if (ch == 'q' || ch == 'Q') {
            break;
        }

        if (ch == '/') {
            destroy_windows();
            endwin();

            char qbuf[QUERY_MAX];
            if (read_new_query(qbuf, sizeof qbuf) != 0) {
                if (!initscr())
                    return 1;
                cbreak();
                noecho();
                nonl();
                curs_set(0);
                keypad(stdscr, TRUE);
                init_pair_highlight();
                if (layout_create() != 0) {
                    endwin();
                    return 1;
                }
                getmaxyx(stdscr, s_term_rows, s_term_cols);
                need_redraw = 1;
                continue;
            }

            if (!initscr()) {
                return 1;
            }
            cbreak();
            noecho();
            nonl();
            curs_set(0);
            keypad(stdscr, TRUE);
            init_pair_highlight();
            if (layout_create() != 0) {
                endwin();
                return 1;
            }
            getmaxyx(stdscr, s_term_rows, s_term_cols);

            char *trim = qbuf;
            while (*trim && isspace((unsigned char)*trim))
                trim++;
            if (*trim) {
                if (run_search_replace(results, count, trim) != 0) {
                    /* mantém resultados anteriores; só redesenha */
                }
                sel = 0;
                scroll = 0;
            }
            need_redraw = 1;
            continue;
        }

        if (*count == 0)
            continue;

        if (ch == KEY_DOWN || ch == 'j') {
            if (sel + 1 < *count) {
                sel++;
                need_redraw = 1;
            }
            continue;
        }
        if (ch == KEY_UP || ch == 'k') {
            if (sel > 0) {
                sel--;
                need_redraw = 1;
            }
            continue;
        }

        if (ch == '\n' || ch == KEY_ENTER || ch == '\r') {
            if (sel < *count && (*results)[sel].magnet)
                open_magnet_xdg((*results)[sel].magnet);
            continue;
        }
    }

    destroy_windows();
    endwin();
    return rc;
}
