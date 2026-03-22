/* ============================================================
 *  StellaresOS -- pkg/spk.c
 *  Gerenciador de pacotes SPK
 * ============================================================ */
#include "spk.h"
#include "../fs/ramfs.h"
#include "../fs/diskfs.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../libc/string.h"

/* Instala um pacote .spk carregado na memória */
spk_result_t spk_install(const void *data, size_t size) {
    if(!data || size < sizeof(spk_header_t)) return SPK_INVALID;

    const spk_header_t *hdr = (const spk_header_t *)data;
    if(hdr->magic != SPK_MAGIC) return SPK_INVALID;
    if(hdr->version != SPK_VERSION) return SPK_INVALID;

    serial_printf("[spkg] instalando: %s %s (%u arquivos)\n",
        hdr->name, hdr->pkg_ver, hdr->nfiles);

    /* Tabela de arquivos vem após o header */
    const spk_file_entry_t *entries = (const spk_file_entry_t *)
        ((const uint8_t*)data + sizeof(spk_header_t));

    /* Extrai cada arquivo */
    for(uint32_t i=0; i<hdr->nfiles; i++){
        const spk_file_entry_t *e = &entries[i];
        const uint8_t *fdata = (const uint8_t*)data + e->offset;

        serial_printf("[spkg]   -> %s (%u bytes)\n", e->path, e->size);

        /* Resolve o caminho e cria o arquivo no RamFS */
        char dir[SPK_PATH_MAX], base[SPK_NAME_MAX];
        /* Separa dirname/basename */
        const char *last = e->path;
        for(const char *p=e->path; *p; p++) if(*p=='/') last=p+1;
        int dl=(int)(last-e->path);
        strncpy(dir,e->path,dl); dir[dl]=0;
        strcpy(base,last);

        ramfs_node_t *parent = ramfs_resolve(dir);
        if(!parent){
            /* Cria diretórios intermediários */
            parent = ramfs_root();
            char tmp[SPK_PATH_MAX];
            strcpy(tmp, dir[0]=='/'?dir+1:dir);
            char *tok = strtok(tmp,"/");
            while(tok){
                ramfs_node_t *d = ramfs_find(parent,tok);
                if(!d) d = ramfs_mkdir(parent,tok);
                if(d) parent=d;
                tok=strtok(NULL,"/");
            }
        }

        /* Cria/atualiza arquivo */
        ramfs_node_t *f = ramfs_find(parent,base);
        if(!f) f = ramfs_create(parent,base);
        if(f) ramfs_write(f,(const char*)fdata,e->size);

        /* Salva no disco também */
        if(diskfs_ready()) diskfs_write(base,(char*)fdata,e->size);
    }

    /* Registra instalação */
    char rec[128]; strcpy(rec,hdr->name); strcat(rec," "); strcat(rec,hdr->pkg_ver); strcat(rec,"\n");
    char rec_name[SPK_NAME_MAX]; strcpy(rec_name,"installed_"); strcat(rec_name,hdr->name);
    if(diskfs_ready()) diskfs_write(rec_name,rec,strlen(rec));

    vga_printf("  Instalado: %s %s\n", hdr->name, hdr->pkg_ver);
    return SPK_OK;
}

spk_result_t spk_remove(const char *name) {
    char rec_name[SPK_NAME_MAX]; strcpy(rec_name,"installed_"); strcat(rec_name,name);
    if(diskfs_ready()) diskfs_delete(rec_name);
    vga_printf("  Removido: %s\n", name);
    return SPK_OK;
}

int spk_installed(const char *name) {
    char rec_name[SPK_NAME_MAX]; strcpy(rec_name,"installed_"); strcat(rec_name,name);
    return diskfs_ready() && diskfs_exists(rec_name);
}

void spk_list(void) {
    if(!diskfs_ready()){ vga_puts("  Disco nao disponivel.\n"); return; }
    char names[DISKFS_MAX_FILES][DISKFS_NAME_MAX];
    uint32_t sizes[DISKFS_MAX_FILES]; int count=0;
    diskfs_list(names,sizes,&count);
    int found=0;
    for(int i=0;i<count;i++){
        if(strncmp(names[i],"installed_",10)==0){
            vga_set_attr(0x0A); /* Verde */
            vga_printf("  %s\n", names[i]+10);
            found++;
        }
    }
    if(!found) vga_puts("  Nenhum pacote instalado.\n");
}
