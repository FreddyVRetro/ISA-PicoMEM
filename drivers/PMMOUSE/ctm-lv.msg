; This file is a part of CuteMouse source
; Here are messages in Latvian (for codepage 775)
; Translated by Kristaps Kaupe, Latvia <neons@digital.lv>

E_needvga       db 'No VGA? Use older CTMOUSE if you need EGA RIL support',nl,eos
E_error		db nl,'Kë×da: ',eos
E_option	db 'Nepareiza opcija',eos
E_argument	db 'Invalid option argument',eos
E_help		db 5,nl,'Ievadiet /? komandrindƒ lai ieg×tu palŒdzŒbu',eos

E_mousepresent	db 5,'Cits peles draiveris jau ir atmiìƒ',eos
E_notfound	db 5,'Kë×da: iekƒrta nav atrasta',eos
;;+E_noIRQ	db 5,'Kë×da: IRQ line not detected',eos

E_nocute	db 1,'CuteMouse draiveris netika ielƒd‰ts!',eos
E_notunload	db 2,'Kë×da izlƒd‰jot draiveri: daØi pƒrtraukumi aizìemti...',eos
S_unloaded	db 0,'Draiveris veiksmŒgi izlƒd‰ts...',eos

S_reset		db 'Rezidentƒ daëa pƒrlƒd‰ta uz ',eos
S_installed	db 'Ielƒd‰ts uz ',eos
S_atPS2		db 'PS/2 porta',eos
S_atCOM		db 'COM'
com_port	db ?,' ('
S_atIO		db '0000h/IRQ'
IRQno		db ?
S_in		db ') ',eos
S_inMSYS	db 'Mouse Systems reØŒmƒ',eos
S_inMS		db 'Microsoft reØŒmƒ',eos
S_inLT		db 'Logitech reØŒmƒ',eos
S_wheel		db ' (pele ar ritenŒti)'
S_CRLF		db nl,eos

Copyright	db nl,'CuteMouse v',CTMRELEASE,' [FreeDOS]',nl,eos
Syntax		label byte
    db 0,'Opcijas:',nl
    db '  /P	   - PS/2 peles reØŒms',nl
    db '  /S[c[i]] - seriƒlƒs peles reØŒms uz COM porta c (1-4) ar IRQ lŒniju i (1-7)',nl
;;+ db '  /S$a[,i] - seriƒlƒs peles reØŒms I/O adres‰ a (heksadecimƒlais numurs)',nl
;;+ db '	     ar IRQ lŒniju i (1-7)',nl
    db '  /O       - enable PS2 and BIOS USB wheel detection (might hang)',nl
; 2008: made /Y (ignore MSys) the default and introduced /M (enable MSys)
;   db '  /Y	   - do not try Mouse Systems mode for non-PnP devices',nl,nl
    db '  /M       - try *old* Mouse Systems / Genius for non-PnP mice',nl,nl
    db '  /V	   - apgrieztƒ mekl‰Õana: mekl‰t PS/2 p‰c seriƒlƒs peles',nl
    db '  /3	   - 3 pogu reØŒms (tikai Microsoft un PS/2 pel‰m)',nl
    db '  /R[h[v]] - horizontƒlƒ/vertikƒlƒ izÕéirtsp‰ja (h,v=0-9; ja nav opcijas R,',nl
    db '	     nav argumentu vai arguments p‰c R ir 0, tiek lietota automƒtiska',nl
    db '	     izÕéirtsp‰ja, ja tr×kst otrƒ argumentƒ tam tiek pieÕéirta tƒda',nl
    db '	     pati v‰rtŒba kƒ pirmajam)',nl
    db '  /L	   - kreisƒs rokas reØŒms (noklus‰tais ir labƒs rokas reØŒms)',nl
    db '  /B	   - pƒrtraukt darbu, ja cits peles draiveris jau ir atmiìƒ',nl
    db '  /N	   - load CuteMouse as new TSR, even if CuteMouse is already loaded',nl
    db '	     (useful for batch files which unload CuteMouse at end)',nl
    db '  /W	   - nelƒd‰t draivera rezidento daëu UMB',nl
    db '  /U	   - izlƒd‰t draiveri',nl
    db '  /?	   - parƒdŒt Õo palŒdzŒbu',eos
