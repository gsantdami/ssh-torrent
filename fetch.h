#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

typedef struct {
    char *Data;
    size_t Size;
} Response;

size_t callback (void *conteudo, size_t size, size_t n_members, Response *resp) {
    size_t total = size * n_members;

    resp->Data = (char *)realloc(resp->Data, resp->Size + total + 1);
    if(!resp->Data) return 0;

    memcpy(&resp->Data[resp->Size], conteudo, total);
    resp->Size += total;
    resp->Data[resp->Size] = '\0';

    return total;
    
}

char *fetch(const char *url) {
    CURL *curl = curl_easy_init();
    if(!curl) return NULL;

    Response resp = {.Data = (char *)malloc(1), .Size = 0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if(res != CURLE_OK) {
        free(resp.Data);
        return NULL;
    }

    return resp.Data;

}

char *build_magnet(const char *info_hash, const char *name) {
    const char *base = "magnet:?xt=urn:btih:";
    const char *tracker = "&tr=udp://tracker.opentrackr.org:1337/announce";

    size_t total = strlen(base) + strlen(info_hash) +
                   strlen("&dn=") + strlen(name) +
                   strlen(tracker) + 1;

    char *magnet = (char*)malloc(total);
    if (!magnet) return NULL;

    strcpy(magnet, base);
    strcat(magnet, info_hash);
    strcat(magnet, "&dn=");
    strcat(magnet, name);
    strcat(magnet, tracker);

    return magnet;
}

void parseResultados (char* json_str) {
    cJSON *json = cJSON_Parse(json_str);
    if(!json) {
        fprintf(stderr, "Erro ao realizar o parsing.");
        return;
    }

    cJSON *Item;


    cJSON_ArrayForEach (Item, json) {

    cJSON *info_hash = cJSON_GetObjectItem(Item, "info_hash");
    cJSON *name = cJSON_GetObjectItem(Item, "name");
    cJSON *seeders = cJSON_GetObjectItem(Item, "seeders");
    cJSON *size = cJSON_GetObjectItem(Item, "size");

    char *magnet = build_magnet(info_hash->valuestring, name->valuestring);

    printf("[SEEDERS: %s]     ", seeders->valuestring);
    printf("%s     ", name->valuestring);


  long long bytes = atoll(size->valuestring);

    if (bytes >= 1073741824)
        printf("SIZE: %.2f GB\n\n", (double)bytes / 1073741824);
    else if (bytes >= 1048576)
        printf("SIZE: %.2f MB\n\n", (double)bytes / 1048576);
    else if (bytes >= 1024)
        printf("SIZE: %.2f KB\n\n", (double)bytes / 1024);
    else
    printf("SIZE: %lld bytes\n\n", bytes);


    printf("MAGNET LINK: %s\n\n", magnet);

    free(magnet);
}
    cJSON_Delete(json);

}