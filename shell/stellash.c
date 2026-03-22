#include "stellash.h"
#include "editor.h"
#include "../fs/diskfs.h"
#include "../kernel/elf_loader.h"
#include "../pkg/spk.h"
#include "../kernel/login.h"
#include "snake.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/pit.h"
#include "../drivers/ata.h"
#include "../kernel/pmm.h"
#include "../kernel/heap.h"
#include "../kernel/scheduler.h"
#include "../libc/string.h"
#include "../fs/ramfs.h"

#define MAX_INPUT 512
#define MAX_ARGS  16
#define HIST_SIZE 20

/* Usuario logado atual */
static char logged_user[32] = "stella";
static char logged_home[64] = "/";

static ramfs_node_t *cwd_node=0;
static char cwd_path[256];
static char hist[HIST_SIZE][MAX_INPUT];
static int hist_n=0,hist_p=0;
static char input[MAX_INPUT];
static int ilen=0;

#define A_PROMPT VGA_ATTR(COLOR_LIGHT_CYAN,COLOR_BLACK)
#define A_AT     VGA_ATTR(COLOR_WHITE,COLOR_BLACK)
#define A_SEP    VGA_ATTR(COLOR_DARK_GREY,COLOR_BLACK)
#define A_CWD    VGA_ATTR(COLOR_LIGHT_GREEN,COLOR_BLACK)
#define A_DOLLAR VGA_ATTR(COLOR_LIGHT_MAGENTA,COLOR_BLACK)
#define A_TEXT   VGA_ATTR(COLOR_LIGHT_GREY,COLOR_BLACK)
#define A_OK     VGA_ATTR(COLOR_LIGHT_GREEN,COLOR_BLACK)
#define A_ERR    VGA_ATTR(COLOR_LIGHT_RED,COLOR_BLACK)
#define A_HDR    VGA_ATTR(COLOR_YELLOW,COLOR_BLACK)
#define A_INFO   VGA_ATTR(COLOR_LIGHT_CYAN,COLOR_BLACK)
#define A_TITLE  VGA_ATTR(COLOR_WHITE,COLOR_BLUE)
#define A_DIM    VGA_ATTR(COLOR_DARK_GREY,COLOR_BLACK)
#define A_DIR    VGA_ATTR(COLOR_LIGHT_BLUE,COLOR_BLACK)
#define A_FILE   VGA_ATTR(COLOR_WHITE,COLOR_BLACK)

static void draw_titlebar(void){
    vga_fill_row(0,' ',A_TITLE);
    vga_puts_at(1,0," StellaresOS",VGA_ATTR(COLOR_YELLOW,COLOR_BLUE));
    vga_puts_at(13,0," v0.1 ",A_TITLE);
    vga_puts_at(19,0,"|",VGA_ATTR(COLOR_DARK_GREY,COLOR_BLUE));
    vga_puts_at(21,0,"Stellash",VGA_ATTR(COLOR_LIGHT_CYAN,COLOR_BLUE));
    vga_puts_at(29,0,"|",VGA_ATTR(COLOR_DARK_GREY,COLOR_BLUE));
    vga_puts_at(31,0,"RamFS",VGA_ATTR(COLOR_LIGHT_GREEN,COLOR_BLUE));
    if(ata_detect()){
        vga_puts_at(36,0,"|",VGA_ATTR(COLOR_DARK_GREY,COLOR_BLUE));
        vga_puts_at(38,0,"ATA",VGA_ATTR(COLOR_YELLOW,COLOR_BLUE));
    }
    char u[16]; itoa((int)pit_seconds(),u,10);
    char ul[24]; strcpy(ul,"uptime: "); strcat(ul,u); strcat(ul,"s ");
    vga_puts_at(80-(int)strlen(ul),0,ul,A_TITLE);
}

static void print_prompt(void){
    draw_titlebar();
    vga_set_attr(A_PROMPT); vga_puts(logged_user);
    vga_set_attr(A_AT);     vga_puts("@");
    vga_set_attr(A_PROMPT); vga_puts("stellaresos");
    vga_set_attr(A_SEP);    vga_puts(":");
    vga_set_attr(A_CWD);    vga_puts(cwd_path);
    vga_set_attr(A_DOLLAR); vga_puts("$ ");
    vga_set_attr(A_TEXT);
}

static void hist_push(const char*l){
    if(!l||!l[0])return;
    strncpy(hist[hist_n%HIST_SIZE],l,MAX_INPUT-1);
    hist_n++;hist_p=hist_n;
}

