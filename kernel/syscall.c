/* ============================================================
 *  StellaresOS -- kernel/syscall.c
 *  Implementação das syscalls Linux i386
 *
 *  ABI: INT 0x80
 *    eax = número da syscall
 *    ebx = arg1, ecx = arg2, edx = arg3
 *    Retorno: eax (negativo = errno)
 * ============================================================ */
#include "syscall.h"
#include "scheduler.h"
#include "idt.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../fs/ramfs.h"
#include "../fs/diskfs.h"
#include "../libc/string.h"

/* ---- Descritores de arquivo por processo ---- */
#define MAX_FDS 16

typedef struct {
    int        used;
    int        flags;
    uint32_t   offset;
    ramfs_node_t *node;  /* NULL = especial (stdout/stdin) */
    int        is_stdout;
    int        is_stdin;
} fd_t;

/* Tabela global de FDs (simplificado — sem per-process ainda) */
static fd_t fds[MAX_FDS];

static void fd_init(void) {
    memset(fds, 0, sizeof(fds));
    /* fd 0 = stdin */
    fds[0].used=1; fds[0].is_stdin=1;
    /* fd 1 = stdout */
    fds[1].used=1; fds[1].is_stdout=1;
    /* fd 2 = stderr (vai para stdout também) */
    fds[2].used=1; fds[2].is_stdout=1;
}

static int fd_alloc(void) {
    for(int i=3;i<MAX_FDS;i++)
        if(!fds[i].used) return i;
    return -1;
}

/* ---- Implementações das syscalls ---- */

static uint32_t sys_exit(uint32_t code) {
    serial_printf("[syscall] exit(%u)\n", code);
    proc_exit();
    return 0;
}

static uint32_t sys_write(uint32_t fd, uint32_t buf_addr, uint32_t count) {
    const char *buf = (const char *)buf_addr;
    if(!buf || count == 0) return 0;

    if(fd == 1 || fd == 2) {
        /* stdout/stderr: escreve na VGA e serial */
        for(uint32_t i=0; i<count; i++) {
            vga_putchar(buf[i]);
            serial_putchar(buf[i]);
        }
        return count;
    }

    if(fd < MAX_FDS && fds[fd].used && fds[fd].node) {
        /* Escreve no arquivo RamFS */
        ramfs_node_t *f = fds[fd].node;
        uint32_t space = RAMFS_DATA_MAX - fds[fd].offset;
        uint32_t n = count < space ? count : space;
        memcpy(f->data + fds[fd].offset, buf, n);
        fds[fd].offset += n;
        if(fds[fd].offset > f->size) f->size = fds[fd].offset;
        /* Sincroniza com disco se disponível */
        if(diskfs_ready())
            diskfs_write(f->name, (char*)f->data, f->size);
        return n;
    }
    return (uint32_t)-9; /* EBADF */
}

static uint32_t sys_read(uint32_t fd, uint32_t buf_addr, uint32_t count) {
    char *buf = (char *)buf_addr;
    if(!buf || count == 0) return 0;

    if(fd == 0) {
        /* stdin: lê do teclado */
        extern int keyboard_getchar(void);
        uint32_t i = 0;
        while(i < count) {
            int c = keyboard_getchar();
            buf[i++] = (char)c;
            if(c == '\n') break;
        }
        return i;
    }

    if(fd < MAX_FDS && fds[fd].used && fds[fd].node) {
        ramfs_node_t *f = fds[fd].node;
        uint32_t avail = f->size - fds[fd].offset;
        uint32_t n = count < avail ? count : avail;
        memcpy(buf, f->data + fds[fd].offset, n);
        fds[fd].offset += n;
        return n;
    }
    return 0;
}

static uint32_t sys_open(uint32_t path_addr, uint32_t flags, uint32_t mode) {
    (void)mode;
    const char *path = (const char *)path_addr;
    if(!path) return (uint32_t)-2; /* ENOENT */

    int fd = fd_alloc();
    if(fd < 0) return (uint32_t)-24; /* EMFILE */

    /* Resolve o caminho no RamFS */
    ramfs_node_t *node = ramfs_resolve(path);

    if(!node && (flags & O_CREAT)) {
        /* Cria o arquivo */
        char dir[256], base[64];
        /* Pega o diretório pai */
        const char *last = path;
        for(const char *p=path; *p; p++) if(*p=='/') last=p+1;
        int dirlen = (int)(last - path);
        if(dirlen == 0) { strcpy(dir,"/"); }
        else { strncpy(dir,path,dirlen); dir[dirlen]=0; }
        strcpy(base,last);

        ramfs_node_t *parent = ramfs_resolve(dir);
        if(!parent) parent = ramfs_root();
        node = ramfs_create(parent, base);
    }

    if(!node) return (uint32_t)-2; /* ENOENT */

    fds[fd].used   = 1;
    fds[fd].flags  = (int)flags;
    fds[fd].offset = (flags & O_APPEND) ? node->size : 0;
    fds[fd].node   = node;
    if(flags & O_TRUNC) node->size = 0;

    return (uint32_t)fd;
}

static uint32_t sys_close(uint32_t fd) {
    if(fd < 3 || fd >= MAX_FDS) return 0;
    if(fds[fd].used) {
        fds[fd].used = 0;
        fds[fd].node = NULL;
    }
    return 0;
}

static uint32_t sys_getpid(void) {
    process_t *p = proc_current();
    return p ? p->pid : 1;
}

static uint32_t sys_brk(uint32_t addr) {
    /* Heap bump simples — retorna addr se válido */
    static uint32_t heap_end = 0x01000000; /* 16MB */
    if(addr == 0) return heap_end;
    if(addr > heap_end) heap_end = addr;
    return heap_end;
}

static uint32_t sys_uname(uint32_t buf_addr) {
    utsname_t *u = (utsname_t *)buf_addr;
    if(!u) return (uint32_t)-14; /* EFAULT */
    strcpy(u->sysname,  "StellaresOS");
    strcpy(u->nodename, "stellares");
    strcpy(u->release,  "0.1.0");
    strcpy(u->version,  "#1 StellaresOS i386");
    strcpy(u->machine,  "i686");
    return 0;
}

/* ---- Dispatcher principal ---- */
uint32_t syscall_handler(uint32_t eax, uint32_t ebx,
                          uint32_t ecx, uint32_t edx) {
    switch(eax) {
        case SYS_EXIT:     return sys_exit(ebx);
        case SYS_EXIT_GRP: return sys_exit(ebx);
        case SYS_READ:     return sys_read(ebx, ecx, edx);
        case SYS_WRITE:    return sys_write(ebx, ecx, edx);
        case SYS_OPEN:     return sys_open(ebx, ecx, edx);
        case SYS_CLOSE:    return sys_close(ebx);
        case SYS_GETPID:   return sys_getpid();
        case SYS_BRK:      return sys_brk(ebx);
        case SYS_UNAME:    return sys_uname(ebx);
        default:
            serial_printf("[syscall] unknown: eax=%u\n", eax);
            return (uint32_t)-38; /* ENOSYS */
    }
}

void syscall_init(void) {
    fd_init();
    serial_puts("[syscall] Linux i386 ABI inicializada\n");
}
