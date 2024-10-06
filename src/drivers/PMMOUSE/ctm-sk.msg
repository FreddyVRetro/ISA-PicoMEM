; This file is a part of CuteMouse source
; Here are messages in Slovak
; Translated by Henrich Fukna <henrich.fukna@kiwwi.sk>, <fuky@azet.sk>
;
; Note: this file should be used with codepage 852

E_needvga       db 'No VGA? Use older CTMOUSE if you need EGA RIL support',nl,eos
E_error		db nl,'Chyba: Nezn my ',eos
E_option	db 'prep¡naŸ',eos
E_argument	db 'argument prep¡naŸa',eos
E_help		db 5,nl,'Nap¡çte /? do pr¡kazov‚ho riadku pre n poveÔ',eos

E_mousepresent	db 5,'Slu§ba myçi u§ je pr¡tomn ',eos
E_notfound	db 5,'Chyba: zariadenie nen jden‚',eos
;;+E_noIRQ	db 5,'Chyba: IRQ nedetegovan‚',eos

E_nocute	db 1,'CuteMouse ovl daŸ nie je nainçtalovanì!',eos
E_notunload	db 2,'Ovl daŸ nemo§no odinçtalovaœ: niektor‚ preruçenia s£ zmenen‚...',eos
S_unloaded	db 0,'Ovl daŸ £speçne odinçtalovanì...',eos

S_reset		db 'Rezidentn  Ÿasœ nastaven  na ',eos
S_installed	db 'Inçtalovanì na ',eos
S_atPS2		db 'porte PS/2',eos
S_atCOM		db 'COM'
com_port	db ?,' ('
S_atIO		db '0000h/IRQ'
IRQno		db ?
S_in		db ') pre ',eos
S_inMSYS	db 'Mouse Systems m¢d',eos
S_inMS		db 'Microsoft m¢d',eos
S_inLT		db 'Logitech m¢d',eos
S_wheel		db ' (podpora kolieska)'
S_CRLF		db nl,eos

Copyright	db nl,'CuteMouse v',CTMRELEASE,' [FreeDOS]',nl,eos
Syntax		label byte
    db 0,'Mo§nosti:',nl
    db '  /P	   - vyn£ti PS/2 m¢d',nl
    db '  /S[c[i]] - vyn£ti s‚riovì m¢d na COM porte c (1-4) s IRQ i (1-7)',nl
;;+ db '  /S$a[,i] - vyn£ti s‚riovì m¢d na I/O adrese a (hexadecim lne Ÿ¡slo)',nl
;;+ db '	     s IRQ i (1-7)',nl
    db '  /O       - enable PS2 and BIOS USB wheel detection (might hang)',nl
; 2008: made /Y (ignore MSys) the default and introduced /M (enable MSys)
;   db '  /Y	   - do not try Mouse Systems mode for non-PnP devices',nl,nl
    db '  /M       - try *old* Mouse Systems / Genius for non-PnP mice',nl,nl
    db '  /V	   - opaŸn‚ h–adanie: PS/2 po s‚riovìch myçiach',nl
    db '  /3	   - vyn£ti 3-tlaŸ¡tkovì m¢d (pre Microsoft a PS/2 myçi)',nl
    db '  /R[h[v]] - horizont lne/vertik lne rozl¡çenie (h,v=0-9; chìbaj£ca vo–ba R,',nl
    db '	     §iadne argumenty alebo 0 ako argument po R znamen  automatick‚',nl
    db '	     rozl¡çenie, chìbaj£ci druhì argument znamen  rovnak£ hodnotu',nl
    db '	     ako pre prvì argument)',nl
    db '  /L	   - m¢d pre –av£ ruku (prednastaven‚ pre prav£ ruku)',nl
    db '  /B	   - preruç¡ vykon vanie ak slu§ba myçi je u§ pr¡tomn ',nl
    db '  /N	   - naŸ¡ta CuteMouse ako novì TSR, aj keÔ je u§ CuteMouse naŸ¡tanì',nl
    db '	     (vhodn‚ pre d vkov‚ s£bory ktor‚ odinçtal£vaj£ CuteMouse na konci)',nl
    db '  /W	   - zabr ni naŸ¡taniu TSR do UMB',nl
    db '  /U	   - odinçtaluje ovl daŸ',nl
    db '  /?	   - zobraz¡ n vod na pou§itie',eos
