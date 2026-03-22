/* ============================================================
 *  StellaresOS -- kernel/login.c
 *  Sistema de login com usuários persistidos no disco
 * ============================================================ */
#include "login.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/pit.h"
#include "../fs/diskfs.h"
#include "../libc/string.h"

/* Formato do /etc/passwd no disco:
 * usuario:senha_hash:home\n
 * Ex: root:toor:/ \n
 *     stella:1234:/home/stella\n
 */

#define MAX_USERS 8
#define PASSWD_FILE "passwd"

typedef struct {
    char name[32];
    char pass[32];
    char home[64];
} passwd_entry_t;

static passwd_entry_t users[MAX_USERS];
static int nusers = 0;

/* Hash simples de senha (djb2) */
static uint32_t hash_pass(const char *s) {
    uint32_t h = 5381;
    while(*s) h = h * 33 + (uint8_t)*s++;
    return h;
}

static void itoa_u(uint32_t v, char *b) {
    char tmp[12]; int i=0;
    if(!v){b[0]='0';b[1]=0;return;}
    while(v){tmp[i++]='0'+v%10;v/=10;}
    int j=0;while(i--)b[j++]=tmp[i];b[j]=0;
}

static int parse_passwd(const char *buf) {
    nusers = 0;
    char line[128];
    const char *p = buf;
    while(*p && nusers < MAX_USERS) {
        /* Lê uma linha */
        int len = 0;
        while(*p && *p != '\n' && len < 127) line[len++] = *p++;
        if(*p == '\n') p++;
        line[len] = 0;
        if(!len) continue;

        /* Parseia usuario:hash:home */
        char *tok = strtok(line, ":");
        if(!tok) continue;
        strncpy(users[nusers].name, tok, 31);

        tok = strtok(NULL, ":");
        if(!tok) continue;
        strncpy(users[nusers].pass, tok, 31);

        tok = strtok(NULL, ":");
        if(!tok) strcpy(users[nusers].home, "/");
        else strncpy(users[nusers].home, tok, 63);

        nusers++;
    }
    return nusers;
}

static void create_default_passwd(void) {
    /* Cria usuários padrão */
    char buf[512] = "";
    /* root:toor:/ */
    char h[12]; itoa_u(hash_pass("toor"), h);
    strcat(buf, "root:"); strcat(buf, h); strcat(buf, ":/\n");
    /* stella:1234:/home/stella */
    itoa_u(hash_pass("1234"), h);
    strcat(buf, "stella:"); strcat(buf, h); strcat(buf, ":/home/stella\n");

    diskfs_write(PASSWD_FILE, buf, strlen(buf));
}

int login_init(void) {
    if(!diskfs_ready()) return -1;

    char buf[512];
    int n = diskfs_read(PASSWD_FILE, buf, sizeof(buf)-1);
    if(n <= 0) {
        create_default_passwd();
        n = diskfs_read(PASSWD_FILE, buf, sizeof(buf)-1);
    }
    if(n > 0) { buf[n] = 0; parse_passwd(buf); }
    return nusers;
}

/* ---- Tela de login ---- */
static void draw_login_screen(void) {
    uint8_t bg  = VGA_ATTR(COLOR_BLACK,       COLOR_BLACK);
    uint8_t box = VGA_ATTR(COLOR_WHITE,       COLOR_BLUE);
    uint8_t ttl = VGA_ATTR(COLOR_YELLOW,      COLOR_BLUE);
    uint8_t dim = VGA_ATTR(COLOR_DARK_GREY,   COLOR_BLACK);
    uint8_t st  = VGA_ATTR(COLOR_LIGHT_CYAN,  COLOR_BLACK);

    vga_clear(bg);

    /* Estrelas de fundo */
    int sx[]={3,10,20,35,50,65,72,15,45,60,8,30,55,70,25};
    int sy[]={2,5,3,1,4,2,6,8,7,9,12,10,11,14,15};
    for(int i=0;i<15;i++) vga_putchar_at(sx[i],sy[i],'.',dim);

    /* Logo pequeno */
    vga_puts_at(28,3,"  ___  _       _ _",st);
    vga_puts_at(28,4," / __|| |_ ___| | |",st);
    vga_puts_at(28,5," \\__ \\|  _/ -_) | |",VGA_ATTR(COLOR_WHITE,COLOR_BLACK));
    vga_puts_at(28,6," |___/ \\__\\___|_|_|",VGA_ATTR(COLOR_LIGHT_CYAN,COLOR_BLACK));
    vga_puts_at(30,7,"StellaresOS v0.1",VGA_ATTR(COLOR_DARK_GREY,COLOR_BLACK));

    /* Caixa de login */
    int bx=25, by=9, bw=30, bh=10;
    for(int y=by;y<by+bh;y++)
        for(int x=bx;x<bx+bw;x++)
            vga_putchar_at(x,y,' ',box);

    vga_putchar_at(bx,   by,    0xC9, box);
    vga_putchar_at(bx+bw-1,by,  0xBB, box);
    vga_putchar_at(bx,   by+bh-1,0xC8,box);
    vga_putchar_at(bx+bw-1,by+bh-1,0xBC,box);
    for(int x=bx+1;x<bx+bw-1;x++){
        vga_putchar_at(x,by,    0xCD,box);
        vga_putchar_at(x,by+bh-1,0xCD,box);
    }
    for(int y=by+1;y<by+bh-1;y++){
        vga_putchar_at(bx,y,    0xBA,box);
        vga_putchar_at(bx+bw-1,y,0xBA,box);
    }

    /* Título da caixa */
    int tlen=(int)strlen("  Login  ");
    vga_puts_at(bx+(bw-tlen)/2, by, "  Login  ", ttl);
}

