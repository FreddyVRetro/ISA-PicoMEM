REM This script will decompress all *.vgz-files in a directory to *.vgm
REM 
REM Note: Requires gzip.exe (http://gnuwin32.sourceforge.net)

copy *.vgz *.vgm.gz
gzip -d *.vgm.gz
