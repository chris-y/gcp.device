VERSION = 1
REVISION = 4

.macro DATE
.ascii "6.5.2017"
.endm

.macro VERS
.ascii "gcp 1.4"
.endm

.macro VSTRING
.ascii "gcp 1.4 (6.5.2017)"
.byte 13,10,0
.endm

.macro VERSTAG
.byte 0
.ascii "$VER: gcp 1.4 (6.5.2017)"
.byte 0
.endm
