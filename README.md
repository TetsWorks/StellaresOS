# StellaresOS

<p align="center">
  <img src="https://img.shields.io/badge/arquitetura-x86%20(i386%2Fi486)-blue?style=flat-square"/>
  <img src="https://img.shields.io/badge/licença-GPL%20v3.0-green?style=flat-square"/>
  <img src="https://img.shields.io/badge/versão-0.1-orange?style=flat-square"/>
  <img src="https://img.shields.io/badge/linguagem-C11%20%2B%20NASM-lightgrey?style=flat-square"/>
  <img src="https://img.shields.io/badge/status-em%20desenvolvimento-yellow?style=flat-square"/>
</p>

**StellaresOS** é um sistema operacional experimental de 32 bits para arquitetura x86, desenvolvido do zero em C11 e Assembly (NASM). O projeto tem fins educacionais e explora os fundamentos de um microkernel: boot via Multiboot, gerenciamento de memória, escalonamento preemptivo, sistema de arquivos, syscalls compatíveis com a ABI Linux i386 e um shell interativo próprio chamado **Stellash**.

---

## Índice

- [Funcionalidades](#funcionalidades)
- [Arquitetura do Sistema](#arquitetura-do-sistema)
- [Estrutura do Projeto](#estrutura-do-projeto)
- [Dependências](#dependências)
- [Como Compilar](#como-compilar)
- [Como Executar](#como-executar)
- [O Shell — Stellash](#o-shell--stellash)
- [Sistema de Arquivos](#sistema-de-arquivos)
- [Gerenciador de Pacotes — SPK](#gerenciador-de-pacotes--spk)
- [Syscalls](#syscalls)
- [Aplicações Nativas](#aplicações-nativas)
- [Drivers](#drivers)
- [Contribuindo](#contribuindo)
- [Licença](#licença)

---

## Funcionalidades

- Boot via protocolo **Multiboot** (compatível com GRUB e QEMU `-kernel`)
- **GDT, IDT e PIC 8259** configurados manualmente
- **PMM** (Physical Memory Manager) com bitmap de frames de 4KB
- **Heap** de 4 MB com alocador simples (`kmalloc` / `kfree`)
- **PIT** (Programmable Interval Timer) a 1000 Hz, base de tempo do sistema
- **Escalonador round-robin preemptivo** com troca de contexto real (salva/restaura registradores)
- **Syscalls** via `INT 0x80` com ABI compatível com Linux i386 (mesmos números de chamada)
- Driver de **VGA** em modo texto 80×25 com suporte a cores e cursor
- Driver de **teclado PS/2** com mapa de teclas PT-BR
- Driver **ATA PIO** para leitura e escrita em disco raw
- Driver de **porta serial** para debug (log em `stdio` do QEMU)
- **RamFS** — sistema de arquivos em memória com suporte a diretórios
- **DiskFS** — sistema de arquivos persistente no disco ATA (formato próprio `STEL`)
- **ELF Loader** para executar binários ELF32 nativos
- **Sistema de login** com usuários e senhas armazenados no disco
- **Instalador interativo** na primeira inicialização
- **Shell Stellash** com histórico, prompt colorido e comandos builtin
- **Editor de texto** interativo no terminal
- **Gerenciador de pacotes SPK** (Stellares Package)
- Jogo **Snake** embutido no shell

---

## Arquitetura do Sistema

```
┌─────────────────────────────────────┐
│              Stellash               │  ← Shell interativo (modo usuário)
│        Editor  │  Snake  │  Apps   │
├────────────────┴──────────┴─────────┤
│              Syscalls (INT 0x80)    │  ← ABI Linux i386
├─────────────────────────────────────┤
│              Kernel                 │
│  IDT/GDT │ PMM │ Heap │ Scheduler  │
│  ELF Loader │ Login │ Installer    │
├─────────────────────────────────────┤
│              Drivers                │
│  VGA │ Teclado │ ATA │ PIT │ Serial│
├─────────────────────────────────────┤
│          Sistemas de Arquivos       │
│         RamFS  │  DiskFS           │
├─────────────────────────────────────┤
│           Hardware (x86 i486)       │
└─────────────────────────────────────┘
```

### Mapa de memória

| Endereço           | Uso                      |
|--------------------|--------------------------|
| `0x00000000`       | Reservado / BIOS         |
| `0x00100000` (1MB) | Código do kernel         |
| `0x00400000` (4MB) | Heap do kernel (4 MB)    |
| `0x00800000` (8MB) | Stacks dos processos     |
| Acima              | Memória livre (PMM)      |

---

## Estrutura do Projeto

```
StellaresOS/
├── Makefile              # Sistema de build principal
├── linker.ld             # Script do linker (elf32, base 1MB)
├── boot/
│   ├── boot.asm          # Entry point Multiboot, configura GDT, chama kmain
│   └── isr.asm           # ISRs e IRQs em assembly, troca de contexto
├── kernel/
│   ├── kernel.c          # kmain: inicializa todos os subsistemas
│   ├── idt.c / idt.h     # Interrupt Descriptor Table + PIC 8259
│   ├── pmm.c / pmm.h     # Gerenciador de memória física (bitmap)
│   ├── heap.c / heap.h   # Alocador de heap do kernel
│   ├── scheduler.c / .h  # Escalonador round-robin preemptivo
│   ├── syscall.c / .h    # Handler de syscalls (INT 0x80)
│   ├── elf_loader.c / .h # Loader de ELF32 para userspace
│   ├── login.c / .h      # Sistema de autenticação de usuários
│   └── installer.c / .h  # Instalador interativo (primeira boot)
├── drivers/
│   ├── vga.c / vga.h     # Driver VGA modo texto 80×25
│   ├── keyboard.c / .h   # Driver teclado PS/2 (layout PT-BR)
│   ├── ata.c / ata.h     # Driver ATA PIO (leitura/escrita de setores)
│   ├── pit.c / pit.h     # PIT 8253/8254, base de tempo 1000 Hz
│   └── serial.c / .h     # Porta serial COM1 (debug via stdio)
├── fs/
│   ├── ramfs.c / ramfs.h # Sistema de arquivos em RAM com diretórios
│   └── diskfs.c / diskfs.h # DiskFS persistente no disco ATA
├── libc/
│   └── string.c / string.h # Implementação própria de funções de string
├── include/
│   ├── stdint.h          # Tipos inteiros (sem stdlib do host)
│   ├── stddef.h          # NULL, size_t
│   └── stdarg.h          # Suporte a variadic (vga_printf)
├── shell/
│   ├── stellash.c / .h   # Shell interativo com builtins e histórico
│   ├── editor.c / .h     # Editor de texto em modo texto VGA
│   └── snake.c / .h      # Jogo Snake
├── pkg/
│   └── spk.c / spk.h     # Gerenciador de pacotes SPK
└── apps/
    ├── Makefile           # Build dos apps nativos
    ├── install.py         # Script de instalação de apps no disco
    ├── hello.asm          # App de exemplo "Hello World" (ELF32)
    └── sysinfo.asm        # App de exemplo "Sysinfo" (ELF32)
```

---

## Dependências

Todas as ferramentas abaixo devem estar instaladas no host Linux:

| Ferramenta        | Versão mínima | Uso                              |
|-------------------|---------------|----------------------------------|
| `clang`           | 14+           | Compilador C (cross-compile i386)|
| `nasm`            | 2.15+         | Assembler                        |
| `ld.lld`          | 14+           | Linker ELF32                     |
| `qemu-system-i386`| 6.0+          | Emulação e testes                |
| `python3`         | 3.8+          | Script de instalação de apps     |
| `dd`, `make`      | —             | Build e criação de imagem de disco|

### Instalando no Ubuntu / Debian

```bash
sudo apt update
sudo apt install clang lld nasm qemu-system-x86 python3 make
```

### Instalando no Arch Linux

```bash
sudo pacman -S clang lld nasm qemu-arch-extra python make
```

### Instalando no macOS (Homebrew)

```bash
brew install llvm nasm qemu python3
# Adicionar o llvm ao PATH conforme indicado pelo brew
```

---

## Como Compilar

```bash
# Clonar o repositório
git clone https://github.com/seu-usuario/StellaresOS.git
cd StellaresOS

# Compilar o kernel
make

# Compilar os apps nativos (ELF32)
make apps

# Criar disco de 64 MB e instalar os apps
make install-apps
```

A compilação gera o arquivo `stellares.elf` (kernel ELF32) e, opcionalmente, `stellares.img` (disco raw de 64 MB).

---

## Como Executar

### Sem disco (modo RAM)

Inicia sem persistência. Útil para testar o kernel rapidamente.

```bash
make run-gui
```

O QEMU abre uma janela VGA. A saída de boot também aparece no terminal via porta serial.

### Com disco ATA (modo completo)

Cria automaticamente o disco `stellares.img` se não existir, depois inicia com suporte a DiskFS, login e pacotes.

```bash
make run-disk
```

Na **primeira inicialização**, o instalador interativo será exibido solicitando nome de usuário e senha. Nas inicializações seguintes, o sistema pedirá login.

### Modo sem interface gráfica (headless)

```bash
make run
```

Redireciona o display VGA para o terminal via `-nographic`. Útil para automação e CI.

---

## O Shell — Stellash

O **Stellash** é o shell interativo do StellaresOS. Possui prompt colorido com usuário, hostname e diretório atual, além de histórico de comandos (↑ / ↓).

```
stella@stellaresos:/$ 
```

### Comandos disponíveis

#### Sistema
| Comando       | Descrição                                     |
|---------------|-----------------------------------------------|
| `help`        | Exibe a lista de comandos disponíveis         |
| `clear`       | Limpa a tela                                  |
| `uname`       | Informações do sistema                        |
| `uptime`      | Tempo de atividade do sistema                 |
| `mem`         | Uso de memória física e heap                  |
| `neofetch`    | Informações do sistema em estilo neofetch     |
| `reboot`      | Reinicia o sistema via teclado PS/2           |
| `halt`        | Encerra o sistema                             |

#### Processos
| Comando        | Descrição                                    |
|----------------|----------------------------------------------|
| `ps`           | Lista os processos em execução               |
| `kill <pid>`   | Encerra um processo pelo PID                 |

#### Sistema de Arquivos (RamFS)
| Comando              | Descrição                              |
|----------------------|----------------------------------------|
| `ls [dir]`           | Lista arquivos e diretórios            |
| `cd <dir>`           | Muda o diretório atual                 |
| `pwd`                | Exibe o diretório atual                |
| `cat <arquivo>`      | Exibe o conteúdo de um arquivo         |
| `touch <arquivo>`    | Cria um arquivo vazio                  |
| `mkdir <dir>`        | Cria um diretório                      |
| `rm <arquivo>`       | Remove um arquivo ou diretório         |
| `cp <orig> <dest>`   | Copia um arquivo                       |
| `mv <orig> <dest>`   | Move/renomeia um arquivo               |
| `write <arq> <texto>`| Escreve texto em um arquivo            |
| `edit <arquivo>`     | Abre o editor de texto interativo      |

#### Disco (DiskFS)
| Comando      | Descrição                                      |
|--------------|------------------------------------------------|
| `disk`       | Informações sobre o disco e DiskFS             |
| `ls` (sem argumento) | Lista arquivos persistidos no disco    |
| `exec <app>` | Carrega e executa um ELF32 do disco            |

#### Pacotes
| Comando       | Descrição                                     |
|---------------|-----------------------------------------------|
| `spk list`    | Lista os pacotes instalados                   |
| `spk install <arquivo.spk>` | Instala um pacote SPK           |
| `spk remove <nome>` | Remove um pacote instalado              |

#### Entretenimento
| Comando  | Descrição                                         |
|----------|---------------------------------------------------|
| `snake`  | Inicia o jogo Snake (teclas WASD, Q para sair)    |

#### Outros
| Comando       | Descrição                                     |
|---------------|-----------------------------------------------|
| `echo <texto>`| Imprime texto na tela                         |

---

## Sistema de Arquivos

O StellaresOS possui dois sistemas de arquivos complementares:

### RamFS

Sistema de arquivos hierárquico em memória RAM. Os dados são perdidos ao reiniciar. Suporta arquivos e diretórios aninhados. É onde o shell opera por padrão.

### DiskFS

Sistema de arquivos persistente escrito diretamente no disco ATA. Usa um formato próprio identificado pela assinatura `STEL` (`0x5354454C`).

**Layout do disco:**

| Setor(es)  | Conteúdo                                  |
|------------|-------------------------------------------|
| 0          | Superbloco (magic, versão, nfiles)        |
| 1–4        | Tabela de inodes (32 arquivos máx, 64B cada) |
| 5+         | Blocos de dados (4 KB = 8 setores por arquivo) |

Limitações atuais: máximo de 32 arquivos, 4 KB por arquivo.

---

## Gerenciador de Pacotes — SPK

O **SPK** (Stellares Package) é o gerenciador de pacotes nativo. Pacotes `.spk` são arquivos binários com um cabeçalho estruturado contendo metadados e os arquivos a instalar.

**Estrutura de um pacote `.spk`:**

```
[spk_header_t]        ← magic, versão, nome, autor, descrição, nfiles
[spk_file_entry_t]×N  ← path, offset, size, mode por arquivo
[dados dos arquivos]  ← conteúdo bruto concatenado
```

O magic number é `0x4B505453` (`"STPK"`). Pacotes são instalados no DiskFS.

---

## Syscalls

As syscalls são invocadas via `INT 0x80`, com a mesma ABI do Linux i386:

- `eax` = número da syscall
- `ebx`, `ecx`, `edx` = argumentos

| Número | Nome         | Descrição                          |
|--------|--------------|------------------------------------|
| 1      | `exit`       | Encerra o processo atual           |
| 2      | `fork`       | (stub)                             |
| 3      | `read`       | Leitura de arquivo/teclado         |
| 4      | `write`      | Escrita em arquivo/VGA             |
| 5      | `open`       | Abre um arquivo no DiskFS          |
| 6      | `close`      | Fecha um descritor                 |
| 7      | `waitpid`    | Aguarda término de processo        |
| 20     | `getpid`     | Retorna o PID do processo atual    |
| 45     | `brk`        | Ajuste de heap (stub)              |
| 90     | `mmap`       | Mapeamento de memória (stub)       |
| 91     | `munmap`     | Desmapeamento (stub)               |
| 122    | `uname`      | Informações do sistema (`utsname`) |
| 252    | `exit_group` | Encerra grupo de threads           |

A compatibilidade com a ABI Linux i386 permite que binários ELF32 compilados para Linux sejam executados com adaptações mínimas.

---

## Aplicações Nativas

Apps são binários ELF32 compilados com NASM e executados via `exec <nome>` no Stellash. Dois exemplos estão incluídos:

### hello

Exibe uma mensagem de boas-vindas via syscall `write`.

```bash
exec hello
```

### sysinfo

Exibe informações do sistema (hostname, release, versão, arquitetura) via syscall `uname`.

```bash
exec sysinfo
```

Para compilar e instalar os apps no disco:

```bash
make apps
make install-apps
# Dentro do QEMU:
exec hello
exec sysinfo
```

---

## Drivers

| Driver      | Arquivo                    | Descrição                                               |
|-------------|----------------------------|---------------------------------------------------------|
| VGA         | `drivers/vga.c`            | Modo texto 80×25, cores, cursor, scroll, printf         |
| Teclado PS/2| `drivers/keyboard.c`       | Leitura de scancodes, layout PT-BR, teclas especiais    |
| ATA PIO     | `drivers/ata.c`            | Leitura/escrita de setores, detecção e identificação    |
| PIT         | `drivers/pit.c`            | Timer a 1000 Hz, `pit_sleep_ms`, `pit_seconds`          |
| Serial      | `drivers/serial.c`         | COM1 para log de debug, visível no terminal do QEMU     |

---

## Contribuindo

Contribuições são bem-vindas! Para colaborar:

1. Faça um fork do repositório
2. Crie uma branch para sua feature ou correção:
   ```bash
   git checkout -b feature/minha-feature
   ```
3. Faça suas alterações e escreva commits descritivos
4. Abra um Pull Request descrevendo o que foi feito

### Áreas que precisam de contribuição

- Suporte a `fork()` real e modo usuário com privilégios separados
- Paginação (x86 paging) e proteção de memória por processo
- Driver de rede (NE2000 via QEMU)
- Expansão do DiskFS (suporte a diretórios e arquivos maiores)
- Mais syscalls (mmap, select, etc.)
- Port de uma libc mínima (musl ou newlib)
- Documentação adicional

---

## Licença

```
StellaresOS — Copyright (C) 2026

Este programa é software livre: você pode redistribuí-lo e/ou modificá-lo
sob os termos da Licença Pública Geral GNU conforme publicada pela Free
Software Foundation, na versão 3 da Licença

Este programa é distribuído na esperança de que seja útil, mas SEM
NENHUMA GARANTIA; sem mesmo a garantia implícita de COMERCIABILIDADE ou
ADEQUAÇÃO A UM DETERMINADO FIM. Veja a Licença Pública Geral GNU para
mais detalhes.

Você deve ter recebido uma cópia da Licença Pública Geral GNU junto com
este programa. Se não, consulte <https://www.gnu.org/licenses/>.
```

Este projeto é licenciado sob a **GNU General Public License v3.0 (GPL-3.0)**.  
Veja o arquivo [`LICENSE`](LICENSE) para o texto completo da licença.

---

<p align="center">
  Feito pela TetsWorks
</p>