static void readline(void){
    ilen=0;input[0]=0;hist_p=hist_n;
    while(1){
        int c=keyboard_getchar();
        if(c=='\n'||c=='\r'){vga_putchar('\n');input[ilen]=0;return;}
        if(c=='\b'){if(ilen>0){ilen--;input[ilen]=0;vga_putchar('\b');}continue;}
        if(c==3){vga_puts("^C\n");ilen=0;input[0]=0;return;}
        if(c==12){vga_clear(A_TEXT);draw_titlebar();vga_set_cursor(0,1);print_prompt();ilen=0;continue;}
        if(c==KEY_UP){if(hist_p>0&&hist_n>0){hist_p--;while(ilen-->0)vga_putchar('\b');strncpy(input,hist[hist_p%HIST_SIZE],MAX_INPUT-1);ilen=(int)strlen(input);vga_puts(input);}continue;}
        if(c==KEY_DOWN){while(ilen-->0)vga_putchar('\b');if(hist_p<hist_n-1){hist_p++;strncpy(input,hist[hist_p%HIST_SIZE],MAX_INPUT-1);}else{hist_p=hist_n;input[0]=0;}ilen=(int)strlen(input);vga_puts(input);continue;}
        if(c>=32&&c<256&&ilen<MAX_INPUT-1){input[ilen++]=(char)c;input[ilen]=0;vga_putchar((char)c);}
    }
}

static char*argv[MAX_ARGS];static int argc;
static void parse(char*l){argc=0;char*t=strtok(l," \t");while(t&&argc<MAX_ARGS){argv[argc++]=t;t=strtok(NULL," \t");}argv[argc]=NULL;}

static ramfs_node_t *resolve_arg(const char*arg){
    if(!arg)return 0;
    if(arg[0]=='/')return ramfs_resolve(arg);
    char full[256];
    if(strcmp(cwd_path,"/")==0){strcpy(full,"/");strcat(full,arg);}
    else{strcpy(full,cwd_path);strcat(full,"/");strcat(full,arg);}
    return ramfs_resolve(full);
}

static void print_cmd(const char*name,const char*desc){
    vga_set_attr(A_HDR);vga_puts("    ");vga_puts(name);
    int l=(int)strlen(name);for(int i=l;i<18;i++)vga_putchar(' ');
    vga_set_attr(A_TEXT);vga_puts(desc);vga_putchar('\n');
}

/* ===== COMANDOS ===== */
static void cmd_help(void){
    vga_set_attr(A_HDR);vga_puts("\n  Comandos do StellaresOS:\n\n");
    print_cmd("help",          "exibe esta ajuda");
    print_cmd("clear",         "limpa a tela");
    print_cmd("ls [dir]",      "lista arquivos e dirs");
    print_cmd("cd <dir>",      "muda diretorio");
    print_cmd("pwd",           "diretorio atual");
    print_cmd("cat <f>",       "exibe conteudo de arquivo");
    print_cmd("touch <f>",     "cria arquivo vazio");
    print_cmd("mkdir <d>",     "cria diretorio");
    print_cmd("rm <f>",        "remove arquivo/dir vazio");
    print_cmd("write <f> <t>", "escreve texto em arquivo");
    print_cmd("cp <src> <dst>","copia arquivo");
    print_cmd("mv <src> <dst>","move/renomeia arquivo");
    print_cmd("edit <f>",      "editor de texto (nano-style)");
    print_cmd("echo",          "imprime texto");
    print_cmd("mem",           "uso de memoria");
    print_cmd("uptime",        "tempo ligado");
    print_cmd("uname",         "info do sistema");
    print_cmd("neofetch",      "info estilo Linux");
    print_cmd("ps",            "lista processos");
    print_cmd("kill <pid>",    "termina processo");
    print_cmd("disk",          "info do disco ATA");
    print_cmd("snake",         "jogo Snake!");
    print_cmd("reboot",        "reinicia");
    print_cmd("halt",          "desliga");
    vga_putchar('\n');
}

static void cmd_clear(void){vga_clear(A_TEXT);draw_titlebar();vga_set_cursor(0,1);vga_set_attr(A_TEXT);}

static void cmd_ls(void){
    ramfs_node_t*dir=cwd_node;
    if(argc>1){dir=resolve_arg(argv[1]);if(!dir){vga_set_attr(A_ERR);vga_printf("  ls: '%s': nao encontrado\n",argv[1]);vga_set_attr(A_TEXT);return;}if(dir->type!=NODE_DIR){vga_set_attr(A_ERR);vga_puts("  ls: nao e diretorio\n");vga_set_attr(A_TEXT);return;}}
    vga_putchar('\n');
    int col=0;
    for(int i=0;i<dir->nchildren;i++){
        ramfs_node_t*c=dir->children[i];
        if(c->type==NODE_DIR){vga_set_attr(A_DIR);vga_puts("  ");vga_puts(c->name);vga_puts("/");int l=(int)strlen(c->name)+1;for(int j=l;j<18;j++)vga_putchar(' ');}
        else{vga_set_attr(A_FILE);vga_puts("  ");vga_puts(c->name);int l=(int)strlen(c->name);/* tamanho */char sz[12];itoa((int)c->size,sz,10);vga_set_attr(A_DIM);vga_puts(" (");vga_puts(sz);vga_puts("B)");l+=(int)strlen(sz)+3;for(int j=l;j<18;j++)vga_putchar(' ');}
        col++;if(col>=3){vga_putchar('\n');col=0;}
    }
    if(col)vga_putchar('\n');
    vga_putchar('\n');vga_set_attr(A_TEXT);
}

