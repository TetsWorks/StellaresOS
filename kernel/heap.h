#pragma once
#include <stddef.h>
#include <stdint.h>
void  heap_init(uint32_t start,uint32_t size);
void *kmalloc(size_t sz);
void  kfree(void *p);
void *krealloc(void *p,size_t sz);
size_t heap_used(void);
