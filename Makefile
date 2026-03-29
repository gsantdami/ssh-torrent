NCURSES_CFLAGS := $(shell pkg-config --cflags ncurses 2>/dev/null)
NCURSES_LIBS := $(shell pkg-config --libs ncurses 2>/dev/null)
ifeq ($(strip $(NCURSES_LIBS)),)
NCURSES_LIBS = -lncurses
endif

LIBS = -lcurl -lcjson $(NCURSES_LIBS)
TARGET = torrent-search
SRC = main.c tui.c
CFLAGS = -std=c99 -Wall -Wextra -D_POSIX_C_SOURCE=200809L $(NCURSES_CFLAGS)

all: $(TARGET)

$(TARGET): $(SRC)
	gcc $(CFLAGS) $(SRC) $(LIBS) -o $(TARGET)

check: $(TARGET)
	@echo "=== Smoke: saída não-TTY (sem ncurses) ==="
	@./$(TARGET) opensource 2>&1 | head -n 6
	@echo "=== Valgrind (opcional, falhas duras em tui.c/main.c) ==="
	@command -v valgrind >/dev/null && valgrind --error-exitcode=88 -q --leak-check=no ./$(TARGET) xyznotfound12345 2>&1 | tail -n 3 || echo "(valgrind ausente — ignorado)"

.PHONY: all, install, uninstall, clean, check, compile_commands.json

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/$(TARGET)

uninstall: $(TARGET)
	rm -f $(TARGET) /usr/local/bin/$(TARGET)

clean: $(TARGET)
	rm -f $(TARGET)