static void cmd_cd(void){
    if(argc<2){cwd_node=ramfs_root();strcpy(cwd_path,"/");return;}
    ramfs_node_t*target;
    if(strcmp(argv[1],"..")==0){target=(cwd_node->parent&&cwd_node->parent!=cwd_node)?cwd_node->parent:ramfs_root();}
    else target=resolve_arg(argv[1]);
    if(!target){vga_set_attr(A_ERR);vga_printf("  cd: '%s': nao encontrado\n",argv[1]);vga_set_attr(A_TEXT);return;}
    if(target->type!=NODE_DIR){vga_set_attr(A_ERR);vga_puts("  cd: nao e diretorio\n");vga_set_attr(A_TEXT);return;}
    cwd_node=target;ramfs_abs_path(cwd_node,cwd_path);
}

static void cmd_cat(void){
    if(argc<2){vga_set_attr(A_ERR);vga_puts("  cat: nome necessario\n");vga_set_attr(A_TEXT);return;}
    char buf[RAMFS_DATA_MAX+1]; int n=0;
    ramfs_node_t*f=resolve_arg(argv[1]);
    if(f&&f->type==NODE_FILE){
        n=ramfs_read(f,buf,RAMFS_DATA_MAX);
    } else if(diskfs_ready()&&diskfs_exists(argv[1])){
        /* Busca no disco se nao achou no RamFS */
        n=diskfs_read(argv[1],buf,RAMFS_DATA_MAX);
    } else {
        vga_set_attr(A_ERR);vga_printf("  cat: '%s': nao encontrado\n",argv[1]);vga_set_attr(A_TEXT);return;
    }
    if(n>0){buf[n]=0;vga_set_attr(A_TEXT);vga_puts(buf);if(buf[n-1]!='\n')vga_putchar('\n');}
    else{vga_set_attr(A_DIM);vga_puts("  (arquivo vazio)\n");}
    vga_set_attr(A_TEXT);
}

static void cmd_touch(void){
    if(argc<2){vga_set_attr(A_ERR);vga_puts("  touch: nome necessario\n");vga_set_attr(A_TEXT);return;}
    ramfs_node_t*f=ramfs_create(cwd_node,argv[1]);
    if(!f){vga_set_attr(A_ERR);vga_puts("  touch: erro\n");vga_set_attr(A_TEXT);return;}
    if(diskfs_ready()) diskfs_write(argv[1],"",0);
}
static void cmd_mkdir(void){if(argc<2){vga_set_attr(A_ERR);vga_puts("  mkdir: nome necessario\n");vga_set_attr(A_TEXT);return;}ramfs_node_t*d=ramfs_mkdir(cwd_node,argv[1]);if(!d){vga_set_attr(A_ERR);vga_puts("  mkdir: erro (ja existe?)\n");vga_set_attr(A_TEXT);}}
static void cmd_rm(void){
    if(argc<2){vga_set_attr(A_ERR);vga_puts("  rm: nome necessario\n");vga_set_attr(A_TEXT);return;}
    int r=ramfs_delete(cwd_node,argv[1]);
    /* Remove do disco tambem */
    if(diskfs_ready()) diskfs_delete(argv[1]);
    if(r==-1&&!diskfs_exists(argv[1])){
        vga_set_attr(A_ERR);vga_printf("  rm: '%s': nao encontrado\n",argv[1]);vga_set_attr(A_TEXT);
    } else if(r==-2){
        vga_set_attr(A_ERR);vga_puts("  rm: dir nao esta vazio\n");vga_set_attr(A_TEXT);
    }
}

static void cmd_cp(void){
    if(argc<3){vga_set_attr(A_ERR);vga_puts("  cp: uso: cp <src> <dst>\n");vga_set_attr(A_TEXT);return;}
    ramfs_node_t*src=resolve_arg(argv[1]);
    if(!src||src->type!=NODE_FILE){vga_set_attr(A_ERR);vga_printf("  cp: '%s': nao encontrado\n",argv[1]);vga_set_attr(A_TEXT);return;}
    ramfs_node_t*dst=ramfs_create(cwd_node,argv[2]);
    if(!dst){vga_set_attr(A_ERR);vga_puts("  cp: erro ao criar destino\n");vga_set_attr(A_TEXT);return;}
    char buf[RAMFS_DATA_MAX];int n=ramfs_read(src,buf,RAMFS_DATA_MAX);
    ramfs_write(dst,buf,(size_t)n);
    vga_set_attr(A_OK);vga_printf("  '%s' copiado para '%s'\n",argv[1],argv[2]);vga_set_attr(A_TEXT);
}

static void cmd_mv(void){
    if(argc<3){vga_set_attr(A_ERR);vga_puts("  mv: uso: mv <src> <dst>\n");vga_set_attr(A_TEXT);return;}
    ramfs_node_t*src=resolve_arg(argv[1]);
    if(!src){vga_set_attr(A_ERR);vga_printf("  mv: '%s': nao encontrado\n",argv[1]);vga_set_attr(A_TEXT);return;}
    strncpy(src->name,argv[2],RAMFS_NAME_MAX-1);
    vga_set_attr(A_OK);vga_printf("  '%s' renomeado para '%s'\n",argv[1],argv[2]);vga_set_attr(A_TEXT);
}

