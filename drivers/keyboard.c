#include "keyboard.h"
#include "../kernel/idt.h"
#include "../libc/string.h"
#define KB_DATA 0x60
#define BUF 256
static inline uint8_t inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static int buf[BUF],head=0,tail=0,shift=0,ext=0;
static const char sc_norm[]={0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '};
static const char sc_shft[]={0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S','D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '};
static void push(int c){int n=(head+1)%BUF;if(n!=tail){buf[head]=c;head=n;}}
static void handler(regs_t*r){(void)r;uint8_t sc=inb(KB_DATA);if(sc==0xE0){ext=1;return;}int rel=sc&0x80;sc&=0x7F;if(ext){ext=0;if(!rel){if(sc==0x48)push(KEY_UP);else if(sc==0x50)push(KEY_DOWN);else if(sc==0x4B)push(KEY_LEFT);else if(sc==0x4D)push(KEY_RIGHT);}return;}if(sc==0x2A||sc==0x36){shift=!rel;return;}if(rel)return;if(sc==0x3B){push(0x10A);return;}if(sc==0x3C){push(0x10B);return;}if(sc==0x3D){push(0x10C);return;}if(sc==0x3E){push(0x10D);return;}if(sc==0x3F){push(0x10E);return;}if(sc<sizeof(sc_norm)){char c=shift?sc_shft[sc]:sc_norm[sc];if(c)push((int)c);}}
void keyboard_init(void){memset(buf,0,sizeof(buf));irq_install_handler(1,handler);}
int keyboard_poll(void){if(head==tail)return -1;int c=buf[tail];tail=(tail+1)%BUF;return c;}
int keyboard_getchar(void){int c;while((c=keyboard_poll())==-1)__asm__ volatile("hlt");return c;}
void keyboard_flush(void){head=tail=0;}
