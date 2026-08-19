.global start

.section code
start:
    ld $0x1000, %sp
    ld $handler, %r1
    csrwr %r1, %handler

    #machine instruction with an unsupported operation code
    .word 0x000000A0
    halt

handler:
    csrrd %cause, %r1
    halt

.end