static void cmd_write(void){
    if(argc<3){vga_set_attr(A_ERR);vga_puts("  write: uso: write <f> <texto>\n");vga_set_attr(A_TEXT);return;}
    ramfs_node_t*f=resolve_arg(argv[1]);if(!f)f=ramfs_create(cwd_node,argv[1]);
    if(!f){vga_set_attr(A_ERR);vga_puts("  write: erro\n");vga_set_attr(A_TEXT);return;}
    char content[RAMFS_DATA_MAX];content[0]=0;
    for(int i=2;i<argc;i++){strcat(content,argv[i]);if(i<argc-1)strcat(content," ");}
    strcat(content,"\n");
    ramfs_write(f,content,strlen(content));
    /* Salva automaticamente no disco */
    if(diskfs_ready()){
        diskfs_write(argv[1],content,strlen(content));
        vga_set_attr(A_OK);
        vga_printf("  '%s' salvo no disco.\n",argv[1]);
    } else {
        vga_set_attr(A_OK);
        vga_printf("  '%s' salvo na RAM.\n",argv[1]);
    }
    vga_set_attr(A_TEXT);
}

static void cmd_mem(void){
    vga_set_attr(A_HDR);vga_puts("\n  Memoria:\n\n");vga_set_attr(A_TEXT);
    vga_printf("    Total:    %u KB\n",pmm_total_kb());
    vga_printf("    Livre:    %u KB\n",pmm_free_kb());
    vga_printf("    Usada:    %u KB\n",pmm_total_kb()-pmm_free_kb());
    vga_printf("    Heap:     %u bytes\n\n",(uint32_t)heap_used());
}


static void cmd_uname(void){vga_set_attr(A_INFO);vga_puts("StellaresOS");vga_set_attr(A_TEXT);vga_puts(" v0.1  i386  Microkernel  Stellash+RamFS  ");vga_set_attr(A_DIM);vga_puts(__DATE__);vga_putchar('\n');}
static void cmd_uptime(void){uint32_t s=pit_seconds();vga_printf("  Uptime: %u:%u:%u\n",s/3600,(s%3600)/60,s%60);}

static void cmd_ps(void){ sched_dump(); }

static void cmd_kill(void){
    if(argc<2){vga_set_attr(A_ERR);vga_puts("  kill: pid necessario\n");vga_set_attr(A_TEXT);return;}
    uint32_t pid=(uint32_t)atoi(argv[1]);
    proc_kill(pid);
    vga_set_attr(A_OK);vga_printf("  Processo %u terminado.\n",pid);vga_set_attr(A_TEXT);
}

static void nf_line(const char*logo,uint8_t la,const char*key,const char*val,uint8_t ka,uint8_t va,uint8_t sa){
    vga_set_attr(la);vga_puts(logo);
    int l=(int)strlen(logo)+2;for(int i=l;i<28;i++)vga_putchar(' ');
    if(key){vga_set_attr(ka);vga_puts(key);vga_set_attr(sa);vga_puts(": ");vga_set_attr(va);vga_puts(val);}
    vga_putchar('\n');
}

