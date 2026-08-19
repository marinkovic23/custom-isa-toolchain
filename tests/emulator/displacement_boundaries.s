.global start
.section code
start:
    ld $0, %r1
    ld $2047, %r2
    ld $-2048, %r3
    ld $-1, %r4
    halt
.end
