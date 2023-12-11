#ifndef PUMP_DRIVER_H
#define PUMP_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void PUMPS_init();
void PUMPS_set_pump_intensity(uint8_t pump, uint8_t intensity);
uint8_t PUMPS_get_pump_intensity(uint8_t pump);

#ifdef __cplusplus
}
#endif

#endif  // PUMP_DRIVER_H