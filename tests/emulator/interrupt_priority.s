.global start, counter, first_cause, second_cause
.section code
start:
    ld $0x00010000, %sp
    ld $handler, %r1
    csrwr %r1, %handler

    # Globally mask asynchronous interrupts while both sources become pending.
    ld $4, %r1
    csrwr %r1, %status
    ld $0, %r1
    st %r1, 0xFFFFFF10

    ld $3000000, %r2
    ld $1, %r3
masked_delay:
    sub %r3, %r2
    bne %r2, %r0, masked_delay

    ld $0, %r1
    csrwr %r1, %status

wait:
    ld counter, %r1
    ld $2, %r2
    bne %r1, %r2, wait

    ld first_cause, %r6
    ld second_cause, %r7
    halt

handler:
    push %r1
    push %r2
    push %r3

    csrrd %cause, %r1
    ld counter, %r2
    beq %r2, %r0, store_first
    st %r1, second_cause
    jmp increment

store_first:
    st %r1, first_cause

increment:
    ld $1, %r3
    add %r3, %r2
    st %r2, counter

    pop %r3
    pop %r2
    pop %r1
    iret

.section data
counter:
    .word 0
first_cause:
    .word 0
second_cause:
    .word 0
.end
