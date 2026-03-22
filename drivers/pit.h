#pragma once
#include <stdint.h>
void pit_init(uint32_t hz);
uint32_t pit_ticks(void);
uint32_t pit_seconds(void);
void pit_sleep_ms(uint32_t ms);
