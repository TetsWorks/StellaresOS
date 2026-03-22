#include "pit.h"
#include "../kernel/idt.h"
#define PIT_CH0 0x40
#define PIT_CMD 0x43
static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static volatile uint32_t tick=0;
static uint32_t hz=0;
static void handler(regs_t*r){(void)r;tick++;}
void pit_init(uint32_t f){hz=f;uint32_t d=1193180/f;outb(PIT_CMD,0x36);outb(PIT_CH0,(uint8_t)(d&0xFF));outb(PIT_CH0,(uint8_t)((d>>8)&0xFF));irq_install_handler(0,handler);}
uint32_t pit_ticks(void){return tick;}
uint32_t pit_seconds(void){return tick/hz;}
void pit_sleep_ms(uint32_t ms){uint32_t end=tick+ms*(hz/1000);while(tick<end)__asm__ volatile("hlt");}
