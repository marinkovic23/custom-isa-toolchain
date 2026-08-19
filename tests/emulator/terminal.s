.global start, counter

.section code
start:
    ld $0x1000, %sp
    ld $handler, %r1
    csrwr %r1, %handler

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

    ld 0xFFFFFF04, %r1
    st %r1, 0xFFFFFF00

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
