/* ============================================================
 *  StellaresOS -- kernel/syscall.h
 *  Syscalls compatíveis com Linux i386 (INT 0x80)
 *  Mesmos números, mesma ABI: eax=num, ebx/ecx/edx=args
 * ============================================================ */
#pragma once
#include <stdint.h>

/* Números das syscalls (idênticos ao Linux i386) */
#define SYS_EXIT       1
#define SYS_FORK       2
#define SYS_READ       3
#define SYS_WRITE      4
#define SYS_OPEN       5
#define SYS_CLOSE      6
#define SYS_WAITPID    7
#define SYS_GETPID    20
#define SYS_BRK       45
#define SYS_MMAP      90
#define SYS_MUNMAP    91
#define SYS_UNAME    122
#define SYS_EXIT_GRP 252

/* Flags de open() — idênticas ao Linux */
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_CREAT   0100
#define O_TRUNC   01000
#define O_APPEND  02000

/* Estrutura utsname (sys_uname) */
typedef struct {
    char sysname[65];    /* "StellaresOS" */
    char nodename[65];   /* hostname */
    char release[65];    /* "0.1.0" */
    char version[65];    /* "#1 StellaresOS" */
    char machine[65];    /* "i686" */
} utsname_t;

/* Registra o handler de syscalls no IDT (vetor 0x80) */
void syscall_init(void);

/* Handler principal chamado pelo ISR 128 */
uint32_t syscall_handler(uint32_t eax, uint32_t ebx,
                         uint32_t ecx, uint32_t edx);
