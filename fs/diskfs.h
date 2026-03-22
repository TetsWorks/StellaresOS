/* ============================================================
 *  StellaresOS -- fs/diskfs.h
 *  DiskFS: filesystem persistente no disco ATA
 *
 *  Layout do disco:
 *  Setor 0:       Superbloco
 *  Setores 1-4:   Tabela de inodes (32 arquivos)
 *  Setores 5+:    Blocos de dados (8 setores = 4KB por arquivo)
 * ============================================================ */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define DISKFS_MAGIC      0x5354454C  /* "STEL" */
#define DISKFS_VERSION    1
#define DISKFS_MAX_FILES  32
#define DISKFS_NAME_MAX   48
#define DISKFS_FILE_SIZE  4096        /* 4KB por arquivo */
#define DISKFS_SECS_FILE  8           /* 8 setores de 512B */

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t nfiles;
    uint32_t inode_start;   /* Setor da tabela de inodes */
    uint32_t data_start;    /* Setor inicial dos dados */
    char     label[16];
    uint8_t  _pad[472];
} diskfs_super_t;

typedef struct __attribute__((packed)) {
    char     name[DISKFS_NAME_MAX];
    uint32_t size;
    uint32_t slot;          /* Índice no data_start */
    uint8_t  used;
    uint8_t  is_dir;
    uint8_t  _pad[6];
} diskfs_inode_t;           /* 64 bytes, 8 por setor, 4 setores = 32 inodes */

int  diskfs_init(void);
int  diskfs_format(void);
int  diskfs_write(const char *name, const void *data, size_t size);
int  diskfs_read(const char *name, void *buf, size_t maxsize);
int  diskfs_delete(const char *name);
int  diskfs_list(char out[][DISKFS_NAME_MAX], uint32_t sizes[], int *count);
int  diskfs_exists(const char *name);
int  diskfs_ready(void);
