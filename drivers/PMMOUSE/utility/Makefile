# Macros for building, deleting ########################################

# AS=tasm -m @..\asmlib.cfg
# AS=jwasmd -mt @asmlib ... so we just hardcode the asmlib/ for includes  
AS=jwasm -mt -0
# CC=tcc
# CFLAGS        = -IC:\TC\INCLUDE -LC:\TC\LIB -f -c -w -ms -a- -k- -N- -d -O -O2
# CFLAGS=-IC:\TC\INCLUDE -LC:\TC\LIB -N -w -a- -f- -f87- -ms -r-
LINKEXE=wlink system dos
LINKCOM=wlink format dos com

RM=rm -f


# Rules to build files #################################################

# .c.obj:
# 	$(CC) $(CFLAGS) $*
.asm.o:
	$(AS) $*.asm


# Targets ##############################################################

all: comtest.com protocol.com mousetst.com

# com does not link...
comtest.com: comtest.o
	$(LINKCOM) file $*.o

# com does not link...
protocol.com: protocol.o
	$(LINKCOM) file $*.o

# mousetst.exe: mousetst.obj prf.obj
# for com use -mt -lt
# mousetst.exe:	mousetst.c prf.c
#		tcc -N -w -a- -f- -f87- -ms -r- \
#		-IC:\TC\INCLUDE -LC:\TC\LIB mousetst prf
# *** mousetst.exe:	mousetst.c
# *** 		tcc -N -w -a- -f- -f87- -ms -r- \
# *** 		-IC:\TC\INCLUDE -LC:\TC\LIB mousetst

mousetst.com: mousetst.asm display.asm
	nasm -o mousetst.com mousetst.asm

comtest.obj: comtest.asm ..\asmlib\asm.mac ..\asmlib\bios\area0.def \
		..\asmlib\macro.mac \
		..\asmlib\convert\digit.mac ..\asmlib\convert\count2x.mac \
		..\asmlib\dos\io.mac ..\asmlib\dos\mem.mac \
		..\asmlib\hard\pic8259a.def ..\asmlib\hard\uart.def

protocol.obj: protocol.asm ..\asmlib\asm.mac ..\asmlib\bios\area0.def \
		..\asmlib\macro.mac ..\asmlib\code.def ..\asmlib\code.mac \
		..\asmlib\convert\digit.mac ..\asmlib\convert\count2x.mac \
		..\asmlib\dos\io.mac ..\asmlib\dos\mem.mac \
		..\asmlib\hard\pic8259a.def ..\asmlib\hard\uart.def

# mousetst.obj: mousetst.c
# prf.obj: prf.c


# Clean up #############################################################

clean
	-$(RM) *.o
	-$(RM) *.obj
	-$(RM) *.com


