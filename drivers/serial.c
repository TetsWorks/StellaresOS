#include "serial.h"
#include "../libc/string.h"
#include <stdarg.h>
#define COM1 0x3F8
static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
void serial_init(void){outb(COM1+1,0);outb(COM1+3,0x80);outb(COM1+0,3);outb(COM1+1,0);outb(COM1+3,3);outb(COM1+2,0xC7);outb(COM1+4,0x0B);}
static void serial_wait(void){while(!(inb(COM1+5)&0x20));}
void serial_putchar(char c){if(c=='\n'){serial_wait();outb(COM1,'\r');}serial_wait();outb(COM1,(uint8_t)c);}
void serial_puts(const char*s){while(*s)serial_putchar(*s++);}
void serial_printf(const char*fmt,...){va_list ap;va_start(ap,fmt);char b[32];while(*fmt){if(*fmt=='%'){fmt++;switch(*fmt){case'd':case'i':itoa(va_arg(ap,int),b,10);serial_puts(b);break;case'u':itoa((int)va_arg(ap,unsigned),b,10);serial_puts(b);break;case'x':int_to_hex(va_arg(ap,unsigned),b);serial_puts(b);break;case's':{const char*s=va_arg(ap,const char*);serial_puts(s?s:"(null)");break;}case'c':serial_putchar((char)va_arg(ap,int));break;case'%':serial_putchar('%');break;}}else serial_putchar(*fmt);fmt++;}va_end(ap);}
