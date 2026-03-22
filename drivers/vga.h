#pragma once
#include <stdint.h>
#include <stddef.h>
#define VGA_COLS 80
#define VGA_ROWS 25
#define VGA_MEM  ((volatile uint16_t*)0xB8000)
typedef enum { COLOR_BLACK=0,COLOR_BLUE=1,COLOR_GREEN=2,COLOR_CYAN=3,COLOR_RED=4,
COLOR_MAGENTA=5,COLOR_BROWN=6,COLOR_LIGHT_GREY=7,COLOR_DARK_GREY=8,COLOR_LIGHT_BLUE=9,
COLOR_LIGHT_GREEN=10,COLOR_LIGHT_CYAN=11,COLOR_LIGHT_RED=12,COLOR_LIGHT_MAGENTA=13,
COLOR_YELLOW=14,COLOR_WHITE=15 } vga_color_t;
#define VGA_ATTR(fg,bg) ((uint8_t)(((bg)<<4)|((fg)&0x0F)))
#define VGA_ENTRY(c,a)  ((uint16_t)(((uint16_t)(a)<<8)|(uint8_t)(c)))
void vga_init(void);
void vga_clear(uint8_t attr);
void vga_set_attr(uint8_t a);
void vga_set_color(vga_color_t fg, vga_color_t bg);
void vga_putchar(char c);
void vga_puts(const char *s);
void vga_putchar_at(int col,int row,char c,uint8_t a);
void vga_puts_at(int col,int row,const char*s,uint8_t a);
void vga_fill_row(int row,char c,uint8_t a);
void vga_set_cursor(int col,int row);
void vga_get_cursor(int*col,int*row);
void vga_draw_hline(int row,int col,int len,char c,uint8_t a);
void vga_draw_box(int x,int y,int w,int h,uint8_t a);
void vga_printf(const char *fmt,...);
void vga_probe(void);
