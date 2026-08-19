.global third
.extern ext
.section text
third:
.word local
local:
.word ext
.section extra
.word third
