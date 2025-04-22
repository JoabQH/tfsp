#include "timefixer.h"
#include <dlog.h>
#include <curl/curl.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

#define TAG "TIMEFIXER"
#define CONFIG_PATH "/opt/usr/apps/org.example.timefixer/shared/config/config.json"

static Ecore_Timer* sync_timer = NULL;

// (Callbacks write_callback, get_json_value, get_timezone_from_ip,
//  get_default_timezone y sync_time() quedan igual que en tu código.)

// Función que el timer invoca periódicamente
static Eina_Bool
_on_sync_timer(void *data) {
    bool ok = sync_time();
    dlog_print(DLOG_INFO, TAG, "sync_time() devolvió: %d", ok);
    return ECORE_CALLBACK_RENEW; // para que siga llamando cada intervalo
}

// Callback de create: inicializamos el timer
static bool
service_app_create(void *user_data) {
    // Sin GUI: arrancamos la sincronización periódica cada 300 segundos (5 min)
    sync_timer = ecore_timer_add(300.0, _on_sync_timer, NULL);
    if (!sync_timer) {
        dlog_print(DLOG_ERROR, TAG, "No pudo crear el timer de sincronización");
        return false;
    }
    // Llamada inmediata al arranque
    _on_sync_timer(NULL);
    return true;
}

// Callback de terminate: liberamos recursos
static void
service_app_terminate(void *user_data) {
    if (sync_timer) {
        ecore_timer_del(sync_timer);
        sync_timer = NULL;
    }
    dlog_print(DLOG_INFO, TAG, "TimeFixer Service terminado");
}

// No necesitamos manejar app_control en este caso
static void
service_app_control(app_control_h app_control, void *user_data) { }

// Entry point de servicio
int
main(int argc, char* argv[]) {
    service_app_lifecycle_callback_s callbacks = {0,};
    callbacks.create = service_app_create;
    callbacks.terminate = service_app_terminate;
    callbacks.app_control = service_app_control;

    // Inicia el loop de EFL para servicios
    return service_app_efl_main(argc, argv, &callbacks, NULL);
}
