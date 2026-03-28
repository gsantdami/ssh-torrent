#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *build_url (const char *base, int argc, char *argv[]) {

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