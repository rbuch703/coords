; main.c : http://pastebin.com/f6wEvwTq
; nasm -f elf64 -o object/printnum.o printnum.asm
; gcc -o bin/printnum object/printnum.o main.c -m64

section .text
    global mul
    global add
    global add3
    global add128
    global sub
    global subc
    global div128
    global sub128

; cheat sheet: 
; - parameter order for integer/pointer is RDI, RSI, RDX, RCX, R8, R9
; - registers that must be saved/restored: RBX, RBP, R12-R15

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
    
mul: ; uint64_t mul (uint64_t a, uint64_t b, uint64_t *hi);
    ; computes a*b = hi:lo; return value is lo, hi is return via 'hi'
    mov RAX, RDI
    mov RCX, RDX    ;backup RDX (third operand), mul will overwrite it
    mul RSI         ; RDX:RAX = RAX * RSI
    mov [RCX], RDX
    ret
    
add: ; uint64_t add(uint64_t a, uint64_t b, uint64_t *carry);
    
    xor RCX, RCX    ; RCX = 0
    mov RAX, RDI    ; RAX = a
    add RAX, RSI    ; RAX = a+b
    
    adc RCX, RCX    ; RCX = carry(a+b)
    mov [RDX], RCX  ; *carry = RCX
    ret    

add128: ; uint64_t add128(uint64_t a_hi/RDI, uint64_t a_lo/RSI, uint64_t b_hi/RDX, uint64_t b_lo/RCX, uint64_t *res_hi/R8, uint64_t *res_lo/R9);
    ;computes (a_hi:a_lo - b_hi:b_lo) in res_hi:res_lo; return value is the over/underflow
    xor RAX, RAX    ; RAX = 0
    add RSI, RCX    ; RSI = a_lo + b_lo
    adc RDI, RDX    ; RDI = a_hi + b_hi + carry(a_lo + b_lo);
    
    adc RAX, RAX    ; RAX = carry( adc ...)    --> RAX is return value
    
    mov [R8], RDI
    mov [R9], RSI
    
    ret


sub: ; uint64_t sub(uint64_t a, uint64_t b, uint64_t *borrow);
    ; computes a-b, return result (return value) and the borrow/carry in 'borrow'
    xor RCX, RCX
    mov RAX, RDI
    sub RAX, RSI
    
    adc RCX, RCX
    mov [RDX], RCX
    ret

sub128: ; uint64_t sub(uint64_t a_hi/RDI, uint64_t a_lo/RSI, uint64_t b_hi/RDX, uint64_t b_lo/RCX, uint64_t *res_hi/R8, uint64_t *res_lo/R9);
    ;computes (a_hi:a_lo - b_hi:b_lo) in res_hi:res_lo; return value is the over/underflow
    xor RAX, RAX    ; RAX = 0
    sub RSI, RCX    ; RSI = a_lo - b_lo
    sbb RDI, RDX    ; RDI = a_hi - b_hi - borrow(a_lo - b_lo);
    
    adc RAX, RAX    ; RAX = borrow( sbb ...)    --> RAX is return value
    
    mov [R8], RDI
    mov [R9], RSI
    
    ret

div128: ; uint64_t div128(uint64_t *a_hi/[RDI], uint64_t *a_lo/[RSI], uint64_t b/RDX)
    ; computes a_hi:a_lo / b; result is stored in a_hi:a_lo, return value is the modulus;
    mov RCX, RDX    ; RCX = b
    xor RDX, RDX    ; RDX = 0
    mov RAX, [RDI]  ; RAX = a_hi
    div RCX         ; RAX = (0:a_hi)/b; RDX = a_hi % b
    mov [RDI], RAX  ; *a_hi = RAX
    mov RAX, [RSI]  ; RAX = a_lo
    div RCX         ; RAX = (mod:a_lo)/b; RDX = (mod:a_lo)/b
    mov [RSI], RAX  ; *a_lo = RAX
    mov RAX, RDX    ; modulus -> RAX (return value)
    ret

add3: ; uint64_t add3(uint64_t a/RDI, uint64_t b/RSI, uint64_t c/RDX, uint64_t *carry/RCX);
    ; computes a+b+c and return the overflow in 'carry'
    xor RAX, RAX    ; RAX = 0
    ;mov RAX,     ; RAX = a
    add RDI, RSI    ; RDI = a+b
    adc RAX, 0      ; RAX = carry(a+b)
    
    add RDI, RDX    ; RDI = RDI + c
    adc RAX, 0      ; RAX+= carry(RDI + c)
    
    mov [RCX], RAX  ; *carry = RAX
    mov RAX, RDI
    ret    


