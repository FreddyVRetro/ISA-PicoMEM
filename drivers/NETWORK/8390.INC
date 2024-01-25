; The EN registers - the DS8390 chip registers
; There are two (really 3) pages of registers in the chip. You select
; which page you want, then address them at offsets 00-0F from base.
; The chip command register (EN_CCMD) appears in both pages.
; EN_OFF is the offset from the board's base register to the EN registers.

EN_CCMD		equ	000h+EN_OFF	; Chip's command register

; Page 0

EN0_CLDALO	equ	001h+EN_OFF	; Low byte of current local dma addr  RD
EN0_STARTPG	equ	001h+EN_OFF	; Starting page of ring bfr WR
EN0_CLDAHI	equ	002h+EN_OFF	; High byte of current local dma addr  RD
EN0_STOPPG	equ	002h+EN_OFF	; Ending page +1 of ring bfr WR
EN0_BOUNDARY	equ	003h+EN_OFF	; Boundary page of ring bfr RD WR
EN0_TSR		equ	004h+EN_OFF	; Transmit status reg RD
EN0_TPSR	equ	004h+EN_OFF	; Transmit starting page WR
EN0_NCR		equ	005h+EN_OFF	; Number of collision reg RD
EN0_TCNTLO	equ	005h+EN_OFF	; Low  byte of tx byte count WR
EN0_FIFO	equ	006h+EN_OFF	; FIFO RD
EN0_TCNTHI	equ	006h+EN_OFF	; High byte of tx byte count WR
EN0_ISR		equ	007h+EN_OFF	; Interrupt status reg RD WR
EN0_CRDALO	equ	008h+EN_OFF	; low byte of current remote dma address RD
EN0_RSARLO	equ	008h+EN_OFF	; Remote start address reg 0
EN0_CRDAHI	equ	009h+EN_OFF	; high byte of current remote dma address RD
EN0_RSARHI	equ	009h+EN_OFF	; Remote start address reg 1
EN0_RCNTLO	equ	00ah+EN_OFF	; Remote byte count reg WR
EN0_RCNTHI	equ	00bh+EN_OFF	; Remote byte count reg WR
EN0_RSR		equ	00ch+EN_OFF	; rx status reg RD
EN0_RXCR	equ	00ch+EN_OFF	; RX control reg WR
EN0_TXCR	equ	00dh+EN_OFF	; TX control reg WR
EN0_COUNTER0	equ	00dh+EN_OFF	; Rcv alignment error counter RD
EN0_DCFG	equ	00eh+EN_OFF	; Data configuration reg WR
EN0_COUNTER1	equ	00eh+EN_OFF	; Rcv CRC error counter RD
EN0_IMR		equ	00fh+EN_OFF	; Interrupt mask reg WR
EN0_COUNTER2	equ	00fh+EN_OFF	; Rcv missed frame error counter RD

; Page 1

EN1_PHYS	equ	001h+EN_OFF	; This board's physical enet addr RD WR
EN1_CURPAG	equ	007h+EN_OFF	; Current memory page RD WR
EN1_MULT	equ	008h+EN_OFF	; Multicast filter mask array (8 bytes) RD WR

; Chip commands in EN_CCMD
ENC_STOP	equ	001h	; Stop the chip
ENC_START	equ	002h	; Start the chip
ENC_TRANS	equ	004h	; Transmit a frame
ENC_RREAD	equ	008h	; remote read
ENC_RWRITE	equ	010h	; remote write
ENC_NODMA	equ	020h	; No remote DMA used on this card
ENC_PAGE0	equ	000h	; Select page 0 of chip registers
ENC_PAGE1	equ	040h	; Select page 1 of chip registers

; Commands for EN0_RXCR - RX control reg
ENRXCR_CRC	equ	001h	; Save error pkts
ENRXCR_RUNT	equ	002h	; Accept runt pkt
ENRXCR_BCST	equ	004h	; Accept broadcasts
ENRXCR_MULTI	equ	008h	; Multicast (if pass filter)
ENRXCR_PROMP	equ	010h	; Promiscuous physical addresses
ENRXCR_MON	equ	020h	; Monitor mode (no packets rcvd)

; Bits in EN0_TXCR - transmit control reg
ENTXCR_CRC	equ	001h		; inhibit CRC, do not append crc
ENTXCR_LOOP	equ	002h		; Set loopback mode
ENTXCR_LB01	equ	006h		; encoded loopback control
ENTXCR_ATD	equ	008h		; auto tx disable
ENTXCR_OFST	equ	010h		; collision offset enable

; Bits in EN0_DCFG - Data config register
ENDCFG_WTS	equ	001h	; word transfer mode selection
ENDCFG_BOS	equ	002h	; byte order selection
ENDCFG_LAS	equ	004h	; long addr selection
ENDCFG_BMS	equ	008h	; burst mode selection
ENDCFG_ARM	equ	010h	; autoinitialize remote
ENDCFG_FT00	equ	000h	; burst length selection
ENDCFG_FT01	equ	020h	; burst length selection
ENDCFG_FT10	equ	040h	; burst length selection
ENDCFG_FT11	equ	060h	; burst length selection

; Bits in EN0_ISR - Interrupt status register
ENISR_RX	equ	001h	; Receiver, no error
ENISR_TX	equ	002h	; Transmitter, no error
ENISR_RX_ERR	equ	004h	; Receiver, with error
ENISR_TX_ERR	equ	008h	; Transmitter, with error
ENISR_OVER	equ	010h	; Receiver overwrote the ring
ENISR_COUNTERS	equ	020h	; Counters need emptying
ENISR_RDC	equ	040h	; remote dma complete
ENISR_RESET	equ	080h	; Reset completed

ENISR_ALL	equ	03fh	; Interrupts we will enable

; Bits in received packet status byte and EN0_RSR
ENRSR_RXOK	equ	001h	; Received a good packet
ENRSR_CRC	equ	002h	; CRC error
ENRSR_FAE	equ	004h	; frame alignment error
ENRSR_FO	equ	008h	; FIFO overrun
ENRSR_MPA	equ	010h	; missed pkt
ENRSR_PHY	equ	020h	; physical/multicase address
ENRSR_DIS	equ	040h	; receiver disable. set in monitor mode
ENRSR_DEF	equ	080h	; deferring

; Bits in EN0_TSR -  TX status reg

ENTSR_PTX	equ	001h	; Packet transmitted without error
ENTSR_DFR	equ	002h	; non deferred tx
ENTSR_COLL	equ	004h	; Collided at least once
ENTSR_COLL16	equ	008h	; Collided 16 times and was dropped
ENTSR_CRS	equ	010h	; carrier sense lost
ENTSR_FU	equ	020h	; TX FIFO Underrun
ENTSR_CDH	equ	040h	; collision detect heartbeat
ENTSR_OWC	equ	080h	; out of window collision

; Description of header of each packet in receive area of memory

EN_RBUF_STAT	equ	0	; Received frame status
EN_RBUF_NXT_PG	equ	1	; Page after this frame
EN_RBUF_SIZE_LO	equ	2	; Length of this frame
EN_RBUF_SIZE_HI	equ	3	; Length of this frame
EN_RBUF_NHDR	equ	4	; Length of above header area

