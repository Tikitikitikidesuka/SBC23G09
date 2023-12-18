#ifndef XKCY25_H
#define XKCY25_H

#ifdef __cplusplus
extern "C" {
#endif

#include <driver/gpio.h>

void XKCY25_init(gpio_num_t new_gpio_pin);
bool XKCY25_get_water_level();

#ifdef __cplusplus
}
#endif

#endif  // PUMP_DRIVER_H