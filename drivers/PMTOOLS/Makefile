obj = pminit.obj pm_lib.obj
bin = pminit.exe

CC = wcc
CFLAGS = -za99 -0 -s -d3
LD = wlink

$(bin): $(obj)
	$(LD) name $@ file pminit.obj, pm_lib.obj

.c.obj:
	$(CC) -fo=$@ $(CFLAGS) $<

clean:
	del *.obj
	del $(bin)