static void cmd_neofetch(void){
    uint8_t lo=VGA_ATTR(COLOR_LIGHT_CYAN,COLOR_BLACK);
    uint8_t hi=VGA_ATTR(COLOR_WHITE,COLOR_BLACK);
    uint8_t ka=VGA_ATTR(COLOR_LIGHT_CYAN,COLOR_BLACK);
    uint8_t va=VGA_ATTR(COLOR_LIGHT_GREY,COLOR_BLACK);
    uint8_t sa=VGA_ATTR(COLOR_DARK_GREY,COLOR_BLACK);
    uint8_t bl=A_TEXT;

    char membuf[32];char t[12];
    itoa((int)pmm_free_kb(),t,10);strcpy(membuf,t);strcat(membuf," KB / ");
    itoa((int)pmm_total_kb(),t,10);strcat(membuf,t);strcat(membuf," KB");

    char upbuf[32];uint32_t s=pit_seconds();
    char hh[8],mm[4],ss[4];
    itoa((int)(s/3600),hh,10);itoa((int)((s%3600)/60),mm,10);itoa((int)(s%60),ss,10);
    strcpy(upbuf,hh);strcat(upbuf,"h ");strcat(upbuf,mm);strcat(upbuf,"m ");strcat(upbuf,ss);strcat(upbuf,"s");

    char diskbuf[32];
    if(ata_detect()){char d[12];itoa((int)(ata_sectors()/2048),d,10);strcpy(diskbuf,d);strcat(diskbuf," MB (ATA)");}
    else strcpy(diskbuf,"sem disco");

    char procbuf[8]; itoa(proc_count(),procbuf,10);

    vga_putchar('\n');
    nf_line("  .oPYo.  ooo         ",lo,"stella@stellaresos","",ka,hi,sa);
    nf_line("  8    8  8           ",lo,"----------------------","",sa,sa,sa);
    nf_line("  8       8 .oPYo.    ",lo,"OS",     "StellaresOS v0.1",ka,va,sa);
    nf_line("  8       8 8.   8    ",hi,"Kernel", "Microkernel x86 i486",ka,va,sa);
    nf_line("  8    8  8 8.   8    ",hi,"Shell",  "Stellash v0.1",ka,va,sa);
    nf_line("  `YooP8  8 `YooP'    ",lo,"Arch",   "i386 32-bit prot.",ka,va,sa);
    nf_line("  :....8  . :....     ",lo,"CPU",    "i486 (QEMU)",ka,va,sa);
    nf_line("  ::::..  :  ::::     ",lo,"Memory", membuf,ka,va,sa);
    nf_line("                      ",bl,"Uptime", upbuf,ka,va,sa);
    nf_line("                      ",bl,"Procs",  procbuf,ka,va,sa);
    nf_line("                      ",bl,"FS",     "RamFS (RAM)",ka,va,sa);
    nf_line("                      ",bl,"Disk",   diskbuf,ka,va,sa);
    nf_line("                      ",bl,"Display","VGA 80x25 16 cores",ka,va,sa);
    nf_line("                      ",bl,"Built",  __DATE__,ka,va,sa);
    uint8_t c1[]={VGA_ATTR(COLOR_BLACK,COLOR_BLACK),VGA_ATTR(COLOR_RED,COLOR_RED),VGA_ATTR(COLOR_GREEN,COLOR_GREEN),VGA_ATTR(COLOR_YELLOW,COLOR_YELLOW),VGA_ATTR(COLOR_BLUE,COLOR_BLUE),VGA_ATTR(COLOR_MAGENTA,COLOR_MAGENTA),VGA_ATTR(COLOR_CYAN,COLOR_CYAN),VGA_ATTR(COLOR_WHITE,COLOR_WHITE)};
    uint8_t c2[]={VGA_ATTR(COLOR_DARK_GREY,COLOR_DARK_GREY),VGA_ATTR(COLOR_LIGHT_RED,COLOR_LIGHT_RED),VGA_ATTR(COLOR_LIGHT_GREEN,COLOR_LIGHT_GREEN),VGA_ATTR(COLOR_YELLOW,COLOR_YELLOW),VGA_ATTR(COLOR_LIGHT_BLUE,COLOR_LIGHT_BLUE),VGA_ATTR(COLOR_LIGHT_MAGENTA,COLOR_LIGHT_MAGENTA),VGA_ATTR(COLOR_LIGHT_CYAN,COLOR_LIGHT_CYAN),VGA_ATTR(COLOR_WHITE,COLOR_WHITE)};
    vga_puts("  ");for(int i=0;i<8;i++){vga_set_attr(c1[i]);vga_puts("   ");}
    vga_putchar('\n');vga_puts("  ");for(int i=0;i<8;i++){vga_set_attr(c2[i]);vga_puts("   ");}
    vga_putchar('\n');vga_set_attr(A_TEXT);
}

static void cmd_reboot(void){vga_set_attr(A_ERR);vga_puts("\n  Reiniciando...\n");pit_sleep_ms(500);uint8_t g=0x02;while(g&0x02){uint8_t sv;__asm__ volatile("inb $0x64,%0":"=a"(sv));g=sv;}__asm__ volatile("outb %0,$0x64"::"a"((uint8_t)0xFE));for(;;)__asm__ volatile("hlt");}
static void cmd_halt(void){vga_clear(A_DIM);vga_puts_at(22,12,"Sistema encerrado. Pode desligar.",A_DIM);for(;;)__asm__ volatile("cli;hlt");}

static void cmd_disk(void);
static void cmd_ls_disk(void);
static void setup_user_home(void);

