; ============================================================
;  sysinfo — Mostra informacoes do StellaresOS
;  Usa sys_uname (syscall 122, identica ao Linux i386)
; ============================================================
bits 32

section .bss
    utsname resb 325    ; 5 campos x 65 bytes

section .data
    str_os   db "  Sistema:  ", 0
    str_ver  db "  Versao:   ", 0
    str_arch db "  Arch:     ", 0
    str_sep  db "  -------------------------", 10, 0
    str_hdr  db 10, "  === StellaresOS sysinfo ===", 10, 0
    newline  db 10, 0

section .text
global _start

; print_sz: imprime string null-terminated em esi
print_sz:
    mov ecx, esi
.find:
    cmp byte [ecx], 0
    je .write
    inc ecx
    jmp .find
.write:
    sub ecx, esi
    jz .done
    mov edx, ecx
    mov ecx, esi
    mov eax, 4
    mov ebx, 1
    int 0x80
.done:
    ret

_start:
    ; Header
    mov esi, str_hdr
    call print_sz
    mov esi, str_sep
    call print_sz

    ; sys_uname
    mov eax, 122
    mov ebx, utsname
    int 0x80

    ; Sistema (sysname, offset 0)
    mov esi, str_os
    call print_sz
    mov esi, utsname
    call print_sz
    mov esi, newline
    call print_sz

    ; Versao (release, offset 130)
    mov esi, str_ver
    call print_sz
    mov esi, utsname + 130
    call print_sz
    mov esi, newline
    call print_sz

    ; Arch (machine, offset 260)
    mov esi, str_arch
    call print_sz
    mov esi, utsname + 260
    call print_sz
    mov esi, newline
    call print_sz

    mov esi, str_sep
    call print_sz
    mov esi, newline
    call print_sz

    mov eax, 1
    xor ebx, ebx
    int 0x80
