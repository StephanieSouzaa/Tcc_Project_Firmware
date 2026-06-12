#ifndef PROTOCOL_H
#define PROTOCOL_H

typedef struct {
    char  device_id[32];
} protocol_t;

void protocol_init(protocol_t *proto);
void protocol_start(protocol_t *proto);
void protocol_send_gpio(protocol_t *proto, int gpio, int state, const char *message_id);
void protocol_handle_gpio_command(protocol_t *proto, int gpio, int state, const char *message_id);

#endif