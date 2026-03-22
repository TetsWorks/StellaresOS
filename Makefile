# ============================================================
#  StellaresOS -- Makefile principal
# ============================================================
AS     = nasm
CC     = clang
LD     = ld.lld
KERNEL = stellares.elf
DISK   = stellares.img

ASFLAGS = -f elf32

CFLAGS = --target=i386-pc-none-elf \
         -march=i486 -mno-sse -mno-mmx \
         -ffreestanding -fno-builtin -fno-stack-protector \
         -fno-pie -fno-PIC -nostdlib -nostdinc \
         -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable \
         -O1 -std=c11 -I. -Iinclude

LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

ASM_SRCS = boot/boot.asm boot/isr.asm

C_SRCS = kernel/kernel.c kernel/idt.c kernel/pmm.c \
         kernel/heap.c kernel/scheduler.c kernel/login.c \
         kernel/installer.c kernel/syscall.c kernel/elf_loader.c \
         drivers/vga.c drivers/serial.c drivers/pit.c \
         drivers/keyboard.c drivers/ata.c \
         libc/string.c \
         fs/ramfs.c fs/diskfs.c \
         pkg/spk.c \
         shell/stellash.c shell/editor.c shell/snake.c

ASM_OBJS = $(ASM_SRCS:.asm=.o)
C_OBJS   = $(C_SRCS:.c=.o)

.PHONY: all clean run run-gui run-disk apps install-apps

# ---- Kernel ----
all: $(KERNEL)
	@echo ""
	@echo "  *** StellaresOS compilado! ***"
	@echo ""
	@echo "  make run-gui      -> QEMU janela VGA (sem disco)"
	@echo "  make run-disk     -> QEMU janela VGA + disco 64MB"
	@echo "  make apps         -> compila os apps da pasta apps/"
	@echo "  make install-apps -> instala apps no disco"
	@echo ""

$(KERNEL): $(ASM_OBJS) $(C_OBJS) linker.ld
	@echo "  [LD] $@"
	$(LD) $(LDFLAGS) -o $@ $(ASM_OBJS) $(C_OBJS)

%.o: %.asm
	@echo "  [AS] $<"
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c
	@echo "  [CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	find . -name "*.o" -delete
	rm -f $(KERNEL)
	$(MAKE) -C apps clean 2>/dev/null || true

# ---- Apps ----
apps:
	@echo ""
	@echo "  Compilando apps..."
	$(MAKE) -C apps
	@echo ""

# Instala apps no disco — cria o disco se não existir
install-apps: apps $(DISK)
	@echo ""
	@echo "  Instalando apps no disco $(DISK)..."
	$(MAKE) -C apps install DISK=../$(DISK)
	@echo ""
	@echo "  Feito! Use make run-disk e dentro do sistema:"
	@echo "    exec hello"
	@echo "    exec sysinfo"
	@echo ""

# Cria disco vazio se não existir
$(DISK):
	@echo "  Criando disco $(DISK) (64MB)..."
	dd if=/dev/zero of=$(DISK) bs=1M count=64 2>/dev/null
	@echo "  Disco criado."

# ---- QEMU ----
run-gui: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -m 64M \
	  -vga std -serial stdio -no-reboot

run-disk: $(KERNEL) $(DISK)
	qemu-system-i386 \
	  -kernel $(KERNEL) \
	  -m 64M \
	  -drive file=$(DISK),format=raw,if=ide,cache=writeback \
	  -vga std \
	  -serial stdio \
	  -no-reboot

run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -m 64M \
	  -nographic -serial mon:stdio -no-reboot
