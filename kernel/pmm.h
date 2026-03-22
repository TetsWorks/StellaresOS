#pragma once
#include <stdint.h>
#include <stddef.h>
#define PMM_FRAME 4096
void     pmm_init(uint32_t mem_kb);
void     pmm_mark_used(uint32_t addr,size_t sz);
void     pmm_mark_free(uint32_t addr,size_t sz);
uint32_t pmm_alloc(void);
void     pmm_free(uint32_t addr);
uint32_t pmm_free_kb(void);
uint32_t pmm_total_kb(void);
