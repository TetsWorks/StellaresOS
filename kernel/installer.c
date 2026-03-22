/* ============================================================
 *  StellaresOS -- kernel/installer.c
 *  Assistente de instalacao - layout testado e alinhado
 * ============================================================ */
#include "installer.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/pit.h"
#include "../fs/diskfs.h"
#include "../libc/string.h"

#define INSTALL_FLAG "installed"

/* ---- Paleta de cores ---- */
#define C_BG    VGA_ATTR(COLOR_BLACK,       COLOR_BLACK)
#define C_BOX   VGA_ATTR(COLOR_WHITE,       COLOR_BLUE)
#define C_TTL   VGA_ATTR(COLOR_YELLOW,      COLOR_BLUE)
#define C_LBL   VGA_ATTR(COLOR_LIGHT_CYAN,  COLOR_BLUE)
#define C_TXT   VGA_ATTR(COLOR_LIGHT_GREY,  COLOR_BLUE)
#define C_HINT  VGA_ATTR(COLOR_DARK_GREY,   COLOR_BLUE)
#define C_INP   VGA_ATTR(COLOR_BLACK,       COLOR_CYAN)
#define C_VAL   VGA_ATTR(COLOR_WHITE,       COLOR_BLUE)
#define C_OK    VGA_ATTR(COLOR_LIGHT_GREEN, COLOR_BLUE)
#define C_ERR   VGA_ATTR(COLOR_LIGHT_RED,   COLOR_BLUE)
#define C_DIM   VGA_ATTR(COLOR_DARK_GREY,   COLOR_BLACK)
#define C_STAR  VGA_ATTR(COLOR_LIGHT_CYAN,  COLOR_BLACK)
#define C_BTN   VGA_ATTR(COLOR_BLACK,       COLOR_CYAN)

/* ---- Utilitários ---- */
static void fill_row_attr(int row, uint8_t a){
    for(int i=0;i<80;i++) vga_putchar_at(i,row,' ',a);
}

static void draw_box(int x,int y,int w,int h,uint8_t a,const char*title){
    /* Preenche interior */
    for(int r=y;r<y+h;r++)
        for(int c=x;c<x+w;c++)
            vga_putchar_at(c,r,' ',a);
    /* Bordas VGA box-drawing */
    vga_putchar_at(x,     y,     0xC9,a);
    vga_putchar_at(x+w-1, y,     0xBB,a);
    vga_putchar_at(x,     y+h-1, 0xC8,a);
    vga_putchar_at(x+w-1, y+h-1, 0xBC,a);
    for(int c=x+1;c<x+w-1;c++){
        vga_putchar_at(c,y,     0xCD,a);
        vga_putchar_at(c,y+h-1,0xCD,a);
    }
    for(int r=y+1;r<y+h-1;r++){
        vga_putchar_at(x,     r,0xBA,a);
        vga_putchar_at(x+w-1, r,0xBA,a);
    }
    /* Título centralizado */
    if(title && title[0]){
        int tl=(int)strlen(title);
        int tx=x+(w-tl)/2;
        vga_puts_at(tx,y,title,C_TTL);
    }
}

static void draw_field(int x,int y,int w,const char*val,int hide,int active){
    uint8_t a = active ? C_INP : VGA_ATTR(COLOR_WHITE,COLOR_BLUE);
    for(int i=0;i<w;i++) vga_putchar_at(x+i,y,' ',a);
    int l=(int)strlen(val);
    for(int i=0;i<l&&i<w;i++)
        vga_putchar_at(x+i,y,hide?'*':val[i],a);
    if(active) vga_set_cursor(x+l,y);
}

static void draw_sep(int x,int y,int w,uint8_t a){
    for(int i=0;i<w;i++) vga_putchar_at(x+i,y,0xC4,a);
}

