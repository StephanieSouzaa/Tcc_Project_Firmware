#ifndef DRV_GPIO_H
#define DRV_GPIO_H

#include "protocol.h"

void drv_gpio_init(protocol_t *proto);
void drv_gpio_set(int gpio, int level);

#endif