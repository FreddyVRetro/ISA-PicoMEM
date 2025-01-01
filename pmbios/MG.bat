nasm -f bin -o pmbios.com  -l pmbios.lst pmbios.asm -dFTYPE=1
copy pmbios.com ..\..\..\DOS\