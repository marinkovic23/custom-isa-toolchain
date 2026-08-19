.global entry
.extern external_symbol
.equ CONSTANT, 0x12345678

.section text
entry:
    .word 1, -1, 0x89ABCDEF, CONSTANT
    .skip 2
    .ascii "A,\n#\x42\0"

.section data
local_data:
    .word entry, external_symbol

.end
this content is deliberately invalid and must be ignored
