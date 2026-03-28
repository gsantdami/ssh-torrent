LIBS = -lcurl -lcjson -lncurses
TARGET = torrent-search
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	gcc $(SRC) $(LIBS) -o $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/$(TARGET)

uninstall: $(TARGET)
	rm -f $(TARGET) /usr/local/bin/$(TARGET)

clean: $(TARGET)
	rm -f $(TARGET)

.PHONY: all, install, uninstall, clean