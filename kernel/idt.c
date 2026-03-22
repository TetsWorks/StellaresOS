#include "idt.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../libc/string.h"
#include "syscall.h"

extern void isr0(void);extern void isr1(void);extern void isr2(void);extern void isr3(void);
extern void isr4(void);extern void isr5(void);extern void isr6(void);extern void isr7(void);
extern void isr8(void);extern void isr9(void);extern void isr10(void);extern void isr11(void);
extern void isr12(void);extern void isr13(void);extern void isr14(void);extern void isr15(void);
extern void isr16(void);extern void isr17(void);extern void isr18(void);extern void isr19(void);
extern void isr20(void);extern void isr21(void);extern void isr22(void);extern void isr23(void);
extern void isr24(void);extern void isr25(void);extern void isr26(void);extern void isr27(void);
extern void isr28(void);extern void isr29(void);extern void isr30(void);extern void isr31(void);
extern void isr128(void);
extern void irq0(void);extern void irq1(void);extern void irq2(void);extern void irq3(void);
extern void irq4(void);extern void irq5(void);extern void irq6(void);extern void irq7(void);
extern void irq8(void);extern void irq9(void);extern void irq10(void);extern void irq11(void);
extern void irq12(void);extern void irq13(void);extern void irq14(void);extern void irq15(void);

static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}

__attribute__((aligned(16)))
static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;
static irq_handler_t irq_handlers[16];

static void idt_gate(uint8_t n,uint32_t base,uint16_t sel,uint8_t flags){
    idt[n].base_low=(uint16_t)(base&0xFFFF);
    idt[n].base_high=(uint16_t)(base>>16);
    idt[n].sel=sel; idt[n].always0=0; idt[n].flags=flags;
}

void idt_init(void){
    idt_ptr.limit=(uint16_t)(sizeof(idt_entry_t)*256-1);
    idt_ptr.base=(uint32_t)&idt[0];

    /* Remapeia PIC 8259 */
    outb(0x20,0x11); outb(0xA0,0x11);
    outb(0x21,0x20); outb(0xA1,0x28);
    outb(0x21,0x04); outb(0xA1,0x02);
    outb(0x21,0x01); outb(0xA1,0x01);
    outb(0x21,0xFC); /* Habilita IRQ0+IRQ1 */
    outb(0xA1,0xFF);

    idt_gate(0,(uint32_t)isr0,0x08,0x8E);   idt_gate(1,(uint32_t)isr1,0x08,0x8E);
    idt_gate(2,(uint32_t)isr2,0x08,0x8E);   idt_gate(3,(uint32_t)isr3,0x08,0x8E);
    idt_gate(4,(uint32_t)isr4,0x08,0x8E);   idt_gate(5,(uint32_t)isr5,0x08,0x8E);
    idt_gate(6,(uint32_t)isr6,0x08,0x8E);   idt_gate(7,(uint32_t)isr7,0x08,0x8E);
    idt_gate(8,(uint32_t)isr8,0x08,0x8E);   idt_gate(9,(uint32_t)isr9,0x08,0x8E);
    idt_gate(10,(uint32_t)isr10,0x08,0x8E); idt_gate(11,(uint32_t)isr11,0x08,0x8E);
    idt_gate(12,(uint32_t)isr12,0x08,0x8E); idt_gate(13,(uint32_t)isr13,0x08,0x8E);
    idt_gate(14,(uint32_t)isr14,0x08,0x8E); idt_gate(15,(uint32_t)isr15,0x08,0x8E);
    idt_gate(16,(uint32_t)isr16,0x08,0x8E); idt_gate(17,(uint32_t)isr17,0x08,0x8E);
    idt_gate(18,(uint32_t)isr18,0x08,0x8E); idt_gate(19,(uint32_t)isr19,0x08,0x8E);
    idt_gate(20,(uint32_t)isr20,0x08,0x8E); idt_gate(21,(uint32_t)isr21,0x08,0x8E);
    idt_gate(22,(uint32_t)isr22,0x08,0x8E); idt_gate(23,(uint32_t)isr23,0x08,0x8E);
    idt_gate(24,(uint32_t)isr24,0x08,0x8E); idt_gate(25,(uint32_t)isr25,0x08,0x8E);
    idt_gate(26,(uint32_t)isr26,0x08,0x8E); idt_gate(27,(uint32_t)isr27,0x08,0x8E);
    idt_gate(28,(uint32_t)isr28,0x08,0x8E); idt_gate(29,(uint32_t)isr29,0x08,0x8E);
    idt_gate(30,(uint32_t)isr30,0x08,0x8E); idt_gate(31,(uint32_t)isr31,0x08,0x8E);
    idt_gate(32,(uint32_t)irq0,0x08,0x8E);  idt_gate(33,(uint32_t)irq1,0x08,0x8E);
    idt_gate(34,(uint32_t)irq2,0x08,0x8E);  idt_gate(35,(uint32_t)irq3,0x08,0x8E);
    idt_gate(36,(uint32_t)irq4,0x08,0x8E);  idt_gate(37,(uint32_t)irq5,0x08,0x8E);
    idt_gate(38,(uint32_t)irq6,0x08,0x8E);  idt_gate(39,(uint32_t)irq7,0x08,0x8E);
    idt_gate(40,(uint32_t)irq8,0x08,0x8E);  idt_gate(41,(uint32_t)irq9,0x08,0x8E);
    idt_gate(42,(uint32_t)irq10,0x08,0x8E); idt_gate(43,(uint32_t)irq11,0x08,0x8E);
    idt_gate(44,(uint32_t)irq12,0x08,0x8E); idt_gate(45,(uint32_t)irq13,0x08,0x8E);
    idt_gate(46,(uint32_t)irq14,0x08,0x8E); idt_gate(47,(uint32_t)irq15,0x08,0x8E);
    idt_gate(128,(uint32_t)isr128,0x08,0xEE);

    __asm__ volatile("lidt %0"::"m"(idt_ptr));
}

