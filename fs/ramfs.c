/* ============================================================
 *  StellaresOS -- fs/ramfs.c
 * ============================================================ */
#include "ramfs.h"
#include "../libc/string.h"

static ramfs_node_t nodes[RAMFS_MAX_NODES];
static ramfs_node_t *root_node = 0;

void ramfs_init(void) {
    /* Zera todos os nós */
    for(int i=0;i<RAMFS_MAX_NODES;i++){
        nodes[i].type=NODE_FREE;
        nodes[i].name[0]=0;
        nodes[i].size=0;
        nodes[i].parent=0;
        nodes[i].nchildren=0;
    }
    /* Cria raiz */
    nodes[0].type=NODE_DIR;
    strcpy(nodes[0].name,"/");
    nodes[0].parent=&nodes[0];
    root_node=&nodes[0];

    /* Cria estrutura inicial */
    ramfs_node_t *bin  = ramfs_mkdir(root_node,"bin");
    ramfs_node_t *etc  = ramfs_mkdir(root_node,"etc");
    ramfs_node_t *home = ramfs_mkdir(root_node,"home");
    ramfs_node_t *tmp  = ramfs_mkdir(root_node,"tmp");
    ramfs_node_t *usr  = ramfs_mkdir(root_node,"usr");
    (void)bin; (void)tmp; (void)usr;

    /* Arquivos default */
    ramfs_node_t *motd = ramfs_create(etc,"motd");
    ramfs_write(motd,
        "Bem-vindo ao StellaresOS v0.1!\n"
        "Um microkernel x86 feito do zero.\n"
        "Digite 'help' para ver os comandos.\n", 0);

    ramfs_node_t *ver = ramfs_create(etc,"version");
    ramfs_write(ver,"StellaresOS v0.1 (i386)\n",0);

    ramfs_node_t *readme = ramfs_create(home,"README.txt");
    ramfs_write(readme,
        "=== StellaresOS ===\n\n"
        "Este e o seu diretorio home.\n"
        "Voce pode criar arquivos com: touch <nome>\n"
        "Escrever com: write <arquivo> <texto>\n"
        "Ler com: cat <arquivo>\n"
        "Listar com: ls\n", 0);
}

ramfs_node_t *ramfs_root(void){ return root_node; }

static ramfs_node_t *alloc_node(void){
    for(int i=0;i<RAMFS_MAX_NODES;i++)
        if(nodes[i].type==NODE_FREE) return &nodes[i];
    return 0;
}

ramfs_node_t *ramfs_find(ramfs_node_t *dir, const char *name){
    if(!dir||dir->type!=NODE_DIR) return 0;
    for(int i=0;i<dir->nchildren;i++)
        if(strcmp(dir->children[i]->name,name)==0)
            return dir->children[i];
    return 0;
}

ramfs_node_t *ramfs_resolve(const char *path){
    if(!path) return 0;
    if(strcmp(path,"/")==0) return root_node;

    char buf[256];
    strncpy(buf, path[0]=='/' ? path+1 : path, 255);

    ramfs_node_t *cur = (path[0]=='/') ? root_node : root_node;
    char *tok = strtok(buf, "/");
    while(tok){
        if(strcmp(tok,".")==0){ tok=strtok(NULL,"/"); continue; }
        if(strcmp(tok,"..")==0){
            if(cur->parent && cur->parent!=cur) cur=cur->parent;
            tok=strtok(NULL,"/"); continue;
        }
        cur=ramfs_find(cur,tok);
        if(!cur) return 0;
        tok=strtok(NULL,"/");
    }
    return cur;
}

ramfs_node_t *ramfs_mkdir(ramfs_node_t *parent, const char *name){
    if(!parent||!name) return 0;
    if(parent->nchildren>=RAMFS_MAX_CHILDREN) return 0;
    if(ramfs_find(parent,name)) return 0; /* já existe */
    ramfs_node_t *n=alloc_node();
    if(!n) return 0;
    n->type=NODE_DIR;
    strncpy(n->name,name,RAMFS_NAME_MAX-1);
    n->parent=parent;
    n->nchildren=0;
    n->size=0;
    parent->children[parent->nchildren++]=n;
    return n;
}

ramfs_node_t *ramfs_create(ramfs_node_t *parent, const char *name){
    if(!parent||!name) return 0;
    if(parent->nchildren>=RAMFS_MAX_CHILDREN) return 0;
    ramfs_node_t *existing=ramfs_find(parent,name);
    if(existing) return existing;
    ramfs_node_t *n=alloc_node();
    if(!n) return 0;
    n->type=NODE_FILE;
    strncpy(n->name,name,RAMFS_NAME_MAX-1);
    n->parent=parent;
    n->size=0;
    parent->children[parent->nchildren++]=n;
    return n;
}

int ramfs_write(ramfs_node_t *f, const char *data, size_t len){
    if(!f||f->type!=NODE_FILE) return -1;
    if(len==0) len=strlen(data);
    if(len>RAMFS_DATA_MAX) len=RAMFS_DATA_MAX;
    memcpy(f->data,data,len);
    f->size=(uint32_t)len;
    return (int)len;
}

int ramfs_read(ramfs_node_t *f, char *buf, size_t len){
    if(!f||f->type!=NODE_FILE) return -1;
    size_t n=f->size<len?f->size:len;
    memcpy(buf,f->data,n);
    return (int)n;
}

int ramfs_delete(ramfs_node_t *parent, const char *name){
    if(!parent) return -1;
    for(int i=0;i<parent->nchildren;i++){
        if(strcmp(parent->children[i]->name,name)==0){
            /* Não apaga diretório com filhos */
            if(parent->children[i]->type==NODE_DIR &&
               parent->children[i]->nchildren>0) return -2;
            parent->children[i]->type=NODE_FREE;
            /* Remove da lista */
            for(int j=i;j<parent->nchildren-1;j++)
                parent->children[j]=parent->children[j+1];
            parent->nchildren--;
            return 0;
        }
    }
    return -1;
}

void ramfs_abs_path(ramfs_node_t *node, char *out){
    if(!node){ out[0]='/'; out[1]=0; return; }
    if(node==root_node){ out[0]='/'; out[1]=0; return; }

    /* Constrói o caminho de baixo para cima */
    char parts[16][RAMFS_NAME_MAX];
    int depth=0;
    ramfs_node_t *cur=node;
    while(cur && cur!=root_node && depth<16){
        strncpy(parts[depth++],cur->name,RAMFS_NAME_MAX-1);
        cur=cur->parent;
    }
    out[0]=0;
    for(int i=depth-1;i>=0;i--){
        strcat(out,"/");
        strcat(out,parts[i]);
    }
    if(out[0]==0){ out[0]='/'; out[1]=0; }
}
