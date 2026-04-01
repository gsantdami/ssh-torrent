#ifndef FETCH_H
#define FETCH_H

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

typedef struct TorrentResult {
    char *name;
    int seeders;
    long long size_bytes;
    char *magnet;
} TorrentResult;

typedef struct {
    char *Data;
    size_t Size;
} Response;

static size_t callback (void *conteudo, size_t size, size_t n_members, Response *resp) {
    size_t total = size * n_members;

    resp->Data = (char *)realloc(resp->Data, resp->Size + total + 1);
    if(!resp->Data) return 0;

    memcpy(&resp->Data[resp->Size], conteudo, total);
    resp->Size += total;
    resp->Data[resp->Size] = '\0';

    return total;
    
}

static char *fetch(const char *url) {
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

static char *build_magnet(const char *info_hash, const char *name) {
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

static int torrent_cmp_seeders_desc (const void *a, const void *b) {
    const TorrentResult *x = (const TorrentResult *)a;
    const TorrentResult *y = (const TorrentResult *)b;
    if (y->seeders > x->seeders) return 1;
    if (y->seeders < x->seeders) return -1;
    return 0;
}

static void torrent_format_size (long long bytes, char *buf, size_t buflen) {
    if (bytes >= 1073741824LL)
        snprintf(buf, buflen, "%.2f GB", (double)bytes / 1073741824.0);
    else if (bytes >= 1048576LL)
        snprintf(buf, buflen, "%.2f MB", (double)bytes / 1048576.0);
    else if (bytes >= 1024LL)
        snprintf(buf, buflen, "%.2f KB", (double)bytes / 1024.0);
    else
        snprintf(buf, buflen, "%lld bytes", (long long)bytes);
}

static void torrent_results_free (TorrentResult *arr, size_t n) {
    if (!arr)
        return;
    for (size_t i = 0; i < n; i++) {
        free(arr[i].name);
        free(arr[i].magnet);
    }
    free(arr);
}

/* 0 ok, -1 erro (mensagem em stderr). *out pode ser NULL se não houver itens. */
static int torrent_results_from_json (const char *json_str, TorrentResult **out, size_t *out_n) {
    *out = NULL;
    *out_n = 0;

    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        fprintf(stderr, "Erro ao realizar o parsing.\n");
        return -1;
    }

    if (!cJSON_IsArray(json)) {
        fprintf(stderr, "Resposta JSON inesperada.\n");
        cJSON_Delete(json);
        return -1;
    }

    int nitems = cJSON_GetArraySize(json);
    if (nitems <= 0) {
        cJSON_Delete(json);
        return 0;
    }

    TorrentResult *arr = (TorrentResult *)calloc((size_t)nitems, sizeof(TorrentResult));
    if (!arr) {
        cJSON_Delete(json);
        return -1;
    }

    int written = 0;
    cJSON *item;

    cJSON_ArrayForEach (item, json) {
        cJSON *info_hash = cJSON_GetObjectItem(item, "info_hash");
        cJSON *name = cJSON_GetObjectItem(item, "name");
        cJSON *seeders = cJSON_GetObjectItem(item, "seeders");
        cJSON *size = cJSON_GetObjectItem(item, "size");

        if (!cJSON_IsString(info_hash) || !cJSON_IsString(name) ||
            !cJSON_IsString(seeders) || !cJSON_IsString(size)) {
            continue;
        }

        char *magnet = build_magnet(info_hash->valuestring, name->valuestring);
        if (!magnet) {
            torrent_results_free(arr, (size_t)written);
            cJSON_Delete(json);
            return -1;
        }

        char *name_copy = strdup(name->valuestring);
        if (!name_copy) {
            free(magnet);
            torrent_results_free(arr, (size_t)written);
            cJSON_Delete(json);
            return -1;
        }

        TorrentResult *r = &arr[written];
        r->name = name_copy;
        r->seeders = atoi(seeders->valuestring);
        r->size_bytes = atoll(size->valuestring);
        r->magnet = magnet;
        written++;
    }

    cJSON_Delete(json);

    if (written == 0) {
        free(arr);
        return 0;
    }

    qsort(arr, (size_t)written, sizeof(TorrentResult), torrent_cmp_seeders_desc);
    *out = arr;
    *out_n = (size_t)written;
    return 0;
}

#endif