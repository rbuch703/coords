; main.c : http://pastebin.com/f6wEvwTq
; nasm -f elf64 -o object/printnum.o printnum.asm
; gcc -o bin/printnum object/printnum.o main.c -m64

section .text
    global sum
    global mul
    global add

; cheat sheet: parameter order is RDI, RSI, RDX, RCX

sum:
;    enter 0, 0

;    mov rax, [rbp + 8]  ; Get the function args from the stac
;    xor rbx, rbx
;    xor rcx, rcx
;    xor rdx, rdx

    mov RAX, RDI
    add RAX, RSI 

;   leave

    ret         ; return to the C routine
    
mul:
    mov RAX, RDI
    mov RBX, RDX    ;backup RDX (third operand), mul will overwrite it
    mul RSI         ; RDX:RAX = RAX * RSI
    mov [RBX], RDX
    ret
    
add: ; uint64_t add(uint64_t a, uint64_t b, uint64_t *carry);
    xor RBX, RBX    ; RBX = 0
    mov RAX, RDI    ; RAX = a
    add RAX, RSI    ; RAX = a+b
    
    adc RBX, RBX    ; RBX = carry(a+b)
    mov [RDX], RBX  ; *carry = RBX
    ret    

