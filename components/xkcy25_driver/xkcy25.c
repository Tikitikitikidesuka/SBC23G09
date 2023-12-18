#include "xkcy25.h"

static gpio_num_t gpio_pin;

void XKCY25_init(gpio_num_t new_gpio_pin) {
    gpio_pin = new_gpio_pin;
    gpio_set_direction(gpio_pin, GPIO_MODE_INPUT);
}

bool XKCY25_get_water_level() {
    return gpio_get_level(gpio_pin);
}