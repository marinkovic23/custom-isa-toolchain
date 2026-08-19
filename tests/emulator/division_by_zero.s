.global start
.section code
start:
    ld $0x00010000, %sp
    ld $handler, %r1
    csrwr %r1, %handler
    ld $10, %r2
    ld $0, %r3
    div %r3, %r2
    halt

handler:
    csrrd %cause, %r4
    halt
.end
