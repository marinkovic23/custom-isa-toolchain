.global start
.section code
start:
    ld $0x00010000, %sp
    ld $0x12345678, %r1
    push %r1
    ld $0, %r1
    pop %r2

    call function

    ld $1, %r3
    ld $1, %r4
    beq %r3, %r4, equal_taken
    ld $0xDEAD0001, %r5

equal_taken:
    ld $2, %r4
    bne %r3, %r4, not_equal_taken
    ld $0xDEAD0002, %r5

not_equal_taken:
    ld $-1, %r6
    ld $1, %r7
    bgt %r7, %r6, greater_taken
    ld $0xDEAD0003, %r5

greater_taken:
    jmp done
    ld $0xDEAD0004, %r5

function:
    ld $0xABCDEF01, %r9
    ret

done:
    ld $0x55, %r8
    halt
.end
