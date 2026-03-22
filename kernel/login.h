#pragma once
typedef struct {
    char username[32];
    char home[64];
} user_t;
int  login_init(void);   /* Carrega usuários do disco */
int  login_screen(user_t *out); /* Tela de login, preenche user */
