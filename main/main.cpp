#include <Espressif_MQTT_Client.h>
#include <ThingsBoard.h>
#include <cJSON.h>
#include <driver/adc.h>
#include <driver/rtc_io.h>
#include <esp_err.h>
#include <esp_ez_provisioning.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <esp_simple_wifi.h>
#include <esp_sleep.h>
#include <esp_spiffs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <pump_driver.h>
#include <xkcy25.h>

#include "config.h"
#include "h1.h"

constexpr char TAG[] = "MAIN";

const char CONFIG_FILE_PATH[] = "/spiffs/config.json";

constexpr const char RPC_SET_AUTO_MODE_CALLBACK_METHOD[] = "set_auto_mode";
constexpr const char RPC_SET_PUMP0_INTENSITY_CALLBACK_METHOD[] = "set_pump0_intensity";
constexpr const char RPC_SET_PUMP1_INTENSITY_CALLBACK_METHOD[] = "set_pump1_intensity";
constexpr const char RPC_SET_PUMP2_INTENSITY_CALLBACK_METHOD[] = "set_pump2_intensity";
constexpr const char RPC_GET_PUMP0_INTENSITY_CALLBACK_METHOD[] = "get_pump0_intensity";
constexpr const char RPC_GET_PUMP1_INTENSITY_CALLBACK_METHOD[] = "get_pump1_intensity";
constexpr const char RPC_GET_PUMP2_INTENSITY_CALLBACK_METHOD[] = "get_pump2_intensity";

constexpr uint16_t MAX_MESSAGE_SIZE = 1024;

bool auto_mode = false;
uint16_t light_level = 0;

Espressif_MQTT_Client mqtt_client;
ThingsBoard tb(mqtt_client, MAX_MESSAGE_SIZE);

const adc1_channel_t LIGHT_ADC_CHANNEL = ADC1_CHANNEL_7;
const gpio_num_t WATER_LEVEL_SENSOR_PIN = GPIO_NUM_4;
const gpio_num_t BUTTON_PIN = GPIO_NUM_2;

bool tb_connect();

void playSong(void* parameters);

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

extern "C" {
bool validate_config(char ssid[MAX_WIFI_SSID_LENGTH + 1],
                     char password[MAX_WIFI_PASSWORD_LENGTH + 1]) {
    bool connected = false;

    sta_start();
    connected = sta_connect(ssid, password, 5);
    sta_stop();

    return connected;
}

void read_config(char ssid[MAX_WIFI_SSID_LENGTH + 1], char password[MAX_WIFI_PASSWORD_LENGTH + 1]) {
    char file_content[128];

    FILE* config_file = fopen(CONFIG_FILE_PATH, "r");
    fgets(file_content, sizeof(file_content), config_file);

    cJSON* json = cJSON_Parse(file_content);
    cJSON* json_ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    cJSON* json_password = cJSON_GetObjectItemCaseSensitive(json, "password");

    memcpy(ssid, json_ssid->valuestring, strnlen(json_ssid->valuestring, MAX_WIFI_SSID_LENGTH) + 1);
    memcpy(password, json_password->valuestring,
           strnlen(json_password->valuestring, MAX_WIFI_PASSWORD_LENGTH) + 1);

    cJSON_Delete(json);
}

void create_config(char ssid[MAX_WIFI_SSID_LENGTH + 1],
                   char password[MAX_WIFI_PASSWORD_LENGTH + 1]) {
    bool connected = false;

    while (!connected) {
        uint8_t mac[6];
        esp_base_mac_addr_get(mac);

        sprintf(ssid, "esp%02X%02X", mac[4], mac[5]);

        ESP_LOGI(TAG, "starting wifi provisioning server.");
        wifi_provisioning(ssid, "password", ssid, password);

        ESP_LOGI(TAG, "testing wifi provisioning credentials.");
        connected = validate_config(ssid, password);
        if (!connected)
            ESP_LOGI(TAG, "wifi provisioning failed.");
        else
            ESP_LOGI(TAG, "wifi provisioning succeeded.");
    }

    ESP_LOGI(TAG, "storing new credentials.");
    FILE* config_file = fopen(CONFIG_FILE_PATH, "w");
    fprintf(config_file, "{\"ssid\": \"%s\", \"password\": \"%s\"}", ssid, password);
    fclose(config_file);
}
}

static void IRAM_ATTR buttonSleep(void* arg) {
    esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 1);
}

