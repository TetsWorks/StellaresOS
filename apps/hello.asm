; ============================================================
;  hello — Primeiro app do StellaresOS
;  Assembly i386 puro, sem libc
;  Syscalls idênticas ao Linux i386
; ============================================================
bits 32
section .data
    msg     db "Ola, StellaresOS!", 10
    msg_len equ $ - msg

section .text
global _start
_start:
    mov eax, 4          ; sys_write
    mov ebx, 1          ; stdout
    mov ecx, msg
    mov edx, msg_len
    int 0x80

    mov eax, 1          ; sys_exit
    xor ebx, ebx
    int 0x80
