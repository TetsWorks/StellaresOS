#include "vga.h"
#include "../libc/string.h"
#include <stdarg.h>

#define VGA_CTRL 0x3D4
#define VGA_DATA 0x3D5

static inline void outb(uint16_t p, uint8_t v){
    __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));
}

static int cur_col=0, cur_row=0;
static uint8_t cur_attr=0;

void vga_init(void){
    cur_attr=VGA_ATTR(COLOR_LIGHT_GREY,COLOR_BLACK);
    vga_clear(cur_attr);
}

void vga_clear(uint8_t a){
    uint16_t b=VGA_ENTRY(' ',a);
    for(int i=0;i<VGA_COLS*VGA_ROWS;i++) VGA_MEM[i]=b;
    cur_col=0; cur_row=0;
    vga_set_cursor(0,0);
}

void vga_set_attr(uint8_t a){ cur_attr=a; }
void vga_set_color(vga_color_t fg, vga_color_t bg){ cur_attr=VGA_ATTR(fg,bg); }

void vga_putchar_at(int c, int r, char ch, uint8_t a){
    if(c<0||c>=VGA_COLS||r<0||r>=VGA_ROWS) return;
    VGA_MEM[r*VGA_COLS+c]=VGA_ENTRY(ch,a);
}

void vga_puts_at(int c, int r, const char *s, uint8_t a){
    while(*s && c<VGA_COLS) vga_putchar_at(c++,r,*s++,a);
}

void vga_fill_row(int r, char c, uint8_t a){
    for(int i=0;i<VGA_COLS;i++) vga_putchar_at(i,r,c,a);
}

static void scroll(void){
    for(int r=0;r<VGA_ROWS-1;r++)
        for(int c=0;c<VGA_COLS;c++)
            VGA_MEM[r*VGA_COLS+c]=VGA_MEM[(r+1)*VGA_COLS+c];
    uint16_t b=VGA_ENTRY(' ',cur_attr);
    for(int c=0;c<VGA_COLS;c++) VGA_MEM[(VGA_ROWS-1)*VGA_COLS+c]=b;
    cur_row=VGA_ROWS-1;
}

void vga_putchar(char c){
    if(c=='\n'){ cur_col=0; cur_row++; }
    else if(c=='\r'){ cur_col=0; }
    else if(c=='\b'){
        if(cur_col>0){ cur_col--; vga_putchar_at(cur_col,cur_row,' ',cur_attr); }
    }
    else if(c=='\t'){ cur_col=(cur_col+8)&~7; if(cur_col>=VGA_COLS){cur_col=0;cur_row++;} }
    else{
        vga_putchar_at(cur_col,cur_row,c,cur_attr);
        cur_col++;
        if(cur_col>=VGA_COLS){ cur_col=0; cur_row++; }
    }
    if(cur_row>=VGA_ROWS) scroll();
    vga_set_cursor(cur_col,cur_row);
}

void vga_puts(const char *s){ while(*s) vga_putchar(*s++); }

void vga_set_cursor(int c, int r){
    cur_col=c; cur_row=r;
    uint16_t p=(uint16_t)(r*VGA_COLS+c);
    outb(VGA_CTRL,0x0F); outb(VGA_DATA,(uint8_t)(p&0xFF));
    outb(VGA_CTRL,0x0E); outb(VGA_DATA,(uint8_t)((p>>8)&0xFF));
}

void vga_get_cursor(int *c, int *r){ if(c)*c=cur_col; if(r)*r=cur_row; }

void vga_draw_hline(int r, int c, int l, char ch, uint8_t a){
    for(int i=0;i<l;i++) vga_putchar_at(c+i,r,ch,a);
}

void vga_draw_box(int x, int y, int w, int h, uint8_t a){
    vga_putchar_at(x,y,0xDA,a); vga_putchar_at(x+w-1,y,0xBF,a);
    vga_putchar_at(x,y+h-1,0xC0,a); vga_putchar_at(x+w-1,y+h-1,0xD9,a);
    for(int i=1;i<w-1;i++){
        vga_putchar_at(x+i,y,0xC4,a);
        vga_putchar_at(x+i,y+h-1,0xC4,a);
    }
    for(int i=1;i<h-1;i++){
        vga_putchar_at(x,y+i,0xB3,a);
        vga_putchar_at(x+w-1,y+i,0xB3,a);
    }
    for(int r2=y+1;r2<y+h-1;r2++)
        for(int c2=x+1;c2<x+w-1;c2++)
            vga_putchar_at(c2,r2,' ',a);
}

void vga_probe(void){
    volatile uint16_t *m=(volatile uint16_t*)0xB8000;
    m[0]=0x0F00|'V'; m[1]=0x0F00|'G'; m[2]=0x0F00|'A';
    m[3]=0x0F00|':'; m[4]=0x0F00|'O'; m[5]=0x0F00|'K';
}

/* ---- vga_printf com suporte completo a %-Ns ---- */
static void vga_puts_padded(const char *s, int width){
    int len=(int)strlen(s);
    vga_puts(s);
    /* Padding à direita */
    for(int i=len; i<width; i++) vga_putchar(' ');
}

void vga_printf(const char *fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    char buf[64];

    while(*fmt){
        if(*fmt!='%'){ vga_putchar(*fmt++); continue; }
        fmt++; /* pula '%' */

        /* Lê flag de alinhamento e largura */
        int left = 0, width = 0;
        if(*fmt=='-'){ left=1; fmt++; }
        while(*fmt>='0' && *fmt<='9'){
            width=width*10+(*fmt-'0');
            fmt++;
        }

        switch(*fmt){
            case 'd': case 'i': {
                int v=va_arg(ap,int);
                itoa(v,buf,10);
                if(left) vga_puts_padded(buf,width);
                else{ int l=(int)strlen(buf); for(int i=l;i<width;i++)vga_putchar(' '); vga_puts(buf); }
                break;
            }
            case 'u': {
                unsigned v=va_arg(ap,unsigned);
                itoa((int)v,buf,10);
                if(left) vga_puts_padded(buf,width);
                else{ int l=(int)strlen(buf); for(int i=l;i<width;i++)vga_putchar(' '); vga_puts(buf); }
                break;
            }
            case 'x': case 'X': {
                unsigned v=va_arg(ap,unsigned);
                int_to_hex(v,buf);
                vga_puts(buf);
                break;
            }
            case 's': {
                const char *s=va_arg(ap,const char*);
                if(!s) s="(null)";
                if(left) vga_puts_padded(s,width);
                else{ int l=(int)strlen(s); for(int i=l;i<width;i++)vga_putchar(' '); vga_puts(s); }
                break;
            }
            case 'c': {
                char c=(char)va_arg(ap,int);
                vga_putchar(c);
                break;
            }
            case '%': vga_putchar('%'); break;
            default:  vga_putchar('%'); vga_putchar(*fmt); break;
        }
        fmt++;
    }
    va_end(ap);
}