static void execute(char*line){
    while(*line==' '||*line=='\t')line++;
    char*e=line+strlen(line)-1;
    while(e>line&&(*e==' '||*e=='\t'||*e=='\n'))*e--=0;
    if(!*line)return;
    hist_push(line);parse(line);if(!argc)return;

    if     (!strcmp(argv[0],"help"))    cmd_help();
    else if(!strcmp(argv[0],"clear"))   cmd_clear();
    else if(!strcmp(argv[0],"ls"))      { if(argc>1) cmd_ls(); else cmd_ls_disk(); }
    else if(!strcmp(argv[0],"cd"))      cmd_cd();
    else if(!strcmp(argv[0],"pwd"))     {vga_puts(cwd_path);vga_putchar('\n');}
    else if(!strcmp(argv[0],"cat"))     cmd_cat();
    else if(!strcmp(argv[0],"touch"))   cmd_touch();
    else if(!strcmp(argv[0],"mkdir"))   cmd_mkdir();
    else if(!strcmp(argv[0],"rm"))      cmd_rm();
    else if(!strcmp(argv[0],"cp"))      cmd_cp();
    else if(!strcmp(argv[0],"mv"))      cmd_mv();
    else if(!strcmp(argv[0],"write"))   cmd_write();
    else if(!strcmp(argv[0],"edit"))    {if(argc<2){vga_set_attr(A_ERR);vga_puts("  edit: nome necessario\n");vga_set_attr(A_TEXT);}else editor_run(argv[1]);}
    else if(!strcmp(argv[0],"echo"))    {for(int i=1;i<argc;i++){vga_puts(argv[i]);if(i<argc-1)vga_putchar(' ');}vga_putchar('\n');}
    else if(!strcmp(argv[0],"mem"))     cmd_mem();
    else if(!strcmp(argv[0],"disk"))    cmd_disk();
    else if(!strcmp(argv[0],"uname"))   cmd_uname();
    else if(!strcmp(argv[0],"uptime"))  cmd_uptime();
    else if(!strcmp(argv[0],"ps"))      cmd_ps();
    else if(!strcmp(argv[0],"kill"))    cmd_kill();
    else if(!strcmp(argv[0],"neofetch"))cmd_neofetch();
    else if(!strcmp(argv[0],"snake"))   snake_run();
    else if(!strcmp(argv[0],"exec")){
        if(argc<2){vga_set_attr(A_ERR);vga_puts("  exec: nome necessario\n");vga_set_attr(A_TEXT);}
        else{
            elf_result_t r=elf_exec(argv[1],argc-1,argv+1);
            if(r!=ELF_OK){
                vga_set_attr(A_ERR);
                const char*msgs[]={"OK","Nao e ELF","Nao e 32-bit","Nao e executavel","Sem memoria","Arquivo nao encontrado"};
                vga_printf("  exec: erro: %s\n",msgs[r<6?r:5]);
                vga_set_attr(A_TEXT);
            }
        }
    }
    else if(!strcmp(argv[0],"spkg")){
        if(argc<2){
            vga_set_attr(A_HDR);vga_puts("\n  spkg - Gerenciador de pacotes StellaresOS\n\n");
            vga_set_attr(A_TEXT);
            vga_puts("  spkg list              lista pacotes instalados\n");
            vga_puts("  spkg install <arquivo>  instala pacote .spk\n");
            vga_puts("  spkg remove  <nome>     remove pacote\n\n");
        } else if(!strcmp(argv[1],"list")){
            vga_set_attr(A_HDR);vga_puts("\n  Pacotes instalados:\n\n");
            spk_list();
            vga_putchar('\n');
        } else if(!strcmp(argv[1],"install")&&argc>2){
            /* Carrega arquivo .spk do RamFS */
            ramfs_node_t*f=resolve_arg(argv[2]);
            if(!f||f->type!=NODE_FILE){vga_set_attr(A_ERR);vga_printf("  spkg: '%s' nao encontrado\n",argv[2]);vga_set_attr(A_TEXT);}
            else{
                spk_result_t r=spk_install(f->data,f->size);
                if(r!=SPK_OK){vga_set_attr(A_ERR);vga_puts("  spkg: erro ao instalar\n");vga_set_attr(A_TEXT);}
            }
        } else if(!strcmp(argv[1],"remove")&&argc>2){
            spk_remove(argv[2]);
        } else {
            vga_set_attr(A_ERR);vga_puts("  spkg: comando invalido\n");vga_set_attr(A_TEXT);
        }
    }
    else if(!strcmp(argv[0],"reboot"))  cmd_reboot();
    else if(!strcmp(argv[0],"halt"))    cmd_halt();
    else{vga_set_attr(A_ERR);vga_printf("  %s: nao encontrado. Digite 'help'.\n",argv[0]);vga_set_attr(A_TEXT);}
}

