.global ext
.extern foo
.section text
ext:
.word local
local:
.word 0x22222222
.section data
.word foo
