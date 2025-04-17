#define _XOPEN_SOURCE 700
#define _GNU_SOURCE

#include "timefixer.h"
#include <dlog.h>
#include <curl/curl.h>


#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#include <time.h>
#include <sys/time.h>
#define TAG "TIMEFIXER"
#define CONFIG_PATH "/opt/usr/apps/org.tizen.timefixer/shared/config/config.json"

static size_t write_callback(void* ptr, size_t size, size_t nmemb, char* resp) {
    strncat(resp, (char*)ptr, size * nmemb);
    return size * nmemb;
}

static char* get_json_value(const char* json, const char* key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    char* start = strstr(json, pattern);
    if(!start) return NULL;
    start += strlen(pattern);
    char* end = strchr(start, '\"');
    if(!end) return NULL;

    size_t len = end - start;
    char* value = malloc(len + 1);
    strncpy(value, start, len);
    value[len] = '\0';
    return value;
}

static char* get_timezone_from_ip() {
    CURL* curl = curl_easy_init();
    char response[512] = {0};
    char* timezone = NULL;

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://ipinfo.io/json");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        if(curl_easy_perform(curl) == CURLE_OK) {
            timezone = get_json_value(response, "timezone");
        }
        curl_easy_cleanup(curl);
    }
    return timezone;
}

static char* get_default_timezone() {
    FILE* file = fopen(CONFIG_PATH, "r");
    if(!file) return NULL;

    char buffer[512] = {0};
    fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    return get_json_value(buffer, "default_timezone");
}

static bool sync_time(Evas_Object* label) {
    char* timezone = get_timezone_from_ip();
    if(!timezone) timezone = get_default_timezone();
    if(!timezone) timezone = strdup("Etc/UTC");

    CURL* curl = curl_easy_init();
    char response[1024] = {0};
    bool success = false;
    char url[256];
    
    snprintf(url, sizeof(url), "http://worldtimeapi.org/api/timezone/%s", timezone);

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

        if(curl_easy_perform(curl) == CURLE_OK) {
            char* datetime = strstr(response, "\"datetime\":\"");
            if(datetime) {
                datetime += strlen("\"datetime\":\"");
                char* end = strchr(datetime, '\"');
                if(end) *end = '\0';

                struct tm tm_time = {0};
                if (strptime(datetime, "%Y-%m-%dT%H:%M:%S", &tm_time)) {
                    time_t raw_time = mktime(&tm_time);
                    struct timeval tv = { raw_time, 0};

                    if (settimeofday(&tv, NULL) == 0) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "Zona: %s", timezone);
                        elm_object_text_set(label, msg);
                        dlog_print(DLOG_INFO, TAG, "Hora ajustada correctamente");
                        success = true;
                    } else {
                    	dlog_print(DLOG_ERROR, TAG, "Error al aplicar la hora");
                    }
                } else {
                	dlog_print(DLOG_ERROR, TAG, "No se encontro 'datetime' en json");
                }
            } else {
            	dlog_print(DLOG_ERROR, TAG, "curl_easy_cleanup fallo");
            }
        }
        curl_easy_cleanup(curl);
    }

    free(timezone);
    return success;
}

static void create_base_gui(appdata_s *ad) {
    ad->win = elm_win_util_standard_add("TimeFixer", "Time Fixer");
    elm_win_autodel_set(ad->win, EINA_TRUE);

    ad->conform = elm_conformant_add(ad->win);
    evas_object_size_hint_weight_set(ad->conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(ad->win, ad->conform);
    evas_object_show(ad->conform);

    ad->label = elm_label_add(ad->conform);
    elm_object_text_set(ad->label, "Sincronizando...");
    evas_object_size_hint_weight_set(ad->label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(ad->label, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_object_content_set(ad->conform, ad->label);
    evas_object_show(ad->label);

    evas_object_show(ad->win);
    sync_time(ad->label);
}

static bool app_create(void *data) {
    appdata_s *ad = data;
    create_base_gui(ad);
    return true;
}

int main(int argc, char *argv[]) {
    appdata_s ad = {0,};
    ui_app_lifecycle_callback_s event_callback = {0,};
    event_callback.create = app_create;
    return ui_app_main(argc, argv, &event_callback, &ad);
}
