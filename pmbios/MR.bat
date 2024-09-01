nasm -f bin -o pmbios.bin  -l pmbios.lst pmbios.asm -dFTYPE=0
trunc pmbios.bin 12288
romcksum32 pmbios.bin