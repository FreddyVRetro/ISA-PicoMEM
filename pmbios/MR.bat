nasm -f bin -o pmbios.bin  -l pmbios.lst pmbios.asm -dFTYPE=0
trunc pmbios.bin 16384
romcksum32 -o pmbios.bin
copy pmbios.bin ..\src\rom\