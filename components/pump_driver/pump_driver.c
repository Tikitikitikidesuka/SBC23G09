#include "pump_driver.h"

#include "driver/ledc.h"

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_SPEED_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_8_BIT  // Set duty resolution to 8 bits
#define PWM_FREQUENCY 5000              // Frequency in Hertz. Set frequency at 5 kHz

const uint8_t PUMP_GPIO_PINS[] = {5, 17, 16};
const uint8_t PUMP_NUM = sizeof(PUMP_GPIO_PINS) / sizeof(PUMP_GPIO_PINS[0]);

static uint8_t pump_intensities[sizeof(PUMP_GPIO_PINS) / sizeof(PUMP_GPIO_PINS[0])] = {0};

ledc_channel_config_t pwm_channel_configs[sizeof(PUMP_GPIO_PINS) / sizeof(PUMP_GPIO_PINS[0])];

void PUMPS_init() {
    ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_SPEED_MODE,
                                      .duty_resolution = LEDC_DUTY_RES,
                                      .timer_num = LEDC_TIMER,
                                      .freq_hz = PWM_FREQUENCY,
                                      .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&ledc_timer);

    for (uint8_t pump_idx = 0; pump_idx < PUMP_NUM; ++pump_idx) {
        ledc_channel_config_t* channel_config = &pwm_channel_configs[pump_idx];

        channel_config->gpio_num = PUMP_GPIO_PINS[pump_idx];
        channel_config->speed_mode = LEDC_SPEED_MODE;
        channel_config->timer_sel = LEDC_TIMER;
        channel_config->channel = pump_idx;
        channel_config->hpoint = 0;
        channel_config->duty = 0;

        ledc_channel_config(channel_config);
    };
}

void PUMPS_set_pump_intensity(uint8_t pump, uint8_t intensity) {
    if (pump > PUMP_NUM - 1)
        return;

    pump_intensities[pump] = intensity;
    ledc_set_duty(LEDC_SPEED_MODE, pwm_channel_configs[pump].channel, intensity);
    ledc_update_duty(LEDC_SPEED_MODE, pwm_channel_configs[pump].channel);
}

uint8_t PUMPS_get_pump_intensity(uint8_t pump) {
    if (pump > PUMP_NUM - 1)
        return 0;

    return pump_intensities[pump];
}