static void draw_splash(void){
    uint8_t bg=VGA_ATTR(COLOR_BLACK,COLOR_BLACK);
    uint8_t st=VGA_ATTR(COLOR_LIGHT_CYAN,COLOR_BLACK);
    uint8_t gl=VGA_ATTR(COLOR_WHITE,COLOR_BLACK);
    uint8_t dim=VGA_ATTR(COLOR_DARK_GREY,COLOR_BLACK);
    uint8_t dot=VGA_ATTR(COLOR_YELLOW,COLOR_BLACK);
    uint8_t grn=VGA_ATTR(COLOR_LIGHT_GREEN,COLOR_BLACK);
    uint8_t yel=VGA_ATTR(COLOR_YELLOW,COLOR_BLACK);
    vga_clear(bg);
    const char*stars[]={".","*",".","+","*",".","+"};
    int sx[]={5,15,70,72,60,45,55},sy[]={3,6,2,8,5,3,10};
    for(int i=0;i<7;i++)vga_puts_at(sx[i],sy[i],stars[i%7],dim);
    const char*L[]={
        "   _____ _       _ _                     ___  _____",
        "  / ____| |     | | |                   / _ \\/  ___|",
        " | (___ | |_ ___| | | __ _ _ __ ___  __| | | \\__ \\",
        "  \\___ \\| __/ _ \\ | |/ _` | '__/ _ \\/ __| | | |__) |",
        "  ____) | ||  __/ | | (_| | | |  __/\\__ \\ |_| / __/",
        " |_____/ \\__\\___|_|_|\\__,_|_|  \\___||___/\\___/_____|}",
    };
    uint8_t lc[]={st,st,gl,gl,st,st};
    for(int l=0;l<6;l++){for(int c=0;L[l][c];c++){vga_putchar_at(12+c,5+l,L[l][c],lc[l]);if(c%4==0)pit_sleep_ms(3);}}
    pit_sleep_ms(80);
    vga_puts_at(15,12,"Microkernel x86  |  RamFS  |  Scheduler  |  v0.1",dim);
    vga_puts_at(20,14,"[",VGA_ATTR(COLOR_DARK_GREY,COLOR_BLACK));
    vga_puts_at(61,14,"]",VGA_ATTR(COLOR_DARK_GREY,COLOR_BLACK));
    for(int i=0;i<40;i++){
        uint8_t ba=(i<13)?VGA_ATTR(COLOR_LIGHT_RED,COLOR_BLACK):(i<26)?VGA_ATTR(COLOR_YELLOW,COLOR_BLACK):VGA_ATTR(COLOR_LIGHT_GREEN,COLOR_BLACK);
        vga_putchar_at(21+i,14,(char)0xDB,ba);
        pit_sleep_ms(15);
    }
    const char*msgs[]={"Inicializando GDT, IDT, PIC...","Configurando memoria + heap...","Iniciando scheduler preemptivo...","Montando RamFS...","Iniciando Stellash..."};
    uint8_t mc[]={grn,grn,yel,grn,yel};
    for(int i=0;i<5;i++){vga_puts_at(22,16+i,msgs[i],mc[i]);pit_sleep_ms(100);}
    pit_sleep_ms(200);
}

void stellash_run(void){
    ramfs_init();
    cwd_node=ramfs_root();
    strcpy(cwd_path,"/");

    /* Cria estrutura de diretorios padrao */
    ramfs_mkdir(ramfs_root(),"home");
    ramfs_mkdir(ramfs_root(),"tmp");
    ramfs_mkdir(ramfs_root(),"etc");
    ramfs_mkdir(ramfs_root(),"bin");

    /* Configura home e navega para la */
    setup_user_home();
    draw_splash();
    vga_clear(A_TEXT);draw_titlebar();vga_set_cursor(0,1);
    vga_set_attr(A_HDR);
    vga_puts("\n  Bem-vindo ao StellaresOS! Digite 'help'.\n");
    vga_puts("  Novidades: ");
    vga_set_attr(A_INFO);vga_puts("edit");vga_set_attr(A_HDR);vga_puts(" (editor), ");
    vga_set_attr(A_INFO);vga_puts("snake");vga_set_attr(A_HDR);vga_puts(" (jogo), ");
    vga_set_attr(A_INFO);vga_puts("ps");vga_set_attr(A_HDR);vga_puts(" (processos reais), ");
    vga_set_attr(A_INFO);vga_puts("disk");vga_set_attr(A_HDR);vga_puts(" (ATA).\n\n");
    vga_set_attr(A_TEXT);
    while(1){print_prompt();readline();if(input[0])execute(input);}
}

/* ---- Ponto de entrada com usuário logado ---- */
void stellash_run_as(user_t *user) {
    if(user) {
        strncpy(logged_user, user->username, 31);
        strncpy(logged_home,  user->home,     63);
    }
    stellash_run();
}

/* Chamado DEPOIS do ramfs_init, configura home do usuario */
static void setup_user_home(void) {
    if(strcmp(logged_home,"/")==0) return; /* root fica em / */

    /* Garante que /home existe */
    ramfs_node_t *home_dir = ramfs_find(ramfs_root(),"home");
    if(!home_dir) home_dir = ramfs_mkdir(ramfs_root(),"home");
    if(!home_dir) return;

    /* Cria /home/usuario se nao existir */
    ramfs_node_t *user_dir = ramfs_find(home_dir, logged_user);
    if(!user_dir) user_dir = ramfs_mkdir(home_dir, logged_user);
    if(!user_dir) return;

    /* Pastas padrao do home (igual ao Linux) */
    const char *xdg_dirs[] = {
        "Documentos", "Downloads", "Imagens",
        "Musicas",    "Videos",    "Desktop",
        "Projetos",   ".config",   NULL
    };
    for(int i=0; xdg_dirs[i]; i++){
        if(!ramfs_find(user_dir, xdg_dirs[i]))
            ramfs_mkdir(user_dir, xdg_dirs[i]);
    }

    /* Carrega arquivos do disco para a pasta home */
    if(diskfs_ready()) {
        /* README */
        char buf[RAMFS_DATA_MAX];
        int n = diskfs_read("README", buf, RAMFS_DATA_MAX-1);
        if(n > 0) {
            buf[n]=0;
            ramfs_node_t *f = ramfs_create(user_dir,"README.txt");
            if(f) ramfs_write(f,buf,(size_t)n);
        }
        /* Carrega outros arquivos do disco para a home */
        char names[DISKFS_MAX_FILES][DISKFS_NAME_MAX];
        uint32_t sizes[DISKFS_MAX_FILES];
        int count=0;
        diskfs_list(names,sizes,&count);
        for(int i=0;i<count;i++){
            /* Ignora arquivos do sistema */
            if(strcmp(names[i],"passwd")==0) continue;
            if(strcmp(names[i],"installed")==0) continue;
            if(strcmp(names[i],"homepath")==0) continue;
            if(strcmp(names[i],"README")==0) continue;
            /* Carrega arquivo do usuario */
            int nn=diskfs_read(names[i],buf,RAMFS_DATA_MAX-1);
            if(nn>0){
                buf[nn]=0;
                ramfs_node_t *f=ramfs_find(user_dir,names[i]);
                if(!f) f=ramfs_create(user_dir,names[i]);
                if(f) ramfs_write(f,buf,(size_t)nn);
            }
        }
    }

    /* Navega para /home/usuario */
    cwd_node = user_dir;
    strcpy(cwd_path, "/home/");
    strcat(cwd_path, logged_user);
}

