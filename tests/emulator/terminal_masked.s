.global start, counter
.section code
start:
    ld $0x00010000, %sp
    ld $handler, %r1
    csrwr %r1, %handler

    # Mask terminal interrupts while input arrives.
    ld $2, %r1
    csrwr %r1, %status

    ld $200000, %r2
    ld $1, %r3
masked_delay:
    sub %r3, %r2
    bne %r2, %r0, masked_delay

    ld counter, %r3

    # Unmask. The pending terminal request must then be serviced.
    ld $0, %r1
    csrwr %r1, %status

wait:
    ld counter, %r1
    ld $1, %r2
    bne %r1, %r2, wait
    halt

handler:
    push %r1
    push %r2

    csrrd %cause, %r1
    ld $3, %r2
    bne %r1, %r2, finish

    ld 0xFFFFFF04, %r4
    st %r4, 0xFFFFFF00

    ld counter, %r1
    ld $1, %r2
    add %r2, %r1
    st %r1, counter

finish:
    pop %r2
    pop %r1
    iret

.section data
counter:
    .word 0
.end
