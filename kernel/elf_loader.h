#pragma once
#include <stdint.h>
#include <stddef.h>

typedef enum {
    ELF_OK=0, ELF_NOTELF=1, ELF_NOT32=2,
    ELF_NOTEXEC=3, ELF_NOMEM=4, ELF_NOFILE=5,
} elf_result_t;

elf_result_t elf_exec(const char *path, int argc, char **argv);
elf_result_t elf_load(const void *data, size_t size, uint32_t *entry_out);
