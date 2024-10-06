; This file is a part of CuteMouse source
; Here are messages in Polish
; Translated by:
;                - Mateusz Viste <mateusz.viste@mail.ru> (until now)
;                - Jerzy 'HARY' Klejnowski <ksionrze@poczta.wp.pl> (until v1.9)


E_needvga       db 'Brak VGA? Ußyj starszej wersji CTMOUSE dla obsíugi EGA RIL',nl,eos
E_error         db nl,'BíÜd: niepoprawn',eos
E_option	db 'a opcja',eos
E_argument	db 'y argument',eos
E_help          db 5,nl,'Pomoc uzyskasz podajÜc /? w wierszu polece§',eos

E_mousepresent  db 5,'Usíugi myszy sÜ juß dostëpne',eos
E_notfound      db 5,'BíÜd: nie znaleziono urzÜdzenia',eos
;;+E_noIRQ      db 5,'BíÜd: nie wykryto linii IRQ',eos

E_nocute        db 1,'Sterownik CuteMouse nie zostaí zainstalowany!',eos
E_notunload     db 2,'Nie mogë odinstalowaç sterownika: zmienione wektory przerwa§...',eos
S_unloaded      db 0,'Sterownik odinstalowany...',eos

S_reset         db 'Czëûç rezydentna ustawiona na ',eos
S_installed	db 'Zainstalowany na: ',eos
S_atPS2		db 'PS/2',eos
S_atCOM		db 'COM'
com_port	db ?,' ('
S_atIO		db '0000h/IRQ'
IRQno		db ?
S_in            db '), protok¢í ',eos
S_inMSYS	db 'Mouse Systems',eos
S_inMS		db 'Microsoft',eos
S_inLT		db 'Logitech',eos
S_wheel         db nl,9,9,'(obsíuga rolki wíÜczona)'
S_CRLF		db nl,eos

Copyright	db nl,'CuteMouse v',CTMRELEASE,' [FreeDOS]',nl,eos
Syntax		label byte
    db 0,'Opcje:',nl
    db '  /P	   - wymusza tryb PS/2',nl
    db '  /S[c[i]] - wymusza ußycie portu szeregowego COM nr c (1-4) na przerwaniu',nl
    db '	     i (1-7)',nl
;;+ db '  /S$a[,i] - wymusza ußycie portu szeregowego pod adresem a (szesnastkowo)',nl
;;+ db '	     na przerwaniu i (1-7)',nl
    db '  /O       - umoßliwia wykrycie k¢íka PS2 i USB BIOS (moße sië zawiesiç)',nl
; 2008: made /Y (ignore MSys) the default and introduced /M (enable MSys)
;   db '  /Y       - nie pr¢buje trybu Mouse Systems dla urzÜdze§ nie-PnP',nl,nl
    db '  /M       - pr¢buje *dawny* Mouse Systems / Genius dla myszy nie-PnP',nl,nl
    db '  /V       - odwraca kolejnoûç: szuka najpierw na COM, potem na PS/2',nl
    db '  /3       - wymusza tryb 3-przyciskowy (tylko dla myszy Microsoft i PS/2)',nl
    db '  /R[h[v]] - rozdzielczoûç pozioma/pionowa (h,v=0-9; brak opcji R, brak',nl
    db '	     argument¢w lub podanie 0 oznacza auto resolution; brak drugiego',nl
    db '             argumentu oznacza takÜ samÜ wartoûç jak dla pierwszego argumentu)',nl
    db '  /L       - mysz dla leworëcznych (domyûlnie dla praworëcznych)',nl
    db '  /B       - nie uruchamiaj, jeûli usíugi myszy sÜ juß dostëpne',nl
    db '  /N       - instaluj, nawet jeûli sterownik CuteMouse jest juß zainstalowany',nl
    db '             (ußyteczne w .BAT, kt¢re deinstalujÜ CuteMouse na ko§cu)',nl
    db '  /W       - nie instaluj czëûci rezydentnej w UMB',nl
    db '  /U       - odinstaluj sterownik',nl
    db '  /?       - pokaß ten tekst',eos
