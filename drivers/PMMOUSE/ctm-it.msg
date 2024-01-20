; This file is a part of CuteMouse source
; Here are messages in Italian
; Translated by Massimo Giussani <massimo-at-suespammers.org>
; updated 25/8/08 from Roberto Perotti Iw2evk
; Please note that suespammers domain is located in California,
; so DON'T spam that account or you'll be sued!!

E_needvga       db 'No VGA? Use older CTMOUSE if you need EGA RIL support',nl,eos
E_error		db nl,'Errore: Opzione invalida'
E_option	db eos
E_argument	db ' (argomento errato)',eos
E_help		db 5,nl,"Per l'aiuto aggiungere /? alla linea di comando",eos

E_mousepresent	db 5,"Servizi Mouse gia' presenti",eos
E_notfound	db 5,'Errore: mouse non trovato',eos
;;+E_noIRQ	db 5,'Errore: nessun IRQ trovato',eos

E_nocute	db 1,"Il driver CuteMouse non e' stato installato!",eos
E_notunload	db 2,'Disinstallazione fallita: alcuni interrupts sono stati intercettati!',eos
S_unloaded	db 0,'Driver disinstallato',eos

S_reset		db "Parte residente impostata su ",eos
S_installed	db 'Installato su ',eos
S_atPS2		db 'Porta PS/2',eos
S_atCOM		db 'COM'
com_port	db ?,' ('
S_atIO		db '0000h/IRQ'
IRQno		db ?
S_in		db ") tipo ",eos
S_inMSYS	db 'Mouse Systems',eos
S_inMS		db 'Microsoft',eos
S_inLT		db 'Logitech',eos
S_wheel		db ' (con rotellina)'
S_CRLF		db nl,eos

Copyright	db nl,'CuteMouse v',CTMRELEASE,' [FreeDOS]',nl,eos
Syntax		label byte
    db 0,'Opzioni:',nl
    db "  /P	   - forza modalita' PS/2",nl
    db "  /S[c[i]] - forza modalita' seriale sulla porta COM c (1-4) ed IRQ i (1-7)",nl
;;+ db "  /S$a[,i] - forza modalita' seriale all'indirizzo I/O a (in esadecimale)",nl
;;+ db '	     con IRQ i (1-7)',nl
    db "  /O       - abilita PS2 e il BIOS USB per riconoscere la ruota",nl
; 2008: made /Y (ignora MSys) come default e introducede /M (abilita MSys)
;   db '  /Y	   - non prova il modo Mouse Systems per  non-PnP devices',nl,nl
    db '  /M       - prova il "vecchio" Mouse Systems / Genius per non-PnP mouse',nl,nl
    db '  /V	   - inverte la ricerca: cerca il mouse PS/2 dopo quello seriale',nl
    db "  /3	   - forza l'uso dei 3 pulsanti (solo per mouse Microsoft e PS/2)",nl
    db "  /R[h[v]] - risoluzione orizzontale/verticale (h,v=0-9; senza l'opzione R, o",nl
    db '	     senza h e v oppure con /R0, significa risoluzione automatica; se',nl
    db "	     v e' mancante allora assume per esso lo stesso valore dato ad h)",nl
    db "  /L	   - imposta la modalita' per utenti mancini",nl
    db "  /B	   - non installa se i servizi mouse sono gia' presenti",nl
    db '  /N	   - forza un ulteriore caricamento di CuteMouse come TSR (utile',nl
    db '	     per quei files batch che alla fine disinstallano CuteMouse)',nl
    db '  /W	   - impedisce il caricamento TSR negli UMB',nl
    db '  /U	   - disinstalla il driver',nl
    db '  /?	   - mostra questo aiuto',eos
