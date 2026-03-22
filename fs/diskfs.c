/* ============================================================
 *  StellaresOS -- fs/diskfs.c
 * ============================================================ */
#include "diskfs.h"
#include "../drivers/ata.h"
#include "../libc/string.h"

#define SUPER_SECTOR  0
#define INODE_SECTOR  1   /* 4 setores, 8 inodes por setor = 32 */
#define DATA_SECTOR   5   /* Começa aqui, 8 setores por arquivo */

static diskfs_super_t super;
static diskfs_inode_t inodes[DISKFS_MAX_FILES];
static int fs_ready = 0;

/* ---- Lê/escreve setor inteiro ---- */
static int read_super(void) {
    return ata_read(SUPER_SECTOR, 1, &super) == ATA_OK ? 0 : -1;
}
static int write_super(void) {
    return ata_write(SUPER_SECTOR, 1, &super) == ATA_OK ? 0 : -1;
}
static int read_inodes(void) {
    return ata_read(INODE_SECTOR, 4, inodes) == ATA_OK ? 0 : -1;
}
static int write_inodes(void) {
    return ata_write(INODE_SECTOR, 4, inodes) == ATA_OK ? 0 : -1;
}

int diskfs_format(void) {
    /* Superbloco */
    memset(&super, 0, sizeof(super));
    super.magic       = DISKFS_MAGIC;
    super.version     = DISKFS_VERSION;
    super.nfiles      = 0;
    super.inode_start = INODE_SECTOR;
    super.data_start  = DATA_SECTOR;
    strcpy(super.label, "StellaresOS");
    if(write_super() < 0) return -1;

    /* Inodes zerados */
    memset(inodes, 0, sizeof(inodes));
    if(write_inodes() < 0) return -1;

    fs_ready = 1;
    return 0;
}

int diskfs_init(void) {
    if(!ata_detect()) return -1;
    if(read_super() < 0) return -1;

    if(super.magic != DISKFS_MAGIC) {
        /* Disco não formatado — formata automaticamente */
        return diskfs_format();
    }
    if(read_inodes() < 0) return -1;
    fs_ready = 1;
    return 0;
}

int diskfs_ready(void) { return fs_ready; }

static int find_inode(const char *name) {
    for(int i = 0; i < DISKFS_MAX_FILES; i++)
        if(inodes[i].used && strcmp(inodes[i].name, name) == 0)
            return i;
    return -1;
}

static int alloc_inode(void) {
    for(int i = 0; i < DISKFS_MAX_FILES; i++)
        if(!inodes[i].used) return i;
    return -1;
}

/* Cada arquivo usa um slot de 8 setores contíguos */
static int alloc_slot(void) {
    /* Marca slots usados */
    int used[DISKFS_MAX_FILES] = {0};
    for(int i = 0; i < DISKFS_MAX_FILES; i++)
        if(inodes[i].used) used[inodes[i].slot] = 1;
    for(int i = 0; i < DISKFS_MAX_FILES; i++)
        if(!used[i]) return i;
    return -1;
}

int diskfs_write(const char *name, const void *data, size_t size) {
    if(!fs_ready || !name || !data) return -1;
    if(size > DISKFS_FILE_SIZE) size = DISKFS_FILE_SIZE;

    int idx = find_inode(name);
    if(idx < 0) {
        /* Novo arquivo */
        idx = alloc_inode();
        if(idx < 0) return -1;
        int slot = alloc_slot();
        if(slot < 0) return -1;
        memset(&inodes[idx], 0, sizeof(diskfs_inode_t));
        strncpy(inodes[idx].name, name, DISKFS_NAME_MAX-1);
        inodes[idx].used = 1;
        inodes[idx].slot = (uint32_t)slot;
        super.nfiles++;
    }

    inodes[idx].size = (uint32_t)size;

    /* Escreve dados em buffer de 4KB alinhado */
    uint8_t buf[DISKFS_FILE_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, data, size);

    uint32_t lba = DATA_SECTOR + inodes[idx].slot * DISKFS_SECS_FILE;
    if(ata_write(lba, DISKFS_SECS_FILE, buf) != ATA_OK) return -1;

    write_inodes();
    write_super();
    return (int)size;
}

int diskfs_read(const char *name, void *buf, size_t maxsize) {
    if(!fs_ready || !name || !buf) return -1;
    int idx = find_inode(name);
    if(idx < 0) return -1;

    uint8_t tmp[DISKFS_FILE_SIZE];
    uint32_t lba = DATA_SECTOR + inodes[idx].slot * DISKFS_SECS_FILE;
    if(ata_read(lba, DISKFS_SECS_FILE, tmp) != ATA_OK) return -1;

    size_t n = inodes[idx].size < maxsize ? inodes[idx].size : maxsize;
    memcpy(buf, tmp, n);
    return (int)n;
}

int diskfs_delete(const char *name) {
    if(!fs_ready) return -1;
    int idx = find_inode(name);
    if(idx < 0) return -1;
    memset(&inodes[idx], 0, sizeof(diskfs_inode_t));
    if(super.nfiles > 0) super.nfiles--;
    write_inodes();
    write_super();
    return 0;
}

int diskfs_list(char out[][DISKFS_NAME_MAX], uint32_t sizes[], int *count) {
    if(!fs_ready) return -1;
    *count = 0;
    for(int i = 0; i < DISKFS_MAX_FILES; i++) {
        if(inodes[i].used) {
            strncpy(out[*count], inodes[i].name, DISKFS_NAME_MAX-1);
            if(sizes) sizes[*count] = inodes[i].size;
            (*count)++;
        }
    }
    return 0;
}

int diskfs_exists(const char *name) {
    return find_inode(name) >= 0 ? 1 : 0;
}