/* ============================================================
 *  Comandos de disco — funcionam em paralelo com RamFS
 *  quando o disco está disponível
 * ============================================================ */

static void cmd_disk(void) {
    vga_set_attr(A_HDR); vga_puts("\n  Disco ATA:\n\n");
    vga_set_attr(A_TEXT);
    if(!ata_detect()){
        vga_set_attr(A_ERR);
        vga_puts("    Sem disco. Use: make run-disk\n\n");
        vga_set_attr(A_TEXT); return;
    }
    vga_printf("    Modelo:  %s\n", ata_model());
    vga_printf("    Setores: %u\n", ata_sectors());
    vga_printf("    Tamanho: %u MB\n", ata_sectors()/2048);
    if(diskfs_ready()){
        char names[DISKFS_MAX_FILES][DISKFS_NAME_MAX];
        uint32_t sizes[DISKFS_MAX_FILES];
        int count=0;
        diskfs_list(names, sizes, &count);
        vga_printf("    Arquivos no disco: %d\n\n", count);
    } else {
        vga_puts("    DiskFS: nao montado\n\n");
    }
}

/* ls com disco: mostra RamFS + arquivos do disco */
static void cmd_ls_disk(void) {
    /* Primeiro mostra o conteúdo normal do RamFS */
    ramfs_node_t *dir = cwd_node;
    vga_putchar('\n');

    /* Se estiver em / e disco disponível, mostra arquivos do disco também */
    if(diskfs_ready() && (strcmp(cwd_path,"/")==0 || strcmp(cwd_path,"/disk")==0)){
        vga_set_attr(VGA_ATTR(COLOR_DARK_GREY,COLOR_BLACK));
        vga_puts("  [disco] ");
        vga_set_attr(A_TEXT);

        char names[DISKFS_MAX_FILES][DISKFS_NAME_MAX];
        uint32_t sizes[DISKFS_MAX_FILES];
        int count=0;
        diskfs_list(names, sizes, &count);
        int col=0;
        for(int i=0;i<count;i++){
            vga_set_attr(A_FILE);
            vga_puts("  "); vga_puts(names[i]);
            vga_set_attr(A_DIM);
            char sz[12]; itoa((int)sizes[i],sz,10);
            vga_puts("("); vga_puts(sz); vga_puts("B)");
            int l=(int)strlen(names[i])+(int)strlen(sz)+3;
            for(int j=l;j<18;j++) vga_putchar(' ');
            col++;
            if(col>=3){ vga_putchar('\n'); col=0; }
        }
        if(col) vga_putchar('\n');
        if(count>0){
            vga_draw_hline(0,0,0,'-',A_DIM);
            vga_puts("\n");
        }
    }

    /* RamFS normal */
    int col=0;
    for(int i=0;i<dir->nchildren;i++){
        ramfs_node_t *c=dir->children[i];
        if(c->type==NODE_DIR){
            vga_set_attr(A_DIR);
            vga_puts("  "); vga_puts(c->name); vga_puts("/");
            int l=(int)strlen(c->name)+1; for(int j=l;j<18;j++) vga_putchar(' ');
        } else {
            vga_set_attr(A_FILE);
            vga_puts("  "); vga_puts(c->name);
            char sz[12]; itoa((int)c->size,sz,10);
            vga_set_attr(A_DIM); vga_puts("("); vga_puts(sz); vga_puts("B)");
            int l=(int)strlen(c->name)+(int)strlen(sz)+3;
            for(int j=l;j<18;j++) vga_putchar(' ');
        }
        col++; if(col>=3){ vga_putchar('\n'); col=0; }
    }
    if(col) vga_putchar('\n');
    vga_putchar('\n'); vga_set_attr(A_TEXT);
}
