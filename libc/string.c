#include "string.h"

void *memset(void *d, int c, size_t n){
    uint8_t *p=(uint8_t*)d;
    while(n--) *p++=(uint8_t)c;
    return d;
}
void *memcpy(void *d, const void *s, size_t n){
    uint8_t *p=(uint8_t*)d;
    const uint8_t *q=(const uint8_t*)s;
    while(n--) *p++=*q++;
    return d;
}
int memcmp(const void *a, const void *b, size_t n){
    const uint8_t *p=(const uint8_t*)a, *q=(const uint8_t*)b;
    while(n--){ if(*p!=*q) return *p-*q; p++;q++; }
    return 0;
}
size_t strlen(const char *s){
    size_t n=0;
    while(*s++) n++;
    return n;
}
char *strcpy(char *d, const char *s){
    char *r=d;
    while((*d++=*s++)) {}
    return r;
}
char *strncpy(char *d, const char *s, size_t n){
    char *r=d;
    while(n && (*d++=*s++)) n--;
    while(n--) *d++=0;
    return r;
}
int strcmp(const char *a, const char *b){
    while(*a && *a==*b){ a++;b++; }
    return (unsigned char)*a-(unsigned char)*b;
}
int strncmp(const char *a, const char *b, size_t n){
    while(n && *a && *a==*b){ a++;b++;n--; }
    if(!n) return 0;
    return (unsigned char)*a-(unsigned char)*b;
}
char *strcat(char *d, const char *s){
    char *r=d;
    while(*d) d++;
    while((*d++=*s++)) {}
    return r;
}
char *strchr(const char *s, int c){
    while(*s){ if(*s==(char)c) return (char*)s; s++; }
    return 0;
}
static char *strtok_p=0;
char *strtok(char *s, const char *d){
    if(s) strtok_p=s;
    if(!strtok_p) return 0;
    while(*strtok_p && strchr(d,*strtok_p)) strtok_p++;
    if(!*strtok_p){ strtok_p=0; return 0; }
    char *r=strtok_p;
    while(*strtok_p && !strchr(d,*strtok_p)) strtok_p++;
    if(*strtok_p){ *strtok_p=0; strtok_p++; }
    else strtok_p=0;
    return r;
}

char *itoa(int v, char *b, int base){
    static const char dig[]="0123456789ABCDEF";
    char tmp[32];
    int i=0, neg=0;
    unsigned u;
    if(v==0){ b[0]='0'; b[1]=0; return b; }
    if(base==10 && v<0){ neg=1; u=(unsigned)(-v); }
    else u=(unsigned)v;
    while(u){ tmp[i++]=dig[u%base]; u/=base; }
    if(neg) tmp[i++]='-';
    int j=0;
    while(i--) b[j++]=tmp[i];
    b[j]=0;
    return b;
}

void int_to_hex(uint32_t v, char *b){
    static const char h[]="0123456789ABCDEF";
    b[0]='0'; b[1]='x';
    for(int i=7;i>=0;i--)
        b[2+(7-i)]=h[(v>>(i*4))&0xF];
    b[10]=0;
}

int atoi(const char *s){
    int r=0,sign=1;
    while(*s==' ') s++;
    if(*s=='-'){ sign=-1; s++; }
    else if(*s=='+') s++;
    while(*s>='0' && *s<='9') r=r*10+(*s++-'0');
    return sign*r;
}
