nasm -f bin -o pmbios.bin  -l pmbios.lst pmbios.asm -dFTYPE=0  -dPMVER=1
trunc pmbios.bin 16384
romcksum32 -o pmbios.bin
copy pmbios.bin ..\rom\

nasm -f bin -o pm2bios.bin  -l pmbios.lst pmbios.asm -dFTYPE=0 -dPMVER=2
trunc pm2bios.bin 16384
romcksum32 -o pm2bios.bin
copy pm2bios.bin ..\rom\pm2bios.bin