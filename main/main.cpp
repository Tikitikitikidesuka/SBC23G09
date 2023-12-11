#include <Espressif_MQTT_Client.h>
#include <ThingsBoard.h>
#include <driver/adc.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_simple_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <pump_driver.h>
#include <ultrasonic.h>

#include "config.h"

constexpr char TAG[] = "MAIN";

constexpr const char RPC_SET_AUTO_MODE_CALLBACK_METHOD[] = "set_auto_mode";
constexpr const char RPC_SET_PUMP0_INTENSITY_CALLBACK_METHOD[] = "set_pump0_intensity";
constexpr const char RPC_SET_PUMP1_INTENSITY_CALLBACK_METHOD[] = "set_pump1_intensity";
constexpr const char RPC_SET_PUMP2_INTENSITY_CALLBACK_METHOD[] = "set_pump2_intensity";
constexpr const char RPC_GET_PUMP0_INTENSITY_CALLBACK_METHOD[] = "get_pump0_intensity";
constexpr const char RPC_GET_PUMP1_INTENSITY_CALLBACK_METHOD[] = "get_pump1_intensity";
constexpr const char RPC_GET_PUMP2_INTENSITY_CALLBACK_METHOD[] = "get_pump2_intensity";

constexpr uint16_t MAX_MESSAGE_SIZE = 1024;

bool auto_mode = false;

Espressif_MQTT_Client mqtt_client;
ThingsBoard tb(mqtt_client, MAX_MESSAGE_SIZE);

const ultrasonic_sensor_t sensor = {
    .trigger_pin = GPIO_NUM_19,
    .echo_pin = GPIO_NUM_18,
};

const adc1_channel_t LIGHT_ADC_CHANNEL = ADC1_CHANNEL_7;

bool tb_connect();

float read_water_depth();
uint16_t read_light_level();

RPC_Response set_auto_mode_callback(const RPC_Data& data);
RPC_Response set_pump0_intensity_callback(const RPC_Data& data);
RPC_Response set_pump1_intensity_callback(const RPC_Data& data);
RPC_Response set_pump2_intensity_callback(const RPC_Data& data);
RPC_Response get_pump0_intensity_callback(const RPC_Data& data);
RPC_Response get_pump1_intensity_callback(const RPC_Data& data);
RPC_Response get_pump2_intensity_callback(const RPC_Data& data);

const std::array<RPC_Callback, 7U> RPC_CALLBACKS = {
    RPC_Callback{RPC_SET_AUTO_MODE_CALLBACK_METHOD, set_auto_mode_callback},
    RPC_Callback{RPC_SET_PUMP0_INTENSITY_CALLBACK_METHOD, set_pump0_intensity_callback},
    RPC_Callback{RPC_SET_PUMP1_INTENSITY_CALLBACK_METHOD, set_pump1_intensity_callback},
    RPC_Callback{RPC_SET_PUMP2_INTENSITY_CALLBACK_METHOD, set_pump2_intensity_callback},
    RPC_Callback{RPC_GET_PUMP0_INTENSITY_CALLBACK_METHOD, get_pump0_intensity_callback},
    RPC_Callback{RPC_GET_PUMP1_INTENSITY_CALLBACK_METHOD, get_pump1_intensity_callback},
    RPC_Callback{RPC_GET_PUMP2_INTENSITY_CALLBACK_METHOD, get_pump2_intensity_callback},
};

extern "C" void app_main() {
    nvs_flash_init();

    PUMPS_init();
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LIGHT_ADC_CHANNEL, ADC_ATTEN_DB_6);
    ultrasonic_init(&sensor);

    sta_start();

    bool subscribed = false;

    while (true) {
        ESP_LOGD(TAG, "Iteration");

        if (!sta_connected()) {
            ESP_LOGD(TAG, "Reconnecting to WiFi");
            if (sta_connect(CONFIG::WIFI_SSID, CONFIG::WIFI_PASSWORD, 5))
                ESP_LOGD(TAG, "WiFi connection succeeded");
        } else if (!tb.connected()) {
            subscribed = false;
            ESP_LOGD(TAG, "Reconnecting to ThingsBoard server");
            if (tb_connect())
                ESP_LOGD(TAG, "Connection to ThingsBoard server succeeded");
        } else if (!subscribed) {
            tb.RPC_Unsubscribe();
            ESP_LOGD(TAG, "Subscribing to RPC");
            if (tb.RPC_Subscribe(RPC_CALLBACKS.cbegin(), RPC_CALLBACKS.cend())) {
                ESP_LOGD(TAG, "RPC subscription succeeded");
                subscribed = true;
            }
        } else {
            ESP_LOGD(TAG, "Sending data...");

            tb.sendTelemetryData("water_level", read_water_depth());
            tb.sendTelemetryData("light", read_light_level());
            tb.sendTelemetryData("auto_mode", auto_mode);

            // Process messages
            tb.loop();
        }

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

bool tb_connect() {
    ESP_LOGD(TAG, "Connecting to: %s", CONFIG::THINGSBOARD_SERVER);
    return tb.connect(CONFIG::THINGSBOARD_SERVER, CONFIG::THINGSBOARD_DEVICE_TOKEN,
                      CONFIG::THINGSBOARD_PORT);
}

float read_water_depth() {
    float distance = 0;

    esp_err_t res = ultrasonic_measure(&sensor, 10, &distance);
    if (res != ESP_OK) {
        switch (res) {
            case ESP_ERR_ULTRASONIC_PING:
                ESP_LOGE(TAG, "Cannot ping (device is in invalid state)\n");
                break;
            case ESP_ERR_ULTRASONIC_PING_TIMEOUT:
                ESP_LOGE(TAG, "Ping timeout (no device found)\n");
                break;
            case ESP_ERR_ULTRASONIC_ECHO_TIMEOUT:
                ESP_LOGE(TAG, "Echo timeout (i.e. distance too big)\n");
                break;
            default:
                ESP_LOGE(TAG, "%s\n", esp_err_to_name(res));
        }
    }

    return 100 * distance;
}

uint16_t read_light_level() {
    ESP_LOGD(TAG, "Light level: %d", adc1_get_raw(LIGHT_ADC_CHANNEL));
    return adc1_get_raw(LIGHT_ADC_CHANNEL);
}

RPC_Response set_auto_mode_callback(const RPC_Data& data) {
    ESP_LOGI(TAG, "Setting auto mode to: %s", data ? "true" : "false");

    auto_mode = (bool)data;
    return RPC_Response(NULL, auto_mode);
}

RPC_Response set_pump_intensity(uint8_t pump, const RPC_Data& data) {
    ESP_LOGI(TAG, "Setting pump %d intensity to %d", pump, (int)data);

    uint8_t intensity = (int)data * 255 / 100;
    PUMPS_set_pump_intensity(pump, intensity);
    return RPC_Response(NULL, intensity);
}

RPC_Response get_pump_intensity(uint8_t pump, const RPC_Data& data) {
    ESP_LOGI(TAG, "Getting pump %d intensity", pump);

    uint8_t intensity = 100 * PUMPS_get_pump_intensity(pump) / 255;
    return RPC_Response(NULL, intensity);
}

RPC_Response set_pump0_intensity_callback(const RPC_Data& data) {
    return set_pump_intensity(0, data);
}

RPC_Response set_pump1_intensity_callback(const RPC_Data& data) {
    return set_pump_intensity(1, data);
}

RPC_Response set_pump2_intensity_callback(const RPC_Data& data) {
    return set_pump_intensity(2, data);
}

RPC_Response get_pump0_intensity_callback(const RPC_Data& data) {
    return get_pump_intensity(0, data);
}

RPC_Response get_pump1_intensity_callback(const RPC_Data& data) {
    return get_pump_intensity(1, data);
}

RPC_Response get_pump2_intensity_callback(const RPC_Data& data) {
    return get_pump_intensity(2, data);
}