
#ifndef HTTP_H
#define HTTP_H

#include "protocol.h"

void http_send_gpio(protocol_t *proto, int gpio, int state, const char *message_id);

#endif