extern "C" void app_main() {
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_intr_enable(BUTTON_PIN);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, buttonSleep, (void*)BUTTON_PIN);

    /*
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) {
        esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 1);
    } else {
        gpio_isr_register()
    }
    */

    nvs_flash_init();

    esp_vfs_spiffs_conf_t spiffs_conf = {.base_path = "/spiffs",
                                         .partition_label = NULL,
                                         .max_files = 5,
                                         .format_if_mount_failed = true};

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffs_conf));

    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(spiffs_conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...",
                 esp_err_to_name(ret));
        esp_spiffs_format(spiffs_conf.partition_label);
        return;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    char ssid[32];
    char password[64];

    FILE* config_file = fopen(CONFIG_FILE_PATH, "r");
    if (config_file != NULL) {  // If config file exists
        fclose(config_file);
        ESP_LOGI(TAG, "config file found.");

        read_config(ssid, password);
        if (!validate_config(ssid, password))
            create_config(ssid, password);
    } else {  // If config file does not exist
        ESP_LOGI(TAG, "config file not found.");

        create_config(ssid, password);
    }

    PUMPS_init();
    XKCY25_init(WATER_LEVEL_SENSOR_PIN);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LIGHT_ADC_CHANNEL, ADC_ATTEN_DB_6);
    sta_start();

    xTaskCreate(&playSong, "autosong", 2048, nullptr, 10, nullptr);

    bool subscribed = false;

    while (true) {
        ESP_LOGI(TAG, "Iteration");

        if (!sta_connected()) {
            ESP_LOGD(TAG, "Reconnecting to WiFi");
            if (sta_connect(ssid, password, 5))
                ESP_LOGD(TAG, "WiFi connection succeeded");
        } else if (!tb.connected()) {
            subscribed = false;
            ESP_LOGI(TAG, "Reconnecting to ThingsBoard server");
            if (tb_connect())
                ESP_LOGI(TAG, "Connection to ThingsBoard server succeeded");
        } else if (!subscribed) {
            tb.RPC_Unsubscribe();
            ESP_LOGI(TAG, "Subscribing to RPC");
            if (tb.RPC_Subscribe(RPC_CALLBACKS.cbegin(), RPC_CALLBACKS.cend())) {
                ESP_LOGI(TAG, "RPC subscription succeeded");
                subscribed = true;
            }
        } else {
            ESP_LOGI(TAG, "Sending data...");

            tb.sendTelemetryData("water_level_ok", 100 * (int)XKCY25_get_water_level());
            ESP_LOGI(TAG, "Water: %d", XKCY25_get_water_level());
            tb.sendTelemetryData("light", read_light_level());
            ESP_LOGI(TAG, "Light: %d", read_light_level());
            tb.sendTelemetryData("auto_mode", auto_mode);

            // Process messages
            tb.loop();
        }

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

bool tb_connect() {
    ESP_LOGI(TAG, "Connecting to: %s", CONFIG::THINGSBOARD_SERVER);
    return tb.connect(CONFIG::THINGSBOARD_SERVER, CONFIG::THINGSBOARD_DEVICE_TOKEN,
                      CONFIG::THINGSBOARD_PORT);
}

uint16_t read_light_level() {
    //ESP_LOGI(TAG, "Light level: %d", adc1_get_raw(LIGHT_ADC_CHANNEL));
    light_level = adc1_get_raw(LIGHT_ADC_CHANNEL);
    return light_level;
}

void playSong(void* parameters) {
    while (true) {
        for (int i = 0; auto_mode && i < SONG_LENGTH; ++i) {
            if (light_level > 2048)
                PUMPS_set_pump_intensity(0, 0);
            else
                PUMPS_set_pump_intensity(0, hardcodedSong1[i] * 255 / 60);

            PUMPS_set_pump_intensity(0, hardcodedSong2[i] * 255 / 70);
            PUMPS_set_pump_intensity(0, hardcodedSong2[i] * 255 / 70);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

RPC_Response set_auto_mode_callback(const RPC_Data& data) {
    ESP_LOGI(TAG, "Setting auto mode to: %s", data ? "true" : "false");

    auto_mode = (bool)data;
    return RPC_Response(NULL, auto_mode);
}

RPC_Response set_pump_intensity(uint8_t pump, const RPC_Data& data) {
    ESP_LOGI(TAG, "Setting pump %d intensity to %d", pump, (int)data);

    uint8_t intensity = (int)data * 255 / 100;
    if (!auto_mode)
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