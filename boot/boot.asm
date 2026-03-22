; ============================================================
;  StellaresOS -- boot/boot.asm
;  Multiboot, GDT, zeragem do BSS e entry point
; ============================================================
bits 32

MBOOT_MAGIC  equ 0x1BADB002
MBOOT_FLAGS  equ (1<<0)|(1<<1)
MBOOT_CHECK  equ -(MBOOT_MAGIC + MBOOT_FLAGS)

section .text
align 4

; Multiboot header no início do .text
mboot:
    dd MBOOT_MAGIC
    dd MBOOT_FLAGS
    dd MBOOT_CHECK

global _start
extern kmain
extern __bss_start
extern __bss_end

_start:
    cli

    ; Salva argumentos do bootloader na pilha temporária
    ; Usa posições fixas de memória para não perder durante rep stosd
    mov [tmp_magic], eax
    mov [tmp_mbi],   ebx

    ; Carrega GDT
    lgdt [gdt_descriptor]
    jmp 0x08:flush_cs
flush_cs:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Configura pilha
    mov esp, stack_top
    xor ebp, ebp

    ; Zera BSS (onde estão IDT, arrays globais, etc.)
    ; Isso é obrigatório — BSS não é zerado pelo QEMU automaticamente
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi        ; tamanho em bytes
    shr ecx, 2          ; em dwords
    xor eax, eax
    rep stosd

    ; Recupera argumentos
    mov eax, [tmp_magic]
    mov ebx, [tmp_mbi]

    ; Chama kmain(magic, mbi)
    push ebx
    push eax
    call kmain

.hang:
    cli
    hlt
    jmp .hang

; ============================================================
;  Armazenamento temporário para magic/mbi (em .data, não BSS)
; ============================================================
section .data
align 4
tmp_magic: dd 0
tmp_mbi:   dd 0

align 8
gdt_start:
    ; 0: Null
    dq 0
    ; 1: Kernel Code (0x08)
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
    ; 2: Kernel Data (0x10)
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
    ; 3: User Code (0x18)
    dw 0xFFFF, 0x0000
    db 0x00, 11111010b, 11001111b, 0x00
    ; 4: User Data (0x20)
    dw 0xFFFF, 0x0000
    db 0x00, 11110010b, 11001111b, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ============================================================
;  Pilha do kernel (16KB)
; ============================================================
section .bss
align 16
stack_bottom: resb 16384
stack_top:
