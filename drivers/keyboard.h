#pragma once
#include <stdint.h>
#define KEY_UP 0x100
#define KEY_DOWN 0x101
#define KEY_LEFT 0x102
#define KEY_RIGHT 0x103
void keyboard_init(void);
int keyboard_getchar(void);
int keyboard_poll(void);
void keyboard_flush(void);
#define KEY_F1   0x10A
#define KEY_F2   0x10B
#define KEY_F3   0x10C
#define KEY_F4   0x10D
#define KEY_F5   0x10E
