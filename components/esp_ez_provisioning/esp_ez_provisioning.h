#ifndef ESP_EZ_PROVISIONING
#define ESP_EZ_PROVISIONING

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_simple_wifi.h"

typedef enum wifi_provisioning_status_t {
    WIFI_PROVISIONING_SUCCESS,
    WIFI_PROVISIONING_INVALID_CREDENTIALS,
} wifi_provisioning_status_t;

wifi_provisioning_status_t wifi_provisioning(const char* server_ssid, const char* server_password,
                                             char new_ssid[MAX_WIFI_SSID_LENGTH + 1],
                                             char new_password[MAX_WIFI_PASSWORD_LENGTH + 1]);

#ifdef __cplusplus
}
#endif

#endif