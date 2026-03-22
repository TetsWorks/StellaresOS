/* ============================================================
 *  StellaresOS -- kernel/scheduler.c
 *  Round-robin preemptivo com troca de contexto real
 * ============================================================ */
#include "scheduler.h"
#include "../drivers/pit.h"
#include "../libc/string.h"

static process_t procs[MAX_PROCS];
static int current_idx = 0;
static uint32_t next_pid = 1;
static int initialized = 0;

/* ---- Troca de contexto em assembly puro ---- */
/* Salva contexto do processo atual e restaura o próximo */
static void __attribute__((noinline)) context_switch(
        uint32_t *save_esp, uint32_t load_esp) {
    __asm__ volatile (
        /* Salva registradores no stack atual */
        "push %%edi\n"
        "push %%esi\n"
        "push %%ebp\n"
        "push %%ebx\n"
        "push %%edx\n"
        "push %%ecx\n"
        "push %%eax\n"
        /* Salva ESP atual */
        "movl %%esp, (%0)\n"
        /* Carrega novo ESP */
        "movl %1, %%esp\n"
        /* Restaura registradores do novo stack */
        "pop %%eax\n"
        "pop %%ecx\n"
        "pop %%edx\n"
        "pop %%ebx\n"
        "pop %%ebp\n"
        "pop %%esi\n"
        "pop %%edi\n"
        :
        : "r"(save_esp), "r"(load_esp)
        : "memory"
    );
}

void sched_init(void) {
    memset(procs, 0, sizeof(procs));

    /* Processo 0 = kernel/idle (processo atual) */
    procs[0].pid   = 0;
    procs[0].state = PROC_RUNNING;
    strcpy(procs[0].name, "idle");
    current_idx = 0;
    initialized = 1;
}

int proc_create(const char *name, void (*entry)(void)) {
    if(!initialized) return -1;

    /* Encontra slot livre */
    int slot = -1;
    for(int i = 1; i < MAX_PROCS; i++) {
        if(procs[i].state == PROC_DEAD) { slot = i; break; }
    }
    if(slot < 0) return -1;

    process_t *p = &procs[slot];
    memset(p, 0, sizeof(process_t));

    p->pid        = next_pid++;
    p->state      = PROC_READY;
    p->stack_base = (uint32_t)p->stack;
    strncpy(p->name, name, PROC_NAME_LEN-1);

    /* Configura stack inicial do processo:
     * Empilha o endereço de proc_exit (retorno) e o entry point
     * O context_switch vai restaurar: eax,ecx,edx,ebx,ebp,esi,edi
     * e então retornar para entry via RET implícito no ESP */
    uint32_t *stack_top = (uint32_t *)(p->stack + STACK_SIZE);

    /* Endereço de retorno = proc_exit (se entry() retornar) */
    stack_top--;
    *stack_top = (uint32_t)proc_exit;

    /* EIP = entry point */
    stack_top--;
    *stack_top = (uint32_t)entry;

    /* Contexto inicial zerado (eax,ecx,edx,ebx,ebp,esi,edi) */
    stack_top -= 7;
    memset(stack_top, 0, 7 * sizeof(uint32_t));

    p->esp = (uint32_t)stack_top;

    return (int)p->pid;
}

static int find_next_ready(void) {
    int n = MAX_PROCS;
    int i = (current_idx + 1) % MAX_PROCS;
    while(n--) {
        if(procs[i].state == PROC_READY) return i;
        i = (i + 1) % MAX_PROCS;
    }
    return 0; /* idle */
}

void sched_tick(void) {
    if(!initialized) return;

    uint32_t now = pit_ticks();

    /* Acorda processos dormindo */
    for(int i = 0; i < MAX_PROCS; i++) {
        if(procs[i].state == PROC_SLEEPING &&
           now >= procs[i].sleep_until) {
            procs[i].state = PROC_READY;
        }
    }

    procs[current_idx].ticks++;

    /* Quantum: 20ms (20 ticks a 1000Hz) */
    if(procs[current_idx].ticks % 20 != 0) return;

    int next = find_next_ready();
    if(next == current_idx) return;

    process_t *old = &procs[current_idx];
    process_t *nxt = &procs[next];

    if(old->state == PROC_RUNNING)
        old->state = PROC_READY;
    nxt->state = PROC_RUNNING;

    int old_idx = current_idx;
    current_idx = next;

    context_switch(&procs[old_idx].esp, nxt->esp);
}

void proc_yield(void) {
    procs[current_idx].ticks = 19; /* Força troca no próximo tick */
}

void proc_sleep(uint32_t ms) {
    procs[current_idx].state       = PROC_SLEEPING;
    procs[current_idx].sleep_until = pit_ticks() + ms;
    proc_yield();
    /* Espera ser acordado */
    while(procs[current_idx].state == PROC_SLEEPING)
        __asm__ volatile("hlt");
}

void proc_exit(void) {
    procs[current_idx].state = PROC_DEAD;
    proc_yield();
    for(;;) __asm__ volatile("hlt");
}

void proc_kill(uint32_t pid) {
    for(int i = 1; i < MAX_PROCS; i++) {
        if(procs[i].pid == pid) {
            procs[i].state = PROC_DEAD;
            return;
        }
    }
}

process_t *proc_current(void) { return &procs[current_idx]; }

process_t *proc_get(uint32_t pid) {
    for(int i = 0; i < MAX_PROCS; i++)
        if(procs[i].pid == pid) return &procs[i];
    return 0;
}

int proc_count(void) {
    int n = 0;
    for(int i = 0; i < MAX_PROCS; i++)
        if(procs[i].state != PROC_DEAD) n++;
    return n;
}

void sched_dump(void) {
    extern void vga_printf(const char *fmt, ...);
    extern void vga_puts(const char *s);
    extern void vga_set_attr(uint8_t a);

    uint8_t hdr = (uint8_t)0x0E; /* Amarelo */
    uint8_t ok  = (uint8_t)0x0A; /* Verde   */
    uint8_t dim = (uint8_t)0x07; /* Cinza   */

    vga_set_attr(hdr);
    vga_puts("\n  PID   ESTADO     TICKS    NOME\n");
    vga_puts("  ---   --------   ------   ----------\n");

    const char *states[] = {"DEAD","READY","RUNNING","SLEEPING","BLOCKED"};
    for(int i = 0; i < MAX_PROCS; i++) {
        if(procs[i].state == PROC_DEAD) continue;
        vga_set_attr(procs[i].state == PROC_RUNNING ? ok : dim);
        vga_printf("  %-4u  %-9s  %-6u   %s\n",
            procs[i].pid,
            states[procs[i].state],
            procs[i].ticks,
            procs[i].name);
    }
    vga_puts("\n");
}