static void read_input(int col, int row, char *buf, int maxlen, int hide) {
    int len=0; buf[0]=0;
    uint8_t ia = VGA_ATTR(COLOR_WHITE, COLOR_BLUE);
    while(1) {
        int c = keyboard_getchar();
        if(c=='\n'||c=='\r') { buf[len]=0; return; }
        if(c=='\b') {
            if(len>0) {
                len--;
                vga_putchar_at(col+len, row, ' ', ia);
            }
            continue;
        }
        if(c>=32 && c<127 && len<maxlen-1) {
            buf[len++]=( char)c;
            buf[len]=0;
            vga_putchar_at(col+len-1, row, hide?'*':( char)c, ia);
        }
    }
}

int login_screen(user_t *out) {
    char username[32], password[32];
    uint8_t box   = VGA_ATTR(COLOR_WHITE, COLOR_BLUE);
    uint8_t label = VGA_ATTR(COLOR_YELLOW, COLOR_BLUE);
    uint8_t err   = VGA_ATTR(COLOR_LIGHT_RED, COLOR_BLUE);
    uint8_t dim   = VGA_ATTR(COLOR_DARK_GREY, COLOR_BLACK);

    int bx=25, by=9;
    int attempts = 0;

    while(1) {
        draw_login_screen();

        /* Campos */
        vga_puts_at(bx+2, by+2, "Usuario:", label);
        /* Campo input usuario */
        for(int i=0;i<16;i++) vga_putchar_at(bx+2+i, by+3, ' ', box);
        vga_set_cursor(bx+2, by+3);

        read_input(bx+2, by+3, username, 30, 0);

        vga_puts_at(bx+2, by+5, "Senha:  ", label);
        for(int i=0;i<16;i++) vga_putchar_at(bx+2+i, by+6, ' ', box);
        vga_set_cursor(bx+2, by+6);

        read_input(bx+2, by+6, password, 30, 1);

        /* Verifica credenciais */
        uint32_t ph = hash_pass(password);
        char ph_str[12]; itoa_u(ph, ph_str);

        int found = -1;
        for(int i=0;i<nusers;i++) {
            if(strcmp(users[i].name, username)==0) {
                /* Compara hash armazenado */
                uint32_t stored = (uint32_t)atoi(users[i].pass);
                if(stored == ph) { found = i; break; }
            }
        }

        if(found >= 0) {
            /* Login OK — animação */
            vga_puts_at(bx+2, by+8, "  Bem-vindo! Carregando...  ",
                VGA_ATTR(COLOR_LIGHT_GREEN, COLOR_BLUE));
            pit_sleep_ms(800);
            strncpy(out->username, users[found].name, 31);
            strncpy(out->home,     users[found].home,  63);
            return 0;
        }

        /* Falha */
        attempts++;
        vga_puts_at(bx+2, by+8, "  Senha incorreta!          ", err);
        pit_sleep_ms(1000);

        if(attempts >= 3) {
            /* Após 3 tentativas: entra como guest */
            vga_puts_at(bx+2, by+8, "  Entrando como guest...    ",
                VGA_ATTR(COLOR_YELLOW, COLOR_BLUE));
            pit_sleep_ms(1000);
            strcpy(out->username, "guest");
            strcpy(out->home, "/tmp");
            return 1;
        }
    }
}
