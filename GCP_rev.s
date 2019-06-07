VERSION = 1
REVISION = 2

.macro DATE
.ascii "2.5.2017"
.endm

.macro VERS
.ascii "GCP 1.2"
.endm

.macro VSTRING
.ascii "GCP 1.2 (2.5.2017)"
.byte 13,10,0
.endm

.macro VERSTAG
.byte 0
.ascii "$VER: GCP 1.2 (2.5.2017)"
.byte 0
.endm
