#pragma once
#include <stdint.h>
typedef struct __attribute__((packed)){uint16_t base_low;uint16_t sel;uint8_t always0;uint8_t flags;uint16_t base_high;}idt_entry_t;
typedef struct __attribute__((packed)){uint16_t limit;uint32_t base;}idt_ptr_t;
typedef struct __attribute__((packed)){
    uint32_t gs,fs,es,ds;
    uint32_t edi,esi,ebp,esp_dummy,ebx,edx,ecx,eax;
    uint32_t int_no,err_code;
    uint32_t eip,cs,eflags,useresp,ss;
}regs_t;
typedef void(*irq_handler_t)(regs_t*);
void idt_init(void);
void irq_install_handler(int irq,irq_handler_t h);
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20
