#ifndef SDCARD_H
#define SDCARD_H

#include <stdbool.h>

bool sdcard_init(void);
void sdcard_log(const char *fmt, ...);

#endif // SDCARD_H
