/* ============================================================
 *  StellaresOS -- fs/ramfs.h
 *  Filesystem em RAM: arvore de diretorios e arquivos
 * ============================================================ */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define RAMFS_NAME_MAX  64
#define RAMFS_DATA_MAX  4096   /* 4KB por arquivo */
#define RAMFS_MAX_NODES 128    /* Max de arquivos/dirs */
#define RAMFS_MAX_CHILDREN 32  /* Max filhos por diretorio */

typedef enum {
    NODE_FREE = 0,
    NODE_FILE = 1,
    NODE_DIR  = 2,
} node_type_t;

typedef struct ramfs_node {
    node_type_t type;
    char        name[RAMFS_NAME_MAX];
    uint32_t    size;
    uint8_t     data[RAMFS_DATA_MAX];
    struct ramfs_node *parent;
    struct ramfs_node *children[RAMFS_MAX_CHILDREN];
    int         nchildren;
} ramfs_node_t;

void          ramfs_init(void);
ramfs_node_t *ramfs_root(void);
ramfs_node_t *ramfs_find(ramfs_node_t *dir, const char *name);
ramfs_node_t *ramfs_resolve(const char *path);
ramfs_node_t *ramfs_mkdir(ramfs_node_t *parent, const char *name);
ramfs_node_t *ramfs_create(ramfs_node_t *parent, const char *name);
int           ramfs_write(ramfs_node_t *f, const char *data, size_t len);
int           ramfs_read(ramfs_node_t *f, char *buf, size_t len);
int           ramfs_delete(ramfs_node_t *parent, const char *name);
void          ramfs_abs_path(ramfs_node_t *node, char *out);
