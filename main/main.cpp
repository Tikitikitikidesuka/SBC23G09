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

constexpr const char RPC_START_CALLBACK_METHOD[] = "start";

constexpr uint16_t MAX_MESSAGE_SIZE = 1024;

Espressif_MQTT_Client mqtt_client;
ThingsBoard tb(mqtt_client, MAX_MESSAGE_SIZE);

bool tb_connect();
RPC_Response start_callback(const RPC_Data& data);

const std::array<RPC_Callback, 1U> RPC_CALLBACKS = {
    RPC_Callback{RPC_START_CALLBACK_METHOD, start_callback},
};

extern "C" void app_main() {
    nvs_flash_init();

    sta_start();

    bool subscribed = false;

    while (true) {
        ESP_LOGI(TAG, "Iteration");

        if (!sta_connected()) {
            ESP_LOGI(TAG, "Reconnecting to WiFi");
            if (sta_connect(CONFIG::WIFI_SSID, CONFIG::WIFI_PASSWORD, 5))
                ESP_LOGI(TAG, "WiFi connection succeeded");
        } else if (!tb.connected()) {
            subscribed = false;
            ESP_LOGI(TAG, "Reconnecting to ThingsBoard server");
            if (tb_connect())
                ESP_LOGI(TAG, "Connection to ThingsBoard server succeeded");
        } else if (!subscribed) {
            ESP_LOGI(TAG, "Subscribing to RPC");
            if (tb.RPC_Subscribe(RPC_CALLBACKS.cbegin(), RPC_CALLBACKS.cend())) {
                ESP_LOGI(TAG, "RPC subscription succeeded");
                subscribed = true;
            }
        } else {
            ESP_LOGI(TAG, "Sending water level...");
            tb.sendTelemetryData("water_level", esp_random() % 101);
            tb.sendTelemetryData("temperature", esp_random() % 20 + 10);

            // Process messages
            tb.loop();
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

bool tb_connect() {
    ESP_LOGI(TAG, "Connecting to: %s", CONFIG::THINGSBOARD_SERVER);
    return tb.connect(CONFIG::THINGSBOARD_SERVER, CONFIG::THINGSBOARD_DEVICE_TOKEN,
                      CONFIG::THINGSBOARD_PORT);
}

RPC_Response start_callback(const RPC_Data& data) {
    ESP_LOGI(TAG, "Start!!!");
    return RPC_Response("XD", 33);
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