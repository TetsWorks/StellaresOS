#pragma once
#include <stdint.h>
#include <stddef.h>

#define SPK_MAGIC    0x4B505453u
#define SPK_VERSION  1
#define SPK_NAME_MAX 48
#define SPK_PATH_MAX 128
#define SPK_DESC_MAX 128

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    char     name[SPK_NAME_MAX];
    char     pkg_ver[16];
    char     description[SPK_DESC_MAX];
    char     author[SPK_NAME_MAX];
    uint32_t nfiles;
    uint32_t install_size;
    uint8_t  _pad[60];
} spk_header_t;

typedef struct __attribute__((packed)) {
    char     path[SPK_PATH_MAX];
    uint32_t offset;
    uint32_t size;
    uint32_t mode;
    uint8_t  _pad[12];
} spk_file_entry_t;

typedef enum { SPK_OK=0, SPK_INVALID=1, SPK_NOSPACE=2, SPK_EXISTS=3, SPK_ERR=4 } spk_result_t;

spk_result_t spk_install(const void *data, size_t size);
spk_result_t spk_remove(const char *name);
int          spk_installed(const char *name);
void         spk_list(void);
