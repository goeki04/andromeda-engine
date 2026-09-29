/**
 *
 * @file peripherals.h
 *
 * @brief peripherals configuration
 *
 *
 */

#ifndef PERIPHERALS_H_
#define PERIPHERALS_H_

#include <stdint.h>
#include "driver/gpio.h"

#ifdef __cplusplus  
extern "C" {
#endif /* __cplusplus */

/* Status LEDs.
 *
 * The Polverine board carried three plain LEDs on GPIO 47, 48 and 38. On a
 * DevKitC-1 those pins hold the addressable WS2812 LED, which cannot be driven
 * with gpio_set_level(). Three free GPIOs are used instead, so the sensor code
 * stays unchanged. Connecting real LEDs (with a series resistor) is optional,
 * without them the writes simply go nowhere.
 *
 * Do not move these to GPIO 0, 3, 45, 46 (strapping), 19, 20 (USB),
 * 26..32 (flash), 33..37 (octal PSRAM), 43, 44 (console UART)
 * or 10..13, which the BMV080 uses for SPI.
 */
#define R_LED_PIN 4  // red   status LED, lights up on a sensor error
#define G_LED_PIN 5  // green status LED, blinks on each successful init step
#define B_LED_PIN 6  // blue  status LED, currently unused


#ifdef __cplusplus  
}
#endif /* __cplusplus */

#endif /* PERIPHERALS_H_ */
