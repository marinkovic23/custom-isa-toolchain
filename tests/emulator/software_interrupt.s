.global start
.section code
start:
    ld $0x00010000, %sp
    ld $handler, %r1
    csrwr %r1, %handler

    int

    csrrd %status, %r6
    halt

handler:
    csrrd %cause, %r3
    csrrd %status, %r4
    ld $1, %r5
    iret
.end
