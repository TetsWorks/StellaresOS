#include "ata.h"
#include "../libc/string.h"

#define ATA_DATA     0x1F0
#define ATA_ERR_REG  0x1F1
#define ATA_SECCOUNT 0x1F2
#define ATA_LBA_LO   0x1F3
#define ATA_LBA_MID  0x1F4
#define ATA_LBA_HI   0x1F5
#define ATA_DRIVE    0x1F6
#define ATA_STATUS   0x1F7
#define ATA_COMMAND  0x1F7
#define ATA_SR_BSY   0x80
#define ATA_SR_DRDY  0x40
#define ATA_SR_DRQ   0x08
#define ATA_SR_ERR   0x01
#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_CMD_FLUSH   0xE7
#define ATA_CMD_IDENTIFY 0xEC

static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline void outw(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline uint16_t inw(uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}

static int    disk_ok = 0;
static uint32_t disk_sectors = 0;
static char   disk_model[41] = {0};

static int ata_wait_bsy(void){
    int t=100000; while((inb(ATA_STATUS)&ATA_SR_BSY)&&t--); return t>0?ATA_OK:ATA_TIMEOUT;
}
static int ata_wait_drq(void){
    int t=100000; uint8_t s;
    while(t--){ s=inb(ATA_STATUS); if(s&ATA_SR_ERR)return ATA_ERR; if(s&ATA_SR_DRQ)return ATA_OK; }
    return ATA_TIMEOUT;
}
static void fix_str(char *s,int l){
    for(int i=0;i<l;i+=2){char t=s[i];s[i]=s[i+1];s[i+1]=t;}
    for(int i=l-1;i>=0&&s[i]==' ';i--)s[i]=0;
}

void ata_init(void){
    disk_ok=0;
    outb(0x3F6,0x04); for(int i=0;i<10000;i++)__asm__ volatile("nop");
    outb(0x3F6,0x00);
    if(ata_wait_bsy()!=ATA_OK) return;
    outb(ATA_DRIVE,0xA0);
    outb(ATA_SECCOUNT,0); outb(ATA_LBA_LO,0); outb(ATA_LBA_MID,0); outb(ATA_LBA_HI,0);
    outb(ATA_COMMAND,ATA_CMD_IDENTIFY);
    uint8_t s=inb(ATA_STATUS); if(!s) return;
    if(ata_wait_bsy()!=ATA_OK) return;
    if(inb(ATA_LBA_MID)||inb(ATA_LBA_HI)) return;
    if(ata_wait_drq()!=ATA_OK) return;
    uint16_t id[256];
    for(int i=0;i<256;i++) id[i]=inw(ATA_DATA);
    disk_sectors=((uint32_t)id[61]<<16)|id[60];
    memcpy(disk_model,&id[27],40); fix_str(disk_model,40);
    disk_ok=1;
}

int ata_detect(void){ return disk_ok; }
uint32_t ata_sectors(void){ return disk_sectors; }
const char *ata_model(void){ return disk_model; }

ata_result_t ata_read(uint32_t lba, uint8_t count, void *buf){
    if(!disk_ok) return ATA_NODISK;
    if(ata_wait_bsy()!=ATA_OK) return ATA_TIMEOUT;
    outb(ATA_DRIVE,0xE0|((lba>>24)&0x0F));
    outb(ATA_SECCOUNT,count);
    outb(ATA_LBA_LO,(uint8_t)lba);
    outb(ATA_LBA_MID,(uint8_t)(lba>>8));
    outb(ATA_LBA_HI,(uint8_t)(lba>>16));
    outb(ATA_COMMAND,ATA_CMD_READ);
    uint16_t *p=(uint16_t*)buf;
    for(int s=0;s<count;s++){
        if(ata_wait_drq()!=ATA_OK) return ATA_ERR;
        for(int i=0;i<256;i++) p[s*256+i]=inw(ATA_DATA);
    }
    return ATA_OK;
}

ata_result_t ata_write(uint32_t lba, uint8_t count, const void *buf){
    if(!disk_ok) return ATA_NODISK;
    if(ata_wait_bsy()!=ATA_OK) return ATA_TIMEOUT;
    outb(ATA_DRIVE,0xE0|((lba>>24)&0x0F));
    outb(ATA_SECCOUNT,count);
    outb(ATA_LBA_LO,(uint8_t)lba);
    outb(ATA_LBA_MID,(uint8_t)(lba>>8));
    outb(ATA_LBA_HI,(uint8_t)(lba>>16));
    outb(ATA_COMMAND,ATA_CMD_WRITE);
    const uint16_t *p=(const uint16_t*)buf;
    for(int s=0;s<count;s++){
        if(ata_wait_drq()!=ATA_OK) return ATA_ERR;
        for(int i=0;i<256;i++) outw(ATA_DATA,p[s*256+i]);
    }
    outb(ATA_COMMAND,ATA_CMD_FLUSH);
    ata_wait_bsy();
    return ATA_OK;
}
