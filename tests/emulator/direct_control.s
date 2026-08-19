.global start

.section code
start:
    ld $0x00001000, %sp
    ld $1, %r1
    ld $1, %r2
    beq %r1, %r2, 0x100
    halt

.section equal_target
    ld $1, %r3
    ld $2, %r2
    bne %r1, %r2, 0x200
    halt

.section not_equal_target
    ld $1, %r4
    ld $-1, %r5
    ld $1, %r6
    bgt %r6, %r5, 0x300
    halt

.section greater_target
    ld $1, %r7
    call 0x400
    jmp 0x500
    halt

.section function_target
    ld $0x66, %r8
    ret

.section finish_target
    halt
.end
