; This file is a part of CuteMouse source
; Here are messages in French
; Translated by Mateusz Viste <mateusz.viste@mail.ru> (until now)
; Translated by Frederic Pasteleurs <masterfrd@freegates.be> (until v1.9)

E_needvga       db 'Pas de VGA? Utilisez une ancienne version de CTMOUSE pour le support EGA RIL',nl,eos
E_error		db nl,'Erreur: Option ',eos
E_option	db 'invalide.',eos
E_argument	db "d'argument invalide.",eos
E_help          db 5,nl,"Entrez /? en ligne de commande pour l'aide.",eos

E_mousepresent  db 5,'Un pilote souris est d‚j… pr‚sent.',eos
E_notfound	db 5,'Erreur: p‚riph‚rique non trouv‚.',eos
;;+E_noIRQ	db 5,'Erreur: ligne IRQ non d‚tect‚e',eos

E_nocute        db 1,"Le pilote CuteMouse n'est pas install‚ !",eos
E_notunload     db 2,'Echec du d‚chargement du driver: les interruptions ont chang‚es...',eos
S_unloaded      db 0,'Le pilote CuteMouse est d‚charg‚.',eos

S_reset		db 'Portion r‚sidante initialis‚e … ',eos
S_installed	db 'Install‚ sur ',eos
S_atPS2		db 'port PS/2',eos
S_atCOM		db 'COM'
com_port	db ?,' ('
S_atIO		db '0000h/IRQ'
IRQno		db ?
S_in		db ') en mode ',eos
S_inMSYS	db '"Mouse Systems"',eos
S_inMS		db 'Microsoft',eos
S_inLT		db 'Logitech',eos
S_wheel         db ' (molette pr‚sente)'
S_CRLF		db nl,eos

Copyright	db nl,'CuteMouse v',CTMRELEASE,' [FreeDOS]',nl,eos
Syntax		label byte
    db 0,'Options:',nl
    db '  /P       - force le mode PS/2.',nl
    db "  /S[c[i]] - force le mode s‚rie sur le port COM c (1-4) avec l'IRQ",nl
    db '	     num‚ro i (1-7).',nl
;;+ db "  /S$a[,i] - force le mode souris s‚rie sur le port d'E/S a (nombre hexa)",nl
;;+ db '	     avec la ligne IRQ i (1-7).',nl
    db '  /O       - active la d‚tection de molette PS2 et BIOS USB (peut planter)',nl
; 2008: made /Y (ignore MSys) the default and introduced /M (enable MSys)
;   db '  /Y       - ne tente pas le mode Mouse Systems pour les souris non-PnP',nl,nl
    db "  /M       - tente *l'ancien* mode MouseSystems/Genius pour les souris non-PnP",nl,nl
    db '  /V       - Recherche invers‚e: recherche la souris PS/2 aprŠs la souris s‚rie',nl
    db '  /3       - force le mode 3-boutons (Souris Microsoft et PS/2 uniquement).',nl
    db "  /R[h[v]] - R‚solution horizontal/vertical (h,v=0-9; Si l'option R manque, si",nl
    db "	     il n'y a pas d'arguments ou si il y a z‚ro aprŠs R, la r‚solution",nl
    db '	     automatique sera utilis‚e; si le deuxiŠme argument est manquant,',nl
    db "	     la mˆme valeur que l'argument pr‚c‚dent sera utilis‚).",nl
    db '  /L       - Mode gaucher (le pilote est en mode droitier par d‚faut).',nl
    db '  /B       - Empˆche le d‚marrage du pilote si les services sont d‚j… pr‚sents.',nl
    db '  /N	   - Charge CuteMouse en tant que nouveau TSR, mˆme si CuteMouse est',nl
    db '             d‚j… charg‚. (utile pour des fichiers .BAT qui enlŠvent CuteMouse',nl
    db '	     … la fin)',nl
    db '  /W       - Empˆche le chargement du pilote dans les UMBs',nl
    db '  /U       - EnlŠve le pilote CuteMouse.',nl
    db '  /?	   - Montre cette aide.',eos
