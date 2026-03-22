#include "idt.h"
#include "pmm.h"
#include "heap.h"
#include "scheduler.h"
#include "login.h"
#include "installer.h"
#include "syscall.h"
#include "elf_loader.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../drivers/pit.h"
#include "../drivers/keyboard.h"
#include "../drivers/ata.h"
#include "../fs/diskfs.h"
#include "../libc/string.h"
#include "../shell/stellash.h"

typedef struct __attribute__((packed)){
    uint32_t flags,mem_lower,mem_upper;
}mbi_t;

#define MBOOT_MAGIC  0x2BADB002
#define HEAP_START   0x00400000
#define HEAP_SIZE    (4*1024*1024)

static int brow=3;
static void blog(int ok,const char*msg){
    uint8_t dot=ok?VGA_ATTR(COLOR_LIGHT_GREEN,COLOR_BLACK):VGA_ATTR(COLOR_YELLOW,COLOR_BLACK);
    uint8_t bd=VGA_ATTR(COLOR_DARK_GREY,COLOR_BLACK);
    vga_puts_at(2,brow," [",bd);
    vga_puts_at(4,brow,ok?"OK":"--",dot);
    vga_puts_at(6,brow,"] ",bd);
    vga_puts_at(8,brow,msg,VGA_ATTR(COLOR_LIGHT_GREY,COLOR_BLACK));
    serial_puts(ok?"[OK] ":"[--] "); serial_puts(msg); serial_puts("\n");
    brow++;
}

static void draw_header(void){
    uint8_t hdr=VGA_ATTR(COLOR_WHITE,COLOR_BLUE);
    uint8_t sub=VGA_ATTR(COLOR_LIGHT_CYAN,COLOR_BLUE);
    uint8_t bg=VGA_ATTR(COLOR_LIGHT_GREY,COLOR_BLACK);
    vga_clear(bg);
    vga_fill_row(0,' ',hdr);
    vga_puts_at(2,0,"StellaresOS",VGA_ATTR(COLOR_YELLOW,COLOR_BLUE));
    vga_puts_at(14,0,"v0.1",hdr);
    vga_puts_at(19,0,"|",VGA_ATTR(COLOR_DARK_GREY,COLOR_BLUE));
    vga_puts_at(21,0,"Microkernel x86",sub);
    vga_puts_at(37,0,"|",VGA_ATTR(COLOR_DARK_GREY,COLOR_BLUE));
    vga_puts_at(39,0,"Inicializando...",hdr);
    vga_draw_hline(1,0,80,0xCD,VGA_ATTR(COLOR_CYAN,COLOR_BLACK));
    vga_puts_at(2,2,"Boot Log:",VGA_ATTR(COLOR_YELLOW,COLOR_BLACK));
}

static void idle_task(void){
    for(;;){
        char u[16]; itoa((int)pit_seconds(),u,10);
        char ul[24]; strcpy(ul,"uptime: "); strcat(ul,u); strcat(ul,"s ");
        vga_puts_at(80-(int)strlen(ul),0,ul,VGA_ATTR(COLOR_WHITE,COLOR_BLUE));
        proc_sleep(500);
    }
}

void kmain(uint32_t magic, mbi_t *mbi){
    serial_init();
    idt_init();

    uint32_t mem_kb=32768;
    if(magic==MBOOT_MAGIC&&mbi&&(mbi->flags&0x01))
        mem_kb=mbi->mem_upper+1024;

    pmm_init(mem_kb>1024?mem_kb-1024:0);
    pmm_mark_free(0x00100000,(mem_kb-1024)*1024);
    pmm_mark_used(0x00000000,HEAP_START);
    heap_init(HEAP_START,HEAP_SIZE);
    pit_init(1000);
    keyboard_init();
    __asm__ volatile("sti");

    vga_init();
    draw_header();

    blog(1,"GDT + IDT + PIC 8259");
    char mbuf[48]; strcpy(mbuf,"RAM: ");
    char tmp[12]; itoa((int)mem_kb,tmp,10);
    strcat(mbuf,tmp); strcat(mbuf," KB");
    blog(1,mbuf);
    blog(1,"PMM + Heap 4MB | PIT 1000Hz | Teclado");

    sched_init();
    blog(1,"Scheduler: round-robin preemptivo");

    syscall_init();
    blog(1,"Syscalls: ABI Linux i386 (INT 0x80)");

    /* Disco ATA */
    ata_init();
    if(ata_detect()){
        char dmsg[48]; strcpy(dmsg,"ATA: ");
        strcat(dmsg,ata_model());
        blog(1,dmsg);

        /* DiskFS */
        if(diskfs_init()==0){
            blog(1,"DiskFS: filesystem persistente montado");
        } else {
            blog(0,"DiskFS: falha ao montar disco");
        }

        /* Sistema de usuarios */
        int nu = login_init();
        if(nu > 0){
            char lmsg[32]; strcpy(lmsg,"Login: ");
            itoa(nu,tmp,10); strcat(lmsg,tmp); strcat(lmsg," usuario(s) carregados");
            blog(1,lmsg);
        } else {
            blog(0,"Login: sem disco, modo guest");
        }
    } else {
        blog(0,"ATA: sem disco (use make run-disk)");
        blog(0,"DiskFS: desabilitado (sem disco)");
    }

    blog(1,"RamFS: filesystem em memoria");

    vga_draw_hline(brow,0,80,0xCD,VGA_ATTR(COLOR_CYAN,COLOR_BLACK));
    brow++;
    vga_set_cursor(0,brow);
    vga_set_attr(VGA_ATTR(COLOR_LIGHT_GREEN,COLOR_BLACK));
    char kb[12]; itoa((int)pmm_free_kb(),kb,10);
    vga_printf("  Sistema pronto! %s KB livres.\n",kb);
    serial_puts("=== Boot OK! ===\n");

    pit_sleep_ms(400);

    /* Instalador (primeira vez) ou Login */
    user_t current_user;
    strcpy(current_user.username, "stella");
    strcpy(current_user.home, "/");

    if(ata_detect() && diskfs_ready()){
        /* Verifica se precisa instalar */
        install_config_t icfg;
        if(installer_run(&icfg) == 0){
            /* Instalou agora — usa o usuário criado */
            strncpy(current_user.username, icfg.username, 31);
            strcpy(current_user.home, "/home/");
            strcat(current_user.home, icfg.username);
            /* Recarrega usuários do disco */
            login_init();
        } else {
            /* Já instalado — vai para login */
            login_screen(&current_user);
        }
    } else {
        /* Sem disco: aviso e entra direto */
        vga_set_attr(VGA_ATTR(COLOR_YELLOW,COLOR_BLACK));
        vga_puts("  Sem disco: entrando sem login. Use make run-disk.\n");
        pit_sleep_ms(1200);
    }

    /* Background task */
    proc_create("uptime-bg", idle_task);

    /* Lança shell com o usuário logado */
    stellash_run_as(&current_user);

    for(;;) __asm__ volatile("cli;hlt");
}
