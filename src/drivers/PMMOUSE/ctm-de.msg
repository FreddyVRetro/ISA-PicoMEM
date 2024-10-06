; This file is a part of CuteMouse source
; Here are messages in German
; Translated by Frank PrÅfer <fp666@t-online.de>

E_needvga       db 'Kein VGA? Verwende ggf. Ñltere CTMOUSE mit EGA RIL Funktion',nl,eos
E_error		db nl,'FEHLER: ungÅltige',eos
E_option	db ' Option!',eos
E_argument	db 's Optionsargument!',eos
E_help		db 5,nl,'Geben Sie /? auf der Kommandozeile ein, um Hilfe zu erhalten.',eos

E_mousepresent	db 5,'Mausfunktionen sind bereits aktiv.',eos
E_notfound	db 5,'FEHLER: Keine Maus gefunden!',eos
;;+E_noIRQ	db 5,'FEHLER: IRQ-Leitung nicht erkannt!',eos

E_nocute	db 1,'Treiber wurde nicht installiert!',eos
E_notunload	db 2,'Treiber konnte nicht entfernt werden: einige Interrupts',nl
		db 'sind von spÑter installierten Programmen belegt...',eos
S_unloaded	db 0,'Treiber wurde erfolgreich entfernt...',eos

S_reset		db 'Residenter Teil zurÅckgesetzt auf ',eos
S_installed	db 'Installiert an ',eos
S_atPS2		db 'PS/2-Port',eos
S_atCOM		db 'COM'
com_port	db ?,' ('
S_atIO		db '0000h/IRQ'
IRQno		db ?
S_in		db ') im ',eos
S_inMSYS	db 'Mouse Systems Modus',eos
S_inMS		db 'Microsoft Modus',eos
S_inLT		db 'Logitech Modus',eos
S_wheel		db ' (mit Rad)'
S_CRLF		db nl,eos

Copyright	db nl,'CuteMouse v',CTMRELEASE,' [FreeDOS]',nl,eos
Syntax		label byte
    db 0,'Optionen:',nl
    db '  /P	   - PS/2-Mausmodus erzwingen',nl
    db '  /S[c[i]] - seriellen Mausmodus erzwingen an COM c (1...4) mit IRQ i (1...7)',nl
;;+ db '  /S$a[,i] - seriellen Mausmodus erzwingen an I/O-Adresse a (hexadezimal)',nl
;;+ db '	     mit IRQ i (1...7)',nl
    db '  /O       - verwende PS/2 / USB BIOS Mausrad Erkennung (experimentell)',nl
; 2008: made /Y (ignore MSys) the default and introduced /M (enable MSys)
;   db '  /Y	   - do not try Mouse Systems mode for non-PnP devices',nl,nl
    db '  /M       - probiere *alten* Mouse Systems / Genius Modus falls keine PnP Maus',nl,nl
    db '  /V	   - Suche umkehren: PS/2 nach serieller Maus finden',nl
    db '  /3	   - 3-Tasten-Modus erzwingen (nur fÅr Microsoft- und PS/2-MÑuse)',nl
    db '  /R[h[v]] - horizontale/vertikale Auflîsung (h,v=0...9; fehlende Option R,',nl
    db '	     bzw. keine Argumente oder 0 als Argument nach R, ergeben',nl
    db '	     automatische Auflîsung; ein fehlendes zweites Argument bedeutet',nl
    db '	     denselben Wert wie fÅr das erste Argument)',nl
    db '  /L	   - LinkshÑndermodus (Vorgabe ist RechtshÑndermodus)',nl
    db '  /B	   - Programm abbrechen, wenn Mausfunktionen bereits aktiv sind',nl
    db '  /N	   - CuteMouse als neues residentes Programm (TSR) laden, selbst',nl
    db '	     dann, wenn CTMOUSE bereits resident ist (sinnvoll fÅr',nl
    db '	     Batchdateien, die CuteMouse am Ende wieder entladen)',nl
    db '  /W	   - TSR nicht automatisch in den UMB laden',nl
    db '  /U	   - CuteMouse Treiber deinstallieren',nl
    db '  /?	   - diese Hilfe anzeigen',eos
