/* ============================================================
 *  StellaresOS -- drivers/ata.h
 *  Driver ATA/IDE PIO mode
 * ============================================================ */
#pragma once
#include <stdint.h>

typedef enum {
    ATA_OK      = 0,
    ATA_ERR     = 1,
    ATA_TIMEOUT = 2,
    ATA_NODISK  = 3,
} ata_result_t;

#define ATA_SECTOR_SIZE 512

void         ata_init(void);
int          ata_detect(void);
ata_result_t ata_read(uint32_t lba, uint8_t count, void *buf);
ata_result_t ata_write(uint32_t lba, uint8_t count, const void *buf);
uint32_t     ata_sectors(void);
const char  *ata_model(void);
