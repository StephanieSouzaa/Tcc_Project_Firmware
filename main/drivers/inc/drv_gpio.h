#ifndef DRV_GPIO_H
#define DRV_GPIO_H

#include <stddef.h>
#include "protocol.h"

void drv_gpio_init(protocol_t *proto);
void drv_gpio_configure(const int *inputs, size_t n_inputs, const int *outputs, size_t n_outputs);
void drv_gpio_set(int gpio, int level);

#endif