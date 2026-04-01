#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void download(const char* magnet) {
    char buffer[4096];
    const char* home = getenv("HOME");
    if (!home) return;

        setenv("RES_OPTIONS", "nameserver:8.8.8.8", 1);

    snprintf(buffer, sizeof(buffer),
        "transmission-cli '%s' -w %s/Downloads --ep --no--portmap", magnet, home);


    printf("%s\n\n", buffer);
    system(buffer);
}