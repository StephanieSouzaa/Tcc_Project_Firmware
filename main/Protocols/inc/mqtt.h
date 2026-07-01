#ifndef MQTT_H
#define MQTT_H

#include "protocol.h"

void app_interface_start_mqtt(protocol_t *proto);
void mqtt_stop(void);

#endif