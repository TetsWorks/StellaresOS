#include "heap.h"
#include "../libc/string.h"
#define USED 0xC0FFEE00
#define FREE 0xDEADBEEF
#define MINSPLIT 32
typedef struct blk{uint32_t magic;size_t size;struct blk*next,*prev;}blk_t;
static blk_t*heap=0;static size_t heap_sz=0,heap_used_b=0;
void heap_init(uint32_t s,uint32_t sz){heap=(blk_t*)s;heap_sz=sz;heap->magic=FREE;heap->size=sz-sizeof(blk_t);heap->next=heap->prev=NULL;}
void*kmalloc(size_t sz){if(!sz)return NULL;sz=(sz+7)&~7;blk_t*c=heap;while(c){if(c->magic==FREE&&c->size>=sz){if(c->size>=sz+sizeof(blk_t)+MINSPLIT){blk_t*n=(blk_t*)((uint8_t*)c+sizeof(blk_t)+sz);n->magic=FREE;n->size=c->size-sz-sizeof(blk_t);n->next=c->next;n->prev=c;if(c->next)c->next->prev=n;c->next=n;c->size=sz;}c->magic=USED;heap_used_b+=c->size;return(void*)((uint8_t*)c+sizeof(blk_t));}c=c->next;}return NULL;}
static void merge(blk_t*b){if(b->next&&b->next->magic==FREE){b->size+=sizeof(blk_t)+b->next->size;b->next=b->next->next;if(b->next)b->next->prev=b;}}
void kfree(void*p){if(!p)return;blk_t*b=(blk_t*)((uint8_t*)p-sizeof(blk_t));if(b->magic!=USED)return;heap_used_b-=b->size;b->magic=FREE;if(b->prev&&b->prev->magic==FREE)merge(b->prev);else merge(b);}
void*krealloc(void*p,size_t sz){if(!p)return kmalloc(sz);blk_t*b=(blk_t*)((uint8_t*)p-sizeof(blk_t));if(sz<=b->size)return p;void*n=kmalloc(sz);if(!n)return NULL;memcpy(n,p,b->size);kfree(p);return n;}
size_t heap_used(void){return heap_used_b;}
