.global foo
.extern ext
.section text
foo:
.word local
local:
.word ext
.section data
.word 0x11111111
