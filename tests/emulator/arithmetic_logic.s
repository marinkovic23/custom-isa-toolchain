.global start
.section code
start:
    ld $5, %r1
    ld $3, %r2
    add %r2, %r1
    sub %r2, %r1
    mul %r2, %r1
    div %r2, %r1
    xchg %r1, %r2
    not %r1

    ld $0xF0F0, %r3
    ld $0x0FF0, %r4
    and %r4, %r3
    or %r4, %r3
    xor %r4, %r3

    ld $1, %r5
    ld $4, %r6
    shl %r6, %r5
    shr %r6, %r5

    ld $0xFFFFFFFF, %r7
    ld $1, %r8
    add %r8, %r7

    ld $123, %r0
    halt
.end