/* Lê campo com edição em tempo real */
static int read_field(int x,int y,int w,char*buf,int maxlen,int hide){
    int len=(int)strlen(buf);
    draw_field(x,y,w,buf,hide,1);

    while(1){
        int c=keyboard_getchar();
        if(c=='\n'||c=='\r'){ buf[len]=0; draw_field(x,y,w,buf,hide,0); return len; }
        if(c==27)            { draw_field(x,y,w,buf,hide,0); return -1; }
        if(c=='\b'){
            if(len>0){
                len--;
                vga_putchar_at(x+len,y,' ',C_INP);
                buf[len]=0;
            }
            vga_set_cursor(x+len,y);
            continue;
        }
        if(c>=32&&c<127&&len<maxlen-1){
            buf[len]=(char)c; len++;
            vga_putchar_at(x+len-1,y,hide?'*':(char)c,C_INP);
            vga_set_cursor(x+len,y);
        }
    }
}

/* Mensagem de erro dentro da caixa */
static void show_err(int x,int y,int w,const char*msg){
    for(int i=0;i<w;i++) vga_putchar_at(x+i,y,' ',C_ERR);
    vga_puts_at(x+1,y,msg,C_ERR);
    pit_sleep_ms(1600);
    for(int i=0;i<w;i++) vga_putchar_at(x+i,y,' ',C_BOX);
}

/* Hash simples de senha */
static uint32_t hash_str(const char*s){
    uint32_t h=5381;
    while(*s) h=h*33+(uint8_t)*s++;
    return h;
}

static void u32toa(uint32_t v,char*b){
    char t[12];int i=0;
    if(!v){b[0]='0';b[1]=0;return;}
    while(v){t[i++]='0'+v%10;v/=10;}
    int j=0;while(i--)b[j++]=t[i];b[j]=0;
}

/* ==============================================================
 *  TELA 1: Boas-vindas
 * ============================================================== */
static void screen_welcome(void){
    vga_clear(C_BG);

    /* Estrelinhas de fundo */
    int sx[]={3,15,70,72,60,45,8,30,55,5,40,68};
    int sy[]={1,3, 1, 5, 2, 1, 6, 4, 3,10,8,11};
    for(int i=0;i<12;i++) vga_putchar_at(sx[i],sy[i],'.',C_DIM);

    /* Logo */
    uint8_t lc=VGA_ATTR(COLOR_LIGHT_CYAN,COLOR_BLACK);
    uint8_t lh=VGA_ATTR(COLOR_WHITE,COLOR_BLACK);
    uint8_t ld=VGA_ATTR(COLOR_DARK_GREY,COLOR_BLACK);
    /* Logo em ASCII puro (17 = offset para centralizar em 80 colunas) */
    vga_puts_at(17,4," ___  _       _ _                  ___  ____",lc);
    vga_puts_at(17,5,"/ __|| |_ ___| | | __ _ _ __ ___  / _ \/  _/",lc);
    vga_puts_at(17,6,"\\__ \\|  _/ -_) | |/ _` | '__/ -_)| (_) |> <",lh);
    vga_puts_at(17,7,"|___/ \\__\\___|_|_|\\__,_|_|  \\___| \\___//_/\\_\\",lh);
    vga_puts_at(26,8,"v 0 . 1   -   M i c r o k e r n e l",ld);

    /* Caixa de info: x=14, y=10, w=52, h=9 */
    draw_box(14,10,52,9,C_BOX," Bem-vindo ao Instalador ");

    vga_puts_at(16,12,"Este assistente vai configurar o sistema.",C_TXT);
    vga_puts_at(16,13,"Vai levar menos de 1 minuto.",C_HINT);
    vga_puts_at(16,15,"O que sera configurado:",C_LBL);
    vga_puts_at(18,16,"* Nome do sistema (hostname)",C_TXT);
    vga_puts_at(18,17,"* Usuario e senha de acesso",C_TXT);

    vga_puts_at(27,20,"[ ENTER para comecar ]",C_BTN);
    keyboard_getchar();
}

/* ==============================================================
 *  TELA 2: Hostname
 * ============================================================== */
