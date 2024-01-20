; This file is a part of CuteMouse source
; Here are messages in Spanish
; Translated by Diego Rodr¡guez <dieymir@teleline.es>

E_needvga       db 'No VGA? Use older CTMOUSE if you need EGA RIL support',nl,eos
E_error		db nl,'Error: ',eos
E_option	db 'Opci¢n desconocida',eos
E_argument	db 'Par metro de opci¢n incorrecto',eos
E_help		db 5,nl,'Teclee "CTMOUSE /?" para obtener ayuda',eos

E_mousepresent	db 5,'Controlador de rat¢n ya instalado',eos
E_notfound	db 5,'Error: rat¢n no encontrado',eos
;;+E_noIRQ	db 5,'Error: IRQ no detectada',eos

E_nocute	db 1,'­CuteMouse no ha sido instalado!',eos
E_notunload	db 2,'Imposible desinstalar Cutemouse: alguna interrupci¢n interceptada...',eos
S_unloaded	db 0,'Cutemouse ha sido desinstalado correctamente...',eos

S_reset		db 'Controlador residente en ',eos
S_installed	db 'Instalado en ',eos
S_atPS2		db 'puerto PS/2',eos
S_atCOM		db 'COM'
com_port	db ?,' ('
S_atIO		db '0000h/IRQ'
IRQno		db ?
S_in		db ') usando protocolo ',eos
S_inMSYS	db 'Mouse Systems',eos
S_inMS		db 'Microsoft',eos
S_inLT		db 'Logitech',eos
S_wheel		db ' (detectada rueda)'
S_CRLF		db nl,eos

Copyright	db nl,'CuteMouse v',CTMRELEASE,' [FreeDOS]',nl,eos
Syntax		label byte
    db 0,'Opciones:',nl
    db '  /P	   - fuerza uso de rat¢n PS/2',nl
    db '  /S[c[i]] - fuerza uso de rat¢n serie en puerto COM c (1-4) con IRQ i (1-7)',nl
;;+ db '  /S$a[,i] - fuerza uso de rat¢n serie en la direcci¢n de E/S a (en',nl
;;+ db '	     hexadecimal) con IRQ i (1-7)',nl
    db '  /O       - enable PS2 and BIOS USB wheel detection (might hang)',nl
; 2008: made /Y (ignore MSys) the default and introduced /M (enable MSys)
;   db '  /Y	   - do not try Mouse Systems mode for non-PnP devices',nl,nl
    db '  /M       - try *old* Mouse Systems / Genius for non-PnP mice',nl,nl
    db '  /V	   - invierte orden de b£squeda: busca el rat¢n antes puerto serie',nl
    db '  /3	   - fuerza modo 3-botones (s¢lo para ratones Microsoft y PS/2)',nl
    db '  /R[h[v]] - resoluci¢n horizontal/vertical (h,v=0-9; especificar R sin',nl
    db '	     argumentos o argumento=0 indica resoluci¢n autom tica, sin',nl
    db '	     segundo argumento indica valor igual a primer argumento)',nl
    db '  /L	   - rat¢n para zurdos (por defecto para diestros)',nl
    db '  /B	   - no se instala si ya hay otro controlador de rat¢n instalado',nl
    db '  /N	   - instala CuteMouse como un nuevo TSR, aunque ya estuviese instalado',nl
    db '	     (£til en archivos "batch" que desinstalan CuteMouse al final)',nl
    db '  /W	   - no instalar en un bloque de memoria UMB',nl
    db '  /U	   - desinstalar el controlador',nl
    db '  /?	   - muestra esta ayuda',eos
