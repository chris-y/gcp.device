#
# Makefile for gcp.device
#

CC		= ppc-amigaos-gcc
CPP		= ppc-amigaos-g++
CFLAGS	= -gstabs -N -Wall -O2 -fomit-frame-pointer -c -fno-builtin \
		  -mmultiple -Wunused -D__USE_INLINE__ -D__NOGLOBALIFACE__ -I..
LDFLAGS	= -nostartfiles
LDLIBS	= -lcurl -lz -lnghttp2 -lssl -lcrypto -ljsmn
OBJS	= gcp_dev.o dev_68k.o token.o

gcp.device:	$(OBJS)
		$(CC) -N $(LDFLAGS) -o $@.debug $(OBJS) $(LDLIBS) -Wl,--cref,-M,-Map=$@.map
		ppc-amigaos-strip -R.comment -R.dstart -R.dend $@.debug -o gcp.device

gcp_dev.o: gcp_dev.c

dev_68k.o: dev_68k.c

token.o: token.c

clean:
		-rm $(OBJS) *.map *.device *.debug