static void screen_hostname(install_config_t*cfg){
    /* Caixa: x=10, y=3, w=60, h=16 */
    int bx=10,by=3,bw=60,bh=16;

    vga_clear(C_BG);
    draw_box(bx,by,bw,bh,C_BOX," StellaresOS - Instalacao ");
    vga_puts_at(bx+2,by+1,"Passo 1 de 2: Nome do sistema",C_LBL);
    draw_sep(bx+1,by+2,bw-2,C_BOX);

    vga_puts_at(bx+2,by+4,"Nome do sistema (hostname):",C_LBL);
    vga_puts_at(bx+2,by+5,"Identifica seu computador. Use letras e numeros.",C_HINT);

    /* Campo: x=bx+2, y=by+7, w=bw-6=54 */
    int fx=bx+2, fy=by+7, fw=bw-6;
    draw_sep(bx+1,by+6,bw-2,C_BOX);

    vga_puts_at(bx+2,by+9, "Exemplos: stellares  meupc  notebook  casa",C_HINT);
    draw_sep(bx+1,by+11,bw-2,C_BOX);
    vga_puts_at(bx+2,by+12,"Deixe vazio e pressione ENTER para usar",C_HINT);
    vga_puts_at(bx+2,by+13,"o nome padrao: stellares",C_HINT);

    draw_sep(bx+1,by+bh-2,bw-2,C_BOX);
    vga_puts_at(bx+2,by+bh-1,"ENTER = continuar",C_BTN);

    while(1){
        /* Mostra valor atual */
        draw_field(fx,fy,fw,cfg->hostname,0,1);

        int r=read_field(fx,fy,fw,cfg->hostname,30,0);
        (void)r;

        /* Valor padrão se vazio */
        if(strlen(cfg->hostname)==0) strcpy(cfg->hostname,"stellares");

        /* Valida */
        int ok=1;
        for(int i=0;cfg->hostname[i];i++){
            char c=cfg->hostname[i];
            if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'))
                { ok=0; break; }
        }
        if(ok) return;
        show_err(bx+2,by+10,bw-4,"  Invalido! Use apenas letras, numeros e hifens.");
    }
}

/* ==============================================================
 *  TELA 3: Usuário e Senha
 * ============================================================== */
static void screen_user(install_config_t*cfg){
    int bx=10,by=2,bw=60,bh=20;

    vga_clear(C_BG);
    draw_box(bx,by,bw,bh,C_BOX," StellaresOS - Instalacao ");
    vga_puts_at(bx+2,by+1,"Passo 2 de 2: Criar usuario",C_LBL);
    draw_sep(bx+1,by+2,bw-2,C_BOX);

    /* Usuário */
    vga_puts_at(bx+2,by+4,"Nome de usuario:",C_LBL);
    vga_puts_at(bx+2,by+5,"Use apenas letras minusculas e numeros.",C_HINT);
    int ux=bx+2,uy=by+6,uw=bw-6;
    draw_sep(bx+1,by+8,bw-2,C_BOX);

    /* Senha */
    vga_puts_at(bx+2,by+10,"Senha:",C_LBL);
    vga_puts_at(bx+2,by+11,"Minimo 4 caracteres.",C_HINT);
    int px=bx+2,py=by+12,pw=bw-6;
    draw_sep(bx+1,by+14,bw-2,C_BOX);

    /* Confirma */
    vga_puts_at(bx+2,by+16,"Confirmar senha:",C_LBL);
    int cx2=bx+2,cy2=by+17,cw2=bw-6;
    draw_sep(bx+1,by+bh-2,bw-2,C_BOX);
    vga_puts_at(bx+2,by+bh-1,"ENTER = continuar  |  ESC = voltar",C_BTN);

    char pass2[64];

    /* Loop de validação do usuário */
    while(1){
        draw_field(ux,uy,uw,cfg->username,0,1);
        int r=read_field(ux,uy,uw,cfg->username,30,0);
        if(r<0) strcpy(cfg->username,"stella");
        if(strlen(cfg->username)==0){ strcpy(cfg->username,"stella"); }
        int ok=1;
        for(int i=0;cfg->username[i];i++){
            char c=cfg->username[i];
            if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')){ ok=0; break; }
        }
        if(ok) break;
        show_err(bx+2,by+9,bw-4,"  Invalido! Use apenas letras minusculas e numeros.");
    }

    /* Loop de validação da senha */
    while(1){
        strcpy(cfg->password,"");
        strcpy(pass2,"");

        draw_field(px,py,pw,cfg->password,1,1);
        read_field(px,py,pw,cfg->password,62,1);

        draw_field(cx2,cy2,cw2,pass2,1,1);
        read_field(cx2,cy2,cw2,pass2,62,1);

        if(strlen(cfg->password)<4){
            show_err(bx+2,by+15,bw-4,"  Senha muito curta! Minimo 4 caracteres.");
            continue;
        }
        if(strcmp(cfg->password,pass2)==0) break;
        show_err(bx+2,by+15,bw-4,"  Senhas nao conferem! Tente novamente.");
    }
}

