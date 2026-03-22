/* ============================================================
 *  StellaresOS -- shell/snake.c
 *  Jogo Snake rodando direto no kernel!
 * ============================================================ */
#include "snake.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/pit.h"
#include "../libc/string.h"

#define BOARD_W   38
#define BOARD_H   20
#define BOARD_X   3
#define BOARD_Y   2
#define MAX_SNAKE 200

typedef struct { int x, y; } point_t;

static point_t snake[MAX_SNAKE];
static int     slen = 0;
static int     dx = 1, dy = 0;
static int     ndx = 1, ndy = 0;
static point_t food;
static int     score = 0;
static int     alive = 1;

#define A_BORDER  VGA_ATTR(COLOR_CYAN,        COLOR_BLACK)
#define A_SNAKE_H VGA_ATTR(COLOR_LIGHT_GREEN, COLOR_BLACK)
#define A_SNAKE_B VGA_ATTR(COLOR_GREEN,       COLOR_BLACK)
#define A_FOOD    VGA_ATTR(COLOR_LIGHT_RED,   COLOR_BLACK)
#define A_SCORE   VGA_ATTR(COLOR_YELLOW,      COLOR_BLACK)
#define A_BG      VGA_ATTR(COLOR_BLACK,       COLOR_BLACK)
#define A_TITLE   VGA_ATTR(COLOR_WHITE,       COLOR_BLUE)
#define A_MSG     VGA_ATTR(COLOR_WHITE,       COLOR_BLACK)

/* LCG simples para posição da comida */
static uint32_t rng_state = 12345;
static uint32_t rng(void) {
    rng_state = rng_state * 1664525 + 1013904223;
    return rng_state;
}

static void place_food(void) {
    int tries = 0;
    while(tries++ < 200) {
        int fx = (int)(rng() % BOARD_W);
        int fy = (int)(rng() % BOARD_H);
        int ok = 1;
        for(int i=0;i<slen;i++)
            if(snake[i].x==fx && snake[i].y==fy){ ok=0; break; }
        if(ok){ food.x=fx; food.y=fy; return; }
    }
}

static void draw_board(void) {
    vga_clear(A_BG);

    /* Título */
    vga_fill_row(0,' ',A_TITLE);
    vga_puts_at(1,0," StellaresOS",VGA_ATTR(COLOR_YELLOW,COLOR_BLUE));
    vga_puts_at(14,0," | Snake",A_TITLE);
    vga_puts_at(23,0," | Setas=mover  Q=sair",VGA_ATTR(COLOR_LIGHT_CYAN,COLOR_BLUE));

    /* Borda */
    for(int x=0;x<=BOARD_W+1;x++){
        vga_putchar_at(BOARD_X+x-1,BOARD_Y-1,    0xCD, A_BORDER);
        vga_putchar_at(BOARD_X+x-1,BOARD_Y+BOARD_H, 0xCD, A_BORDER);
    }
    for(int y=0;y<=BOARD_H+1;y++){
        vga_putchar_at(BOARD_X-1,  BOARD_Y+y-1, 0xBA, A_BORDER);
        vga_putchar_at(BOARD_X+BOARD_W, BOARD_Y+y-1, 0xBA, A_BORDER);
    }
    vga_putchar_at(BOARD_X-1,       BOARD_Y-1,      0xC9, A_BORDER);
    vga_putchar_at(BOARD_X+BOARD_W, BOARD_Y-1,      0xBB, A_BORDER);
    vga_putchar_at(BOARD_X-1,       BOARD_Y+BOARD_H,0xC8, A_BORDER);
    vga_putchar_at(BOARD_X+BOARD_W, BOARD_Y+BOARD_H,0xBC, A_BORDER);

    /* Placar */
    vga_set_attr(A_SCORE);
    vga_puts_at(BOARD_X+BOARD_W+3, BOARD_Y+1,  "SCORE", A_SCORE);
    char s[12]; itoa(score,s,10);
    vga_puts_at(BOARD_X+BOARD_W+3, BOARD_Y+2, s, VGA_ATTR(COLOR_WHITE,COLOR_BLACK));
    vga_puts_at(BOARD_X+BOARD_W+3, BOARD_Y+4,  "LEN",   A_SCORE);
    itoa(slen,s,10);
    vga_puts_at(BOARD_X+BOARD_W+3, BOARD_Y+5, s, VGA_ATTR(COLOR_WHITE,COLOR_BLACK));

    /* Comida */
    vga_putchar_at(BOARD_X+food.x, BOARD_Y+food.y, 0x04, A_FOOD);

    /* Cobra */
    for(int i=0;i<slen;i++){
        uint8_t a = (i==0) ? A_SNAKE_H : A_SNAKE_B;
        char ch   = (i==0) ? 0x02 : 0xFE;
        vga_putchar_at(BOARD_X+snake[i].x, BOARD_Y+snake[i].y, ch, a);
    }
}

