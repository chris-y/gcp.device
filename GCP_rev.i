VERSION		EQU	1
REVISION	EQU	2

DATE	MACRO
		dc.b '2.5.2017'
		ENDM

VERS	MACRO
		dc.b 'GCP 1.2'
		ENDM

VSTRING	MACRO
		dc.b 'GCP 1.2 (2.5.2017)',13,10,0
		ENDM

VERSTAG	MACRO
		dc.b 0,'$VER: GCP 1.2 (2.5.2017)',0
		ENDM