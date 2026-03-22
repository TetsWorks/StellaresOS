/* ============================================================
 *  StellaresOS -- kernel/elf_loader.c
 *  Carrega ELF32 estático e cria processo para executá-lo
 *
 *  Compatível com programas compilados assim no Linux:
 *    gcc -m32 -static -nostdlib -o prog prog.c
 *    gcc -m32 -static -o prog prog.c   (com musl-libc ou uclibc)
 * ============================================================ */
#include "elf_loader.h"
#include "scheduler.h"
#include "syscall.h"
#include "../fs/ramfs.h"
#include "../fs/diskfs.h"
#include "../kernel/heap.h"
#include "../libc/string.h"
#include "../drivers/serial.h"

/* ---- Estruturas ELF32 ---- */
#define ELF_MAGIC    0x464C457F  /* "\x7fELF" */
#define ET_EXEC      2           /* Executável */
#define EM_386       3           /* i386 */
#define PT_LOAD      1           /* Segmento carregável */

typedef struct __attribute__((packed)) {
    uint32_t e_magic;
    uint8_t  e_class;     /* 1=32bit */
    uint8_t  e_data;      /* 1=LE */
    uint8_t  e_version;
    uint8_t  e_osabi;
    uint8_t  e_pad[8];
    uint16_t e_type;      /* ET_EXEC=2 */
    uint16_t e_machine;   /* EM_386=3 */
    uint32_t e_version2;
    uint32_t e_entry;     /* Entry point */
    uint32_t e_phoff;     /* Program header offset */
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;     /* Número de program headers */
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf32_hdr_t;

typedef struct __attribute__((packed)) {
    uint32_t p_type;      /* PT_LOAD=1 */
    uint32_t p_offset;    /* Offset no arquivo */
    uint32_t p_vaddr;     /* Endereço virtual destino */
    uint32_t p_paddr;
    uint32_t p_filesz;    /* Tamanho no arquivo */
    uint32_t p_memsz;     /* Tamanho na memória (pode ser > filesz, zeros no resto) */
    uint32_t p_flags;
    uint32_t p_align;
} elf32_phdr_t;

/* ---- Stack para o processo: argv, envp, auxv ---- */
static uint32_t setup_stack(uint32_t stack_top, int argc, char **argv) {
    /* Monta a stack conforme ABI System V i386:
     * [argc] [argv[0]] ... [argv[n]] [NULL] [envp NULL] [auxv NULL NULL]
     */
    uint32_t *sp = (uint32_t *)stack_top;

    /* Copia strings dos argumentos */
    char *str_area = (char *)(stack_top - 256);
    char *str_ptr = str_area;
    char *arg_ptrs[16];
    for(int i=0;i<argc&&i<16;i++){
        arg_ptrs[i] = str_ptr;
        strcpy(str_ptr, argv[i]);
        str_ptr += strlen(argv[i])+1;
    }

    /* Alinha */
    sp = (uint32_t *)(((uint32_t)str_ptr + 3) & ~3);

    /* NULL terminador de auxv */
    *--sp = 0; *--sp = 0;
    /* NULL terminador de envp */
    *--sp = 0;
    /* NULL terminador de argv */
    *--sp = 0;
    /* argv[n-1]...argv[0] */
    for(int i=argc-1;i>=0;i--)
        *--sp = (uint32_t)arg_ptrs[i];
    /* argc */
    *--sp = (uint32_t)argc;

    return (uint32_t)sp;
}

elf_result_t elf_load(const void *data, size_t size, uint32_t *entry_out) {
    if(!data || size < sizeof(elf32_hdr_t)) return ELF_NOTELF;

    const elf32_hdr_t *hdr = (const elf32_hdr_t *)data;

    /* Verifica magic */
    if(hdr->e_magic != ELF_MAGIC)    return ELF_NOTELF;
    if(hdr->e_class != 1)            return ELF_NOT32;
    if(hdr->e_type  != ET_EXEC)      return ELF_NOTEXEC;
    if(hdr->e_machine != EM_386)     return ELF_NOT32;

    serial_printf("[elf] carregando: entry=0x%x phnum=%d\n",
        hdr->e_entry, hdr->e_phnum);

    /* Carrega segmentos PT_LOAD */
    const uint8_t *base = (const uint8_t *)data;
    const elf32_phdr_t *phdrs = (const elf32_phdr_t *)(base + hdr->e_phoff);

    for(int i=0; i<hdr->e_phnum; i++){
        const elf32_phdr_t *ph = &phdrs[i];
        if(ph->p_type != PT_LOAD) continue;

        serial_printf("[elf] seg %d: vaddr=0x%x filesz=%u memsz=%u\n",
            i, ph->p_vaddr, ph->p_filesz, ph->p_memsz);

        /* Copia segmento para o endereço virtual */
        if(ph->p_filesz > 0)
            memcpy((void *)ph->p_vaddr, base + ph->p_offset, ph->p_filesz);

        /* Zera o BSS (memsz > filesz) */
        if(ph->p_memsz > ph->p_filesz)
            memset((void *)(ph->p_vaddr + ph->p_filesz), 0,
                   ph->p_memsz - ph->p_filesz);
    }

    if(entry_out) *entry_out = hdr->e_entry;
    return ELF_OK;
}

/* Contexto para o processo ELF */
typedef struct {
    uint32_t entry;
    uint32_t stack_ptr;
} elf_ctx_t;

static elf_ctx_t pending_ctx;

static void elf_entry_trampoline(void) {
    /* Restaura stack e pula para o entry point do programa */
    uint32_t entry = pending_ctx.entry;
    uint32_t esp   = pending_ctx.stack_ptr;

    /* Seta stack e chama entry via inline asm */
    __asm__ volatile (
        "mov %0, %%esp\n"
        "jmp *%1\n"
        :
        : "r"(esp), "r"(entry)
    );
}

elf_result_t elf_exec(const char *path, int argc, char **argv) {
    if(!path) return ELF_NOFILE;

    /* Lê o arquivo do RamFS ou disco */
    static uint8_t elf_buf[RAMFS_DATA_MAX];
    int n = 0;

    ramfs_node_t *f = ramfs_resolve(path);
    if(f && f->type == NODE_FILE) {
        n = (int)f->size;
        memcpy(elf_buf, f->data, (size_t)n);
    } else if(diskfs_ready() && diskfs_exists(path)) {
        n = diskfs_read(path, elf_buf, sizeof(elf_buf));
    }

    if(n <= 0) return ELF_NOFILE;

    /* Carrega o ELF */
    uint32_t entry = 0;
    elf_result_t r = elf_load(elf_buf, (size_t)n, &entry);
    if(r != ELF_OK) return r;

    /* Monta a stack do processo */
    static uint8_t proc_stack[65536]; /* 64KB de stack para o programa */
    uint32_t stack_top = (uint32_t)(proc_stack + sizeof(proc_stack));
    uint32_t sp = setup_stack(stack_top, argc, argv);

    /* Cria processo */
    pending_ctx.entry     = entry;
    pending_ctx.stack_ptr = sp;

    int pid = proc_create(path, elf_entry_trampoline);
    serial_printf("[elf] processo criado: pid=%d entry=0x%x\n", pid, entry);

    return ELF_OK;
}