/* ==============================================================
 *  TELA 4: Confirmação
 * ============================================================== */
static int screen_confirm(install_config_t*cfg){
    int bx=12,by=3,bw=56,bh=17;

    vga_clear(C_BG);
    draw_box(bx,by,bw,bh,C_BOX," Confirmar instalacao ");
    vga_puts_at(bx+2,by+1,"Revise as configuracoes abaixo:",C_LBL);
    draw_sep(bx+1,by+2,bw-2,C_BOX);

    /* Tabela de valores */
    vga_puts_at(bx+4, by+4, "Hostname :",C_LBL);
    vga_puts_at(bx+16,by+4, cfg->hostname,C_VAL);

    vga_puts_at(bx+4, by+5, "Usuario  :",C_LBL);
    vga_puts_at(bx+16,by+5, cfg->username,C_VAL);

    vga_puts_at(bx+4, by+6, "Senha    :",C_LBL);
    vga_puts_at(bx+16,by+6, "**********",C_VAL);

    char home[72]; strcpy(home,"/home/"); strcat(home,cfg->username);
    vga_puts_at(bx+4, by+7, "Home     :",C_LBL);
    vga_puts_at(bx+16,by+7, home,C_VAL);

    draw_sep(bx+1,by+9,bw-2,C_BOX);
    vga_puts_at(bx+2,by+11,"Tudo certo? O sistema sera instalado agora.",C_TXT);
    vga_puts_at(bx+2,by+12,"Os dados serao gravados no disco.",C_HINT);

    draw_sep(bx+1,by+14,bw-2,C_BOX);
    vga_puts_at(bx+4, by+15,"[ ENTER = Instalar ]",C_BTN);
    vga_puts_at(bx+28,by+15,"[ ESC = Corrigir ]",
        VGA_ATTR(COLOR_DARK_GREY,COLOR_BLUE));

    while(1){
        int c=keyboard_getchar();
        if(c=='\n'||c=='\r') return 1;
        if(c==27) return 0;
    }
}

/* ==============================================================
 *  TELA 5: Instalando (progresso)
 * ============================================================== */
