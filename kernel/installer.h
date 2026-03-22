#pragma once
typedef struct {
    char username[32];
    char password[64];
    char hostname[32];
    char timezone[32];
} install_config_t;

int installer_run(install_config_t *cfg); /* Retorna 0 se instalou, -1 se já instalado */
