; This file is a part of CuteMouse source
; Here are messages in Portugal Portugese
; Translated by Alain Mouette <alainm@pobox.com>
;
; Note: „” was used instead of Æä (ao-tilde)
;       this allows display both in codepage 437 and 850

E_needvga       db 'VGA nÆo encontrado, use versÆo anterior caso precise de EGA RIL',nl,eos
E_error		db nl,'Erro: op‡„o inv lida',eos
E_option	db eos
E_argument	db ' argumento',eos
E_help		db 5,nl,'Digite "ctmouse /?" para ajuda',eos

E_mousepresent	db 5,'Driver de rato j  instalado',eos
E_notfound	db 5,'Erro: dispositivo n„o encontrado',eos
;;+E_noIRQ	db 5,'Erro: IRQ n„o detectado',eos

E_nocute	db 1,'Driver CuteMouse n„o foi instalado!',eos
E_notunload	db 2,'N„o pode retirar driver da mem¢ria: alguma interup‡„o foi interceptada...',eos
S_unloaded	db 0,'Driver retirado da mem¢ria com sucesso...',eos

S_reset		db 'Driver reconfigurado para ',eos
S_installed	db 'Instalado em ',eos
S_atPS2		db 'porta PS/2',eos
S_atCOM		db 'COM'
com_port	db ?,' ('
S_atIO		db '0000h/IRQ'
IRQno		db ?
S_in		db ') em modo ',eos
S_inMSYS	db 'Mouse Systems',eos
S_inMS		db 'Microsoft',eos
S_inLT		db 'Logitech',eos
S_wheel		db ' (wheel-mouse)'
S_CRLF		db nl,eos

Copyright	db nl,'CuteMouse v',CTMRELEASE,' [FreeDOS]',nl,eos
Syntax		label byte
    db 0,'Op‡”es:',nl
    db '  /P	   - for‡a rato modo PS/2',nl
    db '  /S[c[i]] - for‡a rato modo serial em COM port c (1-4) com IRQ = i (1-7)',nl
;;+ db '  /S$a[,i] - for‡a rato modo serial em I/O = a (em hexadecimal) com',nl
;;+ db '	     IRQ = i (1-7)',nl
    db '  /O       - detecta da roda do rato PS/2 or USB pela BIOS (pode travar)',nl
; 2008: made /Y (ignore MSys) the default and introduced /M (enable MSys)
;   db '  /Y	   - do not try Mouse Systems mode for non-PnP devices',nl,nl
    db '  /M       - tenta rato *antigo* tipo Genius / Mouse System que nÆo ‚ PnP',nl,nl
    db '  /V	   - busca inreversa: procura PS/2 depois do rato serial',nl
    db '  /3	   - for‡a modo 3-bot”es (s¢ para ratos Microsoft e PS/2)',nl
    db '  /R[h[v]] - resolu‡Æo horizontal/vertical (h,v=0-9; op‡„o R faltando, sem',nl
    db '	     argumentos ou argumento=0 indica resolu‡„o autom tica, segundo',nl
    db '	     argumento faltando indica igual a primeiro argumento)',nl
    db '  /L	   - modo canhoto (default ‚ modo dextro)',nl
    db '  /B	   - n„o instala se outro driver j  instalado',nl
    db '  /N	   - carrega CuteMouse como um novo TSR, mesmo se j  esta instalado',nl
    db '	     (£til para arquivos batch que desinstalam CuteMouse no final)',nl
    db '  /W	   - n„o permite carregar TSR na UMB',nl
    db '  /U	   - desinstala driver',nl
    db '  /?	   - mostra esta ajuda',eos

