/* ============================================================
 *  StellaresOS -- kernel/scheduler.h
 *  Scheduler preemptivo round-robin com troca de contexto real
 * ============================================================ */
#pragma once
#include <stdint.h>

#define MAX_PROCS    16
#define STACK_SIZE   8192   /* 8KB por processo */
#define PROC_NAME_LEN 24

typedef enum {
    PROC_DEAD    = 0,
    PROC_READY   = 1,
    PROC_RUNNING = 2,
    PROC_SLEEPING= 3,
    PROC_BLOCKED = 4,
} proc_state_t;

/* Contexto salvo na troca de contexto (ordem importa!) */
typedef struct __attribute__((packed)) {
    uint32_t edi, esi, ebp, ebx, edx, ecx, eax;
    uint32_t eip;
} context_t;

typedef struct process {
    uint32_t      pid;
    char          name[PROC_NAME_LEN];
    proc_state_t  state;
    uint32_t      ticks;        /* Ticks consumidos */
    uint32_t      sleep_until;  /* Para SLEEPING */
    uint32_t      esp;          /* Stack pointer salvo */
    uint32_t      stack_base;   /* Base da pilha */
    uint8_t       stack[STACK_SIZE];
} process_t;

/* API pública */
void      sched_init(void);
int       proc_create(const char *name, void (*entry)(void));
void      proc_exit(void);
void      proc_sleep(uint32_t ms);
void      proc_yield(void);
void      proc_kill(uint32_t pid);
process_t *proc_current(void);
process_t *proc_get(uint32_t pid);
int       proc_count(void);
void      sched_tick(void);      /* Chamado pelo IRQ0 */
void      sched_dump(void);      /* Para o comando ps */
