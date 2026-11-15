obj = pminit.obj pm_lib.obj
bin = pminit.exe

CC = wcc
CFLAGS = -za99 -0 -s -d2
LD = wlink

# 'system dos' is mandatory: without it wlink falls back to the WLSYSTEM
# default (win16 on a modern Open Watcom install) and silently produces an
# NE binary that DOS refuses with "This is a Windows 16-bit executable".
$(bin): $(obj)
	$(LD) system dos name $@ file pminit.obj, pm_lib.obj

fstest.exe : fstest.obj
#    	$(LD) name $@ file fstest.obj -fm=fstest.map
       wcl -y -0 -S -s -d0 -lr -ms -wx -k1024 -fm=fstest.map -os fstest.c -fe=fstest.exe

.c.obj:
	$(CC) -fo=$@ $(CFLAGS) $<

clean:
	del *.obj
	del $(bin)
