.global start, absolute_value

.equ distance, finish - start
.equ absolute_value, base + 4
.equ negative_value, -(2 + 3)
.equ relocatable_value, start + 8
.equ base, 0x100

.section text
start:
    .word distance, absolute_value, negative_value, relocatable_value
finish:
    halt
.end
