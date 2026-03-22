/* ============================================================
 *  StellaresOS -- shell/editor.c
 *  Editor de texto simples (nano-style)
 * ============================================================ */
#include "editor.h"
#include "../drivers/keyboard.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../libc/string.h"
#include "../fs/ramfs.h"

#define ED_ROWS   22   /* Linhas visíveis (sem header/footer) */
#define ED_COLS   78
#define ED_MAXLINES 64
#define ED_LINELEN  128

static char  lines[ED_MAXLINES][ED_LINELEN];
static int   nlines = 0;
static int   cx = 0, cy = 0;   /* Cursor: coluna e linha no buffer */
static int   scroll_y = 0;     /* Linha do topo da tela */
static int   dirty = 0;
static char  filename[64];

#define A_HEADER VGA_ATTR(COLOR_WHITE,      COLOR_BLUE)
#define A_FOOTER VGA_ATTR(COLOR_BLACK,      COLOR_CYAN)
#define A_TEXT   VGA_ATTR(COLOR_WHITE,      COLOR_BLACK)
#define A_CURSOR VGA_ATTR(COLOR_BLACK,      COLOR_WHITE)
#define A_LNUM   VGA_ATTR(COLOR_DARK_GREY,  COLOR_BLACK)

static void ed_draw(void) {
    /* Header */
    vga_fill_row(0, ' ', A_HEADER);
    vga_puts_at(1, 0, "StellaresOS Editor", A_HEADER);
    vga_puts_at(22,0, "|", VGA_ATTR(COLOR_DARK_GREY,COLOR_BLUE));
    vga_puts_at(24,0, filename, VGA_ATTR(COLOR_YELLOW,COLOR_BLUE));
    if(dirty) vga_puts_at(24+(int)strlen(filename), 0, " [modificado]",
        VGA_ATTR(COLOR_LIGHT_RED,COLOR_BLUE));

    /* Conteúdo */
    for(int r = 0; r < ED_ROWS; r++) {
        int line_idx = scroll_y + r;
        vga_fill_row(r+1, ' ', A_TEXT);

        /* Número de linha */
        char lnum[6]; itoa(line_idx+1, lnum, 10);
        int llen = (int)strlen(lnum);
        for(int i=llen;i<4;i++) vga_putchar_at(i-llen,r+1,' ',A_LNUM);
        vga_puts_at(4-llen, r+1, lnum, A_LNUM);
        vga_putchar_at(4, r+1, ' ', A_LNUM);

        if(line_idx < nlines) {
            int dlen = (int)strlen(lines[line_idx]);
            for(int c = 0; c < dlen && c < ED_COLS; c++) {
                uint8_t a = (line_idx==cy && c==cx) ? A_CURSOR : A_TEXT;
                vga_putchar_at(5+c, r+1, lines[line_idx][c], a);
            }
            /* Cursor no final da linha */
            if(line_idx==cy && cx==dlen && cx<ED_COLS)
                vga_putchar_at(5+cx, r+1, ' ', A_CURSOR);
        } else {
            /* Linha vazia com cursor */
            if(line_idx==cy)
                vga_putchar_at(5, r+1, ' ', A_CURSOR);
            else
                vga_putchar_at(5, r+1, '~', A_LNUM);
        }
    }

    /* Footer com atalhos */
    vga_fill_row(23, ' ', A_FOOTER);
    vga_puts_at(1,23,"^S Salvar  ESC/^Q Sair  ^K Apagar linha  Setas Mover",A_FOOTER);
    char pos[24]; strcpy(pos,"Lin:"); char t[8]; itoa(cy+1,t,10); strcat(pos,t);
    strcat(pos," Col:"); itoa(cx+1,t,10); strcat(pos,t);
    vga_puts_at(78-(int)strlen(pos), 23, pos, A_FOOTER);
}

static void ed_insert_char(char c) {
    if(cy >= ED_MAXLINES) return;
    if(nlines == 0) { nlines = 1; lines[0][0] = 0; }
    while(cy >= nlines) { lines[nlines][0]=0; nlines++; }

    char *line = lines[cy];
    int len = (int)strlen(line);
    if(len >= ED_LINELEN-1) return;

    /* Insere c na posição cx */
    for(int i = len; i >= cx; i--) line[i+1] = line[i];
    line[cx] = c;
    cx++;
    dirty = 1;
}

static void ed_delete_char(void) {
    if(cy >= nlines) return;
    char *line = lines[cy];
    int len = (int)strlen(line);
    if(cx > 0) {
        for(int i = cx-1; i < len; i++) line[i] = line[i+1];
        cx--;
        dirty = 1;
    } else if(cy > 0) {
        /* Junta com linha anterior */
        char *prev = lines[cy-1];
        int plen = (int)strlen(prev);
        if(plen + len < ED_LINELEN-1) {
            strcat(prev, line);
            cx = plen;
            for(int i = cy; i < nlines-1; i++)
                strcpy(lines[i], lines[i+1]);
            nlines--;
            cy--;
            dirty = 1;
        }
    }
}

static void ed_newline(void) {
    if(nlines >= ED_MAXLINES) return;
    char *line = lines[cy];
    int len = (int)strlen(line);

    /* Move linhas abaixo para baixo */
    for(int i = nlines; i > cy+1; i--)
        strcpy(lines[i], lines[i-1]);
    nlines++;

    /* Divide a linha atual em cx */
    strcpy(lines[cy+1], line+cx);
    line[cx] = 0;

    cy++;
    cx = 0;
    if(cy - scroll_y >= ED_ROWS) scroll_y++;
    dirty = 1;
}

