.global start
.equ OFFSET4, 4

.section code
start:
    ld $data_start, %r1

    ld $0x11223344, %r2
    st %r2, [%r1]
    ld [%r1], %r3

    ld $0x55667788, %r2
    st %r2, [%r1 + 4]
    ld [%r1 + OFFSET4], %r4

    ld $0xAABBCCDD, %r2
    st %r2, data_two
    ld data_two, %r5
    ld %r5, %r6

    st %r6, 0x00001000
    ld 0x00001000, %r7

    st %r7, 0x12345000
    ld 0x12345000, %r8

    halt

.section data
data_start:
    .word 0
data_one:
    .word 0
data_two:
    .word 0
.end
