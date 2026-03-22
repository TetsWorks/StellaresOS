AS      = nasm
LD      = ld.lld
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -static

APPS = hello sysinfo

.PHONY: all clean install

all: $(APPS)
	@echo ""
	@echo "  Apps compilados:"
	@for app in $(APPS); do echo "    $$app  ($$(wc -c < $$app) bytes)"; done
	@echo ""
	@echo "  Para instalar: make install DISK=../stellares.img"
	@echo ""

hello: hello.asm
	@echo "  [AS+LD] hello"
	$(AS) $(ASFLAGS) $< -o hello.o
	$(LD) $(LDFLAGS) -o $@ hello.o
	@rm -f hello.o

sysinfo: sysinfo.asm
	@echo "  [AS+LD] sysinfo"
	$(AS) $(ASFLAGS) $< -o sysinfo.o
	$(LD) $(LDFLAGS) -o $@ sysinfo.o
	@rm -f sysinfo.o

install: all
	@if [ -z "$(DISK)" ]; then echo "Uso: make install DISK=../stellares.img"; exit 1; fi
	@if [ ! -f "$(DISK)" ]; then echo "Disco nao encontrado. Rode: make run-disk"; exit 1; fi
	python3 install.py $(DISK) $(APPS)

clean:
	rm -f $(APPS) *.o
