; main.c : http://pastebin.com/f6wEvwTq
; nasm -f elf64 -o object/printnum.o printnum.asm
; gcc -o bin/printnum object/printnum.o main.c -m64

section .text
    global mul
    global add
    global add3
    global sub
    global subc
    global div128
    global sub128
;    global adc

; cheat sheet: parameter order is RDI, RSI, RDX, RCX, R8, R9

;sum:   ;kept as an example of how to built a stack frame
;    enter 0, 0

;    mov rax, [rbp + 8]  ; Get the function args from the stac
;    xor rbx, rbx
;    xor rcx, rcx
;    xor rdx, rdx

;    mov RAX, RDI
;    add RAX, RSI 

;   leave
;    ret         ; return to the C routine
    
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

sub: ; uint64_t sub(uint64_t a, uint64_t b, uint64_t *borrow);
    xor RBX, RBX
    mov RAX, RDI
    sub RAX, RSI
    
    adc RBX, RBX
    mov [RDX], RBX
    ret

sub128: ; uint64_t sub(uint64_t a_hi/RDI, uint64_t a_lo/RSI, uint64_t b_hi/RDX, uint64_t b_lo/RCX, uint64_t *res_hi/R8, uint64_t *res_lo/R9);
    xor RAX, RAX    ; RBX = 0
    sub RSI, RCX    ; RSI = a_lo - b_lo
    sbb RDI, RDX    ; RDI = a_hi - b_hi - borrow(a_lo - b_lo);
    
    adc RAX, RAX    ; RAX = borrow( sbb ...)    --> RAX is return value
    
    mov [R8], RDI
    mov [R9], RSI
    
    ret

div128: ; uint64_t div(uint64_t *a_hi/[RDI], uint64_t *a_lo/[RSI], uint64_t b/RDX)
    mov RBX, RDX    ; RBX = b
    xor RDX, RDX    ; RDX = 0
    mov RAX, [RDI]  ; RAX = a_hi
    div RBX         ; RAX = (0:a_hi)/b; RDX = a_hi % b
    mov [RDI], RAX  ; *a_hi = RAX
    mov RAX, [RSI]  ; RAX = a_lo
    div RBX         ; RAX = (mod:a_lo)/b; RDX = (mod:a_lo)/b
    mov [RSI], RAX  ; *a_lo = RAX
    mov RAX, RDX    ; modulus -> RAX (return value)

add3: ; uint64_t add3(uint64_t a, uint64_t b, uint64_t c, uint64_t *carry);
    xor RBX, RBX    ; RBX = 0
    mov RAX, RDI    ; RAX = a
    add RAX, RSI    ; RAX = a+b
    adc RBX, 0      ; RBX = carry(a+b)
    
    add RAX, RDX    ; RAX = RAX + c
    adc RBX, 0      ; RBX+= carry(RAX + c)
    
    mov [RDX], RBX  ; *carry = RBX
    ret    


