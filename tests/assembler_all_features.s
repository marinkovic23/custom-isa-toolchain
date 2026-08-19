.global start, absolute_global
.extern external_symbol

.section text
.equ small_disp, 4
.equ absolute_global, 0x12345678
.equ distance, finish - start
start:
    halt
    int
    push %r1
    pop %r2
    xchg %r1, %r2
    add %r1, %r2
    sub %r1, %r2
    mul %r1, %r2
    div %r1, %r2
    not %r2
    and %r1, %r2
    or %r1, %r2
    xor %r1, %r2
    shl %r1, %r2
    shr %r1, %r2
    csrrd %status, %r1
    csrwr %r1, %handler
    ld $5, %r1
    ld $0x12345678, %r1
    ld $absolute_global, %r1
    ld 4, %r1
    ld 0x12345678, %r1
    ld external_symbol, %r1
    ld %r2, %r1
    ld [%r2], %r1
    ld [%r2 + 4], %r1
    ld [%r2 + small_disp], %r1
    st %r1, 4
    st %r1, 0x12345678
    st %r1, external_symbol
    st %r1, [%r2]
    st %r1, [%r2 + 4]
    st %r1, [%r2 + small_disp]
    call 16
    call 0xF0000000
    call external_symbol
    beq %r1, %r2, finish
    bne %r1, %r2, 16
    bgt %r1, %r2, external_symbol
    jmp finish
finish:
    iret
    ret

.section data
.word 1, -1, 0x89ABCDEF, start, external_symbol, absolute_global, distance
.skip 3
.ascii "a,b#c\n"
.end
this text must be ignored completely
