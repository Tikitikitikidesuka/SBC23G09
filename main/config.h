#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

namespace CONFIG {

constexpr char* WIFI_SSID = <Your WiFi SSID>;
constexpr char* WIFI_PASSWORD = <Your WiFi password>;

constexpr char* THINGSBOARD_DEVICE_TOKEN = <Your ThingsBoard device token>;
constexpr char* THINGSBOARD_SERVER = <Your ThingsBoard server url>;
constexpr uint16_t THINGSBOARD_PORT = <Your ThingsBoard server port>;

};  // namespace CONFIG

#endif  // CONFIG_H