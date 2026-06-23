// PicoMEM and Pico Boards specific definitions
#pragma once

//!!!!!  A Changer /Definir autrement : PM1.5 n est pas forcement Hardcode sur IRQ7
#if BOARD_PM15 || BOARD_PM20
#define PIN_PM_IRQ 41
#else
#define PIN_PM_IRQ 0
#endif 

// Picomem 1.5
#ifdef BOARD_PM15
#include "pm_boards/pm15.h"
#endif

// Picomem 2.0
#ifdef BOARD_PM20   
#include "pm_boards/pm20.h"
#endif

// Picomem 1.0, 1.1, 1.11, 1.12, 1.14
#ifdef BOARD_PM
#include "pm_boards/pm.h"
#endif

// Picomem 1.3 (New multiplexing)
#ifdef BOARD_PM13
#include "pm_boards/pm13.h"
#endif

// Picomem Low Profile and 1.2A
#ifdef BOARD_PM_LP
#include "pm_boards/pm_lp.h"
#endif

#ifdef BOARD_PM_NG
#include "pm_boards/pm_ng.h"
#endif

// Picomem Prototype
#ifdef BOARD_PMPROTO
#include "pm_boards/pm_proto.h"
#endif

// WonderMCA 1.0
#ifdef BOARD_WM10
#include "wm_board.h"
#endif