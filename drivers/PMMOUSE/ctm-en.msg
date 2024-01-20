; This file is a part of CuteMouse source
; Here are messages in English

E_needvga       db 'No VGA? Use older CTMOUSE if you need EGA RIL support',nl,eos
E_error		db nl,'Error: Invalid option'
E_option	db eos
E_argument	db ' argument',eos
E_help		db 5,nl,'Enter /? on command line for help',eos

E_mousepresent	db 5,'Mouse services already present',eos
E_notfound	db 5,'Error: device not found',eos
;;+E_noIRQ	db 5,'Error: IRQ line not detected',eos

E_nocute	db 1,'CuteMouse driver is not installed!',eos
E_notunload	db 2,'Driver unload failed: some interrupts intercepted...',eos
S_unloaded	db 0,'Driver successfully unloaded...',eos

S_reset		db 'Resident part reset to ',eos
S_installed	db 'Installed at ',eos
S_atPS2		db 'PS/2 port',eos
S_atCOM		db 'COM'
com_port	db ?,' ('
S_atIO		db '0000h/IRQ'
IRQno		db ?
S_in		db ') in ',eos
S_inMSYS	db 'Mouse Systems mode',eos
S_inMS		db 'Microsoft mode',eos
S_inLT		db 'Logitech mode',eos
S_inPM      db 'PicoMEM mode',eos
S_wheel		db ' (wheel present)'
S_CRLF		db nl,eos

S_PM_NotDet db 'PicoMEM not found',nl,eos
S_PM_Det    db 'PicoMEM Detected',nl,eos

Copyright	db 'CuteMouse v',CTMRELEASE,' [FreeDOS]',nl,eos
Syntax		label byte
    db 0,nl,'Options:',nl
;if PM_KeepALL	
    db '  /V	   - reverse search: find PS/2 after serial mouse',nl
    db '  /P	   - force PS/2 mouse mode, do not probe serial ports',nl
    db '  /S[c[i]] - force serial mouse mode at COM port c (1-4) with IRQ i (1-7)',nl
    db '  /3	   - force 3-button mode if Microsoft or PS/2 mouse found',nl
; Note: /O now acts inverse compared to 2.1 beta 3
    db '  /O       - enable PS2 and BIOS USB wheel detection (might hang)',nl
; 2008: made /Y (ignore MSys) the default and introduced /M (enable MSys)
;   db '  /Y	   - do not try Mouse Systems mode for non-PnP devices',nl,nl
    db '  /M       - try *old* Mouse Systems / Genius for non-PnP mice',nl,nl
;endif	
    db '  /R[h[v]] - horizontal / vertical resolution: h,v = 1-9, or 0 for auto',nl
    db '	     (no value = use default: auto for h, or "as h" for v)',nl
    db '  /L	   - swap left and right button',nl,nl
;
    db '  /B	   - cancel run if mouse services are already present',nl
    db '  /N	   - load CuteMouse as new TSR, even if CuteMouse is already loaded',nl
    db '	     (useful for batch files which unload CuteMouse at end)',nl
    db '  /W	   - do not allow CuteMouse to move itself into UMB',nl
    db '  /U	   - uninstall driver, remove TSR from memory',nl
    db '  /?	   - show this help',eos
