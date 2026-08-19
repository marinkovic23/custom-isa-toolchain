.global start
.extern external_symbol
.equ SMALL_OFFSET, 4

.section code
start:
    ld $0x12345678, %r1
    ld $external_symbol, %r2
    ld external_symbol, %r3
    st %r1, external_symbol
    call external_symbol
    jmp external_symbol
    beq %r1, %r2, external_symbol
    ld [%r1 + SMALL_OFFSET], %r4
    st %r4, [%r1 + SMALL_OFFSET]
    halt
.end
