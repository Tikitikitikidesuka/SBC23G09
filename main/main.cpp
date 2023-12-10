#include <Espressif_MQTT_Client.h>
#include <ThingsBoard.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_simple_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include "config.h"

constexpr char TAG[] = "MAIN";

constexpr uint16_t MAX_MESSAGE_SIZE = 1024;

Espressif_MQTT_Client mqtt_client;
ThingsBoard tb(mqtt_client, MAX_MESSAGE_SIZE);

bool tb_connect();

extern "C" void app_main() {
    nvs_flash_init();

    sta_start();
    sta_connect(CONFIG::WIFI_SSID, CONFIG::WIFI_PASSWORD, 5);

    tb_connect();

    while (true) {
        ESP_LOGI(TAG, "Iteration");

        if (!tb.connected()) {
            ESP_LOGI(TAG, "Failed to connect to ThingsBoard server");
            if (tb_connect())
                ESP_LOGI(TAG, "Connection to ThingsBoard server succeeded");
        } else {
            ESP_LOGI(TAG, "Sending water level...");
            tb.sendTelemetryData("water_level", esp_random() % 101);
            tb.sendTelemetryData("temperature", esp_random() % 20 + 10);
        }

        if (!sta_connected())
            sta_connect(CONFIG::WIFI_SSID, CONFIG::WIFI_PASSWORD, 5);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

bool tb_connect() {
    ESP_LOGI(TAG, "Connecting to: %s", CONFIG::THINGSBOARD_SERVER);
    return tb.connect(CONFIG::THINGSBOARD_SERVER, CONFIG::THINGSBOARD_DEVICE_TOKEN,
                      CONFIG::THINGSBOARD_PORT);
}

/*
void wifi_connect() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(CONFIG::WIFI_SSID, CONFIG::WIFI_PASSWORD);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println(".");
        delay(1000);
    }
    Serial.println("Connected to AP");
}

void wifi_reconnect() {
    if (WiFi.status() == WL_CONNECTED)
        return;

    wifi_connect();
}


*/