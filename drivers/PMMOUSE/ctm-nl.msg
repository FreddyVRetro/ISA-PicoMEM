; This file is a part of CuteMouse source
; Here are messages in Dutch
; Translated by Bernd Blaauw <bblnews@hotmail.com>

E_needvga       db 'Geen VGA? Gebruik oudere CTMOUSE voor EGA RIL ondersteuning',nl,eos
E_error		db nl,'Fout: Ongeldige optie'
E_option	db eos
E_argument	db ' argument',eos
E_help		db 5,nl,'Voer /? in op opdrachtregel voor hulp',eos

E_mousepresent	db 5,'Muis service reeds aanwezig',eos
E_notfound	db 5,'Fout: apparaat niet gevonden',eos
;;+E_noIRQ	db 5,'Fout: IRQ lijn niet gevonden',eos

E_nocute	db 1,'CuteMouse stuurprogramma is niet geladen!',eos
E_notunload	db 2,'ontladen stuurprogramma mislukt: interrupts onderbroken',eos
S_unloaded	db 0,'Stuurprogramma is succesvol ontladen...',eos

S_reset		db 'Residente gedeelte gereset naar ',eos
S_installed	db 'Geinstalleerd op ',eos
S_atPS2		db 'PS/2 poort',eos
S_atCOM		db 'COM'
com_port	db ?,' ('
S_atIO		db '0000h/IRQ'
IRQno		db ?
S_in		db ') in ',eos
S_inMSYS	db 'Mouse Systems mode',eos
S_inMS		db 'Microsoft mode',eos
S_inLT		db 'Logitech mode',eos
S_wheel		db ' (muiswiel aanwezig)'
S_CRLF		db nl,eos

Copyright	db nl,'CuteMouse v',CTMRELEASE,' [FreeDOS]',nl,eos
Syntax		label byte
    db 0,'Opties:',nl
    db '  /P	   - dwing PS/2 muis-modus af',nl
    db '  /S[c[i]] - forceer seriele muismodus op COM poort c=1-4 met IRQ lijn i=1-7',nl
;;+ db '  /S$a[,i] - dwing seriele muis-modus af op I/O adres a (hexadecimaal getal)',nl
;;+ db '	     met IRQ lijn i=1-7',nl
    db '  /O       - probeer PS/2 of USB BIOS wiel herkenning (experimenteel)',nl
; 2008: made /Y (ignore MSys) the default and introduced /M (enable MSys)
;   db '  /Y	   - do not try Mouse Systems mode for non-PnP devices',nl,nl
    db '  /M       - gebruik *oud* Mouse Systems / Genius protocol als geen PnP muis',nl,nl
    db '  /V	   - omgekeerd zoeken: PS/2 na seriele muis vinden',nl
    db '  /3	   - dwing 3-knops modus af (alleen voor Microsoft en PS/2 muizen)',nl
    db '  /R[h[v]] - horizontale/verticale resolutie (h,v=0-9; geen optie R, geen',nl
    db '	     h en v of 0 als argument na R betekent auto-resolutie, ontbrekend',nl
    db '	     tweede argument houdt dezelfde waarde als voor het 1e argument in',nl
    db '  /L	   - linkshandige modus (standaard ingesteld op rechtshandig)',nl
    db '  /B	   - niet laden als een muis-stuurprogramma al geladen is',nl
    db '  /N	   - CuteMouse laden als een nieuwe TSR, zelfs als het al geladen is',nl
    db '	     (handig voor batchbestanden die op het einde CuteMouse ontladen)',nl
    db '  /W	   - voorkomt het laden van deze TSR in UMBs',nl
    db '  /U	   - stuurprogramma deactiveren',nl
    db '  /?	   - toont dit helpscherm',eos
