#pragma once
#include <stdint.h>
void serial_init(void);
void serial_putchar(char c);
void serial_puts(const char *s);
void serial_printf(const char *fmt, ...);
