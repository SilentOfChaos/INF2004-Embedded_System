/*
 *  app_gpio.h
 *  Plain GPIO set-up and access for IS19 drivers (SIO function).
 *
 *  Each pin is configured only by the task that owns it (TEAM.md
 *  section 2).  Reading and writing go through the SIO set/clear
 *  registers, which are safe from either core.
 */

#ifndef APP_GPIO_H
#define APP_GPIO_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    APP_GPIO_PULL_NONE = 0,
    APP_GPIO_PULL_UP,
    APP_GPIO_PULL_DOWN
} app_gpio_pull_t;

void app_gpio_input(uint32_t pin, app_gpio_pull_t pull);
void app_gpio_output(uint32_t pin, bool b_high);
bool app_gpio_read(uint32_t pin);
void app_gpio_write(uint32_t pin, bool b_high);

#endif /* APP_GPIO_H */

/*** end of file ***/