void irq_install_handler(int irq,irq_handler_t h){
    if(irq>=0&&irq<16) irq_handlers[irq]=h;
}

static const char*exc[32]={
    "Divisao por Zero","Debug","NMI","Breakpoint","Overflow","Bound Range",
    "Opcode Invalido","FPU Ausente","Double Fault","FPU Seg Overrun","TSS Invalido",
    "Segmento Ausente","Stack Fault","Falha de Protecao","Page Fault","Reservado",
    "Erro FPU","Alinhamento","Machine Check","SIMD FP","Virtualizacao",
    "R","R","R","R","R","R","R","R","R","Seguranca","R"
};

void isr_handler(regs_t*r){
    if(r->int_no==128){
        uint32_t ret=syscall_handler(r->eax,r->ebx,r->ecx,r->edx);
        r->eax=ret; return;
    }
    const char*name=r->int_no<32?exc[r->int_no]:"Desconhecido";
    serial_puts("\n!!! KERNEL PANIC: "); serial_puts(name); serial_puts(" !!!\n");

    uint8_t a=VGA_ATTR(COLOR_WHITE,COLOR_RED);
    vga_clear(a);
    vga_draw_hline(0,0,80,' ',a);
    vga_puts_at(25,0,"*** STELLARESOS KERNEL PANIC ***",VGA_ATTR(COLOR_YELLOW,COLOR_RED));
    vga_draw_hline(1,0,80,0xCD,VGA_ATTR(COLOR_LIGHT_RED,COLOR_RED));

    vga_puts_at(2,3,"Excecao:",a);
    vga_puts_at(11,3,name,VGA_ATTR(COLOR_YELLOW,COLOR_RED));

    char b[12];
    vga_puts_at(2,4,"INT num:",a);
    itoa((int)r->int_no,b,10); vga_puts_at(11,4,b,VGA_ATTR(COLOR_YELLOW,COLOR_RED));

    vga_puts_at(2,5,"EIP:    ",a);
    int_to_hex(r->eip,b); vga_puts_at(11,5,b,VGA_ATTR(COLOR_YELLOW,COLOR_RED));

    vga_puts_at(2,6,"Err:    ",a);
    int_to_hex(r->err_code,b); vga_puts_at(11,6,b,VGA_ATTR(COLOR_YELLOW,COLOR_RED));

    vga_draw_hline(8,0,80,0xCD,VGA_ATTR(COLOR_LIGHT_RED,COLOR_RED));
    vga_puts_at(2,10,"Sistema travado. Reinicie o computador.",a);

    for(;;)__asm__ volatile("cli;hlt");
}

void irq_handler(regs_t*r){
    if(r->int_no>=40) outb(0xA0,0x20);
    outb(0x20,0x20);
    int irq=(int)(r->int_no-32);
    if(irq>=0&&irq<16&&irq_handlers[irq]) irq_handlers[irq](r);
}
