#include "esp_ez_provisioning.h"

#include <string.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_simple_wifi.h"
#include "web_page.h"

#ifndef MIN_FUNC
#define MIN_FUNC
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define WEB_PAGE_BIT BIT0

EventGroupHandle_t web_page_event_group;
char *ssid_ptr, *password_ptr;

esp_err_t get_handler(httpd_req_t* req);
esp_err_t get_wifi_portal_css_handler(httpd_req_t* req);
esp_err_t post_connect_handler(httpd_req_t* req);
httpd_handle_t start_webserver();
void stop_webserver(httpd_handle_t server);

httpd_uri_t uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_handler,
    .user_ctx = NULL,
};

httpd_uri_t uri_wifi_portal_css_get = {
    .uri = "/wifi_portal.css",
    .method = HTTP_GET,
    .handler = get_wifi_portal_css_handler,
    .user_ctx = NULL,
};

httpd_uri_t uri_connect_post = {
    .uri = "/connect",
    .method = HTTP_POST,
    .handler = post_connect_handler,
    .user_ctx = NULL,
};

wifi_provisioning_status_t wifi_provisioning(const char* server_ssid, const char* server_password,
                                             char new_ssid[MAX_WIFI_SSID_LENGTH + 1],
                                             char new_password[MAX_WIFI_PASSWORD_LENGTH + 1]) {
    if (ap_start(server_ssid, server_password, 2, 10, false) != WIFI_AP_START_SUCCESS)
        return WIFI_PROVISIONING_INVALID_CREDENTIALS;

    ssid_ptr = new_ssid;
    password_ptr = new_password;

    httpd_handle_t server = start_webserver();

    web_page_event_group = xEventGroupCreate();
    xEventGroupWaitBits(web_page_event_group, WEB_PAGE_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

    stop_webserver(server);
    ap_stop();

    return WIFI_PROVISIONING_SUCCESS;
}

esp_err_t get_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, wifi_portal_html_src, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t get_wifi_portal_css_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, wifi_portal_css_src, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t post_connect_handler(httpd_req_t* req) {
    char content[128];

    size_t recv_size = MIN(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request Timeout");

        return ESP_FAIL;
    }

    cJSON* json = cJSON_Parse(content);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request Error");
        cJSON_Delete(json);

        return ESP_FAIL;
    }

    cJSON* json_ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    cJSON* json_password = cJSON_GetObjectItemCaseSensitive(json, "password");
    if (!cJSON_IsString(json_ssid) || json_ssid->valuestring == NULL ||
        !cJSON_IsString(json_password) || json_password->valuestring == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request Error");
        cJSON_Delete(json);

        return ESP_FAIL;
    }

    const char* ssid = json_ssid->valuestring;
    const char* password = json_password->valuestring;

    memcpy(ssid_ptr, ssid, strnlen(ssid, MAX_WIFI_SSID_LENGTH) + 1);
    memcpy(password_ptr, password, strnlen(password, MAX_WIFI_PASSWORD_LENGTH) + 1);

    cJSON_Delete(json);

    const char* success_resp = "WiFi Configuration Complete!";
    const char* ssid_short_resp = "SSID too short!";
    const char* password_short_resp = "Password too short!";
    const char* ssid_long_resp = "SSID too long!";
    const char* password_long_resp = "Password too long!";

    wifi_credential_status_t cred_status = wifi_credential_validation(ssid, password);
    if (cred_status == WIFI_CRED_SSID_SHORT)
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, ssid_short_resp);
    else if (cred_status == WIFI_CRED_SSID_LONG)
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, ssid_long_resp);
    else if (cred_status == WIFI_CRED_PASSWORD_SHORT)
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, password_short_resp);
    else if (cred_status == WIFI_CRED_PASSWORD_LONG)
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, password_long_resp);
    else {
        httpd_resp_send(req, success_resp, strlen(success_resp));
        xEventGroupSetBits(web_page_event_group, WEB_PAGE_BIT);
    }

    return ESP_OK;
}

httpd_handle_t start_webserver() {
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Empty handle to esp_http_server */
    httpd_handle_t server = NULL;

    /* Start the httpd server */
    if (httpd_start(&server, &config) == ESP_OK) {
        /* Register URI handlers */
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_wifi_portal_css_get);
        httpd_register_uri_handler(server, &uri_connect_post);
        //httpd_register_uri_handler(server, &uri_post);
    }

    /* If server failed to start, handle will be NULL */
    return server;
}

void stop_webserver(httpd_handle_t server) {
    if (server) {
        /* Stop the httpd server */
        httpd_stop(server);
    }
}