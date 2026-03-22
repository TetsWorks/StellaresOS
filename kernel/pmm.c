#include "pmm.h"
#include "../libc/string.h"
#define MAX_FRAMES (256*1024)
static uint32_t bmap[MAX_FRAMES/32];
static uint32_t total=0,used=0;
#define SET(n) (bmap[(n)/32]|=(1u<<((n)%32)))
#define CLR(n) (bmap[(n)/32]&=~(1u<<((n)%32)))
#define TST(n) (bmap[(n)/32]&(1u<<((n)%32)))
void pmm_init(uint32_t kb){total=(1024+kb)/4;if(total>MAX_FRAMES)total=MAX_FRAMES;memset(bmap,0xFF,sizeof(bmap));used=total;}
void pmm_mark_free(uint32_t a,size_t s){uint32_t f=a/PMM_FRAME,c=(s+PMM_FRAME-1)/PMM_FRAME;for(uint32_t i=0;i<c&&f+i<total;i++){if(TST(f+i)){CLR(f+i);used--;}}}
void pmm_mark_used(uint32_t a,size_t s){uint32_t f=a/PMM_FRAME,c=(s+PMM_FRAME-1)/PMM_FRAME;for(uint32_t i=0;i<c&&f+i<total;i++){if(!TST(f+i)){SET(f+i);used++;}}}
uint32_t pmm_alloc(void){for(uint32_t i=0;i<total/32;i++){if(bmap[i]==0xFFFFFFFF)continue;for(int b=0;b<32;b++){uint32_t f=i*32+b;if(!TST(f)){SET(f);used++;return f*PMM_FRAME;}}}return 0;}
void pmm_free(uint32_t a){uint32_t f=a/PMM_FRAME;if(TST(f)){CLR(f);used--;}}
uint32_t pmm_free_kb(void){return(total-used)*4;}
uint32_t pmm_total_kb(void){return total*4;}