static void ed_kill_line(void) {
    if(cy >= nlines) return;
    for(int i = cy; i < nlines-1; i++)
        strcpy(lines[i], lines[i+1]);
    nlines--;
    if(nlines == 0) { nlines = 1; lines[0][0] = 0; }
    if(cy >= nlines) cy = nlines-1;
    cx = 0;
    dirty = 1;
}

static void ed_save(void) {
    ramfs_node_t *root = ramfs_root();
    /* Cria/encontra arquivo */
    ramfs_node_t *f = ramfs_find(root, filename);
    if(!f) f = ramfs_create(root, filename);
    if(!f) return;

    /* Serializa linhas */
    char buf[RAMFS_DATA_MAX];
    buf[0] = 0;
    int used = 0;
    for(int i = 0; i < nlines && used < RAMFS_DATA_MAX-2; i++) {
        int l = (int)strlen(lines[i]);
        if(used + l + 1 >= RAMFS_DATA_MAX-1) break;
        strcpy(buf+used, lines[i]);
        used += l;
        buf[used++] = '\n';
        buf[used] = 0;
    }
    ramfs_write(f, buf, (size_t)used);
    dirty = 0;
}

static void ed_load(void) {
    ramfs_node_t *root = ramfs_root();
    ramfs_node_t *f = ramfs_find(root, filename);

    nlines = 1; lines[0][0] = 0;
    cx = 0; cy = 0; scroll_y = 0;

    if(!f || f->type != NODE_FILE) return;

    char buf[RAMFS_DATA_MAX+1];
    int n = ramfs_read(f, buf, RAMFS_DATA_MAX);
    if(n <= 0) return;
    buf[n] = 0;

    nlines = 0;
    int col = 0;
    for(int i = 0; i <= n && nlines < ED_MAXLINES; i++) {
        if(buf[i] == '\n' || buf[i] == 0) {
            lines[nlines][col] = 0;
            nlines++;
            col = 0;
        } else if(col < ED_LINELEN-1) {
            lines[nlines][col++] = buf[i];
        }
    }
    if(nlines == 0) { nlines = 1; lines[0][0] = 0; }
}

void editor_run(const char *fname) {
    strncpy(filename, fname, 63);
    ed_load();
    dirty = 0;

    while(1) {
        ed_draw();
        int c = keyboard_getchar();

        /* Ctrl+S = salvar */
        /* Ctrl+S ou F2 = salvar */
        if(c == 19 || c == KEY_F2) { ed_save(); continue; }
        /* Ctrl+Q ou ESC = sair */
        if(c == 17 || c == 27) {
            if(dirty) {
                /* Pede confirmação se modificado */
                uint8_t fa = VGA_ATTR(COLOR_BLACK,COLOR_YELLOW);
                vga_fill_row(23,' ',fa);
                vga_puts_at(1,23,"  Arquivo modificado! S=Salvar e sair  N=Sair sem salvar  ESC=Cancelar  ",fa);
                int ch = keyboard_getchar();
                if(ch == 's' || ch == 'S') { ed_save(); vga_clear(VGA_ATTR(COLOR_LIGHT_GREY,COLOR_BLACK)); return; }
                if(ch == 'n' || ch == 'N') { vga_clear(VGA_ATTR(COLOR_LIGHT_GREY,COLOR_BLACK)); return; }
                continue; /* ESC cancela */
            }
            vga_clear(VGA_ATTR(COLOR_LIGHT_GREY,COLOR_BLACK));
            return;
        }
        /* Ctrl+K = apaga linha */
        if(c == 11) { ed_kill_line(); continue; }

        /* Teclas de movimento */
        if(c == KEY_UP) {
            if(cy > 0) {
                cy--;
                int len = cy < nlines ? (int)strlen(lines[cy]) : 0;
                if(cx > len) cx = len;
                if(cy < scroll_y) scroll_y--;
            }
            continue;
        }
        if(c == KEY_DOWN) {
            if(cy < nlines-1) {
                cy++;
                int len = (int)strlen(lines[cy]);
                if(cx > len) cx = len;
                if(cy - scroll_y >= ED_ROWS) scroll_y++;
            }
            continue;
        }
        if(c == KEY_LEFT) {
            if(cx > 0) cx--;
            else if(cy > 0) { cy--; cx=(int)strlen(lines[cy]); if(cy<scroll_y)scroll_y--; }
            continue;
        }
        if(c == KEY_RIGHT) {
            int len = cy < nlines ? (int)strlen(lines[cy]) : 0;
            if(cx < len) cx++;
            else if(cy < nlines-1) { cy++; cx=0; if(cy-scroll_y>=ED_ROWS)scroll_y++; }
            continue;
        }
        if(c == 0x117) { cx=0; continue; }
        if(c == 0x118) { if(cy<nlines)cx=(int)strlen(lines[cy]); continue; }

        /* Enter */
        if(c == '\n' || c == '\r') { ed_newline(); continue; }
        /* Backspace */
        if(c == '\b') { ed_delete_char(); continue; }
        /* Caractere normal */
        if(c >= 32 && c < 127) { ed_insert_char((char)c); continue; }
    }
}
