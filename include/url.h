#ifndef URL_H
#define URL_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *build_url (const char *base, int argc, char *argv[]) {

size_t total_size = strlen(base) + 1;

for(int i = 0; i < argc; i++ ) {
    total_size += strlen(argv[i]) + 1;
}

char *url = (char*)malloc(total_size);
if(!url) return NULL;

strcpy(url, base);

for (int i = 1; i < argc; i++) {
    strcat(url, argv[i]);
    if(i< argc - 1) {
        strcat(url, "+");
    }
}
return url;

}

#endif