static void screen_installing(install_config_t*cfg){
    int bx=12,by=5,bw=56,bh=13;

    vga_clear(C_BG);
    draw_box(bx,by,bw,bh,C_BOX," Instalando StellaresOS ");
    vga_puts_at(bx+2,by+2,"Configurando o sistema, aguarde...",C_TXT);
    draw_sep(bx+1,by+3,bw-2,C_BOX);

    struct { const char*msg; int pct; int ms; } steps[]={
        {"Formatando DiskFS...",          10, 200},
        {"Criando estrutura inicial...",   25, 150},
        {"Salvando configuracoes...",      40, 200},
        {"Criando usuario...",             60, 250},
        {"Gravando senha...",              75, 200},
        {"Criando arquivos de boas-vinda.",90, 300},
        {"Finalizando...",               100, 400},
        {NULL,0,0}
    };

    int msgrow = by+5;
    int barx   = bx+2;
    int barw   = bw-6;
    int bary   = by+7;

    for(int i=0; steps[i].msg; i++){
        /* Limpa linha de mensagem */
        for(int c=0;c<bw-4;c++) vga_putchar_at(bx+2+c,msgrow,' ',C_BOX);
        vga_puts_at(bx+2,msgrow,steps[i].msg,
            VGA_ATTR(COLOR_YELLOW,COLOR_BLUE));

        /* Barra de progresso */
        int filled=(barw*steps[i].pct)/100;
        for(int j=0;j<barw;j++){
            uint8_t ba=(j<filled)
                ?((steps[i].pct<40)?VGA_ATTR(COLOR_LIGHT_RED,COLOR_BLACK)
                 :(steps[i].pct<70)?VGA_ATTR(COLOR_YELLOW,COLOR_BLACK)
                 :VGA_ATTR(COLOR_LIGHT_GREEN,COLOR_BLACK))
                :VGA_ATTR(COLOR_DARK_GREY,COLOR_BLACK);
            vga_putchar_at(barx+j,bary,j<filled?0xDB:0xB0,ba);
        }
        /* Porcentagem */
        char p[6]; itoa(steps[i].pct,p,10); strcat(p,"%");
        for(int c=0;c<5;c++) vga_putchar_at(barx+barw+1+c,bary,' ',C_BOX);
        vga_puts_at(barx+barw+1,bary,p,C_VAL);

        pit_sleep_ms(steps[i].ms);
    }

    /* --- Grava no disco --- */
    diskfs_format();

    /* Flag de instalação */
    char flag[64]; strcpy(flag,"hostname="); strcat(flag,cfg->hostname); strcat(flag,"\n");
    diskfs_write(INSTALL_FLAG,flag,strlen(flag));

    /* passwd: root:hash:/ e usuario:hash:/home/user */
    char pbuf[512]="";
    char h1[12],h2[12];
    u32toa(hash_str("toor"),h1);
    u32toa(hash_str(cfg->password),h2);
    strcat(pbuf,"root:"); strcat(pbuf,h1); strcat(pbuf,":/\n");
    strcat(pbuf,cfg->username); strcat(pbuf,":");
    strcat(pbuf,h2); strcat(pbuf,":/home/");
    strcat(pbuf,cfg->username); strcat(pbuf,"\n");
    diskfs_write("passwd",pbuf,strlen(pbuf));

    /* README no disco */
    char readme[256];
    strcpy(readme,"Bem-vindo ao StellaresOS, ");
    strcat(readme,cfg->username); strcat(readme,"!\n\n");
    strcat(readme,"Seus arquivos sao salvos no disco automaticamente.\n");
    strcat(readme,"Digite 'help' para ver os comandos disponiveis.\n");
    diskfs_write("README",readme,strlen(readme));

    /* Salva o home path no disco para o login saber */
    char homepath[80];
    strcpy(homepath,"/home/"); strcat(homepath,cfg->username); strcat(homepath,"\n");
    diskfs_write("homepath",homepath,strlen(homepath));

    /* Mensagem final */
    for(int c=0;c<bw-4;c++) vga_putchar_at(bx+2+c,msgrow,' ',C_BOX);
    vga_puts_at(bx+2,msgrow,"Instalacao concluida com sucesso!",C_OK);
    draw_sep(bx+1,by+bh-3,bw-2,C_BOX);
    vga_puts_at(bx+2,by+bh-2,"Iniciando o sistema...",C_LBL);
    pit_sleep_ms(1200);
}

/* ==============================================================
 *  Entry point
 * ============================================================== */
int installer_run(install_config_t*cfg){
    if(!diskfs_ready()) return -1;
    if(diskfs_exists(INSTALL_FLAG)) return -1;

    /* Valores padrão */
    strcpy(cfg->hostname,"stellares");
    strcpy(cfg->username,"");
    strcpy(cfg->password,"");

    screen_welcome();

    /* Loop até confirmação */
    while(1){
        screen_hostname(cfg);
        screen_user(cfg);
        if(screen_confirm(cfg)) break;
    }

    screen_installing(cfg);
    return 0;
}
