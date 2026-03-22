#pragma once
#include <stdint.h>
#include <stddef.h>
void  *memset(void *d, int c, size_t n);
void  *memcpy(void *d, const void *s, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
char  *strcpy(char *d, const char *s);
char  *strncpy(char *d, const char *s, size_t n);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcat(char *d, const char *s);
char  *strchr(const char *s, int c);
char  *strtok(char *s, const char *d);
char  *itoa(int v, char *b, int base);
void   int_to_hex(uint32_t v, char *b);
int    atoi(const char *s);