static void game_over_screen(void){
    uint8_t a = VGA_ATTR(COLOR_WHITE,COLOR_RED);
    vga_draw_hline(10,20,40,' ',a);
    vga_draw_hline(11,20,40,' ',a);
    vga_draw_hline(12,20,40,' ',a);
    vga_draw_hline(13,20,40,' ',a);
    vga_puts_at(28,10,"                    ",a);
    vga_puts_at(25,11,"     GAME OVER!     ",a);
    char s[32]; strcpy(s,"     Score: "); char t[12]; itoa(score,t,10);
    strcat(s,t); strcat(s,"        ");
    vga_puts_at(22,12,s,a);
    vga_puts_at(22,13,"  Pressione qualquer tecla  ",a);
    keyboard_getchar();
}

void snake_run(void) {
    /* Inicializa */
    rng_state = pit_ticks() + 12345;
    slen = 3;
    score = 0;
    alive = 1;
    dx=1; dy=0; ndx=1; ndy=0;
    snake[0].x = BOARD_W/2;   snake[0].y = BOARD_H/2;
    snake[1].x = BOARD_W/2-1; snake[1].y = BOARD_H/2;
    snake[2].x = BOARD_W/2-2; snake[2].y = BOARD_H/2;
    place_food();

    uint32_t last_move = pit_ticks();
    int speed = 150; /* ms por movimento */

    while(alive) {
        /* Input não-bloqueante */
        int c = keyboard_poll();
        if(c == KEY_UP    && dy==0){ ndx=0; ndy=-1; }
        if(c == KEY_DOWN  && dy==0){ ndx=0; ndy= 1; }
        if(c == KEY_LEFT  && dx==0){ ndx=-1;ndy= 0; }
        if(c == KEY_RIGHT && dx==0){ ndx= 1;ndy= 0; }
        if(c == 'q' || c == 'Q') break;

        /* Move a cobra no ritmo correto */
        uint32_t now = pit_ticks();
        if(now - last_move < (uint32_t)speed) continue;
        last_move = now;

        /* Aplica nova direção */
        dx = ndx; dy = ndy;

        /* Nova cabeça */
        int nx = snake[0].x + dx;
        int ny = snake[0].y + dy;

        /* Colisão com bordas */
        if(nx<0||nx>=BOARD_W||ny<0||ny>=BOARD_H){ alive=0; break; }

        /* Colisão com o próprio corpo */
        for(int i=1;i<slen;i++)
            if(snake[i].x==nx&&snake[i].y==ny){ alive=0; break; }
        if(!alive) break;

        /* Comeu a comida? */
        int ate = (nx==food.x && ny==food.y);

        /* Move o corpo */
        if(!ate){
            for(int i=slen-1;i>0;i--){
                snake[i].x=snake[i-1].x;
                snake[i].y=snake[i-1].y;
            }
        } else {
            if(slen < MAX_SNAKE){
                for(int i=slen;i>0;i--){
                    snake[i].x=snake[i-1].x;
                    snake[i].y=snake[i-1].y;
                }
                slen++;
            }
            score += 10;
            /* Aumenta velocidade a cada 50 pontos */
            if(speed > 60 && score % 50 == 0) speed -= 10;
            place_food();
        }
        snake[0].x = nx;
        snake[0].y = ny;

        draw_board();
    }

    if(!alive) game_over_screen();
    vga_clear(VGA_ATTR(COLOR_LIGHT_GREY,COLOR_BLACK));
}
