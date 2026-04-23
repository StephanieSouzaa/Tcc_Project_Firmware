#ifndef INTERFACE_H
#define INTERFACE_H

#include "protocol.h"

void app_interface_start_wifi(void);
void app_interface_init_gpio(protocol_t *proto);
void app_interface_start_mqtt(protocol_t *proto);

#endif